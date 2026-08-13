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

#include "ktui.h"
#include "kxdg.h"

#define SH_MAX_TASKS 64
#define SH_MAX_WS    16
#define SH_MAX_TRAY  16
#define SH_TRAY_NAME 128
#define SH_MAX_PRIV  8
#define SH_PRIV_NAME 48

enum { SH_PRIV_MIC = 0, SH_PRIV_CAM };

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
	int activated;
	int minimized;
};

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
/* Spawn kdos-menu for one of them. Double-forked, so the panel neither reaps
 * nor blocks — a menu that takes a moment to scan 400 desktop files must not
 * stop the clock. */
void sh_spawn_menu(int which);

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
	int menu_hit_x[SH_NMENUS], menu_hit_end[SH_NMENUS];
	int menu_open;			/* which label is lit, or -1 */
	/* The bottom panel's hit map: the pager and show-desktop. */
	int pager_hit_x, pager_hit_end;
	int show_hit_x;
	int task_hit_x, task_cell_w;
	int tray_hit_x, tray_hit_end;
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


int notifyd_main(int argc, char **argv);	/* kdos-notifyd  */
int osd_main(int argc, char **argv);		/* kdos-osd      */

int sh_connect(struct sh_state *sh);
void sh_disconnect(struct sh_state *sh);
void sh_dispatch(struct sh_state *sh);
void sh_activate_task(struct sh_state *sh, int i);
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
 * $XDG_CACHE_HOME/kdos/theme. The same file kdos-appbox's TUI reads, so a
 * `kdos theme amber` retints the panel on its next start. */
void sh_theme_from_cache(void);

#endif /* KDOS_SHELL_H */
