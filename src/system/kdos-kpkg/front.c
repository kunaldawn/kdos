/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkg — build and install from ports
 *
 * `kpkg install -f` FORCES ONLY WHAT WAS NAMED. Dependencies pulled in behind
 * a forced package keep the ordinary skip-if-installed behaviour. That is not
 * a nicety: kdosbuild passes `-f` for exactly the ports a build plan selected,
 * and a blanket force would rebuild all ~350 packages on every run.
 *
 * kpkgdepends drops anything already installed, which would make `-f` resolve
 * to nothing at all, so under `-f` resolution runs against an EMPTY database
 * and the decision about what to actually rebuild is taken here, per package.
 *
 * `--build-only` and `--commit` split one install in two, so builds that do
 * not depend on each other can run side by side while installs stay one at a
 * time. `--build-only` resolves nothing: a dependency that is not installed
 * and current is an error naming it, never a build. What it built is recorded
 * in `<PACKAGE_DIR>/<name>.pending` — line 1 the package's path, line 2 the
 * recipe hash taken at build start, line 3 `kept` or `transient` for the xz
 * preset it was packed with, line 4 the package-store key taken before the
 * build (empty when the store is off or the key unknown), further lines
 * ignored — and `--commit` installs from exactly that record.
 *
 * THE PACKAGE STORE ($KPKG_STORE, kp_store.c) sits between the skip and the
 * build. A port that is not forced and whose key has a usable entry is
 * installed from the store (`Reusing <pkg> from the store (<key12>)`), or,
 * under --build-only, copied into PACKAGE_DIR as if built. Otherwise it
 * builds, and once installed its package is stored under the key computed
 * BEFORE the build. With $KPKG_STORE_CHECK set a hit builds anyway, the two
 * packages are compared, and a difference is logged to
 * `<store>/../logs/pkgstore-check.log` and the entry replaced. No store
 * failure fails an install; each is a warning.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

#define VERSION "1.0-kdos"

static void usage(void)
{
	printf("kpkg version %s - Minimal package manager for KDOS\n"
	       "\n"
	       "Usage: kpkg <command> [options]\n"
	       "\n"
	       "Options:\n"
	       "  --root <path>      Operate on a different root directory\n"
	       "  --keep-cache       Keep cached packages after installation\n"
	       "  -f, --force        Rebuild the named packages, skip the\n"
	       "                     file-conflict scan for them\n"
	       "  --overwrite        Let a package take a path another package\n"
	       "                     owns; ownership moves with the file\n"
	       "  --build-only       (install) Build exactly the named ports into\n"
	       "                     the package cache; install nothing. Every\n"
	       "                     dependency must already be installed\n"
	       "  --commit           (install) Install what --build-only built\n"
	       "                     for each named port, in the order named\n"
	       "\n"
	       "Commands:\n"
	       "  install <pkg>...   Install package(s) with dependencies\n"
	       "  remove <pkg>...    Remove package(s)\n"
	       "  update             Update all installed packages\n"
	       "  list               List installed packages\n"
	       "  info <pkg>         Show package information\n"
	       "  meta <pkg>         Print the recipe's metadata as shell\n"
	       "                     assignments, for ports/fetch to eval\n"
	       "  verify <pkg>       Build with kpkgbuild and kpkgbuild.new,\n"
	       "                     then compare the two packages\n"
	       "  verify --repro <pkg>  Build the SAME recipe twice; the two\n"
	       "                     packages must be byte-identical\n"
	       "  keygen <name>      Make an Ed25519 signing key pair\n"
	       "  index <dir>        Write PACKAGES for a directory of packages\n"
	       "                     (--sign <key> signs it and every package)\n"
	       "  verify-index <dir> Check PACKAGES against the trusted keys\n"
	       "  verify-pkg <file>  Check <file>.sig against the trusted keys\n"
	       "  delta <old> <new>  Write the difference between two packages\n"
	       "  apply-delta <old> <delta> -o <new>  Rebuild a package from one\n"
	       "  binhost <dir> <p>  Install the prebuilt package, if arch, build\n"
	       "                     config and recipe hash all match this machine\n"
	       "  store gc <dir> <max-size>  Evict package-store entries, least\n"
	       "                     recently used first, down to max-size\n"
	       "                     (bytes, or K/M/G/T)\n"
	       "  help               Show this help message\n"
	       "\n"
	       "Examples:\n"
	       "  kpkg install bash\n"
	       "  kpkg install --root /mnt/target bash\n"
	       "  kpkg remove bash\n"
	       "  kpkg list\n", VERSION);
}

/* ──────────────────────────────────────────────────────────────────────── */

static int install_pkgfile(const KpConf *c, const char *file, int force,
			   int overwrite)
{
	char *args[8];
	int n = 0;
	args[n++] = (char *)"kpkgadd";
	if (force)
		args[n++] = (char *)"--force";
	if (overwrite)
		args[n++] = (char *)"--overwrite";
	if (c->root[0]) {
		args[n++] = (char *)"--root";
		args[n++] = (char *)c->root;
	}
	args[n++] = (char *)file;
	args[n] = NULL;
	return add_main(n, args);
}

/* ──────────────────────────────────────────────────────────────────────── */

static int report_diff(FILE *out, const char *a, const char *b);
static char *fingerprint_of(const char *pkgfile);

/* $KPKG_STORE, or NULL when the store is off. */
static const char *store_dir(void)
{
	const char *s = getenv("KPKG_STORE");
	return s && *s ? s : NULL;
}

static int store_checking(void)
{
	const char *s = getenv("KPKG_STORE_CHECK");
	return s && *s && strcmp(s, "0");
}

/* The key of `pkg` before its build, or "" when the store is off or the key
 * is unknown; the reason for an unknown key is printed. */
