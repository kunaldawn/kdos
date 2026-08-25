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
 *   ║ Session    │▸crt              55                  live   ║
 *   ║ Input      │ crt_scanlines    60                  live   ║
 *   ║ Apps       │ chrome_font      Terminus:pixel…     login  ║
 *   ╟────────────┴─────────────────────────────────────────────╢
 *   ║ the phosphor shader's strength; 0 is an honest off        ║
 *   ║ ←→ change  Enter edit  a apply  Esc close      [ Apply ]  ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * Every knob on this desktop was a text file and nothing else — which is the
 * right storage and the wrong interface for somebody who does not already know
 * the file exists. This is the OS/2-setup lineage: a category list, a form, and
 * no mode the mouse cannot reach.
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
#include "shell.h"

enum { CAT_APPEARANCE = 0, CAT_PANEL, CAT_DESKTOP, CAT_HARDWARE, CAT_SESSION,
       CAT_INPUT, CAT_APPS, CAT_BOXES, NCAT };

static const char *const CAT_NAMES[NCAT] = {
	"Appearance", "Panel", "Desktop", "Hardware", "Session", "Input",
	"Apps", "Boxes"
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
	"apps", "boxes"
};

/* Where a row's value is stored. ST_THEME is the accent, which is not a file
 * this program writes at all: `kdos theme` owns the whole palette pipeline
 * (icons, cursors, gtk.css, foot, btop, starship) and a second writer of the
 * accent would be a second thing to keep in agreement with it. */
/*
 * ST_PANEL is `~/.config/kdos/panel.conf`, which is a SECOND file in exactly
 * the same `key = value` shape — so write_kv already knows how to rewrite it
 * and the only new thing is which signal to send afterwards. It matters
 * because everything in it is a decision about the bar somebody is looking at
 * (which widgets, which charts, what is hidden behind the chevron), and until
 * now the only way to change any of it was to know the file existed.
 */
/*
 * ST_RES is `~/.config/kdos/res.conf`, a THIRD file in the same `key = value`
 * shape. kdos-res re-reads it on the SIGHUP this program already sends for the
 * panel, so a changed interval or column set reaches the monitor that is on
 * the screen rather than the next one started.
 */
/*
 * ST_BOX is `~/.config/kdos/boxes/<name>.conf` and is the ONE store this
 * program does not write itself: `kdos-box` is the writer, so a box configured
 * here and a box configured at a prompt cannot come out different.
 */
enum { ST_NONE = 0, ST_COMP, ST_THEME, ST_PANEL, ST_RES, ST_BOX };

/* When a change takes effect. */
enum { SC_NONE = 0, SC_LIVE, SC_LOGIN };

/*
 * FT_TOOL is a row that RUNS something rather than storing anything, and it is
 * what makes this a control centre instead of a comp.conf editor. The screens,
 * the network, bluetooth, sound and the cameras are each a whole program with
 * its own protocol; re-implementing a summary of them here would be a second
 * thing to keep true. The row says what it opens and opens it.
 */
enum { FT_CHOICE = 0, FT_INT, FT_TEXT, FT_NOTE, FT_APP, FT_TOOL };

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
static const char *const ONOFF[] = { "on", "off" };
static const char *const LIDS[] = { "off", "lock", "suspend" };
static const char *const PANELS[] = { "bottom", "top", "off" };
static const char *const TASKLAB[] = { "auto", "yes", "no" };
static const char *const CPUPCT[] = { "core", "machine" };
static const char *const UNITS[] = { "1024", "1000" };
static const char *const TEMPU[] = { "c", "f" };

/* The accent names, filled from libktui's own table at startup — the palette
 * lives in libkcolor and every consumer expands the same one. */
static const char *accents[8];
static int naccents;

static struct row rows[] = {
	/* ── Appearance ─────────────────────────────────────────────── */
	{ CAT_APPEARANCE, FT_CHOICE, ST_THEME, SC_LIVE, "accent", "accent",
	  NULL, 0, 0, 0, 0,
	  "runs `kdos theme <accent>`: host, box and window frames together",
	  "phosphor", "phosphor" },
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
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LIVE, "wallpaper", "wallpaper",
	  NULL, 0, 0, 0, 0,
	  "a PNG, scaled to cover and centred; the word `none` is an honest off",
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

