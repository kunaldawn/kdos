/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-peek — what is in this file, without starting its application
 *
 *   ╔═ report.pdf — page 2 of 14 ══════════════════════════╗
 *   ║                                                      ║
 *   ║                  (the page, tiled)                   ║
 *   ║                                                      ║
 *   ╟──────────────────────────────────────────────────────╢
 *   ║ PgDn next  PgUp back  Esc Close                      ║
 *   ╚══════════════════════════════════════════════════════╝
 *
 * SPACE IN A FILE MANAGER, and the reason it is a surface rather than a
 * handler: opening a PDF starts a reader, and a reader is a window, a process
 * and a restored scroll position for a question that was "which one is this".
 *
 * FOUR KINDS AND ONE DECISION, taken in this order, because the cheap and
 * certain tests come first: the magic bytes of a picture, then a document
 * extension mutool can render, then whatever libarchive agrees to open, then
 * text. Anything left is refused by name rather than shown as mojibake.
 *
 * NOTHING HERE DECODES A PICTURE. libkimg is the one place in KDOS that turns
 * untrusted image bytes into pixels, under a budget checked before any
 * allocation, and a page from `mutool` arrives as a PNG and goes through the
 * same call — a renderer that wrote its own path for "our own" pixels would be
 * a second decoder with a second set of bugs.
 *
 * A DIRECTORY IS REFUSED. A file manager inside a viewer that was opened from
 * a file manager is a circle; `mc` shows directories and this shows files.
 * ---------------------------------
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>
#include <pixman.h>

#include "kbase.h"
#include "kcell.h"
#include "kcon.h"
#include "kimg.h"
#include "kwl.h"
#include "shell.h"

#define PK_COLS 96
#define PK_ROWS 32


/* An archive listing stops here. The entry count is the archive's choice. */
#define PK_MAX_ENTRIES 4096

enum { PK_NONE = 0, PK_IMAGE, PK_DOC, PK_ARCHIVE, PK_TEXT };

static char path[PATH_MAX];
static const char *base;
static int kind;
static char note[192];

/* The picture on the screen and the tiles registered for it — picture.c's,
 * shared with kdos-pix. */
static ShPic pic;
static int have_pic;

/* Documents. `npages` is 0 when `mutool info` said nothing usable, which draws
 * a page number without a total rather than a total that is a guess. */
static int page = 1, npages;

/* Archives. */
struct entry {
	char name[256];
	long long bytes;
};
static struct entry *ents;
static int nents, sel, top;

static KtuiKeys keys;

/* ── what kind of file this is ─────────────────────────────────────────── */

static const char *ext_of(const char *p)
{
	const char *dot = strrchr(p, '.');
	const char *slash = strrchr(p, '/');

	if (!dot || (slash && dot < slash))
		return "";
	return dot;
}

/*
 * A DOCUMENT IS AN EXTENSION AND NOT A SNIFF, because the renderer is chosen
 * by it: `mutool` reads these five and reports its own refusal for anything
 * else, so a wrong guess costs a failed fork rather than a wrong picture.
 */
static int is_doc(const char *p)
{
	static const char *const DOC[] = { ".pdf", ".epub", ".cbz", ".xps",
					   ".fb2", NULL };
	const char *e = ext_of(p);

	for (int i = 0; DOC[i]; i++)
		if (!strcasecmp(e, DOC[i]))
			return 1;
	return 0;
}

/* Text is the ABSENCE of a NUL in what a reader would see first. Every other
 * test — a charset guess, a MIME lookup — is a second opinion about a question
 * `less` is about to answer for itself. */
static int looks_like_text(const unsigned char *b, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (!b[i])
			return 0;
	return 1;
}

/* ── the picture ───────────────────────────────────────────────────────── */

/* The magic sniff, the decode, the crop, the tiles and the draw are picture.c's
 * and shared with kdos-pix. What is left here is which of them to call. */

/* ── documents, through mutool ─────────────────────────────────────────── */

/* How many pages, or 0. One fork at open: a page number without a total is
 * honest, a total this program guessed is not. */
