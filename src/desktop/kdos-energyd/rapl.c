/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   rapl.c — the only thing on this machine that measures energy
 *
 * The kernel's powercap tree exposes a free-running microjoule counter per
 * energy domain. Three properties of it are load-bearing, and getting any of
 * them wrong produces numbers that look plausible and are wrong:
 *
 * DOMAINS NEST, AND THE FLAT DIRECTORY HIDES IT. /sys/class/powercap lists
 * `intel-rapl:0` (a package) beside `intel-rapl:0:0` (that package's core
 * domain) — but the second is a SUBSET of the first, and summing the listing
 * counts the cores twice. A domain is a subdomain exactly when it appears
 * inside another domain's directory, which is what the nesting test here
 * checks. `psys` is the other direction: it is the whole platform and it
 * CONTAINS the packages, so where it exists it replaces them rather than
 * adding to them.
 *
 * THE COUNTER WRAPS. `max_energy_range_uj` is around 65 kJ on this hardware,
 * which at 30 W is a wrap every 36 minutes. A daemon that subtracts naively
 * reports one enormous negative spike per wrap — or, after a clamp, one hour
 * of missing energy — and nothing in the output says so.
 *
 * IT IS ROOT-ONLY, and has been since Linux 5.10 closed PLATYPUS
 * (CVE-2020-8694): unprivileged microsecond-resolution RAPL reads recover AES
 * keys. That is why this is a daemon and not a library call in the panel. It
 * is also why the daemon never republishes the counter: what leaves this
 * process is a per-app percentage over minutes, which is not an oracle for
 * anything.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "energyd.h"

/* `a/b` into a fixed buffer, truncating rather than overrunning. Not snprintf:
 * the powercap root comes from the environment, so its length is unknown to the
 * compiler and every `%s/%s` into a fixed array is a truncation warning that is
 * technically right and cannot be silenced with a bigger buffer. */
static void joinp(char *out, size_t cap, const char *a, const char *b)
{
	size_t n = strlen(a);
	if (n > cap - 2)
		n = cap - 2;
	memcpy(out, a, n);
	out[n++] = '/';
	size_t m = strlen(b);
	if (m > cap - n - 1)
		m = cap - n - 1;
	memcpy(out + n, b, m);
	out[n + m] = '\0';
}

static bool dir_has(const char *parent, const char *child)
{
	char p[700];
	joinp(p, sizeof(p), parent, child);
	return kb_is_dir(p);
}

static unsigned long long read_u64(const char *dir, const char *file, bool *ok)
{
	char path[700], buf[64];
	joinp(path, sizeof(path), dir, file);
	/* kb_read_line_file returns the LENGTH, and only a negative is a
	 * failure — a counter that has just wrapped reads as the single
	 * character "0", which a `!= 0` test would call unreadable and refuse
	 * to start the daemon over. */
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0) {
		if (ok)
			*ok = false;
		return 0;
	}
	if (ok)
		*ok = true;
	return strtoull(buf, NULL, 10);
}