	/* ── Session ────────────────────────────────────────────────── */
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
	{ CAT_PANEL, FT_CHOICE, ST_PANEL, SC_LIVE, "task_labels", "task_labels",
	  TASKLAB, 3, 0, 0, 0,
	  "whether a window button carries its name. `auto` is the bar "
	  "deciding; `no` is a dock",
	  "auto", "auto" },
	{ CAT_PANEL, FT_TEXT, ST_PANEL, SC_LIVE, "right", "right",
	  NULL, 0, 0, 0, 0,
	  "the notification area, left to right. An unknown name is reported "
	  "on stderr, never ignored",
	  "pager tray more media privacy mpris clipboard cpu stutter restart "
	  "net volume battery notify clock",
	  "pager tray more media privacy mpris clipboard cpu stutter restart "
	  "net volume battery notify clock" },

	{ CAT_PANEL, FT_NOTE, ST_NONE, SC_NONE, NULL, "pinned launchers",
	  NULL, 0, 0, 0, 0,
	  "~/.config/kdos/favorites, one desktop-entry id per line — the same "
	  "file the Start menu pins from",
	  "", "" },

	/* ── Desktop ────────────────────────────────────────────────── */
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

	/* ── Input ──────────────────────────────────────────────────────
	 *
	 * NOTES, and no fields. Nothing here writes the keyboard's settings,
	 * because nothing on this system would read what it wrote: the layout
	 * comes from /etc/keymap by way of kdos-desktop, and every keyboard knob
	 * the compositor has is rc.xml's. A row that wrote a file no program
	 * opens is exactly the "change a thing, see nothing" this surface
	 * promised not to be, so the category says where the knobs really are. */
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

static const char *const BASES[] = { "pack:base", "image:debian:trixie",
				     "box:kdos-apps" };
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
static char edit_buf[256];

/* ── the files ─────────────────────────────────────────────────────────── */

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

	for (int i = 0; i < ktui_ntheme && naccents < 8; i++)
		accents[naccents++] = ktui_themes[i].name;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].store == ST_THEME) {
			rows[i].choices = accents;
			rows[i].nchoices = naccents;
		}

	/* The accent in force is the one-word state file every other surface
	 * reads, not a comp.conf key. */
	char accent[64] = "";
	const char *cache = getenv("XDG_CACHE_HOME");
	if (cache && *cache)
		snprintf(path, sizeof(path), "%.500s/kdos/theme", cache);
	else
		snprintf(path, sizeof(path), "%.500s/.cache/kdos/theme",
			 kb_home_dir());
	if (kb_read_line_file(path, accent, sizeof(accent)) > 0 && accent[0])
		for (int i = 0; i < NROWS; i++)
			if (rows[i].store == ST_THEME) {
				kb_strlcpy(rows[i].val, accent,
					   sizeof(rows[i].val));
				kb_strlcpy(rows[i].orig, accent,
					   sizeof(rows[i].orig));
			}

	cfg_path("comp.conf", path, sizeof(path));
	load_kv(path, ST_COMP);
	cfg_path("panel.conf", path, sizeof(path));
	load_kv(path, ST_PANEL);
	cfg_path("res.conf", path, sizeof(path));
	load_kv(path, ST_RES);
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

