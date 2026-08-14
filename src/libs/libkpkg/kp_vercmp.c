/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kp_vercmp — which of two version strings is newer
 *
 * It lives in libkpkg rather than in kdos-portup because it now has TWO
 * consumers that must never disagree: portup asks "is upstream newer than what
 * we pin", and `kdos cve` asks "is what we pin older than the version that
 * fixed this CVE". Those are the same question, and a second implementation of
 * it would eventually answer them differently.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "kpkg.h"

/* One token of a version: either a number or a letter run. Splitting this way
 * is what makes 1.10 > 1.9 (numeric) while 3.6a > 3.6 (a letter run after an
 * equal numeric prefix is an increment). For numeric tokens, track whether
 * they saturated (digit run too long for a long) so 25-digit tags still order
 * correctly instead of wrapping to undefined behaviour. */
typedef struct {
	int is_num;
	long num;
	int num_saturated;
	int num_digits;		/* significant digits after leading zeros */
	char num_run[64];	/* the full digit run (with leading zeros) for
				   lexical comparison when saturated */
	char alpha[32];
} Tok;

static const char *next_tok(const char *p, Tok *t)
{
	while (*p && !isalnum((unsigned char)*p))
		p++;			/* separators carry no order */
	if (!*p)
		return NULL;
	memset(t, 0, sizeof(*t));
	if (isdigit((unsigned char)*p)) {
		t->is_num = 1;
		const char *run_start = p;
		/* Skip leading zeros and count significant digits. */
		while (*p == '0' && isdigit((unsigned char)*(p+1)))
			p++;
		const char *start = p;
		/* Saturate rather than wrap. These strings arrive from forge tag
		 * feeds and directory listings, where a 25-digit run is a legal tag
		 * and signed overflow would be undefined behaviour that orders two
		 * versions differently depending on the compiler. A run too long to
		 * represent is by definition larger than one that fits, so length
		 * decides once the value cannot. */
		while (isdigit((unsigned char)*p)) {
			int digit = *p - '0';
			/* Check if adding this digit would overflow (t->num > LONG_MAX/10
			 * or (t->num == LONG_MAX/10 && digit > 7)). Use a conservative
			 * bound to be safe. */
			if (t->num > 922337203685477580LL ||
			    (t->num == 922337203685477580LL && digit > 7)) {
				t->num_saturated = 1;
			}
			if (!t->num_saturated)
				t->num = t->num * 10 + digit;
			p++;
		}
		t->num_digits = (int)(p - start);
		/* Store the full digit run for lexical comparison when saturated. */
		size_t run_len = p - run_start;
		if (run_len >= sizeof(t->num_run))
			run_len = sizeof(t->num_run) - 1;
		memcpy(t->num_run, run_start, run_len);
		t->num_run[run_len] = 0;
	} else {
		size_t i = 0;
		while (isalpha((unsigned char)*p) && i < sizeof(t->alpha) - 1)
			t->alpha[i++] = (char)tolower((unsigned char)*p++);
	}
	return p;
}

/* rc/pre/alpha/beta mean "before the release with the same numeric prefix";
 * every other letter run (tmux's `a`, OpenSSH's `p`) means "after it". That
 * asymmetry is the entire subtlety of this comparator. */
static int alpha_is_prerelease(const char *a)
{
	return !strcmp(a, "rc") || !strcmp(a, "pre") || !strcmp(a, "alpha") ||
	       !strcmp(a, "beta") || !strcmp(a, "dev");
}

int kp_vercmp(const char *a, const char *b)
{
	const char *pa = a, *pb = b;
	Tok ta, tb;

	for (;;) {
		const char *na = next_tok(pa, &ta);
		const char *nb = next_tok(pb, &tb);

		if (!na && !nb)
			return 0;
		/* One side ran out. Trailing components are significant (dpkg/rpm
		 * convention); a prerelease marker is the only thing that lowers
		 * a version. Both branches must mirror each other exactly so that
		 * cmp(a,b) is the negation of cmp(b,a). */
		if (!na)
			return tb.is_num ? -1
					 : (alpha_is_prerelease(tb.alpha) ? 1 : -1);
		if (!nb)
			return ta.is_num ? 1
					 : (alpha_is_prerelease(ta.alpha) ? -1 : 1);

		if (ta.is_num && tb.is_num) {
			/* If either side saturated, compare by digit count; if equal,
			 * compare lexically. Otherwise compare numerically. */
			if (ta.num_saturated || tb.num_saturated) {
				if (ta.num_digits != tb.num_digits)
					return ta.num_digits < tb.num_digits ? -1 : 1;
				/* Same digit count, both saturated or one saturated with
				 * same digits as the other: compare lexically. */
				int r = strcmp(ta.num_run, tb.num_run);
				if (r)
					return r < 0 ? -1 : 1;
			} else if (ta.num != tb.num)
				return ta.num < tb.num ? -1 : 1;
		} else if (!ta.is_num && !tb.is_num) {
			int pa_pre = alpha_is_prerelease(ta.alpha);
			int pb_pre = alpha_is_prerelease(tb.alpha);
			if (pa_pre != pb_pre)
				return pa_pre ? -1 : 1;
			int r = strcmp(ta.alpha, tb.alpha);
			if (r)
				return r < 0 ? -1 : 1;
		} else {
			/* number vs letters at the same position: a letter run
			 * is a suffix on the shorter numeric version. */
			if (ta.is_num)
				return alpha_is_prerelease(tb.alpha) ? 1 : -1;
			return alpha_is_prerelease(ta.alpha) ? -1 : 1;
		}
		pa = na;
		pb = nb;
	}
}

void pu_shape(const char *v, char *out, size_t cap)
{
	if (!cap)
		return;		/* no room even for the terminator */

	size_t o = 0;
	int runs = 0;
	size_t first_digits = 0;

	for (const char *p = v; *p && o + 4 < cap;) {
		if (isdigit((unsigned char)*p)) {
			size_t n = 0;
			while (isdigit((unsigned char)*p)) {
				p++;
				n++;
			}
			if (!runs)
				first_digits = n;
			out[o++] = 'N';
			runs++;
		} else if (isalpha((unsigned char)*p)) {
			while (isalpha((unsigned char)*p))
				p++;
			out[o++] = 'a';
			runs++;
		} else {
			out[o++] = *p++;	/* keep the separator */
		}
	}
	out[o] = 0;

	/* A version that is one unbroken digit run gets its LENGTH appended, so
	 * a date (20250826) can never share a shape with a bare counter (1) and
	 * be offered as an update for it. */
	if (runs == 1 && out[0] == 'N' && !out[1])
		snprintf(out, cap, "N%zu", first_digits);
}
