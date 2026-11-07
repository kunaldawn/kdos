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
 * Generate everything the host needs to present a box's apps as its own.
 *
 *   kdos-appbox genlaunchers --packs <fs-root>
 *       every installed app pack, into the root tree under <fs-root>
 *   kdos-appbox genlaunchers --packs --user
 *       every installed app pack and every store box, into the user tree —
 *       what `kdos app install` regenerates
 *   kdos-appbox genlaunchers --packs-dir <dir> <fs-root>
 *       the build's form: one extracted pack per subdirectory of <dir>
 *   kdos-appbox genlaunchers <desktop-dir> <fs-root>
 *       one applications directory, with no pack field in the table
 *
 * Six outputs, and dropping any one of them breaks something visible:
 *
 *   etc/skel/.local/share/applications/<launcher-id>.desktop
 *       the launcher, named with the LAUNCHER ID — `<box>` for a box with one
 *       launcher, `<box>.<stem>` for each of a box with several — and never
 *       upstream's own desktop-file id. KWin replaces every tagged window's
 *       app_id with the launcher id kdos-boxsock tagged its socket with, and
 *       sets resourceClass and desktopFileName from it, so the Task Manager,
 *       KWin's rules and the window's icon all look for a desktop entry of
 *       exactly that name; under upstream's id every boxed window would show a
 *       generic icon beside a pin that never lights. It also keeps a boxed
 *       copy and a host copy of one program apart, which the Start menu and
 *       "Open with" need.
 *   etc/skel/.local/share/icons/hicolor/<size>/apps/<launcher-id>.<ext>
 *       the application's icon, copied out of its pack under the launcher's
 *       name: the pack's /usr/share/icons is inside the box and nothing on the
 *       host reads it, so an `Icon=` naming upstream's icon finds nothing.
 *   etc/skel/.local/share/applications/mimeinfo.cache
 *       the mime -> desktop-id index. Written here because this directory
 *       is not /usr/share/applications, whose cache kpkg's trigger writes,
 *       and no package install touches it; without the cache the MimeType
 *       lines are never consulted, so no alien app appears in any "Open
 *       with" dialog and none can be a default handler.
 *   usr/share/kdos/alien-apps
 *       name -> in-box command line, its pack and its launcher id, read by
 *       the launch path.
 *   usr/local/bin/<name> -> kdos-appbox
 *       one symlink per app, so every alien app is also a normal command in
 *       $PATH. kdos-appbox dispatches on its own basename, the way busybox
 *       does, which keeps the alien-app path free of any shell.
 *   etc/skel/.config/kwinrulesrc, groups kdos-box-<box>
 *       one window rule per box, the title bar in the box's colour (rules.c).
 *
 * The user's tree is followed by kbuildsycoca6, so KService — the Start menu,
 * KRunner, the Task Manager — sees the new launchers before this returns.
 */

#include "kdos-appbox.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kxdg.h"

/*
 * Shim names that must never be created in /usr/local/bin. /usr/local/bin is
 * ahead of /usr/bin on PATH, so a shim carrying a host tool's name shadows the
 * native program and dispatches to a box instead. git, gnuplot and mpv are
 * native ports that a pack's desktop entry could also name, which is why they
 * are here beside the shell and system tools; any other name a host program
 * already answers to in /usr/bin is refused the same way when the shims are
 * written (host_has()).
 */
static const char *RESERVED[] = {
	"sh", "bash", "env", "ls", "cp", "mv", "rm", "cat", "sed", "awk", "grep",
	"find", "tar", "gzip", "python3", "perl", "make", "gcc", "kdos",
	"git", "gnuplot", "mpv", "konsole",
	"kdos-appbox", "kdos-box", "xdg-open", "kdos-banner",
	"kdos-shot", "kdos-fetch-app", "kdos-fetch-static", "kdos-getty",
	"rec", "play", "sox", "soxi",
	"kinstall", "kpkg", "kpkgadd",
	"kpkgbuild", "kpkgdel", "kpkgdepends", "ksvc", "service", NULL
};

/*
 * ROOTLESS-INERT. These need raw block devices, and a box is rootless podman:
 * gparted cannot open /dev/sda, gsmartcontrol cannot issue an ATA passthrough
 * and Disks cannot mount anything. They ship in the image because Debian's
 * segment carries them, and a launcher that opens a window saying "no
 * permission" is worse than no launcher — it teaches somebody that the machine
 * is broken rather than that they wanted the host tool. The answer is the
 * native recovery set (testdisk, ddrescue, partclone, parted), which runs as
 * root because it is not in a box at all.
 */
static const char *SKIP_ROOTLESS_INERT[] = {
	"gparted", "gsmartcontrol", "org.gnome.DiskUtility", "org.gnome.Disks",
	"gnome-disks", "testdisk", "org.gnome.baobab-root", "timeshift-gtk", NULL
};

/*
 * THE SESSION'S OWN, AND UNREACHABLE FROM A BOX. Spectacle captures through
 * KWin's ScreenShot2 interface on the session bus, which a box's bus proxy
 * hides on purpose — that interface answers any caller with the whole screen
 * — so a boxed copy opens on an error. The host's Spectacle is installed and
 * is the screenshot tool.
 */
static const char *SKIP_HOST_ONLY[] = {
	"org.kde.spectacle", NULL
};

static const char *SKIP_BASENAMES[] = {
	"xfce4-about", "libfm-pref-apps", "lxshortcut", "pcmanfm-desktop-pref",
	"org.xfce.mousepad-settings", "libreoffice-xsltfilter",
	"org.gnome.gThumb.Import", "org.remmina.Remmina-file", "remmina-gnome",
	"assistant", "designer", "linguist",
	"bleachbit-root", "breezestyleconfig", "kcm_breezedecoration",
	"ktelnetservice6", "org.kde.kded6", "org.kde.kiod6", "org.kde.kwalletd6",
	"codium-url-handler", "gnome-disk-image-mounter", "gnome-disk-image-writer",
	"python3.13", "calibre-lrfviewer", "com.github.FontManager.FontViewer",
	"org.kicad.eeschema", "org.kicad.gerbview", "org.kicad.pcbnew", NULL
};

static const char *SKIP_PREFIXES[] = {
	"krita_", "carla", "org.kicad.bitmap2component",
	"org.kicad.pcbcalculator", "cups", "display-im", "mediainfo-gui", NULL
};

/* Short names for upstream ids: the shim on $PATH and the alien-apps table's
 * key, which `kdos app`, the shims and the docs all spell this way. */
