/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos theme — one accent, everywhere at once
 *
 *   kdos theme                     the accent in force
 *   kdos theme list | next | prev
 *   kdos theme <accent>            switch the whole machine to it
 *   kdos theme --from-plasma <a>   kded-kdos, after the Global Theme page
 *   kdos theme --audit [accent]    is what is here what the palette makes?
 *   kdos theme style <file>        an accent and the phosphor tuning, shared
 *
 * THE PLASMA HALF IS ALREADY ON DISK. Every colour scheme, Konsole profile,
 * qt5ct palette, wallpaper and global theme was generated from libkcolor's
 * table when the image was built (kdos-theme, kdos-plasma-look), so an accent
 * switch is a choice between files that exist: Plasma is told which global
 * theme to wear, and repaints itself. What is generated here is only what no
 * package can ship: the configuration of the terminal programs a person lives
 * in, which sit in their home and are read by programs that are not KDE's.
 *
 * KDOS REPLACES ONLY WHAT IS KDOS'S (P8). A wallpaper, a Konsole profile, a
 * dark-theme pairing or a qt5ct palette the person chose is left exactly as
 * they set it: each step first reads the value in force and moves it only
 * while it still names a KDOS file. A KDOS accent switch that took a person's
 * own wallpaper with it would be a switch nobody could use twice.
 *
 * EVERY PROGRAM THROUGH AN ARGUMENT VECTOR and every Plasma call over the
 * session bus (bus.c); no shell anywhere.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

/* ANSI, but only when someone is looking: the same rule kdos.c follows. */
static const char *C_A = "", *C_D = "", *C_0 = "";

static void colours(void)
{
	if (!isatty(STDOUT_FILENO))
		return;
	C_A = "\033[1;32m";
	C_D = "\033[2;32m";
	C_0 = "\033[0m";
}

/* ── names ─────────────────────────────────────────────────────────────── */

/* "Bone": the table's theme name past its "KDOS-". The generators derive every
 * installed file name from the same field, so a title typed here would be a
 * ninth accent the day somebody renamed one. */
const char *kdt_accent_title(const KcolScheme *sc)
{
	static const char pfx[] = "KDOS-";

	if (!strncmp(sc->theme_name, pfx, sizeof(pfx) - 1))
		return sc->theme_name + sizeof(pfx) - 1;
	return sc->theme_name;
}

/* kdos-theme's rule, number for number: a scheme is light when its ground
 * is. Which icon theme, wallpaper variant and dark-theme pairing an accent
 * gets all follow from it, so the two programs must not answer differently. */
int kdt_accent_light(const KcolScheme *sc)
{
	return kcol_luma(sc->deep) >= 0.5;
}

/*
 * THE ACCENT IN FORCE: the word in $XDG_CONFIG_HOME/kdos/accent, which this
 * command writes first and kded-kdos compares the Global Theme page against.
 * With none written, the colour scheme Plasma is wearing names it when that is
 * a KDOS one — a person who picked an accent on the Global Theme page before
 * ever running this has one — and otherwise it is the default.
 */
const char *kdt_current_accent(void)
{
	static char name[32];
	char *p = kdt_cfg_home("kdos/accent"), *cs;

	name[0] = '\0';
	if (kb_read_line_file(p, name, sizeof(name)) > 0 && kcol_find(name)) {
		free(p);
		return name;
	}
	free(p);
	cs = kdt_kcfg_cascade("kdeglobals", "General", "ColorScheme");
	if (cs && !strncmp(cs, "KDOS", 4)) {
		for (int i = 0; i < kcol_nscheme; i++)
			if (!strcmp(kdt_accent_title(&kcol_schemes[i]), cs + 4)) {
				kb_strlcpy(name, kcol_schemes[i].name,
					   sizeof(name));
				free(cs);
				return name;
			}
	}
	free(cs);
	return KCOL_DEFAULT_NAME;
}

/* The sidecar's "under /usr/share/wallpapers/KDOS-" test, with or without the
 * file:// a Plasma URL carries. */
static int kdos_wallpaper(const char *v)
{
	static const char dir[] = "/usr/share/wallpapers/KDOS-";

	if (!v || !*v)
		return 1;
	if (!strncmp(v, "file://", 7))
		v += 7;
	return !strncmp(v, dir, sizeof(dir) - 1);
}

/* ── the terminal programs' palettes ─────────────────────────────────── */

/*
 * The derived ANSI palette, libkcolor's kcol_ansi(): the scheme has no blue,
 * no magenta and no cyan, and mapping those slots onto `dim` and `secondary`
 * would put ls directories at 1.7:1 contrast, make vim comments invisible and
 * collapse three distinguishable colours onto one. The Konsole schemes
 * kdos-theme generates take the same sixteen, so a program's palette and the
 * terminal's agree. The fields below are the slots these writers name.
 */
typedef struct {
	uint32_t blue, magenta, cyan;		/* regular4/5/6            */
	uint32_t bblue, bmagenta, bcyan;	/* bright4/5/6             */
	uint32_t bprimary, bsecondary;		/* bright2/3               */
	uint32_t burgent, btext;		/* bright1/7               */
} AnsiDerived;

static void ansi_all(const KcolScheme *sc, AnsiDerived *o)
{
	KcolAnsi a;

	kcol_ansi(sc, &a);
	o->blue = a.normal[4];
	o->magenta = a.normal[5];
	o->cyan = a.normal[6];
	o->bblue = a.bright[4];
	o->bmagenta = a.bright[5];
	o->bcyan = a.bright[6];
	o->bprimary = a.bright[2];
	o->bsecondary = a.bright[3];
	o->burgent = a.bright[1];
	o->btext = a.bright[7];
}

/*
 * ── THE PROGRAMS A TERMINAL USER LIVES IN ───────────────────────────────
 *
 * Each writes one generated file that the program's own configuration
 * includes, and each was written against the program's own parser, because every one of them fails a generated file
 * DIFFERENTLY and most of them fail it in silence:
 *
 *   micro DISCARDS THE WHOLE SCHEME on one unparsable line and falls back to
 *   its default, so a stray character costs every colour rather than one.
 *
 *   newsboat CANNOT BE TOLD A COLOUR AT ALL. `#` is its comment character, so
 *   a hex value truncates the line and the entry is refused for having too
 *   few parameters. It takes the 256 indices, which is why kcol_xterm256()
 *   exists — the scheme reaches it approximated or not at all.
 *
 *   fzf and delta both treat `#` as a comment in the wrong place: a space
 *   before a colour in fzf, an unquoted value in delta, and the colour is
 *   silently dropped with a zero exit.
 *
 *   bat reads three-digit hex as raw nibbles rather than expanding it, so a
 *   short form renders near-black with no warning.
 *
 * WHAT IS NOT HERE, AND WHY: lazygit. Its configuration has no include, no
 * import and no separate theme file — the colours live in `gui.theme` inside
 * the one `config.yml` a person edits. Writing them would mean owning that
 * file and throwing away whatever else they had put in it, which is the one
 * thing a generated file must never do.
 */

/*
 * bat: a Sublime `.tmTheme` plist, and the name is the FILE STEM rather than
 * the `name` key inside it — `--theme=kdos` finds this file, `--theme="KDOS"`
 * finds nothing. Hex must be the six-digit form: bat reads `#abc` as the raw
 * nibbles rather than expanding it, and renders a near-black palette without
 * a word of complaint.
 *
 * THE CACHE IS REBUILT AFTER, and its exit status means nothing — `bat cache
 * --build` returns 0 having dropped a theme it could not parse, so the only
 * way to know is to ask for the theme back.
 */
