/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   tickcheck — libkwl's frame clock, with no compositor
 *
 * kwl.c is INCLUDED, as paintcheck does, and the wire is replaced under it:
 * wl_proxy_marshal_flags() records every commit and every frame callback
 * asked for, and the display's socket is a pipe this file writes to when it
 * wants a frame callback answered — so kwl_poll_event() runs for real, poll()
 * and all, and what it returns is what a surface's loop would get.
 *
 *   - IDLE IS SILENT: with nothing animating, a wait runs its full length,
 *     returns no event, makes no commit and asks for no frame, and a frame
 *     callback answered for an ordinary commit sends no tick;
 *   - while an animation is live, a wait with no frame in flight asks for
 *     one with an empty commit (once), and the answer comes back as a
 *     KT_EVT_TICK returned as an event, at once and without the wait;
 *   - a callback nobody answers stalls into a tick after KWL_FRAME_STALL_MS
 *     and is dropped, so the next wait asks again;
 *   - one frame is owed after the last one drawn inside the window (it
 *     draws the end value), and none after that — including for a loop
 *     that never draws on its ticks;
 *   - comp.conf's `motion` as the compositor reads it: the last line wins, a
 *     comment is a comment, and a changed file is read again;
 *   - THE WHEEL: a seat at version 8 adds a high-resolution wheel's fractions
 *     up to one tick a detent, and never more than one a frame; a reversal
 *     starts again; the raw stream carries the frame's value120 and the
 *     natural-scrolling bit; the duplicate gate drops a second wheel detent
 *     inside its window and never a finger's tick;
 *   - THE COAST: a finger that lifts while moving keeps the list moving,
 *     slower and slower, then stops; one that stopped before it lifted, one
 *     too slow, a press and motion = no each mean no coast; and a wait
 *     during one returns an event or runs its whole length.
 *
 * usage: tickcheck <scratch dir>   (a comp.conf is written under it)
 * ---------------------------------
 */
#include "kwl.c"

#include <fcntl.h>
#include <sys/stat.h>

/* ── the wire ──────────────────────────────────────────────────────────── */

static char fake_surface, fake_display;
static int commits, frames_asked, pipefd[2];
static int answer_frame;	/* dispatch answers the frame callback */

struct wl_proxy *wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t op,
					const struct wl_interface *iface,
					uint32_t version, uint32_t flags, ...)
{
	(void)version;
	if (proxy == (struct wl_proxy *)&fake_surface) {
		if (op == WL_SURFACE_COMMIT)
			commits++;
		if (op == WL_SURFACE_FRAME)
			frames_asked++;
	}
	return iface && !(flags & WL_MARSHAL_FLAG_DESTROY)
		       ? (struct wl_proxy *)calloc(1, 16)
		       : NULL;
}

int wl_proxy_add_listener(struct wl_proxy *p, void (**impl)(void), void *data)
{
	(void)p;
	(void)impl;
	(void)data;
	return 0;
}

uint32_t wl_proxy_get_version(struct wl_proxy *p)
{
	(void)p;
	return 4;
}

void wl_proxy_destroy(struct wl_proxy *p)
{
	if (p != (struct wl_proxy *)&fake_surface)
		free(p);
}

int wl_display_flush(struct wl_display *d)
{
	(void)d;
	return 0;
}

int wl_display_prepare_read(struct wl_display *d)
{
	(void)d;
	return 0;
}

void wl_display_cancel_read(struct wl_display *d) { (void)d; }

int wl_display_get_fd(struct wl_display *d)
{
	(void)d;
	return pipefd[0];
}

int wl_display_read_events(struct wl_display *d)
{
	char b[64];

	(void)d;
	while (read(pipefd[0], b, sizeof(b)) > 0)
		;
	return 0;
}

/* What the socket carried: the compositor presenting a frame. */
int wl_display_dispatch_pending(struct wl_display *d)
{
	(void)d;
	if (answer_frame && K.frame_cb) {
		answer_frame = 0;
		frame_done(NULL, K.frame_cb, 0);
	}
	return 0;
}

/* ── the harness ───────────────────────────────────────────────────────── */

static int fails, checks;

