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
 * second copy of the table — seven schemes, nine colours, hand-kept in step
 * with the installer's — and the two were edited separately.
 * ---------------------------------
 */

#include <dirent.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/statfs.h>
#include <sys/socket.h>
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

/*
 * One key out of con.conf, or NULL. The user's file first, then the shipped
 * one — the same order libkcon reads them in, stated here because this binary
 * links no libkcon and must not: `kdos` is on every image and the console
 * desktop is not.
 */
static char *con_conf_key(const char *key)
{
	char *user = kdt_cfg_home("kdos-con/con.conf");
	const char *files[2];
	char *out = NULL;

	files[0] = user;
	files[1] = "/etc/kdos/con.conf";

	for (int f = 0; f < 2 && !out; f++) {
		char *data = kb_read_all(files[f], NULL);

		if (!data)
			continue;
		for (char *line = data, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			char *eq;

			next = nl ? nl + 1 : line + strlen(line);
			if (nl)
				*nl = '\0';
			while (*line == ' ' || *line == '\t')
				line++;
			if (strncmp(line, key, strlen(key)))
				continue;
			eq = strchr(line, '=');
			if (!eq)
				continue;
			eq++;
			while (*eq == ' ' || *eq == '\t')
				eq++;
			out = kb_strdup(eq);
			break;
		}
		free(data);
	}
	free(user);
	return out;
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

/* Exported because `kdos thumb` writes into the same cache root and a second
 * answer to where that is would put its thumbnails somewhere nothing else
 * looks. */
char *kdt_cache_home(const char *rest)
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
	char *p = kdt_cache_home("kdos/theme");
	name[0] = 0;
	if (kb_read_line_file(p, name, sizeof(name)) > 0 && kcol_find(name)) {
		free(p);
		return name;
	}
	free(p);
	return "phosphor";
}

/* The same answer, for `kdos update theme` — which has to re-run the
 * generators with the accent the session is already wearing. */
const char *kdt_current_accent(void)
{
	return current_theme();
}

void kdt_mkparent(const char *path)
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
	/*
	 * Every long-lived surface on the desktop, and the compositor.
	 *
	 * The list is by NAME because kdos-shell is basename-dispatched: the
	 * panel, the desktop icons and the notification daemon are three
	 * different argv[0]s of one binary, and `pkill -HUP kdos-shell` reaches
	 * exactly one of them. Photographed on a booted ISO — `kdos theme amber`
	 * left the desktop icons and a toast in phosphor while the panel and the
	 * window frames went amber, which reads as a half-finished repaint
	 * because that is what it was.
	 *
	 * Separate pkills rather than one pattern: `kdos-*` would also signal
	 * kdos-appbox and every alien app launched through it.
	 */
	/*
	 * A name goes here ONLY if that program installs a SIGHUP handler —
	 * the default disposition is death, so this list and sh_theme_watch()
	 * are two things that must agree. kdos-slit is here because it calls
	 * sh_theme_watch() and was on nobody's list, so the dockapp column
	 * kept the accent it started in. Every other front end follows the
	 * state file's mtime instead (sh_theme_poll), which needs no entry
	 * here and cannot be got wrong in this direction.
	 */
	/*
	 * `kdos-res` is here and `kdos-resctl` must never be: the exact match
	 * below is what keeps them apart, since one name is a prefix of the
	 * other. The setuid helper is short-lived and handles no signals.
	 *
	 * Both halves of the console desktop are here and both have to be.
	 * `kdos-con` holds the cells and `kdos-view` holds the palette its
	 * backend paints them with, so signalling one and not the other
	 * retints half a screen. `kdos-con-login` and `kdos-con-start` are NOT
	 * reached: the match is exact, and one is a login and the other a
	 * /bin/sh script that would die of a signal it does not handle.
	 *
	 * `kdos-term` handles it and redraws; foot cannot reload its config at
	 * all, which is why the note below says a foot window keeps the accent
	 * it opened in and this one does not.
	 */
	static const char *const who[] = {
		"kdos-shell", "kdos-desk", "kdos-notifyd", "kdos-slit",
		"kdos-res", "kdos-comp", "kdos-con", "kdos-view", "kdos-term"
	};
	for (size_t i = 0; i < sizeof(who) / sizeof(who[0]); i++) {
		KbArgv a = {0};
		kb_argv_add(&a, "pkill");
		/*
		 * EXACT, and that is not tidiness: `kdos-desk` is a SUBSTRING
		 * of `kdos-desktop` and of `kdos-desktop-start`, which are the
		 * two /bin/sh scripts that own the session. A substring match
		 * would send them a SIGHUP they do not handle, and the default
		 * disposition for that is death — so retinting the desktop
		 * would have taken the session's own helpers with it.
		 */
		kb_argv_add(&a, "-x");
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
	/* The derived values are kcol_sem's — the SAME struct kdos-theme's
	 * build_names expands into the recoloured stylesheet. This layer sits
	 * at a higher style priority, so a mix computed twice was a mix where
	 * the harsher copy won (warning, destructive, the headerbar border). */
	KcolSem sem;
	kcol_sem(sc, &sem);
	uint32_t insens = kcol_mix(sc->text, sc->variant, 55);

	/* Every colour is formatted into its OWN local first. A helper handing
	 * back a shared or rotating buffer cannot be used here: one printf
	 * below takes more colours than any such buffer has slots, and the
	 * early arguments come back overwritten. Do not "simplify" this into
	 * inline calls. */
	char P[8], PD[8], SEC[8], URG[8], DEEP[8], TXT[8], VAR[8];
	char HDR[8], SBKD[8], DLG[8], THUMB[8], ONACC[8], INSENS[8];
	char WBG[8], WFG[8], DBG[8], DFG[8], HBORD[8], BORD[8], UBORD[8];
	kcol_format(sc->primary, P);
	kcol_format(sc->pdark, PD);
	kcol_format(sc->secondary, SEC);
	kcol_format(sc->urgent, URG);
	kcol_format(sc->deep, DEEP);
	kcol_format(sc->text, TXT);
	kcol_format(sc->variant, VAR);
	kcol_format(sem.header, HDR);
	kcol_format(sem.side_backdrop, SBKD);
	kcol_format(sem.dialog, DLG);
	kcol_format(sem.thumb, THUMB);
	kcol_format(sem.on_accent, ONACC);
	kcol_format(insens, INSENS);
	kcol_format(sem.warning_bg, WBG);
	kcol_format(sem.warning_fg, WFG);
	kcol_format(sem.destructive_bg, DBG);
	kcol_format(sem.destructive_fg, DFG);
	kcol_format(sem.headerbar_border, HBORD);
	kcol_format(sem.border, BORD);
	kcol_format(sem.border_unfocused, UBORD);

	/* build_names spells the card surface exactly this way; matching its
	 * spelling is the point of sharing the values at all. */
	char CARD[24];
	snprintf(CARD, sizeof(CARD), "alpha(#%s, 0.07)", TXT);

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
		SEC, WBG, WFG, URG, DBG, DFG, URG, DBG, DFG);
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
		"@define-color card_bg_color %s;\n"
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
		HDR, TXT, HBORD, VAR,
		HDR, TXT, SBKD,
		HDR, TXT, SBKD,
		CARD, TXT,
		DLG, TXT, DLG, TXT,
		THUMB, TXT, DEEP, TXT);
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
		VAR, INSENS, DEEP, BORD, UBORD, DEEP, DEEP);

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
	kdt_mkparent(out);
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
	kdt_mkparent(out);
	KbArgv a = {0};
	kb_argv_add(&a, "kdos-theme");
	kb_argv_add(&a, "cursors");
	kb_argv_add(&a, out);
	kb_argv_add(&a, sc->name);
	kb_argv_end(&a);
	kb_run(&a);
	free(out);
}

/*
 * The derived ANSI palette.
 *
 * The scheme has no blue, no magenta and no cyan, and the first generator
 * mapped those slots onto `dim` and `secondary` — ls directories at 1.7:1
 * contrast, vim comments invisible, and three distinguishable colours
 * collapsed onto one. The hand-tuned phosphor palette in skel had real hues;
 * these functions are those tuned values made per-accent, with phosphor
 * reproducing them byte for byte (verified: the HLS round trip is exact).
 *
 * Two derivations, because the anchors relate to the scheme two ways:
 *
 *   - a BRIGHT variant of a scheme slot (bright green is bright PRIMARY, not
 *     bright #39ff14) transplants the anchor's hue/lightness/saturation offset
 *     from the phosphor slot onto this scheme's slot;
 *   - a hue the scheme cannot supply (blue/magenta/cyan) keeps the anchor's
 *     hue and follows only the scheme's text lightness, so an amber terminal
 *     gets a slightly warm-dark blue rather than a green.
 */
static double ansi_wrap1(double x)
{
	while (x >= 1.0)
		x -= 1.0;
	while (x < 0.0)
		x += 1.0;
	return x;
}

/* `anchor`'s relation to `from`, transplanted onto `to`. Identity when
 * from == to, which is what makes phosphor byte-exact. */
static uint32_t ansi_derive(uint32_t anchor, uint32_t from, uint32_t to)
{
	double ah, al, as, fh, fl, fs, th, tl, ts;
	kcol_to_hls(anchor, &ah, &al, &as);
	kcol_to_hls(from, &fh, &fl, &fs);
	kcol_to_hls(to, &th, &tl, &ts);
	double h = ansi_wrap1(th + (ah - fh));
	double l = tl + (al - fl);
	/* A clip here is not a bright colour, it is WHITE: bone's primary
	 * already sits at l=0.92 and phosphor's +0.14 bright offset lands past
	 * 1.0, so bright-green came out #ffffff — the only pure white in any
	 * palette, out-brightening the bright-white slot beside it. Rescale the
	 * offset into whatever headroom the destination has instead. */
	if (l > 1.0)
		l = tl + (1.0 - tl) * (al - fl) / (1.0 - fl);
	if (l < 0.0)
		l = 0.0;
	if (l > 1.0)
		l = 1.0;
	double s = fs > 0.0 ? ts * (as / fs) : as;
	if (s < 0.0)
		s = 0.0;
	if (s > 1.0)
		s = 1.0;
	return kcol_from_hls(h, l, s);
}

/* Hue and saturation are the anchor's own; lightness follows the scheme's
 * text so the foreign hues sit in the same brightness envelope. */
static uint32_t ansi_fixed(uint32_t anchor, uint32_t phos_text, uint32_t text)
{
	double ah, al, as, dh, dl, ds, th, tl, ts;
	kcol_to_hls(anchor, &ah, &al, &as);
	kcol_to_hls(phos_text, &dh, &dl, &ds);
	kcol_to_hls(text, &th, &tl, &ts);
	double l = dl > 0.0 ? al * (tl / dl) : al;
	if (l > 1.0)
		l = 1.0;
	return kcol_from_hls(ah, l, as);
}

typedef struct {
	uint32_t blue, magenta, cyan;		/* regular4/5/6            */
	uint32_t bblue, bmagenta, bcyan;	/* bright4/5/6             */
	uint32_t bprimary, bsecondary;		/* bright2/3               */
	uint32_t burgent, btext;		/* bright1/7               */
} AnsiDerived;

