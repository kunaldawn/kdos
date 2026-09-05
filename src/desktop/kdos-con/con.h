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
	CON_ACT_SNAP, CON_ACT_WS, CON_ACT_SEND, CON_ACT_RESTORE,
	CON_ACT_FOCUS_DIR, CON_ACT_SWAP_DIR, CON_ACT_WS_STEP,
	/*
	 * THE KEYBOARD'S OWN WINDOW MANAGEMENT, and the reason it is worth
	 * having on a desktop that already has a pointer: every one of these
	 * was a menu item on the text desks this one descends from, and a
	 * person whose hands are on the keys should not have to reach for a
	 * mouse to make five windows visible.
	 */
	CON_ACT_TILE,		/* fill the work area with the workspace   */
	CON_ACT_CASCADE,	/* a staircase, each offset from the last  */
	CON_ACT_REARRANGE,	/* arrows move, Shift+arrows size          */
	CON_ACT_SHOW_DESKTOP,	/* hide every window; again brings them back */
	CON_ACT_WIN_N,		/* raise window N of the ring, arg 1..9    */
	CON_ACT_WINLIST,	/* the window list                         */
	/*
	 * MARK AND TRANSFER, which DESQview had in 1985 and which is cheaper
	 * here than anywhere: the session holds the literal text of every cell
	 * on the screen, so marking a rectangle is a read of a buffer that
	 * already exists rather than a protocol between two programs.
	 */
	CON_ACT_MARK,		/* mark a rectangle of the screen          */
	CON_ACT_PASTE,		/* the session clipboard into the window   */

	/*
	 * NOT AN ACTION — A PREFIX. The next key is looked up as though Super
	 * were held, which is the only way the chord table can be reached from
	 * a view whose terminal never reports Super. Pressing it twice sends
	 * the literal on to the window, so the key it occupies is not lost.
	 */
	CON_ACT_LEADER
};

/*
 * What CON_ACT_EXEC's `arg` names. The command itself is a con.conf key, so a
 * chord and the program it starts are configured in the two different files
 * that own them: which key runs the launcher is a keyboard question, and which
 * program IS the launcher is not.
 */
/*
 * The programs a chord starts, each a con.conf key with a default.
 *
 * The four at the top are the ones an image may replace — a different
 * launcher, a different lock. The rest are this desktop's own surfaces, and
 * they are here rather than as literals in the bind table for the same reason
 * the four are: one place says which program a chord runs, and `kdos-con
 * --keys` prints it, so the card cannot name a program the session does not
 * start.
 */
enum { CON_CMD_MENU = 0, CON_CMD_LAUNCHER, CON_CMD_LOCK, CON_CMD_SAVER,
       CON_CMD_KEYS, CON_CMD_AUDIO, CON_CMD_NET, CON_CMD_BT, CON_CMD_DEVICES,
       CON_CMD_SETTINGS, CON_CMD_CAL, CON_CMD_DOC, CON_CMD_DISPLAY,
       CON_CMD_ENERGY, CON_CMD_RES,
       /* The accessories: summoned over whatever is on screen and dismissed
        * leaving it untouched, which is what Sidekick sold a million copies
        * of and what the overlay role already gives this desktop for free. */
       CON_CMD_CALC, CON_CMD_NOTE, CON_CMD_CLIP,
       CON_CMD_N };

const char *con_command(int which);
void con_spawn(const char *cmd);
/* True while a window is being moved or sized from the keyboard. The frame
 * says so and the taskbar names the keys; see CON_ACT_REARRANGE. */
int con_rearranging(void);
/* True while a rectangle of the screen is being marked. The taskbar names the
 * keys for the same reason; see CON_ACT_MARK. */
int con_marking(void);
/* True while a paste that would execute is waiting to be meant twice. */
int con_paste_armed(void);
/* Draw the mark over the composed grid. Called last, after every window. */
void con_mark_draw(void);
void con_spawn_at(const char *cmd, int x);

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

	/*
	 * THE SELECTION IN THIS TERMINAL, for WIN_TERM only. libkvt decides
	 * what a drag means — the same code `kdos-term` runs — and this is the
	 * per-window state it keeps for the caller.
	 */
	KvtUi ui;
	int full;
	int workspace;

	/*
	 * LAYERS, NOT TOPLEVELS. Both roles are declared in `kdisp.h` with
	 * these semantics and every surface that asks for one already means
	 * them: an OVERLAY is a menu, a toast, a tooltip or a candidate
	 * window, and a BACKGROUND is the desktop's own icon layer.
	 *
	 * A surface given neither flag arrives framed, shadowed, listed in the
	 * taskbar and in the cycle ring — which for a toast is worse than no
	 * toast at all.
	 */
	int overlay;		/* above every window, out of the ring */
	int background;		/* below every window, out of the ring */

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

	/*
	 * WHEN THIS WINDOW'S BELL STOPS SHOWING, in con_now_ms() terms, or 0.
	 * A visible bell is a deadline rather than a countdown because the
	 * frame loop is the only clock: a counter decremented per frame stops
	 * being milliseconds the moment a frame takes longer than one.
	 */
	unsigned long long bell_until;
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
void win_place_corner(Win *w, int want_w, int want_h, int corner, int mx,
		      int my);
