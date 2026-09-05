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

/*
 * A CELL'S PIXEL SIZE, WHERE THERE IS ONE. Under the compositor the backend
 * knows; as a console surface this program has no pixels at all — the display
 * it is eventually drawn on does, and it scales what arrives. So a console
 * client renders at a nominal size, which bounds what goes on the wire without
 * pretending to know the font somebody else is using. The terminal's inline
 * pictures take the same two numbers for the same reason.
 */
#define PK_NOMINAL_CW 10
#define PK_NOMINAL_CH 20

/*
 * WHAT A PICTURE MAY COST, enforced by libkimg before it allocates anything.
 * A header is an allocation request from a file somebody sent you: 65535 by
 * 65535 is eight bytes on disk and sixteen gigabytes in memory.
 */
#define PK_MAX_W    16384
#define PK_MAX_H    16384
#define PK_MAX_PIX  (256u << 20)	/* the decoded image */
#define PK_MAX_FILE (256u << 20)	/* what is read off the disk */
/* Sprite tiles for one page. A viewer holds one picture; the second put
 * replaces the first and the evictor frees it. */
#define PK_SPRITE_BUDGET (64u << 20)
/* An archive listing stops here. The entry count is the archive's choice. */
#define PK_MAX_ENTRIES 4096

enum { PK_NONE = 0, PK_IMAGE, PK_DOC, PK_ARCHIVE, PK_TEXT };

static char path[PATH_MAX];
static const char *base;
static int kind;
static char note[192];

/* The picture on the screen, and the tiles registered for it. */
static pixman_image_t *pic;
/* The picture's own identity, and the key its tiles are registered under —
 * which is that identity mixed with the SIZE, so a resize registers a second
 * grid rather than replacing the first under a name that no longer describes
 * it. Derived on every use rather than accumulated, or two resizes would fold
 * both sizes into one value. */
static uint64_t pic_id, pic_key;
static int pic_cw, pic_ch;
static int pic_w, pic_h;	/* the decoded size, for the status line */

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

/* The magic of the formats libkimg has decoders for. The sniff is here as well
 * as inside libkimg because the decision is taken BEFORE the file is read: a
 * two-gigabyte video must not be loaded to discover it is not a PNG. */
static int looks_like_image(const unsigned char *b, size_t n)
{
	if (n >= 8 && !memcmp(b, "\x89PNG\r\n\x1a\n", 8))
		return 1;
	if (n >= 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff)
		return 1;
	if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4))
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

static int cell_w(void)
{
	int w = kdisp_cell_w();

	return w > 1 ? w : PK_NOMINAL_CW;
}

static int cell_h(void)
{
	int h = kdisp_cell_h();

	return h > 1 ? h : PK_NOMINAL_CH;
}

/*
 * WHERE A SPRITE'S PIXELS COME FROM when this is a console surface. libkcon
 * links no pixel library and must not; it asks for the bytes through this and
 * puts them on the wire, and the display on the other end scales them to
 * whatever a cell is there. Without it the picture crosses as METADATA only
 * and the pane draws blank — a surface whose whole content is the picture then
 * shows nothing at all.
 */
static int sprite_bits(const void *pix, const uint32_t **argb, int *w, int *h,
		       int *stride_px, void *user)
{
	pixman_image_t *img = (pixman_image_t *)pix;

	(void)user;
	if (!img)
		return -1;
	*argb = pixman_image_get_data(img);
	*w = pixman_image_get_width(img);
	*h = pixman_image_get_height(img);
	*stride_px = pixman_image_get_stride(img) / 4;
	return *argb && *w > 0 && *h > 0 ? 0 : -1;
}

static KimgBudget budget(void)
{
	KimgBudget b = { PK_MAX_W, PK_MAX_H, PK_MAX_PIX };

	return b;
}

/* Something rather than nothing where there are no pixels — a tty, a dump, a
 * view with no pixel library. A picture that rendered as blank cells cannot be
 * told apart from one that failed to arrive. */
static uint32_t fallback_cp(void)
{
	return (ktui_caps & KT_CAP_UTF8) ? 0x2593u : (uint32_t)'#';
}

