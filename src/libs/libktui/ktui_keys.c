/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The keys every surface answers, and the row that says so.
 *
 * ONE DESCRIPTOR AND TWO CALLS. A surface holds a `KtuiKeys`, calls
 * `ktui_keys()` first in the dispatch it already has, and `ktui_hint_row()`
 * last in the draw it already has. Nothing else changes: `ktui_keys()` returns
 * PASS for every key it does not own, so a surface that has not been converted
 * behaves byte for byte as it did.
 *
 * THAT SHAPE IS FORCED BY THE TREE, not chosen. libktui carries an
 * immediate-mode frame protocol with exactly two consumers; every desktop
 * surface and every monitor page runs the other idiom — caller-owned state,
 * draw, then a separate hit test — and there is no shared event loop anywhere:
 * thirty-eight hand-written poll loops in thirty-six files. A contract that
 * required the rare idiom would be a contract nothing could adopt.
 *
 * THE HINT ROW IS PUSHED DURING THE DRAW, by whatever holds the focus. That is
 * the whole reason it is a toolkit function rather than a string each surface
 * writes: a fixed string cannot follow the focus, and a row that names keys the
 * focused control does not answer is worse than no row.
 *
 * ESC IS A LADDER OF DECLARED LAYERS, asked at the instant the key arrives and
 * never cached. A dialog that dismissed itself from a click would otherwise
 * leave a raised bit behind, and that bit swallows the next Esc — which is the
 * defect this contract exists to remove, not one to reintroduce inside it.
 *
 * THE MENU IS ROUTED THROUGH HERE AND IS ALWAYS INNERMOST, above the ladder.
 * `F10` opens a bar, `Shift+F10` the context pane where the surface says its
 * focus is, `Alt+letter` a pane by its mark. That is also why `ktui_keys()`
 * takes EVERY event and not only a key: a surface with a menu would otherwise
 * need a second call site in its pointer path, and two call sites for one
 * widget disagree about which of them saw the click.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "ktui.h"

/* ── the hint pool ─────────────────────────────────────────────────────── */

/*
 * TWELVE. Eighty columns hold about six "Key Verb" pairs and a wide screen
 * about twelve; a pool past that is rows nothing can draw. A push beyond it is
 * dropped rather than wrapped: the first hints pushed are the frame's own and
 * are the ones that must survive.
 */
enum { HINT_MAX = 12, HINT_KEY = 12, HINT_VERB = 24 };

static struct {
	char key[HINT_KEY];
	char verb[HINT_VERB];
} pool[HINT_MAX];
static int npool;

void ktui_hint(const char *key, const char *verb)
{
	if (npool >= HINT_MAX || !key || !verb)
		return;
	/* COPIED, both of them. A widget pushes from inside its own draw and
	 * may be pushing a buffer on its stack; a pool of pointers would be a
	 * lifetime rule reaching every caller. */
	snprintf(pool[npool].key, sizeof(pool[0].key), "%s", key);
	snprintf(pool[npool].verb, sizeof(pool[0].verb), "%s", verb);
	npool++;
}

void ktui_hint_if(int on, const char *key, const char *verb)
{
	if (on)
		ktui_hint(key, verb);
}

/* ── the Esc ladder ────────────────────────────────────────────────────── */

void ktui_keys_layer(KtuiKeys *k, const char *verb, KtuiLayerUp up,
		     KtuiLayerClose close, void *user)
{
	if (!k || k->nlayer >= KTUI_LAYER_MAX || !up || !close)
		return;
	k->layer[k->nlayer].verb = verb ? verb : "Back";
	k->layer[k->nlayer].up = up;
	k->layer[k->nlayer].close = close;
	k->layer[k->nlayer].user = user;
	k->nlayer++;
}

/* The innermost layer that is up right now, or -1. Walked from the END,
 * because registration order IS the order Esc unwinds and the last registered
 * is the innermost. */
static int layer_top(const KtuiKeys *k)
{
	for (int i = k->nlayer - 1; i >= 0; i--)
		if (k->layer[i].up(k->layer[i].user))
			return i;
	return -1;
}

const char *ktui_esc_verb(const KtuiKeys *k)
{
	int i;

	if (!k)
		return "Close";
	/* THE MENU IS ALWAYS INNERMOST. It is not on the ladder — it is opened
	 * and closed by keys this file already owns — so the verb has to name
	 * it here or the row says Close on a screen where Esc cancels a menu. */
	if (k->menu && ktui_menu_active(k->menu))
		return "Cancel";
	i = layer_top(k);
	return i >= 0 ? k->layer[i].verb : "Close";
}

/* ── the row ───────────────────────────────────────────────────────────── */

