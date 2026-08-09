/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — phase discovery and the metadata block
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbuild.h"

/* The five keys the orchestrator honours. CHROOT decides the execution
 * wrapper; the KDOS_* keys carry the snapshot contract and the UI labels.
 * Anything else in an env file is for the phase's own shell, not for us. */
static const char *META_KEYS[] = {
	"CHROOT",
	"KDOS_PHASE_TITLE",
	"KDOS_PHASE_DESC",
	"KDOS_SNAPSHOT_PATHS",
	"KDOS_SNAPSHOT_EXCLUDE",
	NULL
};

void kbuild_unquote(const char *raw, char *out, size_t cap)
{
	out[0] = 0;
	while (*raw == ' ' || *raw == '\t')
		raw++;
	if (!*raw)
		return;

	if (*raw == '"' || *raw == '\'') {
		char q = *raw++;
		const char *end = strchr(raw, q);
		if (!end)
			return;		/* unterminated: not a literal */
		size_t n = (size_t)(end - raw);
		if (n >= cap)
			n = cap - 1;
		memcpy(out, raw, n);
		out[n] = 0;
		return;
	}

	/* Unquoted: the value ends at a comment, then at whitespace. */
	char tmp[1024];
	kb_strlcpy(tmp, raw, sizeof(tmp));
	char *hash = strchr(tmp, '#');
	if (hash)
		*hash = 0;
	char *first = strtok(tmp, " \t\r\n");
	if (first)
		kb_strlcpy(out, first, cap);
}

int kbuild_safe_relpath(const char *path)
{
	if (!path || !*path || !strcmp(path, ".") || !strcmp(path, "/"))
		return 0;
	if (path[0] == '/' || path[0] == '~')
		return 0;

	/* ".." as a whole component only — "..foo" is a legitimate name. */
	const char *s = path;
	while (s) {
		const char *slash = strchr(s, '/');
		size_t n = slash ? (size_t)(slash - s) : strlen(s);
		if (n == 2 && !memcmp(s, "..", 2))
			return 0;
		s = slash ? slash + 1 : NULL;
	}
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int is_meta_key(const char *k)
{
	for (int i = 0; META_KEYS[i]; i++)
		if (!strcmp(META_KEYS[i], k))
			return 1;
	return 0;
}

static void split_into(const char *list, char dst[][128], int *n, int max,
		       size_t cap)
{
	*n = 0;
	char tmp[4096];
	kb_strlcpy(tmp, list, sizeof(tmp));
	for (char *t = strtok(tmp, " \t\n"); t && *n < max;
	     t = strtok(NULL, " \t\n")) {
		kb_strlcpy(dst[*n], t, cap);
		(*n)++;
	}
}

static void parse_env(KbuildPhase *p)
{
	size_t len = 0;
	char *data = kb_read_all(p->env_file, &len);
	if (!data)
		return;

	char paths[4096] = {0}, excl[4096] = {0};

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		char *s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		if (strncmp(s, "export", 6) || (s[6] != ' ' && s[6] != '\t'))
			continue;
		s += 6;
		while (*s == ' ' || *s == '\t')
			s++;

		char key[64];
		size_t k = 0;
		while (*s && k < sizeof(key) - 1 &&
		       (isalnum((unsigned char)*s) || *s == '_'))
			key[k++] = *s++;
		key[k] = 0;
		if (*s != '=' || !is_meta_key(key))
			continue;

		char val[4096];
		kbuild_unquote(s + 1, val, sizeof(val));

		if (!strcmp(key, "CHROOT"))
			p->chroot = !strcmp(val, "1");
		else if (!strcmp(key, "KDOS_PHASE_TITLE"))
			kb_strlcpy(p->title, val, sizeof(p->title));
		else if (!strcmp(key, "KDOS_PHASE_DESC"))
			kb_strlcpy(p->desc, val, sizeof(p->desc));
		else if (!strcmp(key, "KDOS_SNAPSHOT_PATHS"))
			kb_strlcpy(paths, val, sizeof(paths));
		else if (!strcmp(key, "KDOS_SNAPSHOT_EXCLUDE"))
			kb_strlcpy(excl, val, sizeof(excl));
	}
	free(data);

	/* A declared path is either kept or REJECTED — never quietly repaired.
	 * These are deleted and re-extracted as root. */
	char declared[KBUILD_MAX_PATHS][128];
	int nd = 0;
	split_into(paths, declared, &nd, KBUILD_MAX_PATHS, 128);
	for (int i = 0; i < nd; i++) {
		char one[128];
		kb_strlcpy(one, declared[i], sizeof(one));
		size_t n = strlen(one);
		while (n && one[n - 1] == '/')
			one[--n] = 0;
		if (kbuild_safe_relpath(one)) {
			if (p->nsnap < KBUILD_MAX_PATHS)
				kb_strlcpy(p->snap_path[p->nsnap++], one,
					   sizeof(p->snap_path[0]));
		} else if (p->nrejected < KBUILD_MAX_PATHS) {
			kb_strlcpy(p->rejected[p->nrejected++], declared[i],
				   sizeof(p->rejected[0]));
		}
	}

	char ex[KBUILD_MAX_PATHS][128];
	int ne = 0;
	split_into(excl, ex, &ne, KBUILD_MAX_PATHS, 128);
	for (int i = 0; i < ne && p->nexclude < KBUILD_MAX_PATHS; i++)
		kb_strlcpy(p->snap_exclude[p->nexclude++], ex[i],
			   sizeof(p->snap_exclude[0]));
}