static int doc_pages(void)
{
	KbArgv a = { 0 };
	char out[4096];
	const char *p;

	if (!kb_have_prog("mutool"))
		return 0;
	kb_argv_add(&a, "mutool");
	kb_argv_add(&a, "info");
	kb_argv_add(&a, path);
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, sizeof(out)) != 0)
		return 0;
	p = strstr(out, "Pages: ");
	return p ? atoi(p + 7) : 0;
}

/*
 * Render one page at the size of the pane, into the same decode every picture
 * takes. `-w` and `-h` are a MAXIMUM here because no `-r` is given, so the
 * page keeps its aspect and lands inside the box rather than being stretched
 * to it.
 */
static int doc_render(int pane_px_w, int pane_px_h)
{
	KbArgv a = { 0 };
	char tmp[512];
	unsigned char *b;
	size_t n = 0;
	int rc;

	if (!kb_have_prog("mutool")) {
		snprintf(note, sizeof(note), "mutool is not on this machine");
		return -1;
	}
	if (pane_px_w < 16 || pane_px_h < 16)
		return -1;
	snprintf(tmp, sizeof(tmp), "%s/kdos-peek-%d.png", kb_runtime_dir(),
		 (int)getpid());
	kb_argv_add(&a, "mutool");
	kb_argv_add(&a, "draw");
	kb_argv_add(&a, "-F");
	kb_argv_add(&a, "png");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, tmp);
	kb_argv_add(&a, "-w");
	kb_argv_addf(&a, "%d", pane_px_w);
	kb_argv_add(&a, "-h");
	kb_argv_addf(&a, "%d", pane_px_h);
	kb_argv_add(&a, path);
	kb_argv_addf(&a, "%d", page);
	kb_argv_end(&a);
	rc = kb_run(&a);
	if (rc != 0) {
		unlink(tmp);
		snprintf(note, sizeof(note), "page %d could not be rendered",
			 page);
		return -1;
	}
	b = sh_pic_slurp(tmp, &n);
	/* The temporary is this call's and nothing else reads it. */
	unlink(tmp);
	if (!b)
		return -1;
	rc = sh_pic_set(&pic, b, n);
	have_pic = rc == 0;
	free(b);
	if (rc != 0)
		snprintf(note, sizeof(note), "page %d is not a picture", page);
	return rc;
}

/* ── archives, through libarchive ──────────────────────────────────────── */

/*
 * The entries, listed and not extracted. `archive_read_open_filename` is also
 * the TEST for whether this is an archive at all: the format probe is
 * libarchive's, which is the same code that would read it, rather than a
 * table of extensions that would disagree with it.
 */
static int archive_list(void)
{
	struct archive *a = archive_read_new();
	struct archive_entry *e;
	int n = 0;

	if (!a)
		return -1;
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	if (archive_read_open_filename(a, path, 65536) != ARCHIVE_OK) {
		archive_read_free(a);
		return -1;
	}
	ents = calloc(PK_MAX_ENTRIES, sizeof(*ents));
	if (!ents) {
		archive_read_free(a);
		return -1;
	}
	while (n < PK_MAX_ENTRIES &&
	       archive_read_next_header(a, &e) == ARCHIVE_OK) {
		const char *nm = archive_entry_pathname(e);

		snprintf(ents[n].name, sizeof(ents[n].name), "%s",
			 nm ? nm : "?");
		ents[n].bytes = (long long)archive_entry_size(e);
		n++;
		archive_read_data_skip(a);
	}
	archive_read_free(a);
	nents = n;
	return 0;
}

/* ── text, which is the pager's ────────────────────────────────────────── */

/*
 * `less` in a terminal, and this exits. A pager inside this window would be a
 * second implementation of scrolling, searching and line wrapping, and the one
 * on the machine is better than the one this file would grow.
 *
 * The path is passed bare because it has already been through `realpath`, so
 * it begins with `/` and can never be read as an option.
 */
static int open_pager(void)
{
	const char *argv[12];
	char id[160];
	int n = sh_term_argv(argv, 0, 12, "less", id, sizeof(id));

	if (n < 0 || n + 3 > 12)
		return -1;
	argv[n++] = "less";
	argv[n++] = path;
	argv[n] = NULL;
	sh_spawn(argv);
	return 0;
}

/* ── the frame ─────────────────────────────────────────────────────────── */

