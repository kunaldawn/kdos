/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ~/.config/kdos/comp.conf — PARSED, never sourced
 *
 * The same rule kpkgbuild and kdos-appbox's box profiles already follow: a
 * config file is data. Nothing here evaluates, expands or substitutes, and the
 * value of a `spawn` binding is split on whitespace into argv and exec'd
 * directly. There is no shell in kdos-comp and this file is where one would
 * otherwise creep in — a compositor that runs its config through /bin/sh turns
 * every keybinding into an injection point for anything that can write a file
 * in the user's home.
 *
 * Consequence, stated rather than worked around: an argv word cannot contain a
 * space. Adding quotes would be the first half of a shell, and the second half
 * always follows. Wrap it in a script and spawn that.
 *
 * Format is flat `key = value`, like every other KDOS config:
 *
 *     repeat_rate       = 25
 *     repeat_delay      = 600
 *     snap_px           = 16
 *     bind Super+Return = spawn foot
 *     bind Super+Q      = close
 *     bind Super+1      = workspace 1
 *
 * A `bind` line whose value does not parse is reported and dropped; the rest of
 * the file still loads. A config with one typo in it must not cost the user
 * every other key they bound.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-comp.h"

/* ── small helpers ─────────────────────────────────────────────────────── */

char *kc_trim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		e--;
	*e = '\0';
	return s;
}

/* Split on whitespace into a NULL-terminated argv. No quoting — see above. */
static char **split_argv(const char *value)
{
	size_t cap = 8, n = 0;
	char **argv = calloc(cap, sizeof(*argv));
	if (!argv)
		return NULL;

	const char *p = value;
	while (*p) {
		while (*p && isspace((unsigned char)*p))
			p++;
		if (!*p)
			break;
		const char *start = p;
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (n + 2 > cap) {
			cap *= 2;
			char **grown = realloc(argv, cap * sizeof(*argv));
			if (!grown)
				goto fail;
			argv = grown;
			memset(argv + n, 0, (cap - n) * sizeof(*argv));
		}
		argv[n] = strndup(start, (size_t)(p - start));
		if (!argv[n])
			goto fail;
		n++;
	}
	if (n == 0)
		goto fail;
	argv[n] = NULL;
	return argv;
fail:
	for (size_t i = 0; i < n; i++)
		free(argv[i]);
	free(argv);
	return NULL;
}

static void free_argv(char **argv)
{
	if (!argv)
		return;
	for (char **p = argv; *p; p++)
		free(*p);
	free(argv);
}

/* ── binding parsing ───────────────────────────────────────────────────── */

static uint32_t modifier_from_name(const char *name)
{
	if (!strcasecmp(name, "super") || !strcasecmp(name, "logo") ||
	    !strcasecmp(name, "mod4"))
		return WLR_MODIFIER_LOGO;
	if (!strcasecmp(name, "shift"))
		return WLR_MODIFIER_SHIFT;
	if (!strcasecmp(name, "ctrl") || !strcasecmp(name, "control"))
		return WLR_MODIFIER_CTRL;
	if (!strcasecmp(name, "alt") || !strcasecmp(name, "mod1"))
		return WLR_MODIFIER_ALT;
	return 0;
}

/*
 * "Super+Shift+Return" -> mods + keysym.
 *
 * The keysym is resolved case-insensitively, which is what makes `Super+Q` and
 * `Super+q` the same binding — and it has to be, because bindings are matched
 * against the UNSHIFTED keysym (see keyboard_key), so a keymap never produces
 * an upper-case `Q` for one to match.
 */
static bool parse_combo(char *spec, uint32_t *mods, xkb_keysym_t *sym)
{
	*mods = 0;
	char *save = NULL;
	char *last = NULL;
	for (char *tok = strtok_r(spec, "+", &save); tok;
	     tok = strtok_r(NULL, "+", &save)) {
		tok = kc_trim(tok);
		if (!*tok)
			return false;
		if (last) {
			uint32_t m = modifier_from_name(last);
			if (!m)
				return false;
			*mods |= m;
		}
		last = tok;
	}
	if (!last)
		return false;

	*sym = xkb_keysym_from_name(last, XKB_KEYSYM_CASE_INSENSITIVE);
	return *sym != XKB_KEY_NoSymbol;
}

