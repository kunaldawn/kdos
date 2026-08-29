/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-doc — the hypertext viewer
 *
 *   ╔═ kdos-doc · index ═══════════════════════════════════════╗
 *   ║ Everything on this machine that can explain itself.      ║
 *   ║                                                          ║
 *   ║   zlib          a port, through kpkg meta                ║
 *   ║   tmp-mode-1777 a recorded debug cycle                   ║
 *   ║   kpkg.1        a manual page, in a terminal             ║
 *   ╟──────────────────────────────────────────────────────────╢
 *   ║ Tab link  Enter follow  Bksp back  / search  Esc close   ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * TempleOS had DolDoc: every file on the machine was hypertext, and a link was
 * a thing the system already knew how to resolve. KDOS has the corpus and not
 * the surface — `kdos why`, `kdos explain`, `kpkg meta` and mandoc each answer
 * one question at a prompt, and none of them can be FOLLOWED from another.
 *
 * A document is plain text with `[[kind:target]]` spans in it, and every kind
 * resolves through a program that already exists:
 *
 *   [[port:zlib]]              kpkg meta — the recipe's own metadata
 *   [[reason:tmp-mode-1777]]   the reasons store `kdos why` reads
 *   [[man:kpkg.1]]             mandoc, in a terminal
 *   [[key:W-d]]                the keybind card
 *   [[doc:index]]              another document in this corpus
 *
 * NOTHING IS AUTHORED HERE. The viewer ships with no corpus at all, and an
 * empty /usr/share/kdos/doc is a working state rather than a broken one: the
 * built-in index page is itself a document, parsed by the same parser, so its
 * links are live and the resolvers can be tried on a machine whose corpus is
 * still empty.
 *
 * A LINK THAT RESOLVES TO NOTHING SAYS SO. `no such port: zlub` on the note
 * row, and the page does not change — a viewer that silently ignored a dead
 * link would make a stale corpus indistinguishable from a working one, which
 * is the failure `kdos why` was built to avoid.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define DOC_MAX_LINES 512
#define DOC_LINE 320
#define DOC_MAX_LINKS 256
#define DOC_MAX_MARKS 128
#define DOC_HIST 32

enum { PG_INDEX = 0, PG_DOC, PG_PORT, PG_REASON };

struct doc_page {
	int kind;
	char name[192];
};

struct doc_link {
	int row, col, w;
	char kind[16];
	char target[192];
};

static char lines[DOC_MAX_LINES][DOC_LINE];
static int nlines;
static struct doc_link links[DOC_MAX_LINKS];
static int nlinks;

static struct doc_page page;
static char *source;		/* the page's raw text, re-wrapped on resize */
static int laid_out_w;

static int topline, sel_link;
static char note[192];

static struct {
	struct doc_page pg;
	int topline, sel_link;
} hist[DOC_HIST];
static int nhist;

/* `/` search: marks, not a filter — a document is read for the context AROUND
 * the hit, which is the same reason kdosbuild's log search marks. */
static int searching;
static char query[64];
static int marks[DOC_MAX_MARKS];
static int nmarks, cur_mark;

/* ── where documents live ──────────────────────────────────────────────── */

static int doc_dirs(char out[][512], int max)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int n = 0;

	if (home && *home)
		snprintf(out[n++], 512, "%.480s/kdos/doc", home);
	else
		snprintf(out[n++], 512, "%.450s/.local/share/kdos/doc",
			 kb_home_dir());

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *p = dirs; *p && n < max;) {
		const char *sep = strchr(p, ':');
		size_t len = sep ? (size_t)(sep - p) : strlen(p);
		while (len > 1 && p[len - 1] == '/')
			len--;
		if (len && len < 460) {
			char base[512];
			memcpy(base, p, len);
			base[len] = '\0';
			snprintf(out[n++], 512, "%.460s/kdos/doc", base);
		}
		if (!sep)
			break;
		p = sep + 1;
	}
	return n;
}

