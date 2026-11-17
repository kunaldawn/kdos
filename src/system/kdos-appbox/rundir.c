/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * The box's private runtime directory, and the record of which boundary held.
 *
 * A BOX SEES $XDG_RUNTIME_DIR/kdos/box/<box> AS ITS /run/user/<uid>, and that
 * directory is the boundary. What is in it is what the box can reach:
 *
 *   wl[.<stem>]@<display>   its tagged Wayland sockets (kdos-boxsock)
 *   dbus-proxy              its filtered session bus (the keeper)
 *   pipewire-0, pulse/native
 *                           HARD LINKS to the session's audio sockets: a
 *                           socket's inode is what a connect() reaches, so a
 *                           link in this directory is the same socket, and
 *                           the session's directory stays out of the box
 *   at-spi/                 the accessibility bus, linked the same way, only
 *                           for somebody who opted in (box_a11y_wanted)
 *   Xauthority              the session's X11 cookie, only for a box whose
 *                           `x11` resolves to yes
 *
 * The session's own Wayland socket (the one $WAYLAND_DISPLAY names, KWin's
 * `wayland-1`) and `bus` are never in it. KWin's tag filters only what
 * reaches the tagged socket, so a box that could open the session's socket
 * would be one connect() away from every global the tag denies; that is why
 * there is no fallback to the session's socket anywhere in this program.
 *
 * A LINK OUTLIVES THE SOCKET IT NAMES. A restarted PipeWire binds a new
 * socket and the link still names the old, dead one, so the keeper watches
 * the session's directory and links again; a launch links again too.
 *
 * THE DIRECTORY IS NOT THE ONLY WAY OUT, and the state file says so. A box
 * created over the session's runtime directory (one distrobox made from
 * somebody else's image) still shares every socket in it, and an X11 display
 * the box can reach bypasses the tag altogether — Xwayland is one client to
 * KWin. Either makes the box `tagged`, never `confined`.
 */

#include "kdos-appbox.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

const char *box_rundir(void)
{
	static char b[256];
	const char *r = getenv("XDG_RUNTIME_DIR");

	if (r && r[0] == '/' && strlen(r) < sizeof(b))
		snprintf(b, sizeof(b), "%s", r);
	else
		snprintf(b, sizeof(b), "/run/user/%u", (unsigned)getuid());
	return b;
}

/*
 * Accessibility: on whenever somebody uses it.
 *
 * The accessibility bus is unfiltered — a client on it can read the text of
 * every other application on it — so a box is given it only when assistive
 * technology is in use: the screen reader is switched on in System Settings
 * (`kaccessrc [ScreenReader] Enabled`), the running at-spi registry says an
 * assistive tool is attached (org.a11y.Status IsEnabled, asked only when the
 * registry's sockets already exist, because asking starts it), or the user
 * opted in for boxes by creating ~/.config/kdos/a11y — an empty file is
 * enough. KDOS_A11Y=1 or 0 decides one launch. Anything else, and every boxed
 * GTK app is told not to probe org.a11y.Bus, which it would only pay for at
 * startup.
 *
 * The bus proxy's policy is set when a box's keeper starts, so a screen
 * reader switched on later reaches a box from its next start.
 */
static int kaccess_screen_reader(void)
{
	char path[MAX_LINE];
	const char *xdg = getenv("XDG_CONFIG_HOME");
	size_t len = 0;
	char *text;
	int in = 0, on = 0;

	if (xdg && *xdg == '/')
		snprintf(path, sizeof(path), "%s/kaccessrc", xdg);
	else
		snprintf(path, sizeof(path), "%s/.config/kaccessrc", kb_home_dir());
	text = kb_read_all(path, &len);
	for (char *l = text, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		if (*l == '[')
			in = !strncmp(l, "[ScreenReader]", 14);
		else if (in && !strncmp(l, "Enabled", 7)) {
			const char *v = strchr(l, '=');

			on = v && !strncmp(v + 1, "true", 4);
		}
	}
	free(text);
	return on;
}

static int atspi_enabled(void)
{
	char dir[MAX_LINE], out[128] = "";
	KbArgv a = {0};

	snprintf(dir, sizeof(dir), "%s/at-spi", box_rundir());
	if (!kb_is_dir(dir) || !kb_have_prog("gdbus"))
		return 0;
	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--timeout");
	kb_argv_add(&a, "1");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.a11y.Bus");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/org/a11y/bus");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.freedesktop.DBus.Properties.Get");
	kb_argv_add(&a, "org.a11y.Status");
	kb_argv_add(&a, "IsEnabled");
	kb_argv_end(&a);
	return kb_run_capture(&a, out, sizeof(out)) == 0 && strstr(out, "true");
}

