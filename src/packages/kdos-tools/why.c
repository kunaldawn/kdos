/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos why — path -> package -> recipe -> the REASON
 *   kdos explain — the same corpus, browsed by topic
 *
 * Provenance is not the novel part and this does not pretend otherwise. Guix
 * records its own provenance and can rebuild the exact system from it;
 * Gentoo's /var/db/pkg/<pkg>/environment.bz2 holds the whole build
 * environment including CFLAGS and SRC_URI. Both answer "where did this file
 * come from" better than a first attempt will.
 *
 * What none of them answers is WHY. NixOS can tell you a value came from
 * configuration.nix:47; it cannot tell you the maintainer set it because the
 * alternative deadlocks on musl. That reason lives in a commit message, an
 * issue thread, or nowhere.
 *
 * KDOS already writes that corpus — CLAUDE.md, where nearly every rule is a
 * recorded debug cycle. This makes it queryable from the machine it is about,
 * offline, with no network and no wiki.
 *
 * ── The failure mode this must not have ────────────────────────────────
 *
 * A hand-maintained reason store diverges from the tree within a month and
 * then actively LIES, which is worse than not existing: a confident wrong
 * answer costs more than no answer. So every `path:` and `port:` key is
 * validated by testing/preflight.sh, and a reason naming something that no
 * longer exists fails the build exactly like an unresolvable `# depends`.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kpkg.h"

#define REASON_DIR "/usr/share/kdos/reasons"

static const char *C_ACC = "", *C_DIM = "", *C_OFF = "";

static void why_colours(void)
{
	if (isatty(STDOUT_FILENO)) {
		C_ACC = "\033[1;32m";
		C_DIM = "\033[2;32m";
		C_OFF = "\033[0m";
	}
}

/* $KDOS_REASONS beats the installed copy, so the tree can be queried from a
 * checkout before anything is installed — which is also how preflight reads
 * the corpus. */
const char *kdt_reason_dir(void)
{
	const char *e = getenv("KDOS_REASONS");
	return (e && *e) ? e : REASON_DIR;
}

/* One `key: value` from a reason's header, or NULL. Occurrence `nth`, so
 * repeated `path:` lines are all reachable. */
