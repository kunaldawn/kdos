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

/* The one-line description a recipe declares. "" when it has none. */
void kp_description(const char *portdir, char *out, size_t cap);

/* ────────────────────────────────────────────────────────────────────────
 * Database
 * ──────────────────────────────────────────────────────────────────────── */

int kp_installed(const KpConf *c, const char *name);

/* Every path claimed by an installed package, sorted, for the conflict scan. */
typedef struct {
	char **path;
	int n;
} KpOwned;

KpOwned *kp_owned_load(const KpConf *c);
int kp_owned_has(const KpOwned *o, const char *rel);
void kp_owned_free(KpOwned *o);
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
