/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos phosphor — the one toggle for Phosphor mode
 *
 *   kdos phosphor on [--preset subtle|retro] [--terminus|--no-terminus]
 *   kdos phosphor off
 *   kdos phosphor status [--json]
 *   kdos phosphor preset subtle|retro
 *   kdos phosphor terminus on|off
 *   kdos phosphor fullscreen on|off
 *   kdos phosphor collapse | unfold | degauss
 *
 * WHAT IT SWITCHES: KWin's built-in `kdos_crt` effect (the CRT pass over the
 * whole screen) and the three motion effects that replace KWin's `login` and
 * `sessionquit`; the phosphor accent in place of bone or paper, which are the
 * two that do not read as a CRT (B19); with the Terminus option, the fixed
 * face and a Terminus Konsole profile; and the boot splash's motion. Off by
 * default on every new user (D2), and no chord.
 *
 * OFF PUTS BACK WHAT ON FOUND, KEY FOR KEY. Before it changes anything, `on`
 * writes $XDG_CONFIG_HOME/kdos/phosphor: the raw value of every key it is
 * about to change AS THE PERSON'S OWN FILE HAD IT — never the cascaded value
 * — under [Saved], or the key's name under [Unset] when the file had none.
 * `off` writes each back through kwriteconfig6, so a file KConfig last wrote
 * comes back byte for byte. A toggle that restored the cascaded value instead
 * would leave a copy of /etc/xdg's choice in the person's file, where it would
 * outrank every later change to the machine's default.
 *
 * SOFTWARE RENDERING IS REFUSED BEFORE ANYTHING IS WRITTEN. The effect's
 * supported() is false on llvmpipe, where a full-screen pass per frame would
 * be the desktop's whole budget; `on` asks KWin's answer first, and its
 * refusal leaves no state file and no key behind. The option verbs (`preset`,
 * `terminus`, `fullscreen`) refuse nothing: each records its option, which
 * takes effect at once while the mode is on and at the next `on` otherwise.
 * `status --json` names a refusal twice: "reason", the sentence `on` would
 * print, and "refusal", "session" or "gpu" (exit status 2 or 3), for a
 * front end that says it in its own words.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"

#define KWIN "org.kde.KWin"
#define EFFECTS_IFACE "org.kde.kwin.Effects"
#define CRT_PATH "/io/kdos/Crt1"
#define CRT_IFACE "io.kdos.Crt1"

static const char REFUSE_SESSION[] =
	"phosphor mode needs a running Plasma session";
static const char REFUSE_GPU[] =
	"phosphor mode needs GPU rendering (this session renders in software)";

/* The four KDOS effects, kdos_crt first: it is the one that can refuse, and
 * the motion effects mean nothing without the pass. */
static const char *const OURS[] = { "kdos_crt", "kdos_unfold", "kdos_login",
				    "kdos_quit" };
/* KWin's own two that ours replace. */
static const char *const THEIRS[] = { "login", "sessionquit" };

/* The presets are values, not a name stored anywhere (5.4.2): the effect's
 * own page edits the same three keys, and a fourth key naming the preset
 * would disagree with them the first time somebody moved a slider. */
static const struct {
	const char *name;
	int strength, scanlines, curve;
} PRESETS[] = {
	{ "subtle", 55, 0, 0 },
	{ "retro", 55, 60, 30 },
};

/* ── the state file ────────────────────────────────────────────────────── */

typedef struct {
	char *sec, *key, *val;
} Row;

typedef struct {
	Row *r;
	int n, cap;
} State;

static char *state_path(void)
{
	return kdt_cfg_home("kdos/phosphor");
}

static void st_free(State *s)
{
	for (int i = 0; i < s->n; i++) {
		free(s->r[i].sec);
		free(s->r[i].key);
		free(s->r[i].val);
	}
	free(s->r);
	memset(s, 0, sizeof(*s));
}

static Row *st_find(State *s, const char *sec, const char *key)
{
	for (int i = 0; i < s->n; i++)
		if (!strcmp(s->r[i].sec, sec) && !strcmp(s->r[i].key, key))
			return &s->r[i];
	return NULL;
}

static const char *st_get(State *s, const char *sec, const char *key)
{
	Row *r = st_find(s, sec, key);

	return r ? r->val : NULL;
}

static void st_set(State *s, const char *sec, const char *key,
		   const char *val)
{
	Row *r = st_find(s, sec, key);

	if (r) {
		free(r->val);
		r->val = kb_strdup(val);
		return;
	}
	if (s->n == s->cap) {
		s->cap = s->cap ? s->cap * 2 : 16;
		s->r = kb_realloc(s->r, (size_t)s->cap * sizeof(*s->r));
	}
	s->r[s->n].sec = kb_strdup(sec);
	s->r[s->n].key = kb_strdup(key);
	s->r[s->n].val = kb_strdup(val);
	s->n++;
}

