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
 * Five rules this file keeps, each one a SILENT failure when it is broken:
 *
 *  - A file that cannot be placed aborts the install and no database entry is
 *    written. An entry written past a failure claims a complete install of a
 *    package that is half on disk.
 *  - The conflict scan and the install walk come off ONE list. Two walks
 *    gathered separately disagree about any path containing a space.
 *  - An upgrade removes orphans: a file present in the old version and absent
 *    from the new otherwise stays on disk forever, owned by nothing. "Absent"
 *    is judged on the canonical spelling, and a path another package claims
 *    is never an orphan: `./bin/x` in the old version and `./usr/bin/x` in
 *    the new are one file, and deleting it deletes what was just installed.
 *  - `./.POSTINSTALL` is kept out of the manifest. It is deliberately never
 *    installed, so an entry for it has a removal try `rm -f /./.POSTINSTALL`.
 *  - The database is edited under its writer lock (kp_db_lock), taken before
 *    ownership is read and released after the index triggers, on every return.
 *    Two installs deciding conflicts against one table while the other is
 *    rewriting it each place a file the other then claims. A postinstall hook
 *    runs inside the lock, so a hook that calls kpkg never returns.
 *  - Nothing cosmetic may abort the install. A path canonicalisation done only
 *    to pretty-print a destination in a log line fails on a symlink loop, and
 *    an install that dies there dies for a reason that never mattered.
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

/* The manifest is the extraction's own listing: `tar -xpvf` prints to stdout
 * exactly what `tar -tf` prints for the same archive, so the package is
 * decompressed once and the listing is taken from that one pass. It stays
 * verbatim — ./-prefixed, directories with a trailing slash — because removal
 * keys off that slash and a decade of database files are in that shape.
 * `./.POSTINSTALL` is dropped: it is hoisted out before installation and was
 * never a file the package owns. `raw` is consumed. */
static char *manifest(char *raw, size_t *len)
{
	KbBuf out = {0};
	for (char *line = raw, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strcmp(line, "./.POSTINSTALL"))
			continue;
		kb_buf_printf(&out, "%s\n", line);
	}
	free(raw);
	if (!out.p)
		kb_buf_str(&out, "");
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
	/* When the caller allows an overwrite, a conflict is recorded rather
	 * than refused: the path and the package that has to give it up. */
	int take;
	char **taken;
	char **taken_from;
	int ntaken, taken_cap;
} Ctx;

static void take_note(Ctx *x, const char *rel, const char *from)
{
	if (x->ntaken == x->taken_cap) {
		x->taken_cap = x->taken_cap ? x->taken_cap * 2 : 64;
		char **p = kb_calloc((size_t)x->taken_cap, sizeof(*p));
		char **f = kb_calloc((size_t)x->taken_cap, sizeof(*f));
		/* The first growth has no old arrays: memcpy from NULL is
		 * undefined even for zero bytes. */
		if (x->ntaken) {
			memcpy(p, x->taken, (size_t)x->ntaken * sizeof(*p));
			memcpy(f, x->taken_from,
			       (size_t)x->ntaken * sizeof(*f));
		}
		free(x->taken);
		free(x->taken_from);
		x->taken = p;
		x->taken_from = f;
	}
	x->taken[x->ntaken] = kb_strdup(rel);
	x->taken_from[x->ntaken] = kb_strdup(from);
	x->ntaken++;
}

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
		 * That is not a loosening for its own sake — it is what
		 * 20_selfhost is: `00_cross` and `10_bootstrap` install tar,
		 * musl, binutils and gcc by hand with `make DESTDIR=$SYSROOT
		 * install`, leaving files no database entry owns, and the
		 * self-hosting bootstrap then rebuilds exactly those packages
		 * with kpkg. Refusing them makes the bootstrap impossible.
		 * A blanket `-f` from the phase is not the alternative: `-f`
		 * forces a rebuild as well as skipping this scan, so it
		 * cannot be handed out merely to get an overwrite. */
		const char *owner =
			x->owned ? kp_owned_owner(x->owned, rel) : NULL;
		if (owner && !(src_linkdir && dst_linkdir)) {
			if (x->take) {
				take_note(x, rel, owner);
			} else {
				x->conflicts++;
				kb_buf_printf(x->report, "\n  %s", rel);
			}
		}
	}
	free(src);
	free(dst);
	return 0;
}

/* The paths check_conflict can say anything about: those already on disk. A
 * staged path with nothing under it in the root cannot conflict, and on a
 * fresh tree that is every path, so the ownership table is read only for the
 * rest. */
