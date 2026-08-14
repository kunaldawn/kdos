/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shell internals
 * ---------------------------------
 */

#ifndef KDOS_SHELL_H
#define KDOS_SHELL_H

#include <signal.h>	/* sig_atomic_t, for the live-retint flag below */

#include "ktui.h"
#include "kxdg.h"

#define SH_MAX_TASKS 64
#define SH_MAX_WS    16
#define SH_MAX_TRAY  16
#define SH_TRAY_NAME 128
#define SH_MAX_PRIV  8
#define SH_PRIV_NAME 48

enum { SH_PRIV_MIC = 0, SH_PRIV_CAM };

/* The clickable applets on the right of the top panel, right to left. */
enum { SH_AP_CLOCK = 0, SH_AP_BATT, SH_AP_VOL, SH_AP_NET, SH_AP_RESTART,
       SH_AP_N };

/* One app holding one capture device. `pid` is set for the camera half, which
 * finds its users in /proc; the microphone half comes from PipeWire, where a
 * stream need not have a pid at all. */
struct sh_priv_use {
	char name[SH_PRIV_NAME];
	int pid;
};

struct sh_priv;

enum { SH_TRAY_PASSIVE = 0, SH_TRAY_ACTIVE, SH_TRAY_ATTENTION };
enum { SH_TRAY_BTN_LEFT = 0, SH_TRAY_BTN_MIDDLE, SH_TRAY_BTN_RIGHT };

/*
 * One StatusNotifierItem. `service` + `path` is its address on the bus and
 * `owner` is the unique name behind it — both are kept because an item may
 * register under a well-known name and die under its unique one.
 */
struct sh_tray_item {
	char service[SH_TRAY_NAME];
	char path[SH_TRAY_NAME];
	char owner[SH_TRAY_NAME];
	char id[64];
	char title[64];
	char icon[64];
	int status;			/* SH_TRAY_* */
	int is_menu;			/* ItemIsMenu: Activate means "show menu" */
	int fdo_iface;			/* publishes the freedesktop spelling */
	int needs_props;		/* read them on the next dispatch */
};

struct sh_tray;

struct sh_task {
	void *handle;			/* zwlr_foreign_toplevel_handle_v1 * */
	char title[128];
	char app_id[64];
	/* The `Name` from the app's own desktop entry, resolved once when the
	 * app_id arrives. Empty when there is no entry. A taskbar reading
	 * `org.gnome.Meld` is showing an identifier chosen not to collide, in
	 * the one place a human name belongs. */
	char name[64];
	int activated;
	int minimized;
};

/* Name > title > app_id, the order a person reads them in. */
static inline const char *sh_task_label(const struct sh_task *t)
{
	if (t->name[0])
		return t->name;
	if (t->title[0])
		return t->title;
	return t->app_id;
}

/*
 * The menu bar: GNOME 2's three words, on the left of the top panel.
 *
 * The mark is `≡` where an icon theme would have put a distributor logo — on a
 * character grid a logo is one cell, and three horizontal rules is what that
 * cell can honestly hold. It falls back to the word KDOS on a font without it,
 * which is checked once rather than per frame.
 */
#define SH_NMENUS 3
#define SH_MENU_MARK "\xe2\x89\xa1"		/* U+2261 IDENTICAL TO */
extern const char *const sh_menu_labels[SH_NMENUS];
/* Spawn kdos-menu for one of them, anchored at (x, y) in PIXELS — where the
 * word that was clicked starts, and how far down the panel reaches. Layer-shell
 * has no coordinates, so that pair becomes an anchor and a margin. Double-
 * forked, so the panel neither reaps nor blocks: a menu that takes a moment to
 * scan 400 desktop files must not stop the clock. */
void sh_spawn_menu(int which, int x, int y);

struct sh_state {
	void *display;			/* the panel shares libkwl's connection */
	void *ftl_mgr;
	void *ws_mgr;

	struct sh_task tasks[SH_MAX_TASKS];
	int ntasks;

	void *ws[SH_MAX_WS];
	int ws_occupied[SH_MAX_WS];
	int nws;
	int active_ws;

	/* The tray host, opaque: it is all sd-bus and nothing else needs its
	 * shape. NULL when there is no session bus, which is a session without
	 * a tray rather than a session that failed to start. */
	struct sh_tray *tray;

	/* The microphone/camera indicator (privacy.c). Opaque for the same
	 * reason the tray is: it is a PipeWire client and a /proc walk, and the
	 * panel needs two counts and two names out of it. */
	struct sh_priv *priv;

	/* Where the last frame put things, so a click can be mapped back to
	 * what was drawn rather than to what the layout code intended. */
	int ws_hit_x, ws_hit_end;
	/*
	 * And where each workspace digit STARTS. A fixed two-cell stride was
	 * right until somebody set `<desktops number="12"/>`: from ten on, the
	 * label is two cells wide plus its separator, every later digit is
	 * offset by one more, and clicking the strip activated the wrong
	 * workspace. The draw records what it drew.
	 */
	int ws_hit[SH_MAX_WS];
	int menu_hit_x[SH_NMENUS], menu_hit_end[SH_NMENUS];
	int menu_open;			/* which label is lit, or -1 */
	/* What the pointer is over, or -1. The panel and the menu are two
	 * processes, so `menu_open` is never set by anything; hover is what
	 * the bar actually knows, and it is what makes three words read as
	 * three buttons. */
	int hover_menu;
	int hover_task;
	/* The bottom panel's hit map: the pager and show-desktop. */
	int pager_hit_x, pager_hit_end;
	int show_hit_x;
	int task_hit_x, task_cell_w;
	int tray_hit_x, tray_hit_end;

