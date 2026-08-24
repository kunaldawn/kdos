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
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int kxdg_recent(const char *app, char out[][512], int max)
{
	const char *home = getenv("HOME");
	const char *xdg = getenv("XDG_DATA_HOME");
	char path[512];
	char *buf;
	long sz;
	FILE *f;
	int n = 0;

	if (!app || !*app || max <= 0)
		return 0;
	if (xdg && *xdg)
		snprintf(path, sizeof(path), "%s/recently-used.xbel", xdg);
	else if (home && *home)
		snprintf(path, sizeof(path),
			 "%s/.local/share/recently-used.xbel", home);
	else
		return 0;

	f = fopen(path, "r");
	if (!f)
		return 0;
	buf = malloc(RECENT_MAX_BYTES + 1);
	if (!buf) {
		fclose(f);
		return 0;
	}
	sz = (long)fread(buf, 1, RECENT_MAX_BYTES, f);
	fclose(f);
	if (sz <= 0) {
		free(buf);
		return 0;
	}
	buf[sz] = '\0';

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

		/* Only this application's entries. The name is the one the
		 * writer chose, which is conventionally the binary. */
		name = attr_val(open, close, "name", &nlen);
		{
			int match = 0;

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
