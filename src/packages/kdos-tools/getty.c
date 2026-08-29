/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-getty <ttyN> <getty...>
 *
 * Console font and palette must be loaded AFTER fbcon takes the console, not
 * in rcS: the kernel defers the take-over until something writes to a VT, and
 * the take-over re-initialises every VT with the kernel's built-in font —
 * wiping anything setfont loaded earlier. So the sequence lives here, wrapping
 * getty: one write ends the deferral, then the font (ter-kdos32n carries λ and
 * the double box glyphs the banner needs) and the phosphor palette are loaded
 * onto this VT.
 *
 * The take-over is scheduled work, so a fixed sleep is a race that first boot
 * reliably loses: wait for fbcon to report it in the kernel ring, then retry
 * setfont until showconsolefont confirms 16x32 is what the VT actually has.
 *
 * setfont/showconsolefont/setvtrgb/loadkeys are still exec'd rather than
 * reimplemented as KDFONTOP/PIO_CMAP/KDSKBENT calls. Deliberate: setfont's
 * real job is parsing a gzipped PSF and its unicode table, and a second
 * implementation of that is a new way to produce exactly the wrong-font bug
 * this wrapper exists to prevent. What DID move into C is the polling — the
 * kernel ring is read with klogctl() instead of forking dmesg fifty times.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/wait.h>

#include "kdos-tools.h"

#define FBCON_MARK "fbcon: Taking over console"
#define FONT       "ter-kdos32n"
#define FONT_GEOM  "16x32"

/* Force the tty's window size to the VT's real character grid.
 *
 * Deferred take-over means the console has no geometry yet when this runs, so
 * whoever asks first gets a 0x0 TIOCGWINSZ and settles on a fallback — and on
 * tty1 that fallback stuck: the VT is 120x33 with the 16x32 font, but every
 * full-screen program came up believing it had 80x24 and drew into the
 * top-left corner of the screen (measured with a full-screen aalib demo,
 * which centred on column 40 of 120). kinstall and every other libktui
 * program are on the same
 * path. The size the loaded font actually produced is the first two bytes of
 * /dev/vcsa<n> — rows, then columns — which is the console's own answer
 * rather than a second guess at the arithmetic. Runs after the font is
 * loaded, because that is what decides the grid. */
