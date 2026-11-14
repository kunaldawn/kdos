/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — package facets: which files of a package are for building
 *
 * One package file holds every file its build installed. A facet is a class
 * of those files, decided by the path alone (and, for a library symlink, its
 * target), so the same package can be installed whole or without what only a
 * compiler or a reader of manuals uses. The rules are a CONTRACT: kpkgadd
 * and `kpkg facets --strip` leave out exactly what they say, the image is
 * trimmed by them, and `kdos update install --dev` restores exactly them.
 *
 *   dev  everything under usr/include/
 *        a `.a` archive anywhere under usr/lib (usr/lib64): static libraries,
 *        gcc's libgcc.a among them. An archive elsewhere is run time:
 *        libclc's bitcode under usr/share/clc is read by Mesa's OpenCL.
 *        everything under usr/lib/cmake/, usr/lib64/cmake/, and
 *        usr/share/cmake/ except CMake's own Help/, Modules/, Templates/ and
 *        include/ there, which cmake itself reads every time it runs
 *        everything under usr/lib/pkgconfig/, usr/lib64/pkgconfig/,
 *        usr/share/pkgconfig/, usr/share/aclocal/, usr/share/gir-1.0/ and
 *        usr/share/vala[-<version>]/vapi/
 *        man sections 0, 2 and 3 (usr/share/man/man0..., man2..., man3...):
 *        the POSIX headers, the system calls and the library functions
 *        a SYMLINK named lib<x>.so directly in usr/lib (usr/lib64) whose
 *        target is that name plus a version (`libz.so -> libz.so.1.3.1`):
 *        the link `-lz` resolves through. A real file named lib<x>.so is run
 *        time — most plugins and every library built without a version are
 *        one — and so is a link to anything else.
 *   doc  everything under usr/share/doc/ except HTML/ (KDE's handbooks,
 *        which help:/ and the applications' Help menus open) and kdos/ and
 *        kfs/ (the two books, which the desktop links into); everything
 *        under usr/share/info/ and usr/share/gtk-doc/
 *   run  everything else, the man sections a terminal user reads (1, 4-8)
 *        among it
 *
 * A path is matched under either spelling of a merged-/usr name: `lib/x` is
 * `usr/lib/x`.
 *
 * A RECIPE MAY ONLY ADD TO THE DEV FACET: its `facet_dev =` line, globs
 * matched by kp_facet_glob against the path without `./` (fnmatch with no
 * flags, so `*` crosses `/`), moves a run-time file of its own package into
 * dev. gcc uses it for its compiler, which a machine that builds nothing
 * does not run; nothing can move a file out of dev or doc. What a program opens BY NAME at run time cannot be seen here:
 * one that dlopen()s an unversioned `libfoo.so` finds nothing where the dev
 * facet is left out, and the image's gates do not see a dlopen().
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>

#include "kpkg.h"

static int under(const char *p, const char *dir)
{
	return !strncmp(p, dir, strlen(dir));
}

int kp_facet_of(const char *path, const char *link)
{
	char buf[4096];
	const char *p = path;
	if (!strncmp(p, "./", 2))
		p += 2;
	/* The merged-/usr spellings, so `./lib/libz.a` is a static library. */
	if (under(p, "lib/") || under(p, "lib64/")) {
		snprintf(buf, sizeof(buf), "usr/%s", p);
		p = buf;
	}

	/* A directory goes with what it holds: kpkgadd and the strip drop
	 * one that held something left out and holds nothing kept. */
	size_t n = strlen(p);
	if (!n || p[n - 1] == '/')
		return KP_FACET_RUN;

	if (under(p, "usr/include/"))
		return KP_FACET_DEV;
	if (under(p, "usr/share/doc/")) {
		const char *s = p + strlen("usr/share/doc/");
		if (under(s, "HTML/") || under(s, "kdos/") || under(s, "kfs/") ||
		    !strcmp(s, "HTML") || !strcmp(s, "kdos") || !strcmp(s, "kfs"))
			return KP_FACET_RUN;
		return KP_FACET_DOC;
	}
	if (under(p, "usr/share/info/") || under(p, "usr/share/gtk-doc/"))
		return KP_FACET_DOC;
	if (under(p, "usr/share/man/man")) {
		char sec = p[strlen("usr/share/man/man")];
		if ((sec == '0' || sec == '2' || sec == '3') &&
		    strchr(p + strlen("usr/share/man/man"), '/'))
			return KP_FACET_DEV;
		return KP_FACET_RUN;
	}
	if (under(p, "usr/lib/cmake/") || under(p, "usr/lib64/cmake/") ||
	    under(p, "usr/lib/pkgconfig/") || under(p, "usr/lib64/pkgconfig/") ||
	    under(p, "usr/share/pkgconfig/") || under(p, "usr/share/aclocal/") ||
	    under(p, "usr/share/gir-1.0/"))
		return KP_FACET_DEV;
	if (under(p, "usr/share/cmake/")) {
		const char *s = p + strlen("usr/share/cmake/");
		if (under(s, "Help/") || under(s, "Modules/") ||
		    under(s, "Templates/") || under(s, "include/"))
			return KP_FACET_RUN;
		return KP_FACET_DEV;
	}
	if (!fnmatch("usr/share/vala*/vapi/*", p, 0))
		return KP_FACET_DEV;

	const char *base = strrchr(p, '/');
	base = base ? base + 1 : p;
	size_t bl = strlen(base);
	if ((under(p, "usr/lib/") || under(p, "usr/lib64/")) && bl > 2 &&
	    !strcmp(base + bl - 2, ".a"))
		return KP_FACET_DEV;

	size_t dl = (size_t)(base - p);
	int libdir = (dl == strlen("usr/lib/") && under(p, "usr/lib/")) ||
		     (dl == strlen("usr/lib64/") && under(p, "usr/lib64/"));
	if (link && libdir && bl > 6 && !strncmp(base, "lib", 3) &&
	    !strcmp(base + bl - 3, ".so")) {
		const char *t = strrchr(link, '/');
		t = t ? t + 1 : link;
		if (!strncmp(t, base, bl) && t[bl] == '.' && t[bl + 1])
			return KP_FACET_DEV;
	}
	return KP_FACET_RUN;
}

int kp_facet_glob(const char *path, const char *globs)
{
	const char *p = !strncmp(path, "./", 2) ? path + 2 : path;
	size_t n = strlen(p);
	if (!n || p[n - 1] == '/' || !globs)
		return 0;
	char g[512];
	for (const char *s = globs; *s;) {
		while (*s == ' ' || *s == '\t')
			s++;
		size_t k = strcspn(s, " \t");
		if (k && k < sizeof(g)) {
			memcpy(g, s, k);
			g[k] = 0;
			if (!fnmatch(g, p, 0))
				return 1;
		}
		s += k;
	}
	return 0;
}

const char *kp_facet_name(int facet)
{
	return facet == KP_FACET_DEV ? "dev" : facet == KP_FACET_DOC ? "doc"
								    : "run";
}

/* ── the record of what was left out ───────────────────────────────────── */

static char *facets_path(const KpConf *c, const char *name, int make)
{
	char *db = kp_db_dir(c);
	char *dir = kb_path_join(db, ".facets");
	if (make)
		kb_mkdir_p(dir);
	char *p = kb_path_join(dir, name);
	free(dir);
	free(db);
	return p;
}

/* Written to a temporary name and renamed, like the file record: a reader
 * never sees half of one. */
int kp_record_facets(const KpConf *c, const char *name, const char *data,
		     size_t len)
{
	char *p = facets_path(c, name, 1);
	KbBuf tmp = {0};
	kb_buf_printf(&tmp, "%s.tmp", p);
	int rc = kb_write_all(tmp.p, data, len);
	if (rc == 0 && rename(tmp.p, p) != 0)
		rc = -1;
	if (rc != 0)
		unlink(tmp.p);
	kb_buf_free(&tmp);
	free(p);
	return rc;
}

char *kp_installed_facets(const KpConf *c, const char *name, size_t *len)
{
	char *p = facets_path(c, name, 0);
	char *data = kb_read_all(p, len);
	free(p);
	return data;
}

void kp_forget_facets(const KpConf *c, const char *name)
{
	char *p = facets_path(c, name, 0);
	unlink(p);
	free(p);
}

int kp_facets_left(const KpConf *c, const char *name, int facet)
{
	char *d = kp_installed_facets(c, name, NULL);
	if (!d)
		return -1;
	const char *w = kp_facet_name(facet);
	size_t wl = strlen(w);
	int n = 0;
	for (const char *l = d; *l;) {
		if (!strncmp(l, w, wl) && l[wl] == ' ')
			n++;
		const char *nl = strchr(l, '\n');
		if (!nl)
			break;
		l = nl + 1;
	}
	free(d);
	return n;
}

/* Every compile reads the C library's headers and the kernel's and links
 * gcc's libgcc.a, and no recipe names the three in `depends =`. */
static const char *const IMPLICIT[] = { "musl", "linux", "gcc", NULL };

char **kp_facet_wanted(const KpConf *c, const char *port)
{
	/* A database with no record anywhere (every build chroot) answers
	 * without walking the closure. */
	char *db = kp_db_dir(c);
	char *rdir = kb_path_join(db, ".facets");
	int any = kb_is_dir(rdir);
	free(rdir);
	free(db);
	if (!any)
		return kb_calloc(1, sizeof(char *));

	char **cl = kp_store_closure(c, port);
	int n = 0, cap = 8;
	char **out = kb_calloc((size_t)cap + 1, sizeof(*out));
	int ncl = 0;
	while (cl[ncl])
		ncl++;
	for (int i = 0; i < ncl + 3; i++) {
		const char *name = i < ncl ? cl[i] : IMPLICIT[i - ncl];
		int dup = 0;
		for (int k = 0; k < n && !dup; k++)
			dup = !strcmp(out[k], name);
		if (dup || !strcmp(name, port) || !kp_installed(c, name) ||
		    kp_facets_left(c, name, KP_FACET_DEV) <= 0)
			continue;
		if (n == cap) {
			cap *= 2;
			char **nv = kb_calloc((size_t)cap + 1, sizeof(*nv));
			memcpy(nv, out, (size_t)n * sizeof(*nv));
			free(out);
			out = nv;
		}
		out[n++] = kb_strdup(name);
	}
	kb_strv_free(cl);
	return out;
}