static void store_key(const KpConf *c, const char *pkg, const char *dir,
		      int transient, char key[65])
{
	key[0] = '\0';
	if (!store_dir())
		return;
	char **closure = kp_store_closure(c, pkg);
	char why[300];
	if (kp_store_key(c, pkg, dir, transient, closure, key, why,
			 sizeof(why)) != 0) {
		key[0] = '\0';
		kp_msg("%s: store key unknown (%s); building, not storing",
		       pkg, why);
	}
	kb_strv_free(closure);
}

/* A usable store entry for `key` that this run may install from: not in
 * check mode, which always builds. */
static int store_hit(const KpConf *c, const char *key, char *file, size_t cap)
{
	char sha[65];
	if (!key[0] || store_checking())
		return 0;
	return kp_store_lookup(c, store_dir(), key, file, cap, sha);
}

/* `<PACKAGE_DIR>/<basename of file>`, a copy of the stored package. */
static int store_copy_out(const KpConf *c, const char *file, char *out,
			  size_t cap)
{
	kb_mkdir_p(c->package_dir);
	char *dst = kb_path_join(c->package_dir, kb_basename(file));
	int rc = kb_copy_file(file, dst);
	kb_strlcpy(out, dst, cap);
	free(dst);
	return rc;
}

/* Check mode found `stored` where a build made `built`: one line, then the
 * first members that differ. */
static void store_log_mismatch(const char *pkg, const char *key,
			       const char *stored, const char *ssha,
			       const char *built, const char *bsha)
{
	char *parent = kb_strdup(store_dir());
	char *sl = strrchr(parent, '/');
	if (sl)
		*sl = '\0';
	char *logs = kb_path_join(sl && parent[0] ? parent : ".", "logs");
	kb_mkdir_p(logs);
	char *log = kb_path_join(logs, "pkgstore-check.log");
	FILE *f = fopen(log, "a");
	kp_msg("%s: the store holds different bytes for %.12s (see %s)", pkg,
	       key, log);
	if (f) {
		fprintf(f, "%s %.12s stored=%s built=%s\n", pkg, key, ssha,
			bsha);
		char *a = fingerprint_of(stored);
		char *b = fingerprint_of(built);
		if (a && b)
			report_diff(f, a, b);
		free(a);
		free(b);
		fclose(f);
	}
	free(log);
	free(logs);
	free(parent);
}

/*
 * After `pkg` is installed from `file`: store it under `key`, taken before the
 * build. An entry already holding the same bytes is only touched; one holding
 * different bytes is logged in check mode and replaced. An undeclared link
 * whose owner has no `.pkgsha` cannot be guarded, so nothing is stored.
 */
static void store_after(const KpConf *c, const char *pkg, const char *key,
			const char *file)
{
	const char *store = store_dir();
	if (!store || !key[0])
		return;
	char **closure = kp_store_closure(c, pkg);
	KbBuf x = {0};
	int lrc = kp_store_links(c, pkg, closure, &x);
	kb_strv_free(closure);
	if (lrc != 0) {
		kp_msg("%s: links a package with no .pkgsha; not stored", pkg);
		kb_buf_free(&x);
		return;
	}
	char bsha[65], sfile[1100], ssha[65];
	if (kb_sha256_file(file, bsha) != 0) {
		kb_warn("%s: cannot hash %s; not stored", pkg, file);
		kb_buf_free(&x);
		return;
	}
	if (kp_store_lookup(c, store, key, sfile, sizeof(sfile), ssha)) {
		if (!strcmp(ssha, bsha)) {
			kp_store_touch(store, key);
			kb_buf_free(&x);
			return;
		}
		if (store_checking())
			store_log_mismatch(pkg, key, sfile, ssha, file, bsha);
	}
	if (kp_store_put(store, key, pkg, file, x.p ? x.p : "") != 0)
		kb_warn("%s: cannot store %s under %.12s", pkg, file, key);
	kb_buf_free(&x);
}