int box_a11y_wanted(void)
{
	char path[MAX_LINE];
	const char *env = getenv("KDOS_A11Y");

	if (env && *env)
		return strcmp(env, "0") != 0;
	snprintf(path, sizeof(path), "%s/.config/kdos/a11y", kb_home_dir());
	return kb_path_exists(path) || kaccess_screen_reader() ||
	       atspi_enabled();
}

const char *box_inbox_rundir(void)
{
	static char b[32];

	snprintf(b, sizeof(b), "/run/user/%u", (unsigned)getuid());
	return b;
}

/*
 * A directory this user owns and nobody else can enter. Made 0700 rather than
 * through kb_mkdir_p, whose 0755 would let another account list a box's
 * sockets; an existing one that is not ours is refused, because a socket put
 * into somebody else's directory is a socket they can replace.
 */
static int mkdir_private(const char *path)
{
	struct stat st;

	if (mkdir(path, 0700) != 0 && errno != EEXIST)
		return -1;
	if (lstat(path, &st) != 0 || !S_ISDIR(st.st_mode) ||
	    st.st_uid != getuid())
		return -1;
	if ((st.st_mode & 0077) && chmod(path, 0700) != 0)
		return -1;
	return 0;
}

int box_private_dir(const char *box, char *out, size_t n)
{
	/* The proxy's socket is the longest fixed leaf and has to fit a
	 * sockaddr_un under this directory; the Wayland sockets are checked by
	 * kdos-boxsock, which knows their names. */
	int w = snprintf(out, n, "%s/kdos/box/%s", box_rundir(), box);

	if (w < 0 || (size_t)w >= n ||
	    (size_t)w + 1 + sizeof(BOX_PROXY_LEAF) > 108)
		return -1;
	return 0;
}

int box_state_dir(char *out, size_t n)
{
	char kd[MAX_LINE];
	int w;

	snprintf(kd, sizeof(kd), "%s/kdos", box_rundir());
	w = snprintf(out, n, "%s/boxes", kd);
	if (w < 0 || (size_t)w >= n)
		return -1;
	if (mkdir_private(kd) != 0 || mkdir_private(out) != 0)
		return -1;
	return 0;
}

/*
 * ONE SOCKET, LINKED. Same inode already there: nothing to do. A source that
 * is gone or is not a socket takes the link with it, so the box finds no
 * socket rather than a dead one.
 */
static void link_socket(const char *src, const char *dst)
{
	struct stat s, d;

	if (stat(src, &s) != 0 || !S_ISSOCK(s.st_mode)) {
		unlink(dst);
		return;
	}
	if (lstat(dst, &d) == 0 && d.st_ino == s.st_ino && d.st_dev == s.st_dev)
		return;
	unlink(dst);
	if (link(src, dst) != 0)
		tracef("rundir: cannot link %s: %s", src, strerror(errno));
}

static void unlink_tree(const char *dir)
{
	char **names = kb_listdir(dir, NULL);

	for (char **f = names; f && *f; f++) {
		char *p = kb_path_join(dir, *f);
		unlink(p);
		free(p);
	}
	kb_strv_free(names);
	rmdir(dir);
}

/*
 * THE ACCESSIBILITY BUS DIRECTORY holds a socket per display at-spi serves,
 * and their names are at-spi's, so the whole directory is mirrored: every
 * socket linked, every link whose socket went removed.
 */
static void link_atspi(const char *pdir)
{
	char src[MAX_LINE], dst[MAX_LINE + 16];
	char **names, **have;

	snprintf(src, sizeof(src), "%s/at-spi", box_rundir());
	snprintf(dst, sizeof(dst), "%s/at-spi", pdir);
	if (mkdir_private(dst) != 0)
		return;
	names = kb_listdir(src, NULL);
	for (char **f = names; f && *f; f++) {
		char *s = kb_path_join(src, *f), *d = kb_path_join(dst, *f);
		link_socket(s, d);
		free(s);
		free(d);
	}
	have = kb_listdir(dst, NULL);
	for (char **f = have; f && *f; f++) {
		int keep = 0;

		for (char **g = names; g && *g && !keep; g++)
			keep = !strcmp(*f, *g);
		if (!keep) {
			char *d = kb_path_join(dst, *f);
			unlink(d);
			free(d);
		}
	}
	kb_strv_free(have);
	kb_strv_free(names);
}

