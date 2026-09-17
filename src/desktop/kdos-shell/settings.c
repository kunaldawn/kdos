/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settings — the knobs, on the grid
 *
 *   ╔═ Settings ═══════════════════════════════════════════════╗
 *   ║ Appearance │ accent           phosphor            live   ║
 *   ║ Session    │▸crt              ◀ █████░░░░  55 ▶    live   ║
 *   ║ Input      │ crt_fullscreen   on               ▼  live   ║
 *   ║ Apps       │ chrome_font      Terminus:pixel…     login  ║
 *   ╟────────────┴─────────────────────────────────────────────╢
 *   ║ the phosphor shader's strength; 0 is an honest off        ║
 *   ║ ◀▶ change  Enter edit  a apply  Esc close      [ Apply ]  ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * Every knob on this desktop was a text file and nothing else — which is the
 * right storage and the wrong interface for somebody who does not already know
 * the file exists. This is the OS/2-setup lineage: a category list, a form, and
 * no mode the mouse cannot reach.
 *
 * AND EVERY VALUE IS A CONTROL. A number is a `ktui_slider` — press its track,
 * drag it, roll the wheel over it, click an end cap for one step — and a
 * choice is a `ktui_dropdown` that opens under its row. Both were printed
 * strings changed with Left and Right, which made this a settings window
 * somebody holding a mouse could select a row in and do nothing else with.
 *
 * THE CONTROL ANSWERS THE FIRST PRESS. One gesture selects the row and sets
 * the value; a slider that needed the row selecting first would be two
 * movements for one. The drag belongs to the press that began it, so the
 * pointer may leave the column and go on setting the value, and an open
 * dropdown owns the pointer and the keyboard while it is down — it is drawn
 * over the rows beneath it, and a press tested against those rows would pick
 * whatever the list is covering.
 *
 * IT WRITES THE SAME TEXT FILES, AND PRESERVES THEM. comp.conf ships as a
 * commented essay about each key; a settings app that rewrote it would delete
 * the documentation the moment anybody touched a slider. Every write is a
 * line-by-line pass that replaces only the keys it owns and appends the ones
 * the file has never carried — comments, blank lines and unknown keys survive
 * verbatim — and it lands through temp + fsync + rename, because a half-written
 * comp.conf is a session that comes up wrong at the next login.
 *
 * AND EVERY FIELD SAYS WHEN IT TAKES EFFECT. `live` means the SIGHUP this
 * program sends is enough (kdos_conf_reload re-reads it); `login` means the key
 * is a child's argv or is read once at startup and the running session keeps
 * what it has. A settings surface that lied about that would be worse than
 * none: the user would change a thing, see nothing, and change it again.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kicon.h"
#include "kwl.h"
#include "pages.h"
#include "shell.h"

enum { CAT_APPEARANCE = 0, CAT_PANEL, CAT_DESKTOP, CAT_HARDWARE, CAT_SESSION,
       CAT_INPUT, CAT_APPS, CAT_BOXES, CAT_SYSTEM, NCAT };

static const char *const CAT_NAMES[NCAT] = {
	"Appearance", "Panel", "Desktop", "Hardware", "Session", "Input",
	"Apps", "Boxes", "System"
};

/*
 * THE HOME SCREEN IS A GRID OF ICONS, and it is the front door.
 *
 * A sidebar of seven words is a fine way to move between pages once you know
 * what is on them and a poor way to find anything the first time — which is
 * why every control panel worth the name, from System 7's through XP's, opens
 * on a field of labelled pictures rather than on a list. This does the same:
 * a tile per category with its icon, its name and a line saying what is behind
 * it, and the list view is what you land in when you pick one.
 *
 * The icon names are checked against the SHIPPED ATLAS rather than taken from
 * the freedesktop naming spec — Papirus has no `preferences-desktop-theme` and
 * no `applications-system`, and a tile whose picture silently resolves to
 * nothing is the one tile that looks broken. A name that misses still draws:
 * the glyph tier is the fallback, which is what a tty and an install with no
 * artwork get.
 */
static const struct {
	const char *icon;
	const char *blurb;
} CAT_TILE[NCAT] = {
	/* The blurbs are cut to what a tile holds at 80 columns — three
	 * across is 26 cells and the text gets 22 of them. A line that only
	 * fits on a wide screen is a line that is truncated on the shipped
	 * one, which is where it has to read. */
	{ "color-picker",      "accent, CRT, wallpaper" },
	{ "panel",             "the taskbar and tray" },
	{ "desktop",           "icons and workspaces" },
	{ "computer",          "screens, net, sound" },
	{ "system-shutdown",   "idle, lock and power" },
	{ "input-keyboard",    "keyboard and pointer" },
	{ "preferences-other", "which app opens what" },
	{ "package-x-generic",  "environments and packs" },
	/*
	 * SYSTEM is the machine itself — disks, printing, backup, the services
	 * that run without being asked. Most of it is not written yet, and the
	 * category ships anyway with a note saying so: a control centre whose
	 * front door has no door for the machine teaches that the machine is
	 * not reachable from here, and that is the harder thing to unteach.
	 */
	{ "computer-symbolic", "disks, jobs, services" },
};

/* Where the app starts and what Escape steps back to. */
enum { SM_HOME = 0, SM_PAGE };
static int mode = SM_HOME;
static int home_sel;

/*
 * `--page <name>`, so an applet can deep-link into the page that owns it —
 * the panel's battery readout opens Session, its network readout opens the
 * manager itself. The names are the category words, lowercased.
 */
static const char *const PAGE_NAMES[NCAT] = {
	"appearance", "panel", "desktop", "hardware", "session", "input",
	"apps", "boxes", "system"
};

/*
 * ASKED FOR RATHER THAN REPEATED. The palette searches the control centre's
 * pages, and a second copy of this list there would be a page that exists and
 * cannot be found, or a row that opens nothing — the same failure `kdos
 * settings <page>` avoids by passing its word through unchecked.
 */
int sh_settings_pages(const char *const **labels, const char *const **names)
{
	*labels = CAT_NAMES;
	*names = PAGE_NAMES;
	return NCAT;
}

/* Where a row's value is stored. Every one of these is a configuration file
 * this program reads and writes; a row that runs a program instead stores
 * nothing and is ST_NONE. */
enum { ST_NONE = 0, ST_COMP, ST_PANEL, ST_RES, ST_BOX, ST_CON,
       ST_LAUNCH, ST_TERM, ST_MENU };

/* When a change takes effect. */
enum { SC_NONE = 0, SC_LIVE, SC_LOGIN };

/*
 * FT_TOOL is a row that RUNS something rather than storing anything, and it is
 * what makes this a control centre instead of a comp.conf editor. The screens,
 * the network, bluetooth, sound and the cameras are each a whole program with
 * its own protocol; re-implementing a summary of them here would be a second
 * thing to keep true. The row says what it opens and opens it.
 */
/*
 * FT_HEAD IS A SECTION RULE AND NOT A ROW. A page of forty knobs read as one
 * undifferentiated list, and the store a row writes — the console's file, the
 * compositor's, the panel's — is exactly what a person needs to know before
 * they change one. The heading says it once for the rows under it instead of
 * every row's help line saying it again.
 *
 * THE CARET STEPS OVER IT, in whichever direction it was travelling: a heading
 * that could be selected is a row where Enter, Left and Right all do nothing,
 * which is the shape of a control that is broken.
 */
enum { FT_CHOICE = 0, FT_INT, FT_TEXT, FT_NOTE, FT_APP, FT_TOOL, FT_HEAD };

struct row {
	int cat;
	int type;
	int store;
	int scope;
	const char *key;
	const char *label;
	const char *const *choices;
	int nchoices;
	int min, max, step;
	const char *help;
	char val[192];
	char orig[192];
};

static const char *const YESNO[] = { "yes", "no" };
static const char *const TASKBAR[] = { "windows", "fkeys" };
static const char *const ONOFF[] = { "on", "off" };
static const char *const LIDS[] = { "off", "lock", "suspend" };
static const char *const PANELS[] = { "bottom", "top", "off" };
static const char *const TASKLAB[] = { "auto", "yes", "no" };
static const char *const CPUPCT[] = { "core", "machine" };
static const char *const UNITS[] = { "1024", "1000" };
static const char *const TEMPU[] = { "c", "f" };
static const char *const TITLEDBL[] = { "maximise", "lower", "none" };
static const char *const REFRESH[] = { "preferred", "fastest" };
static const char *const MEMACCT[] = { "rss", "pss" };
static const char *const SAVERS[] = { "art", "bounce", "rain", "matrix",
				      "pipes", "starfield", "fire", "clock",
				      "random" };


static struct row rows[] = {
	/* ── Appearance ─────────────────────────────────────────────── */
	/*
	 * THE ACCENT IS PICKED IN THE PICKER AND NOWHERE ELSE. A list of names
	 * here would be a second way to choose one, and the worse of the two:
	 * `kdos-style` draws every scheme in its own colours and repaints the
	 * desktop live as the highlight moves, which a row of words cannot.
	 * This row is the way in, and the key is the program it opens.
	 */
	{ CAT_APPEARANCE, FT_HEAD, ST_NONE, SC_NONE, NULL, "Colour",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_APPEARANCE, FT_TOOL, ST_NONE, SC_NONE, "kdos-style",
	  "Accent…", NULL, 0, 0, 0, 0,
	  "every scheme in its own colours, previewed live; Enter keeps one",
	  "", "" },
	{ CAT_APPEARANCE, FT_HEAD, ST_NONE, SC_NONE, NULL, "The phosphor pass · compositor",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt", "crt",
	  NULL, 0, 0, 100, 5,
	  "the phosphor shader's strength; 0 is an honest off and gives the "
	  "fill rate back",
	  "55", "55" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt_scanlines",
	  "crt_scanlines", NULL, 0, 0, 100, 5,
	  "the scanlines on their own — the part people either love or want gone",
	  "60", "60" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt_curve", "crt_curve",
	  NULL, 0, 0, 100, 5,
	  "tube curvature; nothing is cropped at any value, but it argues with "
	  "a character grid",
	  "0", "0" },
	{ CAT_APPEARANCE, FT_CHOICE, ST_COMP, SC_LIVE, "crt_fullscreen",
	  "crt_fullscreen", ONOFF, 2, 0, 0, 0,
	  "off skips the pass while a window is fullscreen — a film gets direct "
	  "scanout back",
	  "on", "on" },
	{ CAT_APPEARANCE, FT_HEAD, ST_NONE, SC_NONE, NULL, "Chrome · compositor",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LIVE, "wallpaper", "wallpaper",
	  NULL, 0, 0, 0, 0,
	  "the COMPOSITOR's: a PNG, scaled to cover and centred, `none` is an "
	  "honest off. The console's ground is `kdos background`, which is "
	  "character art",
	  "/usr/share/backgrounds/kdos/default-wallpaper.png",
	  "/usr/share/backgrounds/kdos/default-wallpaper.png" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LOGIN, "chrome_font",
	  "chrome_font", NULL, 0, 0, 0, 0,
	  "the chrome's font, as a PIXEL size — Terminus:pixelsize=64 is the "
	  "4K answer",
	  "Terminus:pixelsize=32", "Terminus:pixelsize=32" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LOGIN, "clock_format",
	  "clock_format", NULL, 0, 0, 0, 0,
	  "the panel clock as a strftime format; it reaches the panel on its "
	  "command line",
	  "%H:%M", "%H:%M" },
	/*
	 * ── THE CONSOLE SESSION'S OWN KEYS ───────────────────────────
	 *
	 * `con.conf` was reachable from this window by nothing at all, on the
	 * desktop that is the DEFAULT one: how many workspaces, what the bar
	 * shows, whether a boxed application becomes a window, and both
	 * transparency keys were a text file and a manual page.
	 *
	 * They are shown on both desktops and apply to one, which the help
	 * says on every row rather than the category implying it — a person
	 * on the compositor who changes one and sees nothing has been lied to
	 * by omission.
	 */
	{ CAT_APPEARANCE, FT_HEAD, ST_NONE, SC_NONE, NULL, "Transparency · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_APPEARANCE, FT_INT, ST_CON, SC_LOGIN, "window_opacity",
	  "window_opacity", NULL, 0, 20, 100, 5,
	  "THE CONSOLE DESKTOP: how much of a window's own background it "
	  "keeps. Below 100 it is mixed with whatever it covers; the ink is "
	  "never mixed",
	  "100", "100" },
	{ CAT_APPEARANCE, FT_INT, ST_CON, SC_LOGIN, "panel_opacity",
	  "panel_opacity", NULL, 0, 20, 100, 5,
	  "THE CONSOLE DESKTOP: the same, for a docked bar",
	  "80", "80" },