/* The pane the picture gets: inside the border, above the hint row. */
static void pane_cells(int *x, int *y, int *w, int *h)
{
	*x = 1;
	*y = 1;
	*w = ktui_w - 2;
	*h = ktui_h - 4;
	if (*w < 1)
		*w = 1;
	if (*h < 1)
		*h = 1;
}

static void draw_picture(void)
{
	int px, py, pw, ph, cw = 0, ch = 0;

	pane_cells(&px, &py, &pw, &ph);
	sh_pic_fit(pic.w, pic.h, pw, ph, &cw, &ch);
	/* The whole picture, so the source rectangle is all of it: zoom and
	 * pan are kdos-pix's, and a quick look is the file as it is. */
	sh_pic_view(&pic, 0, 0, pic.w, pic.h, cw, ch);
	if (pic.cw < 1) {
		const char *msg = "no pixels on this display";

		ktui_draw_text(px + (pw - (int)strlen(msg)) / 2, py + ph / 2,
			       pw, msg, KT_MID, KT_SURFACE, KT_A_NONE);
		return;
	}
	sh_pic_draw(&pic, px + (pw - pic.cw) / 2, py + (ph - pic.ch) / 2);
}

static void draw_archive(void)
{
	int px, py, pw, ph;

	pane_cells(&px, &py, &pw, &ph);
	if (sel < top)
		top = sel;
	if (sel >= top + ph)
		top = sel - ph + 1;
	for (int i = 0; i < ph && top + i < nents; i++) {
		const struct entry *e = &ents[top + i];
		int y = py + i, on = top + i == sel;
		int bg = on ? KT_ACCENT : KT_SURFACE;
		int fg = on ? KT_SURFACE : KT_TEXT;

		ktui_draw_fill(krect(px, y, pw, 1), bg);
		ktui_draw_text(px + 1, y, pw - 14, e->name, fg, bg,
			       KT_A_NONE);
		ktui_draw_text(px + pw - 12, y, 11,
			       kb_human_size(e->bytes),
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	char title[320];

	if (w < 24 || h < 8)
		return;

	if (kind == PK_DOC && npages > 0)
		snprintf(title, sizeof(title), "%s — page %d of %d", base,
			 page, npages);
	else if (kind == PK_DOC)
		snprintf(title, sizeof(title), "%s — page %d", base, page);
	else if (kind == PK_ARCHIVE)
		snprintf(title, sizeof(title), "%s — %d entr%s", base, nents,
			 nents == 1 ? "y" : "ies");
	else if (kind == PK_IMAGE)
		snprintf(title, sizeof(title), "%s — %dx%d", base, pic.w,
			 pic.h);
	else
		snprintf(title, sizeof(title), "%s", base);

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 1);

	if (kind == PK_ARCHIVE)
		draw_archive();
	else if (have_pic)
		draw_picture();
	else {
		const char *msg = note[0]	 ? note
				  : kind == PK_TEXT ? "text — opens in less"
						    : "nothing here can show this file";

		ktui_draw_text((w - (int)strlen(msg)) / 2, h / 2, w - 2, msg,
			       note[0] ? KT_WARN : KT_MID, KT_SURFACE,
			       KT_A_NONE);
	}

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	ktui_hint_if(kind == PK_DOC, "PgDn", "next");
	ktui_hint_if(kind == PK_DOC, "PgUp", "back");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	ktui_draw_flush();
}

/* ── the program ───────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr, "usage: kdos-peek [--font NAME] [--dump] FILE\n");
	return 2;
}

/*
 * Decide what this file is, and load it. Returns 0, or -1 on a refusal that has
 * already been reported.
 *
 * IT STARTS NOTHING. A dump is the documented way to look at this surface and
 * it must not fork a terminal to do it, which is the same split the panel
 * keeps between measuring and acting.
 */
static int classify(void)
{
	unsigned char head[4096];
	size_t n = 0;
	struct stat st;
	FILE *f;

	if (stat(path, &st) != 0) {
		fprintf(stderr, "kdos-peek: %s: no such file\n", path);
		return -1;
	}
	if (S_ISDIR(st.st_mode)) {
		fprintf(stderr, "kdos-peek: %s is a directory — open it in a "
				"file manager\n", path);
		return -1;
	}
	f = fopen(path, "rb");
	if (f) {
		n = fread(head, 1, sizeof(head), f);
		fclose(f);
	}

	if (sh_pic_is_image(head, n)) {
		unsigned char *b;
		size_t len = 0;

		kind = PK_IMAGE;
		b = sh_pic_slurp(path, &len);
		if (b) {
			if (sh_pic_set(&pic, b, len) != 0)
				snprintf(note, sizeof(note),
					 "this picture could not be decoded");
			else
				have_pic = 1;
			free(b);
		}
		return 0;
	}
	if (is_doc(path)) {
		kind = PK_DOC;
		npages = doc_pages();
		return 0;
	}
	if (archive_list() == 0) {
		kind = PK_ARCHIVE;
		return 0;
	}
	kind = looks_like_text(head, n) ? PK_TEXT : PK_NONE;
	return 0;
}

int peek_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *want = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (argv[i][0] == '-')
			return usage();
		else if (!want)
			want = argv[i];
		else
			return usage();
	}
	if (!want)
		return usage();
	if (!realpath(want, path)) {
		fprintf(stderr, "kdos-peek: %s: no such file\n", want);
		return 1;
	}
	base = kb_basename(path);

	if (classify() != 0)
		return 1;
	/* Text is the pager's, and this exits — but never from a dump, which
	 * draws what it would have done instead. */
	if (kind == PK_TEXT && !dump)
		return open_pager() == 0 ? 0 : 1;

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = PK_COLS,
		.rows = PK_ROWS,
		.app_id = "kdos-peek",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(PK_COLS, PK_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-peek: no display server\n");
		return 1;
	}
	ktui_draw_init();
	/* AFTER kdisp_init, never before — see picture.c. */
	sh_pic_backend();

	if (kind == PK_DOC) {
		int px, py, pw, ph;

		pane_cells(&px, &py, &pw, &ph);
		doc_render(pw * sh_pic_cell_w(), ph * sh_pic_cell_h());
	}

	while (!kdisp_should_close()) {
		draw();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
				/* A page is rendered AT the pane's pixel size,
				 * so a resize is a re-render rather than a
				 * rescale of what was already drawn. */
				if (kind == PK_DOC) {
					int px, py, pw, ph;

					pane_cells(&px, &py, &pw, &ph);
					doc_render(pw * sh_pic_cell_w(),
						   ph * sh_pic_cell_h());
				}
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				break;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		note[0] = '\0';
		switch (ev.key) {
		case KT_K_DOWN:
			if (kind == PK_ARCHIVE && sel + 1 < nents)
				sel++;
			break;
		case KT_K_UP:
			if (kind == PK_ARCHIVE && sel > 0)
				sel--;
			break;
		case KT_K_HOME:
			if (kind == PK_ARCHIVE)
				sel = 0;
			else if (kind == PK_DOC && page != 1) {
				page = 1;
				goto repage;
			}
			break;
		case KT_K_END:
			if (kind == PK_ARCHIVE)
				sel = nents ? nents - 1 : 0;
			else if (kind == PK_DOC && npages > 0 &&
				 page != npages) {
				page = npages;
				goto repage;
			}
			break;
		case KT_K_PGDN:
			if (kind == PK_ARCHIVE) {
				sel += 10;
				if (sel >= nents)
					sel = nents ? nents - 1 : 0;
			} else if (kind == PK_DOC &&
				   (npages <= 0 || page < npages)) {
				page++;
				goto repage;
			}
			break;
		case KT_K_PGUP:
			if (kind == PK_ARCHIVE) {
				sel -= 10;
				if (sel < 0)
					sel = 0;
			} else if (kind == PK_DOC && page > 1) {
				page--;
				goto repage;
			}
			break;
		}
		continue;
repage:
		{
			int px, py, pw, ph;

			pane_cells(&px, &py, &pw, &ph);
			if (doc_render(pw * sh_pic_cell_w(), ph * sh_pic_cell_h()) != 0 &&
			    page > 1 &&
			    npages <= 0)
				/* Past the end of a document whose length is
				 * unknown: the failed render IS the answer. */
				page--;
		}
	}

	sh_pic_free(&pic);
	free(ents);
	kdisp_shutdown();
	return 0;
}
