/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-record — the desktop, into a file, through the portal
 *
 * THE PORTAL AND NOT THE BACKEND, even though the backend is ours. `Start` is
 * what turns a request into a running cast and hands back the PipeWire node it
 * registered, and the front end is what picks the backend per desktop — `kdos`
 * on the console, `wlr` under the compositor. Going round it would mean a
 * second answer to "who may record this screen" and a recorder that worked on
 * one of the two desktops.
 *
 * A SESSION BELONGS TO A CONNECTION, which is why this is a program and not a
 * shell script. `xdg-desktop-portal` keys a session by the unique name that
 * created it and closes the session when that name goes away, so three
 * `gdbus call` invocations are three connections: the second one is told
 * `Invalid session` and the first session is already gone. One connection has
 * to stay open for the whole recording.
 *
 * A PORTAL REPLY IS A SIGNAL, NOT A RETURN VALUE. Every call returns a Request
 * object path and answers later with `Response` on it, so the match is
 * installed BEFORE the call — a signal that arrives with no match is gone, and
 * nothing replays it.
 *
 * HOST-SIDE ONLY. A box has no session bus of its own; the portal is what an
 * application inside one uses, and it is the same three calls.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* basu on the target; libsystemd's sd-bus is the same API and is what a
 * host-side syntax check finds. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#define PORTAL_BUS   "org.freedesktop.portal.Desktop"
#define PORTAL_OBJ   "/org/freedesktop/portal/desktop"
#define PORTAL_CAST  "org.freedesktop.portal.ScreenCast"
#define PORTAL_REQ   "org.freedesktop.portal.Request"

/* Five seconds is a portal that is not there: one on a local bus answers in
 * milliseconds, and a longer wait only makes a broken session look slow. */
#define REPLY_TIMEOUT_US (5ULL * 1000000ULL)

/* `SelectSources` IS ANSWERED BY A PERSON, and a person is not a timeout. The
 * wlr backend's chooser is `slurp` — it covers the screen and waits for the
 * pointer to pick an output — so a deadline of seconds cancels every recording
 * before anybody has decided which screen to record, and the failure reads as
 * `no answer to SelectSources` rather than as the question it really was. Two
 * minutes is long enough to choose and short enough that a chooser nothing
 * ever answers does not hang the terminal it was typed in. */
#define CHOOSE_TIMEOUT_US (120ULL * 1000000ULL)

/* `Start` is the exception, and it is not a slow bus — it is a whole display
 * being brought up. The backend forks a view onto the session, waits for it to
 * attach, load a font and register a PipeWire node, and only then answers. */
#define START_TIMEOUT_US (30ULL * 1000000ULL)

/* What one Response is allowed to tell us. `path` is the request it must be
 * on, because the match is installed for every request this connection makes
 * and two are in flight only by accident. */
typedef struct {
	const char *path;
	int done;
	uint32_t code;
	char session[256];
	uint32_t node;
	int have_node;
} Reply;

static volatile sig_atomic_t interrupted;

static void on_signal(int sig)
{
	(void)sig;
	interrupted = 1;
}

/* The `results` dictionary of a Response, which is the only part of it that
 * differs between the three calls: a session handle from CreateSession and a
 * stream list from Start. Everything else is skipped rather than refused — the
 * portal is allowed to say more than we read. */
