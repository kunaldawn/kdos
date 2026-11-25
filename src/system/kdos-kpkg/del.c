/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgdel — remove installed packages
 *
 *   kpkgdel [--root <path>] <package>...
 *
 * The manifest is walked in REVERSE, so a directory is only reached after
 * everything inside it: an entry ending in `/` is a directory and gets rmdir
 * (which fails harmlessly while it still holds files another package owns)
 * unless another installed package lists the directory too, which keeps an
 * empty directory a package ships (hicolor's icon sizes, a plugin directory)
 * when a package that also listed it goes; anything else gets unlink — unless another installed package claims the
 * same file, under either spelling of a merged-/usr path, in which case the
 * file is that package's and stays.
 *
 * There is deliberately no reverse-dependency check. `kpkgdel bash` will
 * remove bash. That is the distro this is.
 *
 * The entry goes with its sidecars (`.recipe/<name>`, `.pkgsha/<name>`,
 * `.files/<name>`, `.facets/<name>`, `.needs/<name>`): one left behind
 * describes a package that is not there, and the next install of that name
 * inherits it.
 *
 * The whole run holds the database's writer lock (kp_db_lock), from the first
 * ownership read to the last index trigger: an install that scans for
 * conflicts while a removal is rewriting the table decides against files that
 * are about to be gone.
 *
 * A removed package leaves the world set and the origin record
 * (`<root>/var/lib/kdos/world`, `<root>/var/lib/kdos/origin`, kdos update):
 * a name left in the world set is installed again by the next update, which
 * takes the world set as what somebody asked for. Each file is rewritten
 * whole, once per run, and only when a name was removed and the file is
 * there; a name that was not installed stays in both.
 *
 * The removal reaches this root alone. The other A/B slot is a root of its
 * own: an update that has filled it installed the package there too, and its
 * arm gave it a copy of the world set and the origin record, so a removal
 * after the arm leaves the package installed and listed in the slot that
 * boots next, and every update from that slot keeps it. It is removed there
 * by running the removal again once that slot is running.
 *
 * A name that is not installed is reported and skipped, never fatal. A check
 * inside the loop with an `exit 1` makes `kpkgdel a bogus c` remove `a`, then
 * stop and never touch `c` — while reporting only that `bogus` was not
 * installed.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

void kp_dirclaims_load(const KpConf *c, KpDirClaims *d)
{
	memset(d, 0, sizeof(*d));
	kp_canon_load(c, &d->canon);
	char *db = kp_db_dir(c);
	char **names = kb_listdir(db, NULL);
	int cap = 4096, n = 0;
	char **all = malloc((size_t)cap * sizeof(*all));
	for (char **p = names; p && *p && all; p++) {
		if ((*p)[0] == '.')
			continue;
		char *f = kb_path_join(db, *p);
		char *data = kb_read_all(f, NULL);
		free(f);
		char *body = data ? strchr(data, '\n') : NULL;
		for (char *l = body ? body + 1 : NULL, *nl; l && *l; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = 0;
			size_t ll = strlen(l);
			if (!ll || l[ll - 1] != '/')
				continue;
			if (n == cap) {
				cap *= 2;
				char **nv = realloc(all, (size_t)cap * sizeof(*all));
				if (!nv)
					break;
				all = nv;
			}
			all[n++] = kp_canon_path(&d->canon, l);
		}
		free(data);
	}
	kb_strv_free(names);
	free(db);
	if (!all)
		return;
	qsort(all, (size_t)n, sizeof(*all), cmp_str);
	d->dir = malloc((size_t)(n ? n : 1) * sizeof(*d->dir));
	d->count = calloc((size_t)(n ? n : 1), sizeof(*d->count));
	for (int i = 0; i < n && d->dir && d->count; i++) {
		if (d->n && !strcmp(d->dir[d->n - 1], all[i])) {
			d->count[d->n - 1]++;
			free(all[i]);
			continue;
		}
		d->dir[d->n] = all[i];
		d->count[d->n++] = 1;
	}
	free(all);
}

int kp_dirclaims_drop(KpDirClaims *d, const char *line)
{
	if (!d->dir)
		return 0;
	char *k = kp_canon_path(&d->canon, line);
	char **hit = bsearch(&k, d->dir, (size_t)d->n, sizeof(*d->dir),
			     cmp_str);
	free(k);
	if (!hit)
		return 0;
	int *cnt = &d->count[hit - d->dir];
	if (*cnt > 0)
		(*cnt)--;
	return *cnt > 0;
}

void kp_dirclaims_free(KpDirClaims *d)
{
	for (int i = 0; i < d->n; i++)
		free(d->dir[i]);
	free(d->dir);
	free(d->count);
}

static int remove_one(const KpConf *c, const char *name, KpTriggers *trig,
		      KpDirClaims *claims)
{
	char *db = kp_db_dir(c);
	char *dbfile = kb_path_join(db, name);

	size_t len = 0;
	char *data = kb_read_all(dbfile, &len);
	if (!data) {
		kp_err("Package '%s' not installed", name);
		free(dbfile);
		free(db);
		return 1;
	}

	kp_msg("Removing %s", name);
	kp_triggers_gone(trig, data);

	const char *root = c->root[0] ? c->root : "/";

	/* Skip line 1 — that is the version, not a path. Every line is
	 * kept: a path left out of the table stays on disk after its
	 * package's entry is gone, owned by nothing. */
	char *body = strchr(data, '\n');
	size_t cap = 1;
	for (const char *q = data; *q; q++)
		cap += *q == '\n';
	char **paths = malloc(cap * sizeof(*paths));
	if (!paths) {
		kp_err("out of memory removing %s", name);
		free(data);
		free(dbfile);
		free(db);
		return 1;
	}
	int n = 0;
	for (char *l = body ? body + 1 : data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (*l)
			paths[n++] = l;
		l = nl ? nl + 1 : NULL;
	}

	/* Loaded per package, after the previous one's entry is gone, so a
	 * file two named packages both claim goes with the second. Only this
	 * package's own paths are ever asked about, so only their claims are
	 * kept. */
	KpOwned *owned = kp_owned_load_some(c, paths, n);
	for (int i = n - 1; i >= 0; i--) {
		char *full = kb_path_join(root, paths[i]);
		size_t pl = strlen(paths[i]);
		const char *rel = paths[i];
		if (!strncmp(rel, "./", 2))
			rel += 2;
		const char *other = NULL;
		if (pl && paths[i][pl - 1] == '/') {
			if (!kp_dirclaims_drop(claims, paths[i]))
				rmdir(full);	/* only when already empty */
		} else if ((other = kp_owned_other(owned, rel, name)))
			kp_msg("Keeping %s: %s claims it", paths[i], other);
		else
			unlink(full);
		free(full);
	}
	kp_owned_free(owned);
	free(paths);

	free(data);
	unlink(dbfile);
	free(dbfile);
	static const char *const sidecar[] = { ".recipe", ".pkgsha",
						 ".files", ".facets",
						 ".needs" };
	for (size_t i = 0; i < sizeof(sidecar) / sizeof(*sidecar); i++) {
		char *dir = kb_path_join(db, sidecar[i]);
		char *f = kb_path_join(dir, name);
		unlink(f);
		free(f);
		free(dir);
	}
	free(db);
	kp_msg("Package '%s' removed", name);
	return 0;
}

/* The first word of a world or origin line: leading blanks skipped, cut at
 * a blank, a carriage return or a `#`, the way kdos update reads both. */
static int line_names(const char *l, const char *const *names, const int *gone,
		      int n)
{
	l += strspn(l, " \t");
	size_t w = strcspn(l, " \t\r#");
	for (int i = 0; i < n; i++)
		if (gone[i] && strlen(names[i]) == w && !strncmp(l, names[i], w))
			return 1;
	return 0;
}

/* Every line naming a removed package dropped from <root>/<rel>, the file
 * written whole: one cut short by a crash would lose every name after the
 * cut. An absent file is left absent. */
static int drop_names(const char *root, const char *rel,
		      const char *const *names, const int *gone, int n)
{
	char *p = kb_path_join(root, rel);
	char *data = kb_read_all(p, NULL);
	int rc = 0, dropped = 0;
	if (data) {
		KbBuf b = {0};
		kb_buf_str(&b, "");
		for (char *l = data, *nl; l && *l; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = 0;
			if (line_names(l, names, gone, n))
				dropped = 1;
			else
				kb_buf_printf(&b, "%s\n", l);
		}
		if (dropped && kb_write_file_atomic(p, b.p) != 0) {
			kp_err("cannot rewrite %s: %s", p, strerror(errno));
			rc = -1;
		}
		kb_buf_free(&b);
		free(data);
	}
	free(p);
	return rc;
}

int del_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	/* As many names as the command line holds: a fixed table would
	 * drop every name past it, and the run would report success with
	 * those packages still installed. */
	const char **names = calloc((size_t)argc, sizeof(*names));
	if (!names) {
		kp_err("out of memory");
		return 1;
	}
	int n = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c.root, argv[++i], sizeof(c.root));
		else if (argv[i][0] == '-' && argv[i][1]) {
			/* An unknown option is rejected rather than falling
			 * through to the package list: taken as a name, a
			 * stray flag reads as "Package 'x' not installed". */
			kp_err("unknown option: %s", argv[i]);
			free(names);
			return 1;
		} else
			names[n++] = argv[i];
	}

	if (!n) {
		printf("Usage: kpkgdel [--root <path>] <package>...\n");
		free(names);
		return 1;
	}

	const char *root = c.root[0] ? c.root : "/";
	if (kp_root_unwritable(root)) {
		free(names);
		return 1;
	}

	int lock = kp_db_lock(&c);
	if (lock < 0) {
		int e = errno;
		char shown[1100];
		kp_db_shown(&c, shown, sizeof(shown));
		kp_err("cannot lock the package database %s: %s", shown,
		       strerror(e));
		free(names);
		return 1;
	}

	/* Every named package is attempted; the worst status is the exit code. */
	int rc = 0;
	KpTriggers trig = {0};
	KpDirClaims claims;
	kp_dirclaims_load(&c, &claims);
	int *gone = calloc((size_t)(n ? n : 1), sizeof(*gone));
	int any = 0;
	for (int i = 0; i < n; i++) {
		if (remove_one(&c, names[i], &trig, &claims))
			rc = 1;
		else if (gone)
			any = gone[i] = 1;
	}
	kp_dirclaims_free(&claims);
	if (any) {
		int w = drop_names(root, "var/lib/kdos/world", names, gone, n);
		int o = drop_names(root, "var/lib/kdos/origin", names, gone, n);
		if (w || o)
			rc = 1;
	}
	free(gone);
	kp_triggers_run(&trig, root);
	close(lock);
	free(names);
	return rc;
}
