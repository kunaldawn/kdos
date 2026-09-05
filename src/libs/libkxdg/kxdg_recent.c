/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ~/.local/share/recently-used.xbel — what an application opened last
 *
 * This is the data behind a jump list: a right click on a taskbar button
 * offering the files that application was last used with. Windows 7 called
 * them destinations and made the case for them — "the you don't need to even
 * start the program to quickly launch a file" — and freedesktop's recent-files
 * store is where the same facts already live on this machine.
 *
 * A SCANNER, NOT AN XML PARSER, and deliberately: this tree ships no XML
 * library and one bookmark file is not the reason to start. It looks for the
 * two literals the format guarantees — a `<bookmark href="file://...">` and,
 * inside that bookmark, a `<bookmark:application name="...">` — and ignores
 * everything else. Anything it cannot make sense of is simply not offered,
 * which for a convenience list is the right failure: a jump list that is
 * empty costs nothing, and one that is wrong sends somebody to the wrong file.
 *
 * THE WRITE HALF IS THE SAME SCANNER RUN BACKWARDS. It finds the bookmark for
 * this URI, cuts it out whole, and appends a fresh one before `</xbel>` — so a
 * file opened twice has one entry and it is at the end, which is what "newest
 * first" reads as on the way back. A rewrite is temp-and-rename like every
 * other state file here: the store is shared with every other program on the
 * machine that keeps recents, and a half-written one is one they all lose.
 *
 * BOUNDED AT WRITE TIME, not only at read time. The oldest bookmarks past the
 * cap are dropped, because nothing else on this system prunes the file and a
 * store that only grows is one that eventually costs a menu its open.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <time.h>

#include "kbase.h"
#include "kxdg.h"

/* Bounded: the file grows without limit and this runs while a menu is opening.
 * Reading the first megabyte covers thousands of bookmarks. */
#define RECENT_MAX_BYTES (1024 * 1024)

/*
 * An href is percent-encoded and this has to undo it, or every path with a
 * space in it is offered as one that does not exist.
 */
static void unescape(const char *src, char *dst, size_t n)
{
	size_t o = 0;

	for (size_t i = 0; src[i] && o + 1 < n; i++) {
		if (src[i] == '%' && src[i + 1] && src[i + 2]) {
			char hex[3] = { src[i + 1], src[i + 2], 0 };
			char *end;
			long v = strtol(hex, &end, 16);

			if (*end == '\0' && v > 0) {
				dst[o++] = (char)v;
				i += 2;
				continue;
			}
		}
		dst[o++] = src[i];
	}
	dst[o] = '\0';
}

/*
 * The inverse, for a path going in. Everything outside the unreserved set is
 * escaped: an href is read back by every other program that keeps recents, and
 * a raw `#` or `?` in one ends the URI early for all of them.
 */
static void escape(const char *src, char *dst, size_t n)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;

	for (const unsigned char *p = (const unsigned char *)src;
	     *p && o + 4 < n; p++) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
		    (*p >= '0' && *p <= '9') || strchr("-_.~/", *p)) {
			dst[o++] = (char)*p;
			continue;
		}
		dst[o++] = '%';
		dst[o++] = hex[*p >> 4];
		dst[o++] = hex[*p & 0x0f];
	}
	dst[o] = '\0';
}

/* The value of `attr="..."` starting at or after `p`, bounded by `end`. */
static const char *attr_val(const char *p, const char *end, const char *attr,
			    size_t *len)
{
	size_t alen = strlen(attr);

	for (; p && p + alen + 2 < end; p++) {
		const char *q;

		if (strncmp(p, attr, alen) || p[alen] != '=' ||
		    p[alen + 1] != '"')
			continue;
		p += alen + 2;
		q = memchr(p, '"', (size_t)(end - p));
		if (!q)
			return NULL;
		*len = (size_t)(q - p);
		return p;
	}
	return NULL;
}

/* Where the store is. One answer for the read and the write, so a fixture that
 * moves XDG_DATA_HOME moves both. */
