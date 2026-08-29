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
 * Generate everything the host needs to present the appbox's apps as its own.
 * This was ports/appbox/genlaunchers.py.
 *
 *   kdos-appbox genlaunchers <desktop-dir> <fs-root>
 *
 * <desktop-dir> is the image's /usr/share/applications. After an ISO build the
 * flattened appbox layer has it on disk already:
 *   build/fs/home/kdos/.local/share/containers/storage/overlay/<id>/diff/usr/share/applications
 *
 * Four outputs, and dropping any one of them breaks something visible:
 *
 *   etc/skel/.local/share/applications/<upstream-id>.desktop
 *       the launcher, keeping UPSTREAM's own desktop-file id — not
 *       kdos-<name>, and not StartupWMClass either. Measured in a booted VM:
 *       kdos-shell matches a running toplevel to a desktop entry by the
 *       entry's FILE ID and ignores StartupWMClass, and a Wayland app_id is
 *       NOT the X11 WM_CLASS — GIMP's entry says StartupWMClass=gimp-3.0 but
 *       its toplevel announces app_id "gimp" (confirmed with WAYLAND_DEBUG=1),
 *       which is exactly its upstream filename. Get this wrong and every
 *       running alien app shows a second grey cog beside its own pinned icon.
 *   etc/skel/.local/share/applications/mimeinfo.cache
 *       the mime -> desktop-id index. Written here rather than left to
 *       update-desktop-database: the host has no desktop-file-utils, and
 *       without the cache the MimeType lines are never consulted, so no alien
 *       app appears in any "Open with" dialog and none can be a default
 *       handler.
 *   usr/share/kdos/alien-apps
 *       name -> in-box command line, read by the launch path.
 *   usr/local/bin/<name> -> kdos-appbox
 *       one symlink per app, so every alien app is also a normal command in
 *       $PATH. kdos-appbox dispatches on its own basename, the way busybox
 *       does, which keeps the alien-app path free of any shell.
 */

#include "kdos-appbox.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "kxdg.h"

/*
 * Shim names that must never be created in /usr/local/bin. The host userland
 * is musl + toybox + wlroots and currently collides with none of the Debian app
 * names, but the app set moves; a shim shadowing a host tool would be a very
 * confusing bug.
 */
