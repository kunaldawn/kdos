/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgadd — install a built package
 *
 *   kpkgadd [--force] [--root <path>] <name-version-release.tar.xz>
 *
 * The filename is the only metadata a package carries, so it is parsed back by
 * stripping suffixes: hyphens are legal in the NAME and not in the version or
 * the release.
 *
 * Five things the shell version got wrong, all fixed here and all of them
 * silent failures rather than loud ones:
 *
 *  - A failed `mv` ran inside `find | while read`, so its `exit 1` only left
 *    the SUBSHELL. The script carried on and wrote a database entry claiming a
 *    complete install of a package that was half on disk.
 *  - The conflict scan iterated `for f in $(find ...)` while the install loop
 *    used `while read -r` — the two disagreed about any path containing a
 *    space. Both come off one list now.
 *  - An upgrade never removed orphans: a file present in the old version and
 *    absent from the new stayed on disk forever, owned by nothing.
 *  - `./.POSTINSTALL` was recorded in the manifest although it is deliberately
 *    never installed, so removing such a package tried to `rm -f /./.POSTINSTALL`.
 *  - `realpath -m` was called only to pretty-print the destination in a log
 *    line, and under `set -e` a failure of it aborted the install. That is the
 *    documented "Symbolic link loop" abort.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

/* <name>-<version>-<release>.tar.xz, taken apart from the right. */
static int split_pkgname(const char *base, char *name, char *ver, char *rel,
			 size_t cap)
{
	char b[512];
	kb_strlcpy(b, base, sizeof(b));

	size_t n = strlen(b);
	if (n > 7 && !strcmp(b + n - 7, ".tar.xz"))
		b[n - 7] = 0;

	char *dash = strrchr(b, '-');
	if (!dash)
		return -1;
	kb_strlcpy(rel, dash + 1, cap);
	*dash = 0;

	dash = strrchr(b, '-');
	if (!dash)
		return -1;
	kb_strlcpy(ver, dash + 1, cap);
	*dash = 0;

	kb_strlcpy(name, b, cap);
	return *name && *ver && *rel ? 0 : -1;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The manifest is `tar -tf` output verbatim — ./-prefixed, directories with a
 * trailing slash — because removal keys off that slash and because a decade of
 * database files are in that shape. `./.POSTINSTALL` is dropped: it is hoisted
 * out before installation and was never a file the package owns. */
static char *manifest(const char *pkgfile, size_t *len)
{
	KbBuf raw = {0};
	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "-tf");
	kb_argv_add(&a, pkgfile);
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &raw) != 0) {
		kb_buf_free(&raw);
		return NULL;
	}
	char *buf = raw.p;

	KbBuf out = {0};
	for (char *line = buf, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strcmp(line, "./.POSTINSTALL"))
			continue;
		kb_buf_printf(&out, "%s\n", line);
	}
	free(buf);
	if (len)
		*len = out.n;
	return out.p;
}

/* Every path in the staged tree, directories first (pre-order), so reversing
 * the list gives children before their parent. */
static void walk(const char *root, const char *rel, KbBuf *dirs, KbBuf *files)
{
	char *dir = *rel ? kb_path_join(root, rel) : kb_strdup(root);
	char **names = kb_listdir(dir, NULL);
	for (char **p = names; p && *p; p++) {
		char *child = *rel ? kb_path_join(rel, *p) : kb_strdup(*p);
		char *full = kb_path_join(dir, *p);
		struct stat st;
		if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode) &&
		    !S_ISLNK(st.st_mode)) {
			kb_buf_printf(dirs, "%s\n", child);
			walk(root, child, dirs, files);
		} else {
			kb_buf_printf(files, "%s\n", child);
		}
		free(child);
		free(full);
	}
	kb_strv_free(names);
	free(dir);
}