	{ CAT_APPEARANCE, FT_HEAD, ST_NONE, SC_NONE, NULL, "The console screen",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_APPEARANCE, FT_TEXT, ST_CON, SC_LOGIN, "font", "font",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: the face kdos-view rasterises, as a fontconfig "
	  "name. A name fontconfig cannot resolve falls back to the built-in",
	  "monospace:size=12", "monospace:size=12" },
	/* ── Session ────────────────────────────────────────────────── */
	{ CAT_SESSION, FT_HEAD, ST_NONE, SC_NONE, NULL, "Workspaces and memory · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SESSION, FT_INT, ST_CON, SC_LOGIN, "sessions", "sessions",
	  NULL, 0, 1, 9, 1,
	  "THE CONSOLE DESKTOP: how many workspaces. Nine is the ceiling "
	  "because nine is the last digit Super can reach",
	  "4", "4" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "restore", "restore",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: bring back the windows that were open when "
	  "the session last ended cleanly",
	  "no", "no" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "remember", "remember",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: open each program where its window was last "
	  "time",
	  "yes", "yes" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "restore_scrollback",
	  "restore_scrollback", YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: put the last session's output back on a restored "
	  "terminal. Off separately from `restore`: old output above a fresh "
	  "prompt reads as live",
	  "no", "no" },
	{ CAT_SESSION, FT_HEAD, ST_NONE, SC_NONE, NULL, "Logging in · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "greet", "greet",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: whether tty1 asks who you are. `no` autologins "
	  "the account below, which is what the live medium wants",
	  "no", "no" },
	{ CAT_SESSION, FT_TEXT, ST_CON, SC_LOGIN, "autologin", "autologin",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: which account `greet = no` logs in. An account "
	  "that does not exist leaves the machine reachable only from tty2",
	  "kdos", "kdos" },
	{ CAT_SESSION, FT_INT, ST_CON, SC_LOGIN, "views", "views",
	  NULL, 0, 0, 8, 1,
	  "THE CONSOLE DESKTOP: how many displays may be attached at once; 0 is "
	  "no limit. A view that is refused is told why",
	  "0", "0" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "remote", "remote",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: whether `kdos con forward` may tunnel the view "
	  "socket over ssh. It opens no port — there is no TCP listener here",
	  "no", "no" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "a11y", "a11y",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: keep the kernel's text plane so `brltty` can "
	  "read the screen. It costs the pixel half — no pictures, no font chords",
	  "no", "no" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "speak", "speak",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: start kdos-a11y with the session, which says "
	  "what each widget announces through espeak-ng",
	  "no", "no" },
	{ CAT_SESSION, FT_HEAD, ST_NONE, SC_NONE, NULL, "Idle · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SESSION, FT_INT, ST_CON, SC_LOGIN, "idle_saver", "idle_saver",
	  NULL, 0, 0, 3600, 30,
	  "THE CONSOLE DESKTOP: seconds of no input before the saver covers the "
	  "screen; 0 is never. In a virtual machine all three default to 0",
	  "300", "300" },
	{ CAT_SESSION, FT_INT, ST_CON, SC_LOGIN, "idle_lock", "idle_lock",
	  NULL, 0, 0, 7200, 30,
	  "THE CONSOLE DESKTOP: seconds before it locks, measured from the last "
	  "activity and not from the saver. Activity does NOT unlock",
	  "600", "600" },
	{ CAT_SESSION, FT_INT, ST_CON, SC_LOGIN, "idle_off", "idle_off",
	  NULL, 0, 0, 7200, 30,
	  "THE CONSOLE DESKTOP: seconds before the screen powers down. The lock "
	  "happens first, or a screen would come back showing what was on it",
	  "900", "900" },
	{ CAT_SESSION, FT_CHOICE, ST_CON, SC_LOGIN, "saver_mode", "saver_mode",
	  SAVERS, 9, 0, 0, 0,
	  "THE CONSOLE DESKTOP: which effect the saver draws. `art` is the "
	  "picture in screensaver.txt; `random` picks one at each start",
	  "art", "art" },
	{ CAT_SESSION, FT_HEAD, ST_NONE, SC_NONE, NULL, "Idle and power · compositor",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_dim", "idle_dim",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the dim; 0 never. In a VM all three default to 0 — "
	  "writing one here means it",
	  "300", "300" },
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_lock", "idle_lock",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the lock, measured from the last activity, not from the dim",
	  "600", "600" },
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_off", "idle_off",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the outputs going off; activity powers them back on and "
	  "never unlocks",
	  "900", "900" },
	{ CAT_SESSION, FT_CHOICE, ST_COMP, SC_LIVE, "lid_close", "lid_close",
	  LIDS, 3, 0, 0, 0,
	  "what closing the laptop lid does; in a VM the default is off",
	  "suspend", "suspend" },
	/* ── Panel ──────────────────────────────────────────────────── */
	{ CAT_PANEL, FT_HEAD, ST_NONE, SC_NONE, NULL, "The bar · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_PANEL, FT_CHOICE, ST_CON, SC_LOGIN, "taskbar", "taskbar",
	  TASKBAR, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: what the session's own bottom row shows when "
	  "the shell's panel is not up — the window rows, or Norton "
	  "Commander's F1–F10",
	  "windows", "windows" },
	{ CAT_PANEL, FT_CHOICE, ST_CON, SC_LOGIN, "nowplaying", "nowplaying",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: whether the session's own bar shows what is "
	  "playing. Not read when kdos-shell's panel is docked — that has its "
	  "own mpris widget",
	  "yes", "yes" },
	{ CAT_PANEL, FT_HEAD, ST_NONE, SC_NONE, NULL, "The bar · compositor",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_PANEL, FT_CHOICE, ST_COMP, SC_LOGIN, "panel", "panel",
	  PANELS, 3, 0, 0, 0,
	  "which edge the one taskbar is on, or off entirely. There were two "
	  "panels here; there is one",
	  "bottom", "bottom" },
	{ CAT_PANEL, FT_INT, ST_COMP, SC_LOGIN, "panel_cells", "panel_cells",
	  NULL, 0, 1, 4, 1,
	  "its thickness in CELLS. Two is what makes a task button's icon "
	  "square on a 16x32 cell",
	  "2", "2" },
	{ CAT_PANEL, FT_CHOICE, ST_COMP, SC_LOGIN, "panel_autohide",
	  "panel_autohide", YESNO, 2, 0, 0, 0,
	  "the taskbar retreats to an edge strip until the pointer reaches it",
	  "no", "no" },
	{ CAT_PANEL, FT_TEXT, ST_COMP, SC_LOGIN, "clock_format",
	  "clock_format", NULL, 0, 0, 0, 0,
	  "the clock as a strftime format; it reaches the panel on its command "
	  "line",
	  "%H:%M", "%H:%M" },
	{ CAT_PANEL, FT_TEXT, ST_COMP, SC_LOGIN, "panel_font", "panel_font",
	  NULL, 0, 0, 0, 0,
	  "the bar's own face, as a PIXEL size. Empty follows `chrome_font`",
	  "", "" },
	{ CAT_PANEL, FT_INT, ST_COMP, SC_LOGIN, "panel_margin", "panel_margin",
	  NULL, 0, 0, 64, 2,
	  "pixels of gap between the bar and the edge of the screen",
	  "0", "0" },
	{ CAT_PANEL, FT_INT, ST_COMP, SC_LOGIN, "panel_opacity", "panel_opacity",
	  NULL, 0, 20, 100, 5,
	  "THE COMPOSITOR's bar. Floored at 20: a bar at zero is not "
	  "see-through, it is a bar whose every control is invisible and still "
	  "takes the pointer",
	  "80", "80" },
	/* ── panel.conf, the bar's own file ──────────────────────────── */
	{ CAT_PANEL, FT_TEXT, ST_PANEL, SC_LIVE, "overflow", "overflow",
	  NULL, 0, 0, 0, 0,
	  "widgets that live behind the chevron instead of on the bar, so the "
	  "wing stops changing width",
	  "stutter restart clipboard", "stutter restart clipboard" },
	{ CAT_PANEL, FT_TEXT, ST_PANEL, SC_LIVE, "tray_hide", "tray_hide",
	  NULL, 0, 0, 0, 0,
	  "tray ids not drawn on the bar. fcitx5 is here because its menu is "
	  "dbusmenu, which this tray cannot draw",
	  "fcitx fcitx5 org.fcitx.fcitx5", "fcitx fcitx5 org.fcitx.fcitx5" },
	{ CAT_PANEL, FT_TEXT, ST_PANEL, SC_LIVE, "meters", "meters",
	  NULL, 0, 0, 0, 0,
	  "which charts on the second row, in the order of importance: a "
	  "narrow bar drops them from the right",
	  "cpu ram net", "cpu ram net" },
	{ CAT_PANEL, FT_CHOICE, ST_PANEL, SC_LIVE, "task_labels", "task_labels",
	  TASKLAB, 3, 0, 0, 0,
	  "whether a window button carries its name. `auto` is the bar "
	  "deciding; `no` is a dock",
	  "auto", "auto" },
	{ CAT_PANEL, FT_CHOICE, ST_PANEL, SC_LIVE, "start_label", "start_label",
	  YESNO, 2, 0, 0, 0,
	  "whether the Start button carries the word as well as the mark",
	  "yes", "yes" },
	{ CAT_PANEL, FT_TEXT, ST_PANEL, SC_LIVE, "right", "right",
	  NULL, 0, 0, 0, 0,
	  "the notification area, left to right. An unknown name is reported "
	  "on stderr, never ignored",
	  "pager tray more media privacy mpris clipboard cpu stutter restart "
	  "net volume battery notify clock",
	  "pager tray more media privacy mpris clipboard cpu stutter restart "
	  "net volume battery notify clock" },
	/* ── res.conf, the monitor's own file ────────────────────────
	 * Every row here changes a READING, and a reading measured
	 * differently is a different number — so each says what it changes
	 * rather than restating its key. */
	{ CAT_HARDWARE, FT_HEAD, ST_NONE, SC_NONE, NULL, "How the screen is driven · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_HARDWARE, FT_INT, ST_CON, SC_LOGIN, "buffers", "buffers",
	  NULL, 0, 1, 3, 1,
	  "THE CONSOLE DESKTOP: scanout buffers per screen. Three lets the "
	  "painter work while a flip is in flight; two is the 60-to-30 cliff. A "
	  "ceiling, not a promise",
	  "3", "3" },
	{ CAT_HARDWARE, FT_CHOICE, ST_CON, SC_LOGIN, "refresh", "refresh",
	  REFRESH, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: `preferred` is the mode the monitor's EDID "
	  "certifies; `fastest` is the highest refresh AT THAT SAME SIZE and "
	  "never changes the resolution",
	  "preferred", "preferred" },
	{ CAT_HARDWARE, FT_CHOICE, ST_CON, SC_LOGIN, "tearing", "tearing",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: present each frame as it is composed. Removes up "
	  "to a refresh period of latency and TEARS; silently off where the "
	  "device cannot",
	  "no", "no" },
	{ CAT_HARDWARE, FT_HEAD, ST_NONE, SC_NONE, NULL, "The resource monitor · res.conf",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_HARDWARE, FT_INT, ST_RES, SC_LIVE, "interval", "interval",
	  NULL, 0, 200, 60000, 100,
	  "sampling interval in milliseconds. The floor is 200: a monitor "
	  "sampling faster than that is mostly measuring itself",
	  "1000", "1000" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "cpu_percent",
	  "cpu_percent", CPUPCT, 2, 0, 0, 0,
	  "core: eight busy threads read 800%, which is top's convention. "
	  "machine: the same load reads 100%",
	  "core", "core" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "units", "units",
	  UNITS, 2, 0, 0, 0,
	  "1024 gives KiB/MiB/GiB; 1000 gives kB/MB/GB",
	  "1024", "1024" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "temperature",
	  "temperature", TEMPU, 2, 0, 0, 0,
	  "celsius or fahrenheit, everywhere a sensor is shown",
	  "c", "c" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "kernel_threads",
	  "kernel_threads", YESNO, 2, 0, 0, 0,
	  "show kernel threads in the process table. The footer says how "
	  "many are hidden either way",
	  "no", "no" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "virtual_drives",
	  "virtual_drives", YESNO, 2, 0, 0, 0,
	  "show loop, zram and device-mapper devices on the Drives page",
	  "no", "no" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "virtual_net", "virtual_net",
	  YESNO, 2, 0, 0, 0,
	  "show loopback, bridges and container interfaces on the Network page",
	  "no", "no" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "memory", "memory",
	  MEMACCT, 2, 0, 0, 0,
	  "`rss` counts a shared page against every process holding it; `pss` "
	  "divides it between them, which is the number that adds up",
	  "rss", "rss" },
	{ CAT_HARDWARE, FT_CHOICE, ST_RES, SC_LIVE, "icons", "icons",
	  YESNO, 2, 0, 0, 0,
	  "draw pictures beside the rows. `no` is the glyph tier, which is what "
	  "a --tty view and a braille reader get anyway",
	  "yes", "yes" },
	{ CAT_HARDWARE, FT_TEXT, ST_RES, SC_LIVE, "sort", "sort",
	  NULL, 0, 0, 0, 0,
	  "which column each page sorts on, by that page's own identifier — one "
	  "spelling, from the monitor's registry. A name a page has no column "
	  "for leaves that page on its own default",
	  "cpu", "cpu" },
	{ CAT_HARDWARE, FT_TEXT, ST_RES, SC_LIVE, "columns", "columns",
	  NULL, 0, 0, 0, 0,
	  "which columns a page draws, by the same identifiers. Empty is every "
	  "column that page has",
	  "", "" },

	{ CAT_PANEL, FT_HEAD, ST_NONE, SC_NONE, NULL,
	  "The Start menu · menu.conf",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_PANEL, FT_TEXT, ST_MENU, SC_LIVE, "@toplevel", "@toplevel",
	  NULL, 0, 0, 0, 0,
	  "which system rows stay outside the fold below a hundred columns, as "
	  "the menu draws their labels. A label naming no row promotes nothing "
	  "and reports nothing — a preference file is not a wiring diagram",
	  "Network Sound Displays Terminal",
	  "Network Sound Displays Terminal" },
	{ CAT_PANEL, FT_NOTE, ST_NONE, SC_NONE, NULL, "pinned launchers",
	  NULL, 0, 0, 0, 0,
	  "~/.config/kdos/favorites, one desktop-entry id per line — the same "
	  "file the Start menu pins from",
	  "", "" },

	/* ── Desktop ────────────────────────────────────────────────── */
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL, "The console desktop",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_CHOICE, ST_CON, SC_LOGIN, "embed", "embed",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: whether a graphical application's windows "
	  "become windows here. `no` gives each one a terminal of its own, "
	  "full screen",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_INT, ST_CON, SC_LOGIN, "scrollback", "scrollback",
	  NULL, 0, 0, 100000, 500,
	  "THE CONSOLE DESKTOP: lines a terminal window keeps above the "
	  "screen, per window",
	  "2000", "2000" },
	{ CAT_DESKTOP, FT_CHOICE, ST_CON, SC_LOGIN, "paste_guard", "paste_guard",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: ask before an UNBRACKETED paste carrying a "
	  "newline. With bracketed paste off, a newline EXECUTES at a shell "
	  "prompt; the chord again within five seconds means it",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL, "What the role chords open · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "terminal", "terminal",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: what Super+Return opens, in-process as a window. "
	  "Split into an argument vector with no shell in front of it",
	  "sh", "sh" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "files", "files",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+e. Each role runs in a TERMINAL, so a "
	  "graphical program named here would open in one and draw nothing",
	  "mc", "mc" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "mail", "mail",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+e",
	  "aerc", "aerc" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "browser", "browser",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+b",
	  "lynx", "lynx" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "music", "music",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+u",
	  "rmpc", "rmpc" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "agenda", "agenda",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+g",
	  "ikhal", "ikhal" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "chat", "chat",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+c",
	  "iamb", "iamb" },
	{ CAT_DESKTOP, FT_TEXT, ST_CON, SC_LOGIN, "writing", "writing",
	  NULL, 0, 0, 0, 0,
	  "THE CONSOLE DESKTOP: Super+Shift+w. A chord pointed at a program that "
	  "is not installed starts nothing and drops its row from the key card",
	  "micro", "micro" },
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL, "The graphical desktop",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_CHOICE, ST_COMP, SC_LOGIN, "desktop_icons",
	  "desktop_icons", YESNO, 2, 0, 0, 0,
	  "~/Desktop drawn on the background layer, with Home and Trash pinned "
	  "last",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_CHOICE, ST_COMP, SC_LOGIN, "icons", "icons",
	  YESNO, 2, 0, 0, 0,
	  "pixel icons at all. Off is not a degraded mode — it is what a tty "
	  "draws, and every surface falls back to its glyphs",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_CHOICE, ST_COMP, SC_LOGIN, "slit", "slit",
	  YESNO, 2, 0, 0, 0,
	  "the dockapp column (kdos-slit), from ~/.config/kdos/slit.conf: "
	  "<interval> <width> <command…> per line",
	  "no", "no" },
	{ CAT_DESKTOP, FT_CHOICE, ST_COMP, SC_LOGIN, "window_memory",
	  "window_memory", YESNO, 2, 0, 0, 0,
	  "THE COMPOSITOR: a window opens where that program's window last was. "
	  "The console keeps the same idea as `remember`, in cells",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_CHOICE, ST_COMP, SC_LOGIN, "clipboard", "clipboard",
	  YESNO, 2, 0, 0, 0,
	  "THE COMPOSITOR: run the clipboard manager, which keeps what was "
	  "copied after the program that copied it exits",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL,
	  "The terminal · term.conf",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_TEXT, ST_TERM, SC_LIVE, "shell", "shell",
	  NULL, 0, 0, 0, 0,
	  "what an argument-less kdos-term runs. Split as a desktop entry's "
	  "Exec is — there is no shell in front of it. Empty is $SHELL, then "
	  "/bin/sh",
	  "", "" },
	{ CAT_DESKTOP, FT_TEXT, ST_TERM, SC_LIVE, "font", "font",
	  NULL, 0, 0, 0, 0,
	  "the face a terminal OPENS at, as a fontconfig name. Ctrl+= and "
	  "Ctrl+- step it for one window and Ctrl+0 comes back here. Empty is "
	  "the toolkit's",
	  "", "" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "columns", "columns",
	  NULL, 0, 20, 400, 10,
	  "columns asked for on the first configure",
	  "80", "80" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "rows", "rows",
	  NULL, 0, 5, 200, 4,
	  "rows asked for on the first configure",
	  "24", "24" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "scrollback", "scrollback",
	  NULL, 0, 0, 100000, 500,
	  "lines kept above the screen, per window. A line is a cell array as "
	  "wide as the window, so a wide window costs more per line",
	  "2000", "2000" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "opacity", "opacity",
	  NULL, 0, 20, 100, 5,
	  "how much of the window's own background it keeps. ONLY where a "
	  "compositor is under it; on the console the session composes and "
	  "con.conf's window_opacity is the same request",
	  "100", "100" },
	{ CAT_DESKTOP, FT_CHOICE, ST_TERM, SC_LIVE, "paste_guard",
	  "paste_guard", YESNO, 2, 0, 0, 0,
	  "ask before an UNBRACKETED paste carrying a newline, which EXECUTES "
	  "at a shell prompt. A second attempt within five seconds means it",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_CHOICE, ST_TERM, SC_LIVE, "images", "images",
	  YESNO, 2, 0, 0, 0,
	  "decode pictures. `no` turns the three image protocols off in the "
	  "PARSER, not merely in the drawing",
	  "yes", "yes" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "image_max", "image_max",
	  NULL, 0, 16, 16384, 64,
	  "the cap on one image payload, in kilobytes. It is what bounds a "
	  "payload before anything allocates for it, and a terminal is "
	  "reachable by `cat` on a file somebody sent you",
	  "1024", "1024" },
	{ CAT_DESKTOP, FT_INT, ST_TERM, SC_LIVE, "image_cells", "image_cells",
	  NULL, 0, 8, 1000, 8,
	  "the widest and tallest a picture may be, in cells. It bounds what "
	  "one escape sequence can ask this program to scale",
	  "200", "200" },
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL,
	  "The launcher · launcher.conf",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_CHOICE, ST_LAUNCH, SC_LIVE, "files", "files",
	  YESNO, 2, 0, 0, 0,
	  "add the file index to what Super+d searches. OFF by default, and not "
	  "for speed: a launcher that searched the disk unasked would put your "
	  "filenames on screen in front of whoever is behind you",
	  "no", "no" },
	{ CAT_DESKTOP, FT_HEAD, ST_NONE, SC_NONE, NULL, "Screens",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_DESKTOP, FT_TOOL, ST_NONE, SC_NONE, "kdos-display",
	  "Displays…", NULL, 0, 0, 0, 0,
	  "modes, scale, rotation and the left-to-right order of every screen",
	  "", "" },

	/* ── Hardware ───────────────────────────────────────────────────
	 *
	 * Every row here is a whole program with its own protocol. A summary
	 * of NetworkManager drawn in this window would be a second thing to
	 * keep in agreement with the manager itself, so the row says what it
	 * opens and opens it. */
	{ CAT_HARDWARE, FT_HEAD, ST_NONE, SC_NONE, NULL, "Devices",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_HARDWARE, FT_TOOL, ST_NONE, SC_NONE, "kdos-net",
	  "Network…", NULL, 0, 0, 0, 0,
	  "wifi, wired and VPN over NetworkManager — what `foot -e nmtui` used "
	  "to be",
	  "", "" },
	{ CAT_HARDWARE, FT_TOOL, ST_NONE, SC_NONE, "kdos-bt",
	  "Bluetooth…", NULL, 0, 0, 0, 0,
	  "scan, pair and connect over bluez, with the pairing agent a keyboard "
	  "cannot be paired without",
	  "", "" },
	{ CAT_HARDWARE, FT_TOOL, ST_NONE, SC_NONE, "kdos-audio",
	  "Sound…", NULL, 0, 0, 0, 0,
	  "which card the sound comes out of, and the bluetooth headset that "
	  "will not connect",
	  "", "" },
	{ CAT_HARDWARE, FT_TOOL, ST_NONE, SC_NONE, "kdos-devices",
	  "Cameras and microphones…", NULL, 0, 0, 0, 0,
	  "which cameras exist, what is holding one, and the key that mutes "
	  "every microphone at once",
	  "", "" },
	{ CAT_HARDWARE, FT_TOOL, ST_NONE, SC_NONE, "kdos-rec",
	  "Recording…", NULL, 0, 0, 0, 0,
	  "record from a microphone, watch the level, and transcribe what came "
	  "back when a speech model is present",
	  "", "" },

	/* ── Input ──────────────────────────────────────────────────────
	 *
	 * THE POINTING DEVICES ARE WRITTEN HERE AND THE KEYBOARD IS NOT.
	 *
	 * kdos-view opens the devices and applies every `con.conf` key below to
	 * each one that accepts it, so these rows change what the hand feels.
	 * The KEYBOARD's settings are notes at the end of the page for the
	 * reason this whole surface exists: the layout comes from /etc/keymap
	 * by way of kdos-desktop and every keyboard knob the compositor has is
	 * rc.xml's, and a row that wrote a file no program opens is exactly the
	 * "change a thing, see nothing" this window promised not to be.
	 *
	 * THE DEFAULT SHOWN IS libinput's, so a page nobody has touched
	 * describes what the machine is already doing.
	 */
	{ CAT_INPUT, FT_HEAD, ST_NONE, SC_NONE, NULL, "Pointing devices · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_INPUT, FT_INT, ST_CON, SC_LOGIN, "pointer_speed",
	  "pointer_speed", NULL, 0, -10, 10, 1,
	  "THE CONSOLE DESKTOP: acceleration. 0 is the MIDDLE of the device's "
	  "own range and not an unaccelerated pointer",
	  "0", "0" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "natural_scroll",
	  "natural_scroll", YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: the content follows the fingers rather than "
	  "the view",
	  "no", "no" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "tap_to_click",
	  "tap_to_click", YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: a tap on a touchpad is a click. Off in "
	  "libinput, so this is the line a laptop wants; a mouse ignores it",
	  "no", "no" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "tap_drag", "tap_drag",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: a tap straight after a tap begins a drag that "
	  "lasts while the finger stays down. Only matters once tapping is on",
	  "yes", "yes" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "disable_while_typing",
	  "disable_while_typing", YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: suppress the touchpad while the keyboard is "
	  "being used, which is what stops a palm moving the cursor",
	  "yes", "yes" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "left_handed", "left_handed",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: swap the two main buttons",
	  "no", "no" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "middle_emulation",
	  "middle_emulation", YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: both buttons together are the middle one — the "
	  "only middle button a two-button trackpad has, and what pastes",
	  "no", "no" },

	/*
	 * ── WHAT THE POINTER DOES TO A WINDOW ────────────────────────
	 *
	 * These are the session's rather than the device's, and they are on
	 * this page rather than on Desktop because a person looking for what
	 * their mouse does looks under the mouse.
	 */
	{ CAT_INPUT, FT_HEAD, ST_NONE, SC_NONE, NULL, "What the pointer does to a window · console",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "edge_snap", "edge_snap",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: dragging a window against an edge of the work "
	  "area snaps it there — the same tiles Super+arrow gives",
	  "yes", "yes" },
	{ CAT_INPUT, FT_INT, ST_CON, SC_LOGIN, "snap_zone", "snap_zone",
	  NULL, 0, 1, 8, 1,
	  "THE CONSOLE DESKTOP: how far into the screen, in cells, counts as "
	  "an edge for the drag above",
	  "1", "1" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "title_dblclick",
	  "title_dblclick", TITLEDBL, 3, 0, 0, 0,
	  "THE CONSOLE DESKTOP: what two clicks on a title bar do",
	  "maximise", "maximise" },
	{ CAT_INPUT, FT_INT, ST_CON, SC_LOGIN, "dblclick_ms", "dblclick_ms",
	  NULL, 0, 100, 1000, 50,
	  "THE CONSOLE DESKTOP: how long two clicks may be apart and still be a "
	  "double click on a TITLE BAR. A list and a grid run in a process of "
	  "their own and carry libktui's own interval",
	  "400", "400" },
	{ CAT_INPUT, FT_CHOICE, ST_CON, SC_LOGIN, "panel_wheel", "panel_wheel",
	  YESNO, 2, 0, 0, 0,
	  "THE CONSOLE DESKTOP: the wheel over the bottom row steps to the "
	  "next occupied workspace",
	  "yes", "yes" },

	{ CAT_INPUT, FT_HEAD, ST_NONE, SC_NONE, NULL, "The keyboard",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_INPUT, FT_TOOL, ST_NONE, SC_NONE, "kdos-keys",
	  "Keyboard shortcuts…", NULL, 0, 0, 0, 0,
	  "every chord this desktop has, read from whichever one you are on — "
	  "rc.xml under the compositor, `kdos-con --keys` on the console. It is "
	  "a card and not an editor: the chords are keys.conf's and rc.xml's",
	  "", "" },
	{ CAT_INPUT, FT_NOTE, ST_NONE, SC_NONE, NULL, "where layout comes from",
	  NULL, 0, 0, 0, 0,
	  "/etc/keymap is the console map; kdos-desktop maps it to "
	  "XKB_DEFAULT_LAYOUT at login, and rc.xml's <keyboard><layout> "
	  "overrides both inside the compositor",
	  "", "" },
	{ CAT_INPUT, FT_NOTE, ST_NONE, SC_NONE, NULL, "where key repeat comes from",
	  NULL, 0, 0, 0, 0,
	  "rc.xml's <keyboard><repeatRate> and <repeatDelay>; kdos-comp reads "
	  "them at startup and on the Reconfigure a SIGHUP is",
	  "", "" },

	/*
	 * THE MACHINE. Every row here opens a whole program, for the reason
	 * CAT_HARDWARE's rows do: a summary drawn in this window would be a
	 * second thing to keep in agreement with the thing itself.
	 *
	 * THE CATEGORY SHIPS WITH WHAT EXISTS AND SAYS WHAT DOES NOT. Disks,
	 * printing, backup and services are not written; a row that opened
	 * nothing would be worse than no row, and no category at all would
	 * teach that the machine is not reachable from here.
	 */
	{ CAT_SYSTEM, FT_HEAD, ST_NONE, SC_NONE, NULL, "The machine",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-res",
	  "Resources…", NULL, 0, 0, 0, 0,
	  "processes, memory, disks and the network, live — what `top` and "
	  "`df` say, in one place",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-energy",
	  "Power…", NULL, 0, 0, 0, 0,
	  "the battery, what is draining it, and the governor",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-disks",
	  "Disks…", NULL, 0, 0, 0, 0,
	  "what is attached, what is mounted and what is full. Every privileged "
	  "step is a kdos-mountd verb; partitioning is `cfdisk` in a terminal",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-print",
	  "Printers…", NULL, 0, 0, 0, 0,
	  "queues and jobs, over `lpstat` and `lpadmin`. The `lpadmin` group is "
	  "CUPS' own authority, so this needs no daemon of ours in front of it",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-users",
	  "Accounts…", NULL, 0, 0, 0, 0,
	  "who has an account on this machine. It reads /etc/passwd and writes "
	  "only the autologin, through the same wheel-gated daemon",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-time",
	  "Date and time…", NULL, 0, 0, 0, 0,
	  "the zone, from tzdata's own zone1970.tab. Setting it is a kdos-powerd "
	  "verb, because /etc/localtime is root's",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-connect",
	  "Sync…", NULL, 0, 0, 0, 0,
	  "calendars and address books, through vdirsyncer",
	  "", "" },
	{ CAT_SYSTEM, FT_HEAD, ST_NONE, SC_NONE, NULL, "Security and updates",
	  NULL, 0, 0, 0, 0, "", "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-firewall",
	  "Firewall…", NULL, 0, 0, 0, 0,
	  "which services answer the network. It carries no table of ports: "
	  "kdos-powerd owns the names, and a client that could name a port "
	  "could open any port",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-update",
	  "Updates…", NULL, 0, 0, 0, 0,
	  "what is behind and what is vulnerable, from `kdos update check "
	  "--json` and `kdos cve --json` rather than a second comparison",
	  "", "" },
	{ CAT_SYSTEM, FT_TOOL, ST_NONE, SC_NONE, "kdos-backup",
	  "Backup…", NULL, 0, 0, 0, 0,
	  "what is in the restic repository. It restores nothing: `restic "
	  "restore` is the operation you do once under pressure and it wants "
	  "the full command",
	  "", "" },
	{ CAT_SYSTEM, FT_NOTE, ST_NONE, SC_NONE, NULL, "what is not here",
	  NULL, 0, 0, 0, 0,
	  "the service list is not written; `kdos doctor` and `kdos pkg` do that "
	  "work today. /etc/kdos/zram.conf, packd.conf and the timers are root's "
	  "and are edited there",
	  "", "" },
};

