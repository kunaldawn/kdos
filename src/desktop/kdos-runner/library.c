/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   library.c — the offline library in KRunner
 *
 * Typing "photosynthesis" finds it wherever this machine holds it, with no
 * network:
 *
 *   the KDOS and KFS books   headings of /usr/share/doc/{kdos,kfs}, opened
 *                            in KHelpCenter at that heading
 *   manual pages             `kdos*` pages, opened in KHelpCenter
 *   Zeal docsets             entries whose name starts with the query, from
 *                            each docset's own SQLite index (zeal.c)
 *   places                   a typed coordinate, opened as a geo: link in
 *                            the map the session gives that scheme
 *   kiwix-serve, :8080       title suggestions from every served ZIM,
 *                            opened in the browser
 *   Kolibri, :8081           resources whose title matches, opened in
 *                            Kolibri's Learn page in the browser
 *
 * The first four are files on this disk and answer at once; the two servers
 * are asked over loopback (http.c) and the Match waits for them up to its
 * deadline (main.c). A server that is not running is a refused connect,
 * which costs nothing.
 *
 * BELOW THE PALETTE. A plain query gets library rows in the Low category, so
 * a verb or an application is never pushed down by an encyclopedia; a query
 * beginning `?` asks for the library alone and its rows are the highest.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "runner.h"

#define LIB_PER_SOURCE 5

static int catrel(int only)
{
	return only ? RN_CAT_HIGHEST : RN_CAT_LOW;
}

const char *rn_library_query(const char *text, int *only)
{
	*only = 0;
	while (text && (*text == ' ' || *text == '\t'))
		text++;
	if (!text || !*text)
		return NULL;
	if (*text == '?') {
		*only = 1;
		text++;
		while (*text == ' ')
			text++;
	}
	return *text ? text : NULL;
}

static void stat_sig(const char *path, char *out, size_t n)
{
	struct stat st;

	if (stat(path, &st) == 0)
		snprintf(out, n, "%lx:%lld:%lld.%ld", (unsigned long)st.st_ino,
			 (long long)st.st_size, (long long)st.st_mtim.tv_sec,
			 (long)st.st_mtim.tv_nsec);
	else
		snprintf(out, n, "-");
}

/* ── the books ─────────────────────────────────────────────────────────── */

typedef struct {
	char *text;		/* the heading, as plain text      */
	char *file;		/* absolute path of the page       */
	char *id;		/* the heading's anchor, or ""     */
	int level;		/* 1..3                            */
	int book;		/* index into books[]              */
} Heading;

static const struct {
	const char *dir;
	const char *name;
	const char *pkg;	/* the kpkg database entry of its package */
} books[] = {
	{ "/usr/share/doc/kdos", "KDOS book", "/var/lib/kpkg/db/kdos-doc" },
	{ "/usr/share/doc/kfs", "KDOS from Scratch", "/var/lib/kpkg/db/kdos-kfs" },
};

#define NBOOKS ((int)(sizeof(books) / sizeof(books[0])))

static Heading *g_heads;
static int g_nheads;
static char g_book_sig[2048];
static int g_books_read;
static int g_async;		/* build a missing index in a child       */
static long long g_build_at;	/* when that child was started, or 0      */
static char g_build_sig[2048];

static long long mono_ms(void);

static void heads_free(void)
{
	for (int i = 0; i < g_nheads; i++) {
		free(g_heads[i].text);
		free(g_heads[i].file);
		free(g_heads[i].id);
	}
	free(g_heads);
	g_heads = NULL;
	g_nheads = 0;
}

static void head_add(const char *text, const char *file, const char *id,
		     int level, int book)
{
	if (g_nheads >= 50000 || !*text)
		return;
	g_heads = kb_realloc(g_heads, (size_t)(g_nheads + 1) * sizeof(*g_heads));
	g_heads[g_nheads].text = kb_strdup(text);
	g_heads[g_nheads].file = kb_strdup(file);
	g_heads[g_nheads].id = kb_strdup(id);
	g_heads[g_nheads].level = level;
	g_heads[g_nheads].book = book;
	g_nheads++;
}

