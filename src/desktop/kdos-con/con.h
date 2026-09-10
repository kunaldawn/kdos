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
	 * THE SAME RECTANGLE, TWICE. A capture is a mark that also files a
	 * picture: the cells it covers go on the clipboard as text, and their
	 * coordinates go to CON_CMD_CAPTURE, which asks a second view to
	 * rasterise them. Text and picture are the same region because they
	 * come from the same drag, and a person who marked a paragraph gets
	 * both without choosing between them first.
	 */
	CON_ACT_CAPTURE,	/* mark, copy, and file the picture        */

	/*
	 * THE SCREEN'S FONT, ONE STEP AT A TIME.
	 *
	 * The session decides and the display acts, the same split every other
	 * device verb here keeps: the font belongs to the view, which is the
	 * half holding a rasteriser, and the chord belongs to the session,
	 * which is the half holding a keyboard table. A view inside somebody
	 * else's terminal says so in its hello and is told nothing — the
	 * session answers the person instead, because that terminal owns the
	 * font and no message from here can change it.
	 */
	CON_ACT_FONT_UP,
	CON_ACT_FONT_DOWN,
	CON_ACT_FONT_RESET,

	/*
	 * NOT AN ACTION — A PREFIX. The next key is looked up as though Super
	 * were held, which is the only way the chord table can be reached from
	 * a view whose terminal never reports Super. Pressing it twice sends
	 * the literal on to the window, so the key it occupies is not lost.
	 */
	CON_ACT_LEADER,

	/*
	 * A SCRIPT IS KEYS, NOT A COMMAND. `learn` records what reaches the
	 * focused window until it is pressed again and binds the lot to a
	 * letter; `play` types one back into whatever has the focus now. That
	 * is the whole of what a script can be here — a file that named a
	 * program would be a file that runs one, and it is written by
	 * something a person typed into.
	 */
	CON_ACT_LEARN,
	CON_ACT_PLAY,

	/*
	 * ONE KEY PER PROGRAM: raise the one that is running, or start it.
	 *
	 * `arg` is a CON_APP_* index, so the chord names a ROLE — mail, music,
	 * writing — and `con.conf` names the program that fills it. A bind
	 * table carrying program names would be a keyboard file deciding which
	 * mail reader this machine has.
	 *
	 * A SECOND PRESS WHILE THAT WINDOW IS FOCUSED CYCLES to the next
	 * window of the same program, which is what makes one chord enough for
	 * three terminals.
	 */
	CON_ACT_FOCUS_OR_LAUNCH,

	/*
	 * THE SCRATCHPAD. One window a session keeps over everything, on the
	 * key the drop-down terminals have used since Quake.
	 *
	 * SHOW AND HIDE ARE ONE CHORD because there is only ever one of them:
	 * a second key to put it away is a key a person has to remember for a
	 * window they are looking at. The toggle SHOWS a scratchpad that is
	 * hidden or minimised and hides one that is neither, so the two states
	 * a window can be invisible in both answer the same press.
	 */
	CON_ACT_SCRATCH,
	CON_ACT_SCRATCH_MARK
};

/*
 * The roles CON_ACT_FOCUS_OR_LAUNCH's `arg` names.
 *
 * EVERY ONE OF THESE RUNS IN A TERMINAL, and that is what makes a single
 * action enough: the window is `term_open()`'s, on this grid, and there is no
 * display-mode decision to make. A graphical program would need the one
 * `on_run()` makes for a launch that arrives over the socket, and none of
 * these is graphical — the browser here is a text browser.
 *
 * APPENDED, NEVER INSERTED, for the reason CON_CMD_* states below: main.c's
 * table is a designated-initialiser array indexed by this enum.
 */
enum { CON_APP_FILES = 0, CON_APP_MAIL, CON_APP_BROWSER, CON_APP_MUSIC,
       CON_APP_AGENDA, CON_APP_CHAT, CON_APP_WRITE,
       CON_APP_N };

const char *con_app(int which);
/* The role's own name — `files`, `writing` — rather than the program it
 * resolves to. A layout row names a role, so something has to be able to ask
 * the table that binds the key whether a name is one of its own. */