#define NROWS ((int)(sizeof(rows) / sizeof(rows[0])))

/* The Apps category: what ~/.config/mimeapps.list currently says, one row per
 * declared default. Re-read on the idle tick, because the chooser that changes
 * one of them is a SEPARATE process and cannot tell us it wrote. */
#define MAX_APPS 64

struct app_row {
	char mime[128];
	char id[160];
};

static struct app_row apps[MAX_APPS];
static int napps;

/*
 * ── Boxes ──────────────────────────────────────────────────────────────
 *
 * THE CONFIGURATION HALF, where configuration already lives. `kdos-box list`
 * is the runtime half — what is running, what it has written — and belongs at
 * a prompt and on kdos-res's Boxes page. What was missing was the other one:
 * every property of a box could only be set by knowing that
 * `~/.config/kdos/boxes/<name>.conf` existed.
 *
 * THIS PAGE NEVER WRITES A PROFILE. It reads them (they are `key = value`,
 * the shape every file this program touches uses) and it writes by running
 * `kdos-box create` and `kdos-box profile`, which is the same writer a person
 * at a prompt reaches — so a box created here is byte-identical to one created
 * by hand with the same answers, rather than merely similar. `kdos-box`
 * additionally KNOWS things this page must not re-derive: which keys podman
 * can enforce, which cannot, and that a namespace change needs the box
 * recreating.
 *
 * Three states. The list is the boxes; opening one shows its keys; `+ new box`
 * shows the same keys with nothing created yet, and the button reads Create.
 * That last distinction is not cosmetic — namespaces and volumes apply at
 * CREATE time, so a network setting chosen before the box exists takes effect
 * and the same setting chosen afterwards needs a recreate.
 */
enum { BOX_LIST = 0, BOX_EDIT, BOX_NEW };

