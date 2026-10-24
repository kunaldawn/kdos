/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   triggers — the shared indexes a package's files feed
 *
 * Some files do nothing until an index built from EVERY package's copy is
 * rebuilt: a GSettings schema is invisible until gschemas.compiled holds it,
 * a pixbuf loader is never tried until loaders.cache names it, a MIME XML
 * defines no type until the globs are regenerated, and libvlc loads every
 * VLC plugin into its program until plugins.dat describes them. No single
 * package can own that index, so no build.sh can write it, and a per-port
 * postinstall only rebuilds it when that one port is installed — not when
 * the next port adds to it or the last one leaves.
 *
 * So kpkgadd and kpkgdel read the manifest they just acted on, and every
 * index whose directory it touched is rebuilt once from what is now on disk.
 *
 * Rules, each one silent when broken:
 *
 *  - A missing tool is skipped, not an error. The bootstrap installs the
 *    schemas before glib and the fonts before fontconfig; the index is
 *    written when the package carrying the tool arrives, because that
 *    package's own files touch a directory the trigger watches. fontconfig
 *    installs no font, so the font cache also watches `etc/fonts/`: without
 *    it a system whose fonts all came first has no cache at all.
 *  - A failing tool is a warning. The package is already on disk and in the
 *    database; failing the install here would report a half-install that
 *    is not one.
 *  - Every index is rebuilt from the whole directory, never patched with one
 *    package's files — a removal has nothing left to patch with. The manual
 *    index is the one exception, and only while nothing was removed: two
 *    packages in three carry manual pages, and re-reading every page on the
 *    system for each of them costs seconds per install. A placed page is
 *    merged; any removed page, or a missing database, rebuilds the whole
 *    tree, because a merge cannot drop an entry for a file already gone.
 *  - `KPKG_SKIP_INDEX` names indexes (space-separated, by the names in
 *    t_name) that are not rebuilt, with one line saying so. The build sets
 *    `man` in the chroot: 70_image writes every manual index from scratch,
 *    so a merge per install is work the image step throws away. Nothing
 *    else sets it, so a running system keeps every index current.
 *  - `--root`: tools are handed the root-prefixed directory. The pixbuf
 *    loader cache cannot take one — the tool writes the path it was
 *    compiled with — so under `--root` it runs inside the root through
 *    chroot(8), which needs root and the root's own copy of the tool. VLC's
 *    plugin cache goes the same way: vlc-cache-gen loads every plugin to
 *    read it, and a plugin loaded from another root links that root's
 *    libraries against the running system's.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

enum {
	T_SCHEMAS,
	T_GIO,
	T_PIXBUF,
	T_MIME,
	T_FONTS,
	T_INFO,
	T_HWDB,
	T_MAN,
	T_XFONTS,
	T_TEXMF,
	T_DESKTOP,
	T_ICONS,
	T_VLC,
	T_COUNT
};

#define T_MAXDIRS 3

/* Manifest prefixes, without the `./` every manifest line carries. A package
 * may carry `lib/udev/...` as well as `usr/lib/udev/...`: /lib is a symlink
 * on the target, but the manifest records the path the package staged. */
static const char *const t_dir[T_COUNT][T_MAXDIRS] = {
	[T_SCHEMAS] = { "usr/share/glib-2.0/schemas/" },
	[T_GIO] = { "usr/lib/gio/modules/" },
	[T_PIXBUF] = { "usr/lib/gdk-pixbuf-2.0/" },
	[T_MIME] = { "usr/share/mime/packages/" },
	[T_FONTS] = { "usr/share/fonts/", "etc/fonts/" },
	[T_INFO] = { "usr/share/info/" },
	[T_HWDB] = { "etc/udev/hwdb.d/", "usr/lib/udev/hwdb.d/",
		     "lib/udev/hwdb.d/" },
	[T_MAN] = { "usr/share/man/" },
	[T_XFONTS] = { "usr/share/fonts/" },
	[T_TEXMF] = { "usr/share/texmf-dist/", "usr/share/texmf-local/" },
	[T_DESKTOP] = { "usr/share/applications/" },
	[T_ICONS] = { "usr/share/icons/" },
	[T_VLC] = { "usr/lib/vlc/plugins/" },
};