/* $KDOS_REASONS beats the installed copy, exactly as `kdos why` reads it, so a
 * checkout can be browsed before anything is installed. */
static const char *reason_dir(void)
{
	const char *e = getenv("KDOS_REASONS");
	return (e && *e) ? e : "/usr/share/kdos/reasons";
}

/* A name that could name a file and nothing else: no slash, no `..`. Link
 * targets arrive from a text file, and a document that could name
 * `../../etc/shadow` would be a document that reads it. */
static int safe_name(const char *s)
{
	if (!s || !*s || strlen(s) > 160)
		return 0;
	for (const char *p = s; *p; p++)
		if (*p == '/' || *p == '\\' || !isprint((unsigned char)*p))
			return 0;
	return strcmp(s, ".") && strcmp(s, "..");
}

/* ── layout: source text -> cells ──────────────────────────────────────── */

static void layout_reset(void)
{
	nlines = 0;
	nlinks = 0;
}

static void emit_line(const char *text, size_t len)
{
	if (nlines >= DOC_MAX_LINES)
		return;
	if (len >= DOC_LINE)
		len = DOC_LINE - 1;
	memcpy(lines[nlines], text, len);
	lines[nlines][len] = '\0';
	nlines++;
}

/*
 * One source line: the `[[kind:target]]` spans are replaced by their target
 * text and recorded as links, then the result is wrapped to `w` at spaces,
 * keeping the line's own indent on the continuation rows.
 *
 * Substitution and wrapping happen in the same pass over ONE expanded buffer
 * because a link's row and column are only known after both — which is why the
 * spans are carried as byte offsets into that buffer rather than being
 * registered as they are parsed.
 */
struct span {
	size_t at, len;
	char kind[16];
	char target[192];
};

static void layout_line(const char *src, size_t slen, int w)
{
	char out[DOC_LINE];
	struct span sp[16];
	size_t n = 0;
	int nsp = 0;
	int indent = 0;

	while (indent < (int)slen && src[indent] == ' ' && indent < 8)
		indent++;

	for (size_t i = 0; i < slen && n < sizeof(out) - 1;) {
		if (src[i] == '[' && i + 1 < slen && src[i + 1] == '[') {
			const char *close = NULL;
			for (size_t j = i + 2; j + 1 < slen; j++)
				if (src[j] == ']' && src[j + 1] == ']') {
					close = src + j;
					break;
				}
			if (close) {
				size_t inner = (size_t)(close - (src + i + 2));
				char body[256];
				if (inner >= sizeof(body))
					inner = sizeof(body) - 1;
				memcpy(body, src + i + 2, inner);
				body[inner] = '\0';
				char *colon = strchr(body, ':');
				const char *kind = "doc", *target = body;
				if (colon) {
					*colon = '\0';
					kind = body;
					target = colon + 1;
				}
				size_t tl = strlen(target);
				if (tl && n + tl < sizeof(out) - 1 && nsp < 16) {
					sp[nsp].at = n;
					sp[nsp].len = tl;
					kb_strlcpy(sp[nsp].kind, kind,
						   sizeof(sp[0].kind));
					kb_strlcpy(sp[nsp].target, target,
						   sizeof(sp[0].target));
					nsp++;
					memcpy(out + n, target, tl);
					n += tl;
				}
				i = (size_t)(close - src) + 2;
				continue;
			}
		}
		out[n++] = src[i++];
	}
	out[n] = '\0';

	if (w < 8)
		w = 8;

	size_t start = 0;
	int first = 1;
	do {
		int pad = first ? 0 : indent;
		size_t avail = (size_t)(w - pad);
		size_t end = start + avail;
		if (end >= n) {
			end = n;
		} else {
			/* Break at the last space that is not inside a link:
			 * splitting a link would leave half of it clickable and
			 * half of it not. */
			size_t br = 0;
			for (size_t k = end; k > start; k--) {
				if (out[k - 1] != ' ')
					continue;
				int inside = 0;
				for (int s = 0; s < nsp; s++)
					if (k - 1 >= sp[s].at &&
					    k - 1 < sp[s].at + sp[s].len)
						inside = 1;
				if (!inside) {
					br = k - 1;
					break;
				}
			}
			if (br > start)
				end = br;
		}

		char row[DOC_LINE];
		int rn = 0;
		for (int k = 0; k < pad && rn < DOC_LINE - 1; k++)
			row[rn++] = ' ';
		size_t take = end - start;
		if (take > (size_t)(DOC_LINE - 1 - rn))
			take = (size_t)(DOC_LINE - 1 - rn);
		memcpy(row + rn, out + start, take);
		row[rn + take] = '\0';

		int rowidx = nlines;
		emit_line(row, (size_t)rn + take);
		for (int s = 0; s < nsp; s++) {
			/* A span that does not fit ENTIRELY on this row is not
			 * registered at all: it is drawn as plain text and is
			 * not followable. Half a clickable link is the worse
			 * answer — the reader would have to guess which half. */
			if (sp[s].at < start || sp[s].at + sp[s].len > end)
				continue;
			if (nlinks >= DOC_MAX_LINKS)
				break;
			struct doc_link *L = &links[nlinks++];
			L->row = rowidx;
			/* The column is a DISPLAY width, not a byte offset: a
			 * document may carry UTF-8 and a link after it would
			 * otherwise be highlighted several cells to the right of
			 * the word it belongs to. */
			char lead[DOC_LINE];
			size_t ll = sp[s].at - start;
			if (ll > sizeof(lead) - 1)
				ll = sizeof(lead) - 1;
			memcpy(lead, out + start, ll);
			lead[ll] = '\0';
			char txt[256];
			size_t tl = sp[s].len;
			if (tl > sizeof(txt) - 1)
				tl = sizeof(txt) - 1;
			memcpy(txt, out + sp[s].at, tl);
			txt[tl] = '\0';
			L->col = pad + ktui_utf8_width(lead);
			L->w = ktui_utf8_width(txt);
			kb_strlcpy(L->kind, sp[s].kind, sizeof(L->kind));
			kb_strlcpy(L->target, sp[s].target, sizeof(L->target));
		}

		start = end;
		while (start < n && out[start] == ' ')
			start++;
		first = 0;
	} while (start < n && nlines < DOC_MAX_LINES);
}

