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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/utsname.h>

#include "kdos-tools.h"

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

static char *cache_home(const char *rest)
{
	const char *x = getenv("XDG_CACHE_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".cache");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

static char *data_home(const char *rest)
{
	const char *x = getenv("XDG_DATA_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".local/share");
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
	KbArgv a = {0};
	kb_argv_add(&a, "pkill");
	kb_argv_add(&a, "-HUP");
	kb_argv_add(&a, "kdos-shell");
	kb_argv_end(&a);
	kb_run(&a);	/* no session running is not an error */
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

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS foot theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"[colors]\n"
		"background=%s\nforeground=%s\n"
		"selection-background=%s\nselection-foreground=%s\n"
		"regular0=%s\nregular1=%s\nregular2=%s\nregular3=%s\n"
		"regular4=%s\nregular5=%s\nregular6=%s\nregular7=%s\n"
		"bright0=%s\nbright1=%s\nbright2=%s\nbright3=%s\n"
		"bright4=%s\nbright5=%s\nbright6=%s\nbright7=%s\n"
		"\n[colors-dark]\nbackground=%s\nforeground=%s\ncursor=%s %s\n",
		deep, text, dim, p,
		deep, urg, p, sec, dim, sec, p, text,
		dim, urg, p, sec, dim, sec, p, text,
		deep, text, deep, p);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
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

static int cmd_theme(int argc, char **argv)
{
	const char *cur = current_theme();
	const char *want = argc > 0 ? argv[0] : "";

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

	write_gtk(sc);
	write_icons(sc);
	write_foot(sc);
	write_btop(sc);
	write_starship(sc);

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
		{ "kdos status", "packages, containers, exported apps" },
		{ "kdos doctor", "check the session for common breakage" },
		{ "kdos appid", "do launcher icons match the windows they open?" },
		{ "kdos-shot [region]", "screenshot to clipboard and ~/Pictures" },
		{ "kdos-fetch-static", "fetch a single verified static binary" },
		{ "sudo kinstall", "install this live image onto a disk" },
		{ NULL, NULL }
	};
	for (int i = 0; CMDS[i][0]; i++)
		fprintf(o, "  %-26s %s\n", CMDS[i][0], CMDS[i][1]);
	fputc('\n', o);

	fprintf(o, "%sKEYS%s  %s(defaults — remap in ~/.config/kdos/comp.conf)%s\n",
		C_A, C_0, C_D, C_0);
	static const char *KEYS[][2] = {
		{ "Super+D", "open the launcher" },
		{ "Super+Return", "terminal (foot)" },
		{ "Super+Q", "close window" },
		{ "Alt+Tab", "switch window (most recent first)" },
		{ "Super+Arrows", "snap: half, quarter, maximize" },
		{ "Super+Shift+Arrows", "move window between outputs" },
		{ "Super+F", "toggle floating / snapped" },
		{ "Super+1..4", "switch workspace" },
		{ "Super+Shift+1..4", "move window to workspace" },
		{ "Super+L", "lock the screen" },
		{ "PrtSc", "screenshot (region, to clipboard and disk)" },
		{ NULL, NULL }
	};
	for (int i = 0; KEYS[i][0]; i++)
		fprintf(o, "  %s%-22s%s %s\n", C_B, KEYS[i][0], C_0, KEYS[i][1]);
}

static int cmd_help(int argc, char **argv)
{
	if (argc > 0 && !strcmp(argv[0], "--pager") && kb_have_prog("less")) {
		FILE *p = popen("less -R", "w");
		if (p) {
			help_body(p);
			pclose(p);
			return 0;
		}
	}
	help_body(stdout);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

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
	char *apps = data_home("applications");

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
	printf("%s%-16s%s %s\n", C_B, "Session", C_0,
	       getenv("WAYLAND_DISPLAY") ? "wayland" : "tty");
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

static void ok(const char *fmt, ...)
{
	va_list ap;
	printf("  %s[ ok ]%s ", C_A, C_0);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

static void warn_(const char *fmt, ...)
{
	va_list ap;
	printf("  %s[warn]%s ", C_W, C_0);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
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

static int cmd_doctor(void)
{
	printf("%sKDOS doctor%s\n\n", C_A, C_0);

	printf("%sKernel%s\n", C_B, C_0);
	int abi = landlock_abi();
	if (abi > 0)
		ok("Landlock ABI %d", abi);
	else if (abi == -ENOSYS)
		warn_("no Landlock — kernel too old or CONFIG_SECURITY_LANDLOCK off");
	else
		warn_("Landlock present but disabled — add it to CONFIG_LSM or "
		      "the lsm= cmdline");
	putchar('\n');

	printf("%sSession%s\n", C_B, C_0);
	const char *wd = getenv("WAYLAND_DISPLAY");
	if (wd && *wd)
		ok("WAYLAND_DISPLAY=%s", wd);
	else
		warn_("no WAYLAND_DISPLAY — not inside the desktop session");

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
	putchar('\n');

	printf("%sContainers%s\n", C_B, C_0);
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
	putchar('\n');

	printf("%sDesktop%s\n", C_B, C_0);
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

	const char *disp = getenv("DISPLAY");
	if (disp && *disp)
		ok("Xwayland on %s", disp);
	else
		warn_("no DISPLAY — X11-only alien apps will not start");

	const char *path = getenv("PATH");
	char *want = kb_path_join(kb_home_dir(), ".local/bin");
	if (path && strstr(path, want))
		ok("~/.local/bin on PATH");
	else
		warn_("~/.local/bin not on PATH — exported app wrappers will not "
		      "resolve");
	free(want);
	return 0;
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
		return cmd_doctor();
	if (!strcmp(cmd, "why"))
		return why_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "explain"))
		return explain_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "sandbox"))
		return sandbox_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "appid"))
		return appid_main(argc - 1, argv + 1);
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