/* What `KPKG_SKIP_INDEX` calls each index. */
static const char *const t_name[T_COUNT] = {
	[T_SCHEMAS] = "schemas",
	[T_GIO] = "gio",
	[T_PIXBUF] = "pixbuf",
	[T_MIME] = "mime",
	[T_FONTS] = "fonts",
	[T_INFO] = "info",
	[T_HWDB] = "hwdb",
	[T_MAN] = "man",
	[T_XFONTS] = "xfonts",
	[T_TEXMF] = "texmf",
	[T_DESKTOP] = "desktop",
	[T_ICONS] = "icons",
	[T_VLC] = "vlc",
};

static const char *const t_tool[T_COUNT] = {
	[T_SCHEMAS] = "glib-compile-schemas",
	[T_GIO] = "gio-querymodules",
	[T_PIXBUF] = "gdk-pixbuf-query-loaders",
	[T_MIME] = "update-mime-database",
	[T_FONTS] = "fc-cache",
	[T_INFO] = "install-info",
	[T_HWDB] = "udevadm",
	[T_MAN] = "makewhatis",
	[T_XFONTS] = "mkfontdir",
	[T_TEXMF] = "mktexlsr",
	[T_DESKTOP] = "update-desktop-database",
	[T_ICONS] = "gtk-update-icon-cache",
	[T_VLC] = "/usr/lib/vlc/vlc-cache-gen",
};

#define MAN_DIR "usr/share/man/"
#define ICON_DIR "usr/share/icons/"
/* Every theme installed before it has no cache, and its own files touch at
 * most hicolor: the package that places the tool caches every theme. */
#define ICON_TOOL "usr/bin/gtk-update-icon-cache"

/* The theme a path under ICON_DIR belongs to, added to `t->icons` once. */
static void note_theme(KpTriggers *t, const char *p, size_t n)
{
	size_t il = strlen(ICON_DIR);
	const char *name = p + il;
	size_t len = 0;
	while (il + len < n && name[len] != '/')
		len++;
	if (!len)
		return;
	for (size_t at = 0; t->icons.p && at < t->icons.n;) {
		size_t l = strlen(t->icons.p + at);
		if (l == len && !strncmp(t->icons.p + at, name, len))
			return;
		at += l + 1;
	}
	kb_buf_add(&t->icons, name, len);
	kb_buf_add(&t->icons, "", 1);
}

/* Each line of `manifest` that sits under a watched directory marks its
 * index. `gone` marks it as having lost a file too; otherwise a placed
 * manual page is kept for the merge. */
static void note(KpTriggers *t, const char *manifest, int gone)
{
	for (const char *l = manifest; l && *l;) {
		const char *nl = strchr(l, '\n');
		const char *p = l;
		if (!strncmp(p, "./", 2))
			p += 2;
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		if (!gone && n == sizeof(ICON_TOOL) - 1 &&
		    !strncmp(p, ICON_TOOL, n)) {
			t->hit |= 1u << T_ICONS;
			t->icons_all = 1;
		}
		for (int i = 0; i < T_COUNT; i++) {
			for (int d = 0; d < T_MAXDIRS && t_dir[i][d]; d++) {
				size_t dl = strlen(t_dir[i][d]);
				/* The directory entry itself is not a change
				 * to what is in it. */
				if (n <= dl || strncmp(p, t_dir[i][d], dl))
					continue;
				t->hit |= 1u << i;
				if (i == T_ICONS)
					note_theme(t, p, n);
				if (gone)
					t->gone |= 1u << i;
				else if (i == T_MAN && p[n - 1] != '/') {
					size_t ml = strlen(MAN_DIR);
					kb_buf_add(&t->man, p + ml, n - ml);
					kb_buf_add(&t->man, "", 1);
					t->nman++;
				}
			}
		}
		l = nl ? nl + 1 : NULL;
	}
}

void kp_triggers_note(KpTriggers *t, const char *manifest)
{
	note(t, manifest, 0);
}

void kp_triggers_gone(KpTriggers *t, const char *manifest)
{
	note(t, manifest, 1);
}

static int info_page(const char *n)
{
	static const char *const ext[] = { ".info", ".info.gz", ".info.xz",
					   ".info.bz2" };
	size_t nl = strlen(n);
	for (size_t i = 0; i < sizeof(ext) / sizeof(*ext); i++) {
		size_t el = strlen(ext[i]);
		if (nl > el && !strcmp(n + nl - el, ext[i]))
			return 1;
	}
	return 0;
}

