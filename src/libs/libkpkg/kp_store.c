/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpkg — the content-keyed package store
 *
 * The key and the META format are described in kpkg.h. Three rules hold the
 * store to "a hit is the package a build would have made":
 *
 *  - A key that cannot be computed is UNKNOWN, never a default. A dependency
 *    installed without a `.pkgsha` has an unknown identity, and a key that
 *    wrote it as `-` would match a store entry built against a different one.
 *  - The key sees only what a port DECLARES. A library the build found by
 *    itself is invisible to it, so the ELF files are read after the install
 *    and each undeclared owner becomes an X: line the lookup re-checks.
 *  - Every write lands under a temporary name and is renamed, META last. An
 *    interrupted write leaves a directory with no META, which is not an
 *    entry.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kpkg.h"

extern char **environ;

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* ──────────────────────────────────────────────────────────────────────── */

int kp_store_env_denied(const char *name)
{
	static const char *const exact[] = {
		"TERM", "PWD", "OLDPWD", "SHLVL", "_", "MAKEFLAGS",
		"NINJAFLAGS", "KDOS_JOBS", "CMAKE_BUILD_PARALLEL_LEVEL",
		"CARGO_BUILD_JOBS", "KDOS_CCACHE", "CMAKE_C_COMPILER_LAUNCHER",
		"CMAKE_CXX_COMPILER_LAUNCHER", "KDOS_REPLAY",
		"KDOS_ISO_SOURCES", "KDOS_ISO_COMP", "KDOS_PACK_KDOS",
		"KDOS_MAKE_BINHOST", "MARK", "CHROOT", "PORT_REPO", NULL
	};
	static const char *const prefix[] = {
		"CCACHE_", "KPKG_", "KDOS_PKG_STORE", "KDOS_PHASE_",
		"KDOS_SNAPSHOT_", NULL
	};
	for (int i = 0; exact[i]; i++)
		if (!strcmp(name, exact[i]))
			return 1;
	for (int i = 0; prefix[i]; i++)
		if (!strncmp(name, prefix[i], strlen(prefix[i])))
			return 1;
	return 0;
}

static void env_hash(char out[65])
{
	int n = 0;
	for (char **e = environ; e && *e; e++)
		n++;
	char **v = kb_calloc((size_t)n + 1, sizeof(*v));
	int k = 0;
	for (char **e = environ; e && *e; e++) {
		const char *eq = strchr(*e, '=');
		size_t nl = eq ? (size_t)(eq - *e) : strlen(*e);
		char name[256];
		if (nl >= sizeof(name))
			nl = sizeof(name) - 1;
		memcpy(name, *e, nl);
		name[nl] = 0;
		if (!kp_store_env_denied(name))
			v[k++] = *e;
	}
	qsort(v, (size_t)k, sizeof(*v), cmp_str);
	KbSha256 s;
	kb_sha256_init(&s);
	for (int i = 0; i < k; i++) {
		kb_sha256_update(&s, v[i], strlen(v[i]));
		kb_sha256_update(&s, "\n", 1);
	}
	kb_sha256_final(&s, out);
	free(v);
}

/* ──────────────────────────────────────────────────────────────────────── */

static int strv_has(char **v, int n, const char *s)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(v[i], s))
			return 1;
	return 0;
}