/* Every <h1>..<h3> of one page, with its id. Pandoc and lowdown both write
 * `<hN id="x">text</hN>`, with spans for section numbers inside. */
static void page_headings(const char *file, int book)
{
	char *html = kb_read_whole(file, NULL);

	if (!html)
		return;
	for (char *p = html; (p = strstr(p, "<h"));) {
		char id[256] = "", text[512];
		int level = p[2] - '0';
		char *gt, *end, *ida;

		if (level < 1 || level > 3 || (p[3] != '>' && p[3] != ' ')) {
			p += 2;
			continue;
		}
		gt = strchr(p, '>');
		if (!gt)
			break;
		*gt = '\0';
		ida = strstr(p, " id=\"");
		if (ida) {
			char *q = strchr(ida + 5, '"');

			if (q && (size_t)(q - ida - 5) < sizeof(id)) {
				memcpy(id, ida + 5, (size_t)(q - ida - 5));
				id[q - ida - 5] = '\0';
			}
		}
		end = strstr(gt + 1, "</h");
		if (!end)
			break;
		*end = '\0';
		kb_strlcpy(text, gt + 1, sizeof(text));
		rn_html_text(text);
		for (char *c = text; *c; c++)
			if (*c == '\n' || *c == '\t')
				*c = ' ';
		head_add(text, file, id, level, book);
		p = end + 3;
	}
	free(html);
}

static void walk(const char *dir, int book, int depth)
{
	char **names = kb_listdir(dir, NULL);

	for (char **n = names; n && *n && depth < 6; n++) {
		char path[1024];
		size_t l = strlen(*n);

		if ((*n)[0] == '.')
			continue;
		if (snprintf(path, sizeof(path), "%s/%s", dir, *n) >=
		    (int)sizeof(path) || kb_is_link(path))
			continue;
		if (kb_is_dir(path))
			walk(path, book, depth + 1);
		else if (l > 5 && !strcmp(*n + l - 5, ".html"))
			page_headings(path, book);
	}
	kb_strv_free(names);
}

/*
 * THE INDEX IS BUILT ONCE PER PACKAGE CHANGE, and kept in
 * $XDG_CACHE_HOME/kdos/runner-books with the identity of each book's
 * directory and of its package's kpkg database entry. The database entry is
 * rewritten by every install of the package, so an update that changes only
 * pages in subdirectories still rebuilds the index; the directory alone
 * would not show it.
 *
 * THE RUNNER BUILDS IT IN A CHILD. The KFS book is thousands of pages, and
 * one process answers the palette and the library: a build in the loop
 * would hold every palette Match behind it. Until the child has written the
 * file the library answers without the books, and each Match looks for the
 * file again. By hand (`--library`) and in the tests it is built in place.
 */
void rn_library_async(int on)
{
	g_async = on;
}

static void cache_path(char *out, size_t n)
{
	const char *x = getenv("XDG_CACHE_HOME");

	if (x && *x == '/')
		snprintf(out, n, "%s/kdos/runner-books", x);
	else
		snprintf(out, n, "%s/.cache/kdos/runner-books", kb_home_dir());
}

static int cache_load(const char *sig)
{
	char path[1024], first[2048], *text, *save = NULL, *l;
	FILE *f;

	cache_path(path, sizeof(path));
	/* The signature line alone first: while a child builds, every Match
	 * looks, and a stale index is megabytes. */
	if (!(f = fopen(path, "re")))
		return 0;
	l = fgets(first, sizeof(first), f);
	fclose(f);
	if (!l || strncmp(first, sig, strlen(sig)) || first[strlen(sig)] != '\n')
		return 0;
	if (!(text = kb_read_whole(path, NULL)))
		return 0;
	l = strtok_r(text, "\n", &save);
	if (!l || strcmp(l, sig)) {
		free(text);
		return 0;
	}
	while ((l = strtok_r(NULL, "\n", &save))) {
		char *f[5];
		int nf = 0;

		for (char *p = l; nf < 5; nf++) {
			f[nf] = p;
			p = strchr(p, '\t');
			if (!p) {
				nf++;
				break;
			}
			*p++ = '\0';
		}
		if (nf != 5)
			continue;
		head_add(f[4], f[2], f[3], atoi(f[1]), atoi(f[0]) % NBOOKS);
	}
	free(text);
	return 1;
}