/* `dir` is regenerated, not edited: a removed package's entries have no
 * install-info --delete left to run once its files are gone. Split parts
 * (`foo.info-1`) belong to their head file and are not entries. */
static int run_info(const char *root)
{
	char *idir = kb_path_join(root, "usr/share/info");
	char *dirfile = kb_path_join(idir, "dir");
	unlink(dirfile);
	int rc = 0;
	char **names = kb_listdir(idir, NULL);
	for (char **p = names; p && *p; p++) {
		if (!info_page(*p))
			continue;
		char *page = kb_path_join(idir, *p);
		KbArgv a = {0};
		kb_argv_add(&a, t_tool[T_INFO]);
		kb_argv_addf(&a, "--info-dir=%s", idir);
		kb_argv_add(&a, page);
		kb_argv_end(&a);
		if (kb_run(&a) != 0)
			rc = 1;
		free(page);
	}
	kb_strv_free(names);
	free(dirfile);
	free(idir);
	return rc;
}

/* Pages per `makewhatis -d`, under the argument vector's ceiling with room
 * for the tool, the flag and the directory. */
#define MAN_BATCH 200

/* The page names are relative to the manual directory, because makewhatis
 * changes into that directory before it reads a single one.
 *
 * THE DATABASE IS THE ASSERTION, NOT THE STATUS. makewhatis exits non-zero
 * for one unreadable page or dangling link and still writes every other
 * page, so a status alone would warn on every install into a tree that has
 * one bad page. */
static int run_man(const KpTriggers *t, const char *root)
{
	char *mdir = kb_path_join(root, "usr/share/man");
	char *db = kb_path_join(mdir, "mandoc.db");
	int rc = 0;
	if (!kb_is_dir(mdir)) {
		free(db);
		free(mdir);
		return 0;
	}
	if ((t->gone & (1u << T_MAN)) || !t->nman || !kb_path_exists(db)) {
		KbArgv a = {0};
		kb_argv_add(&a, t_tool[T_MAN]);
		kb_argv_add(&a, mdir);
		kb_argv_end(&a);
		rc = kb_run(&a);
	} else {
		const char *p = t->man.p;
		for (int left = t->nman; left > 0;) {
			KbArgv a = {0};
			kb_argv_add(&a, t_tool[T_MAN]);
			kb_argv_add(&a, "-d");
			kb_argv_add(&a, mdir);
			for (int k = 0; k < MAN_BATCH && left > 0; k++, left--) {
				kb_argv_add(&a, p);
				p += strlen(p) + 1;
			}
			kb_argv_end(&a);
			if (kb_run(&a) != 0)
				rc = 1;
		}
	}
	if (rc != 0 && kb_path_exists(db))
		rc = 0;
	free(db);
	free(mdir);
	return rc;
}

/* An X core bitmap face: what `mkfontdir` lists and Xwayland's font path
 * reads. A directory of TrueType faces gets no fonts.dir — fontconfig is what
 * serves those. */
static int x_bitmap_font(const char *n)
{
	static const char *const ext[] = { ".pcf", ".pcf.gz", ".pcf.bz2",
					   ".bdf", ".bdf.gz" };
	size_t nl = strlen(n);
	for (size_t i = 0; i < sizeof(ext) / sizeof(*ext); i++) {
		size_t el = strlen(ext[i]);
		if (nl > el && !strcmp(n + nl - el, ext[i]))
			return 1;
	}
	return 0;
}

/* fonts.dir, per directory of /usr/share/fonts. Several packages install into
 * one directory — font-misc-misc and font-cursor-misc both into `misc/` — so
 * the index lists every package's faces only when it is written here, from
 * the directory; kpkgbuild drops each package's own. A directory left with
 * no face loses its index and, when that empties it, the directory too: an
 * index of nothing is a directory no removal would ever clean up. */
