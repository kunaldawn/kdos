/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-boxsock — one tagged Wayland socket per appbox
 *
 * The sandbox-engine half of security-context-v1. It binds a Wayland socket for
 * one box, hands it to kdos-comp tagged `io.kdos.appbox` / <box> / <instance>,
 * and then STAYS ALIVE holding the close fd. Every client that connects on that
 * socket is tagged by the compositor itself; the client never sees the tag and
 * so cannot forge, choose or drop it.
 *
 * WHY THIS IS A SEPARATE PROGRAM, and not part of kdos-appbox.
 *
 * Two reasons, and both are structural rather than stylistic:
 *
 *   - `kdos-appbox run` EXECS distrobox. The process is replaced, so it cannot
 *     hold anything for the box's lifetime — and close_fd's whole contract is
 *     that the sandbox lives exactly as long as the fd stays open. Somebody has
 *     to outlive the launch.
 *   - kdos-appbox links libkbase, libktui and libkcolor and nothing else. That
 *     is a documented property of the program, not an accident. Speaking a
 *     Wayland protocol means libwayland-client and generated protocol code;
 *     putting that in kdos-appbox would cost it that property for a job that is
 *     ~200 lines and belongs to the desktop rather than to the box manager.
 *
 * So kdos-appbox execs this, detached, and sets WAYLAND_DISPLAY for the box to
 * the socket this created. No fd is passed between them and no protocol runs
 * between them — the only shared thing is the path, which both derive from the
 * box name.
 *
 * IDEMPOTENT BY LOCK. A second instance for the same box takes no action and
 * exits 0. The flock is held for the process's whole life, so "is this box's
 * socket already served?" is answered by whether the lock can be taken — not by
 * whether the socket file exists, which is also true for a stale one left by a
 * crash.
 * ---------------------------------
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
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
 * trusted — the same rule kdos-comp applies to the app_id it reads back, and
 * the same one ksvc applies to a service name.
 */
static bool box_name_ok(const char *name)
{
	if (!name || !*name || strlen(name) > 64)
		return false;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
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

static int listen_socket(const char *path)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	if (strlen(path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "kdos-boxsock: socket path too long\n");
		return -1;
	}
	strcpy(addr.sun_path, path);

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		perror("kdos-boxsock: socket");
		return -1;
	}
	/* We hold the lock, so any socket already at this path is one nobody is
	 * serving — a crash left it behind. bind() would fail with EADDRINUSE
	 * on it forever otherwise. */
	unlink(path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("kdos-boxsock: bind");
		close(fd);
		return -1;
	}
	/* 0700: only this user may connect. The box runs as the same uid, so
	 * this costs nothing and keeps another user off the box's socket. */
	if (chmod(path, 0700) < 0)
		perror("kdos-boxsock: chmod");
	if (listen(fd, 64) < 0) {
		perror("kdos-boxsock: listen");
		close(fd);
		unlink(path);
		return -1;
	}
	return fd;
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: kdos-boxsock <box> [instance-id]\n");
		return 2;
	}
	const char *box = argv[1];
	const char *instance = argc == 3 ? argv[2] : box;

	if (!box_name_ok(box)) {
		fprintf(stderr, "kdos-boxsock: bad box name\n");
		return 2;
	}

	const char *rundir = getenv("XDG_RUNTIME_DIR");
	if (!rundir || !*rundir) {
		fprintf(stderr, "kdos-boxsock: no XDG_RUNTIME_DIR\n");
		return 1;
	}

	char lockpath[256], sockpath[256];
	if (snprintf(lockpath, sizeof(lockpath), "%s/kdos-box-%s.lock", rundir,
		     box) >= (int)sizeof(lockpath) ||
	    snprintf(sockpath, sizeof(sockpath), "%s/kdos-box-%s.sock", rundir,
		     box) >= (int)sizeof(sockpath)) {
		fprintf(stderr, "kdos-boxsock: path too long\n");
		return 1;
	}

	/* Held for the life of the process, and released by the kernel when it
	 * dies however it dies. */
	int lock = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (lock < 0) {
		perror("kdos-boxsock: lock");
		return 1;
	}
	if (flock(lock, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)
			return 0;	/* already served; nothing to do */
		perror("kdos-boxsock: flock");
		return 1;
	}

	/* A dead compositor must not take this process with it mid-write. */
	signal(SIGPIPE, SIG_IGN);

	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "kdos-boxsock: cannot reach the compositor\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!g_mgr) {
		/*
		 * Not fatal to the SESSION, but fatal here: without the manager
		 * this socket would be untagged, and an untagged socket is
		 * indistinguishable from the compositor's own. Exiting means
		 * kdos-appbox falls back to the shared socket and the box runs
		 * unconfined — which is the status quo, and honest, rather than
		 * a socket that merely looks confined.
		 */
		fprintf(stderr, "kdos-boxsock: compositor has no "
				"security-context-v1 — not tagging\n");
		return 1;
	}

	int listen_fd = listen_socket(sockpath);
	if (listen_fd < 0)
		return 1;

	/*
	 * close_fd is the sandbox's lifetime. The compositor gets the READ end
	 * and watches for EOF; this process holds the write end and never
	 * writes to it. When this process dies — cleanly, killed, or crashed —
	 * the write end closes, the compositor sees EOF, stops accepting on the
	 * socket and drops the context. There is no cleanup path that has to
	 * run for that to happen, which is the point.
	 */
	int close_pipe[2];
	if (pipe(close_pipe) < 0) {
		perror("kdos-boxsock: pipe");
		return 1;
	}

	struct wp_security_context_v1 *ctx =
		wp_security_context_manager_v1_create_listener(g_mgr, listen_fd,
							       close_pipe[0]);
	wp_security_context_v1_set_sandbox_engine(ctx, ENGINE);
	wp_security_context_v1_set_app_id(ctx, box);
	wp_security_context_v1_set_instance_id(ctx, instance);
	wp_security_context_v1_commit(ctx);
	wp_security_context_v1_destroy(ctx);

	/* The fds belong to the compositor now. */
	close(listen_fd);
	close(close_pipe[0]);

	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "kdos-boxsock: commit rejected\n");
		unlink(sockpath);
		return 1;
	}

	/* The caller reads this to learn what to set WAYLAND_DISPLAY to. */
	printf("%s\n", sockpath);
	fflush(stdout);

	/*
	 * Now do nothing, forever, holding close_pipe[1] and the flock. Blocking
	 * on the display is what makes the session's end this process's end: the
	 * compositor exiting closes the connection, wl_display_dispatch returns
	 * -1, and the box's socket goes away with the session that served it.
	 */
	while (wl_display_dispatch(display) != -1)
		;

	unlink(sockpath);
	return 0;
}
