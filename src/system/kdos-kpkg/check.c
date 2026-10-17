/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkg check — does the root still hold what was installed
 *
 *   kpkg check [--root <path>] <package>...
 *   kpkg check [--root <path>] --all
 *
 * Each package's manifest is walked against the root and its file record
 * (`<db>/.files/<name>`, kpkg.h):
 *
 *   missing  a path in the manifest with nothing at it, or a directory
 *            entry that is not a directory
 *   changed  a recorded regular file whose bytes do not hash to the
 *            record, or that is not a regular file; an index kpkg's
 *            triggers rewrite (texlive's ls-R) is checked for presence
 *   mode     a recorded regular file whose permission bits differ
 *   config   a recorded file under etc/ that is changed or whose mode
 *            differs: configuration a person edited, reported and never a
 *            failure. A missing file under etc/ is still `missing`.
 *
 * `--all` checks every installed package and also lists, as `unowned`, the
 * files and links under usr/ and opt/ that no package claims, less the
 * indexes kpkg's own triggers write and Python's `__pycache__` bytecode.
 *
 * Exit 0 when nothing is missing or changed, 1 when something is, 2 for a
 * usage error or a package that is not installed. A `config` file and an
 * unowned file are reported and do not change the status: an edited
 * /etc/fstab is the machine's configuration, not damage, and the fs/
 * skeleton and every file a person adds under /usr/local are unowned by
 * design. `kdos update apply` gates a slot on this status, so a file counted
 * as `config` there is one an update keeps without question.
 *
 * Paths are joined to the root as the manifest spells them, so `./bin/x` on a
 * merged-/usr root is checked through the root's own `bin` link. Ownership
 * for the unowned walk is the canonical table (kp_owned_load), so a file
 * claimed under either spelling is owned.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

typedef struct {
	char *path;	/* `./usr/bin/tar`, borrowed from the record blob */
	char *sha;	/* 64 hex, borrowed                               */
	unsigned mode;
} Rec;

typedef struct {
	int missing, changed, mode, config, unowned, unrecorded;
} Tally;

static int cmp_rec(const void *a, const void *b)
{
	return strcmp(((const Rec *)a)->path, ((const Rec *)b)->path);
}

/* The record blob parsed in place: each line split into its three fields. A
 * line that is not `<64 hex> <octal> <path>` is skipped. */
static Rec *parse_record(char *blob, int *n)
{
	int cap = 256;
	Rec *v = kb_calloc((size_t)cap, sizeof(*v));
	*n = 0;
	for (char *l = blob, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		char *sp1 = strchr(l, ' ');
		if (!sp1 || sp1 - l != 64)
			continue;
		char *sp2 = strchr(sp1 + 1, ' ');
		if (!sp2 || !sp2[1])
			continue;
		*sp1 = *sp2 = 0;
		if (*n == cap) {
			cap *= 2;
			Rec *nv = kb_calloc((size_t)cap, sizeof(*nv));
			memcpy(nv, v, (size_t)*n * sizeof(*nv));
			free(v);
			v = nv;
		}
		v[*n].sha = l;
		v[*n].mode = (unsigned)strtoul(sp1 + 1, NULL, 8);
		v[*n].path = sp2 + 1;
		(*n)++;
	}
	qsort(v, (size_t)*n, sizeof(*v), cmp_rec);
	return v;
}

static const Rec *find_rec(const Rec *v, int n, const char *path)
{
	Rec key = { (char *)path, NULL, 0 };
	return bsearch(&key, v, (size_t)n, sizeof(*v), cmp_rec);
}

static int generated(const char *rel);

static int check_one(const KpConf *c, const char *root, const char *name,
		     Tally *t)
{
	char *db = kp_db_dir(c);
	char *entry = kb_path_join(db, name);
	free(db);
	size_t len = 0;
	char *man = kb_read_all(entry, &len);
	free(entry);
	if (!man) {
		kp_err("%s is not installed", name);
		return -1;
	}

	size_t rlen = 0;
	char *blob = kp_installed_files(c, name, &rlen);
	int nrec = 0;
	Rec *rec = blob ? parse_record(blob, &nrec) : NULL;
	if (!blob)
		t->unrecorded++;

	char *body = strchr(man, '\n');
	for (char *l = body ? body + 1 : NULL, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		size_t ll = strlen(l);
		if (!ll)
			continue;
		char *full = kb_path_join(root, l);
		struct stat st;
		if (l[ll - 1] == '/') {
			if (!kb_is_dir(full)) {
				printf("%s: missing %s\n", name, l);
				t->missing++;
			}
		} else if (lstat(full, &st) != 0) {
			printf("%s: missing %s\n", name, l);
			t->missing++;
		} else {
			const Rec *r = rec ? find_rec(rec, nrec, l) : NULL;
			const char *rel = strncmp(l, "./", 2) ? l : l + 2;
			int conf = !strncmp(rel, "etc/", 4);
			char hex[65];
			if (!r || generated(rel)) {
				/* No record: a link, a device node, or an
				 * install that kept none. Present is all
				 * there is to know. An index a trigger
				 * rewrites (texlive's ls-R) differs from
				 * the package's copy by design. */
			} else if (!S_ISREG(st.st_mode) ||
				   kb_sha256_file(full, hex) != 0 ||
				   strcmp(hex, r->sha)) {
				printf("%s: %s %s\n", name,
				       conf ? "config" : "changed", l);
				if (conf)
					t->config++;
				else
					t->changed++;
			} else if ((st.st_mode & 07777) != r->mode) {
				printf("%s: %s %s %04o, installed %04o\n",
				       name, conf ? "config mode" : "mode", l,
				       (unsigned)(st.st_mode & 07777), r->mode);
				if (conf)
					t->config++;
				else
					t->mode++;
			}
		}
		free(full);
	}
	free(rec);
	free(blob);
	free(man);
	return 0;
}

/* ── unowned ────────────────────────────────────────────────────────────── */

/* What kpkg's triggers (triggers.c) write, which no package owns or which
 * the trigger rewrites over the package's copy, and the bytecode Python
 * writes beside a module. `rel` has no `./`. */
static int generated(const char *rel)
{
	static const char *const exact[] = {
		"usr/share/glib-2.0/schemas/gschemas.compiled",
		"usr/lib/gio/modules/giomodule.cache",
		"usr/share/info/dir",
		"usr/share/applications/mimeinfo.cache",
	};
	for (size_t i = 0; i < sizeof(exact) / sizeof(*exact); i++)
		if (!strcmp(rel, exact[i]))
			return 1;

	static const char *const under[] = {
		"usr/lib/fontconfig/cache/",
	};
	for (size_t i = 0; i < sizeof(under) / sizeof(*under); i++)
		if (!strncmp(rel, under[i], strlen(under[i])))
			return 1;

	/* update-mime-database writes everything under mime/ but the XML in
	 * packages/, which is what packages ship. */
	static const char mime[] = "usr/share/mime/";
	if (!strncmp(rel, mime, sizeof(mime) - 1) &&
	    strncmp(rel + sizeof(mime) - 1, "packages/", 9))
		return 1;

	const char *base = strrchr(rel, '/');
	base = base ? base + 1 : rel;
	struct {
		const char *prefix;
		const char *base;
	} named[] = {
		{ "usr/lib/gdk-pixbuf-2.0/", "loaders.cache" },
		{ "usr/share/man/", "mandoc.db" },
		{ "usr/share/fonts/", "fonts.dir" },
		{ "usr/share/texmf-", "ls-R" },
		{ "usr/share/icons/", "icon-theme.cache" },
	};
	for (size_t i = 0; i < sizeof(named) / sizeof(*named); i++)
		if (!strncmp(rel, named[i].prefix, strlen(named[i].prefix)) &&
		    !strcmp(base, named[i].base))
			return 1;

	return strstr(rel, "/__pycache__/") != NULL;
}

static void walk_unowned(const char *root, const char *rel,
			 const KpOwned *owned, Tally *t)
{
	char *dir = kb_path_join(root, rel);
	char **names = kb_listdir(dir, NULL);
	for (char **p = names; p && *p; p++) {
		char *child = kb_path_join(rel, *p);
		char *full = kb_path_join(dir, *p);
		struct stat st;
		if (lstat(full, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				walk_unowned(root, child, owned, t);
			} else if (!kp_owned_owner(owned, child) &&
				   !generated(child)) {
				printf("unowned ./%s\n", child);
				t->unowned++;
			}
		}
		free(child);
		free(full);
	}
	kb_strv_free(names);
	free(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

int kp_cmd_check(KpConf *c, int argc, char **argv)
{
	int all = 0;
	char **want = kb_calloc((size_t)argc + 1, sizeof(*want));
	int nwant = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--all"))
			all = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c->root, argv[++i], sizeof(c->root));
		else if (argv[i][0] == '-') {
			kp_err("unknown option: %s", argv[i]);
			free(want);
			return 2;
		} else
			want[nwant++] = argv[i];
	}
	if (all == (nwant > 0)) {
		printf("Usage: kpkg check [--root <path>] <package>...\n"
		       "       kpkg check [--root <path>] --all\n");
		free(want);
		return 2;
	}

	const char *root = c->root[0] ? c->root : "/";
	char **names = NULL;
	if (all) {
		char *db = kp_db_dir(c);
		names = kb_listdir(db, NULL);
		free(db);
	}

	Tally t = {0};
	int npkg = 0, rc = 0;
	if (all) {
		for (char **p = names; p && *p; p++) {
			if ((*p)[0] == '.')
				continue;
			if (check_one(c, root, *p, &t) != 0)
				rc = 2;
			npkg++;
		}
	} else {
		for (int i = 0; i < nwant; i++) {
			if (check_one(c, root, want[i], &t) != 0)
				rc = 2;
			npkg++;
		}
	}
	kb_strv_free(names);
	free(want);

	if (all) {
		KpOwned *owned = kp_owned_load(c);
		static const char *const tops[] = { "usr", "opt" };
		for (size_t i = 0; i < sizeof(tops) / sizeof(*tops); i++) {
			char *top = kb_path_join(root, tops[i]);
			struct stat st;
			if (lstat(top, &st) == 0 && S_ISDIR(st.st_mode))
				walk_unowned(root, tops[i], owned, &t);
			free(top);
		}
		kp_owned_free(owned);
	}

	printf("%d package%s checked: %d missing, %d changed, %d mode, "
	       "%d config", npkg, npkg == 1 ? "" : "s", t.missing, t.changed,
	       t.mode, t.config);
	if (all)
		printf(", %d unowned", t.unowned);
	putchar('\n');
	if (t.unrecorded)
		printf("%d package%s installed with no file record: checked "
		       "for presence only\n", t.unrecorded,
		       t.unrecorded == 1 ? "" : "s");
	if (rc)
		return rc;
	return t.missing || t.changed || t.mode ? 1 : 0;
}
