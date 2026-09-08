/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the accents, each drawn in its own colours
 *
 *   ╔═ Theme ═════════════════════════════════════════════╗
 *   ║ ▸ phosphor   KDOS-Phosphor   ████████               ║
 *   ║   amber      KDOS-Amber      ████████               ║
 *   ║   ice        KDOS-Ice        ████████               ║
 *   ╟─────────────────────────────────────────────────────╢
 *   ║ ↑↓ preview   Enter keep   Esc back                  ║
 *   ╚═════════════════════════════════════════════════════╝
 *
 * THE SWATCHES ARE LITERAL COLOURS, AND THIS IS THE ONLY SURFACE ON THE
 * DESKTOP THAT MAY SET ONE. Chrome draws in slots so that one word repaints
 * everything; a swatch drawn in slots would take the CURRENT accent and show
 * seven identical rows, which is the one thing this window exists not to do.
 * The rule holds everywhere else and `preflight.sh` names this file as the
 * exception rather than dropping the check.
 *
 * Everything that is not a swatch — the frame, the names, the hint row —
 * follows `kdos theme` like any other surface, which is what makes the live
 * preview visible in the window doing the previewing.
 *
 * A PREVIEW IS HALF A THEME, and saying so is the whole design. `kdos theme
 * --preview` writes the accent's state file and signals the session: every
 * KDOS surface repaints. It generates nothing — not the GTK stylesheet, not
 * the icons, not the cursors, not the eight foreign configuration files —
 * because those take seconds and are read by programs that are not running.
 * So the desktop moves under the highlight and a boxed application does not,
 * and `Enter` is what runs the real `kdos theme` and makes the rest agree.
 *
 * WHICH IS WHY LEAVING RESTORES. The accent this window was opened on is put
 * back on `Esc`, on a close, and on SIGTERM or SIGINT — a picker killed
 * halfway through would otherwise leave the desktop wearing an accent nothing
 * else on the machine had been regenerated for.
 * ---------------------------------
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kcolor.h"
#include "kwl.h"
#include "shell.h"

/* The eight slots a scheme is, in the order a person reads a palette: what
 * things are drawn in first, then what they are drawn on. `pdark` and
 * `backdrop` are not here — they are the plate ladder's ends and differ from
 * `deep` by less than a swatch can show. */
#define TH_SWATCH 8

static KtuiKeys keys;
static int sel;
static char opened_on[32];	/* the accent this window was opened on */
static volatile sig_atomic_t stop;

static void on_term(int sig)
{
	(void)sig;
	stop = 1;
}

/* `kdos theme` and nothing else changes an accent. Two programs that both knew
 * how to switch one would be two answers to what a switch means — the
 * generators, the signal, the wallpaper cache and their order all live there. */
static void run_theme(const char *arg1, const char *arg2)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, "kdos");
	kb_argv_add(&a, "theme");
	if (arg1)
		kb_argv_add(&a, arg1);
	if (arg2)
		kb_argv_add(&a, arg2);
	kb_argv_end(&a);
	kb_run(&a);
}

static void preview(int i)
{
	if (i < 0 || i >= kcol_nscheme)
		return;
	run_theme("--preview", kcol_schemes[i].name);
	/* The window is a surface like any other and follows the state file;
	 * polling here rather than waiting for the next loop keeps the frame
	 * this keystroke draws from being the one before it. */
	sh_theme_poll();
	ktui_draw_invalidate();
}

static void restore(void)
{
	if (opened_on[0] && ktui_theme &&
	    strcmp(opened_on, ktui_theme->name))
		run_theme("--preview", opened_on);
}