static void ok(int c, const char *what)
{
	checks++;
	if (!c) {
		fails++;
		fprintf(stderr, "tickcheck: FAIL %s\n", what);
	}
}

/* The animation clock runs `skew` ahead of the real one, so the end of an
 * animation can be reached without sleeping through it. */
static int64_t skew;
static int64_t anim_clock(void) { return now_ms() + skew; }

/* The compositor presents: the socket wakes and the callback is answered. */
static void present(void)
{
	answer_frame = 1;
	if (write(pipefd[1], "f", 1) != 1)
		ok(0, "the pipe takes a byte");
}

static KtuiCell cells[4];

/*
 * The consumer's draw as the backend sees it: kwl_present() on a surface that
 * is not configured, so it records what the frame was drawn inside and
 * commits nothing — a draw that changed no pixel, which is exactly the case
 * an empty commit exists for.
 */
static void draw(void)
{
	K.configured = 0;
	kwl_present(cells, NULL, 2, 2, 0);
	K.configured = 1;
}

/* One poll with a stopwatch. */
static int poll_ms(KtuiEvent *ev, int timeout, int64_t *took)
{
	int64_t t0 = now_ms();
	int r;

	memset(ev, 0, sizeof(*ev));
	r = kwl_poll_event(ev, timeout);
	*took = now_ms() - t0;
	return r;
}

static void conf_write(const char *dir, const char *text)
{
	char path[600];
	FILE *f;

	snprintf(path, sizeof(path), "%s/kdos", dir);
	mkdir(path, 0755);
	snprintf(path, sizeof(path), "%s/kdos/comp.conf", dir);
	f = fopen(path, "w");
	if (!f) {
		ok(0, "comp.conf written");
		return;
	}
	fputs(text, f);
	fclose(f);
}


/* ── the wheel ─────────────────────────────────────────────────────────── */

/* Every event queued now, as wheel ticks each way; anything else is kept
 * out of the count. */
static void wheels(int *down, int *up)
{
	KtuiEvent e;

	*down = *up = 0;
	while (pop_event(&e))
		if (e.type == KT_EVT_MOUSE && e.btn == KT_MB_WHEEL_DOWN)
			(*down)++;
		else if (e.type == KT_EVT_MOUSE && e.btn == KT_MB_WHEEL_UP)
			(*up)++;
}

/* One pointer frame: a value120 (0 for none), an axis value, the frame. */
static void wframe(int v120, double v, unsigned ms)
{
	if (v120)
		pt_axis_v120(NULL, NULL, WL_POINTER_AXIS_VERTICAL_SCROLL, v120);
	pt_axis(NULL, NULL, ms, WL_POINTER_AXIS_VERTICAL_SCROLL,
		wl_fixed_from_double(v));
	pt_frame(NULL, NULL);
}

/* The last vertical axis event in the raw queue. */
static int raw_last_axis(KtuiRaw *out)
{
	KtuiRaw r;
	int got = 0;

	while (kwl_poll_raw(&r))
		if (r.type == KT_RAW_AXIS && r.axis == KT_RAW_VERT &&
		    (r.value || r.value120)) {
			*out = r;
			got = 1;
		}
	return got;
}

/* A finger moving `per` axis units every 10 ms for `n` frames from `t0`. */
static unsigned finger(unsigned t0, int n, double per)
{
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_FINGER);
	for (int i = 0; i < n; i++)
		wframe(0, per, t0 + 10u * (unsigned)i);
	return t0 + 10u * (unsigned)(n - 1);
}

