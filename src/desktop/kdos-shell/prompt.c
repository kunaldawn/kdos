/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-prompt — yes or no, in cells
 *
 *   ╔═ KDOS ═══════════════════════════════╗
 *   ║  End the session?                    ║
 *   ║                                      ║
 *   ║          [ Cancel ]  [   End   ]     ║
 *   ╚══════════════════════════════════════╝
 *
 * THE ANSWER IS THE EXIT STATUS, because that is the interface kdos-comp
 * already has. labwc's `<action name="If"><prompt>` spawns `promptCommand` and
 * dispatches on what it exits with — 0 takes the `then` branch, 254 means the
 * user cancelled and nothing happens, anything else takes `else`. Upstream's
 * program for that is labnag, which is a GTK-free but pango-and-cairo dialog
 * we do not build (`-Dlabnag=disabled`), so the facility existed and had
 * nothing on the other end of it: rc.prompt_command still named a binary that
 * is not on the image.
 *
 * It is the reason Super+Escape can ask before it ends the session. A
 * compositor whose "quit" is one keystroke away from a full screen of unsaved
 * work is a compositor people learn not to trust.
 *
 * Keyboard and mouse both, like every other front end here: Left/Right or Tab
 * move, Enter takes the highlighted button, Y and N take theirs directly, Esc
 * cancels, and a click does what it looks like it does.
 * ---------------------------------
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kwl.h"
#include "shell.h"

/* kdos-comp's contract (include/action-prompt-codes.h in the fork). 254 is
 * "the user did not choose", which is not the same as choosing No. */
#define EXIT_YES	0
#define EXIT_NO		1
#define EXIT_CANCELLED	254

#define MAX_MSG   512
#define MAX_LINES 6
#define BTN_GAP   2

/*
 * Wrap on spaces into at most MAX_LINES of `w` cells. A message arrives from a
 * config file somebody else wrote and can be any length; truncating it to one
 * line is how a dialog ends up asking half a question.
 */
static int wrap(const char *s, int w, char out[MAX_LINES][MAX_MSG])
{
	int n = 0;
	const char *p = s;

	if (w < 8)
		w = 8;
	while (*p && n < MAX_LINES) {
		int used = 0, last_space = -1;
		const char *start = p;
		while (*p && used < w) {
			if (*p == ' ')
				last_space = (int)(p - start);
			/* Cells, not bytes: a continuation byte is not a
			 * column, and counting it as one makes a wrapped line
			 * of accented text end early. */
			if (((unsigned char)*p & 0xc0) != 0x80)
				used++;
			p++;
		}
		int len = (int)(p - start);
		if (*p && last_space > 0) {
			len = last_space;
			p = start + last_space + 1;
		}
		if (len > MAX_MSG - 1)
			len = MAX_MSG - 1;
		memcpy(out[n], start, (size_t)len);
		out[n][len] = '\0';
		n++;
	}
	return n ? n : 1;
}

