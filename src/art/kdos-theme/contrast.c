/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the contrast check, over files
 *
 *   contrast [-q] FILE...
 *
 * Reads each KColorScheme file (or a kdeglobals carrying the same groups) and
 * holds every role to the floor roles.c solved it against: text 4.5:1 and a
 * decoration 3:1, each against the set's BackgroundNormal AND its
 * BackgroundAlternate, and the window titles 4.5:1 on their own backgrounds.
 * One row per scheme, group and role; the exit status is 0 only when every
 * row holds.
 *
 * It reads the files rather than recomputing the palette on purpose: what is
 * measured is what Plasma will read, so a file edited after it was generated,
 * or written by some other generator, is measured as it is.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

/* "R,G,B" or "R,G,B,A", each 0-255; anything else is not a colour this check
 * can measure, so it reads as missing. */
static uint32_t parse_rgb(const char *v)
{
	unsigned c[4];
	int n = 0;

	if (!v)
		return KTH_MISSING;
	while (n < 4) {
		char *end;
		unsigned long x = strtoul(v, &end, 10);

		if (end == v || x > 255)
			return KTH_MISSING;
		c[n++] = (unsigned)x;
		while (*end == ' ' || *end == '\t')
			end++;
		if (*end != ',') {
			v = end;
			break;
		}
		v = end + 1;
	}
	while (*v == ' ' || *v == '\t' || *v == '\r')
		v++;
	if ((n != 3 && n != 4) || *v)
		return KTH_MISSING;
	return (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
}

static void load(KthIni *ini, KthPalette *p)
{
	for (int s = 0; s < KTH_NSET; s++) {
		KthGroup *g = kth_ini_group(ini, kth_set_group[s], 0);

		for (int k = 0; k < KTH_NROLE; k++)
			p->set[s][k] = parse_rgb(kth_ini_get(g, kth_role_key[k]));
	}
	KthGroup *wm = kth_ini_group(ini, "WM", 0);

	for (int k = 0; k < KTH_NWM; k++)
		p->wm[k] = parse_rgb(kth_ini_get(wm, kth_wm_key[k]));
}

typedef struct {
	const char *scheme;
	int quiet;
} Report;

static void ratio(int v)
{
	if (v < 0)
		printf("  %6s", "-");
	else
		printf("  %3d.%02d", v / 100, v % 100);
}

static void row(void *ctx, const char *group, const char *role, int a, int b,
		int floor, int ok)
{
	const Report *r = ctx;

	if (r->quiet && ok)
		return;
	printf("%-14s %-24s %-20s", r->scheme, group, role);
	ratio(a);
	ratio(b);
	printf("  >=%d.%02d  %s\n", floor / 100, floor % 100,
	       ok ? "ok" : a < 0 ? "MISSING" : "FAIL");
}

int kth_contrast(int nfile, char **files, int quiet)
{
	int bad = 0, unread = 0;

	if (!quiet)
		printf("%-14s %-24s %-20s  %6s  %6s  %s\n", "scheme", "group",
		       "role", "normal", "alt", "floor");
	for (int i = 0; i < nfile; i++) {
		KthIni ini;
		KthPalette p;

		if (kth_ini_load(&ini, files[i]) < 0 || !ini.ng) {
			kb_warn("%s: not a colour scheme", files[i]);
			kth_ini_free(&ini);
			unread++;
			continue;
		}
		load(&ini, &p);

		const char *id = kth_ini_get(kth_ini_group(&ini, "General", 0),
					     "ColorScheme");
		Report r = { id && *id ? id : kb_basename(files[i]), quiet };

		bad += kth_check(&p, row, &r);
		kth_ini_free(&ini);
	}
	if (bad)
		fprintf(stderr, "kdos-theme: %d row%s below the floor\n", bad,
			bad == 1 ? "" : "s");
	return bad || unread ? 1 : 0;
}
