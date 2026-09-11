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
 * HELP THAT CANNOT LIE. The card is GENERATED from the table the desktop it is
 * opened on actually loaded, so a rebound key changes the card in the same
 * edit — a hand-written cheat sheet is a document that starts rotting the day
 * it is written, and a keyboard-driven desktop lives or dies on whether anyone
 * ever finds Super+d.
 *
 * THE READING IS chords.c's AND THE WORDS ARE THIS FILE'S. Which desktop is
 * running and what it binds is one question with one answer, asked by every
 * surface that names a chord; what a chord is CALLED, which of the six
 * sections it is filed under and which rows the tour is made of are the card's
 * alone, and the tables below are where they live.
 *
 * When the reader yields nothing it SAYS SO and shows the built-in table: a
 * help surface that silently comes up empty is worse than one that admits it
 * could not read its own configuration.
 *
 * `--first-run` puts a four-row tour above the list — a terminal, the menu,
 * another workspace, the window left behind — and every row is one of the
 * parsed bindings rather than a sentence about it, for the same reason the
 * list is. It is the login spawn's flag and this program decides whether the
 * welcome is due, so a session that asks at every login still shows it once.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"
#include "chords.h"
#include "progkeys.h"

#define KEYS_COLS 72
#define KEYS_ROWS 24

enum { SEC_LAUNCH = 0, SEC_WINDOW, SEC_WS, SEC_TOOLS, SEC_MEDIA,
       SEC_SYSTEM, SEC_N };

static const char *const sect_name[SEC_N] = {
	"launch", "window", "workspace", "tools", "media", "system"
};

/*
 * THE FIRST-RUN TOUR IS FOUR OF THE BINDINGS, not four sentences beside them.
 * Every source of chords stamps these roles onto the rows it produces, so the
 * tour names the chord this session binds — a rebound terminal changes the
 * welcome in the same edit, which is the whole reason this file generates
 * rather than remembers. WEL_CARD is not a step: it is the chord the hint row
 * names so the card can be brought back.
 */
enum { WEL_NONE = 0, WEL_TERM, WEL_MENU, WEL_WS, WEL_NEXT, WEL_CARD };

/* One card row per chord the reader loaded: this surface's words for it and
 * the group it is filed under. The chord itself is chords.c's storage and is
 * pointed at rather than copied — one table and one answer. */
struct kcard {
	const char *key;	/* as a person reads it: Super+Shift+d     */
	char desc[80];		/* what it does                            */
	int sect;
	int role;		/* WEL_* where this row is a step of the tour */
};

static struct kcard cards[SH_CHORD_MAX];
static int ncards;

/* The display list: section headers and entries in one flat array, because
 * scrolling a grouped list is only simple when the groups are already rows. */
struct drow {
	const char *left;
	const char *right;
	int hdr;
};

static struct drow rows[SH_CHORD_MAX + SEC_N];
static int nrows;
static int keyw = 16;		/* the key column, sized to what was parsed */

/* Non-empty when the card is showing built-in defaults instead of the real
 * configuration — drawn where the user cannot miss it. */
static char parse_note[80];

/* No layers: the card closes on any key, and Esc is the one it names. */
static KtuiKeys keys;

/* ── rendering one binding into words ──────────────────────────────────── */

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
/* `detail` is the one thing the source knew about the binding — the command an
 * Execute runs, the workspace a desktop action names, the menu a ShowMenu
 * opens — because that is what one row of a chord table carries. */
static void describe(const char *act, const char *detail, char *out, size_t n)
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
		snprintf(out, n, "%s", detail[0] ? detail : "run a command");
		return;
	}
	/* ONE KEY PER PROGRAM. `detail` is the program the ForEach looks for,
	 * which is the whole of what the key does — raise it, or start it. */
	if (!strcmp(act, "ForEach")) {
		snprintf(out, n, "%s, or raise it",
			 detail[0] ? detail : "a program");
		return;
	}
	/* A FLAG TOGGLED AGAINST AN APP ID IS THE SCRATCHPAD and not a plain
	 * omnipresence toggle: the key finds the one window started under that
	 * marker, puts it over every window on whatever workspace is up, and the
	 * same press takes it away again. */
	if (!strcmp(act, "ToggleOmnipresent") && detail[0]) {
		snprintf(out, n, "the drop-down terminal, over every window");
		return;
	}
	if (!strcmp(act, "GoToDesktop")) {
		snprintf(out, n, "workspace %s", detail[0] ? detail : "?");
		return;
	}
	if (!strcmp(act, "SendToDesktop")) {
		snprintf(out, n, "send window to workspace %s",
			 detail[0] ? detail : "?");
		return;
	}
	if (!strcmp(act, "SnapToEdge")) {
		snprintf(out, n, "snap %s", detail[0] ? detail : "");
		return;
	}
	if (!strcmp(act, "MoveToEdge")) {
		snprintf(out, n, "move to the %s edge", edge_word(detail));
		return;
	}
	if (!strcmp(act, "GrowToEdge")) {
		snprintf(out, n, "grow to the %s edge", edge_word(detail));
		return;
	}
	if (!strcmp(act, "ShowMenu")) {
		snprintf(out, n, "%s",
			 strstr(detail, "root") ? "the root menu"
						: "the window menu");
		return;
	}
	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (!strcmp(act, tbl[i].act)) {
			snprintf(out, n, "%s", tbl[i].desc);
			return;
		}
	snprintf(out, n, "%s", act);
}

