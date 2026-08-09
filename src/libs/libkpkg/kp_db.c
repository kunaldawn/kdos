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
		if (kp_installed(w->c, deps[i]))
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
		if (kp_installed(c, names[i]))
			continue;
		mark(&w, names[i]);
		walk(&w, names[i]);
		emit(&w, names[i]);
	}
}