/*
 * The XF86 vendor keysym range. XKB assigns 0x1008FFxx to the media and laptop
 * function keys; nothing in it is a character, which is what makes a binding
 * without a modifier safe there and nowhere else.
 */
static bool sym_is_vendor_key(xkb_keysym_t sym)
{
	return sym >= 0x10080000 && sym <= 0x1008FFFF;
}

static void bind_free(struct kc_bind *b)
{
	free_argv(b->argv);
	free(b);
}

/* Replace any existing binding for the same combo, so the user's file
 * overrides a default rather than shadowing it with a duplicate whose
 * precedence depends on list order. */
static void bind_add(struct kc_server *s, struct kc_bind *nb)
{
	struct kc_bind *b, *tmp;
	wl_list_for_each_safe(b, tmp, &s->binds, link) {
		if (b->mods == nb->mods && b->sym == nb->sym) {
			wl_list_remove(&b->link);
			bind_free(b);
			break;
		}
	}
	wl_list_insert(&s->binds, &nb->link);
}

/*
 * Returns 1 on success, 0 for a verb we do not know, and -1 for a verb we DO
 * know used wrongly — which has already reported its own specific reason.
 *
 * The three-way split exists so that `workspace 9` is not reported as an
 * unknown action: the verb was understood, and blaming it sends the user
 * looking in exactly the wrong place.
 */
static int parse_action(struct kc_bind *b, char *value, const char *where,
			int lineno)
{
	char *arg = value;
	while (*arg && !isspace((unsigned char)*arg))
		arg++;
	bool has_arg = *arg != '\0';
	if (has_arg)
		*arg++ = '\0';
	arg = kc_trim(arg);

	if (!strcmp(value, "spawn")) {
		if (!has_arg || !(b->argv = split_argv(arg))) {
			wlr_log(WLR_ERROR, "%s:%d: spawn needs a command",
				where, lineno);
			return -1;
		}
		b->action = KC_ACT_SPAWN;
		return 1;
	}
	if (!strcmp(value, "close")) {
		b->action = KC_ACT_CLOSE;
		return 1;
	}
	if (!strcmp(value, "quit")) {
		b->action = KC_ACT_QUIT;
		return 1;
	}
	if (!strcmp(value, "cycle")) {
		b->action = KC_ACT_CYCLE;
		return 1;
	}
	if (!strcmp(value, "cycle-back")) {
		b->action = KC_ACT_CYCLE_BACK;
		return 1;
	}
	if (!strcmp(value, "workspace") || !strcmp(value, "move-to-workspace")) {
		/* Workspaces are written 1-based because that is what is printed
		 * on the key, and stored 0-based because that is what indexes
		 * ws_tree. */
		char *end = NULL;
		long n = has_arg ? strtol(arg, &end, 10) : 0;
		if (!has_arg || end == arg || *end || n < 1 || n > KC_WORKSPACES) {
			wlr_log(WLR_ERROR, "%s:%d: %s takes a workspace 1..%d",
				where, lineno, value, KC_WORKSPACES);
			return -1;
		}
		b->action = !strcmp(value, "workspace") ? KC_ACT_WORKSPACE
							: KC_ACT_MOVE_TO_WORKSPACE;
		b->arg = (int)n - 1;
		return 1;
	}
	return 0;
}

