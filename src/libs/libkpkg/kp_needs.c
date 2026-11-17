/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — run-time needs: what a package's installed files need
 *
 * The record and what provides each kind of edge are described in kpkg.h.
 * Three rules hold the record to "what the package needs at run time":
 *
 *  - Only the run-time facet is read. A header names nothing a machine runs,
 *    and a test script under site-packages/tests imports pytest, which no
 *    program of the package does: a `.py` file under a `tests`, `test` or
 *    `testing` directory, a `conftest.py`, `test_*.py` or `*_test.py` is not
 *    read.
 *  - A Python import is read only where it runs on every use: in an
 *    executable script whose `#!` names a Python, and in a top-level
 *    package's `__init__.py` or a top-level module of site-packages (py_entry
 *    says why not a submodule); at module level, column 0, outside a
 *    triple-quoted string. An import inside `try:` or `if TYPE_CHECKING:` is
 *    indented, which is what makes it optional, and is never recorded; nor
 *    is a name any `.py` file or package directory of the package itself
 *    carries.
 *  - A `py` edge is recorded only when a site-packages directory of the root
 *    holds the name at install time: a module of the standard library, or one
 *    a program carries beside itself, provides nothing a package could, and
 *    an edge to it would be reported as provided by nothing for ever.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kpkg.h"

typedef struct {
	char **v;
	int n, cap;
} Strs;