static void ansi_all(const KcolScheme *sc, AnsiDerived *o)
{
	const KcolScheme *ph = kcol_find("phosphor");

	o->blue = ansi_fixed(0x2f8fff, ph->text, sc->text);
	o->magenta = ansi_fixed(0xc77dff, ph->text, sc->text);
	o->cyan = ansi_fixed(0x25d0c0, ph->text, sc->text);
	o->bblue = ansi_fixed(0x6bb6ff, ph->text, sc->text);
	o->bmagenta = ansi_fixed(0xe0aaff, ph->text, sc->text);
	o->bcyan = ansi_fixed(0x68f5e6, ph->text, sc->text);
	o->bprimary = ansi_derive(0x7dff5c, ph->primary, sc->primary);
	o->bsecondary = ansi_derive(0xffd166, ph->secondary, sc->secondary);
	o->burgent = ansi_derive(0xff6b6b, ph->urgent, sc->urgent);
	o->btext = ansi_derive(0xe8ffee, ph->text, sc->text);
}

static void write_foot(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("foot/themes/kdos");
	kdt_mkparent(f);

	AnsiDerived a;
	ansi_all(sc, &a);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	char blue[8], mag[8], cyan[8], bblue[8], bmag[8], bcyan[8];
	char bpri[8], bsec[8], burg[8], btxt[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);
	kcol_format(a.blue, blue);
	kcol_format(a.magenta, mag);
	kcol_format(a.cyan, cyan);
	kcol_format(a.bblue, bblue);
	kcol_format(a.bmagenta, bmag);
	kcol_format(a.bcyan, bcyan);
	kcol_format(a.bprimary, bpri);
	kcol_format(a.bsecondary, bsec);
	kcol_format(a.burgent, burg);
	kcol_format(a.btext, btxt);

	/*
	 * One section, and it is `[colors-dark]`, not `[colors]`. foot deprecated
	 * the old section name and warns ONCE PER KEY on stderr — 24 keys, so
	 * every terminal opened on this desktop began with 24 lines of
	 * "deprecated: foot: [colors]: use [colors-dark] instead" above the first
	 * prompt. `initial-color-theme` defaults to `dark`, so the dark section
	 * alone is what foot reads; there is no light KDOS palette to write.
	 *
	 * cursor and urls are written HERE, per accent — foot.ini must carry
	 * neither after its include, or every terminal wears the phosphor cursor
	 * whatever the accent (the trap the old foot.ini shipped). bright0 is
	 * kcol_muted, not dim: bright-black is what ls and vim use for de-
	 * emphasised TEXT, and dim is unreadable as text.
	 */
	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS foot theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"[colors-dark]\n"
		"background=%s\nforeground=%s\ncursor=%s %s\nurls=%s\n"
		"selection-background=%s\nselection-foreground=%s\n"
		"regular0=%s\nregular1=%s\nregular2=%s\nregular3=%s\n"
		"regular4=%s\nregular5=%s\nregular6=%s\nregular7=%s\n"
		"bright0=%s\nbright1=%s\nbright2=%s\nbright3=%s\n"
		"bright4=%s\nbright5=%s\nbright6=%s\nbright7=%s\n",
		deep, text, deep, p, sec, dim, p,
		var, urg, p, sec, blue, mag, cyan, text,
		mut, burg, bpri, bsec, bblue, bmag, bcyan, btxt);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/*
 * tmux, the same shape as foot: the shareable skel tmux.conf keeps the
 * BEHAVIOUR (prefix, binds, status position) and sources this file for every
 * colour-bearing line, so `kdos theme amber` repaints the next attach — and a
 * running server too, via the source-file reload cmd_theme already sends.
 */
static void write_tmux(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("tmux/themes/kdos.conf");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], mut[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(kcol_muted(sc), mut);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS tmux theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"set -g status-style \"bg=#%s,fg=#%s\"\n"
		"\n"
		"set -g status-left-length 40\n"
		"set -g status-left \"#[bg=#%s,fg=#%s,bold] #S #[bg=#%s,fg=#%s] \"\n"
		"\n"
		"set -g status-right-length 80\n"
		"set -g status-right \"#[fg=#%s]|#[fg=#%s] #(uptime | sed "
		"'s/.*load average: //') #[fg=#%s]|#[fg=#%s] %%Y-%%m-%%d "
		"#[fg=#%s]%%H:%%M \"\n"
		"\n"
		"setw -g window-status-format \"#[fg=#%s] #I:#W"
		"#{?window_zoomed_flag,+,} \"\n"
		"setw -g window-status-current-format \"#[bg=#%s,fg=#%s,bold] "
		"#I:#W#{?window_zoomed_flag,+,} \"\n"
		"setw -g window-status-activity-style \"fg=#%s\"\n"
		"setw -g window-status-bell-style \"fg=#%s\"\n"
		"\n"
		"set -g pane-border-style \"fg=#%s\"\n"
		"set -g pane-active-border-style \"fg=#%s\"\n"
		"\n"
		"set -g message-style \"bg=#%s,fg=#%s\"\n"
		"set -g message-command-style \"bg=#%s,fg=#%s\"\n"
		"setw -g mode-style \"bg=#%s,fg=#%s\"\n"
		"\n"
		"set -g display-panes-active-colour \"#%s\"\n"
		"set -g display-panes-colour \"#%s\"\n"
		"set -g clock-mode-colour \"#%s\"\n",
		deep, text,
		p, deep, deep, p,
		dim, mut, dim, sec, p,
		mut, dim, p, sec, urg,
		dim, p,
		dim, p, dim, sec, dim, p,
		p, dim, p);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/*
 * LS_COLORS, as a shell fragment `.bashrc` sources. Four classes are enough
 * for a glance — directory, link, executable, orphan.
 *
 * The consumers are eza (which `.bashrc` aliases `ls` to whenever it is
 * installed) and the appbox's Debian coreutils; toybox's own `ls` colours from
 * a hardcoded table and reads no LS_COLORS at all, so the host's fallback `ls`
 * gains nothing from this file. Each class gets its OWN colour rather than a
 * bold bit: directory and executable used to share `primary` and were told
 * apart only by the intensity attribute, which on a 512-glyph console font is
 * the 9th glyph bit rather than a weight.
 */
static void write_lscolors(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("kdos/ls-colors");
	kdt_mkparent(f);

	AnsiDerived a;
	ansi_all(sc, &a);

	KcolRgb di = kcol_rgb(sc->primary);
	KcolRgb ln = kcol_rgb(sc->secondary);
	KcolRgb ex = kcol_rgb(a.cyan);
	KcolRgb or_ = kcol_rgb(sc->urgent);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS LS_COLORS — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"export LS_COLORS=\""
		"di=1;38;2;%u;%u;%u:"
		"ln=38;2;%u;%u;%u:"
		"ex=38;2;%u;%u;%u:"
		"or=38;2;%u;%u;%u\"\n",
		di.r, di.g, di.b,
		ln.r, ln.g, ln.b,
		ex.r, ex.g, ex.b,
		or_.r, or_.g, or_.b);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}


/*
 * The window frames — labwc's themerc, at ~/.config/kdos-comp/themerc-override.
 *
 * This was the LAST thing on the desktop that an accent switch could not
 * reach. The file shipped as a fixed neutral grey precisely so it would read
 * acceptably under all seven accents without being regenerated, and the cost of
 * that was a desktop where `kdos theme amber` retinted the panel, the shader,
 * the icons, the cursors, GTK, Qt, foot, btop, mc and starship — and left the
 * bar across the top of every window looking like somebody else's desktop.
 *
 * kdos-comp reads it on SIGHUP (labwc's Reconfigure), which `kdos theme`
 * already sends, so this repaints live with the rest.
 *
 * Two things here are not decoration:
 *
 *   - `window.*.title.bg: flat solid` MUST accompany the colour. labwc's
 *     default titlebar texture is a GRADIENT, and a bg.color alone is ignored
 *     by it — the trap that made the first hand-written version of this file
 *     appear to do nothing.
 *   - the close button takes the URGENT slot while the other buttons take the
 *     accent. It is the one control on a window that cannot be undone, it is
 *     red in Turbo Vision and in every desktop since, and on an eight-colour
 *     palette that difference is the entire affordance.
 */