void rundir_link_sockets(const char *box, int audio, int a11y)
{
	char pdir[MAX_LINE], s[MAX_LINE + 32], d[MAX_LINE + 32];

	if (box_private_dir(box, pdir, sizeof(pdir)) != 0)
		return;
	snprintf(d, sizeof(d), "%s/pipewire-0", pdir);
	if (audio) {
		snprintf(s, sizeof(s), "%s/pipewire-0", box_rundir());
		link_socket(s, d);
		snprintf(d, sizeof(d), "%s/pulse", pdir);
		if (mkdir_private(d) == 0) {
			snprintf(s, sizeof(s), "%s/pulse/native", box_rundir());
			snprintf(d, sizeof(d), "%s/pulse/native", pdir);
			link_socket(s, d);
		}
	} else {
		unlink(d);
		snprintf(d, sizeof(d), "%s/pulse", pdir);
		unlink_tree(d);
	}
	snprintf(d, sizeof(d), "%s/at-spi", pdir);
	if (a11y)
		link_atspi(pdir);
	else
		unlink_tree(d);
}

/*
 * THE COOKIE, copied in only when the box is meant to have the display. The
 * file is the session's, and Xwayland accepts any client that presents it, so
 * a box that is handed it is a box that reaches X11 untagged.
 */
static void x11_cookie(const char *pdir, int want)
{
	char dst[MAX_LINE + 16];
	const char *src = getenv("XAUTHORITY");

	snprintf(dst, sizeof(dst), "%s/Xauthority", pdir);
	if (!want || !src || !*src || !kb_path_exists(src)) {
		unlink(dst);
		return;
	}
	if (kb_copy_file(src, dst) != 0 || chmod(dst, 0600) != 0)
		tracef("rundir: cannot copy the X11 cookie: %s", strerror(errno));
}

int rundir_prepare(const Profile *p)
{
	char pdir[MAX_LINE], sd[MAX_LINE], kd[MAX_LINE];

	if (box_private_dir(p->name, pdir, sizeof(pdir)) != 0) {
		kb_warn("%s: the box name is too long for a socket path under "
			"%s", p->name, box_rundir());
		return -1;
	}
	snprintf(kd, sizeof(kd), "%s/kdos/box", box_rundir());
	if (box_state_dir(sd, sizeof(sd)) != 0 || mkdir_private(kd) != 0 ||
	    mkdir_private(pdir) != 0) {
		kb_warn("%s: cannot make the box's private runtime directory "
			"%s (it must be this user's and nobody else's)",
			p->name, pdir);
		return -1;
	}
	rundir_link_sockets(p->name, p->audio, box_a11y_wanted());
	x11_cookie(pdir, profile_x11(p));
	return 0;
}

int rundir_inbox_path(const char *box, int private_layout, const char *leaf,
		      char *out, size_t n)
{
	char pdir[MAX_LINE];
	int w;

	if (private_layout)
		w = snprintf(out, n, "%s/%s", box_inbox_rundir(), leaf);
	else if (box_private_dir(box, pdir, sizeof(pdir)) == 0)
		w = snprintf(out, n, "%s/%s", pdir, leaf);
	else
		return -1;
	return w < 0 || (size_t)w >= n ? -1 : 0;
}

/* ── the container ─────────────────────────────────────────────────────── */

int box_layout_private(const char *box)
{
	KbArgv a = {0};
	KbBuf out = {0};
	char pdir[MAX_LINE];
	const char *want = box_inbox_rundir();
	int rc = 0;

	if (box_private_dir(box, pdir, sizeof(pdir)) != 0)
		return -1;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{range .Mounts}}{{.Destination}}{{\"\\t\"}}"
			"{{.Source}}{{\"\\n\"}}{{end}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0) {
		kb_buf_free(&out);
		return -1;
	}
	for (char *line = out.p, *nl; line && *line; line = nl) {
		char *tab;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab++ = '\0';
		if (!strcmp(line, want)) {
			rc = !strcmp(tab, pdir);
			break;
		}
	}
	kb_buf_free(&out);
	return rc;
}

