/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-keys — the keybind card, Super+F1
 *
 *   ╔═ keys ══════════════════════════════════════════╗
 *   ║ launch                                          ║
 *   ║   Super+d              kdos-launcher            ║
 *   ║   Super+Enter          foot                     ║
 *   ║ window                                          ║
 *   ║   Super+q              close the window         ║
 *   ╚═════════════════════════════════════════════════╝
 *
 * HELP THAT CANNOT LIE. The card is GENERATED from the rc.xml the compositor
 * actually loaded, so a rebound key changes the card in the same edit — a
 * hand-written cheat sheet is a document that starts rotting the day it is
 * written, and a keyboard-driven desktop lives or dies on whether anyone ever
 * finds Super+d.
 *
 * The parse is a line-oriented scanner in this file rather than an XML
 * library, because the labwc subset this reads is tag-regular: a `<keybind
 * key="...">` followed by the `<action name="..."/>` it runs. Comments are
 * stripped FIRST — rc.xml ships a commented-out run-or-raise example with a
 * real `<keybind>` inside it, and a card that advertised a binding the
 * compositor never loaded would be the exact failure this file exists to
 * prevent.
 *
 * When the parse yields nothing it SAYS SO and shows a built-in table: a help
 * surface that silently comes up empty is worse than one that admits it could
 * not read its own configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define KEYS_COLS 72
#define KEYS_ROWS 24
#define KEYS_MAX  128

enum { SEC_LAUNCH = 0, SEC_WINDOW, SEC_WS, SEC_MEDIA, SEC_SYSTEM, SEC_N };

static const char *const sect_name[SEC_N] = {
	"launch", "window", "workspace", "media", "system"
};

struct kbind {
	char key[40];		/* as a person reads it: Super+Shift+d     */
	char desc[80];		/* what it does                            */
	int sect;
};

static struct kbind binds[KEYS_MAX];
static int nbinds;

/* The display list: section headers and entries in one flat array, because
 * scrolling a grouped list is only simple when the groups are already rows. */
struct drow {
	const char *left;
	const char *right;
	int hdr;
};

static struct drow rows[KEYS_MAX + SEC_N];
static int nrows;
static int keyw = 16;		/* the key column, sized to what was parsed */

/* Non-empty when the card is showing built-in defaults instead of the real
 * configuration — drawn where the user cannot miss it. */
static char parse_note[80];

/* ── the file ──────────────────────────────────────────────────────────── */

static char *read_all(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long n;

	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);
	/* A rc.xml is kilobytes; a megabyte of it is not a configuration file
	 * and reading it would be answering a question nobody asked. */
	if (n > 1 << 20) {
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)n + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	*len = fread(buf, 1, (size_t)n, f);
	buf[*len] = '\0';
	fclose(f);
	return buf;
}

/* Comments out, in place and before anything else looks at the text. rc.xml
 * documents itself with commented-out bindings; every one of them would
 * otherwise be advertised as live. */
static void strip_comments(char *s)
{
	char *p = s;

	while ((p = strstr(p, "<!--"))) {
		char *e = strstr(p + 4, "-->");
		size_t n = e ? (size_t)(e + 3 - p) : strlen(p);
		memset(p, ' ', n);
		p += n;
	}
}

/* &amp; and friends, in place. Attribute values are commands and messages, and
 * an `&amp;&amp;` shown verbatim is a card that misquotes the machine. */