typedef struct {
	const char *root;
	char **v;
	int n, cap;
} Present;

static int collect_present(const char *rel, void *u)
{
	Present *p = u;
	char *dst = kb_path_join(p->root, rel);
	struct stat st;
	if (lstat(dst, &st) == 0) {
		if (p->n == p->cap) {
			p->cap = p->cap ? p->cap * 2 : 64;
			char **nv = kb_calloc((size_t)p->cap, sizeof(*nv));
			if (p->n)
				memcpy(nv, p->v, (size_t)p->n * sizeof(*nv));
			free(p->v);
			p->v = nv;
		}
		p->v[p->n++] = kb_strdup(rel);
	}
	free(dst);
	return 0;
}

static void present_free(Present *p)
{
	for (int i = 0; i < p->n; i++)
		free(p->v[i]);
	free(p->v);
}

static int mkdirs(const char *rel, void *u)
{
	Ctx *x = u;
	char *dst = kb_path_join(x->root, rel);
	kb_mkdir_p(dst);
	free(dst);
	return 0;
}

/* The EXDEV path: reproduce `src` at `dst` rather than moving it. Every type a
 * package can legitimately carry is handled — a regular file loses its mode if
 * copied naively, and a symlink or a device node cannot be copied as bytes at
 * all, which is exactly what a /dev entry is. Returns 0 on success. */
