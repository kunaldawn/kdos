/*
 * kcon_conf — con.conf, read once.
 *
 * Two files, later wins: /etc/kdos/con.conf is the system's answer and
 * ~/.config/kdos-con/con.conf is the user's. A key absent from both takes the
 * caller's default, so a machine with neither file boots a working desktop —
 * a configuration file is how a decision is CHANGED here, never how it is
 * made.
 *
 * Lines are `key = value`; `#` to end of line is a comment. Both readers of
 * this file are in the same session (the session server and its view), so the
 * parse is cached: re-reading per lookup would let a mid-session edit change
 * half the answers and leave the other half stale.
 */
#include "kcon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"

#define CONF_CAP  4096
#define CONF_KEYS 32

static struct {
	int loaded;
	int n;
	struct { char *k, *v; } e[CONF_KEYS];
	char buf[2][CONF_CAP];
} C;

static void conf_parse(char *buf)
{
	char *line, *save;

	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *hash = strchr(line, '#');
		char *eq, *k, *v, *end;

		if (hash)
			*hash = '\0';
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';

		k = line;
		while (*k == ' ' || *k == '\t')
			k++;
		end = k + strlen(k);
		while (end > k && (end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';
		if (!*k)
			continue;

		v = eq + 1;
		while (*v == ' ' || *v == '\t')
			v++;
		end = v + strlen(v);
		while (end > v && (end[-1] == ' ' || end[-1] == '\t' ||
				   end[-1] == '\r'))
			*--end = '\0';

		/* LATER WINS, in place: the user's file overrides the
		 * system's rather than appending a second entry the lookup
		 * would never reach. */
		for (int i = 0; i < C.n; i++) {
			if (!strcmp(C.e[i].k, k)) {
				C.e[i].v = v;
				goto next;
			}
		}
		if (C.n < CONF_KEYS) {
			C.e[C.n].k = k;
			C.e[C.n].v = v;
			C.n++;
		}
next:		;
	}
}

static void conf_load(void)
{
	char path[256];
	const char *xdg, *home;

	if (C.loaded)
		return;
	C.loaded = 1;

	/* `> 0`, not `>= 0`: kb_read_file answers the byte count, and a
	 * zero-byte file must leave every default standing. */
	if (kb_read_file("/etc/kdos/con.conf", C.buf[0], sizeof(C.buf[0])) > 0)
		conf_parse(C.buf[0]);

	xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg) {
		snprintf(path, sizeof(path), "%s/kdos-con/con.conf", xdg);
	} else {
		home = kb_home_dir();
		snprintf(path, sizeof(path), "%s/.config/kdos-con/con.conf",
			 home ? home : "/root");
	}
	if (kb_read_file(path, C.buf[1], sizeof(C.buf[1])) > 0)
		conf_parse(C.buf[1]);
}

const char *kcon_conf_str(const char *key, const char *def)
{
	conf_load();
	for (int i = 0; i < C.n; i++) {
		if (!strcmp(C.e[i].k, key))
			return *C.e[i].v ? C.e[i].v : def;
	}
	return def;
}

int kcon_conf_int(const char *key, int def)
{
	const char *v = kcon_conf_str(key, NULL);

	return v ? atoi(v) : def;
}

int kcon_conf_bool(const char *key, int def)
{
	const char *v = kcon_conf_str(key, NULL);

	if (!v)
		return def;
	/* `yes`/`no` is what the shipped file says, so it is what a user who
	 * copies a line will write; the rest are what they will write anyway. */
	return !strcmp(v, "yes") || !strcmp(v, "true") || !strcmp(v, "on") ||
	       !strcmp(v, "1");
}
