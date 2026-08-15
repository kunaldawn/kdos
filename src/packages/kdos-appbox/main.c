/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-appbox
 *
 * Alien app runtime and box manager.
 *
 *   kdos-appbox run <app> [args...]   run an app from a box
 *   kdos-appbox ensure [box]          create the box if missing
 *   kdos-appbox warmup [box]          create + start it in advance
 *   kdos-appbox status                image / box state
 *
 *   kdos-appbox list                  boxes
 *   kdos-appbox apps                  known alien apps
 *   kdos-appbox create <box> [k=v..]  new box with a sandbox profile
 *   kdos-appbox remove <box>          delete a box
 *   kdos-appbox recreate <box>        re-create it with the current profile
 *   kdos-appbox install <pkg> [-b B]  apt install into a box, add launchers
 *   kdos-appbox uninstall <pkg>       apt remove
 *   kdos-appbox refresh [box]         re-scan a box for new desktop entries
 *   kdos-appbox security <box> [k=v]  show or change the sandbox profile
 *   kdos-appbox tui                   full-screen manager
 *
 * The launch path (run/ensure/warmup) is a straight port of the shell script
 * this replaced, down to the ordering, because 91 .desktop launchers and the
 * login warmup all depend on its exact behaviour. Every comment marked "cost a
 * debug cycle" below describes something that actually broke.
 */

#include "kdos-appbox.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *g_box = DEFAULT_BOX;

/*
 * The box's own tagged Wayland socket, or NULL to use the session's.
 *
 * kdos-boxsock binds a socket per box and hands it to kdos-comp through
 * security-context-v1, so every client on it is tagged with this box's name and
 * gets the capability policy from the SAME ~/.config/kdos/boxes/<box>.conf that
 * decided the container's namespaces. It must be started detached and must
 * outlive us: `run` execs distrobox, and the context lives exactly as long as
 * kdos-boxsock holds its close fd. It is idempotent under a flock, so starting
 * it on every launch costs one failed lock after the first.
 *
 * Returns the shared session socket when anything at all is missing —
 * kdos-boxsock, the compositor's support for the protocol, the runtime dir. A
 * box on the shared socket is unconfined at the protocol level, which is what
 * KDOS did before this existed. Silently substituting an untagged socket for a
 * tagged one is the honest failure: the alternative is an app that does not
 * start at all because its sandbox could not be labelled.
 */
static const char *box_wayland_socket(void)
{
	static char path[512];
	const char *rundir = getenv("XDG_RUNTIME_DIR");
	KbArgv a = {0};

	if (!rundir || !*rundir)
		return NULL;
	/* Checked by path, not by PATH search: it is our own binary at a known
	 * place, and the point of the check is to skip the socket wait entirely
	 * on a tree that does not carry it. */
	if (!kb_path_exists(KDOS_BOXSOCK))
		return NULL;

	snprintf(path, sizeof(path), "%s/kdos-box-%s.sock", rundir, g_box);

	kb_argv_add(&a, KDOS_BOXSOCK);
	kb_argv_add(&a, (char *)g_box);
	kb_argv_end(&a);
	kb_run_detach(&a);

	/*
	 * Wait for the socket rather than assuming it: the holder has a registry
	 * round trip to do first, and handing the box a WAYLAND_DISPLAY that
	 * does not exist yet is an app that dies at startup with "failed to
	 * connect to display". Short, because on the second launch it is
	 * already there.
	 */
	for (int i = 0; i < 50; i++) {
		if (kb_path_exists(path))
			return path;
		usleep(20 * 1000);
	}
	tracef("boxsock: no socket for %s, using the session's", g_box);
	return NULL;
}

/*
 * Accessibility, opt-in.
 *
 * The host runs no at-spi registry and nothing answers org.a11y.Bus on the
 * session bus, so by default every boxed GTK app is told not to look — that is
 * a startup probe that can only time out. It is a DEFAULT and not a policy:
 * a user who wants the box's own stack (the image carries at-spi2-core, and a
 * screen reader running inside the box can reach it) opts in by creating
 * ~/.config/kdos/a11y — an empty file is enough — or by exporting KDOS_A11Y=1
 * for one launch. Resolved from $HOME/.config like the box profiles, and for
 * the same reason: two programs resolving it differently is the failure this
 * must not have.
 */
