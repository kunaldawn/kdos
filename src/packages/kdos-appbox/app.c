/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * Apps: the name -> in-box command table, install/uninstall, and turning a
 * box's desktop entries into host launchers.
 *
 * There are TWO tables and the split is deliberate:
 *
 *   /usr/share/kdos/alien-apps          baked into the ISO by
 *                                       ports/appbox/genlaunchers.py, read-only
 *   ~/.local/share/kdos/alien-apps      whatever the user has installed since
 *
 * Lookups check the user table first. The same split applies to the launchers
 * (/etc/skel copy vs ~/.local/share/applications) and to the shims
 * (/usr/local/bin vs ~/.local/bin, which /etc/profile.d/10-wayland.sh already
 * puts on PATH). Nothing this program does at runtime needs root.
 */

#include "kdos-appbox.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *user_table(void)
{
	char *p = kb_calloc(1, MAX_LINE);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, MAX_LINE, "%s/kdos/alien-apps", data);
	else
		snprintf(p, MAX_LINE, "%s/.local/share/kdos/alien-apps", kb_home_dir());
	return p;
}

/* One "name\tcommand" line, in either table. */
/*
 * `name<TAB>command` — and, since the pack lane, an optional third field
 * naming the pack that provides it. Readers split at the SECOND tab and a
 * two-field line still parses, because a table written by an older
 * genlaunchers must not stop working the day the third field exists.
 */
