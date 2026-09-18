/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ktui_control.c — a value on a track
 *
 *       ◀ ███████░░░░░░  55 ▶
 *
 * THE ONE CONTROL A NUMBER NEEDED AND DID NOT HAVE. Every other shape in this
 * toolkit already exists — a button, a check, a radio, a list, a table, a tab
 * strip, a dropdown, a text field, a progress bar — and a number was the one
 * thing a surface had to invent for itself: kdos-settings changed an integer
 * with Left and Right and with nothing else, so a knob on this desktop could
 * not be turned with a mouse at all.
 *
 * DRAW / KEY / HIT, AND THE FRAME CONTROL ON TOP OF THEM. That is the shape
 * `ktui_dropdown_*` already has, and it is what lets a surface with an event
 * loop of its own — kdos-settings, kdos-display — use the same control as one
 * written in the immediate-mode frame. A control that existed only inside
 * ktui_frame_begin() would be a control those surfaces had to write twice.
 *
 * ONE LAYOUT FUNCTION, READ BY BOTH HALVES. The cell a value lands on and the
 * value a cell means are the same arithmetic in opposite directions; computed
 * twice they drift, and the symptom is a thumb that does not sit under the
 * pointer that dragged it.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "ktui.h"

/*
 * WHAT THE TRACK IS, GIVEN THE ROOM. The caps and the number are dropped
 * before the track is, in that order: a slider with no track cannot be
 * pointed at, and one with no number can still be read off the fill.
 */
static void slider_parts(KRect r, int *caps, int *tx, int *tw, int *vw)
{
	*caps = r.w >= KT_SLIDER_MIN_W;
	*vw = r.w >= 10 ? 4 : 0;
	*tx = r.x + (*caps ? 2 : 0);
	*tw = r.w - (*caps ? 4 : 0) - *vw;
	if (*tw < 1)
		*tw = 0;
}

/*
 * THE CELL A VALUE LANDS ON AND THE VALUE A CELL STANDS FOR — ONE MAPPING,
 * WRITTEN BOTH WAYS.
 *
 * They must be inverses over the CELLS: a press on the cell the thumb is drawn
 * on has to leave the thumb where it is, which is the difference between
 * grabbing a slider and knocking it. They cannot be inverses over the VALUES,
 * and nothing should pretend otherwise — a hundred values on sixteen cells is
 * a press that snaps to the cell it landed on, which is what a track is.
 *
 * Both round to nearest, which is what puts the thumb under the pointer.
 */
static int slider_cell(int val, int tw, int min, int max)
{
	int span = max - min;

	if (tw < 2)
		return 0;
	if (span <= 0)
		return tw - 1;
	if (val < min)
		val = min;
	if (val > max)
		val = max;
	return ((val - min) * (tw - 1) + span / 2) / span;
}

static int slider_val_at(int cell, int tw, int min, int max)
{
	int span = max - min;

	if (tw < 2 || span <= 0)
		return min;
	if (cell < 0)
		cell = 0;
	if (cell > tw - 1)
		cell = tw - 1;
	return min + (cell * span + (tw - 1) / 2) / (tw - 1);
}

static int slider_clamp(int v, int min, int max)
{
	return v < min ? min : v > max ? max : v;
}