static void parse_bind(struct kc_server *s, char *combo, char *value,
		       const char *where, int lineno)
{
	struct kc_bind *b = calloc(1, sizeof(*b));
	if (!b)
		return;

	if (!parse_combo(combo, &b->mods, &b->sym)) {
		wlr_log(WLR_ERROR, "%s:%d: unknown key combination", where, lineno);
		bind_free(b);
		return;
	}
	/*
	 * A binding with no modifier swallows a bare key from every client for
	 * the whole session — bind `q` and no text field can ever contain one.
	 * Refusing it is not paternalism: there is no way to notice the mistake
	 * from inside the session and no way to undo it without a config editor
	 * you can no longer type into.
	 *
	 * The XF86 vendor range is the one exception, and it is not a loophole:
	 * those keysyms exist ONLY on dedicated keys — volume, brightness,
	 * media transport — and no application ever receives one as text. A
	 * volume key that required a modifier would not be a volume key.
	 */
	if (!(b->mods & KC_MOD_MASK) && !sym_is_vendor_key(b->sym)) {
		wlr_log(WLR_ERROR, "%s:%d: binding has no modifier — refused",
			where, lineno);
		bind_free(b);
		return;
	}
	int rc = parse_action(b, value, where, lineno);
	if (rc <= 0) {
		if (rc == 0)
			wlr_log(WLR_ERROR, "%s:%d: unknown action `%s`", where,
				lineno, value);
		bind_free(b);
		return;
	}
	bind_add(s, b);
}

/* ── defaults ──────────────────────────────────────────────────────────── */

static void add_default(struct kc_server *s, const char *combo, const char *action)
{
	char cbuf[64], abuf[128];
	snprintf(cbuf, sizeof(cbuf), "%s", combo);
	snprintf(abuf, sizeof(abuf), "%s", action);
	parse_bind(s, cbuf, abuf, "<defaults>", 0);
}

static void load_defaults(struct kc_server *s)
{
	/* Super, not Alt: Alt belongs to the applications. */
	add_default(s, "Super+Return", "spawn foot");
	add_default(s, "Super+d", "spawn kdos-launcher");
	/* The media keys. No modifier by design — these keysyms exist ONLY on
	 * those keys, so they cannot collide with anything a client wants, and
	 * the no-modifier refusal in parse_bind() is about ordinary letters. */
	add_default(s, "XF86AudioRaiseVolume", "spawn kdos-osd volume +5");
	add_default(s, "XF86AudioLowerVolume", "spawn kdos-osd volume -5");
	add_default(s, "XF86AudioMute", "spawn kdos-osd volume toggle");
	add_default(s, "XF86MonBrightnessUp", "spawn kdos-osd brightness +10");
	add_default(s, "XF86MonBrightnessDown", "spawn kdos-osd brightness -10");
	add_default(s, "Super+q", "close");
	add_default(s, "Super+Escape", "quit");
	add_default(s, "Super+Tab", "cycle");
	add_default(s, "Super+Shift+Tab", "cycle-back");
	for (int i = 1; i <= KC_WORKSPACES; i++) {
		char combo[32], action[32];
		snprintf(combo, sizeof(combo), "Super+%d", i);
		snprintf(action, sizeof(action), "workspace %d", i);
		add_default(s, combo, action);
		snprintf(combo, sizeof(combo), "Super+Shift+%d", i);
		snprintf(action, sizeof(action), "move-to-workspace %d", i);
		add_default(s, combo, action);
	}
}

/* ── the file ──────────────────────────────────────────────────────────── */

static bool config_path(char *buf, size_t len)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg)
		return snprintf(buf, len, "%s/kdos/comp.conf", xdg) < (int)len;
	const char *home = getenv("HOME");
	if (!home || !*home)
		return false;
	return snprintf(buf, len, "%s/.config/kdos/comp.conf", home) < (int)len;
}

static void set_int(const char *where, int lineno, const char *value, int lo,
		    int hi, int *out)
{
	char *end = NULL;
	long n = strtol(value, &end, 10);
	if (end == value || *end || n < lo || n > hi) {
		wlr_log(WLR_ERROR, "%s:%d: expected a number in %d..%d", where,
			lineno, lo, hi);
		return;
	}
	*out = (int)n;
}