/* The tour's steps, read off the compositor's own actions. The terminal is
 * matched against the leading word of the command, which the reader has
 * already rewritten to the emulator this desktop runs, so the row names the
 * one that will actually open. */
static int rc_role(const char *act, const char *detail)
{
	const char *t = sh_term();
	size_t tn = strlen(t);

	if (!strcmp(act, "Execute")) {
		if (!strncmp(detail, t, tn) &&
		    (detail[tn] == '\0' || detail[tn] == ' '))
			return WEL_TERM;
		if (!strncmp(detail, "kdos-keys", 9))
			return WEL_CARD;
		/* THE SEARCH IS THE TOUR'S SECOND STEP ON BOTH DESKTOPS, and
		 * under the compositor it is an Execute like any other rather
		 * than a labwc verb — W-space runs the palette where it used
		 * to open the root menu. Without this the step has nothing to
		 * name here and the tour comes up with a hole in it. */
		if (!strncmp(detail, "kdos-palette", 12))
			return WEL_MENU;
		return WEL_NONE;
	}
	/* The root menu is still what the RIGHT BUTTON opens, and it is still
	 * worth a row on the card — it is no longer the tour's step, because
	 * the chord a newcomer is told to press now opens the search. */
	if (!strcmp(act, "ShowMenu"))
		return WEL_NONE;
	if (!strcmp(act, "GoToDesktop"))
		return !strcmp(detail, "right") ? WEL_WS : WEL_NONE;
	if (!strcmp(act, "NextWindow"))
		return WEL_NEXT;
	return WEL_NONE;
}

/*
 * THE MEDIA KEYS, SPELLED THE WAY A PERSON READS THEM. A chord reaches this
 * side already worded, so a media key the speller knows arrives as its friendly
 * name and one it does not know arrives as its XF86 symbol — both spellings are
 * tested. A media key held with a modifier is NOT one of these rows: the chord
 * then names the modifier too and the section is decided by what the key does.
 *
 * `Display` is deliberately absent. XF86Display opens the screen surface, and
 * the key that opens a surface is filed where every other one is.
 */
static int media_chord(const char *chord)
{
	static const char *const named[] = {
		"Volume Up", "Volume Down", "Mute", "Play", "Next",
		"Previous", "Brightness Up", "Brightness Down", NULL
	};

	if (!strncmp(chord, "XF86Audio", 9) ||
	    !strncmp(chord, "XF86MonBrightness", 17))
		return 1;
	for (int i = 0; named[i]; i++)
		if (!strcmp(chord, named[i]))
			return 1;
	return 0;
}

static int classify(const char *chord, const char *act, const char *detail)
{
	static const char *const sys_cmds[] = {
		"kdos-lock", "kdos-display", "kdos-shot", "kdos-power",
		"kdos-keys", "kdos-restarts", NULL
	};

	if (media_chord(chord))
		return SEC_MEDIA;
	if (!strcmp(act, "Exit") || !strcmp(act, "Restart") ||
	    !strcmp(act, "Reconfigure"))
		return SEC_SYSTEM;
	if (!strcmp(act, "Execute")) {
		if (!strncmp(detail, "kdos-osd", 8))
			return SEC_MEDIA;
		for (int i = 0; sys_cmds[i]; i++)
			if (!strncmp(detail, sys_cmds[i], strlen(sys_cmds[i])))
				return SEC_SYSTEM;
		return SEC_LAUNCH;
	}
	if (!strcmp(act, "ForEach"))
		return SEC_LAUNCH;
	if (!strcmp(act, "ShowMenu"))
		return strstr(detail, "root") ? SEC_LAUNCH : SEC_WINDOW;
	/* GoToDesktop, SendToDesktop, ToggleShowDesktop — the workspace verbs
	 * all carry the word. */
	if (strstr(act, "Desktop"))
		return SEC_WS;
	return SEC_WINDOW;
}

/*
 * THE CONSOLE'S CHORDS COME FROM THE SESSION THAT BINDS THEM — chords.c runs
 * `kdos-con --keys` on this desktop and parses `rc.xml` on the other one, so
 * the table that binds the chords is the table that prints them.
 *
 * THIS SIDE OWNS ONLY HOW THEY ARE DESCRIBED AND GROUPED, which is presentation
 * and belongs to the card. An action with no row here is DROPPED rather than
 * shown by its own name: `swap-up` on a card teaches nobody anything, and
 * selftest.sh fails the build when the session binds an action this table has
 * forgotten.
 */
/* The tour's steps on the console, by the action that performs them. A table
 * of its own rather than a fifth column on the one below: four rows of forty
 * would be buried there, and the roles are read by one caller. */
static int con_role(const char *act)
{
	static const struct { const char *act; int role; } tbl[] = {
		{ "terminal",		WEL_TERM },
		/* THE TOUR'S SECOND STEP IS THE PALETTE, because that is what
		 * the chord a newcomer presses now opens: Super+space searches
		 * everything, and the menu is the Start button and the F-key.
		 * A step naming an action nothing binds is dropped from the
		 * tour rather than shown wrong, and selftest.sh fails the
		 * build on it — which is how this was caught. */
		{ "palette",		WEL_MENU },
		{ "workspace-next",	WEL_WS },
		{ "next",		WEL_NEXT },
		{ "keys",		WEL_CARD },
	};

	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (!strcmp(tbl[i].act, act))
			return tbl[i].role;
	return WEL_NONE;
}

