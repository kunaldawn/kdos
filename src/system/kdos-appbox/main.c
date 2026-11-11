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
 *   kdos-appbox throwaway <file>      a file in a box that is gone after
 *   kdos-appbox warmup                compose + start the favourite boxes
 *   kdos-appbox session save|restore  the boxes across a logout
 *   kdos-appbox status                daemon / store / box state
 *   kdos-appbox list                  boxes
 *   kdos-appbox apps                  known alien apps
 *   kdos-appbox genlaunchers …        launchers, icons, shims, mime cache,
 *                                     table, window rules
 *
 * Boxes themselves are `kdos-box`'s — a second name on this binary. The launch
 * path keeps the ordering the launchers and the login warmup depend on: the
 * box exists, it runs, its private runtime directory and its bus are ready,
 * its launcher's tagged socket is served, and only then does the application
 * start — or it does not start, and the reason is printed.
 */

#include "kdos-appbox.h"
#include "kxdg.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const char *g_box = DEFAULT_BOX;
const char *g_launcher;

/*
 * WHICH LAUNCHER A LAUNCH IS, which is the app id KWin gives its windows.
 *
 * KWin replaces a tagged window's app_id with the security context's app id
 * unless the window's own starts with it, and sets resourceClass and
 * desktopFileName from the result. So the id handed to kdos-boxsock decides
 * the icon a window shows, the pinned launcher it lights and the window rules
 * that match it, and it has to be the file name of a launcher genlaunchers
 * wrote: `<box>` for a box with one launcher, `<box>.<stem>` for each of a
 * box with several.
 *
 * `-l` names it outright, and every generated launcher passes one. A shim, a
 * prompt and `kdos-box run` do not, and the box's own launchers are asked
 * which one runs this program — by the program a command line runs, the same
 * key the alien-apps table is matched by. A program no launcher of the box
 * runs gets the box's own name, which is the box's rule in KWin and in the
 * Boxes page whatever its icon.
 *
 * A THROWAWAY BOX takes the launcher id of the box it was made from: it is
 * named `<launcher>.tw<n>`, and its windows are that application's.
 */
static int launcher_related(const char *box, const char *id)
{
	size_t bl = strlen(box), il = strlen(id);

	if (!box_name_ok(id))
		return 0;
	if (!strcmp(id, box))
		return 1;
	if (il > bl + 1 && !strncmp(id, box, bl) && id[bl] == '.' &&
	    !strchr(id + bl + 1, '.'))
		return 1;
	return bl > il + 1 && !strncmp(box, id, il) && box[il] == '.' &&
	       !strchr(box + il + 1, '.');
}

static const char *resolve_launcher(int argc, char **argv)
{
	BoxLauncher *bl;
	char joined[1024] = "", key[256], lk[256];
	const char *found = NULL;
	int n;

	if (g_launcher) {
		if (!launcher_related(g_box, g_launcher))
			kb_die("%s is not a launcher of the box %s — a launcher "
			       "id is the box, or the box and one more component",
			       g_launcher, g_box);
		return g_launcher;
	}
	for (int i = 0; i < argc; i++) {
		size_t l = strlen(joined);

		snprintf(joined + l, sizeof(joined) - l, "%s\"%s\"", i ? " " : "",
			 argv[i]);
	}
	app_exec_key(joined, key, sizeof(key));
	bl = kb_calloc(64, sizeof(*bl));
	n = box_launchers(g_box, bl, 64);
	for (int i = 0; i < n && !found; i++) {
		app_exec_key(bl[i].exec, lk, sizeof(lk));
		if (key[0] && !strcmp(lk, key) && launcher_related(g_box, bl[i].id))
			found = kb_strdup(bl[i].id);
	}
	free(bl);
	return found ? found : g_box;
}

/*
 * A HELPER THAT STAYS, AND THE ONE LINE IT ANSWERS WITH. kdos-boxsock prints
 * `ready <path>` or `error <reason>`, closes its standard output and then
 * holds the socket for the box's life, so it cannot be waited for: it is
 * started detached — a grandchild, reparented as soon as its parent exits, so
 * the `podman exec` this process becomes never inherits it as a child — and
 * its output is read to end of file, bounded, because a helper that hung
 * before answering would otherwise hang the launch with it.
 */
static int detach_read_line(const KbArgv *a, char *out, size_t n, int ms)
{
	int fd[2];
	pid_t pid;
	size_t got = 0;

	out[0] = '\0';
	if (pipe2(fd, O_CLOEXEC) != 0)
		return -1;
	pid = fork();
	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}
	if (pid == 0) {
		if (fork() == 0) {
			int null = open("/dev/null", O_RDWR);

			if (null >= 0) {
				dup2(null, STDIN_FILENO);
				dup2(null, STDERR_FILENO);
			}
			dup2(fd[1], STDOUT_FILENO);
			kb_child_reset_signals();
			setsid();
			execvp(a->v[0], (char *const *)a->v);
			_exit(127);
		}
		_exit(0);
	}
	close(fd[1]);
	waitpid(pid, NULL, 0);
	for (;;) {
		struct pollfd pf = { .fd = fd[0], .events = POLLIN };
		ssize_t r;

		if (poll(&pf, 1, ms) <= 0)
			break;
		r = read(fd[0], out + got, n - 1 - got);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		got += (size_t)r;
		if (got >= n - 1)
			break;
	}
	close(fd[0]);
	out[got] = '\0';
	out[strcspn(out, "\n")] = '\0';
	return got ? 0 : -1;
}