static int a11y_wanted(void)
{
	char path[MAX_LINE];
	const char *env = getenv("KDOS_A11Y");

	if (env && *env)
		return strcmp(env, "0") != 0;
	snprintf(path, sizeof(path), "%s/.config/kdos/a11y", kb_home_dir());
	return kb_path_exists(path);
}

/*
 * Environment every app inside a box gets.
 *
 *   WAYLAND_DISPLAY            the box's own tagged socket, when there is one
 *   GSETTINGS_BACKEND=keyfile  no dconf-service is reachable over the (host)
 *                              session bus; keyfile keeps GNOME app settings
 *                              persistent instead of silently dropped
 *   NO_AT_BRIDGE / GTK_A11Y    no accessibility stack is reachable on the host;
 *                              stop every GTK app probing org.a11y.Bus at
 *                              startup — unless the user opted in, see
 *                              a11y_wanted()
 *   QT_QPA_PLATFORMTHEME       how a Qt app finds a palette: `kde` when the
 *                              image has the KDE segment (it reads the
 *                              kdeglobals `kdos theme` writes into the shared
 *                              home, which is the direct route), `gtk3`
 *                              otherwise. Inert without the matching platform
 *                              theme package, hence the label checks.
 *   GTK_THEME=KDOS             belt and braces next to gtk-3.0/settings.ini
 */
static void box_env(KbArgv *a, const char *image)
{
	const char *display;
	const char *sock;

	kb_argv_add(a, "env");

	/*
	 * A per-box socket is the client's identity, so it must be set before
	 * anything in the box connects. WAYLAND_DISPLAY takes an absolute path
	 * here rather than a name — libwayland accepts either, and the socket
	 * lives beside the session's in $XDG_RUNTIME_DIR, which the box shares.
	 */
	sock = box_wayland_socket();
	if (sock)
		kb_argv_addf(a, "WAYLAND_DISPLAY=%s", sock);
	kb_argv_add(a, "GSETTINGS_BACKEND=keyfile");
	if (!a11y_wanted()) {
		kb_argv_add(a, "NO_AT_BRIDGE=1");
		kb_argv_add(a, "GTK_A11Y=none");
	}
	kb_argv_add(a, "GTK_THEME=KDOS");

	/*
	 * THE PORTAL, AND WHY THE ENV IS WHAT SWITCHES IT ON.
	 *
	 * KDOS serves FileChooser, Settings and OpenURI from
	 * xdg-desktop-portal-kdos, and every one of those is for the benefit of
	 * boxed applications — but a GTK app only ROUTES through the portal
	 * when it believes it is sandboxed, which it decides from
	 * /.flatpak-info or from this variable. A distrobox is neither, so
	 * every one of them went on drawing its own GtkFileChooser: the KDOS
	 * file dialog existed, the portal answered, and nothing ever called it.
	 * Firefox's `widget.use-xdg-desktop-portal.file-picker` default of 2
	 * ("auto") consults the same variable, so it is covered by this line
	 * too.
	 *
	 * The cost, stated: with the portal on, GTK's Print dialog also goes
	 * through it, and KDOS has no Print backend (kdos-portals.conf says
	 * `default=none` and means it). Printing still works from a boxed app:
	 * box_create() shares /run/cups into the box and CUPS_SERVER below
	 * points every toolkit's OWN print dialog at it, which is the route
	 * that needs no portal.
	 *
	 * XDG_CURRENT_DESKTOP goes with it because a toolkit that has decided
	 * to use portals then asks which desktop it is on, and the answer
	 * inside the container is otherwise nothing at all.
	 */
	kb_argv_add(a, "GTK_USE_PORTAL=1");
	kb_argv_add(a, "XDG_CURRENT_DESKTOP=KDOS");

	/*
	 * Printing. box_create() shares /run/cups into the box as a VOLUME —
	 * a create-time property — while this export is per-LAUNCH and probes
	 * the socket again. Asymmetric on purpose: a box created before cups
	 * was up gets the env but not the socket, and needs `kdos-appbox
	 * recreate` to grow the volume, which is the same message the profile
	 * system already sends for a namespace change.
	 */
	if (kb_path_exists("/run/cups/cups.sock"))
		kb_argv_add(a, "CUPS_SERVER=/run/cups/cups.sock");

	/*
	 * Input methods. A boxed app reaches fcitx5 through the COMPOSITOR —
	 * text-input-v3 to kdos-comp, which relays to input-method-v2 — and
	 * never directly, because fcitx5 runs on the host and its bus name is
	 * not what the box would find anyway.
	 *
	 * So the value is `wayland` and emphatically not `fcitx`: the fcitx
	 * module is the X11-era route where each toolkit talks to the IM daemon
	 * itself, and inside a container that daemon is not there. GTK on
	 * Wayland picks the right context on its own when GTK_IM_MODULE is
	 * UNSET, which is why only Qt is named here — setting GTK_IM_MODULE at
	 * all is how a working GTK app stops accepting CJK input.
	 */
	kb_argv_add(a, "QT_IM_MODULE=wayland");

	/*
	 * Two routes to a themed Qt app, and the KDE one is better where it
	 * exists: the `kde` platform theme reads ~/.config/kdeglobals, which
	 * `kdos theme` writes into the home the box already shares, so Breeze
	 * paints in the KDOS palette with no bridge and no style override.
	 *
	 * Without the KDE segment it is qgtk3 plus Fusion, and Fusion is not
	 * optional there: the Breeze style kdenlive and shotcut pull in paints
	 * from its own colour scheme and ignores the palette qgtk3 hands it. But
	 * Fusion with NO platform theme falls back to Qt's built-in LIGHT
	 * palette, worse than doing nothing — hence the label check.
	 */
	if (image_has_label(image, "kdos.qt-kde-theme")) {
		kb_argv_add(a, "QT_QPA_PLATFORMTHEME=kde");
	} else {
		kb_argv_add(a, "QT_QPA_PLATFORMTHEME=gtk3");
		if (image_has_label(image, "kdos.qt-gtk-theme"))
			kb_argv_add(a, "QT_STYLE_OVERRIDE=Fusion");
	}

	/*
	 * Cost a debug cycle: a few apps are X11-only and their own .desktop
	 * says so (debian ships audacity as `env GDK_BACKEND=x11 audacity`).
	 * kdos-comp runs Xwayland rootlessly but exports DISPLAY only to what
	 * IT spawned, so a launcher fired from the dock may not have it and the
	 * app then exits with no window and no message. The socket is the
	 * authority, and distrobox shares the host /tmp.
	 */
	display = getenv("DISPLAY");
	if (display && *display) {
		kb_argv_addf(a, "DISPLAY=%s", display);
	} else {
		DIR *d = opendir("/tmp/.X11-unix");
		struct dirent *e;
		while (d && (e = readdir(d))) {
			if (e->d_name[0] == 'X' && e->d_name[1]) {
				kb_argv_addf(a, "DISPLAY=:%s", e->d_name + 1);
				break;
			}
		}
		if (d)
			closedir(d);
	}
}