static int for_each(const char *blob, int (*fn)(const char *, void *), void *u)
{
	for (const char *line = blob; line && *line;) {
		const char *nl = strchr(line, '\n');
		size_t n = nl ? (size_t)(nl - line) : strlen(line);
		char one[1024];
		if (n < sizeof(one)) {
			memcpy(one, line, n);
			one[n] = 0;
			if (fn(one, u) != 0)
				return -1;
		}
		line = nl ? nl + 1 : NULL;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	const char *stage;
	const char *root;
	int conflicts;
	KbBuf *report;
	const KpOwned *owned;
} Ctx;

static int check_conflict(const char *rel, void *u)
{
	Ctx *x = u;
	char *src = kb_path_join(x->stage, rel);
	char *dst = kb_path_join(x->root, rel);

	struct stat ss, ds;
	int have_dst = lstat(dst, &ds) == 0;
	if (have_dst && lstat(src, &ss) == 0) {
		/* Symlink splice: a link-to-directory landing on a
		 * link-to-directory is the usual /lib -> /usr/lib shape, not a
		 * conflict. It is skipped at install time too. */
		int src_linkdir = S_ISLNK(ss.st_mode) && kb_is_dir(src);
		int dst_linkdir = S_ISLNK(ds.st_mode) && kb_is_dir(dst);

		/* A conflict is between PACKAGES. A file that exists but that
		 * no installed package claims is adopted, not refused.
		 *
		 * That is not a loosening for its own sake — it is what phase
		 * 2 is: `00_toolchain` and `01_phase1` install tar, musl,
		 * binutils and gcc by hand with `make DESTDIR=$SYSROOT
		 * install`, leaving files no database entry owns, and the
		 * self-hosting bootstrap then rebuilds exactly those packages
		 * with kpkg. Refusing them makes the bootstrap impossible.
		 * It used to work because the phase passed a blanket `-f`,
		 * which skipped this scan altogether; `-f` now genuinely
		 * forces a rebuild, so it cannot be handed out just to get
		 * an overwrite. */
		int owned = x->owned && kp_owned_has(x->owned, rel);
		if (owned && !(src_linkdir && dst_linkdir)) {
			x->conflicts++;
			kb_buf_printf(x->report, "\n  %s", rel);
		}
	}
	free(src);
	free(dst);
	return 0;
}

static int mkdirs(const char *rel, void *u)
{
	Ctx *x = u;
	char *dst = kb_path_join(x->root, rel);
	kb_mkdir_p(dst);
	free(dst);
	return 0;
}

static int place(const char *rel, void *u)
{
	Ctx *x = u;
	char *src = kb_path_join(x->stage, rel);
	char *dst = kb_path_join(x->root, rel);
	int rc = 0;

	struct stat ds;
	if (lstat(dst, &ds) == 0 && S_ISLNK(ds.st_mode) && kb_is_dir(dst)) {
		kp_msg("Skipping %s (structure mismatch with host %s)", rel, dst);
	} else if (lstat(dst, &ds) == 0 && S_ISDIR(ds.st_mode) &&
		   !S_ISLNK(ds.st_mode)) {
		kp_err("Conflict: %s is a file in package but a directory on "
		       "system", rel);
	} else {
		kp_msg("Installing %s -> %s", rel, dst);
		if (rename(src, dst) < 0) {
			/* A rename across filesystems cannot work; the staging
			 * dir lives on the target fs precisely so it can. */
			kp_err("Failed to install %s: %s", rel, strerror(errno));
			rc = -1;
		}
	}
	free(src);
	free(dst);
	return rc;
}

/* ──────────────────────────────────────────────────────────────────────── */

int add_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	int force = 0;
	const char *pkgfile = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force"))
			force = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c.root, argv[++i], sizeof(c.root));
		else
			pkgfile = argv[i];
	}

	if (!pkgfile) {
		printf("Usage: kpkgadd [--force] <package.tar.xz>\n");
		return 1;
	}
	if (!kb_path_exists(pkgfile)) {
		kp_err("Package file not found: %s", pkgfile);
		return 1;
	}

	const char *root = c.root[0] ? c.root : "/";
	/* The question is not "am I uid 0" but "can I write here" — that is
	 * what the root check was standing in for, and it is the one that stays
	 * true when a build installs into a sysroot it owns. */
	if (access(root, W_OK) != 0) {
		kp_err("cannot write to %s — run as root", root);
		return 1;
	}

	char name[256], ver[128], rel[128];
	if (split_pkgname(kb_basename(pkgfile), name, ver, rel, sizeof(name))) {
		kp_err("Cannot parse a name-version-release out of %s", pkgfile);
		return 1;
	}

	char *db = kp_db_dir(&c);
	char *dbfile = kb_path_join(db, name);

	char iver[128] = {0}, irel[128] = {0};
	int upgrade = kp_installed_version(&c, name, iver, sizeof(iver), irel,
					   sizeof(irel)) == 0;
	if (upgrade)
		kp_msg("Upgrading %s (%s-%s => %s-%s)", name, iver, irel, ver, rel);
	else
		kp_msg("Installing %s-%s-%s", name, ver, rel);

	/* Staging lives on the TARGET filesystem so that placing a file is a
	 * rename and not a copy. */
	char *vartmp = kb_path_join(root, "var/tmp");
	kb_mkdir_p(vartmp);
	char tmpl[1024];
	snprintf(tmpl, sizeof(tmpl), "%s/kpkgadd.XXXXXX", vartmp);
	free(vartmp);
	if (!mkdtemp(tmpl)) {
		kp_err("cannot create a staging directory: %s", strerror(errno));
		return 1;
	}

	KbArgv x = {0};
	kb_argv_add(&x, "tar");
	kb_argv_add(&x, "-xf");
	kb_argv_add(&x, pkgfile);
	kb_argv_add(&x, "-C");
	kb_argv_add(&x, tmpl);
	kb_argv_end(&x);
	if (kb_run(&x) != 0) {
		kp_err("Failed to extract %s", pkgfile);
		kb_rmtree(tmpl);
		return 1;
	}

	/* The hook is hoisted out of the tree before anything is placed, so it
	 * is never installed and never owned. */
	char *hook = kb_path_join(tmpl, ".POSTINSTALL");
	char *hook_kept = NULL;
	if (kb_path_exists(hook)) {
		char keep[1032];
		snprintf(keep, sizeof(keep), "%s.hook", tmpl);
		if (rename(hook, keep) == 0)
			hook_kept = kb_strdup(keep);
	}
	free(hook);

	KbBuf dirs = {0}, files = {0};
	walk(tmpl, "", &dirs, &files);

	Ctx ctx = { tmpl, root, 0, NULL, NULL };

	if (!upgrade && !force) {
		/* Loaded once: the scan asks it a question per staged file. */
		KpOwned *owned = kp_owned_load(&c);
		ctx.owned = owned;
		KbBuf report = {0};
		ctx.report = &report;
		for_each(files.p, check_conflict, &ctx);
		kp_owned_free(owned);
		ctx.owned = NULL;
		if (ctx.conflicts) {
			kp_err("File conflict detected:%s", report.p);
			kb_rmtree(tmpl);
			return 1;
		}
		kb_buf_free(&report);
	}

	for_each(dirs.p, mkdirs, &ctx);
	if (for_each(files.p, place, &ctx) != 0) {
		kp_err("install of %s aborted; the tree is partially written",
		       name);
		kb_rmtree(tmpl);
		return 1;
	}

	/* An upgrade drops what the old version owned and the new one does not.
	 * Deepest first, so a directory is only removed once it is empty. */
	if (upgrade) {
		size_t on = 0;
		char *old = kb_read_all(dbfile, &on);
		size_t nn = 0;
		char *nw = manifest(pkgfile, &nn);
		if (old && nw) {
			char *body = strchr(old, '\n');
			int lines = 0;
			char *paths[8192];
			for (char *l = body ? body + 1 : old; l && *l;) {
				char *nl = strchr(l, '\n');
				if (nl)
					*nl = 0;
				if (*l && lines < 8192)
					paths[lines++] = l;
				l = nl ? nl + 1 : NULL;
			}
			for (int i = lines - 1; i >= 0; i--) {
				char pat[1100];
				snprintf(pat, sizeof(pat), "%s\n", paths[i]);
				if (strstr(nw, pat))
					continue;
				char *victim = kb_path_join(root, paths[i]);
				size_t vl = strlen(paths[i]);
				if (vl && paths[i][vl - 1] == '/')
					rmdir(victim);
				else if (unlink(victim) == 0)
					kp_msg("Removing orphan %s", paths[i]);
				free(victim);
			}
		}
		free(old);
		free(nw);
	}

	kb_rmtree(tmpl);

	size_t mn = 0;
	char *m = manifest(pkgfile, &mn);
	if (!m) {
		kp_err("cannot list %s", pkgfile);
		return 1;
	}
	kb_mkdir_p(db);
	KbBuf entry = {0};
	kb_buf_printf(&entry, "%s %s\n", ver, rel);
	kb_buf_add(&entry, m, mn);
	kb_write_all(dbfile, entry.p, entry.n);
	kb_buf_free(&entry);
	free(m);

	if (hook_kept) {
		KbArgv h = {0};
		kb_argv_add(&h, hook_kept);
		kb_argv_end(&h);
		setenv("PKG_ROOT", root, 1);
		/* The hook inherits stdio: whatever it prints belongs in the
		 * build log, the same way it did when a shell ran it. */
		if (kb_run_tty(&h) != 0)
			kp_msg("Warning: Postinstall hook failed");
		unlink(hook_kept);
		free(hook_kept);
	}

	kp_msg("Package '%s' installed successfully", name);
	free(dbfile);
	free(db);
	kb_buf_free(&dirs);
	kb_buf_free(&files);
	return 0;
}