static void wheel_checks(const char *confdir)
{
	int down, up, total, first, last, steps;
	KtuiRaw raw;
	KtuiEvent ev;
	int64_t took;

	K.ptr_cx = 3;
	K.ptr_cy = 2;
	wheels(&down, &up);
	while (kwl_poll_raw(&raw))
		;

	/* ── a high-resolution wheel on a version 8 seat ── */
	K.seat_ver = 8;
	K.wheel_last_ms = 0;
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(40, 5.0, 10);
	wheels(&down, &up);
	ok(down == 0 && up == 0, "v120: a third of a detent is no tick");
	ok(raw_last_axis(&raw) && raw.value120 == 40,
	   "v120: the raw stream carries the frame's own 40");
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(40, 5.0, 20);
	wheels(&down, &up);
	ok(down == 0, "v120: two thirds is no tick");
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(40, 5.0, 30);
	wheels(&down, &up);
	ok(down == 1 && up == 0, "v120: the third third is one tick down");
	K.wheel_last_ms = 0;
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(240, 30.0, 40);
	wheels(&down, &up);
	ok(down == 1, "v120: two detents in one frame are one tick");
	K.wheel_last_ms = 0;
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(60, 7.5, 50);
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(-60, -7.5, 60);
	wheels(&down, &up);
	ok(down == 0 && up == 0, "v120: half one way, half back: nothing");
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(-60, -7.5, 70);
	wheels(&down, &up);
	ok(up == 1 && down == 0,
	   "v120: a reversal starts the count again, and a whole one ticks up");
	pt_axis_reldir(NULL, NULL, WL_POINTER_AXIS_VERTICAL_SCROLL,
		       WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED);
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(120, 15.0, 80);
	ok(raw_last_axis(&raw) && (raw.flags & KT_RAW_INVERTED),
	   "v9: natural scrolling reaches the raw stream");
	pt_axis_reldir(NULL, NULL, WL_POINTER_AXIS_VERTICAL_SCROLL,
		       WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
	/* The gate: the same direction again at once is a notch counted
	 * twice. */
	wheels(&down, &up);
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	wframe(120, 15.0, 81);
	wheels(&down, &up);
	ok(down == 0, "the gate drops a second detent inside its window");

	/* ── a version 5 seat: the discrete count ── */
	K.seat_ver = 5;
	K.wheel_last_ms = 0;
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_WHEEL);
	pt_axis_disc(NULL, NULL, WL_POINTER_AXIS_VERTICAL_SCROLL, 2);
	wframe(0, 30.0, 90);
	wheels(&down, &up);
	ok(down == 1, "v5: discrete 2 in one frame is one tick");
	ok(raw_last_axis(&raw) && raw.value120 == 240,
	   "v5: the raw count is the discrete one times 120");

	/* ── a finger: counted, never gated ── */
	K.seat_ver = 8;
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_FINGER);
	wframe(0, 12.0, 100);
	pt_axis_src(NULL, NULL, WL_POINTER_AXIS_SOURCE_FINGER);
	wframe(0, 12.0, 101);
	wheels(&down, &up);
	ok(down == 2, "a finger's two ticks a millisecond apart are both kept");
	/* Fast, then held still for a fifth of a second before the lift. */
	finger(200, 8, 20.0);
	wheels(&down, &up);
	pt_axis_stop(NULL, NULL, 470, WL_POINTER_AXIS_VERTICAL_SCROLL);
	ok(!K.coast_on, "a finger that stopped before it lifted: no coast");

	/* ── the coast ── */
	skew = 30000;
	finger(1000, 8, 12.0);
	wheels(&down, &up);
	pt_axis_stop(NULL, NULL, 1075, WL_POINTER_AXIS_VERTICAL_SCROLL);
	ok(K.coast_on && K.coast_v > 1.0 && K.coast_v < 1.6,
	   "a flick at 1.2 units/ms coasts at its release speed");
	total = first = last = steps = 0;
	for (int i = 0; i < 60 && K.coast_on; i++) {
		skew += 50;
		coast_pump();
		wheels(&down, &up);
		ok(up == 0, "the coast goes the way the finger went");
		if (!steps)
			first = down;
		last = down;
		total += down;
		steps++;
	}
	ok(!K.coast_on, "the coast ends");
	ok(steps > 10 && steps * 50 <= KWL_COAST_MAX_MS,
	   "and ends by slowing down, not by the time limit");
	ok(first >= 4 && last <= 1 && first > last, "it slows");
	/* v * tau / tick: 1.28 * 250 / 10, less the ticks' remainders. */
	ok(total >= 20 && total <= 40, "a quarter second of the release speed");

	finger(5000, 8, -12.0);
	wheels(&down, &up);
	pt_axis_stop(NULL, NULL, 5075, WL_POINTER_AXIS_VERTICAL_SCROLL);
	skew += 30;
	coast_pump();
	wheels(&down, &up);
	ok(up > 0 && down == 0, "a flick up coasts up");
	pt_button(NULL, NULL, 1, 5100, 0x110, WL_POINTER_BUTTON_STATE_PRESSED);
	ok(!K.coast_on, "a press stops the coast");
	skew += 50;
	coast_pump();
	wheels(&down, &up);
	ok(up == 0 && down == 0, "and nothing more arrives");
	pt_button(NULL, NULL, 2, 5110, 0x110, WL_POINTER_BUTTON_STATE_RELEASED);

	finger(6000, 8, 1.0);
	pt_axis_stop(NULL, NULL, 6075, WL_POINTER_AXIS_VERTICAL_SCROLL);
	ok(!K.coast_on, "a finger too slow to fling: no coast");

	/* A wait during a coast answers with its ticks or runs its length;
	 * a coast step is never handed back as the caller's timeout. */
	skew = 0;
	finger(7000, 8, 12.0);
	wheels(&down, &up);
	pt_axis_stop(NULL, NULL, 7075, WL_POINTER_AXIS_VERTICAL_SCROLL);
	{
		int bad = 0, evs = 0;
		int64_t t0 = now_ms();

		while (now_ms() - t0 < 300) {
			int r = poll_ms(&ev, 60, &took);

			if (r)
				evs++;
			else if (took < 55)
				bad++;
		}
		ok(evs > 0, "a wait during a coast gets its ticks");
		ok(!bad, "and none returns before its time without an event");
	}
	pt_leave(NULL, NULL, 0, NULL);
	ok(!K.coast_on, "the pointer leaving stops the coast");
	wheels(&down, &up);

	conf_write(confdir, "motion = no\n");
	{
		struct timespec ts[2] = { { 0, UTIME_OMIT }, { 45678, 0 } };
		char p[600];

		snprintf(p, sizeof(p), "%s/kdos/comp.conf", confdir);
		utimensat(AT_FDCWD, p, ts, 0);
	}
	setenv("XDG_CONFIG_HOME", confdir, 1);
	finger(8000, 8, 12.0);
	pt_axis_stop(NULL, NULL, 8075, WL_POINTER_AXIS_VERTICAL_SCROLL);
	ok(!K.coast_on, "motion = no: no coast");
}