static void st_del(State *s, const char *sec, const char *key)
{
	for (int i = 0; i < s->n; i++) {
		if (strcmp(s->r[i].sec, sec) ||
		    (key && strcmp(s->r[i].key, key)))
			continue;
		free(s->r[i].sec);
		free(s->r[i].key);
		free(s->r[i].val);
		s->r[i] = s->r[--s->n];
		i--;
	}
}

/* The file is ours and written whole; a value is stored exactly as the line
 * held it, so a saved raw KConfig value round-trips untouched. Keys hold a
 * `/` and a group may hold `][`, so a key runs to the first `=`. */
static void st_load(State *s)
{
	char *p = state_path(), *data = kb_read_all(p, NULL);
	char sec[128] = "";

	free(p);
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n'), *eq;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		if (line[0] == '[') {
			char *end = strrchr(line, ']');

			if (end && end > line + 1) {
				*end = '\0';
				kb_strlcpy(sec, line + 1, sizeof(sec));
			}
			continue;
		}
		if (!*sec || line[0] == '#' || !(eq = strchr(line, '=')))
			continue;
		*eq = '\0';
		st_set(s, sec, line, eq + 1);
	}
	free(data);
}

static int st_save(State *s)
{
	static const char *const ORDER[] = { "Settings", "State", "Saved",
					     "Unset" };
	char *p = state_path();
	KbBuf b = { 0 };
	int rc;

	for (size_t o = 0; o < sizeof(ORDER) / sizeof(ORDER[0]); o++) {
		int head = 0;

		for (int i = 0; i < s->n; i++) {
			if (strcmp(s->r[i].sec, ORDER[o]))
				continue;
			if (!head) {
				kb_buf_printf(&b, "%s[%s]\n", b.n ? "\n" : "",
					      ORDER[o]);
				head = 1;
			}
			kb_buf_printf(&b, "%s=%s\n", s->r[i].key, s->r[i].val);
		}
	}
	kdt_mkparent(p);
	rc = kb_write_file_atomic(p, b.n ? b.p : "");
	kb_buf_free(&b);
	free(p);
	return rc;
}

static int st_on(State *s)
{
	return kdt_kcfg_bool(st_get(s, "State", "On"), 0);
}

static int st_terminus(State *s)
{
	return kdt_kcfg_bool(st_get(s, "Settings", "Terminus"), 0);
}

int kdt_phosphor_terminus_active(void)
{
	State s = { 0 };
	int on;

	st_load(&s);
	on = st_on(&s) && st_terminus(&s);
	st_free(&s);
	return on;
}

/* ── saving and putting back one key ───────────────────────────────────── */

static void spec(char *out, size_t n, const char *file, const char *group,
		 const char *key)
{
	snprintf(out, n, "%s/%s/%s", file, group, key);
}

/* Record what the person's own file holds for this key, once: a key already
 * recorded keeps the value from before the FIRST change, which is the one
 * `off` must put back. */
static void save_key(State *s, const char *file, const char *group,
		     const char *key)
{
	char id[256], *path, *raw;

	spec(id, sizeof(id), file, group, key);
	if (st_find(s, "Saved", id) || st_find(s, "Unset", id))
		return;
	path = kdt_cfg_home(file);
	raw = kdt_kcfg_get_raw(path, group, key);
	if (raw)
		st_set(s, "Saved", id, raw);
	else
		st_set(s, "Unset", id, "true");
	free(raw);
	free(path);
}

/* Put one recorded key back and forget it. `id` is file/group/key: the file
 * runs to the first `/` and the key from the last, and the group is what is
 * between, so a nested group's `][` survives. */
static int restore_key(State *s, const char *id)
{
	const char *a = strchr(id, '/'), *z = strrchr(id, '/');
	char file[64], group[160], key[96];
	const char *raw = st_get(s, "Saved", id);
	int rc;

	if (!a || a == z || (size_t)(a - id) >= sizeof(file) ||
	    (size_t)(z - a - 1) >= sizeof(group) ||
	    strlen(z + 1) >= sizeof(key))
		return -1;
	memcpy(file, id, (size_t)(a - id));
	file[a - id] = '\0';
	memcpy(group, a + 1, (size_t)(z - a - 1));
	group[z - a - 1] = '\0';
	kb_strlcpy(key, z + 1, sizeof(key));

	if (raw) {
		char *v = kdt_kcfg_unescape(raw);

		rc = kdt_kcfg_set(file, group, key, v,
				  !strcmp(file, "kdeglobals"));
		free(v);
	} else {
		rc = kdt_kcfg_set(file, group, key, NULL,
				  !strcmp(file, "kdeglobals"));
	}
	return rc;
}

/* Every recorded key whose id starts with `prefix` (all of them for ""),
 * written back; forgotten too when `forget`, and kept otherwise so a later
 * `off` can still put back the value from before the first change. */