static int read_results(sd_bus_message *m, Reply *rp)
{
	int r;

	r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0)
		return r;
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		const char *key = NULL;

		r = sd_bus_message_read(m, "s", &key);
		if (r < 0)
			return r;
		if (!strcmp(key, "session_handle")) {
			const char *v = NULL;

			r = sd_bus_message_read(m, "v", "s", &v);
			if (r < 0)
				return r;
			snprintf(rp->session, sizeof(rp->session), "%s",
				 v ? v : "");
		} else if (!strcmp(key, "streams")) {
			/*
			 * `a(ua{sv})`, and the number in front of each
			 * struct's properties is the node. This backend
			 * answers with exactly one; the first is taken and the
			 * rest skipped, because a pipeline reads one node.
			 */
			r = sd_bus_message_enter_container(m, 'v',
							   "a(ua{sv})");
			if (r < 0)
				return r;
			r = sd_bus_message_enter_container(m, 'a', "(ua{sv})");
			if (r < 0)
				return r;
			if (sd_bus_message_enter_container(m, 'r',
							   "ua{sv}") > 0) {
				uint32_t n = 0;

				if (sd_bus_message_read(m, "u", &n) >= 0) {
					rp->node = n;
					rp->have_node = 1;
				}
				sd_bus_message_skip(m, "a{sv}");
				sd_bus_message_exit_container(m);
			}
			sd_bus_message_skip(m, NULL);
			sd_bus_message_exit_container(m);
			sd_bus_message_exit_container(m);
		} else {
			r = sd_bus_message_skip(m, "v");
			if (r < 0)
				return r;
		}
		r = sd_bus_message_exit_container(m);
		if (r < 0)
			return r;
	}
	if (r < 0)
		return r;
	return sd_bus_message_exit_container(m);
}

static int on_response(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	Reply *rp = userdata;
	const char *path = sd_bus_message_get_path(m);

	(void)e;
	if (!path || !rp->path || strcmp(path, rp->path))
		return 0;
	if (sd_bus_message_read(m, "u", &rp->code) < 0)
		return 0;
	if (rp->code == 0)
		read_results(m, rp);
	rp->done = 1;
	return 0;
}

/* Pump the connection until the Response lands or the wait runs out. */
static int wait_reply(sd_bus *bus, Reply *rp, uint64_t timeout_us)
{
	while (!rp->done) {
		int r = sd_bus_process(bus, NULL);

		if (r < 0)
			return r;
		if (r > 0)
			continue;
		r = sd_bus_wait(bus, timeout_us);
		if (r < 0)
			return r;
		if (r == 0)
			return -ETIMEDOUT;
	}
	return 0;
}

/* The options every call takes, minus the ones only one of them does. */
static int append_token(sd_bus_message *m, const char *token)
{
	return sd_bus_message_append(m, "{sv}", "handle_token", "s", token);
}

static int is_element(const char *name)
{
	pid_t p = fork();
	int st = 0;

	if (p < 0)
		return 0;
	if (p == 0) {
		int null = open("/dev/null", O_WRONLY);

		if (null >= 0) {
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
		}
		execlp("gst-inspect-1.0", "gst-inspect-1.0", name,
		       (char *)NULL);
		_exit(127);
	}
	while (waitpid(p, &st, 0) < 0 && errno == EINTR)
		;
	return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/*
 * THE ONE MARKER THAT SAYS A RECORDING IS RUNNING.
 *
 * `$XDG_RUNTIME_DIR/kdos/screencast.pid`, the directory the now-playing line
 * already lives in and for the same reason: it is tmpfs, so a marker left
 * behind by a crash cannot outlive the boot it lied about. Two readers — the
 * next invocation, which stops the recording this one names, and the console
 * bar, which draws its lamp.
 *
 * `screencast` AND NOT `recording`: `kdos-rec` records a microphone and the
 * console bar already says RECORDING about a keystroke macro, so the word is
 * three things and the path may only be one.
 */
static int marker_path(char *buf, size_t n)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");

	if (!rt || !*rt)
		return 0;
	return snprintf(buf, n, "%s/kdos/screencast.pid", rt) < (int)n;
}

/*
 * The pid a marker names, or 0 — including when the process it names is gone.
 * A stale marker is a crash and not a recording; taking it for one would mean
 * a stop that stops nothing and a lamp that never goes out.
 */
static pid_t marker_read(const char *path)
{
	char buf[32];
	FILE *f = fopen(path, "re");
	long v = 0;

	if (!f)
		return 0;
	if (!fgets(buf, sizeof(buf), f))
		buf[0] = '\0';
	fclose(f);
	v = strtol(buf, NULL, 10);
	if (v <= 0 || kill((pid_t)v, 0) != 0)
		return 0;
	return (pid_t)v;
}

