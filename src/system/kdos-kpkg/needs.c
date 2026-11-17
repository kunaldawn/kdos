/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkg needs — what an installed package needs at run time, measured
 *
 *   kpkg needs [--root <path>] <package>
 *   kpkg needs --resolve [--root <path>] <package>
 *   kpkg needs --scan [--root <path>] <package>
 *
 * The record (`<db>/.needs/<name>`) and its edges are libkpkg's (kpkg.h,
 * kp_needs.c); kpkgadd writes it once a package's files are in place
 * (kp_needs_note). The bare command prints it. --resolve prints each edge
 * with the installed package that provides it, `(unowned)` when a file no
 * package claims does, `(nothing)` when nothing does, and exits 1 when an
 * edge is provided by nothing. --scan reads the package's installed files
 * the way kpkgadd does and prints what the record would hold, writing
 * nothing: a package installed before needs were recorded has no record,
 * and that is how one is measured.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-kpkg.h"

char *kp_needs_measure(const KpConf *c, const char *name)
{
	char *db = kp_db_dir(c);
	char *entry = kb_path_join(db, name);
	free(db);
	char *man = kb_read_all(entry, NULL);
	free(entry);
	if (!man)
		return NULL;
	char *body = strchr(man, '\n');
	char extra[1024], runtime[1024];
	kp_facets_extra(c, name, extra, sizeof(extra));
	kp_needs_runtime(c, name, runtime, sizeof(runtime));
	char *rec = kp_needs_scan(c, name, body ? body + 1 : "", extra,
				  runtime);
	free(man);
	return rec;
}

void kp_needs_runtime(const KpConf *c, const char *name, char *out,
		      size_t cap)
{
	char *dir = NULL;
	char err[1024];
	out[0] = 0;
	if (kp_port_find(c, name, &dir, err, sizeof(err)) == 1)
		kp_recipe_key(dir, "runtime", out, cap);
	free(dir);
}

void kp_needs_note(const KpConf *c, const char *name)
{
	char *rec = kp_needs_measure(c, name);
	if (!rec || kp_record_needs(c, name, rec, strlen(rec)) != 0)
		kp_msg("Warning: no run-time needs recorded for %s; the image "
		       "gate reports it unmeasured", name);
	free(rec);
}

static int resolve(const KpConf *c, const char *rec)
{
	char *copy = kb_strdup(rec);
	int n = 0;
	for (const char *p = copy; *p; p++)
		n += *p == '\n';
	char **edge = kb_calloc((size_t)n + 1, sizeof(*edge));
	char ***where = kb_calloc((size_t)n + 1, sizeof(*where));
	const char **byname = kb_calloc((size_t)n + 1, sizeof(*byname));
	int ne = 0, nask = 0;
	for (char *l = copy, *nl; l && *l && ne < n; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		if (!*l)
			continue;
		edge[ne] = l;
		where[ne] = kp_needs_where(c, l, &byname[ne]);
		for (char **w = where[ne]; *w; w++)
			nask++;
		ne++;
	}
	char **ask = kb_calloc((size_t)nask + 1, sizeof(*ask));
	int k = 0;
	for (int i = 0; i < ne; i++)
		for (char **w = where[i]; *w; w++)
			ask[k++] = *w;
	KpOwned *owned = kp_owned_load_some(c, ask, k);

	int missing = 0;
	for (int i = 0; i < ne; i++) {
		const char *who = NULL;
		for (char **w = where[i]; *w && !who; w++)
			who = kp_owned_owner(owned, *w);
		if (!who && byname[i] && kp_installed(c, byname[i]))
			who = byname[i];
		if (!who && where[i][0] && !byname[i])
			who = "(unowned)";
		if (!who) {
			who = "(nothing)";
			missing++;
		}
		printf("%s %s\n", edge[i], who);
	}
	kp_owned_free(owned);
	free(ask);
	for (int i = 0; i < ne; i++)
		kb_strv_free(where[i]);
	free(where);
	free(byname);
	free(edge);
	free(copy);
	if (missing)
		fprintf(stderr, "%d edge%s nothing installed provides\n",
			missing, missing == 1 ? "" : "s");
	return missing ? 1 : 0;
}

int kp_cmd_needs(KpConf *c, int argc, char **argv)
{
	int res = 0, scan = 0;
	const char *who = NULL;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--resolve"))
			res = 1;
		else if (!strcmp(argv[i], "--scan"))
			scan = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c->root, argv[++i], sizeof(c->root));
		else if (argv[i][0] == '-' || who) {
			who = NULL;
			res = scan = -1;
			break;
		} else
			who = argv[i];
	}
	if (!who || res < 0 || (res && scan)) {
		printf("Usage: kpkg needs [--root <path>] <package>\n"
		       "       kpkg needs --resolve [--root <path>] <package>\n"
		       "       kpkg needs --scan [--root <path>] <package>\n");
		return 2;
	}
	if (!kp_installed(c, who)) {
		kp_err("Package '%s' not installed", who);
		return 1;
	}
	char *rec = scan ? kp_needs_measure(c, who)
			 : kp_installed_needs(c, who, NULL);
	if (!rec) {
		printf("%s: no run-time needs recorded; `kpkg needs --scan %s` "
		       "measures them\n", who, who);
		return 0;
	}
	int rc = 0;
	if (res)
		rc = resolve(c, rec);
	else
		fputs(rec, stdout);
	free(rec);
	return rc;
}