static int restore_some(State *s, const char *prefix, int forget)
{
	int rc = 0;
	char **ids = NULL;
	int n = 0;

	for (int i = 0; i < s->n; i++)
		if ((!strcmp(s->r[i].sec, "Saved") ||
		     !strcmp(s->r[i].sec, "Unset")) &&
		    !strncmp(s->r[i].key, prefix, strlen(prefix))) {
			ids = kb_realloc(ids, (size_t)(n + 1) * sizeof(*ids));
			ids[n++] = kb_strdup(s->r[i].key);
		}
	for (int i = 0; i < n; i++) {
		if (restore_key(s, ids[i]) != 0)
			rc = -1;
		if (forget) {
			st_del(s, "Saved", ids[i]);
			st_del(s, "Unset", ids[i]);
		}
		free(ids[i]);
	}
	free(ids);
	return rc;
}

static int restore_all(State *s, const char *prefix)
{
	return restore_some(s, prefix, 1);
}

/* Records forgotten without being written back. */
static void forget_all(State *s, const char *prefix)
{
	for (int i = 0; i < s->n; i++)
		if ((!strcmp(s->r[i].sec, "Saved") ||
		     !strcmp(s->r[i].sec, "Unset")) &&
		    !strncmp(s->r[i].key, prefix, strlen(prefix))) {
			free(s->r[i].sec);
			free(s->r[i].key);
			free(s->r[i].val);
			s->r[i] = s->r[--s->n];
			i--;
		}
}

/* ── KWin ──────────────────────────────────────────────────────────────── */

static int eff(KdtBus *b, const char *member, const char *name)
{
	return kdt_bus_call_b(b, KWIN, "/Effects", EFFECTS_IFACE, member,
			      NULL, 0, "s", name);
}

static void eff_void(KdtBus *b, const char *member, const char *name)
{
	kdt_bus_call(b, KWIN, "/Effects", EFFECTS_IFACE, member, NULL, NULL,
		     0, "s", name);
}

/* The Plugins keys, in the order 5.4.1 lists them, with the value `on` writes;
 * `off` writes back whatever was recorded. */
static const struct {
	const char *key;
	const char *on;
} PLUGINS[] = {
	{ "kdos_crtEnabled", "true" },
	{ "kdos_unfoldEnabled", "true" },
	{ "kdos_loginEnabled", "true" },
	{ "kdos_quitEnabled", "true" },
	{ "loginEnabled", "false" },
	{ "sessionquitEnabled", "false" },
};
#define NPLUGINS ((int)(sizeof(PLUGINS) / sizeof(PLUGINS[0])))

static void tuning(int *strength, int *scanlines, int *curve, int *full)
{
	char *v;

	v = kdt_kcfg_cascade("kwinrc", "Effect-kdos_crt", "Strength");
	*strength = v && *v ? atoi(v) : 55;
	free(v);
	v = kdt_kcfg_cascade("kwinrc", "Effect-kdos_crt", "Scanlines");
	*scanlines = v && *v ? atoi(v) : 0;
	free(v);
	v = kdt_kcfg_cascade("kwinrc", "Effect-kdos_crt", "Curve");
	*curve = v && *v ? atoi(v) : 0;
	free(v);
	v = kdt_kcfg_cascade("kwinrc", "Effect-kdos_crt", "Fullscreen");
	*full = kdt_kcfg_bool(v, 0);
	free(v);
}

static const char *preset_name(void)
{
	int s, sc, c, f;

	tuning(&s, &sc, &c, &f);
	for (size_t i = 0; i < sizeof(PRESETS) / sizeof(PRESETS[0]); i++)
		if (s == PRESETS[i].strength && sc == PRESETS[i].scanlines &&
		    c == PRESETS[i].curve)
			return PRESETS[i].name;
	return "custom";
}

static int preset_index(const char *name)
{
	for (size_t i = 0; i < sizeof(PRESETS) / sizeof(PRESETS[0]); i++)
		if (!strcmp(name, PRESETS[i].name))
			return (int)i;
	return -1;
}

static int write_preset(KdtBus *b, int i)
{
	char v[16];
	int rc = 0;

	snprintf(v, sizeof(v), "%d", PRESETS[i].strength);
	rc |= kdt_kcfg_set("kwinrc", "Effect-kdos_crt", "Strength", v, 0);
	snprintf(v, sizeof(v), "%d", PRESETS[i].scanlines);
	rc |= kdt_kcfg_set("kwinrc", "Effect-kdos_crt", "Scanlines", v, 0);
	snprintf(v, sizeof(v), "%d", PRESETS[i].curve);
	rc |= kdt_kcfg_set("kwinrc", "Effect-kdos_crt", "Curve", v, 0);
	if (b && eff(b, "isEffectLoaded", "kdos_crt") == 1)
		eff_void(b, "reconfigureEffect", "kdos_crt");
	return rc ? -1 : 0;
}

/* ── the Terminus option ───────────────────────────────────────────────── */