static void write_themerc(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("kdos-comp/themerc-override");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], pdark[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(sc->pdark, pdark);

	/* An inactive frame is the same surface with the life taken out of it,
	 * not a different colour: two hues on screen at once would read as two
	 * kinds of window rather than as focus. */
	char inactive_text[8], hover[8], sepc[8];
	kcol_format(kcol_mix(sc->deep, sc->text, 45), inactive_text);
	kcol_format(kcol_mix(sc->variant, sc->primary, 25), hover);
	kcol_format(kcol_mix(sc->deep, sc->text, 25), sepc);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS window frames — GENERATED by `kdos theme`; edits will "
		"be overwritten.\n"
		"# labwc themerc keys; kdos-comp re-reads this on SIGHUP.\n"
		"\n"
		/*
		 * THE FRAME IS A KDOS ASCII WINDOW, and every line of this
		 * block is one stroke of that picture. `flat kdos` is the
		 * fork's own texture: the titlebar fill carries the DOUBLE
		 * HORIZONTAL RULE the cell grid draws with `═`, broken by the
		 * title and by each button, so an SSD reads
		 * `════ Title ════[_][=][X]` — the same drawing every other
		 * surface on this desktop puts round itself. `colorTo` is that
		 * rule's colour and is ignored by every other texture.
		 *
		 * Two pixels of border, not one: a hairline is what a modern
		 * toolkit draws and it disappears beside a 32-pixel cell. The
		 * square corners are rc.xml's `<cornerRadius>0`, which is the
		 * other half of the same decision.
		 *
		 * The buttons are 32x32, which is exactly FOUR TIMES the 8x8
		 * bitmaps the fork ships. A non-integer multiple under a
		 * nearest-neighbour scale gives one glyph stroke two pixels
		 * and the next three, which on an `X` is visible as a limp.
		 */
		"border.width: 2\n"
		"window.titlebar.padding.width: 4\n"
		"window.titlebar.padding.height: 2\n"
		"window.button.width: 32\n"
		"window.button.height: 32\n"
		"window.button.spacing: 0\n"
		"window.active.border.color: #%s\n"
		"window.inactive.border.color: #%s\n"
		"\n"
		"# `flat kdos` — the frame rule; a plain `flat solid` drops it\n"
		"window.active.title.bg: flat kdos\n"
		"window.inactive.title.bg: flat kdos\n"
		"window.active.title.bg.color: #%s\n"
		"window.inactive.title.bg.color: #%s\n"
		"window.active.title.bg.colorTo: #%s\n"
		"window.inactive.title.bg.colorTo: #%s\n"
		"window.active.label.text.color: #%s\n"
		"window.inactive.label.text.color: #%s\n"
		"window.label.text.justify: center\n"
		"\n"
		"window.active.button.unpressed.image.color: #%s\n"
		"window.inactive.button.unpressed.image.color: #%s\n"
		"window.active.button.close.unpressed.image.color: #%s\n"
		/*
		 * AND THE HOVER PLATE HAS TO BE TRANSLUCENT.
		 *
		 * labwc has no hover ICONS: it copies the plain one and lays
		 * `window.button.hover.bg.color` over it. An OPAQUE colour
		 * there paints the symbol out, so every titlebar button on
		 * this desktop went BLANK under the pointer — the one moment a
		 * button most needs to say what it is. labwc's own default
		 * carries an alpha for exactly this reason; ours has to too.
		 */
		"window.button.hover.bg.color: #%s66\n"
		"\n"
		/*
		 * labwc's own default cap is 200 PIXELS, sized for the ~10px
		 * font a normal theme uses. This desktop draws menus at 32px,
		 * where 200px is eleven characters — measured on a booted ISO,
		 * the root menu read "Applicati...", "Lock Scr..." and
		 * "Reload C...". The cap only truncates; a generous one costs
		 * a narrow menu nothing, because the width is the content's.
		 */
		"menu.width.min: 120\n"
		"menu.width.max: 900\n"
		"menu.border.width: 1\n"
		"menu.border.color: #%s\n"
		"menu.items.bg.color: #%s\n"
		"menu.items.text.color: #%s\n"
		"menu.items.active.bg.color: #%s\n"
		"menu.items.active.text.color: #%s\n"
		"menu.title.bg.color: #%s\n"
		"menu.title.text.color: #%s\n"
		"menu.separator.color: #%s\n"
		"\n"
		"osd.bg.color: #%s\n"
		"osd.label.text.color: #%s\n"
		"osd.border.width: 1\n"
		"osd.border.color: #%s\n"
		"\n"
		/*
		 * #rrggbbaa, not the openbox `#rrggbb 40` form: labwc still
		 * parses that one (40 is a PERCENT, so 0x66) but logs an ERROR
		 * per occurrence and says it may stop. Two of them were the
		 * only errors in a whole booted session's log.
		 */
		"snapping.overlay.region.bg.enabled: yes\n"
		"snapping.overlay.region.bg.color: #%s66\n"
		"snapping.overlay.region.border.enabled: yes\n"
		"snapping.overlay.region.border.width: 1\n"
		"snapping.overlay.region.border.color: #%s\n"
		"snapping.overlay.edge.bg.enabled: yes\n"
		"snapping.overlay.edge.bg.color: #%s66\n"
		"snapping.overlay.edge.border.enabled: yes\n"
		"snapping.overlay.edge.border.width: 1\n"
		"snapping.overlay.edge.border.color: #%s\n"
		"\n"
		"magnifier.border.width: 2\n"
		"magnifier.border.color: #%s\n",
		p, dim,
		var, deep, p, sepc,
		p, inactive_text,
		p, inactive_text, urg, hover,
		pdark, deep, text, p, deep, var, p, sepc,
		deep, text, pdark,
		pdark, p, pdark, p,
		sec);

	/* `kdos theme style` leaves its themerc lines in their own file and
	 * they are re-appended HERE, after the generated block, so they win —
	 * labwc keeps the last spelling of a repeated key. Appending them once
	 * at style time instead lost them on the next plain `kdos theme
	 * <accent>`, which rewrites this file whole: half an applied style
	 * survived (comp.conf) and half vanished, with nothing saying so. */
	char *sf = kdt_cfg_home("kdos/style-themerc");
	size_t sn = 0;
	char *style = kb_read_all(sf, &sn);
	free(sf);
	if (style && sn) {
		kb_buf_str(&b, "\n# style overrides — `kdos theme style`\n");
		kb_buf_add(&b, style, sn);
	}
	free(style);

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
	char mut[10];
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
	HX(kcol_muted(sc), mut);
#undef HX

	/*
	 * $XDG_DATA_HOME, not $XDG_CONFIG_HOME. mc looks for skins in
	 * <data>/mc/skins, /etc/mc/skins and /usr/share/mc/skins and nowhere
	 * else, so a skin written beside the config file is a skin mc will never
	 * find — it falls back to `default` and reports nothing.
	 */
	char *f = kdt_data_home("mc/skins/kdos.ini");
	kdt_mkparent(f);

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
		/* core: 9 pairs — disabled is TEXT, so muted rather than dim */
		text, deep,   deep, p,      sec, deep,    deep, sec,
		deep, p,      text, varied, deep, p,      p, deep,
		mut, deep,
		/* dialog: 5 pairs */
		text, varied, deep, p,      p, varied,    deep, p,
		p, varied,
		/* error: 3 pairs */
		text, urg,    deep, urg,    text, urg,
		/* menu: 5 pairs */
		text, varied, deep, p,      p, varied,    deep, p,
		mut, varied,
		/* buttonbar: 2 pairs — the F-key number must still be readable */
		text, deep,   mut, deep,
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
	kdt_mkparent(ini);
	kdt_ini_set(ini, "Midnight-Commander", "skin", "kdos");
	free(ini);
}

static void write_btop(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("btop/themes/kdos.theme");
	kdt_mkparent(f);

	AnsiDerived a;
	ansi_all(sc, &a);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], mut[8];
	char cyan[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(kcol_muted(sc), mut);
	kcol_format(a.cyan, cyan);

	/* inactive_fg is TEXT (a de-emphasised process row is still read), so it
	 * is kcol_muted; the box frames and div_line are borders and keep dim.
	 * The SELECTED row keeps dim-on-primary: it is a fill, not text, and
	 * `variant` sits at 1.05:1 against `deep` — with selected_fg equal to
	 * main_fg on top of that, the row you are about to send a signal to was
	 * indistinguishable from every other row.
	 * Gradients run primary → secondary → derived cyan (temperature ends on
	 * urgent — the one gauge where the top of the scale is an alarm). */
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
		deep, text, p, cyan, dim, p, mut, text, sec,
		dim, dim, dim, dim, dim,
		p, sec, urg, p, sec, cyan,
		p, sec, cyan, p, sec, cyan, p, sec, cyan,
		p, sec, cyan, p, sec, cyan, p, sec, cyan);
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

/* What this generator owns: whole sections, and named keys in shared ones.
 *
 * KConfig nests groups as `[A][B]`, and a nested group is a DIFFERENT group:
 * `[Colors:View][Inactive]` is an application's, not ours. Owning it swallowed
 * it — silently, on every accent switch — so a header with a subgroup in it is
 * never ours. */
static int kde_owns_section(const char *sec)
{
	if (strchr(sec, ']'))
		return 0;
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

/* Walk `old` line by line; `fn` is called with (section, trimmed line).
 *
 * Every allocation here is per-LINE, not a fixed buffer: the old 512-byte line
 * copy truncated — which is to say CORRUPTED — any long user value on every
 * accent switch (KDE recent-file lists and geometry blobs routinely run past
 * it), and the truncated line then survived as the file's new content. A line
 * this walk does not own must come out byte for byte. */
typedef void (*kde_line_fn)(const char *sec, const char *line, void *user);

static void kde_walk(const char *old, kde_line_fn fn, void *user)
{
	char *sec = kb_strdup("");
	const char *p = old;

	while (p && *p) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		char *line = kb_calloc(1, len + 1);

		memcpy(line, p, len);
		p = nl ? nl + 1 : NULL;

		char *t = kde_trim(line);
		if (!*t || *t == '#') {
			free(line);
			continue;
		}
		if (*t == '[') {
			/* The header is EVERY bracketed run on the line —
			 * KConfig writes a nested group as `[A][B]`. Stopping
			 * at the first `]` filed a subgroup's keys under its
			 * parent and lost the subgroup, and two subgroups of
			 * one parent merged into each other. */
			const char *end = NULL;
			for (const char *q = t; *q == '['; ) {
				const char *c = strchr(q + 1, ']');
				if (!c)
					break;
				end = c;
				q = c + 1;
			}
			size_t l = end ? (size_t)(end - t - 1) : strlen(t + 1);
			free(sec);
			sec = kb_calloc(1, l + 1);
			memcpy(sec, t + 1, l);
			fn(sec, NULL, user);
			free(line);
			continue;
		}
		fn(sec, t, user);
		free(line);
	}
	free(sec);
}

/* Grows without a ceiling: the old fixed 64 silently DROPPED every section
 * past it, and a KDE home accumulates one section per application. */
struct kde_secs {
	char **name;
	int n, cap;
};

static void kde_secs_free(struct kde_secs *s)
{
	for (int i = 0; i < s->n; i++)
		free(s->name[i]);
	free(s->name);
}

static void kde_collect(const char *sec, const char *line, void *user)
{
	struct kde_secs *s = user;
	(void)line;
	if (kde_owns_section(sec))
		return;
	for (int i = 0; i < s->n; i++)
		if (!strcmp(s->name[i], sec))
			return;
	if (s->n == s->cap) {
		int cap = s->cap ? s->cap * 2 : 16;
		char **v = kb_calloc((size_t)cap, sizeof(*v));
		if (s->name) {
			memcpy(v, s->name, (size_t)s->n * sizeof(*v));
			free(s->name);
		}
		s->name = v;
		s->cap = cap;
	}
	s->name[s->n++] = kb_strdup(sec);
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
	const char *eq = strchr(line, '=');
	size_t kl = eq ? (size_t)(eq - line) : strlen(line);
	char *key = kb_calloc(1, kl + 1);
	memcpy(key, line, kl);
	int ours = kde_owns_key(sec, kde_trim(key));
	free(key);
	if (ours)
		return;
	kb_buf_printf(e->out, "%s\n", line);
	e->wrote = 1;
}

static void kde_merge(KbBuf *out, const char *old, const KdeKV *kv, int n)
{
	struct kde_secs secs = {0};

	kde_walk(old, kde_collect, &secs);

	int *done = kb_calloc(secs.n ? (size_t)secs.n : 1, sizeof(int));

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

	free(done);
	kde_secs_free(&secs);
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
	kdt_mkparent(colors);
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
	kdt_mkparent(globals);
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

	AnsiDerived a;
	ansi_all(sc, &a);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	char cyan[8], bcyan[8], mag[8], bpri[8], bsec[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);
	kcol_format(a.cyan, cyan);
	kcol_format(a.bcyan, bcyan);
	kcol_format(a.magenta, mag);
	kcol_format(a.bprimary, bpri);
	kcol_format(a.bsecondary, bsec);

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
				/* `yellow` stays the SECONDARY: these are colour
				 * NAMES a starship.toml resolves by name, the
				 * shipped one styles four modules with it, and
				 * a `yellow` that renders teal is wrong output
				 * rather than a palette choice. The derived
				 * cyan goes on `cyan`, where it belongs. */
				p, urg, sec, sec, cyan, mag, text, deep,
				text, mut, dim, dim, mut, var,
				var, deep, deep, deep, deep, deep,
				cyan, bcyan, mag, bpri, urg, bsec, sec,
				bcyan, bcyan, bpri);
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