/* `kpkg store gc <dir> <max-size>`. */
static int cmd_store(int argc, char **argv)
{
	if (argc != 3 || strcmp(argv[0], "gc")) {
		printf("Usage: kpkg store gc <dir> <max-size>\n");
		return 1;
	}
	char *end = NULL;
	unsigned long long cap = strtoull(argv[2], &end, 10);
	if (end == argv[2]) {
		kp_err("not a size: %s", argv[2]);
		return 1;
	}
	int shift = 0;
	switch (*end) {
	case 'T': case 't': shift = 40; end++; break;
	case 'G': case 'g': shift = 30; end++; break;
	case 'M': case 'm': shift = 20; end++; break;
	case 'K': case 'k': shift = 10; end++; break;
	default: break;
	}
	if (*end) {
		kp_err("not a size: %s", argv[2]);
		return 1;
	}
	cap <<= shift;
	if (!kb_is_dir(argv[1])) {
		kp_msg("No package store at %s", argv[1]);
		return 0;
	}
	int n = kp_store_gc(argv[1], cap);
	if (n < 0) {
		kp_err("cannot read %s", argv[1]);
		return 1;
	}
	kp_msg("Evicted %d store entr%s from %s", n, n == 1 ? "y" : "ies",
	       argv[1]);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* `<PACKAGE_DIR>/<name>.pending`. PACKAGE_DIR is where kpkgbuild wrote the
 * package, whatever --root says, so the record sits beside the file it names. */
static char *pending_path(const KpConf *c, const char *name)
{
	char base[160];
	snprintf(base, sizeof(base), "%s.pending", name);
	return kb_path_join(c->package_dir, base);
}

/*
 * Read a `.pending` record: line 1 into `pkg`, line 2 into `hash` ("" when the
 * build had none), into `*kept` whether line 3 says `kept`, and line 4 into
 * `key` ("" when there is none). A record with no third line reads as
 * transient: only a package known to carry the kept preset may stay in a kept
 * cache. Lines after the fourth are ignored. -1 when the record is absent,
 * unreadable or names no package.
 */
static int pending_read(const KpConf *c, const char *name, char *pkg,
			size_t pcap, char hash[65], int *kept, char key[65])
{
	char *p = pending_path(c, name);
	char *data = kb_read_whole(p, NULL);
	free(p);
	pkg[0] = hash[0] = key[0] = '\0';
	*kept = 0;
	if (!data)
		return -1;
	char *line[4] = { data, NULL, NULL, NULL };
	for (int i = 0; i < 4 && line[i]; i++) {
		char *e = strchr(line[i], '\n');
		if (e) {
			*e = '\0';
			if (i < 3)
				line[i + 1] = e + 1;
		}
	}
	kb_strlcpy(pkg, line[0], pcap);
	if (line[1] && strlen(line[1]) == 64)
		kb_strlcpy(hash, line[1], 65);
	*kept = line[2] && !strcmp(line[2], "kept");
	if (line[3] && strlen(line[3]) == 64)
		kb_strlcpy(key, line[3], 65);
	free(data);
	return pkg[0] ? 0 : -1;
}

/*
 * `kpkg install --build-only <port>...` — build each named port, in the order
 * named, and install nothing.
 *
 * NOTHING IS RESOLVED. The caller has already ordered the work and decides
 * which ports build together, so a dependency that is not installed and
 * current means the caller's order is wrong: building it here would put a
 * second build of it beside the caller's, and building without it would link
 * against whatever the tree happens to hold. It is an error that names it.
 *
 * A record that names an existing package built from the SAME recipe hash is
 * the build already done: an interrupted run resumes without building again.
 * Under a kept cache the record must also say `kept`: a transient package is
 * xz -0, not the bytes kpkgbuild makes, and must not become the binhost's
 * copy, so it is built again with the kept preset.
 *
 * A store hit is copied into PACKAGE_DIR and recorded exactly as a build
 * would be; the key goes on line 4 for --commit to store under.
 */
static int build_only(KpConf *c, char **want, int nwant, int force,
		      int keep_cache)
{
	for (int i = 0; i < nwant; i++) {
		const char *pkg = want[i];
		char *dir = kp_port_dir(c, pkg);
		if (!dir) {
			kp_err("Port not found: %s", pkg);
			return 1;
		}
		if (!force && kp_installed_current(c, pkg)) {
			kp_msg("Skipping %s (already installed)", pkg);
			free(dir);
			continue;
		}

		char deps[KP_MAX_DEPS][128];
		int nd = kp_depends(dir, deps, KP_MAX_DEPS);
		for (int d = 0; d < nd; d++) {
			if (!kp_installed_current(c, deps[d])) {
				kp_err("%s: dependency %s is not installed and "
				       "current; --build-only builds nothing "
				       "it was not named", pkg, deps[d]);
				free(dir);
				return 1;
			}
		}

		char h[65];
		if (kp_recipe_hash(dir, h) != 0)
			h[0] = '\0';

		char prev[1100], ph[65], pkey[65];
		int pkept;
		if (h[0] &&
		    pending_read(c, pkg, prev, sizeof(prev), ph, &pkept,
				 pkey) == 0 &&
		    !strcmp(ph, h) && kb_path_exists(prev) &&
		    (pkept || !keep_cache)) {
			kp_msg("%s: already built (%s)", pkg, prev);
			free(dir);
			continue;
		}

		char key[65], sfile[1100], built[1100];
		store_key(c, pkg, dir, !keep_cache, key);
		if (!force && store_hit(c, key, sfile, sizeof(sfile)) &&
		    store_copy_out(c, sfile, built, sizeof(built)) == 0) {
			kp_msg("Reusing %s from the store (%.12s)", pkg, key);
			kp_store_touch(store_dir(), key);
			free(dir);
		} else {
			kp_msg(force ? "Rebuilding %s (forced)..."
				     : "Building %s...", pkg);
			kp_pack_transient = !keep_cache;
			int rc = kp_build_port(dir, built, sizeof(built));
			kp_pack_transient = 0;
			free(dir);
			if (rc != 0) {
				kp_err("Failed to build %s", pkg);
				return 1;
			}
			if (!built[0]) {
				kp_err("no package produced for %s", pkg);
				return 1;
			}
		}

		/* Written only once the package is complete, and replaced whole:
		 * a record is either absent or names a finished package. */
		KbBuf rec = {0};
		kb_buf_printf(&rec, "%s\n%s\n%s\n%s\n", built, h,
			      keep_cache ? "kept" : "transient", key);
		char *p = pending_path(c, pkg);
		int wr = kb_write_file_atomic(p, rec.p);
		kb_buf_free(&rec);
		if (wr != 0) {
			kp_err("cannot write %s", p);
			free(p);
			return 1;
		}
		kp_msg("%s: built %s", pkg, built);
		free(p);
	}
	return 0;
}

/*
 * `kpkg install --commit <port>...` — install what `--build-only` recorded for
 * each named port, one at a time, in the order named: the caller's order is
 * the dependency order, and a later package's hooks may need an earlier one.
 *
 * kpkgadd holds the database's writer lock for the install itself; the recipe
 * hash is recorded under the same lock, taken once the install has returned
 * it. Nesting the two would wait for ever (kp_db_lock). A name with no record
 * stops the run: a port the caller believes is built and is not must not read
 * as installed. A package the record calls transient is deleted after its
 * install even under a kept cache: the cache keeps only kept-preset bytes.
 * A record carrying a store key has its package stored under it once
 * installed.
 */
static int commit(KpConf *c, char **want, int nwant, int force, int overwrite,
		  int keep_cache)
{
	for (int i = 0; i < nwant; i++) {
		const char *pkg = want[i];
		char file[1100], h[65], key[65];
		int kept;
		if (pending_read(c, pkg, file, sizeof(file), h, &kept, key) !=
		    0) {
			kp_err("%s: nothing built to commit (no %s.pending in "
			       "%s)", pkg, pkg, c->package_dir);
			return 1;
		}
		if (!kb_path_exists(file)) {
			kp_err("%s: %s.pending names %s, which does not exist",
			       pkg, pkg, file);
			return 1;
		}
		if (install_pkgfile(c, file, force, overwrite) != 0) {
			kp_err("Failed to install %s", pkg);
			return 1;
		}
		if (h[0]) {
			int lock = kp_db_lock(c);
			kp_record_recipe_hash(c, pkg, h);
			if (lock >= 0)
				close(lock);
		}
		store_after(c, pkg, key, file);
		if (!keep_cache || !kept) {
			kp_msg("Removing cached package %s...", file);
			unlink(file);
		}
		char *p = pending_path(c, pkg);
		unlink(p);
		free(p);
		/* The one line a caller reads to know the port is in. */
		printf("kpkg: committed %s\n", pkg);
		fflush(stdout);
	}
	return 0;
}

static int cmd_install(KpConf *c, int argc, char **argv)
{
	int force = 0, keep_cache = 0, build_only_mode = 0, commit_mode = 0;
	char *want[KP_MAX_ORDER];
	int nwant = 0;

	/* Env over flag, the same way PORT_REPO and PKGDB_DIR work. The
	 * orchestrator sets it rather than passing a flag, because a restored
	 * snapshot can put an OLDER kpkg in the tree and an unknown env var is
	 * ignored by every version while an unknown flag is not. */
	const char *ov = getenv("KPKG_OVERWRITE");
	int overwrite = ov && *ov && strcmp(ov, "0");
	/* The same, for a build that keeps every package it makes to write a
	 * binhost from. */
	const char *kc = getenv("KPKG_KEEP_CACHE");
	keep_cache = kc && *kc && strcmp(kc, "0");

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force"))
			force = 1;
		else if (!strcmp(argv[i], "--overwrite"))
			overwrite = 1;
		else if (!strcmp(argv[i], "--keep-cache"))
			keep_cache = 1;
		else if (!strcmp(argv[i], "--build-only"))
			build_only_mode = 1;
		else if (!strcmp(argv[i], "--commit"))
			commit_mode = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c->root, argv[++i], sizeof(c->root));
		else if (argv[i][0] == '-' && argv[i][1]) {
			/* An unknown option is rejected rather than falling
			 * through to the package list: `kpkg install
			 * --overwrite zig` on a kpkg that predates the flag
			 * would otherwise report `Port not found: --overwrite`
			 * — a message that names the flag but blames the
			 * ports tree. */
			kp_err("unknown option: %s", argv[i]);
			return 1;
		} else if (nwant < KP_MAX_ORDER)
			want[nwant++] = argv[i];
	}

	if (!nwant) {
		printf("Usage: kpkg install <package>...\n");
		return 1;
	}
	if (build_only_mode && commit_mode) {
		kp_err("--build-only and --commit are two separate runs");
		return 1;
	}
	if (build_only_mode)
		return build_only(c, want, nwant, force, keep_cache);
	if (commit_mode)
		return commit(c, want, nwant, force, overwrite, keep_cache);

	kp_msg("Resolving dependencies...");

	KpConf resolve = *c;
	if (force)
		kb_strlcpy(resolve.pkgdb_dir, "/dev/null",
			   sizeof(resolve.pkgdb_dir));

	KpOrder order;
	kp_resolve(&resolve, want, nwant, &order);
	if (!order.n) {
		kp_msg("Nothing to do");
		return 0;
	}

	KbBuf list = {0};
	for (int i = 0; i < order.n; i++)
		kb_buf_printf(&list, "%s%s", i ? " " : "", order.order[i]);
	kp_msg("Packages to install: %s", list.p);
	kb_buf_free(&list);

	for (int i = 0; i < order.n; i++) {
		const char *pkg = order.order[i];

		/* Forced only if it was NAMED. A dependency dragged in behind
		 * one keeps the ordinary behaviour. */
		int forced = 0;
		if (force)
			for (int j = 0; j < nwant; j++)
				if (!strcmp(want[j], pkg))
					forced = 1;

		char *dir = kp_port_dir(c, pkg);
		if (!dir) {
			kp_err("Port not found: %s", pkg);
			return 1;
		}

		/*
		 * The guard for a package that became installed between the
		 * resolve and now.
		 *
		 * IT MUST ASK THE SAME QUESTION THE SOLVER ASKED.
		 * kp_installed_current() is that question — installed AND still
		 * matching its recipe. Using plain kp_installed() here would
		 * skip exactly the packages the solver deliberately put in the
		 * order because their recipe changed, and the run would report
		 * "Packages to install: <name>" and then build nothing.
		 */
		if (!forced && kp_installed_current(c, pkg)) {
			kp_msg("Skipping %s (already installed)", pkg);
			free(dir);
			continue;
		}
		/*
		 * The recipe hash is taken HERE, before the build starts, and
		 * recorded only after the install succeeds. It is the hash of
		 * the recipe this build ran from: an edit made to the port
		 * while it builds leaves the record naming the older recipe,
		 * so the next run sees a mismatch and rebuilds it. The store
		 * key is taken here for the same reason.
		 */
		char h[65];
		int have_hash = kp_recipe_hash(dir, h) == 0;
		char key[65], sfile[1100];
		store_key(c, pkg, dir, !keep_cache, key);
		if (!forced && store_hit(c, key, sfile, sizeof(sfile))) {
			free(dir);
			kp_msg("Reusing %s from the store (%.12s)", pkg, key);
			if (install_pkgfile(c, sfile, 0, overwrite) != 0) {
				kp_err("Failed to install %s", pkg);
				return 1;
			}
			if (have_hash)
				kp_record_recipe_hash(c, pkg, h);
			kp_store_touch(store_dir(), key);
			char kept[1100];
			if (keep_cache &&
			    store_copy_out(c, sfile, kept, sizeof(kept)) != 0)
				kb_warn("%s: cannot copy %s into %s", pkg,
					sfile, c->package_dir);
			continue;
		}
		kp_msg(forced ? "Rebuilding %s (forced)..." : "Building %s...",
		       pkg);
		/* A package that is deleted right after kpkgadd reads it is
		 * compressed with the cheap preset; a kept one never is. */
		kp_pack_transient = !keep_cache;
		char built[1100];
		int rc = kp_build_port(dir, built, sizeof(built));
		kp_pack_transient = 0;
		free(dir);
		if (rc != 0) {
			kp_err("Failed to build %s", pkg);
			return 1;
		}
		if (!built[0]) {
			kp_err("no package produced for %s", pkg);
			return 1;
		}
		/* The file this build wrote, by its exact path. A prefix scan
		 * of PACKAGE_DIR would also match `<pkg>-<other>-...`, and
		 * with a kept cache it picks whichever sorts last. */
		char *found = kb_strdup(built);

		/* Unlike -f, --overwrite applies to every package in the order:
		 * it is a property of the RUN, not of what was named. A phase
		 * that rebuilds the userland has toybox and GNU sed both
		 * claiming /usr/bin/sed, and whichever comes last wins. */
		if (install_pkgfile(c, found, forced, overwrite) != 0) {
			kp_err("Failed to install %s", pkg);
			if (!keep_cache)
				unlink(found);
			free(found);
			return 1;
		}
		/*
		 * Recorded AFTER the install succeeded, never before: a
		 * sidecar written ahead of a build that then fails would claim
		 * a recipe is installed that is not, and the next run would
		 * skip it. Failure to write is not fatal — the sidecar is an
		 * optimisation hint, and its absence reads as "unknown", which
		 * is the safe direction.
		 */
		if (have_hash)
			kp_record_recipe_hash(c, pkg, h);
		store_after(c, pkg, key, found);

		if (!keep_cache) {
			kp_msg("Removing cached package %s...", found);
			unlink(found);
		}
		free(found);
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * `kpkg verify <port>` — build the port with its current recipe AND with the
 * `kpkgbuild.new` / `build.sh.new` sitting beside it, then diff the two
 * packages. The claim being checked is "the new recipe produces the same
 * package", and nothing weaker is worth having when the recipe changes.
 *
 * Neither build happens in the port directory. A scratch directory is filled
 * with symlinks to everything the port carries — tarballs, patches, vendor
 * bundles — and only the files being TESTED are real. The ports tree is never
 * written to, so an interrupted verify leaves nothing behind.
 */
static int stage_recipe(const char *portdir, const char *recipe, char *out,
			size_t cap)
{
	char tmpl[512];
	snprintf(tmpl, sizeof(tmpl), "/tmp/kpkg-verify.XXXXXX");
	if (!mkdtemp(tmpl))
		return -1;
	kb_strlcpy(out, tmpl, cap);

	char **names = kb_listdir(portdir, NULL);
	for (char **n = names; n && *n; n++) {
		/* The recipe and its build script are copied, not linked, so
		 * the `.new` variants can stand in for them. */
		if (!strcmp(*n, "kpkgbuild") || !strcmp(*n, "kpkgbuild.new") ||
		    !strcmp(*n, "build.sh") || !strcmp(*n, "build.sh.new"))
			continue;
		char *src = kb_path_join(portdir, *n);
		char *dst = kb_path_join(tmpl, *n);
		if (symlink(src, dst) != 0)
			kb_warn("cannot link %s", *n);
		free(src);
		free(dst);
	}
	kb_strv_free(names);

	char *from = kb_path_join(portdir, recipe);
	char *to = kb_path_join(tmpl, "kpkgbuild");
	int rc = kb_copy_file(from, to);
	free(from);
	free(to);
	if (rc != 0)
		return rc;

	/* The build script travels with the recipe: `kpkgbuild.new` is tested
	 * against `build.sh.new` when there is one, and against the current
	 * build.sh when the change is metadata-only. */
	int is_new = strcmp(recipe, "kpkgbuild") != 0;
	char *bs = NULL;
	if (is_new) {
		bs = kb_path_join(portdir, "build.sh.new");
		if (!kb_path_exists(bs)) {
			free(bs);
			bs = NULL;
		}
	}
	if (!bs)
		bs = kb_path_join(portdir, "build.sh");
	to = kb_path_join(tmpl, "build.sh");
	rc = kb_path_exists(bs) ? kb_copy_file(bs, to) : 0;
	free(bs);
	free(to);
	return rc;
}

/*
 * A package's FINGERPRINT: one line per member, `mode  sha256  path`, sorted.
 *
 * A file list answers "are the same files there" and stops. That caught the
 * error it was built for — a missing or extra file — and it is silent about the
 * one that matters more once recipes start changing: the same paths with
 * different CONTENT. Hashing means the answer covers the payload, the modes and
 * the symlink targets, which together are everything an installed package is.
 *
 * The archive is unpacked to compare it. Streaming each member out of tar
 * separately would be one process per file, and a package like zig has 20 000.
 */
static void fingerprint_walk(const char *root, const char *rel, KbBuf *out)
{
	char *dir = *rel ? kb_path_join(root, rel) : kb_strdup(root);
	char **names = kb_listdir(dir, NULL);

	/* kb_listdir returns readdir order, which is not sorted and not stable;
	 * the fingerprint is a comparison key, so it has to be. */
	for (char **a = names; a && *a; a++)
		for (char **b = a + 1; *b; b++)
			if (strcmp(*a, *b) > 0) {
				char *t = *a;
				*a = *b;
				*b = t;
			}

	for (char **n = names; n && *n; n++) {
		char sub[1024];
		snprintf(sub, sizeof(sub), "%s%s%s", rel, *rel ? "/" : "", *n);
		char *path = kb_path_join(dir, *n);
		struct stat st;

		if (lstat(path, &st) != 0) {
			free(path);
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			kb_buf_printf(out, "%04o  dir%54s  %s\n",
				      st.st_mode & 07777, "", sub);
			fingerprint_walk(root, sub, out);
		} else if (S_ISLNK(st.st_mode)) {
			char tgt[1024] = {0};
			ssize_t k = readlink(path, tgt, sizeof(tgt) - 1);
			if (k > 0)
				tgt[k] = 0;
			kb_buf_printf(out, "%04o  link -> %-49s  %s\n",
				      st.st_mode & 07777, tgt, sub);
		} else {
			char hash[65] = "unreadable";
			kb_sha256_file(path, hash);
			kb_buf_printf(out, "%04o  %s  %s\n",
				      st.st_mode & 07777, hash, sub);
		}
		free(path);
	}
	kb_strv_free(names);
	free(dir);
}

static char *fingerprint_of(const char *pkgfile)
{
	char tmpl[] = "/tmp/kpkg-fp.XXXXXX";
	if (!mkdtemp(tmpl))
		return NULL;

	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "-xf");
	kb_argv_add(&a, pkgfile);
	kb_argv_add(&a, "-C");
	kb_argv_add(&a, tmpl);
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		kb_rmtree(tmpl);
		return NULL;
	}

	KbBuf out = {0};
	fingerprint_walk(tmpl, "", &out);
	kb_rmtree(tmpl);
	return out.p ? out.p : kb_strdup("");
}