static void apply(void)
{
	char path[700];
	int comp = dirty_count(ST_COMP), theme = dirty_count(ST_THEME);
	int panel = dirty_count(ST_PANEL), res = dirty_count(ST_RES);
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
	if (theme) {
		for (int i = 0; i < NROWS; i++) {
			if (rows[i].store != ST_THEME ||
			    !strcmp(rows[i].val, rows[i].orig))
				continue;
			/*
			 * Detached, not waited for: `kdos theme` regenerates
			 * ~10 000 icons and every cursor, which is seconds of
			 * work, and it sends its own SIGHUP to the whole
			 * desktop when it is done. A settings window frozen for
			 * the duration would look like the crash it is not.
			 */
			KbArgv a = {0};
			kb_argv_add(&a, "kdos");
			kb_argv_add(&a, "theme");
			kb_argv_add(&a, rows[i].val);
			kb_argv_end(&a);
			kb_run_detach(&a);
		}
	}

	if (failed) {
		snprintf(note, sizeof(note),
			 "could not write %d file(s) — nothing else changed",
			 failed);
		return;
	}
	for (int i = 0; i < NROWS; i++)
		kb_strlcpy(rows[i].orig, rows[i].val, sizeof(rows[i].orig));

	if (!live && !login && !theme)
		snprintf(note, sizeof(note), "nothing to apply");
	else if (login)
		snprintf(note, sizeof(note),
			 "applied: %d now, %d at the next login", live + theme,
			 login);
	else
		snprintf(note, sizeof(note), "applied: %d now", live + theme);
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
	if (!dirty_count(ST_COMP) && !dirty_count(ST_THEME))
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
	 * ◀ ▶ — so a literal `↑↓←→` here came out as `????` in the golden
	 * frame and would come out as four blanks on tty1. */
	ktui_draw_text(2, h - 3, w - 4,
		       "Enter opens a page   arrows move   Esc closes", KT_DIM,
		       KT_SURFACE, KT_A_NONE);
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
					       KT_DIM, KT_SURFACE, KT_A_NONE);
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
		ktui_draw_text(fx, y, 18, label, fg, bg, KT_A_NONE);

		/* The value, then the scope tag hard against the right edge:
		 * `live` and `login` are the two answers to "did that do
		 * anything", and they belong on the row that raises it. */
		const char *tag = r->scope == SC_LIVE ? "live" : "login";
		int vw = fw - 20 - 7;
		if (vw < 4)
			vw = 4;
		char shown[256];
		ktui_draw_text(fx + 19, y, vw,
			       elide(r->val[0] ? r->val : "(unset)", vw,
				     shown, sizeof(shown)),
			       on ? fg : (r->val[0] ? KT_MID : KT_DIM), bg,
			       KT_A_NONE);
		ktui_draw_text_right(0, y, w - 2, tag,
				     on ? fg : (r->scope == SC_LIVE ? KT_MID
							            : KT_DIM),
				     bg, KT_A_NONE);
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
			       KT_DIM, KT_SURFACE, KT_A_NONE);
	} else if (cat == CAT_BOXES && box_mode == BOX_LIST) {
		ktui_draw_text(2, h - 3, w - 4,
			       sel == 0 ? "Enter describes a new box; nothing "
					  "is created until you apply"
					: "Enter opens this box's profile",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
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
				       KT_DIM, KT_SURFACE, KT_A_NONE);
		else
			ktui_draw_text(2, h - 3, w - 4,
				       r && r->help ? r->help : "", KT_DIM,
				       KT_SURFACE, KT_A_NONE);
	} else {
		int ri = cat_row(sel);
		ktui_draw_text(2, h - 3, w - 4,
			       ri >= 0 && rows[ri].help ? rows[ri].help : "",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
	}

	int pending = dirty_count(ST_COMP) + dirty_count(ST_THEME);
	if (cat == CAT_BOXES)
		pending = box_mode == BOX_LIST ? 0 : box_dirty();
	/* The arrows come from the glyph table: the console font has ◀ ▶ and
	 * has no ← →, which is why ktui_glyph carries that pair. */
	char hint[96];
	if (editing)
		snprintf(hint, sizeof(hint), "Enter ok   Esc cancel");
	else if (cat == CAT_APPS)
		snprintf(hint, sizeof(hint),
			 "Enter chooser   Tab panes   Esc close");
	else if (cat == CAT_BOXES && box_mode == BOX_LIST)
		snprintf(hint, sizeof(hint),
			 "Enter open   Tab panes   Esc close");
	else if (cat == CAT_BOXES)
		snprintf(hint, sizeof(hint),
			 "%s%s change   Enter edit   a %s   Esc back",
			 ktui_glyph[KT_G_LEFT], ktui_glyph[KT_G_RIGHT],
			 box_mode == BOX_NEW ? "create" : "write");
	else
		snprintf(hint, sizeof(hint),
			 "%s%s change   Enter edit   a apply   Esc close",
			 ktui_glyph[KT_G_LEFT], ktui_glyph[KT_G_RIGHT]);
	ktui_draw_text(2, h - 2, w - 16, hint, KT_DIM, KT_SURFACE, KT_A_NONE);

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
		ktui_draw_text(btn_x, btn_row, 1, "[", KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(btn_x + 1, btn_row, bw - 2, blabel,
			       pending ? KT_SURFACE : KT_DIM,
			       pending ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		ktui_draw_text(btn_end - 1, btn_row, 1, "]", KT_DIM, KT_SURFACE,
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
	"dump-cells", cap_flush, cap_poll, cap_size, cap_caps
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
	if (!sr || sr->type == FT_NOTE)
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
	if (ri < 0 || rows[ri].type == FT_NOTE)
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

	KwlConfig cfg = {
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
		.role = KWL_ROLE_TOPLEVEL,
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
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-settings: no compositor or no "
				"layer-shell\n");
		return 2;
	}
	/* AFTER kwl_init: the icon layer needs the cell size and the output
	 * scale, and neither exists until the surface does. */
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	while (!kwl_should_close()) {
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
				case KT_K_ESC:
					if (try_quit())
						goto out;
					continue;
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
					pane = 1;
					sel = row;
					sel_follow = 1;
				}
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt = kch_scrollbar_press(0, ev.mx, ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel--;
				sel_follow = 1;
			} else if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel++;
				sel_follow = 1;
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
					/* A click on the row that is already
					 * selected activates it — pick.c's
					 * rule, so one hand learns one thing. */
					if (row == sel && pane == 1)
						activate();
					pane = 1;
					sel = row;
				}
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		sel_follow = 1;	/* a key moves the cursor; the view follows */

		if (editing) {
			struct row *er = sel_row();
			size_t len = strlen(edit_buf);
			if (ev.key == KT_K_ESC) {
				editing = 0;
			} else if (ev.key == KT_K_ENTER) {
				if (er) {
					if (er->type == FT_INT) {
						int v = atoi(edit_buf);
						if (v < er->min)
							v = er->min;
						if (v > er->max)
							v = er->max;
						snprintf(er->val,
							 sizeof(er->val),
							 "%d", v);
					} else {
						kb_strlcpy(er->val, edit_buf,
							   sizeof(er->val));
					}
				}
				editing = 0;
			} else if (ev.key == KT_K_BACKSPACE) {
				if (len)
					edit_buf[len - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   len + 1 < sizeof(edit_buf)) {
				edit_buf[len] = (char)ev.key;
				edit_buf[len + 1] = '\0';
			}
			continue;
		}

		if (ev.key == KT_K_ESC) {
			/* On a box's profile, back one level to the box LIST.
			 * Escape steps out of exactly one thing at a time,
			 * which is what makes it usable without thinking. */
			if (cat == CAT_BOXES && box_mode != BOX_LIST) {
				box_mode = BOX_LIST;
				box_cur[0] = '\0';
				sel = top = 0;
				note[0] = '\0';
				continue;
			}
			/* Back to the grid, not out of the program. Edits are
			 * held in `rows[]` and Apply is still one key away, so
			 * stepping back loses nothing — and the unsaved-changes
			 * guard stays at the single exit, on the home screen,
			 * rather than firing every time somebody backs out of a
			 * page they only wanted to look at. */
			mode = SM_HOME;
			home_sel = cat;
			quit_armed = 0;
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
			sel -= pane_rows;
			break;
		case KT_K_PGDN:
			sel += pane_rows;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
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
	kwl_shutdown();
	return 0;
}