/* The first konsole/<file> along the person's data directory and then
 * $XDG_DATA_DIRS — where Konsole itself looks for a profile. */
static char *profile_find(const char *file)
{
	char rel[160], *p = NULL;
	const char *dirs = getenv("XDG_DATA_DIRS");
	char *copy, *save = NULL;

	snprintf(rel, sizeof(rel), "konsole/%s", file);
	p = kdt_data_home(rel);
	if (kb_path_exists(p))
		return p;
	free(p);
	p = NULL;
	copy = kb_strdup(dirs && *dirs ? dirs : "/usr/local/share:/usr/share");
	for (char *d = strtok_r(copy, ":", &save); d && !p;
	     d = strtok_r(NULL, ":", &save)) {
		char *q = kb_path_join(d, rel);

		if (*d && kb_path_exists(q))
			p = q;
		else
			free(q);
	}
	free(copy);
	return p;
}

/*
 * KDOS-Terminus.profile, from the profile of the accent in force: its colours
 * and everything else it sets, with the name and the face changed. Generated
 * into the person's data directory, where Konsole finds a profile first, and
 * regenerated by `kdos theme` while the option is on so the colours follow
 * the accent.
 */
int kdt_phosphor_terminus_profile(const KcolScheme *sc)
{
	char file[96], *src, *dst;

	snprintf(file, sizeof(file), "%s.profile", sc->theme_name);
	src = profile_find(file);
	if (!src)
		return -1;
	dst = kdt_data_home("konsole/KDOS-Terminus.profile");
	kdt_mkparent(dst);
	if (kb_copy_file(src, dst) != 0) {
		free(src);
		free(dst);
		return -1;
	}
	kdt_ini_set(dst, "General", "Name", "KDOS Terminus");
	kdt_ini_set(dst, "Appearance", "Font", "Terminus (TTF),12");
	free(src);
	free(dst);
	return 0;
}

/* The profile NAME a profile file carries, for the tabs `off` moves back. */
static char *profile_name(const char *file)
{
	char *p = profile_find(file), *name;

	if (!p)
		return NULL;
	name = kdt_kcfg_get(p, "General", "Name");
	free(p);
	return name;
}

static const char *terminus_pair_key(const char *accent)
{
	const KcolScheme *sc = kcol_find(accent);

	return sc && kdt_accent_light(sc) ? "LightThemeProfile"
					  : "DarkThemeProfile";
}

/* Step (g): the face, the profile, and the open tabs. Every key it may move
 * is recorded before the first write. */
static int terminus_apply(KdtBus *b, State *s)
{
	const KcolScheme *sc = kcol_find(kdt_current_accent());
	char *cur;
	int rc = 0;

	if (!sc)
		sc = kcol_default();
	/* All three Konsole keys, whichever this run moves: while the option
	 * is on, `kdos theme` moves the light or the dark half to the Terminus
	 * profile as the accent changes side, and `off` can only put back what
	 * was recorded. */
	save_key(s, "konsolerc", "Desktop Entry", "DefaultProfile");
	save_key(s, "konsolerc", "LightDarkTheme", "DarkThemeProfile");
	save_key(s, "konsolerc", "LightDarkTheme", "LightThemeProfile");
	save_key(s, "kdeglobals", "General", "fixed");
	if (st_save(s) != 0)
		return -1;
	if (kdt_kcfg_set("kdeglobals", "General", "fixed", "Terminus (TTF),12",
			 1) != 0)
		rc = -1;
	if (kdt_phosphor_terminus_profile(sc) != 0)
		return -1;
	cur = kdt_kcfg_cascade("konsolerc", "Desktop Entry", "DefaultProfile");
	if (!cur || !*cur || !strncmp(cur, "KDOS-", 5)) {
		const char *pair = terminus_pair_key(sc->name);
		char *pv;

		if (kdt_kcfg_set("konsolerc", "Desktop Entry", "DefaultProfile",
				 "KDOS-Terminus.profile", 0) != 0)
			rc = -1;
		pv = kdt_kcfg_cascade("konsolerc", "LightDarkTheme", pair);
		if (!pv || !*pv || !strncmp(pv, "KDOS ", 5)) {
			if (kdt_kcfg_set("konsolerc", "LightDarkTheme", pair,
					 "KDOS Terminus", 0) != 0)
				rc = -1;
		}
		free(pv);
	}
	free(cur);
	if (st_save(s) != 0)
		rc = -1;
	if (b)
		kdt_konsole_retarget(b, "KDOS ", "KDOS Terminus");
	return rc;
}

/* The tabs on the Terminus profile go back to the profile a new tab would
 * open in now that the keys are restored. */
static void terminus_tabs_back(KdtBus *b)
{
	char *file = kdt_kcfg_cascade("konsolerc", "Desktop Entry",
				      "DefaultProfile");
	char *name = file && *file ? profile_name(file) : NULL;

	if (b && name)
		kdt_konsole_retarget(b, "KDOS Terminus", name);
	free(name);
	free(file);
}