/* The first few lines that differ, which is what a person needs to see.
 * Returns how many were written. */
static int report_diff(FILE *out, const char *a, const char *b)
{
	const char *pa = a, *pb = b;
	int shown = 0;

	while ((pa || pb) && shown < 10) {
		char la[1024] = "", lb[1024] = "";
		const char *na = pa ? strchr(pa, '\n') : NULL;
		const char *nb = pb ? strchr(pb, '\n') : NULL;
		size_t ka = pa ? (na ? (size_t)(na - pa) : strlen(pa)) : 0;
		size_t kb = pb ? (nb ? (size_t)(nb - pb) : strlen(pb)) : 0;

		if (ka >= sizeof(la))
			ka = sizeof(la) - 1;
		if (kb >= sizeof(lb))
			kb = sizeof(lb) - 1;
		if (pa)
			memcpy(la, pa, ka);
		if (pb)
			memcpy(lb, pb, kb);
		if (strcmp(la, lb)) {
			fprintf(out, "  - %s\n  + %s\n",
				la[0] ? la : "(absent)",
				lb[0] ? lb : "(absent)");
			shown++;
		}
		pa = na ? na + 1 : NULL;
		pb = nb ? nb + 1 : NULL;
		if (!pa && !pb)
			break;
	}
	return shown;
}