static void cache_save(const char *sig)
{
	char path[1024], dir[1024];
	KbBuf b = { 0 };

	cache_path(path, sizeof(path));
	kb_strlcpy(dir, path, sizeof(dir));
	*strrchr(dir, '/') = '\0';
	if (kb_mkdir_p(dir) != 0)
		return;
	kb_buf_printf(&b, "%s\n", sig);
	for (int i = 0; i < g_nheads; i++)
		if (!strchr(g_heads[i].file, '\t') && !strchr(g_heads[i].file, '\n'))
			kb_buf_printf(&b, "%d\t%d\t%s\t%s\t%s\n", g_heads[i].book,
				      g_heads[i].level, g_heads[i].file,
				      g_heads[i].id, g_heads[i].text);
	kb_write_file_atomic(path, b.p ? b.p : "");
	kb_buf_free(&b);
}

static void books_load(void)
{
	char sig[2048] = "v1", one[160], two[160], dir[1024], pkg[1024];

	for (int i = 0; i < NBOOKS; i++) {
		size_t l = strlen(sig);

		rn_sys(books[i].dir, dir, sizeof(dir));
		rn_sys(books[i].pkg, pkg, sizeof(pkg));
		stat_sig(dir, one, sizeof(one));
		stat_sig(pkg, two, sizeof(two));
		if (snprintf(sig + l, sizeof(sig) - l, "|%s|%s|%s", dir, one,
			     two) >= (int)(sizeof(sig) - l))
			sig[l] = '\0';
	}
	if (g_books_read && !strcmp(sig, g_book_sig))
		return;
	heads_free();
	if (cache_load(sig)) {
		kb_strlcpy(g_book_sig, sig, sizeof(g_book_sig));
		g_books_read = 1;
		return;
	}
	if (g_async) {
		pid_t pid;

		/* One child per signature; one that died without writing is
		 * tried again after two minutes. */
		if (g_build_at && !strcmp(g_build_sig, sig) &&
		    mono_ms() - g_build_at < 120000)
			return;
		kb_strlcpy(g_build_sig, sig, sizeof(g_build_sig));
		g_build_at = mono_ms();
		/* Forked twice, so the builder belongs to init and the runner
		 * has no child to reap. */
		if ((pid = fork()) == 0) {
			if (fork() == 0) {
				if (nice(10) < 0)
					errno = 0;
				for (int i = 0; i < NBOOKS; i++) {
					rn_sys(books[i].dir, dir, sizeof(dir));
					walk(dir, i, 0);
				}
				cache_save(sig);
			}
			_exit(0);
		}
		while (pid > 0 && waitpid(pid, NULL, 0) < 0 && errno == EINTR)
			;
		return;
	}
	for (int i = 0; i < NBOOKS; i++) {
		rn_sys(books[i].dir, dir, sizeof(dir));
		walk(dir, i, 0);
	}
	cache_save(sig);
	kb_strlcpy(g_book_sig, sig, sizeof(g_book_sig));
	g_books_read = 1;
}

