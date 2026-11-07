/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — KDE's configuration files, read here and written by KDE
 *
 * READ HERE, WRITTEN BY kwriteconfig6. A Plasma setting is a KConfig file, and
 * KConfig rewrites a file whole — groups and keys in its own order, values in
 * its own escaping — every time any KDE program saves it. A writer of ours that
 * edited the text would leave a file KConfig then reorders, so `kdos phosphor
 * off` could never hand back the bytes `on` found. kwriteconfig6 is KConfig
 * itself, and its `--notify` is the only way a running KDE program hears of a
 * change: there is no file watch to rely on.
 *
 * READING IS NOT kreadconfig6, because the question asked is almost always
 * "what does THE USER'S file say", which decides whether a value is KDOS's to
 * replace (P8). kreadconfig6 answers with the whole cascade, and cannot tell a
 * key the user set from one /etc/xdg supplied. kdt_kcfg_cascade() is the
 * cascade, for the questions that are about it.
 *
 * A GROUP IS NAMED THE WAY THE FILE SPELLS ITS HEADER: `Plugins`, or a nested
 * one as `Greeter][Wallpaper][org.kde.image][General`, so the string in a call
 * is the string a person greps the file for.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-tools.h"

/* KConfig's own escapes: `\s` is a space it had to protect (leading or
 * trailing), `\xHH` a control byte; anything else after a backslash is kept as
 * it was, which is what KConfig itself does with an escape it does not know. */
char *kdt_kcfg_unescape(const char *raw)
{
	size_t n = strlen(raw);
	char *out = kb_calloc(n + 1, 1), *o = out;

	for (const char *p = raw; *p; p++) {
		if (*p != '\\' || !p[1]) {
			*o++ = *p;
			continue;
		}
		p++;
		switch (*p) {
		case 's': *o++ = ' '; break;
		case 't': *o++ = '\t'; break;
		case 'n': *o++ = '\n'; break;
		case 'r': *o++ = '\r'; break;
		case '\\': *o++ = '\\'; break;
		case 'x': {
			unsigned v = 0;
			int k = 0;

			while (k < 2 && p[1] &&
			       ((p[1] >= '0' && p[1] <= '9') ||
				(p[1] >= 'a' && p[1] <= 'f') ||
				(p[1] >= 'A' && p[1] <= 'F'))) {
				char c = *++p;

				v = v * 16 + (unsigned)(c <= '9' ? c - '0'
						: (c | 32) - 'a' + 10);
				k++;
			}
			if (k)
				*o++ = (char)v;
			else
				*o++ = 'x';
			break;
		}
		default:
			*o++ = '\\';
			*o++ = *p;
		}
	}
	*o = '\0';
	return out;
}

static char *trim(char *s)
{
	char *e;

	while (*s == ' ' || *s == '\t')
		s++;
	e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		*--e = '\0';
	return s;
}

/*
 * The header's group, with any `[$i]` flag after it dropped. Returns 0 for a
 * line that is not a header.
 */
static int header(char *line, char **group)
{
	char *end;

	if (line[0] != '[')
		return 0;
	end = line + strlen(line);
	/* `[A][$i]`: the flag is a trailing bracket of its own. */
	if (end - line > 5 && !strcmp(end - 5, "[$i]"))
		end -= 4, *end = '\0';
	if (end <= line + 1 || end[-1] != ']')
		return 0;
	end[-1] = '\0';
	*group = line + 1;
	return 1;
}

char *kdt_kcfg_get_raw(const char *path, const char *group, const char *key)
{
	char *data = kb_read_all(path, NULL), *line, *next, *found = NULL;
	const char *cur = "";
	int in = !group || !*group;

	if (!data)
		return NULL;
	for (line = data; line && *line; line = next) {
		char *nl = strchr(line, '\n'), *t, *g, *eq, *k;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		t = trim(line);
		if (!*t || *t == '#')
			continue;
		if (header(t, &g)) {
			cur = g;
			in = group && !strcmp(cur, group);
			continue;
		}
		if (!in || !(eq = strchr(t, '=')))
			continue;
		*eq = '\0';
		k = trim(t);
		/* `Key[$e]` is the same key with a flag; `Key[de]` is another
		 * language's, and only the untranslated one is asked for. */
		{
			char *br = strchr(k, '[');

			if (br && br[1] == '$')
				*br = '\0';
		}
		if (strcmp(k, key))
			continue;
		free(found);
		found = kb_strdup(trim(eq + 1));
	}
	free(data);
	return found;
}