/*
 * THE LAUNCHER'S TAGGED SOCKET, from kdos-boxsock, or the reason there is
 * none. There is no fallback to the session's own socket: a window on it is
 * untagged, every global KWin denies a box is open to it, and the launch that
 * took it would report a confinement nothing enforces. A box whose socket
 * cannot be had does not start, and `why` is what the person is told.
 *
 * $KDOS_BOXSOCK names another engine, which is how the selftest drives this
 * path without a compositor.
 */
static int box_socket(const char *box, const char *launcher, int confined,
		      char *path, size_t pn, char *why, size_t wn)
{
	const char *engine = getenv("KDOS_BOXSOCK");
	char line[512];
	KbArgv a = {0};

	if (!engine || !*engine)
		engine = KDOS_BOXSOCK;
	if (!kb_path_exists(engine)) {
		snprintf(why, wn, "kdos-boxsock is not installed, so the box "
			 "would have no tagged Wayland socket");
		return -1;
	}
	if (!getenv("WAYLAND_DISPLAY") || !*getenv("WAYLAND_DISPLAY")) {
		snprintf(why, wn, "there is no Wayland session to show it in "
			 "(WAYLAND_DISPLAY is unset)");
		return -1;
	}
	kb_argv_add(&a, engine);
	kb_argv_add(&a, "-s");
	kb_argv_add(&a, confined ? "confined" : "tagged");
	kb_argv_add(&a, box);
	kb_argv_add(&a, launcher);
	kb_argv_end(&a);
	if (detach_read_line(&a, line, sizeof(line), 5000) != 0) {
		snprintf(why, wn, "kdos-boxsock gave no answer within 5 s");
		return -1;
	}
	if (!strncmp(line, "ready ", 6) && line[6] == '/') {
		kb_strlcpy(path, line + 6, pn);
		return 0;
	}
	snprintf(why, wn, "kdos-boxsock: %.200s",
		 !strncmp(line, "error ", 6) ? line + 6 : line);
	return -1;
}

/*
 * kb_argv_add keeps the POINTER, and these are the caller's frame. A value may
 * say `$HOME` for the directory a boxgraft lands in: the catalogue cannot know
 * the user's name, and the box shares the home under the same path.
 */
static void export_env(KbArgv *a, char env[][256], int n)
{
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
}

/*
 * THE STORE LANE ASKS THE CATALOGUE, for the same reason the pack lane asks
 * the pack: a store-built box is `base=image:kdos/<id>` over an image this
 * machine built, so there is no vendor label on it to read and every Qt
 * application would come up grey under an inert QT_QPA_PLATFORMTHEME.
 *
 * The box NAME is the catalogue id — that is what `store_install` creates it
 * as — or `deb.<package>`, the Debian lane's box over the `base` row, which
 * takes that row's chain. A box whose name is neither is not a store box and
 * answers 0, leaving the label branches below to decide.
 */
static int store_env(const Profile *prof, KbArgv *a)
{
	char env[PACK_ENV_MAX][256];
	char err[256];
	const char *row = prof->name;
	int n;

	if (box_lane(prof) != BOX_LANE_STORE)
		return 0;
	if (cat_load(NULL, err, sizeof(err)) != 0)
		return 0;
	if (!strncmp(row, STORE_DEB_PREFIX, sizeof(STORE_DEB_PREFIX) - 1))
		row = "base";
	if (!cat_find(row)) {
		cat_free();
		return 0;
	}
	n = cat_env(row, env, PACK_ENV_MAX);
	export_env(a, env, n);
	cat_free();
	return 1;
}

/* What one launch hands the box: the sockets in it, by their in-box paths. */
typedef struct {
	char wayland[512];	/* "" when the profile says wayland = no      */
	char bus[512];		/* the proxied session bus                    */
	int  x11;		/* DISPLAY and the cookie are handed over     */
	int  a11y;		/* the accessibility bus is in reach          */
	int  private_layout;	/* /run/user/<uid> is the box's own directory */
} Launch;

/*
 * Environment every app inside a box gets.
 *
 *   WAYLAND_DISPLAY            the launcher's own tagged socket, by its path
 *                              inside the box
 *   DBUS_SESSION_BUS_ADDRESS   the box's filtered bus, the keeper's proxy
 *   XDG_RUNTIME_DIR            /run/user/<uid>, which in a pack or store box
 *                              is the box's private directory
 *   GSETTINGS_BACKEND=keyfile  no dconf-service is reachable over the
 *                              filtered bus; keyfile keeps GNOME app settings
 *                              persistent instead of silently dropped
 *   NO_AT_BRIDGE / GTK_A11Y    stop every boxed GTK app probing
 *                              org.a11y.Bus at startup — unless assistive
 *                              technology is in use, see box_a11y_wanted()
 *   QT_QPA_PLATFORMTHEME       how a Qt app finds a palette: `kde` when the
 *                              image has the KDE segment (it reads the
 *                              kdeglobals Plasma writes into the shared
 *                              home, which is the direct route), `gtk3`
 *                              otherwise. Inert without the matching platform
 *                              theme package, so the image lane asks the
 *                              image's labels and the pack lane asks the pack
 *                              stack's own `env =` lines.
 *   GTK_THEME                  the theme the session's GTK settings name,
 *                              see session_gtk_theme(): it wins over the
 *                              name the settings portal hands a box
 *   LIBGL_ALWAYS_SOFTWARE      only where the box's graphics are the CPU's,
 *                              see the `render` block below
 *   DISPLAY / XAUTHORITY       only for a box handed the X11 display
 */