#define MAX_BOXES 64

struct box_row {
	char name[64];
	char base[64];
	char persist[16];
	char export[12];
	int  described;		/* a profile exists; podman may not know it */
};

static struct box_row boxes[MAX_BOXES];
static int nboxes;
static int box_mode = BOX_LIST;
static char box_cur[64];

static const char *const PERSISTS[] = { "persistent", "ephemeral", "frozen" };
static const char *const NETS[] = { "host", "private", "none" };
static const char *const SHARED[] = { "shared", "private" };
static const char *const EXPORTS[] = { "manual", "auto" };
static const char *const ACCENTS_OR_SESSION[] = { "session", "phosphor",
						  "amber", "ice", "bone" };

/*
 * The keys, in the order `profile_save` writes them, so reading this page and
 * reading the file are the same experience. `name` is first and is editable
 * only while creating: renaming a box is a different operation from
 * configuring one and `kdos-box` has no verb for it.
 */
static struct row boxrows[] = {
	{ CAT_BOXES, FT_TEXT, ST_BOX, SC_LOGIN, "name", "name",
	  NULL, 0, 0, 0, 0,
	  "[A-Za-z0-9._-]; it is the container's name, the terminal's title "
	  "and what kdos stutter, kdos-energyd and kdos-oomd call it",
	  "", "" },
	{ CAT_BOXES, FT_TEXT, ST_BOX, SC_LOGIN, "base", "base",
	  NULL, 0, 0, 0, 0,
	  "pack:<id> and box:<name> are offline; image:<ref> FETCHES UNSIGNED "
	  "CONTENT from somebody else's registry",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LIVE, "accent", "accent",
	  ACCENTS_OR_SESSION, 5, 0, 0, 0,
	  "the terminal `kdos-box enter` opens wears it, and the window frame "
	  "carries a chip of it when it is not the session's",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "persistence", "persistence",
	  PERSISTS, 3, 0, 0, 0,
	  "persistent keeps what the box writes; ephemeral puts the upper on "
	  "tmpfs; frozen discards it — the three keys an app box differs by",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "network", "network",
	  NETS, 3, 0, 0, 0,
	  "podman --unshare-netns / --network none. Applied at CREATE time: a "
	  "namespace cannot be re-flagged on a live container",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "ipc", "ipc",
	  SHARED, 2, 0, 0, 0,
	  "podman --unshare-ipc", "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "devices", "devices",
	  SHARED, 2, 0, 0, 0,
	  "podman --unshare-devsys — and this is the key audio and gpu ride "
	  "on: there is no flag that grants a speaker and denies a camera",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "processes", "processes",
	  SHARED, 2, 0, 0, 0,
	  "podman --unshare-process", "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "home", "home",
	  SHARED, 2, 0, 0, 0,
	  "a private $HOME, not the user's. Every theme this desktop writes "
	  "for alien apps goes through $HOME, so a private one is unthemed",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "wayland", "wayland",
	  YESNO, 2, 0, 0, 0,
	  "tagged through kdos-boxsock, which is what denies it screen capture "
	  "and the clipboard managers; no means no display at all",
	  "", "" },
	{ CAT_BOXES, FT_CHOICE, ST_BOX, SC_LOGIN, "export", "export",
	  EXPORTS, 2, 0, 0, 0,
	  "auto turns the box's applications into host launchers when it is "
	  "created; manual is `kdos-box export <box> <app>`",
	  "", "" },
	{ CAT_BOXES, FT_TEXT, ST_BOX, SC_LIVE, "memory", "memory",
	  NULL, 0, 0, 0, 0,
	  "enforced by KDOS-OOMD, not by podman: rootless podman with no "
	  "systemd usually has no cgroup delegation, so --memory does nothing",
	  "", "" },
	{ CAT_BOXES, FT_TEXT, ST_BOX, SC_LOGIN, "cpus", "cpus",
	  NULL, 0, 0, 0, 0,
	  "podman --cpus, and it needs the same cgroup delegation --memory "
	  "does; empty is no limit", "", "" },
	{ CAT_BOXES, FT_INT, ST_BOX, SC_LIVE, "pids", "pids",
	  NULL, 0, 0, 8192, 64,
	  "podman --pids-limit; 0 is unlimited", "", "" },
	{ CAT_BOXES, FT_INT, ST_BOX, SC_LIVE, "autostop", "autostop",
	  NULL, 0, 0, 3600, 60,
	  "idle seconds before `kdos-box gc` stops it — and gc asks the "
	  "compositor first: a box with a mapped window is not idle. 0 is off",
	  "", "" },
};

#define NBOXROWS ((int)(sizeof(boxrows) / sizeof(boxrows[0])))

/* Set a box row's value by key, so the loader below does not care what order
 * the file is in. */
static void boxrow_set(const char *key, const char *val)
{
	for (int i = 0; i < NBOXROWS; i++)
		if (!strcmp(boxrows[i].key, key)) {
			kb_strlcpy(boxrows[i].val, val, sizeof(boxrows[i].val));
			return;
		}
}

static const char *boxrow_get(const char *key)
{
	for (int i = 0; i < NBOXROWS; i++)
		if (!strcmp(boxrows[i].key, key))
			return boxrows[i].val;
	return "";
}

static int cat, sel, top, pane;		/* pane 0 = categories, 1 = fields */
/* The viewport follows the SELECTION only when the selection is what moved
 * — see kch_list_wheel. */
static int sel_follow = 1;
static char note[192];
static int editing, quit_armed;
/* A press that landed on a slider's track owns every drag behind it, so the
 * pointer may leave the column and go on setting the value. Cleared by the
 * release, like any other capture. */
static int drag_slider;

/*
 * THE EVENT THE EDITING FIELD WILL SEE.
 *
 * `ktui_input` is a frame control: it reads the event `ktui_frame_begin` was
 * given, so a key meant for the field has to reach the DRAW rather than be
 * spent in the loop. This surface hand-rolled a buffer instead — backspace and
 * printable bytes, no caret to move, no paste, and a UTF-8 value cut in half by
 * one backspace. Handing the event across is what lets the one field in the
 * toolkit be the field here too.
 *
 * Cleared by the draw that spends it: an event left standing would be applied
 * again on the next repaint, which is one keystroke typed twice.
 */
static KtuiEvent field_ev;
static char edit_buf[256];

/*
 * THREE RUNGS, INNERMOST LAST: the page under the grid, a box profile under
 * the box list, the field editor under everything. Every predicate is guarded
 * on `mode == SM_PAGE` because a right-click back to the grid leaves
 * `box_mode` and `editing` where they were — an unguarded rung would then
 * unwind stale state instead of arming the unsaved-changes question, which is
 * exactly the defect a declared ladder exists to remove.
 */
static KtuiKeys keys;

static int page_up(void *user)
{
	(void)user;
	return mode == SM_PAGE;
}

static void page_close(void *user)
{
	(void)user;
	/* Back to the grid, not out of the program. Edits are held in `rows[]`
	 * and Apply is still one key away, so stepping back loses nothing —
	 * and the unsaved-changes guard stays at the single exit, on the home
	 * screen, rather than firing every time somebody backs out of a page
	 * they only wanted to look at. */
	mode = SM_HOME;
	home_sel = cat;
	quit_armed = 0;
}

static int boxprof_up(void *user)
{
	(void)user;
	return mode == SM_PAGE && cat == CAT_BOXES && box_mode != BOX_LIST;
}

static void boxprof_close(void *user)
{
	(void)user;
	box_mode = BOX_LIST;
	box_cur[0] = '\0';
	sel = top = 0;
	note[0] = '\0';
}

static int edit_up(void *user)
{
	(void)user;
	return mode == SM_PAGE && editing;
}

static void edit_cancel(void *user)
{
	(void)user;
	editing = 0;
}

/* ── the files ─────────────────────────────────────────────────────────── */

/*
 * `con.conf` IS NOT IN `~/.config/kdos`. The console session keeps its own
 * directory — `kdos-con/` — beside its `keys.conf` and its layouts, so the
 * leaf helper below cannot reach it and a second one says where it is rather
 * than growing a special case into the first.
 *
 * EVERY CONSOLE KEY IS `login` AND NOT `live`. `kcon_conf_*` reads the file
 * once, on the first lookup, and holds the answer for the life of the session:
 * a SIGHUP retints the desktop and re-reads nothing here, so a row that
 * promised `live` would be a row that lies about what it just did.
 */
static void con_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%.400s/kdos-con/con.conf", cfg);
	else
		snprintf(out, n, "%.400s/.config/kdos-con/con.conf",
			 kb_home_dir());
}

static void cfg_path(const char *leaf, char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%.400s/kdos/%.60s", cfg, leaf);
	else
		snprintf(out, n, "%.400s/.config/kdos/%.60s", kb_home_dir(),
			 leaf);
}

/*
 * `key = value`, the shape comp.conf uses. A commented line is a comment:
 * comp.conf documents every key as `#crt = 55`, and reading those as settings
 * would show a fresh install a screen of values nothing is using.
 */
static void load_kv(const char *path, int store)
{
	char *data = kb_read_all(path, NULL);

	if (!data)
		return;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';

		char *k = line;
		while (*k == ' ' || *k == '\t')
			k++;
		if (!*k || *k == '#')
			continue;
		char *eq = strchr(k, '=');
		if (!eq)
			continue;
		char *v = eq + 1;
		while (eq > k && (eq[-1] == ' ' || eq[-1] == '\t'))
			eq--;
		*eq = '\0';
		while (*v == ' ' || *v == '\t')
			v++;
		char *e = v + strlen(v);
		while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
			*--e = '\0';

		for (int i = 0; i < NROWS; i++) {
			if (rows[i].store != store || !rows[i].key ||
			    strcmp(rows[i].key, k))
				continue;
			kb_strlcpy(rows[i].val, v, sizeof(rows[i].val));
			kb_strlcpy(rows[i].orig, v, sizeof(rows[i].orig));
		}
	}
	free(data);
}

static void load_apps(void)
{
	char path[700];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char *data;
	int in_sec = 0;

	napps = 0;
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfg);
	else
		snprintf(path, sizeof(path), "%.500s/.config/mimeapps.list",
			 kb_home_dir());
	data = kb_read_all(path, NULL);
	if (!data)
		return;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		if (line[0] == '[') {
			in_sec = !strncmp(line, "[Default Applications]", 22);
			continue;
		}
		if (!in_sec || !line[0] || line[0] == '#')
			continue;
		char *eq = strchr(line, '=');
		if (!eq || napps >= MAX_APPS)
			continue;
		*eq = '\0';
		kb_strlcpy(apps[napps].mime, line, sizeof(apps[0].mime));
		kb_strlcpy(apps[napps].id, eq + 1, sizeof(apps[0].id));
		napps++;
	}
	free(data);
}

/* ── the boxes ─────────────────────────────────────────────────────────── */

static void box_conf_path(const char *name, char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%.400s/kdos/boxes/%.63s.conf", cfg, name);
	else
		snprintf(out, n, "%.400s/.config/kdos/boxes/%.63s.conf",
			 kb_home_dir(), name);
}

/* One value out of a box profile, or "". */
static void box_key(const char *name, const char *key, char *out, size_t n)
{
	char path[700], *data;

	out[0] = '\0';
	box_conf_path(name, path, sizeof(path));
	data = kb_read_all(path, NULL);
	if (!data)
		return;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		char *eq;
		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';
		for (char *e = line + strlen(line); e > line &&
		     (e[-1] == ' ' || e[-1] == '\t'); )
			*--e = '\0';
		if (strcmp(line, key))
			continue;
		eq++;
		while (*eq == ' ' || *eq == '\t')
			eq++;
		eq[strcspn(eq, "\r")] = '\0';
		kb_strlcpy(out, eq, n);
		break;
	}
	free(data);
}

/*
 * THE PROFILES ARE THE LIST, and that is what makes this the configuration
 * half. `kdos-box list` additionally asks podman which of them exist and what
 * state they are in — a fork and a container-engine round trip, for an answer
 * this page does not act on. A box that is described and not running is still
 * a row here, for the same reason it is one on kdos-res's Boxes page.
 */
static void load_boxes(void)
{
	char dir[700];
	char **names;
	const char *cfg = getenv("XDG_CONFIG_HOME");

	nboxes = 0;
	if (cfg && *cfg)
		snprintf(dir, sizeof(dir), "%.500s/kdos/boxes", cfg);
	else
		snprintf(dir, sizeof(dir), "%.500s/.config/kdos/boxes",
			 kb_home_dir());
	/*
	 * THE DEFAULT BOX IS ALWAYS A ROW, profile or not. Every alien app on
	 * this machine runs in `kdos-apps` and it is the one box nobody ever
	 * had to describe, so a page listing only the profiles showed NO BOXES
	 * on a machine with one running — and the one you would most want to
	 * give a memory budget or an accent to. `kdos-box profile` loads the
	 * defaults for a name with no file and writes one on the first change,
	 * so opening this row needs nothing more than the name.
	 */
	{
		struct box_row *b = &boxes[nboxes++];
		memset(b, 0, sizeof(*b));
		kb_strlcpy(b->name, "kdos-apps", sizeof(b->name));
		box_key(b->name, "base", b->base, sizeof(b->base));
		box_key(b->name, "persistence", b->persist, sizeof(b->persist));
		box_key(b->name, "export", b->export, sizeof(b->export));
		b->described = b->persist[0] != '\0';
		if (!b->base[0])
			kb_strlcpy(b->base, "(the image lane)", sizeof(b->base));
		if (!b->persist[0])
			kb_strlcpy(b->persist, "persistent", sizeof(b->persist));
		if (!b->export[0])
			kb_strlcpy(b->export, "manual", sizeof(b->export));
	}

	names = kb_listdir(dir, NULL);
	if (!names)
		return;
	for (char **nm = names; *nm && nboxes < MAX_BOXES; nm++) {
		size_t l = strlen(*nm);
		struct box_row *b;

		if (l < 6 || strcmp(*nm + l - 5, ".conf"))
			continue;
		if (l == 15 && !strncmp(*nm, "kdos-apps.conf", 14))
			continue;	/* already the row above */
		b = &boxes[nboxes++];
		memset(b, 0, sizeof(*b));
		kb_strlcpy(b->name, *nm, l - 4);
		b->described = 1;
		box_key(b->name, "base", b->base, sizeof(b->base));
		box_key(b->name, "persistence", b->persist, sizeof(b->persist));
		box_key(b->name, "export", b->export, sizeof(b->export));
		if (!b->base[0])
			kb_strlcpy(b->base, "(the image lane)", sizeof(b->base));
		if (!b->persist[0])
			kb_strlcpy(b->persist, "persistent", sizeof(b->persist));
		if (!b->export[0])
			kb_strlcpy(b->export, "manual", sizeof(b->export));
	}
	kb_strv_free(names);
}

/*
 * Fill `boxrows` from one profile, or from the defaults when creating. The
 * DEFAULTS ARE STATED HERE and they are `profile_defaults`': an unprofiled
 * box behaves exactly as a plain `distrobox create` does, so a page that
 * offered anything else as its starting point would create boxes nobody
 * asked for.
 */
