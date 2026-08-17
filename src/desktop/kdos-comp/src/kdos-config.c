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
#include <strings.h>
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

/* Returns false when the value was refused: `idle_configured` is the
 * caller that must not be set by a line this rejected. */
static bool
set_int(const char *where, int lineno, const char *value, int lo, int hi,
		int *out)
{
	char *end = NULL;
	long n = strtol(value, &end, 10);
	if (end == value || *end || n < lo || n > hi) {
		wlr_log(WLR_ERROR, "%s:%d: expected a number in %d..%d",
			where, lineno, lo, hi);
		return false;
	}
	*out = (int)n;
	return true;
}

/*
 * yes/no, true/false, on/off, 1/0. A value that is none of those is reported
 * rather than guessed at: this file's own comment promises that a line which
 * does not take effect says so.
 */
static void
set_bool(const char *where, int lineno, const char *value, bool *out)
{
	static const char *const yes[] = { "yes", "true", "on", "1", NULL };
	static const char *const no[] = { "no", "false", "off", "0", NULL };

	for (int i = 0; yes[i]; i++) {
		if (!strcasecmp(value, yes[i])) {
			*out = true;
			return;
		}
	}
	for (int i = 0; no[i]; i++) {
		if (!strcasecmp(value, no[i])) {
			*out = false;
			return;
		}
	}
	wlr_log(WLR_ERROR, "%s:%d: expected yes or no", where, lineno);
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

/* A path value may start `~/` or `$HOME/` — the file is hand-written and
 * both spellings are what people write. Anything else passes through. */
static void
set_path(char *dst, size_t len, const char *value)
{
	const char *rest = NULL;
	if (!strncmp(value, "~/", 2)) {
		rest = value + 1;
	} else if (!strncmp(value, "$HOME/", 6)) {
		rest = value + 5;
	}
	if (rest) {
		const char *home = getenv("HOME");
		if (home && *home) {
			snprintf(dst, len, "%s%s", home, rest);
			return;
		}
	}
	snprintf(dst, len, "%s", value);
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
		/* only a line that PARSED is an opinion: `idle_dim = 5m` is
		 * refused above, and marking the user configured on the
		 * strength of it re-arms the timers the VM gate turned off */
		if (set_int(path, lineno, value, 0, 86400, out)) {
			c->idle_configured = true;
		}
	} else if (!strcmp(key, "lid_close")) {
		if (!strcasecmp(value, "off")) {
			c->lid_close = KDOS_LID_OFF;
		} else if (!strcasecmp(value, "lock")) {
			c->lid_close = KDOS_LID_LOCK;
		} else if (!strcasecmp(value, "suspend")) {
			c->lid_close = KDOS_LID_SUSPEND;
		} else {
			wlr_log(WLR_ERROR, "%s:%d: expected off, lock or "
				"suspend", path, lineno);
			return;
		}
		c->lid_configured = true;
	} else if (!strcmp(key, "crt_fullscreen")) {
		set_bool(path, lineno, value, &c->crt_fullscreen);
	} else if (!strcmp(key, "clock_format")) {
		snprintf(c->clock_format, sizeof(c->clock_format), "%s", value);
	} else if (!strcmp(key, "wallpaper")) {
		set_path(c->wallpaper, sizeof(c->wallpaper), value);
	} else if (!strcmp(key, "chrome_font")) {
		snprintf(c->chrome_font, sizeof(c->chrome_font), "%s", value);
	} else if (!strcmp(key, "panel")) {
		if (!strcasecmp(value, "bottom")) {
			c->panel_edge = KDOS_PANEL_BOTTOM;
		} else if (!strcasecmp(value, "top")) {
			c->panel_edge = KDOS_PANEL_TOP;
		} else if (!strcasecmp(value, "off")
				|| !strcasecmp(value, "none")) {
			c->panel_edge = KDOS_PANEL_OFF;
		} else {
			wlr_log(WLR_ERROR, "%s:%d: expected bottom, top or off",
				path, lineno);
		}
	} else if (!strcmp(key, "panel_cells")) {
		set_int(path, lineno, value, 1, 4, &c->panel_cells);
	} else if (!strcmp(key, "icons")) {
		set_bool(path, lineno, value, &c->icons);
	} else if (!strcmp(key, "slit")) {
		set_bool(path, lineno, value, &c->slit);
	} else if (!strcmp(key, "clipboard")) {
		set_bool(path, lineno, value, &c->clipboard);
	} else if (!strcmp(key, "panel_bottom")) {
		/*
		 * Retired with the two-panel layout. Reported by name rather
		 * than ignored: the shipped comp.conf promises that a line
		 * which does not take effect says so, and a key that silently
		 * stopped meaning anything is indistinguishable from a typo.
		 */
		wlr_log(WLR_INFO, "%s:%d: `panel_bottom` is retired — there is "
			"one taskbar now; use `panel = bottom|top|off`",
			path, lineno);
	} else if (!strcmp(key, "desktop_icons")) {
		set_bool(path, lineno, value, &c->desktop_icons);
	} else if (!strcmp(key, "panel_autohide")) {
		set_bool(path, lineno, value, &c->panel_autohide);
	} else if (!strcmp(key, "window_memory")) {
		set_bool(path, lineno, value, &c->window_memory);
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
	c->crt_fullscreen = true;
	c->idle_dim_s = 300;
	c->idle_lock_s = 600;
	c->idle_off_s = 900;
	c->idle_configured = false;
	c->lid_close = KDOS_LID_SUSPEND;	/* kdos_lid_init() gates the VM */
	c->lid_configured = false;
	c->panel_edge = KDOS_PANEL_BOTTOM;
	c->panel_cells = 2;
	c->icons = true;
	c->slit = false;
	c->clipboard = true;
	c->desktop_icons = true;
	c->panel_autohide = false;
	c->window_memory = true;
	c->chrome_font[0] = '\0';	/* libkwl's Terminus:pixelsize=32 */
	c->clock_format[0] = '\0';	/* the panel's own %H:%M */
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
			/* the file's own comment promises that a line which
			 * does not take effect says so */
			wlr_log(WLR_INFO, "%s:%d: empty value for `%s` — "
				"ignored; use e.g. `wallpaper = none`",
				path, lineno, key);
			continue;
		}
		conf_line(key, value, path, lineno);
	}
	fclose(f);
}

