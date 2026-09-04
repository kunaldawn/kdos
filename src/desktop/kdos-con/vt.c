/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — a graphical guest, on a terminal of its own
 *
 * This desktop composites CHARACTER CELLS and a Wayland client's surface is
 * PIXELS. There is no way to put one inside the other, so a graphical
 * application gets a VT to itself with kdos-cage holding it, and the console
 * desktop stays exactly where it was on the VT it was already on.
 *
 * WHY THE SWITCH HAPPENS BEFORE THE GUEST STARTS, and not the other way round.
 * seatd binds a new client to whatever VT is CURRENTLY ACTIVE — that is the
 * whole of its VT model, and there is no protocol by which a client asks for a
 * particular one. So the order is: allocate, activate, wait for the switch to
 * complete, and only then exec. A guest started before the switch would be
 * bound to the console's own VT, and seatd would refuse it because the view is
 * already there.
 *
 * NOTHING HERE OPENS /dev/ttyN. The session runs as an ordinary user and a
 * spare VT belongs to root until something with privilege opens it — which
 * seatd does, on the guest's behalf, once it binds. Finding one is VT_OPENQRY
 * and taking it is VT_ACTIVATE, and neither needs the terminal itself: the
 * activate is what allocates the console, so the next query steps past it.
 *
 * BOTH STILL NEED A DESCRIPTOR ON A CONSOLE DEVICE to carry the ioctl, and
 * that is what the desktop account's membership of `tty` is for. /dev/tty0 is
 * 0620 root:tty and /dev/console is 0600 root:root, and a session that has
 * been backgrounded has no controlling terminal for /dev/tty to resolve to —
 * so without the group every candidate fails, `vt_alloc()` returns -1, and a
 * guest that needs a terminal is refused with "no free terminal" on a machine
 * that has plenty.
 * ---------------------------------
 */

#include <fcntl.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "con.h"

/*
 * A console fd for the ioctls, opened once and kept. Three candidates in the
 * order a non-root process can hope for them: the controlling terminal first,
 * because a session started from a getty owns that one.
 *
 * VT_GETSTATE IS THE TEST, not the path: `/dev/tty` is a pty when this is run
 * from a terminal emulator, and a pty answers every open and no VT ioctl.
 */
static int console_fd(void)
{
	static const char *const cand[] = { "/dev/tty", "/dev/tty0",
					    "/dev/console" };
	static int fd = -2;

	if (fd != -2)
		return fd;

	fd = -1;
	for (size_t i = 0; i < sizeof(cand) / sizeof(cand[0]); i++) {
		struct vt_stat st;
		int f = open(cand[i], O_RDWR | O_CLOEXEC);

		/*
		 * WRITE-ONLY IS ENOUGH, and it is the only way in for a
		 * non-root process. /dev/tty0 is 0620 root:tty, so a member of
		 * `tty` has WRITE and not read — an O_RDWR open fails for
		 * exactly the account this desktop runs as. The ioctls below
		 * carry their answer in the argument and read nothing from the
		 * descriptor, so a write-only one serves them.
		 */
		if (f < 0)
			f = open(cand[i], O_WRONLY | O_CLOEXEC);
		if (f < 0)
			continue;
		if (ioctl(f, VT_GETSTATE, &st) == 0) {
			fd = f;
			break;
		}
		close(f);
	}
	return fd;
}

/* The VT the desktop is on, which is the one a guest returns the screen to. */
static int vt_console(void)
{
	int fd = console_fd();
	struct vt_stat st;

	if (fd < 0 || ioctl(fd, VT_GETSTATE, &st) != 0)
		return -1;
	return st.v_active;
}

/*
 * The first free terminal, or -1 when there is none. There is no retry and no
 * bookkeeping of what has been handed out: the caller ACTIVATES what this
 * returns, activating allocates the console, and an allocated console is not
 * what the next query answers with.
 */
static int vt_alloc(void)
{
	int fd = console_fd();
	int n = 0;

	if (fd < 0)
		return -1;
	if (ioctl(fd, VT_OPENQRY, &n) != 0 || n <= 0)
		return -1;
	return n;
}

int vt_show(Win *w)
{
	int fd = console_fd();

	if (!w || w->vt <= 0 || fd < 0)
		return -1;
	if (ioctl(fd, VT_ACTIVATE, w->vt) != 0)
		return -1;
	return 0;
}

/*
 * Back to the terminal the desktop is on, and let the kernel have the guest's
 * back. A VT that is still busy stays allocated, which is the kernel saying it
 * is not ours to take back — best effort, and the query above steps over it
 * either way.
 */
static void vt_release(Win *w)
{
	int fd = console_fd();

	if (fd < 0 || w->vt <= 0)
		return;
	if (w->vt_home > 0)
		ioctl(fd, VT_ACTIVATE, w->vt_home);
	ioctl(fd, VT_DISALLOCATE, w->vt);
	w->vt = 0;
}