/*
 * The option off while the mode stays on. The face is the option's alone and
 * goes back for good. The Konsole keys go back to what `on` found and stay
 * recorded, because the accent may have moved since (bone paired to phosphor,
 * or one the person picked): the accent in force then takes them the way
 * `kdos theme` would, and `off` still puts back the value from before the
 * mode.
 */
static int terminus_revert(KdtBus *b, State *s)
{
	int rc = 0;
	char *dst;

	if (restore_all(s, "kdeglobals/General/fixed") != 0 ||
	    restore_some(s, "konsolerc/", 0) != 0)
		rc = -1;
	if (st_save(s) != 0 || kdt_theme_konsole_follow(b) != 0)
		rc = -1;
	terminus_tabs_back(b);
	dst = kdt_data_home("konsole/KDOS-Terminus.profile");
	unlink(dst);
	free(dst);
	if (st_save(s) != 0)
		rc = -1;
	return rc;
}

/* ── the splash ────────────────────────────────────────────────────────── */

/* The boot splash's motion is the machine's, in the boot menu's command
 * line, so it goes through kdos-powerd; refused or unanswered, the rest of
 * the mode stands and the line says what did not move. */
static void splash(const char *motion)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, "kdos-power");
	kb_argv_add(&a, "splash");
	kb_argv_add(&a, motion);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		printf("boot splash unchanged (kdos-powerd did not answer)\n");
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos phosphor on [--preset subtle|retro] "
		"[--terminus|--no-terminus]\n"
		"       kdos phosphor off\n"
		"       kdos phosphor status [--json]\n"
		"       kdos phosphor preset subtle|retro\n"
		"       kdos phosphor terminus on|off\n"
		"       kdos phosphor fullscreen on|off\n"
		"       kdos phosphor collapse|unfold|degauss\n");
	return 2;
}

/* KWin is reachable and offers the effect interface: (a) and (b). Returns 0,
 * or the exit status with the refusal printed. */
static int gate(KdtBus *b)
{
	int sup;

	if (!b || kdt_bus_has_owner(b, KWIN) != 1) {
		fprintf(stderr, "%s\n", REFUSE_SESSION);
		return 2;
	}
	sup = eff(b, "isEffectSupported", "kdos_crt");
	if (sup != 1) {
		fprintf(stderr, "%s\n", REFUSE_GPU);
		return 3;
	}
	return 0;
}

static int load_effects(KdtBus *b)
{
	for (size_t i = 0; i < sizeof(THEIRS) / sizeof(THEIRS[0]); i++)
		eff_void(b, "unloadEffect", THEIRS[i]);
	if (eff(b, "loadEffect", OURS[0]) != 1)
		return -1;
	for (size_t i = 1; i < sizeof(OURS) / sizeof(OURS[0]); i++)
		eff(b, "loadEffect", OURS[i]);
	return 0;
}

/* KWin's two come back when the restored key says they are on, or says
 * nothing: both are enabled by default. */
static void unload_effects(KdtBus *b)
{
	for (int i = (int)(sizeof(OURS) / sizeof(OURS[0])) - 1; i >= 0; i--)
		eff_void(b, "unloadEffect", OURS[i]);
	for (size_t i = 0; i < sizeof(THEIRS) / sizeof(THEIRS[0]); i++) {
		char key[64], *v;

		snprintf(key, sizeof(key), "%sEnabled", THEIRS[i]);
		v = kdt_kcfg_cascade("kwinrc", "Plugins", key);
		if (kdt_kcfg_bool(v, 1))
			eff(b, "loadEffect", THEIRS[i]);
		free(v);
	}
}