/*
 * The wallpaper follows the accent: the shipped PNG is retinted through
 * kcol_remap — the same operation every icon goes through — into
 * $XDG_CACHE_HOME/kdos/wallpaper.png, which kdos-comp prefers over the
 * comp.conf path when it re-reads the wallpaper on SIGHUP. Without this,
 * `kdos theme ice` was an ice desktop over a green background.
 *
 * Gated on KDOS_HAVE_LIBPNG (build.sh passes it with -lpng) so the host
 * selftest, which links kdos-tools without libpng, still builds; without the
 * macro an accent switch simply leaves no cache file, which kdos-comp treats
 * as "use the configured path".
 */
#ifdef KDOS_HAVE_LIBPNG
#include <png.h>

#define WALLPAPER_SRC "/usr/share/backgrounds/kdos/default-wallpaper.png"

static void write_wallpaper(const KcolScheme *sc)
{
	/* KDOS_WALLPAPER_SRC moves the source the same way KDOS_INITRD moves
	 * the initrd: it is what lets a test retint a wallpaper on a machine
	 * whose /usr/share is not the subject. */
	const char *src = getenv("KDOS_WALLPAPER_SRC");
	if (!src || !*src)
		src = WALLPAPER_SRC;
	if (!kb_path_exists(src))
		return;		/* no shipped wallpaper: nothing to retint */

	png_image in;
	memset(&in, 0, sizeof(in));
	in.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&in, src))
		return;
	in.format = PNG_FORMAT_RGBA;
	uint8_t *px = kb_calloc(1, PNG_IMAGE_SIZE(in));
	if (!png_image_finish_read(&in, NULL, px, 0, NULL)) {
		png_image_free(&in);
		free(px);
		return;
	}

	/* kcol_remap is six HLS conversions per call and a 4K wallpaper is
	 * eight million pixels; a photograph-free wallpaper repeats colours
	 * constantly, so a direct-mapped cache turns most pixels into a load. */
	size_t n = PNG_IMAGE_SIZE(in);
	uint32_t *ck = kb_calloc(1 << 16, sizeof(uint32_t));
	uint32_t *cv = kb_calloc(1 << 16, sizeof(uint32_t));
	memset(ck, 0xff, (1 << 16) * sizeof(uint32_t));	/* no 24-bit colour */
	for (size_t i = 0; i + 3 < n; i += 4) {
		uint32_t c = (uint32_t)px[i] << 16 | (uint32_t)px[i + 1] << 8 |
			     px[i + 2];
		unsigned slot = (c ^ (c >> 13)) & 0xffff;
		uint32_t v;
		if (ck[slot] == c) {
			v = cv[slot];
		} else {
			v = kcol_remap(sc, c);
			ck[slot] = c;
			cv[slot] = v;
		}
		px[i] = (uint8_t)(v >> 16);
		px[i + 1] = (uint8_t)(v >> 8);
		px[i + 2] = (uint8_t)v;
	}
	free(ck);
	free(cv);

	char *out = kdt_cache_home("kdos/wallpaper.png");
	kdt_mkparent(out);
	KbBuf tb = {0};
	kb_buf_printf(&tb, "%s.tmp", out);

	png_image wr;
	memset(&wr, 0, sizeof(wr));
	wr.version = PNG_IMAGE_VERSION;
	wr.width = in.width;
	wr.height = in.height;
	wr.format = PNG_FORMAT_RGBA;
	/* Written beside and renamed over: kdos-comp may re-decode this file on
	 * any SIGHUP, and a half-written PNG is a black screen, not an error. */
	if (png_image_write_to_file(&wr, tb.p, 0, px, 0, NULL))
		rename(tb.p, out);
	else
		unlink(tb.p);
	png_image_free(&wr);
	png_image_free(&in);
	kb_buf_free(&tb);
	free(out);
	free(px);
}
#else
static void write_wallpaper(const KcolScheme *sc)
{
	(void)sc;
}
#endif

/* Everything an accent switch produces, and nothing else — no state file, no
 * signals. `kdos theme --audit` runs exactly this into a scratch $HOME, which
 * only works because it is one function with no side effects beyond the files.
 * The wallpaper cache is NOT here: it is minutes-of-pixels heavy next to
 * everything else and lives under $XDG_CACHE_HOME, so cmd_theme writes it
 * beside the state file instead and the audit skips it. */
static void theme_apply(const KcolScheme *sc)
{
	write_gtk(sc);
	write_icons(sc);
	write_cursors(sc);
	write_kde(sc);
	write_themerc(sc);
	write_foot(sc);
	write_tmux(sc);
	write_btop(sc);
	write_mc(sc);
	write_starship(sc);
	write_lscolors(sc);
}

/* The switch's tail, shared with `kdos theme style`: everything that happens
 * AFTER the artefacts are regenerated, in the one order that works — the
 * wallpaper cache and the state file are both inputs to the SIGHUP, so both
 * are written before it is sent. */
static void theme_commit(const KcolScheme *sc)
{
	write_wallpaper(sc);

	/* The state file is the desktop's ONLY input, so it is written before
	 * the session is signalled — a SIGHUP that arrives first would make the
	 * shell re-read the accent it already had. ATOMIC for the other half of
	 * that race: a plain O_TRUNC write is zero bytes until it finishes, and
	 * four processes re-read this file the moment the signal lands. */
	char *state = kdt_cache_home("kdos/theme");
	kdt_mkparent(state);
	char line[40];
	snprintf(line, sizeof(line), "%s\n", sc->name);
	kb_write_file_atomic(state, line);
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
		kb_run(&a);	/* no server running is not an error */
		free(conf);
	}
}

/*
 * `kdos theme style <file>` — a shareable LOOK: the accent plus the knobs that
 * make a machine somebody's machine, in one flat key=value file.
 *
 *   accent = amber            the normal accent path
 *   crt = 40                  ┐ rewritten in ~/.config/kdos/comp.conf,
 *   crt_scanlines = 50        │ preserving every line the style does not name
 *   chrome_font = ...         ┘ (kdos-comp re-reads them on the same SIGHUP)
 *   osd.bg.color: ...         any dotted key is a themerc-override line, kept
 *                             in ~/.config/kdos/style-themerc and re-appended
 *                             AFTER the generated block on every regeneration
 *
 * `=` or `:` separates a key from its value, whichever comes first — the
 * themerc half is written `key: value` everywhere else and a value may itself
 * carry a colon (`chrome_font = Terminus:pixelsize=64`).
 */
static const char *const STYLE_COMP_KEYS[] = {
	"crt", "crt_scanlines", "crt_curve", "crt_fullscreen",
	"chrome_font", "clock_format",
};
#define NSTYLE_COMP ((int)(sizeof(STYLE_COMP_KEYS) / sizeof(STYLE_COMP_KEYS[0])))

/* Rewrite the named keys in comp.conf, preserving everything else verbatim.
 * A key the file does not carry yet is appended at the end. */
static void style_write_comp(char *const *val)
{
	char *f = kdt_cfg_home("kdos/comp.conf");
	kdt_mkparent(f);
	char *old = kb_read_all(f, NULL);
	KbBuf out = {0};
	int done[NSTYLE_COMP] = {0};

	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		size_t len = (size_t)(next - line);

		const char *t = line;
		while (*t == ' ' || *t == '\t')
			t++;
		int hit = -1;
		for (int i = 0; i < NSTYLE_COMP && hit < 0; i++) {
			if (!val[i])
				continue;
			size_t kl = strlen(STYLE_COMP_KEYS[i]);
			if (!strncmp(t, STYLE_COMP_KEYS[i], kl) &&
			    (t[kl] == ' ' || t[kl] == '\t' || t[kl] == '='))
				hit = i;
		}
		if (hit >= 0) {
			kb_buf_printf(&out, "%s = %s\n", STYLE_COMP_KEYS[hit],
				      val[hit]);
			done[hit] = 1;
		} else {
			kb_buf_add(&out, line, len);
		}
	}

	for (int i = 0; i < NSTYLE_COMP; i++) {
		if (!val[i] || done[i])
			continue;
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		kb_buf_printf(&out, "%s = %s\n", STYLE_COMP_KEYS[i], val[i]);
	}

	kb_write_all(f, out.p, out.n);
	kb_buf_free(&out);
	free(old);
	free(f);
}

static int cmd_theme_style(const char *path)
{
	char *data = kb_read_all(path, NULL);
	if (!data)
		kb_die("cannot read style '%s'", path);

	char accent[32] = "";
	char *comp[NSTYLE_COMP] = {0};
	KbBuf trc = {0};

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		char *k = line;
		while (*k == ' ' || *k == '\t')
			k++;
		if (!*k || *k == '#')
			continue;
		/* Whichever separator comes FIRST: a themerc line is spelled
		 * `key: value`, which is how write_themerc emits it and how
		 * the block above documents it, while a value may itself carry
		 * a colon — `chrome_font = Terminus:pixelsize=64`. */
		char *eq = strchr(k, '=');
		char *co = strchr(k, ':');
		if (!eq || (co && co < eq))
			eq = co;
		if (!eq) {
			kb_warn("style: '%s' is not key = value — ignored", k);
			continue;
		}
		char *v = eq + 1;
		while (eq > k && (eq[-1] == ' ' || eq[-1] == '\t'))
			eq--;
		*eq = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		char *e = v + strlen(v);
		while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
			*--e = 0;

		if (!strcmp(k, "accent")) {
			kb_strlcpy(accent, v, sizeof(accent));
			continue;
		}
		int hit = -1;
		for (int i = 0; i < NSTYLE_COMP; i++)
			if (!strcmp(k, STYLE_COMP_KEYS[i]))
				hit = i;
		if (hit >= 0) {
			free(comp[hit]);
			comp[hit] = kb_strdup(v);
			continue;
		}
		/* A dotted key is labwc themerc vocabulary. Anything else is a
		 * typo, and the skel comp.conf promises a line that does not
		 * take effect says so. */
		if (strchr(k, '.'))
			kb_buf_printf(&trc, "%s: %s\n", k, v);
		else
			kb_warn("style: unknown key '%s' — ignored", k);
	}
	free(data);

	const KcolScheme *sc = kcol_find(accent[0] ? accent : current_theme());
	if (!sc)
		kb_die("style names unknown accent '%s'", accent);

	int any_comp = 0;
	for (int i = 0; i < NSTYLE_COMP; i++)
		any_comp |= comp[i] != NULL;
	if (any_comp)
		style_write_comp(comp);
	for (int i = 0; i < NSTYLE_COMP; i++)
		free(comp[i]);

	/* Written to their own file BEFORE the generators run: write_themerc
	 * rewrites themerc-override whole and re-appends this after its own
	 * block, so the style's lines survive a later plain `kdos theme
	 * <accent>` and still win. A style with no themerc lines clears them —
	 * a style is a whole look, not a patch on the last one. */
	{
		char *f = kdt_cfg_home("kdos/style-themerc");
		kdt_mkparent(f);
		if (trc.n)
			kb_write_all(f, trc.p, trc.n);
		else
			unlink(f);
		free(f);
	}
	kb_buf_free(&trc);

	theme_apply(sc);

	theme_commit(sc);
	printf("%s%s%s (%s) — style applied\n", C_A, sc->name, C_0,
	       sc->theme_name);
	return 0;
}