static void write_bat(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("bat/themes/kdos.tmTheme");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n<dict>\n"
		"<!-- KDOS bat theme — GENERATED by `kdos theme`; edits will "
		"be overwritten. -->\n"
		"  <key>name</key><string>KDOS</string>\n"
		"  <key>settings</key>\n  <array>\n"
		"    <dict><key>settings</key><dict>\n"
		"      <key>background</key><string>#%s</string>\n"
		"      <key>foreground</key><string>#%s</string>\n"
		"      <key>caret</key><string>#%s</string>\n"
		"      <key>lineHighlight</key><string>#%s</string>\n"
		"      <key>selection</key><string>#%s</string>\n"
		"      <key>gutterForeground</key><string>#%s</string>\n"
		"    </dict></dict>\n"
		"    <dict><key>scope</key><string>comment, "
		"punctuation.definition.comment</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>string</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>constant.numeric, "
		"constant.language</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>keyword, storage</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string><key>fontStyle</key><string>bold</string>"
		"</dict></dict>\n"
		"    <dict><key>scope</key><string>entity.name.function, "
		"support.function</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>entity.name.type, "
		"support.type, support.class</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>variable</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"    <dict><key>scope</key><string>invalid</string>\n"
		"      <key>settings</key><dict><key>foreground</key>"
		"<string>#%s</string></dict></dict>\n"
		"  </array>\n</dict>\n</plist>\n",
		deep, text, p, dim, dim, mut,
		mut, p, sec, urg, p, var, text, urg);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);

	/*
	 * THE CACHE, REBUILT — a theme file bat has not cached is a theme bat
	 * does not have. Forked rather than linked for the reason the icon and
	 * cursor writers fork: it is another program's job.
	 */
	if (kb_have_prog("bat")) {
		KbArgv a = { 0 };

		kb_argv_add(&a, "bat");
		kb_argv_add(&a, "cache");
		kb_argv_add(&a, "--build");
		kb_argv_end(&a);
		kb_run(&a);
	}
}

/* micro: `include "kdos"` from a scheme of the person's own, or the file is
 * selected by name in settings.json. The directory is made here because micro
 * creates only its config root and reads this one with the error discarded —
 * an absent directory registers nothing and the name resolves to a built-in. */
static void write_micro(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("micro/colorschemes/kdos.micro");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS micro colorscheme — GENERATED by `kdos theme`; edits "
		"will be overwritten.\n"
		"# Whole-line comments only: micro discards the entire scheme "
		"on one bad line.\n"
		"color-link default \"#%s,#%s\"\n"
		"color-link comment \"#%s\"\n"
		"color-link identifier \"#%s\"\n"
		"color-link constant \"#%s\"\n"
		"color-link constant.string \"#%s\"\n"
		"color-link statement \"#%s\"\n"
		"color-link preproc \"#%s\"\n"
		"color-link type \"#%s\"\n"
		"color-link special \"#%s\"\n"
		"color-link symbol \"#%s\"\n"
		"color-link underlined \"underline #%s\"\n"
		"color-link error \"bold #%s\"\n"
		"color-link todo \"bold #%s\"\n"
		"color-link selection \"#%s,#%s\"\n"
		"color-link statusline \"#%s,#%s\"\n"
		"color-link tabbar \"#%s,#%s\"\n"
		"color-link line-number \"#%s,#%s\"\n"
		"color-link current-line-number \"#%s,#%s\"\n"
		"color-link cursor-line \"#%s\"\n"
		"color-link color-column \"#%s\"\n"
		"color-link match-brace \"#%s,#%s\"\n",
		text, deep, mut, p, sec, p, urg, urg, sec, var, sec,
		p, urg, sec, deep, p, deep, text, deep, text,
		mut, dim, sec, dim, dim, dim, deep, sec);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* helix: the host's helix, and a helix installed in a box (a box takes
 * `home = shared` by default and so is bind-mounted this `$HOME`), both read
 * this file, and nothing else would give it the accent. Nothing in /etc/skel
 * selects it — the `theme = "kdos"` line is the person's to write, which is
 * why the generated header says so.
 *
 * No include exists — `inherits = "kdos"` from a theme of the person's own is
 * the composition it has. A palette entry must be hex or an index; a colour
 * NAME there is a parse error that loses the theme. */
static void write_helix(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("helix/themes/kdos.toml");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS helix theme — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"# Your own theme may `inherits = \"kdos\"`; helix has no "
		"include.\n"
		"# Select it with `theme = \"kdos\"` in your own "
		"config.toml — that file is yours and is not shipped.\n"
		"\"ui.background\" = { bg = \"deep\" }\n"
		"\"ui.text\" = { fg = \"text\" }\n"
		"\"ui.text.focus\" = { fg = \"primary\", modifiers = [\"bold\"] }\n"
		"\"ui.cursor\" = { fg = \"deep\", bg = \"primary\" }\n"
		"\"ui.cursor.primary\" = { fg = \"deep\", bg = \"primary\" }\n"
		"\"ui.linenr\" = { fg = \"muted\" }\n"
		"\"ui.linenr.selected\" = { fg = \"secondary\" }\n"
		"\"ui.selection\" = { bg = \"dim\" }\n"
		"\"ui.statusline\" = { fg = \"deep\", bg = \"primary\" }\n"
		"\"ui.statusline.inactive\" = { fg = \"text\", bg = \"dim\" }\n"
		"\"ui.help\" = { fg = \"text\", bg = \"dim\" }\n"
		"\"ui.popup\" = { fg = \"text\", bg = \"dim\" }\n"
		"\"ui.menu\" = { fg = \"text\", bg = \"dim\" }\n"
		"\"ui.menu.selected\" = { fg = \"deep\", bg = \"primary\" }\n"
		"\"ui.virtual.whitespace\" = { fg = \"muted\" }\n"
		"\"comment\" = { fg = \"muted\", modifiers = [\"italic\"] }\n"
		"\"variable\" = { fg = \"text\" }\n"
		"\"constant\" = { fg = \"secondary\" }\n"
		"\"string\" = { fg = \"primary\" }\n"
		"\"keyword\" = { fg = \"urgent\" }\n"
		"\"function\" = { fg = \"primary\" }\n"
		"\"type\" = { fg = \"variant\" }\n"
		"\"operator\" = { fg = \"secondary\" }\n"
		"\"punctuation\" = { fg = \"muted\" }\n"
		"\"error\" = { fg = \"urgent\" }\n"
		"\"warning\" = { fg = \"secondary\" }\n"
		"\n[palette]\n"
		"primary = \"#%s\"\n"
		"secondary = \"#%s\"\n"
		"urgent = \"#%s\"\n"
		"deep = \"#%s\"\n"
		"text = \"#%s\"\n"
		"variant = \"#%s\"\n"
		"dim = \"#%s\"\n"
		"muted = \"#%s\"\n",
		p, sec, urg, deep, text, var, dim, mut);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* neovim: an ordinary vimscript colorscheme. LF endings — a CRLF file half
 * loads, which is worse than not loading, and `hi clear` before anything is
 * what makes a re-source clean. */
