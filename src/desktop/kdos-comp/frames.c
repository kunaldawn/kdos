/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — frame timing, reported to whoever asks
 *
 * Half of stutter attribution. The compositor is the only thing on the machine
 * that knows a frame was late; the other half — WHO was hogging the CPU or the
 * disk at that instant — is knowable from /proc and from the container layer,
 * and is `kdos stutter`'s. Neither half is interesting alone, which is exactly
 * why nobody has joined them: the closest prior art (Latency Lens) reads PSI and
 * says outright that it "cannot identify which specific process caused a frame
 * miss."
 *
 * WHAT COUNTS AS A MISS. Presentation events are the truth when the backend
 * emits them: `wlr_output.events.present` carries the moment the content turned
 * into light and the refresh interval, so a gap of more than one and a half
 * intervals means frames did not reach the screen. The headless and Wayland
 * backends do not present, so the fallback is the frame clock itself — the gap
 * between successive frame events. Both are reported with a `source` field
 * rather than silently averaged, because they do not mean the same thing: a
 * present gap is what the user SAW, a frame gap is what the compositor was
 * given.
 *
 * The compositor also times its own render, and that number is what separates
 * the two explanations a user cares about: `render_ms` near the frame budget
 * means the desktop itself was slow (a shader, a huge damage region, a software
 * renderer); `render_ms` tiny with a big `late_ms` means the compositor was
 * ready and something ELSE had the machine.
 *
 * THE REPORTING SOCKET MUST NEVER SLOW THE FRAME LOOP. It is
 * `$XDG_RUNTIME_DIR/kdos-frames.sock`, it is non-blocking on both ends, and a
 * client that cannot keep up loses lines rather than stalling the compositor —
 * a telemetry channel that can block the thing it measures is a bug generator,
 * not an instrument. Same reason there is no history: a consumer that connects
 * late has missed what happened, and saying so is honest; a ring buffer would
 * make it look otherwise.
 *
 * Not a Wayland protocol on purpose. This is one distro's diagnostic channel
 * between two of its own programs, `presentation-time` already exists for
 * clients that want their own timings, and inventing a protocol extension for
 * it would be a claim to generality this does not have.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "kdos-comp.h"

/*
 * A gap has to exceed this many refresh intervals to count. 1.5 rather than 1.0
 * because a frame that lands a hair late is a rounding artefact of when the
 * clock was read, not a stutter anyone can see; anything past one and a half
 * intervals means at least one refresh went by with nothing new on it.
 */
#define KC_MISS_FACTOR 1.5

/* With no refresh interval to go on — a headless output, or a present event
 * with `refresh` 0 — assume 60 Hz. Stated rather than hidden: the alternative
 * is reporting nothing at all on those outputs. */
#define KC_ASSUMED_REFRESH_NS 16666667L

struct kc_frames_client {
	struct wl_list link;
	int fd;
	struct wl_event_source *src;
	long dropped_lines;
};

struct kc_frames {
	struct kc_server *server;
	int listen_fd;
	struct wl_event_source *listen_src;
	char path[256];
	struct wl_list clients;
	long misses, frames;
};

static int64_t now_ns(clockid_t clk)
{
	struct timespec ts;
	clock_gettime(clk, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* ── the socket ────────────────────────────────────────────────────────── */

static void client_drop(struct kc_frames_client *c)
{
	if (c->src)
		wl_event_source_remove(c->src);
	close(c->fd);
	wl_list_remove(&c->link);
	free(c);
}

static int client_readable(int fd, uint32_t mask, void *data)
{
	struct kc_frames_client *c = data;
	(void)fd;
	/* Nothing is ever expected FROM a consumer; this handler exists so a
	 * consumer that goes away is noticed and reaped instead of accumulating.
	 */
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		client_drop(c);
		return 0;
	}
	char buf[64];
	ssize_t n = read(c->fd, buf, sizeof(buf));
	if (n <= 0 && !(n < 0 && (errno == EAGAIN || errno == EINTR)))
		client_drop(c);
	return 0;
}

static void broadcast(struct kc_frames *f, const char *line, size_t len)
{
	struct kc_frames_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &f->clients, link) {
		ssize_t n = write(c->fd, line, len);
		if (n == (ssize_t)len)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			/* The consumer is behind. Its problem, not ours. */
			c->dropped_lines++;
			continue;
		}
		client_drop(c);
	}
}

static int listen_ready(int fd, uint32_t mask, void *data)
{
	struct kc_frames *f = data;
	(void)mask;
	int cfd = accept(fd, NULL, NULL);
	if (cfd < 0)
		return 0;
	int fl = fcntl(cfd, F_GETFL, 0);
	fcntl(cfd, F_SETFL, (fl < 0 ? 0 : fl) | O_NONBLOCK);
	fcntl(cfd, F_SETFD, FD_CLOEXEC);

	struct kc_frames_client *c = calloc(1, sizeof(*c));
	if (!c) {
		close(cfd);
		return 0;
	}
	c->fd = cfd;
	c->src = wl_event_loop_add_fd(wl_display_get_event_loop(f->server->display),
				      cfd, WL_EVENT_READABLE, client_readable, c);
	wl_list_insert(&f->clients, &c->link);

	/*
	 * A greeting, so a consumer knows what it is attached to and — more
	 * usefully — knows it attached at all. It also carries the counters, so
	 * `kdos stutter` can say "42 misses since the session started" without
	 * having been running for them.
	 */
	char line[320];
	int n = snprintf(line, sizeof(line),
		"{\"event\":\"hello\",\"kdos_comp\":\"%s\",\"mono_ms\":%.3f,"
		"\"misses\":%ld,\"frames\":%ld}\n",
		WLR_VERSION_STR, now_ns(CLOCK_MONOTONIC) / 1e6, f->misses,
		f->frames);
	if (n > 0)
		broadcast(f, line, (size_t)n);
	return 0;
}

