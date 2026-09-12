/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-pix — one picture, and the folder it is in
 *
 *   ╔═ photo.jpg — 2 of 37 — 4032x3024 — 100% ═════════════╗
 *   ║                                                      ║
 *   ║                    (the picture)                     ║
 *   ║                                                      ║
 *   ╟──────────────────────────────────────────────────────╢
 *   ║ +/- zoom  0 fit  Space next  Bksp back  Esc Close    ║
 *   ╚══════════════════════════════════════════════════════╝
 *
 * THE FOLDER IS THE ALBUM. Opening one picture opens the sorted list of every
 * picture beside it, so `Space` is the next photograph rather than an error —
 * which is what every viewer since ACDSee has done and the one thing `kdos-peek`
 * deliberately does not, because a quick look is about the file somebody named.
 *
 * ZOOM IS A SOURCE RECTANGLE, NOT A SCALED SPRITE. The window keeps a factor
 * and a centre, and what is registered is the crop those two describe scaled to
 * the pane — so zooming in reads MORE of the original's pixels rather than
 * enlarging the ones already on the screen. That is the whole difference
 * between a viewer and a magnifying glass over a thumbnail.
 *
 * FIT NEVER ENLARGES. A 32-pixel icon opened here is 32 pixels; `+` is how a
 * person asks for it bigger, and a viewer that guessed would show every icon on
 * the machine as a blur.
 * ---------------------------------
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define PX_COLS 96
#define PX_ROWS 32
/* A folder listing stops here; a photograph directory is hundreds, not
 * millions, and the cap is what keeps a mistaken argument bounded. */
#define PX_MAX_FILES 4096

static char dir[PATH_MAX];
static char (*files)[256];
static int nfiles, cur;

static ShPic pic;
static int have_pic;
static char note[192];

/*
 * The zoom, as a percentage of the source's own pixels, and where in the source
 * the middle of the pane is. Zero means FIT, which is not a percentage — it
 * depends on the window and changes when the window does.
 */
static int zoom;		/* 0 = fit, else per cent */
static int cx, cy;		/* the centre, in source pixels */

static KtuiKeys keys;

/* ── the folder ────────────────────────────────────────────────────────── */

static int is_picture_name(const char *n)
{
	static const char *const EXT[] = { ".png", ".jpg", ".jpeg", ".webp",
					   NULL };
	const char *dot = strrchr(n, '.');

	if (!dot)
		return 0;
	for (int i = 0; EXT[i]; i++)
		if (!strcasecmp(dot, EXT[i]))
			return 1;
	return 0;
}