/*
 * THE GTK THEME, NAMED OUTRIGHT. Plasma's GTK bridge writes the session's
 * theme into ~/.config/gtk-3.0/settings.ini and into dconf. Under GTK_USE_PORTAL
 * a boxed GTK program reads the name from the settings portal's GTK backend;
 * without the portal GSettings is the keyfile backend below, which holds none,
 * and GTK falls back to Adwaita with the session's colours in colors.css laid
 * over a theme they were not written for. GTK_THEME wins over both sources,
 * so the theme a boxed program draws does not depend on which of them it
 * reads. The name the bridge wrote, with `:dark` when it prefers the dark
 * variant, is therefore exported at every launch. A running program keeps
 * the theme and the colours it started with; an accent switch reaches it at
 * its next start. A name that is not a plain word is not exported.
 */
static int session_gtk_theme(char *out, size_t n)
{
	char path[MAX_LINE], text[4096], name[64] = "";
	const char *xdg = getenv("XDG_CONFIG_HOME");
	int dark = 0;

	if (xdg && *xdg == '/')
		snprintf(path, sizeof(path), "%s/gtk-3.0/settings.ini", xdg);
	else
		snprintf(path, sizeof(path), "%s/.config/gtk-3.0/settings.ini",
			 kb_home_dir());
	if (kb_read_file(path, text, sizeof(text)) <= 0)
		return 0;
	for (char *l = text, *nl; l && *l; l = nl) {
		const char *v;

		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		v = strchr(l, '=');
		if (!v)
			continue;
		v++;
		if (!strncmp(l, "gtk-theme-name=", 15))
			kb_strlcpy(name, v, sizeof(name));
		else if (!strncmp(l, "gtk-application-prefer-dark-theme=", 34))
			dark = !strcmp(v, "true") || !strcmp(v, "1");
	}
	if (!name[0] || strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnop"
				     "qrstuvwxyz0123456789._-") != strlen(name))
		return 0;
	snprintf(out, n, "%s%s", name, dark ? ":dark" : "");
	return 1;
}