static void unescape(char *s)
{
	static const struct { const char *ent; char ch; } tbl[] = {
		{ "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' },
		{ "&quot;", '"' }, { "&apos;", '\'' },
	};
	char *r = s, *w = s;

	while (*r) {
		size_t i;
		for (i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
			size_t n = strlen(tbl[i].ent);
			if (!strncmp(r, tbl[i].ent, n)) {
				*w++ = tbl[i].ch;
				r += n;
				break;
			}
		}
		if (i == sizeof(tbl) / sizeof(tbl[0]))
			*w++ = *r++;
	}
	*w = '\0';
}

/* One attribute out of one tag. `beg` points at the '<', `end` past the '>'. */
static int tag_attr(const char *beg, const char *end, const char *name,
		    char *out, size_t n)
{
	char pat[32];
	const char *p;

	snprintf(pat, sizeof(pat), "%s=\"", name);
	for (p = beg; (p = strstr(p, pat)); p++) {
		if (p >= end)
			break;
		/* `name` must be a whole attribute, or `id="x"` matches
		 * `app_id="x"` and the card names the wrong thing. */
		if (p > beg && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '\n')
			continue;
		p += strlen(pat);
		const char *q = strchr(p, '"');
		if (!q || q > end)
			return -1;
		size_t len = (size_t)(q - p);
		if (len >= n)
			len = n - 1;
		memcpy(out, p, len);
		out[len] = '\0';
		unescape(out);
		return 0;
	}
	return -1;
}

/* ── rendering one binding into words ──────────────────────────────────── */

/* labwc's modifier letters, expanded. `W-S-comma` is what the file says and
 * `Super+Shift+comma` is what a person can act on. */
static void pretty_key(const char *in, char *out, size_t n)
{
	static const struct { const char *sym; const char *name; } named[] = {
		{ "Return", "Enter" }, { "Escape", "Esc" },
		{ "space", "Space" }, { "comma", "," }, { "period", "." },
		{ "grave", "`" }, { "minus", "-" }, { "equal", "=" },
		{ "slash", "/" }, { "backslash", "\\" },
		{ "XF86AudioRaiseVolume", "Volume Up" },
		{ "XF86AudioLowerVolume", "Volume Down" },
		{ "XF86AudioMute", "Mute" },
		{ "XF86AudioPlay", "Play" }, { "XF86AudioNext", "Next" },
		{ "XF86AudioPrev", "Previous" },
		{ "XF86MonBrightnessUp", "Brightness Up" },
		{ "XF86MonBrightnessDown", "Brightness Down" },
		{ "XF86Display", "Display" },
	};
	char buf[64];
	size_t used = 0;

	snprintf(buf, sizeof(buf), "%s", in);
	out[0] = '\0';

	char *save = NULL;
	for (char *tok = strtok_r(buf, "-", &save); tok;
	     tok = strtok_r(NULL, "-", &save)) {
		const char *word = tok;
		/* A single modifier letter, and only while something follows
		 * it: `W-s` is Super+s and the trailing `s` is the key. */
		if (!tok[1] && save && *save) {
			switch (*tok) {
			case 'W': word = "Super"; break;
			case 'A': word = "Alt"; break;
			case 'C': word = "Ctrl"; break;
			case 'S': word = "Shift"; break;
			default: break;
			}
		}
		if (word == tok) {
			size_t i;
			for (i = 0; i < sizeof(named) / sizeof(named[0]); i++)
				if (!strcmp(tok, named[i].sym)) {
					word = named[i].name;
					break;
				}
		}
		used += (size_t)snprintf(out + used, used < n ? n - used : 0,
					 "%s%s", used ? "+" : "", word);
		if (used >= n)
			break;
	}
}

/* labwc says up and down; an edge is called top and bottom. */
static const char *edge_word(const char *dir)
{
	if (!strcmp(dir, "up"))
		return "top";
	if (!strcmp(dir, "down"))
		return "bottom";
	return dir[0] ? dir : "?";
}

/* The action, in words. An action this table has not heard of is printed by
 * NAME rather than dropped: a row that says `ToggleTearing` is still a row the
 * user can go and look up, and a missing row is a key nobody knows exists. */
static void describe(const char *act, const char *cmd, const char *to,
		     const char *dir, const char *menu, char *out, size_t n)
{
	static const struct { const char *act; const char *desc; } tbl[] = {
		{ "Close", "close the window" },
		{ "Iconify", "minimise" },
		{ "ToggleMaximize", "maximise / restore" },
		{ "ToggleFullscreen", "fullscreen" },
		{ "ToggleShade", "shade / unshade" },
		{ "ToggleAlwaysOnTop", "keep above" },
		{ "ToggleOmnipresent", "on every workspace" },
		{ "ToggleShowDesktop", "show the desktop" },
		{ "ToggleDecorations", "titlebar on / off" },
		{ "NextWindow", "next window" },
		{ "PreviousWindow", "previous window" },
		{ "Focus", "focus" }, { "Raise", "raise" },
		{ "Lower", "lower" }, { "Unfocus", "unfocus" },
		{ "Exit", "end the session" },
		{ "Reconfigure", "reload the configuration" },
		{ "Restart", "restart the compositor" },
	};

	if (!strcmp(act, "Execute")) {
		snprintf(out, n, "%s", cmd[0] ? cmd : "run a command");
		return;
	}
	if (!strcmp(act, "GoToDesktop")) {
		snprintf(out, n, "workspace %s", to[0] ? to : "?");
		return;
	}
	if (!strcmp(act, "SendToDesktop")) {
		snprintf(out, n, "send window to workspace %s", to[0] ? to : "?");
		return;
	}
	if (!strcmp(act, "SnapToEdge")) {
		snprintf(out, n, "snap %s", dir[0] ? dir : "");
		return;
	}
	if (!strcmp(act, "MoveToEdge")) {
		snprintf(out, n, "move to the %s edge", edge_word(dir));
		return;
	}
	if (!strcmp(act, "GrowToEdge")) {
		snprintf(out, n, "grow to the %s edge", edge_word(dir));
		return;
	}
	if (!strcmp(act, "ShowMenu")) {
		snprintf(out, n, "%s",
			 strstr(menu, "root") ? "the root menu" : "the window menu");
		return;
	}
	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (!strcmp(act, tbl[i].act)) {
			snprintf(out, n, "%s", tbl[i].desc);
			return;
		}
	snprintf(out, n, "%s", act);
}

static int classify(const char *key, const char *act, const char *cmd,
		    const char *menu)
{
	static const char *const sys_cmds[] = {
		"kdos-lock", "kdos-display", "kdos-shot", "kdos-power",
		"kdos-keys", "kdos-restarts", NULL
	};

	if (!strncmp(key, "XF86Audio", 9) ||
	    !strncmp(key, "XF86MonBrightness", 17))
		return SEC_MEDIA;
	if (!strcmp(act, "Exit") || !strcmp(act, "Restart") ||
	    !strcmp(act, "Reconfigure"))
		return SEC_SYSTEM;
	if (!strcmp(act, "Execute")) {
		if (!strncmp(cmd, "kdos-osd", 8))
			return SEC_MEDIA;
		for (int i = 0; sys_cmds[i]; i++)
			if (!strncmp(cmd, sys_cmds[i], strlen(sys_cmds[i])))
				return SEC_SYSTEM;
		return SEC_LAUNCH;
	}
	if (!strcmp(act, "ShowMenu"))
		return strstr(menu, "root") ? SEC_LAUNCH : SEC_WINDOW;
	/* GoToDesktop, SendToDesktop, ToggleShowDesktop — the workspace verbs
	 * all carry the word. */
	if (strstr(act, "Desktop"))
		return SEC_WS;
	return SEC_WINDOW;
}

/* ── the parse ─────────────────────────────────────────────────────────── */

static int parse_rc(const char *path)
{
	size_t len = 0;
	char *buf = read_all(path, &len);
	const char *p;

	if (!buf)
		return -1;
	strip_comments(buf);

	p = buf;
	while (nbinds < KEYS_MAX && (p = strstr(p, "<keybind"))) {
		const char *tend = strchr(p, '>');
		char key[40] = "", act[48] = "", cmd[96] = "", to[24] = "",
		     dir[24] = "", menu[32] = "";
		if (!tend)
			break;
		if (tag_attr(p, tend, "key", key, sizeof(key)) != 0 || !key[0]) {
			p = tend;
			continue;
		}

		const char *close = strstr(tend, "</keybind>");
		if (!close)
			close = buf + len;

		/*
		 * The FIRST action that actually does something — except that a
		 * container's own branch beats it. `If` and `ForEach` are
		 * containers: labwc runs the actions inside their <then>/<none>,
		 * so reporting one would tell the user that Super+Escape "ifs",
		 * which is not a thing a key can do. And the actions BEFORE a
		 * container are its preamble: W-Escape saves the session and
		 * then asks whether to Exit, so a card naming kdos-session-save
		 * would file the key that ends the session under `launch`.
		 */
		const char *q = tend;
		int in_cont = 0;
		while ((q = strstr(q, "<action")) && q < close) {
			const char *qe = strchr(q, '>');
			char nm[48] = "";
			if (!qe || qe > close)
				break;
			if (tag_attr(q, qe, "name", nm, sizeof(nm)) != 0) {
				q = qe;
				continue;
			}
			if (!strcmp(nm, "If") || !strcmp(nm, "ForEach")) {
				in_cont = 1;
				q = qe;
				continue;
			}
			if (act[0] && !in_cont)
				break;
			snprintf(act, sizeof(act), "%s", nm);
			cmd[0] = to[0] = dir[0] = menu[0] = '\0';
			tag_attr(q, qe, "command", cmd, sizeof(cmd));
			tag_attr(q, qe, "to", to, sizeof(to));
			tag_attr(q, qe, "direction", dir, sizeof(dir));
			tag_attr(q, qe, "menu", menu, sizeof(menu));
			if (in_cont)
				break;
			q = qe;
		}
		if (!act[0]) {
			p = close;
			continue;
		}

		struct kbind *b = &binds[nbinds];
		pretty_key(key, b->key, sizeof(b->key));
		describe(act, cmd, to, dir, menu, b->desc, sizeof(b->desc));
		b->sect = classify(key, act, cmd, menu);
		nbinds++;
		p = close;
	}
	free(buf);
	return nbinds ? 0 : -1;
}

/* The table the card falls back to. Deliberately short: it is what this
 * desktop cannot be used without, not a second copy of rc.xml that would
 * itself go stale. */
static void builtin_table(void)
{
	static const struct { const char *k, *d; int s; } tbl[] = {
		{ "Super+d", "kdos-launcher", SEC_LAUNCH },
		{ "Super+Enter", "foot", SEC_LAUNCH },
		{ "Alt+F2", "kdos-run", SEC_LAUNCH },
		{ "Super+Space", "the root menu", SEC_LAUNCH },
		{ "Super+q", "close the window", SEC_WINDOW },
		{ "Super+Tab", "next window", SEC_WINDOW },
		{ "Super+m", "maximise / restore", SEC_WINDOW },
		{ "Super+n", "minimise", SEC_WINDOW },
		{ "Super+1..4", "workspace 1 to 4", SEC_WS },
		{ "Super+l", "kdos-lock", SEC_SYSTEM },
		{ "Super+F1", "this card", SEC_SYSTEM },
		{ "Super+Escape", "end the session", SEC_SYSTEM },
	};

	nbinds = 0;
	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
		snprintf(binds[nbinds].key, sizeof(binds[0].key), "%s", tbl[i].k);
		snprintf(binds[nbinds].desc, sizeof(binds[0].desc), "%s", tbl[i].d);
		binds[nbinds].sect = tbl[i].s;
		nbinds++;
	}
}