static void strs_add(Strs *x, char *s)
{
	if (x->n == x->cap) {
		x->cap = x->cap ? x->cap * 2 : 64;
		char **nv = kb_calloc((size_t)x->cap + 1, sizeof(*nv));
		if (x->n)
			memcpy(nv, x->v, (size_t)x->n * sizeof(*nv));
		free(x->v);
		x->v = nv;
	}
	x->v[x->n++] = s;
	x->v[x->n] = NULL;
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static void strs_sort(Strs *x)
{
	if (x->n)
		qsort(x->v, (size_t)x->n, sizeof(*x->v), cmp_str);
}

static int strs_has(const Strs *x, const char *s)
{
	return x->n && bsearch(&s, x->v, (size_t)x->n, sizeof(*x->v),
			       cmp_str) != NULL;
}

static void strs_free(Strs *x)
{
	for (int i = 0; i < x->n; i++)
		free(x->v[i]);
	free(x->v);
	memset(x, 0, sizeof(*x));
}

static const char *root_of(const KpConf *c)
{
	return c->root[0] ? c->root : "/";
}

static int present(const char *root, const char *rel)
{
	char *p = kb_path_join(root, rel);
	struct stat st;
	int ok = lstat(p, &st) == 0;
	free(p);
	return ok;
}

static int is_dir_at(const char *root, const char *rel)
{
	char *p = kb_path_join(root, rel);
	int d = kb_is_dir(p);
	free(p);
	return d;
}

/* The loader's directories, relative: etc/ld-musl-x86_64.path, or musl's
 * default when the root has none. */
static void ld_dirs(const char *root, Strs *out)
{
	char *p = kb_path_join(root, "etc/ld-musl-x86_64.path");
	char *d = kb_read_all(p, NULL);
	free(p);
	if (d) {
		for (char *s = d; *s;) {
			size_t k = strcspn(s, ":\n");
			char *w = s;
			s += k;
			if (*s)
				*s++ = 0;
			while (*w == '/')
				w++;
			size_t wl = strlen(w);
			while (wl && (w[wl - 1] == '/' || w[wl - 1] == ' '))
				w[--wl] = 0;
			if (wl)
				strs_add(out, kb_strdup(w));
		}
		free(d);
	}
	if (!out->n) {
		strs_add(out, kb_strdup("lib"));
		strs_add(out, kb_strdup("usr/local/lib"));
		strs_add(out, kb_strdup("usr/lib"));
	}
}

/* usr/lib/python3.<minor>/site-packages of the root, each that exists. */
static void site_dirs(const char *root, Strs *out)
{
	char *lib = kb_path_join(root, "usr/lib");
	char **e = kb_listdir(lib, NULL);
	free(lib);
	for (char **x = e; x && *x; x++) {
		if (strncmp(*x, "python3.", 8))
			continue;
		KbBuf b = {0};
		kb_buf_printf(&b, "usr/lib/%s/site-packages", *x);
		if (is_dir_at(root, b.p))
			strs_add(out, b.p);
		else
			kb_buf_free(&b);
	}
	kb_strv_free(e);
}

static int ident_char(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
	       (ch >= '0' && ch <= '9') || ch == '_';
}

static int ident_start(char ch)
{
	return ident_char(ch) && !(ch >= '0' && ch <= '9');
}

/* ── what provides an edge ─────────────────────────────────────────────── */

static void where_py(const char *root, const char *name, Strs *out)
{
	Strs sp = {0};
	site_dirs(root, &sp);
	size_t nl = strlen(name);
	for (int i = 0; i < sp.n; i++) {
		KbBuf b = {0};
		kb_buf_printf(&b, "%s/%s/__init__.py", sp.v[i], name);
		if (present(root, b.p))
			strs_add(out, kb_strdup(b.p));
		kb_buf_free(&b);
		kb_buf_printf(&b, "%s/%s.py", sp.v[i], name);
		if (present(root, b.p))
			strs_add(out, kb_strdup(b.p));
		kb_buf_free(&b);

		char *full = kb_path_join(root, sp.v[i]);
		char **e = kb_listdir(full, NULL);
		free(full);
		for (char **x = e; x && *x; x++) {
			size_t xl = strlen(*x);
			if (strncmp(*x, name, nl) || (*x)[nl] != '.' || xl < 3 ||
			    strcmp(*x + xl - 3, ".so"))
				continue;
			kb_buf_printf(&b, "%s/%s", sp.v[i], *x);
			strs_add(out, kb_strdup(b.p));
			kb_buf_free(&b);
		}
		kb_strv_free(e);

		/* A namespace package: a directory with no __init__.py, whose
		 * files are what some package installed. */
		kb_buf_printf(&b, "%s/%s", sp.v[i], name);
		KbBuf init = {0};
		kb_buf_printf(&init, "%s/__init__.py", b.p);
		if (is_dir_at(root, b.p) && !present(root, init.p)) {
			char *d = kb_path_join(root, b.p);
			char **f = kb_listdir(d, NULL);
			for (char **x = f; x && *x; x++) {
				KbBuf q = {0};
				kb_buf_printf(&q, "%s/%s", b.p, *x);
				char *qf = kb_path_join(root, q.p);
				struct stat st;
				if (lstat(qf, &st) == 0 && !S_ISDIR(st.st_mode))
					strs_add(out, kb_strdup(q.p));
				free(qf);
				kb_buf_free(&q);
			}
			kb_strv_free(f);
			free(d);
		}
		kb_buf_free(&init);
		kb_buf_free(&b);
	}
	strs_free(&sp);
}

char **kp_needs_where(const KpConf *c, const char *edge, const char **pkg)
{
	const char *root = root_of(c);
	Strs out = {0};
	strs_add(&out, NULL);
	out.n = 0;
	if (pkg)
		*pkg = NULL;
	const char *v = strchr(edge, ' ');
	if (!v)
		return out.v;
	size_t kl = (size_t)(v - edge);
	v++;

	if (kl == 2 && !strncmp(edge, "so", 2)) {
		if (strchr(v, '/')) {
			while (*v == '/')
				v++;
			if (present(root, v))
				strs_add(&out, kb_strdup(v));
		} else {
			Strs ld = {0};
			ld_dirs(root, &ld);
			for (int i = 0; i < ld.n; i++) {
				KbBuf b = {0};
				kb_buf_printf(&b, "%s/%s", ld.v[i], v);
				if (present(root, b.p))
					strs_add(&out, b.p);
				else
					kb_buf_free(&b);
			}
			strs_free(&ld);
		}
	} else if (kl == 3 && !strncmp(edge, "exe", 3)) {
		while (*v == '/')
			v++;
		if (*v && present(root, v))
			strs_add(&out, kb_strdup(v));
	} else if (kl == 3 && !strncmp(edge, "qml", 3)) {
		char path[1024];
		kb_strlcpy(path, v, sizeof(path));
		for (char *p = path; *p; p++)
			if (*p == '.')
				*p = '/';
		static const char *const qmldirs[] = { "usr/lib/qt6/qml",
						       "usr/lib/qt5/qml" };
		for (int i = 0; i < 2; i++) {
			KbBuf b = {0};
			kb_buf_printf(&b, "%s/%s/qmldir", qmldirs[i], path);
			if (present(root, b.p))
				strs_add(&out, b.p);
			else
				kb_buf_free(&b);
		}
		size_t fl = strcspn(v, ".");
		if (!out.n && pkg &&
		    ((fl == 5 && (!strncmp(v, "QtQml", 5))) ||
		     (fl == 7 && !strncmp(v, "QtQuick", 7)) ||
		     (fl == 3 && !strncmp(v, "QML", 3))))
			*pkg = "qt6-qtdeclarative";
	} else if (kl == 2 && !strncmp(edge, "py", 2)) {
		where_py(root, v, &out);
	} else if (kl == 4 && !strncmp(edge, "need", 4)) {
		if (pkg)
			*pkg = v;
	}
	return out.v;
}

/* ── the scan ──────────────────────────────────────────────────────────── */

typedef struct {
	const KpConf *c;
	const char *root;
	const char *name;
	KpCanon canon;
	Strs own;	/* canonical bare paths of the manifest, dirs with `/` */
	Strs ownbase;	/* basenames of its non-directory lines             */
	Strs ownpy;	/* top-level names it installs under site-packages  */
	Strs ld;
	Strs pyseen;	/* names already asked about, unsorted              */
	Strs out;
} Scan;

static int owns(const Scan *s, const char *path)
{
	while (*path == '/')
		path++;
	char *k = kp_canon_path(&s->canon, path);
	int r = strs_has(&s->own, k);
	free(k);
	return r;
}

static void emit(Scan *s, const char *kind, const char *val)
{
	KbBuf b = {0};
	kb_buf_printf(&b, "%s %s", kind, val);
	strs_add(&s->out, b.p);
}

static void scan_elf(Scan *s, const char *line, const char *full)
{
	char **need = NULL, **search = NULL;
	if (kp_elf_dynamic(full, &need, &search) != 0)
		return;
	char origin[4096];
	kb_strlcpy(origin, line, sizeof(origin));	/* `/usr/lib/x.so` */
	char *sl = strrchr(origin, '/');
	if (sl)
		*sl = 0;
	for (char **n = need; *n; n++) {
		if (strchr(*n, '/')) {
			if (!owns(s, *n))
				emit(s, "so", *n);
			continue;
		}
		if (strs_has(&s->ownbase, *n))
			continue;
		int found = 0;
		for (int i = 0; i < s->ld.n && !found; i++) {
			KbBuf b = {0};
			kb_buf_printf(&b, "%s/%s", s->ld.v[i], *n);
			found = present(s->root, b.p);
			kb_buf_free(&b);
		}
		/* Not on the loader's path: the file's own RUNPATH, with
		 * $ORIGIN its directory, names where it is. */
		char at[4096] = "";
		for (char **r = search; !found && r && *r; r++) {
			char *list = kb_strdup(*r);
			for (char *p = list, *e; p && *p && !found; p = e) {
				e = strchr(p, ':');
				if (e)
					*e++ = 0;
				KbBuf d = {0};
				if (!strncmp(p, "$ORIGIN", 7))
					kb_buf_printf(&d, "%s%s", origin, p + 7);
				else if (!strncmp(p, "${ORIGIN}", 9))
					kb_buf_printf(&d, "%s%s", origin, p + 9);
				else
					kb_buf_str(&d, p);
				if (d.p && d.p[0] == '/') {
					snprintf(at, sizeof(at), "%s/%s", d.p, *n);
					found = present(s->root, at + 1);
					if (!found)
						at[0] = 0;
				}
				kb_buf_free(&d);
			}
			free(list);
		}
		if (at[0]) {
			if (!owns(s, at))
				emit(s, "so", at);
		} else {
			emit(s, "so", *n);
		}
	}
	kb_strv_free(need);
	kb_strv_free(search);
}

/* The command `env` runs from a `#!` line's words: the first that is not
 * an option or an assignment; `-S` splits the next word. */
static const char *env_command(char **w, int n)
{
	for (int i = 1; i < n; i++) {
		const char *a = w[i];
		if (!strcmp(a, "-S") || !strcmp(a, "--split-string"))
			continue;
		if (!strncmp(a, "-S", 2))
			return a[2] ? a + 2 : NULL;
		if (a[0] == '-' || strchr(a, '='))
			continue;
		return a;
	}
	return NULL;
}

/* 1 when the interpreter is a Python, so the script's imports are read. */
static int python_name(const char *path)
{
	const char *b = strrchr(path, '/');
	return !strncmp(b ? b + 1 : path, "python", 6);
}

static int scan_shebang(Scan *s, const char *head)
{
	char line[256];
	size_t k = strcspn(head + 2, "\n");
	if (k >= sizeof(line))
		k = sizeof(line) - 1;
	memcpy(line, head + 2, k);
	line[k] = 0;
	char *w[16];
	int n = 0;
	char *save = NULL;
	for (char *p = strtok_r(line, " \t\r", &save); p && n < 16;
	     p = strtok_r(NULL, " \t\r", &save))
		w[n++] = p;
	if (!n || w[0][0] != '/')
		return 0;
	if (!owns(s, w[0]))
		emit(s, "exe", w[0]);
	const char *base = strrchr(w[0], '/') + 1;
	if (strcmp(base, "env"))
		return python_name(w[0]);
	const char *cmd = env_command(w, n);
	if (!cmd)
		return 0;
	char cw[256];
	kb_strlcpy(cw, cmd, sizeof(cw));
	cw[strcspn(cw, " \t")] = 0;
	if (cw[0] == '/') {
		if (!owns(s, cw))
			emit(s, "exe", cw);
		return python_name(cw);
	}
	if (strchr(cw, '/') || !cw[0])
		return 0;
	static const char *const path[] = { "usr/local/sbin", "usr/local/bin",
					    "usr/sbin", "usr/bin" };
	char at[600];
	snprintf(at, sizeof(at), "/usr/bin/%s", cw);
	for (int i = 0; i < 4; i++) {
		char rel[512];
		snprintf(rel, sizeof(rel), "%s/%s", path[i], cw);
		if (present(s->root, rel)) {
			snprintf(at, sizeof(at), "/%s", rel);
			break;
		}
	}
	if (!owns(s, at))
		emit(s, "exe", at);
	return python_name(cw);
}

static void scan_qml(Scan *s, const char *full)
{
	char *d = kb_read_all(full, NULL);
	if (!d)
		return;
	for (char *l = d, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (strncmp(l, "import", 6) || (l[6] != ' ' && l[6] != '\t'))
			continue;
		l += 6;
		while (*l == ' ' || *l == '\t')
			l++;
		if (!ident_start(*l))
			continue;	/* a quoted directory or script */
		size_t k = 0;
		while (ident_char(l[k]) || l[k] == '.')
			k++;
		l[k] = 0;
		char mod[512];
		kb_strlcpy(mod, l, sizeof(mod));
		size_t ml = strlen(mod);
		while (ml && mod[ml - 1] == '.')
			mod[--ml] = 0;
		char path[512];
		kb_strlcpy(path, mod, sizeof(path));
		for (char *p = path; *p; p++)
			if (*p == '.')
				*p = '/';
		int self = 0;
		static const char *const qmldirs[] = { "usr/lib/qt6/qml",
						       "usr/lib/qt5/qml" };
		for (int i = 0; i < 2 && !self; i++) {
			char q[1100];
			snprintf(q, sizeof(q), "%s/%s/qmldir", qmldirs[i], path);
			self = owns(s, q);
		}
		if (!self)
			emit(s, "qml", mod);
	}
	free(d);
}

/* A module whose imports run whenever its package is imported: a top-level
 * package's `__init__.py` or a top-level module, directly in site-packages.
 * A submodule's imports are the submodule's own feature and run only when
 * a program asks for it (lxml.html.html5parser, urllib3.contrib.pyopenssl);
 * read, they would name every optional dependency as a need. */
static int py_entry(const char *line)
{
	const char *sp = strstr(line, "/site-packages/");
	if (!sp)
		return 0;
	sp += strlen("/site-packages/");
	const char *sl = strchr(sp, '/');
	if (!sl)
		return 1;
	return !strcmp(sl, "/__init__.py");
}

static int py_test_path(const char *line)
{
	const char *base = strrchr(line, '/');
	base = base ? base + 1 : line;
	size_t bl = strlen(base);
	if (!strcmp(base, "conftest.py") || !strncmp(base, "test_", 5) ||
	    (bl > 8 && !strcmp(base + bl - 8, "_test.py")))
		return 1;
	return strstr(line, "/tests/") || strstr(line, "/test/") ||
	       strstr(line, "/testing/");
}

static void py_name(Scan *s, const char *line, const char *w, size_t k)
{
	char name[256];
	if (!k || k >= sizeof(name))
		return;
	memcpy(name, w, k);
	name[k] = 0;
	if (!strcmp(name, "__future__") || strs_has(&s->ownpy, name))
		return;
	/* A module beside the file is the file's own. */
	char dir[4096];
	kb_strlcpy(dir, line + 2, sizeof(dir));
	char *sl = strrchr(dir, '/');
	if (sl) {
		*sl = 0;
		char q[4400];
		snprintf(q, sizeof(q), "%s/%s.py", dir, name);
		if (owns(s, q))
			return;
		snprintf(q, sizeof(q), "%s/%s/", dir, name);
		if (owns(s, q))
			return;
	}
	for (int i = 0; i < s->pyseen.n; i++)
		if (!strcmp(s->pyseen.v[i], name))
			return;
	strs_add(&s->pyseen, kb_strdup(name));
	char edge[300];
	snprintf(edge, sizeof(edge), "py %s", name);
	char **at = kp_needs_where(s->c, edge, NULL);
	if (at[0])
		emit(s, "py", name);
	kb_strv_free(at);
}

static void scan_py(Scan *s, const char *line, const char *full)
{
	char *d = kb_read_all(full, NULL);
	if (!d)
		return;
	int quote = 0;	/* 0, or the character of the open triple quote */
	for (char *l = d, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		if (!quote && (!strncmp(l, "import ", 7) ||
			       !strncmp(l, "from ", 5))) {
			int from = l[0] == 'f';
			char *w = l + (from ? 5 : 7);
			for (;;) {
				while (*w == ' ' || *w == '\t' || *w == '(')
					w++;
				if (!ident_start(*w))
					break;	/* `from .x`, a relative import */
				size_t k = 0;
				while (ident_char(w[k]))
					k++;
				py_name(s, line, w, k);
				if (from)
					break;
				char *comma = strchr(w, ',');
				char *stop = w + strcspn(w, "#;\\");
				if (!comma || comma > stop)
					break;
				w = comma + 1;
			}
		}
		for (char *p = l; *p; p++) {
			if ((*p == '"' || *p == '\'') && p[1] == *p &&
			    p[2] == *p) {
				if (!quote)
					quote = *p;
				else if (quote == *p)
					quote = 0;
				p += 2;
			}
		}
	}
	free(d);
}

static void own_add(Scan *s, const char *line)
{
	const char *p = !strncmp(line, "./", 2) ? line + 2 : line;
	strs_add(&s->own, kp_canon_path(&s->canon, p));
	size_t ll = strlen(p);
	if (!ll || p[ll - 1] == '/') {
		const char *sp = strstr(p, "/site-packages/");
		if (sp) {
			sp += strlen("/site-packages/");
			size_t k = strcspn(sp, "/");
			if (k && sp[k] == '/' && !sp[k + 1]) {
				char *n = kb_calloc(k + 1, 1);
				memcpy(n, sp, k);
				strs_add(&s->ownpy, n);
			}
		}
		return;
	}
	const char *b = strrchr(p, '/');
	b = b ? b + 1 : p;
	strs_add(&s->ownbase, kb_strdup(b));
	/* Any module the package carries is its own wherever it is filed: a
	 * vendored copy (setuptools/_vendor/packaging) is on the path its
	 * importer sets up. */
	size_t bl = strlen(b);
	if (bl > 3 && !strcmp(b + bl - 3, ".py")) {
		if (!strcmp(b, "__init__.py") && b - p >= 2) {
			const char *e = b - 1, *a = e;
			while (a > p && a[-1] != '/')
				a--;
			char *n = kb_calloc((size_t)(e - a) + 1, 1);
			memcpy(n, a, (size_t)(e - a));
			strs_add(&s->ownpy, n);
		} else {
			char *n = kb_calloc(bl - 2, 1);
			memcpy(n, b, bl - 3);
			strs_add(&s->ownpy, n);
		}
	}
	const char *sp = strstr(p, "/site-packages/");
	if (sp) {
		sp += strlen("/site-packages/");
		size_t k = strcspn(sp, "/.");
		if (k && (sp[k] == '/' || !strcmp(sp + k, ".py") ||
			  (sp[k] == '.' && strlen(sp + k) >= 3 &&
			   !strcmp(sp + strlen(sp) - 3, ".so")))) {
			char *n = kb_calloc(k + 1, 1);
			memcpy(n, sp, k);
			strs_add(&s->ownpy, n);
		}
	}
}

char *kp_needs_scan(const KpConf *c, const char *name, const char *body,
		    const char *extra, const char *runtime)
{
	Scan s;
	memset(&s, 0, sizeof(s));
	s.c = c;
	s.root = root_of(c);
	s.name = name;
	kp_canon_load(c, &s.canon);
	ld_dirs(s.root, &s.ld);

	char *copy = kb_strdup(body ? body : "");
	Strs lines = {0};
	for (char *l = copy, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		if (*l) {
			strs_add(&lines, kb_strdup(l));
			own_add(&s, l);
		}
	}
	free(copy);
	strs_sort(&s.own);
	strs_sort(&s.ownbase);
	strs_sort(&s.ownpy);

	for (int i = 0; i < lines.n; i++) {
		const char *l = lines.v[i];
		size_t ll = strlen(l);
		if (strncmp(l, "./", 2) || l[ll - 1] == '/')
			continue;
		char *full = kb_path_join(s.root, l);
		struct stat st;
		if (lstat(full, &st) != 0 || !S_ISREG(st.st_mode) ||
		    kp_facet_of(l, NULL) != KP_FACET_RUN ||
		    (extra && kp_facet_glob(l, extra))) {
			free(full);
			continue;
		}
		char head[256] = "";
		int fd = open(full, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
		ssize_t hn = fd >= 0 ? read(fd, head, sizeof(head) - 1) : -1;
		if (fd >= 0)
			close(fd);
		head[hn > 0 ? hn : 0] = 0;
		int py = 0;
		if (hn >= 4 && !memcmp(head, "\177ELF", 4))
			scan_elf(&s, l + 1, full);
		else if (hn >= 2 && head[0] == '#' && head[1] == '!' &&
			 (st.st_mode & 0111))
			py = scan_shebang(&s, head);
		if (ll > 4 && !strcmp(l + ll - 4, ".qml"))
			scan_qml(&s, full);
		else if (py || (ll > 3 && !strcmp(l + ll - 3, ".py") &&
				py_entry(l) && !py_test_path(l)))
			scan_py(&s, l, full);
		free(full);
	}

	if (runtime) {
		char *r = kb_strdup(runtime), *save = NULL;
		for (char *w = strtok_r(r, " \t", &save); w;
		     w = strtok_r(NULL, " \t", &save))
			if (strcmp(w, name))
				emit(&s, "need", w);
		free(r);
	}

	strs_sort(&s.out);
	KbBuf b = {0};
	kb_buf_str(&b, "");
	for (int i = 0; i < s.out.n; i++)
		if (!i || strcmp(s.out.v[i], s.out.v[i - 1]))
			kb_buf_printf(&b, "%s\n", s.out.v[i]);
	strs_free(&lines);
	strs_free(&s.own);
	strs_free(&s.ownbase);
	strs_free(&s.ownpy);
	strs_free(&s.ld);
	strs_free(&s.pyseen);
	strs_free(&s.out);
	return b.p;
}

/* ── the record ────────────────────────────────────────────────────────── */

static char *needs_path(const KpConf *c, const char *name, int make)
{
	char *db = kp_db_dir(c);
	char *dir = kb_path_join(db, ".needs");
	if (make)
		kb_mkdir_p(dir);
	char *p = kb_path_join(dir, name);
	free(dir);
	free(db);
	return p;
}

/* Written to a temporary name and renamed; a failed write removes the old
 * record, which describes the previous version's files. */
int kp_record_needs(const KpConf *c, const char *name, const char *data,
		    size_t len)
{
	char *p = needs_path(c, name, 1);
	KbBuf tmp = {0};
	kb_buf_printf(&tmp, "%s.tmp", p);
	int rc = kb_write_all(tmp.p, data, len);
	if (rc == 0 && rename(tmp.p, p) != 0)
		rc = -1;
	if (rc != 0) {
		unlink(tmp.p);
		unlink(p);
	}
	kb_buf_free(&tmp);
	free(p);
	return rc;
}

char *kp_installed_needs(const KpConf *c, const char *name, size_t *len)
{
	char *p = needs_path(c, name, 0);
	char *data = kb_read_all(p, len);
	free(p);
	return data;
}

void kp_forget_needs(const KpConf *c, const char *name)
{
	char *p = needs_path(c, name, 0);
	unlink(p);
	free(p);
}
