/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tip — the tooltip, as a surface
 *
 *   ╭──────────────────────────────╮
 *   │ Network                      │
 *   │ eth0 — down 1 kB/s, up 10 kB │
 *   ╰──────────────────────────────╯
 *
 * HALF THE BAR IS PICTURES WITH NO WORDS. The notification area is a row of
 * 32-pixel icons with a number under each; the quick-launch row is icons and
 * nothing else; a window button in icon mode is a picture and a state marker.
 * Every one of those is a control, and the only way to find out what any of
 * them was for was to click it and see what happened. That is exactly what a
 * tooltip is for, and this desktop had none.
 *
 * WHY A PROCESS. libktui has ONE cell buffer per process, so a second surface
 * is a second program — the same reason the panel's menus, popups and the
 * calendar are all separate binaries. The panel spawns this after a dwell and
 * SIGTERMs it when the pointer moves on; at 700 ms of stillness that is one
 * fork per thing somebody actually looked at.
 *
 * IT TAKES NO INPUT AT ALL. `kwl_input_cells(NULL, 0)` is an EMPTY input
 * region, so every click, every wheel notch and every motion goes straight
 * through to whatever is underneath — which for a tooltip anchored over the
 * panel is the panel. A tip that ate the click that was aimed at the thing it
 * describes would be worse than no tip; the volume bezel learned this the hard
 * way and it is the same rule here.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcell.h"
#include "kcolor.h"
#include "kwl.h"
#include "shell.h"

/* ── the preview ───────────────────────────────────────────────────────── */

/*
 * A WINDOW'S OWN PICTURE, AS CHARACTERS.
 *
 * Every taskbar since Windows 7 shows the window when you hover its button,
 * and the reason is the case a name cannot answer: three terminals called
 * `foot`, or eleven browser windows. A thumbnail says which one.
 *
 * IT IS A GRID OF CELLS LIKE EVERYTHING ELSE HERE — a block per cell in four
 * levels of the accent, which is the whole of the rendering and is described
 * where the levels are. The tip draws at every glyph tier.
 *
 * THE COMPOSITOR TAKES THE PICTURE, and it must: a client cannot see another
 * client's buffer, which is the whole point of Wayland. `kdos-comp`'s `thumb`
 * verb writes one PPM to a path of ITS choosing in $XDG_RUNTIME_DIR — see
 * kdos-thumb.c — and the answer for a window whose pixels are not host-readable
 * is "no", which on real hardware with a dmabuf client is the common case.
 *
 * SO THE PREVIEW IS NEVER REQUIRED. No compositor, no socket, no buffer, no
 * font, a PPM that does not parse — every one of them leaves the tip exactly
 * the two lines of text it was already going to be.
 */
#define PV_COLS 34
#define PV_ROWS 9
#define PV_MAX  (PV_COLS * PV_ROWS)

static uint32_t pv_cp[PV_MAX];
static uint32_t pv_tint[PV_MAX];
static int pv_cols, pv_rows;

/*
 * IT IS A GAME BOY SCREEN: FOUR SOLID LEVELS, AND THE COLOUR CARRIES THE
 * PICTURE.
 *
 * `kcell_ascii_image` is the wrong renderer at this size and the reason is
 * arithmetic rather than taste. It picks a glyph by SHAPE, and a 1280-pixel
 * window sampled into a 34x9 grid is nine source pixels per cell: a cell of
 * text is a featureless grey blur with no shape left in it to match. So every
 * textured cell matched the same glyph and a terminal previewed as a wall of
 * `b`, a minimised one as a wall of `|` — photographed on the booted ISO. The
 * shape matcher keeps its other callers (`Super+A`, `kdos-shot --text`, the
 * camera preview), which run at grids where a cell still holds a shape.
 *
 * Every cell here is a FULL BLOCK instead, drawn in one of four slots off the
 * palette's own brightness ladder. That is a 2-bit LCD: chunky, no stipple
 * texture pretending to be detail, and it wears whatever accent the desktop
 * is in, because the ladder is the accent's. It reads at every glyph tier —
 * the console font has FULL BLOCK, and eight colours is more than four.
 */