static int store_path(char *out, size_t n)
{
	const char *home = getenv("HOME");
	const char *xdg = getenv("XDG_DATA_HOME");

	if (xdg && *xdg)
		return snprintf(out, n, "%s/recently-used.xbel", xdg) < (int)n;
	if (home && *home)
		return snprintf(out, n, "%s/.local/share/recently-used.xbel",
				home) < (int)n;
	return 0;
}

/* The whole file, NUL-terminated, or NULL. The caller frees. */
static char *store_read(long *sz_out)
{
	char path[512];
	char *buf;
	long sz;
	FILE *f;

	if (!store_path(path, sizeof(path)))
		return NULL;
	f = fopen(path, "r");
	if (!f)
		return NULL;
	buf = malloc(RECENT_MAX_BYTES + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	sz = (long)fread(buf, 1, RECENT_MAX_BYTES, f);
	fclose(f);
	if (sz <= 0) {
		free(buf);
		return NULL;
	}
	buf[sz] = '\0';
	*sz_out = sz;
	return buf;
}

/*
 * One backward walk, shared by both readers. `app` NULL means every
 * application's entries — which is what a Recent list on a menu wants, where
 * a jump list wants one program's.
 */
static int recent_scan(const char *app, char out[][512], int max)
{
	char *buf;
	long sz = 0;
	int n = 0;

	if (max <= 0)
		return 0;
	buf = store_read(&sz);
	if (!buf)
		return 0;

	/*
	 * BACKWARDS, because the file is written in the order things were
	 * added and a jump list wants the newest first. Walking forward and
	 * reversing afterwards would mean holding every match.
	 */
	for (const char *p = buf + sz; p > buf && n < max;) {
		const char *open = NULL, *close, *href, *name;
		size_t hlen = 0, nlen = 0;
		char raw[512];

		/* the previous `<bookmark href=` */
		for (const char *q = p - 1; q >= buf; q--)
			if (!strncmp(q, "<bookmark href=", 15)) {
				open = q;
				break;
			}
		if (!open)
			break;
		p = open;

		close = strstr(open, "</bookmark>");
		if (!close)
			close = buf + sz;

		/* Only this application's entries, where one was named. The
		 * name is the one the writer chose, which is conventionally
		 * the binary. */
		if (app) {
			int match = 0;

			name = attr_val(open, close, "name", &nlen);
			while (name) {
				if (nlen == strlen(app) &&
				    !strncasecmp(name, app, nlen)) {
					match = 1;
					break;
				}
				name = attr_val(name + nlen, close, "name",
						&nlen);
			}
			if (!match)
				continue;
		}
		(void)name;

		href = attr_val(open, close, "href", &hlen);
		if (!href || hlen < 8 || strncmp(href, "file://", 7) ||
		    hlen >= sizeof(raw))
			continue;
		snprintf(raw, sizeof(raw), "%.*s", (int)(hlen - 7), href + 7);
		unescape(raw, out[n], 512);
		/* A recent file that has been deleted is not a destination. */
		if (access(out[n], R_OK) == 0) {
			int dup = 0;

			for (int i = 0; i < n; i++)
				if (!strcmp(out[i], out[n]))
					dup = 1;
			if (!dup)
				n++;
		}
	}
	free(buf);
	return n;
}

int kxdg_recent(const char *app, char out[][512], int max)
{
	if (!app || !*app)
		return 0;
	return recent_scan(app, out, max);
}

int kxdg_recent_all(char out[][512], int max)
{
	return recent_scan(NULL, out, max);
}

/* How many bookmarks the store keeps. Nothing else on this system prunes it. */
#define RECENT_KEEP 60

/* The start of the bookmark holding `at`, or NULL. */
static const char *bookmark_start(const char *buf, const char *at)
{
	for (const char *q = at; q >= buf; q--)
		if (!strncmp(q, "<bookmark ", 10))
			return q;
	return NULL;
}

int kxdg_recent_add(const char *app, const char *path, const char *mime)
{
	char store[512], tmp[544], uri[1200], esc[1100], stamp[32], type[128];
	char *buf = NULL;
	long sz = 0;
	const char *body = "", *cut_a = NULL, *cut_b = NULL;
	time_t now = time(NULL);
	struct tm tmv;
	FILE *f;

	if (!app || !*app || !path || path[0] != '/')
		return -1;
	if (!store_path(store, sizeof(store)))
		return -1;

	escape(path, esc, sizeof(esc));
	snprintf(uri, sizeof(uri), "file://%s", esc);
	if (mime && *mime)
		snprintf(type, sizeof(type), "%s", mime);
	else
		kxdg_mime_for_path(path, type, sizeof(type));

	/* UTC, and the format the store's other writers use. A local time here
	 * would sort wrong against theirs the moment a zone changes. */
	gmtime_r(&now, &tmv);
	strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tmv);

	buf = store_read(&sz);
	if (buf) {
		char *end = strstr(buf, "</xbel>");
		char *hit;

		/* CUT THE OLD ONE OUT WHOLE. A second bookmark for one URI is
		 * one the readers offer twice, and the older of the two is
		 * the one a backward walk finds first. */
		hit = strstr(buf, uri);
		if (hit) {
			const char *a = bookmark_start(buf, hit);
			const char *b = a ? strstr(a, "</bookmark>") : NULL;

			if (a && b) {
				cut_a = a;
				cut_b = b + 11;
			}
		}
		if (end)
			*end = '\0';
		body = buf;
	}

	snprintf(tmp, sizeof(tmp), "%s.new", store);
	{
		char dir[512], *slash;

		snprintf(dir, sizeof(dir), "%s", store);
		slash = strrchr(dir, '/');
		if (slash) {
			*slash = '\0';
			kb_mkdir_p(dir);
		}
	}
	f = fopen(tmp, "w");
	if (!f) {
		free(buf);
		return -1;
	}
	if (!buf) {
		fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			   "<xbel version=\"1.0\"\n"
			   "      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n"
			   "      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\">\n");
	} else {
		/*
		 * THE HEADER, THEN THE BOOKMARKS THAT SURVIVE. Two things are
		 * dropped on the way through: the one this URI already had,
		 * and the oldest past the cap. Nothing else on this system
		 * prunes the file, and a store that only grows is one that
		 * eventually costs a menu its open.
		 */
		const char *head_end = strstr(body, "<bookmark ");
		int total = 0, skip;

		if (!head_end)
			head_end = body + strlen(body);
		fwrite(body, 1, (size_t)(head_end - body), f);

		for (const char *q = head_end; (q = strstr(q, "<bookmark "));
		     q += 10)
			if (q != cut_a)
				total++;
		skip = total - (RECENT_KEEP - 1);
		if (skip < 0)
			skip = 0;

		for (const char *q = head_end; q && *q;) {
			const char *nx = strstr(q, "<bookmark ");
			const char *close;

			if (!nx)
				break;
			close = strstr(nx, "</bookmark>");
			if (!close)
				break;
			close += 11;
			if (nx == cut_a) {
				q = cut_b;
				continue;
			}
			if (skip > 0) {
				skip--;
				q = close;
				continue;
			}
			fwrite(nx, 1, (size_t)(close - nx), f);
			fputc('\n', f);
			q = close;
		}
	}

	/*
	 * The three timestamps are the same instant on purpose: this call is
	 * the open, so the file was added, modified and visited now as far as
	 * this store is concerned. Claiming otherwise would need a history
	 * nothing here keeps.
	 */
	fprintf(f,
		"  <bookmark href=\"%s\" added=\"%s\" modified=\"%s\" visited=\"%s\">\n"
		"    <info><metadata owner=\"http://freedesktop.org\">\n"
		"      <mime:mime-type type=\"%s\"/>\n"
		"      <bookmark:applications>\n"
		"        <bookmark:application name=\"%s\" exec=\"&apos;%s %%u&apos;\" modified=\"%s\" count=\"1\"/>\n"
		"      </bookmark:applications>\n"
		"    </metadata></info>\n"
		"  </bookmark>\n",
		uri, stamp, stamp, stamp, type, app, app, stamp);
	fputs("</xbel>\n", f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	free(buf);

	/* TEMP AND RENAME. The store is shared with every other program on the
	 * machine that keeps recents, and a half-written one is one they all
	 * lose. */
	if (rename(tmp, store) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}
