/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — KConfig files, read as lines and written back as lines
 *
 * Not a KConfig implementation. It knows exactly what the merge and the
 * contrast check need: where a group starts, which line assigns a plain key,
 * and how to put the file back. Everything else on a line — `$e` expansion
 * flags, localised keys, comments, KConfig's escaped values — is carried as
 * the bytes it was read as, because a writer that re-spelt a value it did not
 * own would change what another program reads.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-theme.h"

static void push_line(KthGroup *g, const char *s, size_t n)
{
	if (g->nline == g->cap) {
		g->cap = g->cap ? g->cap * 2 : 16;
		g->line = kb_realloc(g->line, (size_t)g->cap * sizeof(*g->line));
	}
	char *l = kb_calloc(1, n + 1);

	memcpy(l, s, n);
	g->line[g->nline++] = l;
}

static KthGroup *push_group(KthIni *ini, const char *name, size_t n)
{
	if (ini->ng == ini->cap) {
		ini->cap = ini->cap ? ini->cap * 2 : 16;
		ini->g = kb_realloc(ini->g, (size_t)ini->cap * sizeof(*ini->g));
	}
	KthGroup *g = &ini->g[ini->ng++];

	memset(g, 0, sizeof(*g));
	g->name = kb_calloc(1, n + 1);
	memcpy(g->name, name, n);
	return g;
}

int kth_ini_load(KthIni *ini, const char *path)
{
	memset(ini, 0, sizeof(*ini));

	size_t len = 0;
	char *data = kb_read_all(path, &len);

	if (!data)
		return access(path, F_OK) == 0 ? -1 : 0;

	KthGroup *cur = NULL;
	char *p = data, *end = data + len;

	while (p < end) {
		char *nl = memchr(p, '\n', (size_t)(end - p));
		size_t n = nl ? (size_t)(nl - p) : (size_t)(end - p);

		if (n && p[n - 1] == '\r')
			n--;

		/* A header is `[` at column 0 and `]` as the line's last byte;
		 * the name is what lies between the first and the last, so a
		 * nested `[A][B]` keeps its inner `][` and is one group. */
		if (n >= 2 && p[0] == '[' && p[n - 1] == ']') {
			cur = push_group(ini, p + 1, n - 2);
		} else {
			if (!cur)
				cur = push_group(ini, "", 0);
			push_line(cur, p, n);
		}
		p = nl ? nl + 1 : end;
	}
	free(data);
	return 0;
}

void kth_ini_free(KthIni *ini)
{
	for (int i = 0; i < ini->ng; i++) {
		for (int j = 0; j < ini->g[i].nline; j++)
			free(ini->g[i].line[j]);
		free(ini->g[i].line);
		free(ini->g[i].name);
	}
	free(ini->g);
	memset(ini, 0, sizeof(*ini));
}

KthGroup *kth_ini_group(KthIni *ini, const char *name, int create)
{
	/* The LAST group of a name is the one a key is added to: KConfig reads
	 * a name that appears twice as one group, and appending to the last
	 * keeps the new key after every earlier assignment of it. */
	for (int i = ini->ng - 1; i >= 0; i--)
		if (!strcmp(ini->g[i].name, name))
			return &ini->g[i];
	if (!create)
		return NULL;

	KthGroup *g = push_group(ini, name, strlen(name));

	g->fresh = 1;
	return g;
}

/* The key a line assigns, or 0 when it assigns none. A key is everything
 * before `=` with its trailing blanks dropped; a `[` in it is a localised or
 * flagged spelling, which is not the plain key. */
static int line_is_key(const char *l, const char *key)
{
	while (*l == ' ' || *l == '\t')
		l++;
	size_t k = strlen(key);

	if (strncmp(l, key, k))
		return 0;
	l += k;
	while (*l == ' ' || *l == '\t')
		l++;
	return *l == '=';
}

