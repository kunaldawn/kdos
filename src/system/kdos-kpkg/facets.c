/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkg facets — a package installed without its development and
 *   documentation files
 *
 *   kpkg facets --strip [--root <path>] [<package>...]
 *   kpkg facets --list [--root <path>] <package>
 *
 * The rules are libkpkg's (kp_facet_of, and kp_facet_glob over the recipe's
 * `facet_dev =` line, which only adds to dev); this file applies them to a
 * manifest. kpkgadd applies them to a package as it is staged when FACETS is
 * `run` (add.c); --strip applies them to what is already installed, every
 * package when none is named, which is how 70_image/017_stage.sh trims the
 * image. Either way a file left out is in neither the manifest nor the
 * `.files` record, and its `<facet> <path>` line is in `<db>/.facets/<name>`
 * (kpkg.h), so `kpkg check`, a removal and an upgrade's orphan sweep never
 * ask about it.
 *
 * A DIRECTORY leaves the manifest when it held a file that was left out and
 * holds no file that was kept, or when the directory above it left: kept, it
 * would be listed and not on disk, and `kpkg check` would call it missing.
 * `./` itself always stays.
 *
 * A file another installed package also claims stays on disk when --strip
 * drops it from this package's manifest: it is that package's file too. A
 * run over many packages reads the ownership table once, before the first,
 * so a file two stripped packages share is kept by both and asked about
 * again at the end, against the manifests as they then are.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Longest first, so a directory is reached after everything inside it. */
static int cmp_len_desc(const void *a, const void *b)
{
	size_t la = strlen(*(char *const *)a), lb = strlen(*(char *const *)b);
	return la < lb ? 1 : la > lb ? -1 : strcmp(*(char *const *)a,
						      *(char *const *)b);
}

typedef struct {
	char **v;
	int n, cap;
} Vec;

static void vec_add(Vec *x, char *s)
{
	if (x->n == x->cap) {
		x->cap = x->cap ? x->cap * 2 : 256;
		char **nv = kb_calloc((size_t)x->cap, sizeof(*nv));
		if (x->n)
			memcpy(nv, x->v, (size_t)x->n * sizeof(*nv));
		free(x->v);
		x->v = nv;
	}
	x->v[x->n++] = s;
}

static void vec_free(Vec *x)
{
	for (int i = 0; i < x->n; i++)
		free(x->v[i]);
	free(x->v);
}

/* Every directory above `line` (`./usr/include/z.h` -> `./usr/include/`,
 * `./usr/`, `./`), each a new string, into `x`. */
static void ancestors(const char *line, Vec *x)
{
	size_t n = strlen(line);
	if (n && line[n - 1] == '/')
		n--;
	for (size_t i = n; i-- > 0;)
		if (line[i] == '/') {
			char *a = kb_calloc(i + 2, 1);
			memcpy(a, line, i + 1);
			vec_add(x, a);
		}
}

static void vec_sort(Vec *x)
{
	if (x->n)
		qsort(x->v, (size_t)x->n, sizeof(*x->v), cmp_str);
}

static int vec_has(const Vec *x, const char *s)
{
	return x->n && bsearch(&s, x->v, (size_t)x->n, sizeof(*x->v),
			       cmp_str) != NULL;
}

/* The parent directory line of a directory line, or "" for `./`. */
static void parent_of(const char *dir, char *out, size_t cap)
{
	kb_strlcpy(out, dir, cap);
	size_t n = strlen(out);
	if (n && out[n - 1] == '/')
		out[--n] = 0;
	char *sl = strrchr(out, '/');
	if (sl)
		sl[1] = 0;
	else
		out[0] = 0;
}

void kp_facets_extra(const KpConf *c, const char *name, char *out,
		     size_t cap)
{
	char *dir = NULL;
	char err[1024];
	out[0] = 0;
	if (kp_port_find(c, name, &dir, err, sizeof(err)) == 1)
		kp_recipe_key(dir, "facet_dev", out, cap);
	free(dir);
}