/* ── the measurement ───────────────────────────────────────────────────── */

static void report(struct kc_output *o, int64_t gap_ns, int64_t refresh_ns,
		   const char *source)
{
	struct kc_frames *f = o->server->frames;
	int dropped = (int)(gap_ns / refresh_ns) - 1;
	if (dropped < 1)
		dropped = 1;
	f->misses++;

	/* Logged as well as broadcast, at DEBUG: a session log with a stutter in
	 * it is worth something even when nothing was listening at the time. */
	wlr_log(WLR_DEBUG, "frames: %s missed %d frame%s on %s (%.1f ms, render "
		"%.2f ms)", source, dropped, dropped == 1 ? "" : "s",
		o->wlr_output->name, gap_ns / 1e6, o->render_ns / 1e6);

	if (wl_list_empty(&f->clients))
		return;

	char line[512];
	int n = snprintf(line, sizeof(line),
		"{\"event\":\"miss\",\"mono_ms\":%.3f,\"wall_ms\":%.3f,"
		"\"output\":\"%s\",\"source\":\"%s\",\"late_ms\":%.3f,"
		"\"dropped\":%d,\"render_ms\":%.3f,\"refresh_hz\":%.2f}\n",
		now_ns(CLOCK_MONOTONIC) / 1e6, now_ns(CLOCK_REALTIME) / 1e6,
		o->wlr_output->name, source, gap_ns / 1e6, dropped,
		o->render_ns / 1e6, 1e9 / (double)refresh_ns);
	if (n > 0 && n < (int)sizeof(line))
		broadcast(f, line, (size_t)n);
}

/*
 * One tick of a frame clock, whichever clock it was.
 *
 * `when_ns` is the moment the frame happened, `refresh_ns` the interval if the
 * backend told us one. The state is per output and per SOURCE, because the two
 * clocks tick at different moments and interleaving them would manufacture gaps
 * that never existed.
 */
static void tick(struct kc_output *o, int64_t when_ns, int64_t refresh_ns,
		 int64_t *last, const char *source)
{
	struct kc_frames *f = o->server->frames;
	if (!f)
		return;
	if (refresh_ns <= 0)
		refresh_ns = o->refresh_ns > 0 ? o->refresh_ns
					       : KC_ASSUMED_REFRESH_NS;
	o->refresh_ns = refresh_ns;
	f->frames++;

	int64_t prev = *last;
	*last = when_ns;
	if (prev <= 0 || when_ns <= prev)
		return;			/* first frame, or a clock that moved */

	int64_t gap = when_ns - prev;
	if ((double)gap > KC_MISS_FACTOR * (double)refresh_ns)
		report(o, gap, refresh_ns, source);
}

void kc_frames_present(struct kc_output *o,
		       const struct wlr_output_event_present *ev)
{
	if (!ev->presented)
		return;
	int64_t when = (int64_t)ev->when.tv_sec * 1000000000LL + ev->when.tv_nsec;
	tick(o, when, ev->refresh, &o->last_present_ns, "present");
	/* A backend that presents is the authority; the frame-clock fallback
	 * would otherwise double-count every miss. */
	o->presenting = true;
}

void kc_frames_frame(struct kc_output *o)
{
	if (o->presenting)
		return;
	tick(o, now_ns(CLOCK_MONOTONIC), 0, &o->last_frame_ns, "frame");
}

void kc_frames_rendered(struct kc_output *o, int64_t ns)
{
	o->render_ns = ns;
}

int64_t kc_frames_now(void)
{
	return now_ns(CLOCK_MONOTONIC);
}

/* ── setup ─────────────────────────────────────────────────────────────── */

void kc_frames_init(struct kc_server *s)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt) {
		wlr_log(WLR_INFO, "frames: no XDG_RUNTIME_DIR, not reporting "
				  "frame timing");
		return;
	}

	struct kc_frames *f = calloc(1, sizeof(*f));
	if (!f)
		return;
	f->server = s;
	f->listen_fd = -1;
	wl_list_init(&f->clients);
	snprintf(f->path, sizeof(f->path), "%s/kdos-frames.sock", rt);

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	if (strlen(f->path) >= sizeof(addr.sun_path)) {
		wlr_log(WLR_ERROR, "frames: %s is too long for a unix socket",
			f->path);
		free(f);
		return;
	}
	strcpy(addr.sun_path, f->path);

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		free(f);
		return;
	}
	/* A stale socket from a session that did not shut down cleanly would
	 * make every future session fail to bind. Removing it is safe: this path
	 * is per-user and per-boot, and a live session holds the only listener. */
	unlink(f->path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(fd, 4) < 0) {
		wlr_log(WLR_ERROR, "frames: cannot listen on %s: %s", f->path,
			strerror(errno));
		close(fd);
		free(f);
		return;
	}
	f->listen_fd = fd;
	f->listen_src = wl_event_loop_add_fd(wl_display_get_event_loop(s->display),
					     fd, WL_EVENT_READABLE, listen_ready, f);
	s->frames = f;
	wlr_log(WLR_INFO, "frames: reporting on %s", f->path);
}

void kc_frames_free(struct kc_server *s)
{
	struct kc_frames *f = s->frames;
	if (!f)
		return;
	struct kc_frames_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &f->clients, link)
		client_drop(c);
	if (f->listen_src)
		wl_event_source_remove(f->listen_src);
	if (f->listen_fd >= 0)
		close(f->listen_fd);
	if (*f->path)
		unlink(f->path);
	free(f);
	s->frames = NULL;
}
