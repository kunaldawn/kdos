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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

/*
 * SIXTY-FOUR COLUMNS, and it is a measurement rather than a taste. At 44 the
 * detail line of every tip that lists what the three buttons do came out cut
 * mid-word — `right-cl`, `mid` — photographed on the booted ISO. Sixty-four is
 * eighty percent of the shipped 1280x800 bar, which is as wide as a label over
 * a panel control can be before it stops being a label; place_clamp pulls it
 * back from the right edge, so a tip on the clock is not pushed off screen.
 */
#define TIP_MAX 64		/* display columns of text; the rest is clipped */

/*
 * A FILLED BOX IN THE FILL COLOUR, not a framed one. `dim` is exactly what a
 * raised surface wants and the text on it is KT_TEXT at better than 4:1; a
 * border would cost two of the four rows a two-line tip has, on a surface that
 * is never more than two cells tall.
 */
static void tip_draw(const char *t1, const char *t2)
{
	ktui_draw_fill(krect(0, 0, ktui_w, ktui_h), KT_DIM);
	ktui_draw_text(1, 0, ktui_w - 1, t1, KT_TEXT, KT_DIM, KT_A_NONE);
	if (t2[0] && ktui_h > 1)
		ktui_draw_text(1, 1, ktui_w - 1, t2, KT_MID, KT_DIM, KT_A_NONE);
}

int tip_main(int argc, char **argv)
{
	const char *font = NULL, *title = NULL, *detail = NULL;
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
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else if (argv[i][0] == '-' && argv[i][1] == '-') {
			fprintf(stderr, "usage: kdos-tip [--at X Y] "
					"[--at-bottom X Y] [--ms N] "
					"[--font NAME] TEXT [DETAIL]\n");
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

	sh_utf8_trunc(t1, sizeof(t1), title, TIP_MAX);
	sh_utf8_trunc(t2, sizeof(t2), detail ? detail : "", TIP_MAX);

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
	if (kwl_init(&cfg) != 0)
		return 1;
	ktui_draw_init();
	/* Transparent to the pointer — see the head. */
	kwl_input_cells(NULL, 0);

	tip_draw(t1, t2);
	ktui_draw_flush();

	/*
	 * IT DIES ON ITS OWN. The panel SIGTERMs it when the pointer moves, and
	 * the timeout is the backstop for the case that cannot be signalled: a
	 * panel that was killed, or a pointer that left the screen entirely.
	 * A tooltip stuck on the desktop forever is the failure everybody who
	 * has written one has shipped once.
	 */
	int64_t left = ms;

	while (!kwl_should_close() && left > 0) {
		KtuiEvent ev;
		int slice = left > 500 ? 500 : (int)left;

		if (!ktui_backend()->poll_event(&ev, slice))
			left -= slice;
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
