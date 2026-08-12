/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos cve — which pinned versions have known holes, offline
 *
 * The data is a vendored, pruned copy of ALPINE's security database
 * (src/packages/kdos-tools/secdb, installed to /usr/share/kdos/secdb.txt).
 * Alpine is the right proxy for KDOS — musl, the same upstream tarballs,
 * comparable pins — and it is the only distro publishing this in a form small
 * enough to carry: 270 KB for 798 packages and 4 099 fix records.
 *
 * The question asked is exactly the one the data can answer: **is the version
 * we pin older than the version Alpine says fixed this CVE?** That is a
 * comparison, not a scan; nothing here reads a binary, and the comparator is
 * libkpkg's `kp_vercmp` — the same one `kdos-portup` uses to decide whether
 * upstream is newer, because they are the same question.
 *
 * What it will NOT do:
 *
 *   - claim completeness. secdb is keyed to ALPINE package names and Alpine
 *     versions. A port whose name differs needs `secdb = <alpine-name>` in its
 *     recipe; a CVE fixed in a version Alpine never shipped is invisible. There
 *     is no reliable mapping from a source tarball to CVEs — cve-bin-tool's CPE
 *     lookups need a human to assert `zlib -> cpe:2.3:a:zlib:zlib` — so this
 *     reports what Alpine recorded and says how old the record is.
 *   - go online. The whole point is an answer on a machine with no network, and
 *     `ports/update --cve` is the separate online cross-check.
 *   - guess when it cannot tell. A package with no secdb entry is reported as
 *     UNKNOWN in the summary rather than counted as clean.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kdos-tools.h"
#include "kpkg.h"

#define CVE_DEFAULT_DB "/usr/share/kdos/secdb.txt"
/* Vendored data ages, and an answer from a stale database is worth less than
 * the same answer from a fresh one. Six months is roughly two Alpine releases. */
#define CVE_STALE_DAYS 180

/* libkbase has kb_strlcpy and no kb_strlcat; this is the one place that wants
 * one, so it stays local rather than growing the library for a single caller. */
static void append(char *dst, size_t cap, const char *src)
{
	size_t n = strlen(dst);
	if (n + 1 < cap)
		snprintf(dst + n, cap - n, "%s", src);
}

struct cve_hit {
	char port[128];
	char have[64];
	char fixed[64];
	char cves[512];
};

/* ── the table ─────────────────────────────────────────────────────────── */

static char *load_db(const char *path, char *stamp, size_t stamp_cap)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);

	stamp[0] = 0;
	if (!data)
		return NULL;
	/* The generation date is a comment in the file rather than the file's
	 * mtime: an mtime survives neither `git clone` nor a package install,
	 * and this number is quoted to the user. */
	const char *g = strstr(data, "# generated ");
	if (g) {
		g += 12;
		size_t n = 0;
		while (g[n] && g[n] != ' ' && g[n] != '\n' && n + 1 < stamp_cap)
			n++;
		memcpy(stamp, g, n);
		stamp[n] = 0;
	}
	return data;
}

static int days_since(const char *ymd)
{
	struct tm tm = {0};
	if (!ymd || !*ymd || sscanf(ymd, "%d-%d-%d", &tm.tm_year, &tm.tm_mon,
				    &tm.tm_mday) != 3)
		return -1;
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	time_t then = timegm(&tm);
	if (then <= 0)
		return -1;
	double d = difftime(time(NULL), then) / 86400.0;
	return d < 0 ? 0 : (int)d;
}

/*
 * Alpine versions carry a package revision — `1.2.12-r2` — and KDOS pins carry
 * upstream's version alone. The revision is Alpine's own packaging counter and
 * says nothing about upstream's code, so it is cut off before the comparison:
 * comparing `1.2.12` against `1.2.12-r2` with it left on makes every pin look
 * older than the fix.
 */
static void strip_rev(const char *v, char *out, size_t cap)
{
	kb_strlcpy(out, v, cap);
	char *r = strstr(out, "-r");
	if (r && r[2] >= '0' && r[2] <= '9')
		*r = 0;
}

/* Is this CVE already in the accumulated list? Compared whole, so CVE-2026-1
 * does not match CVE-2026-12. */
static int cve_seen(const char *list, const char *want)
{
	size_t wlen = strlen(want);
	for (const char *p = list; *p;) {
		if (!strncmp(p, want, wlen) && (p[wlen] == 0 || p[wlen] == ','))
			return 1;
		const char *c = strchr(p, ',');
		if (!c)
			break;
		p = c + 1;
	}
	return 0;
}

