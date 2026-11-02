/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-boxsock — one tagged Wayland socket per box launcher
 *
 *     kdos-boxsock [-s confined|tagged] <box> [<launcher-id>]
 *
 * The sandbox-engine half of security-context-v1. It binds a Wayland socket in
 * the box's private runtime directory, hands it to KWin tagged with the engine
 * `io.kdos.appbox+<state>`, the launcher id as the app id and the BOX as the
 * instance id, and then STAYS ALIVE holding the close fd. Every client that
 * connects on that socket is tagged by KWin itself; the client never sees the
 * tag and so cannot forge, choose or drop it.
 *
 * THE INSTANCE ID IS THE BOX, and that is what KWin's sandbox policy reads a
 * box's `grant` by (~/.config/kdos/boxes/<instance>.conf) and what the badge's
 * "In box" names: a launcher id says which application a window is, the
 * instance id which box it runs in, and the two differ for a box with several
 * launchers and for a throwaway box.
 *
 * THE LAUNCHER ID IS THE APP ID KWIN GIVES THE BOX'S WINDOWS. KWin replaces a
 * tagged window's app_id with the context's app id unless the window's own
 * starts with it, and sets both resourceClass and desktopFileName from the
 * result, so the id passed here has to be the name of the launcher
 * genlaunchers wrote: `<box>` for a box with one launcher, `<box>.<stem>` for
 * each launcher of a box with several. Anything else and the box's windows
 * match no desktop entry: a generic icon, a pin that never lights and window
 * rules that never apply.
 *
 * THE STATE RIDES ON THE ENGINE NAME. kdos-appbox decides which boundary held
 * for the box — `confined` (a private runtime directory with no `wayland-0`
 * and no `bus`, the bus filtered, no X11 display) or `tagged` (the tag, and a
 * way past it) — and the engine carries it to the window decoration as
 * `+private` or `+tagged`. An engine is fixed when a context is committed, so
 * a holder serving one state is replaced by a run asking for the other, and
 * the replacement is what the next client connects through.
 *
 * WHY THIS IS A SEPARATE PROGRAM, and not part of kdos-appbox.
 *
 *   - `kdos-appbox run` EXECS podman. The process is replaced, so it cannot
 *     hold anything for the box's lifetime — and close_fd's whole contract is
 *     that the sandbox lives exactly as long as the fd stays open. Somebody has
 *     to outlive the launch.
 *   - kdos-appbox links no Wayland library. Speaking the protocol means
 *     libwayland-client and generated protocol code, for a job this size.
 *
 * WHAT IT PRINTS is the whole interface with kdos-appbox: one line on standard
 * output, `ready <path>` once the socket is served (by this process, or by a
 * live holder already serving the same state), or `error <reason>` when it
 * cannot tag. Standard output is closed straight after, so the caller reads to
 * end of file and has its answer.
 *
 * IDEMPOTENT BY LOCK. The lock lives in $XDG_RUNTIME_DIR/kdos/box, beside the
 * box's private directory and not inside it, so nothing in the box can take it
 * and keep the socket from being served. The holder writes `<pid> <engine>`
 * into it; a second run for the same socket either prints the holder's path
 * (same engine) or stops the holder (another engine) and takes over. SIGTERM
 * keeps its default action: the kernel closes the close fd and releases the
 * lock however the holder dies, and the next holder unlinks the stale socket
 * file before it binds.
 * ---------------------------------
 */

/* Guarded: libkbase needs -D_GNU_SOURCE on the command line, and an
 * unconditional define here collides with it under -Werror and fails
 * testing/selftest.sh's compile gate. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>

#include "security-context-v1-client-protocol.h"

#define ENGINE "io.kdos.appbox"

static struct wp_security_context_manager_v1 *g_mgr;

static void registry_global(void *data, struct wl_registry *registry,
			    uint32_t name, const char *iface, uint32_t version)
{
	(void)data;
	(void)version;
	if (!strcmp(iface, wp_security_context_manager_v1_interface.name))
		g_mgr = wl_registry_bind(registry, name,
					 &wp_security_context_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *registry,
				   uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/*
 * A box name is interpolated into a socket path, so it is checked rather than
 * trusted — the rule kdos-appbox applies to a box name and ksvc to a service
 * name. It starts with a letter or a digit: one starting with `-` is an option
 * nobody here knows, and taken as a name it would bind a socket called
 * `--help`.
 */
