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
int g_verbose;

/*
 * Environment every app inside a box gets.
 *
 *   GSETTINGS_BACKEND=keyfile  no dconf-service is reachable over the (host)
 *                              session bus; keyfile keeps GNOME app settings
 *                              persistent instead of silently dropped
 *   NO_AT_BRIDGE / GTK_A11Y    KDOS ships no accessibility stack; stop every
 *                              GTK app probing org.a11y.Bus at startup
 *   QT_QPA_PLATFORMTHEME=gtk3  Qt apps take their palette from the GTK theme
 *                              via qgtk3. Inert without debian's
 *                              qt{5,6}-gtk-platformtheme in the image.
 *   GTK_THEME=KDOS             belt and braces next to gtk-3.0/settings.ini
 */
static void box_env(Argv *a, const char *image)
{
	const char *display;

	argv_add(a, "env");
	argv_add(a, "GSETTINGS_BACKEND=keyfile");
	argv_add(a, "NO_AT_BRIDGE=1");
	argv_add(a, "GTK_A11Y=none");
	argv_add(a, "QT_QPA_PLATFORMTHEME=gtk3");
	argv_add(a, "GTK_THEME=KDOS");

	/*
	 * The Breeze style that kdenlive and shotcut pull in paints from its own
	 * colour scheme and ignores the palette qgtk3 hands it, so the platform
	 * theme alone leaves those apps grey; Fusion honours it. But Fusion with
	 * NO platform theme falls back to Qt's built-in LIGHT palette, which is
	 * worse than doing nothing — hence the image label check.
	 */
	if (image_has_qt_gtk(image))
		argv_add(a, "QT_STYLE_OVERRIDE=Fusion");

	/*
	 * Cost a debug cycle: a few apps are X11-only and their own .desktop
	 * says so (debian ships audacity as `env GDK_BACKEND=x11 audacity`).
	 * cosmic-comp runs Xwayland rootlessly but exports DISPLAY only to what
	 * IT spawned, so a launcher fired from the dock may not have it and the
	 * app then exits with no window and no message. The socket is the
	 * authority, and distrobox shares the host /tmp.
	 */
	display = getenv("DISPLAY");
	if (display && *display) {
		argv_addf(a, "DISPLAY=%s", display);
	} else {
		DIR *d = opendir("/tmp/.X11-unix");
		struct dirent *e;
		while (d && (e = readdir(d))) {
			if (e->d_name[0] == 'X' && e->d_name[1]) {
				argv_addf(a, "DISPLAY=:%s", e->d_name + 1);
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
	const char *home = home_dir();
	size_t homelen = strlen(home);

	snprintf(conf, sizeof(conf), "%s/.config/containers/storage.conf", home);
	if (file_exists(conf))
		return;
	{
		char cj[MAX_LINE], buf[4096];
		snprintf(cj, sizeof(cj), "%s/.local/share/containers/storage/"
			 "overlay-containers/containers.json", home);
		if (read_file(cj, buf, sizeof(buf)) > 0 && strstr(buf, "\"id\""))
			return;
	}
	if (read_file("/proc/mounts", mounts, sizeof(mounts)) < 0)
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
		mkdir_p(dir);
	}
	write_file(conf, "[storage]\ndriver = \"overlay\"\n");
}

int cmd_run(int argc, char **argv)
{
	Profile p;
	Argv a = {0};
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
			Argv k = {0};
			argv_add(&k, "podman");
			argv_add(&k, "kill");
			argv_add(&k, g_box);
			argv_end(&k);
			run_quiet(&k);
			sleep(2);
			box_state(g_box, state, sizeof(state));
		}
		if (!strcmp(state, "stopping")) {
			Argv r = {0};
			notify("Resetting app container",
			       "The app container was stuck — recreating it.");
			argv_add(&r, "podman");
			argv_add(&r, "rm");
			argv_add(&r, "-f");
			argv_add(&r, "-t");
			argv_add(&r, "0");
			argv_add(&r, g_box);
			argv_end(&r);
			run_quiet(&r);
		}
	}

	tracef("run %s status=%s", app, state);
	if (box_ensure(g_box) != 0)
		die("could not create box '%s'", g_box);
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
	argv_add(&a, "distrobox");
	argv_add(&a, "enter");
	argv_add(&a, g_box);
	argv_add(&a, "--");
	box_env(&a, p.image);
	for (i = 0; i < argc; i++)
		argv_add(&a, argv[i]);
	argv_end(&a);
	execvp(a.v[0], (char *const *)a.v);
	die("distrobox: not found");
	return 1;
}

int cmd_ensure(void)
{
	return box_ensure(g_box);
}