static int marker_write(const char *path)
{
	char dir[4096];
	char *slash;
	FILE *f;

	snprintf(dir, sizeof(dir), "%s", path);
	slash = strrchr(dir, '/');
	if (slash && slash != dir) {
		*slash = '\0';
		if (mkdir(dir, 0700) != 0 && errno != EEXIST)
			return 0;
	}
	f = fopen(path, "we");
	if (!f)
		return 0;
	fprintf(f, "%d\n", (int)getpid());
	fclose(f);
	return 1;
}

/*
 * The parent directory, made if it is not there. One level only: a path whose
 * grandparent is missing is a typo, and creating a tree from one would hide it.
 */
static void ensure_dir(const char *path)
{
	char buf[4096];
	char *slash;

	snprintf(buf, sizeof(buf), "%s", path);
	slash = strrchr(buf, '/');
	if (!slash || slash == buf)
		return;
	*slash = '\0';
	if (mkdir(buf, 0755) != 0 && errno != EEXIST)
		fprintf(stderr, "kdos-record: %s: %s\n", buf, strerror(errno));
}

static void default_path(char *out, size_t n)
{
	const char *home = getenv("HOME");
	time_t now = time(NULL);
	struct tm tm;
	char leaf[64];

	localtime_r(&now, &tm);
	strftime(leaf, sizeof(leaf), "%Y-%m-%d-%H%M%S.mkv", &tm);
	snprintf(out, n, "%s/Videos/%s", home && *home ? home : ".", leaf);
}

/*
 * `gst-launch-1.0` rather than a pipeline built in process, because the
 * elements are the whole of it and linking GStreamer here would put a second
 * copy of its initialisation in a program that only needs one node read.
 */
static pid_t start_gst(uint32_t node, const char *out)
{
	char path[64];
	char loc[4128];
	const char *av[24];
	int n = 0;
	pid_t p;

	snprintf(path, sizeof(path), "path=%u", node);
	snprintf(loc, sizeof(loc), "location=%s", out);

	av[n++] = "gst-launch-1.0";
	av[n++] = "-e";
	av[n++] = "pipewiresrc";
	av[n++] = path;
	av[n++] = "!";
	av[n++] = "videoconvert";
	av[n++] = "!";
	/* H.264 where the encoder is, VP8 where it is not: both plugin sets
	 * ship, and asking is cheaper than a pipeline that fails at the first
	 * frame. */
	if (is_element("x264enc")) {
		av[n++] = "x264enc";
		av[n++] = "tune=zerolatency";
		av[n++] = "speed-preset=veryfast";
	} else {
		av[n++] = "vp8enc";
		av[n++] = "deadline=1";
	}
	av[n++] = "!";
	av[n++] = "matroskamux";
	av[n++] = "!";
	av[n++] = "filesink";
	av[n++] = loc;
	av[n] = NULL;

	p = fork();
	if (p == 0) {
		execvp(av[0], (char *const *)av);
		_exit(127);
	}
	return p;
}