static int con_section(const char *act, const char **desc)
{
	static const struct { const char *act, *desc; int sect; } tbl[] = {
		{ "terminal",	NULL,			SEC_LAUNCH },
		{ "launcher",	"kdos-launcher",	SEC_LAUNCH },
		{ "launcher-fkey", "kdos-launcher",	SEC_LAUNCH },
		{ "palette",	"search everything: windows, apps, settings",
							SEC_LAUNCH },
		{ "menu",	"the root menu",	SEC_LAUNCH },
		{ "menu-fkey",	"the root menu",	SEC_LAUNCH },
		/*
		 * ONE KEY PER PROGRAM. The description is the ROLE and not the
		 * program's name — which program fills it is a con.conf key,
		 * and the row is dropped entirely when that program is not
		 * installed, so a card never offers a key that opens nothing.
		 */
		{ "files",	"the file manager, or raise it",	SEC_LAUNCH },
		{ "mail",	"mail, or raise it",	SEC_LAUNCH },
		{ "browser",	"the web, or raise it",	SEC_LAUNCH },
		{ "music",	"music, or raise it",	SEC_LAUNCH },
		{ "agenda",	"the diary, or raise it",	SEC_LAUNCH },
		{ "chat",	"chat, or raise it",	SEC_LAUNCH },
		{ "writing",	"the editor, or raise it",	SEC_LAUNCH },
		{ "close",	"close the window",	SEC_WINDOW },
		{ "maximise",	"maximise / restore",	SEC_WINDOW },
		{ "fullscreen",	"fullscreen",		SEC_WINDOW },
		{ "minimise",	"minimise",		SEC_WINDOW },
		{ "restore",	"bring back the last minimised",	SEC_WINDOW },
		/* THE SAME WORDS THE COMPOSITOR'S ROW USES. One key, two
		 * desktops: a card that called it a scratchpad here and a
		 * drop-down there would read as two features. */
		{ "scratchpad",	"the drop-down terminal, over every window",
							SEC_WINDOW },
		{ "scratchpad-mark", "make this window the drop-down",
							SEC_WINDOW },
		{ "next",	"next window",		SEC_WINDOW },
		{ "prev",	"previous window",	SEC_WINDOW },
		{ "next-alt",	"next window",		SEC_WINDOW },
		{ "prev-alt",	"previous window",	SEC_WINDOW },
		{ "snap-left",	"snap left",		SEC_WINDOW },
		{ "snap-right",	"snap right",		SEC_WINDOW },
		{ "snap-up",	"snap up",		SEC_WINDOW },
		{ "snap-down",	"snap down",		SEC_WINDOW },
		{ "focus-left",	 "focus left",		SEC_WINDOW },
		{ "focus-right", "focus right",		SEC_WINDOW },
		{ "focus-up",	 "focus up",		SEC_WINDOW },
		{ "focus-down",	 "focus down",		SEC_WINDOW },
		{ "swap-left",	 "swap with the window left",	SEC_WINDOW },
		{ "swap-right",	 "swap with the window right",	SEC_WINDOW },
		{ "swap-up",	 "swap with the window above",	SEC_WINDOW },
		{ "swap-down",	 "swap with the window below",	SEC_WINDOW },
		{ "workspace-prev", "previous workspace in use",	SEC_WS },
		{ "workspace-next", "next workspace in use",	SEC_WS },
		/* The two the session prints no line for: it answers a digit
		 * directly rather than binding nine actions, and the reader
		 * synthesises the rows so a desktop with workspaces does not
		 * read as one without. */
		{ "workspace-digit", "switch workspace",	SEC_WS },
		{ "workspace-send-digit", "send the window there",	SEC_WS },
		{ "tile",	"tile this workspace",		SEC_WINDOW },
		{ "tile-fkey",	"tile this workspace",		SEC_WINDOW },
		{ "cascade",	"cascade this workspace",	SEC_WINDOW },
		{ "rearrange",	"move and size from the keyboard",	SEC_WINDOW },
		{ "rearrange-fkey", "move and size from the keyboard",	SEC_WINDOW },
		{ "show-desktop", "hide every window, and bring them back",
							SEC_WINDOW },
		{ "windows",	"the window list",		SEC_WINDOW },
		{ "mark",	"mark text anywhere on the screen",	SEC_WINDOW },
		{ "paste",	"paste what was marked",	SEC_WINDOW },
		{ "capture",	"mark a rectangle: text copied, picture filed",	SEC_WINDOW },
		{ "capture-menu", "capture: a region, a recording, a QR code",
							SEC_TOOLS },
		{ "capture-screen", "the whole screen to a file",	SEC_TOOLS },
		{ "capture-print", "mark a rectangle: text copied, picture filed",
							SEC_TOOLS },
		{ "capture-record", "record the screen to a file, and stop",
							SEC_TOOLS },
		{ "time",	"the time and date, as a notice",	SEC_TOOLS },
		{ "battery",	"what the battery says, as a notice",	SEC_TOOLS },
		{ "remind",	"remind me — `in 20m tea`",	SEC_TOOLS },
		{ "remind-ls",	"the reminders still waiting",	SEC_TOOLS },
		{ "remind-clear", "forget every reminder",	SEC_TOOLS },
		{ "dismiss",	"put the newest notice away",	SEC_TOOLS },
		{ "dismiss-all", "put every notice away",	SEC_TOOLS },
		{ "dnd",	"hold notices back, and let them through",
							SEC_TOOLS },
		{ "undismiss",	"bring the last one back",	SEC_TOOLS },
		{ "stay-awake",	"never lock or blank on idle, or do",
							SEC_SYSTEM },
		{ "taskbar",	"put the bar away, and bring it back",
							SEC_WINDOW },
		{ "night-light", "warm the palette, and cool it",	SEC_SYSTEM },
		{ "learn",	"record the keys you type, and stop",	SEC_WINDOW },
		{ "play",	"type a recorded script back",	SEC_WINDOW },
		/* The screen's own font, which is the view's and not a
		 * window's — filed under system for that reason. */
		{ "font-up",	"bigger text on the screen",	SEC_SYSTEM },
		{ "font-down",	"smaller text on the screen",	SEC_SYSTEM },
		{ "font-reset",	"the text size back",		SEC_SYSTEM },
		{ "theme",	"the accent, with a live preview",	SEC_SYSTEM },
		{ "background",	"the console's character-art ground",	SEC_SYSTEM },
		{ "volume-up",	"louder",		SEC_WINDOW },
		{ "volume-down", "quieter",		SEC_WINDOW },
		{ "volume-mute", "mute and unmute",	SEC_WINDOW },
		{ "media-play",	"play or pause",	SEC_WINDOW },
		{ "media-stop",	"stop playing",		SEC_WINDOW },
		{ "media-next",	"the next track",	SEC_WINDOW },
		{ "media-prev",	"the track before",	SEC_WINDOW },
		/*
		 * The surfaces. Eleven chords reaching eleven programs, and
		 * the description is what the program IS rather than its name:
		 * a card that read `kdos-bt` would be a card only somebody who
		 * already knew the answer could use.
		 */
		{ "setup-menu",	"every one of them, in one list",	SEC_TOOLS },
		{ "keys",	"this card",		SEC_TOOLS },
		{ "audio",	"sound devices and volume",	SEC_TOOLS },
		{ "net",	"networking",		SEC_TOOLS },
		{ "bluetooth",	"Bluetooth",		SEC_TOOLS },
		{ "devices",	"cameras, microphones, disks",	SEC_TOOLS },
		{ "settings",	"settings",		SEC_TOOLS },
		{ "calendar",	"the calendar",		SEC_TOOLS },
		{ "find",	"files by name or contents",	SEC_TOOLS },
		{ "docs",	"the documentation",	SEC_TOOLS },
		{ "displays",	"screens",		SEC_TOOLS },
		{ "power",	"power and battery",	SEC_TOOLS },
		{ "monitor",	"processes, CPU, memory",	SEC_TOOLS },
		{ "calculator",	"a calculator that reads units",	SEC_TOOLS },
		{ "notes",	"the scratch pad",		SEC_TOOLS },
		{ "clipboard",	"what has been copied",		SEC_TOOLS },
		{ "characters",	"any character, by its name",	SEC_TOOLS },
		{ "contacts",	"names, numbers and addresses",	SEC_TOOLS },
		{ "lock",	"kdos-lock",		SEC_SYSTEM },
		{ "saver",	"kdos-saver",		SEC_SYSTEM },
		{ "quit",	"end the session",	SEC_SYSTEM },
		{ "leader",	"then a chord's own key, where Super does not arrive",
							SEC_SYSTEM },
	};

	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (!strcmp(tbl[i].act, act)) {
			/* The terminal is not the same program on the two
			 * desktops, and the card names the one that opens. */
			*desc = tbl[i].desc ? tbl[i].desc : sh_term();
			return tbl[i].sect;
		}
	return -1;
}