static void match_books(const RnQuery *q, int only, RnList *out)
{
	struct {
		int i, s;
	} best[LIB_PER_SOURCE];
	int nb = 0;

	books_load();
	for (int i = 0; i < g_nheads; i++) {
		int s = rn_words_match(g_heads[i].text, q);

		if (!s)
			continue;
		/* A page's own title outranks a heading inside another page
		 * that happens to share the words. */
		s += (4 - g_heads[i].level) * 5;
		if (nb < LIB_PER_SOURCE) {
			best[nb].i = i;
			best[nb].s = s;
			nb++;
			continue;
		}
		{
			int lo = 0;

			for (int k = 1; k < nb; k++)
				if (best[k].s < best[lo].s)
					lo = k;
			if (s > best[lo].s) {
				best[lo].i = i;
				best[lo].s = s;
			}
		}
	}
	for (int k = 0; k < nb; k++) {
		const Heading *h = &g_heads[best[k].i];
		RnMatch *m = rn_add(out);
		char url[1200];

		if (!m)
			return;
		kb_strlcpy(m->text, h->text, sizeof(m->text));
		kb_strlcpy(m->subtext, books[h->book].name, sizeof(m->subtext));
		kb_strlcpy(m->icon, "help-contents", sizeof(m->icon));
		kb_strlcpy(m->category, "Offline library", sizeof(m->category));
		m->catrel = catrel(only);
		m->rel = 0.3 + (best[k].s > 100 ? 100 : best[k].s) / 250.0;
		snprintf(url, sizeof(url), "file://%s%s%s", h->file,
			 *h->id ? "#" : "", h->id);
		kb_strlcpy(m->url, url, sizeof(m->url));
		rn_arg(m, "khelpcenter");
		rn_arg(m, url);
	}
}

/* ── manual pages ──────────────────────────────────────────────────────── */

static void match_man(const RnQuery *q, int only, RnList *out)
{
	static const char *const secs[] = { "1", "5", "7", "8" };
	int shown = 0;

	for (size_t si = 0; si < sizeof(secs) / sizeof(secs[0]); si++) {
		char dir[1024], **names;

		{
			char rel[32];

			snprintf(rel, sizeof(rel), "/usr/share/man/man%s", secs[si]);
			rn_sys(rel, dir, sizeof(dir));
		}
		names = kb_listdir(dir, NULL);
		for (char **n = names; n && *n && shown < LIB_PER_SOURCE; n++) {
			char name[128], url[256], *dot;
			RnMatch *m;
			int s;

			if (strncmp(*n, "kdos", 4) || strlen(*n) >= sizeof(name))
				continue;
			kb_strlcpy(name, *n, sizeof(name));
			dot = strchr(name, '.');
			if (!dot || strncmp(dot + 1, secs[si], 1))
				continue;
			*dot = '\0';
			if (!(s = rn_words_match(name, q)))
				continue;
			if (!(m = rn_add(out)))
				break;
			snprintf(m->text, sizeof(m->text), "%s(%s)", name, secs[si]);
			kb_strlcpy(m->subtext, "Manual page", sizeof(m->subtext));
			kb_strlcpy(m->icon, "help-contents", sizeof(m->icon));
			kb_strlcpy(m->category, "Offline library", sizeof(m->category));
			m->catrel = catrel(only);
			m->rel = 0.3 + (s > 100 ? 100 : s) / 250.0;
			snprintf(url, sizeof(url), "man:/%s(%s)", name, secs[si]);
			rn_arg(m, "khelpcenter");
			rn_arg(m, url);
			shown++;
		}
		kb_strv_free(names);
	}
}

/* ── places ────────────────────────────────────────────────────────────── */

/* `geo:48.8584,2.2945`, `48.8584, 2.2945` or `48.8584 2.2945`. */
static int parse_coords(const char *s, double *lat, double *lon)
{
	char *end;

	if (!strncasecmp(s, "geo:", 4))
		s += 4;
	*lat = strtod(s, &end);
	if (end == s)
		return 0;
	s = end;
	while (*s == ' ')
		s++;
	if (*s == ',')
		s++;
	while (*s == ' ')
		s++;
	*lon = strtod(s, &end);
	if (end == s)
		return 0;
	while (*end == ' ')
		end++;
	return !*end && *lat >= -90 && *lat <= 90 && *lon >= -180 &&
	       *lon <= 180 && strchr(s, '.') != NULL;
}

