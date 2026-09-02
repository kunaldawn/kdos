/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — the session, which draws nothing
 *
 * A compositor whose framebuffer is a character grid. Windows are rectangles
 * of cells, compositing is a z-ordered copy, damage is the diff libktui
 * already computes, and input routing is a hit test.
 *
 * IT HOLDS NO DISPLAY. A view attaches and provides one — on a KMS device, in
 * a terminal, or in a window of the graphical session — which is what makes
 * detach, reattach and a desktop over ssh fall out rather than be built.
 *
 * NO WINDOW-MODEL ARITHMETIC LIVES HERE. Placement, tiling, the edge search
 * and the ring walks are libkwm's, shared with kdos-comp, so a defect in any
 * of them is one fix.
 * ---------------------------------
 */

#ifndef CON_H
#define CON_H

#include <sys/types.h>

#include "kcon.h"
#include "kdisp.h"
#include "ktui.h"
#include "kvt.h"
#include "kwm.h"

/*
 * WIN_EMBED is a graphical application that IS a window: kdos-cage composites
 * it in a process of its own and hands back the pixels, and the session cuts
 * them into sprites so they occupy cells like any other picture. Chrome,
 * snapping, workspaces and the taskbar are then the ordinary ones.
 *
 * WIN_VT is a window with NO CELLS, kept for an application that needs
 * acceleration a software renderer cannot give it: the guest is full screen on
 * a terminal of its own, it is in the window list to be in the taskbar and the
 * Alt-Tab ring, and selecting it is a VT switch rather than a raise.
 */
enum { WIN_TERM = 0, WIN_SURFACE, WIN_VT, WIN_EMBED };

/* embed.c owns every byte of it; con.h needs only the pointer. */
struct Embed;

/* What a chord does. `arg` is a KWM_EDGE_* for a snap and a workspace index
 * for the two workspace actions; it is unused by the rest. */
enum {
	CON_ACT_NONE = 0,
	CON_ACT_TERM, CON_ACT_CLOSE, CON_ACT_QUIT,
	CON_ACT_MAX, CON_ACT_FULL, CON_ACT_MIN, CON_ACT_EXEC,
	CON_ACT_NEXT, CON_ACT_PREV,
	CON_ACT_SNAP, CON_ACT_WS, CON_ACT_SEND
};

/*
 * What CON_ACT_EXEC's `arg` names. The command itself is a con.conf key, so a
 * chord and the program it starts are configured in the two different files
 * that own them: which key runs the launcher is a keyboard question, and which
 * program IS the launcher is not.
 */
enum { CON_CMD_MENU = 0, CON_CMD_LAUNCHER, CON_CMD_LOCK, CON_CMD_SAVER,
       CON_CMD_N };

const char *con_command(int which);
void con_spawn(const char *cmd);

/*
 * The frame is one cell on every side, and the title sits in the top one —
 * which is why the content rect and the frame rect differ by exactly that.
 */
#define CON_FRAME 1

typedef struct Win {
	struct Win *next;
	int id;
	int kind;

	KwmRect geom;		/* the CONTENT, in cells */
	KwmRect restore;	/* what an untile returns to */
	unsigned tiled;
	int minimised;
	int full;
	int workspace;

	char title[128];
	char app_id[64];

	struct kvt_term *term;	/* WIN_TERM */
	KconSurface *surf;	/* WIN_SURFACE */
	struct Embed *em;	/* WIN_EMBED */

	/* WIN_VT: the terminal it was given, the terminal to come back to, and
	 * the compositor holding it. */
	int vt;
	int vt_home;
	pid_t vt_pid;

	/*
	 * A PANEL IS NOT A WINDOW. It is docked to an edge at a thickness it
	 * asked for, carries no frame, and when it reserves an exclusive zone
	 * it takes that space away from every window rather than sitting over
	 * them.
	 */
	int panel;
	int panel_edge;
	int exclusive;
} Win;

typedef struct {
	int cols, rows;
	int workspace;
	int nworkspace;
	int gap;

	Win *wins;		/* front of the list is the TOP of the stack */
	int focus;		/* window id, or 0 */
	int next_id;

	KconServer *server;
	char sock[128];

	/*
	 * THE SESSION OWNS THE LOCK, not the program that draws it. `locked`
	 * outlives the lock surface: a client that crashes leaves a locked
	 * screen with nothing on it, which is the entire reason a lock screen
	 * is not simply a fullscreen window. Only an explicit dismissal from a
	 * lock-role surface clears it.
	 */
	int locked;
	Win *lock;

	/*
	 * The saver, when one is up. NOT a second `locked` flag: it covers the
	 * grid and it is taken away by the first keystroke, so there is no
	 * state to outlive the surface — the window itself is the whole of it.
	 */
	Win *saver;
} Session;

extern Session S;

/* windows.c */
Win *win_find(int id);
Win *win_focused(void);
void win_raise(int id);
void win_close(Win *w);				/* ask */
void win_drop(Win *w);				/* and take it out */
void win_place(Win *w, int want_w, int want_h);
KwmRect win_frame(const Win *w);		/* the content rect, inflated */
KwmRect win_workarea(void);
Win *win_at(int x, int y);
void win_snap(Win *w, unsigned edge, int combine);
void win_resized(Win *w);
void win_maximise(Win *w);
void win_fullscreen(Win *w);
void win_minimise(Win *w);
void win_send(Win *w, int ws);
void win_cycle(int dir);
void win_draw_all(void);
void win_gc(void);
void win_dock(Win *w);
void win_lock_draw(void);

/* embed.c */
/*
 * HOW A GRAPHICAL APPLICATION IS SHOWN. Embedding is the default; a terminal of
 * its own is what an application needing acceleration a software renderer
 * cannot give it asks for, in its box profile.
 */
enum { CON_DISPLAY_EMBED = 0, CON_DISPLAY_VT };
int con_display_mode(const char *const argv[], const char **why);

Win *embed_open(const char *const argv[], const char *title);
void embed_pump(void);
void embed_reap(void);
void embed_resized(Win *w);
void embed_view_attached(void);
void embed_close(Win *w);
void embed_close_all(void);
void embed_free(Win *w);
int embed_alive(const Win *w);
int embed_fds(int *fds, int max);
void embed_draw(const Win *w);
int embed_key(Win *w, const KtuiEvent *ev);
int embed_ptr(Win *w, const KtuiEvent *ev);

/* vt.c */
Win *vt_open(const char *const argv[], const char *title, int cage);
void vt_reap(void);
int vt_show(Win *w);
void vt_close(Win *w);
void vt_close_all(void);

/* term.c */
Win *term_open(const char *const argv[]);
void term_pump_all(void);
int term_key(Win *w, const KtuiEvent *ev);

/* panel.c */
void panel_draw(void);
int panel_rows(void);
int panel_have_shell(void);

/* sessions.c */
int con_rundir(char *out, size_t cap);
int con_session_paths(const char *name, char *sock, size_t scap,
		      char *view, size_t vcap);
int con_sessions_list(void);
int con_session_kill(const char *name);

/* greet.c */
int con_login(const char *tty);

/* keys.c */
int keys_action(int key, int mods, int *arg);

/* main.c */
void con_quit(void);

#endif /* CON_H */