void ktui_slider_draw(KRect r, int val, int min, int max, int focus, int bg)
{
	int caps, tx, tw, vw;
	int span = max - min;
	int fill;
	/*
	 * THE INK IS CHOSEN AGAINST THE BACKGROUND IT IS ON, not against the
	 * palette. A slider sits in a row somebody else filled — a settings
	 * form highlights the selected row in the accent — and ink picked
	 * without asking is the one colour that row is already painted in.
	 */
	int rev = bg == KT_ACCENT;
	int ink = rev ? KT_SURFACE : KT_ACCENT;
	int rest = rev ? KT_MID : KT_DIM;
	int mark = rev ? KT_BG : KT_TEXT;
	int edge = rev ? KT_SURFACE : KT_MID;

	if (r.w < 1 || r.h < 1)
		return;
	r.h = 1;
	val = slider_clamp(val, min, max);
	slider_parts(r, &caps, &tx, &tw, &vw);

	ktui_draw_fill(r, bg);

	if (caps) {
		ktui_draw_text(r.x, r.y, 1, ktui_glyph[KT_G_LEFT],
			       val > min ? edge : rest, bg, KT_A_NONE);
		ktui_draw_text(r.x + r.w - 1 - vw, r.y, 1,
			       ktui_glyph[KT_G_RIGHT],
			       val < max ? edge : rest, bg, KT_A_NONE);
	}

	/*
	 * THE FILL FOLLOWS THE THUMB AND NOT THE FRACTION. A progress bar's
	 * cells are how much is done and round independently; here the filled
	 * run exists to say where the thumb is, so it ends ON the thumb's cell
	 * — a fill computed its own way would put the two a cell apart at half
	 * the values on the track.
	 */
	fill = tw ? slider_cell(val, tw, min, max) + 1 : 0;
	(void)span;
	for (int i = 0; i < tw; i++)
		ktui_draw_text(tx + i, r.y, 1,
			       ktui_glyph[i < fill ? KT_G_FULL : KT_G_SHADE],
			       i < fill ? ink : rest, bg, KT_A_NONE);

	/*
	 * AND THE THUMB IS A DIFFERENT MARK, not merely the end of the fill. A
	 * bar says how full and a slider says where a thing is; the two read
	 * alike until somebody tries to grab one.
	 */
	if (tw > 0)
		ktui_draw_text(tx + fill - 1, r.y, 1, ktui_glyph[KT_G_SQUARE],
			       focus ? mark : ink, bg, KT_A_NONE);

	if (vw) {
		char num[16];

		snprintf(num, sizeof(num), "%d", val);
		ktui_draw_text_right(r.x + r.w - vw, r.y, vw, num,
				     rev ? KT_SURFACE : focus ? KT_TEXT
							      : KT_MID,
				     bg, KT_A_NONE);
	}
}

int ktui_slider_key(int *val, int min, int max, int step, int k)
{
	int was = *val;

	if (step < 1)
		step = 1;
	switch (k) {
	case KT_K_LEFT:
		*val -= step;
		break;
	case KT_K_RIGHT:
		*val += step;
		break;
	case KT_K_HOME:
		*val = min;
		break;
	case KT_K_END:
		*val = max;
		break;
	/* A page is a tenth of the range or one step, whichever is more: a
	 * tenth of a range of five is nothing at all. */
	case KT_K_PGUP:
		*val -= (max - min) / 10 > step ? (max - min) / 10 : step;
		break;
	case KT_K_PGDN:
		*val += (max - min) / 10 > step ? (max - min) / 10 : step;
		break;
	default:
		return 0;
	}
	*val = slider_clamp(*val, min, max);
	return *val != was;
}

int ktui_slider_hit(KRect r, int *val, int min, int max, int step, int mx,
		    int my, int press)
{
	int caps, tx, tw, vw, was = *val;

	if (r.w < 1 || r.h < 1 || my != r.y)
		return 0;
	r.h = 1;
	if (step < 1)
		step = 1;
	slider_parts(r, &caps, &tx, &tw, &vw);

	/*
	 * THE CAPS ANSWER A PRESS AND NEVER A DRAG. A drag that crossed an end
	 * cap would step the value on top of the position it is already
	 * setting, and the two fight: the thumb jumps past the pointer and
	 * comes back on the next sample.
	 */
	if (caps && press) {
		if (mx == r.x) {
			*val = slider_clamp(*val - step, min, max);
			return *val != was;
		}
		if (mx == r.x + r.w - 1 - vw) {
			*val = slider_clamp(*val + step, min, max);
			return *val != was;
		}
	}

	if (tw < 1 || mx < tx || mx >= tx + tw)
		return 0;
	*val = slider_val_at(mx - tx, tw, min, max);
	return *val != was;
}