static void match_places(const RnQuery *q, int only, RnList *out)
{
	double lat, lon;
	RnMatch *m;
	char url[96];

	if (!parse_coords(q->raw, &lat, &lon) || !(m = rn_add(out)))
		return;
	snprintf(url, sizeof(url), "geo:%.6f,%.6f", lat, lon);
	snprintf(m->text, sizeof(m->text), "Show %.5f, %.5f on the map", lat, lon);
	kb_strlcpy(m->subtext, url, sizeof(m->subtext));
	kb_strlcpy(m->icon, "map-globe", sizeof(m->icon));
	kb_strlcpy(m->category, "Offline library", sizeof(m->category));
	kb_strlcpy(m->url, url, sizeof(m->url));
	m->catrel = only ? RN_CAT_HIGHEST : RN_CAT_HIGH;
	m->rel = 1.0;
	rn_arg(m, "xdg-open");
	rn_arg(m, url);
}

void rn_library_match(const RnQuery *q, int only, RnList *out)
{
	if (!q->nw)
		return;
	match_places(q, only, out);
	match_books(q, only, out);
	match_man(q, only, out);
	rn_zeal_match(q, only, out);
}

/* ── the servers ───────────────────────────────────────────────────────── */

typedef struct {
	char name[160];		/* the name kiwix-serve routes       */
	char title[160];
} Zim;

static Zim *g_zims;
static int g_nzims;
static char g_zim_sig[256];

static int port_of(const char *var, int def)
{
	const char *v = getenv(var);
	int p = v ? atoi(v) : 0;

	return p > 0 && p < 65536 ? p : def;
}

/* An attribute of the tag at `tag` (which is NUL-cut at its `>`). */
static void attr(const char *tag, const char *key, char *out, size_t n)
{
	char want[32];
	const char *p, *e;

	out[0] = '\0';
	snprintf(want, sizeof(want), " %s=\"", key);
	if (!(p = strstr(tag, want)))
		return;
	p += strlen(want);
	if (!(e = strchr(p, '"')) || (size_t)(e - p) >= n)
		return;
	memcpy(out, p, (size_t)(e - p));
	out[e - p] = '\0';
	rn_html_text(out);
}

/*
 * The books kiwix-serve serves, from the library file 86_kiwix-serve gives
 * it. The name a book is routed by is its file name without `.zim`, with
 * `_` for a space and `plus` for `+` (libkiwix's
 * Book::getHumanReadableIdFromPath).
 */
static void zims_load(void)
{
	char path[1024], sig[256], *xml;

	rn_sys("/var/lib/kiwix/library.xml", path, sizeof(path));
	stat_sig(path, sig, sizeof(sig));
	if (!strcmp(sig, g_zim_sig))
		return;
	kb_strlcpy(g_zim_sig, sig, sizeof(g_zim_sig));
	free(g_zims);
	g_zims = NULL;
	g_nzims = 0;
	if (!(xml = kb_read_whole(path, NULL)))
		return;
	for (char *p = xml; (p = strstr(p, "<book ")) && g_nzims < 16;) {
		char *gt = strchr(p, '>'), file[512], *base, *dot;
		Zim z = { 0 };
		size_t o = 0;

		if (!gt)
			break;
		*gt = '\0';
		attr(p, "path", file, sizeof(file));
		attr(p, "title", z.title, sizeof(z.title));
		p = gt + 1;
		base = strrchr(file, '/');
		base = base ? base + 1 : file;
		if ((dot = strstr(base, ".zim")))
			*dot = '\0';
		for (const char *c = base; *c && o + 5 < sizeof(z.name); c++) {
			if (*c == ' ') {
				z.name[o++] = '_';
			} else if (*c == '+') {
				memcpy(z.name + o, "plus", 4);
				o += 4;
			} else {
				z.name[o++] = *c;
			}
		}
		z.name[o] = '\0';
		if (!z.name[0])
			continue;
		if (!z.title[0])
			kb_strlcpy(z.title, z.name, sizeof(z.title));
		g_zims = kb_realloc(g_zims, (size_t)(g_nzims + 1) * sizeof(*g_zims));
		g_zims[g_nzims++] = z;
	}
	free(xml);
}