static void layout(int w)
{
	layout_reset();
	laid_out_w = w;
	if (!source)
		return;
	for (const char *l = source; *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		while (len && (l[len - 1] == '\r'))
			len--;
		if (!len)
			emit_line("", 0);
		else
			layout_line(l, len, w);
		if (!nl)
			break;
		l = nl + 1;
	}
	/* The last row SAYS it stopped. emit_line has nowhere left to put a
	 * notice by then, so the notice replaces the row it could not add —
	 * a document that simply ends where the buffer did is one nobody knows
	 * is incomplete. */
	if (nlines >= DOC_MAX_LINES)
		snprintf(lines[DOC_MAX_LINES - 1], DOC_LINE,
			 "%s (document truncated)", ktui_glyph[KT_G_ELLIPSIS]);
}

/* ── the pages ─────────────────────────────────────────────────────────── */

static void source_set(KbBuf *b)
{
	free(source);
	source = b->p ? b->p : kb_strdup("");
	b->p = NULL;
	b->n = b->cap = 0;
	laid_out_w = 0;		/* force a re-layout at the next draw */
}

/*
 * The built-in index, which is a DOCUMENT: it goes through the same parser, so
 * every link on it is live and a machine with no corpus can still be used to
 * try each resolver. It also lists whatever corpus is installed.
 */