static void box_env(KbArgv *a, const Profile *prof, const char *pack,
		    const Launch *l)
{
	kb_argv_add(a, "env");

	/*
	 * A per-launcher socket is the client's identity, so it must be set
	 * before anything in the box connects. WAYLAND_DISPLAY takes an
	 * absolute path here rather than a name — libwayland accepts either —
	 * and with `wayland = no` it is set to nothing at all, so a toolkit
	 * that would otherwise guess `wayland-0` finds no display to guess at.
	 */
	if (l->wayland[0])
		kb_argv_addf(a, "WAYLAND_DISPLAY=%s", l->wayland);
	else
		kb_argv_add(a, "WAYLAND_DISPLAY=");
	/* PODMAN EXEC DOES NOT INHERIT PID 1's ENVIRONMENT. kdos-boxinit sets
	 * this PATH for what IT starts; a launch is a separate exec and gets
	 * podman's default, which has no /usr/games — so without this every
	 * Debian game in the catalogue dies on
	 * `env: 'sol': No such file or directory`. */
	kb_argv_add(a, "PATH=" KB_BOX_PATH);

	/*
	 * THE REST OF WHAT A LOGIN GIVES YOU, because `podman exec` gives none
	 * of it, unlike distrobox-enter. A launch is a bare exec, and with only
	 * the variables above a boxed application comes up with no locale, no
	 * USER and no XDG_RUNTIME_DIR. Measured across the catalogue:
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
	kb_argv_addf(a, "XDG_RUNTIME_DIR=%s", box_inbox_rundir());
	/* The bus is the box's proxy, stated rather than left to a fallback:
	 * GLib and sd-bus look for `bus` in the runtime directory when the
	 * variable is unset, and a box has none; a GApplication that cannot
	 * reach a bus blocks in single-instance negotiation with no window and
	 * no message. */
	kb_argv_addf(a, "DBUS_SESSION_BUS_ADDRESS=unix:path=%s", l->bus);
	kb_argv_add(a, "XDG_SESSION_TYPE=wayland");
	kb_argv_addf(a, "HOME=%s", kb_home_dir());
	{
		struct passwd *me = getpwuid(getuid());
		const char *who = me && me->pw_name ? me->pw_name : "kdos";
		kb_argv_addf(a, "USER=%s", who);
		kb_argv_addf(a, "LOGNAME=%s", who);
	}
	kb_argv_add(a, "LANG=C.UTF-8");

	/* The box's own /etc/xdg first, then KDOS's defaults, bound in at
	 * create (pack_box_create): the order the host session has. A box
	 * created before the bind existed finds no such directory and reads
	 * its own /etc/xdg alone. */
	kb_argv_add(a, "XDG_CONFIG_DIRS=/etc/xdg:" BOX_KDOS_XDG);
	kb_argv_add(a, "GSETTINGS_BACKEND=keyfile");
	if (!l->a11y) {
		kb_argv_add(a, "NO_AT_BRIDGE=1");
		kb_argv_add(a, "GTK_A11Y=none");
	}
	/*
	 * THE PORTAL, AND WHY THE ENV IS WHAT SWITCHES IT ON.
	 *
	 * xdg-desktop-portal-kde serves FileChooser, OpenURI's application
	 * chooser, Settings, Print and the rest, and the box's bus proxy lets
	 * org.freedesktop.portal.* through — but a GTK app only ROUTES through
	 * the portal when it believes it is sandboxed, which it decides from
	 * /.flatpak-info or from this variable. A box is neither, so without it
	 * every boxed GTK app draws its own GtkFileChooser, reading a home the
	 * box may not share, and the KDE file dialog is never called. Firefox's
	 * `widget.use-xdg-desktop-portal.file-picker` default of 2 ("auto")
	 * consults the same variable, so it is covered by this line too.
	 *
	 * XDG_CURRENT_DESKTOP goes with it because a toolkit that has decided
	 * to use portals then asks which desktop it is on, and the answer
	 * inside the container is otherwise nothing at all.
	 */
	kb_argv_add(a, "GTK_USE_PORTAL=1");
	kb_argv_add(a, "XDG_CURRENT_DESKTOP=KDE");
	{
		char theme[80];

		if (session_gtk_theme(theme, sizeof(theme)))
			kb_argv_addf(a, "GTK_THEME=%s", theme);
	}

	/*
	 * Printing. pack_box_create() shares /run/cups into the box as a
	 * VOLUME — a create-time property — while this export is per-LAUNCH
	 * and probes the socket again. Asymmetric on purpose: a box created
	 * before cups was up gets the env but not the socket, and needs
	 * `kdos-box remove` and a new create to grow the volume, which is the
	 * same message the profile system sends for a namespace change.
	 */
	if (kb_path_exists("/run/cups/cups.sock"))
		kb_argv_add(a, "CUPS_SERVER=/run/cups/cups.sock");

	/*
	 * Input methods. A boxed app reaches fcitx5 through the COMPOSITOR —
	 * text-input to KWin, which relays to the input method KWin itself
	 * started — and never directly, because fcitx5 runs on the host and its
	 * bus name is not one the box's proxy passes.
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
	 * Plasma writes into the home the box already shares, so Breeze paints
	 * in the session's colours with no bridge and no style override.
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

		export_env(a, env, n);
	} else if (store_env(prof, a)) {
		/* The store lane declared its own, out of the catalogue. */
	} else if (image_has_label(prof->image, "kdos.qt-kde-theme")) {
		kb_argv_add(a, "QT_QPA_PLATFORMTHEME=kde");
	} else {
		kb_argv_add(a, "QT_QPA_PLATFORMTHEME=gtk3");
		if (image_has_label(prof->image, "kdos.qt-gtk-theme"))
			kb_argv_add(a, "QT_STYLE_OVERRIDE=Fusion");
	}

	/*
	 * WHICH GRAPHICS THE GUEST'S MESA MAY LOAD, out of the profile's
	 * `render` key resolved against this machine — see
	 * profile_render_gpu().
	 *
	 * NOTHING IS EXPORTED FOR THE HARDWARE ANSWER, because Mesa already
	 * asks the right question: it loads a driver for the render node it
	 * finds and falls back to llvmpipe on its own when the compositor
	 * offers neither linux-dmabuf nor wl_drm. A variable that pinned
	 * hardware would only take that fallback away.
	 *
	 * The software answer is the one that has to be stated. The render
	 * node is bound into the box and the drivers are in the base pack, so
	 * a box told `render = software` would otherwise load a hardware
	 * driver and draw with it. It governs GL and EGL and nothing else:
	 * VA-API and Vulkan find the render node by their own route, and
	 * denying those is the `gpu` key's job.
	 *
	 * ADVISORY AND PRINTED AS SUCH: it is an environment variable an
	 * application may unset, and `kdos-box profile` names it on the render
	 * line rather than claiming confinement this cannot enforce.
	 */
	if (!profile_render_gpu(prof))
		kb_argv_add(a, "LIBGL_ALWAYS_SOFTWARE=1");

	/*
	 * DISPLAY, for a box that is handed X11 (profile_x11): an application
	 * whose own entry or runtime forces an X11 backend, or one the profile
	 * says yes for. The socket is the authority — KWin exports DISPLAY only
	 * into what the session started, and a launch from a prompt may not
	 * have it — and the cookie is the box's copy in its own runtime
	 * directory, or the session's own where the box shares that directory.
	 * Every other box is told there is no display, so a toolkit with an X11
	 * fallback does not reach for one it would be refused.
	 */
	if (l->x11) {
		const char *display = getenv("DISPLAY");
		const char *xa = getenv("XAUTHORITY");

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
		if (l->private_layout)
			kb_argv_addf(a, "XAUTHORITY=%s/Xauthority",
				     box_inbox_rundir());
		else if (xa && *xa)
			kb_argv_addf(a, "XAUTHORITY=%s", xa);
	} else {
		kb_argv_add(a, "DISPLAY=");
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
 * WHAT THIS LAUNCH CAN HAND THE BOX, decided once and recorded.
 *
 * The order is the boundary's: the private directory is refreshed (audio
 * links, the cookie), the keeper is asked for the box's proxied bus, the
 * state that follows from the two decides which engine string KWin is given
 * (`io.kdos.appbox+private` for a confined box, `+tagged` otherwise), and only
 * then is the tagged socket asked for. A launch that cannot get the socket or
 * the bus is refused with the reason, and the refusal is recorded as the
 * box's state, so the Boxes page and the badge say what happened rather than
 * what was meant to.
 */