const char *con_app_name(int which);

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
       CON_CMD_CALC, CON_CMD_NOTE, CON_CMD_CLIP, CON_CMD_CHARS,
       CON_CMD_FIND, CON_CMD_CAPTURE, CON_CMD_THEME, CON_CMD_BACKGROUND,
       /*
        * THE MEDIA KEYS, and they run a program rather than doing anything
        * themselves: what "louder" means is the mixer's and what "next" means
        * is the player's, and neither is the window manager's business.
        *
        * A KMS VIEW ONLY. A media key produces no character, so no terminal
        * reports one and a view that reads a terminal never sees one — the
        * chord exists on tty1 and over ssh it does not.
        */
       CON_CMD_VOLUP, CON_CMD_VOLDOWN, CON_CMD_MUTE,
       CON_CMD_PLAY, CON_CMD_STOP, CON_CMD_NEXT, CON_CMD_PREV,
       /*
        * APPENDED, NEVER INSERTED. main.c's table is a designated-initialiser
        * array indexed by this enum, so a value added in the middle repoints
        * every command below it at a different program — and it compiles
        * clean, because every index still has an initialiser.
        */
       CON_CMD_PALETTE,
       /* The palette opened at the capture group. APPENDED, like every id
        * here: main.c's tables are designated-initialiser arrays indexed by
        * this enum. */
       CON_CMD_CAPTMENU,
       /* The whole screen with no rectangle to draw, and the screen into a
        * file. APPENDED, like every id here. */
       CON_CMD_CAPTSCREEN, CON_CMD_RECORD,
       CON_CMD_N };

const char *con_command(int which);
/* The con.conf key itself — `monitor`, `notes` — rather than the command it
 * resolves to; see con_app_name(). */
const char *con_command_name(int which);
void con_spawn(const char *cmd);
/* True while a window is being moved or sized from the keyboard. The frame
 * says so and the taskbar names the keys; see CON_ACT_REARRANGE. */
int con_rearranging(void);
/* True while a rectangle of the screen is being marked. The taskbar names the
 * keys for the same reason; see CON_ACT_MARK. */
int con_marking(void);
/* True while the colour picker is waiting for a click. The taskbar says so and
 * names the way out, for the reason the mark does. */
int con_picking(void);
/* True while a paste that would execute is waiting to be meant twice. */
int con_paste_armed(void);

/*
 * A LINE THE BAR SHOWS FOR A MOMENT, and the text it is showing or NULL.
 *
 * The console has one row that is always on screen and nothing else that can
 * answer a person immediately — a notification is a window, and a chord that
 * could not do what was asked has to say so before any window exists. It
 * expires on its own; a message that stayed would be a bar that had stopped
 * being a taskbar.
 */
void con_notice(const char *text);
const char *con_notice_text(void);
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
	 * THE SCRATCHPAD'S TWO FLAGS, and neither is a shade of `minimised`.
	 *
	 * `sticky` means the window is on NO workspace and therefore on every
	 * one: `workspace` is not consulted for it anywhere, and it is counted
	 * in no workspace's occupancy — a dot under every number would say the
	 * desk is full when only the scratchpad is open.
	 *
	 * `hidden` means drawn nowhere, listed nowhere and hit-testable
	 * nowhere. That is what separates it from a minimise, whose entire
	 * point is that the taskbar row IS the way back: a hidden window has
	 * no row, and the chord that hid it is the only thing that brings it
	 * back.
	 */
	int sticky;
	int hidden;

	/*
	 * THE SELECTION IN THIS TERMINAL, for WIN_TERM only. libkvt decides
	 * what a drag means — the same code `kdos-term` runs — and this is the
	 * per-window state it keeps for the caller.
	 */
	KvtUi ui;
	/* The OSC 8 link the pointer is over in THIS terminal, 0 for none. Per
	 * window, because the pointer is over one of them and the others must
	 * not light up. */
	unsigned int hover;
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

	/*
	 * THE PROGRAM THIS WINDOW WAS OPENED FOR, written once and never
	 * again. Neither field above can answer that question: `title` is the
	 * guest's to rewrite the moment it emits an OSC, and `app_id` says
	 * what KIND of window this is — every WIN_TERM is "terminal" and every
	 * caged guest is "kdos-cage" — so a run-or-raise matching on either
	 * would find the wrong window or none. Empty when nothing named a
	 * program, and an empty `prog` matches nothing.
	 */
	char prog[64];

	/*
	 * THE SMALLEST GRID THIS WINDOW CAN BE GIVEN, from the surface's
	 * attach. Zero is no minimum, which is what a terminal reports — a
	 * terminal reflows to any size, and the program inside it decides for
	 * itself whether it can draw.
	 */
	int min_w, min_h;

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

	/*
	 * THE SCRATCHPAD'S WINDOW ID, or 0. An id rather than a pointer for
	 * the reason every other window is named by one here: the window can
	 * be closed by its own client between two presses, and a stale pointer
	 * is a crash where a stale id is a lookup that returns NULL.
	 */
	int scratch;
} Session;