static int copy_across(const char *src, const char *dst)
{
	struct stat st;
	if (lstat(src, &st) != 0)
		return -1;

	unlink(dst);	/* replacing, not merging */

	if (S_ISLNK(st.st_mode)) {
		char target[4096];
		ssize_t n = readlink(src, target, sizeof(target) - 1);
		if (n < 0)
			return -1;
		target[n] = 0;
		return symlink(target, dst);
	}
	if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISFIFO(st.st_mode) ||
	    S_ISSOCK(st.st_mode))
		return mknod(dst, st.st_mode, st.st_rdev);
	if (S_ISDIR(st.st_mode))
		return kb_mkdir_p(dst);

	if (kb_copy_file(src, dst) != 0)
		return -1;
	/* After the bytes, because kb_copy_file creates with the umask — and a
	 * fusermount3 that arrives without its setuid bit is a rootless
	 * fuse-overlayfs that cannot mount. */
	if (chmod(dst, st.st_mode & 07777) != 0)
		return -1;
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
			/* The staging dir is on the target fs so that placing a
			 * file is a rename — but the target fs is not one
			 * filesystem. /dev, /proc, /sys, /run and /tmp are all
			 * separate mounts inside the build chroot, so a package
			 * shipping ANY path under one of them gets EXDEV;
			 * without this copy fallback the install aborts
			 * half-written. libfuse is the real case: its
			 * install_helper mknods a /dev/fuse into DESTDIR. */
			if (errno == EXDEV && copy_across(src, dst) == 0) {
				unlink(src);
			} else {
				kp_err("Failed to install %s: %s", rel,
				       strerror(errno));
				rc = -1;
			}
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

	const char *ov = getenv("KPKG_OVERWRITE");
	int overwrite = ov && *ov && strcmp(ov, "0");

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force"))
			force = 1;
		else if (!strcmp(argv[i], "--overwrite"))
			overwrite = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c.root, argv[++i], sizeof(c.root));
		else if (argv[i][0] == '-' && argv[i][1]) {
			/* An unknown option is rejected rather than falling
			 * through to become the package FILE: taken as one, a
			 * stray flag produces "Package file not found:
			 * --whatever". */
			kp_err("unknown option: %s", argv[i]);
			return 1;
		} else
			pkgfile = argv[i];
	}

	if (!pkgfile) {
		printf("Usage: kpkgadd [--force] [--overwrite] "
		       "<package.tar.xz>\n");
		return 1;
	}
	if (!kb_path_exists(pkgfile)) {
		kp_err("Package file not found: %s", pkgfile);
		return 1;
	}

	/*
	 * The signature, BEFORE anything is unpacked.
	 *
	 * A `<file>.sig` that does not verify stops the install: a bad signature
	 * is a stronger statement than no signature, and treating the two the
	 * same is how signing becomes decoration. An ABSENT sidecar is allowed
	 * by default, because the packages kpkg builds locally are the
	 * overwhelming majority and are never signed — `KPKG_REQUIRE_SIG=1` is
	 * the stricter rule for a machine that only installs from a binhost.
	 */
	const char *req = getenv("KPKG_REQUIRE_SIG");
	int need_sig = req && *req && strcmp(req, "0");
	char signer[KSIG_ID_HEX];
	int vr = kp_verify_package(pkgfile, need_sig, signer);
	if (vr < 0) {
		kp_err("%s: signature check FAILED — not installing", pkgfile);
		return 1;
	}
	if (vr == 0)
		kp_msg("Signature good (key %s)", signer);

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
	/*
	 * `-p` OR EVERY SETUID BIT IN EVERY PACKAGE IS SILENTLY DROPPED.
	 *
	 * GNU tar restores permissions in full only for the superuser unless
	 * it is asked; extracting as an ordinary user applies the umask and
	 * strips setuid and setgid without a word. A build run as a person
	 * rather than as root therefore produced an image whose `su`, `mount`
	 * and `newuidmap` were plain executables — and nothing failed until
	 * something needed the privilege, in a program that had no way to say
	 * why it could not have it.
	 */
	kb_argv_add(&x, "-xpvf");
	kb_argv_add(&x, pkgfile);
	kb_argv_add(&x, "-C");
	kb_argv_add(&x, tmpl);
	kb_argv_end(&x);
	KbBuf listing = {0};
	if (kb_run_capture_buf(&x, &listing) != 0) {
		kp_err("Failed to extract %s", pkgfile);
		kb_buf_free(&listing);
		kb_rmtree(tmpl);
		return 1;
	}
	size_t mn = 0;
	char *m = manifest(listing.p, &mn);

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

	Ctx ctx = { tmpl, root, 0, NULL, NULL, overwrite, NULL, NULL, 0, 0 };

	int lock = kp_db_lock(&c);
	if (lock < 0) {
		kp_err("cannot lock the package database %s: %s", db,
		       strerror(errno));
		kb_rmtree(tmpl);
		return 1;
	}

	/* `--force` skips the scan outright, which is what it has always done.
	 * `--overwrite` still runs it, because the paths it finds are exactly
	 * the ones that have to change hands afterwards. */
	if (!upgrade && !force) {
		Present pr = { root, NULL, 0, 0 };
		for_each(files.p, collect_present, &pr);
		KbBuf report = {0};
		if (pr.n) {
			/* Loaded once, for exactly the paths it is asked
			 * about. */
			KpOwned *owned = kp_owned_load_some(&c, pr.v, pr.n);
			ctx.owned = owned;
			ctx.report = &report;
			for (int i = 0; i < pr.n; i++)
				check_conflict(pr.v[i], &ctx);
			kp_owned_free(owned);
			ctx.owned = NULL;
		}
		present_free(&pr);
		if (ctx.conflicts) {
			kp_err("File conflict detected:%s", report.p);
			kb_rmtree(tmpl);
			close(lock);
			return 1;
		}
		kb_buf_free(&report);
	}

	for_each(dirs.p, mkdirs, &ctx);
	if (for_each(files.p, place, &ctx) != 0) {
		kp_err("install of %s aborted; the tree is partially written",
		       name);
		kb_rmtree(tmpl);
		close(lock);
		return 1;
	}

	KpTriggers trig = {0};

	/* An upgrade drops what the old version owned and the new one does not.
	 * Deepest first, so a directory is only removed once it is empty. */
	if (upgrade) {
		size_t on = 0;
		char *old = kb_read_all(dbfile, &on);
		if (old) {
			/* Both sides canonical, each line fenced by newlines so
			 * that a path matches only as a whole line. */
			KpCanon canon;
			kp_canon_load(&c, &canon);
			KbBuf keep = {0};
			kb_buf_str(&keep, "\n");
			for (char *l = m, *e; l && *l; l = e ? e + 1 : NULL) {
				e = strchr(l, '\n');
				if (e)
					*e = 0;
				char *k = kp_canon_path(&canon, l);
				kb_buf_printf(&keep, "%s\n", k);
				free(k);
				if (e)
					*e = '\n';
			}
			char *body = strchr(old, '\n');
			int lines = 0, pcap = 8192;
			/* Grown, not capped: zig's manifest is 20831 paths and
			 * a fixed ceiling silently leaves the tail owned by a
			 * version that is no longer installed. */
			char **paths = kb_calloc((size_t)pcap, sizeof(*paths));
			for (char *l = body ? body + 1 : old; l && *l;) {
				char *nl = strchr(l, '\n');
				if (nl)
					*nl = 0;
				if (*l) {
					if (lines == pcap) {
						pcap *= 2;
						char **nv = kb_calloc(
							(size_t)pcap,
							sizeof(*nv));
						memcpy(nv, paths,
						       (size_t)lines *
							       sizeof(*nv));
						free(paths);
						paths = nv;
					}
					paths[lines++] = l;
				}
				l = nl ? nl + 1 : NULL;
			}
			/* An orphan is a path of the old version the new one
			 * does not list. Only an orphan FILE is asked about,
			 * so only those claims are read. */
			char *orphan = kb_calloc((size_t)(lines ? lines : 1), 1);
			char **ask = kb_calloc((size_t)(lines ? lines : 1),
					       sizeof(*ask));
			int nask = 0;
			for (int i = 0; i < lines; i++) {
				char *k = kp_canon_path(&canon, paths[i]);
				KbBuf pat = {0};
				kb_buf_printf(&pat, "\n%s\n", k);
				orphan[i] = strstr(keep.p, pat.p) == NULL;
				kb_buf_free(&pat);
				free(k);
				size_t vl = strlen(paths[i]);
				if (orphan[i] && vl && paths[i][vl - 1] != '/')
					ask[nask++] = paths[i];
			}
			KpOwned *owned = kp_owned_load_some(&c, ask, nask);
			for (int i = lines - 1; i >= 0; i--) {
				if (!orphan[i])
					continue;
				char *victim = kb_path_join(root, paths[i]);
				size_t vl = strlen(paths[i]);
				const char *rel = paths[i];
				if (!strncmp(rel, "./", 2))
					rel += 2;
				const char *other = NULL;
				if (vl && paths[i][vl - 1] == '/') {
					if (rmdir(victim) == 0)
						kp_triggers_gone(&trig, paths[i]);
				} else if ((other = kp_owned_other(owned, rel,
								   name))) {
					kp_msg("Keeping %s: %s claims it",
					       paths[i], other);
				} else if (unlink(victim) == 0) {
					kp_msg("Removing orphan %s", paths[i]);
					kp_triggers_gone(&trig, paths[i]);
				}
				free(victim);
			}
			free(orphan);
			free(ask);
			free(paths);
			kb_buf_free(&keep);
			kp_owned_free(owned);
		}
		free(old);
	}

	kb_rmtree(tmpl);

	kp_triggers_note(&trig, m);
	kb_mkdir_p(db);
	KbBuf entry = {0};
	kb_buf_printf(&entry, "%s %s\n", ver, rel);
	kb_buf_add(&entry, m, mn);
	int wrote = kb_write_all(dbfile, entry.p, entry.n);
	kb_buf_free(&entry);
	free(m);

	/* The package file's hash, after the entry and only when it landed:
	 * the package store keys every dependent port on it, and a hash beside
	 * an entry that is not there names an install that did not happen. An
	 * unwritten sidecar reads as unknown, which only costs a store hit. */
	char pkgsha[65];
	if (wrote == 0 && kb_sha256_file(pkgfile, pkgsha) == 0)
		kp_record_pkg_sha(&c, name, pkgsha);

	/* The new owner's entry is written FIRST and the old owners are edited
	 * after: an interruption in between leaves a path claimed twice, which
	 * is recoverable, rather than claimed by nobody, which is not. */
	for (int i = 0; i < ctx.ntaken; i++) {
		if (!ctx.taken_from[i])
			continue;
		const char *from = ctx.taken_from[i];
		char *group[512];
		int gn = 0;
		for (int j = i; j < ctx.ntaken && gn < 512; j++) {
			if (ctx.taken_from[j] && !strcmp(ctx.taken_from[j], from)) {
				group[gn++] = ctx.taken[j];
				if (j != i) {
					free(ctx.taken_from[j]);
					ctx.taken_from[j] = NULL;
				}
			}
		}
		int dropped = kp_db_drop_paths(&c, from, group, gn);
		if (dropped)
			kp_msg("Taking %d path%s from %s", dropped,
			       dropped == 1 ? "" : "s", from);
	}
	for (int i = 0; i < ctx.ntaken; i++) {
		free(ctx.taken[i]);
		free(ctx.taken_from[i]);
	}
	free(ctx.taken);
	free(ctx.taken_from);

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

	kp_triggers_run(&trig, root);
	close(lock);

	kp_msg("Package '%s' installed successfully", name);
	free(dbfile);
	free(db);
	kb_buf_free(&dirs);
	kb_buf_free(&files);
	return 0;
}