static int run_xfonts(const char *root)
{
	char *top = kb_path_join(root, "usr/share/fonts");
	char **dirs = kb_listdir(top, NULL);
	int rc = 0;
	for (char **d = dirs; d && *d; d++) {
		char *dir = kb_path_join(top, *d);
		if (!kb_is_dir(dir)) {
			free(dir);
			continue;
		}
		int faces = 0;
		char **names = kb_listdir(dir, NULL);
		for (char **p = names; p && *p && !faces; p++)
			faces = x_bitmap_font(*p);
		kb_strv_free(names);
		if (faces) {
			KbArgv a = {0};
			kb_argv_add(&a, t_tool[T_XFONTS]);
			kb_argv_add(&a, dir);
			kb_argv_end(&a);
			if (kb_run(&a) != 0)
				rc = 1;
		} else {
			char *idx = kb_path_join(dir, "fonts.dir");
			if (unlink(idx) == 0)
				rmdir(dir);
			free(idx);
		}
		free(dir);
	}
	kb_strv_free(dirs);
	free(top);
	return rc;
}

/* ls-R, per texmf tree. kpathsea searches the distribution and local trees
 * through ls-R ONLY: a file a later package adds under either is invisible to
 * every TeX program until its tree's ls-R lists it. texmf-var is not watched
 * — nothing a package ships goes there — but its ls-R names the formats, and
 * rewriting all three together costs one run. A tree that does not exist is
 * not named, so the tool never creates one. */
static int run_texmf(const char *root)
{
	static const char *const tree[] = { "usr/share/texmf-dist",
					    "usr/share/texmf-local",
					    "usr/share/texmf-var" };
	char *dirs[sizeof(tree) / sizeof(*tree)];
	int n = 0, rc = 0;
	KbArgv a = {0};

	kb_argv_add(&a, t_tool[T_TEXMF]);
	kb_argv_add(&a, "--quiet");
	for (size_t i = 0; i < sizeof(tree) / sizeof(*tree); i++) {
		char *d = kb_path_join(root, tree[i]);
		if (!kb_is_dir(d)) {
			free(d);
			continue;
		}
		dirs[n++] = d;
		kb_argv_add(&a, d);
	}
	kb_argv_end(&a);
	if (n)
		rc = kb_run(&a);
	for (int i = 0; i < n; i++)
		free(dirs[i]);
	return rc;
}

/* Every directory's mtime at or below `dir`, the directory included, set back
 * to `epoch` where it is later. GTK uses an icon-theme.cache only while it is
 * no older than its theme's directory, and the image is packed with every
 * timestamp at SOURCE_DATE_EPOCH: a directory left at its install time and a
 * cache stamped later both become the epoch, and a directory touched after
 * the cache was written makes every GTK program walk the theme by hand.
 * Clamped before the cache is written, the cache is never the older. */
static void clamp_dirs(const char *dir, time_t epoch)
{
	struct stat st;
	if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
		return;
	char **names = kb_listdir(dir, NULL);
	for (char **p = names; p && *p; p++) {
		char *child = kb_path_join(dir, *p);
		clamp_dirs(child, epoch);
		free(child);
	}
	kb_strv_free(names);
	if (st.st_mtime > epoch) {
		struct timespec ts[2] = { { epoch, 0 }, { epoch, 0 } };
		utimensat(AT_FDCWD, dir, ts, AT_SYMLINK_NOFOLLOW);
	}
}

/* icon-theme.cache, per theme an install or removal touched: each real
 * directory of usr/share/icons among them that holds an index.theme. Only
 * those: hicolor is touched by most applications, and rewriting the large
 * themes beside it on each of those installs costs seconds apiece. `-t`
 * because a cursor theme's index.theme names no icon directories, `-f`
 * because the tool's own staleness test reads the very mtimes clamped above.
 * A directory whose index.theme left with its package loses its cache and,
 * when that empties it, the directory too: a cache of nothing is a file no
 * removal would ever clean up. The install that places the tool itself
 * caches every theme, since each one placed before it has none. */