void kp_facets_split(const char *root, const char *body, const char *extra,
		     KpFacetSplit *s)
{
	memset(s, 0, sizeof(*s));
	char *copy = kb_strdup(body ? body : "");
	Vec lines = {0}, kept_up = {0}, left_up = {0};
	char *facet = NULL;

	for (char *l = copy, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		if (*l)
			vec_add(&lines, kb_strdup(l));
	}
	facet = kb_calloc((size_t)(lines.n ? lines.n : 1), 1);
	for (int i = 0; i < lines.n; i++) {
		const char *l = lines.v[i];
		size_t ll = strlen(l);
		if (l[ll - 1] == '/')
			continue;
		char *full = kb_path_join(root, l);
		struct stat st;
		char target[4096];
		const char *link = NULL;
		if (lstat(full, &st) == 0 && S_ISLNK(st.st_mode)) {
			ssize_t tn = readlink(full, target, sizeof(target) - 1);
			if (tn >= 0) {
				target[tn] = 0;
				link = target;
			}
		}
		free(full);
		facet[i] = (char)kp_facet_of(l, link);
		if (facet[i] == KP_FACET_RUN && extra && kp_facet_glob(l, extra))
			facet[i] = KP_FACET_DEV;
		ancestors(l, facet[i] == KP_FACET_RUN ? &kept_up : &left_up);
	}
	vec_sort(&kept_up);
	vec_sort(&left_up);

	/* The directories in byte order, which puts each one after the
	 * directory above it; each is dropped when it holds nothing kept and
	 * something left out, or the one above it was dropped. */
	Vec dirs = {0}, dropped = {0};
	for (int i = 0; i < lines.n; i++) {
		size_t ll = strlen(lines.v[i]);
		if (lines.v[i][ll - 1] == '/')
			vec_add(&dirs, kb_strdup(lines.v[i]));
	}
	vec_sort(&dirs);
	char *drop = kb_calloc((size_t)(dirs.n ? dirs.n : 1), 1);
	for (int i = 0; i < dirs.n; i++) {
		const char *d = dirs.v[i];
		if (!strcmp(d, "./") || vec_has(&kept_up, d))
			continue;
		char par[4096];
		const char *pp = par;
		parent_of(d, par, sizeof(par));
		char **up = par[0] ? bsearch(&pp, dirs.v, (size_t)dirs.n,
					     sizeof(*dirs.v), cmp_str)
				   : NULL;
		if (vec_has(&left_up, d) || (up && drop[up - dirs.v])) {
			drop[i] = 1;
			vec_add(&dropped, kb_strdup(d));
		}
	}
	free(drop);
	vec_sort(&dropped);

	for (int i = 0; i < lines.n; i++) {
		const char *l = lines.v[i];
		size_t ll = strlen(l);
		if (l[ll - 1] == '/') {
			if (vec_has(&dropped, l))
				kb_buf_printf(&s->gone, "%s\n", l);
			else
				kb_buf_printf(&s->keep, "%s\n", l);
		} else if (facet[i] != KP_FACET_RUN) {
			kb_buf_printf(&s->left, "%s %s\n",
				      kp_facet_name(facet[i]), l);
			kb_buf_printf(&s->gone, "%s\n", l);
			s->nfiles++;
		} else {
			kb_buf_printf(&s->keep, "%s\n", l);
		}
	}
	if (!s->keep.p)
		kb_buf_str(&s->keep, "");
	if (!s->left.p)
		kb_buf_str(&s->left, "");
	if (!s->gone.p)
		kb_buf_str(&s->gone, "");
	free(facet);
	free(copy);
	vec_free(&lines);
	vec_free(&kept_up);
	vec_free(&left_up);
	vec_free(&dirs);
	vec_free(&dropped);
}

void kp_facets_split_free(KpFacetSplit *s)
{
	kb_buf_free(&s->keep);
	kb_buf_free(&s->left);
	kb_buf_free(&s->gone);
}

void kp_facets_remove(const char *root, const char *gone,
		      unsigned long long *bytes)
{
	Vec dirs = {0};
	char *copy = kb_strdup(gone);
	for (char *l = copy, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		size_t ll = strlen(l);
		if (!ll)
			continue;
		char *full = kb_path_join(root, l);
		if (l[ll - 1] == '/') {
			vec_add(&dirs, full);
			continue;
		}
		struct stat st;
		if (lstat(full, &st) == 0 && unlink(full) == 0 && bytes &&
		    S_ISREG(st.st_mode))
			*bytes += (unsigned long long)st.st_size;
		free(full);
	}
	if (dirs.n)
		qsort(dirs.v, (size_t)dirs.n, sizeof(*dirs.v), cmp_len_desc);
	for (int i = 0; i < dirs.n; i++)
		rmdir(dirs.v[i]);	/* only when it is empty */
	vec_free(&dirs);
	free(copy);
}

/* ── kpkg facets --strip ───────────────────────────────────────────────── */

/* `blob` without the lines whose path (the last field, after `skip`
 * fields) is a line of `gone`. */
