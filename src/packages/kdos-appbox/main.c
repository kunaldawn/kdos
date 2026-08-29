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
 *   kdos-appbox run <app> [args...]   run an app from its pack's box
 *   kdos-appbox open <path>           open a file in whatever opens it
 *   kdos-appbox warmup                compose + start the pinned set
 *   kdos-appbox status                daemon / store / box state
 *   kdos-appbox list                  boxes
 *   kdos-appbox apps                  known alien apps
 *   kdos-appbox genlaunchers …        launchers, shims, mime cache, table
 *
 * Boxes themselves are `kdos-box`'s — a second name on this binary. The launch
 * path keeps the ordering the launchers and the login warmup depend on; every
 * comment marked "cost a debug cycle" below describes something that broke.
 */

#include "kdos-appbox.h"
#include "kxdg.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
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
 * outlive us: `run` execs podman, and the context lives exactly as long as
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
 *                              theme package, so the image lane asks the
 *                              image's labels and the pack lane asks the pack
 *                              stack's own `env =` lines.
 *   GTK_THEME=KDOS             belt and braces next to gtk-3.0/settings.ini
 */
static void box_env(KbArgv *a, const char *image, const char *pack)
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
	/* PODMAN EXEC DOES NOT INHERIT PID 1's ENVIRONMENT. kdos-boxinit sets
	 * this PATH for what IT starts; a launch is a separate exec and gets
	 * podman's default, which has no /usr/games — so every Debian game in
	 * the catalogue died on `env: 'sol': No such file or directory`. */
	kb_argv_add(a, "PATH=" KB_BOX_PATH);

	/*
	 * THE REST OF WHAT A LOGIN GIVES YOU, because `podman exec` gives none
	 * of it and distrobox-enter used to. The image lane inherited a full
	 * environment from distrobox; the pack lane replaced that with a bare
	 * exec and kept only the variables this function had always added, so a
	 * boxed application came up with no locale, no USER and no
	 * XDG_RUNTIME_DIR. Measured across the catalogue:
	 *
	 *   aisleriot   Non UTF-8 locale (ANSI_X3.4-1968) is not supported!
	 *   solvespace  Sorry, only UTF-8 locales are supported.
	 *   bleachbit   KeyError: 'USER'
	 *
	 * and those are only the ones that SAY so — most of the rest simply
	 * never mapped a window. `C.UTF-8` rather than the host's own LANG:
	 * it exists in every Debian without a locale being generated, and the
	 * box's userland is Debian's whatever the host is set to.
	 */
	kb_argv_addf(a, "XDG_RUNTIME_DIR=/run/user/%u", (unsigned)getuid());
	/* The session bus is at a FIXED path the box shares (see kdos-desktop);
	 * GLib would find it from XDG_RUNTIME_DIR alone, but a GApplication
	 * that cannot reach the bus blocks in single-instance negotiation with
	 * no window and no message, so the address is stated rather than
	 * left to a fallback. */
	kb_argv_addf(a, "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%u/bus",
		     (unsigned)getuid());
	kb_argv_add(a, "XDG_SESSION_TYPE=wayland");
	kb_argv_addf(a, "HOME=%s", kb_home_dir());
	{
		struct passwd *me = getpwuid(getuid());
		const char *who = me && me->pw_name ? me->pw_name : "kdos";
		kb_argv_addf(a, "USER=%s", who);
		kb_argv_addf(a, "LOGNAME=%s", who);
	}
	if (!getenv("LANG") || !strstr(getenv("LANG"), "UTF-8"))
		kb_argv_add(a, "LANG=C.UTF-8");
	else
		kb_argv_addf(a, "LANG=%s", getenv("LANG"));

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
	if (pack && *pack) {
		/*
		 * THE PACK LANE ASKS THE PACK, because there is no image to
		 * label: a pack box is `podman --rootfs`, so `podman image
		 * inspect` answers no to everything and every Qt app would
		 * come up grey under an inert QT_QPA_PLATFORMTHEME. The
		 * runtime that installs the platform theme declares the
		 * variable that selects it, in its own metadata, which is the
		 * same "cannot drift" property the label had.
		 *
		 * Not only Qt: whatever the stack declares is exported, so a
		 * runtime can state anything its own packages need without a
		 * line of C being written for it.
		 */
		char env[PACK_ENV_MAX][256];
		int n = pack_env(pack, env, PACK_ENV_MAX);

		/* kb_argv_add keeps the POINTER, and these are this frame's.
		 * A value may say `$HOME` for the directory a boxgraft lands
		 * in: the pack cannot know the user's name and the box shares
		 * the home under the same path. */
		for (int i = 0; i < n; i++) {
			char *eq = strchr(env[i], '=');
			if (eq && !strncmp(eq + 1, "$HOME", 5)) {
				char *v = kb_calloc(1, 1024);
				snprintf(v, 1024, "%.*s=%s%s", (int)(eq - env[i]),
					 env[i], kb_home_dir(), eq + 6);
				kb_argv_add(a, v);
			} else {
				kb_argv_add(a, kb_strdup(env[i]));
			}
		}
	} else if (image_has_label(image, "kdos.qt-kde-theme")) {
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
 * One-time rootless storage tuning, and it governs THE PODMAN STORE ONLY —
 * which is the image lane and a dev box built from an `image:` base. A pack is
 * mounted by kdos-packd as root and is not in the store at all, so the driver
 * question does not arise for an app box.
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

/*
 * The pack lane's launch. Everything the image lane does that is about the
 * MACHINE rather than about distrobox is shared: the stuck-in-`stopping`
 * recovery, the readiness wait on `container_setup_done`, the notification,
 * the trace lines and the whole of box_env(). What differs is three calls —
 * compose, create, exec — in place of `distrobox enter`.
 */
static int run_pack(int argc, char **argv, const char *app, const char *state)
{
	Profile p;
	KbArgv a = {0};
	char id[128];
	int i;

	profile_load(&p, g_box);

	/* The box is named after the pack, so the box name IS the id unless
	 * somebody aimed --box somewhere else. */
	kb_strlcpy(id, p.base[0] && !strncmp(p.base, "pack:", 5) ? p.base + 5
								 : g_box,
		   sizeof(id));

	if (pack_box_ensure(g_box, id) != 0)
		kb_die("could not compose box '%s' from packs", g_box);
	tracef("ensured");

	if (pack_box_start(g_box) != 0)
		kb_die("could not start box '%s'", g_box);

	/*
	 * The same readiness contract as the image lane: kdos-boxinit prints
	 * `container_setup_done` and this waits for it. It is quick here —
	 * boxinit writes three records and prints — but a launch that raced it
	 * would exec into a box with no user in it, which is the failure the
	 * marker exists for.
	 */
	if (!box_setup_done(g_box) && box_wait_ready(g_box, 30) != 0)
		tracef("wait_ready timed out");

	tracef("entering");
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "exec");
	kb_argv_add(&a, "--interactive");
	/* A terminal program asked for from a terminal gets one. Without
	 * `--tty` the app's stdin is a pipe: R refuses to start, and every
	 * REPL in the catalogue loses line editing and its prompt. A launcher
	 * has no tty and takes the plain exec. */
	if (isatty(0))
		kb_argv_add(&a, "--tty");
	kb_argv_addf(&a, "--user=%u:%u", (unsigned)getuid(), (unsigned)getgid());
	kb_argv_addf(&a, "--workdir=%s", kb_home_dir());
	kb_argv_add(&a, g_box);
	/* box_env writes `env NAME=value …` in front of the command, so the
	 * environment reaches the app the same way in both lanes and there is
	 * one place where GTK_USE_PORTAL and the rest are decided. */
	box_env(&a, p.image, id);
	for (i = 0; i < argc; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);
	execvp(a.v[0], (char *const *)a.v);
	kb_die("podman: not found");
	(void)state;
	(void)app;
	return 1;
}

