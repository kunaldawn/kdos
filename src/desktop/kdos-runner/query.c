/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   query.c — what was typed, and the rows it becomes
 *
 * WORDS START WORDS. Every source here is matched the same way: each typed
 * word must begin some word of the row, so `net set` finds `setup.network`
 * and `ff` finds nothing it should not. A subsequence matcher would let two
 * letters reach half of every list, and a palette that answers everything
 * answers nothing.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner.h"

RnMatch *rn_add(RnList *l)
{
	if (l->n >= RN_MAX)
		return NULL;
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 16;
		l->v = kb_realloc(l->v, (size_t)l->cap * sizeof(*l->v));
	}
	memset(&l->v[l->n], 0, sizeof(l->v[0]));
	kb_strlcpy(l->v[l->n].category, "KDOS", sizeof(l->v[0].category));
	return &l->v[l->n++];
}

void rn_arg(RnMatch *m, const char *s)
{
	if (m->argc < RN_ARGV - 1)
		m->argv[m->argc++] = kb_strdup(s);
}

void rn_argf(RnMatch *m, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	rn_arg(m, buf);
}

void rn_match_free(RnMatch *m)
{
	for (int i = 0; i < m->argc; i++)
		free(m->argv[i]);
	m->argc = 0;
}

void rn_list_free(RnList *l)
{
	for (int i = 0; i < l->n; i++)
		rn_match_free(&l->v[i]);
	free(l->v);
	memset(l, 0, sizeof(*l));
}

static int cmp_match(const void *pa, const void *pb)
{
	const RnMatch *a = pa, *b = pb;

	if (a->catrel != b->catrel)
		return b->catrel - a->catrel;
	if (a->rel != b->rel)
		return a->rel < b->rel ? 1 : -1;
	return strcmp(a->text, b->text);
}

void rn_list_sort(RnList *l)
{
	if (l->n > 1)
		qsort(l->v, (size_t)l->n, sizeof(*l->v), cmp_match);
}

void rn_list_print(const RnList *l, KbBuf *out)
{
	for (int i = 0; i < l->n; i++) {
		const RnMatch *m = &l->v[i];

		kb_buf_printf(out, "%s\n  sub %s\n  icon %s\n  cat %s %d %.2f%s%s\n",
			      m->text, m->subtext, m->icon, m->category,
			      m->catrel, m->rel,
			      m->how == RN_RUN_TERM ? " term" : "",
			      m->term_action ? " +term" : "");
		if (m->url[0])
			kb_buf_printf(out, "  url %s\n", m->url);
		for (int k = 0; k < m->argc; k++)
			kb_buf_printf(out, "  [%s]\n", m->argv[k]);
	}
}

/* ── the query ─────────────────────────────────────────────────────────── */

void rn_query(RnQuery *q, const char *text)
{
	size_t o = 0;
	int space = 1;

	memset(q, 0, sizeof(*q));
	for (const char *p = text ? text : ""; *p && o + 1 < sizeof(q->raw); p++) {
		unsigned char c = (unsigned char)*p;

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (!space)
				q->raw[o++] = ' ';
			space = 1;
			continue;
		}
		/* A control character is nothing anybody typed into a search
		 * field, and passing one on into an argument vector would put
		 * it in front of a program that did not expect it. */
		if (c < 0x20 || c == 0x7f)
			continue;
		q->raw[o++] = (char)c;
		space = 0;
	}
	while (o && q->raw[o - 1] == ' ')
		o--;
	q->raw[o] = '\0';
	for (size_t i = 0; i <= o; i++)
		q->low[i] = (char)tolower((unsigned char)q->raw[i]);

	memcpy(q->buf, q->low, o + 1);
	memcpy(q->rbuf, q->raw, o + 1);
	for (char *p = q->buf, *r = q->rbuf; *p && q->nw < RN_WORDS;) {
		char *e = strchr(p, ' ');

		q->w[q->nw] = p;
		q->rw[q->nw] = r;
		q->nw++;
		if (!e)
			break;
		*e = '\0';
		r[e - p] = '\0';
		r += e - p + 1;
		p = e + 1;
	}
}

const char *rn_query_rest(const RnQuery *q, int i)
{
	const char *p = q->raw;

	if (i >= q->nw)
		return "";
	for (int k = 0; k < i; k++) {
		p = strchr(p, ' ');
		if (!p)
			return "";
		p++;
	}
	return p;
}

int rn_prefix(const char *word, const char *typed)
{
	size_t n = strlen(typed);

	return n && !strncasecmp(word, typed, n);
}

static int is_sep(char c)
{
	return c == ' ' || c == '.' || c == '-' || c == '_' || c == '/' ||
	       c == ':' || c == ',' || c == '(' || c == ')';
}

/* Does `typed` begin a word of `hay`? 2 for a whole word, 1 for a prefix. */
static int starts_word(const char *hay, const char *typed)
{
	size_t n = strlen(typed);
	int best = 0;

	for (const char *p = hay; *p; p++) {
		if (p != hay && !is_sep(p[-1]))
			continue;
		if (is_sep(*p) || strncasecmp(p, typed, n))
			continue;
		if (!p[n] || is_sep(p[n]))
			return 2;
		best = 1;
	}
	return best;
}

int rn_words_match(const char *hay, const RnQuery *q)
{
	int score = 0;
	size_t typed = 0, len = strlen(hay);

	if (!q->nw)
		return 0;
	for (int i = 0; i < q->nw; i++) {
		int s = starts_word(hay, q->w[i]);

		if (!s)
			return 0;
		score += s * 10;
		typed += strlen(q->w[i]);
	}
	/* Coverage: `firewall` typed whole over `Firewall` outranks `fire`
	 * over `Firefox Developer Edition`. */
	if (len)
		score += (int)(typed * 40 / len);
	return score;
}