/*
 * One-time rootless storage tuning.
 *
 * /etc/containers/storage.conf pins fuse-overlayfs, which the live ISO NEEDS:
 * $HOME sits on the boot overlay and the kernel refuses to stack an overlay
 * upperdir on overlayfs — podman does not fall back, the container simply
 * fails to mount. On an installed system ($HOME on ext4) the kernel mounts
 * rootless overlays natively and every file an app reads stops going through
 * a userspace FUSE daemon.
 *
 * Decided once, before the store holds any container: fuse and native overlay
 * write incompatible whiteout metadata into container rw layers, so the choice
 * must never flip afterwards.
 */
static void tune_storage(void)
{
	char conf[MAX_LINE], mounts[1 << 16], best[256] = {0}, type[64] = {0};
	char *line, *save;
	const char *home = kb_home_dir();
	size_t homelen = strlen(home);

	snprintf(conf, sizeof(conf), "%s/.config/containers/storage.conf", home);
	if (kb_path_exists(conf))
		return;
	{
		char cj[MAX_LINE], buf[4096];
		snprintf(cj, sizeof(cj), "%s/.local/share/containers/storage/"
			 "overlay-containers/containers.json", home);
		if (kb_read_file(cj, buf, sizeof(buf)) > 0 && strstr(buf, "\"id\""))
			return;
	}
	if (kb_read_file("/proc/mounts", mounts, sizeof(mounts)) < 0)
		return;

	for (line = strtok_r(mounts, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[256], mnt[256], fs[64];
		size_t mlen;
		if (sscanf(line, "%255s %255s %63s", dev, mnt, fs) != 3)
			continue;
		mlen = strlen(mnt);
		if (mlen > homelen || strncmp(home, mnt, mlen))
			continue;
		if (mlen > 1 && home[mlen] && home[mlen] != '/')
			continue;
		if (mlen >= strlen(best)) {
			snprintf(best, sizeof(best), "%s", mnt);
			snprintf(type, sizeof(type), "%s", fs);
		}
	}
	if (strcmp(type, "ext4") && strcmp(type, "ext3") && strcmp(type, "ext2") &&
	    strcmp(type, "btrfs") && strcmp(type, "xfs") && strcmp(type, "f2fs"))
		return;

	{
		char dir[MAX_LINE];
		snprintf(dir, sizeof(dir), "%s/.config/containers", home);
		kb_mkdir_p(dir);
	}
	kb_write_file(conf, "[storage]\ndriver = \"overlay\"\n");
}

int cmd_run(int argc, char **argv)
{
	Profile p;
	KbArgv a = {0};
	char state[64];
	const char *app = argv[0];
	int i;

	profile_load(&p, g_box);
	box_state(g_box, state, sizeof(state));

	if (strcmp(state, "running"))
		notify("Starting app", "Bringing up the app container — "
				       "a few extra seconds.");

	/*
	 * Cost a debug cycle: a hung app (D-state I/O on fuse-overlayfs) can
	 * wedge the box in "stopping" forever, and every enter then dies
	 * instantly with "container state improper" — no app ever opens again.
	 * Wait it out, then force our way back to a startable container. The box
	 * is stateless (image + shared $HOME), so nothing is lost.
	 */
	if (!strcmp(state, "stopping")) {
		for (i = 0; i < 15 && !strcmp(state, "stopping"); i++) {
			sleep(1);
			box_state(g_box, state, sizeof(state));
		}
		if (!strcmp(state, "stopping")) {
			KbArgv k = {0};
			kb_argv_add(&k, "podman");
			kb_argv_add(&k, "kill");
			kb_argv_add(&k, g_box);
			kb_argv_end(&k);
			kb_run(&k);
			sleep(2);
			box_state(g_box, state, sizeof(state));
		}
		if (!strcmp(state, "stopping")) {
			KbArgv r = {0};
			notify("Resetting app container",
			       "The app container was stuck — recreating it.");
			kb_argv_add(&r, "podman");
			kb_argv_add(&r, "rm");
			kb_argv_add(&r, "-f");
			kb_argv_add(&r, "-t");
			kb_argv_add(&r, "0");
			kb_argv_add(&r, g_box);
			kb_argv_end(&r);
			kb_run(&r);
		}
	}

	tracef("run %s status=%s", app, state);
	if (box_ensure(g_box) != 0)
		kb_die("could not create box '%s'", g_box);
	tracef("ensured");

	/*
	 * Only wait when someone ELSE already started the box: then
	 * distrobox-enter will not wait for setup itself. When the box is
	 * stopped we hand straight over — distrobox-enter starts it and does its
	 * own (progress-printing) wait.
	 *
	 * This used to be a blind 120-second lock on the warmup, which
	 * serialized every launch behind the whole login warmup: on a slow disk
	 * that is minutes of a dead-looking desktop for the first click.
	 */
	if (!strcmp(state, "running") && !box_setup_done(g_box)) {
		notify("Starting app", "Finishing app container setup…");
		if (box_wait_ready(g_box, 120) != 0)
			tracef("wait_ready timed out");
	}

	tracef("entering");
	kb_argv_add(&a, "distrobox");
	kb_argv_add(&a, "enter");
	kb_argv_add(&a, g_box);
	kb_argv_add(&a, "--");
	box_env(&a, p.image);
	for (i = 0; i < argc; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);
	execvp(a.v[0], (char *const *)a.v);
	kb_die("distrobox: not found");
	return 1;
}

int cmd_ensure(void)
{
	return box_ensure(g_box);
}

int cmd_warmup(void)
{
	Profile p;
	KbArgv a = {0};
	char *lockpath;
	int fd;

	profile_load(&p, g_box);
	if (!image_exists(p.image))
		return 0;

	/* Non-blocking: a second warmup is a no-op, never a queue. */
	lockpath = kb_path_join(kb_runtime_dir(), "kdos-appbox.warmup.lock");
	fd = kb_lock_file(lockpath, 1);
	free(lockpath);
	if (fd < 0)
		return 0;

	if (box_ensure(g_box) == 0) {
		kb_argv_add(&a, "distrobox");
		kb_argv_add(&a, "enter");
		kb_argv_add(&a, g_box);
		kb_argv_add(&a, "--");
		kb_argv_add(&a, "true");
		kb_argv_end(&a);
		kb_run(&a);
	}
	close(fd);
	return 0;
}

int cmd_status(void)
{
	Profile p;
	char state[64];

	profile_load(&p, g_box);
	printf("image : %s\n", image_exists(p.image) ? "baked" : "missing");
	box_state(g_box, state, sizeof(state));
	printf("box   : %s (%s)\n",
	       strcmp(state, "absent") ? "created" : "not created (first launch will)",
	       state);
	/* Which route Qt apps take to the KDOS palette, in the order box_env
	 * picks them. "absent" for both means an image baked before either
	 * platform theme was added — a re-bake, not a config bug. */
	printf("qt    : %s\n",
	       image_has_label(p.image, "kdos.qt-kde-theme")
		       ? "kde platform theme (reads ~/.config/kdeglobals)"
	       : image_has_label(p.image, "kdos.qt-gtk-theme")
		       ? "qgtk3 + Fusion (reads the GTK theme)"
		       : "absent (Qt apps keep their own palette)");
	return 0;
}

static int cmd_security(int argc, char **argv)
{
	Profile p;
	int i, changed = 0;

	if (argc < 1)
		kb_die("usage: kdos-appbox security <box> [key=value ...]");
	profile_load(&p, argv[0]);
	for (i = 1; i < argc; i++) {
		if (profile_set(&p, argv[i]) != 0)
			kb_die("unknown setting '%s' (try: network=private, "
			    "ipc=private, devices=private, processes=private, "
			    "home=private, init=yes, image=<ref>)", argv[i]);
		changed = 1;
	}
	if (!changed) {
		profile_print(&p);
		return 0;
	}
	if (profile_save(&p) != 0)
		kb_die("could not write the profile");
	profile_print(&p);
	if (box_exists(p.name))
		printf("\nNamespaces cannot be re-flagged on a live container.\n"
		       "Run: kdos-appbox recreate %s\n", p.name);
	return 0;
}

static int cmd_create(int argc, char **argv)
{
	Profile p;
	int i;

	if (argc < 1)
		kb_die("usage: kdos-appbox create <box> [key=value ...]");
	profile_defaults(&p, argv[0]);
	for (i = 1; i < argc; i++)
		if (profile_set(&p, argv[i]) != 0)
			kb_die("unknown setting '%s'", argv[i]);
	if (box_exists(p.name))
		kb_die("box '%s' already exists", p.name);
	if (profile_save(&p) != 0)
		kb_die("could not write the profile");
	if (box_create(&p) != 0)
		kb_die("could not create box '%s'", p.name);
	printf("created %s\n", p.name);
	return 0;
}

static int cmd_recreate(const char *box)
{
	Profile p;
	profile_load(&p, box);
	if (box_exists(box) && box_remove(box, 1) != 0)
		kb_die("could not remove box '%s'", box);
	if (box_create(&p) != 0)
		kb_die("could not create box '%s'", box);
	printf("recreated %s\n", box);
	return 0;
}

static void usage(void)
{
	fputs(
"Usage: kdos-appbox run <app> [args...]     run an app from a box\n"
"       kdos-appbox open [--print|--choose] <path>\n"
"                                           open a file in whatever opens it\n"
"       kdos-appbox ensure | warmup | status\n"
"\n"
"       kdos-appbox list                    boxes and their profiles\n"
"       kdos-appbox apps                    known alien apps\n"
"       kdos-appbox create <box> [k=v...]   new box with a sandbox profile\n"
"       kdos-appbox remove <box>            delete a box\n"
"       kdos-appbox recreate <box>          re-create with the saved profile\n"
"       kdos-appbox install <pkg>           apt install into the box\n"
"       kdos-appbox uninstall <pkg>         apt remove\n"
"       kdos-appbox refresh                 re-scan the box for new apps\n"
"       kdos-appbox security <box> [k=v...] show or change confinement\n"
"       kdos-appbox tui                     full-screen manager\n"
"\n"
"  -b, --box <name>   operate on <name> instead of " DEFAULT_BOX "\n"
"  -v, --verbose      let podman/distrobox print to stderr\n"
"\n"
"Profile keys: network, ipc, devices, processes, home (shared|private),\n"
"              init (yes|no), image (<ref>). Applied when a box is created.\n",
	      stderr);
}

/*
 * Invoked through one of the /usr/local/bin symlinks named after an app —
 * `gimp photo.png` rather than `kdos-appbox run gimp-3.0 photo.png`. Same
 * dispatch busybox does, and it keeps the alien-app path free of any shell.
 */
static int run_as_shim(const char *name, int argc, char **argv)
{
	char cmd[512];
	char *vec[KB_MAX_ARGV];
	int n = 0, i;
	char *w, *save;

	if (!app_lookup(name, cmd, sizeof(cmd)))
		kb_die("unknown alien app '%s' (try: kdos-appbox apps)", name);

	for (w = strtok_r(cmd, " ", &save); w && n < KB_MAX_ARGV - argc - 1;
	     w = strtok_r(NULL, " ", &save)) {
		/* Field codes are .desktop placeholders for the file the user
		 * picked; from a terminal the user's own argv takes that role. */
		if (w[0] == '%' && w[1] && !w[2])
			continue;
		vec[n++] = w;
	}
	if (!n)
		kb_die("empty command for '%s'", name);
	for (i = 0; i < argc; i++)
		vec[n++] = argv[i];
	vec[n] = NULL;
	return cmd_run(n, vec);
}

int main(int argc, char **argv)
{
	const char *self = strrchr(argv[0], '/');
	int i = 1;

	self = self ? self + 1 : argv[0];
	kb_set_progname("kdos-appbox");
	tune_storage();

	if (strcmp(self, "kdos-appbox"))
		return run_as_shim(self, argc - 1, argv + 1);

	while (i < argc && argv[i][0] == '-') {
		if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--box")) {
			if (i + 1 >= argc)
				kb_die("--box needs a name");
			g_box = argv[++i];
		} else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			kb_proc_verbose = 1;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage();
			return 0;
		} else {
			kb_die("unknown option '%s'", argv[i]);
		}
		i++;
	}
	if (i >= argc) {
		usage();
		return 1;
	}

