/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos — the front door
 *
 *   kdos help     commands + the keybind cheat sheet
 *   kdos theme    switch the accent across the desktop, GTK, icons, foot, btop,
 *                 starship
 *   kdos status   packages, containers, exported apps
 *   kdos doctor   the checks that have actually caught something on this distro
 *   kdos app      install an alien app
 *   kdos version
 *
 * The palette is libkcolor's and nothing else's. This file used to carry a
 * second copy of the table — four schemes, nine colours, hand-kept in step
 * with the installer's — and the two were edited separately.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>

#include <stdbool.h>

#include "kdos-tools.h"

/* Where kdos-cursors installs its artwork. kdos-theme's own CURSOR_ART_DEFAULT
 * is this same path; asking here is only "is there anything to recolour", so an
 * older install without the art keeps its cursors instead of getting an empty
 * theme. */
#define CURSOR_ART_PATH "/usr/share/kdos/cursors/art"

/* ANSI, but only when someone is looking. */
static const char *C_A = "", *C_B = "", *C_D = "", *C_W = "", *C_0 = "";

static void colours(void)
{
	if (!isatty(STDOUT_FILENO))
		return;
	C_A = "\033[1;32m";
	C_B = "\033[0;32m";
	C_D = "\033[2;32m";
	C_W = "\033[1;33m";
	C_0 = "\033[0m";
}

/* ──────────────────────────────────────────────────────────────────────── */

