/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ktui_gesture — touch becomes a gesture, and a mouse
 *
 * ONE recogniser for every backend that has touch. libinput feeds it under the
 * KMS backend and wl_touch feeds it under the Wayland one; a disambiguator
 * written inside a backend would be written twice and would disagree twice.
 *
 * NO CLOCK IS READ HERE. Both sources carry a timestamp and the event brings
 * it, which is what makes this a pure function of its input — the test suite
 * drives a whole long-press without sleeping for half a second.
 *
 * MOVEMENT IS IN CELLS. A drag begins when the finger leaves the cell it
 * started in. Coarse on purpose: everything above this file is a grid, and a
 * threshold in pixels is a number this library cannot see.
 * ---------------------------------
 */

#include <string.h>

#include "ktui.h"

#define MAX_FINGERS 2

struct finger {
	int active;
	int slot;
	int sx, sy;		/* cell it went down in                    */
	int x, y;		/* cell it is in now                       */
	unsigned t0;
	int moved;		/* has left its starting cell              */
};

static struct finger fingers[MAX_FINGERS];
static int nfingers;
static int long_fired;
static int two_finger;		/* a second finger arrived; no tap can follow */
static int prev_span;		/* last centroid separation, for pinch     */

static struct finger *find(int slot, int make)
{
	for (int i = 0; i < MAX_FINGERS; i++)
		if (fingers[i].active && fingers[i].slot == slot)
			return &fingers[i];

	if (!make)
		return NULL;

	for (int i = 0; i < MAX_FINGERS; i++)
		if (!fingers[i].active)
			return &fingers[i];

	return NULL;
}