static void fix_winsize(int fd, const char *tty)
{
	char path[128];
	unsigned char hdr[2];
	struct winsize ws, cur;
	int v;

	if (strncmp(tty, "tty", 3) || !isdigit((unsigned char)tty[3]))
		return;
	snprintf(path, sizeof(path), "/dev/vcsa%s", tty + 3);
	v = open(path, O_RDONLY | O_CLOEXEC);
	if (v < 0)
		return;
	if (read(v, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
		close(v);
		return;
	}
	close(v);
	if (!hdr[0] || !hdr[1])
		return;

	memset(&ws, 0, sizeof(ws));
	ws.ws_row = hdr[0];
	ws.ws_col = hdr[1];
	if (ioctl(fd, TIOCGWINSZ, &cur) == 0 &&
	    cur.ws_row == ws.ws_row && cur.ws_col == ws.ws_col)
		return;
	if (ioctl(fd, TIOCSWINSZ, &ws) != 0)
		fprintf(stderr, "kdos-getty: TIOCSWINSZ %s: %s\n", tty,
			strerror(errno));
	else
		fprintf(stderr, "kdos-getty: %s winsize -> %ux%u\n", tty,
			ws.ws_col, ws.ws_row);
}

static void nap(long ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}

/* SYSLOG_ACTION_READ_ALL — the whole ring, without disturbing it. */
static int kmsg_has(const char *needle)
{
	static char buf[1 << 18];
	int n = klogctl(3, buf, (int)sizeof(buf) - 1);
	if (n <= 0)
		return 0;
	buf[n] = 0;
	return strstr(buf, needle) != NULL;
}

static int run(const char *const *argv, char *out, size_t outcap)
{
	int fd[2] = { -1, -1 };
	if (out && pipe(fd) < 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		if (null >= 0) {
			dup2(null, STDERR_FILENO);
			if (!out)
				dup2(null, STDOUT_FILENO);
		}
		if (out) {
			dup2(fd[1], STDOUT_FILENO);
			close(fd[0]);
			close(fd[1]);
		}
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	if (out) {
		close(fd[1]);
		ssize_t got = read(fd[0], out, outcap - 1);
		out[got > 0 ? got : 0] = 0;
		close(fd[0]);
	}
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

int getty_main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: kdos-getty <ttyN> <getty> [args...]\n");
		return 1;
	}
	const char *tty = argv[1];

	/* Boot-time diagnostics: /run is tmpfs, so this costs nothing
	 * persistent and `kdos doctor` gets something to read when a VT comes
	 * up with the wrong font. */
	char logpath[128];
	snprintf(logpath, sizeof(logpath), "/run/kdos-getty.%s.log", tty);
	int lg = open(logpath, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (lg >= 0) {
		dup2(lg, STDERR_FILENO);
		if (lg > STDERR_FILENO)
			close(lg);
	}

	char dev[128];
	snprintf(dev, sizeof(dev), "/dev/%s", tty);

	int vt = open(dev, O_WRONLY | O_CLOEXEC);
	if (vt >= 0 && kb_have_prog("setfont")) {
		/* End fbcon's deferred take-over. Neither escape sequences NOR
		 * SPACES do it — escapes are eaten by the VT state machine and
		 * spaces are skipped by the render path; only a real glyph
		 * reaches the putc that arms the take-over (verified: ' ' ->
		 * nothing, 'K' -> "fbcon: Taking over console"). So: one K,
		 * cleared away before anyone sees it. */
		if (write(vt, "K\033[2J\033[H", 8) < 0)
			fprintf(stderr, "kdos-getty: cannot write %s\n", dev);

		for (int i = 0; i < 50 && !kmsg_has(FBCON_MARK); i++)
			nap(100);

		for (int i = 0; i < 10; i++) {
			const char *set[] = { "setfont", "-C", dev, FONT, NULL };
			run(set, NULL, 0);

			char shown[256];
			const char *show[] = { "showconsolefont", "-i", "-C",
					       dev, NULL };
			run(show, shown, sizeof(shown));
			if (!strncmp(shown, FONT_GEOM, strlen(FONT_GEOM)))
				break;
			nap(200);
		}

		/* Palette BEFORE the final clear: cells painted before the
		 * palette load keep the old colours, and a screen half-cleared
		 * in pure black and half in phosphor-black looks smudged. */
		if (kb_have_prog("setvtrgb") && kb_path_exists("/etc/vtrgb")) {
			pid_t pid = fork();
			if (pid == 0) {
				int in = open(dev, O_RDONLY | O_CLOEXEC);
				if (in >= 0)
					dup2(in, STDIN_FILENO);
				dup2(vt, STDOUT_FILENO);
				int null = open("/dev/null", O_WRONLY);
				if (null >= 0)
					dup2(null, STDERR_FILENO);
				execlp("setvtrgb", "setvtrgb", "/etc/vtrgb",
				       (char *)NULL);
				_exit(127);
			}
			if (pid > 0) {
				int st;
				while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
					;
			}
		}

		/* /etc/keymap is written by the installer. loadkeys is
		 * console-wide rather than per-VT, so doing it again on the
		 * second getty is a no-op; it lives here rather than in rcS
		 * only because this is the one place guaranteed to run after
		 * fbcon has finished resetting the VTs. */
		char km[128] = {0};
		if (kb_have_prog("loadkeys") &&
		    kb_read_line_file("/etc/keymap", km, sizeof(km)) > 0 && km[0]) {
			const char *lk[] = { "loadkeys", km, NULL };
			run(lk, NULL, 0);
		}

		if (write(vt, "\033[2J\033[H", 7) < 0)
			fprintf(stderr, "kdos-getty: cannot clear %s\n", dev);

		fix_winsize(vt, tty);
	}
	if (vt >= 0)
		close(vt);

	/*
	 * PUT THE AUTOLOGIN SESSION INSIDE ITS DELEGATED CGROUP. 15_userdirs.sh
	 * delegates `user.slice/user-<uid>` to each human user, and that is
	 * decoration until a session actually runs in it: rootless podman with
	 * the cgroupfs manager creates a container's cgroup as a SIBLING of
	 * the one it is called from, so a session left in the root cgroup gets
	 * `--memory` accepted and ignored. Measured: a shell moved into
	 * `user-1000/session` gives every box `user-1000/<id>` with
	 * memory.max set; the same box from the root cgroup reads `max`. The
	 * move needs root — a process cannot write itself out of `/` — and
	 * this is the last root process before login, so it is done here,
	 * for the user `--autologin` names. `session` is a LEAF: a cgroup
	 * with processes in it may not enable controllers for children, and
	 * the container cgroups have to be siblings of the shell's, not
	 * children. A tty that logs in interactively and an ssh session are
	 * not covered and stay in the root cgroup.
	 */
	for (int i = 3; i + 1 < argc; i++) {
		if (strcmp(argv[i], "--autologin") && strcmp(argv[i], "-a"))
			continue;
		struct passwd *pw = getpwnam(argv[i + 1]);
		char cgp[256], pid[32];
		int fd;
		if (!pw)
			break;
		snprintf(cgp, sizeof(cgp),
			 "/sys/fs/cgroup/user.slice/user-%u/session/cgroup.procs",
			 (unsigned)pw->pw_uid);
		snprintf(pid, sizeof(pid), "%ld\n", (long)getpid());
		fd = open(cgp, O_WRONLY | O_CLOEXEC);
		if (fd < 0 || write(fd, pid, strlen(pid)) < 0)
			fprintf(stderr, "kdos-getty: not moved into %s: %s\n",
				cgp, strerror(errno));
		if (fd >= 0)
			close(fd);
		break;
	}

	execvp(argv[2], argv + 2);
	fprintf(stderr, "kdos-getty: cannot exec %s: %s\n", argv[2],
		strerror(errno));
	return 127;
}