int cmd_run(int argc, char **argv)
{
	Profile p;
	char state[64];
	const char *app = argv[0];
	int i;

	/* The desktop entry's form: resolve the pack from the exec. An exec no
	 * installed pack carries is refused by name — composing a box called
	 * "kdos-apps" out of a pack of that name fails a step later with a
	 * sentence about a box nobody asked for. */
	if (!strcmp(g_box, DEFAULT_BOX)) {
		char pack[128] = "", joined[1024] = "";
		/* The whole argv, so `sh -c "…"` and `env X=y prog` resolve
		 * to the program they run rather than to the wrapper. */
		for (i = 0; i < argc; i++) {
			size_t l = strlen(joined);
			snprintf(joined + l, sizeof(joined) - l, "%s\"%s\"",
				 i ? " " : "", argv[i]);
		}
		if (app_pack_by_exec(joined, pack, sizeof(pack)) && pack[0])
			g_box = kb_strdup(pack);
		else
			kb_die("no installed pack carries '%s' — "
			       "kdos app list, then kdos app install <pack>", app);
	}

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
	return run_pack(argc, argv, app, state);
}

/*
 * WARM THE PINNED SET. One box per application means the image lane's single
 * warmup no longer covers anything: every application's first launch of the
 * session paid compose + create + boxinit + its own start, measured at 18 s
 * cold, and the warmup that used to hide that started a box nothing runs any
 * more. `~/.config/kdos/favorites` is the set the user CHOSE, so those are the
 * boxes worth having running before the first click — resolved through the
 * alien-apps table to their packs, composed and started one at a time.
 *
 * Non-blocking lock, so a second warmup is a no-op and never a queue; `nice`
 * is the caller's (kdos-desktop runs this at 10). A box that is started and
 * idle costs a conmon and nothing else, and `kdos-box gc` — run on a timer
 * by the same session — is what gives it back.
 */