char *kdt_cfg_home(const char *rest)
{
	const char *x = getenv("XDG_CONFIG_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".config");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

char *kdt_data_home(const char *rest)
{
	const char *x = getenv("XDG_DATA_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".local/share");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

static char *cache_home(const char *rest)
{
	const char *x = getenv("XDG_CACHE_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".cache");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

static const char *current_theme(void)
{
	static char name[32];
	char *p = cache_home("kdos/theme");
	name[0] = 0;
	if (kb_read_line_file(p, name, sizeof(name)) > 0 && kcol_find(name)) {
		free(p);
		return name;
	}
	free(p);
	return "phosphor";
}

static void mkparent(const char *path)
{
	char *copy = kb_strdup(path);
	char *slash = strrchr(copy, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(copy);
	}
	free(copy);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The desktop needs no theme file at all: kdos-comp and kdos-shell link
 * libkcolor, so they carry the same KCOL_SCHEMES table this program does, and
 * they read the accent NAME from $XDG_CACHE_HOME/kdos/theme — which
 * write_state() below is what writes. A running session is retinted by
 * signalling it, not by handing it colours. That is the whole reason
 * kdos-theme-helper (Rust, and a dependency on cosmic-theme's ThemeBuilder)
 * could be deleted rather than ported.
 *
 * Everything under this point exists for software that is NOT ours and cannot
 * be told: GTK and Qt apps in the appbox, foot, btop, starship. */
static void reload_session(void)
{
	if (!kb_have_prog("pkill"))
		return;
	/* Both halves of the desktop: the shell repaints its chrome, the
	 * compositor re-reads the accent its CRT shader tints with. Separate
	 * pkills rather than one pattern — `kdos-*` would also signal kdos-appbox
	 * and any alien app launched through it. */
	static const char *const who[] = { "kdos-shell", "kdos-comp" };
	for (size_t i = 0; i < sizeof(who) / sizeof(who[0]); i++) {
		KbArgv a = {0};
		kb_argv_add(&a, "pkill");
		kb_argv_add(&a, "-HUP");
		kb_argv_add(&a, who[i]);
		kb_argv_end(&a);
		kb_run(&a);	/* no session running is not an error */
	}
}

/*
 * Two layers, and both are needed.
 *
 *   1. ~/.themes/KDOS — the full stylesheet, regenerated for this accent by
 *      kdos-theme. Straight @define-color in the user config is NOT enough on
 *      its own for GTK3: stock Adwaita is compiled from SASS with literal hex
 *      in every rule, so the names reach only the widgets that reference them
 *      (GIMP's spin-scales did; its panels did not). The theme KDOS ships is
 *      adw-gtk3, written against the named colours end to end, so retinting it
 *      retints everything.
 *   2. ~/.config/gtk-{3,4}.0/gtk.css — the same palette as @define-color, at
 *      the highest style priority. libadwaita apps ignore GTK themes entirely
 *      and read only this, and it also overrides the theme for any GTK3 app
 *      started before the theme was regenerated.
 *
 * Alien apps in the appbox share $HOME, which is why both live there rather
 * than in /usr/share. Qt apps in the box follow via the qgtk3 platform theme.
 */
static void write_gtk(const KcolScheme *sc)
{
	uint32_t lift = kcol_mix(sc->deep, sc->text, 9);
	uint32_t pop = kcol_mix(sc->deep, sc->text, 11);
	uint32_t header = kcol_mix(sc->deep, sc->text, 7);
	uint32_t onacc = kcol_mix(0xffffff, sc->text, 35);
	uint32_t insens = kcol_mix(sc->text, sc->variant, 55);

	/* Every colour is formatted into its OWN local first. A helper handing
	 * back a shared or rotating buffer cannot be used here: one printf
	 * below takes more colours than any such buffer has slots, and the
	 * early arguments come back overwritten. Do not "simplify" this into
	 * inline calls. */
	char P[8], PD[8], SEC[8], URG[8], DEEP[8], TXT[8], VAR[8], DIM[8];
	char LIFT[8], POP[8], HDR[8], ONACC[8], INSENS[8], WHITE[8];
	kcol_format(sc->primary, P);
	kcol_format(sc->pdark, PD);
	kcol_format(sc->secondary, SEC);
	kcol_format(sc->urgent, URG);
	kcol_format(sc->deep, DEEP);
	kcol_format(sc->text, TXT);
	kcol_format(sc->variant, VAR);
	kcol_format(sc->dim, DIM);
	kcol_format(lift, LIFT);
	kcol_format(pop, POP);
	kcol_format(header, HDR);
	kcol_format(onacc, ONACC);
	kcol_format(insens, INSENS);
	(void)WHITE;

	if (kb_have_prog("kdos-theme")) {
		char *themes = kb_path_join(kb_home_dir(), ".themes/KDOS");
		kb_mkdir_p(themes);
		KbArgv a = {0};
		kb_argv_add(&a, "kdos-theme");
		kb_argv_add(&a, "gtk");
		kb_argv_add(&a, themes);
		kb_argv_add(&a, sc->name);
		kb_argv_end(&a);
		kb_run(&a);
		free(themes);
	}

	/* One palette, written twice: GTK3 spells the surfaces "theme_", GTK4
	 * and libadwaita spell them "window_"/"view_". Both files carry both
	 * spellings so neither toolkit has to fall back to the stylesheet's
	 * own value. */
	KbBuf b = {0};
	kb_buf_printf(&b,
		"@define-color accent_color #%s;\n"
		"@define-color accent_bg_color #%s;\n"
		"@define-color accent_fg_color #%s;\n"
		"@define-color success_color #%s;\n"
		"@define-color success_bg_color #%s;\n"
		"@define-color success_fg_color #%s;\n"
		"@define-color warning_color #%s;\n"
		"@define-color warning_bg_color #%s;\n"
		"@define-color warning_fg_color #%s;\n"
		"@define-color destructive_color #%s;\n"
		"@define-color destructive_bg_color #%s;\n"
		"@define-color destructive_fg_color #%s;\n"
		"@define-color error_color #%s;\n"
		"@define-color error_bg_color #%s;\n"
		"@define-color error_fg_color #%s;\n"
		"\n",
		P, PD, ONACC, P, PD, ONACC,
		SEC, SEC, DEEP, URG, URG, DEEP, URG, URG, DEEP);
	kb_buf_printf(&b,
		"@define-color window_bg_color #%s;\n"
		"@define-color window_fg_color #%s;\n"
		"@define-color view_bg_color #%s;\n"
		"@define-color view_fg_color #%s;\n"
		"@define-color headerbar_bg_color #%s;\n"
		"@define-color headerbar_fg_color #%s;\n"
		"@define-color headerbar_border_color #%s;\n"
		"@define-color headerbar_backdrop_color #%s;\n"
		"@define-color headerbar_shade_color rgba(0, 0, 0, 0.36);\n"
		"@define-color headerbar_darker_shade_color rgba(0, 0, 0, 0.72);\n"
		"@define-color sidebar_bg_color #%s;\n"
		"@define-color sidebar_fg_color #%s;\n"
		"@define-color sidebar_backdrop_color #%s;\n"
		"@define-color sidebar_shade_color rgba(0, 0, 0, 0.25);\n"
		"@define-color secondary_sidebar_bg_color #%s;\n"
		"@define-color secondary_sidebar_fg_color #%s;\n"
		"@define-color secondary_sidebar_backdrop_color #%s;\n"
		"@define-color secondary_sidebar_shade_color rgba(0, 0, 0, 0.25);\n"
		"@define-color card_bg_color #%s;\n"
		"@define-color card_fg_color #%s;\n"
		"@define-color card_shade_color rgba(0, 0, 0, 0.36);\n"
		"@define-color dialog_bg_color #%s;\n"
		"@define-color dialog_fg_color #%s;\n"
		"@define-color popover_bg_color #%s;\n"
		"@define-color popover_fg_color #%s;\n"
		"@define-color popover_shade_color rgba(0, 0, 0, 0.36);\n"
		"@define-color thumbnail_bg_color #%s;\n"
		"@define-color thumbnail_fg_color #%s;\n"
		"@define-color panel_bg_color #%s;\n"
		"@define-color panel_fg_color #%s;\n"
		"@define-color shade_color rgba(0, 0, 0, 0.36);\n"
		"@define-color scrollbar_outline_color rgba(0, 0, 0, 0.5);\n"
		"\n",
		VAR, TXT, DEEP, TXT,
		HDR, TXT, DIM, VAR,
		HDR, TXT, VAR,
		HDR, TXT, VAR,
		LIFT, TXT,
		POP, TXT, POP, TXT,
		LIFT, TXT, DEEP, TXT);
	kb_buf_printf(&b,
		"@define-color theme_bg_color #%s;\n"
		"@define-color theme_fg_color #%s;\n"
		"@define-color theme_base_color #%s;\n"
		"@define-color theme_text_color #%s;\n"
		"@define-color theme_selected_bg_color #%s;\n"
		"@define-color theme_selected_fg_color #%s;\n"
		"@define-color theme_unfocused_bg_color #%s;\n"
		"@define-color theme_unfocused_fg_color #%s;\n"
		"@define-color theme_unfocused_base_color #%s;\n"
		"@define-color theme_unfocused_text_color #%s;\n"
		"@define-color theme_unfocused_selected_bg_color #%s;\n"
		"@define-color theme_unfocused_selected_fg_color #%s;\n"
		"@define-color insensitive_bg_color #%s;\n"
		"@define-color insensitive_fg_color #%s;\n"
		"@define-color insensitive_base_color #%s;\n"
		"@define-color borders #%s;\n"
		"@define-color unfocused_borders #%s;\n"
		"@define-color content_view_bg #%s;\n"
		"@define-color text_view_bg #%s;\n",
		VAR, TXT, DEEP, TXT, PD, ONACC,
		VAR, TXT, DEEP, TXT, PD, ONACC,
		VAR, INSENS, DEEP, DIM, DIM, DEEP, DEEP);

	static const struct {
		const char *dir;
		const char *head;
	} OUT[] = {
		{ "gtk-3.0", "/* KDOS GTK3 palette — GENERATED by `kdos theme`; "
			     "edits will be overwritten. */\n" },
		{ "gtk-4.0", "/* KDOS GTK4/libadwaita palette — GENERATED by "
			     "`kdos theme`; edits overwritten. */\n" },
	};
	for (int i = 0; i < 2; i++) {
		char *dir = kdt_cfg_home(OUT[i].dir);
		kb_mkdir_p(dir);
		char *f = kb_path_join(dir, "gtk.css");
		KbBuf full = {0};
		kb_buf_str(&full, OUT[i].head);
		kb_buf_add(&full, b.p, b.n);
		kb_write_all(f, full.p, full.n);
		kb_buf_free(&full);
		free(f);
		free(dir);
	}
	kb_buf_free(&b);
}

/* Papirus is flat single-fill artwork, so the accent lives in the SVGs
 * themselves and no amount of CSS reaches it. ~/.icons wins over
 * /usr/share/icons for the session AND is the only icon path the appbox can
 * see, so regenerating it here retints the host and the box in one go. */
static void write_icons(const KcolScheme *sc)
{
	if (!kb_have_prog("kdos-theme"))
		return;
	char *out = kb_path_join(kb_home_dir(), ".icons/KDOS");
	mkparent(out);
	KbArgv a = {0};
	kb_argv_add(&a, "kdos-theme");
	kb_argv_add(&a, "icons");
	kb_argv_add(&a, out);
	kb_argv_add(&a, sc->name);
	kb_argv_end(&a);
	kb_run(&a);
	free(out);
}

/*
 * The cursors, into ~/.icons/KDOS-cursors — where XCURSOR_THEME already points
 * and, as with the icons, the only path the appbox can see. This was the one
 * artefact an accent switch could not reach: the generator was always
 * parameterised, but the art it needs was not installed on the target until
 * kdos-cursors started shipping it to /usr/share/kdos/cursors/art.
 *
 * A cursor theme is 4.4 MB of premultiplied Xcursor files and takes a moment to
 * write, which is why the accent switch prints before it rather than after.
 */
static void write_cursors(const KcolScheme *sc)
{
	const char *art = getenv("KDOS_CURSOR_ART");
	if (!kb_have_prog("kdos-theme"))
		return;
	/* Same precedence kdos-theme itself uses: the environment beats the
	 * installed art. An install with neither keeps the cursors it has rather
	 * than getting an empty theme. */
	if (!(art && *art && kb_is_dir(art)) && !kb_is_dir(CURSOR_ART_PATH))
		return;
	char *out = kb_path_join(kb_home_dir(), ".icons/KDOS-cursors");
	mkparent(out);
	KbArgv a = {0};
	kb_argv_add(&a, "kdos-theme");
	kb_argv_add(&a, "cursors");
	kb_argv_add(&a, out);
	kb_argv_add(&a, sc->name);
	kb_argv_end(&a);
	kb_run(&a);
	free(out);
}

static void write_foot(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("foot/themes/kdos");
	mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);

	/*
	 * One section, and it is `[colors-dark]`, not `[colors]`. foot deprecated
	 * the old section name and warns ONCE PER KEY on stderr — 24 keys, so
	 * every terminal opened on this desktop began with 24 lines of
	 * "deprecated: foot: [colors]: use [colors-dark] instead" above the first
	 * prompt. `initial-color-theme` defaults to `dark`, so the dark section
	 * alone is what foot reads; there is no light KDOS palette to write.
	 */
	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS foot theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"[colors-dark]\n"
		"background=%s\nforeground=%s\ncursor=%s %s\n"
		"selection-background=%s\nselection-foreground=%s\n"
		"regular0=%s\nregular1=%s\nregular2=%s\nregular3=%s\n"
		"regular4=%s\nregular5=%s\nregular6=%s\nregular7=%s\n"
		"bright0=%s\nbright1=%s\nbright2=%s\nbright3=%s\n"
		"bright4=%s\nbright5=%s\nbright6=%s\nbright7=%s\n",
		deep, text, deep, p, dim, p,
		deep, urg, p, sec, dim, sec, p, text,
		dim, urg, p, sec, dim, sec, p, text);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}


/*
 * Midnight Commander's skin.
 *
 * mc IS the file manager on this desktop — already a port, already Turbo
 * Vision, and on the same glyph grid as the window frames when foot is
 * configured with Terminus. What it was NOT was phosphor: it shipped its own
 * blue-on-cyan default, so the one full-screen application the desktop leans on
 * was the one thing that did not match it.
 *
 * TRUECOLOR, in mc's `rgb:rrggbb` spelling. mc's 16-colour names would round
 * the palette to the nearest ANSI slot, which turns four distinguishable
 * accents into two; the `truecolor` capability in [skin] is what makes the
 * literal values take effect, and mc falls back to the 256-colour path by
 * itself on a terminal that cannot do better.
 *
 * The `ini` file is written too, because a skin nothing selects is a skin
 * nobody sees — and it is written with a MERGE, not a clobber: mc keeps
 * genuine user settings in there (panel modes, the editor's options), and the
 * same rule kdeglobals taught applies to any file an application writes back.
 */
/*
 * Set one key in one section of an ini file, keeping everything else.
 *
 * The same rule kdeglobals taught, applied to mc's `ini`: an application that
 * writes its own settings back into a file must not have them thrown away by a
 * theme generator. Only the named key in the named section is touched; the
 * section is created at the end if it is absent, and a file that does not exist
 * yet becomes a file with exactly that one section in it.
 */
static void kdt_ini_set(const char *path, const char *section, const char *key,
			const char *value)
{
	char *old = kb_read_all(path, NULL);
	KbBuf out = {0};
	bool in_section = false, done = false;
	size_t klen = strlen(key);

	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		size_t len = (size_t)(next - line);

		const char *t = line;
		while (t < line + len && (*t == ' ' || *t == '\t'))
			t++;

		if (*t == '[') {
			/* Leaving the section without having found the key: add
			 * it before the next header rather than at the end of
			 * the file, where it would land in somebody else's
			 * section. */
			if (in_section && !done) {
				kb_buf_printf(&out, "%s=%s\n", key, value);
				done = true;
			}
			in_section = !strncmp(t + 1, section, strlen(section)) &&
				     t[1 + strlen(section)] == ']';
		} else if (in_section && !done && !strncmp(t, key, klen)) {
			const char *after = t + klen;
			while (*after == ' ' || *after == '\t')
				after++;
			if (*after == '=') {
				kb_buf_printf(&out, "%s=%s\n", key, value);
				done = true;
				continue;
			}
		}
		kb_buf_add(&out, line, len);
	}

	if (!done) {
		/*
		 * A file whose last line has no trailing newline would otherwise
		 * get the new key concatenated onto it — `skin=kdos` welded to
		 * the end of whatever mc last wrote. Terminate first.
		 */
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		if (!in_section)
			kb_buf_printf(&out, "%s[%s]\n", out.n ? "\n" : "",
				      section);
		kb_buf_printf(&out, "%s=%s\n", key, value);
	}

	kb_write_all(path, out.p, out.n);
	kb_buf_free(&out);
	free(old);
}

static void write_mc(const KcolScheme *sc)
{
	/*
	 * `#rrggbb`, not mc's `rgb:` — there is no such spelling. mc accepts a
	 * colour name, `color0..255`, `rgbRGB` where each of R,G,B is a DIGIT
	 * 0-5 (the 216-entry cube), `grayN`, or `#rrggbb`. Only the last one can
	 * express this palette, and it is honoured only when [skin] says
	 * `truecolors=true` — spelled with the S, which is the whole reason the
	 * first version of this file rendered as mc's stock blue.
	 */
	char p[10], dim[10], sec[10], urg[10], deep[10], text[10], varied[10];
	char raw[8];
#define HX(field, out) do {                    \
		kcol_format((field), raw);     \
		snprintf((out), sizeof(out), "#%s", raw); \
	} while (0)
	HX(sc->primary, p);
	HX(sc->dim, dim);
	HX(sc->secondary, sec);
	HX(sc->urgent, urg);
	HX(sc->deep, deep);
	HX(sc->text, text);
	HX(sc->variant, varied);
#undef HX

	/*
	 * $XDG_DATA_HOME, not $XDG_CONFIG_HOME. mc looks for skins in
	 * <data>/mc/skins, /etc/mc/skins and /usr/share/mc/skins and nowhere
	 * else, so a skin written beside the config file is a skin mc will never
	 * find — it falls back to `default` and reports nothing.
	 */
	char *f = kdt_data_home("mc/skins/kdos.ini");
	mkparent(f);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS skin for Midnight Commander — GENERATED by `kdos theme`;\n"
		"# edits will be overwritten.\n"
		"[skin]\n"
		"description=KDOS\n"
		"truecolors=true\n"
		"\n"
		/*
		 * Literal UTF-8, not `\\u2500` escapes. mc parses this file with
		 * GLib's key-file reader, which rejects `\\u` as an invalid
		 * escape sequence and drops the whole group — the frames then
		 * come out as mc's fallback `+-|` ASCII.
		 */
		"[Lines]\n"
		"horiz=─\nvert=│\nlefttop=┌\nrighttop=┐\n"
		"leftbottom=└\nrightbottom=┘\n"
		"topmiddle=┬\nbottommiddle=┴\n"
		"leftmiddle=├\nrightmiddle=┤\ncross=┼\n"
		"dhoriz=═\ndvert=║\ndlefttop=╔\ndrighttop=╗\n"
		"dleftbottom=╚\ndrightbottom=╝\n"
		"dtopmiddle=╦\ndbottommiddle=╩\n"
		"dleftmiddle=╠\ndrightmiddle=╣\n"
		"\n"
		"[core]\n"
		"_default_=%s;%s\n"
		"selected=%s;%s\n"
		"marked=%s;%s\n"
		"markselect=%s;%s\n"
		"gauge=%s;%s\n"
		"input=%s;%s\n"
		"reverse=%s;%s\n"
		"header=%s;%s\n"
		"disabled=%s;%s\n"
		"\n"
		"[dialog]\n"
		"_default_=%s;%s\n"
		"dfocus=%s;%s\n"
		"dhotnormal=%s;%s\n"
		"dhotfocus=%s;%s\n"
		"dtitle=%s;%s\n"
		"\n"
		"[error]\n"
		"_default_=%s;%s\n"
		"errdhotnormal=%s;%s\n"
		"errdtitle=%s;%s\n"
		"\n"
		"[menu]\n"
		"_default_=%s;%s\n"
		"menusel=%s;%s\n"
		"menuhot=%s;%s\n"
		"menuhotsel=%s;%s\n"
		"menuinactive=%s;%s\n"
		"\n"
		"[buttonbar]\n"
		"# The F-key bar. `hotkey` is mc's name for the number; `button`\n"
		"# is the word beside it. It is the one row of mc that is read at\n"
		"# a glance rather than looked at, so it gets the least colour.\n"
		"button=%s;%s\n"
		"hotkey=%s;%s\n"
		"\n"
		"[statusbar]\n"
		"_default_=%s;%s\n"
		"\n"
		"[help]\n"
		"_default_=%s;%s\n"
		"helpitalic=%s;%s\n"
		"helpbold=%s;%s\n"
		"helplink=%s;%s\n"
		"helpslink=%s;%s\n"
		"\n"
		"[editor]\n"
		"_default_=%s;%s\n"
		"editbold=%s;%s\n"
		"editmarked=%s;%s\n"
		"editlinestate=%s;%s\n"
		"\n"
		"[viewer]\n"
		"_default_=%s;%s\n"
		"viewbold=%s;%s\n"
		"viewselected=%s;%s\n",
		/* core: 9 pairs */
		text, deep,   deep, p,      sec, deep,    deep, sec,
		deep, p,      text, varied, deep, p,      p, deep,
		dim, deep,
		/* dialog: 5 pairs */
		text, varied, deep, p,      p, varied,    deep, p,
		p, varied,
		/* error: 3 pairs */
		text, urg,    deep, urg,    text, urg,
		/* menu: 5 pairs */
		text, varied, deep, p,      p, varied,    deep, p,
		dim, varied,
		/* buttonbar: 2 pairs */
		text, deep,   dim, deep,
		/* statusbar: 1 pair */
		text, varied,
		/* help: 5 pairs */
		text, varied, sec, varied, p, varied,     p, varied,
		deep, p,
		/* editor: 4 pairs */
		text, deep,   p, deep,     deep, p,       dim, deep,
		/* viewer: 3 pairs */
		text, deep,   p, deep,     deep, p);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);

	/*
	 * Point mc at it. Merged rather than written: mc stores real user state
	 * in this file, and the lesson kdeglobals taught — replace only the keys
	 * you own — is not specific to KDE.
	 */
	char *ini = kdt_cfg_home("mc/ini");
	mkparent(ini);
	kdt_ini_set(ini, "Midnight-Commander", "skin", "kdos");
	free(ini);
}

static void write_btop(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("btop/themes/kdos.theme");
	mkparent(f);
	char p[8], dim[8], sec[8], urg[8], deep[8], text[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS btop theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"theme[main_bg]=\"#%s\"\ntheme[main_fg]=\"#%s\"\n"
		"theme[title]=\"#%s\"\ntheme[hi_fg]=\"#%s\"\n"
		"theme[selected_bg]=\"#%s\"\ntheme[selected_fg]=\"#%s\"\n"
		"theme[inactive_fg]=\"#%s\"\ntheme[graph_text]=\"#%s\"\n"
		"theme[proc_misc]=\"#%s\"\n"
		"theme[cpu_box]=\"#%s\"\ntheme[mem_box]=\"#%s\"\n"
		"theme[net_box]=\"#%s\"\ntheme[proc_box]=\"#%s\"\n"
		"theme[div_line]=\"#%s\"\n"
		"theme[temp_start]=\"#%s\"\ntheme[temp_mid]=\"#%s\"\n"
		"theme[temp_end]=\"#%s\"\n"
		"theme[cpu_start]=\"#%s\"\ntheme[cpu_mid]=\"#%s\"\n"
		"theme[cpu_end]=\"#%s\"\n"
		"theme[free_start]=\"#%s\"\ntheme[free_mid]=\"#%s\"\n"
		"theme[free_end]=\"#%s\"\n"
		"theme[cached_start]=\"#%s\"\ntheme[cached_mid]=\"#%s\"\n"
		"theme[cached_end]=\"#%s\"\n"
		"theme[available_start]=\"#%s\"\ntheme[available_mid]=\"#%s\"\n"
		"theme[available_end]=\"#%s\"\n"
		"theme[used_start]=\"#%s\"\ntheme[used_mid]=\"#%s\"\n"
		"theme[used_end]=\"#%s\"\n"
		"theme[download_start]=\"#%s\"\ntheme[download_mid]=\"#%s\"\n"
		"theme[download_end]=\"#%s\"\n"
		"theme[upload_start]=\"#%s\"\ntheme[upload_mid]=\"#%s\"\n"
		"theme[upload_end]=\"#%s\"\n",
		deep, text, p, p, dim, p, dim, text, sec,
		dim, dim, dim, dim, dim,
		p, sec, urg, p, sec, urg,
		dim, sec, p, dim, sec, p, dim, sec, p,
		dim, sec, urg, dim, sec, p, dim, sec, urg);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/*
 * KDE, which is the OTHER half of theming Qt.
 *
 * A KDE app reads its palette from ~/.config/kdeglobals, and the appbox shares
 * $HOME — so writing that file here is the whole bridge. No portal, no daemon,
 * no D-Bus: the same trick that already themes GTK apps through ~/.themes.
 *
 * Two files, answering different questions. `kdeglobals` is the palette in
 * FORCE; `~/.local/share/color-schemes/KDOS.colors` is the palette OFFERED,
 * which is what puts KDOS in a KDE app's own colour picker and what lets a user
 * come back to it after trying another.
 *
 * Colours are decimal `R,G,B` triples — that is KColorScheme's format, and a
 * hex string in these files reads as black.
 *
 * The selection background is `pdark`, never `primary`, for the same reason the
 * GTK generator uses it: a full-intensity #39ff14 fill under text is an
 * unreadable neon block.
 *
 * **kdeglobals is MERGED, not overwritten.** KDE apps write their own settings
 * into it — dolphin's view modes, kate's session state — and clobbering the file
 * on every `kdos theme` would throw those away every time an accent changed.
 * Only what this generator owns is replaced: the [Colors:*] and [WM] sections
 * outright, and four named keys elsewhere. Everything else in the file survives
 * verbatim, in its original order.
 */

#define KDE_MAX_KV 128

/* Leading and trailing blanks, in place. */
static char *kde_trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	char *e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		*--e = 0;
	return s;
}

typedef struct {
	const char *sec;
	char key[32];
	char val[24];
} KdeKV;

static void kde_add(KdeKV *kv, int *n, const char *sec, const char *key,
		    uint32_t rgb)
{
	if (*n >= KDE_MAX_KV)
		return;
	KcolRgb v = kcol_rgb(rgb);
	kv[*n].sec = sec;
	snprintf(kv[*n].key, sizeof(kv[*n].key), "%s", key);
	snprintf(kv[*n].val, sizeof(kv[*n].val), "%u,%u,%u", v.r, v.g, v.b);
	(*n)++;
}

static void kde_add_str(KdeKV *kv, int *n, const char *sec, const char *key,
			const char *val)
{
	if (*n >= KDE_MAX_KV)
		return;
	kv[*n].sec = sec;
	snprintf(kv[*n].key, sizeof(kv[*n].key), "%s", key);
	snprintf(kv[*n].val, sizeof(kv[*n].val), "%s", val);
	(*n)++;
}

/* The palette, as KColorScheme wants to read it. */
static int kde_palette(KdeKV *kv, const KcolScheme *sc)
{
	static const char *const SECS[] = {
		"Colors:View", "Colors:Window", "Colors:Button",
		"Colors:Selection", "Colors:Tooltip", "Colors:Complementary",
		"Colors:Header",
	};
	uint32_t view = sc->variant;
	uint32_t alt = kcol_mix(sc->variant, sc->text, 5);
	uint32_t btn = kcol_mix(sc->deep, sc->text, 9);
	uint32_t btnalt = kcol_mix(sc->deep, sc->text, 11);
	uint32_t hdr = kcol_mix(sc->deep, sc->text, 7);
	uint32_t tip = kcol_mix(sc->deep, sc->text, 13);
	uint32_t inact = kcol_mix(sc->text, sc->variant, 55);
	int n = 0;

	for (size_t i = 0; i < sizeof(SECS) / sizeof(SECS[0]); i++) {
		const char *s = SECS[i];
		uint32_t bg = view, bga = alt, fg = sc->text;

		if (!strcmp(s, "Colors:Window")) {
			bg = sc->deep;
			bga = view;
		} else if (!strcmp(s, "Colors:Button")) {
			bg = btn;
			bga = btnalt;
		} else if (!strcmp(s, "Colors:Selection")) {
			bg = bga = sc->pdark;
		} else if (!strcmp(s, "Colors:Tooltip")) {
			bg = bga = tip;
		} else if (!strcmp(s, "Colors:Complementary") ||
			   !strcmp(s, "Colors:Header")) {
			bg = bga = hdr;
		}

		kde_add(kv, &n, s, "BackgroundNormal", bg);
		kde_add(kv, &n, s, "BackgroundAlternate", bga);
		kde_add(kv, &n, s, "DecorationFocus", sc->primary);
		kde_add(kv, &n, s, "DecorationHover", sc->pdark);
		kde_add(kv, &n, s, "ForegroundNormal", fg);
		kde_add(kv, &n, s, "ForegroundInactive", inact);
		kde_add(kv, &n, s, "ForegroundActive", sc->primary);
		kde_add(kv, &n, s, "ForegroundLink", sc->primary);
		kde_add(kv, &n, s, "ForegroundVisited", sc->pdark);
		kde_add(kv, &n, s, "ForegroundNegative", sc->urgent);
		kde_add(kv, &n, s, "ForegroundNeutral", sc->secondary);
		kde_add(kv, &n, s, "ForegroundPositive", sc->primary);
	}

	kde_add(kv, &n, "WM", "activeBackground", hdr);
	kde_add(kv, &n, "WM", "activeForeground", sc->text);
	kde_add(kv, &n, "WM", "activeBlend", sc->primary);
	kde_add(kv, &n, "WM", "inactiveBackground", sc->deep);
	kde_add(kv, &n, "WM", "inactiveForeground", inact);
	kde_add(kv, &n, "WM", "inactiveBlend", inact);

	kde_add_str(kv, &n, "General", "ColorScheme", "KDOS");
	kde_add_str(kv, &n, "General", "Name", "KDOS");
	kde_add_str(kv, &n, "KDE", "widgetStyle", "Breeze");
	kde_add_str(kv, &n, "Icons", "Theme", "KDOS");
	return n;
}

/* What this generator owns: whole sections, and named keys in shared ones. */
static int kde_owns_section(const char *sec)
{
	return !strncmp(sec, "Colors:", 7) || !strcmp(sec, "WM");
}

static int kde_owns_key(const char *sec, const char *key)
{
	if (!strcmp(sec, "General"))
		return !strcmp(key, "ColorScheme") || !strcmp(key, "Name");
	if (!strcmp(sec, "KDE"))
		return !strcmp(key, "widgetStyle");
	if (!strcmp(sec, "Icons"))
		return !strcmp(key, "Theme");
	return 0;
}

static void kde_emit_section(KbBuf *b, const KdeKV *kv, int n, const char *sec)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(kv[i].sec, sec))
			kb_buf_printf(b, "%s=%s\n", kv[i].key, kv[i].val);
}