static void launch_prepare(const Profile *p, const char *pack,
			   const char *launcher, Launch *l)
{
	BoxBoundary b;
	char why[256] = "", host[512];
	int layout = box_layout_private(p->name);

	memset(l, 0, sizeof(*l));
	memset(&b, 0, sizeof(b));
	kb_strlcpy(b.launcher, launcher, sizeof(b.launcher));
	l->private_layout = b.private_rundir = layout == 1;
	l->a11y = box_a11y_wanted();

	if (rundir_prepare(p, pack) != 0)
		kb_die("%s: its private runtime directory could not be made", p->name);

	/* The X11 display is in reach when it is handed over, when the box
	 * shares the session's runtime directory and so the cookie in it, and
	 * when the cookie sits where the box can read it anyway. */
	l->x11 = profile_x11(p, pack);
	b.x11 = l->x11 || !l->private_layout || x11_cookie_exposed(p);

	if (keeper_ensure(p->name, why, sizeof(why)) != 0) {
		snprintf(b.reason, sizeof(b.reason), "%s", why);
		box_state_write(p->name, &b);
		notify("App not started", why);
		kb_die("%s: %s — the box was not started", p->name, why);
	}
	b.bus = 1;
	if (rundir_inbox_path(p->name, l->private_layout, BOX_PROXY_LEAF,
			      l->bus, sizeof(l->bus)) != 0)
		kb_die("%s: the bus path does not fit", p->name);

	if (!p->wayland) {
		/* No display at all is tagged by construction where the box
		 * cannot reach the session's socket either; where it can, it
		 * is not confined, and the state says so. */
		b.tagged = l->private_layout;
		if (!b.tagged)
			snprintf(b.reason, sizeof(b.reason), "wayland = no, and "
				 "the box shares the session's runtime directory "
				 "with its untagged socket in it");
	} else {
		BoxBoundary want = b;

		want.tagged = 1;
		if (box_socket(p->name, launcher,
			       boundary_state(&want) == CONF_CONFINED, host,
			       sizeof(host), why, sizeof(why)) != 0) {
			snprintf(b.reason, sizeof(b.reason), "%s", why);
			box_state_write(p->name, &b);
			notify("App not started", why);
			kb_die("%s: %s — the box was not started rather than "
			       "started untagged", p->name, why);
		}
		b.tagged = 1;
		if (rundir_inbox_path(p->name, l->private_layout,
				      kb_basename(host), l->wayland,
				      sizeof(l->wayland)) != 0)
			kb_die("%s: the socket path does not fit", p->name);
	}

	if (!b.reason[0] && boundary_state(&b) != CONF_CONFINED)
		snprintf(b.reason, sizeof(b.reason), "%s",
			 !l->private_layout
			 ? "the box shares the session's runtime directory, so "
			   "the session's own Wayland socket and bus are in reach"
			 : "the box is handed the X11 display, whose windows "
			   "reach KWin untagged");
	box_state_write(p->name, &b);
	tracef("launch %s as %s: %s", p->name, launcher,
	       confinement_name(boundary_state(&b)));
}

/*
 * The pack lane's launch. Everything the image lane does that is about the
 * MACHINE rather than about distrobox is shared: the stuck-in-`stopping`
 * recovery, the readiness wait on `container_setup_done`, the notification,
 * the trace lines and the whole of box_env(). What differs is three calls —
 * compose, create, exec — in place of `distrobox enter`.
 *
 * `wait` runs the application as a child and returns its exit status, which
 * is what a throwaway box needs in order to remove itself; otherwise this
 * process BECOMES `podman exec`, and its pid is the launch's for the ledger.
 */
static int run_pack(int argc, char **argv, int wait)
{
	Profile p;
	KbArgv a = {0};
	Launch l;
	char id[128];
	const char *launcher;
	int i;

	profile_load(&p, g_box);
	launcher = resolve_launcher(argc, argv);

	/* The box is named after the pack, so the box name IS the id unless
	 * somebody aimed --box somewhere else. A store box (`base=image:`) has
	 * no pack, and its empty id is what sends box_env() to the catalogue
	 * rather than to kdos-packd. */
	if (!strncmp(p.base, "image:", 6))
		id[0] = '\0';
	else
		kb_strlcpy(id, !strncmp(p.base, "pack:", 5) ? p.base + 5
							    : g_box,
			   sizeof(id));

	/* A failure follows the "Starting app" notification with podman's
	 * reason, so a click that opens nothing says why. */
	if (pack_box_ensure(g_box, id[0] ? id : g_box) != 0) {
		notify("App not started", pack_box_error());
		kb_die("could not create box '%s': %s", g_box, pack_box_error());
	}
	tracef("ensured");

	if (pack_box_start(g_box) != 0) {
		notify("App not started", pack_box_error());
		kb_die("could not start box '%s': %s", g_box, pack_box_error());
	}

	/*
	 * The same readiness contract as the image lane: kdos-boxinit prints
	 * `container_setup_done` and this waits for it. It is quick here —
	 * boxinit writes three records and prints — but a launch that raced it
	 * would exec into a box with no user in it, which is the failure the
	 * marker exists for.
	 */
	if (!box_setup_done(g_box) && box_wait_ready(g_box, 30) != 0)
		tracef("wait_ready timed out");

	launch_prepare(&p, id, launcher, &l);

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
	box_env(&a, &p, id, &l);
	for (i = 0; i < argc; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);

	if (!wait) {
		box_ledger_add(g_box, launcher, argc, argv);
		execvp(a.v[0], (char *const *)a.v);
		kb_die("podman: not found");
	}
	{
		pid_t pid = fork();
		int st = 0;

		if (pid < 0)
			kb_die("fork: %s", strerror(errno));
		if (pid == 0) {
			kb_child_reset_signals();
			execvp(a.v[0], (char *const *)a.v);
			_exit(127);
		}
		while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
			;
		return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
	}
}

/*
 * NO `-b`, SO THE EXEC HAS TO NAME THE PACK. A generated launcher for an app
 * that belongs to a pack passes `-b <pack>` and never reaches here; a prompt,
 * a shim and an entry naming no box do, and the pack is looked up from the
 * exec. An exec no installed pack carries is refused by name — composing a box
 * called "kdos-apps" out of a pack of that name fails a step later with a
 * sentence about a box nobody asked for.
 */