int ktui_hint_row(const KtuiKeys *k, KRect r, int bg)
{
	int x = r.x, end = r.x + r.w;
	int drawn = 0;

	/*
	 * CLEARED FIRST, on every path. Twenty-odd `--dump` paths draw one
	 * frame and never flush, so a pool emptied at flush time would carry
	 * one surface's hints into the next dump in the same process — and
	 * kdos-shell is one binary with thirty-two front ends.
	 */
	int n = npool;
	struct { char key[HINT_KEY]; char verb[HINT_VERB]; } mine[HINT_MAX];

	memcpy(mine, pool, sizeof(mine));
	npool = 0;

	/*
	 * NOT ON A SHORT WINDOW. A hint row that eats a third of a tooltip is
	 * worse than none, and eight rows is where a surface stops being a
	 * window and starts being a label.
	 */
	if (ktui_h < 8 || r.w < 8 || r.h < 1)
		return 0;

	ktui_draw_fill(r, bg);

	/*
	 * F1 IS DRAWN FIRST AND IS NOT PUSHED. It belongs to the descriptor
	 * rather than to a frame — the surface declares a page once and the
	 * key answers for the life of the program — so putting it in the pool
	 * would mean every draw in every surface remembering to push it. It
	 * leads the row for the same reason it does on every system that has
	 * ever had one.
	 */
	if (k && k->doc && k->help) {
		x += ktui_draw_text(x, r.y, end - x, "F1", KT_ACCENT, bg,
				    KT_A_NONE);
		x += ktui_draw_text(x, r.y, end - x, " help  ", KT_MID, bg,
				    KT_A_NONE);
		drawn = 1;
	}

	for (int i = 0; i < n; i++) {
		char cell[HINT_KEY + HINT_VERB + 2];
		int w;

		/* The precisions are the fields' own widths. Without them the
		 * compiler cannot bound the copy — it has no way to know the
		 * pool's strings are terminated — and warns on a buffer that
		 * is provably large enough. */
		snprintf(cell, sizeof(cell), "%.*s %.*s", HINT_KEY - 1,
			 mine[i].key, HINT_VERB - 1, mine[i].verb);
		w = (int)strlen(cell);
		/* WHOLE OR NOT AT ALL. Half a hint names a key that does
		 * something else, which is worse than a row that stops. */
		if (x + w > end)
			break;
		/* The KEY in the accent and the verb beside it in KT_MID: the
		 * thing a hand is looking for is the part that stands out. */
		x += ktui_draw_text(x, r.y, end - x, mine[i].key, KT_ACCENT, bg,
				    KT_A_NONE);
		x += ktui_draw_text(x, r.y, end - x, " ", KT_MID, bg,
				    KT_A_NONE);
		x += ktui_draw_text(x, r.y, end - x, mine[i].verb, KT_MID, bg,
				    KT_A_NONE);
		if (x + 2 <= end)
			x += ktui_draw_text(x, r.y, end - x, "  ", KT_MID, bg,
					    KT_A_NONE);
		drawn = 1;
	}
	return drawn;
}

/* ── the dispatch ──────────────────────────────────────────────────────── */

int ktui_keys(KtuiKeys *k, const KtuiEvent *ev)
{
	if (!k || !ev)
		return KTUI_KEY_PASS;

	/*
	 * THE MENU FIRST, and every event rather than only keys. A pane that
	 * is down owns the arrows, Enter and Esc; the pointer path would
	 * otherwise need a second call site, and two call sites for one widget
	 * disagree about which of them saw the click.
	 */
	if (k->menu) {
		int id = 0, r;

		if (ktui_menu_alt(k->menu, ev))
			return KTUI_KEY_TAKEN;
		r = ktui_menu_event(k->menu, ev, &id);
		if (r == KTUI_MENU_PICKED) {
			k->menu_id = id;
			return KTUI_KEY_MENU;
		}
		if (r == KTUI_MENU_TAKEN)
			return KTUI_KEY_TAKEN;
	}

	if (ev->type != KT_EVT_KEY)
		return KTUI_KEY_PASS;

	/*
	 * F1 IS ONLY ANSWERED WHERE THERE IS A PAGE. A surface with no
	 * documentation says nothing in the row and binds nothing — a key that
	 * opens an index reading "no such document" teaches that help is
	 * broken, which is worse than a surface that never offered it.
	 */
	if (ev->key == KT_K_F1 && k->doc && k->help) {
		k->help(k->doc, k->user);
		return KTUI_KEY_TAKEN;
	}

	/*
	 * F10 OPENS THE BAR AND Shift+F10 THE CONTEXT PANE. Neither is
	 * answered where there is nothing to open: a surface with three verbs
	 * gets no bar, and one with nothing focused gets no context menu —
	 * the same rule F1 keeps, for the same reason.
	 */
	if (ev->key == KT_K_F10 && k->menu) {
		if (ev->mods & KT_MOD_SHIFT) {
			int x = 0, y = 0;

			if (k->ctx_at && k->ctx_at(&x, &y, k->user)) {
				ktui_menu_open(k->menu, k->ctx_pane, x, y);
				return KTUI_KEY_TAKEN;
			}
			return KTUI_KEY_PASS;
		}
		if (k->menu->has_bar) {
			ktui_menu_open(k->menu, 0, k->menu->bar_x[0],
				       k->menu->bar_row + 1);
			return KTUI_KEY_TAKEN;
		}
		return KTUI_KEY_PASS;
	}

	if (ev->key == KT_K_ESC) {
		int i = layer_top(k);

		if (i >= 0) {
			k->layer[i].close(k->layer[i].user);
			return KTUI_KEY_TAKEN;
		}
		/* Nothing is up, so Esc means the surface. The caller closes
		 * it: the toolkit does not own a program's lifetime. */
		return KTUI_KEY_CLOSE;
	}

	return KTUI_KEY_PASS;
}
