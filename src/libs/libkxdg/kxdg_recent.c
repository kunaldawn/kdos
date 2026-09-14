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
 *
 * THE WRITE HALF MUST SEE THE END OF THE FILE. The prune keeps the LAST
 * bookmarks in the buffer it was handed, so a buffer that is not the file's
 * tail keeps the oldest and destroys every newer one. A store too large to
 * hold whole is therefore left untouched rather than rewritten from a
 * fragment. The read half keeps its own cap; it only ever wants the newest.
 *
 * EVERY VALUE WRITTEN IS XML-ESCAPED AND EVERY VALUE READ BACK IS UNESCAPED.
 * The scanner here tolerates anything, but the other readers of this store
 * are real XML parsers: one bookmark carrying a bare `&` or `"` makes the
 * whole file unreadable, and that costs every program on the machine its
 * list. Escaping only the writer would be worse than escaping neither — a
 * name stored as `Foo &amp; Bar` still has to match the raw `Foo & Bar` the
 * caller asks with, or that application's jump list is permanently empty.
 *
 * THE PARSE IS MEMOIZED on the store's size and mtime, because the Find
 * surface rebuilds its rows on every keystroke and a scan costs a whole-file
 * read, a backward walk and an unescape per bookmark. EXISTENCE IS NOT
 * memoized: the store is unchanged by a file being deleted, so a memo hit
 * re-runs access() over its rows before handing them out, or a destination
 * that opens nothing stays on the menu until some program records an open.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <time.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kxdg.h"

/* Bounded: the file grows without limit and this runs while a menu is opening.
 * Reading the first megabyte covers thousands of bookmarks. */
#define RECENT_MAX_BYTES (1024 * 1024)

/* The rewrite has to hold the whole store, so it needs a bound of its own.
 * The writer caps at RECENT_KEEP bookmarks; anything past this is not a store
 * this code produced and is left alone rather than rewritten. */
#define RECENT_MAX_REWRITE (16 * 1024 * 1024)

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

/*
 * XML-escaping for an attribute value. The bound leaves room for the longest
 * entity, so the output can never stop mid-escape and emit the bare `&` this
 * exists to prevent. Bytes below 0x20 are dropped: XML 1.0 cannot carry them
 * in any form, escaped or not.
 */
static void xml_attr(const char *src, char *dst, size_t n)
{
	size_t o = 0;

	for (const unsigned char *p = (const unsigned char *)src;
	     *p && o + 6 < n; p++) {
		const char *ent = NULL;

		if (*p < 0x20)
			continue;
		switch (*p) {
		case '&':
			ent = "&amp;";
			break;
		case '<':
			ent = "&lt;";
			break;
		case '>':
			ent = "&gt;";
			break;
		case '"':
			ent = "&quot;";
			break;
		case '\'':
			ent = "&apos;";
			break;
		}
		if (ent) {
			size_t l = strlen(ent);

			memcpy(dst + o, ent, l);
			o += l;
			continue;
		}
		dst[o++] = (char)*p;
	}
	dst[o] = '\0';
}

/*
 * The inverse, for a value compared against what a caller asked with. These
 * five entities are all this store's writers emit; anything else stands as
 * written, which for a scanner is the right failure.
 */