static int rc_path(char *buf, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(buf, n, "%s/kdos-comp/rc.xml", cfg);
	else if (home && *home)
		snprintf(buf, n, "%s/.config/kdos-comp/rc.xml", home);
	else
		return -1;
	return 0;
}

static void build_rows(void)
{
	nrows = 0;
	keyw = 12;
	for (int i = 0; i < nbinds; i++) {
		int w = (int)strlen(binds[i].key);
		/* Four in from the border, two of gap: without the gap the
		 * longest key in the file runs straight into its own
		 * description and the two read as one word. */
		if (w + 6 > keyw)
			keyw = w + 6;
	}
	if (keyw > 30)
		keyw = 30;

	for (int s = 0; s < SEC_N; s++) {
		int first = 1;
		for (int i = 0; i < nbinds; i++) {
			if (binds[i].sect != s)
				continue;
			if (first) {
				rows[nrows].left = sect_name[s];
				rows[nrows].right = NULL;
				rows[nrows].hdr = 1;
				nrows++;
				first = 0;
			}
			rows[nrows].left = binds[i].key;
			rows[nrows].right = binds[i].desc;
			rows[nrows].hdr = 0;
			nrows++;
		}
	}
}

/* ── first run ─────────────────────────────────────────────────────────── */

static int marker_path(char *buf, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg) {
		snprintf(buf, n, "%s/kdos", cfg);
	} else if (home && *home) {
		/* The parent too: a home materialised without one (a user
		 * added by hand rather than from /etc/skel) would otherwise
		 * fail the mkdir below and never record its first login. */
		snprintf(buf, n, "%s/.config", home);
		mkdir(buf, 0700);
		snprintf(buf, n, "%s/.config/kdos", home);
	} else {
		return -1;
	}
	mkdir(buf, 0700);
	strncat(buf, "/first-run", n - strlen(buf) - 1);
	return 0;
}