static void load_index(void)
{
	char dirs[16][512];
	int nd = doc_dirs(dirs, 16);
	KbBuf b = {0};
	int found = 0;

	kb_buf_str(&b, "KDOS DOC\n\n");
	kb_buf_str(&b, "Everything on this machine that can explain itself, in "
		       "one place. A link\nis followed with Enter or a click; "
		       "Backspace goes back.\n\n");

	for (int i = 0; i < nd; i++) {
		DIR *d = opendir(dirs[i]);
		struct dirent *e;
		if (!d)
			continue;
		while ((e = readdir(d))) {
			if (e->d_name[0] == '.')
				continue;
			char stem[192];
			kb_strlcpy(stem, e->d_name, sizeof(stem));
			char *dot = strrchr(stem, '.');
			if (dot && !strcmp(dot, ".txt"))
				*dot = '\0';
			if (!safe_name(stem))
				continue;
			if (!found) {
				kb_buf_str(&b, "Documents\n\n");
				found = 1;
			}
			kb_buf_printf(&b, "  [[doc:%s]]\n", stem);
		}
		closedir(d);
	}
	if (!found)
		kb_buf_str(&b, "No documents are installed, which is a working "
			       "state: every\nresolver below needs no corpus at "
			       "all. Try one.\n");

	kb_buf_str(&b, "\nWhat a link resolves to\n\n");
	/* Each example is a LIVE link: this page goes through the same parser a
	 * document does, so the resolvers can be tried on a machine whose
	 * corpus is still empty. */
	kb_buf_str(&b, "  port    a recipe's own metadata     [[port:zlib]]\n");
	kb_buf_str(&b, "  reason  a recorded debug cycle      "
		       "[[reason:tmp-mode-1777]]\n");
	kb_buf_str(&b, "  man     a manual page, in foot      [[man:kpkg.1]]\n");
	kb_buf_str(&b, "  key     the keybind card            [[key:W-d]]\n");
	kb_buf_str(&b, "  doc     another document            [[doc:index]]\n");
	/* The link syntax cannot be written out here — the parser would eat it,
	 * which is the one joke a hypertext viewer is not allowed to make. */
	kb_buf_str(&b, "\nA document is plain text with kind:target spans in "
		       "double square\nbrackets. Drop one in\n");
	kb_buf_printf(&b, "  %s\nor in /usr/share/kdos/doc and it is listed "
			  "above.\n", dirs[0]);
	source_set(&b);
}

static int load_doc(const char *name)
{
	char dirs[16][512], path[700];
	int nd = doc_dirs(dirs, 16);

	if (!safe_name(name))
		return -1;
	for (int i = 0; i < nd; i++) {
		for (int ext = 0; ext < 2; ext++) {
			char *text;
			snprintf(path, sizeof(path), "%.500s/%.160s%s", dirs[i],
				 name, ext ? ".txt" : "");
			text = kb_read_all(path, NULL);
			if (!text)
				continue;
			KbBuf b = {0};
			kb_buf_str(&b, text);
			free(text);
			source_set(&b);
			return 0;
		}
	}
	return -1;
}

/*
 * A port page is `kpkg meta`'s own output — the recipe's fields as shell
 * assignments — rather than a second parser for kpkgbuild. One parser for that
 * file exists and it is kpkg's; a copy here would be the thing that disagrees
 * with the build.
 */
static int load_port(const char *name)
{
	char out[8192];
	KbArgv a = {0};

	if (!safe_name(name) || !kb_have_prog("kpkg"))
		return -1;
	kb_argv_add(&a, "kpkg");
	kb_argv_add(&a, "meta");
	kb_argv_add(&a, name);
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, sizeof(out)) != 0 || !out[0])
		return -1;

	KbBuf b = {0};
	kb_buf_printf(&b, "PORT %s\n\n", name);
	kb_buf_str(&b, out);
	if (out[strlen(out) - 1] != '\n')
		kb_buf_str(&b, "\n");
	kb_buf_printf(&b, "\n  [[reason:%s]]   a recorded reason for this port, "
			  "if there is one\n", name);
	source_set(&b);
	return 0;
}

/*
 * A reason, from the same store `kdos why` reads: the `title:` header becomes
 * the heading, every `see:` becomes a link to the reason it names, and the
 * body is the text. The file format is a header block, a blank line and prose.
 */