/*
 * Run `argv` full screen on a terminal of its own.
 *
 * `-s` IS NOT OPTIONAL where a cage is used. Without it kdos-cage swallows the
 * VT-switch chords and a full-screen application becomes one nobody can leave —
 * which on a machine whose desktop is on another VT is a wedged machine.
 */
Win *vt_open(const char *const argv[], const char *title, int cage)
{
	const char *av[KCON_MAX_ARGV + 4];
	int n = 0;

	if (!argv || !argv[0])
		return NULL;

	int fd = console_fd();

	if (fd < 0) {
		fprintf(stderr, "kdos-con: no console to allocate a terminal "
				"on — is this session on a VT?\n");
		return NULL;
	}

	int home = vt_console();
	int vt = vt_alloc();

	if (vt <= 0) {
		fprintf(stderr, "kdos-con: every virtual terminal is in use\n");
		return NULL;
	}

	/*
	 * A GUEST THAT IS ITSELF A COMPOSITOR GOES ON THE TERMINAL DIRECTLY.
	 * The graphical session is the case: kdos-desktop starts kdos-comp, and
	 * a compositor inside a kiosk compositor is a screen inside a screen.
	 */
	if (cage) {
		av[n++] = "kdos-cage";
		av[n++] = "-s";
		av[n++] = "--";
	}
	for (int i = 0; argv[i] && n < (int)(sizeof(av) / sizeof(av[0])) - 1;
	     i++)
		av[n++] = argv[i];
	av[n] = NULL;

	Win *w = calloc(1, sizeof(*w));

	if (!w)
		return NULL;

	w->kind = WIN_VT;
	w->id = ++S.next_id;
	w->workspace = S.workspace;
	w->vt = vt;
	w->vt_home = home;
	snprintf(w->title, sizeof(w->title), "%s",
		 title && *title ? title : argv[0]);
	snprintf(w->app_id, sizeof(w->app_id), "kdos-cage");

	/* The switch is asked for HERE and waited for in the child: a session
	 * that blocked on VT_WAITACTIVE would stop drawing until the view had
	 * acknowledged the switch, and the view is what draws it. */
	ioctl(fd, VT_ACTIVATE, vt);

	pid_t pid = fork();

	if (pid < 0) {
		free(w);
		return NULL;
	}

	if (pid == 0) {
		/*
		 * THE CHILD. Its own session, so a signal for the desktop's
		 * process group does not reach a guest on another terminal.
		 *
		 * stdin and stdout go to /dev/null and STDERR IS KEPT: a
		 * compositor that failed to start says why on stderr, and the
		 * session's own log is somewhere a person can read afterwards.
		 * A guest's log on a VT nobody switched to is a log nobody
		 * reads.
		 */
		int null = open("/dev/null", O_RDWR);

		ioctl(fd, VT_WAITACTIVE, vt);
		setsid();
		if (null >= 0) {
			dup2(null, 0);
			dup2(null, 1);
			if (null > 2)
				close(null);
		}

		char nr[16];

		snprintf(nr, sizeof(nr), "%d", vt);
		setenv("XDG_VTNR", nr, 1);
		/* The guest is not a client of this session and must not try to
		 * be one: kdos-term started inside it is a Wayland window
		 * there, not a cell surface here. */
		unsetenv("KDOS_CON");

		execvp(av[0], (char *const *)av);
		_exit(127);
	}

	w->vt_pid = pid;
	w->next = S.wins;
	S.wins = w;
	S.focus = w->id;
	return w;
}

/*
 * A GUEST THAT DIED FREES ITS TERMINAL, and the desktop comes back. Polled
 * rather than driven by SIGCHLD: the session already wakes on a timer to redraw
 * the clock, a handler would be a signal racing the window list, and a guest
 * that exited half a second ago is not a guest anybody noticed.
 */
void vt_reap(void)
{
	Win *next;

	for (Win *w = S.wins; w; w = next) {
		next = w->next;
		if (w->kind != WIN_VT || w->vt_pid <= 0)
			continue;
		if (waitpid(w->vt_pid, NULL, WNOHANG) != w->vt_pid)
			continue;
		w->vt_pid = 0;
		vt_release(w);
		win_close(w);
	}
}

/*
 * Closing a guest is asking its compositor to go, not taking its terminal away.
 * The entry stays until the child is actually gone — vt_reap is what removes
 * it — because a taskbar that dropped an application the moment somebody asked
 * it to close is a taskbar that lies about a program still saving a file.
 */
void vt_close(Win *w)
{
	if (!w || w->kind != WIN_VT)
		return;
	if (w->vt_pid > 0)
		kill(w->vt_pid, SIGTERM);
}

/* Every guest, at shutdown: the desktop is going and a compositor holding a
 * terminal it opened is a terminal nobody can get back. */
void vt_close_all(void)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->kind == WIN_VT && w->vt_pid > 0)
			kill(w->vt_pid, SIGTERM);
}