/*
 * Merge our keys into an existing kdeglobals.
 *
 * The file is NORMALISED rather than patched in place: sections in their
 * original order, foreign keys first, ours after, one blank line between. That
 * is what makes the result IDEMPOTENT — a patch-in-place version of this kept
 * moving [KDE] and re-emitting its own header comment, so `kdos theme` twice in
 * a row produced two different files and `--audit` had a permanent complaint.
 *
 * Blank lines and comments are dropped, which costs nothing: KConfig writes
 * neither, and every line this file cares about is `key=value`.
 */

/* The section a line belongs to, or "" for the lines before the first header. */
static void kde_line_section(const char *line, char *sec, size_t n)
{
	const char *end = strchr(line + 1, ']');
	size_t l = end ? (size_t)(end - line - 1) : strlen(line + 1);
	if (l >= n)
		l = n - 1;
	memcpy(sec, line + 1, l);
	sec[l] = 0;
}

/* Walk `old` line by line; `fn` is called with (section, trimmed line). */
typedef void (*kde_line_fn)(const char *sec, const char *line, void *user);

static void kde_walk(const char *old, kde_line_fn fn, void *user)
{
	char sec[64] = "";
	const char *p = old;

	while (p && *p) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		char line[512];

		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, p, len);
		line[len] = 0;
		p = nl ? nl + 1 : NULL;

		char *t = kde_trim(line);
		if (!*t || *t == '#')
			continue;
		if (*t == '[') {
			kde_line_section(t, sec, sizeof(sec));
			fn(sec, NULL, user);
			continue;
		}
		fn(sec, t, user);
	}
}