static void write_nvim(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("nvim/colors/kdos.vim");
	kdt_mkparent(f);

	char p[8], dim[8], sec[8], urg[8], deep[8], text[8], var[8], mut[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->dim, dim);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(sc->deep, deep);
	kcol_format(sc->text, text);
	kcol_format(sc->variant, var);
	kcol_format(kcol_muted(sc), mut);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"\" KDOS neovim colorscheme — GENERATED by `kdos theme`; edits "
		"will be overwritten.\n"
		"hi clear\n"
		"if exists(\"syntax_on\")\n  syntax reset\nendif\n"
		"set background=dark\n"
		"let g:colors_name = \"kdos\"\n"
		"hi Normal guifg=#%s guibg=#%s\n"
		"hi Comment guifg=#%s gui=italic\n"
		"hi Constant guifg=#%s\n"
		"hi String guifg=#%s\n"
		"hi Identifier guifg=#%s\n"
		"hi Function guifg=#%s\n"
		"hi Statement guifg=#%s\n"
		"hi PreProc guifg=#%s\n"
		"hi Type guifg=#%s\n"
		"hi Special guifg=#%s\n"
		"hi Underlined guifg=#%s gui=underline\n"
		"hi Error guifg=#%s gui=bold\n"
		"hi Todo guifg=#%s guibg=#%s gui=bold\n"
		"hi LineNr guifg=#%s\n"
		"hi CursorLineNr guifg=#%s\n"
		"hi CursorLine guibg=#%s\n"
		"hi Visual guibg=#%s\n"
		"hi StatusLine guifg=#%s guibg=#%s\n"
		"hi StatusLineNC guifg=#%s guibg=#%s\n"
		"hi Pmenu guifg=#%s guibg=#%s\n"
		"hi PmenuSel guifg=#%s guibg=#%s\n"
		"hi Search guifg=#%s guibg=#%s\n"
		"hi MatchParen guifg=#%s guibg=#%s\n"
		"hi VertSplit guifg=#%s\n"
		"hi NonText guifg=#%s\n",
		text, deep, mut, sec, p, p, p, urg, urg, var, sec,
		p, urg, deep, sec, mut, sec, dim, dim,
		deep, p, text, dim, text, dim, deep, p, deep, sec,
		deep, sec, mut, mut);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* delta: git's own `[include] path`, which is delta's documented mechanism.
 * Every hex value is QUOTED — unquoted, git takes the `#` as a comment and the
 * style silently loses its colour. */
static void write_delta(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("git/kdos-delta");
	kdt_mkparent(f);

	char p[8], sec[8], urg[8], mut[8], minus[8], plus[8];
	kcol_format(sc->primary, p);
	kcol_format(sc->secondary, sec);
	kcol_format(sc->urgent, urg);
	kcol_format(kcol_muted(sc), mut);
	/* A removed line is the urgent hue laid over the ground and an added
	 * one the primary: dark enough to read text on, which a full-strength
	 * accent as a background is not. */
	kcol_format(kcol_mix(sc->urgent, sc->deep, 78), minus);
	kcol_format(kcol_mix(sc->primary, sc->deep, 78), plus);

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS delta colours — GENERATED by `kdos theme`; edits will "
		"be overwritten.\n"
		"# Included from the shipped gitconfig. Every hex is quoted: "
		"unquoted, `#` starts a comment.\n"
		"[delta \"kdos\"]\n"
		"\tminus-style = \"normal\" \"#%s\"\n"
		"\tminus-emph-style = \"normal\" \"#%s\"\n"
		"\tplus-style = \"syntax\" \"#%s\"\n"
		"\tplus-emph-style = \"syntax\" \"#%s\"\n"
		"\tline-numbers-minus-style = \"#%s\"\n"
		"\tline-numbers-plus-style = \"#%s\"\n"
		"\tline-numbers-zero-style = \"#%s\"\n"
		"\tfile-style = \"#%s\" bold\n"
		"\tfile-decoration-style = \"#%s\" ul\n"
		"\thunk-header-style = \"#%s\"\n"
		"\thunk-header-decoration-style = \"#%s\"\n"
		"\tcommit-style = \"#%s\" bold\n"
		"\tcommit-decoration-style = \"#%s\"\n"
		"\twhitespace-error-style = \"#%s\" reverse\n",
		minus, urg, plus, p, urg, p, mut, sec, mut, mut, mut, sec,
		mut, urg);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* newsboat: `include <path>` from the shipped config. INDICES, not hex — `#`
 * is newsboat's comment character, so a hex colour truncates the line and the
 * entry is refused outright. kcol_xterm256 is where the scheme is
 * approximated, and it is approximated because there is no other way in.
 * ONLY ELEMENTS NEWSBOAT KNOWS: an unknown one (2.44 has no `important`) is
 * a configuration error, and newsboat then refuses to start at all. */
static void write_newsboat(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("newsboat/kdos-colors");
	kdt_mkparent(f);

	int p = kcol_xterm256(sc->primary);
	int sec = kcol_xterm256(sc->secondary);
	int deep = kcol_xterm256(sc->deep);
	int text = kcol_xterm256(sc->text);
	int mut = kcol_xterm256(kcol_muted(sc));

	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS newsboat colours — GENERATED by `kdos theme`; edits "
		"will be overwritten.\n"
		"# INDICES, NOT HEX: `#` is a comment here, so a hex colour "
		"truncates the line.\n"
		"color background color%d color%d\n"
		"color listnormal color%d color%d\n"
		"color listnormal_unread color%d color%d bold\n"
		"color listfocus color%d color%d bold\n"
		"color listfocus_unread color%d color%d bold\n"
		"color info color%d color%d bold\n"
		"color article color%d color%d\n"
		"color hint-key color%d color%d bold\n"
		"color hint-description color%d color%d\n"
		"color title color%d color%d bold\n"
		"color end-of-text-marker color%d color%d\n",
		text, deep, text, deep, p, deep, deep, p, deep, p,
		deep, sec, text, deep, sec, deep, text, deep,
		deep, sec, mut, deep);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* aerc: a styleset is its own file, named once in the shipped aerc.conf —
 * there is no include, but there does not need to be, because a styleset is
 * already a file of its own rather than a section of somebody's config. */
static void write_aerc(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("aerc/stylesets/kdos");
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
		"# KDOS aerc styleset — GENERATED by `kdos theme`; edits will "
		"be overwritten.\n"
		/* A KEY IS object[.pseudo].attribute AND THE ONLY PSEUDO IS
		 * `selected`. `*.default.fg` names a pseudo called `default`,
		 * which aerc refuses — and it refuses the whole file, so a
		 * single bad key is a mail client that will not start. */
		"*.fg=#%s\n"
		"*.bg=#%s\n"
		"*.selected.fg=#%s\n"
		"*.selected.bg=#%s\n"
		"title.fg=#%s\n"
		"title.bg=#%s\n"
		"header.fg=#%s\n"
		"header.bold=true\n"
		"statusline_default.fg=#%s\n"
		"statusline_default.bg=#%s\n"
		"statusline_error.fg=#%s\n"
		"statusline_success.fg=#%s\n"
		"msglist_unread.fg=#%s\n"
		"msglist_unread.bold=true\n"
		"msglist_read.fg=#%s\n"
		"msglist_deleted.fg=#%s\n"
		"msglist_marked.fg=#%s\n"
		"dirlist_default.fg=#%s\n"
		"dirlist_unread.fg=#%s\n"
		"border.fg=#%s\n"
		"completion_default.fg=#%s\n",
		text, deep, deep, p, deep, p, sec,
		deep, p, urg, p, p, text, mut, sec, text, p, mut, text);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