static int cmp_name(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

/*
 * Every picture beside this one, sorted by name. By EXTENSION rather than by
 * magic: this reads a directory that may hold thousands of files, and opening
 * each one to sniff it would be the slowest part of starting a viewer.
 */
static void scan_dir(const char *want)
{
	DIR *d = opendir(dir);
	struct dirent *e;

	if (!d)
		return;
	files = calloc(PX_MAX_FILES, sizeof(files[0]));
	if (!files) {
		closedir(d);
		return;
	}
	while (nfiles < PX_MAX_FILES && (e = readdir(d))) {
		if (e->d_name[0] == '.' || !is_picture_name(e->d_name))
			continue;
		snprintf(files[nfiles], sizeof(files[0]), "%s", e->d_name);
		nfiles++;
	}
	closedir(d);
	qsort(files, (size_t)nfiles, sizeof(files[0]), cmp_name);
	for (int i = 0; i < nfiles; i++)
		if (!strcmp(files[i], want)) {
			cur = i;
			return;
		}
	/* The named file is not in the list — an extension nothing here reads,
	 * or a name that changed under us. It is still the one to show, so it
	 * becomes a list of one. */
	snprintf(files[0], sizeof(files[0]), "%s", want);
	nfiles = 1;
	cur = 0;
}

static void load_cur(void)
{
	char full[PATH_MAX];

	note[0] = '\0';
	if (cur < 0 || cur >= nfiles)
		return;
	if (snprintf(full, sizeof(full), "%s/%s", dir, files[cur]) >=
	    (int)sizeof(full)) {
		snprintf(note, sizeof(note), "that path is too long");
		return;
	}
	have_pic = sh_pic_load(&pic, full) == 0;
	if (!have_pic) {
		snprintf(note, sizeof(note), "%s could not be decoded",
			 files[cur]);
		return;
	}
	/* A new picture is shown whole: carrying the last one's zoom would put
	 * somebody at 400% in the corner of a photograph they have not seen. */
	zoom = 0;
	cx = pic.w / 2;
	cy = pic.h / 2;
}

static void step(int by)
{
	if (nfiles < 2)
		return;
	cur = (cur + by + nfiles) % nfiles;
	load_cur();
}

/* ── the frame ─────────────────────────────────────────────────────────── */

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

/* What percentage `fit` currently is, for the title. */
static int fit_percent(int pane_w, int pane_h)
{
	int cw = 0, ch = 0;

	sh_pic_fit(pic.w, pic.h, pane_w, pane_h, &cw, &ch);
	if (pic.w <= 0)
		return 100;
	return (int)((long long)cw * sh_pic_cell_w() * 100 / pic.w);
}

/*
 * The source rectangle the pane shows, from the zoom and the centre. At `fit`
 * it is the whole picture; at a percentage it is as many source pixels as the
 * pane can hold at that scale, clamped inside the image — so panning stops at
 * the edge rather than sliding the picture off the screen.
 */
static void source_rect(int pane_w, int pane_h, int *sx, int *sy, int *sw,
			int *sh)
{
	long long px = (long long)pane_w * sh_pic_cell_w();
	long long py = (long long)pane_h * sh_pic_cell_h();

	if (!zoom) {
		*sx = 0;
		*sy = 0;
		*sw = pic.w;
		*sh = pic.h;
		return;
	}
	*sw = (int)(px * 100 / zoom);
	*sh = (int)(py * 100 / zoom);
	if (*sw > pic.w)
		*sw = pic.w;
	if (*sh > pic.h)
		*sh = pic.h;
	if (*sw < 1)
		*sw = 1;
	if (*sh < 1)
		*sh = 1;
	*sx = cx - *sw / 2;
	*sy = cy - *sh / 2;
	if (*sx < 0)
		*sx = 0;
	if (*sy < 0)
		*sy = 0;
	if (*sx + *sw > pic.w)
		*sx = pic.w - *sw;
	if (*sy + *sh > pic.h)
		*sy = pic.h - *sh;
}

/* A pan is a fraction of what is on the screen, not a fixed number of pixels:
 * one press moves a tenth of the view at every zoom. */
static void pan(int dx, int dy)
{
	int px, py, pw, ph, sx, sy, sw, sh;

	if (!zoom)
		return;
	pane_cells(&px, &py, &pw, &ph);
	source_rect(pw, ph, &sx, &sy, &sw, &sh);
	cx += dx * (sw / 10 > 1 ? sw / 10 : 1);
	cy += dy * (sh / 10 > 1 ? sh / 10 : 1);
	if (cx < 0)
		cx = 0;
	if (cy < 0)
		cy = 0;
	if (cx > pic.w)
		cx = pic.w;
	if (cy > pic.h)
		cy = pic.h;
}

static void zoom_by(int step_pct)
{
	int px, py, pw, ph;

	if (!have_pic)
		return;
	pane_cells(&px, &py, &pw, &ph);
	if (!zoom)
		zoom = fit_percent(pw, ph);
	zoom += step_pct;
	/* A ceiling and a floor, both stated: past 800% a cell is a colour and
	 * below 5% the picture is smaller than the border around it. */
	if (zoom > 800)
		zoom = 800;
	if (zoom < 5)
		zoom = 5;
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int px, py, pw, ph;
	char title[400];

	if (w < 24 || h < 8)
		return;
	pane_cells(&px, &py, &pw, &ph);

	if (have_pic) {
		int pct = zoom ? zoom : fit_percent(pw, ph);

		if (nfiles > 1)
			snprintf(title, sizeof(title),
				 "%s — %d of %d — %dx%d — %d%%", files[cur],
				 cur + 1, nfiles, pic.w, pic.h, pct);
		else
			snprintf(title, sizeof(title), "%s — %dx%d — %d%%",
				 files[cur], pic.w, pic.h, pct);
	} else {
		snprintf(title, sizeof(title), "%s",
			 nfiles ? files[cur] : "no picture");
	}

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 1);

	if (have_pic) {
		int sx, sy, sw, sh, cw = 0, ch = 0;

		source_rect(pw, ph, &sx, &sy, &sw, &sh);
		if (zoom) {
			/* `sw` source pixels are drawn at `zoom` per cent of
			 * themselves — the factor MULTIPLIES, and the inverse
			 * is what source_rect() already used to decide how
			 * many source pixels the pane can hold. */
			cw = ((long long)sw * zoom / 100 + sh_pic_cell_w() - 1) /
			     sh_pic_cell_w();
			ch = ((long long)sh * zoom / 100 + sh_pic_cell_h() - 1) /
			     sh_pic_cell_h();
			if (cw > pw)
				cw = pw;
			if (ch > ph)
				ch = ph;
			if (cw < 1)
				cw = 1;
			if (ch < 1)
				ch = 1;
		} else {
			sh_pic_fit(pic.w, pic.h, pw, ph, &cw, &ch);
		}
		sh_pic_view(&pic, sx, sy, sw, sh, cw, ch);
		if (pic.cw > 0)
			sh_pic_draw(&pic, px + (pw - pic.cw) / 2,
				    py + (ph - pic.ch) / 2);
		else {
			const char *msg = "no pixels on this display";

			ktui_draw_text(px + (pw - (int)strlen(msg)) / 2,
				       py + ph / 2, pw, msg, KT_MID,
				       KT_SURFACE, KT_A_NONE);
		}
	} else {
		const char *msg = note[0] ? note : "nothing to show";

		ktui_draw_text((w - (int)strlen(msg)) / 2, h / 2, w - 2, msg,
			       note[0] ? KT_WARN : KT_MID, KT_SURFACE,
			       KT_A_NONE);
	}

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	ktui_hint_if(have_pic, "+/-", "zoom");
	ktui_hint_if(have_pic && zoom, "0", "fit");
	ktui_hint_if(nfiles > 1, "Space", "next");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	ktui_draw_flush();
}