static uint64_t hash_bytes(const void *p, size_t n, int cw, int ch)
{
	const unsigned char *b = p;
	uint64_t h = 0xcbf29ce484222325ULL;

	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3ULL;
	}
	h ^= (uint64_t)cw << 32 | (uint64_t)ch;
	h *= 0x100000001b3ULL;
	return h;
}

/* The whole file, or NULL. Bounded, because the caller has already decided it
 * is a picture and a picture is read into memory entire. */
static unsigned char *slurp(const char *p, size_t *len)
{
	struct stat st;
	unsigned char *b;
	FILE *f = fopen(p, "rb");

	if (!f)
		return NULL;
	if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode) ||
	    (unsigned long long)st.st_size > PK_MAX_FILE) {
		fclose(f);
		return NULL;
	}
	b = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
	if (!b) {
		fclose(f);
		return NULL;
	}
	*len = fread(b, 1, (size_t)st.st_size, f);
	fclose(f);
	return b;
}

/* Give the table back whatever grid is registered, if any. */
static void tiles_drop(void)
{
	if (pic_cw > 0)
		ktui_sprite_drop_tiled(pic_key, pic_cw, pic_ch);
	pic_cw = pic_ch = 0;
}

static void pic_free(void)
{
	tiles_drop();
	if (pic)
		pixman_image_unref(pic);
	pic = NULL;
}

/* Decode `n` bytes into the picture this surface shows. The key is over the
 * BYTES, so the same page rendered twice reuses its slots. */
static int pic_set(const unsigned char *b, size_t n)
{
	KimgBudget bud = budget();

	/*
	 * THE SOURCE IS REPLACED HERE; THE TILES ARE NOT. They are given back
	 * only once the next page's are registered — see tiles_for(). Each
	 * tile is an image of its own that the table owns, so unreffing this
	 * one takes none of them with it.
	 */
	if (pic)
		pixman_image_unref(pic);
	pic = kimg_decode(b, n, KIMG_AUTO, &bud);
	if (!pic) {
		tiles_drop();
		return -1;
	}
	pic_w = pixman_image_get_width(pic);
	pic_h = pixman_image_get_height(pic);
	/* The WHOLE buffer: two pages of a scan can share their first
	 * kilobytes, and a key that collided would draw the previous page. */
	pic_id = hash_bytes(b, n, pic_w, pic_h);
	pic_key = pic_id;
	return 0;
}

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
	b = slurp(tmp, &n);
	/* The temporary is this call's and nothing else reads it. */
	unlink(tmp);
	if (!b)
		return -1;
	rc = pic_set(b, n);
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

/*
 * Fit the decoded picture to the pane in CELLS, never enlarging it: a 32-pixel
 * icon blown up to a window is a blur of what the file actually holds. A cell
 * is not square, so the two axes are converted through the cell's pixel size
 * rather than compared directly.
 */
static void fit_cells(int pane_w, int pane_h, int *cw, int *ch)
{
	int cellw = cell_w();
	int cellh = cell_h();
	long long maxw = (long long)pane_w * cellw;
	long long maxh = (long long)pane_h * cellh;
	long long dw = pic_w, dh = pic_h;

	if (dw <= 0 || dh <= 0) {
		*cw = *ch = 0;
		return;
	}
	if (dw > maxw) {
		dh = dh * maxw / dw;
		dw = maxw;
	}
	if (dh > maxh) {
		dw = dw * maxh / dh;
		dh = maxh;
	}
	*cw = (int)((dw + cellw - 1) / cellw);
	*ch = (int)((dh + cellh - 1) / cellh);
	if (*cw < 1)
		*cw = 1;
	if (*ch < 1)
		*ch = 1;
	if (*cw > pane_w)
		*cw = pane_w;
	if (*ch > pane_h)
		*ch = pane_h;
}

/*
 * Register the tiles for the current picture at the current pane size, once
 * per size rather than once per frame: the slots are keyed by content and a
 * re-put of the same key would still rescale the picture every draw.
 */
