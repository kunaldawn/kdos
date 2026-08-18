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
 *     callers rely on it (kpkg's own -f, buildlib/phases.py, mini_build.py)
 *     and it works because `/dev/null/<name>` cannot be a file. Anything here
 *     that stats the DIRECTORY first would silently break all three.
 *   - `PORT_REPO` is a whitespace-separated list, first match wins, and a
 *     directory without a `kpkgbuild` in it is invisible.
 *   - Every config value is env-over-file, because the phase env files export
 *     these and expect to win.
 * ---------------------------------
 */

#ifndef KPKG_H
#define KPKG_H

#include "kbase.h"

#define KP_MAX_REPOS 8
#define KP_MAX_DEPS  64
#define KP_MAX_ORDER 2048

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
} KpConf;

/* Reads the config file, then lets the environment override every value —
 * `${X:-default}` in shell, in that order. */
void kp_conf_load(KpConf *c);

/* $KPKG_ROOT-prefixed database directory. */
char *kp_db_dir(const KpConf *c);

/* ────────────────────────────────────────────────────────────────────────
 * Ports
 * ──────────────────────────────────────────────────────────────────────── */

/* First repo holding <name>/kpkgbuild, or NULL. malloc'd. */
char *kp_port_dir(const KpConf *c, const char *name);

/* Every port in the tree, sorted; first repo wins. kb_strv_free the result. */
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

/* ────────────────────────────────────────────────────────────────────────
 * Binary-package identity (kp_hash.c)
 *
 * A prebuilt package is usable when three things match: the architecture, the
 * BUILD CONFIG hash and the RECIPE hash. KDOS has no USE flags, so that is the
 * whole of the question Gentoo needs flag matching for.
 * ──────────────────────────────────────────────────────────────────────── */

/* SHA-256 over kpkgbuild, build.sh, postinstall.sh and every .patch, sorted,
 * each contributing its name, its length and its bytes. -1 when the directory
 * holds none of them. */
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
 * The hash is kp_recipe_hash() — the same SHA-256 over kpkgbuild, build.sh,
 * postinstall.sh and every patch that the binhost's `E:` uses. One definition
 * of "the recipe changed", not two.
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

/* Every path claimed by an installed package, sorted, for the conflict scan.
 * `owner[i]` is the package that claims `path[i]`, which is what an overwrite
 * needs: the path has to leave the old owner's manifest, or the file ends up
 * claimed twice and removing either package deletes the other's file. */
typedef struct {
	char **path;
	char **owner;
	int n;
} KpOwned;

KpOwned *kp_owned_load(const KpConf *c);
int kp_owned_has(const KpOwned *o, const char *rel);
/* The package claiming `rel`, or NULL. */
const char *kp_owned_owner(const KpOwned *o, const char *rel);
void kp_owned_free(KpOwned *o);

/* Remove `paths` (relative, no `./`) from `pkg`'s manifest. The version line
 * and every other path are left as they were. Returns the number dropped. */
int kp_db_drop_paths(const KpConf *c, const char *pkg, char *const *paths,
		     int n);
/* Version and release from line 1. Returns 0 when the package is installed. */
int kp_installed_version(const KpConf *c, const char *name, char *ver,
			 size_t vcap, char *rel, size_t rcap);

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
