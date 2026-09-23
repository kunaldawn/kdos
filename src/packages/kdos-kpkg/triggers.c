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
 * defines no type until the globs are regenerated. No single package can own
 * that index, so no build.sh can write it, and a per-port postinstall only
 * rebuilds it when that one port is installed — not when the next port adds
 * to it or the last one leaves.
 *
 * So kpkgadd and kpkgdel read the manifest they just acted on, and every
 * index whose directory it touched is rebuilt once from what is now on disk.
 *
 * Rules, each one silent when broken:
 *
 *  - A missing tool is skipped, not an error. The bootstrap installs the
 *    schemas before glib and the fonts before fontconfig; the index is
 *    written when the package carrying the tool arrives, because that
 *    package's own files touch the same directory.
 *  - A failing tool is a warning. The package is already on disk and in the
 *    database; failing the install here would report a half-install that
 *    is not one.
 *  - Every index is rebuilt from the whole directory, never patched with one
 *    package's files — a removal has nothing left to patch with.
 *  - `--root`: tools are handed the root-prefixed directory. The one that
 *    cannot take a directory (the pixbuf loader cache, which writes the
 *    path it was compiled with) runs only against `/`.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

enum {
	T_SCHEMAS,
	T_GIO,
	T_PIXBUF,
	T_MIME,
	T_FONTS,
	T_INFO,
	T_COUNT
};

/* Manifest prefixes, without the `./` every manifest line carries. */
static const char *const t_dir[T_COUNT] = {
	[T_SCHEMAS] = "usr/share/glib-2.0/schemas/",
	[T_GIO] = "usr/lib/gio/modules/",
	[T_PIXBUF] = "usr/lib/gdk-pixbuf-2.0/",
	[T_MIME] = "usr/share/mime/packages/",
	[T_FONTS] = "usr/share/fonts/",
	[T_INFO] = "usr/share/info/",
};

static const char *const t_tool[T_COUNT] = {
	[T_SCHEMAS] = "glib-compile-schemas",
	[T_GIO] = "gio-querymodules",
	[T_PIXBUF] = "gdk-pixbuf-query-loaders",
	[T_MIME] = "update-mime-database",
	[T_FONTS] = "fc-cache",
	[T_INFO] = "install-info",
};

void kp_triggers_note(KpTriggers *t, const char *manifest)
{
	for (const char *l = manifest; l && *l;) {
		const char *nl = strchr(l, '\n');
		const char *p = l;
		if (!strncmp(p, "./", 2))
			p += 2;
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		for (int i = 0; i < T_COUNT; i++) {
			size_t dl = strlen(t_dir[i]);
			/* The directory entry itself is not a change to what
			 * is in it. */
			if (n > dl && !strncmp(p, t_dir[i], dl))
				t->hit |= 1u << i;
		}
		l = nl ? nl + 1 : NULL;
	}
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

/* The vector keeps the pointers it is given, so `path` lives until the run
 * is over. */
static int run_one(int i, const char *root, int rooted)
{
	if (i == T_INFO)
		return run_info(root);
	if (i == T_PIXBUF && rooted)
		return 0;

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

void kp_triggers_run(const KpTriggers *t, const char *root)
{
	int rooted = strcmp(root, "/") != 0;
	for (int i = 0; i < T_COUNT; i++) {
		if (!(t->hit & (1u << i)))
			continue;
		if (!kb_have_prog(t_tool[i]))
			continue;
		kp_msg("Updating index: %s", t_tool[i]);
		if (run_one(i, root, rooted) != 0)
			kp_msg("Warning: %s failed", t_tool[i]);
	}
}