/*
 * Walk the table for one package. `have` is what we pin; every row whose fixed
 * version is NEWER is a hole we are still in.
 *
 * Alpine writes `0` for "this branch was never affected", and that falls out of
 * the comparison for free: nothing is older than 0.
 */
static int check_one(const char *db, const char *alpine, const char *have,
		     struct cve_hit *hit)
{
	size_t alen = strlen(alpine);
	int found = 0;

	hit->cves[0] = 0;
	hit->fixed[0] = 0;
	for (const char *line = db; line && *line;) {
		const char *nl = strchr(line, '\n');
		size_t len = nl ? (size_t)(nl - line) : strlen(line);

		if (*line != '#' && !strncmp(line, alpine, alen) &&
		    line[alen] == ' ') {
			char row[1024];
			if (len >= sizeof(row))
				len = sizeof(row) - 1;
			memcpy(row, line, len);
			row[len] = 0;

			char *ver = row + alen + 1;
			char *sp = strchr(ver, ' ');
			if (sp) {
				*sp = 0;
				char bare[64];
				strip_rev(ver, bare, sizeof(bare));
				if (kp_vercmp(have, bare) < 0) {
					found = 1;
					/* The NEWEST fix we are behind is the
					 * one to report as "fixed in": it is
					 * the version that closes every hole
					 * below it too. */
					if (!hit->fixed[0] ||
					    kp_vercmp(bare, hit->fixed) > 0)
						kb_strlcpy(hit->fixed, bare,
							   sizeof(hit->fixed));
					/* The same CVE is recorded by every
					 * branch that shipped the fix, at that
					 * branch's version — so a merged table
					 * names it several times and the report
					 * would too. */
					char one[64];
					for (char *cv = sp + 1; *cv;) {
						char *comma = strchr(cv, ',');
						size_t n = comma ? (size_t)(comma - cv)
								 : strlen(cv);
						if (n >= sizeof(one))
							n = sizeof(one) - 1;
						memcpy(one, cv, n);
						one[n] = 0;
						if (!cve_seen(hit->cves, one)) {
							if (hit->cves[0])
								append(hit->cves,
								       sizeof(hit->cves),
								       ",");
							append(hit->cves,
							       sizeof(hit->cves),
							       one);
						}
						cv = comma ? comma + 1 : cv + n;
					}
				}
			}
		}
		line = nl ? nl + 1 : NULL;
	}
	return found;
}

/* ── what to check ─────────────────────────────────────────────────────── */

/*
 * The installed database when there is one, the ports tree otherwise.
 *
 * On a running KDOS the question is "what is on this machine"; in the repo it
 * is "what would we ship". Both are one name and one version per entry, so the
 * checker does not care which it got — but it says which, because the answers
 * differ and a reader has to know which one they are looking at.
 */
static int collect(const KpConf *c, char names[][128], char vers[][64],
		   char alpine[][128], int max, int *from_db)
{
	int n = 0;

	char *db = kp_db_dir(c);
	char **installed = kb_listdir(db, NULL);
	free(db);
	if (installed && installed[0]) {
		*from_db = 1;
		for (char **p = installed; *p && n < max; p++) {
			char ver[128] = "", rel[32] = "";
			if (kp_installed_version(c, *p, ver, sizeof(ver), rel,
						 sizeof(rel)) != 0)
				continue;
			kb_strlcpy(names[n], *p, 128);
			kb_strlcpy(vers[n], ver, 64);
			alpine[n][0] = 0;
			char *dir = kp_port_dir(c, *p);
			if (dir) {
				kp_recipe_key(dir, "secdb", alpine[n], 128);
				free(dir);
			}
			if (!alpine[n][0])
				kb_strlcpy(alpine[n], *p, 128);
			n++;
		}
		kb_strv_free(installed);
		return n;
	}
	kb_strv_free(installed);

	*from_db = 0;
	int count = 0;
	char **ports = kp_all_ports(c, &count);
	for (char **p = ports; p && *p && n < max; p++) {
		char *dir = kp_port_dir(c, *p);
		if (!dir)
			continue;
		char ver[64] = "";
		kp_recipe_key(dir, "version", ver, sizeof(ver));
		if (ver[0]) {
			kb_strlcpy(names[n], *p, 128);
			kb_strlcpy(vers[n], ver, 64);
			kp_recipe_key(dir, "secdb", alpine[n], 128);
			if (!alpine[n][0])
				kb_strlcpy(alpine[n], *p, 128);
			n++;
		}
		free(dir);
	}
	kb_strv_free(ports);
	return n;
}

/* ── the command ───────────────────────────────────────────────────────── */

#define CVE_MAX 1024