static int load_reason(const char *name)
{
	char path[700];
	char *text;

	if (!safe_name(name))
		return -1;
	snprintf(path, sizeof(path), "%.500s/%.160s.txt", reason_dir(), name);
	text = kb_read_all(path, NULL);
	if (!text) {
		snprintf(path, sizeof(path), "%.500s/%.160s", reason_dir(), name);
		text = kb_read_all(path, NULL);
	}
	if (!text)
		return -1;

	KbBuf b = {0};
	const char *body = text;
	int in_header = 1;

	for (const char *l = text; *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		if (!len) {
			body = nl ? nl + 1 : l + len;
			in_header = 0;
			break;
		}
		if (in_header) {
			if (!strncmp(l, "title:", 6)) {
				const char *v = l + 6;
				while (*v == ' ')
					v++;
				kb_buf_printf(&b, "%.*s\n\n",
					      (int)(len - (size_t)(v - l)), v);
			} else if (!strncmp(l, "see:", 4)) {
				const char *v = l + 4;
				while (*v == ' ')
					v++;
				kb_buf_printf(&b, "see  [[reason:%.*s]]\n",
					      (int)(len - (size_t)(v - l)), v);
			} else if (!strncmp(l, "port:", 5)) {
				const char *v = l + 5;
				while (*v == ' ')
					v++;
				kb_buf_printf(&b, "port [[port:%.*s]]\n",
					      (int)(len - (size_t)(v - l)), v);
			} else {
				kb_buf_printf(&b, "%.*s\n", (int)len, l);
			}
		}
		if (!nl)
			break;
		l = nl + 1;
	}
	kb_buf_str(&b, "\n");
	kb_buf_str(&b, body);
	free(text);
	source_set(&b);
	return 0;
}

/* Load whatever `page` names. Returns 0, or -1 leaving the previous page. */
static int page_load(void)
{
	switch (page.kind) {
	case PG_DOC:
		return load_doc(page.name);
	case PG_PORT:
		return load_port(page.name);
	case PG_REASON:
		return load_reason(page.name);
	default:
		load_index();
		return 0;
	}
}

static const char *page_label(void)
{
	static char buf[224];
	static const char *const KINDS[] = { "index", "doc", "port", "reason" };

	if (page.kind == PG_INDEX)
		snprintf(buf, sizeof(buf), " kdos-doc %s index ",
			 ktui_glyph[KT_G_BULLET]);
	else
		snprintf(buf, sizeof(buf), " kdos-doc %s %s:%s ",
			 ktui_glyph[KT_G_BULLET], KINDS[page.kind], page.name);
	return buf;
}

/* ── following a link ──────────────────────────────────────────────────── */

/* Double-forked: the viewer stays open and never reaps what it opened. */
static void spawn_detached(const char **argv)
{
	pid_t p = fork();

	if (p == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	}
	if (p > 0) {
		int st;
		waitpid(p, &st, 0);
	}
}

static void hist_push(void)
{
	if (nhist >= DOC_HIST) {
		/* Drop the oldest: a viewer that stopped remembering after 32
		 * hops would be one that stops going back at all. */
		memmove(&hist[0], &hist[1], sizeof(hist[0]) * (DOC_HIST - 1));
		nhist--;
	}
	hist[nhist].pg = page;
	hist[nhist].topline = topline;
	hist[nhist].sel_link = sel_link;
	nhist++;
}

static void go_back(void)
{
	if (!nhist) {
		snprintf(note, sizeof(note), "nothing to go back to");
		return;
	}
	nhist--;
	page = hist[nhist].pg;
	if (page_load() != 0)
		load_index();
	topline = hist[nhist].topline;
	sel_link = hist[nhist].sel_link;
	note[0] = '\0';
}