static int run_icons(const KpTriggers *t, const char *root)
{
	const char *sde = getenv("SOURCE_DATE_EPOCH");
	char *top = kb_path_join(root, "usr/share/icons");
	int rc = 0;
	KbBuf every = {0};
	const KbBuf *themes = &t->icons;
	if (t->icons_all) {
		char **names = kb_listdir(top, NULL);
		for (char **p = names; p && *p; p++)
			kb_buf_add(&every, *p, strlen(*p) + 1);
		kb_strv_free(names);
		themes = &every;
	}
	for (size_t at = 0; themes->p && at < themes->n;) {
		const char *theme = themes->p + at;
		at += strlen(theme) + 1;
		char *dir = kb_path_join(top, theme);
		struct stat st;
		if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
			free(dir);
			continue;
		}
		char *index = kb_path_join(dir, "index.theme");
		if (kb_path_exists(index)) {
			if (sde && *sde)
				clamp_dirs(dir, (time_t)strtoll(sde, NULL, 10));
			KbArgv a = {0};
			kb_argv_add(&a, t_tool[T_ICONS]);
			kb_argv_add(&a, "-q");
			kb_argv_add(&a, "-t");
			kb_argv_add(&a, "-f");
			kb_argv_add(&a, dir);
			kb_argv_end(&a);
			if (kb_run(&a) != 0)
				rc = 1;
			/* Placing the cache touches the theme directory itself,
			 * a second boundary may fall between the two, and the
			 * directory would then be the newer. */
			char *cache = kb_path_join(dir, "icon-theme.cache");
			struct stat cs;
			if (lstat(cache, &cs) == 0 &&
			    lstat(dir, &st) == 0 && st.st_mtime > cs.st_mtime) {
				struct timespec ts[2] = { cs.st_mtim, cs.st_mtim };
				utimensat(AT_FDCWD, dir, ts, AT_SYMLINK_NOFOLLOW);
			}
			free(cache);
		} else {
			char *cache = kb_path_join(dir, "icon-theme.cache");
			if (unlink(cache) == 0)
				rmdir(dir);
			free(cache);
		}
		free(index);
		free(dir);
	}
	kb_buf_free(&every);
	free(top);
	return rc;
}

/* Under `--root`, inside the root: the root's own tool, which writes the
 * cache path it was compiled with, relative to the root it runs in. */
static int run_pixbuf_rooted(const char *root)
{
	KbArgv a = {0};
	kb_argv_add(&a, "chroot");
	kb_argv_add(&a, root);
	kb_argv_add(&a, "/usr/bin/gdk-pixbuf-query-loaders");
	kb_argv_add(&a, "--update-cache");
	kb_argv_end(&a);
	return kb_run(&a);
}

#define VLC_PLUGINS "usr/lib/vlc/plugins"

/* plugins.dat, which libvlc reads each plugin's description from. Without
 * it libvlc loads every plugin at start to learn what it does, the Qt 5
 * interface included, and a Qt 6 program that links libvlc (Elisa) crashes
 * inside it. The vlc package carries no copy, because the cache records each
 * plugin's mtime and packaging resets those. Rooted, inside the root. */
static int run_vlc(const char *root, int rooted)
{
	KbArgv a = {0};
	if (rooted) {
		kb_argv_add(&a, "chroot");
		kb_argv_add(&a, root);
	}
	kb_argv_add(&a, t_tool[T_VLC]);
	kb_argv_add(&a, "/" VLC_PLUGINS);
	kb_argv_end(&a);
	return kb_run(&a);
}

/* A root whose cache tool has gone with vlc keeps nothing to read the cache
 * with: the cache goes, and so do the directories only it held, which no
 * removal would otherwise ever clean up. 1 when the tool is gone. */
static int vlc_gone(const char *root)
{
	char *tool = kb_path_join(root, t_tool[T_VLC] + 1);
	int gone = !kb_path_exists(tool);
	free(tool);
	if (!gone)
		return 0;
	char *dir = kb_path_join(root, VLC_PLUGINS);
	char *cache = kb_path_join(dir, "plugins.dat");
	char *top = kb_path_join(root, "usr/lib/vlc");
	if (unlink(cache) == 0 && rmdir(dir) == 0)
		rmdir(top);
	free(top);
	free(cache);
	free(dir);
	return 1;
}

/* The vector keeps the pointers it is given, so `path` lives until the run
 * is over. */