char *kdt_kcfg_get(const char *path, const char *group, const char *key)
{
	char *raw = kdt_kcfg_get_raw(path, group, key), *v;

	if (!raw)
		return NULL;
	v = kdt_kcfg_unescape(raw);
	free(raw);
	return v;
}

char *kdt_kcfg_user(const char *file, const char *group, const char *key)
{
	char *path = kdt_cfg_home(file);
	char *v = kdt_kcfg_get(path, group, key);

	free(path);
	return v;
}

/*
 * THE CASCADE, the way KConfig walks it: the user's file, then each
 * $XDG_CONFIG_DIRS entry in order (/etc/xdg when it is unset), the first file
 * that carries the key winning. KDOS's own defaults sit in
 * /usr/share/kdos/xdg at the end of that list (B10), so a machine owner's
 * /etc/xdg still beats them.
 */
char *kdt_kcfg_cascade(const char *file, const char *group, const char *key)
{
	char *v = kdt_kcfg_user(file, group, key);
	const char *dirs = getenv("XDG_CONFIG_DIRS");
	char *copy, *save = NULL;

	if (v)
		return v;
	copy = kb_strdup(dirs && *dirs ? dirs : "/etc/xdg");
	for (char *d = strtok_r(copy, ":", &save); d && !v;
	     d = strtok_r(NULL, ":", &save)) {
		char *p;

		if (!*d)
			continue;
		p = kb_path_join(d, file);
		v = kdt_kcfg_get(p, group, key);
		free(p);
	}
	free(copy);
	return v;
}

int kdt_kcfg_bool(const char *value, int dflt)
{
	if (!value || !*value)
		return dflt;
	return !strcmp(value, "true") || !strcmp(value, "1") ||
	       !strcmp(value, "on") || !strcmp(value, "yes");
}

/*
 * KConfig's change notice, the signal kwriteconfig6 --notify sends: a{saay},
 * each group named with the key names that moved, on /<file>. A running KDE
 * program that watches the file re-reads those keys.
 */
static void notify_unset(const char *file, const char *group, const char *key)
{
	KdtBus *b = kdt_bus_open(0);
	char path[256], grp[512];
	KdtMsg *m;
	size_t o = 0;

	if (!b)
		return;
	snprintf(path, sizeof(path), "/%s", file);
	for (const char *g = group; *g && o + 1 < sizeof(grp); g++) {
		if (!strncmp(g, "][", 2)) {
			grp[o++] = '\x1d';
			g++;
		} else {
			grp[o++] = *g;
		}
	}
	grp[o] = '\0';
	m = kdt_bus_new_signal(b, path, "org.kde.kconfig.notify",
			       "ConfigChanged");
	if (m) {
		kdt_bus_open_container(b, m, 'a', "{saay}");
		kdt_bus_open_container(b, m, 'e', "saay");
		kdt_bus_append(b, m, "s", grp);
		kdt_bus_open_container(b, m, 'a', "ay");
		kdt_bus_open_container(b, m, 'a', "y");
		for (const char *k = key; *k; k++)
			kdt_bus_append(b, m, "y", (int)(unsigned char)*k);
		kdt_bus_close_container(b, m);
		kdt_bus_close_container(b, m);
		kdt_bus_close_container(b, m);
		kdt_bus_close_container(b, m);
		kdt_bus_send(b, m);
	}
	kdt_bus_close(b);
}

/*
 * A KEY TAKEN OUT OF THE PERSON'S FILE, NOT DELETED THROUGH KCONFIG.
 * kwriteconfig6 --delete on a key the cascade also holds (/etc/xdg,
 * /usr/share/kdos/xdg) writes `key[$d]`, a marker that MASKS the machine's
 * value, so "back to the default" would become "no value at all" — the
 * shipped animation speed, the Konsole light profile, the fixed face. Taking
 * the line out lets the cascade answer again. The file is KConfig's own
 * layout (groups apart by one blank line), and a group left empty goes with
 * its blank line, so the bytes are the ones KConfig would have written
 * without the key.
 */