static void comp_conf_line(const char *key, char *value, const char *path,
			   int lineno, void *user)
{
	struct kc_server *s = user;

	if (!strncmp(key, "bind", 4) && isspace((unsigned char)key[4])) {
		char combo[128];
		snprintf(combo, sizeof(combo), "%s", key + 4);
		parse_bind(s, kc_trim(combo), value, path, lineno);
	} else if (!strcmp(key, "startup")) {
		/*
		 * Repeatable, and the FIRST one in the user's file replaces the
		 * default rather than adding to it — otherwise there is no way
		 * to stop kdos-shell from starting, only ways to add more.
		 */
		if (!s->startup_overridden) {
			for (int i = 0; i < s->nstartup; i++)
				free_argv(s->startup[i]);
			s->nstartup = 0;
			s->startup_overridden = 1;
		}
		if (s->nstartup >= KC_MAX_STARTUP) {
			wlr_log(WLR_ERROR, "%s:%d: too many startup entries",
				path, lineno);
			return;
		}
		char **argv = split_argv(value);
		if (argv)
			s->startup[s->nstartup++] = argv;
	} else if (!strcmp(key, "repeat_rate")) {
		int n = s->repeat_rate;
		set_int(path, lineno, value, 0, 1000, &n);
		s->repeat_rate = n;
	} else if (!strcmp(key, "repeat_delay")) {
		int n = s->repeat_delay;
		set_int(path, lineno, value, 0, 10000, &n);
		s->repeat_delay = n;
	} else if (!strcmp(key, "snap_px")) {
		/* 0 disables snapping. A large value would snap from the middle
		 * of the screen, so it is capped rather than trusted. */
		set_int(path, lineno, value, 0, 200, &s->snap_px);
	} else {
		wlr_log(WLR_ERROR, "%s:%d: unknown key `%s`", path, lineno, key);
	}
}

void kc_config_load(struct kc_server *s)
{
	wl_list_init(&s->binds);
	s->repeat_rate = 25;
	s->repeat_delay = 600;
	s->snap_px = 16;
	/* The shell, not a terminal. A desktop that opens a terminal at login
	 * is a development stopgap, and M1's was exactly that. */
	s->startup[0] = split_argv("kdos-shell");
	s->nstartup = s->startup[0] ? 1 : 0;
	/* Every alien app expects a notification daemon to exist. Without one,
	 * a GTK app's notify call fails and anything using gdbus waits out its
	 * 25-second default reply timeout first. */
	s->startup[s->nstartup] = split_argv("kdos-notifyd");
	if (s->startup[s->nstartup])
		s->nstartup++;
	load_defaults(s);

	char path[512];
	if (!config_path(path, sizeof(path)))
		return;
	kc_config_read(path, comp_conf_line, s);
}

/*
 * The line reader both KDOS compositor configs share — comp.conf here and the
 * box profiles in security.c. One reader means one answer to what a comment is,
 * what an empty value means and how a malformed line is reported; two would
 * drift, and the two files are read by the same program.
 *
 * `value` is handed over mutable because callers tokenise it in place.
 */
bool kc_config_read(const char *path, kc_kv_fn fn, void *user)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return false;	/* absent is the normal case, not an error */

	char line[1024];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;
		char *p = kc_trim(line);
		/* A comment is a `#` that starts the line. Not "anywhere", so a
		 * spawn argument may contain one. */
		if (!*p || *p == '#')
			continue;

		char *eq = strchr(p, '=');
		if (!eq) {
			wlr_log(WLR_ERROR, "%s:%d: not `key = value`", path, lineno);
			continue;
		}
		*eq = '\0';
		char *key = kc_trim(p);
		char *value = kc_trim(eq + 1);
		if (!*value) {
			wlr_log(WLR_ERROR, "%s:%d: empty value", path, lineno);
			continue;
		}
		fn(key, value, path, lineno, user);
	}
	fclose(f);
	return true;
}

void kc_config_free(struct kc_server *s)
{
	struct kc_bind *b, *tmp;
	wl_list_for_each_safe(b, tmp, &s->binds, link) {
		wl_list_remove(&b->link);
		bind_free(b);
	}
	for (int i = 0; i < s->nstartup; i++)
		free_argv(s->startup[i]);
	s->nstartup = 0;
}