static int run_one(const KpTriggers *t, int i, const char *root, int rooted)
{
	if (i == T_INFO)
		return run_info(root);
	if (i == T_MAN)
		return run_man(t, root);
	if (i == T_XFONTS)
		return run_xfonts(root);
	if (i == T_TEXMF)
		return run_texmf(root);
	if (i == T_ICONS)
		return run_icons(t, root);
	if (i == T_PIXBUF && rooted)
		return run_pixbuf_rooted(root);
	if (i == T_VLC)
		return run_vlc(root, rooted);

	char *path = NULL;
	KbArgv a = {0};
	kb_argv_add(&a, t_tool[i]);
	switch (i) {
	case T_SCHEMAS:
		path = kb_path_join(root, "usr/share/glib-2.0/schemas");
		break;
	case T_GIO:
		path = kb_path_join(root, "usr/lib/gio/modules");
		break;
	case T_MIME:
		path = kb_path_join(root, "usr/share/mime");
		break;
	case T_DESKTOP:
		/* mimeinfo.cache: the MIME-type-to-application table
		 * that `kdos-appbox open` (xdg-open) and the shell's
		 * Open With read for a type no mimeapps.list names. */
		kb_argv_add(&a, "-q");
		path = kb_path_join(root, "usr/share/applications");
		break;
	case T_PIXBUF:
		kb_argv_add(&a, "--update-cache");
		break;
	case T_FONTS:
		kb_argv_add(&a, "-s");
		if (rooted) {
			kb_argv_add(&a, "--sysroot");
			kb_argv_add(&a, root);
		}
		break;
	case T_HWDB:
		/* The trie goes to <root>/etc/udev/hwdb.bin, the path
		 * libudev reads first and the one the image is built with.
		 * No hwdb.d source left writes an empty trie, which is the
		 * right answer once the last one is removed. */
		kb_argv_add(&a, "hwdb");
		kb_argv_add(&a, "--update");
		if (rooted) {
			kb_argv_add(&a, "--root");
			kb_argv_add(&a, root);
		}
		break;
	}
	/* A removal can take the last file and the directory with it. */
	if (path && !kb_is_dir(path)) {
		free(path);
		return 0;
	}
	if (path)
		kb_argv_add(&a, path);
	kb_argv_end(&a);
	int rc = kb_run(&a);
	free(path);
	return rc;
}

/* Whether the tool is there to run. Rooted, the pixbuf and VLC caches are
 * built by the root's copy, so it is the root that must carry it — and
 * chroot(8) needs root, which is a warning worth printing rather than a skip.
 * vlc_gone has already answered for a root without vlc-cache-gen. */
static int have_tool(int i, const char *root, int rooted)
{
	if (i == T_PIXBUF && rooted) {
		char *q = kb_path_join(root, "usr/bin/gdk-pixbuf-query-loaders");
		int ok = kb_path_exists(q);
		free(q);
		if (ok && geteuid() != 0) {
			kp_msg("Warning: %s under %s needs root, to chroot "
			       "into it; loaders.cache is not rebuilt",
			       t_tool[i], root);
			return 0;
		}
		return ok && kb_have_prog("chroot");
	}
	if (i == T_VLC && rooted) {
		if (geteuid() != 0) {
			kp_msg("Warning: %s under %s needs root, to chroot "
			       "into it; plugins.dat is not rebuilt",
			       t_tool[i], root);
			return 0;
		}
		return kb_have_prog("chroot");
	}
	return kb_have_prog(t_tool[i]);
}

/* `name` as a whole word of the space-separated $KPKG_SKIP_INDEX. */
static int skipped(const char *name)
{
	const char *v = getenv("KPKG_SKIP_INDEX");
	size_t nl = strlen(name);
	for (const char *p = v; p && *p;) {
		while (*p == ' ')
			p++;
		size_t n = strcspn(p, " ");
		if (n == nl && !strncmp(p, name, nl))
			return 1;
		p += n;
	}
	return 0;
}

void kp_triggers_run(KpTriggers *t, const char *root)
{
	int rooted = strcmp(root, "/") != 0;
	for (int i = 0; i < T_COUNT; i++) {
		if (!(t->hit & (1u << i)))
			continue;
		if (skipped(t_name[i])) {
			kp_msg("index skipped: %s", t_name[i]);
			continue;
		}
		if (i == T_VLC && vlc_gone(root))
			continue;
		if (!have_tool(i, root, rooted))
			continue;
		kp_msg("Updating index: %s", t_tool[i]);
		if (run_one(t, i, root, rooted) != 0)
			kp_msg("Warning: %s failed", t_tool[i]);
	}
	kb_buf_free(&t->man);
	kb_buf_free(&t->icons);
	t->icons_all = 0;
	t->nman = 0;
	t->hit = t->gone = 0;
}