struct kde_secs {
	char name[64][64];
	int n;
};

static void kde_collect(const char *sec, const char *line, void *user)
{
	struct kde_secs *s = user;
	(void)line;
	if (kde_owns_section(sec))
		return;
	for (int i = 0; i < s->n; i++)
		if (!strcmp(s->name[i], sec))
			return;
	if (s->n < 64)
		snprintf(s->name[s->n++], 64, "%s", sec);
}

struct kde_emit {
	KbBuf *out;
	const char *want;
	int wrote;
};

static void kde_emit_foreign(const char *sec, const char *line, void *user)
{
	struct kde_emit *e = user;
	if (!line || strcmp(sec, e->want))
		return;
	/* Nothing in a section we own survives — not even a key we do not
	 * generate. Leaving the old [Colors:Window] keys in place is what made
	 * this file grow by one stale colour on every run. */
	if (kde_owns_section(sec))
		return;
	char key[64];
	const char *eq = strchr(line, '=');
	size_t kl = eq ? (size_t)(eq - line) : strlen(line);
	if (kl >= sizeof(key))
		kl = sizeof(key) - 1;
	memcpy(key, line, kl);
	key[kl] = 0;
	kde_trim(key);
	if (kde_owns_key(sec, key))
		return;
	kb_buf_printf(e->out, "%s\n", line);
	e->wrote = 1;
}

static void kde_merge(KbBuf *out, const char *old, const KdeKV *kv, int n)
{
	struct kde_secs secs = {0};
	int done[64] = {0};

	kde_walk(old, kde_collect, &secs);

	/*
	 * Two passes, and the split is what makes the result stable.
	 *
	 * A section that still carries something of the USER'S keeps its place
	 * in the file. A section that would contain only our keys goes at the
	 * end in generator order — wherever it happened to sit before. Emitting
	 * both in file order instead moved [KDE] and [Icons] up on the second
	 * run and down on the first, so `kdos theme` twice in a row produced two
	 * different files.
	 */
	for (int i = 0; i < secs.n; i++) {
		const char *sec = secs.name[i];
		KbBuf foreign = {0};
		struct kde_emit e = { &foreign, sec, 0 };

		if (!*sec)
			continue;	/* stray keys before any header */
		kde_walk(old, kde_emit_foreign, &e);
		if (!e.wrote) {
			kb_buf_free(&foreign);
			continue;
		}
		kb_buf_printf(out, "[%s]\n", sec);
		kb_buf_printf(out, "%.*s", (int)foreign.n, foreign.p);
		kde_emit_section(out, kv, n, sec);
		kb_buf_printf(out, "\n");
		kb_buf_free(&foreign);
		done[i] = 1;
	}

	for (int i = 0; i < n; i++) {
		int already = 0;
		for (int j = 0; j < secs.n; j++)
			if (!strcmp(secs.name[j], kv[i].sec) && done[j])
				already = 1;
		for (int j = 0; j < i; j++)
			if (!strcmp(kv[j].sec, kv[i].sec))
				already = 1;
		if (already)
			continue;
		kb_buf_printf(out, "[%s]\n", kv[i].sec);
		kde_emit_section(out, kv, n, kv[i].sec);
		kb_buf_printf(out, "\n");
	}
}

