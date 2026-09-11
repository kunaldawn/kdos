/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-style — how the screen looks: the accent, and the font
 *
 *   ╔═ Style ═════════════════════════════════════════════╗
 *   ║  Accent │ Font                                      ║
 *   ║ ▸ phosphor   KDOS-Phosphor   ████████               ║
 *   ║   amber      KDOS-Amber      ████████               ║
 *   ║   ice        KDOS-Ice        ████████               ║
 *   ╟─────────────────────────────────────────────────────╢
 *   ║ ↑↓ preview   Enter keep   Esc back                  ║
 *   ╚═════════════════════════════════════════════════════╝
 *
 * `kdos-style` AND NOT `kdos-theme`. `kdos-theme` is the artwork GENERATOR —
 * a port of its own, which `kdos theme` runs as `kdos-theme gtk|icons|cursors`
 * — and two packages installing one path leaves one of the two programs
 * unreachable by name. This is the surface; that is the tool.
 *
 * TWO PAGES BECAUSE THEY ARE ONE QUESTION. How the screen looks is the accent
 * and the face it draws in, and a second window for the second half would be a
 * second thing to find.
 *
 * THE FONT LIST IS THE DISPLAY'S. A surface never loads a font — it draws
 * cells and something else turns them into pixels — so the faces come from the
 * display through `libkdisp`, and what goes back is an INDEX into that list
 * and never a name: the display may be at the far end of an ssh link with its
 * own machine's fonts.
 *
 * AND THE SAMPLE ROW IS THE SCREEN. A cell grid has exactly one font at a
 * time, so a row cannot be drawn in a face the screen is not wearing — the
 * arrows put the highlighted face ON, live, and every cell in this window is
 * then a sample of it. That is why the sample text is beside each name rather
 * than only under the list: whichever row the highlight is on, its letters are
 * the ones being judged.
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

/* How many faces the list holds and how long one name is — the wire's own
 * caps, so a display offering more sends the first of them and this window
 * never has to say it dropped any. */
#define TH_FONT_MAX  64
#define TH_FONT_NAME 192

/* The letters a face is judged on: an ascender, a descender, the two digits
 * that are confused for each other, and the punctuation a terminal reads. */
#define TH_SAMPLE "AaBbGg 0O1lI {}[]()"

/* The name column, and how many faces fit. A fontconfig family runs long and a
 * window is not a place to read one in full: the column is cut and the SAMPLE
 * is never, because a face is judged on its letters. */
#define TH_FONT_NAMEW 28
#define TH_FONT_ROWS  12

enum { PG_ACCENT, PG_FONT, PG_N };

static const KtuiTab PAGES[PG_N] = { { "Accent", NULL }, { "Font", NULL } };

static KtuiKeys keys;
static int page;
static int sel;
static int fsel;		/* the highlighted face */
static int fopen_idx = -1;	/* the face this window was opened on */
/* What this window has put on the screen, or TH_NO_FACE when it has changed
 * nothing. A negative index is a real answer here — it is what the display
 * says when it cannot name which of its own list is in force — so "nothing"
 * needs a value of its own. */
#define TH_NO_FACE (-2)
static int fapplied = TH_NO_FACE;
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
static int th_usage(void)
{
	fprintf(stderr, "usage: kdos-style [--page accent|font] "
			"[--font NAME] [--dump]\n");
	return 2;
}

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

/*
 * THE FACES THE DISPLAY OFFERS, re-read every turn.
 *
 * The list arrives over a socket some pumps after it is asked for, so a
 * caller that asked once and believed the first count would draw an empty
 * list for ever — the rule the window list keeps, for the same reason.
 */
static int font_rows(char out[][TH_FONT_NAME], int max)
{
	int n = kdisp_font_count();

	if (n > max)
		n = max;
	for (int i = 0; i < n; i++)
		if (!kdisp_font_at(i, out[i], TH_FONT_NAME))
			return i;
	return n;
}

/*
 * A PREVIEW IS A REAL FONT ON A REAL SCREEN, and it is not kept.
 *
 * `keep` is 0 here and 1 only under Enter: a step that persisted would make
 * the last face a highlight passed over the one the next login comes up in.
 * The grid changes under this window as the cell does, which is why nothing is
 * cached across the change — the next frame is drawn from what the display
 * says now.
 */
