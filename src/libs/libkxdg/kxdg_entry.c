/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkxdg — the desktop-entry reader
 * ---------------------------------
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "kxdg.h"

static void add(KxdgEntry *e, const char *key, size_t klen, const char *val,
		size_t vlen)
{
	char *k = kb_calloc(1, klen + 1);
	memcpy(k, key, klen);
	for (size_t i = 0; i < klen; i++)
		k[i] = (char)tolower((unsigned char)k[i]);

	/* A repeated key takes its last value, the way configparser does with
	 * strict=False — which is what these files were parsed with when the
	 * committed launchers were generated. */
	for (int i = 0; i < e->n; i++) {
		if (strcmp(e->v[i].key, k))
			continue;
		free(k);
		free(e->v[i].val);
		e->v[i].val = kb_calloc(1, vlen + 1);
		memcpy(e->v[i].val, val, vlen);
		return;
	}

	if (e->n >= e->cap) {
		int cap = e->cap ? e->cap * 2 : 32;
		KxdgPair *nv = kb_calloc((size_t)cap, sizeof(*nv));
		/* Guarded because the FIRST grow has e->v == NULL, and
		 * memcpy(dst, NULL, 0) is undefined however harmless it looks —
		 * memcpy's second parameter is declared never-null, so a
		 * sanitized build reports every .desktop file it reads. */
		if (e->v)
			memcpy(nv, e->v, (size_t)e->n * sizeof(*nv));
		free(e->v);
		e->v = nv;
		e->cap = cap;
	}
	e->v[e->n].key = k;
	e->v[e->n].val = kb_calloc(1, vlen + 1);
	memcpy(e->v[e->n].val, val, vlen);
	e->n++;
}

int kxdg_load(KxdgEntry *e, const char *path, const char *section)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	if (!data)
		return -1;

	memset(e, 0, sizeof(*e));
	int in_section = 0, found = 0;
	size_t i = 0;

	while (i < len) {
		size_t eol = i;
		while (eol < len && data[eol] != '\n')
			eol++;

		const char *line = data + i;
		size_t n = eol - i;
		while (n && (line[n - 1] == '\r' || line[n - 1] == ' ' ||
			     line[n - 1] == '\t'))
			n--;
		size_t s = 0;
		while (s < n && (line[s] == ' ' || line[s] == '\t'))
			s++;

		if (s < n && line[s] != '#' && line[s] != ';') {
			if (line[s] == '[') {
				size_t close = s + 1;
				while (close < n && line[close] != ']')
					close++;
				size_t slen = strlen(section);
				in_section = (close < n && close - s - 1 == slen &&
					      !memcmp(line + s + 1, section, slen));
				if (in_section)
					found = 1;
			} else if (in_section) {
				size_t eq = s;
				while (eq < n && line[eq] != '=')
					eq++;
				if (eq < n) {
					size_t ke = eq;
					while (ke > s && (line[ke - 1] == ' ' ||
							  line[ke - 1] == '\t'))
						ke--;
					size_t vs = eq + 1;
					while (vs < n && (line[vs] == ' ' ||
							  line[vs] == '\t'))
						vs++;
					if (ke > s)
						add(e, line + s, ke - s,
						    line + vs, n - vs);
				}
			}
		}
		i = eol + 1;
	}

	free(data);
	if (!found)
		kxdg_free(e);
	return found ? 0 : -1;
}

const char *kxdg_get(const KxdgEntry *e, const char *key, const char *def)
{
	char low[128];
	size_t n = strlen(key);
	if (n >= sizeof(low))
		return def;
	for (size_t i = 0; i <= n; i++)
		low[i] = (char)tolower((unsigned char)key[i]);

	for (int i = 0; i < e->n; i++)
		if (!strcmp(e->v[i].key, low))
			return e->v[i].val;
	return def;
}

int kxdg_bool(const KxdgEntry *e, const char *key, int def)
{
	const char *v = kxdg_get(e, key, NULL);
	if (!v)
		return def;
	return kb_str_ieq(v, "true");
}

void kxdg_free(KxdgEntry *e)
{
	for (int i = 0; i < e->n; i++) {
		free(e->v[i].key);
		free(e->v[i].val);
	}
	free(e->v);
	memset(e, 0, sizeof(*e));
}

/*
 * THE LAUNCH KEYS, ONCE. See kxdg.h for why there is one reader and not one
 * per surface.
 *
 * The Exec line is copied WITH its field codes. kxdg_exec_split spends them on
 * the documents a launch carries and decides from the same codes that a line
 * carrying none takes its documents appended instead, so a reader that
 * stripped them here would make every entry look like `Exec=xterm` and open a
 * file in the wrong argument.
 */
int kxdg_launch_read(const KxdgEntry *e, KxdgLaunch *out)
{
	const char *exec;

	if (!e || !out)
		return -1;
	memset(out, 0, sizeof(*out));

	exec = kxdg_get(e, "Exec", NULL);
	if (!exec || !*exec ||
	    !kb_str_ieq(kxdg_get(e, "Type", "Application"), "Application"))
		return -1;

	kb_strlcpy(out->exec, exec, sizeof(out->exec));
	kb_strlcpy(out->name, kxdg_get(e, "Name", ""), sizeof(out->name));
	kb_strlcpy(out->term, kxdg_get(e, "X-KDOS-Term", ""),
		   sizeof(out->term));
	kb_strlcpy(out->size, kxdg_get(e, "X-KDOS-Size", ""),
		   sizeof(out->size));
	out->terminal = kxdg_bool(e, "Terminal", 0);
	out->floating = kxdg_bool(e, "X-KDOS-Float", 0);
	return 0;
}