static void write_kde(const KcolScheme *sc)
{
	KdeKV kv[KDE_MAX_KV];
	int n = kde_palette(kv, sc);
	char *colors = kdt_data_home("color-schemes/KDOS.colors");
	char *globals = kdt_cfg_home("kdeglobals");
	KbBuf b = {0};

	/* The offered scheme: ours alone, so it is written whole. */
	kb_buf_printf(&b,
		"# KDOS colour scheme — GENERATED by `kdos theme`; edits will "
		"be overwritten.\n");
	{
		char last[64] = "";
		for (int i = 0; i < n; i++) {
			if (!strcmp(kv[i].sec, "Icons") ||
			    !strcmp(kv[i].sec, "KDE"))
				continue;	/* not part of a colour scheme */
			if (strcmp(last, kv[i].sec)) {
				kb_buf_printf(&b, "\n[%s]\n", kv[i].sec);
				snprintf(last, sizeof(last), "%s", kv[i].sec);
			}
			kb_buf_printf(&b, "%s=%s\n", kv[i].key, kv[i].val);
		}
	}
	mkparent(colors);
	kb_write_all(colors, b.p, b.n);
	kb_buf_free(&b);

	/* The applied palette: merged, because KDE apps write here too. */
	size_t len = 0;
	char *old = kb_read_all(globals, &len);
	kb_buf_printf(&b,
		"# KDOS: the [Colors:*] and [WM] sections and the four keys "
		"below them are\n"
		"# GENERATED by `kdos theme`. Everything else in this file is "
		"yours and is kept.\n");
	kde_merge(&b, old ? old : "", kv, n);
	free(old);
	mkparent(globals);
	kb_write_all(globals, b.p, b.n);
	kb_buf_free(&b);

	free(colors);
	free(globals);
}

/* Everything between the two markers is replaced wholesale. A file with no
 * markers is left exactly as it is. */
static void write_starship(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("starship.toml");
	size_t len = 0;
	char *data = kb_read_all(f, &len);
	if (!data) {
		free(f);
		return;
	}

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);

	KbBuf out = {0};
	int inpal = 0;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		size_t n = nl ? (size_t)(nl - line) : strlen(line);

		int open = !strncmp(line, "# >>> NOCTALIA STARSHIP PALETTE >>>", 35) ||
			   !strncmp(line, "# >>> KDOS STARSHIP PALETTE >>>", 31);
		int close = !strncmp(line, "# <<< NOCTALIA STARSHIP PALETTE <<<", 35) ||
			    !strncmp(line, "# <<< KDOS STARSHIP PALETTE <<<", 31);

		if (open) {
			inpal = 1;
			kb_buf_printf(&out,
				"# >>> KDOS STARSHIP PALETTE >>>\n"
				"# Regenerated by `kdos theme`. Everything between the markers is replaced\n"
				"# wholesale — do not edit by hand.\n"
				"[palettes.kdos]\n"
				"blue      = \"#%s\"\nred       = \"#%s\"\n"
				"green     = \"#%s\"\nyellow    = \"#%s\"\n"
				"cyan      = \"#%s\"\nmagenta   = \"#%s\"\n"
				"white     = \"#%s\"\nblack     = \"#%s\"\n"
				"text      = \"#%s\"\nsubtext1  = \"#%s\"\n"
				"subtext0  = \"#%s\"\noverlay2  = \"#%s\"\n"
				"overlay1  = \"#%s\"\noverlay0  = \"#%s\"\n"
				"surface2  = \"#%s\"\nsurface1  = \"#%s\"\n"
				"surface0  = \"#%s\"\nbase      = \"#%s\"\n"
				"mantle    = \"#%s\"\ncrust     = \"#%s\"\n"
				"rosewater = \"#%s\"\nflamingo  = \"#%s\"\n"
				"pink      = \"#%s\"\nmauve     = \"#%s\"\n"
				"maroon    = \"#%s\"\npeach     = \"#%s\"\n"
				"teal      = \"#%s\"\nsky       = \"#%s\"\n"
				"sapphire  = \"#%s\"\nlavender  = \"#%s\"\n"
				"# <<< KDOS STARSHIP PALETTE <<<\n",
				p, urg, sec, sec, sec, sec, text, deep,
				text, dim, dim, dim, dim, var,
				var, deep, deep, deep, deep, deep,
				sec, sec, sec, p, urg, sec, sec, sec, sec, p);
			continue;
		}
		if (close) {
			inpal = 0;
			continue;
		}
		if (inpal)
			continue;

		kb_buf_add(&out, line, n);
		if (nl)
			kb_buf_add(&out, "\n", 1);
	}

	kb_write_all(f, out.p, out.n);
	kb_buf_free(&out);
	free(data);
	free(f);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Everything an accent switch produces, and nothing else — no state file, no
 * signals. `kdos theme --audit` runs exactly this into a scratch $HOME, which
 * only works because it is one function with no side effects beyond the files. */
static void theme_apply(const KcolScheme *sc)
{
	write_gtk(sc);
	write_icons(sc);
	write_cursors(sc);
	write_kde(sc);
	write_foot(sc);
	write_btop(sc);
	write_mc(sc);
	write_starship(sc);
}