char *kdt_reason_header(const char *text, const char *key, int nth)
{
	size_t klen = strlen(key);
	int seen = 0;
	for (const char *l = text; l && *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		if (!len)
			break;			/* blank line ends the header */
		if (!strncmp(l, key, klen) && l[klen] == ':') {
			if (seen++ == nth) {
				const char *v = l + klen + 1;
				while (*v == ' ' || *v == '\t')
					v++;
				size_t n = len - (size_t)(v - l);
				char *out = kb_calloc(n + 1, 1);
				memcpy(out, v, n);
				out[n] = 0;
				return out;
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	return NULL;
}

static const char *body(const char *text)
{
	const char *b = strstr(text, "\n\n");
	return b ? b + 2 : text;
}

/* Case-insensitive substring, so `kdos explain MUSL` works. No libkbase
 * equivalent, and one caller does not justify widening that API. */
static int contains_ci(const char *hay, const char *needle)
{
	size_t n = strlen(needle);
	if (!n)
		return 1;
	for (const char *p = hay; *p; p++) {
		size_t i = 0;
		while (i < n && p[i] &&
		       tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
			i++;
		if (i == n)
			return 1;
	}
	return 0;
}

static char **reason_files(int *count)
{
	*count = 0;
	DIR *d = opendir(kdt_reason_dir());
	if (!d)
		return NULL;
	char **v = NULL;
	int n = 0, cap = 0;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		if (n == cap) {
			int ncap = cap ? cap * 2 : 16;
			char **nv = kb_calloc((size_t)ncap + 1, sizeof(*nv));
			for (int k = 0; k < n; k++)
				nv[k] = v[k];
			free(v);
			v = nv;
			cap = ncap;
		}
		v[n++] = kb_path_join(kdt_reason_dir(), e->d_name);
	}
	closedir(d);
	*count = n;
	return v;
}

/* Does this reason claim `subject`, either as a path or as a port? A path key
 * matches the subject itself or any file beneath it, so a reason filed
 * against /etc/fstab answers for /etc/fstab and a reason filed against /tmp
 * answers for anything in it. */
static int claims(const char *text, const char *subject)
{
	for (int i = 0;; i++) {
		char *p = kdt_reason_header(text, "path", i);
		if (!p)
			break;
		size_t n = strlen(p);
		int hit = !strcmp(p, subject) ||
			  (!strncmp(p, subject, n) &&
			   (subject[n] == '/' || subject[n] == 0));
		free(p);
		if (hit)
			return 1;
	}
	for (int i = 0;; i++) {
		char *p = kdt_reason_header(text, "port", i);
		if (!p)
			break;
		int hit = !strcmp(p, subject);
		free(p);
		if (hit)
			return 1;
	}
	return 0;
}

static void print_reason(const char *path, const char *text, int full)
{
	char *t = kdt_reason_header(text, "title", 0);
	char *stem = kb_strdup(kb_basename(path));
	char *dot = strrchr(stem, '.');
	if (dot)
		*dot = 0;

	printf("\n  %s%s%s\n", C_ACC, t ? t : stem, C_OFF);
	printf("  %s(%s)%s\n", C_DIM, stem, C_OFF);
	if (full) {
		printf("\n");
		for (const char *l = body(text); l && *l;) {
			const char *nl = strchr(l, '\n');
			int len = nl ? (int)(nl - l) : (int)strlen(l);
			printf("  %.*s\n", len, l);
			l = nl ? nl + 1 : NULL;
		}
	}
	free(t);
	free(stem);
}

int why_main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: kdos why <path|port>\n");
		return 2;
	}
	why_colours();
	const char *subject = argv[1];

	/* An absolute path gets the provenance half first. */
	if (subject[0] == '/') {
		KpConf c;
		kp_conf_load(&c);
		KpOwned *o = kp_owned_load(&c);
		const char *owner = o ? kp_owned_owner(o, subject + 1) : NULL;

		printf("%s%s%s\n", C_ACC, subject, C_OFF);
		if (owner) {
			char ver[64] = "", rel[32] = "";
			kp_installed_version(&c, owner, ver, sizeof(ver), rel,
					     sizeof(rel));
			printf("  package  %s %s-%s\n", owner, ver, rel);

			char *pd = kp_port_dir(&c, owner);
			if (pd) {
				printf("  recipe   %s/kpkgbuild\n", pd);
				free(pd);
			}
		} else {
			/* Everything under fs/ is copied in wholesale rather
			 * than installed by a package, so "no owner" is a real
			 * and common answer, not a failure. */
			printf("  package  %s(none — provided by fs/, or not "
			       "installed)%s\n", C_DIM, C_OFF);
		}
		if (o)
			kp_owned_free(o);
	} else {
		printf("%s%s%s\n", C_ACC, subject, C_OFF);
	}

	int n = 0;
	char **files = reason_files(&n);
	int hits = 0;
	for (int i = 0; i < n; i++) {
		size_t len = 0;
		char *text = kb_read_all(files[i], &len);
		if (!text)
			continue;
		if (claims(text, subject)) {
			print_reason(files[i], text, 1);
			hits++;
		}
		free(text);
	}
	kb_strv_free(files);

	if (!hits)
		printf("\n  %sno recorded reason — if you just spent a debug "
		       "cycle on this,\n  that is exactly what reasons/ is "
		       "for.%s\n", C_DIM, C_OFF);
	return 0;
}

int explain_main(int argc, char **argv)
{
	why_colours();
	const char *want = argc > 1 ? argv[1] : NULL;

	int n = 0;
	char **files = reason_files(&n);
	if (!n) {
		fprintf(stderr, "kdos explain: no reasons installed (%s)\n",
			kdt_reason_dir());
		return 1;
	}

	int hits = 0;
	for (int i = 0; i < n; i++) {
		size_t len = 0;
		char *text = kb_read_all(files[i], &len);
		if (!text)
			continue;
		const char *stem = kb_basename(files[i]);
		/* Substring match over the whole document, so `kdos explain
		 * musl` finds the reasons that only mention it in passing. */
		int hit = !want || strstr(stem, want) || contains_ci(text, want);
		if (hit) {
			print_reason(files[i], text, want != NULL);
			hits++;
		}
		free(text);
	}
	kb_strv_free(files);

	if (!hits)
		printf("  nothing recorded about '%s'\n", want);
	else if (!want)
		printf("\n  %skdos explain <topic>  ·  kdos why <path>%s\n",
		       C_DIM, C_OFF);
	return 0;
}