/*
 * THE BUILT-IN DEFAULTS' WORDS. The chords are the reader's — it knows which
 * desktop is running and which spelling the one default they do not share
 * takes — and the words are the card's, like every other source's.
 *
 * THE FIELDS ARE NAMED here on purpose. selftest.sh proves con_section() above
 * has a row for every action `kdos-con --keys` prints by looking for
 * `{ "name",` in this file, and a second bare-brace table would answer in the
 * place of a row that had gone. An omitted `.role` is WEL_NONE.
 */
static int builtin_words(const char *act, const char **desc, int *role)
{
	static const struct { const char *act, *desc; int sect, role; } tbl[] = {
		{ .act = "launcher", .desc = "kdos-launcher",
		  .sect = SEC_LAUNCH },
		/* NULL: the terminal is not the same program on the two
		 * desktops, and the card names the one that will open. */
		{ .act = "terminal", .desc = NULL,
		  .sect = SEC_LAUNCH, .role = WEL_TERM },
		{ .act = "run", .desc = "kdos-run", .sect = SEC_LAUNCH },
		{ .act = "menu", .desc = "the root menu",
		  .sect = SEC_LAUNCH, .role = WEL_MENU },
		{ .act = "close", .desc = "close the window",
		  .sect = SEC_WINDOW },
		{ .act = "next", .desc = "next window",
		  .sect = SEC_WINDOW, .role = WEL_NEXT },
		{ .act = "maximise", .desc = "maximise / restore",
		  .sect = SEC_WINDOW },
		{ .act = "fullscreen", .desc = "fullscreen",
		  .sect = SEC_WINDOW },
		{ .act = "minimise", .desc = "minimise", .sect = SEC_WINDOW },
		{ .act = "workspace-1-4", .desc = "workspace 1 to 4",
		  .sect = SEC_WS, .role = WEL_WS },
		{ .act = "lock", .desc = "kdos-lock", .sect = SEC_SYSTEM },
		{ .act = "keys", .desc = "this card",
		  .sect = SEC_SYSTEM, .role = WEL_CARD },
		{ .act = "quit", .desc = "end the session",
		  .sect = SEC_SYSTEM },
	};

	for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
		if (!strcmp(tbl[i].act, act)) {
			/* The terminal is not the same program on the two
			 * desktops, and the card names the one that opens. */
			*desc = tbl[i].desc ? tbl[i].desc : sh_term();
			*role = tbl[i].role;
			return tbl[i].sect;
		}
	return -1;
}