static const char *RENAME[][2] = {
	{ "firefox-esr", "firefox" },
	{ "org.xfce.mousepad", "mousepad" },
	{ "org.gnome.SimpleScan", "simple-scan" },
	{ "org.pwmt.zathura", "zathura" },
	{ "transmission-gtk", "transmission" },
	{ "org.musicbrainz.Picard", "picard" },
	{ "com.github.xournalpp.xournalpp", "xournalpp" },
	{ "com.github.maoschanz.drawing", "drawing" },
	{ "com.github.johnfactotum.Foliate", "foliate" },
	{ "com.obsproject.Studio", "obs" },
	{ "org.gnome.gThumb", "gthumb" },
	{ "org.gnome.Meld", "meld" },
	{ "im.dino.Dino", "dino" },
	{ "io.github.Hexchat", "hexchat" },
	{ "org.remmina.Remmina", "remmina" },
	{ "org.wireshark.Wireshark", "wireshark" },
	{ "org.inkscape.Inkscape", "inkscape" },
	{ "org.kde.krita", "krita" },
	{ "org.darktable.darktable", "darktable" },
	{ "org.freecad.FreeCAD", "freecad" },
	{ "org.keepassxc.KeePassXC", "keepassxc" },
	{ "org.kicad.kicad", "kicad" },
	{ "org.octave.Octave", "octave" },
	{ "org.shotcut.Shotcut", "shotcut" },
	{ "org.stellarium.Stellarium", "stellarium" },
	{ "org.musescore.MuseScore", "musescore" },
	{ "org.zim_wiki.Zim", "zim" },
	{ "fr.handbrake.ghb", "handbrake" },
	{ "org.gnome.baobab", "baobab" },
	{ "org.gnome.DiskUtility", "disks" },
	{ "org.gnome.DejaDup", "dejadup" },
	{ "libreoffice-startcenter", "libreoffice" },
	{ "sol", "aisleriot" },
	{ "supertux2", "supertux" },
	{ "net.minetest.minetest", "luanti" },
	{ "com.libretro.RetroArch", "retroarch" },
	{ "io.mgba.mGBA", "mgba" },
	{ "org.wesnoth.Wesnoth-1.18", "wesnoth" },
	{ "io.github.wxmaxima_developers.wxMaxima", "wxmaxima" },
	{ "io.github.xiaoyifang.goldendict_ng", "goldendict" },
	{ "com.github.wwmm.easyeffects", "easyeffects" },
	{ "org.hydrogenmusic.Hydrogen", "hydrogen" },
	{ "com.github.jeromerobert.pdfarranger", "pdfarranger" },
	{ "com.github.FontManager.FontManager", "font-manager" },
	{ "org.gnome.GHex", "ghex" },
	{ "org.zealdocs.zeal", "zeal" },
	{ "org.bleachbit.BleachBit", "bleachbit" },
	{ "org.fontforge.FontForge", "fontforge" },
	{ "org.kde.kdenlive", "kdenlive" },
	{ "org.scummvm.scummvm", "scummvm" },
	{ "org.gnome.Chess", "gnome-chess" },
	{ "org.gnome.Mines", "gnome-mines" },
	{ "org.gnome.Sudoku", "gnome-sudoku" },
	{ "org.gnome.Quadrapassel", "quadrapassel" },
	{ "calibre-gui", "calibre" },
	{ "calibre-ebook-edit", "calibre-editor" },
	{ "calibre-ebook-viewer", "calibre-viewer" },
	{ "PrusaSlicer", "prusa-slicer" },
	{ "PrusaGcodeviewer", "prusa-gcodeviewer" },
	{ "codium", "vscodium" },
	{ NULL, NULL }
};

/*
 * Extra argv an app needs to work inside the container / on this compositor,
 * appended after the upstream Exec. All three VSCodium flags were established
 * by launching it in a booted VM and reading why it died:
 *   --no-sandbox                chrome-sandbox wants a setuid helper and
 *                               CLONE_NEWUSER, gets neither as a non-root user
 *                               in an unprivileged podman container, and exits
 *                               rather than falling back
 *   --ozone-platform-hint=auto  otherwise Electron does not pick Wayland
 *   --disable-gpu-compositing   without it the renderer dies with "create_immed
 *                               failed and produced an invalid wl_buffer" ->
 *                               "launch-failed, code 1002". --disable-gpu also
 *                               fixes it but turns off GPU rasterisation too;
 *                               this is the smaller hammer.
 */
static const char *EXEC_EXTRA[][2] = {
	{ "vscodium",
	  "--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing" },
	{ NULL, NULL }
};

/*
 * Environment assignments in an upstream Exec that force X11. KDOS is
 * Wayland-only, so these are a guaranteed silent failure: debian ships
 * audacity as `env GDK_BACKEND=x11 audacity`, and with no X server GTK exits
 * before printing anything at all. Measured: audacity runs fine on Wayland
 * once the prefix is dropped.
 */
/*
 * Alien software whose interface is a COMMAND and not a launcher.
 *
 * wine is the case that forced this: what a person wants from it is
 * `wine setup.exe` at a prompt, and Debian's own entries for it are
 * NoDisplay=true, which parse_dir correctly drops — so without this table the
 * box would contain wine and the host would have no way to reach it. These get
 * an alien-apps row and a /usr/local/bin shim, and deliberately NOT a .desktop:
 * a launcher for `wine` with no arguments opens nothing.
 *
 * Only emitted when the image actually carries the binary: a shim written for
 * a binary an image does not have is a command that dies on "not found".
 */
static const char *COMMANDS[] = {
	"wine", "winecfg", "winetricks", NULL
};

/*
 * Alien software that IS an application and ships no desktop entry at all, so
 * `parse_dir` has nothing to read and the box would hold a program the host
 * cannot reach from a menu.
 *
 * surf is the case that forced it: Debian's package is the binary, a web
 * extension and a man page, because upstream ships no entry and a browser with
 * no launcher claims no scheme — which on a machine whose only other answer is
 * a text browser means installing it changes nothing about what opens a link.
 *
 * WRITTEN HERE AND NOT SHIPPED IN THE PACK. An entry in the image would be an
 * entry apt could replace, and the `Exec` has to name `kdos-appbox run`, which
 * only this side knows how to spell.
 *
 * Only emitted where the image actually carries the binary, the same rule
 * COMMANDS keeps.
 */