static bool name_ok(const char *name)
{
	if (!name || !*name || strlen(name) >= 64)
		return false;
	if (!((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') ||
	      (*name >= '0' && *name <= '9')))
		return false;
	for (const char *p = name; *p; p++) {
		bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
			  (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
			  *p == '.';
		if (!ok)
			return false;
	}
	return true;
}

/*
 * The launcher id is the box itself, the box plus ONE more component (a box
 * with several launchers), or the box less its last component (a throwaway box,
 * `<launcher-id>.tw<n>`, whose windows are its application's). The stem
 * carries no dot, so the id and the box always differ by one component. Each
 * launcher of a box gets a socket of its own, named by its stem; a throwaway
 * box runs one launcher and has one socket.
 */
static bool launcher_ok(const char *box, const char *id, const char **suffix)
{
	size_t bl = strlen(box), il = strlen(id);

	if (!name_ok(id))
		return false;
	if (!strcmp(id, box)) {
		*suffix = "";
		return true;
	}
	if (il > bl + 1 && !strncmp(id, box, bl) && id[bl] == '.' &&
	    !strchr(id + bl + 1, '.')) {
		*suffix = id + bl;
		return true;
	}
	if (bl > il + 1 && !strncmp(box, id, il) && box[il] == '.' &&
	    !strchr(box + il + 1, '.')) {
		*suffix = "";
		return true;
	}
	return false;
}

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
	va_list ap;
	int null;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	fflush(stdout);
	/* The caller reads to end of file; a holder that kept stdout open
	 * would keep it waiting for the box's whole life. */
	null = open("/dev/null", O_WRONLY | O_CLOEXEC);
	if (null >= 0) {
		dup2(null, STDOUT_FILENO);
		close(null);
	}
}

static int fail(const char *what)
{
	say("error %s", what);
	fprintf(stderr, "kdos-boxsock: %s\n", what);
	return 1;
}

static int listen_socket(const char *path)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	int fd;

	if (strlen(path) >= sizeof(addr.sun_path))
		return -1;
	memcpy(addr.sun_path, path, strlen(path) + 1);

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	/* We hold the lock, so any socket already at this path is one nobody is
	 * serving — a crash left it behind, or something in the box put it
	 * there. bind() would fail with EADDRINUSE on it otherwise. */
	unlink(path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	/* 0700: only this user may connect. The box runs as the same uid, so
	 * this costs nothing and keeps another user off the box's socket. */
	if (chmod(path, 0700) < 0)
		perror("kdos-boxsock: chmod");
	if (listen(fd, 64) < 0) {
		close(fd);
		unlink(path);
		return -1;
	}
	return fd;
}

/*
 * WHICH COMPOSITOR A TAGGED SOCKET IS FOR, as a path component.
 *
 * A tagged socket is a listener on ONE compositor. Keyed on the box alone the
 * path would be the FIRST launch's compositor for ever after: a launch under a
 * second compositor (a nested KWin, a second seat) would find the file already
 * there, connect to it, and open its window on the first one. kdos-appbox
 * derives the same component from the same variable, which is what keeps the
 * two in step with nothing passed between them.
 *
 * Twelve characters, because the whole path has to fit a sockaddr_un's 108.
 * Anything a path may not carry is dropped rather than escaped: the component
 * only has to be the same on both sides and different between compositors.
 */
static void display_tag(char *out, size_t cap)
{
	const char *d = getenv("WAYLAND_DISPLAY");
	const char *slash;
	size_t n = 0;

	if (cap > 13)
		cap = 13;
	if (d && (slash = strrchr(d, '/')) != NULL)
		d = slash + 1;
	for (; d && *d && n + 1 < cap; d++) {
		char c = *d;

		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')
			out[n++] = c;
	}
	out[n] = '\0';
	if (!n)
		snprintf(out, cap, "session");
}

/*
 * THE HOLDER'S RECORD, read under the caller's own failed lock. `<pid>
 * <engine>`; an unreadable record is answered as a holder of another engine,
 * because the one wrong answer here is a socket served with a state it does
 * not have.
 */
static int holder_read(int lock, pid_t *pid, char *engine, size_t n)
{
	char buf[256];
	ssize_t got = pread(lock, buf, sizeof(buf) - 1, 0);
	char *sp;
	long p;

	engine[0] = '\0';
	*pid = 0;
	if (got <= 0)
		return -1;
	buf[got] = '\0';
	buf[strcspn(buf, "\n")] = '\0';
	sp = strchr(buf, ' ');
	if (!sp)
		return -1;
	*sp = '\0';
	p = strtol(buf, NULL, 10);
	if (p <= 1)
		return -1;
	*pid = (pid_t)p;
	snprintf(engine, n, "%s", sp + 1);
	return 0;
}

static void holder_write(int lock, const char *engine)
{
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "%ld %s\n", (long)getpid(), engine);

	if (ftruncate(lock, 0) == 0 && len > 0)
		(void)!pwrite(lock, buf, (size_t)len, 0);
}

static int usage(FILE *f, int rc)
{
	fputs("usage: kdos-boxsock [-s confined|tagged] <box> [<launcher-id>]\n",
	      f);
	return rc;
}