/*
 * THE BOX'S CGROUP IS A SCOPE IN THE LOGIN SESSION'S DELEGATED SUBTREE.
 * elogind with DelegateSessionCgroups=yes runs a login in
 * /sys/fs/cgroup/<session>/session and gives the user
 * /sys/fs/cgroup/<session>/delegated, the one subtree in which an
 * unprivileged podman can create cgroups and enable the memory, cpu and pids
 * controllers — without one a profile's `memory` is accepted and enforced by
 * nothing.
 *
 * THE MOVE HAS TO BE ALLOWED, NOT ONLY THE DIRECTORY. The box's first process
 * starts in this login's `session` cgroup and is moved into the scope, and
 * cgroup2 refuses an unprivileged move unless the mover can write the
 * cgroup.procs of the common ancestor of the two — the session's own
 * /sys/fs/cgroup/<session>. Where that file is root's, a scope under
 * `delegated` is one podman can make and never put the box in, and every
 * launch fails; the box is then created under podman's own default instead,
 * where `memory`, `cpus` and `pids` are unenforced, and `kdos-box profile`
 * says so.
 *
 * NAMED THE WAY systemd NAMES AN APPLICATION'S SCOPE,
 * `app-kdos-<box>-<n>.scope` with the box's dashes escaped as `\x2d`, which
 * is the shape System Monitor and the desktop portal parse an application id
 * out of: the launcher id of a box with one launcher is the box, so the
 * scope's id names the launcher and its icon.
 */
int box_cgroup_parent(const char *box, char *out, size_t n)
{
	char cg[1024], deleg[1200], esc[256];
	char *path, *nl, *last;
	size_t e = 0;
	int w;

	out[0] = '\0';
	if (kb_read_file("/proc/self/cgroup", cg, sizeof(cg)) <= 0)
		return 0;
	path = strstr(cg, "0::");
	if (!path)
		return 0;
	path += 3;
	nl = strchr(path, '\n');
	if (nl)
		*nl = '\0';
	last = strrchr(path, '/');
	if (!last || last == path || strcmp(last, "/session"))
		return 0;
	*last = '\0';
	snprintf(deleg, sizeof(deleg), "/sys/fs/cgroup%s/cgroup.procs", path);
	if (access(deleg, W_OK) != 0)
		return 0;
	snprintf(deleg, sizeof(deleg), "/sys/fs/cgroup%s/delegated", path);
	if (!kb_is_dir(deleg) || access(deleg, W_OK) != 0)
		return 0;
	for (const char *c = box; *c && e + 5 < sizeof(esc); c++) {
		if (*c == '-') {
			memcpy(esc + e, "\\x2d", 4);
			e += 4;
		} else {
			esc[e++] = *c;
		}
	}
	esc[e] = '\0';
	w = snprintf(out, n, "%s/delegated/app-kdos-%s-%ld.scope", path, esc,
		     (long)time(NULL));
	if (w < 0 || (size_t)w >= n) {
		out[0] = '\0';
		return 0;
	}
	return 1;
}

int box_cgroup_stale(const char *box)
{
	KbArgv a = {0};
	char parent[1024] = "", dir[1100];
	char *slash;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.HostConfig.CgroupParent}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, parent, sizeof(parent)) != 0)
		return 0;
	parent[strcspn(parent, "\r\n")] = '\0';
	/* Only a parent this program chose is judged: podman's own default is
	 * not under a session, and is created again wherever it was. */
	if (!strstr(parent, "/delegated/app-kdos-"))
		return 0;
	slash = strrchr(parent, '/');
	if (!slash)
		return 0;
	*slash = '\0';
	snprintf(dir, sizeof(dir), "/sys/fs/cgroup%s", parent);
	return !kb_is_dir(dir);
}

/* ── which boundary held ───────────────────────────────────────────────── */

Confinement boundary_state(const BoxBoundary *b)
{
	if (!b->tagged)
		return CONF_UNCONFINED;
	if (b->private_rundir && b->bus == 1 && !b->x11)
		return CONF_CONFINED;
	return CONF_TAGGED;
}

/* The Boxes page's three words, and the only spelling of them here. */
const char *confinement_name(Confinement c)
{
	switch (c) {
	case CONF_CONFINED:
		return "confined";
	case CONF_TAGGED:
		return "tagged";
	default:
		return "unconfined";
	}
}