static int cmd_theme(int argc, char **argv)
{
	const char *cur = current_theme();
	const char *want = argc > 0 ? argv[0] : "";

	if (!strcmp(want, "--audit") || !strcmp(want, "audit")) {
		/* An accent may follow: `kdos theme --audit amber` asks what
		 * would have to change for amber, which is how you check a switch
		 * before making it. Without one it audits what is in force. */
		const char *which = argc > 1 ? argv[1] : cur;
		const KcolScheme *sc = kcol_find(which);
		if (!sc)
			kb_die("unknown theme '%s' (try: kdos theme list)", which);
		return kdt_theme_audit(sc, theme_apply, C_A, C_0);
	}

	if (!*want || !strcmp(want, "show") || !strcmp(want, "current")) {
		printf("%s\n", cur);
		return 0;
	}
	if (!strcmp(want, "list")) {
		for (int i = 0; i < kcol_nscheme; i++) {
			const KcolScheme *s = &kcol_schemes[i];
			if (!strcmp(s->name, cur))
				printf("%s* %-10s%s %s\n", C_A, s->name, C_0,
				       s->theme_name);
			else
				printf("  %-10s %s\n", s->name, s->theme_name);
		}
		return 0;
	}

	char pick[32];
	if (!strcmp(want, "next") || !strcmp(want, "prev")) {
		int at = 0;
		for (int i = 0; i < kcol_nscheme; i++)
			if (!strcmp(kcol_schemes[i].name, cur))
				at = i;
		int step = !strcmp(want, "next") ? 1 : kcol_nscheme - 1;
		kb_strlcpy(pick, kcol_schemes[(at + step) % kcol_nscheme].name,
			   sizeof(pick));
		want = pick;
	}

	const KcolScheme *sc = kcol_find(want);
	if (!sc)
		kb_die("unknown theme '%s' (try: kdos theme list)", want);

	theme_apply(sc);

	/* The state file is the desktop's ONLY input, so it is written before
	 * the session is signalled — a SIGHUP that arrives first would make the
	 * shell re-read the accent it already had. */
	char *state = cache_home("kdos/theme");
	mkparent(state);
	char line[40];
	snprintf(line, sizeof(line), "%s\n", sc->name);
	kb_write_file(state, line);
	free(state);

	reload_session();

	/* A regenerated file does not repaint a running process. kdos-shell and
	 * kdos-comp retint on the SIGHUP above; starship on the next prompt;
	 * btop and foot on next start (foot cannot reload its config at all).
	 * GTK apps — every alien app — pick up the new theme and icons when
	 * they are next launched; GTK re-reads neither on a file change. */
	if (kb_have_prog("tmux")) {
		char *conf = kdt_cfg_home("tmux/tmux.conf");
		KbArgv a = {0};
		kb_argv_add(&a, "tmux");
		kb_argv_add(&a, "source-file");
		kb_argv_add(&a, conf);
		kb_argv_end(&a);
		kb_run(&a);
		free(conf);
	}

	printf("%s%s%s (%s)\n", C_A, sc->name, C_0, sc->theme_name);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void help_body(FILE *o)
{
	fprintf(o, "%s", C_A);
	fputs("██╗  ██╗██████╗  ██████╗ ███████╗\n"
	      "██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝\n"
	      "█████╔╝ ██║  ██║██║   ██║███████╗\n"
	      "██╔═██╗ ██║  ██║██║   ██║╚════██║\n"
	      "██║  ██╗██████╔╝╚██████╔╝███████║\n"
	      "╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝\n", o);
	fprintf(o, "%s   KD's Homebrew Linux Distro%s\n\n", C_D, C_0);

	fprintf(o, "%sCOMMANDS%s\n", C_A, C_0);
	static const char *CMDS[][2] = {
		{ "why <path|port>", "what provides this, and why it is that way" },
		{ "explain [topic]", "the recorded debug cycles, browsable" },
		{ "sandbox <prof> -- <cmd>", "run a native app under Landlock" },
		{ "desktop", "start the KDOS desktop from a tty (kdos-desktop)" },
		{ "kdos app <name>", "install an alien app (distrobox + export)" },
		{ "kdos theme [name]", "phosphor | amber | ice | bone | next | prev | list" },
		{ "kdos theme --audit", "is every generated colour still the palette's?" },
		{ "kdos status", "packages, containers, exported apps" },
		{ "kdos doctor", "check the session for common breakage" },
		{ "kdos appid", "do launcher icons match the windows they open?" },
		{ "kdos restarts", "what is running code an upgrade replaced" },
		{ "kdos stutter", "why the desktop hiccuped — with the app's name" },
		{ "kdos-shot [region]", "screenshot to clipboard and ~/Pictures" },
		{ "kdos-fetch-static", "fetch a single verified static binary" },
		{ "kdos-power suspend", "suspend; also poweroff and reboot" },
		{ "kdos-energy", "which app is spending the battery" },
		{ "sudo kinstall", "install this live image onto a disk" },
		{ NULL, NULL }
	};
	for (int i = 0; CMDS[i][0]; i++)
		fprintf(o, "  %-26s %s\n", CMDS[i][0], CMDS[i][1]);
	fputc('\n', o);

	fprintf(o, "%sKEYS%s  %s(defaults — remap in "
		"~/.config/kdos-comp/rc.xml)%s\n", C_A, C_0, C_D, C_0);
	/* Every line here is a binding the skel rc.xml actually installs, and
	 * the file named above is the one that installs it: since the labwc
	 * fork, comp.conf keeps only the KDOS keys and skips a `bind` line in
	 * silence, so pointing a remapper at it was pointing them at a file
	 * that would ignore them. The list used to carry four keys that
	 * nothing bound — Alt+Tab, the snap arrows, Super+F, PrtSc — which is
	 * worse than a short cheat sheet: a key that the help says exists and
	 * the desktop ignores reads as a broken desktop. Super+F is bound now
	 * and is back on the list for that reason, not for the old one. */
	static const char *KEYS[][2] = {
		{ "Super+D", "open the launcher" },
		{ "Super+Return", "terminal (foot)" },
		{ "Super+Q", "close window" },
		{ "Super+Tab", "switch window (most recent first)" },
		{ "Super+Shift+Tab", "switch window, backwards" },
		{ "Super+M", "maximize / restore" },
		{ "Super+F", "fullscreen / restore" },
		{ "Super+1..4", "switch workspace" },
		{ "Super+Shift+1..4", "move window to workspace" },
		{ "Super+drag", "move a window; Super+right-drag resizes" },
		{ "Super+L", "lock the screen" },
		{ "Super+Escape", "end the session" },
		{ "Volume / Brightness", "the media keys, with an on-screen gauge" },
		/* fcitx5's own binding, not kdos-comp's — but it is the one key
		 * a CJK user needs and nothing else on the machine tells them
		 * about it. Listed only because the compositor now speaks
		 * text-input-v3, so it works wherever fcitx5 is installed. */
		{ "Ctrl+Space", "switch input method (fcitx5, if installed)" },
		{ NULL, NULL }
	};
	for (int i = 0; KEYS[i][0]; i++)
		fprintf(o, "  %s%-22s%s %s\n", C_B, KEYS[i][0], C_0, KEYS[i][1]);
}

/*
 * Rendered to a buffer and fed to the pager's stdin, rather than written into
 * a popen(). popen() runs `/bin/sh -c`, and this tree's rule is that no C
 * program here spawns a shell — not because this particular string could be
 * attacked (it could not) but because a shell in the tree is a place the next
 * person puts a variable. kdosbuild's `O` key already opens $PAGER by argv;
 * this is the same answer, and it gains $PAGER along the way — `less` was
 * hardcoded, so a machine whose owner prefers something else got less anyway.
 */
static int cmd_help(int argc, char **argv)
{
	const char *pager = getenv("PAGER");
	if (!pager || !*pager)
		pager = "less";

	if (argc > 0 && !strcmp(argv[0], "--pager") && kb_have_prog(pager)) {
		char *text = NULL;
		size_t len = 0;
		FILE *m = open_memstream(&text, &len);
		if (m) {
			help_body(m);
			fclose(m);
			KbArgv a = {0};
			kb_argv_add(&a, pager);
			/* -R only if it IS less: another pager may not have it,
			 * and an unknown flag is an error rather than colour. */
			if (!strcmp(pager, "less"))
				kb_argv_add(&a, "-R");
			kb_argv_end(&a);
			int rc = kb_run_feed_tty(&a, text, len);
			free(text);
			if (rc == 0)
				return 0;
		}
	}
	help_body(stdout);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Is this process up? Defined with doctor's other probes below; declared here
 * because `kdos status` names the compositor and asks the same question. */
static int running(const char *exact, const char *contains);

/* Non-empty, non-comment lines. The alien-apps table is one line per app. */
static int count_lines(const char *path)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	int n = 0;

	if (!data)
		return 0;
	for (char *p = data; *p;) {
		char *nl = strchr(p, '\n');
		if (*p != '\n' && *p != '#')
			n++;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return n;
}

static int count_dir(const char *path, const char *suffix)
{
	int n = 0, total = 0;
	char **v = kb_listdir(path, &total);
	for (char **p = v; p && *p; p++) {
		if (!suffix) {
			n++;
			continue;
		}
		size_t a = strlen(*p), b = strlen(suffix);
		if (a >= b && !strcmp(*p + a - b, suffix))
			n++;
	}
	kb_strv_free(v);
	return n;
}

static int count_containers(void)
{
	if (!kb_have_prog("podman"))
		return 0;
	char buf[1 << 15];
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || !buf[0])
		return 0;
	int n = 1;
	for (char *p = buf; *p; p++)
		if (*p == '\n')
			n++;
	return n;
}

static int cmd_status(int argc, char **argv)
{
	char *apps = kdt_data_home("applications");

	if (argc > 0 && !strcmp(argv[0], "--bar")) {
		printf("%d box · %d app\n", count_containers(),
		       count_dir(apps, ".desktop"));
		free(apps);
		return 0;
	}

	struct utsname u;
	uname(&u);
	printf("%sKDOS%s  %s %s\n\n", C_A, C_0, u.sysname, u.release);

	printf("%s%-16s%s %s\n", C_B, "Theme", C_0, current_theme());
	printf("%s%-16s%s %d\n", C_B, "Packages", C_0,
	       count_dir("/var/lib/kpkg/db", NULL));
	printf("%s%-16s%s %d\n", C_B, "Exported apps", C_0,
	       count_dir(apps, ".desktop"));
	/* The alien apps are a baked table, so this is a file read rather than a
	 * podman call — and it answers on a machine where the box has never been
	 * created, which is every machine before the first launch. */
	printf("%s%-16s%s %d\n", C_B, "Alien apps", C_0,
	       count_lines("/usr/share/kdos/alien-apps"));
	/* Naming the compositor rather than saying "wayland": on KDOS the
	 * answer is normally kdos-comp, and "wayland" would hide the case where
	 * the session is something else entirely. */
	printf("%s%-16s%s %s\n", C_B, "Session", C_0,
	       !getenv("WAYLAND_DISPLAY") ? "tty"
	       : running("kdos-comp", NULL) ? "kdos-comp"
					  : "wayland (not kdos-comp)");
	putchar('\n');

	printf("%sCONTAINERS%s\n", C_A, C_0);
	if (!kb_have_prog("podman")) {
		printf("  podman not installed\n");
	} else {
		char buf[1 << 15];
		KbArgv a = {0};
		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "ps");
		kb_argv_add(&a, "-a");
		kb_argv_add(&a, "--format");
		kb_argv_add(&a, "  {{.Names}}\t{{.Status}}\t{{.Image}}");
		kb_argv_end(&a);
		if (kb_run_capture(&a, buf, sizeof(buf)) == 0 && buf[0])
			printf("%s\n", buf);
		else
			printf("  none — try: kdos app gimp\n");
	}
	putchar('\n');

	printf("%sEXPORTED APPS%s\n", C_A, C_0);
	int found = 0;
	char **v = kb_listdir(apps, NULL);
	for (char **p = v; p && *p; p++) {
		size_t n = strlen(*p);
		if (n < 9 || strcmp(*p + n - 8, ".desktop"))
			continue;
		found = 1;
		printf("  %.*s\n", (int)(n - 8), *p);
	}
	kb_strv_free(v);
	if (!found)
		printf("  none\n");

	free(apps);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void ok(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void warn_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Doctor's two funnels, and `--json` is a mode on them rather than a second
 * pass over the same checks. Every check reports through exactly one of these,
 * so the machine-readable output cannot drift from what a human is told — which
 * is the entire point of N15 and the reason it is worth doing at all.
 */
static int doctor_json;		/* emitting records rather than lines */
static const char *doctor_section = "";
static int doctor_first = 1;
static int doctor_warns;

static void doctor_record(const char *level, const char *fmt, va_list ap)
{
	char msg[512];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	KbBuf b = {0};
	kb_buf_printf(&b, "%s\n    {\"section\": ", doctor_first ? "" : ",");
	kb_json_str(&b, doctor_section);
	kb_buf_str(&b, ", \"level\": ");
	kb_json_str(&b, level);
	kb_buf_str(&b, ", \"message\": ");
	kb_json_str(&b, msg);
	kb_buf_str(&b, "}");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	doctor_first = 0;
}

static void ok(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("ok", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[ ok ]%s ", C_A, C_0);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

static void warn_(const char *fmt, ...)
{
	va_list ap;
	doctor_warns++;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("warn", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[warn]%s ", C_W, C_0);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

/* A section header in text mode; a field on every record in JSON mode. A
 * machine consumer wants the grouping attached to the item, not emitted as a
 * separate thing it has to remember. */
static void doctor_head(const char *name)
{
	doctor_section = name;
	if (!doctor_json)
		printf("%s%s%s\n", C_B, name, C_0);
}

/* The blank line between sections is text-mode furniture. Left unguarded it
 * lands in the middle of the JSON array — which still parses, and looks like a
 * bug to anyone reading the output. */
static void doctor_gap(void)
{
	if (!doctor_json)
		putchar('\n');
}

/* `grep -q "^<prefix>"` — line-anchored, which matters: a bare substring
 * search would let a user named "os" match the line for "kdos". */
static int has_line_prefix(const char *path, const char *prefix)
{
	size_t n = 0, plen = strlen(prefix);
	char *data = kb_read_all(path, &n);
	if (!data)
		return 0;
	int hit = 0;
	for (char *line = data; line && *line && !hit;) {
		char *nl = strchr(line, '\n');
		if (!strncmp(line, prefix, plen))
			hit = 1;
		line = nl ? nl + 1 : NULL;
	}
	free(data);
	return hit;
}

static int running(const char *exact, const char *contains)
{
	char buf[4096];
	KbArgv a = {0};
	kb_argv_add(&a, "pgrep");
	if (exact) {
		kb_argv_add(&a, "-x");
		kb_argv_add(&a, exact);
	} else {
		kb_argv_add(&a, "-f");
		kb_argv_add(&a, contains);
	}
	kb_argv_end(&a);
	return kb_run_capture(&a, buf, sizeof(buf)) == 0 && buf[0];
}

/* The kernel reports its Landlock ABI through the create syscall itself:
 * a NULL attr with LANDLOCK_CREATE_RULESET_VERSION (1<<0) returns the version
 * rather than a ruleset fd. musl declares __NR_landlock_create_ruleset (444)
 * and needs no wrapper, so this costs nothing to ask.
 *
 * -EOPNOTSUPP means the LSM is compiled in but not enabled in CONFIG_LSM or
 * lsm=, which is the failure worth naming: everything silently degrades to
 * "no sandbox" and nothing says so. */
static int landlock_abi(void)
{
	long v = syscall(__NR_landlock_create_ruleset, NULL, 0, 1UL);
	return v < 0 ? -errno : (int)v;
}

static int blob_has(const char *hay, size_t n, const char *needle)
{
	size_t m = strlen(needle);
	if (m == 0 || m > n)
		return 0;
	for (size_t i = 0; i + m <= n; i++)
		if (hay[i] == needle[0] && !memcmp(hay + i, needle, m))
			return 1;
	return 0;
}

/* The early microcode loader never touches a filesystem: before anything is
 * decompressed it scans the raw initrd for one literal path. So the honest
 * check is that same search against the image this machine actually boots.
 * CONFIG_MICROCODE_LATE_LOADING is off, so a rebuild that lost the ucode cpio
 * has no second chance and no symptom — the CPU just keeps running whatever
 * revision its firmware loaded, which is the state the errata are written
 * about. */
static void check_microcode(void)
{
	char *cpu = kb_read_all("/proc/cpuinfo", NULL);
	const char *want = NULL, *vendor = NULL;
	if (cpu && strstr(cpu, "GenuineIntel")) {
		want = "kernel/x86/microcode/GenuineIntel.bin";
		vendor = "Intel";
	} else if (cpu && strstr(cpu, "AuthenticAMD")) {
		want = "kernel/x86/microcode/AuthenticAMD.bin";
		vendor = "AMD";
	}
	free(cpu);
	if (!want) {
		warn_("unknown CPU vendor — cannot say which microcode applies");
		return;
	}

	char note[96] = "";
	char *v = kb_read_all("/sys/devices/system/cpu/cpu0/microcode/version",
			      NULL);
	if (v) {
		char rev[32];
		kb_strlcpy(rev, v, sizeof(rev));
		rev[strcspn(rev, "\r\n")] = '\0';
		if (rev[0])
			snprintf(note, sizeof(note), " (revision %s)", rev);
		free(v);
	}

	/* KDOS_INITRD moves the image the same way KDOS_PRIVACY_PROC moves the
	 * /proc walk: it is what lets selftest.sh assert both answers on a
	 * machine whose own /boot is not the subject. */
	const char *path = getenv("KDOS_INITRD");
	if (!path || !*path)
		path = "/boot/initramfs.cpio.gz";

	size_t n = 0;
	char *img = kb_read_all(path, &n);
	if (!img) {
		warn_("no %s — cannot tell whether microcode is carried", path);
		return;
	}
	int carried = blob_has(img, n, want);
	free(img);

	if (carried)
		ok("%s microcode in the initrd%s", vendor, note);
	else
		warn_("the initrd carries no %s microcode — this CPU runs "
		      "whatever the firmware loaded%s", vendor, note);
}

static int cmd_doctor(int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			doctor_json = 1;
		else if (!strcmp(argv[i], "--cve")) {
			/*
			 * Delegated rather than inlined. The CVE answer is a
			 * table of findings with its own exit code and its own
			 * vintage to quote, and folding it into doctor's
			 * ok/warn lines would flatten "17 packages are behind a
			 * recorded fix" into one warning that says nothing.
			 */
			int rest = argc - i - 1;
			return kdt_cve(rest, argv + i + 1, C_A, C_W, C_0);
		} else {
			fprintf(stderr, "usage: kdos doctor [--json] [--cve]\n");
			return 2;
		}
	}

	if (doctor_json)
		printf("{\n  \"checks\": [");
	else
		printf("%sKDOS doctor%s\n\n", C_A, C_0);

	doctor_head("Kernel");
	int abi = landlock_abi();
	if (abi > 0)
		ok("Landlock ABI %d", abi);
	else if (abi == -ENOSYS)
		warn_("no Landlock — kernel too old or CONFIG_SECURITY_LANDLOCK off");
	else
		warn_("Landlock present but disabled — add it to CONFIG_LSM or "
		      "the lsm= cmdline");
	check_microcode();
	doctor_gap();

	doctor_head("Session");
	/*
	 * The SOCKET, not the variable. A variable that outlived its compositor
	 * is the state this check exists to catch — a session that died leaves
	 * WAYLAND_DISPLAY behind in every shell that inherited it, and a doctor
	 * that reports it green sends the user looking anywhere but here.
	 */
	const char *wd = getenv("WAYLAND_DISPLAY");
	const char *wl_rt = getenv("XDG_RUNTIME_DIR");
	if (!wd || !*wd) {
		warn_("no WAYLAND_DISPLAY — not inside the desktop session");
	} else {
		char sock[512];
		if (*wd == '/')
			snprintf(sock, sizeof(sock), "%s", wd);
		else
			snprintf(sock, sizeof(sock), "%s/%s",
				 wl_rt && *wl_rt ? wl_rt : "/run/user/1000", wd);
		if (kb_path_exists(sock))
			ok("WAYLAND_DISPLAY=%s", wd);
		else
			warn_("WAYLAND_DISPLAY=%s but %s does not exist — the "
			      "compositor it names is gone", wd, sock);
	}

	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (rt && *rt && kb_is_dir(rt))
		ok("XDG_RUNTIME_DIR=%s", rt);
	else
		warn_("XDG_RUNTIME_DIR missing — pipewire and podman will "
		      "misbehave");

	if (running("kdos-comp", NULL))
		ok("kdos-comp running");
	else
		warn_("kdos-comp not running — no desktop (start with: "
		      "kdos-desktop)");
	if (running("kdos-shell", NULL))
		ok("kdos-shell running");
	else
		warn_("kdos-shell not running — no panel or launcher");
	if (running(NULL, "xdg-desktop-portal-wlr"))
		ok("wlr portal running");
	else
		warn_("wlr portal not running — screen capture and file pickers "
		      "degraded");
	doctor_gap();

	doctor_head("Containers");
	/* The failure this distro actually had: toybox switch_root never
	 * MS_MOVEs the root, so anything that JOINS a mount namespace lands on
	 * an empty tree. */
	char root[256] = {0};
	ssize_t rn = readlink("/proc/self/root", root, sizeof(root) - 1);
	if (rn > 0)
		root[rn] = 0;
	if (rn <= 0 || !strcmp(root, "/"))
		ok("mount namespace root is /");
	else
		warn_("mount namespace root is %s — podman exec and distrobox "
		      "enter will fail", root);

	if (geteuid() != 0) {
		const char *user = getenv("USER");
		char want[80];
		snprintf(want, sizeof(want), "%s:", user && *user ? user : "kdos");
		if (has_line_prefix("/etc/subuid", want))
			ok("subuid mapping present");
		else
			warn_("no /etc/subuid entry — rootless podman cannot map "
			      "users");
		if (has_line_prefix("/etc/subgid", want))
			ok("subgid mapping present");
		else
			warn_("no /etc/subgid entry");
	} else {
		warn_("running as root — alien apps are meant to run as the kdos "
		      "user");
	}
	doctor_gap();

	doctor_head("Desktop");
	/* The accent NAME in the cache is what kdos-comp and kdos-shell read;
	 * they carry the palette itself in libkcolor. No colours are written
	 * for the desktop, so this file is the whole of its theme state. */
	char *ct = cache_home("kdos/theme");
	if (kb_path_exists(ct))
		ok("accent applied (%s)", current_theme());
	else
		warn_("no accent applied — run: kdos theme phosphor");
	free(ct);

	char *ft = kdt_cfg_home("foot/themes/kdos");
	if (kb_path_exists(ft))
		ok("foot theme present");
	else
		warn_("foot theme missing — run: kdos theme phosphor");
	free(ft);

	/*
	 * The KDE bridge is one file in the shared home: a boxed dolphin with no
	 * kdeglobals is a grey dolphin, and that looks like a broken image
	 * rather than a missing config. Checked by CONTENT, because a kdeglobals
	 * written by a KDE app itself exists and says nothing about our palette.
	 */
	char *kg = kdt_cfg_home("kdeglobals");
	char *kgd = kb_read_all(kg, NULL);
	if (kgd && strstr(kgd, "ColorScheme=KDOS"))
		ok("boxed KDE apps read the KDOS palette (kdeglobals)");
	else if (kgd)
		warn_("kdeglobals is not wearing the KDOS scheme — run: "
		      "kdos theme %s", current_theme());
	else
		warn_("no kdeglobals — KDE apps in the box will be grey (run: "
		      "kdos theme %s)", current_theme());
	free(kgd);
	free(kg);

	/*
	 * The SOCKET again, and for the opposite reason: kdos-comp exports
	 * DISPLAY only into what IT spawned, so a shell reached over ssh or from
	 * another VT has none while Xwayland is perfectly alive. kdos-appbox
	 * already probes /tmp/.X11-unix for exactly this — reading getenv here
	 * warned about a working Xwayland on every session.
	 */
	const char *disp = getenv("DISPLAY");
	char xsock[32] = {0};
	DIR *xd = opendir("/tmp/.X11-unix");
	if (xd) {
		struct dirent *xe;
		while ((xe = readdir(xd)))
			if (xe->d_name[0] == 'X' && xe->d_name[1]) {
				snprintf(xsock, sizeof(xsock), ":%.30s",
					 xe->d_name + 1);
				break;
			}
		closedir(xd);
	}
	if (disp && *disp)
		ok("Xwayland on %s", disp);
	else if (*xsock)
		ok("Xwayland on %s (not in this shell's env — the compositor "
		   "exports DISPLAY only to what it spawned)", xsock);
	else
		warn_("no DISPLAY and no /tmp/.X11-unix socket — X11-only alien "
		      "apps will not start");

	/*
	 * The lock screen's two halves, and the worst failure mode in the
	 * system: kdos-checkpass without its setuid bit cannot read
	 * /etc/shadow, so every password is refused and the session locks you
	 * OUT — recoverable only from another VT. A packaging mistake, an rsync
	 * without -p or a restore from a tarball all lose that bit silently,
	 * which is why it is checked here rather than trusted.
	 */
	struct stat cst;
	if (stat("/usr/bin/kdos-checkpass", &cst) != 0)
		warn_("kdos-checkpass missing — the lock screen can never "
		      "unlock");
	else if ((cst.st_mode & S_ISUID) && cst.st_uid == 0)
		ok("kdos-checkpass is setuid root");
	else
		warn_("kdos-checkpass is not setuid root — every password will "
		      "be refused (fix: chown root and chmod 4755)");

	if (kb_path_exists("/run/kdos-powerd.sock"))
		ok("kdos-powerd listening");
	else
		warn_("kdos-powerd not running — no suspend, poweroff or reboot "
		      "from the desktop (start with: service kdos-powerd start)");

	/*
	 * kdos-energyd, and WHY it is not running is the whole of what is worth
	 * reporting. A machine with no RAPL at all — most VMs — is a machine
	 * where per-app energy cannot be measured by anyone, and saying that is
	 * a different answer from "the daemon is not started".
	 */
	if (kb_path_exists("/run/kdos-energyd.sock")) {
		ok("kdos-energyd sampling (kdos-energy)");
	} else {
		int nrapl = 0;
		char **dom = kb_listdir("/sys/class/powercap", &nrapl);
		int any = 0;
		for (int i = 0; i < nrapl; i++) {
			char *e = kb_path_join("/sys/class/powercap", dom[i]);
			char *f = kb_path_join(e, "energy_uj");
			any |= kb_path_exists(f);
			free(f);
			free(e);
		}
		kb_strv_free(dom);
		if (any)
			warn_("kdos-energyd not running — no per-app energy "
			      "attribution (start with: service kdos-energyd "
			      "start)");
		else
			warn_("no RAPL energy domain on this machine — per-app "
			      "energy cannot be measured here at all");
	}

	/*
	 * The input method. Two halves, and only reporting both distinguishes
	 * "no IME installed" from "installed and not running" — which look
	 * identical from a text field that will not accept CJK.
	 */
	if (kb_have_prog("fcitx5")) {
		KbArgv pg = {0};
		kb_argv_add(&pg, "pgrep");
		kb_argv_add(&pg, "-x");
		kb_argv_add(&pg, "fcitx5");
		kb_argv_end(&pg);
		char out[64] = {0};
		if (kb_run_capture(&pg, out, sizeof(out)) == 0 && *out)
			ok("fcitx5 is running — Ctrl+Space switches input method");
		else
			warn_("fcitx5 is installed but not running — no CJK input "
			      "this session (kdos-desktop-start launches it)");
	}

	/* The frame-timing socket. Absent means `kdos stutter` has nothing to
	 * watch — which is normal outside a session and worth saying inside one,
	 * because the alternative is a tool that appears to hang. */
	const char *rtd = getenv("XDG_RUNTIME_DIR");
	if (rtd && *rtd) {
		char *fs = kb_path_join(rtd, "kdos-frames.sock");
		if (kb_path_exists(fs))
			ok("kdos-comp is reporting frame timing (kdos stutter)");
		else
			warn_("no frame-timing socket — `kdos stutter` has "
			      "nothing to watch (is kdos-comp this session's "
			      "compositor?)");
		free(fs);
	}

	/*
	 * The screen-capture portal, which is two files and one naming rule.
	 * xdg-desktop-portal picks a backend by reading <desktop>-portals.conf
	 * with XDG_CURRENT_DESKTOP lowercased, and wlr.portal's own UseIn= list
	 * has never heard of KDOS — so without that file ScreenCast comes back
	 * with no backend and the app says "no capture sources available",
	 * which sounds like a driver problem and is not.
	 */
	if (!kb_path_exists("/usr/lib/xdg-desktop-portal-wlr"))
		warn_("xdg-desktop-portal-wlr missing — no screen sharing and "
		      "no screenshot portal for boxed apps");
	else if (kb_path_exists("/usr/share/xdg-desktop-portal/kdos-portals.conf"))
		ok("screen-capture portal installed and selected for KDOS");
	else
		warn_("kdos-portals.conf missing — the portal is installed but "
		      "nothing selects it, so ScreenCast will report no "
		      "backend");

	const char *path = getenv("PATH");
	char *want = kb_path_join(kb_home_dir(), ".local/bin");
	if (path && strstr(path, want))
		ok("~/.local/bin on PATH");
	else
		warn_("~/.local/bin not on PATH — exported app wrappers will not "
		      "resolve");
	free(want);

	if (doctor_json)
		printf("\n  ],\n  \"warnings\": %d\n}\n", doctor_warns);
	/*
	 * Exit status carries the verdict in BOTH modes, so a script does not
	 * have to choose between reading the text and parsing the JSON.
	 */
	return doctor_warns ? 1 : 0;
}

static int cmd_version(void)
{
	struct utsname u;
	uname(&u);
	printf("%sKDOS%s — KD's Homebrew Linux Distro\n", C_A, C_0);
	printf("  kernel   %s\n", u.release);
	printf("  libc     musl\n");
	printf("  userland toybox\n");
	printf("  session  kdos-comp (%s)\n", current_theme());
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int kdos_main(int argc, char **argv)
{
	colours();

	const char *cmd = argc > 1 ? argv[1] : "help";
	int rest = argc - 2;
	char **restv = argv + 2;

	if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return cmd_help(rest, restv);
	if (!strcmp(cmd, "theme"))
		return cmd_theme(rest, restv);
	if (!strcmp(cmd, "status"))
		return cmd_status(rest, restv);
	if (!strcmp(cmd, "doctor"))
		return cmd_doctor(rest, restv);
	if (!strcmp(cmd, "why"))
		return why_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "explain"))
		return explain_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "sandbox"))
		return sandbox_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "appid"))
		return appid_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "restarts"))
		return restarts_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "stutter"))
		return stutter_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "march"))
		return march_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "rebuild"))
		return rebuild_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "cve"))
		return kdt_cve(rest, restv, C_A, C_W, C_0);
	if (!strcmp(cmd, "version") || !strcmp(cmd, "-V"))
		return cmd_version();
	if (!strcmp(cmd, "app")) {
		/* argv[1] becomes the program name kdos-fetch-app expects. */
		argv[1] = (char *)"kdos-fetch-app";
		return fetch_app_main(argc - 1, argv + 1);
	}

	fprintf(stderr, "%skdos:%s unknown command '%s' — try: kdos help\n", C_W,
		C_0, cmd);
	return 1;
}
