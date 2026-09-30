/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — configuration and the ports tree
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kpkg.h"

/*
 * kpkg.conf is sourced shell whose every line is `NAME="${NAME:-default}"`.
 * Nothing in it is ever anything else, so it is read as key=value with the
 * `${...:-...}` unwrapped, rather than by running a shell. Whatever the file
 * says, an exported environment variable still wins — that is what the
 * `${X:-...}` meant, and the phase env files depend on it.
 */
static void unwrap(const char *raw, char *out, size_t cap)
{
	const char *s = raw;
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '"' || *s == '\'')
		s++;

	/* ${NAME:-VALUE} -> VALUE */
	if (!strncmp(s, "${", 2)) {
		const char *d = strstr(s, ":-");
		if (d)
			s = d + 2;
		char tmp[512];
		kb_strlcpy(tmp, s, sizeof(tmp));
		char *close = strchr(tmp, '}');
		if (close)
			*close = 0;
		kb_strlcpy(out, tmp, cap);
		return;
	}

	kb_strlcpy(out, s, cap);
	size_t n = strlen(out);
	while (n && (out[n - 1] == '"' || out[n - 1] == '\'' || out[n - 1] == '\n'))
		out[--n] = 0;
}

static void set_from_env(char *dst, size_t cap, const char *name)
{
	const char *v = getenv(name);
	if (v && *v)
		kb_strlcpy(dst, v, cap);
}

/* A regular file at <dir>/kpkgbuild: what makes a directory a port. */
static int is_port(const char *dir)
{
	char *recipe = kb_path_join(dir, "kpkgbuild");
	int ok = kb_path_exists(recipe) && !kb_is_dir(recipe);
	free(recipe);
	return ok;
}

/* A directory of <repo> that is not a port, and so is a shelf. Dot-names are
 * never shelves: `.git`, and a tool's hidden cache beside the ports. A flat
 * repository is read the same way, so src/system/kdos-kpkg is a shelf of
 * src/system: a <dir>/kpkgbuild inside it would be a port of that name. */
static int is_shelf(const char *repo, const char *entry)
{
	if (entry[0] == '.')
		return 0;
	char *dir = kb_path_join(repo, entry);
	int ok = kb_is_dir(dir) && !is_port(dir);
	free(dir);
	return ok;
}

/* The shelves of `repo`, NUL-separated into `buf` and ended by an empty name.
 * -1 when they do not fit, which leaves the repository uncached rather than
 * holding a list that silently stops part-way. */
static int list_shelves(const char *repo, char *buf, size_t cap)
{
	size_t at = 0;
	char **names = kb_listdir(repo, NULL);
	for (char **e = names; e && *e; e++) {
		if (!is_shelf(repo, *e))
			continue;
		size_t len = strlen(*e) + 1;
		if (at + len + 1 > cap) {
			kb_strv_free(names);
			return -1;
		}
		memcpy(buf + at, *e, len);
		at += len;
	}
	kb_strv_free(names);
	buf[at] = 0;
	return 0;
}

int kp_conf_set_repos(KpConf *c, const char *list)
{
	c->nrepos = 0;
	char tmp[2048];
	if (strlen(list) >= sizeof(tmp))
		kb_warn("PORT_REPO is longer than %zu bytes and is cut short "
			"there", sizeof(tmp) - 1);
	kb_strlcpy(tmp, list, sizeof(tmp));
	/*
	 * THE TAIL IS DROPPED, SO SAY SO. A repository past KP_MAX_REPOS does
	 * not exist to any lookup, and every port in it reads as "no such
	 * port" — a phase list that names one fails far from the cause.
	 */
	for (char *t = strtok(tmp, " \t\n"); t; t = strtok(NULL, " \t\n")) {
		if (c->nrepos == KP_MAX_REPOS) {
			kb_warn("PORT_REPO names more than %d repositories; "
				"ignoring %s and every one after it",
				KP_MAX_REPOS, t);
			break;
		}
		int i = c->nrepos++;
		kb_strlcpy(c->repos[i], t, sizeof(c->repos[0]));
		c->shelf_cached[i] = list_shelves(c->repos[i], c->shelves[i],
						  sizeof(c->shelves[0])) == 0;
	}
	return c->nrepos;
}