/* ── the program ───────────────────────────────────────────────────────── */

int pix_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *want = NULL;
	char full[PATH_MAX];
	char *slash;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (argv[i][0] == '-') {
			fprintf(stderr,
				"usage: kdos-pix [--font NAME] [--dump] FILE\n");
			return 2;
		} else if (!want) {
			want = argv[i];
		}
	}
	if (!want) {
		fprintf(stderr, "usage: kdos-pix [--font NAME] [--dump] FILE\n");
		return 2;
	}
	if (!realpath(want, full)) {
		fprintf(stderr, "kdos-pix: %s: no such file\n", want);
		return 1;
	}
	snprintf(dir, sizeof(dir), "%s", full);
	slash = strrchr(dir, '/');
	if (!slash) {
		fprintf(stderr, "kdos-pix: %s has no directory\n", full);
		return 1;
	}
	*slash = '\0';
	if (!dir[0])
		snprintf(dir, sizeof(dir), "/");
	scan_dir(slash + 1);
	if (!nfiles) {
		fprintf(stderr, "kdos-pix: %s: nothing to show\n", full);
		return 1;
	}
	load_cur();

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = PX_COLS,
		.rows = PX_ROWS,
		.app_id = "kdos-pix",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(PX_COLS, PX_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-pix: no display server\n");
		return 1;
	}
	ktui_draw_init();
	/* AFTER kdisp_init, never before — see picture.c. */
	sh_pic_backend();

	while (!kdisp_should_close()) {
		draw();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
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

		switch (ev.key) {
		case ' ':
		case KT_K_PGDN:
			step(1);
			break;
		case KT_K_BACKSPACE:
		case KT_K_PGUP:
			step(-1);
			break;
		case KT_K_HOME:
			cur = 0;
			load_cur();
			break;
		case KT_K_END:
			cur = nfiles - 1;
			load_cur();
			break;
		case '+':
		case '=':
			zoom_by(25);
			break;
		case '-':
		case '_':
			zoom_by(-25);
			break;
		case '0':
			zoom = 0;
			break;
		case KT_K_LEFT:
			pan(-1, 0);
			break;
		case KT_K_RIGHT:
			pan(1, 0);
			break;
		case KT_K_UP:
			pan(0, -1);
			break;
		case KT_K_DOWN:
			pan(0, 1);
			break;
		}
	}

	sh_pic_free(&pic);
	free(files);
	kdisp_shutdown();
	return 0;
}