static void box_open(const char *name, int creating)
{
	static const char *const DEF[][2] = {
		{ "name", "" }, { "base", "" }, { "accent", "session" },
		{ "persistence", "persistent" }, { "network", "host" },
		{ "ipc", "shared" }, { "devices", "shared" },
		{ "processes", "shared" }, { "home", "shared" },
		{ "wayland", "yes" }, { "export", "manual" },
		{ "memory", "" }, { "cpus", "" }, { "pids", "0" },
		{ "autostop", "0" },
	};

	for (size_t i = 0; i < sizeof(DEF) / sizeof(DEF[0]); i++)
		boxrow_set(DEF[i][0], DEF[i][1]);

	if (!creating) {
		for (int i = 0; i < NBOXROWS; i++) {
			char v[192];
			if (!strcmp(boxrows[i].key, "name"))
				continue;
			box_key(name, boxrows[i].key, v, sizeof(v));
			if (v[0])
				kb_strlcpy(boxrows[i].val, v,
					   sizeof(boxrows[i].val));
		}
		boxrow_set("name", name);
		/* `autostop` is written as `120s`; the row is a number. */
		for (int i = 0; i < NBOXROWS; i++) {
			size_t l = strlen(boxrows[i].val);
			if (strcmp(boxrows[i].key, "autostop"))
				continue;
			if (l && boxrows[i].val[l - 1] == 's')
				boxrows[i].val[l - 1] = '\0';
		}
	}
	kb_strlcpy(box_cur, creating ? "" : name, sizeof(box_cur));
	box_mode = creating ? BOX_NEW : BOX_EDIT;
	for (int i = 0; i < NBOXROWS; i++)
		kb_strlcpy(boxrows[i].orig, boxrows[i].val,
			   sizeof(boxrows[i].orig));
}

static int box_dirty(void)
{
	int n = 0;

	for (int i = 0; i < NBOXROWS; i++)
		if (strcmp(boxrows[i].val, boxrows[i].orig))
			n++;
	return n;
}

/*
 * Write by RUNNING `kdos-box`, never by writing the file. That is what makes
 * the checkpoint's claim true rather than approximately true: a box created
 * here and a box created at a prompt with the same answers are the same
 * bytes, because the same program wrote both.
 *
 * Only the keys that CHANGED are passed. `kdos-box profile` rewrites the whole
 * file from what it loaded, so a key left out keeps its value; passing all
 * fifteen every time would additionally rewrite the two — `image` and any
 * unknown key somebody added — that this page does not show.
 */
static int box_write(void)
{
	KbArgv a = {0};
	const char *name = box_mode == BOX_NEW ? boxrow_get("name") : box_cur;
	int changed = 0, rc;

	if (!name || !*name) {
		snprintf(note, sizeof(note), "a box needs a name");
		return -1;
	}
	for (const char *c = name; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
		      (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' ||
		      *c == '_')) {
			snprintf(note, sizeof(note),
				 "a box name is [A-Za-z0-9._-]");
			return -1;
		}

	kb_argv_add(&a, "kdos-box");
	kb_argv_add(&a, box_mode == BOX_NEW ? "create" : "profile");
	kb_argv_add(&a, name);
	for (int i = 0; i < NBOXROWS; i++) {
		if (!strcmp(boxrows[i].key, "name"))
			continue;
		if (box_mode != BOX_NEW &&
		    !strcmp(boxrows[i].val, boxrows[i].orig))
			continue;
		if (box_mode == BOX_NEW && !boxrows[i].val[0])
			continue;
		/* `autostop` round-trips through the file as seconds with an
		 * `s`, which is what `kdos-box` parses. */
		if (!strcmp(boxrows[i].key, "autostop"))
			kb_argv_addf(&a, "autostop=%ss", boxrows[i].val);
		else
			kb_argv_addf(&a, "%s=%s", boxrows[i].key,
				     boxrows[i].val);
		changed++;
	}
	kb_argv_end(&a);
	if (!changed && box_mode != BOX_NEW) {
		snprintf(note, sizeof(note), "nothing to apply");
		return 0;
	}
	rc = kb_run(&a);
	if (rc != 0) {
		snprintf(note, sizeof(note),
			 "kdos-box exited %d — run it at a prompt to see why",
			 rc);
		return -1;
	}
	for (int i = 0; i < NBOXROWS; i++)
		kb_strlcpy(boxrows[i].orig, boxrows[i].val,
			   sizeof(boxrows[i].orig));
	load_boxes();
	if (box_mode == BOX_NEW) {
		kb_strlcpy(box_cur, name, sizeof(box_cur));
		box_mode = BOX_EDIT;
		snprintf(note, sizeof(note), "%.40s created", name);
	} else {
		snprintf(note, sizeof(note),
			 "%.40s written — a namespace or volume change needs "
			 "`kdos-box remove %.20s` and creating it again",
			 name, name);
	}
	return 0;
}

static void load_all(void)
{
	char path[700];

	cfg_path("comp.conf", path, sizeof(path));
	load_kv(path, ST_COMP);
	cfg_path("panel.conf", path, sizeof(path));
	load_kv(path, ST_PANEL);
	cfg_path("res.conf", path, sizeof(path));
	load_kv(path, ST_RES);
	cfg_path("launcher.conf", path, sizeof(path));
	load_kv(path, ST_LAUNCH);
	cfg_path("term.conf", path, sizeof(path));
	load_kv(path, ST_TERM);
	/*
	 * THE USER'S HALF ONLY. menu.conf is merged system-then-user by every
	 * reader, and the system file is a ROUTE TABLE: loading it here would
	 * put its routes in front of a writer that rewrites what it loaded,
	 * and this page owns one `@` setting in it and no route at all.
	 */
	cfg_path("menu.conf", path, sizeof(path));
	load_kv(path, ST_MENU);
	/*
	 * THE SYSTEM FILE FIRST AND THE HOME FILE OVER IT, which is the order
	 * the session itself reads them in. con.conf is the one store whose
	 * system copy is the answer on a machine nobody has edited — /etc/skel
	 * ships no ~/.config/kdos-con/con.conf — so a page loaded from the
	 * home file alone showed this program's built-in defaults as though
	 * they were the machine's, and every row read `*` the moment it was
	 * touched.
	 *
	 * What is WRITTEN is still the home file: the rows that changed go
	 * there, and /etc stays the administrator's.
	 */
	load_kv("/etc/kdos/con.conf", ST_CON);
	con_path(path, sizeof(path));
	load_kv(path, ST_CON);
	load_boxes();
	load_apps();
}

/* ── writing ───────────────────────────────────────────────────────────── */

/*
 * Rewrite `path`, replacing every key this store owns that has changed and
 * appending the ones the file never carried. Everything else — comments, blank
 * lines, keys nothing here knows about — is copied byte for byte.
 *
 * A commented `#crt = 55` is left alone deliberately: it is the file's
 * documentation of the default, the appended `crt = 40` below it wins, and the
 * two together read as "the default was this, I chose that".
 */
static int write_kv(const char *path, int store)
{
	char tmp[720], dir[700];
	char *old = kb_read_all(path, NULL);
	KbBuf out = {0};
	int done[NROWS];
	int rc = -1;

	/* A file that EXISTS and did not read is not an empty one. kb_read_all
	 * answers NULL to ENOENT and to EACCES alike, and taking the second for
	 * "no file yet" would replace a comp.conf somebody root-owns by hand
	 * with the two lines this program changed. */
	if (!old && access(path, F_OK) == 0)
		return -1;

	for (int i = 0; i < NROWS; i++)
		done[i] = 0;

	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}

	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		size_t len = (size_t)(next - line);

		const char *t = line;
		while (*t == ' ' || *t == '\t')
			t++;
		int hit = -1;
		for (int i = 0; i < NROWS && hit < 0; i++) {
			if (rows[i].store != store || !rows[i].key ||
			    !strcmp(rows[i].val, rows[i].orig))
				continue;
			size_t kl = strlen(rows[i].key);
			if (!strncmp(t, rows[i].key, kl) &&
			    (t[kl] == ' ' || t[kl] == '\t' || t[kl] == '='))
				hit = i;
		}
		if (hit >= 0) {
			kb_buf_printf(&out, "%s = %s\n", rows[hit].key,
				      rows[hit].val);
			done[hit] = 1;
		} else {
			kb_buf_add(&out, line, len);
		}
	}

	for (int i = 0; i < NROWS; i++) {
		if (rows[i].store != store || !rows[i].key || done[i] ||
		    !strcmp(rows[i].val, rows[i].orig))
			continue;
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		kb_buf_printf(&out, "%s = %s\n", rows[i].key, rows[i].val);
	}

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *f = fopen(tmp, "w");
	if (f) {
		int ok = out.n == 0 || fwrite(out.p, 1, out.n, f) == out.n;
		if (ok && fflush(f) == 0 && fsync(fileno(f)) == 0 &&
		    fclose(f) == 0) {
			f = NULL;
			if (rename(tmp, path) == 0) {
				rc = 0;
				/* The directory entry is what the rename
				 * created, and it has to reach the disk too —
				 * the fsync people leave out. */
				if (slash) {
					int d = open(dir, O_RDONLY);
					if (d >= 0) {
						fsync(d);
						close(d);
					}
				}
			}
		}
		if (f)
			fclose(f);
		if (rc != 0)
			remove(tmp);
	}
	kb_buf_free(&out);
	free(old);
	return rc;
}

/*
 * The signal `kdos theme` already sends, for the same reason and by the same
 * route: pkill -x, exact, because `kdos-comp` must not also match
 * `kdos-desktop-start` — an unhandled SIGHUP kills a shell, and those two
 * /bin/sh scripts own the session.
 */
static void sighup(const char *name)
{
	KbArgv a = {0};

	if (!kb_have_prog("pkill"))
		return;
	kb_argv_add(&a, "pkill");
	kb_argv_add(&a, "-x");
	kb_argv_add(&a, "-HUP");
	kb_argv_add(&a, name);
	kb_argv_end(&a);
	kb_run(&a);		/* no session running is not an error */
}

static int dirty_count(int store)
{
	int n = 0;

	for (int i = 0; i < NROWS; i++)
		if (rows[i].store == store && strcmp(rows[i].val, rows[i].orig))
			n++;
	return n;
}

/*
 * EVERY STORE, WHICH IS WHAT THE COUNTER AND THE QUIT GUARD MEAN.
 *
 * Both asked ST_COMP alone. A change to the panel's file, the monitor's or the
 * console session's therefore showed `0 pending` on the Apply button and was
 * discarded by a single Escape without the guard saying anything — which is
 * the one thing that guard exists to stop.
 */
static int dirty_any(void)
{
	int n = 0;

	for (int i = 0; i < NROWS; i++)
		if (rows[i].store != ST_NONE &&
		    strcmp(rows[i].val, rows[i].orig))
			n++;
	return n;
}

static void apply(void)
{
	char path[700];
	int comp = dirty_count(ST_COMP);
	int panel = dirty_count(ST_PANEL), res = dirty_count(ST_RES);
	int con = dirty_count(ST_CON), launch = dirty_count(ST_LAUNCH);
	int term = dirty_count(ST_TERM), menu = dirty_count(ST_MENU);
	int live = 0, login = 0, failed = 0;
	/* `pkill -x` is EXACT and that is load-bearing: `kdos-comp` is a
	 * substring of `kdos-desktop-start`, which is a /bin/sh script that
	 * owns the session and dies on an unhandled SIGHUP. */
	static const char kdos_comp[] = "kdos-comp";

	for (int i = 0; i < NROWS; i++) {
		if (!strcmp(rows[i].val, rows[i].orig))
			continue;
		if (rows[i].scope == SC_LIVE)
			live++;
		else if (rows[i].scope == SC_LOGIN)
			login++;
	}

	if (comp) {
		cfg_path("comp.conf", path, sizeof(path));
		if (write_kv(path, ST_COMP) != 0)
			failed++;
		else
			sighup(kdos_comp);
	}
	if (panel) {
		/* The panel re-reads panel.conf on the same signal `kdos theme`
		 * uses, so these take effect on the bar that is on the screen
		 * rather than at the next login. */
		cfg_path("panel.conf", path, sizeof(path));
		if (write_kv(path, ST_PANEL) != 0)
			failed++;
		else
			sighup("kdos-shell");
	}
	if (res) {
		/*
		 * `kdos-res`, EXACTLY. `kdos-resctl` is a longer name with
		 * this one as its prefix, and it is setuid: a substring match
		 * would send a SIGHUP to a privileged helper that handles no
		 * signals, whose default disposition is death.
		 */
		cfg_path("res.conf", path, sizeof(path));
		if (write_kv(path, ST_RES) != 0)
			failed++;
		else
			sighup("kdos-res");
	}
	if (term) {
		/*
		 * `kdos-term` RE-READS ON THE SAME SIGNAL `kdos theme` sends,
		 * so these reach every terminal already open. A window that
		 * has stepped its own font or its own transparency keeps what
		 * it stepped: this file is where a window STARTS.
		 */
		cfg_path("term.conf", path, sizeof(path));
		if (write_kv(path, ST_TERM) != 0)
			failed++;
		else
			sighup("kdos-term");
	}
	if (menu) {
		/*
		 * NO SIGNAL: kdos-start is opened by its chord and reads the
		 * file as it comes up. Every line this page does not own —
		 * every route — is copied through byte for byte, which is what
		 * makes writing one `@` key into a route table safe.
		 */
		cfg_path("menu.conf", path, sizeof(path));
		if (write_kv(path, ST_MENU) != 0)
			failed++;
	}
	if (launch) {
		/*
		 * NO SIGNAL EITHER, and for a different reason: the launcher
		 * is started by its chord and reads this file as it comes up,
		 * so the next time it is opened it has the answer. There is no
		 * long-lived process holding a stale one.
		 */
		cfg_path("launcher.conf", path, sizeof(path));
		if (write_kv(path, ST_LAUNCH) != 0)
			failed++;
	}
	if (con) {
		/*
		 * NO SIGNAL. The console session reads con.conf once and keeps
		 * the answer; a SIGHUP is its retint and re-reads nothing
		 * here. Every row of this store therefore says `login`, and
		 * sending a signal that did nothing would be the program
		 * pretending otherwise.
		 */
		con_path(path, sizeof(path));
		if (write_kv(path, ST_CON) != 0)
			failed++;
	}
	if (failed) {
		snprintf(note, sizeof(note),
			 "could not write %d file(s) — nothing else changed",
			 failed);
		return;
	}
	for (int i = 0; i < NROWS; i++)
		kb_strlcpy(rows[i].orig, rows[i].val, sizeof(rows[i].orig));

	if (!live && !login)
		snprintf(note, sizeof(note), "nothing to apply");
	else if (login)
		snprintf(note, sizeof(note),
			 "applied: %d now, %d at the next login", live, login);
	else
		snprintf(note, sizeof(note), "applied: %d now", live);
	quit_armed = 0;
}

/*
 * Closing with edits that were never applied asks once — Escape, the right
 * button and the compositor's own dismissal all come through here. The staging
 * IS the safety in this program (nothing has been written yet), so the one
 * thing it must not do is discard silently; the second press does close.
 */
static int try_quit(void)
{
	if (!dirty_any())
		return 1;
	if (quit_armed)
		return 1;
	quit_armed = 1;
	snprintf(note, sizeof(note),
		 "unapplied changes — again to discard, a to apply");
	return 0;
}