static int cmd_theme(int argc, char **argv)
{
	const char *cur = current_theme();
	const char *want = argc > 0 ? argv[0] : "";

	if (!strcmp(want, "style")) {
		if (argc < 2)
			kb_die("usage: kdos theme style <file>");
		return cmd_theme_style(argv[1]);
	}

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
	theme_commit(sc);

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

	/*
	 * THE THREE LANES, first, because the most common question on this
	 * machine is which of them a thing belongs to — and the value of a
	 * list like this was never the commands, it is that one place shows
	 * the whole system's verbs.
	 */
	fprintf(o, "%sWHERE THINGS LIVE%s\n", C_A, C_0);
	fprintf(o, "  %-24s %s\n", "the host",
		"kpkg — compiled here from source, against musl");
	fprintf(o, "  %-24s %s\n", "applications",
		"kdos app — one signed file each; installing is a mount");
	fprintf(o, "  %-24s %s\n", "environments",
		"kdos-box — a box you built is a file you can give away");
	fprintf(o, "\n");

	fprintf(o, "%sCOMMANDS%s\n", C_A, C_0);
	static const char *CMDS[][2] = {
		{ "why <path|port>", "what provides this, and why it is that way" },
		{ "explain [topic]", "the recorded debug cycles, browsable" },
		{ "sandbox <prof> -- <cmd>", "run a native app under Landlock" },
		{ "desktop", "start the KDOS desktop from a tty (kdos-desktop)" },
		{ "kdos app search <t>", "the applications here and on the medium" },
		{ "kdos app install <id>", "one signed file, mounted — also remove, rollback" },
		{ "kdos-box list", "environments: create, enter, freeze, export" },
		{ "kdos-fetch-app <name>", "install an alien app from a network" },
		{ "kdos theme [name]", "phosphor | amber | ice | bone | norton | borland | perfect | next | prev | list" },
		{ "kdos theme style <f>", "apply a style file: accent + crt + fonts, shareable" },
		{ "kdos theme --audit", "is every generated colour still the palette's?" },
		{ "kdos status", "packages, containers, exported apps" },
		{ "kdos doctor", "check the session for common breakage" },
		{ "kdos appid", "do launcher icons match the windows they open?" },
		{ "kdos restarts", "what is running code an upgrade replaced" },
		{ "kdos stutter", "why the desktop hiccuped — with the app's name" },
		{ "kdos hey list", "every window, from a prompt; run <action> <id>" },
		{ "kdos update check", "what the ports tree pins that is not installed" },
		{ "kdos oracle", "one recorded lesson, picked for today" },
		{ "kdos trash <file>", "the desktop's trash, from a prompt — also --restore" },
		{ "kdos places", "the places column the desktop shows — also `add DIR`" },
		{ "kdos toggle [name]", "stay-awake, night-light, dnd — list, flip or set" },
		{ "kdos notify <text>", "raise a toast: `make && kdos notify done`" },
		{ "kdos con ls", "console sessions: new, attach, detach, kill, forward, run" },
		{ "kdos settings [page]", "the control centre — appearance, panel, hardware, system…" },
		{ "kdos clone [<dev>]", "the stick writes the stick — verified by read-back" },
		{ "kdos-shot [region]", "screenshot to clipboard and ~/Pictures" },
		{ "kdos-sfx notify", "the machine's four noises: login/notify/error/degauss" },
		{ "kdos-display [--list]", "the screens: mode, scale, rotation, order" },
		{ "kdos-fetch-static", "fetch a single verified static binary" },
		{ "kdos-power suspend", "suspend; also poweroff and reboot" },
		{ "kdos-energy", "which app is spending the battery" },
		{ "kdos-res", "resources: per-device pages, processes, apps" },
		{ "sudo kinstall", "install this live image onto a disk" },
		{ NULL, NULL }
	};
	for (int i = 0; CMDS[i][0]; i++)
		fprintf(o, "  %-26s %s\n", CMDS[i][0], CMDS[i][1]);
	fputc('\n', o);

	fprintf(o, "%sKEYS%s  %s(defaults — remap in "
		"~/.config/kdos-comp/rc.xml)%s\n", C_A, C_0, C_D, C_0);

	/*
	 * The authority on what is bound is the rc.xml the session loaded, and
	 * `kdos-keys` — the W-F1 keybind card in kdos-shell — is the program
	 * that reads it. This POINTS AT it rather than pasting it: `--dump`
	 * renders the card into a fixed 72x24 viewport with no scroll, so what
	 * came back was a 72-column box holding the first twenty bindings and
	 * an interactive hint row, silently missing every workspace, lock,
	 * screenshot and media key. A cheat sheet that stops two thirds of the
	 * way through is worse than a short one.
	 *
	 * So the table below is what `kdos help` prints, and it is not dead
	 * weight either way: this is answerable on a machine with no desktop
	 * installed at all, and that is exactly where somebody is reading it.
	 */
	if (kb_have_prog("kdos-keys"))
		fprintf(o, "  %s%-22s%s %s\n", C_B, "Super+F1", C_0,
			"the full card, generated from your own rc.xml");

	/* Every line here is a binding the skel rc.xml actually installs, and
	 * the file named above is the one that installs it: since the labwc
	 * fork, comp.conf keeps only the KDOS keys and skips a `bind` line in
	 * silence, so pointing a remapper at it was pointing them at a file
	 * that would ignore them. The list used to carry four keys that
	 * nothing bound — Alt+Tab, the snap arrows, Super+F, PrtSc — which is
	 * worse than a short cheat sheet: a key that the help says exists and
	 * the desktop ignores reads as a broken desktop.
	 *
	 * They are all bound now. Alt+Tab, Alt+F4 and the snap arrows come
	 * from labwc's OWN defaults, which the skel rc.xml loads with
	 * <default /> — without that line a file that binds one key throws
	 * every default away, and with them went click-to-focus and the
	 * titlebar. */
	static const char *KEYS[][2] = {
		{ "Super+D", "open the launcher" },
		{ "Ctrl+Shift+Esc", "open Resources" },
		{ "Alt+F2", "run a command" },
		{ "Super+Return", "terminal (foot)" },
		{ "Super+E", "files (mc)" },
		{ "PrtSc / Shift+PrtSc", "screenshot: a region / the screen" },
		{ "Super+Q / Alt+F4", "close window" },
		{ "Super+Tab / Alt+Tab", "switch window (most recent first)" },
		{ "Super+Shift+Tab", "switch window, backwards" },
		{ "Super+M", "maximize / restore" },
		{ "Super+F", "fullscreen / restore" },
		{ "Super+N", "minimize the window" },
		{ "Super+Space", "the root menu, without the desktop" },
		{ "Super+P", "the screens (kdos-display)" },
		{ "Super+Arrows", "snap the window to that half or corner" },
		{ "Alt+Space", "the window menu" },
		{ "Super+1..4", "switch workspace" },
		{ "Super+Shift+1..4", "move window to workspace" },
		{ "Super+drag", "move a window; Super+right-drag resizes" },
		{ "right-click desktop", "the root menu" },
		{ "Super+L", "lock the screen" },
		{ "Super+Escape", "end the session (it asks first)" },
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
/* Which session this process is in, or NULL. One place decides, so `kdos
 * info`, `kdos version` and `kdos doctor` cannot disagree. */
static const char *session_name(void);

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
	/* Naming the session rather than saying "wayland": on KDOS the answer
	 * is kdos-con or kdos-comp, and "wayland" would hide the case where the
	 * session is something else entirely. `session_name()` is the one place
	 * that decides, so this line cannot disagree with `kdos version`. */
	const char *sn = session_name();

	printf("%s%-16s%s %s\n", C_B, "Session", C_0,
	       !sn			      ? "tty"
	       : strcmp(sn, "kdos-comp")      ? sn
	       : running("kdos-comp", NULL)   ? "kdos-comp"
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

/*
 * The third level, and the Hardware section is what needs it. Half of what that
 * section asks cannot be answered in a VM: no SOF-capable audio controller, no
 * Wi-Fi, no NVIDIA GPU, no boot medium. `ok` there would be a green line for
 * something never tested, and `warn` would make every VM look broken. A check
 * that could not run says so and says why.
 */
static void skip_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void skip_(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("skip", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[skip]%s ", C_W, C_0);
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

/*
 * WHICH SESSION IS RUNNING. $KDOS_CON is set by the console session and by
 * nothing else, so it is the test and it is asked once: a check that reports a
 * missing compositor on a machine whose desktop is a cell grid teaches people
 * to ignore the tool that reported it.
 *
 * Returns "kdos-con", "kdos-comp", or NULL where neither is running — a shell
 * on tty2 has no session and saying so is the true answer.
 */
/*
 * `display` OUT OF A BOX PROFILE, by the same rule `kdos-con` reads it with.
 *
 * The parse is duplicated for the reason `con_conf_key` above it is: this
 * binary is on every image and links no libkcon, which would drag libktui and
 * the cell model in with it. What must NOT be duplicated is the rule, so this
 * matches `profile_display()` in `src/desktop/kdos-con/embed.c` line for line:
 * leading whitespace skipped, the key matched at the start of the line so a
 * commented-out `# display = vt` is not one, and whitespace after the `=`
 * skipped so `display=vt` is.
 *
 * A `strstr` for "display = vt" got both of those backwards: it reported a
 * commented-out line as pinned and missed the spaceless form the session
 * honours — a diagnostic disagreeing with the thing it is diagnosing.
 */
static int box_pinned_to_vt(const char *path)
{
	char *data = kb_read_all(path, NULL);
	int vt = 0;

	if (!data)
		return 0;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		char *eq;

		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		while (*line == ' ' || *line == '\t')
			line++;
		if (strncmp(line, "display", 7))
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		eq++;
		while (*eq == ' ' || *eq == '\t')
			eq++;
		vt = !strncmp(eq, "vt", 2);
		break;
	}

	free(data);
	return vt;
}

static const char *session_name(void)
{
	const char *kcon = getenv("KDOS_CON");
	const char *wl = getenv("WAYLAND_DISPLAY");

	if (kcon && *kcon)
		return "kdos-con";
	if (wl && *wl)
		return "kdos-comp";
	return NULL;
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

/*
 * The shipped image logs in as kdos/kdos and ships sshd enabled, so a machine
 * whose owner never ran passwd is a machine anyone on the network can log
 * into. The shipped hash cannot be compiled in (the source cannot read fs/ at
 * build time, and a copied literal goes stale the day the shadow file
 * changes), so the check IS the question: does "kdos" still hash to what
 * /etc/shadow holds?
 *
 * crypt() is weak so the host selftest — which links kdos-tools without
 * libcrypt — still links; on the target musl's libc carries it. Declared here
 * rather than via <crypt.h>, which a glibc host without libxcrypt lacks.
 */
#pragma weak crypt
extern char *crypt(const char *key, const char *salt);

static void check_default_password(void)
{
	/* Root-only: shadow is unreadable otherwise, and silence is correct —
	 * a non-root doctor cannot answer this and must not pretend to. */
	char *data = kb_read_all("/etc/shadow", NULL);
	if (!data || !crypt) {
		free(data);
		return;
	}

	char hash[128] = "";
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		if (!strncmp(line, "kdos:", 5)) {
			char *end = strchr(line + 5, ':');
			size_t l = end ? (size_t)(end - line - 5)
				       : strcspn(line + 5, "\n");
			if (l < sizeof(hash)) {
				memcpy(hash, line + 5, l);
				hash[l] = 0;
			}
			break;
		}
		line = nl ? nl + 1 : NULL;
	}
	free(data);

	/* No kdos user, or a locked/empty field: not this check's business. */
	if (!hash[0] || hash[0] == '!' || hash[0] == '*')
		return;

	char *h = crypt("kdos", hash);
	if (h && !strcmp(h, hash)) {
		if (kb_path_exists(INIT_DIR "/70_sshd"))
			warn_("the kdos password is still the shipped default "
			      "— and sshd is enabled, so anyone who can reach "
			      "this machine can log in (run: passwd)");
		else
			warn_("the kdos password is still the shipped default "
			      "(run: passwd)");
	} else {
		ok("the kdos password is not the shipped default");
	}
}


/* ------------------------------------------------------------------------
 * The Hardware section.
 *
 * Every check here covers a failure that is SILENT. A machine with no
 * regulatory.db has Wi-Fi; it just has no 5 GHz. A machine with no SOF
 * firmware has an audio device; it just makes no sound. A user outside
 * `dialout` has a /dev/ttyUSB0; they just cannot open it. None of those
 * announces itself, and all three read as broken hardware.
 * ------------------------------------------------------------------------ */

/* Read one small sysfs/proc file, trimmed. Returns NULL if absent. */
static char *hw_slurp(const char *path)
{
	size_t n = 0;
	char *d = kb_read_all(path, &n);
	if (!d)
		return NULL;
	while (n && (d[n - 1] == '\n' || d[n - 1] == ' ' || d[n - 1] == '\t'))
		d[--n] = 0;
	return d;
}

/* Does any directory entry under `dir` start with `pfx`? */
static int hw_dir_has_prefix(const char *dir, const char *pfx)
{
	DIR *d = opendir(dir);
	if (!d)
		return 0;
	struct dirent *e;
	size_t n = strlen(pfx);
	int found = 0;
	while ((e = readdir(d))) {
		if (!strncmp(e->d_name, pfx, n)) {
			found = 1;
			break;
		}
	}
	closedir(d);
	return found;
}

/*
 * The wireless regulatory database.
 *
 * kdos.config sets CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y, so the kernel will
 * load regulatory.db ONLY with a valid regulatory.db.p7s beside it. A database
 * regenerated locally from db.txt fails that check in silence and leaves the
 * radio exactly where it was: country 00, world-roaming, no 5 GHz DFS, reduced
 * TX power. So both files are checked, and the SIGNATURE is not optional.
 */
static void check_regdb(void)
{
	int have_db  = kb_path_exists("/lib/firmware/regulatory.db");
	int have_sig = kb_path_exists("/lib/firmware/regulatory.db.p7s");

	if (!have_db) {
		warn_("no /lib/firmware/regulatory.db — every radio is stuck "
		      "world-roaming: no 5 GHz DFS, reduced TX power");
		return;
	}
	if (!have_sig) {
		warn_("regulatory.db present but regulatory.db.p7s is not — "
		      "REQUIRE_SIGNED_REGDB rejects it silently, so this is "
		      "the same as having none");
		return;
	}

	/*
	 * The files being present is not the same as the kernel having loaded
	 * them. cfg80211 exposes the alpha2 it settled on; "00" is the world
	 * regulatory domain, which is what a failed load looks like.
	 */
	char *a2 = hw_slurp("/sys/module/cfg80211/parameters/ieee80211_regdom");
	if (a2 && *a2 && strcmp(a2, "00"))
		ok("regulatory.db signed and loaded (regdom %s)", a2);
	else if (!hw_dir_has_prefix("/sys/class/ieee80211", "phy"))
		skip_("regulatory.db and signature present; no wireless device "
		      "here to load them");
	else
		warn_("regulatory.db present but the regulatory domain is 00 "
		      "(world) — set one with: iw reg set <CC>");
	free(a2);
}

/*
 * Intel SOF audio firmware.
 *
 * CONFIG_SND_SOC_SOF=m binds the driver on every Tiger Lake and newer laptop,
 * and the firmware it then requests is not part of linux-firmware — upstream
 * ships it separately as thesofproject/sof-bin. Missing, it leaves a working
 * audio driver and no sound, which presents as the distro having no audio
 * support rather than as a missing file.
 */
static void check_sof(void)
{
	int sof_bound = hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-tgl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-mtl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-icl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-skl", "0000:");
	int have_fw  = kb_path_exists("/lib/firmware/intel/sof");
	int have_tpl = kb_path_exists("/lib/firmware/intel/sof-tplg");

	if (!sof_bound) {
		if (have_fw)
			ok("SOF firmware installed (no SOF audio device here)");
		else
			skip_("no SOF audio device on this machine — Intel DSP "
			      "firmware not required");
		return;
	}
	if (have_fw && have_tpl)
		ok("SOF firmware and topologies present");
	else if (have_fw)
		warn_("SOF firmware present but /lib/firmware/intel/sof-tplg "
		      "is missing — the DSP loads and no topology binds, which "
		      "is still silence");
	else
		warn_("an Intel SOF audio device is bound and "
		      "/lib/firmware/intel/sof is absent — this machine has no "
		      "sound (install the sof-firmware package)");
}

/*
 * The GPU firmware. nouveau on Turing and later cannot initialise without the
 * GSP blobs; without them the machine falls back to efifb, wlroots falls back
 * to pixman, and kdos_crt_init() declines a fullscreen post-process on
 * software rendering. The blobs are checked directly because the consequence
 * — a desktop that renders but does not look like KDOS — is several steps
 * removed from the cause.
 */
static void check_gpu_firmware(void)
{
	int nouveau = hw_dir_has_prefix("/sys/bus/pci/drivers/nouveau", "0000:");
	int have_gsp = kb_path_exists("/lib/firmware/nvidia");

	if (nouveau && !have_gsp)
		warn_("nouveau is bound and /lib/firmware/nvidia is absent — "
		      "Turing and later need GSP firmware to initialise at "
		      "all, so this falls back to efifb, then to the pixman "
		      "renderer, and the CRT pass declines");
	else if (nouveau)
		ok("nouveau bound with NVIDIA firmware present");
	else if (have_gsp)
		ok("NVIDIA GSP firmware installed (no nouveau device here)");
	else
		skip_("no NVIDIA GPU on this machine — GSP firmware not "
		      "required");
}

/*
 * DEVICE PRESENT BUT UNOPENABLE is the state this looks for, and the only way
 * to catch a udev rule that has stopped granting: the node exists, `ls -l`
 * looks plausible, and the program that wanted it reports something that
 * sounds like broken hardware.
 *
 * Walk the classes KDOS ships tools for and report each node the CALLING user
 * cannot open, naming the group that owns it — "add yourself to dialout" is an
 * instruction where "permission denied" is not.
 */
static void check_dev_access(void)
{
	static const char *globs[] = {
		"/dev/ttyUSB", "/dev/ttyACM", "/dev/video", "/dev/usbtmc", NULL
	};
	int checked = 0, denied = 0;
	char denied_names[512] = "";

	for (int g = 0; globs[g]; g++) {
		for (int i = 0; i < 8; i++) {
			char path[64];
			snprintf(path, sizeof(path), "%s%d", globs[g], i);
			if (!kb_path_exists(path))
				continue;
			checked++;
			if (access(path, R_OK | W_OK) == 0)
				continue;
			denied++;
			struct stat st;
			const char *grp = "?";
			char gbuf[64];
			if (!stat(path, &st)) {
				struct group *gr = getgrgid(st.st_gid);
				if (gr && gr->gr_name)
					grp = gr->gr_name;
				else {
					snprintf(gbuf, sizeof(gbuf), "gid %u",
						 (unsigned)st.st_gid);
					grp = gbuf;
				}
			}
			size_t used = strlen(denied_names);
			snprintf(denied_names + used, sizeof(denied_names) - used,
				 "%s%s (%s)", used ? ", " : "", path, grp);
		}
	}

	if (!checked)
		skip_("no serial, camera or instrument device plugged in — "
		      "nothing to check reachability against");
	else if (!denied)
		ok("all %d attached device nodes are reachable by this user",
		   checked);
	else
		warn_("%d of %d device nodes cannot be opened: %s — this user "
		      "is not in the owning group",
		      denied, checked, denied_names);
}

/*
 * The boot medium, after switch_root.
 *
 * The initramfs mounts the ISO at /mnt/iso and must MOVE that mount into the
 * new root; left behind it dies with the initramfs namespace and the booted
 * system cannot read the medium it is running from — `kdos rebuild` resolves
 * /mnt/iso/sources under it. On an installed system there is no boot medium
 * and this is not a fault.
 */
/*
 * BOXES AND PACKS. Half of this cannot be answered in a VM — there is often no
 * erofs module and no store — so those lines are `skip` with a reason. A green
 * line for something never tested is worse than an absent one; a `warn` for
 * every VM would make every VM look broken.
 */
static void check_packs(void)
{
	char buf[8192];
	int have_fs = 0;

	/* Can this kernel mount a pack at all? Everything else depends on it,
	 * and 59_packd.sh skips the daemon rather than respawning one that
	 * cannot do its job. */
	if (kb_read_file("/proc/filesystems", buf, sizeof(buf)) >= 0 &&
	    strstr(buf, "erofs"))
		have_fs = 1;
	if (have_fs)
		ok("erofs is available to the kernel");
	else if (kb_path_exists("/sbin/modprobe") || kb_path_exists("/usr/sbin/modprobe"))
		warn_("erofs is not loaded — `modprobe erofs`; without it "
		      "kdos-packd skips itself and no pack can be mounted");
	else
		skip_("no erofs in /proc/filesystems and no modprobe to try — "
		      "this kernel may not have the module at all");

	/* The daemon. Its absence is not a fault on a machine with no packs. */
	{
		struct sockaddr_un a;
		int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		int up = 0;

		if (fd >= 0) {
			memset(&a, 0, sizeof(a));
			a.sun_family = AF_UNIX;
			kb_strlcpy(a.sun_path, "/run/kdos-packd.sock",
				   sizeof(a.sun_path));
			if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0) {
				up = 1;
				if (write(fd, "status\n", 7) == 7) {
					ssize_t n = read(fd, buf, sizeof(buf) - 1);
					buf[n > 0 ? n : 0] = 0;
				} else {
					buf[0] = 0;
				}
			}
			close(fd);
		}
		if (up) {
			char *route = strstr(buf, "route\t");
			ok("kdos-packd is answering%s%.*s", route ? " — " : "",
			   route ? (int)strcspn(route + 6, "\n") : 0,
			   route ? route + 6 : "");
		} else if (kb_path_exists("/var/lib/kdos/packs/base.kpack")) {
			warn_("there are packs in /var/lib/kdos/packs and "
			      "kdos-packd is not answering — `service packd "
			      "status`");
		} else {
			skip_("no packs installed and no kdos-packd — this "
			      "machine runs the monolithic appbox image");
		}
	}

	/*
	 * THE uid-1000 ASSUMPTION. Packs are built --force-uid=1000, which is
	 * the one human user this distro ships. A second user's boxes still
	 * run — their /usr reads as somebody else — and saying so is better
	 * than leaving it to be discovered.
	 */
	if (getuid() != 0 && getuid() != 1000)
		warn_("you are uid %u and packs are built for uid 1000 — a box "
		      "will run, but its /usr will not read as yours",
		      (unsigned)getuid());

	/* An overlay upper cannot live on overlayfs, and a live session's
	 * $HOME does. The fallback is tmpfs and it is not silent. */
	{
		struct statfs st;
		const char *home = kb_home_dir();

		if (statfs(home, &st) != 0)
			skip_("cannot stat %s to say where a box's writes would "
			      "go", home);
		else if ((unsigned long)st.f_type == 0x794c7630UL)
			warn_("$HOME is on overlayfs (a live session), so a "
			      "persistent box cannot exist — its upper falls "
			      "back to tmpfs and its changes end with the "
			      "session");
		else
			ok("$HOME can hold an overlay upper — a persistent box "
			   "keeps what you write");
	}

	/* A mounted pack whose FILE is gone is a box that works until it is
	 * restarted, which is the worst kind of broken. */
	{
		size_t len = 0;
		char *mounts = kb_read_all("/proc/mounts", &len);
		int missing = 0, mounted = 0;

		if (mounts) {
			char *line, *save;
			for (line = strtok_r(mounts, "\n", &save); line;
			     line = strtok_r(NULL, "\n", &save)) {
				char *sp = strchr(line, ' ');
				char id[128], path[512];
				if (!sp || strncmp(sp + 1,
						   "/var/lib/kdos/packs/mnt/", 24))
					continue;
				mounted++;
				kb_strlcpy(id, sp + 25, sizeof(id));
				id[strcspn(id, " ")] = 0;
				snprintf(path, sizeof(path),
					 "/var/lib/kdos/packs/%s.kpack", id);
				if (!kb_path_exists(path))
					missing++;
			}
			free(mounts);
		}
		if (!mounted)
			skip_("nothing is mounted from the pack store");
		else if (missing)
			warn_("%d of %d mounted pack(s) no longer have a file "
			      "behind them — a box using one works until it is "
			      "restarted", missing, mounted);
		else
			ok("%d pack(s) mounted, every file still present",
			   mounted);
	}
}

static void check_boot_medium(void)
{
	char *mounts = kb_read_all("/proc/mounts", NULL);
	int live = mounts && strstr(mounts, " / overlay ") != NULL;
	int iso  = mounts && strstr(mounts, " /mnt/iso ") != NULL;
	free(mounts);

	if (!live) {
		skip_("installed system — no boot medium to reach");
		return;
	}
	if (iso)
		ok("boot medium reachable at /mnt/iso");
	else
		warn_("live session and /mnt/iso is not mounted — the "
		      "initramfs did not move it across switch_root, so "
		      "nothing on the medium is readable by name");
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



	doctor_head("Hardware");
	check_regdb();
	check_sof();
	check_gpu_firmware();
	check_dev_access();
	check_boot_medium();
	doctor_gap();

	doctor_head("Boxes");
	check_packs();
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
	const char *con_sock = getenv("KDOS_CON");

	/*
	 * THE CONSOLE SESSION IS A SESSION. Its surface socket is what a child
	 * inherits, and it is checked the same way the Wayland one is — the
	 * socket rather than the variable, because a variable that outlived
	 * its session is exactly the state this block exists to catch.
	 */
	if (con_sock && *con_sock) {
		if (kb_path_exists(con_sock))
			ok("KDOS_CON=%s", con_sock);
		else
			warn_("KDOS_CON=%s but the socket is gone — the "
			      "session it names has ended", con_sock);
	}
	if (!wd || !*wd) {
		if (!con_sock || !*con_sock)
			warn_("neither KDOS_CON nor WAYLAND_DISPLAY — not "
			      "inside a desktop session");
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

	/*
	 * THREE CHECKS THAT ONLY APPLY TO ONE OF THE TWO DESKTOPS. The
	 * compositor, its panel and the wlroots portal are absent by design on
	 * a console session — the panel needs a foreign-toplevel manager the
	 * console does not offer, and the console starts a portal backend of
	 * its own — so reporting them as faults there fails a working machine.
	 */
	const char *sess = session_name();
	int on_console = sess && !strcmp(sess, "kdos-con");

	if (on_console) {
		if (running("kdos-con", NULL))
			ok("kdos-con running");
		else
			warn_("kdos-con not running — no session (start with: "
			      "kdos-con-start)");
		if (running("kdos-view", NULL))
			ok("kdos-view attached — the session has a display");
		else
			warn_("no kdos-view attached — the session is running "
			      "and nothing is showing it. See "
			      "$XDG_RUNTIME_DIR/kdos-view.log");
	} else {
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
			warn_("wlr portal not running — screen capture and "
			      "file pickers degraded");
	}

	/*
	 * HOW A GRAPHICAL APPLICATION WILL BE SHOWN on the console desktop.
	 * The two answers look nothing alike — a window among the cells, or a
	 * full-screen guest on a terminal of its own — and which one a person
	 * gets is decided by files rather than by anything they can see. So the
	 * inputs are reported, not the decision: kdos-con makes that, and a
	 * second implementation here would be a second thing to be wrong.
	 */
	if (on_console) {
		char *ce = con_conf_key("embed");
		int off = ce && (!strcmp(ce, "false") || !strcmp(ce, "0") ||
				 !strcmp(ce, "no"));

		if (off)
			ok("graphical applications take a terminal of their "
			   "own — con.conf says embed = false");
		else
			ok("graphical applications are windows — kdos-cage "
			   "composites each in a process of its own");
		free(ce);

		char *bd = kdt_cfg_home("kdos/boxes");
		DIR *bdd = bd ? opendir(bd) : NULL;
		char pinned[256] = { 0 };

		if (bdd) {
			struct dirent *be;

			while ((be = readdir(bdd))) {
				char path[600];
				size_t nl = strlen(be->d_name);

				if (nl < 6 || strcmp(be->d_name + nl - 5, ".conf"))
					continue;
				snprintf(path, sizeof(path), "%s/%s", bd,
					 be->d_name);
				if (box_pinned_to_vt(path)) {
					size_t at = strlen(pinned);

					snprintf(pinned + at,
						 sizeof(pinned) - at, "%s%.*s",
						 at ? ", " : "",
						 (int)(nl - 5), be->d_name);
				}
			}
			closedir(bdd);
		}
		free(bd);
		if (*pinned)
			ok("pinned to a terminal of their own: %s", pinned);
	}
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
	char *ct = kdt_cache_home("kdos/theme");
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

	/*
	 * The SECOND setuid bit. Losing it is silent in a different way from
	 * the lock screen's: the monitor still runs and still draws, and only
	 * the verbs that need privilege stop working — so it presents as a
	 * task manager that cannot end anything it does not own.
	 */
	struct stat rst;
	if (stat("/usr/bin/kdos-resctl", &rst) != 0)
		warn_("kdos-resctl missing — kdos-res cannot end a process it "
		      "does not own, and the Memory page has no DIMM details");
	else if ((rst.st_mode & S_ISUID) && rst.st_uid == 0)
		ok("kdos-resctl is setuid root");
	else
		warn_("kdos-resctl is not setuid root — kdos-res cannot end a "
		      "process it does not own (fix: chown root and chmod 4755)");

	/*
	 * AND THE TWO THIS DISTRO DOES NOT OWN, which are the ones every BOX
	 * depends on. podman runs `newuidmap` to write /proc/<pid>/uid_map for
	 * the user namespace a rootless container needs; without the bit it
	 * refuses with "should have setuid or have filecaps setuid" and exits
	 * 125 having printed nothing else, so every alien application on the
	 * machine stops starting and nothing says why. `/etc/subuid` is
	 * checked with them because an empty one fails at the same call for a
	 * different reason.
	 */
	static const char *const uidmap[] = { "/usr/bin/newuidmap",
					      "/usr/bin/newgidmap", NULL };
	for (int i = 0; uidmap[i]; i++) {
		struct stat ust;
		if (stat(uidmap[i], &ust) != 0)
			warn_("%s missing — no rootless container can map a "
			      "uid, so no box will start", uidmap[i]);
		else if ((ust.st_mode & S_ISUID) && ust.st_uid == 0)
			ok("%s is setuid root", uidmap[i]);
		else
			warn_("%s is not setuid root — podman exits 125 and no "
			      "box starts (fix: chown root and chmod 4755)",
			      uidmap[i]);
	}
	if (kb_path_exists("/etc/subuid") && kb_path_exists("/etc/subgid"))
		ok("/etc/subuid and /etc/subgid are present");
	else
		warn_("/etc/subuid or /etc/subgid is missing — a rootless box "
		      "has no subordinate range to map into");

	/* `service <name>` keys on the init SCRIPT — ksvc strips the numeric
	 * prefix and the .sh, so 55_powerd.sh is `powerd`. The NAME= inside the
	 * script is the pidfile, not the lookup key, and naming that here
	 * printed a command that finds no service. */
	if (kb_path_exists("/run/kdos-powerd.sock"))
		ok("kdos-powerd listening");
	else
		warn_("kdos-powerd not running — no suspend, poweroff or reboot "
		      "from the desktop (start with: service powerd start)");

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
			      "attribution (start with: service energyd "
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

	/*
	 * The out-of-memory killer. Without it the appbox — which is the
	 * likeliest thing on this machine to eat the memory — takes the desktop
	 * down with it, and the kernel's own OOM killer picks by badness rather
	 * than by who the user is looking at.
	 */
	if (kb_path_exists("/run/kdos-oomd.sock"))
		ok("kdos-oomd watching /proc/pressure/memory");
	else
		warn_("kdos-oomd not running — nothing protects the session "
		      "under memory pressure (start with: service oomd "
		      "start)");

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

		/*
		 * The command socket, and the two failures it distinguishes.
		 * A compositor that IS running without one is an older
		 * kdos-comp — `kdos hey` and kdos-teams then fail with a
		 * message about a socket, which sounds like a permissions
		 * problem and is a version.
		 */
		char *cs = kb_path_join(rtd, "kdos-cmd.sock");
		if (kb_path_exists(cs))
			ok("kdos-comp answers `kdos hey`");
		else if (running("kdos-comp", NULL))
			warn_("kdos-comp is running but exposes no command "
			      "socket — `kdos hey` and kdos-teams have nothing "
			      "to talk to (this compositor predates it)");
		else
			warn_("no command socket — `kdos hey` needs a running "
			      "kdos-comp");
		free(cs);
	}

	/*
	 * The keybind card. It is the one surface that answers "what can I
	 * press", on a desktop where every operation has a key and almost none
	 * of them is discoverable any other way — and `kdos help` quietly falls
	 * back to a static table when it is missing, so its absence is
	 * otherwise invisible.
	 */
	if (kb_have_prog("kdos-keys"))
		ok("kdos-keys installed — W-F1 and `kdos help` read the real "
		   "rc.xml");
	else
		warn_("kdos-keys missing — no keybind card, and `kdos help` "
		      "falls back to a hand-written key list that rc.xml can "
		      "contradict");

	/*
	 * The screen-capture portal, which is two files and one naming rule.
	 * xdg-desktop-portal picks a backend by reading <desktop>-portals.conf
	 * with XDG_CURRENT_DESKTOP lowercased, and wlr.portal's own UseIn= list
	 * has never heard of KDOS — so without that file ScreenCast comes back
	 * with no backend and the app says "no capture sources available",
	 * which sounds like a driver problem and is not.
	 *
	 * TWO FILES, BECAUSE THERE ARE TWO DESKTOPS. The compositor's backend is
	 * a Wayland client and there is none on the console, where the backend
	 * is a second kdos-view rasterising into a stream instead.
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

	if (kb_path_exists("/usr/share/xdg-desktop-portal/kdos-console-portals.conf"))
		ok("the console session records through kdos-view --cast");
	else
		warn_("kdos-console-portals.conf missing — recording the "
		      "console session would look for a Wayland backend that "
		      "cannot run there");

	const char *path = getenv("PATH");
	char *want = kb_path_join(kb_home_dir(), ".local/bin");
	if (path && strstr(path, want))
		ok("~/.local/bin on PATH");
	else
		warn_("~/.local/bin not on PATH — exported app wrappers will not "
		      "resolve");
	free(want);

	/* Root-only — a normal user cannot read shadow and gets no section at
	 * all rather than a check that pretends it looked. */
	if (access("/etc/shadow", R_OK) == 0) {
		doctor_gap();
		doctor_head("Security");
		check_default_password();
	}

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
	const char *sess = session_name();

	if (sess)
		printf("  session  %s (%s)\n", sess, current_theme());
	else
		printf("  session  none (%s)\n", current_theme());
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * `kdos con …` — the console desktop's sessions.
 *
 * EXECS kdos-con RATHER THAN LINKING THE PROTOCOL. kdos-tools is on every
 * image and must stay thin; libkcon would drag libktui and the cell model in
 * behind it for the sake of five verbs that are one argument each.
 *
 * `forward` is the exception and lives here, because it is entirely an ssh
 * command line and kdos-con has no business knowing about ssh.
 */
static int cmd_con_forward(int argc, char **argv)
{
	const char *host = argc > 0 ? argv[0] : NULL;
	const char *run = getenv("XDG_RUNTIME_DIR");
	const char *name = argc > 1 ? argv[1] : "con";
	char local[256];

	if (!host) {
		fprintf(stderr, "usage: kdos con forward <host> [session]\n");
		return 2;
	}
	if (!run || !*run) {
		fprintf(stderr, "kdos con: no XDG_RUNTIME_DIR\n");
		return 1;
	}

	/*
	 * REMOTE IS OFF BY DEFAULT AND THIS IS WHERE IT IS ENFORCED. There is
	 * no TCP listener anywhere in this desktop, so a remote view can only
	 * arrive through a tunnel somebody set up — and refusing to set one up
	 * is a complete refusal, not a check that can be bypassed by connecting
	 * some other way.
	 */
	char conf[4096];
	int allow = 0;

	if (kb_read_file("/etc/kdos/con.conf", conf, sizeof(conf)) > 0) {
		char *line, *save;

		for (line = strtok_r(conf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *hash = strchr(line, '#'), *eq;

			if (hash)
				*hash = '\0';
			eq = strchr(line, '=');
			if (!eq || strncmp(line, "remote", 6))
				continue;
			allow = strstr(eq, "yes") || strstr(eq, "1");
			break;
		}
	}
	if (!allow) {
		fprintf(stderr,
			"kdos con: remote is off. Set `remote = yes` in\n"
			"          /etc/kdos/con.conf (or ~/.config/kdos-con/con.conf)\n"
			"          to allow a view on another machine.\n");
		return 1;
	}

	snprintf(local, sizeof(local), "%s/kdos/%s.view", run, name);
	if (!kb_path_exists(local)) {
		fprintf(stderr, "kdos con: no session '%s'\n", name);
		return 1;
	}

	/*
	 * THE VIEW SOCKET AND ONLY THE VIEW SOCKET. A display is handed cells
	 * and reports events; forwarding the surface socket would hand the far
	 * end the right to place windows in this session, which is a different
	 * thing entirely from showing it.
	 *
	 * The tunnel dies with the ssh process, so there is nothing to tear
	 * down on a dropped connection — that is the whole reason it is a
	 * foreground ssh rather than a daemon and a lock file.
	 */
	char spec[512];

	snprintf(spec, sizeof(spec), "/tmp/kdos-%s-%d.view:%s", name,
		 (int)getuid(), local);

	char cmd[768];

	snprintf(cmd, sizeof(cmd),
		 "kdos-view --tty --socket /tmp/kdos-%s-%d.view", name,
		 (int)getuid());

	printf("forwarding %s to %s — the session ends when this exits\n",
	       local, host);

	execlp("ssh", "ssh", "-t", "-R", spec, host, cmd, (char *)NULL);
	fprintf(stderr, "kdos con: cannot run ssh\n");
	return 127;
}

/* The longest guest command line this front end passes on. It matches
 * KCON_MAX_ARGV, which is what the session's wire carries — a longer one is
 * refused there, and refusing it here as well would be a second limit to keep
 * in step. */
#define KDT_RUN_MAX 32

/*
 * `kdos settings [page]` — the control centre from a prompt.
 *
 * THE PAGE NAME IS NOT VALIDATED HERE. `kdos-settings` owns the list and
 * already refuses a name it does not have; a second copy of it in this file is
 * a second list to keep in step, and the failure mode of the copy going stale
 * is a page that exists and cannot be reached from the command line.
 *
 * No shell: the page word comes from a command line and reaches execvp as one
 * argument, so a name with a space in it is a name, not two arguments.
 */
static int cmd_settings(int argc, char **argv)
{
	const char *av[4];
	int n = 0;

	av[n++] = "kdos-settings";
	if (argc > 0 && argv[0][0]) {
		av[n++] = "--page";
		av[n++] = argv[0];
	}
	av[n] = NULL;
	execvp(av[0], (char *const *)av);
	fprintf(stderr, "kdos settings: kdos-settings is not installed\n");
	return 127;
}

static int cmd_con(int argc, char **argv)
{
	static const struct { const char *verb, *flag; } V[] = {
		{ "ls",     "--ls" },
		{ "new",    "--new" },
		{ "attach", "--attach" },
		{ "detach", "--detach" },
		{ "kill",   "--kill" },
	};
	const char *verb = argc > 0 ? argv[0] : "ls";

	if (!strcmp(verb, "forward"))
		return cmd_con_forward(argc - 1, argv + 1);

	/*
	 * `run` IS the argument tunnel the five verbs below are not, and
	 * deliberately: everything after it is the guest's argument vector,
	 * passed on whole. Re-joining it into a string here would be inventing
	 * a quoting rule for something that already had none.
	 */
	if (!strcmp(verb, "run")) {
		const char *av[KDT_RUN_MAX + 4];
		int n = 0, i = 1;

		if (argc > 1 && !strcmp(argv[1], "--"))
			i = 2;
		if (i >= argc) {
			fprintf(stderr, "usage: kdos con run [--] CMD [ARG...]\n");
			return 2;
		}
		av[n++] = "kdos-con";
		av[n++] = "--run";
		for (; i < argc && n < KDT_RUN_MAX + 3; i++)
			av[n++] = argv[i];
		av[n] = NULL;
		execvp(av[0], (char *const *)av);
		fprintf(stderr, "kdos con: kdos-con is not installed\n");
		return 127;
	}

	for (int i = 0; i < (int)(sizeof(V) / sizeof(V[0])); i++) {
		if (strcmp(verb, V[i].verb))
			continue;

		const char *av[8];
		int n = 0;

		av[n++] = "kdos-con";
		av[n++] = V[i].flag;
		/* A name, if one was given. Anything else is not passed on:
		 * this is a five-verb front end, not an argument tunnel. */
		if (argc > 1 && n + 2 < (int)(sizeof(av) / sizeof(av[0]))) {
			av[n++] = "-t";
			av[n++] = argv[1];
		}
		av[n] = NULL;
		execvp(av[0], (char *const *)av);
		fprintf(stderr, "kdos con: kdos-con is not installed\n");
		return 127;
	}

	fprintf(stderr,
		"usage: kdos con {ls|new|attach|detach|kill} [session]\n"
		"       kdos con forward <host> [session]\n"
		"       kdos con run [--] CMD [ARG...]\n");
	return 2;
}

/*
 * ── A TOAST FROM A PROMPT ───────────────────────────────────────────────
 *
 *   make && kdos notify "the build finished"
 *
 * `notify-send` is not on this image and `libnotify` is not a port, so a long
 * job had no way to say it was done.
 *
 * `kdos-notify` IS NOT THE SENDER — it is the notification centre, a viewer of
 * what has already arrived. A toast comes from the bus, which is what
 * `kb_notify()` speaks, and that is the one sender in the whole tree: the same
 * call a terminal makes for a child's OSC 9, so the two cannot drift apart.
 */
static int cmd_notify(int argc, char **argv)
{
	if (argc < 1) {
		fprintf(stderr, "usage: kdos notify <summary> [body]\n");
		return 2;
	}
	kb_notify("kdos", argv[0], argc > 1 ? argv[1] : "");
	return 0;
}

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
	if (!strcmp(cmd, "toggle"))
		return cmd_toggle(rest, restv);
	if (!strcmp(cmd, "notify"))
		return cmd_notify(rest, restv);
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
	if (!strcmp(cmd, "clone"))
		return clone_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "cve"))
		return kdt_cve(rest, restv, C_A, C_W, C_0);
	if (!strcmp(cmd, "hey"))
		return hey_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "oracle"))
		return oracle_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "update"))
		return kdt_update(rest, restv, cmd_theme);
	if (!strcmp(cmd, "version") || !strcmp(cmd, "-V"))
		return cmd_version();
	if (!strcmp(cmd, "app"))
		return kdt_app(argc - 2, argv + 2);
	if (!strcmp(cmd, "trash"))
		return kdt_trash(argc - 2, argv + 2);
	if (!strcmp(cmd, "places"))
		return kdt_places(argc - 2, argv + 2);
	if (!strcmp(cmd, "thumb"))
		return kdt_thumb(argc - 2, argv + 2);
	if (!strcmp(cmd, "con"))
		return cmd_con(argc - 2, argv + 2);
	if (!strcmp(cmd, "settings"))
		return cmd_settings(argc - 2, argv + 2);

	fprintf(stderr, "%skdos:%s unknown command '%s' — try: kdos help\n", C_W,
		C_0, cmd);
	return 1;
}