#define CMD(s) (!strcmp(argv[i], s))
	if (CMD("run")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox run <app> [args...]");
		return cmd_run(argc - i - 1, argv + i + 1);
	}
	if (CMD("open")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox open [--print] [--choose] "
			       "<path> [path...]");
		return cmd_open(argc - i - 1, argv + i + 1);
	}
	if (CMD("ensure"))
		return cmd_ensure();
	if (CMD("warmup"))
		return cmd_warmup();
	if (CMD("status"))
		return cmd_status();
	if (CMD("list"))
		return box_list();
	if (CMD("apps"))
		return app_list();
	if (CMD("image"))
		return cmd_image(argc - i - 1, argv + i + 1);
	if (CMD("genlaunchers")) {
		if (i + 2 >= argc)
			kb_die("usage: kdos-appbox genlaunchers "
			       "<desktop-dir> <fs-root>");
		return cmd_genlaunchers(argv[i + 1], argv[i + 2]);
	}
	if (CMD("create"))
		return cmd_create(argc - i - 1, argv + i + 1);
	if (CMD("remove")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox remove <box>");
		return box_remove(argv[i + 1], 1);
	}
	if (CMD("recreate")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox recreate <box>");
		return cmd_recreate(argv[i + 1]);
	}
	if (CMD("install")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox install <pkg>");
		return app_install(g_box, argv[i + 1]);
	}
	if (CMD("uninstall")) {
		if (i + 1 >= argc)
			kb_die("usage: kdos-appbox uninstall <pkg>");
		return app_uninstall(g_box, argv[i + 1]);
	}
	if (CMD("refresh"))
		return app_refresh(g_box);
	if (CMD("security"))
		return cmd_security(argc - i - 1, argv + i + 1);
	if (CMD("tui"))
		return tui_main();
#undef CMD

	usage();
	return 1;
}