static const char *line_value(const char *l)
{
	const char *eq = strchr(l, '=');

	if (!eq)
		return NULL;
	eq++;
	while (*eq == ' ' || *eq == '\t')
		eq++;
	return eq;
}

const char *kth_ini_get(const KthGroup *g, const char *key)
{
	const char *v = NULL;

	if (!g)
		return NULL;
	for (int i = 0; i < g->nline; i++)
		if (line_is_key(g->line[i], key))
			v = line_value(g->line[i]);
	return v;
}

void kth_ini_unset(KthGroup *g, const char *key)
{
	int o = 0;

	for (int i = 0; i < g->nline; i++) {
		if (line_is_key(g->line[i], key)) {
			free(g->line[i]);
			continue;
		}
		g->line[o++] = g->line[i];
	}
	g->nline = o;
}

void kth_ini_set(KthGroup *g, const char *key, const char *val)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "%s=%s", key, val);

	/* Replaced where it stood, so a merge that changes a value moves no
	 * line; every later duplicate goes, so the file says it once. */
	int at = -1;

	for (int i = 0; i < g->nline; i++)
		if (line_is_key(g->line[i], key)) {
			at = i;
			break;
		}
	if (at < 0) {
		/* After the group's last non-blank line, so the blank line
		 * that separates it from the next group stays at the end. */
		int pos = g->nline;

		while (pos > 0 && !g->line[pos - 1][0])
			pos--;
		push_line(g, b.p, b.n);
		char *l = g->line[g->nline - 1];

		memmove(&g->line[pos + 1], &g->line[pos],
			(size_t)(g->nline - 1 - pos) * sizeof(*g->line));
		g->line[pos] = l;
	} else {
		free(g->line[at]);
		g->line[at] = kb_strdup(b.p);
		int o = at + 1;

		for (int i = at + 1; i < g->nline; i++) {
			if (line_is_key(g->line[i], key)) {
				free(g->line[i]);
				continue;
			}
			g->line[o++] = g->line[i];
		}
		g->nline = o;
	}
	kb_buf_free(&b);
}

void kth_ini_drop(KthIni *ini, const char *name)
{
	int o = 0;

	for (int i = 0; i < ini->ng; i++) {
		KthGroup *g = &ini->g[i];

		if (!strcmp(g->name, name)) {
			for (int j = 0; j < g->nline; j++)
				free(g->line[j]);
			free(g->line);
			free(g->name);
			continue;
		}
		ini->g[o++] = *g;
	}
	ini->ng = o;
}

void kth_ini_write(const KthIni *ini, KbBuf *out)
{
	int wrote = 0, blank = 1;

	for (int i = 0; i < ini->ng; i++) {
		const KthGroup *g = &ini->g[i];

		/* The unnamed group holds only the lines before the first
		 * header; with none left it is not written at all. */
		if (!g->name[0] && !g->nline)
			continue;

		/* A group added since the file was read is set off from the
		 * one above it by a blank line; a group that was read keeps
		 * exactly the spacing it was read with, so a comment written
		 * over its header stays against that header. */
		if (g->name[0]) {
			if (wrote && !blank && g->fresh)
				kb_buf_str(out, "\n");
			kb_buf_printf(out, "[%s]\n", g->name);
			blank = 0;
		}
		for (int j = 0; j < g->nline; j++) {
			kb_buf_printf(out, "%s\n", g->line[j]);
			blank = !g->line[j][0];
		}
		wrote = 1;
	}
}

void kth_emit(const char *dir, const char *name, const KbBuf *b)
{
	if (kb_mkdir_p(dir) < 0)
		kb_die("cannot create %s", dir);

	char *path = kb_path_join(dir, name);
	KbBuf tmp = {0};

	kb_buf_printf(&tmp, "%s.tmp", path);
	if (kb_write_all(tmp.p, b->n ? b->p : "", b->n) < 0 ||
	    rename(tmp.p, path) < 0) {
		unlink(tmp.p);
		kb_die("cannot write %s", path);
	}
	kb_buf_free(&tmp);
	free(path);
}