/* `kpkg meta` — the recipe's fields as shell assignments, single-quoted.
 * A recipe is not a shell script, so ports/fetch cannot source one for its
 * fields and asks through this subcommand instead. Accepts a port NAME or a
 * directory, because fetch walks directories. */
static int cmd_meta(const KpConf *c, const char *who)
{
	char path[1040];
	char *dir = NULL;

	if (strchr(who, '/') || kb_is_dir(who))
		snprintf(path, sizeof(path), "%s/kpkgbuild", who);
	else {
		dir = kp_port_dir(c, who);
		if (!dir) {
			kp_err("no port named %s", who);
			return 1;
		}
		snprintf(path, sizeof(path), "%s/kpkgbuild", dir);
		free(dir);
	}

	KpDecl *d = kp_decl_parse(path);
	if (!d) {
		kp_err("%s: not a recipe", path);
		return 1;
	}
	kp_decl_meta(d);
	kp_decl_free(d);
	return 0;
}

/*
 * `kpkg verify [--repro] <port>`.
 *
 * Two questions, one machine:
 *
 *   default   build with `kpkgbuild` and with `kpkgbuild.new`, and ask whether
 *             the recipe change altered the package
 *   --repro   build the SAME recipe twice, and ask whether the package depends
 *             on anything but the recipe — the clock, the filesystem's readdir
 *             order, the builder's uid, the umask
 *
 * The second is the acceptance test for reproducible packages, and it is the
 * same code path because it is the same comparison: two archives, one
 * fingerprint each.
 */
