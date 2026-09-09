/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The routes — the reader for /etc/kdos/menu.conf
 *
 * `/etc/kdos/menu.conf` then `~/.config/kdos/menu.conf`. A route named in both
 * is the user's; one named only in theirs is added. There is no delete, and
 * that is the point: a name a script may hold has to keep resolving.
 *
 * The value is an argument vector split on blanks and run without a shell.
 * There is no quoting and there will not be: a route that needed a shell would
 * be a route a menu file could run anything with, and this file merges a copy
 * the user owns over the system's.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "routes.h"

static struct sh_route routes[SH_ROUTE_MAX];
static int nroutes;
static int loaded;

/* One route, replacing a route of the same name rather than repeating it: the
 * user's file is read second, and a name that resolved twice would resolve to
 * whichever copy a search reached first. */
static void route_put(const char *name, const char *value)
{
	struct sh_route *r = NULL;
	int slot = nroutes;

	for (int i = 0; i < nroutes; i++)
		if (!strcmp(routes[i].name, name)) {
			slot = i;
			break;
		}
	if (slot >= SH_ROUTE_MAX)
		return;
	r = &routes[slot];
	memset(r, 0, sizeof(*r));
	snprintf(r->name, sizeof(r->name), "%s", name);
	snprintf(r->cmd, sizeof(r->cmd), "%s", value);
	snprintf(r->store, sizeof(r->store), "%s", value);

	/* Split on runs of blanks, in place. No quoting: a route that needed a
	 * shell would be a route a menu file could run anything with, and this
	 * file merges a copy the user owns over the system's. */
	int n = 0;
	char *p = r->store;

	while (*p && n < (int)(sizeof(r->argv) / sizeof(r->argv[0])) - 1) {
		while (*p == ' ' || *p == '\t')
			*p++ = '\0';
		if (!*p)
			break;
		r->argv[n++] = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
	}
	if (!n) {			/* a route naming no command at all */
		memset(r, 0, sizeof(*r));
		return;
	}
	r->argv[n] = NULL;
	r->nargv = n;
	if (slot == nroutes)
		nroutes++;
}

/*
 * The `@name = value` lines. There are few enough that a linear walk is the
 * whole of the lookup, and a setting named twice is the LATER one for the same
 * reason a route is: the user's file is read second.
 */
static struct { char name[32]; char value[SH_ROUTE_CMD]; } settings[8];
static int nsettings;

static void setting_put(const char *name, const char *value)
{
	int slot = nsettings;

	for (int i = 0; i < nsettings; i++)
		if (!strcmp(settings[i].name, name)) {
			slot = i;
			break;
		}
	if (slot >= (int)(sizeof(settings) / sizeof(settings[0])))
		return;
	kb_strlcpy(settings[slot].name, name, sizeof(settings[0].name));
	kb_strlcpy(settings[slot].value, value, sizeof(settings[0].value));
	if (slot == nsettings)
		nsettings++;
}

const char *sh_route_setting(const char *name)
{
	sh_routes_load();
	for (int i = 0; i < nsettings; i++)
		if (!strcmp(settings[i].name, name))
			return settings[i].value;
	return NULL;
}

static void route_file(const char *path)
{
	size_t len = 0;
	char *buf = kb_read_whole(path, &len);

	if (!buf)
		return;
	for (char *line = strtok(buf, "\r\n"); line;
	     line = strtok(NULL, "\r\n")) {
		char *hash = strchr(line, '#');
		char *eq;

		if (hash)
			*hash = '\0';
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';

		char *name = line, *value = eq + 1;
		char *end;

		while (*name == ' ' || *name == '\t')
			name++;
		end = name + strlen(name);
		while (end > name && (end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';
		while (*value == ' ' || *value == '\t')
			value++;
		end = value + strlen(value);
		while (end > value && (end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';
		/*
		 * A `@name` LINE IS A SETTING ABOUT THE MENU, NOT A ROUTE.
		 *
		 * A setting's value is read by whichever surface asks for it;
		 * a route's is an argument vector that gets RUN. A setting
		 * that fell through to route_put() would be a launchable row
		 * running the first word of its value as a program, and
		 * `preflight.sh` would report that word as a missing command.
		 * `@` cannot begin a route name, so it is the whole of the
		 * distinction and it is greppable.
		 */
		if (*name == '@') {
			if (name[1] && *value)
				setting_put(name + 1, value);
			continue;
		}
		if (*name && *value)
			route_put(name, value);
	}
	free(buf);
}

int sh_routes_load(void)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	char path[512];

	if (loaded)
		return nroutes;
	loaded = 1;
	nroutes = 0;
	route_file("/etc/kdos/menu.conf");
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/menu.conf", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/menu.conf", home);
	else
		return nroutes;
	route_file(path);
	return nroutes;
}

int sh_routes_count(void)
{
	return nroutes;
}

const struct sh_route *sh_route_at(int i)
{
	if (i < 0 || i >= nroutes)
		return NULL;
	return &routes[i];
}