static char *drop_lines(const char *blob, const Vec *gone, int skip)
{
	KbBuf out = {0};
	kb_buf_str(&out, "");
	char *copy = kb_strdup(blob ? blob : "");
	for (char *l = copy, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		const char *path = l;
		for (int k = 0; k < skip && path; k++) {
			path = strchr(path, ' ');
			if (path)
				path++;
		}
		if (path && vec_has(gone, path))
			continue;
		kb_buf_printf(&out, "%s\n", l);
	}
	free(copy);
	return out.p;
}

static int strip_one(const KpConf *c, const char *root, const char *name,
		     const KpOwned *table, Vec *kept, KpTriggers *trig,
		     unsigned long long *bytes, int *stripped)
{
	char *db = kp_db_dir(c);
	char *dbfile = kb_path_join(db, name);
	free(db);
	char *man = kb_read_all(dbfile, NULL);
	if (!man) {
		kp_err("Package '%s' not installed", name);
		free(dbfile);
		return 1;
	}
	char *body = strchr(man, '\n');
	if (body)
		*body++ = 0;

	char extra[1024];
	kp_facets_extra(c, name, extra, sizeof(extra));
	KpFacetSplit s;
	kp_facets_split(root, body, extra, &s);
	char *old = kp_installed_facets(c, name, NULL);
	if (!s.gone.n) {
		/* Nothing to leave out: the record still says the package is
		 * installed with its run-time facet only. */
		if (!old)
			kp_record_facets(c, name, "", 0);
		free(old);
		kp_facets_split_free(&s);
		free(man);
		free(dbfile);
		return 0;
	}

	/* The new entry first: an interruption after it leaves files on disk
	 * that nothing claims, which `kpkg check --all` reports, rather than
	 * claims on files that are gone. */
	KbBuf entry = {0};
	kb_buf_printf(&entry, "%s\n", man);
	kb_buf_add(&entry, s.keep.p, s.keep.n);
	if (kb_write_file_atomic(dbfile, entry.p) != 0) {
		kp_err("%s: the database entry could not be rewritten", name);
		kb_buf_free(&entry);
		kp_facets_split_free(&s);
		free(old);
		free(man);
		free(dbfile);
		return 1;
	}
	kb_buf_free(&entry);

	Vec gone = {0};
	char *gcopy = kb_strdup(s.gone.p);
	for (char *l = gcopy, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		if (*l)
			vec_add(&gone, kb_strdup(l));
	}
	free(gcopy);
	vec_sort(&gone);

	size_t flen = 0;
	char *files = kp_installed_files(c, name, &flen);
	if (files) {
		char *nf = drop_lines(files, &gone, 2);
		if (kp_record_files(c, name, nf, strlen(nf)) != 0)
			kp_msg("Warning: no file record written for %s", name);
		free(nf);
		free(files);
	}

	KbBuf rec = {0};
	kb_buf_str(&rec, old ? old : "");
	kb_buf_add(&rec, s.left.p, s.left.n);
	if (kp_record_facets(c, name, rec.p, rec.n) != 0)
		kp_msg("Warning: no facet record written for %s", name);
	kb_buf_free(&rec);

	/* Files another package claims stay where they are. */
	Vec ask = {0};
	for (int i = 0; i < gone.n; i++) {
		size_t gl = strlen(gone.v[i]);
		if (gl && gone.v[i][gl - 1] != '/')
			vec_add(&ask, kb_strdup(gone.v[i] + 2));
	}
	KpOwned *mine = table ? NULL : kp_owned_load_some(c, ask.v, ask.n);
	const KpOwned *owned = table ? table : mine;
	KbBuf rm = {0};
	kb_buf_str(&rm, "");
	for (char *l = s.gone.p, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		size_t ll = nl ? (size_t)(nl - l) : strlen(l);
		char one[4096];
		if (ll < sizeof(one)) {
			memcpy(one, l, ll);
			one[ll] = 0;
			const char *other =
				one[ll - 1] == '/'
					? NULL
					: kp_owned_other(owned, one + 2, name);
			if (other && table)
				vec_add(kept, kb_strdup(one));
			else if (other)
				kp_msg("Keeping %s: %s claims it", one, other);
			else
				kb_buf_printf(&rm, "%s\n", one);
		}
		nl = nl ? nl + 1 : NULL;
	}
	kp_owned_free(mine);
	unsigned long long freed = 0;
	kp_facets_remove(root, rm.p, &freed);
	kp_triggers_gone(trig, rm.p);
	*bytes += freed;
	(*stripped)++;
	kp_msg("%s: %d file%s left out, %s", name, s.nfiles,
	       s.nfiles == 1 ? "" : "s", kb_human_size(freed));

	kb_buf_free(&rm);
	vec_free(&ask);
	vec_free(&gone);
	kp_facets_split_free(&s);
	free(old);
	free(man);
	free(dbfile);
	return 0;
}