static const struct {
	const char *bin;
	const char *name;
	const char *generic;
	const char *cats;
	const char *mime;
	const char *keywords;
	const char *icon;
	const char *exec;
} ENTRIES[] = {
	{ "surf", "Surf", "Web Browser", "Network;WebBrowser;",
	  "text/html;application/xhtml+xml;x-scheme-handler/http;"
	  "x-scheme-handler/https;",
	  "web;browser;http;suckless;surf;", "gtk-network", "surf %u" },
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const char *X11_FORCING[] = {
	"GDK_BACKEND=x11", "CLUTTER_BACKEND=x11", "QT_QPA_PLATFORM=xcb",
	"SDL_VIDEODRIVER=x11", "MOZ_ENABLE_WAYLAND=0",
	"ELECTRON_OZONE_PLATFORM_HINT=x11", NULL
};

int env_forces_x11(const char *assignment)
{
	for (int i = 0; X11_FORCING[i]; i++)
		if (!strcmp(X11_FORCING[i], assignment))
			return 1;
	return 0;
}

static int in_list(const char *const *list, const char *s)
{
	for (int i = 0; list[i]; i++)
		if (!strcmp(list[i], s))
			return 1;
	return 0;
}

static const char *lookup(const char *table[][2], const char *key)
{
	for (int i = 0; table[i][0]; i++)
		if (!strcmp(table[i][0], key))
			return table[i][1];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char id[96];
	char base[128];
	char name[160];
	char exec[1024];
	char icon[160];
	char cats[512];
	char mime[2048];
	char keywords[512];
	char generic[160];
	char pack[96];		/* the pack that provides it, or "" for an image */
	char lid[128];		/* its launcher id, "" until assign_ids()       */
	char root[1024];	/* the pack's root, where its icons are         */
	int cmdonly;		/* a command, not an application: no launcher */
	int terminal;		/* upstream's Terminal=true: it draws in one */
} Launcher;

/*
 * THE TABLE GROWS, because a fixed one silently loses applications. The pack
 * lane parses each pack's OWN desktop entries, so the count is entries, not
 * packs (LibreOffice alone carries eight), and a fixed ceiling drops the tail:
 * no error, an exit status of 0, and a Start menu missing whatever sorted
 * last. It is on the heap so the dispatcher, which is
 * this same binary and runs on every launch, carries no fixed cost for a
 * table only `genlaunchers` fills.
 */
static Launcher *apps;
static int napps, appcap;

static Launcher *app_new(void)
{
	if (napps == appcap) {
		int cap = appcap ? appcap * 2 : 128;
		Launcher *grown = kb_calloc((size_t)cap, sizeof(*grown));
		if (apps) {
			memcpy(grown, apps, (size_t)napps * sizeof(*grown));
			free(apps);
		}
		apps = grown;
		appcap = cap;
	}
	memset(&apps[napps], 0, sizeof(apps[napps]));
	return &apps[napps++];
}

/* Stamped onto every launcher parsed while it is set. The pack lane walks one
 * mounted pack at a time, and this is what ties a shim to the box it should
 * launch in — the third field of the alien-apps table — and an icon to the
 * root it is copied out of. */
static char cur_pack[96];

/*
 * The image root, from the desktop directory it was pointed at: the caller
 * passes <image>/usr/share/applications, so three components up is the root —
 * "" for the host's own /usr/share/applications. -1, and "", when the path is
 * too short to have one.
 */
static int root_of(const char *srcdir, char *root, size_t n)
{
	kb_strlcpy(root, srcdir, n);
	for (int up = 0; up < 3; up++) {
		char *slash = strrchr(root, '/');
		if (!slash) {
			root[0] = 0;
			return -1;
		}
		*slash = 0;
	}
	return 0;
}

/* A field code is a `%` and one letter and nothing else. The four that name
 * the file or URL the user picked are kept; the rest (%i, %c, %k …) are
 * dropped, because nothing fills them in here. */
static int drop_field_code(const char *w)
{
	if (w[0] != '%' || !isalpha((unsigned char)w[1]) || w[2])
		return 0;
	return !(w[1] == 'U' || w[1] == 'F' || w[1] == 'f' || w[1] == 'u');
}

/*
 * QUOTE-AWARE, and it has to be. An Exec value is not a whitespace-separated
 * list: debian ships `Exec="/usr/bin/gsmartcontrol-root"` and
 * `Exec=sh -c "wesnoth-1.18 >/dev/null 2>&1"`, and a `strtok(" ")` turns the
 * first into a path that begins with a quote and the second into five words.
 * Both were in the shipped table and both looked, from the launcher, exactly
 * like an application that does not start.
 *
 * The split keeps field codes verbatim (`nfiles < 0`) because this REWRITES an
 * Exec line rather than running one — the placeholders have to survive into
 * the file — and the join re-quotes anything that would not read back as one
 * argument.
 */
static void clean_exec(const char *in, char *out, size_t cap)
{
	const char *words[128];
	char store[2048];
	int nw = kxdg_exec_split(in, NULL, -1, store, sizeof(store), words,
				 128);
	int keep_n = 0;

	for (int i = 0; i < nw; i++)
		if (!drop_field_code(words[i]))
			words[keep_n++] = words[i];
	nw = keep_n;

	if (nw && !strcmp(words[0], "env")) {
		const char *keep[128];
		int nk = 0;
		keep[nk++] = words[0];
		int i = 1;
		while (i < nw && strchr(words[i], '=')) {
			if (!in_list(X11_FORCING, words[i]))
				keep[nk++] = words[i];
			i++;
		}
		if (nk == 1)		/* nothing left to set */
			nk = 0;
		for (int j = i; j < nw && nk < 128; j++)
			keep[nk++] = words[j];
		memcpy(words, keep, (size_t)nk * sizeof(*keep));
		nw = nk;
	}

	out[0] = 0;
	size_t used = 0;
	for (int i = 0; i < nw; i++) {
		char q[1024];
		if (kxdg_exec_quote(words[i], q, sizeof(q)) != 0)
			continue;
		size_t n = strlen(q);
		if (used + n + 2 >= cap)
			break;
		if (used)
			out[used++] = ' ';
		memcpy(out + used, q, n);
		used += n;
		out[used] = 0;
	}
}

static int ends_with(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);
	return n >= m && !strcmp(s + n - m, suffix);
}

static int starts_with_any(const char *const *list, const char *s)
{
	for (int i = 0; list[i]; i++)
		if (!strncmp(s, list[i], strlen(list[i])))
			return 1;
	return 0;
}

/* `Categories=` is `;`-separated tokens; a token is matched whole —
 * `X-GNOME-NetworkSettings` contains the word Settings and is not the category,
 * and a substring test would drop Remmina's launcher for it. */
static int has_category(const char *cats, const char *want)
{
	size_t wl = strlen(want);
	for (const char *p = cats; p && *p; ) {
		const char *e = strchr(p, ';');
		size_t l = e ? (size_t)(e - p) : strlen(p);
		if (l == wl && !strncmp(p, want, wl))
			return 1;
		if (!e)
			break;
		p = e + 1;
	}
	return 0;
}

static void parse_dir(const char *srcdir)
{
	char **files = kb_listdir(srcdir, NULL);
	char root[1024];

	if (!files)
		kb_die("cannot read %s", srcdir);
	root_of(srcdir, root, sizeof(root));

	for (char **f = files; *f; f++) {
		if (!ends_with(*f, ".desktop"))
			continue;

		char base[128];
		size_t blen = strlen(*f) - 8;
		if (blen >= sizeof(base))
			continue;
		memcpy(base, *f, blen);
		base[blen] = 0;

		if (in_list(SKIP_BASENAMES, base) ||
		    in_list(SKIP_ROOTLESS_INERT, base) ||
		    in_list(SKIP_HOST_ONLY, base) ||
		    starts_with_any(SKIP_PREFIXES, base))
			continue;

		char *path = kb_path_join(srcdir, *f);
		KxdgEntry e;
		int ok = kxdg_load(&e, path, "Desktop Entry") == 0;
		free(path);
		if (!ok)
			continue;

		const char *type = kxdg_get(&e, "Type", "Application");
		const char *name = kxdg_get(&e, "Name", NULL);
		const char *ex = kxdg_get(&e, "Exec", "");
		const char *cats = kxdg_get(&e, "Categories", "");

		if (kxdg_bool(&e, "NoDisplay", 0) || strcmp(type, "Application") ||
		    !name || !*ex ||
		    /* A Settings-only entry belongs to the box's own desktop,
		     * not to ours; one that is also System is a real tool. */
		    (has_category(cats, "Settings") && !has_category(cats, "System"))) {
			kxdg_free(&e);
			continue;
		}

		Launcher *a = app_new();

		kb_strlcpy(a->pack, cur_pack, sizeof(a->pack));
		kb_strlcpy(a->base, base, sizeof(a->base));
		/*
		 * THE SHIM IS NAMED AFTER THE PROGRAM, not after the desktop
		 * id. `org.kde.rkward.desktop` runs `rkward`, and `rkward` is
		 * what a person types and what Debian put in /usr/bin — a shim
		 * called `org.kde.rkward` is one nobody finds, and the whole
		 * KDE segment is reverse-DNS. RENAME still wins where upstream's
		 * program name is not the one people know (firefox-esr →
		 * firefox); a program name that is reserved, empty or odd falls
		 * back to the lowercased id, which is unique by construction.
		 */
		const char *ren = lookup(RENAME, base);
		char key[128];
		app_exec_key(ex, key, sizeof(key));
		if (ren) {
			kb_strlcpy(a->id, ren, sizeof(a->id));
		} else if (key[0] && !in_list(RESERVED, key) &&
			   strspn(key, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				       "abcdefghijklmnopqrstuvwxyz0123456789._+-")
			   == strlen(key)) {
			kb_strlcpy(a->id, key, sizeof(a->id));
		} else {
			kb_strlcpy(a->id, base, sizeof(a->id));
			for (char *p = a->id; *p; p++)
				*p = (char)tolower((unsigned char)*p);
		}

		kb_strlcpy(a->name, name, sizeof(a->name));
		kb_strlcpy(a->cats, cats, sizeof(a->cats));
		kb_strlcpy(a->icon, kxdg_get(&e, "Icon", ""), sizeof(a->icon));
		kb_strlcpy(a->mime, kxdg_get(&e, "MimeType", ""), sizeof(a->mime));
		kb_strlcpy(a->keywords, kxdg_get(&e, "Keywords", ""),
			   sizeof(a->keywords));
		kb_strlcpy(a->generic, kxdg_get(&e, "GenericName", ""),
			   sizeof(a->generic));
		kb_strlcpy(a->root, root, sizeof(a->root));
		/* R, octave-cli and the like say `Terminal=true`, and the Start
		 * menu runs such an entry in Konsole. Written as false, the
		 * program was started with a pipe for stdin, and R answers that
		 * with "you must specify --save" and exits. */
		a->terminal = kxdg_bool(&e, "Terminal", 0);

		char cleaned[1024];
		clean_exec(ex, cleaned, sizeof(cleaned));
		const char *extra = lookup(EXEC_EXTRA, a->id);
		if (extra) {
			char *pct = strchr(cleaned, '%');
			if (!pct) {
				snprintf(a->exec, sizeof(a->exec), "%s %s",
					 cleaned, extra);
			} else {
				/* The extra argv has to land BEFORE the field
				 * code, or it is read as another filename. */
				size_t head = (size_t)(pct - cleaned);
				snprintf(a->exec, sizeof(a->exec), "%.*s%s %s",
					 (int)head, cleaned, extra, pct);
			}
		} else {
			kb_strlcpy(a->exec, cleaned, sizeof(a->exec));
		}

		kxdg_free(&e);
	}
	kb_strv_free(files);
}