static RnAsk *ask_new(int kind, int port)
{
	RnAsk *a = kb_calloc(1, sizeof(*a));

	a->fd = -1;
	a->kind = kind;
	a->port = port;
	return a;
}

int rn_library_asks(const RnQuery *q, RnAsk **out)
{
	RnAsk *head = NULL, **tail = &head;
	int n = 0;

	*out = NULL;
	if (strlen(q->raw) < 3)
		return 0;
	zims_load();
	for (int i = 0; i < g_nzims && i < 4; i++) {
		RnAsk *a = ask_new(RN_ASK_KIWIX, port_of("KDOS_RUNNER_KIWIX_PORT", 8080));
		KbBuf b = { 0 };

		kb_strlcpy(a->book, g_zims[i].name, sizeof(a->book));
		kb_strlcpy(a->title, g_zims[i].title, sizeof(a->title));
		kb_buf_str(&b, "GET /suggest?content=");
		rn_urlenc(&b, a->book, 0);
		kb_buf_str(&b, "&term=");
		rn_urlenc(&b, q->raw, 0);
		kb_buf_str(&b, "&count=4 HTTP/1.0\r\nHost: 127.0.0.1\r\n"
			       "Accept: application/json\r\n\r\n");
		if (b.n < sizeof(a->req))
			kb_strlcpy(a->req, b.p, sizeof(a->req));
		kb_buf_free(&b);
		if (!a->req[0]) {
			rn_http_free(a);
			continue;
		}
		*tail = a;
		tail = &a->next;
		n++;
	}
	{
		RnAsk *a = ask_new(RN_ASK_KOLIBRI,
				   port_of("KDOS_RUNNER_KOLIBRI_PORT", 8081));
		KbBuf b = { 0 };

		kb_buf_str(&b, "GET /api/content/contentnode_search/?search=");
		rn_urlenc(&b, q->raw, 0);
		kb_buf_str(&b, "&max_results=4 HTTP/1.0\r\nHost: 127.0.0.1\r\n"
			       "Accept: application/json\r\n\r\n");
		if (b.n < sizeof(a->req))
			kb_strlcpy(a->req, b.p, sizeof(a->req));
		kb_buf_free(&b);
		if (a->req[0]) {
			*tail = a;
			n++;
		} else {
			rn_http_free(a);
		}
	}
	*out = head;
	return n;
}

static void kiwix_rows(RnAsk *a, int only, RnList *out)
{
	const char *body = rn_http_body(a);
	int shown = 0;

	for (const char *e = json_first(body); e && shown < 4; e = json_next(e)) {
		char kind[16], value[256], path[512], url[1200];
		KbBuf u = { 0 };
		RnMatch *m;

		/* The last suggestion is "containing '…'", a full-text search
		 * with no path; only titles are rows. */
		if (!json_str(json_get(e, "kind"), kind, sizeof(kind)) ||
		    strcmp(kind, "path") ||
		    !json_str(json_get(e, "value"), value, sizeof(value)) ||
		    !json_str(json_get(e, "path"), path, sizeof(path)))
			continue;
		rn_html_text(value);
		rn_html_text(path);
		if (!*value || !*path || !(m = rn_add(out)))
			continue;
		kb_buf_printf(&u, "http://127.0.0.1:%d/content/", a->port);
		rn_urlenc(&u, a->book, 0);
		kb_buf_str(&u, "/");
		rn_urlenc(&u, path, 1);
		kb_strlcpy(url, u.p, sizeof(url));
		kb_buf_free(&u);
		kb_strlcpy(m->text, value, sizeof(m->text));
		kb_strlcpy(m->subtext, a->title, sizeof(m->subtext));
		kb_strlcpy(m->icon, "kiwix-desktop", sizeof(m->icon));
		kb_strlcpy(m->category, "Offline library", sizeof(m->category));
		kb_strlcpy(m->url, url, sizeof(m->url));
		m->catrel = catrel(only);
		m->rel = 0.6 - shown * 0.05;
		rn_arg(m, "xdg-open");
		rn_arg(m, url);
		shown++;
	}
}