static void follow(const struct doc_link *L)
{
	struct doc_page next = { 0, "" };
	const char *argv[8];

	note[0] = '\0';

	if (!strcmp(L->kind, "man")) {
		if (!kb_have_prog("man")) {
			snprintf(note, sizeof(note), "no such man: %.120s (mandoc "
				 "is not installed)", L->target);
			return;
		}
		/* The one link kind that leaves the grid: a manual page is
		 * somebody else's formatter and belongs in a terminal. */
		argv[0] = "foot";
		argv[1] = "-e";
		argv[2] = "man";
		argv[3] = L->target;
		argv[4] = NULL;
		spawn_detached(argv);
		snprintf(note, sizeof(note), "man %.120s opened in a terminal",
			 L->target);
		return;
	}
	if (!strcmp(L->kind, "key")) {
		if (!kb_have_prog("kdos-keys")) {
			snprintf(note, sizeof(note),
				 "no such key: %.120s (kdos-keys is not installed)",
				 L->target);
			return;
		}
		/* The card is one screen and lists every bind, which is why the
		 * key is not passed: kdos-keys takes no filter, and a document
		 * that named a flag the card does not have would be a link that
		 * opened a usage message. */
		argv[0] = "kdos-keys";
		argv[1] = NULL;
		spawn_detached(argv);
		snprintf(note, sizeof(note), "%.120s — the keybind card is open",
			 L->target);
		return;
	}

	if (!strcmp(L->kind, "doc"))
		next.kind = PG_DOC;
	else if (!strcmp(L->kind, "port"))
		next.kind = PG_PORT;
	else if (!strcmp(L->kind, "reason"))
		next.kind = PG_REASON;
	else {
		snprintf(note, sizeof(note), "no such link kind: %.14s", L->kind);
		return;
	}
	kb_strlcpy(next.name, L->target, sizeof(next.name));

	struct doc_page saved = page;
	page = next;
	if (page_load() != 0) {
		page = saved;
		snprintf(note, sizeof(note), "no such %.14s: %.120s", L->kind,
			 L->target);
		return;
	}
	page = saved;			/* hist_push records where we WERE */
	hist_push();
	page = next;
	topline = 0;
	sel_link = 0;
	nmarks = 0;
}

/* ── search ────────────────────────────────────────────────────────────── */

static const char *strcasestr_(const char *hay, const char *needle)
{
	size_t n = strlen(needle);

	if (!n)
		return NULL;
	for (const char *p = hay; *p; p++)
		if (!strncasecmp(p, needle, n))
			return p;
	return NULL;
}