int main(int argc, char **argv)
{
	KtuiEvent ev;
	KtuiAnim a;
	int64_t took;
	int r;

	if (argc < 2) {
		fprintf(stderr, "usage: tickcheck <scratch dir>\n");
		return 2;
	}
	if (pipe(pipefd) != 0 || fcntl(pipefd[0], F_SETFL, O_NONBLOCK) != 0)
		return 2;
	setenv("XDG_CONFIG_HOME", argv[1], 1);
	conf_write(argv[1], "#motion = no\n");

	K.display = (struct wl_display *)&fake_display;
	K.surface = (struct wl_surface *)&fake_surface;
	K.configured = 1;
	K.paste_fd = -1;
	for (int i = 0; i < KWL_COPY_SENDS; i++)
		copy_send[i].fd = -1;
	ktui_backend_set(&kwl_backend);
	ktui_anim_set_motion_fn(kwl_conf_motion);
	ktui_anim_set_clock(anim_clock);

	/* ── idle ── */
	r = poll_ms(&ev, 60, &took);
	ok(r == 0 && took >= 55, "idle: the wait runs its whole length");
	ok(commits == 0 && frames_asked == 0,
	   "idle: no commit and no frame asked for");
	/* An ordinary commit's callback, answered: no tick. */
	K.frame_cb = (struct wl_callback *)calloc(1, 16);
	K.frame_at_ms = now_ms();
	present();
	r = poll_ms(&ev, 60, &took);
	ok(r == 0 && ev.type != KT_EVT_TICK,
	   "idle: an answered frame callback is no tick");
	ok(!K.frame_cb && !K.tick_due, "idle: the callback is gone, nothing owed");
	draw();
	ok(!K.anim_frame, "idle: a draw owes no frame");

	/* ── an animation ── */
	ktui_anim_start(&a, 0.0f, 1.0f, 5000, KT_EASE_OUT, 0);
	ok(!a.still && ktui_anim_live(), "a commented `#motion = no` is no setting");
	draw();
	ok(K.anim_frame, "a frame drawn inside the window owes the next one");
	present();	/* the socket is readable before any frame is asked */
	answer_frame = 0;
	r = poll_ms(&ev, 1000, &took);
	ok(commits == 1 && frames_asked == 1,
	   "live, nothing in flight: one empty commit asks for a frame");
	/* That wake carried no frame: the wait goes on, cut to the stall. */
	present();
	r = poll_ms(&ev, 1000, &took);
	ok(r == 1 && ev.type == KT_EVT_TICK, "the frame answered is a tick");
	ok(took < 50, "and it comes at once, not at the timeout");
	ok(commits == 1 && frames_asked == 1, "no second request while one was out");
	draw();
	/* Nothing answers now: the stall bound turns it into a tick. */
	r = poll_ms(&ev, 1000, &took);
	ok(frames_asked == 2, "the next wait asks for the next frame");
	ok(r == 1 && ev.type == KT_EVT_TICK && took >= KWL_FRAME_STALL_MS - 5 &&
	   took < 400, "an unanswered frame stalls into a tick");
	ok(!K.frame_cb, "and the stalled callback is dropped");

	/* ── the end ── */
	draw();			/* the last frame inside the window */
	skew = 10000;		/* ...and the window is over */
	ok(!ktui_anim_live() && K.anim_frame, "past the end: one frame still owed");
	present();
	r = poll_ms(&ev, 1000, &took);
	if (r == 0) {		/* the wake that only asked */
		present();
		r = poll_ms(&ev, 1000, &took);
	}
	ok(r == 1 && ev.type == KT_EVT_TICK, "the owed frame is ticked");
	ok(!K.anim_frame, "and paid: nothing more is owed");
	/* A loop that does not draw on it is not asked again. */
	{
		int c0 = commits, f0 = frames_asked;

		if (K.frame_cb) {
			wl_callback_destroy(K.frame_cb);
			K.frame_cb = NULL;
		}
		r = poll_ms(&ev, 60, &took);
		ok(r == 0 && took >= 55 && commits == c0 && frames_asked == f0,
		   "after the owed tick: silent, the whole wait, no commit");
	}
	draw();
	ok(!K.anim_frame, "a frame drawn after the end owes nothing");

	/* ── motion off ── */
	skew = 20000;
	conf_write(argv[1], "motion = yes\nmotion = no\n");
	/* mtime granularity: make sure the new file reads as new */
	{
		struct timespec ts[2] = { { 0, UTIME_OMIT }, { 12345, 0 } };
		char p[600];

		snprintf(p, sizeof(p), "%s/kdos/comp.conf", argv[1]);
		utimensat(AT_FDCWD, p, ts, 0);
	}
	ok(!kwl_conf_motion(), "motion: the last line wins (no)");
	ktui_anim_start(&a, 0.0f, 1.0f, 5000, KT_EASE_OUT, 0);
	ok(a.still && !ktui_anim_live(), "motion = no: still, nothing live");
	{
		int c0 = commits;

		r = poll_ms(&ev, 60, &took);
		ok(r == 0 && took >= 55 && commits == c0,
		   "motion = no: no ticks, no commits");
	}
	conf_write(argv[1], "motion = no\n  motion = OFF\nmotion=on\n");
	{
		struct timespec ts[2] = { { 0, UTIME_OMIT }, { 23456, 0 } };
		char p[600];

		snprintf(p, sizeof(p), "%s/kdos/comp.conf", argv[1]);
		utimensat(AT_FDCWD, p, ts, 0);
	}
	ok(kwl_conf_motion(), "motion: a changed file is read again (on)");
	conf_write(argv[1], "motion = sideways\n");
	{
		struct timespec ts[2] = { { 0, UTIME_OMIT }, { 34567, 0 } };
		char p[600];

		snprintf(p, sizeof(p), "%s/kdos/comp.conf", argv[1]);
		utimensat(AT_FDCWD, p, ts, 0);
	}
	ok(kwl_conf_motion(), "motion: a value that is neither is the default (on)");
	setenv("XDG_CONFIG_HOME", "/nonexistent-tickcheck", 1);
	ok(kwl_conf_motion(), "motion: no file is on");

	wheel_checks(argv[1]);

	printf("tickcheck: %d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