static int table_lookup(const char *path, const char *name, char *cmd, size_t n,
			char *pack, size_t pn)
{
	char *buf = kb_calloc(1, 1 << 18);
	char *line, *save;
	int found = 0;

	if (kb_read_file(path, buf, 1 << 18) < 0) {
		free(buf);
		return 0;
	}
	if (pack && pn)
		pack[0] = '\0';
	for (line = strtok_r(buf, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab, *tab2;
		if (*line == '#')
			continue;
		tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab = '\0';
		if (strcmp(line, name))
			continue;
		tab2 = strchr(tab + 1, '\t');
		if (tab2) {
			*tab2 = '\0';
			if (pack && pn)
				snprintf(pack, pn, "%s", tab2 + 1);
		}
		snprintf(cmd, n, "%s", tab + 1);
		found = 1;
	}
	free(buf);
	return found;
}

/*
 * THE PACK BEHIND A COMMAND, for the launch form a desktop entry uses.
 * `run_as_shim` is keyed by the shim NAME — `gimp` — and finds its pack in the
 * table's third field. A `.desktop` entry runs `kdos-appbox run <exec>` with
 * the application's own path, `/usr/lib/firefox-esr/firefox-esr`, and that
 * form found no pack at all: it launched into the default box, `kdos-apps`,
 * which the pack lane never had — "click Firefox in the Start menu, nothing
 * opens", while the shim from a prompt worked. The command column's first
 * token is the exec; it is matched whole, then by basename, so
 * `firefox-esr` and `/usr/lib/firefox-esr/firefox-esr` both resolve.
 */
/*
 * THE PROGRAM A COMMAND LINE RUNS IS NOT ALWAYS ITS FIRST WORD. Debian ships
 * `sh -c "wesnoth-1.18 >/dev/null 2>&1"` and `env GDK_BACKEND=x11 audacity`,
 * and a lookup keyed on the first token asks the table about `sh` — which no
 * pack carries, so the Start menu's launch of every wrapped entry was refused
 * while the shim, keyed by NAME, worked. This skips the wrappers: `env` and
 * its assignments, and `sh -c`/`bash -c` down to the first word of the
 * string they run. Path and desktop-entry quotes are stripped, so what comes
 * back is a bare basename that compares against another bare basename.
 */
void app_exec_key(const char *cmdline, char *out, size_t n)
{
	char toks[8][512];
	const char *p = cmdline;
	int nt = 0, k = 0;

	/* desktop-entry tokens: double quotes group, backslash escapes */
	while (nt < 8) {
		size_t i = 0;
		int q = 0;
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		while (*p && (q || (*p != ' ' && *p != '\t'))) {
			if (*p == '"') {
				q = !q;
				p++;
				continue;
			}
			if (*p == '\\' && p[1])
				p++;
			if (i + 1 < sizeof(toks[0]))
				toks[nt][i++] = *p;
			p++;
		}
		toks[nt][i] = 0;
		nt++;
	}
	while (k < nt && (!strcmp(toks[k], "env") ||
			  !strcmp(toks[k], "/usr/bin/env") ||
			  (strchr(toks[k], '=') && toks[k][0] != '/' &&
			   toks[k][0] != '.')))
		k++;
	if (k + 2 < nt &&
	    (!strcmp(toks[k], "sh") || !strcmp(toks[k], "bash") ||
	     !strcmp(toks[k], "/bin/sh") || !strcmp(toks[k], "/bin/bash")) &&
	    !strcmp(toks[k + 1], "-c")) {
		/* the string sh runs: its first word is the program */
		char *sp = toks[k + 2];
		while (*sp == ' ' || *sp == '\t')
			sp++;
		sp[strcspn(sp, " \t")] = 0;
		k += 2;
		memmove(toks[k], sp, strlen(sp) + 1);
	}
	if (k >= nt) {
		out[0] = 0;
		return;
	}
	const char *b = strrchr(toks[k], '/');
	snprintf(out, n, "%s", b ? b + 1 : toks[k]);
}

static int table_pack_by_exec(const char *path, const char *exec, char *pack,
			      size_t pn)
{
	char *buf = kb_calloc(1, 1 << 18);
	char *line, *save;
	char eb[512];
	int found = 0;

	app_exec_key(exec, eb, sizeof(eb));
	if (!eb[0] || kb_read_file(path, buf, 1 << 18) < 0) {
		free(buf);
		return 0;
	}
	for (line = strtok_r(buf, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab, *tab2;
		char cb[512];
		if (*line == '#' || !(tab = strchr(line, '\t')))
			continue;
		tab2 = strchr(tab + 1, '\t');
		if (!tab2)
			continue;		/* a two-field row names no pack */
		*tab2 = '\0';
		app_exec_key(tab + 1, cb, sizeof(cb));
		if (!strcmp(cb, eb)) {
			snprintf(pack, pn, "%s", tab2 + 1);
			found = 1;
		}
	}
	free(buf);
	return found;
}

int app_pack_by_exec(const char *exec, char *pack, size_t pn)
{
	char *ut = user_table();
	int found = table_pack_by_exec(ut, exec, pack, pn);
	free(ut);
	if (!found)
		found = table_pack_by_exec(APP_TABLE, exec, pack, pn);
	return found;
}

int app_lookup(const char *name, char *cmd, size_t n)
{
	return app_lookup_pack(name, cmd, n, NULL, 0);
}

/* The same lookup, also answering which pack provides it. User entries win, as
 * they always have. */
int app_lookup_pack(const char *name, char *cmd, size_t n, char *pack, size_t pn)
{
	char *ut = user_table();
	int found = table_lookup(ut, name, cmd, n, pack, pn);
	free(ut);
	if (!found)
		found = table_lookup(APP_TABLE, name, cmd, n, pack, pn);
	return found;
}

int app_list(void)
{
	char *ut = user_table();
	const char *paths[2];
	char *buf = kb_calloc(1, 1 << 18);
	int i;

	paths[0] = APP_TABLE;
	paths[1] = ut;
	for (i = 0; i < 2; i++) {
		char *line, *save;
		if (kb_read_file(paths[i], buf, 1 << 18) < 0)
			continue;
		for (line = strtok_r(buf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *tab;
			if (*line == '#' || !*line)
				continue;
			tab = strchr(line, '\t');
			if (!tab)
				continue;
			*tab = '\0';
			printf("%-24s %-8s %s\n", line,
			       i ? "user" : "baked", tab + 1);
		}
	}
	free(buf);
	free(ut);
	return 0;
}

/* ------------------------------------------------------------------ */

int app_table_load(App **out)
{
	char *buf = kb_calloc(1, 1 << 18);
	char *ut = user_table();
	const char *paths[2];
	App *apps = NULL;
	int n = 0, cap = 0, i;

	paths[0] = APP_TABLE;
	paths[1] = ut;
	for (i = 0; i < 2; i++) {
		char *line, *save;
		if (kb_read_file(paths[i], buf, 1 << 18) < 0)
			continue;
		for (line = strtok_r(buf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *tab;
			if (*line == '#' || !*line)
				continue;
			tab = strchr(line, '\t');
			if (!tab)
				continue;
			*tab = '\0';
			{
				char *tab2 = strchr(tab + 1, '\t');
				if (tab2)
					*tab2 = '\0';
			}
			if (n == cap) {
				cap = cap ? cap * 2 : 64;
				apps = realloc(apps, (size_t)cap * sizeof(*apps));
				if (!apps)
					kb_die("out of memory");
			}
			snprintf(apps[n].name, sizeof(apps[n].name), "%s", line);
			snprintf(apps[n].cmd, sizeof(apps[n].cmd), "%s", tab + 1);
			n++;
		}
	}
	free(buf);
	free(ut);
	*out = apps;
	return n;
}