static void resolve_box(int argc, char **argv)
{
	char pack[128] = "", joined[1024] = "";

	if (strcmp(g_box, DEFAULT_BOX))
		return;
	/* The whole argv, so `sh -c "…"` and `env X=y prog` resolve to the
	 * program they run rather than to the wrapper. */
	for (int i = 0; i < argc; i++) {
		size_t l = strlen(joined);
		snprintf(joined + l, sizeof(joined) - l, "%s\"%s\"",
			 i ? " " : "", argv[i]);
	}
	if (app_pack_by_exec(joined, pack, sizeof(pack)) && pack[0])
		g_box = kb_strdup(pack);
	else
		kb_die("no installed pack carries '%s' — "
		       "kdos app list, then kdos app install <pack>", argv[0]);
}

static int run_common(int argc, char **argv, int wait)
{
	Profile p;
	char state[64];
	const char *app = argv[0];

	resolve_box(argc, argv);
	if (!box_name_ok(g_box))
		kb_die("'%s' is not a box name — [A-Za-z0-9._-]", g_box);
	profile_load(&p, g_box);
	box_state(g_box, state, sizeof(state));

	if (strcmp(state, "running"))
		notify("Starting app", "Bringing up the app container — "
				       "a few extra seconds.");

	/*
	 * A hung app (D-state I/O on fuse-overlayfs) can wedge the box in
	 * "stopping" forever, and every enter then dies instantly with
	 * "container state improper" — no app ever opens again. box_unstick
	 * runs the stop sequence; a pack box that still will not settle is
	 * removed, and run_pack composes it again over the same stack, which
	 * loses nothing. A store box (`base=image:`) keeps its writable layer
	 * inside the container, so it is left stuck rather than deleted, and
	 * the launch stops with the command that would discard it.
	 */
	switch (box_unstick(g_box, state, sizeof(state))) {
	case 3:
		notify("Resetting app container",
		       "The app container was stuck — recreating it.");
		break;
	case 4:
		notify("App container stuck",
		       "It is kept with its data; kdos-box remove --force "
		       "discards it.");
		return 1;
	}

	tracef("run %s status=%s", app, state);
	return run_pack(argc, argv, wait);
}

int cmd_run(int argc, char **argv)
{
	return run_common(argc, argv, 0);
}

int cmd_run_wait(int argc, char **argv)
{
	return run_common(argc, argv, 1);
}

/*
 * WHICH BOX A FAVOURITE NAMES. A line is a desktop-entry id or a box name.
 * A generated launcher says its box outright (`X-KDOS-Box`); an entry without
 * that key, written by hand, names it through its Exec — `kdos-appbox -b
 * <pack> … run <exec>` or a shim the table knows — and a word that is no
 * entry at all but has a profile is a box by name. A host application's entry
 * names no box and is skipped: the list is also the pinned set the shipped
 * desks are checked against, and those are host applications.
 */
static int favourite_box(const char *id, char *box, size_t bn)
{
	char path[MAX_LINE], pack[128] = "", cmd[512], shim[128] = "";
	const char *xdg = getenv("XDG_DATA_HOME");
	KxdgEntry e;

	box[0] = '\0';
	if (xdg && *xdg)
		snprintf(path, sizeof(path), "%s/applications/%s.desktop", xdg, id);
	else
		snprintf(path, sizeof(path),
			 "%s/.local/share/applications/%s.desktop",
			 kb_home_dir(), id);
	if (!kb_path_exists(path))
		snprintf(path, sizeof(path), "/usr/share/applications/%s.desktop",
			 id);
	if (kxdg_load(&e, path, "Desktop Entry") == 0) {
		/* kxdg_get answers `def` or a stored value, and every stored
		 * value is allocated — so neither is NULL. */
		const char *b = kxdg_get(&e, "X-KDOS-Box", "");
		const char *ex = kxdg_get(&e, "Exec", "");

		if (b && *b) {
			kb_strlcpy(box, b, bn);
		} else if (ex) {
			/* THE FIRST WORD, THEN ITS BASENAME, in that order:
			 * basenaming the whole line lands on the last slash of
			 * the boxed program's own path and never sees this
			 * binary. `-b` IS AN OPTION, SO IT PRECEDES THE VERB:
			 * past `run` the words are the boxed program's own argv,
			 * where a `-b` means whatever that program says. */
			const char *sl;

			snprintf(shim, sizeof(shim), "%.*s",
				 (int)strcspn(ex, " \t"), ex);
			sl = strrchr(shim, '/');
			if (sl)
				memmove(shim, sl + 1, strlen(sl + 1) + 1);
			if (!strcmp(shim, "kdos-appbox")) {
				const char *rn = strstr(ex, " run ");
				const char *r = strstr(ex, " -b ");

				if (r && (!rn || r < rn))
					snprintf(pack, sizeof(pack), "%.*s",
						 (int)strcspn(r + 4, " \t"), r + 4);
				else if (rn)
					app_pack_by_exec(rn + 5, pack, sizeof(pack));
			} else if (!app_lookup_pack(shim, cmd, sizeof(cmd), pack,
						    sizeof(pack))) {
				pack[0] = '\0';
			}
			kb_strlcpy(box, pack, bn);
		}
		kxdg_free(&e);
	} else {
		char *pp = profile_path(id);

		if (box_name_ok(id) && kb_path_exists(pp))
			kb_strlcpy(box, id, bn);
		free(pp);
	}
	return box[0] && box_name_ok(box);
}

/*
 * WARM THE FAVOURITES. One box per application means a single shared warmup
 * covers nothing: it would start a box no application runs in, while every
 * application's first launch of the session still pays compose + create +
 * boxinit + its own start, measured at 18 s cold. The warmup therefore has to
 * name boxes. `~/.config/kdos/favorites` is the list of boxes the user wants
 * running before the first click, so those are composed and started one at a
 * time, each with its keeper, so the first launch finds the bus proxy up too.
 *
 * Non-blocking lock, so a second warmup is a no-op and never a queue; `nice`
 * is the caller's (kdos-session-wrapper runs this at 10). A box that is
 * started and idle costs a conmon, a keeper and an xdg-dbus-proxy, and
 * `kdos-box gc` — run on a timer by the user's services — is what gives it
 * back.
 */