int prompt_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *msg = "Are you sure?";
	const char *yes = "Yes", *no = "No";
	char lines[MAX_LINES][MAX_MSG];

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--message") && i + 1 < argc)
			msg = argv[++i];
		else if (!strcmp(argv[i], "--yes") && i + 1 < argc)
			yes = argv[++i];
		else if (!strcmp(argv[i], "--no") && i + 1 < argc)
			no = argv[++i];
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr,
				"usage: kdos-prompt --message TEXT "
				"[--yes LABEL] [--no LABEL] [--font NAME]\n"
				"exit: 0 yes, 1 no, 254 cancelled\n");
			return EXIT_CANCELLED;
		}
	}

	/* Sized to the question. Wide enough for the two buttons whatever the
	 * message is, capped so a pasted paragraph cannot become a dialog
	 * wider than the screen. */
	int bw = ktui_utf8_width(yes) + ktui_utf8_width(no) + 8 + BTN_GAP;
	int cols = ktui_utf8_width(msg) + 6;
	if (cols < bw + 4)
		cols = bw + 4;
	if (cols > 64)
		cols = 64;
	if (cols < 28)
		cols = 28;
	int nlines = wrap(msg, cols - 4, lines);
	int rows = nlines + 4;

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = cols,
		.rows = rows,
		.app_id = "kdos-prompt",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		/*
		 * No compositor is not an answer. Cancelled is the only honest
		 * status: taking the `then` branch here would mean a dialog
		 * that could not be drawn had agreed to end the session.
		 */
		fprintf(stderr, "kdos-prompt: no compositor or no layer-shell\n");
		return EXIT_CANCELLED;
	}
	ktui_draw_init();

	int sel = 0;			/* 0 = No, 1 = Yes — No is the default */
	int rc = EXIT_CANCELLED;
	int no_x = 0, no_end = 0, yes_x = 0, yes_end = 0;

	while (!kwl_should_close()) {
		int w = ktui_w, h = ktui_h;
		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
		ktui_draw_box(krect(0, 0, w, h), "KDOS", KT_ACCENT, KT_SURFACE, 1);

		for (int i = 0; i < nlines && 1 + i < h - 3; i++)
			ktui_draw_text(2, 1 + i, w - 4, lines[i], KT_TEXT,
				       KT_SURFACE, KT_A_NONE);

		/* Right-aligned as a pair, which is where a hand goes looking
		 * for the affirmative in every dialog since the mid-nineties. */
		char nb[64], yb[64];
		snprintf(nb, sizeof(nb), " %s ", no);
		snprintf(yb, sizeof(yb), " %s ", yes);
		int nw = ktui_utf8_width(nb) + 2, yw = ktui_utf8_width(yb) + 2;
		int by = h - 2;
		yes_x = w - 2 - yw;
		yes_end = yes_x + yw;
		no_x = yes_x - BTN_GAP - nw;
		no_end = no_x + nw;
		if (no_x < 2)
			no_x = 2;

		for (int b = 0; b < 2; b++) {
			int x = b ? yes_x : no_x;
			const char *label = b ? yb : nb;
			bool on = sel == b;
			int fg = on ? KT_SURFACE : KT_TEXT;
			int bg = on ? KT_ACCENT : KT_SURFACE;
			ktui_draw_text(x, by, 1, "[", KT_DIM, KT_SURFACE,
				       KT_A_NONE);
			ktui_draw_fill(krect(x + 1, by,
					     ktui_utf8_width(label), 1), bg);
			ktui_draw_text(x + 1, by, ktui_utf8_width(label), label,
				       fg, bg, KT_A_NONE);
			ktui_draw_text(x + 1 + ktui_utf8_width(label), by, 1, "]",
				       KT_DIM, KT_SURFACE, KT_A_NONE);
		}
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				/* Hover tracks, so the button under the pointer
				 * is the one Enter would take. */
				if (ev.my == ktui_h - 2) {
					if (ev.mx >= no_x && ev.mx < no_end)
						sel = 0;
					else if (ev.mx >= yes_x && ev.mx < yes_end)
						sel = 1;
				}
				continue;
			}
			if (ev.press != KT_MP_PRESS || ev.btn != KT_MB_LEFT)
				continue;
			if (ev.my != ktui_h - 2)
				continue;
			if (ev.mx >= no_x && ev.mx < no_end) {
				rc = EXIT_NO;
				break;
			}
			if (ev.mx >= yes_x && ev.mx < yes_end) {
				rc = EXIT_YES;
				break;
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		switch (ev.key) {
		case KT_K_ESC:
			rc = EXIT_CANCELLED;
			goto done;
		case KT_K_LEFT:
			sel = 0;
			break;
		case KT_K_RIGHT:
			sel = 1;
			break;
		case KT_K_TAB:
		case KT_K_BTAB:
			sel = !sel;
			break;
		case KT_K_ENTER:
			rc = sel ? EXIT_YES : EXIT_NO;
			goto done;
		case 'y':
		case 'Y':
			rc = EXIT_YES;
			goto done;
		case 'n':
		case 'N':
			rc = EXIT_NO;
			goto done;
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	return rc;
}