/* ── the rows ──────────────────────────────────────────────────────────── */

/*
 * A CARD ROW FOR EVERY CHORD THE READER LOADED, in the order it loaded them.
 *
 * WHICH TABLE WORDS A ROW IS THE SOURCE'S: the console's actions are named in
 * con_section(), the compositor's in describe(), and the fallback's in
 * builtin_words(). The discriminator is the one the reader itself used —
 * $KDOS_CON — because a row worded out of the other desktop's table would be
 * as wrong as a chord read from it.
 */
static void build_cards(void)
{
	const char *con = getenv("KDOS_CON");
	int on_con = con && *con;
	int n = sh_chords_load();
	int defaults = sh_chords_builtin();

	ncards = 0;
	for (int i = 0; i < n && ncards < SH_CHORD_MAX; i++) {
		const struct sh_chord *c = sh_chord_at(i);
		const char *desc = NULL;
		char words[80];
		int role = WEL_NONE;
		int sect;

		if (!c)
			continue;
		if (defaults) {
			sect = builtin_words(c->action, &desc, &role);
		} else if (on_con) {
			/* The recorded scripts are the one console row whose
			 * words are the source's: the letters somebody
			 * recorded are data, and this side has nothing to add
			 * to them. */
			if (!strcmp(c->action, "script")) {
				desc = c->detail;
				sect = SEC_WINDOW;
			} else {
				sect = con_section(c->action, &desc);
			}
			role = con_role(c->action);
		} else {
			describe(c->action, c->detail, words, sizeof(words));
			desc = words;
			sect = classify(c->chord, c->action, c->detail);
			role = rc_role(c->action, c->detail);
		}
		if (sect < 0)
			continue;
		cards[ncards].key = c->chord;
		snprintf(cards[ncards].desc, sizeof(cards[0].desc), "%s", desc);
		cards[ncards].sect = sect;
		cards[ncards].role = role;
		ncards++;
	}

	/*
	 * READ SOMETHING AND COULD WORD NONE OF IT is the same failure as
	 * reading nothing, and it has to say so the same way. The reader
	 * cannot detect this — it does not have con_section()'s table — so the
	 * card checks its own output and asks for the built-ins, which is what
	 * also puts the note on screen.
	 */
	if (!ncards && n && !defaults) {
		sh_chords_use_builtin();
		build_cards();
	}
}

/*
 * WHAT WAS TYPED, AND THE ONE MATCHER. A card of ninety rows is a card people
 * scroll rather than read, and the thing somebody wants is nearly always one
 * row of it. `kb_fuzzy()` is the desktop's only answer to "does this match",
 * so the card ranks a query the same way the palette and the launcher do —
 * three surfaces disagreeing about one query is what that function exists to
 * stop.
 *
 * BOTH COLUMNS ARE SEARCHED. Somebody looking for the tiling chord may type
 * `tile` or may type `Super`, and a card that only matched the description
 * would answer the first and not the second.
 */
static char query[64];
static int qlen;

/*
 * ESCAPE CLEARS THE QUERY BEFORE IT CLOSES THE CARD, through the same layer
 * contract every other surface here uses rather than by testing Esc first: a
 * field whose Escape throws away the whole surface is a field people lose
 * their place in, and the hint row says which of the two the next press does
 * because the contract writes it.
 */
static void build_rows(void);

/* ── the second page ───────────────────────────────────────────────────────
 *
 * THE FOCUSED PROGRAM'S OWN KEYS, for the three that publish them. `Tab`
 * reaches it and `Tab` comes back, and the page is offered ONLY when there is
 * a reader for what is focused — a Tab that leads to an empty screen is worse
 * than no Tab, because it teaches that the feature is broken rather than that
 * this program does not publish its keys.
 *
 * WHICH PROGRAM IS FOCUSED IS THE DISPLAY'S ANSWER, and it is asked in two
 * ways because a terminal gives two. The app id is the window's own name, and
 * for a program running INSIDE a terminal — which is what tmux and mc always
 * are — the title is what carries it: a terminal sets its title from the
 * program it is running, which is the whole reason `xterm_title` is on in the
 * shipped mc configuration.
 */
enum { PAGE_CHORDS = 0, PAGE_PROG };

static int page;
static char prog[64];
static struct sh_progkey pkeys[SH_CHORD_MAX];
static int npkeys;

/* The first word of a title, lowercased: a title reads `mc [user@host]:/path`
 * or `tmux: 0:bash`, and the program is the head of it. */