int cmd_warmup(void)
{
	Profile p;
	Argv a = {0};
	char *lockpath;
	int fd;

	profile_load(&p, g_box);
	if (!image_exists(p.image))
		return 0;

	/* Non-blocking: a second warmup is a no-op, never a queue. */
	lockpath = path_join(runtime_dir(), "kdos-appbox.warmup.lock");
	fd = lock_file(lockpath, 1);
	free(lockpath);
	if (fd < 0)
		return 0;

	if (box_ensure(g_box) == 0) {
		argv_add(&a, "distrobox");
		argv_add(&a, "enter");
		argv_add(&a, g_box);
		argv_add(&a, "--");
		argv_add(&a, "true");
		argv_end(&a);
		run_quiet(&a);
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
	printf("qt-gtk: %s\n", image_has_qt_gtk(p.image)
	       ? "available" : "absent (Qt apps keep their own palette)");
	return 0;
}

static int cmd_security(int argc, char **argv)
{
	Profile p;
	int i, changed = 0;

	if (argc < 1)
		die("usage: kdos-appbox security <box> [key=value ...]");
	profile_load(&p, argv[0]);
	for (i = 1; i < argc; i++) {
		if (profile_set(&p, argv[i]) != 0)
			die("unknown setting '%s' (try: network=private, "
			    "ipc=private, devices=private, processes=private, "
			    "home=private, init=yes, image=<ref>)", argv[i]);
		changed = 1;
	}
	if (!changed) {
		profile_print(&p);
		return 0;
	}
	if (profile_save(&p) != 0)
		die("could not write the profile");
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
		die("usage: kdos-appbox create <box> [key=value ...]");
	profile_defaults(&p, argv[0]);
	for (i = 1; i < argc; i++)
		if (profile_set(&p, argv[i]) != 0)
			die("unknown setting '%s'", argv[i]);
	if (box_exists(p.name))
		die("box '%s' already exists", p.name);
	if (profile_save(&p) != 0)
		die("could not write the profile");
	if (box_create(&p) != 0)
		die("could not create box '%s'", p.name);
	printf("created %s\n", p.name);
	return 0;
}

static int cmd_recreate(const char *box)
{
	Profile p;
	profile_load(&p, box);
	if (box_exists(box) && box_remove(box, 1) != 0)
		die("could not remove box '%s'", box);
	if (box_create(&p) != 0)
		die("could not create box '%s'", box);
	printf("recreated %s\n", box);
	return 0;
}

static void usage(void)
{
	fputs(
"Usage: kdos-appbox run <app> [args...]     run an app from a box\n"
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
	char *vec[MAX_ARGV];
	int n = 0, i;
	char *w, *save;

	if (!app_lookup(name, cmd, sizeof(cmd)))
		die("unknown alien app '%s' (try: kdos-appbox apps)", name);

	for (w = strtok_r(cmd, " ", &save); w && n < MAX_ARGV - argc - 1;
	     w = strtok_r(NULL, " ", &save)) {
		/* Field codes are .desktop placeholders for the file the user
		 * picked; from a terminal the user's own argv takes that role. */
		if (w[0] == '%' && w[1] && !w[2])
			continue;
		vec[n++] = w;
	}
	if (!n)
		die("empty command for '%s'", name);
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
	tune_storage();

	if (strcmp(self, "kdos-appbox"))
		return run_as_shim(self, argc - 1, argv + 1);

	while (i < argc && argv[i][0] == '-') {
		if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--box")) {
			if (i + 1 >= argc)
				die("--box needs a name");
			g_box = argv[++i];
		} else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			g_verbose = 1;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage();
			return 0;
		} else {
			die("unknown option '%s'", argv[i]);
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
			die("usage: kdos-appbox run <app> [args...]");
		return cmd_run(argc - i - 1, argv + i + 1);
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
	if (CMD("create"))
		return cmd_create(argc - i - 1, argv + i + 1);
	if (CMD("remove")) {
		if (i + 1 >= argc)
			die("usage: kdos-appbox remove <box>");
		return box_remove(argv[i + 1], 1);
	}
	if (CMD("recreate")) {
		if (i + 1 >= argc)
			die("usage: kdos-appbox recreate <box>");
		return cmd_recreate(argv[i + 1]);
	}
	if (CMD("install")) {
		if (i + 1 >= argc)
			die("usage: kdos-appbox install <pkg>");
		return app_install(g_box, argv[i + 1]);
	}
	if (CMD("uninstall")) {
		if (i + 1 >= argc)
			die("usage: kdos-appbox uninstall <pkg>");
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
