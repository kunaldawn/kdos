/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — motion: a value over time, and whether anything moves
 *
 * A KtuiAnim IS READ, NEVER RUN. The draw asks what it is worth now and the
 * backend's frame clock brings the next draw; nothing here holds a list of
 * animations, so one that is dropped mid-way costs nothing and cannot keep a
 * clock running. The one thing shared is a DEADLINE — the latest end of any
 * animation started while motion was possible — and that is all a backend
 * needs to know to keep ticking or to fall silent.
 *
 * THE END VALUE WHEREVER NOTHING MOVES: a backend with no frame clock, and
 * motion switched off. See ktui.h.
 * ---------------------------------
 */

#include <time.h>

#include "ktui.h"

static int64_t (*anim_clock)(void);
static int (*anim_motion)(void);
/* The latest end of anything started moving. ktui_anim_live() is now < it. */
static int64_t live_until;

int64_t ktui_anim_now(void)
{
	struct timespec ts;

	if (anim_clock)
		return anim_clock();
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void ktui_anim_set_clock(int64_t (*now)(void))
{
	anim_clock = now;
}

void ktui_anim_set_motion_fn(int (*fn)(void))
{
	anim_motion = fn;
}

/* A backend that will tick is installed. Cheap: a draw asks it per value. */
static int clocked(void)
{
	const KtuiBackend *b = ktui_backend();

	return b && b->animates && b->animates();
}

int ktui_anim_moving(void)
{
	return clocked() && (!anim_motion || anim_motion());
}

int ktui_anim_live(void)
{
	return live_until && ktui_anim_now() < live_until && clocked();
}

float ktui_ease(int ease, float t)
{
	if (!(t > 0.0f))	/* a NaN settles at the start, not in the air */
		return 0.0f;
	if (t >= 1.0f)
		return 1.0f;
	switch (ease) {
	case KT_EASE_IN_OUT:
		if (t < 0.5f)
			return 4.0f * t * t * t;
		{
			float u = 2.0f - 2.0f * t;

			return 1.0f - u * u * u / 2.0f;
		}
	case KT_EASE_LINEAR:
		return t;
	case KT_EASE_OUT:
	default: {
		float u = 1.0f - t;

		return 1.0f - u * u * u;
	}
	}
}

void ktui_anim_start(KtuiAnim *a, float from, float to, int dur_ms, int ease,
		     int beats)
{
	a->from = from;
	a->to = to;
	a->dur_ms = dur_ms > 0 ? dur_ms : 0;
	a->ease = (unsigned char)ease;
	a->beats = (unsigned char)(beats > 0 ? (beats > 255 ? 255 : beats) : 0);
	a->t0 = ktui_anim_now();
	/* A clock at exactly 0 is a real time; 0 in t0 means "never ran". */
	if (!a->t0)
		a->t0 = 1;
	a->still = !a->dur_ms || !ktui_anim_moving();
	if (!a->still && a->t0 + a->dur_ms > live_until)
		live_until = a->t0 + a->dur_ms;
}

float ktui_anim_end(const KtuiAnim *a)
{
	return a->beats ? a->from : a->to;
}

float ktui_anim_value_at(const KtuiAnim *a, int64_t now)
{
	float t, p;

	if (!a->t0 || a->still || now >= a->t0 + a->dur_ms)
		return ktui_anim_end(a);
	t = now <= a->t0 ? 0.0f : (float)(now - a->t0) / (float)a->dur_ms;
	if (a->beats) {
		/* There and back `beats` times: the phase counts half-beats,
		 * and every odd one runs the curve backwards. */
		float ph = t * (float)a->beats * 2.0f;
		int k = (int)ph;
		float u = ph - (float)k;

		p = k & 1 ? 1.0f - ktui_ease(a->ease, u)
			  : ktui_ease(a->ease, u);
	} else {
		p = ktui_ease(a->ease, t);
	}
	return a->from + (a->to - a->from) * p;
}

float ktui_anim_value(const KtuiAnim *a)
{
	/* The clock is asked again here and not only at the start: a capture
	 * backend swapped in for a golden draws the end, whatever was running
	 * when it was installed. */
	if (!a->t0 || a->still || !clocked())
		return ktui_anim_end(a);
	return ktui_anim_value_at(a, ktui_anim_now());
}

int ktui_anim_running(const KtuiAnim *a)
{
	return ktui_anim_left(a) > 0;
}

int ktui_anim_left(const KtuiAnim *a)
{
	int64_t left;

	if (!a->t0)
		return 0;
	left = a->t0 + a->dur_ms - ktui_anim_now();
	return left > 0 ? (int)left : 0;
}

void ktui_anim_stop(KtuiAnim *a)
{
	a->still = 1;
	a->dur_ms = 0;
}