int ke_rapl_open(KeRapl *r)
{
	memset(r, 0, sizeof(*r));
	/* Copied into a bounded buffer rather than used straight from the
	 * environment: every path below is built into a fixed array, and a root
	 * of unknown length is a truncation the compiler is right to refuse. */
	char root[512];
	kb_strlcpy(root, ke_powercap(), sizeof(root));

	int count = 0;
	char **names = kb_listdir(root, &count);
	if (!names) {
		snprintf(r->why, sizeof(r->why),
			 "no %s — this kernel exposes no energy counter", root);
		return -1;
	}

	/*
	 * Two passes. The first collects every domain that carries an
	 * energy_uj; the second drops the ones that live inside another,
	 * because those are already counted in their parent.
	 */
	char cand[KE_MAX_DOMAIN * 4][64];
	int ncand = 0;
	for (int i = 0; i < count && ncand < (int)(sizeof(cand) / sizeof(cand[0]));
	     i++) {
		char path[640], sub[600];
		joinp(sub, sizeof(sub), root, names[i]);
		joinp(path, sizeof(path), sub, "energy_uj");
		if (!kb_path_exists(path))
			continue;
		kb_strlcpy(cand[ncand], names[i], sizeof(cand[0]));
		ncand++;
	}

	bool have_psys = false;
	for (int i = 0; i < ncand; i++) {
		char dir[600], nm[64] = "";
		joinp(dir, sizeof(dir), root, cand[i]);
		char npath[672];
		joinp(npath, sizeof(npath), dir, "name");
		kb_read_line_file(npath, nm, sizeof(nm));
		if (!strcmp(nm, "psys"))
			have_psys = true;
		/* An `uncore` domain is the integrated GPU's share of the
		 * package. Its presence is how the report knows whether GPU
		 * energy is inside the number it is about to print. */
		if (!strcmp(nm, "uncore"))
			r->gpu_inside = true;
	}

	for (int i = 0; i < ncand && r->n < KE_MAX_DOMAIN; i++) {
		bool nested = false;
		for (int j = 0; j < ncand && !nested; j++) {
			if (j == i)
				continue;
			char parent[600];
			joinp(parent, sizeof(parent), root, cand[j]);
			nested = dir_has(parent, cand[i]);
		}
		if (nested)
			continue;

		char dir[600], nm[32] = "";
		joinp(dir, sizeof(dir), root, cand[i]);
		char npath[672];
		joinp(npath, sizeof(npath), dir, "name");
		if (kb_read_line_file(npath, nm, sizeof(nm)) < 1)
			kb_strlcpy(nm, cand[i], sizeof(nm));

		/* psys is the platform and it contains the packages. Where it
		 * exists it is the whole answer; adding a package to it would
		 * count that package twice. */
		if (have_psys && strcmp(nm, "psys"))
			continue;

		KeDomain *d = &r->d[r->n];
		kb_strlcpy(d->name, nm, sizeof(d->name));
		kb_strlcpy(d->path, dir, sizeof(d->path));
		d->range = read_u64(dir, "max_energy_range_uj", NULL);
		r->n++;
	}
	kb_strv_free(names);

	if (r->n == 0) {
		snprintf(r->why, sizeof(r->why),
			 "%s exists but exposes no energy domain", root);
		return -1;
	}

	/* Is it actually readable? On a kernel since 5.10 the answer for
	 * anyone but root is no, and a daemon that discovered that only in its
	 * sampling loop would report a machine that uses no energy at all. */
	bool ok = false;
	read_u64(r->d[0].path, "energy_uj", &ok);
	r->readable = ok;
	if (!ok)
		snprintf(r->why, sizeof(r->why),
			 "%s/energy_uj is not readable — RAPL has been root-only "
			 "since Linux 5.10 (PLATYPUS)", r->d[0].path);
	return ok ? 0 : -1;
}

long long ke_rapl_delta(KeRapl *r)
{
	long long total = 0;
	bool any = false;

	for (int i = 0; i < r->n; i++) {
		KeDomain *d = &r->d[i];
		bool ok = false;
		unsigned long long now = read_u64(d->path, "energy_uj", &ok);
		if (!ok)
			continue;

		if (d->have_last) {
			unsigned long long delta;
			if (now >= d->last) {
				delta = now - d->last;
			} else if (d->range > d->last) {
				/* One wrap. More than one in a window would mean
				 * the sampler stopped for 36 minutes, and there
				 * is no way to tell that from this counter — so
				 * a single wrap is what is assumed and the
				 * interval is kept far below it. */
				delta = (d->range - d->last) + now;
			} else {
				delta = 0;
			}
			total += (long long)delta;
			any = true;
		}
		d->last = now;
		d->have_last = true;
	}
	return any ? total : -1;
}