/* ──────────────────────────────────────────────────────────────────────── */

static int numeric_prefix(const char *name)
{
	if (!isdigit((unsigned char)name[0]))
		return 0;
	const char *s = name;
	while (isdigit((unsigned char)*s))
		s++;
	return *s == '_';
}

int kbuild_discover(const char *script_dir, KbuildPhase *out, int max)
{
	char **names = kb_listdir(script_dir, NULL);
	if (!names)
		return 0;

	int n = 0;
	for (char **e = names; *e && n < max; e++) {
		char *full = kb_path_join(script_dir, *e);
		if (!kb_is_dir(full) || !numeric_prefix(*e)) {
			free(full);
			continue;
		}

		KbuildPhase *p = &out[n];
		memset(p, 0, sizeof(*p));
		p->index = n;
		kb_strlcpy(p->dir_name, *e, sizeof(p->dir_name));
		kb_strlcpy(p->dir_path, full, sizeof(p->dir_path));
		free(full);

		const char *bare = strchr(*e, '_');
		kb_strlcpy(p->name, bare ? bare + 1 : *e, sizeof(p->name));

		char leaf[128];
		snprintf(leaf, sizeof(leaf), "%s.env.sh", p->name);
		char *env = kb_path_join(script_dir, leaf);
		if (kb_path_exists(env)) {
			kb_strlcpy(p->env_file, env, sizeof(p->env_file));
			parse_env(p);
		}
		free(env);

		/* No declared title: the directory name, tidied. */
		if (!p->title[0]) {
			kb_strlcpy(p->title, p->name, sizeof(p->title));
			for (char *c = p->title; *c; c++)
				if (*c == '_' || *c == '-')
					*c = ' ';
		}
		n++;
	}
	kb_strv_free(names);
	return n;
}

int kbuild_snapshottable(const KbuildPhase *p)
{
	return p->nsnap > 0;
}

void kbuild_label(const KbuildPhase *p, char *out, size_t cap)
{
	char base[64];
	kb_strlcpy(base, p->name, sizeof(base));
	for (char *c = base; *c; c++)
		if (*c == '_' || *c == '-')
			*c = ' ';
	snprintf(out, cap, "%s%s", base, p->chroot ? " (Chroot)" : "");
}