int ktui_slider(KRect r, int *val, int min, int max, int step,
		const char *label)
{
	int id = ktui_id();
	int focus = ktui_focused(id);
	int changed = 0;
	const KtuiEvent *ev = ktui_event();

	if (step < 1)
		step = 1;
	ktui_slider_draw(r, *val, min, max, focus,
			 focus ? KT_ACCENT : KT_SURFACE);
	ktui_hit(r, id);

	/*
	 * THE PRESS AND EVERY DRAG BEHIND IT. ktui_drag() names the control
	 * the press was captured by, which is what lets the pointer leave the
	 * track and go on setting the value — a slider that stopped at its own
	 * edge would need the hand to stay inside four cells.
	 */
	if (ev->type == KT_EVT_MOUSE) {
		int press = ktui_clicked() == id;

		if (press || ktui_drag() == id)
			changed |= ktui_slider_hit(r, val, min, max, step,
						   ktui_mouse_x(),
						   ktui_mouse_y(), press);
	}

	/* A DETENT IS A STEP, and it is claimed only where a scrolling control
	 * did not want it — see ktui_wheel_take. */
	{
		int w = ktui_wheel_take(r);

		if (w) {
			int was = *val;

			*val = slider_clamp(*val + (w < 0 ? step : -step), min,
					    max);
			changed |= *val != was;
		}
	}

	if (focus && !ktui_consumed() && ev->type == KT_EVT_KEY &&
	    ktui_slider_key(val, min, max, step, ev->key)) {
		ktui_consume();
		changed = 1;
	}

	if (focus) {
		char num[16];

		snprintf(num, sizeof(num), "%d", slider_clamp(*val, min, max));
		ktui_announce(KT_A11Y_SLIDER, label, num, 0, 0);
	}
	return changed;
}

/* ────────────────────────────────────────────────────────────────────────
 * A LIST OF ROWS, POINTED AT
 *
 * THE ELEVEN SURFACES THAT DROPPED EVERY POINTER EVENT. `kdos-style`,
 * `kdos-chars`, `kdos-notes`, `kdos-trash`, `kdos-find`, `kdos-contacts`,
 * `kdos-rec`, `kdos-palette` and the rest all end their event loop with
 * `if (ev.type != KT_EVT_KEY) continue;` — so a person holding a mouse could
 * not choose an accent, pick a character or open a note at all.
 *
 * ONE IMPLEMENTATION OF WHAT A PRESS ON A ROW MEANS, because eleven of them
 * would be eleven desktops. The rule is `kdos-pick`'s and every other list
 * here already keeps it: a press MOVES the caret, and a press on the row the
 * caret is already on PICKS. One hand learns one thing.
 *
 * IT IS NOT A FRAME CONTROL, and that is the point: these surfaces run their
 * own event loop and draw their rows by hand — a glyph grid, a colour swatch,
 * a file size — so what they are missing is not a widget to draw but an answer
 * to "which row is that, and what did they mean by it".
 * ──────────────────────────────────────────────────────────────────────── */

int ktui_rows_hit(KRect r, int top, int count, int mx, int my)
{
	int i;

	if (!krect_hit(r, mx, my))
		return -1;
	i = top + (my - r.y);
	return i >= 0 && i < count ? i : -1;
}

int ktui_rows_event(KRect r, int *sel, int *top, int count,
		    const KtuiEvent *ev)
{
	int i;

	if (!ev || ev->type != KT_EVT_MOUSE || count < 1)
		return KTUI_ROWS_NONE;

	/*
	 * A DETENT IS A PRESS WITH NO RELEASE, so it is answered before the
	 * button arm below and never falls into it — a wheel tick read as a
	 * click would pick whichever row it happened to pass over.
	 */
	if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
		int was = *sel;

		*sel += ev->btn == KT_MB_WHEEL_UP ? -1 : 1;
		if (*sel < 0)
			*sel = 0;
		if (*sel >= count)
			*sel = count - 1;
		return *sel != was ? KTUI_ROWS_MOVED : KTUI_ROWS_NONE;
	}

	if (ev->press != KT_MP_PRESS)
		return KTUI_ROWS_NONE;

	/*
	 * THE RIGHT BUTTON IS BACK, which is what it means on every other
	 * surface here: a list with no way out but Escape is a list somebody
	 * holding a mouse is stuck in.
	 */
	if (ev->btn == KT_MB_RIGHT)
		return KTUI_ROWS_CLOSE;
	if (ev->btn != KT_MB_LEFT)
		return KTUI_ROWS_NONE;

	i = ktui_rows_hit(r, *top, count, ev->mx, ev->my);
	if (i < 0)
		return KTUI_ROWS_NONE;
	if (i == *sel)
		return KTUI_ROWS_PICKED;
	*sel = i;
	return KTUI_ROWS_MOVED;
}

void ktui_rows_follow(KRect r, int sel, int *top, int count)
{
	if (r.h < 1 || count < 1)
		return;
	if (*top > sel)
		*top = sel;
	if (*top < sel - r.h + 1)
		*top = sel - r.h + 1;
	if (*top > count - r.h)
		*top = count - r.h;
	if (*top < 0)
		*top = 0;
}