static void xml_unattr(const char *src, size_t len, char *dst, size_t n)
{
	static const struct {
		const char *ent;
		char ch;
	} TAB[] = {
		{ "&amp;", '&' },  { "&lt;", '<' },    { "&gt;", '>' },
		{ "&quot;", '"' }, { "&apos;", '\'' },
	};
	size_t o = 0;

	for (size_t i = 0; i < len && o + 1 < n; i++) {
		size_t k = 0;

		if (src[i] == '&') {
			for (; k < sizeof(TAB) / sizeof(TAB[0]); k++) {
				size_t el = strlen(TAB[k].ent);

				if (len - i >= el &&
				    !strncmp(src + i, TAB[k].ent, el)) {
					dst[o++] = TAB[k].ch;
					i += el - 1;
					break;
				}
			}
			if (k < sizeof(TAB) / sizeof(TAB[0]))
				continue;
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

/*
 * The head of the store for a reader, NUL-terminated, or NULL. The caller
 * frees. `size` is the file's size as the caller already stat'd it: the
 * allocation is sized from the file, because RECENT_MAX_BYTES is a bound and
 * not an expectation and a real store is a fraction of it.
 */
static char *store_read(const char *path, off_t size, long *sz_out)
{
	size_t want;
	char *buf;
	long sz;
	FILE *f;

	if (size <= 0)
		return NULL;
	want = (size_t)size < RECENT_MAX_BYTES ? (size_t)size
					       : RECENT_MAX_BYTES;
	f = fopen(path, "r");
	if (!f)
		return NULL;
	buf = malloc(want + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	sz = (long)fread(buf, 1, want, f);
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
 * The WHOLE store, for the rewrite. Returns 1 with *out set when the file was
 * read, 1 with *out NULL when there is no store yet, and 0 when a store exists
 * that cannot be held here — too large, or unreadable. The caller must leave
 * the file alone on 0: rewriting from anything short of the whole file drops
 * every bookmark past the end of the buffer, and those are the newest.
 */
static int store_read_all(const char *path, char **out)
{
	struct stat st;
	size_t len = 0;
	char *buf;

	*out = NULL;
	if (stat(path, &st) != 0)
		return 1;
	if (!S_ISREG(st.st_mode) || st.st_size > RECENT_MAX_REWRITE)
		return 0;
	if (st.st_size == 0)
		return 1;
	buf = kb_read_all(path, &len);
	if (!buf)
		return 0;
	if (len == 0) {
		free(buf);
		return 1;
	}
	*out = buf;
	return 1;
}

/*
 * THE FILTERED ANSWER, KEYED ON THE STORE'S IDENTITY AND THE QUESTION. The
 * store is its path, its size and its mtime — the path because XDG_DATA_HOME
 * moves the store and a fixture that moves it must not read the last one's
 * answer. The question is `app`: kxdg_recent asks for one application's
 * entries and kxdg_recent_all for every one, so a memo that ignored it would
 * hand a jump list the whole store. `req` is the `max` the memo was filled
 * at, so a larger request than the one that filled it rescans rather than
 * answering short.
 */
#define RECENT_MEMO_ROWS 64

static struct {
	struct timespec mtim;
	off_t size;
	char path[512];
	char app[64];
	int all;
	int req;
	int n;
	int valid;
	char paths[RECENT_MEMO_ROWS][512];
} g_memo;

/*
 * One backward walk, shared by both readers. `app` NULL means every
 * application's entries — which is what a Recent list on a menu wants, where
 * a jump list wants one program's.
 */
static int recent_scan(const char *app, char out[][512], int max)
{
	char path[512];
	struct stat st;
	char *buf;
	long sz = 0;
	int n = 0, memoize;

	if (max <= 0)
		return 0;
	if (!store_path(path, sizeof(path)) || stat(path, &st) != 0)
		return 0;

	memoize = max <= RECENT_MEMO_ROWS &&
		  (!app || strlen(app) < sizeof(g_memo.app));
	if (memoize && g_memo.valid && g_memo.all == !app &&
	    g_memo.size == st.st_size && max <= g_memo.req &&
	    g_memo.mtim.tv_sec == st.st_mtim.tv_sec &&
	    g_memo.mtim.tv_nsec == st.st_mtim.tv_nsec &&
	    !strcmp(g_memo.path, path) &&
	    (!app || !strcmp(g_memo.app, app))) {
		/*
		 * THE PARSE IS MEMOIZED, EXISTENCE IS NOT. The filter below
		 * asks the filesystem and the store says nothing about it, so
		 * a hit that copied its rows out verbatim would keep offering
		 * a file that was deleted since — a row that opens nothing,
		 * for as long as no program records an open. The rows that
		 * fail stay IN the memo: a file that comes back belongs in
		 * the list again.
		 *
		 * WHICH IS WHY THE MEMO HOLDS CANDIDATES AND NOT SURVIVORS.
		 * It is filled to RECENT_MEMO_ROWS rather than to the `max`
		 * that filled it, so a row that fails here is replaced by the
		 * next-oldest — a list of six that loses one comes back as
		 * six, not five.
		 */
		for (int i = 0; i < g_memo.n && n < max; i++)
			if (access(g_memo.paths[i], R_OK) == 0)
				kb_strlcpy(out[n++], g_memo.paths[i], 512);
		return n;
	}

	buf = store_read(path, st.st_size, &sz);
	if (!buf)
		return 0;

	/*
	 * A MEMOISED SCAN COLLECTS CANDIDATES, and the access() filter runs
	 * over them afterwards — the same filter the hit path runs, over the
	 * same rows. It walks to RECENT_MEMO_ROWS rather than to `max` so
	 * there is something behind a row that later disappears.
	 */
	char cand[RECENT_MEMO_ROWS][512];
	char (*dst)[512] = memoize ? cand : out;
	int lim = memoize ? RECENT_MEMO_ROWS : max;

	/*
	 * BACKWARDS, because the file is written in the order things were
	 * added and a jump list wants the newest first. Walking forward and
	 * reversing afterwards would mean holding every match.
	 */
	for (const char *p = buf + sz; p > buf && n < lim;) {
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
				char nm[256];

				xml_unattr(name, nlen, nm, sizeof(nm));
				if (!strcasecmp(nm, app)) {
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

		/*
		 * XML UNESCAPE FIRST, PERCENT-DECODE SECOND. The store is
		 * shared, and a writer that leaves `&` raw in a URI and then
		 * escapes the attribute stores `file:///tmp/a&amp;b`; decoded
		 * the other way round the path keeps the entity and fails the
		 * access() below, dropping a live entry without a trace. The
		 * order is safe for what this library writes, which
		 * percent-encodes `&` to %26 before the attribute is formed,
		 * so the unescape is a no-op on it.
		 */
		href = attr_val(open, close, "href", &hlen);
		if (!href || hlen < 8 || hlen >= sizeof(raw))
			continue;
		xml_unattr(href, hlen, raw, sizeof(raw));
		if (strncmp(raw, "file://", 7))
			continue;
		unescape(raw + 7, dst[n], 512);
		/* A recent file that has been deleted is not a destination —
		 * asked here for a scan that answers directly, and after the
		 * memo is filled for one that does not, so the two paths
		 * apply the same test to the same rows. */
		if (!memoize && access(dst[n], R_OK) != 0)
			continue;
		int dup = 0;

		for (int i = 0; i < n; i++)
			if (!strcmp(dst[i], dst[n]))
				dup = 1;
		if (!dup)
			n++;
	}
	free(buf);

	if (memoize) {
		g_memo.mtim = st.st_mtim;
		g_memo.size = st.st_size;
		g_memo.all = !app;
		/* Every later ask of `max <= RECENT_MEMO_ROWS` is a hit: the
		 * rows held are candidates, not one caller's answer. */
		g_memo.req = RECENT_MEMO_ROWS;
		g_memo.n = n;
		g_memo.valid = 1;
		kb_strlcpy(g_memo.path, path, sizeof(g_memo.path));
		kb_strlcpy(g_memo.app, app ? app : "", sizeof(g_memo.app));
		for (int i = 0; i < n; i++)
			kb_strlcpy(g_memo.paths[i], cand[i], 512);

		/* And the answer is the filtered head of them. */
		int k = 0;

		for (int i = 0; i < n && k < max; i++)
			if (access(cand[i], R_OK) == 0)
				kb_strlcpy(out[k++], cand[i], 512);
		n = k;
	}
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

/* How many bookmarks one rewrite will cut. A store carrying more duplicates
 * of one URI than this keeps the surplus, which the readers dedupe; an
 * unbounded list here would let a pathological store size a stack array. */
#define RECENT_MAX_CUTS 8

static int is_cut(const char *const *cut, int ncut, const char *p)
{
	for (int i = 0; i < ncut; i++)
		if (cut[i] == p)
			return 1;
	return 0;
}

int kxdg_recent_add(const char *app, const char *path, const char *mime)
{
	char store[512], tmp[544], uri[1200], esc[1100], stamp[32], type[128];
	char eapp[768], etype[768];
	char *buf = NULL;
	const char *body = "", *cut[RECENT_MAX_CUTS];
	int ncut = 0;
	size_t ulen;
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

	/* A store this code cannot hold whole is left exactly as it is: a
	 * rewrite from a fragment drops every bookmark past the fragment, and
	 * in an append-ordered file those are the newest. */
	if (!store_read_all(store, &buf))
		return -1;
	if (buf) {
		char *end = strstr(buf, "</xbel>");

		/*
		 * CUT EVERY OLD ONE OUT WHOLE. A second bookmark for one URI
		 * is one the readers offer twice, and the older of the two is
		 * the one a backward walk finds first.
		 *
		 * The URI has to match a COMPLETE attribute value, delimiter
		 * to delimiter: as a bare substring `file:///tmp/a` lands
		 * inside `href="file:///tmp/ab"` and cuts out an unrelated
		 * file's bookmark. Both quote characters are accepted on both
		 * sides, because the other writers of this store are not
		 * obliged to use double quotes — and a needle that misses
		 * only leaves a duplicate, which the readers survive, where
		 * one that over-matches destroys somebody else's entry.
		 */
		ulen = strlen(uri);
		for (const char *hit = strstr(buf, uri);
		     hit && ncut < RECENT_MAX_CUTS;
		     hit = strstr(hit + 1, uri)) {
			const char *a, *b;

			if (hit == buf ||
			    (hit[-1] != '"' && hit[-1] != '\''))
				continue;
			if (hit[ulen] != '"' && hit[ulen] != '\'')
				continue;
			a = bookmark_start(buf, hit);
			b = a ? strstr(a, "</bookmark>") : NULL;
			if (a && b && !is_cut(cut, ncut, a))
				cut[ncut++] = a;
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
			if (!is_cut(cut, ncut, q))
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
			if (is_cut(cut, ncut, nx)) {
				q = close;
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

	/* The href is already percent-encoded, so it carries no XML
	 * metacharacter; the caller's name and MIME type are arbitrary and
	 * do. */
	xml_attr(app, eapp, sizeof(eapp));
	xml_attr(type, etype, sizeof(etype));

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
		uri, stamp, stamp, stamp, etype, eapp, eapp, stamp);
	fputs("</xbel>\n", f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	free(buf);
	g_memo.valid = 0;

	/* TEMP AND RENAME. The store is shared with every other program on the
	 * machine that keeps recents, and a half-written one is one they all
	 * lose. */
	if (rename(tmp, store) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}
