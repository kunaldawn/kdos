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
	char *p = xmalloc(MAX_LINE);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, MAX_LINE, "%s/kdos/alien-apps", data);
	else
		snprintf(p, MAX_LINE, "%s/.local/share/kdos/alien-apps", home_dir());
	return p;
}

static char *user_apps_dir(void)
{
	char *p = xmalloc(MAX_LINE);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, MAX_LINE, "%s/applications", data);
	else
		snprintf(p, MAX_LINE, "%s/.local/share/applications", home_dir());
	return p;
}

static char *user_bin_dir(void)
{
	char *p = xmalloc(MAX_LINE);
	snprintf(p, MAX_LINE, "%s/.local/bin", home_dir());
	return p;
}

/* One "name\tcommand" line, in either table. */
static int table_lookup(const char *path, const char *name, char *cmd, size_t n)
{
	char *buf = xmalloc(1 << 18);
	char *line, *save;
	int found = 0;

	if (read_file(path, buf, 1 << 18) < 0) {
		free(buf);
		return 0;
	}
	for (line = strtok_r(buf, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab;
		if (*line == '#')
			continue;
		tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab = '\0';
		if (!strcmp(line, name)) {
			snprintf(cmd, n, "%s", tab + 1);
			found = 1;
		}
	}
	free(buf);
	return found;
}

int app_lookup(const char *name, char *cmd, size_t n)
{
	char *ut = user_table();
	int found = table_lookup(ut, name, cmd, n);
	free(ut);
	if (!found)
		found = table_lookup(APP_TABLE, name, cmd, n);
	return found;
}

int app_list(void)
{
	char *ut = user_table();
	const char *paths[2];
	char *buf = xmalloc(1 << 18);
	int i;

	paths[0] = APP_TABLE;
	paths[1] = ut;
	for (i = 0; i < 2; i++) {
		char *line, *save;
		if (read_file(paths[i], buf, 1 << 18) < 0)
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

/* Minimal .desktop reader: first [Desktop Entry] section only, so an Action
 * section cannot shadow Name or Icon. */
static int de_get(const char *text, const char *key, char *out, size_t n)
{
	const char *p = strstr(text, "[Desktop Entry]");
	size_t klen = strlen(key);

	out[0] = '\0';
	if (!p)
		return 0;
	p += 15;
	while (*p) {
		const char *eol = strchr(p, '\n');
		size_t len = eol ? (size_t)(eol - p) : strlen(p);
		if (*p == '[')
			break;
		if (!strncmp(p, key, klen) && p[klen] == '=') {
			size_t vlen = len - klen - 1;
			if (vlen >= n)
				vlen = n - 1;
			memcpy(out, p + klen + 1, vlen);
			out[vlen] = '\0';
			return 1;
		}
		if (!eol)
			break;
		p = eol + 1;
	}
	return 0;
}

/* Drop the field codes a wrapper cannot honour, keep the ones that carry the
 * user's file or URL through. */
static void strip_field_codes(char *exec)
{
	char *r = exec, *w = exec;
	while (*r) {
		char *sp = strchr(r, ' ');
		size_t len = sp ? (size_t)(sp - r) : strlen(r);
		int keep = 1;
		if (len == 2 && r[0] == '%') {
			char c = r[1];
			keep = (c == 'U' || c == 'F' || c == 'f' || c == 'u');
		}
		if (keep) {
			if (w != exec)
				*w++ = ' ';
			memmove(w, r, len);
			w += len;
		}
		if (!sp)
			break;
		r = sp + 1;
	}
	*w = '\0';
}

static void lower(char *s)
{
	for (; *s; s++)
		if (*s >= 'A' && *s <= 'Z')
			*s += 32;
}

/*
 * Pull the box's desktop entries out to a directory both sides can see.
 * $HOME is shared with the box, so a plain cp inside the container lands the
 * files where the host can read them — no shell, no pipe, no tar.
 */
static char *dump_entries(const char *box)
{
	char *dir = xmalloc(MAX_LINE);
	Argv a = {0};

	snprintf(dir, MAX_LINE, "%s/.cache/kdos/appdump", home_dir());
	mkdir_p(dir);
	argv_add(&a, "distrobox");
	argv_add(&a, "enter");
	argv_add(&a, box);
	argv_add(&a, "--");
	argv_add(&a, "cp");
	argv_add(&a, "-rT");
	argv_add(&a, "/usr/share/applications");
	argv_add(&a, dir);
	argv_end(&a);
	if (run_quiet(&a) != 0) {
		warn("could not read desktop entries out of '%s'", box);
		free(dir);
		return NULL;
	}
	return dir;
}

/*
 * Add host launchers for entries the system does not already ship one for.
 *
 * Deliberately additive: the baked launchers carry curated ids (the dock
 * favorites reference kdos-firefox, not kdos-firefox-esr) and regenerating
 * them here would rename them out from under the dock. New packages get the
 * upstream id, lowercased.
 */
int app_refresh(const char *box)
{
	char *dump = dump_entries(box);
	char *appdir = user_apps_dir();
	char *bindir = user_bin_dir();
	char *table = user_table();
	char *text = xmalloc(1 << 16);
	FILE *tf;
	DIR *d;
	struct dirent *e;
	int added = 0;

	if (!dump)
		return 1;
	mkdir_p(appdir);
	mkdir_p(bindir);
	{
		char *slash = strrchr(table, '/');
		*slash = '\0';
		mkdir_p(table);
		*slash = '/';
	}

	tf = fopen(table, "a");
	d = opendir(dump);
	while (d && (e = readdir(d))) {
		char id[128], base[128], name[256], exec[512], icon[128], cats[256];
		char mime[2048], wmclass[128], generic[256];
		char *src, *dst, *shim;
		size_t len = strlen(e->d_name);
		FILE *out;

		if (len < 9 || strcmp(e->d_name + len - 8, ".desktop"))
			continue;
		/* base keeps upstream's spelling — it is the desktop-file id and
		 * therefore the app_id the window will announce. id is the
		 * lowercased shim name and is a different thing. */
		snprintf(base, sizeof(base), "%.*s", (int)(len - 8), e->d_name);
		snprintf(id, sizeof(id), "%s", base);
		lower(id);

		src = path_join(dump, e->d_name);
		if (read_file(src, text, 1 << 16) < 0) {
			free(src);
			continue;
		}
		free(src);

		if (!de_get(text, "Name", name, sizeof(name)) ||
		    !de_get(text, "Exec", exec, sizeof(exec)))
			continue;
		if (de_get(text, "NoDisplay", cats, sizeof(cats)) &&
		    !strncmp(cats, "true", 4))
			continue;
		strip_field_codes(exec);
		de_get(text, "Icon", icon, sizeof(icon));
		de_get(text, "Categories", cats, sizeof(cats));
		de_get(text, "MimeType", mime, sizeof(mime));
		de_get(text, "GenericName", generic, sizeof(generic));
		if (!de_get(text, "StartupWMClass", wmclass, sizeof(wmclass)))
			snprintf(wmclass, sizeof(wmclass), "%s", base);

		/* Keeps upstream's id: cosmic-app-list matches a toplevel to an
		 * entry by FILE ID and ignores StartupWMClass, so anything else
		 * leaves every running alien app showing a generic placeholder. */
		dst = xmalloc(MAX_LINE);
		snprintf(dst, MAX_LINE, "%s/%s.desktop", appdir, base);
		if (file_exists(dst)) {         /* already shipped or added */
			free(dst);
			continue;
		}
		out = fopen(dst, "w");
		if (out) {
			fprintf(out, "[Desktop Entry]\nType=Application\n");
			fprintf(out, "Name=%s\n", name);
			fprintf(out, "Comment=%s (alien app, %s box)\n", name, box);
			fprintf(out, "Exec=kdos-appbox run %s\n", exec);
			fprintf(out, "Icon=%s\nTerminal=false\n", icon);
			fprintf(out, "Categories=%s\n", cats);
			if (generic[0])
				fprintf(out, "GenericName=%s\n", generic);
			if (mime[0])
				fprintf(out, "MimeType=%s\n", mime);
			fprintf(out, "StartupWMClass=%s\nX-KDOS-Alien=true\n",
				wmclass);
			fclose(out);
			added++;
		}
		free(dst);

		if (tf)
			fprintf(tf, "%s\t%s\n", id, exec);

		shim = path_join(bindir, id);
		if (!file_exists(shim))
			symlink("/usr/local/bin/kdos-appbox", shim);
		free(shim);
	}
	if (d)
		closedir(d);
	if (tf)
		fclose(tf);

	free(text);
	free(table);
	free(bindir);
	free(appdir);
	free(dump);
	printf("%d new launcher%s\n", added, added == 1 ? "" : "s");
	return 0;
}

static int apt(const char *box, const char *verb, const char *pkg)
{
	Argv a = {0};
	argv_add(&a, "distrobox");
	argv_add(&a, "enter");
	argv_add(&a, box);
	argv_add(&a, "--");
	argv_add(&a, "sudo");
	argv_add(&a, "apt-get");
	argv_add(&a, verb);
	argv_add(&a, "-y");
	if (!strcmp(verb, "install"))
		argv_add(&a, "--no-install-recommends");
	argv_add(&a, pkg);
	argv_end(&a);
	return run_quiet(&a);
}

int app_install(const char *box, const char *pkg)
{
	if (box_ensure(box) != 0)
		return 1;
	printf("installing %s into %s...\n", pkg, box);
	if (apt(box, "install", pkg) != 0) {
		warn("apt-get install %s failed — the appbox is offline unless "
		     "this machine has network", pkg);
		return 1;
	}
	return app_refresh(box);
}

int app_uninstall(const char *box, const char *pkg)
{
	if (!box_exists(box)) {
		warn("box '%s' does not exist", box);
		return 1;
	}
	printf("removing %s from %s...\n", pkg, box);
	if (apt(box, "remove", pkg) != 0) {
		warn("apt-get remove %s failed", pkg);
		return 1;
	}
	/* Launchers for what is gone are left for `kdos-appbox prune`: an
	 * apt remove can pull a dependency that another app still needs, and
	 * deleting launchers on the way out has removed working apps before. */
	return 0;
}

int app_table_load(App **out)
{
	char *buf = xmalloc(1 << 18);
	char *ut = user_table();
	const char *paths[2];
	App *apps = NULL;
	int n = 0, cap = 0, i;

	paths[0] = APP_TABLE;
	paths[1] = ut;
	for (i = 0; i < 2; i++) {
		char *line, *save;
		if (read_file(paths[i], buf, 1 << 18) < 0)
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
			if (n == cap) {
				cap = cap ? cap * 2 : 64;
				apps = realloc(apps, (size_t)cap * sizeof(*apps));
				if (!apps)
					die("out of memory");
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