extern Session S;

/* windows.c */
Win *win_find(int id);
/*
 * The next window running `prog`, starting after window id `after` (0 for the
 * first). Minimised windows and other workspaces ARE included: a run-or-raise
 * that skipped them would start a second copy of a program already open.
 */
Win *win_find_prog(const char *prog, int after);
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
/* A window exactly where it was, clamped to the screen it comes back on. What
 * a restored session uses; `win_place` is for a window that has no place yet. */
void win_place_at(Win *w, int x, int y, int cw, int ch);
void win_maximise(Win *w);
void win_fullscreen(Win *w);
void win_minimise(Win *w);
void win_restore(Win *w);
Win *win_last_minimised(void);
void win_send(Win *w, int ws);
void win_workspace(int ws);
/*
 * THE SCRATCHPAD, or NULL when the session has none. Asked rather than read:
 * `S.scratch` outlives the window it names when a client goes away, and this
 * clears it in the one place that looks.
 */
Win *win_scratch(void);
/* Over everything, on the workspace being looked at, in the drop-down shape.
 * The shape is applied on every show rather than remembered, so a scratchpad
 * survives a resize of the grid instead of coming back off the screen. */
void win_scratch_show(Win *w);
void win_scratch_hide(Win *w);
/* Make this window the scratchpad. The previous one — there is at most one —
 * returns to the workspace being looked at as an ordinary window, because a
 * window left sticky and hidden with no chord naming it is a window nothing
 * can reach. */
void win_scratch_mark(Win *w);
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

/* ── geom.c ────────────────────────────────────────────────────────────── */

/*
 * WINDOWS REOPEN WHERE YOU LEFT THEM. A rectangle per program per workspace,
 * in `~/.local/state/kdos/con/geometry`, written when a window goes and used
 * when one running the same program next appears.
 *
 * `geo_recall` is called from `win_place()` and nowhere else, which is what
 * decides the roles: an overlay is placed by `win_place_corner()` and a
 * restored session by `win_place_at()`, so neither can inherit a terminal's
 * rectangle and the session record still wins over this one. It answers 1 when
 * it has set `geom`, and the placement search runs when it answers 0.
 *
 * The file is NOT the compositor's `winpos` — those rectangles are pixels and
 * these are cells, so one file with both writers would restore every window at
 * a size taken from the other desktop's units.
 */
int geo_recall(Win *w);
void geo_record(const Win *w);

/* ── state.c ───────────────────────────────────────────────────────────── */

/* How many windows one saved session may carry. A list longer than this is a
 * session nobody arranged, and the file is a person's state directory rather
 * than an archive. */
#define CON_STATE_MAX 64

/* `$XDG_STATE_HOME/kdos/con/<name>.session`. 0 when there is nowhere to put
 * it, or the name could not be a file name. */
int con_state_path(const char *name, char *out, size_t n);
/* What is open, as text. Returns the row count, or -1. */
int con_state_save(const char *name);
/* The rows a save would write, rendered into `buf`. A layout is the same rows
 * under a different name, and one renderer is what stops the two files from
 * becoming two formats. */
int con_state_rows(char *buf, size_t cap, const char *name, int text);

/* ── layout.c ──────────────────────────────────────────────────────────── */

/*
 * AN ARRANGEMENT OF WINDOWS, WITH A NAME. `save` writes what is open to
 * `~/.config/kdos-con/layouts/<name>`; `load` opens every entry of that file
 * — or of `/usr/share/kdos/layouts/<name>` — that is not already open, and
 * closes nothing. Both answer how many rows they wrote or opened, or -1.
 *
 * A ROW NAMES A ROLE AND `con.conf` NAMES THE PROGRAM, which is what lets a
 * layout hold a file manager at all: every terminal window's app id is the
 * literal `terminal`, so the record cannot say which program a terminal was
 * running, and a file that named the program would be naming an argv.
 */