static int cmd_on(int argc, char **argv)
{
	const char *preset = NULL;
	int terminus = -1, rc, pair = 0, pi = -1;
	State s = { 0 };
	KdtBus *b;
	char accent[32];

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--preset") && i + 1 < argc)
			preset = argv[++i];
		else if (!strcmp(argv[i], "--terminus"))
			terminus = 1;
		else if (!strcmp(argv[i], "--no-terminus"))
			terminus = 0;
		else
			return usage();
	}
	if (preset && (pi = preset_index(preset)) < 0)
		return usage();

	b = kdt_bus_open(0);
	if ((rc = gate(b)) != 0) {
		kdt_bus_close(b);
		return rc;
	}

	st_load(&s);
	kb_strlcpy(accent, kdt_current_accent(), sizeof(accent));

	if (st_on(&s)) {
		/* Already on: only the options move. */
		int was = st_terminus(&s);

		if (pi >= 0 && write_preset(b, pi) != 0)
			goto write_failed;
		if (terminus >= 0) {
			st_set(&s, "Settings", "Terminus",
			       terminus ? "true" : "false");
			if (st_save(&s) != 0)
				goto write_failed;
			if (terminus && !was && terminus_apply(b, &s) != 0)
				goto write_failed;
			if (!terminus && was && terminus_revert(b, &s) != 0)
				goto write_failed;
		}
		printf("phosphor mode on (%s)\n", preset_name());
		st_free(&s);
		kdt_bus_close(b);
		return 0;
	}

	/* (c) Everything this run may change, recorded before any of it is:
	 * the Plugins keys, and — when the accent is paired — the keys the
	 * accent switch itself moves, so `off` can put them back exactly. */
	if (terminus >= 0)
		st_set(&s, "Settings", "Terminus", terminus ? "true" : "false");
	pair = !strcmp(accent, "bone") || !strcmp(accent, "paper");
	st_set(&s, "State", "On", "true");
	if (pair)
		st_set(&s, "State", "PreviousAccent", accent);
	else
		st_del(&s, "State", "PreviousAccent");
	for (int i = 0; i < NPLUGINS; i++)
		save_key(&s, "kwinrc", "Plugins", PLUGINS[i].key);
	if (pair || st_terminus(&s)) {
		save_key(&s, "konsolerc", "Desktop Entry", "DefaultProfile");
		save_key(&s, "konsolerc", "LightDarkTheme", "DarkThemeProfile");
		save_key(&s, "konsolerc", "LightDarkTheme",
			 "LightThemeProfile");
	}
	if (pair) {
		save_key(&s, "kdeglobals", "KDE", "DefaultDarkLookAndFeel");
		save_key(&s, "kdeglobals", "KDE", "LookAndFeelPackage");
	}
	if (st_terminus(&s))
		save_key(&s, "kdeglobals", "General", "fixed");
	if (st_save(&s) != 0) {
		fprintf(stderr, "kdos phosphor: cannot write the state file: "
				"%s\n", strerror(errno));
		st_free(&s);
		kdt_bus_close(b);
		return 1;
	}

	/* (d) The keys, so the next login keeps the mode, then the effects
	 * themselves, so this one has it now. */
	for (int i = 0; i < NPLUGINS; i++)
		if (kdt_kcfg_set("kwinrc", "Plugins", PLUGINS[i].key,
				 PLUGINS[i].on, 0) != 0) {
			restore_all(&s, "");
			st_del(&s, "State", NULL);
			st_save(&s);
			goto write_failed;
		}
	if (load_effects(b) != 0) {
		/* KWin said yes to supported() and no to loading: undo all of
		 * it, and say the one thing a person can act on. */
		restore_all(&s, "");
		st_del(&s, "State", NULL);
		st_save(&s);
		unload_effects(b);
		fprintf(stderr, "%s\n", REFUSE_GPU);
		st_free(&s);
		kdt_bus_close(b);
		return 3;
	}

	/* (e) */
	if (pi >= 0 && write_preset(b, pi) != 0)
		goto write_failed;

	/* (f) B19: bone and paper are the two accents that do not read as a
	 * CRT. The switch is the ordinary one, so its degauss plays the new
	 * tint. */
	if (pair) {
		const KcolScheme *ph = kcol_find("phosphor");

		if (ph)
			kdt_theme_set(ph, 0);
	}

	/* (g) */
	if (st_terminus(&s) && terminus_apply(b, &s) != 0)
		goto write_failed;

	/* (h) */
	splash("phosphor");

	printf("phosphor mode on (%s)\n", preset_name());
	st_free(&s);
	kdt_bus_close(b);
	return 0;

write_failed:
	fprintf(stderr, "kdos phosphor: kwriteconfig6 could not write the "
			"settings\n");
	st_free(&s);
	kdt_bus_close(b);
	return 1;
}

static int cmd_off(int argc, char **argv)
{
	State s = { 0 };
	KdtBus *b;
	char prev[32] = "";
	int rc = 0, paired_back;

	(void)argv;
	if (argc)
		return usage();
	st_load(&s);
	if (!st_on(&s)) {
		printf("phosphor mode is already off\n");
		st_free(&s);
		return 0;
	}
	b = kdt_bus_open(0);
	if (b && kdt_bus_has_owner(b, KWIN) != 1) {
		kdt_bus_close(b);
		b = NULL;
	}
	if (st_get(&s, "State", "PreviousAccent"))
		kb_strlcpy(prev, st_get(&s, "State", "PreviousAccent"),
			   sizeof(prev));

	/* The mode is off from here: a `kdos theme` run below must not take
	 * the Terminus profile for the one in force. */
	st_del(&s, "State", NULL);

	/* An accent the person picked while the mode was on is theirs, and so
	 * are the global theme keys it wrote: bone's put back under it would
	 * be a Global Theme page that disagrees with the accent, which
	 * kded-kdos takes for a new choice. Those records are dropped, and the
	 * Konsole keys follow the accent in force below. */
	paired_back = *prev && !strcmp(kdt_current_accent(), "phosphor");
	if (*prev && !paired_back)
		forget_all(&s, "kdeglobals/KDE/");

	if (restore_all(&s, "") != 0)
		rc = 1;
	if (st_save(&s) != 0)
		rc = 1;
	terminus_tabs_back(b);
	{
		char *dst = kdt_data_home("konsole/KDOS-Terminus.profile");

		unlink(dst);
		free(dst);
	}
	if (b)
		unload_effects(b);

	/* The paired accent goes back; any other accent keeps the profiles
	 * that are its own, which the restored keys may not name when the
	 * accent moved while the mode was on. */
	if (paired_back) {
		const KcolScheme *sc = kcol_find(prev);

		if (sc)
			kdt_theme_set(sc, 0);
	} else if (kdt_theme_konsole_follow(b) != 0) {
		rc = 1;
	}
	splash("modern");

	st_del(&s, "Saved", NULL);
	st_del(&s, "Unset", NULL);
	if (st_save(&s) != 0)
		rc = 1;
	st_free(&s);
	kdt_bus_close(b);
	if (rc) {
		fprintf(stderr, "kdos phosphor: kwriteconfig6 could not put "
				"every setting back\n");
		return 1;
	}
	printf("phosphor mode off\n");
	return 0;
}