static void kolibri_rows(RnAsk *a, int only, RnList *out)
{
	const char *body = rn_http_body(a);
	int shown = 0;

	for (const char *e = json_first(json_get(body, "results")); e && shown < 4;
	     e = json_next(e)) {
		char id[64], title[256], kind[32], url[256];
		RnMatch *m;
		int ok = 1;

		if (!json_str(json_get(e, "id"), id, sizeof(id)) ||
		    !json_str(json_get(e, "title"), title, sizeof(title)))
			continue;
		json_str(json_get(e, "kind"), kind, sizeof(kind));
		/* The id goes into a URL fragment: Kolibri's ids are hex, and
		 * anything else is not one of them. */
		for (const char *c = id; *c; c++)
			if (!isxdigit((unsigned char)*c))
				ok = 0;
		if (!ok || !*id || !*title || !(m = rn_add(out)))
			continue;
		snprintf(url, sizeof(url),
			 "http://127.0.0.1:%d/en/learn/#/topics/%s/%s", a->port,
			 strcmp(kind, "topic") ? "c" : "t", id);
		kb_strlcpy(m->text, title, sizeof(m->text));
		snprintf(m->subtext, sizeof(m->subtext), "Kolibri%s%s",
			 *kind ? " · " : "", kind);
		kb_strlcpy(m->icon, "applications-education", sizeof(m->icon));
		kb_strlcpy(m->category, "Offline library", sizeof(m->category));
		kb_strlcpy(m->url, url, sizeof(m->url));
		m->catrel = catrel(only);
		m->rel = 0.55 - shown * 0.05;
		rn_arg(m, "xdg-open");
		rn_arg(m, url);
		shown++;
	}
}

void rn_library_answer(RnAsk *a, const RnQuery *q, int only, RnList *out)
{
	(void)q;
	if (a->kind == RN_ASK_KIWIX)
		kiwix_rows(a, only, out);
	else
		kolibri_rows(a, only, out);
}

void rn_library_reset(void)
{
	g_books_read = 0;
	g_build_at = 0;
	rn_zeal_reset();
	g_zim_sig[0] = '\0';
}

/* ── all of it, waiting ────────────────────────────────────────────────── */

static long long mono_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void rn_library_collect(const char *text, int deadline, RnList *out)
{
	int only;
	const char *t = rn_library_query(text, &only);
	long long end = mono_ms() + deadline;
	RnAsk *asks = NULL;
	RnQuery q;

	if (!t)
		return;
	rn_query(&q, t);
	rn_library_match(&q, only, out);
	rn_library_asks(&q, &asks);
	for (RnAsk *a = asks; a; a = a->next)
		rn_http_start(a);
	for (;;) {
		struct pollfd pfd[64];
		RnAsk *who[64];
		int n = 0;
		long long now = mono_ms();

		for (RnAsk *a = asks; a && n < 64; a = a->next) {
			short ev = rn_http_events(a);

			if (ev) {
				pfd[n].fd = a->fd;
				pfd[n].events = ev;
				who[n++] = a;
			}
		}
		if (!n || now >= end)
			break;
		if (poll(pfd, (nfds_t)n, (int)(end - now)) < 0 && errno != EINTR)
			break;
		for (int i = 0; i < n; i++)
			if (pfd[i].revents)
				rn_http_step(who[i], pfd[i].revents);
	}
	for (RnAsk *a = asks; a; a = a->next)
		if (a->done == 1)
			rn_library_answer(a, &q, only, out);
	rn_http_free(asks);
	rn_list_sort(out);
}