int main(int argc, char **argv)
{
	const char *state = "confined", *suffix = "";
	const char *box, *launcher, *rundir;
	char engine[64], dir[256], lockpath[256], tag[16];
	char sockpath[sizeof(((struct sockaddr_un *)0)->sun_path)];
	struct stat st;
	int i = 1, lock;

	if (argc == 2 &&
	    (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
		return usage(stdout, 0);
	if (i + 1 < argc && !strcmp(argv[i], "-s")) {
		state = argv[i + 1];
		i += 2;
	}
	if (argc - i < 1 || argc - i > 2)
		return usage(stderr, 2);
	if (strcmp(state, "confined") && strcmp(state, "tagged"))
		return usage(stderr, 2);
	box = argv[i];
	launcher = argc - i >= 2 ? argv[i + 1] : box;

	if (!name_ok(box))
		return fail("bad box name");
	if (!launcher_ok(box, launcher, &suffix))
		return fail("a launcher id is the box, or differs from it by one "
			    "component");
	snprintf(engine, sizeof(engine), "%s+%s", ENGINE,
		 !strcmp(state, "confined") ? "private" : "tagged");

	rundir = getenv("XDG_RUNTIME_DIR");
	if (!rundir || !*rundir)
		return fail("no XDG_RUNTIME_DIR");

	/*
	 * THE PRIVATE DIRECTORY IS kdos-appbox's, and it has to be there and be
	 * this user's alone before a socket goes into it: a box sees that
	 * directory as its whole runtime directory, so a socket bound anywhere
	 * else is a socket the box cannot reach.
	 */
	display_tag(tag, sizeof(tag));
	if (snprintf(dir, sizeof(dir), "%s/kdos/box/%s", rundir, box) >=
		    (int)sizeof(dir) ||
	    snprintf(sockpath, sizeof(sockpath), "%s/wl%s@%s", dir, suffix,
		     tag) >= (int)sizeof(sockpath) ||
	    snprintf(lockpath, sizeof(lockpath), "%s/kdos/box/.%s.wl%s@%s.lock",
		     rundir, box, suffix, tag) >= (int)sizeof(lockpath))
		return fail("the socket path does not fit a Unix socket address");
	if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode) ||
	    st.st_uid != getuid() || (st.st_mode & 0077))
		return fail("the box's private runtime directory is missing, or "
			    "is not this user's alone");

	lock = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (lock < 0)
		return fail("cannot open the lock file");
	if (flock(lock, LOCK_EX | LOCK_NB) < 0) {
		char held[64];
		pid_t pid;
		int got = 0;

		if (errno != EWOULDBLOCK)
			return fail("cannot take the lock");
		if (holder_read(lock, &pid, held, sizeof(held)) == 0 &&
		    !strcmp(held, engine) && kill(pid, 0) == 0) {
			say("ready %s", sockpath);
			return 0;
		}
		/* Another state, or a record that says nothing: the holder
		 * goes, and its connected clients keep the tag they have. */
		if (pid > 1)
			kill(pid, SIGTERM);
		for (int t = 0; t < 100 && !got; t++) {
			struct timespec ts = { 0, 20 * 1000 * 1000 };

			if (flock(lock, LOCK_EX | LOCK_NB) == 0)
				got = 1;
			else
				nanosleep(&ts, NULL);
		}
		if (!got)
			return fail("the socket's holder did not let go");
	}
	holder_write(lock, engine);

	/* A dead compositor must not take this process with it mid-write. */
	signal(SIGPIPE, SIG_IGN);

	struct wl_display *display = wl_display_connect(NULL);
	if (!display)
		return fail("cannot reach the compositor");
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!g_mgr)
		/*
		 * Fatal: without the manager this socket would be untagged,
		 * and an untagged socket is indistinguishable from the
		 * compositor's own. kdos-appbox refuses to start the box with
		 * this line as the reason, rather than starting it unconfined.
		 */
		return fail("the compositor has no security-context-v1");

	int listen_fd = listen_socket(sockpath);
	if (listen_fd < 0)
		return fail("cannot bind the box's socket");

	/*
	 * close_fd is the sandbox's lifetime. The compositor gets the READ end
	 * and watches for EOF; this process holds the write end and never
	 * writes to it. When this process dies — cleanly, killed, or crashed —
	 * the write end closes, the compositor sees EOF, stops accepting on the
	 * socket and drops the context. There is no cleanup path that has to
	 * run for that to happen, which is the point.
	 */
	int close_pipe[2];
	if (pipe2(close_pipe, O_CLOEXEC) < 0) {
		unlink(sockpath);
		return fail("cannot make the context's close pipe");
	}

	struct wp_security_context_v1 *ctx =
		wp_security_context_manager_v1_create_listener(g_mgr, listen_fd,
							       close_pipe[0]);
	wp_security_context_v1_set_sandbox_engine(ctx, engine);
	wp_security_context_v1_set_app_id(ctx, launcher);
	wp_security_context_v1_set_instance_id(ctx, box);
	wp_security_context_v1_commit(ctx);
	wp_security_context_v1_destroy(ctx);

	/* The fds belong to the compositor now. */
	close(listen_fd);
	close(close_pipe[0]);

	if (wl_display_roundtrip(display) < 0) {
		unlink(sockpath);
		return fail("the compositor rejected the security context");
	}

	say("ready %s", sockpath);

	/*
	 * Now do nothing, forever, holding close_pipe[1] and the flock.
	 * Blocking on the display is what makes the session's end this
	 * process's end: the compositor exiting closes the connection,
	 * wl_display_dispatch returns -1, and the box's socket goes away with
	 * the session that served it.
	 */
	while (wl_display_dispatch(display) != -1)
		;

	unlink(sockpath);
	return 0;
}