static int span(void)
{
	if (nfingers < 2)
		return 0;

	int dx = fingers[0].x - fingers[1].x;
	int dy = fingers[0].y - fingers[1].y;

	/* Manhattan, not Euclidean: no sqrt, because libktui links no maths
	 * library. Pinch only compares this against itself, so the metric
	 * matters less than never changing it. */
	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

void ktui_gesture_reset(void)
{
	memset(fingers, 0, sizeof(fingers));
	nfingers = 0;
	long_fired = 0;
	two_finger = 0;
	prev_span = 0;
}

static void emit_mouse(KtuiEvent *m, int *have, int x, int y, int btn, int press)
{
	if (!m || !have)
		return;

	memset(m, 0, sizeof(*m));
	m->type = KT_EVT_MOUSE;
	m->mx = x;
	m->my = y;
	m->btn = btn;
	m->press = press;
	*have = 1;
}

/* Which edge a gesture started against, or 0. */
static int start_edge(const struct finger *f)
{
	if (f->sx <= 0)
		return KT_K_LEFT;
	if (ktui_w > 0 && f->sx >= ktui_w - 1)
		return KT_K_RIGHT;
	if (f->sy <= 0)
		return KT_K_UP;
	if (ktui_h > 0 && f->sy >= ktui_h - 1)
		return KT_K_DOWN;
	return 0;
}

int ktui_gesture_feed(const KtuiEvent *ev, KtuiGesture *g,
		      KtuiEvent *mouse, int *have_mouse)
{
	if (have_mouse)
		*have_mouse = 0;
	if (!ev || ev->type != KT_EVT_TOUCH || !g)
		return 0;

	memset(g, 0, sizeof(*g));

	struct finger *f;

	switch (ev->phase) {
	case KT_TOUCH_DOWN:
		f = find(ev->slot, 1);
		if (!f)
			return 0;	/* more fingers than we track */
		f->active = 1;
		f->slot = ev->slot;
		f->sx = f->x = ev->mx;
		f->sy = f->y = ev->my;
		f->t0 = ev->ms;
		f->moved = 0;
		nfingers++;
		long_fired = 0;
		if (nfingers >= 2) {
			two_finger = 1;
			prev_span = span();
		}
		emit_mouse(mouse, have_mouse, ev->mx, ev->my,
			   KT_MB_LEFT, KT_MP_PRESS);
		return 0;

	case KT_TOUCH_MOVE:
		f = find(ev->slot, 0);
		if (!f)
			return 0;

		int dx = ev->mx - f->x;
		int dy = ev->my - f->y;

		f->x = ev->mx;
		f->y = ev->my;
		if (ev->mx != f->sx || ev->my != f->sy)
			f->moved = 1;

		if (nfingers >= 2) {
			/*
			 * SCROLL AND PINCH ARE TOLD APART BY WHETHER THE TWO
			 * FINGERS AGREE ON A DIRECTION, not by what one of them
			 * did. Touch events arrive a finger at a time, and one
			 * finger moving away from a stationary one is a pinch
			 * and a scroll and a drag all at once — there is no
			 * answer until the other finger has said something.
			 * Reporting a guess here makes pinch and scroll flip
			 * back and forth mid-gesture, which is unusable.
			 */
			int d0x = fingers[0].x - fingers[0].sx;
			int d0y = fingers[0].y - fingers[0].sy;
			int d1x = fingers[1].x - fingers[1].sx;
			int d1y = fingers[1].y - fingers[1].sy;

			if ((!d0x && !d0y) || (!d1x && !d1y))
				return 0;	/* undecided, and honest */

			g->fingers = nfingers;
			g->x = ev->mx;
			g->y = ev->my;

			if (d0x * d1x + d0y * d1y < 0) {
				int now = span();

				g->type = KT_GEST_PINCH;
				g->dx = now - prev_span;
				prev_span = now;
				return 1;
			}

			g->type = KT_GEST_SCROLL;
			g->dx = dx;
			g->dy = dy;
			if (dy)
				emit_mouse(mouse, have_mouse, ev->mx, ev->my,
					   dy < 0 ? KT_MB_WHEEL_UP
						  : KT_MB_WHEEL_DOWN,
					   KT_MP_PRESS);
			return 1;
		}

		if (!dx && !dy)
			return 0;

		emit_mouse(mouse, have_mouse, ev->mx, ev->my,
			   KT_MB_LEFT, KT_MP_DRAG);

		g->fingers = 1;
		g->x = ev->mx;
		g->y = ev->my;
		g->dx = dx;
		g->dy = dy;
		g->edge = start_edge(f);
		g->type = g->edge ? KT_GEST_SWIPE_EDGE : KT_GEST_DRAG;
		return 1;

	case KT_TOUCH_UP:
		f = find(ev->slot, 0);
		if (!f)
			return 0;

		emit_mouse(mouse, have_mouse, f->x, f->y,
			   KT_MB_LEFT, KT_MP_RELEASE);

		int tap = !f->moved && !two_finger && !long_fired
			&& (unsigned)(ev->ms - f->t0) < (unsigned)KT_TAP_MS;
		int tx = f->x, ty = f->y;

		f->active = 0;
		nfingers--;
		if (nfingers <= 0) {
			nfingers = 0;
			two_finger = 0;
			long_fired = 0;
		}

		if (tap) {
			g->type = KT_GEST_TAP;
			g->fingers = 1;
			g->x = tx;
			g->y = ty;
			return 1;
		}
		return 0;

	case KT_TOUCH_CANCEL:
	default:
		/* Taken away, not finished: nothing is reported and nothing
		 * half-recognised is left behind for the next sequence. */
		ktui_gesture_reset();
		return 0;
	}
}

int ktui_gesture_tick(unsigned ms, KtuiGesture *g)
{
	if (!g || nfingers != 1 || long_fired || two_finger)
		return 0;

	for (int i = 0; i < MAX_FINGERS; i++) {
		struct finger *f = &fingers[i];

		if (!f->active || f->moved)
			continue;
		if ((unsigned)(ms - f->t0) < (unsigned)KT_LONG_MS)
			continue;

		long_fired = 1;
		memset(g, 0, sizeof(*g));
		g->type = KT_GEST_LONG;
		g->fingers = 1;
		g->x = f->x;
		g->y = f->y;
		return 1;
	}

	return 0;
}