/* ── the rows of the selected category ─────────────────────────────────── */

static int cat_rows(void)
{
	int n = 0;

	if (cat == CAT_APPS)
		return napps ? napps : 1;
	if (cat == CAT_BOXES)
		return box_mode == BOX_LIST ? nboxes + 1 : NBOXROWS;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].cat == cat)
			n++;
	return n;
}

/* Index into rows[] of the n'th field of the current category, or -1. */
static int cat_row(int n)
{
	int k = 0;

	if (cat == CAT_APPS || cat == CAT_BOXES)
		return -1;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].cat == cat && k++ == n)
			return i;
	return -1;
}

/*
 * The field under the cursor whichever array it lives in. The Boxes page's
 * keys are `struct row`s of their own so that the editor, the choice cycler
 * and the help line work on them unchanged; this is the one place that has to
 * know there are two arrays.
 */
static struct row *sel_row(void)
{
	int ri;

	if (cat == CAT_BOXES) {
		if (box_mode == BOX_LIST || sel < 0 || sel >= NBOXROWS)
			return NULL;
		/* `name` is set at CREATE time and nowhere else: renaming a
		 * box is a different operation and kdos-box has no verb for
		 * it, so a row that could be typed into here would be a
		 * control that silently does nothing. */
		if (box_mode != BOX_NEW && !strcmp(boxrows[sel].key, "name"))
			return NULL;
		return &boxrows[sel];
	}
	ri = cat_row(sel);
	return ri >= 0 ? &rows[ri] : NULL;
}

/* The same lookup for a row the POINTER is on rather than the caret. */
static struct row *row_at(int n)
{
	int ri;

	if (cat == CAT_BOXES) {
		if (box_mode == BOX_LIST || n < 0 || n >= NBOXROWS)
			return NULL;
		if (box_mode != BOX_NEW && !strcmp(boxrows[n].key, "name"))
			return NULL;
		return &boxrows[n];
	}
	ri = cat_row(n);
	return ri >= 0 ? &rows[ri] : NULL;
}

/*
 * THE CARET NEVER RESTS ON A SECTION RULE.
 *
 * Called after every clamp rather than inside each key: Page Down, End, a
 * click and a wheel all land wherever the arithmetic puts them, and one place
 * that settles the result is one rule instead of six. `dir` is the way the
 * caret was last travelling, so stepping off a heading continues the movement
 * rather than reversing it — and a run of headings at either end of a page
 * bounces off the end instead of walking past it.
 */
static int sel_dir = 1;

static void sel_settle(void)
{
	int n = cat_rows();
	int dir = sel_dir ? sel_dir : 1;

	if (cat == CAT_APPS || cat == CAT_BOXES || n < 1)
		return;
	for (int i = 0; i < n; i++) {
		int ri = cat_row(sel);

		if (ri < 0 || rows[ri].type != FT_HEAD)
			return;
		sel += dir;
		if (sel < 0) {
			sel = 0;
			dir = 1;
		} else if (sel >= n) {
			sel = n - 1;
			dir = -1;
		}
	}
}