static int list_one(const KpConf *c, const char *name)
{
	if (!kp_installed(c, name)) {
		kp_err("Package '%s' not installed", name);
		return 1;
	}
	char *d = kp_installed_facets(c, name, NULL);
	if (!d) {
		printf("%s: every facet installed\n", name);
		return 0;
	}
	fputs(d, stdout);
	free(d);
	return 0;
}

int kp_cmd_facets(KpConf *c, int argc, char **argv)
{
	int strip = 0, list = 0;
	char **names = kb_calloc((size_t)argc + 1, sizeof(*names));
	int n = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--strip"))
			strip = 1;
		else if (!strcmp(argv[i], "--list"))
			list = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c->root, argv[++i], sizeof(c->root));
		else if (argv[i][0] == '-') {
			kp_err("unknown option: %s", argv[i]);
			free(names);
			return 2;
		} else
			names[n++] = argv[i];
	}
	if (strip == list || (list && n != 1)) {
		printf("Usage: kpkg facets --strip [--root <path>] "
		       "[<package>...]\n"
		       "       kpkg facets --list [--root <path>] <package>\n");
		free(names);
		return 2;
	}
	if (list) {
		int rc = list_one(c, names[0]);
		free(names);
		return rc;
	}

	const char *root = c->root[0] ? c->root : "/";
	if (access(root, W_OK) != 0) {
		kp_err("cannot write to %s — run as root", root);
		free(names);
		return 1;
	}
	char **all = NULL;
	if (!n) {
		char *db = kp_db_dir(c);
		int nd = 0;
		all = kb_listdir(db, &nd);
		free(db);
		for (int i = 0; all && i < nd; i++)
			if (all[i][0] != '.')
				names[n++] = all[i];
	}

	int lock = kp_db_lock(c);
	if (lock < 0) {
		kp_err("cannot lock the package database");
		free(names);
		kb_strv_free(all);
		return 1;
	}
	int rc = 0, stripped = 0;
	unsigned long long bytes = 0;
	KpTriggers trig = {0};
	KpOwned *table = n > 1 ? kp_owned_load(c) : NULL;
	Vec kept = {0};
	for (int i = 0; i < n; i++)
		if (strip_one(c, root, names[i], table, &kept, &trig, &bytes,
			      &stripped))
			rc = 1;
	kp_owned_free(table);
	/* A shared file whose every claimant was stripped is nobody's now. */
	if (kept.n) {
		Vec ask = {0};
		for (int i = 0; i < kept.n; i++)
			vec_add(&ask, kb_strdup(kept.v[i] + 2));
		KpOwned *now = kp_owned_load_some(c, ask.v, ask.n);
		KbBuf rm = {0};
		kb_buf_str(&rm, "");
		for (int i = 0; i < kept.n; i++) {
			const char *owner = kp_owned_owner(now, ask.v[i]);
			if (owner)
				kp_msg("Keeping %s: %s claims it", kept.v[i],
				       owner);
			else
				kb_buf_printf(&rm, "%s\n", kept.v[i]);
		}
		kp_owned_free(now);
		kp_facets_remove(root, rm.p, &bytes);
		kp_triggers_gone(&trig, rm.p);
		kb_buf_free(&rm);
		vec_free(&ask);
	}
	vec_free(&kept);
	kp_triggers_run(&trig, root);
	close(lock);
	kp_msg("facets: %d package%s stripped, %llu bytes (%s) freed",
	       stripped, stripped == 1 ? "" : "s", bytes, kb_human_size(bytes));
	free(names);
	kb_strv_free(all);
	return rc;
}

int kp_facets_guard(const KpConf *c, const char *port)
{
	char **want = kp_facet_wanted(c, port);
	if (!want[0]) {
		kb_strv_free(want);
		return 0;
	}
	KbBuf l = {0};
	for (int i = 0; want[i]; i++)
		kb_buf_printf(&l, "%s%s", i ? " " : "", want[i]);
	kp_err("%s builds against %s, installed without %s development "
	       "files; `sudo kdos update install --dev %s` restores them",
	       port, l.p, want[1] ? "their" : "its", l.p);
	kb_buf_free(&l);
	kb_strv_free(want);
	return -1;
}