void kp_conf_load(KpConf *c)
{
	memset(c, 0, sizeof(*c));

	kb_strlcpy(c->conf, "/etc/kpkg.conf", sizeof(c->conf));
	set_from_env(c->conf, sizeof(c->conf), "KPKG_CONF");
	set_from_env(c->root, sizeof(c->root), "KPKG_ROOT");

	/*
	 * Off by default and set by the phase env files, so the BUILD is strict
	 * — where a package that no longer matches its recipe is a defect —
	 * while an interactive `kpkg install` is not.
	 */
	{
		const char *v = getenv("KPKG_STRICT_RECIPE");
		c->strict_recipe = (v && *v == '1');
	}

	char repos[2048];
	kb_strlcpy(repos, "/ports/core", sizeof(repos));
	kb_strlcpy(c->source_dir, "/var/cache/kpkg/sources", sizeof(c->source_dir));
	kb_strlcpy(c->package_dir, "/var/cache/kpkg/packages",
		   sizeof(c->package_dir));
	kb_strlcpy(c->work_dir, "/var/cache/kpkg/work", sizeof(c->work_dir));
	kb_strlcpy(c->pkgdb_dir, "/var/lib/kpkg/db", sizeof(c->pkgdb_dir));

	size_t len = 0;
	char *data = kb_read_all(c->conf, &len);
	if (data) {
		for (char *line = data, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = 0;
			while (*line == ' ' || *line == '\t')
				line++;
			if (*line == '#' || !*line)
				continue;
			char *eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq = 0;
			char val[2048];
			unwrap(eq + 1, val, sizeof(val));

			if (!strcmp(line, "PORT_REPO"))
				kb_strlcpy(repos, val, sizeof(repos));
			else if (!strcmp(line, "SOURCE_DIR"))
				kb_strlcpy(c->source_dir, val, sizeof(c->source_dir));
			else if (!strcmp(line, "PACKAGE_DIR"))
				kb_strlcpy(c->package_dir, val,
					   sizeof(c->package_dir));
			else if (!strcmp(line, "WORK_DIR"))
				kb_strlcpy(c->work_dir, val, sizeof(c->work_dir));
			else if (!strcmp(line, "PKGDB_DIR"))
				kb_strlcpy(c->pkgdb_dir, val, sizeof(c->pkgdb_dir));
		}
		free(data);
	}

	/* The environment wins over the file, always. */
	set_from_env(repos, sizeof(repos), "PORT_REPO");
	set_from_env(c->source_dir, sizeof(c->source_dir), "SOURCE_DIR");
	set_from_env(c->package_dir, sizeof(c->package_dir), "PACKAGE_DIR");
	set_from_env(c->work_dir, sizeof(c->work_dir), "WORK_DIR");
	set_from_env(c->pkgdb_dir, sizeof(c->pkgdb_dir), "PKGDB_DIR");

	kp_conf_set_repos(c, repos);
}