KwmRect win_frame(const Win *w);		/* the content rect, inflated */
KwmRect win_workarea(void);
Win *win_at(int x, int y);
void win_snap(Win *w, unsigned edge, int combine);
void win_resized(Win *w);
void win_maximise(Win *w);
void win_fullscreen(Win *w);
void win_minimise(Win *w);
void win_restore(Win *w);
Win *win_last_minimised(void);
void win_send(Win *w, int ws);
void win_workspace(int ws);
/* The session's monotonic clock, in milliseconds. */
unsigned long long con_now_ms(void);
/*
 * A TERMINAL RANG. The window is marked for the flash and every attached view
 * is told, because the audible half belongs to whichever display the person is
 * sitting at and the visible half belongs to whoever owns the cells.
 */
void con_bell(Win *w);
/* Paste into a window, through the guard: an unbracketed payload carrying a
 * newline is refused once and taken on the second try, because the session
 * cannot raise a modal over a window whose toolkit it does not own. */
void con_paste_win(Win *w, const char *text);
/* The rectangle a tiled state asks for, maximise included. See windows.c. */
KwmRect win_tile_rect(unsigned tiled);
Win *win_dir(unsigned dir);
void win_swap(Win *a, Win *b);
void win_workspace_step(int reverse);
/*
 * ARRANGEMENTS, over the windows of the current workspace in ring order.
 * Neither is a tile STATE: both clear `tiled`, so a Super+arrow afterwards
 * snaps from the new rectangle rather than from a half nothing put it in.
 */
void win_tile_all(void);
void win_cascade(void);
/*
 * HIDE EVERY WINDOW, and remember the set so the same chord brings back
 * exactly those. A window opened while the desktop is showing is left alone —
 * it was not hidden, so it is not something to restore.
 */
void win_show_desktop(void);
/* The Nth reachable window of the current workspace in ring order, 1-based. */
Win *win_nth(int n);
/* Its position in that ring, 1-based, or 0 when it is not in it. */
int win_index(const Win *w);
void win_cycle(int dir);
void win_draw_all(void);
/* The window list: Turbo Vision's Alt+0, drawn by the session until Task
 * 6.4 lets kdos-teams read the list over libkdisp. */
void win_list_toggle(void);
int win_list_active(void);
void win_list_draw(void);
int win_list_key(int key);
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
void term_mouse(Win *w, const KtuiEvent *ev);
void term_paste(Win *w, int primary);
void term_pump_all(void);
int term_key(Win *w, const KtuiEvent *ev);

/* panel.c */
/*
 * WHAT A CLICK ON THE PANEL ROW LANDED ON. The row is painted by the session
 * and is not a window, so `win_at()` cannot answer for it and the spans are
 * recorded as each element is drawn rather than re-derived afterwards.
 */
enum {
	PANEL_HIT_NONE = 0,
	PANEL_HIT_START,	/* arg unused                              */
	PANEL_HIT_WIN,		/* arg is the window id                    */
	PANEL_HIT_CLOCK,	/* arg unused                              */
	PANEL_HIT_WS,		/* arg is the workspace index              */
	PANEL_HIT_FKEY		/* arg is 1..10 — the Super+F<n> it names   */
};

#define PANEL_HITS 72

typedef struct {
	int x0, x1;
	int kind, arg;
} PanelHit;

/*
 * THE THREE BUTTONS ON A WINDOW FRAME, and where each one was drawn.
 *
 * Recorded as the frame is painted rather than re-derived from the window's
 * rectangle, which is the rule every hit map in this tree follows: a title is
 * truncated to what fits and a frame is clipped to the work area, so a second
 * calculation is a second thing to get wrong.
 */
enum {
	WIN_BTN_NONE = 0,
	WIN_BTN_MIN,
	WIN_BTN_MAX,
	WIN_BTN_CLOSE
};

int win_button_at(int x, int y, int *id);

/*
 * THE SELECTION, held by the session because nothing else can hold it: a
 * client that owned its own bytes would take them with it when it exited,
 * which is not what a person means by copying.
 */
void clip_offer(KconSurface *f, const char *text, size_t len, int primary,
		void *user);
void clip_request(KconSurface *f, int primary, void *user);
void clip_put(const char *text, size_t len, int primary);
const char *clip_get(int primary, size_t *len);
void clip_free(void);

/*
 * THE WINDOW LIST, out to the shell. Published by comparing a snapshot against
 * the last one rather than by a call at every place window state changes:
 * a diff has one place to be wrong, and it cannot miss a path that did not
 * exist when it was written.
 */
void mgmt_publish(int force);
void mgmt_resend(void);

void panel_draw(void);
int panel_rows(void);
int panel_span_x0(int kind);
int panel_have_shell(void);
/* True when con.conf asks for the function-key row instead of the window
 * list. See panel.c. */
int panel_fkeys(void);
int panel_hit(int x, int y, int *arg);

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
void keys_print(void);

/* main.c */
void con_quit(void);

#endif /* CON_H */