int main(int argc, char **argv)
{
	char out[4096];
	char token[64], tok[80];
	sd_bus *bus = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus_slot *slot = NULL;
	Reply rp;
	const char *req = NULL;
	char session[256];
	uint32_t node;
	pid_t gst;
	char marker[4096];
	int have_marker, marked = 0;
	int st = 0, rc = 1, r;

	if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("usage: kdos-record [FILE.mkv]\n");
		printf("Records this session's screen. Run it again to stop.\n");
		return 0;
	}

	/*
	 * AGAIN TO STOP, WHATEVER THE ARGUMENTS. One screen and one portal
	 * session, so a second recording is not a second thing a person could
	 * want — and a chord that started a recording has to be able to end
	 * one, which means the stop cannot need a flag nobody can press.
	 *
	 * SIGINT AND NOT SIGTERM. The running process forwards what it is sent
	 * to the pipeline, and `gst-launch-1.0 -e` turns an INT into an
	 * end-of-stream so the muxer writes its index; a TERM would leave a
	 * file without one.
	 */
	have_marker = marker_path(marker, sizeof(marker));
	if (have_marker) {
		pid_t running = marker_read(marker);

		if (running) {
			kill(running, SIGINT);
			printf("kdos-record: stopping the recording\n");
			return 0;
		}
	}

	if (argc > 1)
		snprintf(out, sizeof(out), "%s", argv[1]);
	else
		default_path(out, sizeof(out));
	ensure_dir(out);

	r = sd_bus_default_user(&bus);
	if (r < 0) {
		fprintf(stderr, "kdos-record: no session bus: %s\n",
			strerror(-r));
		return 1;
	}

	snprintf(token, sizeof(token), "kdosrec%d", (int)getpid());

	memset(&rp, 0, sizeof(rp));
	r = sd_bus_match_signal(bus, &slot, PORTAL_BUS, NULL, PORTAL_REQ,
				"Response", on_response, &rp);
	if (r < 0) {
		fprintf(stderr, "kdos-record: cannot watch for a reply: %s\n",
			strerror(-r));
		goto out;
	}

	/* CreateSession. */
	snprintf(tok, sizeof(tok), "%sc", token);
	r = sd_bus_message_new_method_call(bus, &m, PORTAL_BUS, PORTAL_OBJ,
					   PORTAL_CAST, "CreateSession");
	if (r >= 0)
		r = sd_bus_message_open_container(m, 'a', "{sv}");
	if (r >= 0)
		r = sd_bus_message_append(m, "{sv}", "session_handle_token",
					  "s", token);
	if (r >= 0)
		r = append_token(m, tok);
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	if (r >= 0)
		r = sd_bus_call(bus, m, 0, &err, &reply);
	if (r < 0) {
		fprintf(stderr, "kdos-record: CreateSession: %s\n",
			err.message ? err.message : strerror(-r));
		goto out;
	}
	sd_bus_message_read(reply, "o", &req);
	rp.path = req;
	if (wait_reply(bus, &rp, REPLY_TIMEOUT_US) < 0) {
		fprintf(stderr, "kdos-record: no answer to CreateSession\n");
		goto out;
	}
	if (rp.code != 0 || !rp.session[0]) {
		fprintf(stderr,
			"kdos-record: the portal opened no session (%u)\n",
			rp.code);
		goto out;
	}
	snprintf(session, sizeof(session), "%s", rp.session);
	m = sd_bus_message_unref(m);
	reply = sd_bus_message_unref(reply);

	/*
	 * SelectSources. `types` 1 is MONITOR and it is the only source this
	 * session has: there is one grid and no notion of a monitor inside it,
	 * so a window picker would be a picker with one entry.
	 */
	memset(&rp, 0, sizeof(rp));
	snprintf(tok, sizeof(tok), "%ss", token);
	r = sd_bus_message_new_method_call(bus, &m, PORTAL_BUS, PORTAL_OBJ,
					   PORTAL_CAST, "SelectSources");
	if (r >= 0)
		r = sd_bus_message_append(m, "o", session);
	if (r >= 0)
		r = sd_bus_message_open_container(m, 'a', "{sv}");
	if (r >= 0)
		r = sd_bus_message_append(m, "{sv}", "types", "u",
					  (uint32_t)1);
	if (r >= 0)
		r = sd_bus_message_append(m, "{sv}", "multiple", "b", 0);
	if (r >= 0)
		r = append_token(m, tok);
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	if (r >= 0)
		r = sd_bus_call(bus, m, 0, &err, &reply);
	if (r < 0) {
		fprintf(stderr, "kdos-record: SelectSources: %s\n",
			err.message ? err.message : strerror(-r));
		goto out;
	}
	sd_bus_message_read(reply, "o", &req);
	rp.path = req;
	if (wait_reply(bus, &rp, CHOOSE_TIMEOUT_US) < 0) {
		fprintf(stderr, "kdos-record: no screen was picked\n");
		goto out;
	}
	if (rp.code != 0) {
		fprintf(stderr, "kdos-record: nothing was shared (%u)\n",
			rp.code);
		goto out;
	}
	m = sd_bus_message_unref(m);
	reply = sd_bus_message_unref(reply);

	/* Start. The parent window is empty: this has no window of its own. */
	memset(&rp, 0, sizeof(rp));
	snprintf(tok, sizeof(tok), "%st", token);
	r = sd_bus_message_new_method_call(bus, &m, PORTAL_BUS, PORTAL_OBJ,
					   PORTAL_CAST, "Start");
	if (r >= 0)
		r = sd_bus_message_append(m, "os", session, "");
	if (r >= 0)
		r = sd_bus_message_open_container(m, 'a', "{sv}");
	if (r >= 0)
		r = append_token(m, tok);
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	if (r >= 0)
		r = sd_bus_call(bus, m, 0, &err, &reply);
	if (r < 0) {
		fprintf(stderr, "kdos-record: Start: %s\n",
			err.message ? err.message : strerror(-r));
		goto out;
	}
	sd_bus_message_read(reply, "o", &req);
	rp.path = req;
	/*
	 * The cast has to be started before it can answer, and starting it is
	 * a view attaching to the session — so this reply is the slow one.
	 */
	if (wait_reply(bus, &rp, START_TIMEOUT_US) < 0) {
		fprintf(stderr, "kdos-record: the cast did not start\n");
		goto out;
	}
	if (rp.code != 0) {
		fprintf(stderr, "kdos-record: the cast was refused (%u)\n",
			rp.code);
		goto out;
	}
	if (!rp.have_node) {
		fprintf(stderr, "kdos-record: the portal named no node\n");
		goto out;
	}
	node = rp.node;

	/*
	 * `sigaction` WITHOUT SA_RESTART, which is what `signal()` would have
	 * set. The wait below has to come back with EINTR for the forward to
	 * happen at all: a restarted wait is a stop that never reaches the
	 * pipeline and a process holding the portal session for ever.
	 */
	{
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = on_signal;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
	}

	printf("kdos-record: recording node %u to %s — run kdos-record again "
	       "to stop\n", node, out);
	fflush(stdout);
	gst = start_gst(node, out);
	if (gst < 0) {
		fprintf(stderr, "kdos-record: cannot start gst-launch-1.0\n");
		goto out;
	}
	/* WITH THE PIPELINE AND NOT BEFORE IT: the marker is what the bar's
	 * lamp and the next invocation both read, and one written for a
	 * recording that failed to start is a lamp nothing can put out. */
	if (have_marker)
		marked = marker_write(marker);

	/*
	 * THE CONNECTION STAYS OPEN until the pipeline is done: the portal
	 * closes the session when this name leaves the bus, and the node would
	 * go with it.
	 *
	 * `gst-launch-1.0 -e` turns an INT into an end-of-stream, so the muxer
	 * writes its index; a TERM would leave a file without one. A Ctrl+C
	 * reaches the whole process group and the pipeline already has it; the
	 * forward is for the stop that arrives from outside the group, which
	 * is what a second `kdos-record` sends.
	 */
	for (;;) {
		pid_t w = waitpid(gst, &st, 0);

		if (w == gst)
			break;
		if (errno == EINTR) {
			if (interrupted) {
				interrupted = 0;
				kill(gst, SIGINT);
			}
			continue;
		}
		break;
	}
	rc = WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
	if (rc)
		fprintf(stderr, "kdos-record: the pipeline failed\n");
out:
	/* Only a marker this process wrote. Removing one it merely found would
	 * be this process putting out somebody else's lamp. */
	if (marked)
		unlink(marker);
	sd_bus_error_free(&err);
	sd_bus_message_unref(m);
	sd_bus_message_unref(reply);
	sd_bus_slot_unref(slot);
	sd_bus_unref(bus);
	return rc;
}
