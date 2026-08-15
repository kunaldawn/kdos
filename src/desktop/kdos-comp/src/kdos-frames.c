// SPDX-License-Identifier: GPL-2.0-only
/*
 * Frame timing, reported to whoever asks — ported from the pre-fork
 * kdos-comp. Half of `kdos stutter`: the compositor is the only thing
 * that knows a frame was late; /proc knows who was busy. See the
 * pre-fork header comment for the full rationale (Latency Lens's
 * "cannot identify which specific process caused a frame miss" is the
 * sentence this finishes).
 *
 * Rules carried over verbatim:
 * - Presentation events are the truth where the backend emits them;
 *   headless/nested backends do not present, so the frame clock is the
 *   fallback, and the two are reported with a `source` field rather
 *   than averaged — a present gap is what the user SAW, a frame gap is
 *   what the compositor was GIVEN.
 * - The socket ($XDG_RUNTIME_DIR/kdos-frames.sock) must never slow the
 *   frame loop: non-blocking both ends, a slow consumer loses lines,
 *   and there is no history.
 * - Threshold is 1.5 refresh intervals.
 *
 * Fork adaptation: per-output state lives in this file's own records
 * keyed by struct output *, with our own listeners on the wlr_output's
 * present and destroy signals — labwc's structs stay untouched. The
 * only upstream hooks are in handle_output_frame() (frame tick + render
 * timing around the scene commit) and handle_new_output() (record
 * creation).
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <wlr/version.h>

#include "kdos.h"
#include "labwc.h"
#include "output.h"

#define KF_MISS_FACTOR 1.5
#define KF_ASSUMED_REFRESH_NS 16666667L

struct kf_client {
	struct wl_list link;
	int fd;
	struct wl_event_source *src;
	long dropped_lines;
};

struct kf_output {
	struct wl_list link;
	struct output *output;
	struct wl_listener present;
	struct wl_listener destroy;
	int64_t last_present_ns, last_frame_ns;
	int64_t refresh_ns, render_ns;
	bool presenting;
	/*
	 * Set the moment a frame event submits nothing: this desktop
	 * renders SPARSELY by design, so the next gap is idle time and
	 * must not be reported as dropped frames. Cleared by the next
	 * real submission — a stall while something wanted to draw keeps
	 * counting, which is what the SIGSTOP proof measures.
	 */
	bool quiesced;
};

struct kf {
	int listen_fd;
	struct wl_event_source *listen_src;
	char path[256];
	struct wl_list clients;
	struct wl_list outputs;
	long misses, frames;
};

static struct kf *kf;

static int64_t
now_ns(clockid_t clk)
{
	struct timespec ts;
	clock_gettime(clk, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* ── the socket ─────────────────────────────────────────────────── */

static void
client_drop(struct kf_client *c)
{
	if (c->src) {
		wl_event_source_remove(c->src);
	}
	close(c->fd);
	wl_list_remove(&c->link);
	free(c);
}

static int
client_readable(int fd, uint32_t mask, void *data)
{
	struct kf_client *c = data;
	(void)fd;
	/* nothing is expected FROM a consumer; this reaps the departed */
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		client_drop(c);
		return 0;
	}
	char buf[64];
	ssize_t n = read(c->fd, buf, sizeof(buf));
	if (n <= 0 && !(n < 0 && (errno == EAGAIN || errno == EINTR))) {
		client_drop(c);
	}
	return 0;
}

static void
broadcast(const char *line, size_t len)
{
	struct kf_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &kf->clients, link) {
		ssize_t n = write(c->fd, line, len);
		if (n == (ssize_t)len) {
			continue;
		}
		/* EWOULDBLOCK is EAGAIN on linux; naming both is a
		 * -Wlogical-op warning, not extra portability */
		if (n < 0 && errno == EAGAIN) {
			/* the consumer is behind — its problem, not ours;
			 * NOTHING went out, so its stream is still whole */
			c->dropped_lines++;
			continue;
		}
		/*
		 * A short write already tore an NDJSON line in half, and
		 * healing it would mean blocking the frame loop on this
		 * consumer. The stream is unusable; the client goes.
		 */
		if (n >= 0) {
			c->dropped_lines++;
		}
		client_drop(c);
	}
}

