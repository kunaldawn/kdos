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
 * (src/system/kdos-tools/secdb, installed to /usr/share/kdos/secdb.txt).
 * Alpine is the right proxy for KDOS — musl, the same upstream tarballs,
 * comparable pins — and it is the only distro publishing this in a form small
 * enough to carry: 270 KB for 798 packages and 4 099 fix records.
 *
 * The question asked is exactly the one the data can answer: **is the version
 * we pin older than the version Alpine says fixed this CVE, on the branch we
 * pin?** (see check_one). That is a comparison, not a scan; nothing here
 * reads a binary, and the comparator is libkpkg's `kp_vercmp` — the same one
 * `kdos-portup` uses to decide whether upstream is newer, because they are
 * the same question.
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
 *
 * A port that compiles its own copy of a library (the Rust ports that embed
 * AWS-LC through aws-lc-sys) carries a version no package database names. The
 * bundled inventory (secdb/bundled.txt, installed to
 * /usr/share/kdos/bundled.txt) lists each such copy as port, crate, component
 * and version; every row whose port is checked is looked up under the
 * component's name and listed in the report, known or not, so the copy is
 * never silent.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kdos-tools.h"
#include "kpkg.h"

#define CVE_DEFAULT_DB "/usr/share/kdos/secdb.txt"
#define CVE_DEFAULT_BUNDLED "/usr/share/kdos/bundled.txt"
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

/* One row of the bundled inventory: <port> <crate> <component> <version>. */
struct cve_bundled {
	char port[128];
	char crate[64];
	char component[64];
	char version[64];
	int known;	/* the component has rows in the database */
	int behind;	/* and the version is older than a recorded fix */
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

/* The branch a version belongs to: its first two dot-separated fields,
 * `3.6` of `3.6.7`. A version with one field is its own branch. */
static void branch_of(const char *v, char *out, size_t cap)
{
	kb_strlcpy(out, v, cap);
	char *dot = strchr(out, '.');
	if (dot && (dot = strchr(dot + 1, '.')))
		*dot = 0;
}

#define CVE_ROWS 256

struct cve_row {
	char ver[64];		/* the fixed version, revision cut off */
	const char *cves;	/* into the caller's copy of the table */
};

/*
 * Every row for one package, with the fixed versions bare. The CVE lists
 * point into `copy`, the package's lines copied out of the table, which the
 * caller frees.
 */
static int rows_for(const char *db, const char *alpine, struct cve_row *rows,
		    int max, char **copy)
{
	size_t alen = strlen(alpine);
	KbBuf b = {0};
	int n = 0;

	for (const char *line = db; line && *line;) {
		const char *nl = strchr(line, '\n');
		size_t len = nl ? (size_t)(nl - line) : strlen(line);
		if (*line != '#' && !strncmp(line, alpine, alen) &&
		    line[alen] == ' ') {
			kb_buf_add(&b, line, len);
			kb_buf_add(&b, "", 1);
		}
		line = nl ? nl + 1 : NULL;
	}
	*copy = b.p;
	for (size_t o = 0; b.p && o < b.n && n < max;) {
		char *row = b.p + o;
		o += strlen(row) + 1;
		char *ver = row + alen + 1;
		char *sp = strchr(ver, ' ');
		if (!sp)
			continue;
		*sp = 0;
		strip_rev(ver, rows[n].ver, sizeof(rows[n].ver));
		rows[n].cves = sp + 1;
		n++;
	}
	return n;
}

/*
 * Which of one package's recorded CVEs are still open at `have`, the version
 * we pin.
 *
 * Alpine records a CVE once per branch that shipped the fix, each at that
 * branch's version: mbedtls's 3.6.7 row and its 4.1.1 row name the same
 * CVEs. Taken row by row, a pin of 3.6.7 is "behind" 4.1.1 for holes its own
 * release already closed. So the question is asked per CVE: it is CLOSED when
 * a row naming it on have's branch (see branch_of) fixed it at or below have,
 * or when have is at or above every row naming it, and open otherwise. An open
 * CVE's fix is the lowest row above have on have's branch, or, when that
 * branch has none, the lowest row above have on any — the nearest release that
 * closes it. The report's "fixed in" is the newest of those, the version that
 * closes every open one.
 *
 * Alpine writes `0` for "this branch was never affected", and that falls out
 * of the comparison for free: nothing is older than 0.
 */
static int check_one(const char *db, const char *alpine, const char *have,
		     struct cve_hit *hit)
{
	static struct cve_row rows[CVE_ROWS];
	char *copy = NULL;
	int n = rows_for(db, alpine, rows, CVE_ROWS, &copy);
	char hb[64];
	branch_of(have, hb, sizeof(hb));

