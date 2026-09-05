/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-calc — the desk accessory Sidekick had in 1984
 *
 *   ╔═ Calculator ═════════════════════════╗
 *   ║ 3 inch to mm                         ║
 *   ║                              76.2 mm ║
 *   ╟──────────────────────────────────────╢
 *   ║ 2^10 + sqrt(2)          1025.414214  ║
 *   ╚══════════════════════════════════════╝
 *
 * IT DOES NOT DO THE ARITHMETIC. `qalc` does — the tree already carries
 * `libqalculate`, which parses what a person actually typed: units, currencies
 * a person names, hexadecimal, `to`, and precedence that matches a pocket
 * calculator rather than a programming language.
 *
 * FORKED, NOT LINKED. `libqalculate` is C++ and this binary is C and carries
 * twenty-nine other surfaces; linking it would put libstdc++ on the panel
 * package on every image for one accessory. `qalc -t` prints the result and
 * nothing else, which is a one-line answer to a one-line question.
 *
 * ONCE PER PAUSE, NOT ONCE PER KEYSTROKE. The evaluation happens when the poll
 * loop goes idle with the input changed — which is a debounce that costs no
 * timer, because the loop already has a timeout. A fork per keystroke would be
 * the shape this surface exists not to have.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define CALC_COLS 46
#define CALC_ROWS 16
#define CALC_MAX  128		/* an expression a person types by hand */
#define CALC_HIST 10

/* The contract: F1 where there is a page, Esc where there is a layer, and the
 * hint row. This surface has no layers — Esc closes it — so the ladder is
 * empty and `ktui_keys` answers Esc with CLOSE. */
static KtuiKeys keys;

static char input[CALC_MAX];
static int caret;
static char result[CALC_MAX];
static int dirty;		/* the input changed since the last evaluate */

static struct {
	char expr[CALC_MAX];
	char res[CALC_MAX];
} hist[CALC_HIST];
static int nhist;
static int recall = -1;		/* which history row the arrows are on */

/*
 * `qalc -t <expr>` and the first line of what it says.
 *
 * A parse error is an answer too — `qalc` says so on stdout and the surface
 * shows it, because a person who typed `2 +` wants to see that rather than an
 * empty row that could equally mean the calculator is broken.
 */
static void evaluate(void)
{
	char out[CALC_MAX * 4];
	KbArgv a = { 0 };

	result[0] = '\0';
	dirty = 0;
	if (!input[0])
		return;
	if (!kb_have_prog("qalc")) {
		snprintf(result, sizeof(result), "qalc is not installed");
		return;
	}
	kb_argv_add(&a, "qalc");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, input);
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, sizeof(out)) != 0 || !out[0])
		return;
	out[strcspn(out, "\r\n")] = '\0';
	snprintf(result, sizeof(result), "%s", out);
}

static void input_set(const char *s)
{
	snprintf(input, sizeof(input), "%s", s ? s : "");
	caret = (int)strlen(input);
	dirty = 1;
	result[0] = '\0';
}