int theme_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			fprintf(stderr, "usage: kdos-theme [--font NAME] "
					"[--dump]\n");
			return 2;
		}
	}

	sh_theme_from_cache();
	kb_strlcpy(opened_on, ktui_theme ? ktui_theme->name : "",
		   sizeof(opened_on));
	for (int i = 0; i < kcol_nscheme; i++)
		if (!strcmp(kcol_schemes[i].name, opened_on))
			sel = i;

	/* The name column is the longest name plus a gap, the theme-name column
	 * likewise, then the swatch. Measured rather than guessed: an accent
	 * added to `kcolor.h` must not need this window edited. */
	int namew = 0, themew = 0;

	for (int i = 0; i < kcol_nscheme; i++) {
		int n = (int)strlen(kcol_schemes[i].name);
		int t = (int)strlen(kcol_schemes[i].theme_name);

		if (n > namew)
			namew = n;
		if (t > themew)
			themew = t;
	}

	int cols = 2 + 2 + namew + 2 + themew + 2 + TH_SWATCH + 2;
	int rows = kcol_nscheme + 4;

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = cols,
		.rows = rows,
		.app_id = "kdos-theme",
		.font = font,
		.keyboard = 1,
	};

	if (dump) {
		ktui_offscreen_init(cols, rows);
		ktui_draw_init();
	} else if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-theme: no display server\n");
		return 1;
	} else {
		ktui_draw_init();
		kch_px_popup(KT_SURFACE);
		signal(SIGTERM, on_term);
		signal(SIGINT, on_term);
	}

	do {
		int w = ktui_w, h = ktui_h;

		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
		ktui_draw_box(krect(0, 0, w, h), "Theme", KT_ACCENT,
			      KT_SURFACE, 1);

		for (int i = 0; i < kcol_nscheme && 1 + i < h - 2; i++) {
			const KcolScheme *sc = &kcol_schemes[i];
			int y = 1 + i;
			int x = 2;

			ktui_draw_text(x, y, 1,
				       i == sel ? ktui_glyph[KT_G_RIGHT] : " ",
				       KT_ACCENT, KT_SURFACE, KT_A_NONE);
			x += 2;
			ktui_draw_text(x, y, namew, sc->name, KT_TEXT,
				       KT_SURFACE,
				       i == sel ? KT_A_BOLD : KT_A_NONE);
			x += namew + 2;
			ktui_draw_text(x, y, themew, sc->theme_name, KT_MID,
				       KT_SURFACE, KT_A_NONE);
			x += themew + 2;

			/*
			 * THE ONE LITERAL. Eight blocks, each the scheme's own
			 * colour, so a row is a sample of the palette it names
			 * rather than a row of the palette in force. The
			 * glyph is a block and not a space: a dump carries
			 * characters and no colour, so a swatch of spaces
			 * would be a blank rectangle in every golden and in
			 * every terminal that cannot do colour, where a run of
			 * blocks is still a swatch.
			 */
			const uint32_t slot[TH_SWATCH] = {
				sc->primary, sc->secondary, sc->urgent,
				sc->text, kcol_muted(sc), sc->dim,
				sc->variant, sc->deep,
			};

			uint32_t block = '#';

			/* THROUGH THE GLYPH TABLE, so the swatch degrades with
			 * everything else: `ktui_draw_put` is the one drawing
			 * call that does not go past the tier check, and a
			 * hard-coded U+2588 here would be the one character on
			 * a 7-bit terminal that came out as a replacement. */
			ktui_utf8_next(ktui_glyph[KT_G_FULL], &block);

			for (int k = 0; k < TH_SWATCH && x + k < w - 1; k++) {
				KtuiCell c = { 0 };

				c.ch = block;
				c.fg = KT_TEXT;
				c.bg = KT_SURFACE;
				c.attr = KT_A_FGRGB;
				c.fgc = slot[k];
				ktui_draw_put(x + k, y, &c);
			}

			/* The selection is a reverse rather than a colour: the
			 * row's whole point is the colours in it, and painting
			 * a background under them would be this window
			 * recolouring its own samples. */
			if (i == sel)
				ktui_draw_reverse(krect(1, y, w - 2, 1));
		}

		char updown[8];

		snprintf(updown, sizeof(updown), "%s%s",
			 ktui_glyph[KT_G_UP], ktui_glyph[KT_G_DOWN]);
		ktui_hint(updown, "preview");
		ktui_hint("Enter", "keep");
		ktui_hint("Esc", "back");
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);

		if (dump) {
			ktui_draw_dump();
			break;
		}
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (stop)
				break;
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			sh_theme_poll();
			continue;
		}
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (ev.type != KT_EVT_KEY)
			continue;
		if (ev.key == KT_K_UP && sel > 0) {
			sel--;
			preview(sel);
		} else if (ev.key == KT_K_DOWN && sel < kcol_nscheme - 1) {
			sel++;
			preview(sel);
		} else if (ev.key == KT_K_HOME) {
			sel = 0;
			preview(sel);
		} else if (ev.key == KT_K_END) {
			sel = kcol_nscheme - 1;
			preview(sel);
		} else if (ev.key == KT_K_ENTER) {
			/*
			 * KEPT MEANS THE WHOLE SWITCH, not the preview made
			 * permanent: the generators have not run, so leaving
			 * here without this would be a desktop in one accent
			 * and every alien application in another.
			 */
			run_theme(kcol_schemes[sel].name, NULL);
			opened_on[0] = '\0';
			break;
		}
	} while (!stop && !kdisp_should_close());

	restore();
	if (!dump)
		kdisp_shutdown();
	return 0;
}
