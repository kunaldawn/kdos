/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — reading a version out of a name, and judging it
 *
 * Pure functions, no I/O: anchors (where a version sits in a file name or a
 * tag), the class a version belongs to, and whether it is a pre-release or a
 * development series. Every input here arrives from a remote listing, so each
 * copy is bounded by its destination and each scan stops at the terminator.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "kbase.h"
#include "kpkg.h"
#include "portup.h"

const char *pu_source_url(const char *src)
{
	const char *sep = strstr(src, "::");
	return sep ? sep + 2 : src;
}

/* The longest one a name ends in wins, so ".tar.gz" is read whole and not as
 * a literal ".tar" before ".gz". A name that ends in none of these (a
 * detached signature, a checksum file, a README) is not an archive and never
 * matches an archive anchor. */
static const char *const ARCHIVES[] = {
	".tar.gz", ".tar.xz", ".tar.bz2", ".tar.lz", ".tar.lzma", ".tar.zst",
	".tar.Z", ".tgz", ".txz", ".tbz2", ".tbz", ".tlz", ".tar", ".zip",
	".7z", ".crate", ".gem", ".pem", ".py", ".gz", ".xz", ".bz2", ".zst",
	NULL
};

size_t pu_archive_len(const char *s, size_t n)
{
	size_t best = 0;
	for (int i = 0; ARCHIVES[i]; i++) {
		size_t l = strlen(ARCHIVES[i]);
		if (l <= n && l > best && !strncasecmp(s + n - l, ARCHIVES[i], l))
			best = l;
	}
	return best;
}

/* `version` with each '.' written as `sep`. */
static int spell(const char *version, char sep, char *out, size_t cap)
{
	size_t n = strlen(version);
	if (n + 1 > cap)
		return -1;
	for (size_t i = 0; i <= n; i++)
		out[i] = version[i] == '.' ? sep : version[i];
	return 0;
}