static const char *RESERVED[] = {
	"sh", "bash", "env", "ls", "cp", "mv", "rm", "cat", "sed", "awk", "grep",
	"find", "tar", "gzip", "python3", "perl", "make", "gcc", "kdos", "foot",
	"kdos-appbox", "kdos-box", "kdos-banner", "kdos-desktop",
	"kdos-desktop-start",
	"kdos-shot", "kdos-fetch-app", "kdos-fetch-static", "kdos-getty",
	"kdos-theme", "kdos-theme-helper", "kinstall", "kpkg", "kpkgadd",
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
 * KWIN-ONLY. Spectacle on Wayland asks KWin's own screenshot interface and
 * opens an error dialog on any other compositor — measured on this desktop:
 * "On Wayland, Spectacle requires KDE Plasma's KWin compositor". Screenshots
 * are the host's, `kdos-shot`, which is where the capture globals are.
 */
static const char *SKIP_NEEDS_KWIN[] = {
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

/* Old-launcher names the dock favorites and the docs already reference. */
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
 * Only emitted when the image actually carries the binary, so an appbox baked
 * before this segment existed does not get a shim that dies on "not found".
 */
static const char *COMMANDS[] = {
	"wine", "winecfg", "winetricks", NULL
};

static const char *X11_FORCING[] = {
	"GDK_BACKEND=x11", "CLUTTER_BACKEND=x11", "QT_QPA_PLATFORM=xcb",
	"SDL_VIDEODRIVER=x11", "MOZ_ENABLE_WAYLAND=0",
	"ELECTRON_OZONE_PLATFORM_HINT=x11", NULL
};

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
	char wmclass[160];
	char pack[96];		/* the pack that provides it, or "" for an image */
	int cmdonly;		/* a command, not an application: no launcher */
	int terminal;		/* upstream's Terminal=true: it draws in one */
} Launcher;

/*
 * THE TABLE GROWS, because a fixed one silently loses applications. It held
 * 256, which covered the monolith's ~105 launchers with room to spare — and
 * the pack lane parses each pack's OWN desktop entries, so 108 app packs
 * produce well past that (LibreOffice alone carries eight). What the ceiling
 * did was drop the tail: no error, an exit status of 0, and a Start menu
 * missing whatever sorted last. It is on the heap so the dispatcher, which is
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
 * launch in — the third field of the alien-apps table. */
static char cur_pack[96];

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
 * and a substring test dropped Remmina's launcher for it. */
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
	if (!files)
		kb_die("cannot read %s", srcdir);

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
		    in_list(SKIP_NEEDS_KWIN, base) ||
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
		/* The window this launcher opens announces the APP's app_id,
		 * not ours, so without this the dock cannot tie a running alien
		 * app back to any desktop entry. */
		kb_strlcpy(a->wmclass, kxdg_get(&e, "StartupWMClass", base),
			   sizeof(a->wmclass));
		/* R, octave-cli and the like say `Terminal=true`, and the Start
		 * menu wraps such an entry in `foot -e`. Written as false, the
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

/*
 * The image root, from the desktop directory it was pointed at: the caller
 * passes <image>/usr/share/applications, so three components up is the root.
 * Nothing else here needs it, which is why it is derived rather than passed.
 */
static void add_commands(const char *srcdir)
{
	char root[1024];
	kb_strlcpy(root, srcdir, sizeof(root));
	for (int up = 0; up < 3; up++) {
		char *slash = strrchr(root, '/');
		if (!slash)
			return;
		*slash = 0;
	}

	for (int i = 0; COMMANDS[i]; i++) {
		char probe[1200];
		snprintf(probe, sizeof(probe), "%s/usr/bin/%s", root, COMMANDS[i]);
		if (!kb_path_exists(probe))
			continue;
		Launcher *a = app_new();
		a->cmdonly = 1;
		kb_strlcpy(a->id, COMMANDS[i], sizeof(a->id));
		kb_strlcpy(a->base, COMMANDS[i], sizeof(a->base));
		kb_strlcpy(a->name, COMMANDS[i], sizeof(a->name));
		kb_strlcpy(a->exec, COMMANDS[i], sizeof(a->exec));
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static void write_launchers(const char *dir)
{
	kb_mkdir_p(dir);

	/* Cleared by MARKER, not by name: these files have been called
	 * kdos-<id> and <app_id> at different times and an orphan launcher is a
	 * dead icon on the desktop. */
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

	for (int i = 0; i < napps; i++) {
		Launcher *a = &apps[i];
		if (a->cmdonly)
			continue;
		KbBuf b = {0};
		kb_buf_printf(&b,
			      "[Desktop Entry]\n"
			      "Type=Application\n"
			      "Name=%s\n"
			      "Comment=%s (alien app, kdos-apps box)\n"
			      "Exec=kdos-appbox run %s\n"
			      "Icon=%s\n"
			      "Terminal=%s\n"
			      "Categories=%s\n",
			      a->name, a->name, a->exec, a->icon,
			      a->terminal ? "true" : "false", a->cats);
		if (a->generic[0])
			kb_buf_printf(&b, "GenericName=%s\n", a->generic);
		if (a->mime[0])
			kb_buf_printf(&b, "MimeType=%s\n", a->mime);
		if (a->keywords[0])
			kb_buf_printf(&b, "Keywords=%s\n", a->keywords);
		kb_buf_printf(&b, "StartupWMClass=%s\nX-KDOS-Alien=true\n",
			      a->wmclass);

		char leaf[160];
		snprintf(leaf, sizeof(leaf), "%s.desktop", a->base);
		char *p = kb_path_join(dir, leaf);
		kb_write_all(p, b.p, b.n);
		free(p);
		kb_buf_free(&b);
	}
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
				 "%s%s.desktop", used ? ";" : "", apps[i].base);
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

static int write_shims(const char *share, const char *bindir)
{
	kb_mkdir_p(share);

	KbBuf t = {0};
	/* The third field is the pack, and it is OPTIONAL: a table written
	 * before the pack lane is two fields and still parses, which is why
	 * every reader splits at the second tab rather than counting them. */
	kb_buf_str(&t, "# name\tcommand[\tpack] — GENERATED by kdos-appbox "
		       "genlaunchers\n");
	for (int i = 0; i < napps; i++) {
		if (apps[i].pack[0])
			kb_buf_printf(&t, "%s\t%s\t%s\n", apps[i].id,
				      apps[i].exec, apps[i].pack);
		else
			kb_buf_printf(&t, "%s\t%s\n", apps[i].id, apps[i].exec);
	}
	char *table = kb_path_join(share, "alien-apps");
	/* A WRITE THAT FAILED MUST NOT REPORT A LAUNCHER SET. Every output
	 * here is under /usr, so a run as anyone but root writes nothing at
	 * all — and this program went on to print "11 launchers, 188 mime
	 * types, 0 shims" and exit 0, which reads as the pack lane having
	 * taken over when the shipped table is still whoever wrote it last. */
	if (kb_write_all(table, t.p, t.n) != 0)
		kb_die("cannot write %s: %s", table, strerror(errno));
	free(table);
	kb_buf_free(&t);

	kb_mkdir_p(bindir);

	/*
	 * Every shim goes, whatever it used to point at — the dispatcher has
	 * changed name once already and a stale symlink is a dead command. A
	 * RELATIVE link target is the marker for one this program wrote.
	 *
	 * RESERVED IS CONSULTED HERE AS WELL AS AT CREATE TIME, and that is
	 * what keeps `kdos-box` alive: it is the box manager's name on this
	 * same binary, installed by the recipe as a relative symlink, so a
	 * sweep that went by the marker alone deleted the front door to every
	 * box on the machine — leaving `kdos-box: command not found` on a
	 * system where nothing was missing but a link.
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
		return 0;
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
		 * makes it installed — so one regeneration turned 163 available
		 * packs into 163 mounted ones, and `kdos app remove` could never
		 * take a launcher away because the regeneration that followed
		 * put the pack straight back. The fourth field is the state. */
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
 * TWO TREES, AND WHICH ONE IS WHOSE DECISION.
 *
 * The ROOT tree — /etc/skel, /usr/share/kdos/alien-apps, /usr/local/bin — is
 * what the BUILD writes for the recommended set. It is a system decision and
 * it is root's.
 *
 * The USER tree — ~/.local/share/applications, ~/.local/share/kdos/alien-apps,
 * ~/.local/bin — is what `kdos app install` writes, AS THE USER, the moment a
 * pack is installed. Every reader already looks there first: the Start menu
 * reads the user's applications directory, the dispatcher reads the user's
 * alien-apps table before the system one, and ~/.local/bin is on the PATH the
 * skel profile sets. So an installed application is in the menu before the
 * install command returns, with no root anywhere.
 *
 * Without this the pack lane's install ended in an application nobody could
 * launch: `genlaunchers` was a separate program that had to be run BY HAND, AS
 * ROOT, and nothing ran it at boot, at login or after an install. The Start
 * menu's own INSTALL FROM THE MEDIUM row ran the install and then showed the
 * same menu it showed before, with the application still absent from it.
 */
/*
 * THE BUILD'S SOURCE IS EXTRACTED PACKS, ONE DIRECTORY EACH. The ISO has to
 * ship launchers for the recommended set — a Start menu with nothing in it is
 * a distribution with no applications — and at packaging time there is no
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
		if (kb_is_dir(dir)) {
			kb_strlcpy(cur_pack, *s, sizeof(cur_pack));
			parse_dir(dir);
			add_commands(dir);
			cur_pack[0] = 0;
			packs++;
		}
		free(dir);
		free(sub);
	}
	kb_strv_free(subs);
	return packs;
}

int cmd_genlaunchers(const char *srcdir, const char *fsroot, int user, int packsdir)
{
	napps = 0;
	if (srcdir && packsdir) {
		int n = genlaunchers_dirs(srcdir);
		fprintf(stderr, "%d extracted pack(s) read\n", n);
	} else if (srcdir) {
		parse_dir(srcdir);
		add_commands(srcdir);
	} else {
		int n = genlaunchers_packs();
		fprintf(stderr, "%d app pack(s) read\n", n);
	}

	char *apps_dir, *share, *bindir;
	if (user) {
		const char *home = kb_home_dir();
		apps_dir = kb_path_join(home, ".local/share/applications");
		share = kb_path_join(home, ".local/share/kdos");
		bindir = kb_path_join(home, ".local/bin");
	} else {
		apps_dir = kb_path_join(fsroot, "etc/skel/.local/share/applications");
		share = kb_path_join(fsroot, "usr/share/kdos");
		bindir = kb_path_join(fsroot, "usr/local/bin");
	}
	write_launchers(apps_dir);
	int mimes = write_mimeinfo(apps_dir);
	free(apps_dir);

	int shims = write_shims(share, bindir);
	free(share);
	free(bindir);
	int cmds = 0;
	for (int i = 0; i < napps; i++)
		cmds += apps[i].cmdonly;
	fprintf(stderr, "%d launchers, %d command-only, %d mime types, %d shims\n",
		napps - cmds, cmds, mimes, shims);
	return 0;
}
