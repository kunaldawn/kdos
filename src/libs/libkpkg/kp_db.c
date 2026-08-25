/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — the installed-package database and the solver
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kpkg.h"

/*
 * `[ -f "$PKGDB_DIR/$1" ]` and nothing more. In particular the DIRECTORY is
 * never stat'ed: `PKGDB_DIR=/dev/null` is how three callers ask for "resolve
 * against an empty database", and it works precisely because
 * `/dev/null/<name>` cannot be a regular file. Checking that the directory
 * exists first would answer "yes, it is a character device" and quietly break
 * `kpkg install -f`, the build system's phase resolution and mini_build.
 */
int kp_installed(const KpConf *c, const char *name)
{
	char *db = kp_db_dir(c);
	char *p = kb_path_join(db, name);
	size_t n = 0;
	char *data = kb_read_all(p, &n);
	int yes = data != NULL;
	free(data);
	free(p);
	free(db);
	return yes;
}

/*
 * "Installed, and still matching the recipe in the tree."
 *
 * THE SOLVER USES THIS, NOT kp_installed(), and that placement is load-bearing:
 * kp_resolve() drops an installed package before the install loop ever sees it,
 * so a test further downstream reaches only packages named on the command line
 * and misses every DEPENDENCY whose recipe has changed.
 *
 * With strict_recipe off this is kp_installed() and nothing else.
 */
int kp_installed_current(const KpConf *c, const char *name)
{
	if (!kp_installed(c, name))
		return 0;
	if (!c->strict_recipe)
		return 1;

	char *dir = kp_port_dir(c, name);
	if (!dir)
		return 1;	/* no port to compare against: not our business */

	char want[65], have[65];
	int rc = kp_recipe_hash(dir, want);
	free(dir);
	if (rc != 0)
		return 1;	/* cannot hash the recipe: unknown, so leave it */
	if (kp_installed_recipe_hash(c, name, have) != 0)
		return 1;	/* no sidecar: UNKNOWN, never "changed" */

	return strcmp(want, have) == 0;
}

int kp_installed_version(const KpConf *c, const char *name, char *ver,
			 size_t vcap, char *rel, size_t rcap)
{
	char *db = kp_db_dir(c);
	char *p = kb_path_join(db, name);
	char line[256];
	int rc = kb_read_line_file(p, line, sizeof(line));
	free(p);
	free(db);
	if (rc < 0)
		return -1;

	ver[0] = rel[0] = 0;
	char *sp = strchr(line, ' ');
	if (sp) {
		*sp = 0;
		kb_strlcpy(rel, sp + 1, rcap);
	}
	kb_strlcpy(ver, line, vcap);
	return 0;
}

/*
 * The recipe-hash sidecar. See kpkg.h for why this is a separate file rather
 * than a third field on the database entry's first line.
 *
 * Returns 0 and fills `out` when a hash is recorded, -1 when it is not. -1 is
 * "unknown", and every caller must treat it as "do not know, so do not force a
 * rebuild": a tree that predates the sidecar has none, and reading their
 * absence as "changed" rebuilds the entire userland.
 */
int kp_installed_recipe_hash(const KpConf *c, const char *name, char out[65])
{
	char *db = kp_db_dir(c);
	char *dir = kb_path_join(db, ".recipe");
	char *p = kb_path_join(dir, name);
	char line[128];
	int rc = kb_read_line_file(p, line, sizeof(line));
	free(p);
	free(dir);
	free(db);
	if (rc < 0)
		return -1;

	/* A sidecar that is not exactly 64 hex characters is CORRUPT, and
	 * corrupt reads as absent rather than as a hash that can never match.
	 * Treating it as a mismatch would rebuild that one package on every
	 * run for ever, with nothing saying why. */
	size_t n = strlen(line);
	if (n != 64)
		return -1;
	for (size_t i = 0; i < n; i++) {
		char ch = line[i];
		if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
			return -1;
	}
	memcpy(out, line, 65);
	out[64] = 0;
	return 0;
}

int kp_record_recipe_hash(const KpConf *c, const char *name, const char *hash)
{
	if (!hash || strlen(hash) != 64)
		return -1;
	char *db = kp_db_dir(c);
	char *dir = kb_path_join(db, ".recipe");
	kb_mkdir_p(dir);
	char *p = kb_path_join(dir, name);
	char line[66];
	snprintf(line, sizeof(line), "%s\n", hash);
	/* kb_write_all is 0/-1, not a truth value. */
	int rc = kb_write_all(p, line, strlen(line));
	free(p);
	free(dir);
	free(db);
	return rc;
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char seen[KP_MAX_ORDER][128];
	int nseen;
	KpOrder *out;
	const KpConf *c;
} Walk;

static int contains(char list[][128], int n, const char *want)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(list[i], want))
			return 1;
	return 0;
}

static void mark(Walk *w, const char *name)
{
	if (w->nseen < KP_MAX_ORDER)
		kb_strlcpy(w->seen[w->nseen++], name, 128);
}

static void emit(Walk *w, const char *name)
{
	if (!contains(w->out->order, w->out->n, name) &&
	    w->out->n < KP_MAX_ORDER)
		kb_strlcpy(w->out->order[w->out->n++], name, 128);
}

static void walk(Walk *w, const char *name)
{
	char *dir = kp_port_dir(w->c, name);
	if (!dir)
		return;		/* unknown port: no deps, and it still ships */

	char deps[KP_MAX_DEPS][128];
	int n = kp_depends(dir, deps, KP_MAX_DEPS);
	free(dir);

	for (int i = 0; i < n; i++) {
		if (contains(w->seen, w->nseen, deps[i]))
			continue;
		if (kp_installed_current(w->c, deps[i]))
			continue;
		/* Marked BEFORE the recursion, which is what makes a cycle
		 * terminate instead of overflowing the stack. */
		mark(w, deps[i]);
		walk(w, deps[i]);
		emit(w, deps[i]);
	}
}

void kp_resolve(const KpConf *c, char **names, int nnames, KpOrder *out)
{
	Walk w;
	memset(&w, 0, sizeof(w));
	w.out = out;
	w.c = c;
	out->n = 0;

	for (int i = 0; i < nnames; i++) {
		if (contains(w.seen, w.nseen, names[i]))
			continue;
		if (kp_installed_current(c, names[i]))
			continue;
		mark(&w, names[i]);
		walk(&w, names[i]);
		emit(&w, names[i]);
	}
}