static int cmd_preset(int argc, char **argv)
{
	KdtBus *b;
	int i, rc;

	if (argc != 1 || (i = preset_index(argv[0])) < 0)
		return usage();
	b = kdt_bus_open(0);
	rc = write_preset(b, i);
	kdt_bus_close(b);
	if (rc != 0) {
		fprintf(stderr, "kdos phosphor: kwriteconfig6 could not write "
				"kwinrc\n");
		return 1;
	}
	printf("preset %s\n", PRESETS[i].name);
	return 0;
}

/*
 * Whether the pass is drawn over a fullscreen window. Off by default
 * ([Effect-kdos_crt] Fullscreen absent), which leaves a game or a video on
 * KWin's direct scanout path; on curves and tints it like everything else.
 * A tuning key like the preset's, so `off` keeps it.
 */
static int cmd_fullscreen(int argc, char **argv)
{
	KdtBus *b;
	int want, rc;

	if (argc != 1 || (strcmp(argv[0], "on") && strcmp(argv[0], "off")))
		return usage();
	want = !strcmp(argv[0], "on");
	rc = kdt_kcfg_set("kwinrc", "Effect-kdos_crt", "Fullscreen",
			  want ? "true" : "false", 0);
	b = kdt_bus_open(0);
	if (rc == 0 && b && eff(b, "isEffectLoaded", "kdos_crt") == 1)
		eff_void(b, "reconfigureEffect", "kdos_crt");
	kdt_bus_close(b);
	if (rc != 0) {
		fprintf(stderr, "kdos phosphor: kwriteconfig6 could not write "
				"kwinrc\n");
		return 1;
	}
	printf("fullscreen %s\n", want ? "drawn" : "exempt");
	return 0;
}

static int cmd_terminus(int argc, char **argv)
{
	State s = { 0 };
	KdtBus *b = NULL;
	int want, was, rc = 0;

	if (argc != 1 || (strcmp(argv[0], "on") && strcmp(argv[0], "off")))
		return usage();
	want = !strcmp(argv[0], "on");
	st_load(&s);
	was = st_terminus(&s);
	st_set(&s, "Settings", "Terminus", want ? "true" : "false");
	if (st_save(&s) != 0)
		rc = 1;
	if (!rc && st_on(&s) && want != was) {
		b = kdt_bus_open(0);
		rc = (want ? terminus_apply(b, &s) : terminus_revert(b, &s))
		     ? 1 : 0;
	}
	st_free(&s);
	kdt_bus_close(b);
	if (rc) {
		fprintf(stderr, "kdos phosphor: the Terminus setting could not "
				"be written\n");
		return 1;
	}
	printf("terminus %s\n", want ? "on" : "off");
	return 0;
}

static int cmd_motion(const char *verb)
{
	static const struct { const char *verb, *method; } M[] = {
		{ "collapse", "Collapse" },
		{ "unfold", "Unfold" },
		{ "degauss", "Degauss" },
	};
	KdtBus *b = kdt_bus_open(0);
	const char *method = NULL;
	int rc;

	for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++)
		if (!strcmp(verb, M[i].verb))
			method = M[i].method;
	if (!b || kdt_bus_has_owner(b, KWIN) != 1) {
		fprintf(stderr, "%s\n", REFUSE_SESSION);
		kdt_bus_close(b);
		return 2;
	}
	rc = kdt_bus_call(b, KWIN, CRT_PATH, CRT_IFACE, method, NULL, NULL, 0,
			  "");
	kdt_bus_close(b);
	if (rc != 0) {
		fprintf(stderr, "phosphor mode is off\n");
		return 4;
	}
	return 0;
}