static int build_and_keep(const char *portdir, const char *recipe, int slot,
			  char **out)
{
	char stage[512];

	if (stage_recipe(portdir, recipe, stage, sizeof(stage)) != 0) {
		kp_err("cannot stage %s", recipe);
		return 1;
	}
	kp_msg("Building with %s...", recipe);
	char path[1100];
	if (kp_build_port(stage, path, sizeof(path)) != 0) {
		kp_err("build with %s failed", recipe);
		kb_rmtree(stage);
		return 1;
	}
	kb_rmtree(stage);

	if (!path[0]) {
		kp_err("no package produced by %s", recipe);
		return 1;
	}
	char *built = kb_strdup(path);
	char keep[600];
	snprintf(keep, sizeof(keep), "/tmp/kpkg-verify-%d.tar.xz", slot);
	if (rename(built, keep) != 0 && kb_copy_file(built, keep) != 0) {
		kp_err("cannot set aside %s", built);
		free(built);
		return 1;
	}
	unlink(built);
	free(built);
	*out = kb_strdup(keep);
	return 0;
}

static int cmd_verify(const KpConf *c, const char *name, int repro)
{
	char *portdir = kp_port_dir(c, name);
	if (!portdir) {
		kp_err("Port not found: %s", name);
		return 1;
	}

	const char *which[2] = { "kpkgbuild",
				 repro ? "kpkgbuild" : "kpkgbuild.new" };
	if (!repro) {
		char *newp = kb_path_join(portdir, "kpkgbuild.new");
		if (!kb_path_exists(newp)) {
			kp_err("%s has no kpkgbuild.new to verify against",
			       name);
			free(newp);
			free(portdir);
			return 1;
		}
		free(newp);
	}

	char *pkg[2] = { NULL, NULL };
	for (int i = 0; i < 2; i++)
		if (build_and_keep(portdir, which[i], i, &pkg[i]) != 0) {
			free(pkg[0]);
			free(portdir);
			return 1;
		}

	/*
	 * Byte equality first, because for --repro that is the whole claim and
	 * for a recipe change it is a stronger answer than any comparison of
	 * contents: identical archives cannot differ in anything at all.
	 */
	char ha[65] = "", hb[65] = "";
	int identical = kb_sha256_file(pkg[0], ha) == 0 &&
			kb_sha256_file(pkg[1], hb) == 0 && !strcmp(ha, hb);

	char *a = fingerprint_of(pkg[0]);
	char *b = fingerprint_of(pkg[1]);
	int same_payload = a && b && !strcmp(a, b);

	printf("\n");
	if (identical) {
		kp_msg("%s: the two packages are BYTE-IDENTICAL (%.16s...)",
		       name, ha);
	} else if (same_payload) {
		/* Every file the same, the archive not: the metadata around
		 * them moved. That is a reproducibility bug in the tar
		 * invocation, not in the recipe, and it is worth saying so
		 * rather than reporting success. */
		kp_err("%s: same payload, DIFFERENT archive — the packaging is "
		       "not reproducible", name);
		printf("  %s  %s\n  %s  %s\n", ha, pkg[0], hb, pkg[1]);
	} else {
		kp_err("%s: the payloads DIFFER", name);
		if (a && b)
			report_diff(stdout, a, b);
		printf("  old: %s\n  new: %s\n", pkg[0], pkg[1]);
	}
	free(a);
	free(b);
	free(pkg[0]);
	free(pkg[1]);
	free(portdir);
	return identical ? 0 : same_payload && !repro ? 0 : 1;
}