/* Push the finished sum onto the history and keep the newest ten. */
static void commit(void)
{
	if (!input[0] || !result[0])
		return;
	if (nhist == CALC_HIST) {
		memmove(&hist[0], &hist[1], sizeof(hist[0]) * (CALC_HIST - 1));
		nhist--;
	}
	snprintf(hist[nhist].expr, sizeof(hist[0].expr), "%s", input);
	snprintf(hist[nhist].res, sizeof(hist[0].res), "%s", result);
	nhist++;
	recall = -1;
	/*
	 * AND ONTO THE CLIPBOARD, because the answer to "what is 3 inches in
	 * millimetres" is nearly always going somewhere else. It is the
	 * session's clipboard on the console and the compositor's under
	 * Wayland, from one call.
	 */
	kdisp_copy(result, strlen(result), 0);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "Calculator", KT_ACCENT, KT_SURFACE, 1);

	/* The sum being typed, and its answer under it in the accent — the
	 * shape every calculator has had since they had two rows. */
	ktui_draw_text(2, 1, w - 4, input[0] ? input : "type a sum",
		       input[0] ? KT_TEXT : KT_DIM, KT_SURFACE, KT_A_NONE);
	if (result[0])
		ktui_draw_text_right(0, 2, w - 3, result, KT_ACCENT,
				     KT_SURFACE, KT_A_BOLD);

	for (int x = 1; x < w - 1; x++)
		ktui_draw_text(x, 3, 1, "-", KT_DIM, KT_SURFACE, KT_A_NONE);

	/* Newest first: the thing a person wants back is usually the last one
	 * they worked out. */
	for (int i = 0; i < nhist && 4 + i < h - 2; i++) {
		int r = nhist - 1 - i;
		int on = recall == r;

		if (on)
			ktui_draw_fill(krect(1, 4 + i, w - 2, 1), KT_ACCENT);
		ktui_draw_text(2, 4 + i, (w - 4) / 2, hist[r].expr,
			       on ? KT_BG : KT_MID,
			       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		ktui_draw_text_right(0, 4 + i, w - 3, hist[r].res,
				     on ? KT_BG : KT_TEXT,
				     on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
	}

	/* PUSHED, not written: the row names what the surface answers RIGHT
	 * NOW, so recall appears only when there is something to recall. */
	ktui_hint("Enter", "copy");
	ktui_hint_if(nhist > 0, "Up/Down", "recall");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	ktui_term_caret(2 + caret, 1);
}

int calc_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (argv[i][0] != '-')
			input_set(argv[i]);	/* a sum on the command line */
		else {
			fprintf(stderr, "usage: kdos-calc [--font NAME] "
					"[--dump] [EXPRESSION]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = CALC_COLS,
		.rows = CALC_ROWS,
		.app_id = "kdos-calc",
		.font = font,
		.keyboard = 1,
		/*
		 * NOT dismiss_on_unfocus. An accessory is summoned over
		 * whatever a person is working in, and the answer is usually
		 * being read while they type into that — a calculator that
		 * vanished when they clicked back would be one they had to
		 * summon twice for every sum.
		 */
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(CALC_COLS, CALC_ROWS);
		ktui_draw_init();
		evaluate();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-calc: no display server\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 180)) {
			/*
			 * IDLE, WHICH IS THE DEBOUNCE. The loop's own timeout
			 * is what says a person has stopped typing, so the
			 * evaluation costs no timer and no fork per keystroke.
			 */
			if (dirty)
				evaluate();
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* FIRST, above this surface's own switch. Everything it does
		 * not own comes back PASS, so the arms below are unchanged. */
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			goto done;

		switch (ev.key) {
		case KT_K_ENTER:
			if (dirty)
				evaluate();
			commit();
			/* The answer row goes with the sum it answered: a
			 * result left standing over an empty input reads as
			 * the answer to nothing. It is in the history, which
			 * is where a person looks for it. */
			input[0] = '\0';
			result[0] = '\0';
			caret = 0;
			break;
		case KT_K_BACKSPACE:
			if (caret > 0) {
				memmove(input + caret - 1, input + caret,
					strlen(input) - (size_t)caret + 1);
				caret--;
				dirty = 1;
			}
			break;
		case KT_K_LEFT:
			if (caret > 0)
				caret--;
			break;
		case KT_K_RIGHT:
			if (input[caret])
				caret++;
			break;
		case KT_K_UP:
			if (nhist && recall != 0)
				recall = recall < 0 ? nhist - 1 : recall - 1;
			if (recall >= 0)
				input_set(hist[recall].expr);
			break;
		case KT_K_DOWN:
			if (recall >= 0 && recall + 1 < nhist) {
				recall++;
				input_set(hist[recall].expr);
			} else if (recall >= 0) {
				recall = -1;
				input_set("");
			}
			break;
		default:
			/* Printable ASCII only: an expression is typed on a
			 * keyboard and a control byte in one is a mistake, not
			 * a character. */
			if (ev.key >= 0x20 && ev.key < 0x7f &&
			    strlen(input) + 1 < sizeof(input)) {
				memmove(input + caret + 1, input + caret,
					strlen(input) - (size_t)caret + 1);
				input[caret++] = (char)ev.key;
				dirty = 1;
			}
			break;
		}
	}
done:
	kdisp_shutdown();
	return 0;
}