/* fzf: no include and no rc file of its own — the flags go in an environment
 * variable, so the generated file is a shell fragment `profile.d` sources. NO
 * SPACE may appear before a colour: fzf takes the `#` as a comment and drops
 * the rest of the line, with a zero exit and nothing on stderr. */
static void write_fzf(const KcolScheme *sc)
{
	char *f = kdt_cfg_home("kdos/fzf-colors");
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
		"# KDOS fzf colours — GENERATED by `kdos theme`; edits will be "
		"overwritten.\n"
		"# Sourced by /etc/profile.d/30-kdos-colors.sh. No space "
		"before a colour: fzf reads `#` as a comment.\n"
		"KDOS_FZF_COLORS='--color=fg:#%s,bg:#%s,hl:#%s"
		",fg+:#%s,bg+:#%s,hl+:#%s"
		",info:#%s,prompt:#%s,pointer:#%s"
		",marker:#%s,spinner:#%s,header:#%s,border:#%s'\n",
		text, deep, p, text, dim, p, mut, sec, p, urg, sec, mut, mut);
	kb_write_all(f, b.p, b.n);
	kb_buf_free(&b);
	free(f);
}

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
 * installed), the host's GNU `ls` where eza is not, and the appbox's Debian
 * coreutils. Each class gets its OWN colour rather than a bold bit: two
 * classes sharing one colour are told apart only by the intensity attribute,
 * which on a 512-glyph console font is the 9th glyph bit rather than a
 * weight.
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
 * Midnight Commander's skin.
 *
 * mc's own default is blue on cyan, which is no accent's: a console's
 * full-screen file manager drawn in a palette of its own is the one thing on
 * the terminal that does not match the rest of it.
 *
 * TRUECOLOR, in mc's `rgb:rrggbb` spelling. mc's 16-colour names would round
 * the palette to the nearest ANSI slot, which turns four distinguishable
 * accents into two; the `truecolor` capability in [skin] is what makes the
 * literal values take effect, and mc falls back to the 256-colour path by
 * itself on a terminal that cannot do better.
 *
 * The `ini` file is written too, because a skin nothing selects is a skin
 * nobody sees — and it is written with a MERGE, not a clobber: mc keeps
 * genuine user settings in there (panel modes, the editor's options), and a
 * file an application writes back is a file only its own keys may change in.
 */
/*
 * Set one key in one section of an ini file, keeping everything else.
 *
 * An application that writes its own settings back into a file (mc's `ini`,
 * qt5ct's qt5ct.conf) must not have them thrown away by a theme generator. Only the named key in the named section is touched; the
 * section is created at the end if it is absent, and a file that does not exist
 * yet becomes a file with exactly that one section in it.
 */
void kdt_ini_set(const char *path, const char *section, const char *key,
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
	 * in this file, so only the key this owns is replaced.
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

static void write_yazi(const KcolScheme *sc)
{
	/*
	 * PARTIAL, AND THAT IS THE FORMAT. yazi deserializes `theme.toml`
	 * OVER its own `theme-dark.toml` preset, key by key, so a file naming
	 * only what this palette decides leaves the rest of the preset intact
	 * — the separators and the file-type rules. A full copy would be a
	 * copy that goes stale on the next yazi release.
	 *
	 * THE ICONS ARE THE ONE PART THAT IS EMPTIED RATHER THAN LEFT. The
	 * preset's `[icon]` is nine hundred Nerd Font codepoints, which are
	 * private-use and are exactly what the 512-glyph console font cannot
	 * carry: on `tty1` every one is a blank cell, so a name arrives with a
	 * hole punched in front of it. Five empty arrays replace five full
	 * ones, because deserializing OVER means an empty array is an answer
	 * and an absent key is not.
	 *
	 * `#rrggbb`, which is what a flavour writes; a colour NAME here would
	 * be one of the terminal's sixteen and not this scheme's.
	 *
	 * `[flavor]` IS NOT WRITTEN. It is the one part the preset splits by
	 * dark and light mode, and a `theme.toml` that set it would override
	 * that split for both.
	 */
	char p[10], dim[10], sec[10], urg[10], deep[10], text[10];
	char cyan[10], mut[10];
	char raw[8];
	AnsiDerived a;

	/*
	 * THE FOURTH COLOUR IS THE DERIVED CYAN AND NOT `variant`. A scheme's
	 * variant is a near-black — a second ground, not a foreground — so a
	 * marker bar drawn in it is invisible and a permission letter written
	 * in it is unreadable. The ANSI derivation is where a scheme's fourth
	 * READABLE hue comes from, which is what btop's gauges already use.
	 */
	ansi_all(sc, &a);

#define HX(field, out) do {                       \
		kcol_format((field), raw);        \
		snprintf((out), sizeof(out), "#%s", raw); \
	} while (0)
	HX(sc->primary, p);
	HX(sc->dim, dim);
	HX(sc->secondary, sec);
	HX(sc->urgent, urg);
	HX(sc->deep, deep);
	HX(sc->text, text);
	HX(a.cyan, cyan);
	HX(kcol_muted(sc), mut);
#undef HX

	char *f = kdt_cfg_home("yazi/theme.toml");
	kdt_mkparent(f);

	/* A BORDER IS A BORDER AND A LABEL IS A LABEL: frames take `dim`,
	 * which is a fill, and anything with words in it takes `mut` or
	 * `text`, which are read. The hovered row is reversed by the preset
	 * and stays that way — a colour of its own would be a second answer to
	 * "which row am I on". */
	KbBuf b = {0};
	kb_buf_printf(&b,
		"# KDOS theme for yazi — GENERATED by `kdos theme`;\n"
		"# edits will be overwritten. It is deserialized OVER yazi's\n"
		"# own preset, so only what this palette decides is here.\n"
		"\n"
		"[mgr]\n"
		"cwd = { fg = \"%s\" }\n"
		"find_keyword = { fg = \"%s\", bold = true }\n"
		"find_position = { fg = \"%s\", bold = true }\n"
		"marker_copied = { fg = \"%s\", bg = \"%s\" }\n"
		"marker_cut = { fg = \"%s\", bg = \"%s\" }\n"
		"marker_marked = { fg = \"%s\", bg = \"%s\" }\n"
		"marker_selected = { fg = \"%s\", bg = \"%s\" }\n"
		"count_copied = { fg = \"%s\", bg = \"%s\" }\n"
		"count_cut = { fg = \"%s\", bg = \"%s\" }\n"
		"count_selected = { fg = \"%s\", bg = \"%s\" }\n"
		"border_style = { fg = \"%s\" }\n"
		"\n"
		"[tabs]\n"
		"active = { fg = \"%s\", bg = \"%s\", bold = true }\n"
		"inactive = { fg = \"%s\", bg = \"%s\" }\n"
		"\n"
		"[mode]\n"
		"normal_main = { fg = \"%s\", bg = \"%s\", bold = true }\n"
		"normal_alt = { fg = \"%s\", bg = \"%s\" }\n"
		"select_main = { fg = \"%s\", bg = \"%s\", bold = true }\n"
		"select_alt = { fg = \"%s\", bg = \"%s\" }\n"
		"unset_main = { fg = \"%s\", bg = \"%s\", bold = true }\n"
		"unset_alt = { fg = \"%s\", bg = \"%s\" }\n"
		"\n"
		"[status]\n"
		"perm_sep = { fg = \"%s\" }\n"
		"perm_type = { fg = \"%s\" }\n"
		"perm_read = { fg = \"%s\" }\n"
		"perm_write = { fg = \"%s\" }\n"
		"perm_exec = { fg = \"%s\" }\n"
		"progress_label = { fg = \"%s\", bold = true }\n"
		"progress_normal = { fg = \"%s\", bg = \"%s\" }\n"
		"progress_error = { fg = \"%s\", bg = \"%s\" }\n"
		"\n"
		"[which]\n"
		"border = { fg = \"%s\" }\n"
		"cand = { fg = \"%s\" }\n"
		"rest = { fg = \"%s\" }\n"
		"desc = { fg = \"%s\" }\n"
		"separator_style = { fg = \"%s\" }\n"
		"\n"
		"[confirm]\n"
		"border = { fg = \"%s\" }\n"
		"title = { fg = \"%s\" }\n"
		"\n"
		"[spot]\n"
		"border = { fg = \"%s\" }\n"
		"title = { fg = \"%s\" }\n"
		"tbl_col = { fg = \"%s\" }\n"
		"\n"
		"[notify]\n"
		"title_info = { fg = \"%s\" }\n"
		"title_warn = { fg = \"%s\" }\n"
		"title_error = { fg = \"%s\" }\n"
		"\n"
		"[pick]\n"
		"border = { fg = \"%s\" }\n"
		"active = { fg = \"%s\", bold = true }\n"
		"\n"
		"[input]\n"
		"border = { fg = \"%s\" }\n"
		"\n"
		"# Nerd Font codepoints the console font cannot carry: five\n"
		"# empty arrays for five full ones. A person who wants them\n"
		"# installs a Nerd Font and puts the preset's [icon] back.\n"
		"[icon]\n"
		"globs = []\n"
		"dirs = []\n"
		"files = []\n"
		"exts = []\n"
		"conds = []\n",
		p, sec, sec,
		/* FOUR MARKERS AND FOUR COLOURS. The bars say which of copy,
		 * cut, mark and select a row is in, so two of them sharing a
		 * colour is a bar that answers nothing; the count badge on the
		 * status line takes the same four, or the two halves of one
		 * state disagree. */
		sec, sec, urg, urg, p, p, cyan, cyan,
		deep, sec, deep, urg, deep, cyan,
		dim,
		deep, p, p, dim,
		deep, p, p, dim,
		deep, urg, urg, dim,
		deep, urg, urg, dim,
		mut, p, sec, urg, cyan,
		text, p, deep, urg, deep,
		p, sec, mut, cyan, mut,
		p, p,
		p, p, p,
		p, sec, urg,
		p, sec,
		p);
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

/* ── qt5ct, for the Qt 5 programs ──────────────────────────────────────── */

/*
 * THE QT 5 PROGRAMS (VLC, KeePassXC, LMMS…) LOAD qt5ct, under the platform
 * theme name `kde` that Qt 6 programs answer with plasma-integration; there is
 * no KF5, so qt5ct is their only way to the accent (A11). The palettes are
 * installed, one per accent, by kdos-plasma-look; this points the person's
 * qt5ct.conf at the accent's, and only while it points at a KDOS palette or
 * at none — a palette chosen in qt5ct's own window is the person's.
 *
 * QT5CT COPIES ITS FIRST FILE ITSELF, from the first qt5ct/qt5ct.conf along
 * XDG_CONFIG_DIRS, when a program starts and finds none in the home. A file
 * created here with four keys in it would stop that copy for good, so a home
 * with none gets the same copy first and the keys are merged into it.
 */
static void write_qt5ct(const KcolScheme *sc)
{
	char *conf = kdt_cfg_home("qt5ct/qt5ct.conf"), *cur;
	char path[256];

	if (!kb_path_exists(conf)) {
		const char *dirs = getenv("XDG_CONFIG_DIRS");
		char *copy = kb_strdup(dirs && *dirs ? dirs : "/etc/xdg");
		char *save = NULL;

		kdt_mkparent(conf);
		for (char *d = strtok_r(copy, ":", &save); d;
		     d = strtok_r(NULL, ":", &save)) {
			char *src = kb_path_join(d, "qt5ct/qt5ct.conf");
			int done = *d && kb_copy_file(src, conf) == 0;

			free(src);
			if (done)
				break;
		}
		free(copy);
	}

	cur = kdt_kcfg_get(conf, "Appearance", "color_scheme_path");
	if (cur && *cur) {
		const char *leaf = strrchr(cur, '/');
		size_t n;

		leaf = leaf ? leaf + 1 : cur;
		n = strlen(leaf);
		if (n < 10 || strncmp(leaf, "KDOS-", 5) ||
		    strcmp(leaf + n - 5, ".conf")) {
			free(cur);
			free(conf);
			return;
		}
	}
	free(cur);

	kdt_mkparent(conf);
	snprintf(path, sizeof(path), "/usr/share/qt5ct/colors/%s.conf",
		 sc->theme_name);
	kdt_ini_set(conf, "Appearance", "color_scheme_path", path);
	kdt_ini_set(conf, "Appearance", "custom_palette", "true");
	kdt_ini_set(conf, "Appearance", "style", "Fusion");
	kdt_ini_set(conf, "Appearance", "icon_theme",
		    kdt_accent_light(sc) ? "KDOS-Light" : "KDOS");
	/* QUOTED: QSettings reads an unquoted value with a comma in it as a
	 * string list, and a list is no font at all. */
	kdt_ini_set(conf, "Fonts", "general", "\"Noto Sans,10\"");
	kdt_ini_set(conf, "Fonts", "fixed", "\"JetBrains Mono,10\"");
	free(conf);
}

/* Everything this command generates into the home, and nothing else — no
 * accent file, no Plasma call. `kdos theme --audit` runs exactly this into a
 * scratch $HOME and compares, which only works because it has no side effect
 * beyond the files. */
static void theme_apply_cli(const KcolScheme *sc)
{
	write_bat(sc);
	write_micro(sc);
	write_helix(sc);
	write_nvim(sc);
	write_delta(sc);
	write_newsboat(sc);
	write_aerc(sc);
	write_fzf(sc);
	write_tmux(sc);
	write_btop(sc);
	write_mc(sc);
	write_yazi(sc);
	write_starship(sc);
	write_lscolors(sc);
}

/* ── Konsole ───────────────────────────────────────────────────────────── */

/*
 * THE OPEN TERMINALS, not only the next one. A Konsole window registers
 * `org.kde.konsole-<pid>` and one object per tab under /Sessions; a tab whose
 * profile NAME starts with `prefix` is moved to `name`. A tab on a profile the
 * person made keeps it. Returns how many tabs moved.
 */
int kdt_konsole_retarget(KdtBus *b, const char *prefix, const char *name)
{
	KdtMsg *r = NULL;
	char **svc = NULL;
	int nsvc = 0, cap = 0, moved = 0;
	const char *s;

	if (!b || kdt_bus_call(b, "org.freedesktop.DBus",
			       "/org/freedesktop/DBus",
			       "org.freedesktop.DBus", "ListNames", &r, NULL,
			       0, "") != 0)
		return 0;
	if (kdt_bus_enter(b, r, 'a', "s") > 0) {
		while (kdt_bus_read(b, r, "s", &s) > 0) {
			if (strncmp(s, "org.kde.konsole-", 16))
				continue;
			if (nsvc == cap) {
				cap = cap ? cap * 2 : 8;
				svc = kb_realloc(svc, (size_t)cap * sizeof(*svc));
			}
			svc[nsvc++] = kb_strdup(s);
		}
		kdt_bus_leave(b, r);
	}
	kdt_bus_free(b, r);

	for (int i = 0; i < nsvc; i++) {
		char *xml = kdt_bus_call_s(b, svc[i], "/Sessions",
					   "org.freedesktop.DBus.Introspectable",
					   "Introspect", NULL, 0, "");

		for (char *p = xml; p && (p = strstr(p, "<node name=\""));) {
			char path[64], *cur;
			int id;

			p += 12;
			if (sscanf(p, "%d\"", &id) != 1 || id < 0)
				continue;
			snprintf(path, sizeof(path), "/Sessions/%d", id);
			cur = kdt_bus_call_s(b, svc[i], path,
					     "org.kde.konsole.Session",
					     "profile", NULL, 0, "");
			if (cur && !strncmp(cur, prefix, strlen(prefix)) &&
			    strcmp(cur, name) &&
			    kdt_bus_call(b, svc[i], path,
					 "org.kde.konsole.Session",
					 "setProfile", NULL, NULL, 0, "s",
					 name) == 0)
				moved++;
			free(cur);
		}
		free(xml);
		free(svc[i]);
	}
	free(svc);
	return moved;
}

/*
 * STEP 4: the profile a new tab opens in, and the open tabs. Written into the
 * person's own konsolerc only — /etc/xdg/konsolerc is the machine's default
 * and stays it — and only while the value in force names a KDOS profile.
 * DefaultProfile is a FILE name and the light/dark pair are profile NAMES,
 * which is Konsole's own split; LightThemeProfile stays on Paper, the one
 * light accent, so the Quick Settings flip still lands somewhere light.
 * QMLKonsole's colour scheme, in the person's qmlkonsolerc, follows by the
 * same rule.
 *
 * WITH PHOSPHOR MODE'S TERMINUS FACE ON, the KDOS profile in force is
 * KDOS-Terminus, regenerated from the new accent's own profile, so the face
 * survives the switch and the colours follow it.
 */
static int theme_konsole(KdtBus *b, const KcolScheme *sc, int in_session)
{
	char file[96], pname[96];
	char *cur;
	int terminus = kdt_phosphor_terminus_active();

	if (terminus && kdt_phosphor_terminus_profile(sc) == 0) {
		snprintf(file, sizeof(file), "KDOS-Terminus.profile");
		snprintf(pname, sizeof(pname), "KDOS Terminus");
	} else {
		terminus = 0;
		snprintf(file, sizeof(file), "%s.profile", sc->theme_name);
		snprintf(pname, sizeof(pname), "KDOS %s",
			 kdt_accent_title(sc));
	}

	cur = kdt_kcfg_cascade("konsolerc", "Desktop Entry", "DefaultProfile");
	if ((!cur || !*cur || !strncmp(cur, "KDOS-", 5)) &&
	    (!cur || strcmp(cur, file)) &&
	    kdt_kcfg_set("konsolerc", "Desktop Entry", "DefaultProfile", file,
			 0) != 0) {
		free(cur);
		return -1;
	}
	free(cur);

	/* QMLKonsole names a colour SCHEME, not a profile, by its file name in
	 * /usr/share/konsole; the Terminus face is Konsole's alone, so the
	 * scheme is always the accent's own. Moved only while the value in
	 * force names a KDOS scheme. */
	cur = kdt_kcfg_cascade("qmlkonsolerc", "General", "colorScheme");
	if ((!cur || !*cur || !strncmp(cur, "KDOS-", 5)) &&
	    (!cur || strcmp(cur, sc->theme_name)) &&
	    kdt_kcfg_set("qmlkonsolerc", "General", "colorScheme",
			 sc->theme_name, 0) != 0) {
		free(cur);
		return -1;
	}
	free(cur);

	/* A light accent is the light half of the pair, and Paper already is;
	 * the dark half follows every dark accent. With the Terminus face on,
	 * a light accent's tabs are the light half, so that one moves. */
	{
		const char *key = !kdt_accent_light(sc) ? "DarkThemeProfile"
				  : terminus		? "LightThemeProfile"
							: NULL;

		if (key) {
			cur = kdt_kcfg_cascade("konsolerc", "LightDarkTheme",
					       key);
			if ((!cur || !*cur || !strncmp(cur, "KDOS ", 5)) &&
			    (!cur || strcmp(cur, pname)) &&
			    kdt_kcfg_set("konsolerc", "LightDarkTheme", key,
					 pname, 0) != 0) {
				free(cur);
				return -1;
			}
			free(cur);
		}
	}
	if (in_session)
		kdt_konsole_retarget(b, "KDOS ", pname);
	return 0;
}

/* Step 4 alone, for the accent in force: the Konsole keys and tabs that name
 * a KDOS profile are moved to that accent's, and keys already on it are not
 * written at all, so a file that already agrees keeps its bytes. */
int kdt_theme_konsole_follow(KdtBus *b)
{
	const KcolScheme *sc = kcol_find(kdt_current_accent());

	return theme_konsole(b, sc ? sc : kcol_default(), b != NULL);
}

/* ── Plasma ────────────────────────────────────────────────────────────── */

/*
 * STEP 3, THE WALLPAPER, which a global theme does not change on a running
 * desktop: the image plugin reads its default once, so a switch with no
 * explicit step leaves the old accent's picture until plasmashell restarts.
 * Only a desktop showing a KDOS wallpaper, or the theme's default (an empty
 * Image), is moved.
 */
static int theme_wallpaper(KdtBus *b, const KcolScheme *sc, char *err,
			   size_t errn)
{
	KbBuf js = { 0 };
	char want[128];
	char *cur, *out;

	snprintf(want, sizeof(want), "file:///usr/share/wallpapers/%s/",
		 sc->theme_name);
	kb_buf_printf(&js,
		"const want='%s'; for (const d of desktops()) { "
		"if (d.wallpaperPlugin !== 'org.kde.image') continue; "
		"d.currentConfigGroup = ['Wallpaper','org.kde.image','General']; "
		"const img = d.readConfig('Image', ''); "
		"if (img === '' || "
		"img.startsWith('file:///usr/share/wallpapers/KDOS-') || "
		"img.startsWith('/usr/share/wallpapers/KDOS-')) "
		"d.writeConfig('Image', want); }", want);
	out = kdt_bus_call_s(b, "org.kde.plasmashell", "/PlasmaShell",
			     "org.kde.PlasmaShell", "evaluateScript", err,
			     errn, "s", js.p);
	kb_buf_free(&js);
	if (!out) {
		if (!*err)
			snprintf(err, errn, "plasmashell did not answer");
		return -1;
	}
	free(out);

	cur = kdt_kcfg_cascade("kscreenlockerrc",
			       "Greeter][Wallpaper][org.kde.image][General",
			       "Image");
	if (kdos_wallpaper(cur) && (!cur || strcmp(cur, want)) &&
	    kdt_kcfg_set("kscreenlockerrc",
			 "Greeter][Wallpaper][org.kde.image][General", "Image",
			 want, 1) != 0) {
		free(cur);
		snprintf(err, errn, "kwriteconfig6 could not write "
				    "kscreenlockerrc");
		return -1;
	}
	free(cur);
	return 0;
}

/* STEP 2's pairing half: the Quick Settings light/dark flip pairs a dark
 * accent with Paper, but only while the dark half is still a KDOS theme. */
static int theme_dark_pair(const KcolScheme *sc, const char *lnf)
{
	char *cur;
	int rc = 0;

	if (kdt_accent_light(sc))
		return 0;
	cur = kdt_kcfg_cascade("kdeglobals", "KDE", "DefaultDarkLookAndFeel");
	if ((!cur || !*cur || !strncmp(cur, "org.kdos.", 9)) &&
	    (!cur || strcmp(cur, lnf)))
		rc = kdt_kcfg_set("kdeglobals", "KDE", "DefaultDarkLookAndFeel",
				  lnf, 0);
	free(cur);
	return rc;
}

/*
 * STEP 8, THE MACHINE'S HALF: the boot menu, the splash, the console palette
 * and the login screen, all in files this command cannot write. kdos-powerd
 * validates the name against the palette's own closed list and writes them.
 *
 * NOT REACHING IT IS NOT A FAILED THEME CHANGE. The live medium has no
 * writable boot menu and a user outside wheel is refused; the session HAS been
 * themed by now. The line says what was left, and it is the last line,
 * because kded-kdos shows the last line it is given.
 */
static void theme_boot(const KcolScheme *sc, char *line, size_t n)
{
	KbArgv a = { 0 };
	char err[512] = "";
	int rc;

	line[0] = '\0';
	kb_argv_add(&a, "kdos-power");
	kb_argv_add(&a, "accent");
	kb_argv_add(&a, sc->name);
	kb_argv_end(&a);
	rc = kb_run_feed_env(&a, NULL, 0, "", 0, err, sizeof(err));
	if (rc == 0)
		return;
	err[strcspn(err, "\n")] = '\0';
	{
		/* kdos-power prefixes its own name; the sentence is the
		 * reason. */
		const char *why = err;

		if (!strncmp(why, "kdos-power: ", 12))
			why += 12;
		if (*why && rc != 127)
			snprintf(line, n, "boot menu, splash and login screen "
					  "unchanged (%.200s)", why);
		else
			snprintf(line, n, "boot menu, splash and login screen "
					  "unchanged (kdos-powerd did not "
					  "answer)");
	}
}

/* tmux reads its configuration once; a running server is told to re-read
 * it. No server running is not an error. */
static void theme_tmux(void)
{
	char *conf;
	KbArgv a = { 0 };

	if (!kb_have_prog("tmux"))
		return;
	conf = kdt_cfg_home("tmux/tmux.conf");
	kb_argv_add(&a, "tmux");
	kb_argv_add(&a, "source-file");
	kb_argv_add(&a, conf);
	kb_argv_end(&a);
	kb_run(&a);
	free(conf);
}

/*
 * THE SWITCH, steps 1 to 8 of the plan in that order. Returns 0 when the
 * session wears the accent (the machine's half may still have been refused:
 * that is said, not failed), 1 when a session step failed — with the reason
 * as the last line printed, because kded-kdos shows the last line it reads.
 */
int kdt_theme_set(const KcolScheme *sc, int flags)
{
	int from_plasma = flags & KDT_THEME_FROM_PLASMA;
	const char *full = getenv("KDE_FULL_SESSION");
	int in_session = from_plasma || (full && !strcmp(full, "true"));
	char lnf[96], fail[512] = "", boot[512], err[512];
	char *accent = kdt_cfg_home("kdos/accent");
	KdtBus *b = NULL;

	colours();
	snprintf(lnf, sizeof(lnf), "org.kdos.%s.desktop", sc->name);

	/* 1. The accent, FIRST and atomically: kded-kdos compares the Global
	 * Theme page against exactly this file, and a half-written word would
	 * read as a foreign scheme. */
	{
		char line[40];

		kdt_mkparent(accent);
		snprintf(line, sizeof(line), "%s\n", sc->name);
		if (kb_write_file_atomic(accent, line) != 0) {
			printf("cannot write %s: %s\n", accent,
			       strerror(errno));
			free(accent);
			return 1;
		}
	}
	free(accent);

	if (in_session) {
		/* 2. The global theme: colours, Plasma style, icons, cursor,
		 * decoration, splash and fonts in one call. `-k` keeps the
		 * automatic light/dark switch; no --resetLayout, so the
		 * panels are not touched. */
		if (!from_plasma) {
			KbArgv a = { 0 };

			kb_argv_add(&a, "plasma-apply-lookandfeel");
			kb_argv_add(&a, "-a");
			kb_argv_add(&a, lnf);
			kb_argv_add(&a, "-k");
			kb_argv_end(&a);
			if (kb_run(&a) != 0 && !*fail)
				snprintf(fail, sizeof(fail),
					 "plasma-apply-lookandfeel could not "
					 "apply %s", lnf);
		}
		if (theme_dark_pair(sc, lnf) != 0 && !*fail)
			snprintf(fail, sizeof(fail), "kwriteconfig6 could not "
						     "write kdeglobals");

		b = kdt_bus_open(0);
		/* 3. The wallpaper and the lock screen's. */
		err[0] = '\0';
		if (!b) {
			if (!*fail)
				snprintf(fail, sizeof(fail),
					 "the session bus could not be "
					 "reached");
		} else if (theme_wallpaper(b, sc, err, sizeof(err)) != 0 &&
			   !*fail) {
			snprintf(fail, sizeof(fail), "wallpaper unchanged: %.400s",
				 err);
		}
	} else {
		/* Outside a session the next login applies it: startplasma
		 * reads LookAndFeelPackage and applies it in its defaults
		 * mode. */
		if (kdt_kcfg_set("kdeglobals", "KDE", "LookAndFeelPackage", lnf,
				 0) != 0 && !*fail)
			snprintf(fail, sizeof(fail), "kwriteconfig6 could not "
						     "write kdeglobals");
		if (theme_dark_pair(sc, lnf) != 0 && !*fail)
			snprintf(fail, sizeof(fail), "kwriteconfig6 could not "
						     "write kdeglobals");
	}

	/* 4. Konsole. */
	if (theme_konsole(b, sc, in_session && b) != 0 && !*fail)
		snprintf(fail, sizeof(fail), "kwriteconfig6 could not write "
					     "konsolerc or qmlkonsolerc");

	/* 5. The terminal programs, and a running tmux. */
	theme_apply_cli(sc);
	theme_tmux();

	/* 6. qt5ct. */
	write_qt5ct(sc);

	/* 7. Phosphor mode's tint follows the accent: one degauss pulse. */
	if (b && kdt_bus_call_b(b, "org.kde.KWin", "/Effects",
				"org.kde.kwin.Effects", "isEffectLoaded", NULL,
				0, "s", "kdos_crt") == 1)
		kdt_bus_call(b, "org.kde.KWin", "/Effects",
			     "org.kde.kwin.Effects", "reconfigureEffect", NULL,
			     NULL, 0, "s", "kdos_crt");
	kdt_bus_close(b);

	printf("%s%s%s (%s)%s\n", C_A, sc->name, C_0, sc->theme_name,
	       in_session ? "" : " — Plasma takes it at the next login");

	/* 8. The machine's half. */
	theme_boot(sc, boot, sizeof(boot));
	if (*boot)
		printf("%s%s%s\n", C_D, boot, C_0);
	if (*fail) {
		printf("%s\n", fail);
		fflush(stdout);
		fprintf(stderr, "kdos theme: %s\n", fail);
		return 1;
	}
	return 0;
}

/* ── `kdos theme style` ────────────────────────────────────────────────── */

/*
 * A SHAREABLE LOOK: the accent and Phosphor mode's tuning, in one flat
 * `key = value` file.
 *
 *   accent = amber          the switch above
 *   crt = 55                kwinrc [Effect-kdos_crt] Strength
 *   crt_scanlines = 60      Scanlines
 *   crt_curve = 30          Curve
 *   crt_fullscreen = true   Fullscreen
 *
 * The tuning is written whether Phosphor mode is on or not, and the effect
 * re-reads it at once when it is loaded. Any other key is reported and
 * ignored: a line in a shared look that silently did nothing would be a look
 * that differs on the second machine with nobody told.
 */
static const struct {
	const char *key, *kwin;
	int numeric;
} STYLE_KEYS[] = {
	{ "crt", "Strength", 1 },
	{ "crt_scanlines", "Scanlines", 1 },
	{ "crt_curve", "Curve", 1 },
	{ "crt_fullscreen", "Fullscreen", 0 },
};
#define NSTYLE ((int)(sizeof(STYLE_KEYS) / sizeof(STYLE_KEYS[0])))

static int cmd_theme_style(const char *path)
{
	char *data = kb_read_all(path, NULL);
	char accent[32] = "";
	char *val[NSTYLE] = { 0 };
	int rc = 0, any = 0;

	if (!data) {
		fprintf(stderr, "kdos theme: cannot read style '%s'\n", path);
		return 1;
	}
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n'), *k, *eq, *v, *e;
		int hit = -1;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		k = line;
		while (*k == ' ' || *k == '\t')
			k++;
		if (!*k || *k == '#')
			continue;
		if (!(eq = strchr(k, '='))) {
			kb_warn("style: '%s' is not key = value — ignored", k);
			continue;
		}
		v = eq + 1;
		while (eq > k && (eq[-1] == ' ' || eq[-1] == '\t'))
			eq--;
		*eq = '\0';
		while (*v == ' ' || *v == '\t')
			v++;
		e = v + strlen(v);
		while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
			*--e = '\0';
		if (!strcmp(k, "accent")) {
			kb_strlcpy(accent, v, sizeof(accent));
			continue;
		}
		for (int i = 0; i < NSTYLE; i++)
			if (!strcmp(k, STYLE_KEYS[i].key))
				hit = i;
		if (hit < 0) {
			kb_warn("style: unknown key '%s' — ignored", k);
			continue;
		}
		if (STYLE_KEYS[hit].numeric) {
			char *end;
			long n = strtol(v, &end, 10);

			if (end == v || *end || n < 0 || n > 100) {
				kb_warn("style: %s takes 0 to 100 — ignored", k);
				continue;
			}
		} else if (strcmp(v, "true") && strcmp(v, "false")) {
			kb_warn("style: %s takes true or false — ignored", k);
			continue;
		}
		free(val[hit]);
		val[hit] = kb_strdup(v);
		any = 1;
	}
	free(data);

	const KcolScheme *sc = kcol_find(accent[0] ? accent
						   : kdt_current_accent());
	if (!sc) {
		fprintf(stderr, "kdos theme: style names unknown accent '%s'\n",
			accent);
		for (int i = 0; i < NSTYLE; i++)
			free(val[i]);
		return 1;
	}
	for (int i = 0; i < NSTYLE; i++) {
		if (val[i] && kdt_kcfg_set("kwinrc", "Effect-kdos_crt",
					   STYLE_KEYS[i].kwin, val[i], 0) != 0)
			rc = 1;
		free(val[i]);
	}
	if (any) {
		KdtBus *b = kdt_bus_open(0);

		if (b && kdt_bus_call_b(b, "org.kde.KWin", "/Effects",
					"org.kde.kwin.Effects",
					"isEffectLoaded", NULL, 0, "s",
					"kdos_crt") == 1)
			kdt_bus_call(b, "org.kde.KWin", "/Effects",
				     "org.kde.kwin.Effects",
				     "reconfigureEffect", NULL, NULL, 0, "s",
				     "kdos_crt");
		kdt_bus_close(b);
	}
	if (rc) {
		fprintf(stderr, "kdos theme: kwriteconfig6 could not write "
				"kwinrc\n");
		return 1;
	}
	return kdt_theme_set(sc, 0);
}

/* ── the verb ──────────────────────────────────────────────────────────── */

static int theme_usage(void)
{
	fprintf(stderr,
		"usage: kdos theme [list | next | prev | <accent>]\n"
		"       kdos theme --from-plasma <accent>\n"
		"       kdos theme --audit [accent]\n"
		"       kdos theme style <file>\n");
	return 2;
}

int kdt_theme_main(int argc, char **argv)
{
	const char *cur = kdt_current_accent();
	const char *want = argc > 0 ? argv[0] : "";
	const KcolScheme *sc;

	colours();
	if (!strcmp(want, "-h") || !strcmp(want, "--help")) {
		theme_usage();
		return 0;
	}
	if (!strcmp(want, "style")) {
		if (argc != 2)
			return theme_usage();
		return cmd_theme_style(argv[1]);
	}
	if (!strcmp(want, "--audit") || !strcmp(want, "audit")) {
		const char *which = argc > 1 ? argv[1] : cur;

		if (argc > 2)
			return theme_usage();
		sc = kcol_find(which);
		if (!sc) {
			fprintf(stderr, "kdos theme: unknown accent '%s' "
					"(try: kdos theme list)\n", which);
			return 1;
		}
		return kdt_theme_audit(sc, theme_apply_cli, C_A, C_0);
	}
	if (!strcmp(want, "--from-plasma")) {
		if (argc != 2)
			return theme_usage();
		sc = kcol_find(argv[1]);
		if (!sc) {
			fprintf(stderr, "kdos theme: unknown accent '%s' "
				"(try: kdos theme list)\n", argv[1]);
			return 1;
		}
		return kdt_theme_set(sc, KDT_THEME_FROM_PLASMA);
	}
	if (argc > 1 || (*want == '-' && strcmp(want, "-")))
		return theme_usage();
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
	if (!strcmp(want, "next") || !strcmp(want, "prev")) {
		int at = 0, step;

		for (int i = 0; i < kcol_nscheme; i++)
			if (!strcmp(kcol_schemes[i].name, cur))
				at = i;
		step = !strcmp(want, "next") ? 1 : kcol_nscheme - 1;
		want = kcol_schemes[(at + step) % kcol_nscheme].name;
	}
	sc = kcol_find(want);
	if (!sc) {
		fprintf(stderr, "kdos theme: unknown accent '%s' (try: kdos "
				"theme list)\n", want);
		return 1;
	}
	return kdt_theme_set(sc, 0);
}