	hit->cves[0] = 0;
	hit->fixed[0] = 0;
	for (int r = 0; r < n; r++) {
		if (kp_vercmp(have, rows[r].ver) >= 0)
			continue;	/* every CVE here is closed by this row */
		char one[64];
		for (const char *cv = rows[r].cves; *cv;) {
			const char *comma = strchr(cv, ',');
			size_t len = comma ? (size_t)(comma - cv) : strlen(cv);
			if (len >= sizeof(one))
				len = sizeof(one) - 1;
			memcpy(one, cv, len);
			one[len] = 0;
			cv = comma ? comma + 1 : cv + len;
			if (cve_seen(hit->cves, one))
				continue;

			int closed = 0;
			const char *on_branch = NULL, *any = NULL;
			for (int k = 0; k < n && !closed; k++) {
				if (!cve_seen(rows[k].cves, one))
					continue;
				char rb[64];
				branch_of(rows[k].ver, rb, sizeof(rb));
				int same = !strcmp(rb, hb);
				if (kp_vercmp(rows[k].ver, have) <= 0) {
					closed = same;
					continue;
				}
				if (!any || kp_vercmp(rows[k].ver, any) < 0)
					any = rows[k].ver;
				if (same && (!on_branch ||
					     kp_vercmp(rows[k].ver,
						       on_branch) < 0))
					on_branch = rows[k].ver;
			}
			if (closed)
				continue;
			const char *fix = on_branch ? on_branch : any;
			if (hit->cves[0])
				append(hit->cves, sizeof(hit->cves), ",");
			append(hit->cves, sizeof(hit->cves), one);
			if (fix && (!hit->fixed[0] ||
				    kp_vercmp(fix, hit->fixed) > 0))
				kb_strlcpy(hit->fixed, fix, sizeof(hit->fixed));
		}
	}
	free(copy);
	return hit->cves[0] != 0;
}

/* Does the database carry any row for this name? Matched whole at a line
 * start, so `aws-lc` does not match `aws-lc-rs`. */
static int db_knows(const char *db, const char *name)
{
	char probe[256];
	snprintf(probe, sizeof(probe), "\n%.120s ", name);
	if (strstr(db, probe))
		return 1;
	size_t n = strlen(name);
	return !strncmp(db, name, n) && db[n] == ' ';
}

/*
 * The bundled inventory: one `<port> <crate> <component> <version>` row per
 * line, `#` comments. A missing file is an empty inventory; a malformed row is
 * skipped.
 */
static int load_bundled(const char *path, struct cve_bundled *out, int max)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	int n = 0;

	if (!data)
		return 0;
	for (char *line = data; line && *line && n < max;) {
		char *nl = strchr(line, '\n');
		if (nl)
			*nl = 0;
		struct cve_bundled b = {0};
		if (*line != '#' &&
		    sscanf(line, "%127s %63s %63s %63s", b.port, b.crate,
			   b.component, b.version) == 4)
			out[n++] = b;
		line = nl ? nl + 1 : NULL;
	}
	free(data);
	return n;
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
			if ((*p)[0] == '.')
				continue;	/* sidecars and the lock */
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

/* Above the tree's port count: collect() stops at this many, and a package
 * past the limit is neither checked nor counted. */
#define CVE_MAX 4096

int kdt_cve(int argc, char **argv, const char *tty_accent,
	    const char *tty_warn, const char *tty_reset)
{
	int json = 0;
	const char *only = NULL;

	/* One port or every one. A second name is refused rather than taken in
	 * place of the first: the report would then answer for a port the
	 * caller never meant to ask about alone, and say nothing of the others. */
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json")) {
			json = 1;
		} else if (argv[i][0] != '-' && !only) {
			only = argv[i];
		} else {
			fprintf(stderr, "usage: kdos cve [--json] [port]\n");
			return 2;
		}
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
		if (!db_knows(db, alpine[i])) {
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

	/* A bundled copy is checked only when its port is: installed on a
	 * running system, in the tree in a checkout, or the one port named. */
	const char *bpath = getenv("KDOS_BUNDLED");
	if (!bpath || !*bpath)
		bpath = CVE_DEFAULT_BUNDLED;
	static struct cve_bundled inv[CVE_MAX], bundled[CVE_MAX];
	int ninv = load_bundled(bpath, inv, CVE_MAX), nbund = 0, bunknown = 0;
	for (int r = 0; r < ninv; r++) {
		if (only && strcmp(inv[r].port, only))
			continue;
		int present = 0;
		for (int i = 0; i < n && !present; i++)
			present = !strcmp(names[i], inv[r].port);
		if (!present)
			continue;
		struct cve_bundled b = inv[r];
		b.known = db_knows(db, b.component);
		if (!b.known)
			bunknown++;
		struct cve_hit h = {0};
		if (b.known && check_one(db, b.component, b.version, &h)) {
			b.behind = 1;
			snprintf(h.port, sizeof(h.port), "%.60s/%.60s", b.port,
				 b.component);
			kb_strlcpy(h.have, b.version, sizeof(h.have));
			if (nhit < CVE_MAX)
				hits[nhit++] = h;
		}
		bundled[nbund++] = b;
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
		printf("  ],\n  \"bundled\": [\n");
		for (int i = 0; i < nbund; i++)
			printf("    {\"port\": \"%s\", \"crate\": \"%s\", "
			       "\"component\": \"%s\", \"version\": \"%s\", "
			       "\"in_database\": %s, \"behind\": %s}%s\n",
			       bundled[i].port, bundled[i].crate,
			       bundled[i].component, bundled[i].version,
			       bundled[i].known ? "true" : "false",
			       bundled[i].behind ? "true" : "false",
			       i + 1 < nbund ? "," : "");
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
		if (nbund) {
			printf("\n  bundled copies\n");
			for (int i = 0; i < nbund; i++)
				printf("  %-22s %-12s %s %s%s\n",
				       bundled[i].port, bundled[i].version,
				       bundled[i].component, bundled[i].crate,
				       bundled[i].known ? ""
							: "  (not in the database)");
		}
		printf("\n  %d checked, %d not in the database, %d behind a "
		       "recorded fix\n", checked, unknown, nhit);
		if (nbund)
			printf("  %d bundled copies, %d not in the database\n",
			       nbund, bunknown);
		if (age > CVE_STALE_DAYS)
			printf("  %sthe database is %d days old — a newer one "
			       "arrives with kdos-tools (`kdos update`), or "
			       "KDOS_SECDB names another%s\n",
			       tty_warn, age, tty_reset);
		if (unknown)
			printf("  packages Alpine does not carry under that "
			       "name are UNKNOWN, not clean (`secdb =` in a "
			       "recipe maps them)\n");
	}

	free(db);
	return nhit ? 1 : 0;
}