int cmd_warmup(void)
{
	char *lockpath, *fav;
	char buf[4096], *line, *save;
	int fd, n = 0;

	lockpath = kb_path_join(kb_runtime_dir(), "kdos-appbox.warmup.lock");
	fd = kb_lock_file(lockpath, 1);
	free(lockpath);
	if (fd < 0)
		return 0;

	fav = kb_path_join(kb_home_dir(), ".config/kdos/favorites");
	if (kb_read_file(fav, buf, sizeof(buf)) <= 0) {
		tracef("warmup: no favorites at %s", fav);
		free(fav);
		close(fd);
		return 0;
	}
	tracef("warmup: reading %s", fav);
	free(fav);

	for (line = strtok_r(buf, "\n", &save); line && n < 8;
	     line = strtok_r(NULL, "\n", &save)) {
		char pack[128] = "", cmd[512];

		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		/*
		 * A FAVORITE IS A DESKTOP ID AND THE TABLE IS KEYED BY SHIM.
		 * `org.gnome.GHex` is the launcher's file id; its Exec= names
		 * the shim, `ghex`, which is the table's key and carries the
		 * pack in its third field. The desktop entry is the one place
		 * both names meet, so it is read rather than guessed at.
		 */
		/*
		 * A GENERATED LAUNCHER'S EXEC IS `kdos-appbox run <exec>`, and
		 * the pack is resolved from that <exec> exactly as `run` does
		 * (app_pack_by_exec: the table's command column, whole then by
		 * basename). Taking the first word as the shim answered
		 * 'kdos-appbox' for every entry genlaunchers writes, and the
		 * warmup skipped the whole pinned set while exiting 0. An entry
		 * somebody wrote by hand naming the shim itself still resolves
		 * through the table by name.
		 */
		char shim[128] = "";
		{
			char path[MAX_LINE];
			KxdgEntry e;
			snprintf(path, sizeof(path),
				 "%s/.local/share/applications/%s.desktop",
				 kb_home_dir(), line);
			if (!kb_path_exists(path))
				snprintf(path, sizeof(path),
					 "/usr/share/applications/%s.desktop", line);
			if (kxdg_load(&e, path, "Desktop Entry") == 0) {
				const char *ex = kxdg_get(&e, "Exec", "");
				const char *b = strrchr(ex, '/');
				b = b ? b + 1 : ex;
				snprintf(shim, sizeof(shim), "%.*s",
					 (int)strcspn(b, " \t"), b);
				if (!strcmp(shim, "kdos-appbox")) {
					const char *r = strstr(ex, " run ");
					if (r)
						app_pack_by_exec(r + 5, pack,
								 sizeof(pack));
					shim[0] = 0;
				}
				kxdg_free(&e);
			}
		}
		if (shim[0] &&
		    (!app_lookup_pack(shim, cmd, sizeof(cmd), pack, sizeof(pack))))
			pack[0] = 0;
		if (!pack[0]) {
			tracef("warmup: %s -> pack '%s': skipped", line, pack);
			continue;
		}
		tracef("warmup %s -> %s", line, pack);
		if (pack_box_ensure(pack, pack) == 0)
			pack_box_start(pack);
		n++;
	}
	close(fd);
	return 0;
}

int cmd_status(void)
{
	Profile p;
	char state[64], st[4096];

	profile_load(&p, g_box);
	if (packd_ask("status", st, sizeof(st)) == 0)
		fputs(st, stdout);
	else
		printf("packd : not answering\n");
	box_state(g_box, state, sizeof(state));
	printf("box   : %s (%s)\n",
	       strcmp(state, "absent") ? "created"
				       : "not created (first launch will)", state);
	return 0;
}

static void usage(void)
{
	fputs(
"Usage: kdos-appbox run <app> [args...]     run an app from its pack's box\n"
"       kdos-appbox open [--print|--choose] <path>\n"
"                                           open a file in whatever opens it\n"
"       kdos-appbox warmup | status\n"
"       kdos-appbox list                    boxes and their profiles\n"
"       kdos-appbox apps                    known alien apps\n"
"       kdos-appbox genlaunchers --packs <fs-root> | --packs --user\n"
"                               --packs-dir <dir> <fs-root>\n"
"                               <desktop-dir> <fs-root>\n"
"\n"
"  -b, --box <name>   run in <name> instead of the app's own pack box\n"
"  -v, --verbose      let podman print to stderr\n"
"\n"
"Boxes are kdos-box's: kdos-box list|create|enter|profile|remove ...\n",
	      stderr);
}