int cmd_warmup(void)
{
	char *lockpath, *fav;
	char buf[4096], *line, *save;
	char done[8][64];
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
		char box[64], why[256];
		Profile p;
		int seen = 0;

		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		/* The id is the first word: anything after it on the line is
		 * a note for a person. */
		line[strcspn(line, " \t")] = '\0';
		if (!favourite_box(line, box, sizeof(box))) {
			tracef("warmup: %s names no box: skipped", line);
			continue;
		}
		for (int i = 0; i < n && !seen; i++)
			seen = !strcmp(done[i], box);
		if (seen)
			continue;
		kb_strlcpy(done[n++], box, sizeof(done[0]));
		profile_load(&p, box);
		tracef("warmup %s -> %s", line, box);
		if (pack_box_ensure(box, !strncmp(p.base, "pack:", 5)
					 ? p.base + 5 : box) == 0 &&
		    pack_box_start(box) == 0 &&
		    keeper_ensure(box, why, sizeof(why)) != 0)
			tracef("warmup %s: %s", box, why);
	}
	close(fd);
	return 0;
}

int cmd_status(void)
{
	Profile p;
	char state[64], st[4096], sp[MAX_LINE];

	profile_load(&p, g_box);
	if (packd_ask("status", st, sizeof(st)) == 0)
		fputs(st, stdout);
	else
		printf("packd : not answering\n");
	box_state(g_box, state, sizeof(state));
	printf("box   : %s (%s)\n",
	       strcmp(state, "absent") ? "created"
				       : "not created (first launch will)", state);
	/* The boundary the last launch recorded, as the Boxes page reads it. */
	if (box_state_dir(sp, sizeof(sp)) == 0) {
		char path[MAX_LINE + 80], text[2048];

		snprintf(path, sizeof(path), "%s/%s.state", sp, g_box);
		if (kb_read_file(path, text, sizeof(text)) > 0) {
			for (char *l = text, *nl; l && *l; l = nl) {
				nl = strchr(l, '\n');
				if (nl)
					*nl++ = '\0';
				if (*l != '#')
					printf("state : %s\n", l);
			}
		}
	}
	return 0;
}