static void cycle(struct row *r, int dir)
{
	if (r->type == FT_CHOICE && r->nchoices) {
		int at = 0;
		for (int i = 0; i < r->nchoices; i++)
			if (!strcmp(r->val, r->choices[i]))
				at = i;
		at = (at + dir + r->nchoices) % r->nchoices;
		kb_strlcpy(r->val, r->choices[at], sizeof(r->val));
	} else if (r->type == FT_INT) {
		int v = atoi(r->val) + dir * (r->step ? r->step : 1);
		if (v < r->min)
			v = r->min;
		if (v > r->max)
			v = r->max;
		snprintf(r->val, sizeof(r->val), "%d", v);
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

#define CATW 13

/*
 * ── THE VALUE COLUMN IS A CONTROL ────────────────────────────────
 *
 * Every knob here was a printed string changed with Left and Right, which is a
 * settings window a mouse cannot use: a person who came to this surface with a
 * pointer could select a row and nothing else. A number is a slider, a choice
 * is a dropdown, and both answer a press, a drag and a wheel.
 *
 * ONE RECTANGLE FUNCTION, READ BY THE DRAW AND BY THE HIT TEST. A control the
 * pointer misses by a cell is a control that does not exist.
 */
/*
 * THE LABEL COLUMN IS A THIRD OF THE PANE, floored at 14 and capped at 26.
 *
 * A fixed width truncated `disable_while_typing` at every size, including the
 * ones with cells to spare, and gave a 132-column window a value column
 * starting at the same cell an 80-column one did. The cap is there because a
 * value is the thing being changed: past a quarter of a wide window the label
 * would be taking room from the control.
 */
static int lab_w(int fw)
{
	int w = fw / 3;

	if (w < 14)
		w = 14;
	if (w > 26)
		w = 26;
	if (w > fw - 12)
		w = fw - 12 > 4 ? fw - 12 : 4;
	return w;
}

/*
 * The value column, which begins one cell past the label and stops eight short
 * of the right edge — that is where the `live`/`login` tag is drawn.
 */
static KRect val_rect(int y, int fx, int fw)
{
	int lw = lab_w(fw);
	int vw = fw - lw - 9;

	if (vw < 4)
		vw = 4;
	return krect(fx + lw + 1, y, vw, 1);
}

/* The same rectangle from a SCREEN row, which is what a pointer reports. The
 * two columns are the page's and not the caller's, so both halves derive them
 * here rather than each measuring the window for itself. */
static KRect val_rect_at(int screen_y)
{
	int fx = CATW + 3;
	int fw = ktui_w - fx - 1;

	if (fw < 8)
		fw = 8;
	return val_rect(screen_y, fx, fw);
}

/*
 * ONE DROPDOWN, BELONGING TO WHICHEVER ROW IS SELECTED. Only one list can be
 * open at a time — it is drawn over the rows under it — so a `KtuiDrop` per
 * row would be fifty copies of one piece of state, forty-nine of them stale.
 * `drop_row` is which row it is currently describing; a selection that moves
 * closes it, because a list hanging under a row nobody is on is a list that
 * answers for the wrong key.
 */
static KtuiDrop drop;
static int drop_row = -1;

static int choice_at(const struct row *r)
{
	for (int i = 0; i < r->nchoices; i++)
		if (!strcmp(r->val, r->choices[i]))
			return i;
	return 0;
}

static void drop_close(void)
{
	drop.open = 0;
	drop_row = -1;
}

/*
 * WHAT WAS TYPED, INTO THE ROW. One place, because Enter and a click away from
 * the field are the same answer — two copies of the clamp would be two
 * different ideas of what a number out of range becomes.
 */
static void commit_edit(void)
{
	struct row *er = sel_row();

	if (!er)
		return;
	if (er->type == FT_INT) {
		int v = atoi(edit_buf);

		if (v < er->min)
			v = er->min;
		if (v > er->max)
			v = er->max;
		snprintf(er->val, sizeof(er->val), "%d", v);
	} else {
		kb_strlcpy(er->val, edit_buf, sizeof(er->val));
	}
}

/*
 * A value too wide for its column, marked as such — libktui clips silently,
 * and a clipped value is indistinguishable from the real one. Measured on a
 * booted ISO: `wallpaper` read `/usr/share/backgrounds/kdos/`, which is a
 * directory that exists, so nothing about it looked wrong.
 *
 * A PATH keeps its TAIL, because the file name is the part being identified
 * and every wallpaper on this machine shares the first three components.
 * Anything else keeps its head. The glyph is one cell of the reserved width,
 * never an extra one.
 */
static const char *elide(const char *s, int width, char *buf, size_t cap)
{
	if (width < 2 || ktui_utf8_width(s) <= width)
		return s;

	/*
	 * Cut at a SEPARATOR, not at whatever character the width lands on:
	 * the first cut that fit produced `…kgrounds/kdos/default-wallpaper.png`,
	 * which invents a directory. Left to right, the first `/` whose tail
	 * fits is the longest one that does. A single component too long for
	 * the column falls through to the plain head cut below.
	 */
	for (const char *p = strchr(s, '/'); p; p = strchr(p + 1, '/')) {
		if (ktui_utf8_width(p) <= width - 1) {
			snprintf(buf, cap, "…%s", p);
			return buf;
		}
	}

	size_t n = 0;
	int cells = 0;
	while (s[n] && cells < width - 1) {
		/* Step whole UTF-8 sequences: a half-copied one is a broken
		 * glyph, and this is the same rule ktui_utf8_width counts by. */
		size_t len = 1;
		while ((s[n + len] & 0xc0) == 0x80)
			len++;
		char one[8];
		snprintf(one, sizeof(one), "%.*s", (int)len, s + n);
		cells += ktui_utf8_width(one);
		if (cells > width - 1)
			break;
		n += len;
	}
	snprintf(buf, cap, "%.*s…", (int)n, s);
	return buf;
}

static int btn_x, btn_end, btn_row;

/* ── the home grid ─────────────────────────────────────────────────────── */

/* Where the last frame put each tile, so a click maps back to what was drawn
 * rather than to arithmetic that has to be kept in step with it. */
static KRect tile_hit[NCAT];
static int icons_on = 1;

/* Tiles as wide as the window allows, at least two across. A tile is three
 * rows: the picture and the name on one, the blurb under it, and a blank. */
#define TILE_H 3
static int tile_cols(int w)
{
	int cols = (w - 2) / 26;

	if (cols < 1)
		cols = 1;
	if (cols > 3)
		cols = 3;
	return cols;
}

static void draw_home(void)
{
	int w = ktui_w, h = ktui_h;
	int cols = tile_cols(w);
	int tw = (w - 2) / cols;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	sh_frame(w, h, " Settings ", KT_ACCENT, KT_SURFACE, 1);

	for (int i = 0; i < NCAT; i++) {
		int cx = 1 + (i % cols) * tw;
		int cy = 1 + (i / cols) * TILE_H;
		int on = i == home_sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_SURFACE;

		tile_hit[i] = krect(0, 0, 0, 0);
		if (cy + 1 >= h - 3)
			break;
		/* FILL then swap the slots — never KT_A_REVERSE over the
		 * label, which inverts only the cells the text covers and
		 * turns a two-word name into two lit blocks. */
		ktui_draw_fill(krect(cx, cy, tw - 1, 2), bg);

		/* 2x1 — a 32x32 square on the name's own row. The same rule
		 * the panel keeps: a sprite given two ROWS is centred across
		 * the boundary between them and lines up with nothing. */
		int icon = icons_on ? kicon_slot(CAT_TILE[i].icon, 2, 1) : -1;
		int tx = cx + 1;
		if (icon >= 0) {
			ktui_draw_sprite(krect(cx + 1, cy, 2, 1), icon, fg, bg);
			tx = cx + 4;
		}
		ktui_draw_text(tx, cy, cx + tw - 2 - tx, CAT_NAMES[i], fg, bg,
			       KT_A_NONE);
		ktui_draw_text(cx + 1, cy + 1, tw - 3, CAT_TILE[i].blurb,
			       on ? fg : KT_DIM, bg, KT_A_NONE);
		tile_hit[i] = krect(cx, cy, tw - 1, 2);
	}

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	/* WORDS, not arrows. The ascii tier has no ← →, and the console font
	 * has no left/right arrow either — which is why ktui_glyph carries
	 * ◀ ▶ — so a literal `↑↓←→` here would come out as `????` in a golden
	 * frame and as four blanks on tty1. */
	ktui_hint("Enter", "open");
	ktui_hint("Arrows", "move");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_SURFACE);
	ktui_draw_text(2, h - 2, w - 4, CAT_TILE[home_sel].blurb, KT_MID,
		       KT_SURFACE, KT_A_NONE);
	ktui_draw_flush();
}

static void draw_page(void)
{
	int w = ktui_w, h = ktui_h;
	int pane_rows = h - 5;
	int n = cat_rows();

	if (pane_rows < 1)
		pane_rows = 1;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	sh_frame(w, h, " Settings ", KT_ACCENT, KT_SURFACE, 1);
	ktui_draw_vline(CATW + 1, 1, pane_rows, KT_G_VL, KT_DIM, KT_SURFACE);

	for (int i = 0; i < NCAT && i < pane_rows; i++) {
		int on = i == cat;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? (pane == 0 ? KT_ACCENT : KT_DIM) : KT_SURFACE;
		if (on)
			ktui_draw_fill(krect(1, 1 + i, CATW, 1), bg);
		ktui_draw_text(2, 1 + i, CATW - 1, CAT_NAMES[i],
			       on ? fg : KT_TEXT, bg, KT_A_NONE);
	}

	int fx = CATW + 3;
	int fw = w - fx - 1;
	if (fw < 8)
		fw = 8;

	for (int i = 0; i < pane_rows; i++) {
		int idx = top + i;
		if (idx >= n)
			break;
		int y = 1 + i;
		int on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? (pane == 1 ? KT_ACCENT : KT_DIM) : KT_SURFACE;

		if (on)
			ktui_draw_fill(krect(fx - 1, y, w - fx, 1), bg);

		if (cat == CAT_APPS) {
			if (!napps) {
				ktui_draw_text(fx, y, fw,
					       "no default handlers are "
					       "recorded yet",
					       KT_MID, KT_SURFACE, KT_A_NONE);
				break;
			}
			ktui_draw_text(fx, y, fw / 2, apps[idx].mime, fg, bg,
				       KT_A_NONE);
			ktui_draw_text(fx + fw / 2, y, fw - fw / 2 - 1,
				       apps[idx].id, on ? fg : KT_MID, bg,
				       KT_A_NONE);
			continue;
		}

		const struct row *r;

		if (cat == CAT_BOXES && box_mode == BOX_LIST) {
			/* Row 0 is the way IN, not a box. Every other list on
			 * this desktop puts its create where the eye lands
			 * first, and a Boxes page with no boxes and no way to
			 * make one reads as broken. */
			if (idx == 0) {
				char mk[32];
				snprintf(mk, sizeof(mk), "%s new box",
					 ktui_glyph[KT_G_RIGHT]);
				ktui_draw_text(fx, y, fw, mk,
					       on ? fg : KT_ACCENT, bg,
					       KT_A_NONE);
				continue;
			}
			const struct box_row *b = &boxes[idx - 1];
			int c2 = fw / 3, c3 = fw * 2 / 3;
			ktui_draw_text(fx, y, c2 - 1, b->name, fg, bg,
				       KT_A_NONE);
			ktui_draw_text(fx + c2, y, c3 - c2 - 1, b->base,
				       on ? fg : KT_MID, bg, KT_A_NONE);
			/* A box with no profile says so rather than showing
			 * the defaults as though somebody chose them. */
			ktui_draw_text(fx + c3, y, fw - c3 - 1,
				       b->described ? b->persist
						    : "no profile yet",
				       on ? fg : b->described ? KT_MID : KT_DIM,
				       bg, KT_A_NONE);
			continue;
		}
		if (cat == CAT_BOXES) {
			r = &boxrows[idx];
			/* `name` is fixed once the box exists — drawn as what
			 * it is rather than as a field that will not take. */
			if (box_mode != BOX_NEW && !strcmp(r->key, "name")) {
				char t[128];
				snprintf(t, sizeof(t), "%-18s %s", r->label,
					 r->val);
				ktui_draw_text(fx, y, fw, t, on ? fg : KT_MID,
					       bg, KT_A_NONE);
				continue;
			}
		} else {
			int ri = cat_row(idx);
			if (ri < 0)
				continue;
			r = &rows[ri];
		}
		if (r->type == FT_HEAD) {
			/* THE LABEL, THEN A RULE TO THE EDGE OF THE PANE. The
			 * rule is what makes a heading read as a divider at a
			 * glance rather than as a row with no value. */
			int lw = (int)strlen(r->label);

			if (lw > fw - 4)
				lw = fw - 4;
			ktui_draw_fill(krect(fx, y, fw, 1), bg);
			ktui_draw_text(fx, y, lw, r->label, KT_ACCENT, bg,
				       KT_A_BOLD);
			if (fw - lw - 2 > 0)
				ktui_draw_hline(fx + lw + 1, y, fw - lw - 2,
						KT_G_HL, KT_DIM, bg);
			continue;
		}
		if (r->type == FT_NOTE) {
			ktui_draw_text(fx, y, fw, r->label, on ? fg : KT_MID, bg,
				       KT_A_NONE);
			continue;
		}
		if (r->type == FT_TOOL) {
			/* A row that opens a program, drawn as one: the label
			 * and the ▶ that says Enter goes somewhere. */
			ktui_draw_text(fx, y, fw - 2, r->label,
				       on ? fg : KT_ACCENT, bg, KT_A_NONE);
			ktui_draw_text(fx + fw - 2, y, 1,
				       ktui_glyph[KT_G_RIGHT],
				       on ? fg : KT_DIM, bg, KT_A_NONE);
			continue;
		}

		char label[64];
		snprintf(label, sizeof(label), "%s%s", r->label,
			 strcmp(r->val, r->orig) ? " *" : "");
		ktui_draw_text(fx, y, lab_w(fw), label, fg, bg, KT_A_NONE);

		/* The value, then the scope tag hard against the right edge:
		 * `live` and `login` are the two answers to "did that do
		 * anything", and they belong on the row that raises it. */
		const char *tag = r->scope == SC_LIVE ? "live" : "login";
		KRect vr = val_rect(y, fx, fw);
		char shown[256];

		/*
		 * A NUMBER IS A SLIDER AND A CHOICE IS A DROPDOWN, and the
		 * row being EDITED is neither: while a person is typing an
		 * exact value, what belongs in that column is what they have
		 * typed.
		 */
		if (r->type == FT_INT && !(on && editing)) {
			ktui_slider_draw(vr, atoi(r->val), r->min, r->max, on,
					 bg);
		} else if (r->type == FT_CHOICE && !(on && editing)) {
			KtuiDrop d = { choice_at(r), 0, 0 };

			if (on && drop_row == idx)
				d = drop;
			ktui_dropdown_draw(vr, &d, r->choices, r->nchoices, on);
		} else if (on && editing) {
			/*
			 * THE ONE FIELD IN THE TOOLKIT, with the caret, the
			 * arrows, Home and End, a click to place it and the
			 * paste queue — and with UTF-8 handled in columns, so
			 * a backspace cannot cut a character in half.
			 *
			 * The frame is opened and closed around this one
			 * control because the surface has an event loop of its
			 * own; with a single control claimed the focus is
			 * always it.
			 */
			ktui_draw_fill(vr, bg);
			ktui_frame_begin(&field_ev);
			ktui_input(vr, edit_buf, sizeof(edit_buf), 0, NULL);
			ktui_frame_end();
			memset(&field_ev, 0, sizeof(field_ev));
		} else {
			ktui_draw_text(vr.x, y, vr.w,
				       elide(r->val[0] ? r->val : "(unset)",
					     vr.w, shown, sizeof(shown)),
				       on ? fg : (r->val[0] ? KT_MID : KT_DIM),
				       bg, KT_A_NONE);
		}
		ktui_draw_text_right(0, y, w - 2, tag,
				     on ? fg : (r->scope == SC_LIVE ? KT_MID
							            : KT_DIM),
				     bg, KT_A_NONE);
	}

	/*
	 * AND THE OPEN LIST LAST, OVER THE ROWS IT COVERS. A dropdown drawn in
	 * its own row's turn would be painted over by every row below it.
	 */
	if (drop.open && drop_row >= top && drop_row < top + pane_rows) {
		struct row *dr = sel_row();

		if (dr && dr->type == FT_CHOICE)
			ktui_dropdown_draw_open(val_rect(1 + drop_row - top, fx,
							 fw),
						&drop, dr->choices,
						dr->nchoices);
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE — see kch_scrollbar. The
	 * Apps page is every recorded default handler on the machine, which is
	 * the longest list this window has and the one that gave no sign of
	 * having a below-the-fold at all.
	 */
	kch_scrollbar(0, w - 1, 1, pane_rows, n, top, KT_SURFACE);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	/* The junction where the category divider meets the rule under it —
	 * without it the two panes read as one column of text with a line
	 * through the middle of it. */
	ktui_draw_text(CATW + 1, h - 4, 1, ktui_glyph[KT_G_TEE_B], KT_DIM,
		       KT_SURFACE, KT_A_NONE);

	/* The help row: what the selected field is, and what it costs. */
	if (editing) {
		char line[320];
		const struct row *er = sel_row();
		snprintf(line, sizeof(line), "%s: %s",
			 er ? er->label : "value", edit_buf);
		const char *el = line;
		if ((int)strlen(line) > w - 4)
			el = line + strlen(line) - (size_t)(w - 4);
		ktui_draw_text(2, h - 3, w - 4, el, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (note[0]) {
		ktui_draw_text(2, h - 3, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	} else if (cat == CAT_APPS) {
		ktui_draw_text(2, h - 3, w - 4,
			       "Enter opens the chooser for this type — it "
			       "writes the default itself",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else if (cat == CAT_BOXES && box_mode == BOX_LIST) {
		ktui_draw_text(2, h - 3, w - 4,
			       sel == 0 ? "Enter describes a new box; nothing "
					  "is created until you apply"
					: "Enter opens this box's profile",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else if (cat == CAT_BOXES) {
		/* `sel_row` and not `boxrows[sel]`: it is the one place that
		 * knows `name` is fixed once the box exists, and a help line
		 * describing how to type into a row that will not take is
		 * worse than none. */
		const struct row *r = sel_row();
		int fixed_name = !r && sel >= 0 && sel < NBOXROWS &&
				 !strcmp(boxrows[sel].key, "name");
		/*
		 * THE NETWORK WARNING IS A ROW, not a footnote. `base =
		 * image:<ref>` fetches unsigned content from somebody else's
		 * registry; KDOS_REQUIRE_SIG does not cover it and pretending
		 * otherwise would be dishonest — so it beats the selected
		 * row's own help, which is a description of one key against a
		 * statement about the whole box.
		 */
		if (!strncmp(boxrow_get("base"), "image:", 6))
			ktui_draw_text(2, h - 3, w - 4,
				       "an image: base is ONLINE and unsigned "
				       "— pack: and box: are the offline kinds",
				       KT_WARN, KT_SURFACE, KT_A_NONE);
		else if (fixed_name)
			ktui_draw_text(2, h - 3, w - 4,
				       "a box is renamed by creating another "
				       "one — kdos-box has no verb for it",
				       KT_MID, KT_SURFACE, KT_A_NONE);
		else
			ktui_draw_text(2, h - 3, w - 4,
				       r && r->help ? r->help : "", KT_MID,
				       KT_SURFACE, KT_A_NONE);
	} else {
		int ri = cat_row(sel);
		ktui_draw_text(2, h - 3, w - 4,
			       ri >= 0 && rows[ri].help ? rows[ri].help : "",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	}

	int pending = dirty_any();
	if (cat == CAT_BOXES)
		pending = box_mode == BOX_LIST ? 0 : box_dirty();
	/*
	 * THE ROW FOLLOWS THE PANE AND THE ROW TYPE, which the fixed string
	 * could not: on the categories pane it said ◀▶ change over a list
	 * that answers neither, and on an FT_TOOL row it said Enter edit over
	 * a row that runs a program.
	 *
	 * The arrows come from the glyph table: the console font has ◀ ▶ and
	 * no ← →, which is why ktui_glyph carries that pair. The key is copied
	 * by ktui_hint, so a buffer on this stack is enough.
	 */
	char lr[8];
	const struct row *hr = sel_row();
	int fields = !editing && pane == 1;
	int boxlist = cat == CAT_BOXES && box_mode == BOX_LIST;

	snprintf(lr, sizeof(lr), "%s%s", ktui_glyph[KT_G_LEFT],
		 ktui_glyph[KT_G_RIGHT]);

	ktui_hint_if(editing, "Enter", "ok");
	ktui_hint_if(!editing && pane == 0, "Up/Down", "category");
	ktui_hint_if(!editing && pane == 0, "Enter", "fields");
	ktui_hint_if(fields && cat == CAT_APPS && napps > 0, "Enter",
		     "chooser");
	ktui_hint_if(fields && cat == CAT_BOXES && boxlist, "Enter", "open");
	ktui_hint_if(fields && cat != CAT_APPS && !boxlist && hr &&
			     (hr->type == FT_CHOICE || hr->type == FT_INT),
		     lr, "change");
	ktui_hint_if(fields && cat != CAT_APPS && !boxlist && hr &&
			     (hr->type == FT_TEXT || hr->type == FT_INT),
		     "Enter", "edit");
	ktui_hint_if(fields && cat != CAT_APPS && cat != CAT_BOXES && hr &&
			     hr->type == FT_TOOL,
		     "Enter", "open");
	/* `a` is not gated on `pending`: apply() also writes panel and monitor
	 * state that the counter does not count, so gating it would hide a
	 * live key. */
	ktui_hint_if(fields && cat != CAT_APPS && !boxlist, "a",
		     cat == CAT_BOXES
			     ? (box_mode == BOX_NEW ? "create" : "write")
			     : "apply");
	ktui_hint_if(!editing, "Tab", "panes");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 16, 1), KT_SURFACE);

	/* One button, and it is the one irreversible thing on the screen. */
	char blabel[24];
	if (cat == CAT_BOXES && box_mode == BOX_NEW)
		snprintf(blabel, sizeof(blabel), " Create ");
	else if (cat == CAT_BOXES && box_mode == BOX_LIST)
		snprintf(blabel, sizeof(blabel), " New box ");
	else
		snprintf(blabel, sizeof(blabel), " Apply%s ",
			 pending ? " *" : "");
	int bw = ktui_utf8_width(blabel) + 2;
	btn_x = w - 2 - bw;
	btn_end = btn_x + bw;
	btn_row = h - 2;
	if (btn_x > 24) {
		ktui_draw_text(btn_x, btn_row, 1, "[", KT_MID, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(btn_x + 1, btn_row, bw - 2, blabel,
			       pending ? KT_SURFACE : KT_DIM,
			       pending ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		ktui_draw_text(btn_end - 1, btn_row, 1, "]", KT_MID, KT_SURFACE,
			       KT_A_NONE);
	} else {
		btn_x = btn_end = 0;
	}
	ktui_draw_flush();
}

/* One draw entry point, so the loop and the `--dump` path cannot diverge about
 * which screen is on. */
static void draw(void)
{
	if (mode == SM_HOME)
		draw_home();
	else
		draw_page();
}

/*
 * `--dump-cells`: one line per painted cell, codepoint and colour SLOT, which
 * is what a golden frame has to compare — a plain text dump proves the layout
 * and says nothing about whether the surface is wearing the palette.
 *
 * Through the backend vtable rather than ktui_offscreen_init(), because the
 * cell buffer is private to libktui and offscreen mode short-circuits the
 * flush entirely. Same seam as keys.c, per front end.
 */
static int cap_w = 72, cap_h = 20;

static void cap_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)prev;
	(void)ff;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const KtuiCell *c = &cur[y * w + x];
			if (!c->ch || c->ch == ' ' || c->ch == KTUI_WIDE_CONT)
				continue;
			printf("%d %d U+%04X %d %d %d\n", y, x, c->ch, c->fg,
			       c->bg, c->attr);
		}
}

static int cap_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static void cap_size(int *w, int *h)
{
	*w = cap_w;
	*h = cap_h;
}

static int cap_caps(void)
{
	return KT_CAP_UTF8 | KT_CAP_TRUECOLOR;
}

static const KtuiBackend cap_backend = {
	.name = "dump-cells",
	.flush = cap_flush,
	.poll_event = cap_poll,
	.size = cap_size,
	.caps = cap_caps,
};

/* ── main ──────────────────────────────────────────────────────────────── */

/* Enter on a row: a choice cycles, an int or a text opens the editor, an Apps
 * row hands the type to the chooser that owns default handlers. */
static void activate(void)
{
	if (cat == CAT_APPS) {
		if (!napps || sel < 0 || sel >= napps)
			return;
		const char *argv[4] = { "kdos-openwith", "--mime",
					apps[sel].mime, NULL };
		pid_t p = fork();
		if (p == 0) {
			if (fork() == 0) {
				setsid();
				kb_child_reset_signals();
				execvp(argv[0], (char *const *)argv);
				_exit(127);
			}
			_exit(0);
		}
		if (p > 0) {
			int st;
			waitpid(p, &st, 0);
		}
		snprintf(note, sizeof(note),
			 "chooser opened for %.60s — this list re-reads itself",
			 apps[sel].mime);
		return;
	}

	if (cat == CAT_BOXES && box_mode == BOX_LIST) {
		if (sel == 0) {
			box_open("", 1);
			sel = top = 0;
			note[0] = '\0';
		} else if (sel - 1 < nboxes) {
			box_open(boxes[sel - 1].name, 0);
			sel = top = 0;
			note[0] = '\0';
		}
		return;
	}

	struct row *sr = sel_row();
	if (!sr || sr->type == FT_NOTE || sr->type == FT_HEAD)
		return;
	if (cat == CAT_BOXES) {
		if (sr->type == FT_CHOICE) {
			cycle(sr, 1);
			return;
		}
		editing = 1;
		kb_strlcpy(edit_buf, sr->val, sizeof(edit_buf));
		note[0] = '\0';
		return;
	}

	int ri = cat_row(sel);
	if (ri < 0 || rows[ri].type == FT_NOTE || rows[ri].type == FT_HEAD)
		return;
	if (rows[ri].type == FT_TOOL) {
		/* Spawned DETACHED, never waited for: the managers are
		 * long-lived surfaces of their own, and a settings window that
		 * froze until the network dialog closed would be the thing
		 * this hub exists not to be. */
		const char *argv[2] = { rows[ri].key, NULL };
		sh_spawn(argv);
		snprintf(note, sizeof(note), "opened %.40s", rows[ri].key);
		return;
	}
	if (rows[ri].type == FT_CHOICE) {
		cycle(&rows[ri], 1);
		return;
	}
	editing = 1;
	kb_strlcpy(edit_buf, rows[ri].val, sizeof(edit_buf));
	note[0] = '\0';
}

int settings_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, cells = 0;
	int start_cat = -1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			cells = 1;
		/* comp.conf's `icons = no`. Off is not a degraded mode: the
		 * grid falls back to its glyph tier, which is what a tty and
		 * an install with no artwork draw. */
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		/* A golden frame is named for its size, so the harness has to
		 * be able to ask for one. */
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 23 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		}
		/* `--page <name>`: an applet opens the page that owns it. The
		 * names are the category words, lowercased. An unknown one is
		 * reported rather than ignored — a deep link that silently
		 * opened the first page would be a link nobody could debug. */
		else if (!strcmp(argv[i], "--page") && i + 1 < argc) {
			const char *want = argv[++i];
			int found = -1;
			for (int c = 0; c < NCAT; c++)
				if (!strcasecmp(PAGE_NAMES[c], want))
					found = c;
			if (found < 0) {
				fprintf(stderr, "kdos-settings: no page named "
						"'%s'\n", want);
				return 2;
			}
			start_cat = found;
		} else {
			fprintf(stderr, "usage: kdos-settings "
					"[--dump|--dump-cells] [--dump-size WxH]\n"
					"                     [--page NAME] "
					"[--no-icons] [--font NAME]\n");
			return 2;
		}
	}

	/* BEFORE the dump branches: both draw, `--page` makes the page rung up
	 * in a dump, and a frame that read the verb through an empty ladder
	 * would disagree with the live surface. */
	/* The page in /usr/share/kdos/doc that F1 opens. A name with no
	 * file there is refused by testing/preflight.sh. */
	keys.doc = "settings";
	keys.help = sh_help;
	ktui_keys_layer(&keys, "Back", page_up, page_close, NULL);
	ktui_keys_layer(&keys, "Back", boxprof_up, boxprof_close, NULL);
	ktui_keys_layer(&keys, "Cancel", edit_up, edit_cancel, NULL);

	load_all();
	/* A deep link lands ON the page, not on the grid: the panel's battery
	 * readout opening Settings and making you pick Session again would be
	 * a link that does half its job. */
	if (start_cat >= 0) {
		cat = start_cat;
		home_sel = start_cat;
		sel = top = 0;
		mode = SM_PAGE;
	}
	pane = 1;

	if (dump || cells) {
		sh_theme_from_cache();
		icons_on = 0;
		if (cells) {
			ktui_backend_set(&cap_backend);
			ktui_draw_init();
			draw();
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw();
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = 72,
		.rows = 20,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Settings",
		.app_id = "kdos-settings",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-settings: no compositor or no "
				"layer-shell\n");
		return 2;
	}
	/* AFTER kdisp_init: the icon layer needs the cell size and the output
	 * scale, and neither exists until the surface does. */
	/*
	 * THE NOMINAL CELL WHERE THERE IS NO REAL ONE, and the sprite backend
	 * before it. A console surface has no pixel size of its own —
	 * kdisp_cell_w() answers 1 — so rasterising at it makes every icon a
	 * picture a pixel or two across, which is a blank cell by a longer
	 * route; sh_pic_cell_w() is the size the wire is bounded by and the
	 * display rescales to its own font. sh_pic_backend() must come after
	 * kdisp_init: the console backend clears its client state when it
	 * connects, so a callback registered before that point is erased.
	 */
	sh_pic_backend();
	if (icons_on)
		kicon_init(sh_pic_cell_w(), sh_pic_cell_h(), kdisp_scale());
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int pane_rows = ktui_h - 5;
		int n = cat_rows();

		if (pane_rows < 1)
			pane_rows = 1;
		if (sel >= n)
			sel = n ? n - 1 : 0;
		if (sel < 0)
			sel = 0;
		/* The viewport follows the SELECTION only when the selection is
		 * what moved. `sel_follow` was declared for this and read by
		 * nothing, so a hand-rolled pull sat here instead — which is
		 * why the wheel and the scrollbar could not move the page: the
		 * next frame put it straight back. */
		sel_settle();
		kch_list_clamp(&top, sel, n, pane_rows, sel_follow);

		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			/* The chooser is another process; the only way to know
			 * it wrote is to look again. */
			if (cat == CAT_APPS && !editing)
				load_apps();
			continue;
		}

		/*
		 * THE LADDER FIRST, above both screens. CLOSE reaches here
		 * only with no rung up — which is the home grid — and runs the
		 * same unsaved-changes question the removed arm did.
		 */
		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE) {
				if (try_quit())
					goto out;
				continue;
			}
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		/* ── the home grid owns its own input ── */
		if (mode == SM_HOME) {
			int cols = tile_cols(ktui_w);
			int open = -1;

			if (ev.type == KT_EVT_MOUSE) {
				int hit = -1;
				for (int i = 0; i < NCAT; i++)
					if (tile_hit[i].w > 0 &&
					    ev.mx >= tile_hit[i].x &&
					    ev.mx < tile_hit[i].x + tile_hit[i].w &&
					    ev.my >= tile_hit[i].y &&
					    ev.my < tile_hit[i].y + tile_hit[i].h)
						hit = i;
				if (ev.press == KT_MP_DRAG) {
					if (hit >= 0)
						home_sel = hit;
					continue;
				}
				if (ev.press != KT_MP_PRESS)
					continue;
				/* ONE click opens a tile. A grid of seven
				 * pictures is aimed at, not browsed — the
				 * double-click a file manager needs is there
				 * to protect a selection this screen does not
				 * have. */
				if (ev.btn == KT_MB_LEFT && hit >= 0) {
					home_sel = hit;
					open = hit;
				} else if (ev.btn == KT_MB_RIGHT) {
					break;
				} else {
					continue;
				}
			} else if (ev.type == KT_EVT_KEY) {
				switch (ev.key) {
				case KT_K_LEFT:
					if (home_sel > 0)
						home_sel--;
					continue;
				case KT_K_RIGHT:
					if (home_sel < NCAT - 1)
						home_sel++;
					continue;
				case KT_K_UP:
					if (home_sel - cols >= 0)
						home_sel -= cols;
					continue;
				case KT_K_DOWN:
					if (home_sel + cols < NCAT)
						home_sel += cols;
					continue;
				case KT_K_HOME:
					home_sel = 0;
					continue;
				case KT_K_END:
					home_sel = NCAT - 1;
					continue;
				case KT_K_ENTER:
					open = home_sel;
					break;
				default:
					continue;
				}
			} else {
				continue;
			}
			if (open < 0)
				continue;
			cat = open;
			sel = top = 0;
			pane = 1;
			mode = SM_PAGE;
			if (cat == CAT_APPS)
				load_apps();
			if (cat == CAT_BOXES) {
				load_boxes();
				box_mode = BOX_LIST;
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + top;
			int in_cats = ev.mx <= CATW && ev.my >= 1 &&
				      ev.my - 1 < NCAT;
			int in_fields = ev.mx > CATW + 1 && ev.my >= 1 &&
					ev.my < ktui_h - 4 && row >= 0 &&
					row < n;
			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				if (in_fields && !editing) {
					/*
					 * A DRAG ON A SLIDER SETS IT, and on
					 * anything else moves the caret. The
					 * row is the one the press began on,
					 * which is what lets the pointer leave
					 * the column and go on setting the
					 * value.
					 */
					struct row *dr = row_at(sel);

					if (dr && dr->type == FT_INT &&
					    drag_slider) {
						int v = atoi(dr->val);

						if (ktui_slider_hit(
							    val_rect_at(1 + sel -
									top),
							    &v, dr->min,
							    dr->max,
							    dr->step ? dr->step
								     : 1,
							    ev.mx, ev.my, 0))
							snprintf(dr->val,
								 sizeof(dr->val),
								 "%d", v);
						continue;
					}
					pane = 1;
					sel = row;
					sel_follow = 1;
				}
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				drag_slider = 0;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			/*
			 * AN OPEN LIST IS OVER THE ROWS AND ANSWERS FIRST.
			 * It was drawn last, so a press tested against the
			 * rows would pick whatever the list is covering — and
			 * a press outside it closes without choosing, which is
			 * the only reading of a click on the thing a list is
			 * standing in front of.
			 */
			if (drop.open && ev.btn == KT_MB_LEFT) {
				struct row *dr = row_at(drop_row);

				if (dr && dr->type == FT_CHOICE &&
				    ktui_dropdown_hit(val_rect_at(1 + drop_row -
								  top),
						      &drop, dr->nchoices,
						      ev.mx, ev.my) &&
				    drop.sel >= 0 && drop.sel < dr->nchoices)
					kb_strlcpy(dr->val,
						   dr->choices[drop.sel],
						   sizeof(dr->val));
				if (!drop.open)
					drop_close();
				continue;
			}
			if (ev.btn == KT_MB_LEFT) {
				int bt = kch_scrollbar_press(0, ev.mx, ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				/*
				 * A DETENT OVER A CONTROL TURNS IT and
				 * anywhere else scrolls the page. A wheel that
				 * always scrolled would leave the pointer
				 * unable to nudge a value at all.
				 */
				int up = ev.btn == KT_MB_WHEEL_UP;
				struct row *wr = in_fields && !editing
						 ? row_at(row) : NULL;

				if (wr && (wr->type == FT_INT ||
					   wr->type == FT_CHOICE) &&
				    krect_hit(val_rect_at(ev.my), ev.mx,
					      ev.my)) {
					pane = 1;
					sel = row;
					cycle(wr, up ? 1 : -1);
				} else {
					/* AND THE DETENT IS A DIRECTION. A
					 * section rule is stepped over the way
					 * the wheel was turning; without this
					 * the settle uses whichever way a KEY
					 * last went, so scrolling up through a
					 * heading jumps down past it. */
					sel_dir = up ? -1 : 1;
					sel += up ? -1 : 1;
					sel_follow = 1;
				}
			} else if (ev.btn == KT_MB_RIGHT) {
				/* Back one level, the same as Escape. */
				mode = SM_HOME;
				home_sel = cat;
			} else if (ev.btn == KT_MB_LEFT) {
				if (btn_end > btn_x && ev.my == btn_row &&
				    ev.mx >= btn_x && ev.mx < btn_end) {
					if (cat != CAT_BOXES) {
						apply();
					} else if (box_mode == BOX_LIST) {
						box_open("", 1);
						sel = top = 0;
					} else {
						box_write();
					}
				} else if (in_cats && !editing) {
					if (ev.my - 1 != cat) {
						cat = ev.my - 1;
						sel = 0;
						top = 0;
						if (cat == CAT_APPS)
							load_apps();
						if (cat == CAT_BOXES) {
							load_boxes();
							box_mode = BOX_LIST;
						}
					}
					pane = 0;
				} else if (in_fields && !editing) {
					struct row *cr = row_at(row);
					KRect vr = val_rect_at(ev.my);
					int on_val = krect_hit(vr, ev.mx,
							       ev.my);
					/* Where the caret WAS: a click on the
					 * row it is already on is the one that
					 * activates, and moving it first would
					 * make every first click an activate. */
					int sel_was = sel, pane_was = pane;

					pane = 1;
					sel = row;

					/*
					 * THE CONTROL IN THE VALUE COLUMN
					 * ANSWERS FIRST, and it answers on the
					 * FIRST press rather than the second:
					 * a slider that needed the row
					 * selecting before it could be dragged
					 * would be two gestures for one
					 * movement.
					 */
					if (cr && on_val &&
					    cr->type == FT_INT) {
						int v = atoi(cr->val);

						drag_slider = 1;
						if (ktui_slider_hit(
							    vr, &v, cr->min,
							    cr->max,
							    cr->step ? cr->step
								     : 1,
							    ev.mx, ev.my, 1))
							snprintf(cr->val,
								 sizeof(cr->val),
								 "%d", v);
						continue;
					}
					if (cr && on_val &&
					    cr->type == FT_CHOICE) {
						drop.sel = choice_at(cr);
						drop.hi = drop.sel;
						drop.open = !drop.open ||
							    drop_row != row;
						drop_row = drop.open ? row
								     : -1;
						continue;
					}
					/* A click on the row that is already
					 * selected activates it — pick.c's
					 * rule, so one hand learns one thing. */
					if (row == sel_was && pane_was == 1)
						activate();
				}
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		sel_follow = 1;	/* a key moves the cursor; the view follows */

		/*
		 * AN OPEN LIST OWNS THE KEYBOARD WHILE IT IS DOWN, exactly as
		 * it owns the pointer: a caret that walked the rows underneath
		 * would leave the list describing a key nobody is on.
		 */
		if (drop.open) {
			struct row *dr = row_at(drop_row);

			if (!dr || dr->type != FT_CHOICE) {
				drop_close();
			} else if (ktui_dropdown_key(&drop, dr->nchoices,
						     ev.key) &&
				   drop.sel >= 0 && drop.sel < dr->nchoices) {
				kb_strlcpy(dr->val, dr->choices[drop.sel],
					   sizeof(dr->val));
			}
			if (!drop.open)
				drop_close();
			continue;
		}

		if (editing) {
			if (ev.key == KT_K_ENTER) {
				commit_edit();
				editing = 0;
			} else {
				/*
				 * EVERYTHING ELSE IS THE FIELD'S, and it is
				 * handed across rather than acted on here —
				 * the control reads the event the frame was
				 * given. Escape is not excepted: `ktui_keys`
				 * above has already taken it.
				 */
				field_ev = ev;
			}
			continue;
		}

		quit_armed = 0;

		struct row *sr = sel_row();
		switch (ev.key) {
		case KT_K_TAB:
			pane = !pane;
			break;
		case KT_K_UP:
			if (pane == 0) {
				if (cat > 0) {
					cat--;
					sel = top = 0;
					if (cat == CAT_APPS)
						load_apps();
					if (cat == CAT_BOXES) {
						load_boxes();
						box_mode = BOX_LIST;
					}
				}
			} else {
				sel_dir = -1;
				sel--;
			}
			break;
		case KT_K_DOWN:
			if (pane == 0) {
				if (cat < NCAT - 1) {
					cat++;
					sel = top = 0;
					if (cat == CAT_APPS)
						load_apps();
					if (cat == CAT_BOXES) {
						load_boxes();
						box_mode = BOX_LIST;
					}
				}
			} else {
				sel_dir = 1;
				sel++;
			}
			break;
		case KT_K_LEFT:
			if (pane == 1 && sr)
				cycle(sr, -1);
			else
				pane = 0;
			break;
		case KT_K_RIGHT:
			if (pane == 0)
				pane = 1;
			else if (sr)
				cycle(sr, 1);
			break;
		case KT_K_PGUP:
			sel_dir = -1;
			sel -= pane_rows;
			break;
		case KT_K_PGDN:
			sel_dir = 1;
			sel += pane_rows;
			break;
		case KT_K_HOME:
			sel_dir = 1;
			sel = 0;
			break;
		case KT_K_END:
			sel_dir = -1;
			sel = n - 1;
			break;
		case KT_K_ENTER:
			if (pane == 0)
				pane = 1;
			else
				activate();
			break;
		case 'a':
			if (cat == CAT_BOXES) {
				if (box_mode != BOX_LIST)
					box_write();
				else
					snprintf(note, sizeof(note),
						 "open a box, or `%s new box`",
						 ktui_glyph[KT_G_RIGHT]);
			} else {
				apply();
			}
			break;
		default:
			break;
		}
	}
out:
	kdisp_shutdown();
	return 0;
}
