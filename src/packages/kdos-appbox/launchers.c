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
	"kdos-appbox", "kdos-banner", "kdos-desktop", "kdos-desktop-start",
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
	"gnome-disks", "testdisk", "org.gnome.baobab-root", NULL
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

#define MAX_APPS 256

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
} Launcher;

static Launcher apps[MAX_APPS];
static int napps;

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
		    (strstr(cats, "Settings") && !strstr(cats, "System"))) {
			kxdg_free(&e);
			continue;
		}

		if (napps >= MAX_APPS) {
			kxdg_free(&e);
			kb_warn("more than %d apps; the rest are ignored", MAX_APPS);
			break;
		}
		Launcher *a = &apps[napps++];
		memset(a, 0, sizeof(*a));

		kb_strlcpy(a->pack, cur_pack, sizeof(a->pack));
		kb_strlcpy(a->base, base, sizeof(a->base));
		const char *ren = lookup(RENAME, base);
		if (ren) {
			kb_strlcpy(a->id, ren, sizeof(a->id));
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
		if (napps >= MAX_APPS)
			return;
		Launcher *a = &apps[napps++];
		memset(a, 0, sizeof(*a));
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
			      "Terminal=false\n"
			      "Categories=%s\n",
			      a->name, a->name, a->exec, a->icon, a->cats);
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

static int write_shims(const char *fsroot)
{
	char *share = kb_path_join(fsroot, "usr/share/kdos");
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
	kb_write_all(table, t.p, t.n);
	free(table);
	free(share);
	kb_buf_free(&t);

	char *bindir = kb_path_join(fsroot, "usr/local/bin");
	kb_mkdir_p(bindir);

	/* Every shim goes, whatever it used to point at — the dispatcher has
	 * changed name once already and a stale symlink is a dead command. A
	 * RELATIVE link target is the marker: the hand-written entries in this
	 * directory are real files. */
	char **old = kb_listdir(bindir, NULL);
	for (char **f = old; f && *f; f++) {
		char *p = kb_path_join(bindir, *f);
		char target[256];
		ssize_t n = kb_is_link(p) ? readlink(p, target, sizeof(target) - 1)
					  : -1;
		if (n > 0) {
			target[n] = 0;
			if (target[0] != '/')
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
		if (symlink("kdos-appbox", p) == 0)
			n++;
		free(p);
	}
	free(bindir);
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

int cmd_genlaunchers(const char *srcdir, const char *fsroot)
{
	napps = 0;
	if (srcdir) {
		parse_dir(srcdir);
		add_commands(srcdir);
	} else {
		int n = genlaunchers_packs();
		fprintf(stderr, "%d app pack(s) read\n", n);
	}

	char *skel = kb_path_join(fsroot, "etc/skel/.local/share/applications");
	write_launchers(skel);
	int mimes = write_mimeinfo(skel);
	free(skel);

	int shims = write_shims(fsroot);
	int cmds = 0;
	for (int i = 0; i < napps; i++)
		cmds += apps[i].cmdonly;
	fprintf(stderr, "%d launchers, %d command-only, %d mime types, %d shims\n",
		napps - cmds, cmds, mimes, shims);
	return 0;
}