static void usage(void)
{
	fputs(
"Usage: kdos-appbox run <app> [args...]     run an app from its pack's box\n"
"       kdos-appbox open [--print|--choose] <path>\n"
"                                           open a file in whatever opens it\n"
"       kdos-appbox throwaway [--pack <id>] <file>\n"
"                                           the file in a fresh box with no\n"
"                                           network, gone when it closes\n"
"       kdos-appbox warmup | status\n"
"       kdos-appbox session save | restore  the boxes a logout left running\n"
"       kdos-appbox list                    boxes and their profiles\n"
"       kdos-appbox apps                    known alien apps\n"
"       kdos-appbox catalogue [--groups | --chain]  what this system can build\n"
"       kdos-appbox install <id|group>... [--dry-run]\n"
"       kdos-appbox uninstall <id|group>...\n"
"       kdos-appbox export <file.ktar> <id|group>...\n"
"       kdos-appbox import <file.ktar> [<id>...]\n"
"       kdos-appbox genlaunchers --packs <fs-root> | --packs --user\n"
"                               --packs-dir <dir> <fs-root>\n"
"                               <desktop-dir> <fs-root>\n"
"\n"
"  -b, --box <name>       run in <name> instead of the app's own pack box\n"
"  -l, --launcher <id>    the launcher id its windows take (<box>[.<stem>])\n"
"  -v, --verbose          let podman print to stderr\n"
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
	char cmd[512], store[1024], pack[128] = "", launcher[128] = "";
	const char *slot[KB_MAX_ARGV];
	char *vec[KB_MAX_ARGV];
	int n = 0, i;

	if (!app_lookup(name, cmd, sizeof(cmd), pack, sizeof(pack), launcher,
			sizeof(launcher))) {
		/*
		 * `<command>@<box>` IS A `kdos-box export` SHIM, which writes no
		 * alien-apps row: split at the LAST `@` and run what
		 * `kdos-box run <box> <command>` runs. The box part is checked
		 * as a box name here, because it becomes a podman name and a
		 * socket path; anything else falls through to the refusal.
		 */
		const char *at = strrchr(name, '@');
		const char *box = at ? at + 1 : NULL;

		if (!at || at == name || !box_name_ok(box) ||
		    (size_t)(at - name) >= sizeof(cmd) ||
		    argc + 1 >= KB_MAX_ARGV)
			kb_die("unknown alien app '%s' (try: kdos-appbox apps)",
			       name);
		memcpy(cmd, name, (size_t)(at - name));
		cmd[at - name] = '\0';
		g_box = kb_strdup(box);
		vec[n++] = cmd;
		for (i = 0; i < argc; i++)
			vec[n++] = argv[i];
		vec[n] = NULL;
		return cmd_run(n, vec);
	}

	/*
	 * ONE BOX PER APPLICATION in the pack lane, named after the pack. The
	 * launch path below is otherwise identical, and `--box` still wins —
	 * that is how the same application is run in a second box. The row's
	 * launcher id goes with the box it names, and nowhere else.
	 */
	if (pack[0] && !strcmp(g_box, DEFAULT_BOX)) {
		g_box = kb_strdup(pack);
		if (launcher[0] && !g_launcher)
			g_launcher = kb_strdup(launcher);
	}

	/*
	 * kxdg_exec_split, not a `strtok(" ")`: the table's commands come out
	 * of upstream .desktop files and carry the format's QUOTING, which a
	 * whitespace split gets wrong: `gsmartcontrol` is
	 * `"/usr/bin/gsmartcontrol-root"`, which a split execs with the quotes
	 * still on the path, and `wesnoth` is
	 * `sh -c "wesnoth-1.18 >/dev/null 2>&1"`, whose one shell argument a
	 * split hands to sh in three pieces. Both look exactly like an app that
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
	/*
	 * AND xdg-open IS A THIRD, because everything that opens a link says
	 * that word and means "whatever this machine opens it with": a mail
	 * client's :open-link, a portal, anything reading $BROWSER.
	 * /usr/local/bin comes first on the shipped $PATH, so this answers
	 * before xdg-utils' script — which stays installed and is still what
	 * cmd_open falls back to, by absolute path, when nothing here claims
	 * the type.
	 */
	if (!strcmp(self, "xdg-open"))
		return cmd_open(argc - 1, argv + 1);
	if (strcmp(self, "kdos-appbox"))
		return run_as_shim(self, argc - 1, argv + 1);

	while (i < argc && argv[i][0] == '-') {
		if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--box")) {
			if (i + 1 >= argc)
				kb_die("--box needs a name");
			g_box = argv[++i];
		} else if (!strcmp(argv[i], "-l") ||
			   !strcmp(argv[i], "--launcher")) {
			if (i + 1 >= argc)
				kb_die("--launcher needs an id");
			g_launcher = argv[++i];
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
	if (CMD("throwaway"))
		return cmd_throwaway(argc - i - 1, argv + i + 1);
	/* The keeper is started by a launch, detached, and is no command a
	 * person types; it still checks its box name like every other verb. */
	if (CMD("keeper")) {
		if (i + 2 != argc)
			kb_die("usage: kdos-appbox keeper <box>");
		return cmd_keeper(argv[i + 1]);
	}
	if (CMD("session"))
		return cmd_session(argc - i - 1, argv + i + 1);
	/* The catalogue, for the surfaces, and the parser's own assertions.
	 * Before anything that needs a daemon or a display: --selftest is
	 * offline and pure and must stay runnable where neither exists. */
	if (CMD("catalogue")) {
		if (i + 1 < argc && !strcmp(argv[i + 1], "--selftest"))
			return cat_selftest();
		return cmd_catalogue(argc - i - 1, argv + i + 1);
	}
	if (CMD("boxes") && i + 1 < argc && !strcmp(argv[i + 1], "--selftest"))
		return boxes_selftest();
	if (CMD("install") || CMD("uninstall")) {
		int dry = 0, first = i + 1, argn = 0;
		const char *ids[CAT_MAX_PACKS];

		for (int k = first; k < argc; k++) {
			if (!strcmp(argv[k], "--dry-run")) {
				dry = 1;
				continue;
			}
			if (argn < CAT_MAX_PACKS)
				ids[argn++] = argv[k];
		}
		if (!argn)
			kb_die("usage: kdos-appbox %s <id|group|deb:package>... "
			       "[--dry-run]", argv[i]);
		{
			char err[256];
			if (cat_load(NULL, err, sizeof(err)) != 0)
				kb_die("%s", err);
		}
		/* `deb:<package>` is Discover's Debian lane and names no
		 * catalogue row, so it is taken out before the catalogue
		 * expands the rest. */
		{
			int nrow = 0, failed = 0;
			for (int k = 0; k < argn; k++) {
				if (strncmp(ids[k], "deb:", 4)) {
					ids[nrow++] = ids[k];
					continue;
				}
				setvbuf(stdout, NULL, _IOLBF, 0);
				failed += CMD("install")
					  ? store_deb_install(ids[k], dry)
					  : store_deb_uninstall(ids[k]);
			}
			if (nrow)
				failed += CMD("install")
					  ? store_install_many(ids, nrow, dry)
					  : store_uninstall_many(ids, nrow);
			return failed;
		}
	}
	if (CMD("store") && i + 1 < argc && !strcmp(argv[i + 1], "--selftest"))
		return store_selftest();
	if (CMD("export") || CMD("import")) {
		const char *file = i + 1 < argc ? argv[i + 1] : NULL;
		const char *ids[CAT_MAX_PACKS];
		char err[256];
		int argn = 0;

		if (!file)
			kb_die("usage: kdos-appbox %s <file.ktar> [<id|group>...]",
			       argv[i]);
		for (int k = i + 2; k < argc && argn < CAT_MAX_PACKS; k++)
			ids[argn++] = argv[k];
		if (cat_load(NULL, err, sizeof(err)) != 0)
			kb_die("%s", err);
		/* EXPORT NEEDS A SELECTION and import does not: an archive
		 * carries its own SELECTION, so `import <file>` is the whole
		 * set and narrowing is the exception. */
		if (CMD("export") && !argn)
			kb_die("usage: kdos-appbox export <file.ktar> "
			       "<id|group>...");
		return CMD("export") ? store_export(file, ids, argn)
				     : store_import(file, ids, argn);
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
		 * this is what `kdos app install` runs as the calling user. It
		 * reads the store boxes as well as the packs, because the tree
		 * is written whole and either source alone would delete the
		 * other's launchers. */
		if (i + 2 < argc && !strcmp(argv[i + 1], "--packs") &&
		    !strcmp(argv[i + 2], "--user")) {
			if (i + 3 < argc)
				kb_die("usage: kdos-appbox genlaunchers --packs --user");
			store_launchers();
			return 0;
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