/*
 * WHERE THE COOKIE IS DECIDES WHETHER `x11 = no` MEANS ANYTHING. Xwayland
 * listens on /tmp/.X11-unix, which a box shares, and on an abstract socket,
 * which any box in the host's network namespace reaches; what keeps a box out
 * is the cookie it does not have. A cookie under /tmp, or under a home the box
 * shares, is one it can read, and then nothing withholds the display. An X
 * server with a display and no cookie at all is taken as one that asks for
 * none.
 */
int x11_cookie_exposed(const Profile *p)
{
	const char *xa = getenv("XAUTHORITY");
	const char *home = kb_home_dir();
	size_t hl = strlen(home);
	char dot[MAX_LINE];

	if (xa && *xa) {
		if (!strncmp(xa, "/tmp/", 5))
			return 1;
		if (!p->privhome && !strncmp(xa, home, hl) && xa[hl] == '/')
			return 1;
	} else if (getenv("DISPLAY") && *getenv("DISPLAY")) {
		return 1;
	}
	snprintf(dot, sizeof(dot), "%s/.Xauthority", home);
	return !p->privhome && kb_path_exists(dot);
}

static int state_path(const char *box, const char *ext, char *out, size_t n)
{
	char sd[MAX_LINE];
	int w;

	if (box_state_dir(sd, sizeof(sd)) != 0)
		return -1;
	w = snprintf(out, n, "%s/%s.%s", sd, box, ext);
	return w < 0 || (size_t)w >= n ? -1 : 0;
}

int box_state_write(const char *box, const BoxBoundary *b)
{
	char path[MAX_LINE];
	KbBuf t = {0};
	Confinement c = boundary_state(b);
	int rc;

	if (state_path(box, "state", path, sizeof(path)) != 0)
		return -1;
	kb_buf_printf(&t,
		"# kdos-appbox: which boundary held for this box's last launch\n"
		"confinement = %s\n"
		"launcher    = %s\n"
		"socket      = %s\n"
		"runtime     = %s\n"
		"bus         = %s\n"
		"x11         = %s\n"
		"engine      = %s\n"
		"reason      = %s\n"
		"time        = %ld\n",
		confinement_name(c), b->launcher,
		b->tagged ? "tagged" : "refused",
		b->private_rundir ? "private" : "session",
		b->bus == 1 ? "filtered" : "none",
		b->x11 ? "reachable" : "no",
		c == CONF_CONFINED ? "io.kdos.appbox+private"
		: c == CONF_TAGGED ? "io.kdos.appbox+tagged" : "",
		b->reason, (long)time(NULL));
	rc = kb_write_file_atomic(path, t.p);
	kb_buf_free(&t);
	return rc;
}

void box_state_clear(const char *box)
{
	char path[MAX_LINE];

	if (state_path(box, "state", path, sizeof(path)) == 0)
		unlink(path);
	if (state_path(box, "apps", path, sizeof(path)) == 0)
		unlink(path);
}

/*
 * THE LEDGER LINE IS WRITTEN BY THE PROCESS THAT BECOMES `podman exec`, so its
 * pid is the launch's for as long as the application runs, and the start time
 * beside it is what tells that pid from a later process given the same
 * number. An argument with a tab or a newline in it cannot be written back as
 * one word, and that launch is left out rather than restored as something
 * else.
 */
void box_ledger_add(const char *box, const char *launcher, int argc,
		    char **argv)
{
	char path[MAX_LINE];
	unsigned long long start;
	KbBuf t = {0};
	int fd;

	if (state_path(box, "apps", path, sizeof(path)) != 0)
		return;
	for (int i = 0; i < argc; i++)
		if (strpbrk(argv[i], "\t\n"))
			return;
	start = proc_start_time((long)getpid());
	if (!start)
		return;
	kb_buf_printf(&t, "%ld %llu run %s", (long)getpid(), start, launcher);
	for (int i = 0; i < argc; i++)
		kb_buf_printf(&t, "\t%s", argv[i]);
	kb_buf_str(&t, "\n");
	/* Under the file's lock: the keeper rewrites the ledger in place
	 * under the same lock when a launch ends. */
	fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
	if (fd >= 0) {
		if (flock(fd, LOCK_EX) == 0)
			(void)!write(fd, t.p, t.n);
		close(fd);
	}
	kb_buf_free(&t);
}