static int
listen_ready(int fd, uint32_t mask, void *data)
{
	(void)mask;
	(void)data;
	int cfd = accept(fd, NULL, NULL);
	if (cfd < 0) {
		return 0;
	}
	int fl = fcntl(cfd, F_GETFL, 0);
	fcntl(cfd, F_SETFL, (fl < 0 ? 0 : fl) | O_NONBLOCK);
	fcntl(cfd, F_SETFD, FD_CLOEXEC);

	struct kf_client *c = calloc(1, sizeof(*c));
	if (!c) {
		close(cfd);
		return 0;
	}
	c->fd = cfd;
	c->src = wl_event_loop_add_fd(
		wl_display_get_event_loop(server.wl_display), cfd,
		WL_EVENT_READABLE, client_readable, c);
	wl_list_insert(&kf->clients, &c->link);

	/*
	 * Greeting: proves attachment, carries the session counters. To
	 * THIS consumer only — broadcasting it put a second hello into the
	 * middle of every stream already running, which is a line no
	 * reader of an append-only protocol should have to expect.
	 */
	char line[384];
	int n = snprintf(line, sizeof(line),
		"{\"event\":\"hello\",\"kdos_comp\":\"%s\",\"wlroots\":\"%s\","
		"\"mono_ms\":%.3f,\"misses\":%ld,\"frames\":%ld}\n",
		LABWC_VERSION, WLR_VERSION_STR,
		now_ns(CLOCK_MONOTONIC) / 1e6, kf->misses, kf->frames);
	/* whole or not at all — a torn hello is a torn stream */
	if (n > 0 && n < (int)sizeof(line)
			&& write(cfd, line, (size_t)n) != (ssize_t)n) {
		client_drop(c);
	}
	return 0;
}

/* ── the measurement ────────────────────────────────────────────── */

static void
report(struct kf_output *ko, int64_t gap_ns, int64_t refresh_ns,
		const char *source)
{
	int dropped = (int)(gap_ns / refresh_ns) - 1;
	if (dropped < 1) {
		dropped = 1;
	}
	kf->misses++;

	/* logged too, at DEBUG: a session log with a stutter in it is
	 * worth something even when nothing was listening */
	wlr_log(WLR_DEBUG, "frames: %s missed %d frame%s on %s "
		"(%.1f ms, render %.2f ms)", source, dropped,
		dropped == 1 ? "" : "s", ko->output->wlr_output->name,
		gap_ns / 1e6, ko->render_ns / 1e6);

	if (wl_list_empty(&kf->clients)) {
		return;
	}

	char line[512];
	int n = snprintf(line, sizeof(line),
		"{\"event\":\"miss\",\"mono_ms\":%.3f,\"wall_ms\":%.3f,"
		"\"output\":\"%s\",\"source\":\"%s\",\"late_ms\":%.3f,"
		"\"dropped\":%d,\"render_ms\":%.3f,\"refresh_hz\":%.2f}\n",
		now_ns(CLOCK_MONOTONIC) / 1e6, now_ns(CLOCK_REALTIME) / 1e6,
		ko->output->wlr_output->name, source, gap_ns / 1e6, dropped,
		ko->render_ns / 1e6, 1e9 / (double)refresh_ns);
	if (n > 0 && n < (int)sizeof(line)) {
		broadcast(line, (size_t)n);
	}
}

/*
 * One tick of a frame clock. State is per output and per SOURCE —
 * the two clocks tick at different moments and interleaving them
 * would manufacture gaps that never existed.
 */
static void
tick(struct kf_output *ko, int64_t when_ns, int64_t refresh_ns,
		int64_t *last, const char *source)
{
	if (refresh_ns <= 0) {
		refresh_ns = ko->refresh_ns > 0 ? ko->refresh_ns
			: KF_ASSUMED_REFRESH_NS;
	}
	ko->refresh_ns = refresh_ns;
	kf->frames++;

	int64_t prev = *last;
	*last = when_ns;
	if (prev <= 0 || when_ns <= prev) {
		return;		/* first frame, or a clock that moved */
	}
	if (ko->quiesced) {
		return;		/* the gap was idleness, not lateness */
	}

	int64_t gap = when_ns - prev;
	if ((double)gap > KF_MISS_FACTOR * (double)refresh_ns) {
		report(ko, gap, refresh_ns, source);
	}
}

static struct kf_output *
kf_output_get(struct output *output)
{
	struct kf_output *ko;
	wl_list_for_each(ko, &kf->outputs, link) {
		if (ko->output == output) {
			return ko;
		}
	}
	return NULL;
}

static void
handle_present(struct wl_listener *listener, void *data)
{
	struct kf_output *ko = wl_container_of(listener, ko, present);
	struct wlr_output_event_present *ev = data;
	if (!ev->presented) {
		return;
	}
	int64_t when = (int64_t)ev->when.tv_sec * 1000000000LL
		+ ev->when.tv_nsec;
	tick(ko, when, ev->refresh, &ko->last_present_ns, "present");
	/* a backend that presents is the authority; the frame-clock
	 * fallback would double-count every miss */
	ko->presenting = true;
}