static void first_word(const char *in, char *out, size_t n)
{
	size_t i = 0;

	out[0] = '\0';
	if (!in)
		return;
	while (*in == ' ' || *in == '\t')
		in++;
	for (; in[i] && i + 1 < n; i++) {
		char c = in[i];

		if (c == ' ' || c == ':' || c == '\t' || c == '[')
			break;
		out[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
	}
	out[i] = '\0';
}

static void focused_prog(void)
{
	prog[0] = '\0';
	if (!kdisp_win_supported())
		return;
	for (int i = 0; i < kdisp_win_count(); i++) {
		KDispWin w;
		char word[64];

		if (!kdisp_win_at(i, &w) || !(w.flags & KDISP_WIN_FOCUSED))
			continue;
		first_word(w.app_id, word, sizeof(word));
		if (sh_progkeys_known(word)) {
			kb_strlcpy(prog, word, sizeof(prog));
			return;
		}
		first_word(w.title, word, sizeof(word));
		if (sh_progkeys_known(word))
			kb_strlcpy(prog, word, sizeof(prog));
		return;
	}
}

static int query_up(void *user)
{
	(void)user;
	return qlen > 0;
}

static void query_clear(void *user)
{
	(void)user;
	query[0] = '\0';
	qlen = 0;
	build_rows();
}

static int card_hit(const struct kcard *c)
{
	if (!qlen)
		return 1;
	return kb_fuzzy(c->desc, query) || kb_fuzzy(c->key, query);
}

static void build_rows(void)
{
	nrows = 0;
	keyw = 12;

	/*
	 * THE PROGRAM'S PAGE IS ONE FLAT LIST, with no sections: the sections
	 * on the chord page are this desktop's own grouping, and inventing
	 * groups for somebody else's key list would be this card deciding what
	 * tmux's keys are about.
	 */
	if (page == PAGE_PROG) {
		for (int i = 0; i < npkeys; i++) {
			int w = (int)strlen(pkeys[i].key);

			if (w + 6 > keyw)
				keyw = w + 6;
		}
		if (keyw > 30)
			keyw = 30;
		for (int i = 0; i < npkeys && nrows < SH_CHORD_MAX; i++) {
			if (qlen && !kb_fuzzy(pkeys[i].desc, query) &&
			    !kb_fuzzy(pkeys[i].key, query))
				continue;
			rows[nrows].left = pkeys[i].key;
			rows[nrows].right = pkeys[i].desc;
			rows[nrows].hdr = 0;
			nrows++;
		}
		return;
	}

	for (int i = 0; i < ncards; i++) {
		int w = (int)strlen(cards[i].key);
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
		for (int i = 0; i < ncards; i++) {
			if (cards[i].sect != s)
				continue;
			/* A SECTION WITH NO HIT DRAWS NO HEADING, because the
			 * heading is written when its first row is — a column
			 * of section names over nothing reads as a card that
			 * failed to load rather than as a search that found
			 * one row. */
			if (!card_hit(&cards[i]))
				continue;
			if (first) {
				rows[nrows].left = sect_name[s];
				rows[nrows].right = NULL;
				rows[nrows].hdr = 1;
				nrows++;
				first = 0;
			}
			rows[nrows].left = cards[i].key;
			rows[nrows].right = cards[i].desc;
			rows[nrows].hdr = 0;
			nrows++;
		}
	}
}

/*
 * THE CARD AS TEXT, for a printer and for a wall.
 *
 * The same rows the surface draws, so a printed sheet cannot disagree with the
 * screen — which a second table written by hand eventually would. Two columns
 * at 132 characters because that is the width every terminal printer and every
 * wide terminal has, and because the table does not fit one page in one.
 *
 * NO DISPLAY SERVER IS TOUCHED. This runs before `kdisp_init`, so it works
 * over ssh, from a script and on a machine whose session is not up — which is
 * most of the times somebody wants the card on paper.
 */
#define PRINT_COLS 132
#define PRINT_ROWS 66

static void print_one(const struct drow *r, int w)
{
	if (!r) {
		printf("%*s", w, "");
		return;
	}
	if (r->hdr) {
		int n = printf("%s", r->left);

		/* A rule under the heading, to the column's own width: the
		 * groups are what makes a page of eighty chords findable. */
		while (n < w - 1 && n >= 0)
			n += printf("-");
		printf("%*s", n < w ? w - n : 0, "");
		return;
	}
	printf("  %-*.*s%-*.*s", keyw - 2, keyw - 2, r->left,
	       w - keyw, w - keyw, r->right ? r->right : "");
}

static int print_rows(void)
{
	int col = PRINT_COLS / 2;
	int per = PRINT_ROWS - 3;
	int pages = (nrows + per * 2 - 1) / (per * 2);

	if (pages < 1)
		pages = 1;
	for (int p = 0; p < pages; p++) {
		int base = p * per * 2;

		printf("%-*s%s\n", col, "KDOS keyboard",
		       parse_note[0] ? parse_note : "");
		printf("\n");
		for (int y = 0; y < per; y++) {
			int l = base + y, r = base + per + y;

			if (l >= nrows && r >= nrows)
				break;
			print_one(l < nrows ? &rows[l] : NULL, col);
			if (r < nrows)
				print_one(&rows[r], col);
			printf("\n");
		}
		/* A form feed BETWEEN pages and not after the last: a trailing
		 * one ejects a blank sheet on every printer that honours it. */
		if (p + 1 < pages)
			printf("\f");
	}
	return 0;
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

/* ── the first-run tour ────────────────────────────────────────────────── */

struct welstep {
	const char *key;
	const char *what;
};

static struct welstep wel[4];
static int nwel;

static const char *role_key(int role)
{
	for (int i = 0; i < ncards; i++)
		if (cards[i].role == role)
			return cards[i].key;
	return NULL;
}

/*
 * Four things to do, in the order somebody sitting down does them: a terminal,
 * the menu, another workspace, the window they left behind. A step whose chord
 * this session does not bind is DROPPED rather than guessed — the tour is the
 * one frame whose reader has no way to tell a live chord from a dead one.
 */
static void build_welcome(void)
{
	static const struct { int role; const char *what; } step[] = {
		{ WEL_TERM, "open a terminal" },
		{ WEL_MENU, "search for anything" },
		{ WEL_WS,   "switch workspaces" },
		{ WEL_NEXT, "reach another terminal" },
	};

	nwel = 0;
	for (size_t i = 0; i < sizeof(step) / sizeof(step[0]); i++) {
		const char *k = role_key(step[i].role);

		if (!k)
			continue;
		wel[nwel].key = k;
		wel[nwel].what = step[i].what;
		nwel++;
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/* The tour costs a greeting, its rows and a rule. Under twelve rows it is not
 * drawn at all: what it pushes off the bottom is the card, and a welcome that
 * leaves three bindings showing has taken more than it taught. */
static int welcome_on(int welcome)
{
	return welcome && ktui_h >= 12;
}

/*
 * THE INPUT ROW IS ALWAYS DRAWN, and it costs the list one row. A field that
 * appeared once somebody started typing would be a field nobody discovers:
 * the card is the surface people open when they do not know what to press, and
 * the one thing it must say is that it can be asked.
 */
#define KEYS_INPUT_ROWS 1

static int list_top_y(int welcome)
{
	return (welcome_on(welcome) ? nwel + 3 : 1) + KEYS_INPUT_ROWS;
}

static int list_rows(int welcome)
{
	int n = ktui_h - 2 - list_top_y(welcome);	/* the hint row too */
	return n < 1 ? 1 : n;
}

/* Where the field itself goes: directly above the list it filters. */
static int input_y(int welcome)
{
	return list_top_y(welcome) - KEYS_INPUT_ROWS;
}

static void draw(int top, int welcome)
{
	int w = ktui_w, h = ktui_h;
	int y0 = list_top_y(welcome);
	int rowsv = list_rows(welcome);

	if (w < 20 || h < 6)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	{
		char title[80];

		/* The page names the program, or the card would be two
		 * screens of keys with nothing saying whose. */
		if (page == PAGE_PROG)
			snprintf(title, sizeof(title), " %s ", prog);
		else
			snprintf(title, sizeof(title), " keys ");
		ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE,
			      1);
	}

	/* The tour reuses the list's two columns, so the four things to do and
	 * the bindings under them read as one table. */
	if (welcome_on(welcome)) {
		ktui_draw_text(2, 1, w - 4, "Welcome to KDOS. I use KDOS btw.",
			       KT_ACCENT, KT_SURFACE, KT_A_NONE);
		for (int i = 0; i < nwel; i++) {
			ktui_draw_text(4, 2 + i, keyw - 4, wel[i].key, KT_TEXT,
				       KT_SURFACE, KT_A_NONE);
			ktui_draw_text(keyw, 2 + i, w - keyw - 2, wel[i].what,
				       KT_MID, KT_SURFACE, KT_A_NONE);
		}
		ktui_draw_hline(1, nwel + 2, w - 2, KT_G_HL, KT_DIM,
				KT_SURFACE);
	}

	/*
	 * The caret is a block rather than a bare column, because a dump has
	 * no cursor: a golden of an empty field could not otherwise be told
	 * from a golden of a missing one.
	 */
	ktui_draw_textf(2, input_y(welcome), w - 4, KT_TEXT, KT_SURFACE,
			KT_A_NONE, "%s%s", query, ktui_glyph[KT_G_FULL]);
	if (!qlen)
		ktui_draw_text(4, input_y(welcome), w - 6, "type to filter",
			       KT_DIM, KT_SURFACE, KT_A_NONE);

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

	/* The row says how to get out and how to move, because this surface
	 * takes the keyboard and a card you cannot dismiss is a card that has
	 * taken the desktop. Scrolling is named only where there is something
	 * off the bottom to scroll to. */
	ktui_hint_if(nrows > rowsv - note, "Up/Down", "scroll");
	/* Named only where the other page exists, which is the same test that
	 * decides whether Tab does anything. */
	ktui_hint_if(prog[0], "Tab",
		     page == PAGE_PROG ? "this desktop's keys" : prog);
	ktui_hint("Esc", ktui_esc_verb(&keys));
	/* Named on the welcome and nowhere else: every other frame was asked
	 * for by the chord this would print. */
	if (welcome_on(welcome) && role_key(WEL_CARD))
		ktui_hint(role_key(WEL_CARD), "this card again");
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	ktui_draw_flush();
}

/* `--dump-cells` — one line per painted cell, colours included. The
 * backend is cells.c's; these two are what the size flag writes. */
static int cap_w = KEYS_COLS, cap_h = KEYS_ROWS;

/* ── main ──────────────────────────────────────────────────────────────── */

int keys_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, dump_cells = 0, first_run = 0, print_card = 0, welcome;

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
		else if (!strcmp(argv[i], "--print"))
			print_card = 1;
		else if (!strcmp(argv[i], "--program") && i + 1 < argc)
			kb_strlcpy(prog, argv[++i], sizeof(prog));
		/* A dump has no keyboard, so the query a golden filters by is
		 * given rather than typed. */
		else if (!strcmp(argv[i], "--dump-query") && i + 1 < argc)
			qlen = snprintf(query, sizeof(query), "%s",
					argv[++i]);
		else {
			fprintf(stderr, "usage: kdos-keys [--first-run] "
					"[--print] [--program NAME]\n"
					"                 "
					"[--dump|--dump-cells] [--dump-size WxH] "
					"[--dump-query TEXT] [--font NAME]\n");
			return 2;
		}
	}

	if (first_run && !dump && !dump_cells && marker_seen())
		return 0;

	const char *on_con = getenv("KDOS_CON");

	sh_chords_load();
	build_cards();

	/*
	 * WHICH PAGE IS SHOWING, and a print run has no screen to read it off.
	 * Interactively the focused window decides — and `--print` returns
	 * before any display is opened, which is the whole reason it works
	 * over ssh, so there is no focused window to ask. `--program NAME` is
	 * how a print run says which page it wants; without it the card prints
	 * this desktop's chords, which is what it is for.
	 */
	if (prog[0]) {
		npkeys = sh_progkeys(prog, pkeys, SH_CHORD_MAX);
		if (npkeys)
			page = PAGE_PROG;
		else if (print_card) {
			fprintf(stderr, "kdos-keys: no keys published for "
					"'%s'\n", prog);
			return 1;
		}
	}
	/*
	 * AFTER build_cards() AND NOT BEFORE. The reader is one of the two
	 * ways this ends up on the built-in table; the other is the card
	 * reading rows it can word none of, which only build_cards() can
	 * discover. Deciding the note first would leave that second case
	 * showing the defaults with nothing saying why.
	 *
	 * WHICH READER FAILED IS WHAT THE NOTE HAS TO SAY, and the desktop
	 * decides that the same way the reader decided which one to run.
	 */
	if (sh_chords_builtin())
		snprintf(parse_note, sizeof(parse_note), "%s",
			 (on_con && *on_con)
			 ? "kdos-con --keys gave nothing - built-in defaults shown"
			 : "rc.xml unreadable - built-in defaults shown");
	build_rows();
	build_welcome();

	if (print_card)
		return print_rows();

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

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = KEYS_COLS,
		.rows = KEYS_ROWS,
		.app_id = "kdos-keys",
		.font = font,
		.keyboard = 1,
		/* A card, not a dialog: clicking on a window puts it away. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-keys: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	int top = 0;

	/* Registered once, and it is the only layer this card has: Escape
	 * clears a query if there is one and otherwise takes the card away. */
	ktui_keys_layer(&keys, "Clear search", query_up, query_clear, NULL);

	/* ASKED AFTER THE DISPLAY IS UP, because that is the only point at
	 * which there is a focused window to ask about — and only when a page
	 * was not already chosen with `--program`. */
	if (!prog[0])
		focused_prog();

	while (!kdisp_should_close()) {
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
		 * A PRINTABLE KEY TYPES; IT DOES NOT CLOSE. The card used to
		 * dismiss on any key but the ones that scroll it, which is the
		 * right rule for a card you only read and the wrong one for a
		 * card you can ask. Escape is the way out — and where a query
		 * has been typed it clears the query first, because a field
		 * whose Escape throws away the whole surface is a field people
		 * lose their place in.
		 */
		/* FIRST, though every key here closes the card in the end: the
		 * contract has to see Esc before `default` does, or a surface
		 * that adopts it later answers Esc twice. */
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			goto done;

		if (ev.key == KT_K_BACKSPACE) {
			if (qlen) {
				query[--qlen] = '\0';
				build_rows();
				top = 0;
			}
			continue;
		}
		/*
		 * TAB IS THE OTHER PAGE, and it is offered only where there is
		 * one: `sh_progkeys_known()` is asked before the reader runs,
		 * so a Tab on a program nobody wrote a reader for does nothing
		 * rather than opening an empty screen. The query is kept
		 * across the switch — somebody who typed `split` wants it on
		 * both pages.
		 */
		if (ev.key == KT_K_TAB && prog[0]) {
			page = page == PAGE_CHORDS ? PAGE_PROG : PAGE_CHORDS;
			if (page == PAGE_PROG && !npkeys)
				npkeys = sh_progkeys(prog, pkeys,
						     SH_CHORD_MAX);
			build_rows();
			top = 0;
			continue;
		}
		if (ev.key >= 0x20 && ev.key < 0x7f) {
			if (qlen < (int)sizeof(query) - 1) {
				query[qlen++] = (char)ev.key;
				query[qlen] = '\0';
				build_rows();
				top = 0;
			}
			continue;
		}

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
			/* A key this card has no answer for is not a reason to
			 * take it away: the printable ones type, the rest are
			 * ignored, and Escape is the way out. */
			break;
		}
	}
done:
	kdisp_shutdown();
	if (welcome)
		marker_write();
	return 0;
}
