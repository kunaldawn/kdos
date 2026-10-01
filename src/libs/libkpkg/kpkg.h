/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — the package database, the ports tree and the solver
 *
 * Every format and every default in here is a CONTRACT. kpkg is what the
 * whole distro is built with, and the build system parses its output:
 *
 *   - `/var/lib/kpkg/db/<name>` is one file per installed package. Line 1 is
 *     "<version> <release>"; lines 2..N are a verbatim `tar -tf` listing,
 *     every entry `./`-prefixed and every DIRECTORY carrying a trailing `/`.
 *     Removal keys its rmdir-vs-unlink decision off that slash, and four
 *     things on the running system count the files in that directory.
 *   - `PKGDB_DIR=/dev/null` means "resolve against an empty database". Three
 *     callers rely on it (kpkg's own -f, the build driver, mini_build.py)
 *     and it works because `/dev/null/<name>` cannot be a file. Anything here
 *     that stats the DIRECTORY first would silently break all three.
 *   - `PORT_REPO` is a whitespace-separated list of at most KP_MAX_REPOS
 *     repositories, and the first repository holding a name wins. A
 *     repository holds its ports directly (`<repo>/<name>/kpkgbuild`, the
 *     `src/` areas) or one SHELF down (`<repo>/<shelf>/<name>/kpkgbuild`,
 *     `ports/core`); a directory with no `kpkgbuild` in it is a shelf, in
 *     every repository. So a non-port directory in a flat `src/` area (the
 *     kpkg sources in `src/system`) is read as a shelf, and a `kpkgbuild`
 *     one level inside it would be a port. A port is its bare name wherever
 *     it is filed, so inside one repository a name at two paths — two
 *     shelves, or flat and shelved — is an ERROR that names both, never a
 *     silent first-wins. A port nested below its shelf is refused by the
 *     walk (kp_ports_scan) and not found by a lookup (kp_port_find), which
 *     reports it as no such port. Shelves are never listed on `PORT_REPO`:
 *     the list has a fixed length, and one past it is dropped with a
 *     warning.
 *   - Every config value is env-over-file, because the phase env files export
 *     these and expect to win.
 * ---------------------------------
 */

#ifndef KPKG_H
#define KPKG_H

#include "kbase.h"

#define KP_MAX_REPOS 8
/* The shelf names of one repository, NUL-separated. A repository whose
 * shelves outgrow it is listed on every lookup instead of from the cache. */
#define KP_SHELF_BYTES 4096
/* KP_MAX_DEPS bounds ONE recipe's `depends` line: kp_depends stops reading at
 * it, so a recipe naming more loses the rest from its build closure. The
 * largest recipes (vlc, chromium, libreoffice) name 60-75. */
#define KP_MAX_DEPS  128
#define KP_MAX_ORDER 4096

typedef struct {
	char conf[512];		/* $KPKG_CONF, default /etc/kpkg.conf      */
	char root[512];		/* $KPKG_ROOT — alternate install root     */
	char repos[KP_MAX_REPOS][512];
	int nrepos;
	char source_dir[512];
	char package_dir[512];
	char work_dir[512];
	char pkgdb_dir[512];

	/*
	 * $KPKG_STRICT_RECIPE=1 — treat an installed package whose RECIPE has
	 * changed as not installed, so the solver puts it back in the order.
	 *
	 * On KpConf rather than read at one call site because the answer must
	 * be the same everywhere: `kpkg install`, the `kpkgdepends` output the
	 * orchestrator parses, and the build plan all have to agree about what
	 * is installed, or the order disagrees with what the build then does.
	 */
	int strict_recipe;

	/*
	 * Each repository's shelves, sorted, listed once when the repositories
	 * are set, so a lookup stats one path per shelf instead of listing the
	 * repository again. shelf_cached[i] is 0 for a KpConf whose repos were
	 * written by hand, and for a repository whose shelf names outgrow
	 * KP_SHELF_BYTES: both are listed live on every lookup, which is slower
	 * and gives the same answer. A shelf created after the list was taken
	 * is not seen until the repositories are set again.
	 */
	char shelves[KP_MAX_REPOS][KP_SHELF_BYTES];
	int shelf_cached[KP_MAX_REPOS];
} KpConf;

/* Reads the config file, then lets the environment override every value —
 * `${X:-default}` in shell, in that order. */
void kp_conf_load(KpConf *c);

/* Replaces the repositories with a whitespace-separated list and lists their
 * shelves. What kp_conf_load does with $PORT_REPO, for a caller that builds
 * its own list. Returns the number kept; more than KP_MAX_REPOS warns. */
int kp_conf_set_repos(KpConf *c, const char *list);

/* $KPKG_ROOT-prefixed database directory. */
char *kp_db_dir(const KpConf *c);

/* ────────────────────────────────────────────────────────────────────────
 * Ports
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * The directory of port <name>: <repo>/<name> or <repo>/<shelf>/<name> in the
 * first repository holding it. 1 with *dir set (malloc'd), 0 when no
 * repository holds it, -1 when one repository holds it at two paths — `err`
 * then names both. A name with a `/` in it is only tried as a path under each
 * repository, never under a shelf.
 */
int kp_port_find(const KpConf *c, const char *name, char **dir, char *err,
		 size_t errcap);

/* kp_port_find for the callers that read NULL as "no such port". A name filed
 * twice DIES naming both paths: a caller told NULL would skip the port, build
 * without it, or pick one of the two copies, and every one of those is a tree
 * that does not mean what it says. */
char *kp_port_dir(const KpConf *c, const char *name);

/*
 * Every port in every repository, sorted, NULL-terminated; the first
 * repository wins a name two of them hold. Walks each repository at both
 * depths. NULL, with `err` set, when one repository holds a name twice or a
 * port is nested below its shelf. A repository that exists and holds no port
 * at either depth warns: that is a walker that does not match the tree,
 * not an empty one. kb_strv_free the result.
 */
char **kp_ports_scan(const KpConf *c, int *count, char *err, size_t errcap);

/* kp_ports_scan that DIES on a malformed tree, for the same reason
 * kp_port_dir does. Never NULL. */
char **kp_all_ports(const KpConf *c, int *count);

/*
 * `# depends<blank>*:<blank>*` from a recipe, split on SPACES only — a tab
 * inside the list stays part of its token, which is what `tr ' ' '\n'` did.
 * Fills up to KP_MAX_DEPS names; returns how many.
 */
int kp_depends(const char *portdir, char out[][128], int max);

/* One declarative key out of a recipe, verbatim. "" when it has none. The
 * recipe is READ, not run — kp_decl is the authority for anything that needs
 * helper expansion. */
void kp_recipe_key(const char *portdir, const char *key, char *out, size_t cap);

/* The one-line description a recipe declares. "" when it has none. */
void kp_description(const char *portdir, char *out, size_t cap);

/*
 * Version ordering, shared. -1/0/1, tokenised into numeric and alpha runs so
 * 1.10 > 1.9 and 3.6a > 3.6. Two consumers ask the same question of it —
 * `kdos-portup` ("is upstream newer than the pin") and `kdos cve` ("is the pin
 * older than the version that fixed this") — and a second implementation would
 * eventually answer them differently.
 */
int kp_vercmp(const char *a, const char *b);

/*
 * The SHAPE of a version string: digit-runs collapse to 'N', letter-runs to
 * 'a', separators are kept. A string that is ONE digit run records its length
 * ("N8") so a date cannot compare equal in shape to a bare "1". A candidate
 * whose shape differs from the current version's is a different numbering
 * scheme, not a newer release.
 */
void kp_vershape(const char *v, char *out, size_t cap);

/* ────────────────────────────────────────────────────────────────────────
 * Binary-package identity (kp_hash.c)
 *
 * A prebuilt package is usable when three things match: the architecture, the
 * BUILD CONFIG hash and the RECIPE hash. KDOS has no USE flags, so that is the
 * whole of the question Gentoo needs flag matching for.
 * ──────────────────────────────────────────────────────────────────────── */

/* SHA-256 over kpkgbuild, build.sh, postinstall.sh and every .patch, sorted,
 * each contributing its name, its length and its bytes, then over every other
 * file beside them, recursively and sorted, except the files a `sha256 =` line
 * names. A port with no `source =` also hashes `../../libs`. -1 when the
 * directory holds none of the four. */
int kp_recipe_hash(const char *portdir, char out[65]);

/* SHA-256 over arch, libc, target triplet, compiler version and the three flag
 * variables. `human` optionally receives the canonical text that was hashed,
 * which is what a mismatch has to be explained with. */
int kp_buildconfig_hash(char out[65], char *human, size_t hcap);

void kp_arch(char *out, size_t cap);

/* ────────────────────────────────────────────────────────────────────────
 * Database
 * ──────────────────────────────────────────────────────────────────────── */

int kp_installed(const KpConf *c, const char *name);

/*
 * THE RECIPE HASH OF WHAT IS INSTALLED — "does the package on this machine
 * still match the recipe in the tree?"
 *
 * kp_installed() answers only whether a database entry exists, so on its own it
 * skips an installed package whatever its version, its release or its recipe
 * say. A recipe edited on an incremental tree would ship the previously built
 * binary from a build that reports success, which is indistinguishable from a
 * change that did not work.
 *
 * The hash is kp_recipe_hash() — the same SHA-256 over the recipe files and
 * every file beside them that no `sha256 =` names that the binhost's `E:`
 * uses. One definition of "the recipe changed", not two.
 *
 * It lives in a SIDECAR (`<db>/.recipe/<name>`) rather than on the database
 * entry's first line. That line is `"<version> <release>"` and
 * kp_installed_version() splits it on the first space and copies the whole
 * remainder into `rel`, so a third field there would become part of the
 * release string for every caller. The sidecar is also what makes this safe on
 * a tree that predates it: an ABSENT sidecar reads as "unknown", never as
 * "changed", so no package is rebuilt merely for lacking one.
 */
/* Installed AND still matching its recipe (see kp_db.c). The SOLVER's test. */
int kp_installed_current(const KpConf *c, const char *name);

int kp_installed_recipe_hash(const KpConf *c, const char *name, char out[65]);
int kp_record_recipe_hash(const KpConf *c, const char *name, const char *hash);

/*
 * THE PACKAGE-FILE HASH OF WHAT IS INSTALLED: sha256 of the `.tar.xz` kpkgadd
 * installed, in `<db>/.pkgsha/<name>`, written by kpkgadd after the database
 * entry and removed with it. The package store (kp_store.c) keys a port on its
 * dependencies' values. Same contract as `.recipe`: 64 lowercase hex or it
 * reads as absent, and absent is "unknown". The hash covers the compressed
 * bytes, so the same tree packed with the transient and the kept xz preset
 * records two different values.
 */
int kp_installed_pkg_sha(const KpConf *c, const char *name, char out[65]);
int kp_record_pkg_sha(const KpConf *c, const char *name, const char *hash);

/* The merged-/usr aliases of the install root: each top-level name (`bin`,
 * `sbin`, `lib`, `lib64`, `lib32`) that is a symlink to `usr/<same>` there.
 *
 * ONE FILE HAS TWO SPELLINGS through such a link. A package built with
 * `--exec-prefix=` records `./bin/free`, toybox records `./usr/bin/free`, and
 * both name /usr/bin/free. Every ownership question — the conflict scan, the
 * overwrite that moves a path between manifests, the orphan sweep of an
 * upgrade, a removal — compares the CANONICAL spelling, or a shared file is
 * invisible to the scan, claimed twice, and deleted by whichever of the two
 * packages is upgraded or removed next.
 *
 * Read from the root rather than assumed, because a root whose `bin` is a
 * real directory holds two different files under those two names. */
typedef struct {
	int n;
	char from[5][8];
	char to[5][16];
} KpCanon;

void kp_canon_load(const KpConf *c, KpCanon *k);
/* `rel` with a leading alias replaced (`bin/free` -> `usr/bin/free`), newly
 * allocated. A `./` prefix is kept when present, so a stored manifest line
 * and a staged path each come back in their own spelling. */
char *kp_canon_path(const KpCanon *k, const char *rel);

/* Every path claimed by an installed package, sorted, for the conflict scan.
 * `owner[i]` is the package that claims `path[i]`, which is what an overwrite
 * needs: the path has to leave the old owner's manifest, or the file ends up
 * claimed twice and removing either package deletes the other's file.
 * `path[i]` is the CANONICAL spelling (see KpCanon); two packages that claim
 * one file under its two spellings appear as two entries under one key.
 *
 * `owner[i]` is a BORROWED pointer into `ownerv`, one copy per package rather
 * than one per path; it is valid until kp_owned_free and must not be freed or
 * kept after it. `ownerv`/`nowner` belong to the loader alone. */
typedef struct {
	char **path;
	char **owner;
	int n;
	char **ownerv;
	int nowner;
	KpCanon canon;
} KpOwned;

KpOwned *kp_owned_load(const KpConf *c);
/* The same table holding only the claims on `rel[0..n)` (relative, with or
 * without `./`), canonicalised the way kp_owned_owner canonicalises a
 * question. kp_owned_owner and kp_owned_other answer exactly as they would
 * from kp_owned_load for those keys, and NULL for any other. It reads every
 * manifest but keeps a handful of pairs, so a caller that knows which paths it
 * will ask about pays for a scan, not for a million-entry sort. */
KpOwned *kp_owned_load_some(const KpConf *c, char **rel, int n);
/* The package claiming `rel` (`usr/bin/tar`; the database spells it
 * `./usr/bin/tar`), under either spelling of a merged-/usr path, or NULL. A
 * path no package claims is NOT a conflict: the bootstrap phases install tar,
 * musl, binutils and gcc by hand, so those files exist with no database entry,
 * and the self-hosting phase that rebuilds them with kpkg cannot run if an
 * unowned file counts as one. */
const char *kp_owned_owner(const KpOwned *o, const char *rel);
/* A package OTHER than `self` claiming `rel`, or NULL. An upgrade's orphan
 * sweep and a removal ask this before deleting a file: a path the old version
 * listed that another package also claims is that package's file now. */
const char *kp_owned_other(const KpOwned *o, const char *rel,
			   const char *self);
void kp_owned_free(KpOwned *o);

/* Remove `paths` (relative, no `./`) from `pkg`'s manifest, matching either
 * spelling of a merged-/usr path. The version line and every other path are
 * left as they were. Returns the number dropped. */
int kp_db_drop_paths(const KpConf *c, const char *pkg, char *const *paths,
		     int n);
/* THE WRITER LOCK: `<db>/.lock`, flock(LOCK_EX), blocking. Returns the open
 * descriptor, or -1 when the database directory cannot hold one. kpkgadd and
 * kpkgdel take it before they read ownership and release it after the index
 * triggers, so two installs into one root serialise their database edits and
 * their placement instead of each deciding against a table the other is
 * rewriting. One writer at a time: a postinstall hook that calls kpkg waits
 * for the lock its own installer holds, for ever. close() releases it. */
int kp_db_lock(const KpConf *c);

/* Version and release from line 1. Returns 0 when the package is installed. */
int kp_installed_version(const KpConf *c, const char *name, char *ver,
			 size_t vcap, char *rel, size_t rcap);

/* ────────────────────────────────────────────────────────────────────────
 * The package store (kp_store.c)
 *
 * `$KPKG_STORE/<key[0:2]>/<key>/` holds one built package and its META, and a
 * port whose key is there is installed from it instead of being built. The key
 * is SHA-256 over `field=value` lines:
 *
 *   format=  KP_STORE_FORMAT
 *   recipe=  kp_recipe_hash of the port
 *   pack=    kept | transient, the xz preset the package carries
 *   salt=    $KPKG_STORE_SALT, the bootstrap scripts and what they build
 *   env=     sha256 of the sorted environment minus kp_store_env_denied()
 *   dep=     `<name> <pkgsha|->` for every port in the declared transitive
 *            closure and $KPKG_STORE_BASE, sorted
 *
 * An installed closure member with no `.pkgsha` makes the key UNKNOWN: the
 * port builds and nothing is stored. META is K:value lines — P V R F, C (sha256
 * of the file) and one `X:<name> <pkgsha>` per package the port's ELF files
 * link and its closure does not name; a hit needs every X: to match what is
 * installed now. A directory without META is not an entry.
 * ──────────────────────────────────────────────────────────────────────── */

/* Bump whenever build.c changes what a package contains or how it is packed
 * for the same inputs: every stored entry then misses. */
#define KP_STORE_FORMAT 1

/* The environment names the key ignores: job counts, launchers, the store's
 * own knobs, kpkg's run options and the terminal. 1 when `name` is one. */
int kp_store_env_denied(const char *name);

/* The declared transitive closure of `name` (kp_depends, recursively, cycles
 * cut) with the words of $KPKG_STORE_BASE, sorted and unique, `name` itself
 * left out. kb_strv_free the result; never NULL. */
char **kp_store_closure(const KpConf *c, const char *name);

/* The key of `name`, built from `portdir` with the `transient` preset, over
 * `closure`. 0 with `out` set; -1 when the key is unknown (no recipe hash, no
 * $KPKG_STORE_SALT, or an installed closure member without a `.pkgsha`), and
 * `why` says which. */
int kp_store_key(const KpConf *c, const char *name, const char *portdir,
		 int transient, char *const *closure, char out[65], char *why,
		 size_t wcap);

/* DT_NEEDED of an ELF64 little-endian file, read with no process: the
 * sonames, NULL-terminated (kb_strv_free), or NULL for anything that is not
 * such a file or has no dynamic section. */
char **kp_store_needed(const char *path);

/* The `X:<owner> <pkgsha>\n` lines of installed package `name`: the owner of
 * every library its ELF files need, resolved in usr/lib, lib and
 * usr/local/lib, that is not `name` and not in `closure`. Each such owner is
 * also reported as `<name> links <owner> without declaring it`. -1 when an
 * owner has no `.pkgsha`, and then the package must not be stored. */
int kp_store_links(const KpConf *c, const char *name, char *const *closure,
		   KbBuf *x);

/* A usable entry: META present, every X: matching the installed `.pkgsha`,
 * the file present and hashing to C:. 1 with `file` (the package's path) and
 * `sha` set, 0 otherwise. */
int kp_store_lookup(const KpConf *c, const char *store, const char *key,
		    char *file, size_t fcap, char sha[65]);

/* Store `pkgfile` under `key`, replacing any entry there: the package first,
 * META last, each written to `.tmp.<pid>` and renamed. `x` is the X: lines.
 * 0, or -1 with the store left without a META for this key. */
int kp_store_put(const char *store, const char *key, const char *name,
		 const char *pkgfile, const char *x);

/* Mark an entry used: META's mtime is what gc evicts by. */
void kp_store_touch(const char *store, const char *key);

/* Delete entries, least recently used META first, until the store's files
 * total at most `cap` bytes. Directories with no META go first. Returns the
 * number of entries removed, -1 when `store` cannot be read. */
int kp_store_gc(const char *store, unsigned long long cap);

/* KPKG_STORE_SALT: sha256 over the sorted contents of script/phases/00_cross,
 * script/phases/10_bootstrap, script/lib and script/env under `repo`, the
 * recipe hash of every port those scripts name through extract_port_source,
 * get_port_version or port_dir (looked up in `repo`/ports/core), and
 * fs/etc/passwd, group and ld-musl-x86_64.path. -1 when none can be read.
 * Absent or empty in the environment, every kp_store_key() is unknown and each
 * port builds without storing. */
int kp_store_salt(const char *repo, char out[65]);

/* ────────────────────────────────────────────────────────────────────────
 * Solver
 *
 * Depth-first post-order with a global visited set. A cycle terminates
 * silently because a name is marked visited BEFORE its dependencies are
 * walked — the back edge simply finds it already there. A port that does not
 * exist is passed through rather than diagnosed, and fails later at the build
 * step, which is where the message is useful.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char order[KP_MAX_ORDER][128];
	int n;
} KpOrder;

void kp_resolve(const KpConf *c, char **names, int nnames, KpOrder *out);

#endif /* KPKG_H */