/*
 * `--json` is offered on `list` and `info` and NOWHERE ELSE in kpkg, which is
 * deliberate rather than incomplete.
 *
 * `kpkgdepends` prints one bare space-separated line that the build orchestrator
 * parses, and `kpkg meta` prints shell assignments that `ports/fetch` evals.
 * Both are load-bearing formats with existing consumers; a flag near them is a
 * flag someone eventually passes. These two commands are the ones that only
 * ever report to a person, so they are the two that can safely gain a second
 * shape.
 */
static int cmd_list(const KpConf *c, int json)
{
	char *db = kp_db_dir(c);
	char **v = kb_listdir(db, NULL);
	/* A dot-name is not a package: `.recipe/` holds sidecars and `.lock` is
	 * the writer lock, and an empty `.lock` reads as a version line. */
	int keep = 0;
	for (char **p = v; p && *p; p++) {
		if ((*p)[0] == '.')
			free(*p);
		else
			v[keep++] = *p;
	}
	if (v)
		v[keep] = NULL;
	int width = 0;
	for (char **p = v; p && *p; p++) {
		int n = (int)strlen(*p);
		if (n > width)
			width = n;
	}

	if (json) {
		KbBuf b = {0};
		int first = 1;
		kb_buf_str(&b, "{\n  \"packages\": [");
		for (char **p = v; p && *p; p++) {
			char ver[128], rel[128];
			if (kp_installed_version(c, *p, ver, sizeof(ver), rel,
						 sizeof(rel)) != 0)
				continue;
			kb_buf_printf(&b, "%s\n    {\"name\": ", first ? "" : ",");
			kb_json_str(&b, *p);
			kb_buf_str(&b, ", \"version\": ");
			kb_json_str(&b, ver);
			kb_buf_str(&b, ", \"release\": ");
			kb_json_str(&b, rel);
			kb_buf_str(&b, "}");
			first = 0;
		}
		kb_buf_printf(&b, "%s  ]\n}\n", first ? "" : "\n");
		fwrite(b.p, 1, b.n, stdout);
		kb_buf_free(&b);
		kb_strv_free(v);
		free(db);
		return 0;
	}

	for (char **p = v; p && *p; p++) {
		char ver[128], rel[128];
		if (kp_installed_version(c, *p, ver, sizeof(ver), rel,
					 sizeof(rel)) == 0)
			printf("%-*s  %s-%s\n", width, *p, ver, rel);
	}
	kb_strv_free(v);
	free(db);
	return 0;
}