char **kp_store_closure(const KpConf *c, const char *name)
{
	int cap = 256, n = 0, done = 0;
	char **v = kb_calloc((size_t)cap + 1, sizeof(*v));
	char (*deps)[128] = malloc(sizeof(char[KP_MAX_DEPS][128]));

	/* Breadth-first over a list that is also the seen-set: every name is
	 * appended once, so a cycle ends when it comes back round. */
	const char *next = name;
	for (;;) {
		char *dir = deps ? kp_port_dir(c, next) : NULL;
		int nd = dir ? kp_depends(dir, deps, KP_MAX_DEPS) : 0;
		free(dir);
		for (int i = 0; i < nd; i++) {
			if (!strcmp(deps[i], name) || strv_has(v, n, deps[i]))
				continue;
			if (n == cap) {
				cap *= 2;
				char **nv = kb_calloc((size_t)cap + 1,
						      sizeof(*nv));
				memcpy(nv, v, (size_t)n * sizeof(*nv));
				free(v);
				v = nv;
			}
			v[n++] = kb_strdup(deps[i]);
		}
		if (done >= n)
			break;
		next = v[done++];
	}
	free(deps);

	const char *base = getenv("KPKG_STORE_BASE");
	char word[128];
	for (const char *p = base ? base : ""; *p;) {
		while (*p == ' ' || *p == '\t' || *p == '\n')
			p++;
		size_t k = 0;
		while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
			if (k + 1 < sizeof(word))
				word[k++] = *p;
			p++;
		}
		word[k] = 0;
		if (!k || !strcmp(word, name) || strv_has(v, n, word))
			continue;
		if (n == cap) {
			cap *= 2;
			char **nv = kb_calloc((size_t)cap + 1, sizeof(*nv));
			memcpy(nv, v, (size_t)n * sizeof(*nv));
			free(v);
			v = nv;
		}
		v[n++] = kb_strdup(word);
	}
	qsort(v, (size_t)n, sizeof(*v), cmp_str);
	v[n] = NULL;
	return v;
}