static void font_preview(int i)
{
	kdisp_font_set(i, 0);
	fapplied = i;
	ktui_draw_invalidate();
}

/*
 * THE FACE THIS WINDOW OPENED ON, back — and only when this window changed
 * it.
 *
 * WHAT IS ON THE SCREEN IS TRACKED HERE and not asked of the display: the
 * display reports what it last LISTED, and a preview does not re-list. A
 * window that asked would be told the face it started with and would put that
 * one back over the one a person had just kept.
 */
static void font_restore(void)
{
	if (fapplied != TH_NO_FACE && fapplied != fopen_idx)
		kdisp_font_set(fopen_idx, 0);
}

int theme_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--page") && i + 1 < argc) {
			/* THE PAGE IS A NAME, not a number: `style.font` is
			 * the route a script holds, and a route that said
			 * `--page 1` would break the day a page is added
			 * before it. */
			const char *want = argv[++i];

			if (!strcmp(want, "font"))
				page = PG_FONT;
			else if (!strcmp(want, "accent"))
				page = PG_ACCENT;
			else
				return th_usage();
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			return th_usage();
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

	/*
	 * ONE WINDOW FOR BOTH PAGES, sized for the wider and the taller of
	 * them: this surface asks for its grid once and never resizes, so a
	 * page that needed more than it asked for would be a page clipped
	 * rather than scrolled.
	 *
	 * THE FONT PAGE'S WIDTH IS THE SAMPLE'S. A face is judged on letters
	 * and not on its name, so the sample is what must fit; the name column
	 * is capped and the sample is never cut.
	 */
	int accent_cols = 2 + 2 + namew + 2 + themew + 2 + TH_SWATCH + 2;
	int font_cols = 2 + 2 + TH_FONT_NAMEW + 2 +
			(int)sizeof(TH_SAMPLE) - 1 + 2;
	int cols = accent_cols > font_cols ? accent_cols : font_cols;
	/* One row for the page strip, and the font list is capped at what a
	 * short screen can show — the display's list can be far longer than a
	 * window, and a picker taller than the grid is a picker with rows
	 * nobody can reach. */
	int rows = kcol_nscheme + 5;

	if (rows < TH_FONT_ROWS + 5)
		rows = TH_FONT_ROWS + 5;

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = cols,
		.rows = rows,
		.app_id = "kdos-style",
		.font = font,
		.keyboard = 1,
		/*
		 * THE SCREEN'S FONT IS A MANAGEMENT VERB. Changing the cell
		 * re-cuts the grid under every window on the desktop, so it is
		 * asked for explicitly here for the reason the window list is:
		 * a surface that did not ask cannot do it.
		 */
		.manage = 1,
	};

	if (dump) {
		ktui_offscreen_init(cols, rows);
		ktui_draw_init();
	} else if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-style: no display server\n");
		return 1;
	} else {
		ktui_draw_init();
		kch_px_popup(KT_SURFACE);
		signal(SIGTERM, on_term);
		signal(SIGINT, on_term);
	}

	/*
	 * ASKED ONCE, HERE, and re-read every turn after: the display answers
	 * over a socket some pumps later, so nothing below may believe the
	 * first count it sees.
	 */
	kdisp_font_ask();
	fopen_idx = kdisp_font_current();
	fsel = fopen_idx > 0 ? fopen_idx : 0;

	do {
		int w = ktui_w, h = ktui_h;
		char fonts[TH_FONT_MAX][TH_FONT_NAME];
		int nfonts = font_rows(fonts, TH_FONT_MAX);

		/* THE LIST ARRIVES LATE, so the highlight follows it: a face
		 * in force when this opened is the row the eye should start
		 * on, and the index for it is not known until the answer is. */
		if (fopen_idx < 0 && nfonts > 0) {
			fopen_idx = kdisp_font_current();
			if (fopen_idx > 0)
				fsel = fopen_idx;
		}
		if (fsel >= nfonts)
			fsel = nfonts > 0 ? nfonts - 1 : 0;

		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
		ktui_draw_box(krect(0, 0, w, h), "Style", KT_ACCENT,
			      KT_SURFACE, 1);
		ktui_tabs_draw(krect(2, 1, w - 4, 1), PAGES, PG_N, page, -1, 0);

		for (int i = 0; page == PG_ACCENT && i < kcol_nscheme &&
		     2 + i < h - 2; i++) {
			const KcolScheme *sc = &kcol_schemes[i];
			int y = 2 + i;		/* under the page strip */
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

		/*
		 * THE FONT PAGE. A name, cut, and the sample beside it — and
		 * the sample is drawn in whatever the SCREEN is wearing,
		 * because a cell grid has one font at a time. That is not a
		 * limitation worked around: the arrows put the highlighted
		 * face on, so the row under the highlight is being shown in
		 * itself, and so is every other cell in this window.
		 */
		for (int i = 0; page == PG_FONT && i < nfonts &&
		     2 + i < h - 2; i++) {
			int y = 2 + i;
			int x = 2;

			ktui_draw_text(x, y, 1,
				       i == fsel ? ktui_glyph[KT_G_RIGHT] : " ",
				       KT_ACCENT, KT_SURFACE, KT_A_NONE);
			x += 2;
			ktui_draw_text(x, y, TH_FONT_NAMEW, fonts[i], KT_TEXT,
				       KT_SURFACE,
				       i == fsel ? KT_A_BOLD : KT_A_NONE);
			x += TH_FONT_NAMEW + 2;
			ktui_draw_text(x, y, w - 1 - x, TH_SAMPLE, KT_MID,
				       KT_SURFACE, KT_A_NONE);
			if (i == fsel)
				ktui_draw_reverse(krect(1, y, w - 2, 1));
		}

		/*
		 * AND THE HONEST ANSWER WHERE THERE IS NOTHING TO OFFER. A
		 * view running inside somebody else's terminal cannot change
		 * a font — that terminal owns it — and the session says so
		 * with the same sentence the chord does, because a person who
		 * pressed `Super+equal` and then opened this must not be told
		 * two different things.
		 */
		if (page == PG_FONT && nfonts == 0)
			ktui_draw_text(2, 3, w - 4,
				       "the terminal this view runs in owns "
				       "the font — change it there",
				       KT_MID, KT_SURFACE, KT_A_NONE);

		char updown[8];

		snprintf(updown, sizeof(updown), "%s%s",
			 ktui_glyph[KT_G_UP], ktui_glyph[KT_G_DOWN]);
		char leftright[8];

		snprintf(leftright, sizeof(leftright), "%s%s",
			 ktui_glyph[KT_G_LEFT], ktui_glyph[KT_G_RIGHT]);
		ktui_hint(updown, "preview");
		ktui_hint("Enter", "keep");
		ktui_hint("Esc", "back");
		ktui_hint(leftright, "page");
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
		/*
		 * THE PAGE STRIP, ON LEFT AND RIGHT ONLY.
		 *
		 * `ktui_tabs_key` also takes Home and End, and on both of
		 * these pages those belong to the LIST — a person pressing End
		 * on a list of forty faces means the last face, not the last
		 * page. The strip is asked only about the two keys that can
		 * mean nothing else here.
		 */
		if ((ev.key == KT_K_LEFT || ev.key == KT_K_RIGHT) &&
		    ktui_tabs_key(&page, PG_N, 0, ev.key))
			continue;

		if (page == PG_FONT) {
			if (ev.key == KT_K_UP && fsel > 0) {
				fsel--;
				font_preview(fsel);
			} else if (ev.key == KT_K_DOWN &&
				   fsel + 1 < kdisp_font_count()) {
				fsel++;
				font_preview(fsel);
			} else if (ev.key == KT_K_HOME) {
				fsel = 0;
				font_preview(fsel);
			} else if (ev.key == KT_K_END) {
				fsel = kdisp_font_count() - 1;
				if (fsel < 0)
					fsel = 0;
				font_preview(fsel);
			} else if (ev.key == KT_K_ENTER &&
				   kdisp_font_count() > 0) {
				/*
				 * KEPT IS THE ONLY THING THAT WRITES. Every
				 * arrow above was a real font on a real
				 * screen and none of them persisted, so
				 * leaving without this puts back what was
				 * there — which is what a person who was
				 * looking rather than choosing meant.
				 */
				kdisp_font_set(fsel, 1);
				fopen_idx = fapplied = fsel;
				break;
			}
			continue;
		}

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
	font_restore();
	if (!dump)
		kdisp_shutdown();
	return 0;
}