/*
 * Invoked through one of the /usr/local/bin symlinks named after an app —
 * `gimp photo.png` rather than `kdos-appbox run gimp-3.0 photo.png`. Same
 * dispatch busybox does, and it keeps the alien-app path free of any shell.
 */
static int run_as_shim(const char *name, int argc, char **argv)
{
	char cmd[512], store[1024], pack[128] = "";
	const char *slot[KB_MAX_ARGV];
	char *vec[KB_MAX_ARGV];
	int n = 0, i;

	if (!app_lookup_pack(name, cmd, sizeof(cmd), pack, sizeof(pack)))
		kb_die("unknown alien app '%s' (try: kdos-appbox apps)", name);

	/*
	 * ONE BOX PER APPLICATION in the pack lane, named after the pack. The
	 * launch path below is otherwise identical, and `--box` still wins —
	 * that is how the same application is run in a second box.
	 */
	if (pack[0] && !strcmp(g_box, DEFAULT_BOX))
		g_box = kb_strdup(pack);

	/*
	 * kxdg_exec_split, not a `strtok(" ")`: the table's commands come out
	 * of upstream .desktop files and carry the format's QUOTING, which a
	 * whitespace split gets wrong. Measured on the shipped image —
	 * `gsmartcontrol` is `"/usr/bin/gsmartcontrol-root"` and was exec'd
	 * with the quotes still on the path, and `wesnoth` is
	 * `sh -c "wesnoth-1.18 >/dev/null 2>&1"`, whose one shell argument was
	 * handed to sh in three pieces. Both looked exactly like an app that
	 * does not start.
	 *
	 * Field codes are .desktop placeholders for the file the user picked;
	 * from a terminal the user's own argv takes that role, so they expand
	 * to nothing here and the argv is appended after.
	 */
	n = kxdg_exec_split(cmd, NULL, 0, store, sizeof(store), slot,
			    KB_MAX_ARGV - argc - 1);
	if (n <= 0)
		kb_die("empty or unparseable command for '%s'", name);
	for (i = 0; i < n; i++)
		vec[i] = (char *)slot[i];
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

	/* kdos-box is a second NAME on this binary, the way kpkg/kpkgadd and
	 * ksvc/service already are — not a second program. */
	if (!strcmp(self, "kdos-box"))
		return box_main(argc - 1, argv + 1);
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
	if (CMD("warmup"))
		return cmd_warmup();
	if (CMD("status"))
		return cmd_status();
	if (CMD("list"))
		return box_list();
	if (CMD("apps"))
		return app_list();
	if (CMD("genlaunchers")) {
		/*
		 * Two sources, one set of outputs. `--packs <fs-root>` walks
		 * every installed app pack; the two-argument form reads one
		 * directory, which is what the image lane's bake hands it.
		 */
		/* `--packs --user` writes the USER tree — no fs-root, no root:
		 * this is what `kdos app install` runs as the calling user. */
		if (i + 2 < argc && !strcmp(argv[i + 1], "--packs") &&
		    !strcmp(argv[i + 2], "--user")) {
			if (i + 3 < argc)
				kb_die("usage: kdos-appbox genlaunchers --packs --user");
			return cmd_genlaunchers(NULL, NULL, 1, 0);
		}
		/* `--packs-dir <root> <fs-root>`: the BUILD's form — one extracted
		 * pack per subdirectory of <root>, named after the pack. */
		if (i + 3 < argc && !strcmp(argv[i + 1], "--packs-dir"))
			return cmd_genlaunchers(argv[i + 2], argv[i + 3], 0, 1);
		if (i + 1 < argc && !strcmp(argv[i + 1], "--packs")) {
			/* AN EXTRA ARGUMENT IS REFUSED RATHER THAN IGNORED.
			 * The two forms differ by one argument, so
			 * `--packs <desktop-dir> <fs-root>` — the image lane's
			 * shape with the flag added — reads the DESKTOP-DIR as
			 * the fs-root and writes the whole set under it:
			 * a table at <dir>/usr/share/kdos/alien-apps that
			 * nothing reads, no shims swept, and the real table
			 * left as whoever wrote it last. It exits 0. */
			if (i + 2 >= argc || i + 3 < argc)
				kb_die("usage: kdos-appbox genlaunchers --packs "
				       "<fs-root>   (no desktop-dir: the packs "
				       "are the source)");
			return cmd_genlaunchers(NULL, argv[i + 2], 0, 0);
		}
		if (i + 2 >= argc)
			kb_die("usage: kdos-appbox genlaunchers "
			       "<desktop-dir> <fs-root>\n"
			       "       kdos-appbox genlaunchers --packs <fs-root>");
		return cmd_genlaunchers(argv[i + 1], argv[i + 2], 0, 0);
	}
#undef CMD

	usage();
	return 1;
}