static void add_commands(const char *srcdir)
{
	char root[1024];

	if (root_of(srcdir, root, sizeof(root)) != 0)
		return;

	for (int i = 0; COMMANDS[i]; i++) {
		char probe[1200];
		snprintf(probe, sizeof(probe), "%s/usr/bin/%s", root, COMMANDS[i]);
		if (!kb_path_exists(probe))
			continue;
		Launcher *a = app_new();
		a->cmdonly = 1;
		kb_strlcpy(a->pack, cur_pack, sizeof(a->pack));
		kb_strlcpy(a->id, COMMANDS[i], sizeof(a->id));
		kb_strlcpy(a->base, COMMANDS[i], sizeof(a->base));
		kb_strlcpy(a->name, COMMANDS[i], sizeof(a->name));
		kb_strlcpy(a->exec, COMMANDS[i], sizeof(a->exec));
	}

	for (int i = 0; ENTRIES[i].bin; i++) {
		char probe[1200];

		snprintf(probe, sizeof(probe), "%s/usr/bin/%s", root,
			 ENTRIES[i].bin);
		if (!kb_path_exists(probe))
			continue;
		Launcher *a = app_new();

		kb_strlcpy(a->pack, cur_pack, sizeof(a->pack));
		kb_strlcpy(a->id, ENTRIES[i].bin, sizeof(a->id));
		kb_strlcpy(a->base, ENTRIES[i].bin, sizeof(a->base));
		kb_strlcpy(a->name, ENTRIES[i].name, sizeof(a->name));
		kb_strlcpy(a->generic, ENTRIES[i].generic, sizeof(a->generic));
		kb_strlcpy(a->cats, ENTRIES[i].cats, sizeof(a->cats));
		kb_strlcpy(a->mime, ENTRIES[i].mime, sizeof(a->mime));
		kb_strlcpy(a->keywords, ENTRIES[i].keywords,
			   sizeof(a->keywords));
		kb_strlcpy(a->exec, ENTRIES[i].exec, sizeof(a->exec));
		kb_strlcpy(a->icon, ENTRIES[i].icon, sizeof(a->icon));
		kb_strlcpy(a->root, root, sizeof(a->root));
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * THE LAUNCHER ID. A box with one launcher is its own launcher; a box whose
 * pack exports several (app.maxima carries maxima and wxmaxima) gives each
 * `<box>.<stem>`, the stem being the shim name with every character an app id
 * component cannot carry turned into `_`. The stem carries no dot, which is
 * what lets a reader find the box from the id: the id itself, or the id less
 * its last component.
 */
void launcher_id(const char *box, const char *stem, int several, char *out,
		 size_t n)
{
	size_t at;

	if (!several || !stem || !*stem) {
		kb_strlcpy(out, box, n);
		return;
	}
	at = (size_t)snprintf(out, n, "%s.", box);
	for (const char *c = stem; *c && at + 1 < n; c++)
		out[at++] = isalnum((unsigned char)*c) || *c == '_' || *c == '-'
			  ? *c : '_';
	if (at < n)
		out[at] = '\0';
	/* A stem that cannot start a component is given one that can. */
	if (out[strlen(box) + 1] == '-')
		out[strlen(box) + 1] = '_';
}

/* Every application of a pack gets its launcher id, once the whole set is
 * read, because whether a pack has several is a fact about the set. Two
 * shims that clean to the same stem keep the first one's id and the second
 * gets its upstream id's stem, so no two launchers share a file name. */
static void assign_ids(void)
{
	for (int i = 0; i < napps; i++) {
		Launcher *a = &apps[i];
		int several = 0;

		if (a->cmdonly || !a->pack[0])
			continue;
		for (int k = 0; k < napps && !several; k++)
			several = k != i && !apps[k].cmdonly &&
				  !strcmp(apps[k].pack, a->pack);
		launcher_id(a->pack, a->id, several, a->lid, sizeof(a->lid));
		for (int k = 0; k < i; k++)
			if (!strcmp(apps[k].lid, a->lid)) {
				launcher_id(a->pack, a->base, 1, a->lid,
					    sizeof(a->lid));
				break;
			}
	}
}

/*
 * THE ICON, COPIED OUT OF THE PACK UNDER THE LAUNCHER'S NAME. An `Icon=` is a
 * theme name looked up in the HOST's icon themes, and the application's own
 * icon is inside its pack, which nothing on the host reads; a boxed launcher
 * naming upstream's icon shows a generic one. Every size and format the pack
 * has in hicolor is copied, then /usr/share/pixmaps as the unthemed fallback
 * KIconLoader also searches, and an absolute `Icon=` path inside the pack is
 * copied as it is.
 *
 * The files written are listed in <data>/kdos/launcher-icons, and that list is
 * what the next run deletes before it writes: the icon directories are shared
 * with every other program that installs one, so nothing is swept by pattern.
 */
static const char *const ICON_EXTS[] = { "svg", "svgz", "png", "xpm", NULL };

static int copy_icon(const char *src, const char *dst, KbBuf *manifest,
		     const char *data)
{
	char dir[1024];
	char *slash;

	kb_strlcpy(dir, dst, sizeof(dir));
	slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}
	if (kb_copy_file(src, dst) != 0)
		return 0;
	kb_buf_printf(manifest, "%s\n", dst + strlen(data) + 1);
	return 1;
}

static int export_icon(const Launcher *a, const char *data, KbBuf *manifest)
{
	char path[2048], dst[4096];
	char **sizes;
	int n = 0;

	if (!a->icon[0] || !a->root[0] || !a->lid[0])
		return 0;
	if (a->icon[0] == '/') {
		const char *dot = strrchr(a->icon, '.');

		snprintf(path, sizeof(path), "%s%s", a->root, a->icon);
		if (!dot || !kb_path_exists(path))
			return 0;
		snprintf(dst, sizeof(dst), "%s/pixmaps/%s%s", data, a->lid, dot);
		return copy_icon(path, dst, manifest, data);
	}
	if (strchr(a->icon, '/'))
		return 0;
	snprintf(path, sizeof(path), "%s/usr/share/icons/hicolor", a->root);
	sizes = kb_listdir(path, NULL);
	for (char **sz = sizes; sz && *sz; sz++) {
		for (int e = 0; ICON_EXTS[e]; e++) {
			char src[4096];

			snprintf(src, sizeof(src), "%s/%s/apps/%s.%s", path, *sz,
				 a->icon, ICON_EXTS[e]);
			if (!kb_path_exists(src))
				continue;
			snprintf(dst, sizeof(dst), "%s/icons/hicolor/%s/apps/%s.%s",
				 data, *sz, a->lid, ICON_EXTS[e]);
			n += copy_icon(src, dst, manifest, data);
		}
	}
	kb_strv_free(sizes);
	if (n)
		return n;
	for (int e = 0; ICON_EXTS[e]; e++) {
		char src[4096];

		snprintf(src, sizeof(src), "%s/usr/share/pixmaps/%s.%s", a->root,
			 a->icon, ICON_EXTS[e]);
		if (!kb_path_exists(src))
			continue;
		snprintf(dst, sizeof(dst), "%s/pixmaps/%s.%s", data, a->lid,
			 ICON_EXTS[e]);
		return copy_icon(src, dst, manifest, data);
	}
	return 0;
}

/* The previous run's icons go, by the list it left. */
static void sweep_icons(const char *data)
{
	char list[2048];
	size_t len = 0;
	char *text;

	snprintf(list, sizeof(list), "%s/kdos/launcher-icons", data);
	text = kb_read_all(list, &len);
	for (char *l = text, *nl; l && *l; l = nl) {
		char p[2048];

		nl = strchr(l, '\n');
		if (nl)
			*nl++ = '\0';
		/* A line naming a parent directory is no icon this wrote. */
		if (!*l || strstr(l, "..") || *l == '/')
			continue;
		snprintf(p, sizeof(p), "%s/%s", data, l);
		unlink(p);
	}
	free(text);
	unlink(list);
}

static void write_launchers(const char *dir, const char *data)
{
	KbBuf manifest = {0};

	kb_mkdir_p(dir);

	/* Cleared by MARKER, not by name: a launcher's file name follows its
	 * box and its stem, either of which a pack version can change, and an
	 * orphan launcher is a dead icon in the Start menu. */
	char **old = kb_listdir(dir, NULL);
	for (char **f = old; f && *f; f++) {
		if (!ends_with(*f, ".desktop"))
			continue;
		char *p = kb_path_join(dir, *f);
		size_t n = 0;
		char *d = kb_read_all(p, &n);
		if (d && strstr(d, "X-KDOS-Alien=true"))
			unlink(p);
		free(d);
		free(p);
	}
	kb_strv_free(old);
	sweep_icons(data);

	for (int i = 0; i < napps; i++) {
		Launcher *a = &apps[i];
		if (a->cmdonly)
			continue;
		KbBuf b = {0};
		char lead[256] = "", icon[160];
		const char *leafid = a->lid[0] ? a->lid : a->base;

		/*
		 * THE LAUNCHER CARRIES THE BOX AND ITS OWN ID. `-b <pack>` is
		 * the pack id, and the pack id is what a box profile is filed
		 * under (~/.config/kdos/boxes/<pack>.conf) — so `run` skips the
		 * exec→pack table entirely and a reader takes a guest's policy
		 * key straight off the argv instead of reversing the box layout
		 * out of an absolute Exec. `-l` is the launcher id its windows
		 * take, which is this file's own name. A row with no pack keeps
		 * the bare verb, and both sides fall back to the program's own
		 * name.
		 */
		if (a->pack[0])
			snprintf(lead, sizeof(lead), "-b %s -l %s ", a->pack,
				 a->lid);
		kb_strlcpy(icon, a->icon, sizeof(icon));
		if (export_icon(a, data, &manifest))
			kb_strlcpy(icon, a->lid, sizeof(icon));
		kb_buf_printf(&b,
			      "[Desktop Entry]\n"
			      "Type=Application\n"
			      "Name=%s\n"
			      "Comment=%s%s%s\n"
			      "Exec=kdos-appbox %srun %s\n"
			      "Icon=%s\n"
			      "Terminal=%s\n"
			      "Categories=%s\n",
			      a->name, a->generic[0] ? a->generic : a->name,
			      a->pack[0] ? " — in the box " : " — boxed",
			      a->pack,
			      lead, a->exec, icon,
			      a->terminal ? "true" : "false", a->cats);
		if (a->generic[0])
			kb_buf_printf(&b, "GenericName=%s\n", a->generic);
		if (a->mime[0])
			kb_buf_printf(&b, "MimeType=%s\n", a->mime);
		/* "box" is a word a person can search the Start menu by, and
		 * it is how the boxed copy of a program the host also carries
		 * is told from the native one in KRunner. */
		kb_buf_printf(&b, "Keywords=%s%sbox;\n", a->keywords,
			      a->keywords[0] && !ends_with(a->keywords, ";")
			      ? ";" : "");
		kb_buf_printf(&b, "StartupWMClass=%s\n", leafid);
		if (a->pack[0])
			kb_buf_printf(&b, "X-KDOS-Box=%s\nX-KDOS-Upstream=%s\n",
				      a->pack, a->base);
		kb_buf_str(&b, "X-KDOS-Alien=true\n");

		char leaf[160];
		snprintf(leaf, sizeof(leaf), "%s.desktop", leafid);
		char *p = kb_path_join(dir, leaf);
		kb_write_all(p, b.p, b.n);
		free(p);
		kb_buf_free(&b);
	}
	if (manifest.n) {
		char list[2048];

		snprintf(list, sizeof(list), "%s/kdos", data);
		kb_mkdir_p(list);
		snprintf(list, sizeof(list), "%s/kdos/launcher-icons", data);
		kb_write_all(list, manifest.p, manifest.n);
	}
	kb_buf_free(&manifest);
}

/* mime -> the desktop ids that handle it, both sorted. */
typedef struct {
	char mime[192];
	char ids[1024];
} MimeRow;

static int cmp_mime(const void *a, const void *b)
{
	return strcmp(((const MimeRow *)a)->mime, ((const MimeRow *)b)->mime);
}

static int write_mimeinfo(const char *dir)
{
	static MimeRow rows[4096];
	int nrows = 0;

	for (int i = 0; i < napps; i++) {
		if (apps[i].cmdonly)
			continue;	/* no MimeType, nothing to cache */
		char *copy = kb_strdup(apps[i].mime), *save = NULL;
		for (char *m = strtok_r(copy, ";", &save); m;
		     m = strtok_r(NULL, ";", &save)) {
			while (*m == ' ' || *m == '\t')
				m++;
			size_t n = strlen(m);
			while (n && (m[n - 1] == ' ' || m[n - 1] == '\t'))
				m[--n] = 0;
			if (!n)
				continue;

			int at = -1;
			for (int r = 0; r < nrows; r++)
				if (!strcmp(rows[r].mime, m)) {
					at = r;
					break;
				}
			if (at < 0) {
				if (nrows >= (int)(sizeof(rows) / sizeof(rows[0])))
					continue;
				at = nrows++;
				kb_strlcpy(rows[at].mime, m, sizeof(rows[0].mime));
				rows[at].ids[0] = 0;
			}
			size_t used = strlen(rows[at].ids);
			snprintf(rows[at].ids + used, sizeof(rows[0].ids) - used,
				 "%s%s.desktop", used ? ";" : "",
				 apps[i].lid[0] ? apps[i].lid : apps[i].base);
		}
		free(copy);
	}

	qsort(rows, (size_t)nrows, sizeof(rows[0]), cmp_mime);

	KbBuf b = {0};
	kb_buf_str(&b, "[MIME Cache]\n");
	for (int r = 0; r < nrows; r++)
		kb_buf_printf(&b, "%s=%s;\n", rows[r].mime, rows[r].ids);

	char *p = kb_path_join(dir, "mimeinfo.cache");
	kb_write_all(p, b.p, b.n);
	free(p);
	kb_buf_free(&b);
	return nrows;
}

/*
 * A NAME THE HOST ALREADY ANSWERS TO. Every Gear application is native here
 * and a pack may carry the same one, so a shim named after it would sit ahead
 * of /usr/bin on PATH and send the native program's name into a box. The host
 * program keeps its name; the boxed copy is still its launcher, and still
 * `<name>@<box>` through `kdos-box export`.
 */
static int host_has(const char *fsroot, const char *name)
{
	char p[1024];

	snprintf(p, sizeof(p), "%s/usr/bin/%s", fsroot && strcmp(fsroot, "/")
		 ? fsroot : "", name);
	return kb_path_exists(p);
}

static int write_shims(const char *share, const char *bindir,
		       const char *fsroot)
{
	kb_mkdir_p(share);

	KbBuf t = {0};
	/* The third field is the pack and the fourth the launcher id, and both
	 * are OPTIONAL: a row with no pack is two fields and a command with no
	 * launcher of its own three, and every reader splits at the tabs it
	 * needs rather than counting them. */
	kb_buf_str(&t, "# name\tcommand[\tpack[\tlauncher]] — GENERATED by "
		       "kdos-appbox genlaunchers\n");
	for (int i = 0; i < napps; i++) {
		if (apps[i].pack[0] && apps[i].lid[0])
			kb_buf_printf(&t, "%s\t%s\t%s\t%s\n", apps[i].id,
				      apps[i].exec, apps[i].pack, apps[i].lid);
		else if (apps[i].pack[0])
			kb_buf_printf(&t, "%s\t%s\t%s\n", apps[i].id,
				      apps[i].exec, apps[i].pack);
		else
			kb_buf_printf(&t, "%s\t%s\n", apps[i].id, apps[i].exec);
	}
	char *table = kb_path_join(share, "alien-apps");
	/* A WRITE THAT FAILED MUST NOT REPORT A LAUNCHER SET. Every output
	 * here is under /usr, so a run as anyone but root writes nothing at
	 * all — and printing "11 launchers, 188 mime types, 0 shims" and
	 * exiting 0 reads as the pack lane having taken over when the shipped
	 * table is still whoever wrote it last. */
	if (kb_write_all(table, t.p, t.n) != 0)
		kb_die("cannot write %s: %s", table, strerror(errno));
	free(table);
	kb_buf_free(&t);

	kb_mkdir_p(bindir);

	/*
	 * Every shim goes and is written again, whatever it points at: a shim
	 * naming a dispatcher under any other name is a dead command, and the
	 * name is not guaranteed for ever. A RELATIVE link target is the
	 * marker for one this program wrote.
	 *
	 * RESERVED IS CONSULTED HERE AS WELL AS AT CREATE TIME, and that is
	 * what keeps `kdos-box` alive: it is the box manager's name on this
	 * same binary, installed by the recipe as a relative symlink, so a
	 * sweep going by the marker alone deletes the front door to every box
	 * on the machine — leaving `kdos-box: command not found` on a system
	 * where nothing is missing but a link.
	 */
	char **old = kb_listdir(bindir, NULL);
	for (char **f = old; f && *f; f++) {
		char *p = kb_path_join(bindir, *f);
		char target[256];
		ssize_t n = (!in_list(RESERVED, *f) && kb_is_link(p))
			  ? readlink(p, target, sizeof(target) - 1) : -1;
		if (n > 0) {
			target[n] = 0;
			/* ours: a relative `kdos-appbox` (the root tree) or the
			 * absolute dispatcher path (the user tree) */
			if (target[0] != '/' ||
			    !strcmp(target, "/usr/local/bin/kdos-appbox"))
				unlink(p);
		}
		free(p);
	}
	kb_strv_free(old);

	int n = 0;
	for (int i = 0; i < napps; i++) {
		if (in_list(RESERVED, apps[i].id)) {
			kb_warn("shim %s: reserved name, skipped", apps[i].id);
			continue;
		}
		if (host_has(fsroot, apps[i].id)) {
			kb_warn("shim %s: the host has /usr/bin/%s, skipped",
				apps[i].id, apps[i].id);
			continue;
		}
		char *p = kb_path_join(bindir, apps[i].id);
		/* ABSOLUTE in the user's tree: `~/.local/bin/gimp -> kdos-appbox`
		 * resolves nothing, since the dispatcher is not beside it. The
		 * root tree's shims stay relative — that is the marker for one
		 * this program wrote, and there the dispatcher IS beside them. */
		const char *tgt = bindir[0] == '/' && strstr(bindir, "/.local/")
				? "/usr/local/bin/kdos-appbox" : "kdos-appbox";
		if (symlink(tgt, p) == 0)
			n++;
		else if (errno != EEXIST)
			kb_warn("shim %s: %s", apps[i].id, strerror(errno));
		free(p);
	}
	return n;
}

/*
 * THE PACK LANE'S SOURCE IS THE PACKS THEMSELVES. A pack carries its own
 * /usr/share/applications — it is the application's own filesystem — so the
 * parse above is reused whole rather than reimplemented against the metadata
 * blob. What the metadata is for is a pack that is NOT installed: the store
 * lists it from `name`, `category`, `mime` and `command` without anything
 * being mounted.
 *
 * Each pack is mounted through kdos-packd, which is also the only thing that
 * can mount one, and the mount stays: the box that runs the application is
 * about to need it.
 */
static int genlaunchers_packs(void)
{
	char *list = pack_list();
	char *line, *save;
	int packs = 0;

	if (!list) {
		kb_warn("kdos-packd is not answering — no packs to read");
		return -1;
	}
	for (line = strtok_r(list, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char id[96], kind[32], req[256], mnt[1024], dir[1152];
		char *f[6] = {0};
		int nf = 0;
		char *tok, *s2 = NULL;

		for (tok = strtok_r(line, "\t", &s2); tok && nf < 6;
		     tok = strtok_r(NULL, "\t", &s2))
			f[nf++] = tok;
		if (nf < 3)
			continue;
		kb_strlcpy(id, f[0], sizeof(id));
		kb_strlcpy(kind, f[2], sizeof(kind));
		/* Only an app pack carries launchers. A runtime's
		 * /usr/share/applications is whatever Debian's libraries
		 * dropped there and is not this desktop's business. */
		if (strcmp(kind, "app"))
			continue;
		/* AND ONLY AN INSTALLED ONE. The list carries every pack on the
		 * medium as `available`, and mounting each to read its entries
		 * makes it installed — so a regeneration that read them all
		 * would mount every available pack, and `kdos app remove` could
		 * never take a launcher away because the regeneration that
		 * follows would put the pack straight back. The fourth field is
		 * the state. */
		if (nf < 4 || (strcmp(f[3], "installed") && strcmp(f[3], "mounted")))
			continue;

		snprintf(req, sizeof(req), "mount %s", id);
		if (packd_ask(req, mnt, sizeof(mnt)) != 0) {
			kb_warn("%s: %s", id, mnt[0] ? mnt : "cannot mount");
			continue;
		}
		snprintf(dir, sizeof(dir), "%s/usr/share/applications", mnt);
		if (!kb_is_dir(dir))
			continue;
		kb_strlcpy(cur_pack, id, sizeof(cur_pack));
		parse_dir(dir);
		add_commands(dir);
		cur_pack[0] = 0;
		packs++;
	}
	free(list);
	return packs;
}

/*
 * A STORE BOX'S COMMANDS ARE THE CATALOGUE'S `cmd` ROWS. Only its desktop
 * entries are copied out of the image, so there is no usr/bin to probe, and a
 * command-only application — a solver, a converter — would otherwise reach
 * the host as nothing at all. The catalogue is loaded only on the store path,
 * so the build's extracted packs never take this.
 */
static int add_catalogue_commands(const char *id)
{
	char cmds[16][64];
	int n;

	if (!cat_count() || !cat_find(id))
		return 0;
	n = cat_cmds(id, cmds, 16);
	for (int i = 0; i < n; i++) {
		Launcher *a = app_new();

		a->cmdonly = 1;
		kb_strlcpy(a->pack, id, sizeof(a->pack));
		kb_strlcpy(a->id, cmds[i], sizeof(a->id));
		kb_strlcpy(a->base, cmds[i], sizeof(a->base));
		kb_strlcpy(a->name, cmds[i], sizeof(a->name));
		kb_strlcpy(a->exec, cmds[i], sizeof(a->exec));
	}
	return n;
}

/*
 * TWO TREES, AND WHICH ONE IS WHOSE DECISION.
 *
 * The ROOT tree — /etc/skel, /usr/share/kdos/alien-apps, /usr/local/bin — is
 * what the BUILD writes for the recommended set. It is a system decision and
 * it is root's.
 *
 * The USER tree — $XDG_DATA_HOME/applications and
 * $XDG_DATA_HOME/kdos/alien-apps (~/.local/share when the variable is unset),
 * and ~/.local/bin — is what `kdos app install` writes, AS THE USER, the
 * moment a pack is installed. Every reader already looks there first: the
 * Start menu reads the user's applications directory, the dispatcher reads the
 * user's
 * alien-apps table before the system one, and ~/.local/bin is on the PATH the
 * skel profile sets. So an installed application is in the menu before the
 * install command returns, with no root anywhere.
 *
 * Nothing regenerates the root tree at boot, at login or after an install, so
 * an install that wrote only there would end in an application missing from
 * the menu until somebody ran genlaunchers by hand as root.
 */
/*
 * THE BUILD'S SOURCE IS EXTRACTED PACKS, ONE DIRECTORY EACH. The ISO has to
 * ship launchers for the recommended set — a Start menu with nothing in it is
 * a distribution with no applications — and at image time there is no
 * kdos-packd and no mount: the build is a chroot in an unprivileged container.
 * `kdos-pack image` + `fsck.erofs --extract` put each pack's desktop entries
 * under <root>/<pack-id>/, and the directory NAME is what the table's third
 * field carries, so a launcher written here dispatches to the same box a
 * runtime regeneration would. A pack whose entries are absent contributes
 * nothing rather than failing the set.
 */
static int genlaunchers_dirs(const char *root)
{
	char **subs = kb_listdir(root, NULL);
	int packs = 0;

	for (char **s = subs; s && *s; s++) {
		/* <root>/<pack>/usr/share/applications — the pack's own layout,
		 * so the parser sees exactly what a mount would show it. */
		char *sub = kb_path_join(root, *s);
		char *dir = kb_path_join(sub, "usr/share/applications");
		int cmds = add_catalogue_commands(*s);

		if (kb_is_dir(dir)) {
			kb_strlcpy(cur_pack, *s, sizeof(cur_pack));
			parse_dir(dir);
			add_commands(dir);
			cur_pack[0] = 0;
		}
		if (cmds || kb_is_dir(dir))
			packs++;
		free(dir);
		free(sub);
	}
	kb_strv_free(subs);
	return packs;
}

/*
 * THE LAUNCHERS A BOX HAS, read back: every entry in the user's applications
 * directory, then the skeleton's, that names the box in `X-KDOS-Box` — the
 * generated ones and `kdos-box export`'s alike. The in-box command is what
 * follows the verb in the Exec line, field codes dropped; a launcher id that
 * is already listed is not listed twice.
 */
static int read_box_entry(const char *path, const char *box, BoxLauncher *o)
{
	KxdgEntry e;
	char store[2048];
	const char *w[KB_MAX_ARGV];
	const char *b, *ex;
	int nw, at = -1, ok = 0;
	KbBuf cmd = {0};

	if (kxdg_load(&e, path, "Desktop Entry") != 0)
		return 0;
	b = kxdg_get(&e, "X-KDOS-Box", "");
	ex = kxdg_get(&e, "Exec", "");
	if (!b || strcmp(b, box) || !ex) {
		kxdg_free(&e);
		return 0;
	}
	memset(o, 0, sizeof(*o));
	{
		const char *leaf = kb_basename(path);
		size_t l = strlen(leaf);

		snprintf(o->id, sizeof(o->id), "%.*s", (int)(l > 8 ? l - 8 : l),
			 leaf);
	}
	kb_strlcpy(o->upstream, kxdg_get(&e, "X-KDOS-Upstream", ""),
		   sizeof(o->upstream));
	kb_strlcpy(o->mime, kxdg_get(&e, "MimeType", ""), sizeof(o->mime));
	nw = kxdg_exec_split(ex, NULL, -1, store, sizeof(store), w, KB_MAX_ARGV);
	/* `kdos-appbox [-b B] [-l L] run CMD…` or `kdos-box run [-l L] B CMD…` */
	for (int i = 0; i < nw && at < 0; i++) {
		if (strcmp(w[i], "run"))
			continue;
		at = i + 1;
		if (!strcmp(kb_basename(w[0]), "kdos-box")) {
			if (at + 1 < nw && !strcmp(w[at], "-l"))
				at += 2;
			at++;		/* the box */
		}
	}
	for (int i = at; at >= 0 && i < nw; i++) {
		char q[1024];

		if (w[i][0] == '%' && w[i][1] && !w[i][2])
			continue;
		if (kxdg_exec_quote(w[i], q, sizeof(q)) == 0)
			kb_buf_printf(&cmd, "%s%s", cmd.p ? " " : "", q);
	}
	if (cmd.p) {
		kb_strlcpy(o->exec, cmd.p, sizeof(o->exec));
		ok = 1;
	}
	kb_buf_free(&cmd);
	kxdg_free(&e);
	return ok;
}

int box_launchers(const char *box, BoxLauncher *out, int max)
{
	char user[1024];
	const char *xdg = getenv("XDG_DATA_HOME");
	const char *dirs[2];
	int n = 0;

	if (xdg && *xdg)
		snprintf(user, sizeof(user), "%s/applications", xdg);
	else
		snprintf(user, sizeof(user), "%s/.local/share/applications",
			 kb_home_dir());
	dirs[0] = user;
	dirs[1] = "/etc/skel/.local/share/applications";
	for (int d = 0; d < 2 && n < max; d++) {
		char **names = kb_listdir(dirs[d], NULL);

		for (char **f = names; f && *f && n < max; f++) {
			char *p;
			int dup = 0;

			if (!ends_with(*f, ".desktop"))
				continue;
			p = kb_path_join(dirs[d], *f);
			if (read_box_entry(p, box, &out[n])) {
				for (int k = 0; k < n && !dup; k++)
					dup = !strcmp(out[k].id, out[n].id);
				if (!dup)
					n++;
			}
			free(p);
		}
		kb_strv_free(names);
	}
	return n;
}

/*
 * A `kdos-box export` LAUNCHER WHOSE BOX IS GONE. Its box is neither a
 * container nor a profile any more — removed with `kdos-box remove` and its
 * profile deleted — so the entry and its `<command>@<box>` shim are a Start
 * menu row and a command that open nothing. They are the user's, written by
 * export and marked `X-KDOS-Export=true`, and they go here, where the rest of
 * the menu is reconciled.
 */
static int sweep_stale_exports(const char *dir)
{
	char **names = kb_listdir(dir, NULL);
	int n = 0;

	for (char **f = names; f && *f; f++) {
		KxdgEntry e;
		char *p, box[64] = "";
		int stale = 0;

		if (!ends_with(*f, ".desktop"))
			continue;
		p = kb_path_join(dir, *f);
		if (kxdg_load(&e, p, "Desktop Entry") == 0) {
			if (kxdg_bool(&e, "X-KDOS-Export", 0))
				kb_strlcpy(box, kxdg_get(&e, "X-KDOS-Box", ""),
					   sizeof(box));
			kxdg_free(&e);
		}
		if (box[0] && box_name_ok(box)) {
			char *pp = profile_path(box);

			stale = !kb_path_exists(pp) && !box_exists(box);
			free(pp);
		}
		if (stale && unlink(p) == 0) {
			char shim[1024], target[256];
			char **bins;
			char *bin = kb_path_join(kb_home_dir(), ".local/bin");
			size_t bl = strlen(box);

			bins = kb_listdir(bin, NULL);
			for (char **s = bins; s && *s; s++) {
				size_t l = strlen(*s);
				ssize_t r;

				if (l <= bl + 1 || (*s)[l - bl - 1] != '@' ||
				    strcmp(*s + l - bl, box))
					continue;
				snprintf(shim, sizeof(shim), "%s/%s", bin, *s);
				r = readlink(shim, target, sizeof(target) - 1);
				if (r > 0) {
					target[r] = '\0';
					if (!strcmp(target, "/usr/local/bin/kdos-appbox"))
						unlink(shim);
				}
			}
			kb_strv_free(bins);
			free(bin);
			n++;
		}
		free(p);
	}
	kb_strv_free(names);
	return n;
}

/*
 * DOLPHIN'S "OPEN IN A THROWAWAY BOX", offered only on the types a boxed
 * application opens: a KIO service menu whose MimeType is the union of every
 * boxed launcher's, written beside the launchers it follows and removed when
 * there are none. A service menu in a user's own data directory runs only
 * with its execute bit set — KIO's rule for menus anybody could have written
 * — so it is written 0755.
 */
static void write_throwaway_menu(const char *data)
{
	char dir[2048], path[2100];
	KbBuf mimes = {0}, b = {0};
	int n = 0;

	snprintf(dir, sizeof(dir), "%s/kio/servicemenus", data);
	snprintf(path, sizeof(path), "%s/kdos-disposable.desktop", dir);
	for (int i = 0; i < napps; i++) {
		char *copy, *save = NULL;

		if (apps[i].cmdonly || !apps[i].pack[0] || !apps[i].mime[0])
			continue;
		copy = kb_strdup(apps[i].mime);
		for (char *m = strtok_r(copy, ";", &save); m;
		     m = strtok_r(NULL, ";", &save)) {
			char probe[256];

			while (*m == ' ')
				m++;
			/* No scheme handlers: a throwaway box opens a file. */
			if (!*m || !strncmp(m, "x-scheme-handler/", 17))
				continue;
			snprintf(probe, sizeof(probe), ";%s;", m);
			if (mimes.p && strstr(mimes.p, probe))
				continue;
			kb_buf_printf(&mimes, "%s%s;", mimes.p ? "" : ";", m);
			n++;
		}
		free(copy);
	}
	if (!n) {
		unlink(path);
		kb_buf_free(&mimes);
		return;
	}
	kb_mkdir_p(dir);
	kb_buf_printf(&b,
		      "[Desktop Entry]\n"
		      "Type=Service\n"
		      "MimeType=%s\n"
		      "Actions=throwaway;\n"
		      "X-KDE-Priority=TopLevel\n"
		      "X-KDOS-Alien=true\n"
		      "\n"
		      "[Desktop Action throwaway]\n"
		      "Name=Open in a Throwaway Box\n"
		      "Icon=security-high\n"
		      "Exec=kdos-appbox throwaway %%f\n",
		      mimes.p + 1);
	if (kb_write_all(path, b.p, b.n) == 0)
		chmod(path, 0755);
	kb_buf_free(&b);
	kb_buf_free(&mimes);
}

/* KService's cache, rebuilt so the Start menu, KRunner and the Task Manager
 * see the launchers now rather than whenever kded next notices. Best effort:
 * a session without KDE's tools reads the directories itself. */
static void rebuild_sycoca(void)
{
	KbArgv a = {0};

	if (!kb_have_prog("kbuildsycoca6"))
		return;
	kb_argv_add(&a, "kbuildsycoca6");
	kb_argv_end(&a);
	kb_run(&a);
}

/* The skeleton's window rules: one per pack box the image ships, in the
 * neutral box scheme, because no box has a profile yet when the image is
 * made. A user's genlaunchers writes their own over them, accents and all. */
static void skel_rules(const char *fsroot)
{
	char path[1024];
	const char *boxes[256];
	int n = 0;

	for (int i = 0; i < napps && n < 256; i++) {
		int seen = 0;

		if (!apps[i].pack[0] || apps[i].cmdonly)
			continue;
		for (int k = 0; k < n && !seen; k++)
			seen = !strcmp(boxes[k], apps[i].pack);
		if (!seen)
			boxes[n++] = apps[i].pack;
	}
	snprintf(path, sizeof(path), "%s/etc/skel/.config/kwinrulesrc",
		 strcmp(fsroot, "/") ? fsroot : "");
	if (rules_write(path, boxes, NULL, n) < 0)
		kb_warn("cannot write %s", path);
}

int cmd_genlaunchers(const char *srcdir, const char *fsroot, int user, int packsdir)
{
	napps = 0;
	if (srcdir && packsdir) {
		int n = genlaunchers_dirs(srcdir);
		fprintf(stderr, "%d extracted pack(s) read\n", n);
		/* The user tree is written whole, so a store regeneration that
		 * read only the store boxes would delete every installed
		 * pack's launchers; it reads both sources. */
		if (user) {
			n = genlaunchers_packs();
			if (n < 0) {
				kb_warn("the launchers are left as they were");
				return 1;
			}
			fprintf(stderr, "%d app pack(s) read\n", n);
		}
	} else if (srcdir) {
		parse_dir(srcdir);
		add_commands(srcdir);
	} else {
		/* A DAEMON THAT DOES NOT ANSWER IS NOT AN EMPTY STORE. The set
		 * is written whole, so reading nothing would sweep every pack's
		 * launcher out of the menu until the next install; it is left
		 * as it is instead. */
		int n = genlaunchers_packs();
		if (n < 0) {
			kb_warn("the launchers are left as they were");
			return 1;
		}
		fprintf(stderr, "%d app pack(s) read\n", n);
	}
	assign_ids();

	char *apps_dir, *share, *bindir, *data;
	if (user) {
		/* $XDG_DATA_HOME when it is set, as the Start menu, `kdos app`
		 * and the launch path read it: a table written under one
		 * directory and read under another is an install whose
		 * applications never launch. ~/.local/bin has no XDG variable
		 * and is the PATH the skel profile sets. */
		const char *home = kb_home_dir();
		const char *xdg = getenv("XDG_DATA_HOME");

		data = xdg && *xdg ? kb_strdup(xdg)
				   : kb_path_join(home, ".local/share");
		apps_dir = kb_path_join(data, "applications");
		share = kb_path_join(data, "kdos");
		bindir = kb_path_join(home, ".local/bin");
	} else {
		data = kb_path_join(fsroot, "etc/skel/.local/share");
		apps_dir = kb_path_join(fsroot, "etc/skel/.local/share/applications");
		share = kb_path_join(fsroot, "usr/share/kdos");
		bindir = kb_path_join(fsroot, "usr/local/bin");
	}
	write_launchers(apps_dir, data);
	write_throwaway_menu(data);
	int mimes = write_mimeinfo(apps_dir);
	int stale = user ? sweep_stale_exports(apps_dir) : 0;
	free(apps_dir);
	free(data);

	int shims = write_shims(share, bindir, user ? "/" : fsroot);
	free(share);
	free(bindir);
	int cmds = 0;
	for (int i = 0; i < napps; i++)
		cmds += apps[i].cmdonly;
	fprintf(stderr, "%d launchers, %d command-only, %d mime types, %d shims"
		"%s\n", napps - cmds, cmds, mimes, shims,
		stale ? ", stale exports removed" : "");
	if (user) {
		box_rules_refresh();
		rebuild_sycoca();
	} else {
		skel_rules(fsroot);
	}
	return 0;
}