static int cmd_status(int argc, char **argv)
{
	int json = 0, strength, scanlines, curve, full, sup = 0, on, term;
	int loaded[4] = { 0 };
	const char *reason = NULL, *refusal = NULL;
	char *phase = NULL;
	State s = { 0 };
	KdtBus *b;
	const char *prev, *preset = preset_name();

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			json = 1;
		else
			return usage();
	}
	st_load(&s);
	on = st_on(&s);
	term = st_terminus(&s);
	prev = st_get(&s, "State", "PreviousAccent");
	tuning(&strength, &scanlines, &curve, &full);

	b = kdt_bus_open(0);
	if (!b || kdt_bus_has_owner(b, KWIN) != 1) {
		reason = REFUSE_SESSION;
		refusal = "session";
	} else {
		sup = eff(b, "isEffectSupported", "kdos_crt") == 1;
		if (!sup) {
			reason = REFUSE_GPU;
			refusal = "gpu";
		}
		for (int i = 0; i < 4; i++)
			loaded[i] = eff(b, "isEffectLoaded", OURS[i]) == 1;
		if (loaded[0])
			phase = kdt_bus_get_prop_s(b, KWIN, CRT_PATH,
						   CRT_IFACE, "Phase", NULL,
						   0);
	}
	kdt_bus_close(b);

	if (json) {
		KbBuf o = { 0 };

		kb_buf_printf(&o, "{\"state\": \"%s\", \"on\": %s, "
				  "\"supported\": %s, \"reason\": ",
			      on ? "on" : "off", on ? "true" : "false",
			      sup ? "true" : "false");
		if (reason)
			kb_json_str(&o, reason);
		else
			kb_buf_str(&o, "null");
		kb_buf_str(&o, ", \"refusal\": ");
		if (refusal)
			kb_json_str(&o, refusal);
		else
			kb_buf_str(&o, "null");
		kb_buf_printf(&o, ", \"preset\": \"%s\", \"strength\": %d, "
				  "\"scanlines\": %d, \"curve\": %d, "
				  "\"fullscreen\": %s, \"terminus\": %s, "
				  "\"accent\": ",
			      preset, strength, scanlines, curve,
			      full ? "true" : "false",
			      term ? "true" : "false");
		kb_json_str(&o, kdt_current_accent());
		kb_buf_str(&o, ", \"previous_accent\": ");
		if (prev && *prev)
			kb_json_str(&o, prev);
		else
			kb_buf_str(&o, "null");
		kb_buf_printf(&o, ", \"loaded\": {\"kdos_crt\": %s, "
				  "\"kdos_unfold\": %s, \"kdos_login\": %s, "
				  "\"kdos_quit\": %s}, \"phase\": ",
			      loaded[0] ? "true" : "false",
			      loaded[1] ? "true" : "false",
			      loaded[2] ? "true" : "false",
			      loaded[3] ? "true" : "false");
		if (phase)
			kb_json_str(&o, phase);
		else
			kb_buf_str(&o, "null");
		kb_buf_str(&o, "}\n");
		fwrite(o.p, 1, o.n, stdout);
		kb_buf_free(&o);
	} else {
		printf("phosphor: %s\n", on ? "on" : "off");
		printf("supported: %s%s%s\n", sup ? "yes" : "no",
		       reason ? " — " : "", reason ? reason : "");
		printf("preset: %s (strength %d, scanlines %d, curve %d)\n",
		       preset, strength, scanlines, curve);
		printf("fullscreen: %s\n", full ? "drawn" : "exempt");
		printf("terminus: %s\n", term ? "on" : "off");
		printf("accent: %s", kdt_current_accent());
		if (prev && *prev)
			printf(" (previous %s)", prev);
		putchar('\n');
		if (phase)
			printf("phase: %s\n", phase);
	}
	free(phase);
	st_free(&s);
	return 0;
}

int kdt_phosphor_main(int argc, char **argv)
{
	const char *verb = argc > 0 ? argv[0] : "status";

	if (!strcmp(verb, "on"))
		return cmd_on(argc - 1, argv + 1);
	if (!strcmp(verb, "off"))
		return cmd_off(argc - 1, argv + 1);
	if (!strcmp(verb, "status"))
		return cmd_status(argc > 0 ? argc - 1 : 0, argv + 1);
	if (!strcmp(verb, "preset"))
		return cmd_preset(argc - 1, argv + 1);
	if (!strcmp(verb, "terminus"))
		return cmd_terminus(argc - 1, argv + 1);
	if (!strcmp(verb, "fullscreen"))
		return cmd_fullscreen(argc - 1, argv + 1);
	if (!strcmp(verb, "collapse") || !strcmp(verb, "unfold") ||
	    !strcmp(verb, "degauss")) {
		if (argc != 1)
			return usage();
		return cmd_motion(verb);
	}
	if (!strcmp(verb, "-h") || !strcmp(verb, "--help")) {
		usage();
		return 0;
	}
	return usage();
}