char *kp_db_dir(const KpConf *c)
{
	if (!c->root[0])
		return kb_strdup(c->pkgdb_dir);
	/* The shell built "$KPKG_ROOT/$PKGDB_DIR", double slash and all. */
	return kb_path_join(c->root, c->pkgdb_dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The shelves of repository `i`, from the cache or listed now. Freed with
 * kb_strv_free. */
static char **shelves_of(const KpConf *c, int i)
{
	int cap = 16, n = 0;
	char **out = kb_calloc((size_t)cap + 1, sizeof(*out));
	char **live = NULL;

	if (!c->shelf_cached[i])
		live = kb_listdir(c->repos[i], NULL);
	const char *at = c->shelf_cached[i] ? c->shelves[i] : NULL;
	for (char **e = live;;) {
		const char *name;
		if (at) {
			if (!*at)
				break;
			name = at;
			at += strlen(at) + 1;
		} else {
			if (!e || !*e)
				break;
			name = *e++;
			if (!is_shelf(c->repos[i], name))
				continue;
		}
		if (n == cap) {
			cap *= 2;
			char **grown = kb_calloc((size_t)cap + 1, sizeof(*grown));
			memcpy(grown, out, (size_t)n * sizeof(*out));
			free(out);
			out = grown;
		}
		out[n++] = kb_strdup(name);
	}
	kb_strv_free(live);
	return out;
}

int kp_port_find(const KpConf *c, const char *name, char **dir, char *err,
		 size_t errcap)
{
	*dir = NULL;
	if (err && errcap)
		err[0] = 0;
	if (!name || !*name)
		return 0;
	/* A path is not a name: it is looked up where it points and nowhere
	 * else, so `a/b` cannot be found twice by also reading as shelf `a`. */
	int bare = !strchr(name, '/') && strcmp(name, ".") && strcmp(name, "..");

	for (int i = 0; i < c->nrepos; i++) {
		char *found = NULL;

		char *flat = kb_path_join(c->repos[i], name);
		if (is_port(flat))
			found = flat;
		else
			free(flat);

		char **shelves = bare ? shelves_of(c, i) : NULL;
		for (char **s = shelves; s && *s; s++) {
			char *shelf = kb_path_join(c->repos[i], *s);
			char *d = kb_path_join(shelf, name);
			free(shelf);
			if (!is_port(d)) {
				free(d);
				continue;
			}
			if (found) {
				snprintf(err, errcap,
					 "port %s is filed twice: %s and %s",
					 name, found, d);
				free(d);
				free(found);
				kb_strv_free(shelves);
				return -1;
			}
			found = d;
		}
		kb_strv_free(shelves);
		if (found) {
			*dir = found;
			return 1;
		}
	}
	return 0;
}

char *kp_port_dir(const KpConf *c, const char *name)
{
	char err[1600], *dir = NULL;
	if (kp_port_find(c, name, &dir, err, sizeof(err)) < 0)
		kb_die("%s", err);
	return dir;
}

typedef struct {
	char **name;
	char **path;		/* where it was found, for the duplicate message */
	int n, cap;
} PortSet;

static int set_find(const PortSet *ps, const char *name)
{
	for (int k = 0; k < ps->n; k++)
		if (!strcmp(ps->name[k], name))
			return k;
	return -1;
}

static void set_add(PortSet *ps, char *name, char *path)
{
	if (ps->n == ps->cap) {
		int ncap = ps->cap ? ps->cap * 2 : 512;
		char **nn = kb_calloc((size_t)ncap + 1, sizeof(*nn));
		char **np = kb_calloc((size_t)ncap + 1, sizeof(*np));
		if (ps->n) {
			memcpy(nn, ps->name, (size_t)ps->n * sizeof(*nn));
			memcpy(np, ps->path, (size_t)ps->n * sizeof(*np));
		}
		free(ps->name);
		free(ps->path);
		ps->name = nn;
		ps->path = np;
		ps->cap = ncap;
	}
	ps->name[ps->n] = name;
	ps->path[ps->n] = path;
	ps->n++;
}

static void set_free(PortSet *ps, int keep_names)
{
	for (int k = 0; k < ps->n; k++) {
		if (!keep_names)
			free(ps->name[k]);
		free(ps->path[k]);
	}
	if (!keep_names)
		free(ps->name);
	free(ps->path);
}

/* One port of the repository being walked, found at `dir`. -1 with `err` set
 * when the repository already holds the name at another path. */
static int take_port(PortSet *repo, const char *name, const char *dir,
		     char *err, size_t errcap)
{
	int at = set_find(repo, name);
	if (at >= 0) {
		snprintf(err, errcap, "port %s is filed twice: %s and %s", name,
			 repo->path[at], dir);
		return -1;
	}
	set_add(repo, kb_strdup(name), kb_strdup(dir));
	return 0;
}

/* A directory inside a shelf that is not a port. It is invisible, as a stray
 * directory beside the ports is, UNLESS a port is filed inside it:
 * that is a port one level too deep, which no lookup would ever find. */
static int check_nested(const char *dir, char *err, size_t errcap)
{
	char **names = kb_listdir(dir, NULL);
	int rc = 0;
	for (char **e = names; e && *e && !rc; e++) {
		if ((*e)[0] == '.')
			continue;
		char *d = kb_path_join(dir, *e);
		if (is_port(d)) {
			snprintf(err, errcap,
				 "port %s is nested below its shelf: %s", *e, d);
			rc = -1;
		}
		free(d);
	}
	kb_strv_free(names);
	return rc;
}

/* Every port of one repository, at both depths, into `ps`. */
static int scan_repo(const char *repo, PortSet *ps, char *err, size_t errcap)
{
	char **names = kb_listdir(repo, NULL);
	int rc = 0;
	for (char **e = names; e && *e && !rc; e++) {
		if ((*e)[0] == '.')
			continue;
		char *dir = kb_path_join(repo, *e);
		if (is_port(dir)) {
			rc = take_port(ps, *e, dir, err, errcap);
		} else if (kb_is_dir(dir)) {
			/* A shelf: exactly one level of ports below it. */
			char **inner = kb_listdir(dir, NULL);
			for (char **f = inner; f && *f && !rc; f++) {
				if ((*f)[0] == '.')
					continue;
				char *pd = kb_path_join(dir, *f);
				if (is_port(pd))
					rc = take_port(ps, *f, pd, err, errcap);
				else if (kb_is_dir(pd))
					rc = check_nested(pd, err, errcap);
				free(pd);
			}
			kb_strv_free(inner);
		}
		free(dir);
	}
	kb_strv_free(names);
	return rc;
}

static int cmp_name(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

char **kp_ports_scan(const KpConf *c, int *count, char *err, size_t errcap)
{
	/*
	 * THE ARRAY GROWS. A fixed ceiling here is not a cap on a pathological
	 * input, it is a cap on the TREE: the core repo alone is past what any
	 * round number would have been, and a scan that stopped early returned
	 * a count that looked like an answer — every port alphabetically after
	 * the cut simply did not exist, to every consumer, with nothing said.
	 */
	PortSet all = {0};
	char scratch[1600];
	int rc = 0;

	if (!err || !errcap) {
		err = scratch;
		errcap = sizeof(scratch);
	}
	err[0] = 0;
	if (count)
		*count = 0;

	for (int i = 0; i < c->nrepos && !rc; i++) {
		if (!kb_is_dir(c->repos[i]))
			continue;
		PortSet one = {0};
		rc = scan_repo(c->repos[i], &one, err, errcap);
		/* A repository that is there and yields nothing is a walker
		 * that does not match the tree, which reads exactly like an
		 * empty one to every consumer; the second never happens. */
		if (!rc && !one.n)
			kb_warn("%s holds no port at <name>/kpkgbuild or "
				"<shelf>/<name>/kpkgbuild", c->repos[i]);
		/* The first repository holding a name wins it. */
		for (int k = 0; k < one.n; k++) {
			if (!rc && set_find(&all, one.name[k]) < 0) {
				set_add(&all, one.name[k], one.path[k]);
				one.name[k] = one.path[k] = NULL;
			}
			free(one.name[k]);
			free(one.path[k]);
		}
		free(one.name);
		free(one.path);
	}

	if (rc) {
		set_free(&all, 0);
		return NULL;
	}
	set_free(&all, 1);
	if (!all.name)
		all.name = kb_calloc(1, sizeof(*all.name));
	qsort(all.name, (size_t)all.n, sizeof(*all.name), cmp_name);
	all.name[all.n] = NULL;
	if (count)
		*count = all.n;
	return all.name;
}

char **kp_all_ports(const KpConf *c, int *count)
{
	char err[1600];
	char **v = kp_ports_scan(c, count, err, sizeof(err));
	if (!v)
		kb_die("%s", err);
	return v;
}

/* The merged-/usr aliases, read off the root. A link is an alias only when it
 * points at `usr/<its own name>` (or `/usr/<its own name>`): anything else is
 * a layout this code does not understand, and treating it as an alias would
 * merge two different files into one key. */
void kp_canon_load(const KpConf *c, KpCanon *k)
{
	static const char *const names[] = { "bin", "sbin", "lib", "lib64",
					     "lib32" };
	const char *root = c->root[0] ? c->root : "/";
	k->n = 0;
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) {
		char *link = kb_path_join(root, names[i]);
		char target[64];
		ssize_t tn = readlink(link, target, sizeof(target) - 1);
		free(link);
		if (tn <= 0)
			continue;
		target[tn] = 0;
		const char *t = target;
		if (*t == '/')
			t++;
		char want[16];
		snprintf(want, sizeof(want), "usr/%s", names[i]);
		if (strcmp(t, want))
			continue;
		kb_strlcpy(k->from[k->n], names[i], sizeof(k->from[k->n]));
		kb_strlcpy(k->to[k->n], want, sizeof(k->to[k->n]));
		k->n++;
	}
}

char *kp_canon_path(const KpCanon *k, const char *rel)
{
	int dot = !strncmp(rel, "./", 2);
	const char *p = dot ? rel + 2 : rel;
	for (int i = 0; k && i < k->n; i++) {
		size_t fl = strlen(k->from[i]);
		/* `bin/x` and `bin/` are aliased; `bin` alone is the link
		 * itself and `binutils/` is another name entirely. */
		if (strncmp(p, k->from[i], fl) || p[fl] != '/')
			continue;
		KbBuf b = {0};
		kb_buf_printf(&b, "%s%s%s", dot ? "./" : "", k->to[i], p + fl);
		return b.p;
	}
	return kb_strdup(rel);
}

/* Every path any installed package claims, as one sorted list.
 *
 * The database is one file per package: line 1 is `<version> <release>` and
 * the rest is the `tar -tf` listing, `./`-prefixed, with directories carrying
 * a trailing slash. Loading it once and asking N questions of the result is
 * the difference between an install being instant and being quadratic.
 */
typedef struct {
	char *path;
	char *owner;
} OwnedPair;

static int cmp_owned(const void *a, const void *b)
{
	return strcmp(((const OwnedPair *)a)->path, ((const OwnedPair *)b)->path);
}

KpOwned *kp_owned_load(const KpConf *c)
{
	KpOwned *o = kb_calloc(1, sizeof(*o));
	kp_canon_load(c, &o->canon);
	char *db = kp_db_dir(c);
	char **names = kb_listdir(db, NULL);
	if (!names) {
		free(db);
		return o;
	}

	int cap = 4096;
	OwnedPair *pair = kb_calloc((size_t)cap, sizeof(*pair));
	int ocap = 64;
	o->ownerv = kb_calloc((size_t)ocap, sizeof(*o->ownerv));
	for (char **n = names; *n; n++) {
		char *f = kb_path_join(db, *n);
		size_t len = 0;
		char *data = kb_read_all(f, &len);
		free(f);
		if (!data)
			continue;

		/* One copy of the package name per package, shared by every
		 * path it claims: a quarter of a million paths come from under
		 * a thousand names, and a copy each is megabytes of identical
		 * strings. Taken only once the file has been read, so a name
		 * never enters the pool without a package behind it. */
		if (o->nowner == ocap) {
			ocap *= 2;
			char **nv = kb_calloc((size_t)ocap, sizeof(*nv));
			memcpy(nv, o->ownerv,
			       (size_t)o->nowner * sizeof(*nv));
			free(o->ownerv);
			o->ownerv = nv;
		}
		char *owner = kb_strdup(*n);
		o->ownerv[o->nowner++] = owner;

		int first = 1;
		for (char *line = data, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = 0;
			if (first) {		/* the version line */
				first = 0;
				continue;
			}
			size_t l = strlen(line);
			if (!l || line[l - 1] == '/')
				continue;	/* directories are shared */
			if (o->n == cap) {
				cap *= 2;
				OwnedPair *nv =
					kb_calloc((size_t)cap, sizeof(*nv));
				memcpy(nv, pair, (size_t)o->n * sizeof(*nv));
				free(pair);
				pair = nv;
			}
			pair[o->n].path = kp_canon_path(&o->canon, line);
			pair[o->n].owner = owner;
			o->n++;
		}
		free(data);
	}
	kb_strv_free(names);
	free(db);

	qsort(pair, (size_t)o->n, sizeof(*pair), cmp_owned);
	o->path = kb_calloc((size_t)(o->n ? o->n : 1), sizeof(*o->path));
	o->owner = kb_calloc((size_t)(o->n ? o->n : 1), sizeof(*o->owner));
	for (int i = 0; i < o->n; i++) {
		o->path[i] = pair[i].path;
		o->owner[i] = pair[i].owner;
	}
	free(pair);
	return o;
}

/* Order a stored `./usr/bin/tar` against a bare `usr/bin/tar`, byte for byte
 * as strcmp would against the `./`-prefixed spelling, so the binary search
 * below stays consistent with the sort. Comparing in place rather than
 * building the prefixed key is what keeps a path of any length findable: a
 * fixed key buffer silently truncates the long ones, and a truncated key
 * matches nothing. */
static int cmp_stored_rel(const char *stored, const char *rel)
{
	static const char pre[2] = { '.', '/' };
	for (int i = 0; i < 2; i++) {
		unsigned char a = (unsigned char)stored[i];
		if (a != (unsigned char)pre[i])
			return a < (unsigned char)pre[i] ? -1 : 1;
	}
	return strcmp(stored + 2, rel);
}

/* Binary search; -1 when nothing claims it. */
static int owned_find(const KpOwned *o, const char *rel)
{
	int lo = 0, hi = o->n - 1;
	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		int r = cmp_stored_rel(o->path[mid], rel);
		if (!r)
			return mid;
		if (r < 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

/* `rel` is `usr/bin/tar`; the database spells it `./usr/bin/tar`. */
const char *kp_owned_owner(const KpOwned *o, const char *rel)
{
	char *key = kp_canon_path(&o->canon, rel);
	int i = owned_find(o, key);
	free(key);
	return i < 0 ? NULL : o->owner[i];
}

/* Equal keys sit side by side in the sorted table and the search lands on any
 * one of them, so the run is walked both ways from where it landed. */
const char *kp_owned_other(const KpOwned *o, const char *rel,
			   const char *self)
{
	char *key = kp_canon_path(&o->canon, rel);
	int i = owned_find(o, key);
	const char *hit = NULL;
	if (i >= 0) {
		for (int j = i; j >= 0 && !hit &&
				!cmp_stored_rel(o->path[j], key); j--)
			if (strcmp(o->owner[j], self))
				hit = o->owner[j];
		for (int j = i + 1; j < o->n && !hit &&
				!cmp_stored_rel(o->path[j], key); j++)
			if (strcmp(o->owner[j], self))
				hit = o->owner[j];
	}
	free(key);
	return hit;
}

void kp_owned_free(KpOwned *o)
{
	if (!o)
		return;
	for (int i = 0; i < o->n; i++)
		free(o->path[i]);
	/* `owner[i]` points into the name pool and is never its own
	 * allocation; the pool is what has to be freed. */
	for (int i = 0; i < o->nowner; i++)
		free(o->ownerv[i]);
	free(o->ownerv);
	free(o->path);
	free(o->owner);
	free(o);
}

/* An overwrite moves a path from one package to another. The old owner's
 * manifest has to lose it, or `kpkgdel <old>` deletes a file the new owner
 * installed — the "owned by nothing / owned by two" failure this exists to
 * prevent. Rewritten whole: the file is a few hundred KB at most. */
int kp_db_drop_paths(const KpConf *c, const char *pkg, char *const *paths,
		     int n)
{
	if (n <= 0)
		return 0;
	char *db = kp_db_dir(c);
	char *file = kb_path_join(db, pkg);
	free(db);

	size_t len = 0;
	char *data = kb_read_all(file, &len);
	if (!data) {
		free(file);
		return 0;
	}

	KpCanon k;
	kp_canon_load(c, &k);
	char **want = kb_calloc((size_t)n, sizeof(*want));
	for (int i = 0; i < n; i++)
		want[i] = kp_canon_path(&k, paths[i]);

	KbBuf out = {0};
	int dropped = 0, first = 1;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		int drop = 0;
		if (!first) {
			/* `paths` is relative, the manifest is `./`-prefixed.
			 * Compare past the prefix rather than formatting the
			 * prefixed key: this runs once per (line x path) pair,
			 * a manifest is tens of thousands of lines and a batch
			 * hundreds of paths, and a fixed key buffer would also
			 * truncate — leaving a long path in the old owner's
			 * manifest, the double claim this function prevents. */
			char *have = kp_canon_path(&k, line);
			for (int i = 0; i < n && !drop; i++)
				if (!cmp_stored_rel(have, want[i]))
					drop = 1;
			free(have);
		}
		first = 0;
		if (drop)
			dropped++;
		else
			kb_buf_printf(&out, "%s\n", line);
	}
	free(data);
	for (int i = 0; i < n; i++)
		free(want[i]);
	free(want);

	if (dropped)
		kb_write_all(file, out.p, out.n);
	kb_buf_free(&out);
	free(file);
	return dropped;
}

/*
 * `depends = a b c`.
 *
 * Split on SPACES only. The shell pipeline this replaced ended in `tr ' '`, so
 * a TAB inside the list stayed part of its token — every recipe in the tree
 * uses single spaces, and reproducing the quirk costs nothing.
 */
int kp_depends(const char *portdir, char out[][128], int max)
{
	char *recipe = kb_path_join(portdir, "kpkgbuild");
	size_t len = 0;
	char *data = kb_read_all(recipe, &len);
	free(recipe);
	if (!data)
		return 0;

	int n = 0;
	for (char *line = data, *next; line && *line && n < max; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		if (strncmp(line, "depends", 7))
			continue;
		char *p = line + 7;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '=')
			continue;
		p++;
		while (*p == ' ' || *p == '\t')
			p++;

		for (char *t = p; *t && n < max;) {
			char *sp = strchr(t, ' ');
			if (sp)
				*sp = 0;
			if (*t)
				kb_strlcpy(out[n++], t, 128);
			if (!sp)
				break;
			t = sp + 1;
		}
		break;		/* no recipe has a second depends line */
	}
	free(data);
	return n;
}

/*
 * One declarative key out of a recipe, by name.
 *
 * kpkg's own parser (kp_decl) expands helpers and command-less substitutions
 * and is the authority; this is the cheap reader for consumers that want a
 * single literal field — the description for `kpkg info`, the version and the
 * `secdb =` override for `kdos cve`. It reads the file, it does not run it.
 */
void kp_recipe_key(const char *portdir, const char *key, char *out, size_t cap)
{
	size_t klen = strlen(key);
	out[0] = 0;
	char *recipe = kb_path_join(portdir, "kpkgbuild");
	size_t len = 0;
	char *data = kb_read_all(recipe, &len);
	free(recipe);
	if (!data)
		return;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (strncmp(line, key, klen))
			continue;
		char *p = line + klen;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '=')
			continue;
		p++;
		while (*p == ' ' || *p == '\t')
			p++;
		kb_strlcpy(out, p, cap);
		break;
	}
	free(data);
}

void kp_description(const char *portdir, char *out, size_t cap)
{
	kp_recipe_key(portdir, "description", out, cap);
}