static int unset_key(const char *file, const char *group, const char *key,
		     int notify)
{
	char *path = kdt_cfg_home(file), *data = kb_read_all(path, NULL);
	KbBuf out = { 0 };
	size_t klen = strlen(key);
	int removed = 0, rc = 0;
	/* The file in blocks: what comes before the first header, then one
	 * block per group, header first. */
	struct { char *start; size_t len; int target, keys; } blk[512];
	int nb = 0;

	if (!data) {
		free(path);
		return 0;
	}
	blk[0].start = data;
	blk[0].len = 0;
	blk[0].target = !*group;
	blk[0].keys = 0;
	nb = 1;
	for (char *l = data, *next; l && *l; l = next) {
		char *nl = strchr(l, '\n'), *g, hdr[1024];
		size_t len;

		next = nl ? nl + 1 : NULL;
		len = nl ? (size_t)(nl - l + 1) : strlen(l);
		snprintf(hdr, sizeof(hdr), "%.*s", (int)len, l);
		hdr[strcspn(hdr, "\n")] = '\0';
		if (l[0] == '[' && nb < (int)(sizeof(blk) / sizeof(blk[0]))) {
			blk[nb].start = l;
			blk[nb].len = len;
			blk[nb].target = header(hdr, &g) && !strcmp(g, group);
			blk[nb].keys = 0;
			nb++;
			continue;
		}
		if (blk[nb - 1].target && !strncmp(l, key, klen) &&
		    (l[klen] == '=' || l[klen] == '[' || l[klen] == ' ')) {
			/* Cut out in place: the block keeps only what follows. */
			memmove(l, l + len, strlen(l + len) + 1);
			next = l;
			removed = 1;
			continue;
		}
		if (hdr[0] && hdr[0] != '#')
			blk[nb - 1].keys++;
		blk[nb - 1].len += len;
	}
	if (removed) {
		for (int i = 0; i < nb; i++) {
			size_t len = blk[i].len;

			if (i > 0 && blk[i].target && !blk[i].keys)
				continue;
			/* KConfig's layout: a block's own trailing blank lines
			 * dropped, one blank line between blocks. */
			while (len && (blk[i].start[len - 1] == '\n') &&
			       (len < 2 || blk[i].start[len - 2] == '\n'))
				len--;
			if (!len)
				continue;
			if (out.n)
				kb_buf_add(&out, "\n", 1);
			kb_buf_add(&out, blk[i].start, len);
			if (blk[i].start[len - 1] != '\n')
				kb_buf_add(&out, "\n", 1);
		}
		kb_buf_add(&out, "", 1);
		rc = kb_write_file_atomic(path, out.p) == 0 ? 0 : -1;
		if (!rc && notify)
			notify_unset(file, group, key);
	}
	kb_buf_free(&out);
	free(data);
	free(path);
	return rc;
}

/*
 * One key, through kwriteconfig6; `value` NULL takes the key out of the
 * person's file (see unset_key). A nested group is one `--group` per level,
 * split at the `][` the file spells.
 */
int kdt_kcfg_set(const char *file, const char *group, const char *key,
		 const char *value, int notify)
{
	KbArgv a = { 0 };
	char *copy, *g;
	int rc;

	if (!value)
		return unset_key(file, group ? group : "", key, notify);
	copy = kb_strdup(group ? group : "");
	g = copy;
	kb_argv_add(&a, "kwriteconfig6");
	kb_argv_add(&a, "--file");
	kb_argv_add(&a, file);
	while (g && *g) {
		char *sep = strstr(g, "][");

		if (sep)
			*sep = '\0';
		kb_argv_add(&a, "--group");
		kb_argv_add(&a, g);
		g = sep ? sep + 2 : NULL;
	}
	kb_argv_add(&a, "--key");
	kb_argv_add(&a, key);
	if (notify)
		kb_argv_add(&a, "--notify");
	/* `--` keeps a value that starts with a dash from being taken for an
	 * option. */
	kb_argv_add(&a, "--");
	kb_argv_add(&a, value);
	kb_argv_end(&a);
	rc = kb_run(&a);
	free(copy);
	return rc == 0 ? 0 : -1;
}