int con_layout_save(const char *name);
int con_layout_load(const char *name);

/*
 * WHAT ONE ROW NAMES, and the one place that decides. A row is resolved by
 * asking the tables that already exist, most specific first: `term` is
 * con.conf's terminal, a role name is con.conf's key for that role opened in a
 * terminal, a con.conf command key is one of this desktop's own surfaces, and
 * anything else is an app id for `kdos-appbox run`. The session record reads
 * its rows through this too, so a restored session and a loaded layout cannot
 * disagree about what a row means.
 */
enum { CON_ROW_NONE = 0, CON_ROW_TERM, CON_ROW_ROLE, CON_ROW_SURFACE,
       CON_ROW_APP };
int con_layout_resolve(const char *kind, const char *app, const char **cmd);
/* The role a program fills, or NULL — what the writer puts in a row for a
 * terminal, since a terminal's app id says only that it is one. */
const char *con_layout_role_of(const char *prog);
/*
 * Open what was. Terminals come back through `con.conf`'s own `terminal` key
 * and applications through their desktop entry by `app_id` — NEVER a command
 * line replayed from the file, which is written by a program and read by a
 * program. Returns how many rows it acted on.
 */
int con_state_restore(const char *name);
/*
 * The saved rectangle for an app_id, taken once. A restored application's
 * window does not exist until it attaches, so this is what carries the place
 * across that gap; a second window of the same application is placed the
 * ordinary way.
 */
/* The flags column of one row — the eighth field, or "-" when the row was
 * written before the column existed. */
void con_state_flags(const char *line, char *out, size_t n);
/* What a rectangle cannot say, put back. After the placement, because both
 * flags REPLACE the rectangle rather than adjust it. */
void con_state_apply_flags(Win *w, const char *flags);
int con_state_take(const char *app_id, int *ws, int *x, int *y, int *w,
		   int *h, char *flags, size_t nflags);
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
/* The reader's socket, from the view's. See sessions.c. */
int con_a11y_path(const char *view, char *out, size_t n);
/* ── scripts.c ─────────────────────────────────────────────────────────── */

/* True while keys are being recorded, and while the letter prompt is up. */
int scr_learning(void);
int scr_prompt_active(void);
/* Start, or stop and ask for a letter. Refused, with the reason said, while
 * the screen is locked. */
void scr_learn_toggle(void);
/* One key the session is about to route to a window. */
void scr_note(const KtuiEvent *ev);
/* The letter, or Escape. 1 when the prompt consumed the key. */
int scr_prompt_key(const KtuiEvent *ev);
/* What the taskbar draws while a recording or a prompt is up, or NULL. */
const char *scr_status(void);
/* Arm `play`: the next key is the letter. */
void scr_play_arm(void);
int scr_play_armed(void);
/* Queue a script to be typed into the focused window. 0 when there is no such
 * script. The keys leave on the session's tick, not in this call. */
int scr_play(int letter);
int scr_playing(void);
void scr_stop(void);
/* Every turn of the session loop: whatever of a replay is due. */
void scr_pump(void);

int con_session_paths(const char *name, char *sock, size_t scap,
		      char *view, size_t vcap);
int con_sessions_list(void);
int con_session_kill(const char *name);

/* greet.c */
int con_login(const char *tty);

/* keys.c */
int keys_action(int key, int mods, int *arg);
void keys_print(void);
/* "Super+Shift+Tab" for a key and its modifiers. The table that binds the
 * chords is the one that prints them: a second table goes stale. */
void keys_chord_name(int key, int mods, char *out, size_t n);
/* The other direction, and the same spelling `keys.conf` uses. Zero on a chord
 * naming no key, which is how a typo in that file leaves the default standing
 * rather than unbinding the action. */
int keys_chord_parse(const char *s, int *key, int *mods);

/* main.c */
void con_quit(void);
/* One key into whatever holds the focus, past the chord table. A replay uses
 * it so a recorded chord cannot fire the session's own actions. */
void con_key_to_window(const KtuiEvent *ev);

#endif /* CON_H */