static void marks_rebuild(void)
{
	nmarks = 0;
	cur_mark = 0;
	if (!query[0])
		return;
	for (int i = 0; i < nlines && nmarks < DOC_MAX_MARKS; i++)
		if (strcasestr_(lines[i], query))
			marks[nmarks++] = i;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int body_rows(void) { return ktui_h - 3; }

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int rows = body_rows();

	if (rows < 1)
		rows = 1;
	if (laid_out_w != w - 4) {
		layout(w - 4);
		marks_rebuild();
	}

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	sh_frame(w, h, page_label(), KT_ACCENT, KT_SURFACE, 1);

	for (int i = 0; i < rows; i++) {
		int idx = topline + i;
		if (idx >= nlines)
			break;
		ktui_draw_text(2, 1 + i, w - 4, lines[idx], KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		/* The search marks, painted OVER the text: a document is read
		 * for what is around the hit, so nothing is hidden and only the
		 * matching cells change colour. */
		if (query[0]) {
			const char *p = lines[idx];
			for (const char *m = strcasestr_(p, query); m;
			     m = strcasestr_(m + 1, query)) {
				char lead[DOC_LINE];
				size_t ll = (size_t)(m - p);
				if (ll > sizeof(lead) - 1)
					ll = sizeof(lead) - 1;
				memcpy(lead, p, ll);
				lead[ll] = '\0';
				int x = 2 + ktui_utf8_width(lead);
				char hit[64];
				/* The matched BYTES, not the query: the match
				 * is case-insensitive, so painting the query
				 * over it rewrites the document — searching
				 * `kdos` turned the index's own "KDOS DOC"
				 * heading into "kdos DOC". */
				size_t hl = strlen(query);
				if (hl > sizeof(hit) - 1)
					hl = sizeof(hit) - 1;
				memcpy(hit, m, hl);
				hit[hl] = '\0';
				ktui_draw_text(x, 1 + i, w - 2 - x, hit,
					       KT_SURFACE, KT_WARN, KT_A_NONE);
			}
		}
	}

	/*
	 * The links, over the text they belong to. The selected one is a FILL
	 * with the slots swapped, never KT_A_REVERSE over the label — the
	 * attribute inverts only the cells the glyphs cover, which on a target
	 * with a space in it lights one block per word.
	 */
	for (int i = 0; i < nlinks; i++) {
		const struct doc_link *L = &links[i];
		int y = L->row - topline + 1;
		if (y < 1 || y >= 1 + rows)
			continue;
		int on = i == sel_link;
		int x = 2 + L->col;
		int avail = w - 2 - x;
		if (avail < 1)
			continue;
		int lw = L->w < avail ? L->w : avail;
		if (on)
			ktui_draw_fill(krect(x, y, lw, 1), KT_ACCENT);
		ktui_draw_text(x, y, lw, L->target, on ? KT_SURFACE : KT_ACCENT,
			       on ? KT_ACCENT : KT_SURFACE,
			       on ? KT_A_NONE : KT_A_UNDERLINE);
	}

	if (searching) {
		char line[128];
		snprintf(line, sizeof(line), "/%s", query);
		ktui_draw_text(2, h - 2, w - 4, line, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (note[0]) {
		ktui_draw_text(2, h - 2, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	} else {
		char hint[160];
		if (query[0] && nmarks)
			snprintf(hint, sizeof(hint),
				 "%d matches   n/N walk   Tab link   Enter "
				 "follow   Bksp back   Esc close",
				 nmarks);
		else
			snprintf(hint, sizeof(hint),
				 "Tab link   Enter follow   Bksp back   "
				 "/ search   Esc close");
		ktui_draw_text(2, h - 2, w - 4, hint, KT_MID, KT_SURFACE,
			       KT_A_NONE);
	}
	ktui_draw_flush();
}

/* `--dump-cells` — one line per painted cell, colours included. The
 * backend is cells.c's; these two are what the size flag writes. */
static int cap_w = 80, cap_h = 24;

/* ── main ──────────────────────────────────────────────────────────────── */

/* Keep the selected link on screen — Tab is a navigation key, and a link
 * selected off the bottom of the page reads as Tab having done nothing. */
static void scroll_to_link(void)
{
	int rows = body_rows();

	if (sel_link < 0 || sel_link >= nlinks || rows < 1)
		return;
	int r = links[sel_link].row;
	if (r < topline)
		topline = r;
	if (r >= topline + rows)
		topline = r - rows + 1;
}

static void clamp_scroll(void)
{
	int rows = body_rows();

	if (rows < 1)
		rows = 1;
	if (topline > nlines - rows)
		topline = nlines - rows;
	if (topline < 0)
		topline = 0;
}

int doc_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *want = NULL;
	int dump = 0, cells = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			cells = 1;
		/* A golden frame is named for its size, so the harness has to
		 * be able to ask for one. */
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 23 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		} else if (argv[i][0] != '-' && !want) {
			want = argv[i];
		} else {
			fprintf(stderr,
				"usage: kdos-doc [<doc>|port:<name>|reason:<name>]\n"
				"                [--dump|--dump-cells] "
				"[--dump-size WxH] [--font NAME]\n");
			return 2;
		}
	}

	page.kind = PG_INDEX;
	if (want) {
		const char *colon = strchr(want, ':');
		if (colon) {
			size_t kl = (size_t)(colon - want);
			if (!strncmp(want, "port", kl) && kl == 4)
				page.kind = PG_PORT;
			else if (!strncmp(want, "reason", kl) && kl == 6)
				page.kind = PG_REASON;
			else
				page.kind = PG_DOC;
			kb_strlcpy(page.name, colon + 1, sizeof(page.name));
		} else {
			page.kind = PG_DOC;
			kb_strlcpy(page.name, want, sizeof(page.name));
		}
	}
	if (page_load() != 0) {
		/* An argument that names nothing lands on the index with the
		 * reason said out loud, rather than on an empty screen. */
		static const char *const KINDS[] = { "document", "document",
						     "port", "reason" };
		snprintf(note, sizeof(note), "no such %s: %.120s",
			 KINDS[page.kind], page.name);
		page.kind = PG_INDEX;
		page.name[0] = '\0';
		load_index();
	}

	if (dump || cells) {
		sh_theme_from_cache();
		if (cells) {
			ktui_backend_set(sh_cells_backend(cap_w, cap_h));
			ktui_draw_init();
			draw();
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = KWL_ROLE_TOPLEVEL,
		.cols = 80,
		.rows = 24,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Help",
		.app_id = "kdos-doc",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-doc: no compositor or no layer-shell\n");
		return 2;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		clamp_scroll();
		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int row = topline + ev.my - 1;
			int hit = -1;
			for (int i = 0; i < nlinks; i++)
				if (links[i].row == row &&
				    ev.mx >= 2 + links[i].col &&
				    ev.mx < 2 + links[i].col + links[i].w)
					hit = i;
			if (ev.press == KT_MP_DRAG) {
				if (hit >= 0)
					sel_link = hit;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP)
				topline -= 3;
			else if (ev.btn == KT_MB_WHEEL_DOWN)
				topline += 3;
			else if (ev.btn == KT_MB_RIGHT) {
				/* Right backs out one level, then closes — the
				 * contract every other surface here keeps. On
				 * the page it opened on, go_back() only writes
				 * a note, which left the viewer with no way out
				 * that was not the keyboard. */
				if (!nhist)
					goto done;
				go_back();
			} else if (ev.btn == KT_MB_LEFT && hit >= 0) {
				sel_link = hit;
				follow(&links[hit]);
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (searching) {
			size_t n = strlen(query);
			if (ev.key == KT_K_ESC) {
				searching = 0;
				query[0] = '\0';
				nmarks = 0;
			} else if (ev.key == KT_K_ENTER) {
				searching = 0;
				marks_rebuild();
				if (nmarks)
					topline = marks[0];
				else
					snprintf(note, sizeof(note),
						 "no match for '%s'", query);
			} else if (ev.key == KT_K_BACKSPACE) {
				if (n)
					query[n - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   n + 1 < sizeof(query)) {
				query[n] = (char)ev.key;
				query[n + 1] = '\0';
			}
			continue;
		}

		int rows = body_rows();
		if (rows < 1)
			rows = 1;

		switch (ev.key) {
		case KT_K_ESC:
		case 'q':
			goto done;
		case KT_K_UP:
			topline--;
			break;
		case KT_K_DOWN:
			topline++;
			break;
		case KT_K_PGUP:
			topline -= rows;
			break;
		case KT_K_PGDN:
			topline += rows;
			break;
		case KT_K_HOME:
			topline = 0;
			break;
		case KT_K_END:
			topline = nlines;
			break;
		case KT_K_TAB:
			if (nlinks) {
				sel_link = (sel_link + 1) % nlinks;
				scroll_to_link();
			}
			break;
		case KT_K_BTAB:
			if (nlinks) {
				sel_link = (sel_link + nlinks - 1) % nlinks;
				scroll_to_link();
			}
			break;
		case KT_K_ENTER:
			if (sel_link >= 0 && sel_link < nlinks)
				follow(&links[sel_link]);
			break;
		case KT_K_BACKSPACE:
			go_back();
			break;
		case '/':
			searching = 1;
			query[0] = '\0';
			note[0] = '\0';
			break;
		case 'n':
			if (nmarks) {
				cur_mark = (cur_mark + 1) % nmarks;
				topline = marks[cur_mark];
			}
			break;
		case 'N':
			if (nmarks) {
				cur_mark = (cur_mark + nmarks - 1) % nmarks;
				topline = marks[cur_mark];
			}
			break;
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	free(source);
	source = NULL;
	return 0;
}