static int cmd_info(const KpConf *c, const char *name, int json)
{
	char ver[128], rel[128];
	if (kp_installed_version(c, name, ver, sizeof(ver), rel, sizeof(rel)) == 0) {
		char *db = kp_db_dir(c);
		char *f = kb_path_join(db, name);
		size_t n = 0;
		char *data = kb_read_all(f, &n);
		int files = 0;
		for (char *p = data; p && *p; p++)
			if (*p == '\n')
				files++;
		free(data);
		free(f);
		free(db);
		files = files > 0 ? files - 1 : 0;
		if (json) {
			KbBuf b = {0};
			kb_buf_str(&b, "{\"name\": ");
			kb_json_str(&b, name);
			kb_buf_str(&b, ", \"installed\": true, \"version\": ");
			kb_json_str(&b, ver);
			kb_buf_str(&b, ", \"release\": ");
			kb_json_str(&b, rel);
			kb_buf_printf(&b, ", \"files\": %d}\n", files);
			fwrite(b.p, 1, b.n, stdout);
			kb_buf_free(&b);
			return 0;
		}
		printf("Package: %s\nVersion: %s-%s\nFiles: %d\n", name, ver, rel,
		       files);
		return 0;
	}

	char *dir = kp_port_dir(c, name);
	if (!dir) {
		/* An error stays on stderr and out of the JSON: a consumer that
		 * gets a parse failure and a non-zero exit knows more than one
		 * that gets a well-formed object describing nothing. */
		kp_err("Package not found or not installed: %s", name);
		return 1;
	}

	/* The description is READ from the recipe. Printing the queried name
	 * in its place has every uninstalled package describe itself as its
	 * own name and leaves the recipe's `description` parsed by nothing at
	 * all. */
	char desc[256];
	kp_description(dir, desc, sizeof(desc));

	char deps[KP_MAX_DEPS][128];
	int nd = kp_depends(dir, deps, KP_MAX_DEPS);

	if (json) {
		KbBuf b = {0};
		kb_buf_str(&b, "{\"name\": ");
		kb_json_str(&b, name);
		kb_buf_str(&b, ", \"installed\": false, \"description\": ");
		kb_json_str(&b, desc[0] ? desc : NULL);
		kb_buf_str(&b, ", \"depends\": [");
		for (int i = 0; i < nd; i++) {
			if (i)
				kb_buf_str(&b, ", ");
			kb_json_str(&b, deps[i]);
		}
		kb_buf_str(&b, "]}\n");
		fwrite(b.p, 1, b.n, stdout);
		kb_buf_free(&b);
		free(dir);
		return 0;
	}

	printf("Package: %s\n", name);
	if (desc[0])
		printf("Description: %s\n", desc);
	if (nd) {
		printf("Depends:");
		for (int i = 0; i < nd; i++)
			printf(" %s", deps[i]);
		putchar('\n');
	}
	printf("Status: not installed\n");
	free(dir);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int front_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	if (argc < 2) {
		usage();
		return 1;
	}

	const char *cmd = argv[1];
	int rest = argc - 2;
	char **restv = argv + 2;

	if (!strcmp(cmd, "install") || !strcmp(cmd, "i"))
		return cmd_install(&c, rest, restv);

	if (!strcmp(cmd, "remove") || !strcmp(cmd, "r")) {
		char *args[KP_MAX_ORDER + 4];
		int n = 0;
		args[n++] = (char *)"kpkgdel";
		for (int i = 0; i < rest && n < KP_MAX_ORDER; i++)
			args[n++] = restv[i];
		args[n] = NULL;
		return del_main(n, args);
	}

	if (!strcmp(cmd, "list") || !strcmp(cmd, "l")) {
		int json = 0;
		for (int i = 0; i < rest; i++) {
			if (!strcmp(restv[i], "--root") && i + 1 < rest)
				kb_strlcpy(c.root, restv[++i], sizeof(c.root));
			else if (!strcmp(restv[i], "--json"))
				json = 1;
		}
		return cmd_list(&c, json);
	}

	if (!strcmp(cmd, "meta")) {
		if (!rest) {
			printf("Usage: kpkg meta <package|portdir>\n");
			return 1;
		}
		return cmd_meta(&c, restv[0]);
	}

	if (!strcmp(cmd, "keygen"))
		return kp_cmd_keygen(rest, restv);
	if (!strcmp(cmd, "index"))
		return kp_cmd_index(&c, rest, restv);
	if (!strcmp(cmd, "delta"))
		return kp_cmd_delta(rest, restv);
	if (!strcmp(cmd, "apply-delta"))
		return kp_cmd_apply_delta(rest, restv);
	if (!strcmp(cmd, "verify-pkg"))
		return kp_cmd_verify_pkg(rest, restv);
	if (!strcmp(cmd, "verify-index"))
		return kp_cmd_verify_index(rest, restv);
	if (!strcmp(cmd, "binhost"))
		return kp_cmd_binhost(&c, rest, restv);
	if (!strcmp(cmd, "store"))
		return cmd_store(rest, restv);

	if (!strcmp(cmd, "verify")) {
		int repro = 0;
		const char *who = NULL;
		for (int i = 0; i < rest; i++) {
			if (!strcmp(restv[i], "--repro"))
				repro = 1;
			else if (!who)
				who = restv[i];
		}
		if (!who) {
			printf("Usage: kpkg verify [--repro] <package>\n");
			return 1;
		}
		return cmd_verify(&c, who, repro);
	}

	if (!strcmp(cmd, "info")) {
		const char *who = NULL;
		int json = 0;
		for (int i = 0; i < rest; i++) {
			if (!strcmp(restv[i], "--root") && i + 1 < rest)
				kb_strlcpy(c.root, restv[++i], sizeof(c.root));
			else if (!strcmp(restv[i], "--json"))
				json = 1;
			else if (!who)
				who = restv[i];
		}
		if (!who) {
			printf("Usage: kpkg info [--json] <package>\n");
			return 1;
		}
		return cmd_info(&c, who, json);
	}

	if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") ||
	    !strcmp(cmd, "-h")) {
		usage();
		return 0;
	}

	usage();
	return 1;
}