#define PV_LEVELS 4
static const uint8_t pv_slot[PV_LEVELS] = {
	KT_SURFACE, KT_DIM, KT_MID, KT_ACCENT
};

/* The glyph table is UTF-8 strings; a cell holds a codepoint. */
static uint32_t pv_block(void)
{
	uint32_t cp = 0;

	ktui_utf8_next(ktui_glyph[KT_G_FULL], &cp);
	return cp;
}

/* A binary P6 with 8-bit channels — the only thing kdos-thumb.c writes, so
 * this reads that and refuses everything else rather than growing a parser for
 * a format nobody here produces. */
static uint32_t *ppm_read(const char *path, int *w, int *h)
{
	FILE *f = fopen(path, "rb");
	int vals[3] = { 0, 0, 0 }, got = 0, c;
	uint32_t *argb = NULL;
	uint8_t *row = NULL;

	if (!f)
		return NULL;
	if (fgetc(f) != 'P' || fgetc(f) != '6')
		goto out;
	while (got < 3) {
		c = fgetc(f);
		if (c == EOF)
			goto out;
		if (c == '#') {
			while ((c = fgetc(f)) != EOF && c != '\n')
				;
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;
		if (c < '0' || c > '9')
			goto out;
		vals[got] = 0;
		do {
			vals[got] = vals[got] * 10 + (c - '0');
			c = fgetc(f);
		} while (c >= '0' && c <= '9');
		got++;
	}
	/* One whitespace byte after the maxval, then the pixels. */
	if (vals[0] < 1 || vals[1] < 1 || vals[0] > 4096 || vals[1] > 4096 ||
	    vals[2] != 255)
		goto out;
	argb = malloc((size_t)vals[0] * vals[1] * 4);
	row = malloc((size_t)vals[0] * 3);
	if (!argb || !row) {
		free(argb);
		argb = NULL;
		goto out;
	}
	for (int y = 0; y < vals[1]; y++) {
		if (fread(row, 1, (size_t)vals[0] * 3, f) !=
		    (size_t)vals[0] * 3) {
			free(argb);
			argb = NULL;
			goto out;
		}
		for (int x = 0; x < vals[0]; x++)
			argb[(size_t)y * vals[0] + x] =
				0xff000000u | ((uint32_t)row[x * 3] << 16) |
				((uint32_t)row[x * 3 + 1] << 8) |
				(uint32_t)row[x * 3 + 2];
	}
	*w = vals[0];
	*h = vals[1];
out:
	free(row);
	fclose(f);
	return argb;
}

static void preview_take(const char *app_id)
{
	char req[256], rep[512], path[400];
	const char *p, *e;
	uint32_t *argb;
	int w = 0, h = 0, cw, ch;

	pv_cols = pv_rows = 0;
	if (!app_id || !*app_id || strchr(app_id, '"'))
		return;
	/*
	 * The pixel size asked for is the CELL grid the tip will draw, times a
	 * sampling factor: the renderer wants several pixels per cell to have
	 * anything to average, and a cell is twice as tall as it is wide so
	 * the grid must be about twice as wide as it is tall or the window
	 * comes out stretched — the same constraint genlogo.py keeps for the
	 * mascot and devices.c for the camera.
	 */
	snprintf(req, sizeof(req),
		 "{\"cmd\":\"thumb\",\"app_id\":\"%s\",\"w\":%d,\"h\":%d}\n",
		 app_id, PV_COLS * 4, PV_ROWS * 8);
	if (sh_cmd_call(req, rep, sizeof(rep), NULL, 0) != 0)
		return;
	p = strstr(rep, "\"path\":\"");
	if (!p)
		return;
	p += 8;
	e = strchr(p, '"');
	if (!e || (size_t)(e - p) >= sizeof(path))
		return;
	memcpy(path, p, (size_t)(e - p));
	path[e - p] = '\0';

	argb = ppm_read(path, &w, &h);
	if (!argb)
		return;
	cw = w / PV_COLS;
	ch = h / PV_ROWS;
	if (cw < 1)
		cw = 1;
	if (ch < 1)
		ch = 1;
	pv_cols = w / cw;
	pv_rows = h / ch;
	if (pv_cols > PV_COLS)
		pv_cols = PV_COLS;
	if (pv_rows > PV_ROWS)
		pv_rows = PV_ROWS;
	{
		uint32_t peak = 0, blk = pv_block();

		/*
		 * The mean of the block, which is the whole of the rendering.
		 * Every cell is a FULL BLOCK and only its colour carries the
		 * picture, so nothing here has to decide what shape a cell
		 * resembles — see the note above tip_draw().
		 */
		for (int r = 0; r < pv_rows; r++) {
			for (int c = 0; c < pv_cols; c++) {
				uint32_t sum = 0;
				int n = 0;

				for (int y = r * ch; y < (r + 1) * ch; y++)
					for (int x = c * cw; x < (c + 1) * cw;
					     x++) {
						sum += kcol_lum(
							argb[(size_t)y * w + x]
							& 0xffffff);
						n++;
					}
				sum = n ? sum / (uint32_t)n : 0;
				pv_tint[(size_t)r * pv_cols + c] = sum;
				if (sum > peak)
					peak = sum;
			}
		}
		/*
		 * NORMALISED AGAINST THIS PICTURE'S OWN BRIGHTEST CELL, AND
		 * ON A SQUARE-ROOT CURVE.
		 *
		 * Both halves are needed. Nine source pixels average into one,
		 * so even a lit cell of a terminal comes back at a few percent
		 * of full and an absolute scale puts every window on the
		 * darkest level — a blank rectangle. But a LINEAR scale
		 * against the peak is barely better: a window's brightest
		 * element is usually its titlebar, and at four levels
		 * everything an order of magnitude below it lands on level 0
		 * too. Measured on a terminal running `top`: the accent
		 * titlebar sampled around 200 and the text rows around 15, so
		 * the whole of the content the preview exists to show
		 * disappeared under the one bar.
		 *
		 * A square root is the usual answer and it is enough here —
		 * 15/200 lifts from 0.07 to 0.27, which is level 1 rather than
		 * level 0. No libm: `lv = isqrt(v * L * L / peak)` is the same
		 * curve in integers.
		 */
		for (int i = 0; i < pv_cols * pv_rows; i++) {
			uint32_t sq = peak ? pv_tint[i] * PV_LEVELS * PV_LEVELS
						     / (peak + 1)
					   : 0;
			uint32_t lv = 0;

			while ((lv + 1) * (lv + 1) <= sq)
				lv++;
			if (lv >= PV_LEVELS)
				lv = PV_LEVELS - 1;
			pv_tint[i] = lv;
			pv_cp[i] = blk;
		}
	}
	free(argb);
}

/*
 * SIXTY-FOUR COLUMNS, and it is a measurement rather than a taste. At 44 the
 * detail line of every tip that lists what the three buttons do came out cut
 * mid-word — `right-cl`, `mid` — photographed on the booted ISO. Sixty-four is
 * eighty percent of the shipped 1280x800 bar, which is as wide as a label over
 * a panel control can be before it stops being a label; place_clamp pulls it
 * back from the right edge, so a tip on the clock is not pushed off screen.
 */
#define TIP_MAX 64		/* display columns of text; the rest is clipped */

/* At most TIP_MAX columns, and say so when there was more. */
static void tip_trunc(char *dst, size_t n, const char *src)
{
	if (!*src || ktui_utf8_width(src) <= TIP_MAX) {
		snprintf(dst, n, "%s", src);
		return;
	}
	sh_utf8_trunc(dst, n - 4, src, TIP_MAX - 1);
	strncat(dst, "\xe2\x80\xa6", n - strlen(dst) - 1);
}

/* The tip's own lifetime is measured against this and nothing else. */
static int64_t tip_now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


/*
 * THE SAME BODY EVERY OTHER POPUP WEARS, and not a framed box: a border would
 * cost two of the four rows a two-line tip has, on a surface that is never
 * more than two cells tall. KT_DIM is the slot the backdrop owns here — a
 * tooltip's plate is the fill colour, where a menu's is the page — so the
 * gradient shows through wherever the tip is not drawing a glyph and the text
 * on it is KT_TEXT at better than 4:1.
 */
static void tip_draw(const char *t1, const char *t2)
{
	int y = 0;

	ktui_draw_fill(krect(0, 0, ktui_w, ktui_h), KT_DIM);
	if (pv_cols && pv_rows) {
		int x0 = (ktui_w - pv_cols) / 2;

		if (x0 < 0)
			x0 = 0;
		for (int r = 0; r < pv_rows && r < ktui_h; r++)
			for (int c = 0; c < pv_cols && x0 + c < ktui_w; c++) {
				size_t i = (size_t)r * pv_cols + c;
				/*
				 * THE PALETTE'S LADDER, NOT THE WINDOW'S OWN
				 * COLOURS. A full-colour rectangle inside a
				 * phosphor tooltip is the one thing on the
				 * screen that would not belong to it — the
				 * rule the camera preview already keeps — so
				 * the picture is rendered in the accent's four
				 * steps and comes out looking like the machine
				 * it is running on.
				 */
				ktui_draw_cell(x0 + c, r, pv_cp[i],
					       pv_slot[pv_tint[i] % PV_LEVELS],
					       KT_DIM, KT_A_NONE);
			}
		y = pv_rows;
	}
	ktui_draw_text(1, y, ktui_w - 1, t1, KT_TEXT, KT_DIM, KT_A_NONE);
	if (t2[0] && ktui_h > y + 1)
		ktui_draw_text(1, y + 1, ktui_w - 1, t2, KT_MID, KT_DIM,
			       KT_A_NONE);
}

int tip_main(int argc, char **argv)
{
	const char *font = NULL, *title = NULL, *detail = NULL;
	const char *preview = NULL;
	int at_x = -1, at_y = 0, at_bottom = 0, ms = 6000, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--ms") && i + 1 < argc) {
			ms = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--preview") && i + 1 < argc) {
			preview = argv[++i];
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else if (argv[i][0] == '-' && argv[i][1] == '-') {
			fprintf(stderr, "usage: kdos-tip [--at X Y] "
					"[--at-bottom X Y] [--ms N] "
					"[--font NAME] [--preview APP_ID] "
					"TEXT [DETAIL]\n");
			return 2;
		} else if (!title) {
			title = argv[i];
		} else if (!detail) {
			detail = argv[i];
		}
	}
	if (!title || !*title)
		return 2;

	char t1[192], t2[192];

	/* Cut with an ellipsis: a hint that ends mid-word — `right-click to` —
	 * reads as a rendering fault rather than as a line that was too long,
	 * which is the same reason every row in the Start menu wears one. */
	tip_trunc(t1, sizeof(t1), title);
	tip_trunc(t2, sizeof(t2), detail ? detail : "");

	int w1 = ktui_utf8_width(t1), w2 = ktui_utf8_width(t2);
	int cols = (w1 > w2 ? w1 : w2) + 2;
	int rows = t2[0] ? 2 : 1;



	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(cols, rows);
		tip_draw(t1, t2);
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = cols,
		.rows = rows,
		.corner = at_x < 0	? KWL_CORNER_CENTER
			  : at_bottom	? KWL_CORNER_BOTTOM_LEFT
					: KWL_CORNER_TOP_LEFT,
		.margin_x = at_x < 0 ? 0 : at_x,
		.margin_y = at_x < 0 ? 0 : at_y,
		.app_id = "kdos-tip",
		.font = font,
		/* No keyboard and no dismiss-on-unfocus: this surface never has
		 * the focus to lose, and taking one would pull it off whatever
		 * the pointer is hovering over. */
		.keyboard = 0,
	};

	sh_theme_from_cache();
	/*
	 * SAYS SO WHEN IT CANNOT START. Every other front end here prints; this
	 * one returned 1 in silence, and the panel does not read its child's
	 * exit status — so a tooltip that could not bring up a surface was a
	 * hover that did nothing, with no evidence anywhere that it had even
	 * been tried.
	 */
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-tip: no compositor, no layer-shell or no "
				"font\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_DIM);

	/*
	 * THE PICTURE IS TAKEN AFTER kwl_init, AND THE SURFACE IS THEN GROWN.
	 *
	 * The tip does not know how tall it is until it knows whether there is
	 * a thumbnail to put in it, and it cannot resize a surface that does
	 * not exist yet. So the order is init, capture, resize, draw. A capture
	 * that came back with nothing skips the resize and the tip is the two
	 * lines of text it was always going to be.
	 */
	if (preview) {
		preview_take(preview);
		if (pv_cols && pv_rows) {
			int want_c = pv_cols + 2 > cols ? pv_cols + 2 : cols;

			/*
			 * AND THE PICTURE IS DROPPED IF THE SURFACE WILL NOT
			 * GROW. A tip that drew a nine-row thumbnail into the
			 * two-row surface it already had would attach a buffer
			 * the compositor never configured, which is a protocol
			 * error and disconnects the client — no window, no
			 * message. The words are what the tooltip is for; the
			 * picture is the enhancement, and this is the same rule
			 * every icon on this desktop keeps.
			 */
			if (kwl_overlay_resize(want_c, rows + pv_rows) == 0) {
				cols = want_c;
				rows += pv_rows;
				ktui_draw_resize();
				ktui_draw_invalidate();
			} else {
				pv_cols = pv_rows = 0;
			}
		}
	}

	tip_draw(t1, t2);
	ktui_draw_flush();

	/*
	 * TRANSPARENT TO THE POINTER — see the head — AND SET AFTER THE FIRST
	 * BUFFER, NOT BEFORE IT.
	 *
	 * An input region is a surface state change and takes effect on a
	 * commit; libkwl issues that commit itself, because a surface whose
	 * content has not changed is deliberately not committed by the flush.
	 * Before the first buffer that commit is an UNMAP, and a layer surface
	 * wlroots has reset to uninitialised waits for a handshake that will
	 * never come — the program then paints for its whole life into a
	 * surface nobody can see.
	 */
	kwl_input_cells(NULL, 0);

	if (getenv("KDOS_PANEL_DEBUG"))
		fprintf(stderr, "kdos-tip: %dx%d cells at %d,%d%s\n", ktui_w,
			ktui_h, at_x, at_y, at_bottom ? " (bottom)" : "");

	/*
	 * IT DIES ON ITS OWN. The panel SIGTERMs it when the pointer moves, and
	 * the timeout is the backstop for the case that cannot be signalled: a
	 * panel that was killed, or a pointer that left the screen entirely.
	 * A tooltip stuck on the desktop forever is the failure everybody who
	 * has written one has shipped once.
	 *
	 * THE DEADLINE IS A CLOCK, NOT A TALLY OF POLL TIMEOUTS, and getting
	 * that wrong is why hovering the bar produced a process and never a
	 * tooltip.
	 *
	 * `poll_event` returns as soon as the display has ANYTHING to say, and
	 * a mapped surface has a frame callback arriving every vblank — none of
	 * which is a KtuiEvent, so each one came back 0 and the old loop
	 * subtracted a FULL 500 ms slice for a wait that had lasted sixteen.
	 * Six seconds of budget was spent in twelve frames: the tip flashed for
	 * about a fifth of a second and exited 0, which from the outside is a
	 * hover that did nothing at all.
	 */
	int64_t end = tip_now_ms() + ms;

	while (!kwl_should_close()) {
		KtuiEvent ev;
		int64_t left = end - tip_now_ms();
		int slice = left > 500 ? 500 : (int)left;

		if (left <= 0)
			break;
		ktui_backend()->poll_event(&ev, slice);
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
			tip_draw(t1, t2);
			ktui_draw_flush();
		}
	}
	kwl_shutdown();
	return 0;
}
