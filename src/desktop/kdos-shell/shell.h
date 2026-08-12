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

struct sh_task {
	void *handle;			/* zwlr_foreign_toplevel_handle_v1 * */
	char title[128];
	char app_id[64];
	int activated;
	int minimized;
};

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

	/* Where the last frame put things, so a click can be mapped back to
	 * what was drawn rather than to what the layout code intended. */
	int ws_hit_x, ws_hit_end;
	int task_hit_x, task_cell_w;
};

/* One binary, dispatched on its own basename — the same trick kdos-tools and
 * kdos-appbox use, and for the same reason: no shell wrapper anywhere in the
 * chain from a keybinding to a running program. */
int panel_main(int argc, char **argv);		/* kdos-shell    */
int launcher_main(int argc, char **argv);	/* kdos-launcher */
int notifyd_main(int argc, char **argv);	/* kdos-notifyd  */
int osd_main(int argc, char **argv);		/* kdos-osd      */

int sh_connect(struct sh_state *sh);
void sh_disconnect(struct sh_state *sh);
void sh_dispatch(struct sh_state *sh);
void sh_activate_task(struct sh_state *sh, int i);
void sh_activate_workspace(struct sh_state *sh, int i);

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
