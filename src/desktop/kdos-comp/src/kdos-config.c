// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDOS-only configuration: ~/.config/kdos/comp.conf, `key = value` lines,
 * PARSED, never sourced. Ported from the pre-fork kdos-comp config.c —
 * only the keys the graft layer owns (crt*, idle_*, wallpaper) are read
 * here; `bind`, `startup` and the rest of the old schema are rc.xml's
 * business now and are skipped silently so an old file does not spam the
 * log.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>

#include "kdos.h"

struct kdos_conf kdos_conf;

static char *
trim(char *s)
{
	while (isspace((unsigned char)*s)) {
		s++;
	}
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) {
		*--e = '\0';
	}
	return s;
}

static bool
config_path(char *buf, size_t len)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg) {
		return snprintf(buf, len, "%s/kdos/comp.conf", xdg) < (int)len;
	}
	const char *home = getenv("HOME");
	if (!home || !*home) {
		return false;
	}
	return snprintf(buf, len, "%s/.config/kdos/comp.conf", home) < (int)len;
}

static void
set_int(const char *where, int lineno, const char *value, int lo, int hi,
		int *out)
{
	char *end = NULL;
	long n = strtol(value, &end, 10);
	if (end == value || *end || n < lo || n > hi) {
		wlr_log(WLR_ERROR, "%s:%d: expected a number in %d..%d",
			where, lineno, lo, hi);
		return;
	}
	*out = (int)n;
}

/*
 * The pre-fork schema put the binding IN the key — `bind Super+Return = spawn
 * foot` — so the key of a line worth naming is `bind Super+Return`, not
 * `bind`. An exact compare here matched nothing anyone had actually written.
 */
static bool
first_word_is(const char *key, const char *word)
{
	size_t n = strlen(word);
	return !strncmp(key, word, n) && (key[n] == '\0' || key[n] == ' '
		|| key[n] == '\t');
}

static void
conf_line(const char *key, char *value, const char *path, int lineno)
{
	struct kdos_conf *c = &kdos_conf;

	if (!strcmp(key, "crt") || !strcmp(key, "crt_scanlines")
			|| !strcmp(key, "crt_curve")) {
		int *out = !strcmp(key, "crt") ? &c->crt
			: !strcmp(key, "crt_scanlines") ? &c->crt_scanlines
			: &c->crt_curve;
		set_int(path, lineno, value, 0, 100, out);
	} else if (!strcmp(key, "idle_dim") || !strcmp(key, "idle_lock")
			|| !strcmp(key, "idle_off")) {
		/*
		 * Seconds; 0 means never. A day is the ceiling — larger is a
		 * typo. idle_configured records that the user has an opinion:
		 * the VM default is off, and overriding "off" must include a
		 * 0 that is meant.
		 */
		int *out = !strcmp(key, "idle_dim") ? &c->idle_dim_s
			: !strcmp(key, "idle_lock") ? &c->idle_lock_s
			: &c->idle_off_s;
		set_int(path, lineno, value, 0, 86400, out);
		c->idle_configured = true;
	} else if (!strcmp(key, "wallpaper")) {
		snprintf(c->wallpaper, sizeof(c->wallpaper), "%s", value);
	} else if (first_word_is(key, "bind") || first_word_is(key, "startup")
			|| first_word_is(key, "workspaces")
			|| first_word_is(key, "mouse")) {
		/*
		 * The pre-fork schema. Saying where it went beats both
		 * alternatives: silence leaves a keybind that stopped working
		 * with nothing to read, and honouring it would be a second
		 * place to configure what rc.xml already owns.
		 */
		wlr_log(WLR_INFO, "%s:%d: `%s` is rc.xml's since the labwc "
			"fork — see ~/.config/kdos-comp/rc.xml; ignored here",
			path, lineno, key);
	} else {
		/*
		 * A typo used to be indistinguishable from a setting that had
		 * no effect, and this file's own comment promises otherwise.
		 */
		wlr_log(WLR_INFO, "%s:%d: unknown key `%s` — ignored",
			path, lineno, key);
	}
}

void
kdos_conf_load(void)
{
	struct kdos_conf *c = &kdos_conf;

	/*
	 * Defaults. The CRT pass is ON at the pre-fork shipped strength —
	 * it is the reason the boot splash, the TTY and the desktop look
	 * like one machine. Curvature stays off: the one effect that
	 * argues with a character grid. kdos_crt_init() still forces 0 on
	 * a renderer that is not GLES2.
	 */
	c->crt = 55;
	c->crt_scanlines = 60;
	c->crt_curve = 0;
	c->idle_dim_s = 300;
	c->idle_lock_s = 600;
	c->idle_off_s = 900;
	c->idle_configured = false;
	snprintf(c->wallpaper, sizeof(c->wallpaper), "%s",
		"/usr/share/backgrounds/kdos/default-wallpaper.png");

	char path[512];
	if (!config_path(path, sizeof(path))) {
		return;
	}
	FILE *f = fopen(path, "r");
	if (!f) {
		return; /* absent is the normal case, not an error */
	}

	char line[1024];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;
		char *p = trim(line);
		/* a comment is a `#` that STARTS the line */
		if (!*p || *p == '#') {
			continue;
		}
		char *eq = strchr(p, '=');
		if (!eq) {
			wlr_log(WLR_ERROR, "%s:%d: not `key = value`",
				path, lineno);
			continue;
		}
		*eq = '\0';
		char *key = trim(p);
		char *value = trim(eq + 1);
		if (!*value) {
			continue;
		}
		conf_line(key, value, path, lineno);
	}
	fclose(f);
}