static void
handle_kf_output_destroy(struct wl_listener *listener, void *data)
{
	struct kf_output *ko = wl_container_of(listener, ko, destroy);
	(void)data;
	wl_list_remove(&ko->present.link);
	wl_list_remove(&ko->destroy.link);
	wl_list_remove(&ko->link);
	free(ko);
}

/* ── hooks called from upstream files ───────────────────────────── */

void
kdos_frames_output_add(struct output *output)
{
	if (!kf) {
		return;
	}
	struct kf_output *ko = calloc(1, sizeof(*ko));
	if (!ko) {
		return;
	}
	ko->output = output;
	ko->present.notify = handle_present;
	wl_signal_add(&output->wlr_output->events.present, &ko->present);
	ko->destroy.notify = handle_kf_output_destroy;
	wl_signal_add(&output->wlr_output->events.destroy, &ko->destroy);
	wl_list_insert(&kf->outputs, &ko->link);
}

void
kdos_frames_frame(struct output *output)
{
	if (!kf) {
		return;
	}
	struct kf_output *ko = kf_output_get(output);
	if (!ko || ko->presenting) {
		return;
	}
	tick(ko, now_ns(CLOCK_MONOTONIC), 0, &ko->last_frame_ns, "frame");
}

/*
 * From the output-frame hook, AFTER the commit: did this frame event
 * actually put a buffer out? `false` quiesces the output's gap clocks —
 * see the field comment — and the first submission after a quiet spell
 * re-primes the present clock, whose last tick predates the idle gap
 * (the present for the final pre-idle commit can land after the
 * quiescing frame event, which is why clearing a timestamp here alone
 * was not enough).
 */
void
kdos_frames_submit(struct output *output, bool submitted)
{
	if (!kf) {
		return;
	}
	struct kf_output *ko = kf_output_get(output);
	if (!ko) {
		return;
	}
	if (!submitted) {
		ko->quiesced = true;
		return;
	}
	if (ko->quiesced) {
		ko->quiesced = false;
		ko->last_present_ns = 0;
	}
}

void
kdos_frames_render_ns(struct output *output, int64_t ns)
{
	if (!kf) {
		return;
	}
	struct kf_output *ko = kf_output_get(output);
	if (ko) {
		ko->render_ns = ns;
	}
}

int64_t
kdos_frames_now(void)
{
	return now_ns(CLOCK_MONOTONIC);
}

/* ── setup ──────────────────────────────────────────────────────── */

void
kdos_frames_init(void)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt) {
		wlr_log(WLR_INFO, "frames: no XDG_RUNTIME_DIR, not "
			"reporting frame timing");
		return;
	}

	struct kf *f = calloc(1, sizeof(*f));
	if (!f) {
		return;
	}
	f->listen_fd = -1;
	wl_list_init(&f->clients);
	wl_list_init(&f->outputs);
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
	/* a stale socket from an unclean shutdown must not block the
	 * next session: the path is per-user and per-boot */
	unlink(f->path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0
			|| listen(fd, 4) < 0) {
		wlr_log(WLR_ERROR, "frames: cannot listen on %s: %s",
			f->path, strerror(errno));
		close(fd);
		free(f);
		return;
	}
	f->listen_fd = fd;
	kf = f;
	f->listen_src = wl_event_loop_add_fd(
		wl_display_get_event_loop(server.wl_display), fd,
		WL_EVENT_READABLE, listen_ready, f);
	wlr_log(WLR_INFO, "frames: reporting on %s", f->path);
}

void
kdos_frames_finish(void)
{
	if (!kf) {
		return;
	}
	struct kf_client *c, *ctmp;
	wl_list_for_each_safe(c, ctmp, &kf->clients, link) {
		client_drop(c);
	}
	struct kf_output *ko, *otmp;
	wl_list_for_each_safe(ko, otmp, &kf->outputs, link) {
		wl_list_remove(&ko->present.link);
		wl_list_remove(&ko->destroy.link);
		wl_list_remove(&ko->link);
		free(ko);
	}
	if (kf->listen_src) {
		wl_event_source_remove(kf->listen_src);
	}
	if (kf->listen_fd >= 0) {
		close(kf->listen_fd);
	}
	if (*kf->path) {
		unlink(kf->path);
	}
	free(kf);
	kf = NULL;
}