int kp_store_key(const KpConf *c, const char *name, const char *portdir,
		 int transient, char *const *closure, char out[65], char *why,
		 size_t wcap)
{
	char recipe[65], env[65], sha[65];
	if (kp_recipe_hash(portdir, recipe) != 0) {
		snprintf(why, wcap, "%s has no recipe hash", name);
		return -1;
	}
	env_hash(env);
	const char *salt = getenv("KPKG_STORE_SALT");
	if (!salt || !*salt) {
		snprintf(why, wcap, "no KPKG_STORE_SALT");
		return -1;
	}

	KbBuf b = {0};
	kb_buf_printf(&b, "format=%d\nrecipe=%s\npack=%s\nsalt=%s\nenv=%s\n",
		      KP_STORE_FORMAT, recipe, transient ? "transient" : "kept",
		      salt, env);
	for (int i = 0; closure && closure[i]; i++) {
		if (!kp_installed(c, closure[i])) {
			kb_buf_printf(&b, "dep=%s -\n", closure[i]);
			continue;
		}
		if (kp_installed_pkg_sha(c, closure[i], sha) != 0) {
			snprintf(why, wcap, "%s is installed with no .pkgsha",
				 closure[i]);
			kb_buf_free(&b);
			return -1;
		}
		kb_buf_printf(&b, "dep=%s %s\n", closure[i], sha);
	}
	KbSha256 s;
	kb_sha256_init(&s);
	kb_sha256_update(&s, b.p, b.n);
	kb_sha256_final(&s, out);
	kb_buf_free(&b);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int rd(int fd, void *buf, size_t n, uint64_t off)
{
	return pread(fd, buf, n, (off_t)off) == (ssize_t)n ? 0 : -1;
}

static uint16_t u16(const unsigned char *p)
{
	return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t u32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

static uint64_t u64(const unsigned char *p)
{
	return (uint64_t)u32(p) | (uint64_t)u32(p + 4) << 32;
}

/* The file offset of virtual address `va`, through the PT_LOAD headers. */
static int va_to_off(int fd, uint64_t phoff, int phnum, uint64_t va,
		     uint64_t *off)
{
	unsigned char ph[56];
	for (int i = 0; i < phnum; i++) {
		if (rd(fd, ph, sizeof(ph), phoff + (uint64_t)i * 56) != 0)
			return -1;
		if (u32(ph) != 1)	/* PT_LOAD */
			continue;
		uint64_t po = u64(ph + 8), pv = u64(ph + 16),
			 fs = u64(ph + 32);
		if (va >= pv && va < pv + fs) {
			*off = po + (va - pv);
			return 0;
		}
	}
	return -1;
}

char **kp_store_needed(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return NULL;
	unsigned char eh[64];
	char **v = NULL;
	/* ELF, 64-bit, little-endian; anything else links nothing we key. */
	if (rd(fd, eh, sizeof(eh), 0) != 0 || memcmp(eh, "\177ELF", 4) ||
	    eh[4] != 2 || eh[5] != 1 || u16(eh + 54) != 56)
		goto out;

	uint64_t phoff = u64(eh + 32);
	int phnum = u16(eh + 56);
	uint64_t dynoff = 0, dynsz = 0;
	unsigned char ph[56];
	for (int i = 0; i < phnum; i++) {
		if (rd(fd, ph, sizeof(ph), phoff + (uint64_t)i * 56) != 0)
			goto out;
		if (u32(ph) == 2) {	/* PT_DYNAMIC */
			dynoff = u64(ph + 8);
			dynsz = u64(ph + 32);
			break;
		}
	}
	if (!dynsz || dynsz > (1u << 20))
		goto out;

	unsigned char *dyn = malloc(dynsz);
	if (!dyn || rd(fd, dyn, dynsz, dynoff) != 0) {
		free(dyn);
		goto out;
	}
	uint64_t strtab = 0, strsz = 0;
	int nneed = 0;
	for (uint64_t o = 0; o + 16 <= dynsz; o += 16) {
		uint64_t tag = u64(dyn + o), val = u64(dyn + o + 8);
		if (tag == 0)
			break;
		if (tag == 5)
			strtab = val;
		else if (tag == 10)
			strsz = val;
		else if (tag == 1)
			nneed++;
	}
	uint64_t stroff;
	if (!nneed || !strtab || !strsz || strsz > (16u << 20) ||
	    va_to_off(fd, phoff, phnum, strtab, &stroff) != 0) {
		free(dyn);
		goto out;
	}
	char *str = malloc(strsz + 1);
	if (!str || rd(fd, str, strsz, stroff) != 0) {
		free(str);
		free(dyn);
		goto out;
	}
	str[strsz] = 0;
	v = kb_calloc((size_t)nneed + 1, sizeof(*v));
	int k = 0;
	for (uint64_t o = 0; o + 16 <= dynsz && k < nneed; o += 16) {
		uint64_t tag = u64(dyn + o), val = u64(dyn + o + 8);
		if (tag == 0)
			break;
		if (tag == 1 && val < strsz)
			v[k++] = kb_strdup(str + val);
	}
	v[k] = NULL;
	free(str);
	free(dyn);
out:
	close(fd);
	return v;
}

int kp_store_links(const KpConf *c, const char *name, char *const *closure,
		   KbBuf *x)
{
	char *db = kp_db_dir(c);
	char *entry = kb_path_join(db, name);
	free(db);
	char *man = kb_read_all(entry, NULL);
	free(entry);
	if (!man)
		return 0;

	const char *root = c->root[0] ? c->root : "/";
	static const char *const libdirs[] = { "usr/lib", "lib",
					       "usr/local/lib" };
	int cap = 64, n = 0;
	char **ask = kb_calloc((size_t)cap, sizeof(*ask));

	char *body = strchr(man, '\n');
	for (char *l = body ? body + 1 : NULL, *e; l && *l; l = e ? e + 1 : NULL) {
		e = strchr(l, '\n');
		if (e)
			*e = 0;
		size_t ll = strlen(l);
		if (!ll || l[ll - 1] == '/')
			continue;
		char *path = kb_path_join(root, l);
		struct stat st;
		char **need = lstat(path, &st) == 0 && S_ISREG(st.st_mode)
				      ? kp_store_needed(path)
				      : NULL;
		free(path);
		for (char **s = need; s && *s; s++) {
			for (int d = 0; d < 3; d++) {
				char rel[640];
				snprintf(rel, sizeof(rel), "%s/%s", libdirs[d],
					 *s);
				char *full = kb_path_join(root, rel);
				int there = kb_path_exists(full);
				free(full);
				if (!there)
					continue;
				if (!strv_has(ask, n, rel)) {
					if (n == cap) {
						cap *= 2;
						char **nv = kb_calloc(
							(size_t)cap,
							sizeof(*nv));
						memcpy(nv, ask,
						       (size_t)n * sizeof(*nv));
						free(ask);
						ask = nv;
					}
					ask[n++] = kb_strdup(rel);
				}
				break;
			}
		}
		kb_strv_free(need);
	}
	free(man);

	int rc = 0;
	KpOwned *owned = n ? kp_owned_load_some(c, ask, n) : NULL;
	char **seen = kb_calloc((size_t)n + 1, sizeof(*seen));
	int nseen = 0;
	for (int i = 0; i < n; i++) {
		const char *o = kp_owned_owner(owned, ask[i]);
		if (!o || !strcmp(o, name) || strv_has(seen, nseen, o))
			continue;
		int declared = 0;
		for (int j = 0; closure && closure[j]; j++)
			if (!strcmp(closure[j], o))
				declared = 1;
		if (declared)
			continue;
		seen[nseen++] = (char *)o;
		printf("%s links %s without declaring it\n", name, o);
		char sha[65];
		if (kp_installed_pkg_sha(c, o, sha) != 0) {
			rc = -1;
			continue;
		}
		kb_buf_printf(x, "X:%s %s\n", o, sha);
	}
	fflush(stdout);
	free(seen);
	kp_owned_free(owned);
	for (int i = 0; i < n; i++)
		free(ask[i]);
	free(ask);
	return rc;
}

/* ──────────────────────────────────────────────────────────────────────── */

static char *entry_dir(const char *store, const char *key)
{
	char sub[80];
	snprintf(sub, sizeof(sub), "%.2s/%s", key, key);
	return kb_path_join(store, sub);
}

int kp_store_lookup(const KpConf *c, const char *store, const char *key,
		    char *file, size_t fcap, char sha[65])
{
	char *dir = entry_dir(store, key);
	char *meta = kb_path_join(dir, "META");
	char *data = kb_read_whole(meta, NULL);
	free(meta);
	int ok = data != NULL;
	char want[65] = "", fname[512] = "";

	for (char *l = data, *e; ok && l && *l; l = e ? e + 1 : NULL) {
		e = strchr(l, '\n');
		if (e)
			*e = 0;
		if (!strncmp(l, "F:", 2))
			kb_strlcpy(fname, l + 2, sizeof(fname));
		else if (!strncmp(l, "C:", 2))
			kb_strlcpy(want, l + 2, sizeof(want));
		else if (!strncmp(l, "X:", 2)) {
			char who[256], h[65], have[65];
			if (sscanf(l + 2, "%255s %64s", who, h) != 2 ||
			    kp_installed_pkg_sha(c, who, have) != 0 ||
			    strcmp(h, have))
				ok = 0;
		}
	}
	free(data);
	if (ok && (!fname[0] || strchr(fname, '/') || strlen(want) != 64))
		ok = 0;
	if (ok) {
		char *p = kb_path_join(dir, fname);
		char got[65];
		if (kb_sha256_file(p, got) != 0 || strcmp(got, want))
			ok = 0;
		else {
			kb_strlcpy(file, p, fcap);
			memcpy(sha, got, 65);
		}
		free(p);
	}
	free(dir);
	return ok;
}

/* `<dir>/<final>` from `src` through `<dir>/.tmp.<pid>`. */
static int place(const char *dir, const char *final, const char *src,
		 const char *text)
{
	char tmp[64];
	snprintf(tmp, sizeof(tmp), ".tmp.%d", (int)getpid());
	char *t = kb_path_join(dir, tmp);
	char *f = kb_path_join(dir, final);
	int rc = src ? kb_copy_file(src, t)
		     : kb_write_all(t, text, strlen(text));
	if (rc == 0 && rename(t, f) != 0)
		rc = -1;
	if (rc != 0)
		unlink(t);
	free(t);
	free(f);
	return rc;
}

int kp_store_put(const char *store, const char *key, const char *name,
		 const char *pkgfile, const char *x)
{
	char sha[65];
	if (kb_sha256_file(pkgfile, sha) != 0)
		return -1;
	char *dir = entry_dir(store, key);
	kb_mkdir_p(dir);
	char *meta = kb_path_join(dir, "META");
	/* The old META goes first: from here until the new one lands the
	 * directory is not an entry, whatever else it holds. */
	unlink(meta);
	free(meta);

	/* Anything left from an earlier entry under this key is a package
	 * the new META does not name. */
	char **old = kb_listdir(dir, NULL);
	for (char **o = old; o && *o; o++) {
		char *p = kb_path_join(dir, *o);
		unlink(p);
		free(p);
	}
	kb_strv_free(old);

	const char *base = kb_basename(pkgfile);
	/* name-version-release.tar.xz, split from the right the way kpkgadd
	 * does, for the P V R lines. */
	char ver[256] = "", rel[64] = "";
	char stem[512];
	kb_strlcpy(stem, base, sizeof(stem));
	char *dot = strstr(stem, ".tar.");
	if (dot)
		*dot = 0;
	char *r = strrchr(stem, '-');
	if (r) {
		kb_strlcpy(rel, r + 1, sizeof(rel));
		*r = 0;
		char *v = strrchr(stem, '-');
		if (v)
			kb_strlcpy(ver, v + 1, sizeof(ver));
	}

	int rc = place(dir, base, pkgfile, NULL);
	if (rc == 0) {
		KbBuf m = {0};
		kb_buf_printf(&m, "P:%s\nV:%s\nR:%s\nF:%s\nC:%s\n%s", name,
			      ver, rel, base, sha, x ? x : "");
		rc = place(dir, "META", NULL, m.p);
		kb_buf_free(&m);
	}
	free(dir);
	return rc;
}

void kp_store_touch(const char *store, const char *key)
{
	char *dir = entry_dir(store, key);
	char *meta = kb_path_join(dir, "META");
	utimensat(AT_FDCWD, meta, NULL, 0);
	free(meta);
	free(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char *dir;
	time_t used;		/* META mtime; 0 for no META */
	unsigned long long bytes;
} Entry;

static int cmp_entry(const void *a, const void *b)
{
	const Entry *x = a, *y = b;
	if (x->used != y->used)
		return x->used < y->used ? -1 : 1;
	return strcmp(x->dir, y->dir);
}

int kp_store_gc(const char *store, unsigned long long cap)
{
	char **top = kb_listdir(store, NULL);
	if (!top)
		return -1;
	int n = 0, ecap = 256;
	Entry *e = kb_calloc((size_t)ecap, sizeof(*e));
	unsigned long long total = 0;
	for (char **t = top; *t; t++) {
		char *sub = kb_path_join(store, *t);
		char **keys = kb_is_dir(sub) ? kb_listdir(sub, NULL) : NULL;
		for (char **k = keys; k && *k; k++) {
			char *d = kb_path_join(sub, *k);
			if (!kb_is_dir(d)) {
				free(d);
				continue;
			}
			if (n == ecap) {
				ecap *= 2;
				Entry *ne = kb_calloc((size_t)ecap, sizeof(*ne));
				memcpy(ne, e, (size_t)n * sizeof(*ne));
				free(e);
				e = ne;
			}
			Entry *x = &e[n++];
			x->dir = d;
			char **files = kb_listdir(d, NULL);
			for (char **f = files; f && *f; f++) {
				char *p = kb_path_join(d, *f);
				struct stat st;
				if (lstat(p, &st) == 0 && S_ISREG(st.st_mode)) {
					x->bytes += (unsigned long long)st.st_size;
					if (!strcmp(*f, "META"))
						x->used = st.st_mtime > 0
								  ? st.st_mtime
								  : 1;
				}
				free(p);
			}
			kb_strv_free(files);
			total += x->bytes;
		}
		if (keys && !keys[0])
			rmdir(sub);
		kb_strv_free(keys);
		free(sub);
	}
	kb_strv_free(top);

	qsort(e, (size_t)n, sizeof(*e), cmp_entry);
	int removed = 0;
	for (int i = 0; i < n; i++) {
		if (e[i].used && total <= cap)
			continue;
		if (kb_rmtree(e[i].dir) == 0) {
			total -= e[i].bytes;
			removed++;
		}
	}
	for (int i = 0; i < n; i++) {
		/* The two-character shard goes with its last entry; rmdir
		 * refuses one that still holds any. */
		char *sl = strrchr(e[i].dir, '/');
		if (sl) {
			*sl = '\0';
			rmdir(e[i].dir);
		}
		free(e[i].dir);
	}
	free(e);
	return removed;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Every name a script passes to one of the three port helpers. */
static void scan_ports(const char *text, char ***v, int *n, int *cap)
{
	static const char *const verbs[] = { "extract_port_source",
					     "get_port_version", "port_dir",
					     NULL };
	for (int i = 0; verbs[i]; i++) {
		size_t vl = strlen(verbs[i]);
		for (const char *p = strstr(text, verbs[i]); p;
		     p = strstr(p + vl, verbs[i])) {
			const char *q = p + vl;
			if (*q != ' ' && *q != '\t')
				continue;
			while (*q == ' ' || *q == '\t')
				q++;
			char word[128];
			size_t k = 0;
			while ((q[k] >= 'a' && q[k] <= 'z') ||
			       (q[k] >= 'A' && q[k] <= 'Z') ||
			       (q[k] >= '0' && q[k] <= '9') || q[k] == '-' ||
			       q[k] == '_' || q[k] == '.' || q[k] == '+') {
				if (k + 1 >= sizeof(word))
					break;
				word[k] = q[k];
				k++;
			}
			word[k] = 0;
			if (!k || strv_has(*v, *n, word))
				continue;
			if (*n + 1 >= *cap) {
				*cap *= 2;
				char **nv = kb_calloc((size_t)*cap, sizeof(*nv));
				memcpy(nv, *v, (size_t)*n * sizeof(*nv));
				free(*v);
				*v = nv;
			}
			(*v)[(*n)++] = kb_strdup(word);
		}
	}
}

static int salt_walk(const char *root, const char *rel, KbSha256 *s,
		     char ***ports, int *np, int *pcap)
{
	char *path = kb_path_join(root, rel);
	struct stat st;
	int got = 0;
	if (lstat(path, &st) != 0) {
		free(path);
		return 0;
	}
	if (S_ISDIR(st.st_mode)) {
		char **names = kb_listdir(path, NULL);
		for (char **nm = names; nm && *nm; nm++) {
			char *sub = kb_path_join(rel, *nm);
			got += salt_walk(root, sub, s, ports, np, pcap);
			free(sub);
		}
		kb_strv_free(names);
	} else if (S_ISREG(st.st_mode)) {
		size_t len = 0;
		char *data = kb_read_all(path, &len);
		if (data) {
			char head[600];
			snprintf(head, sizeof(head), "%s\n%zu\n", rel, len);
			kb_sha256_update(s, head, strlen(head));
			kb_sha256_update(s, data, len);
			scan_ports(data, ports, np, pcap);
			free(data);
			got = 1;
		}
	}
	free(path);
	return got;
}

int kp_store_salt(const char *repo, char out[65])
{
	static const char *const parts[] = {
		"script/phases/00_cross", "script/phases/10_bootstrap",
		"script/lib", "script/env", "fs/etc/passwd", "fs/etc/group",
		"fs/etc/ld-musl-x86_64.path", NULL
	};
	int cap = 64, np = 0, got = 0;
	char **ports = kb_calloc((size_t)cap, sizeof(*ports));
	KbSha256 s;
	kb_sha256_init(&s);
	for (int i = 0; parts[i]; i++)
		got += salt_walk(repo, parts[i], &s, &ports, &np, &cap);

	qsort(ports, (size_t)np, sizeof(*ports), cmp_str);
	KpConf *c = kb_calloc(1, sizeof(*c));
	char *core = kb_path_join(repo, "ports/core");
	kp_conf_set_repos(c, core);
	free(core);
	for (int i = 0; i < np; i++) {
		char *dir = NULL, err[256], h[65];
		if (kp_port_find(c, ports[i], &dir, err, sizeof(err)) == 1 &&
		    kp_recipe_hash(dir, h) == 0) {
			char line[256];
			snprintf(line, sizeof(line), "port %s %s\n", ports[i],
				 h);
			kb_sha256_update(&s, line, strlen(line));
		}
		free(dir);
		free(ports[i]);
	}
	free(ports);
	free(c);
	kb_sha256_final(&s, out);
	return got ? 0 : -1;
}