/*
 * SIGHUP/Reconfigure. The CRT knobs are per-frame uniforms and the lid
 * policy is read at event time, so re-parsing IS applying them; idle and
 * wallpaper need their modules told. The chrome keys are a child's argv
 * and stay what they were — logged, because a key that silently does
 * nothing is the bug this file's comments keep promising away.
 */
void
kdos_conf_reload(void)
{
	struct kdos_conf old = kdos_conf;
	kdos_conf_load();

	if (old.panel_edge != kdos_conf.panel_edge
			|| old.panel_cells != kdos_conf.panel_cells
			|| old.icons != kdos_conf.icons
			|| old.slit != kdos_conf.slit
			|| old.clipboard != kdos_conf.clipboard
			|| old.desktop_icons != kdos_conf.desktop_icons
			|| old.panel_autohide != kdos_conf.panel_autohide) {
		wlr_log(WLR_INFO, "comp.conf: panel/panel_cells/icons/slit/"
			"desktop_icons/panel_autohide changed — applies at the "
			"next login");
		kdos_conf.panel_edge = old.panel_edge;
		kdos_conf.panel_cells = old.panel_cells;
		kdos_conf.icons = old.icons;
		kdos_conf.slit = old.slit;
		kdos_conf.clipboard = old.clipboard;
		kdos_conf.desktop_icons = old.desktop_icons;
		kdos_conf.panel_autohide = old.panel_autohide;
	}
	if (strcmp(old.chrome_font, kdos_conf.chrome_font)) {
		wlr_log(WLR_INFO, "comp.conf: chrome_font changed — applies "
			"at the next login");
		snprintf(kdos_conf.chrome_font, sizeof(kdos_conf.chrome_font),
			"%s", old.chrome_font);
	}
	if (strcmp(old.clock_format, kdos_conf.clock_format)) {
		wlr_log(WLR_INFO, "comp.conf: clock_format changed — applies "
			"at the next login");
		snprintf(kdos_conf.clock_format, sizeof(kdos_conf.clock_format),
			"%s", old.clock_format);
	}

	kdos_idle_reconfigure();
	kdos_lid_reconfigure();
	kdos_wallpaper_reload();
}

/* The accent, from the same one-word file kdos-shell reads. Absent is
 * the normal case on a fresh system and means phosphor. */
const KcolScheme *
kdos_accent_scheme(void)
{
	char path[512];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	if (cache && *cache) {
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	} else if (home && *home) {
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	} else {
		return kcol_find("phosphor");
	}

	const KcolScheme *sc = NULL;
	FILE *f = fopen(path, "r");
	if (f) {
		char name[64] = { 0 };
		if (fgets(name, sizeof(name), f)) {
			char *nl = strpbrk(name, "\r\n");
			if (nl) {
				*nl = '\0';
			}
			sc = kcol_find(name);
		}
		fclose(f);
	}
	return sc ? sc : kcol_find("phosphor");
}