static void tiles_for(int cw, int ch)
{
	uint64_t key = pic_id ^ ((uint64_t)cw << 40) ^ ((uint64_t)ch << 24);
	uint64_t okey = pic_key;
	int ocw = pic_cw, och = pic_ch;

	if (!pic || cw < 1 || ch < 1)
		return;
	if (pic_cw == cw && pic_ch == ch && pic_key == key)
		return;

	/*
	 * THE NEW GRID IS REGISTERED BEFORE THE OLD ONE IS GIVEN BACK, and
	 * both halves of that matter. The table hands a freed slot straight
	 * out again, so dropping first lets the next page take the same slot
	 * numbers — the cells then encode what they already encoded, the row
	 * diff sees nothing, and the screen keeps the previous page. The
	 * allocator does the same with the freed tiles' memory, so the picture
	 * behind a slot comes back as a pointer the display has already been
	 * sent and the pixels are never sent again either.
	 */
	if (kcell_tile_picture(pic, key, cw, ch, cell_w(), cell_h(),
			       fallback_cp()) > 0) {
		pic_key = key;
		pic_cw = cw;
		pic_ch = ch;
		if (ocw > 0 && okey != key)
			ktui_sprite_drop_tiled(okey, ocw, och);
	} else {
		/* Nothing rather than half a picture: the pane says so. */
		tiles_drop();
	}
}

static void draw_picture(void)
{
	int px, py, pw, ph, cw = 0, ch = 0;

	pane_cells(&px, &py, &pw, &ph);
	fit_cells(pw, ph, &cw, &ch);
	tiles_for(cw, ch);
	if (pic_cw < 1) {
		const char *msg = "no pixels on this display";

		ktui_draw_text(px + (pw - (int)strlen(msg)) / 2, py + ph / 2,
			       pw, msg, KT_MID, KT_SURFACE, KT_A_NONE);
		return;
	}
	{
		int x0 = px + (pw - pic_cw) / 2;
		int y0 = py + (ph - pic_ch) / 2;

		/* Tile by tile, because the table registers a picture as a
		 * GRID of sprites and each one is drawn at its own origin. */
		for (int ty = 0; ty < pic_ch; ty += 16)
			for (int tx = 0; tx < pic_cw; tx += 16) {
				int slot = ktui_sprite_tile_at(pic_key, pic_cw,
							      tx, ty, NULL,
							      NULL);
				int tw = pic_cw - tx, th = pic_ch - ty;

				if (slot < 0)
					continue;
				if (tw > 16)
					tw = 16;
				if (th > 16)
					th = 16;
				ktui_draw_sprite(krect(x0 + tx, y0 + ty, tw,
						       th),
						 slot, KT_TEXT, KT_SURFACE);
			}
	}
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
		snprintf(title, sizeof(title), "%s — %dx%d", base, pic_w,
			 pic_h);
	else
		snprintf(title, sizeof(title), "%s", base);

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 1);

	if (kind == PK_ARCHIVE)
		draw_archive();
	else if (pic)
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

	if (looks_like_image(head, n)) {
		unsigned char *b;
		size_t len = 0;

		kind = PK_IMAGE;
		b = slurp(path, &len);
		if (b) {
			if (pic_set(b, len) != 0)
				snprintf(note, sizeof(note),
					 "this picture could not be decoded");
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
	/* AFTER kdisp_init, never before: the console backend clears its whole
	 * client state when it connects, so a callback registered earlier is
	 * erased and every picture crosses as metadata the session maps to
	 * nothing — which draws as blank cells, not as the fallback. */
	kcon_set_sprite_bits(sprite_bits, NULL);
	/* The evictor and the budget together: the table holds a borrowed
	 * pointer, so without an evictor every page turn leaks a picture and
	 * the table fills. */
	ktui_sprite_evictor(kcell_tile_free, NULL);
	ktui_sprite_budget(PK_SPRITE_BUDGET, cell_w(), cell_h());

	if (kind == PK_DOC) {
		int px, py, pw, ph;

		pane_cells(&px, &py, &pw, &ph);
		doc_render(pw * cell_w(), ph * cell_h());
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
					doc_render(pw * cell_w(),
						   ph * cell_h());
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
			if (doc_render(pw * cell_w(), ph * cell_h()) != 0 &&
			    page > 1 &&
			    npages <= 0)
				/* Past the end of a document whose length is
				 * unknown: the failed render IS the answer. */
				page--;
		}
	}

	pic_free();
	free(ents);
	kdisp_shutdown();
	return 0;
}