	/*
	 * The right wing's applets, and where the last frame put each of them.
	 *
	 * Everything on that side of the panel used to be a picture: the clock,
	 * the battery and the "something needs restarting" mark were drawn and
	 * answered nothing, and there was no volume or network indicator at all
	 * — so on a laptop with no media keys there was NO WAY to change the
	 * volume from the desktop, and nothing said whether the machine was on
	 * a network. Each is a span now, and each does the obvious thing when
	 * it is clicked.
	 */
	int ap_x[SH_AP_N], ap_end[SH_AP_N];
};

/* One binary, dispatched on its own basename — the same trick kdos-tools and
 * kdos-appbox use, and for the same reason: no shell wrapper anywhere in the
 * chain from a keybinding to a running program. */
int panel_main(int argc, char **argv);		/* kdos-shell    */
int launcher_main(int argc, char **argv);	/* kdos-launcher */
int menu_main(int argc, char **argv);		/* kdos-menu     */
int desk_main(int argc, char **argv);		/* kdos-desk     */
int pick_main(int argc, char **argv);		/* kdos-pick     */
int asciicmd_main(int argc, char **argv);	/* kdos-ascii    */
int run_main(int argc, char **argv);		/* kdos-run      */
int prompt_main(int argc, char **argv);		/* kdos-prompt   */


int notifyd_main(int argc, char **argv);	/* kdos-notifyd  */
int osd_main(int argc, char **argv);		/* kdos-osd      */
int cal_main(int argc, char **argv);		/* kdos-cal      */
int display_main(int argc, char **argv);	/* kdos-display  */

/*
 * The ALSA mixer, shared with the panel (osd.c).
 *
 * The OSD owns this code because it is the thing that CHANGES the volume, and
 * the panel needs the same numbers to draw them — two readers of one mixer,
 * never two implementations of what "62%" means. `sh_volume_get` returns -1
 * when the machine has no mixer at all, which is a panel with no volume applet
 * rather than a panel that lies about one.
 */
int sh_volume_get(int *muted);
void sh_volume_set(int pct);
void sh_volume_toggle(void);

int sh_connect(struct sh_state *sh);
void sh_disconnect(struct sh_state *sh);
void sh_dispatch(struct sh_state *sh);
void sh_activate_task(struct sh_state *sh, int i);
/* Left click on a task entry: minimise the window you are in, restore the one
 * you are not — what every taskbar does, and what makes the entry worth
 * clicking once the window is already on screen. */
void sh_toggle_task(struct sh_state *sh, int i);
/* Middle click: the protocol's polite close, so an editor still gets to ask. */
void sh_close_task(struct sh_state *sh, int i);
/* What show-desktop is made of. Already-minimised windows are left alone, so
 * pressing it twice does not un-minimise half the screen. */
void sh_minimize_task(struct sh_state *sh, int i);
void sh_activate_workspace(struct sh_state *sh, int i);

/*
 * The tray: StatusNotifierWatcher + Host on the session bus (tray.c). Init
 * failing is not fatal — a session with no bus is a session with no tray.
 * Dispatch is called once per panel loop; nothing here ever blocks on an app.
 */
int sh_tray_init(struct sh_state *sh);
void sh_tray_dispatch(struct sh_state *sh);
void sh_tray_free(struct sh_state *sh);
int sh_tray_count(const struct sh_state *sh);
const struct sh_tray_item *sh_tray_get(const struct sh_state *sh, int i);
void sh_tray_activate(struct sh_state *sh, int i, int button, int x, int y);

/*
 * Which app is recording (privacy.c). `count` is how many apps hold that kind
 * of device right now and `name` is the first of them — the panel has one row,
 * so it names one and counts the rest.
 */
int sh_priv_init(struct sh_state *sh);
void sh_priv_dispatch(struct sh_state *sh);
/* Wait out PipeWire's roundtrips, up to `ms`. Only --dump needs it. */
void sh_priv_settle(struct sh_state *sh, int ms);
void sh_priv_free(struct sh_state *sh);
int sh_priv_count(const struct sh_state *sh, int kind);
const char *sh_priv_name(const struct sh_state *sh, int kind);

/* One line from a /sys or /proc file, newline stripped. Returns 0 on success.
 * There is no libkbase here — the shell links libktui, libkcolor and libkwl,
 * and one 15-line reader is cheaper than dragging in another archive. */
int sh_read_line(const char *path, char *buf, size_t len);

/*
 * How many processes are running code an upgrade replaced, or -1 when the
 * check could not run. Re-checked rarely: `kdos restarts` walks every process's
 * maps, which is far too much work for a panel redraw, and the answer only
 * changes when a package is installed.
 */
int sh_restart_count(void);

/* The accent the desktop is currently wearing, from
 * $XDG_CACHE_HOME/kdos/theme. The same file kdos-appbox's TUI reads. */
void sh_theme_from_cache(void);

/*
 * Live retint. A long-lived surface (the panel, the notification daemon) calls
 * sh_theme_watch() once and then checks sh_theme_dirty each time round its
 * loop: on set, clear it, sh_theme_from_cache(), ktui_draw_invalidate(). The
 * short-lived front ends (launcher, menu, osd, pick) need neither — they read
 * the accent when they start, which is after the change.
 */
extern volatile sig_atomic_t sh_theme_dirty;
void sh_theme_watch(void);

#endif /* KDOS_SHELL_H */