int kdt_cve(int argc, char **argv, const char *tty_accent,
	    const char *tty_warn, const char *tty_reset)
{
	int json = 0;
	const char *only = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			json = 1;
		else if (argv[i][0] != '-')
			only = argv[i];
	}

	const char *path = getenv("KDOS_SECDB");
	if (!path || !*path)
		path = CVE_DEFAULT_DB;

	char stamp[32];
	char *db = load_db(path, stamp, sizeof(stamp));
	if (!db) {
		fprintf(stderr,
			"kdos: no security database at %s — this check is "
			"offline and needs the vendored table\n", path);
		return 2;
	}
	int age = days_since(stamp);

	KpConf c;
	kp_conf_load(&c);

	static char names[CVE_MAX][128], vers[CVE_MAX][64], alpine[CVE_MAX][128];
	int from_db = 0;
	int n = collect(&c, names, vers, alpine, CVE_MAX, &from_db);

	static struct cve_hit hits[CVE_MAX];
	int nhit = 0, checked = 0, unknown = 0;

	for (int i = 0; i < n; i++) {
		if (only && strcmp(names[i], only))
			continue;
		checked++;
		/* "Alpine has never heard of this package" and "Alpine has and
		 * we are current" are different answers, and only the second is
		 * good news. */
		char probe[256];
		snprintf(probe, sizeof(probe), "\n%.120s ", alpine[i]);
		if (!strstr(db, probe) && strncmp(db, alpine[i], strlen(alpine[i]))) {
			unknown++;
			continue;
		}
		struct cve_hit h = {0};
		if (check_one(db, alpine[i], vers[i], &h)) {
			kb_strlcpy(h.port, names[i], sizeof(h.port));
			kb_strlcpy(h.have, vers[i], sizeof(h.have));
			if (nhit < CVE_MAX)
				hits[nhit++] = h;
		}
	}

	if (json) {
		printf("{\n  \"source\": \"alpine-secdb\",\n"
		       "  \"generated\": \"%s\",\n"
		       "  \"age_days\": %d,\n"
		       "  \"scope\": \"%s\",\n"
		       "  \"checked\": %d,\n  \"unknown\": %d,\n"
		       "  \"findings\": [\n",
		       stamp, age, from_db ? "installed" : "ports", checked,
		       unknown);
		for (int i = 0; i < nhit; i++)
			printf("    {\"port\": \"%s\", \"version\": \"%s\", "
			       "\"fixed_in\": \"%s\", \"cves\": \"%s\"}%s\n",
			       hits[i].port, hits[i].have, hits[i].fixed,
			       hits[i].cves, i + 1 < nhit ? "," : "");
		printf("  ]\n}\n");
	} else {
		printf("%sKDOS cve%s  —  Alpine secdb of %s (%d days old), %s\n\n",
		       tty_accent, tty_reset, stamp[0] ? stamp : "unknown", age,
		       from_db ? "installed packages" : "the ports tree");
		for (int i = 0; i < nhit; i++) {
			/* curl is behind 40 CVEs and expat 21; printing them
			 * all turns the answer into a wall nobody reads. The
			 * first few name the problem, the count sizes it, and
			 * `--json` has every one of them. */
			char shown[128] = "";
			int commas = 0, extra = 0;
			for (const char *p = hits[i].cves; *p; p++)
				if (*p == ',')
					commas++;
			for (const char *p = hits[i].cves; *p; p++) {
				if (*p == ',' && ++extra > 3)
					break;
				size_t n = strlen(shown);
				if (n + 2 < sizeof(shown)) {
					shown[n] = *p;
					shown[n + 1] = 0;
				}
			}
			printf("  %s%-22s%s %-12s fixed in %-12s %s%s\n",
			       tty_warn, hits[i].port, tty_reset, hits[i].have,
			       hits[i].fixed, shown,
			       commas > 3 ? " ..." : "");
			if (commas > 3)
				printf("  %-22s %-12s %*s+%d more\n", "", "",
				       22, "", commas - 3);
		}
		if (!nhit)
			printf("  nothing known-vulnerable\n");
		printf("\n  %d checked, %d not in the database, %d behind a "
		       "recorded fix\n", checked, unknown, nhit);
		if (age > CVE_STALE_DAYS)
			printf("  %sthe database is %d days old — re-run "
			       "src/packages/kdos-tools/secdb/vendor.py%s\n",
			       tty_warn, age, tty_reset);
		if (unknown)
			printf("  packages Alpine does not carry under that "
			       "name are UNKNOWN, not clean (`secdb =` in a "
			       "recipe maps them)\n");
	}

	free(db);
	return nhit ? 1 : 0;
}