static int marker_seen(void)
{
	char path[512];
	struct stat st;

	if (marker_path(path, sizeof(path)) != 0)
		return 1;	/* no home: never claim a first run */
	return stat(path, &st) == 0;
}

static void marker_write(void)
{
	char path[512];
	int fd;

	if (marker_path(path, sizeof(path)) != 0)
		return;
	/* O_EXCL: two logins racing must not have one truncate the other's
	 * marker, and there is nothing in the file worth rewriting. */
	fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd >= 0)
		close(fd);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int list_top_y(int welcome)
{
	return welcome ? 4 : 1;
}

static int list_rows(int welcome)
{
	int n = ktui_h - 2 - list_top_y(welcome);	/* the hint row too */
	return n < 1 ? 1 : n;
}

static void draw(int top, int welcome)
{
	int w = ktui_w, h = ktui_h;
	int y0 = list_top_y(welcome);
	int rowsv = list_rows(welcome);

	if (w < 20 || h < 6)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " keys ", KT_ACCENT, KT_SURFACE, 1);

	if (welcome) {
		ktui_draw_text(2, 1, w - 4, "Welcome to KDOS. I use KDOS btw.",
			       KT_ACCENT, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(2, 2, w - 4,
			       "These are the keys. Super+F1 brings this back.",
			       KT_TEXT, KT_SURFACE, KT_A_NONE);
		ktui_draw_hline(1, 3, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	}

	if (parse_note[0])
		ktui_draw_text(2, y0, w - 4, parse_note, KT_ERR, KT_SURFACE,
			       KT_A_NONE);

	int note = parse_note[0] ? 1 : 0;
	for (int i = 0; i < rowsv - note && top + i < nrows; i++) {
		const struct drow *r = &rows[top + i];
		int y = y0 + note + i;

		if (r->hdr) {
			ktui_draw_text(2, y, w - 4, r->left, KT_WARN,
				       KT_SURFACE, KT_A_NONE);
			continue;
		}
		ktui_draw_text(4, y, keyw - 4, r->left, KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(keyw, y, w - keyw - 2, r->right, KT_MID,
			       KT_SURFACE, KT_A_NONE);
	}

	/* The hint row says how to get out and how to move, because this
	 * surface takes the keyboard and a card you cannot dismiss is a card
	 * that has taken the desktop. */
	char hint[80];
	if (nrows > rowsv - note)
		snprintf(hint, sizeof(hint), "%s%s scroll   any key closes",
			 ktui_glyph[KT_G_UP], ktui_glyph[KT_G_DOWN]);
	else
		snprintf(hint, sizeof(hint), "any key closes");
	ktui_draw_text(2, h - 2, w - 4, hint, KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_flush();
}

/* `--dump-cells` — one line per painted cell, colours included. The
 * backend is cells.c's; these two are what the size flag writes. */
static int cap_w = KEYS_COLS, cap_h = KEYS_ROWS;

/* ── main ──────────────────────────────────────────────────────────────── */

int keys_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, dump_cells = 0, first_run = 0, welcome;
	char path[512];

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			dump_cells = 1;
		/* A golden frame is named for its size, so the harness has to
		 * be able to ask for one — the cell dump goes through a
		 * backend rather than through ktui_offscreen_init() and is out
		 * of reach of the linker wrap that overrides the other. */
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 19 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		}
		/* The login spawn. It is this program rather than the caller
		 * that decides whether the welcome is due, so a session that
		 * spawns it every login still shows it exactly once. */
		else if (!strcmp(argv[i], "--first-run"))
			first_run = 1;
		else {
			fprintf(stderr, "usage: kdos-keys [--first-run] "
					"[--dump|--dump-cells] [--dump-size WxH] "
					"[--font NAME]\n");
			return 2;
		}
	}

	if (first_run && !dump && !dump_cells && marker_seen())
		return 0;

	if (rc_path(path, sizeof(path)) != 0 || parse_rc(path) != 0) {
		/* /etc/skel's copy is what a home with no rc.xml is running:
		 * the compositor reads its own default from the same file the
		 * user's would have been copied from. */
		if (parse_rc("/etc/skel/.config/kdos-comp/rc.xml") != 0) {
			builtin_table();
			snprintf(parse_note, sizeof(parse_note),
				 "rc.xml unreadable - built-in defaults shown");
		}
	}
	build_rows();

	/*
	 * The welcome belongs to the login spawn and to nothing else. Deciding
	 * it from the marker alone made a Super+F1 pressed before that spawn had
	 * run show the banner AND consume the marker, so the real first run then
	 * returned above having shown nothing. `--first-run` already returned
	 * when the marker is there, so this is the whole of the test; a dump
	 * decides nothing and returns before the marker is written.
	 */
	welcome = first_run;

	if (dump || dump_cells) {
		sh_theme_from_cache();
		if (dump_cells) {
			ktui_backend_set(sh_cells_backend(cap_w, cap_h));
			ktui_draw_init();
			draw(0, welcome);
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw(0, welcome);
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = KEYS_COLS,
		.rows = KEYS_ROWS,
		.app_id = "kdos-keys",
		.font = font,
		.keyboard = 1,
		/* A card, not a dialog: clicking on a window puts it away. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-keys: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	int top = 0;

	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int rowsv = list_rows(welcome) - (parse_note[0] ? 1 : 0);
		int maxtop = nrows - rowsv;

		if (maxtop < 0)
			maxtop = 0;
		if (top > maxtop)
			top = maxtop;
		if (top < 0)
			top = 0;

		draw(top, welcome);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP)
				top--;
			else if (ev.btn == KT_MB_WHEEL_DOWN)
				top++;
			else
				break;	/* any click on the card dismisses it */
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/*
		 * Any key closes — except the ones that move the list. A card
		 * taller than the screen that dismissed itself on Down would
		 * be a card whose second half nobody could read.
		 */
		switch (ev.key) {
		case KT_K_UP:
			top--;
			break;
		case KT_K_DOWN:
			top++;
			break;
		case KT_K_PGUP:
			top -= rowsv;
			break;
		case KT_K_PGDN:
			top += rowsv;
			break;
		case KT_K_HOME:
			top = 0;
			break;
		case KT_K_END:
			top = maxtop;
			break;
		default:
			goto done;
		}
	}
done:
	kwl_shutdown();
	if (welcome)
		marker_write();
	return 0;
}