int pu_anchor_from(const char *name, const char *version, int archive,
		   PuAnchor *a)
{
	static const char seps[] = { '.', '_', '-' };
	memset(a, 0, sizeof(*a));
	if (!version[0])
		return 0;

	for (size_t si = 0; si < sizeof(seps); si++) {
		char sep = seps[si];
		/* A version with no dot is spelled one way only, and one that
		 * already contains the separator cannot be told apart from its
		 * own dots once mapped. */
		if (sep != '.' && (!strchr(version, '.') || strchr(version, sep)))
			continue;
		char sp[PU_MAX_VER];
		if (spell(version, sep, sp, sizeof(sp)))
			continue;
		size_t vl = strlen(sp);

		for (const char *at = strstr(name, sp); at; at = strstr(at + 1, sp)) {
			/* Bounded on both sides: "1.2" is not the version in
			 * "11.2" or in "1.2.3". A dot or separator before it is
			 * fine — LVM2.2.03.35 is "LVM2." + version. */
			if (at > name && isdigit((unsigned char)at[-1]))
				continue;
			const char *end = at + vl;
			if (isdigit((unsigned char)*end))
				continue;
			if ((*end == '.' || *end == sep) &&
			    isdigit((unsigned char)end[1]))
				continue;

			size_t pl = (size_t)(at - name);
			const char *rest = end;
			size_t rl = strlen(rest);
			size_t xl = archive ? pu_archive_len(rest, rl) : 0;
			size_t sl = rl - xl;
			if (pl >= sizeof(a->pre) || sl >= sizeof(a->suf))
				return 0;
			memcpy(a->pre, name, pl);
			a->pre[pl] = 0;
			memcpy(a->suf, rest, sl);
			a->suf[sl] = 0;
			a->sep = sep;
			a->archive = xl > 0;
			a->digit_lead = isdigit((unsigned char)version[0]) != 0;
			return 1;
		}
	}

	/* Mixed: each dot written as whichever of '.', '-' or '_' the source
	 * chose at that place — ImageMagick-7.1.2-31 for 7.1.2.31. Bounded as
	 * above, and read back with every one of them a dot. */
	int ndots = 0;
	for (const char *v = version; *v; v++)
		ndots += *v == '.';
	if (ndots && ndots <= (int)sizeof(a->seps)) {
		size_t vl = strlen(version);
		for (const char *at = name; *at; at++) {
			if (at > name && isdigit((unsigned char)at[-1]))
				continue;
			size_t i = 0;
			int d = 0;
			char seps[sizeof(a->seps)];
			for (; i < vl && at[i]; i++) {
				if (version[i] == '.') {
					if (!strchr(".-_", at[i]))
						break;
					seps[d++] = at[i];
				} else if (at[i] != version[i]) {
					break;
				}
			}
			if (i < vl)
				continue;
			const char *end = at + vl;
			if (isdigit((unsigned char)*end) ||
			    (strchr(".-_", *end) && *end && isdigit((unsigned char)end[1])))
				continue;
			size_t pl = (size_t)(at - name);
			size_t rl = strlen(end);
			size_t xl = archive ? pu_archive_len(end, rl) : 0;
			size_t sl = rl - xl;
			if (pl >= sizeof(a->pre) || sl >= sizeof(a->suf))
				return 0;
			memcpy(a->pre, name, pl);
			a->pre[pl] = 0;
			memcpy(a->suf, end, sl);
			a->suf[sl] = 0;
			a->sep = '*';
			memcpy(a->seps, seps, (size_t)d);
			a->nwidths = d;
			a->archive = xl > 0;
			a->digit_lead = isdigit((unsigned char)version[0]) != 0;
			return 1;
		}
	}

	/* Joined: a pin of digits alone that the source writes in groups —
	 * cacert-2026-08-13.pem for 20260813. One separator throughout, two
	 * to eight groups, and the digits in order are the pin's. */
	size_t pdl = strlen(version);
	if (pdl >= 4 && strspn(version, "0123456789") == pdl) {
		for (const char *at = name; *at; at++) {
			if (!isdigit((unsigned char)*at) ||
			    (at > name && isdigit((unsigned char)at[-1])))
				continue;
			const char *q = at;
			size_t got = 0;
			int nw = 0;
			char jsep = 0;
			unsigned char w[8];
			while (got < pdl && nw < 8) {
				size_t run = 0;
				while (isdigit((unsigned char)q[run]) && got + run < pdl &&
				       q[run] == version[got + run])
					run++;
				if (!run || isdigit((unsigned char)q[run]))
					break;
				w[nw++] = (unsigned char)run;
				got += run;
				q += run;
				if (got == pdl)
					break;
				if (!strchr(".-_", *q) || !*q || (jsep && *q != jsep))
					break;
				jsep = *q++;
			}
			if (got != pdl || nw < 2 ||
			    (*q && strchr(".-_", *q) && isdigit((unsigned char)q[1])))
				continue;
			size_t pl = (size_t)(at - name);
			size_t rl = strlen(q);
			size_t xl = archive ? pu_archive_len(q, rl) : 0;
			size_t sl = rl - xl;
			if (pl >= sizeof(a->pre) || sl >= sizeof(a->suf))
				return 0;
			memcpy(a->pre, name, pl);
			a->pre[pl] = 0;
			memcpy(a->suf, q, sl);
			a->suf[sl] = 0;
			a->sep = jsep;
			a->joined = 1;
			memcpy(a->widths, w, (size_t)nw);
			a->nwidths = nw;
			a->archive = xl > 0;
			a->digit_lead = 1;
			return 1;
		}
	}

	/* Squashed: every dot dropped, as gs10071 spells 10.07.1 and
	 * unzip60 spells 6.0. Tried last, and only for an all-numeric
	 * version of two to eight parts, whose part widths then say where
	 * the dots go back. */
	char sq[PU_MAX_VER];
	int nw = 0;
	size_t o = 0, run = 0;
	for (const char *v = version;; v++) {
		if (isdigit((unsigned char)*v)) {
			if (o + 1 >= sizeof(sq))
				return 0;
			sq[o++] = *v;
			run++;
			continue;
		}
		if ((*v != '.' && *v) || !run || nw >= 8)
			return 0;
		a->widths[nw++] = (unsigned char)run;
		run = 0;
		if (!*v)
			break;
	}
	sq[o] = 0;
	if (nw < 2)
		return 0;
	for (const char *at = strstr(name, sq); at; at = strstr(at + 1, sq)) {
		const char *end = at + o;
		if ((at > name && isdigit((unsigned char)at[-1])) ||
		    isdigit((unsigned char)*end))
			continue;
		size_t pl = (size_t)(at - name);
		size_t rl = strlen(end);
		size_t xl = archive ? pu_archive_len(end, rl) : 0;
		size_t sl = rl - xl;
		if (pl >= sizeof(a->pre) || sl >= sizeof(a->suf))
			return 0;
		memcpy(a->pre, name, pl);
		a->pre[pl] = 0;
		memcpy(a->suf, end, sl);
		a->suf[sl] = 0;
		a->sep = 0;
		a->nwidths = nw;
		a->archive = xl > 0;
		a->digit_lead = 1;
		return 1;
	}
	memset(a, 0, sizeof(*a));
	return 0;
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	c = tolower(c);
	return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

static int is_ver_char(char c)
{
	return isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_' ||
	       c == '+' || c == '~';
}

int pu_anchor_match(const PuAnchor *a, const char *raw, char *v, size_t cap)
{
	/* Percent-decoded, without the query or fragment: a listing's hrefs
	 * and a forge URL's tag are both URL text, and v1.0%2Bgit is the
	 * tag v1.0+git. */
	char buf[PU_MAX_RAW * 2];
	size_t o = 0;
	for (const char *p = raw; *p && *p != '?' && *p != '#' &&
	     o + 1 < sizeof(buf); p++) {
		if (*p == '%' && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
			buf[o++] = (char)(hexval(p[1]) * 16 + hexval(p[2]));
			p += 2;
		} else {
			buf[o++] = *p;
		}
	}
	buf[o] = 0;

	/* A directory entry's trailing slash, and the "/download" SourceForge
	 * puts after every file's own name. */
	while (o && buf[o - 1] == '/')
		buf[--o] = 0;
	if (o > 9 && !strcmp(buf + o - 9, "/download")) {
		o -= 9;
		buf[o] = 0;
	}

	/* The last segment, unless the anchor itself spans segments — a tag
	 * like gopls/v0.20.0 is "gopls/v" + version. */
	const char *name = buf;
	if (!strchr(a->pre, '/')) {
		const char *slash = strrchr(buf, '/');
		if (slash)
			name = slash + 1;
	}

	size_t pl = strlen(a->pre);
	if (strncasecmp(name, a->pre, pl))
		return -1;
	const char *rest = name + pl;
	size_t rl = strlen(rest);

	if (a->archive) {
		size_t xl = pu_archive_len(rest, rl);
		if (!xl)
			return -1;
		rl -= xl;
	}
	size_t sl = strlen(a->suf);
	if (rl < sl || strncasecmp(rest + rl - sl, a->suf, sl))
		return -1;
	rl -= sl;

	if (!rl || rl >= cap || rl >= PU_MAX_VER)
		return -1;
	if (a->digit_lead ? !isdigit((unsigned char)rest[0])
			  : !isalnum((unsigned char)rest[0]))
		return -1;
	if (a->joined) {
		/* Groups of the pin's widths joined by its separator, read
		 * back as the digits alone. */
		size_t o = 0, at = 0;
		for (int i = 0; i < a->nwidths; i++) {
			if (i && (at >= rl || rest[at++] != a->sep))
				return -1;
			for (int k = 0; k < a->widths[i]; k++, at++) {
				if (at >= rl || !isdigit((unsigned char)rest[at]) ||
				    o + 1 >= cap)
					return -1;
				v[o++] = rest[at];
			}
		}
		if (at != rl)
			return -1;
		v[o] = 0;
		return 0;
	}
	if (!a->sep) {
		/* Squashed: digits only, as many as the pin's parts hold,
		 * dotted back at the same widths. */
		size_t want = 0, o = 0, at = 0;
		for (int i = 0; i < a->nwidths; i++)
			want += a->widths[i];
		if (rl != want || want + (size_t)a->nwidths > cap)
			return -1;
		for (size_t i = 0; i < rl; i++)
			if (!isdigit((unsigned char)rest[i]))
				return -1;
		for (int i = 0; i < a->nwidths; i++) {
			if (i)
				v[o++] = '.';
			memcpy(v + o, rest + at, a->widths[i]);
			o += a->widths[i];
			at += a->widths[i];
		}
		v[o] = 0;
		return 0;
	}
	int dots = 0, seps = 0, digits = 0;
	for (size_t i = 0; i < rl; i++) {
		if (!is_ver_char(rest[i]))
			return -1;
		dots += rest[i] == '.';
		seps += a->sep != '.' && rest[i] == a->sep;
		digits += isdigit((unsigned char)rest[i]) != 0;
	}
	if (!digits || (a->sep != '.' && a->sep != '*' && dots && seps))
		return -1;
	if (a->digits_only)
		for (size_t i = 0; i < rl; i++)
			if (isalpha((unsigned char)rest[i]))
				return -1;
	for (size_t i = 0; i < rl; i++)
		v[i] = (a->sep == '*' && strchr(".-_", rest[i])) ||
		       (a->sep != '.' && rest[i] == a->sep) ? '.' : rest[i];
	v[rl] = 0;
	return 0;
}

int pu_version_prefix(const char *v, int ncomp, char *out, size_t cap)
{
	if (ncomp < 1)
		return -1;
	int seen = 1;
	size_t i = 0;
	for (; v[i]; i++) {
		if (v[i] == '.' && ++seen > ncomp)
			break;
	}
	if (seen < ncomp || i >= cap)
		return -1;
	memcpy(out, v, i);
	out[i] = 0;
	return 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * Classes and filters
 * ──────────────────────────────────────────────────────────────────────── */

static int is_prerelease_word(const char *w, size_t n)
{
	static const char *const words[] = {
		"rc", "alpha", "beta", "pre", "preview", "dev", "snapshot",
		"wip", "test", "nightly", "unstable", "trunk", "cr", NULL
	};
	for (int i = 0; words[i]; i++)
		if (strlen(words[i]) == n && !strncasecmp(w, words[i], n))
			return 1;
	return 0;
}

/* A letter run that marks a pre-release: one of the words above anywhere,
 * or a lone a/b before a number (PEP 440's 3.14.0b2, socat's 2.0.0-b9). A
 * lone letter at the end is not one — tmux 3.6a and OpenSSL 1.1.1w are
 * releases. */
static int has_prerelease_marker(const char *s)
{
	for (const char *p = s; *p;) {
		if (!isalpha((unsigned char)*p)) {
			p++;
			continue;
		}
		const char *w = p;
		while (isalpha((unsigned char)*p))
			p++;
		size_t n = (size_t)(p - w);
		if (is_prerelease_word(w, n))
			return 1;
		if (n == 1 && (tolower((unsigned char)*w) == 'a' ||
			       tolower((unsigned char)*w) == 'b') &&
		    w > s && (isdigit((unsigned char)w[-1]) || w[-1] == '-' ||
			      w[-1] == '.' || w[-1] == '_') &&
		    isdigit((unsigned char)*p))
			return 1;
	}
	return 0;
}

/* The numeric components of a dotted version, up to `max`; 0 when `s` is not
 * purely digits and dots. */
static int components(const char *s, long *c, int max)
{
	int n = 0;
	const char *p = s;
	while (*p) {
		if (!isdigit((unsigned char)*p) || n >= max)
			return 0;
		long v = 0;
		int digits = 0;
		while (isdigit((unsigned char)*p)) {
			if (digits++ < 9)
				v = v * 10 + (*p - '0');
			p++;
		}
		c[n++] = v;
		if (*p == '.')
			p++;
		else if (*p)
			return 0;
	}
	return n;
}

/* Letter runs that mark a release of the numbering they follow — before it
 * (rc, beta) or after it (OpenSSH's p1, libburnia's pl02, PEP 440's post1).
 * Any other word is a variant of the file, not a version: 5.1.22_dict,
 * 3.14.7-win32, 5.9.2-doc and 1.5.7-kernel sit beside the release they
 * name. */
static int is_marker_word(const char *w, size_t n)
{
	static const char *const words[] = {
		"rc", "alpha", "beta", "pre", "preview", "dev", "p", "pl",
		"post", "patch", NULL
	};
	if (n == 1)
		return 1;
	for (int i = 0; words[i]; i++)
		if (strlen(words[i]) == n && !strncasecmp(w, words[i], n))
			return 1;
	return 0;
}

/* `s` without a trailing commit id (passt's 2026_07_28.f8df3f1): its digits
 * and letters differ in every release and are no part of the numbering — nor
 * a marker, though a1e48a0 holds an "a" between digits. */
static void strip_commit(const char *s, char *out, size_t cap)
{
	kb_strlcpy(out, s, cap);
	size_t n = strlen(out);
	size_t h = n, hex_alpha = 0;
	while (h && isxdigit((unsigned char)out[h - 1])) {
		hex_alpha += isalpha((unsigned char)out[h - 1]) != 0;
		h--;
	}
	if (hex_alpha && n - h >= 7 && n - h <= 40 && h > 1 &&
	    strchr(".+~-_", out[h - 1]))
		out[h - 1] = 0;
}

/* `s` without a trailing commit id or a trailing pre- or post-release
 * marker: 10.2p1, 1.5.8.pl02, 1.2.3.post1, 1.4rc5, 3.0.0b2, 3.6a and
 * 2.9.0dev.12 all read as their dotted base. */
static void release_base(const char *s, char *out, size_t cap)
{
	strip_commit(s, out, cap);
	size_t n = strlen(out);
	/* Strip a trailing [sep]letters[sep]digits run, once. */
	size_t e = n;
	while (e && isdigit((unsigned char)out[e - 1]))
		e--;
	if (e && (out[e - 1] == '.' || out[e - 1] == '-' || out[e - 1] == '_'))
		e--;
	size_t w = e;
	while (w && isalpha((unsigned char)out[w - 1]))
		w--;
	if (w == e || !w || !is_marker_word(out + w, e - w))
		return;			/* no marker, or nothing before it */
	if (out[w - 1] == '.' || out[w - 1] == '-' || out[w - 1] == '_' ||
	    out[w - 1] == '+')
		w--;
	if (w && isdigit((unsigned char)out[w - 1]))
		out[w] = 0;
}

static int first_run_len(const char *s)
{
	int n = 0;
	while (isdigit((unsigned char)s[n]))
		n++;
	return n;
}

/* A letter run of `s` at `w`, `n` long, also stands as a letter run in `cur`:
 * a word the pin carries (john's 1.9.0.jumbo1) is part of its numbering. */
static int pin_has_word(const char *cur, const char *w, size_t n)
{
	for (const char *p = cur; *p;) {
		if (!isalpha((unsigned char)*p)) {
			p++;
			continue;
		}
		const char *q = p;
		while (isalpha((unsigned char)*p))
			p++;
		if ((size_t)(p - q) == n && !strncasecmp(q, w, n))
			return 1;
	}
	return 0;
}

/* A word in `cand` that no version carries. A letter run is part of one when
 * it is a pre-release word, a post-release marker with its number after it
 * (10.2p1, 1.5.8.pl02, 5.0-post1), a lone a or b with a number after it
 * (3.0.0b2, 2.0.0-b9), a lone letter straight after a digit (3.6a, 1.1.1w),
 * or a word the pin carries too. Anything else names a file beside the
 * release: a platform build (3.8.13-w64), a patch (1.8.1.3.patch), a variant
 * (5.1.22_dict), a branch tag (22-init), an archive's suffix (56.7z). A
 * trailing commit id is not read. */
static int foreign_word(const char *cand, const char *cur)
{
	static const char *const post[] = { "p", "pl", "post", "patch", NULL };
	char cb[PU_MAX_VER];
	strip_commit(cand, cb, sizeof(cb));
	size_t cl = strlen(cb);
	size_t xl = pu_archive_len(cb, cl);
	if (xl && xl < cl)
		return 1;		/* 56.7z: a file name's tail */
	for (const char *p = cb; *p;) {
		if (!isalpha((unsigned char)*p)) {
			p++;
			continue;
		}
		const char *w = p;
		while (isalpha((unsigned char)*p))
			p++;
		size_t n = (size_t)(p - w);
		int num_after = isdigit((unsigned char)*p) ||
				(strchr(".-_", *p) && *p && isdigit((unsigned char)p[1]));
		int digit_before = w > cb && isdigit((unsigned char)w[-1]);
		if (is_prerelease_word(w, n) || pin_has_word(cur, w, n))
			continue;
		int ok = 0;
		for (int i = 0; post[i] && !ok; i++)
			ok = strlen(post[i]) == n && !strncasecmp(w, post[i], n) &&
			     num_after;
		if (!ok && n == 1)
			ok = (strchr("abAB", *w) && num_after) ||
			     (digit_before && !isalpha((unsigned char)*p));
		if (!ok)
			return 1;
	}
	return 0;
}

int pu_in_series(const char *v, const char *series)
{
	size_t n = strlen(series);
	if (!n)
		return 1;
	return !strncmp(v, series, n) && (v[n] == 0 || v[n] == '.');
}

int pu_same_class(const char *cand, const char *cur)
{
	if (foreign_word(cand, cur))
		return 0;
	char cb[PU_MAX_VER], ub[PU_MAX_VER];
	release_base(cand, cb, sizeof(cb));
	release_base(cur, ub, sizeof(ub));

	/* A marker may order a version after its own base, never after a
	 * later base: capstone's 5.0-post1 is older than 5.0.9, though the
	 * comparator puts a letter run after a number. */
	if (kp_vercmp(cand, cur) > 0 && kp_vercmp(cb, ub) < 0)
		return 0;

	long c[16], u[16];
	int nc = components(cb, c, 16), nu = components(ub, u, 16);
	if (nc > 1 && nu > 1) {
		if ((first_run_len(cb) >= 4) != (first_run_len(ub) >= 4))
			return 0;
		/* A zero-padded component where the pin has none of that
		 * width is another numbering: intel's old 600.0132 tags beside
		 * its 26.2.4 ones. The same width is the same scheme — helix's
		 * 26.01 after 25.10. */
		const char *pc = cb, *pu = ub;
		while (*pc && *pu) {
			size_t lc = strspn(pc, "0123456789");
			size_t lu = strspn(pu, "0123456789");
			if (lc > 1 && pc[0] == '0' && lc != lu)
				return 0;
			pc += lc + (pc[lc] == '.');
			pu += lu + (pu[lu] == '.');
		}
		return 1;
	}

	char sc[PU_MAX_VER], su[PU_MAX_VER];
	kp_vershape(cb, sc, sizeof(sc));
	kp_vershape(ub, su, sizeof(su));
	return !strcmp(sc, su);
}

int pu_prerelease(const char *cand, const char *cur)
{
	char cb[PU_MAX_VER], ub[PU_MAX_VER];
	strip_commit(cand, cb, sizeof(cb));
	strip_commit(cur, ub, sizeof(ub));
	if (has_prerelease_marker(ub))
		return 0;
	return has_prerelease_marker(cb);
}

int pu_pretest(const char *cand, const char *cur, char all[][PU_MAX_VER], int n)
{
	/* 90-99 in the third place or later marks the pretest of the next
	 * release: wayland 1.25.91, GNU make 4.4.0.90, X.Org's 26.0.99.902.
	 * Where the pin already has 90 or more in that place, the project
	 * counts that high for real. */
	long c[16], u[16], o[16];
	int nc = components(cand, c, 16), nu = components(cur, u, 16);
	for (int i = 2; i < nc; i++) {
		long ui = i < nu ? u[i] : 0;
		if (c[i] < 90 || c[i] > 99 || ui >= 90)
			continue;
		/* A pretest jumps there from a small number; a counter
		 * passes through the eighties on the way. */
		int walked = 0;
		for (int k = -1; k < n && !walked; k++) {
			int no = components(k < 0 ? cur : all[k], o, 16);
			if (no <= i || o[i] < 80 || o[i] > 89)
				continue;
			walked = 1;
			for (int j = 0; j < i && walked; j++)
				walked = o[j] == c[j];
		}
		if (!walked)
			return 1;
	}
	return 0;
}

static int has_word(const char *list, const char *word)
{
	size_t wl = strlen(word);
	for (const char *p = list; p && *p;) {
		while (*p == ' ' || *p == ',')
			p++;
		const char *w = p;
		while (*p && *p != ' ' && *p != ',')
			p++;
		if ((size_t)(p - w) == wl && !strncmp(w, word, wl))
			return 1;
	}
	return 0;
}

/* The second numeric component, or -1. */
static long minor_of(const char *s)
{
	const char *dot = strchr(s, '.');
	if (!dot || !isdigit((unsigned char)dot[1]))
		return -1;
	return strtol(dot + 1, NULL, 10);
}

/* Hosts whose every project follows one convention. download.gnome.org is
 * not among them: libxml2 2.15, libsecret 0.21 and librsvg 2.63 are stable
 * releases there, so its odd-minor projects declare the key themselves. */
static const struct {
	const char *host;
	const char *conventions;
} HOST_CONVENTIONS[] = {
	{ "gstreamer.freedesktop.org", "odd-minor" },
	{ NULL, NULL }
};

int pu_devseries(const char *cand, const char *cur, const char *conventions,
		 const char *url)
{
	const char *builtin = "";
	const char *h = url ? strstr(url, "://") : NULL;
	if (h) {
		h += 3;
		for (int i = 0; HOST_CONVENTIONS[i].host; i++) {
			size_t l = strlen(HOST_CONVENTIONS[i].host);
			if (!strncmp(h, HOST_CONVENTIONS[i].host, l) &&
			    (h[l] == '/' || h[l] == ':' || !h[l]))
				builtin = HOST_CONVENTIONS[i].conventions;
		}
	}

	long mc = minor_of(cand), mu = minor_of(cur);
	if (mc < 0 || mu < 0)
		return 0;
	if ((has_word(conventions, "odd-minor") || has_word(builtin, "odd-minor")) &&
	    (mc % 2) && !(mu % 2))
		return 1;
	if ((has_word(conventions, "preview-minor") ||
	     has_word(builtin, "preview-minor")) && mc >= 90 && mu < 90)
		return 1;
	return 0;
}
