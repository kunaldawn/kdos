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
 * A WINDOW IS NOT A PROCESS. One kdos-cage speaks for as many toplevels as its
 * guest maps — a toolbox, an image window, two docks and a modal file dialog
 * are five WIN_EMBED windows over one channel — so a window is created without
 * a fork and retired without one, and only the last window of a guest going
 * says anything about the process behind it.
 *
 * WIN_VT is a window with NO CELLS, kept for an application the card cannot be
 * given to through a window — one that sets its own full-screen mode, or whose
 * driver will not run against a headless output. The guest is full screen on a
 * terminal of its own, it is in the window list to be in the taskbar and the
 * Alt-Tab ring, and selecting it is a VT switch rather than a raise.
 */
enum { WIN_TERM = 0, WIN_SURFACE, WIN_VT, WIN_EMBED };

/* embed.c owns every byte of it; con.h needs only the pointer. One per
 * WINDOW, never one per guest: the channel the window speaks over is behind
 * it and is shared with the guest's other windows. */
struct EmbedWin;

/* What a chord does. `arg` is a KWM_EDGE_* for a snap and a workspace index
 * for the two workspace actions; it is unused by the rest. */
enum {
	CON_ACT_NONE = 0,
	CON_ACT_TERM, CON_ACT_CLOSE, CON_ACT_QUIT,
	CON_ACT_MAX, CON_ACT_FULL, CON_ACT_MIN, CON_ACT_EXEC,
	CON_ACT_NEXT, CON_ACT_PREV,
	CON_ACT_SNAP, CON_ACT_WS, CON_ACT_SEND, CON_ACT_RESTORE,
	CON_ACT_RESTORE_ALL,	/* every window put away on this workspace   */
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
	CON_ACT_SCRATCH_MARK,
	/* Put the bar away, and bring it back. An ACTION and not a command:
	 * it is this session's own layout and nothing outside the process can
	 * do it. APPENDED, like every value here. */
	CON_ACT_BAR,
	/*
	 * TO THE BACK. The inverse of the raise every other verb here ends in,
	 * and the only thing that reaches the window under one of the same
	 * size: the ring raises, so it brings the same window forward however
	 * many times it is pressed.
	 */
	CON_ACT_LOWER,
	/* The window menu, over the focused window. Every frame verb in one
	 * list with its chord beside it, which is how the ones with no chip
	 * are found at all. */
	CON_ACT_WINMENU,
	/*
	 * TABS ON A WINDOW FRAME, and stacking is the whole of what they are:
	 * `stack` folds the next window of the ring into the focused one, the
	 * two steps walk the strip, and `unstack` takes the group apart again.
	 * Nothing here moves a window beside another — see win_stack_join().
	 * APPENDED, like every value here.
	 */
	CON_ACT_STACK, CON_ACT_STACK_NEXT, CON_ACT_STACK_PREV,
	CON_ACT_UNSTACK,
	/*
	 * HOW MUCH OF THIS ONE WINDOW'S BACKGROUND SURVIVES. `con.conf` says
	 * what a window starts at; these three are the same question asked of
	 * the window in front of you, because which window wants to be seen
	 * through is a thing a person decides while looking at it — a
	 * reference under a text editor, a terminal over a picture. Reset
	 * gives the window back to the configured answer, so a step is never
	 * a one-way door. APPENDED, like every value here.
	 */
	CON_ACT_OPACITY_UP, CON_ACT_OPACITY_DOWN, CON_ACT_OPACITY_RESET
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
       /*
        * The two notices and the three reminder verbs. A chord runs ONE
        * command and cannot compute a string, so what the time is and what the
        * battery says are `kdos` verbs rather than an argument somebody would
        * have to write into two configuration files in two syntaxes.
        * APPENDED, like every id here.
        */
       CON_CMD_TIME, CON_CMD_BATTERY,
       CON_CMD_REMIND, CON_CMD_REMINDLS, CON_CMD_REMINDCLR,
       /* The notices already on the screen, and the one switch a keyboard
        * reaches for. APPENDED, like every id here. */
       CON_CMD_DISMISS, CON_CMD_DISMISSALL, CON_CMD_DND, CON_CMD_UNDISMISS,
       CON_CMD_AWAKE, CON_CMD_NIGHT,
       /* The palette opened at the setup group, the way CON_CMD_CAPTMENU
        * opens it at the capture one. APPENDED, like every id here. */
       CON_CMD_SETUPMENU,
       /* The address book, the fifth desk accessory. APPENDED, like every id
        * here. */
       CON_CMD_CONTACTS,
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
/* True while something is being carried across the session. The taskbar says
 * so and names the way out, for the reason the mark does. */
int con_dragging(void);

/*
 * THE BAR IS AWAY. One switch for both bars this session can have — its own,
 * and a `kdos-shell` panel docked over it — because a chord that hid one of
 * them and left the other is a chord whose meaning depends on what is
 * installed. It is session state and not a `kdos toggle` file: nothing outside
 * this process can act on it, and a bar hidden on a machine somebody walked
 * away from should come back with the next session.
 */
int con_bar_hidden(void);
void con_bar_toggle(void);
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
/*
 * THE WINDOW AN INTERACTIVE RESIZE IS HOLDING, or 0 — a pointer drag on an
 * edge, or the keyboard rearrange mode. What is being sized changes shape once
 * per motion, and anything whose cost is per RECTANGLE rather than per frame
 * waits for the gesture to end.
 */
int con_sizing_id(void);

void con_notice(const char *text);
const char *con_notice_text(void);
/* Draw the mark over the composed grid. Called last, after every window. */
void con_mark_draw(void);
void con_spawn_at(const char *cmd, int x);

/*
 * THE FRAME'S THICKNESS, PER AXIS, in cells — and the two are different
 * numbers because a cell is.
 *
 * A CELL IS TWICE AS TALL AS IT IS WIDE: 8x16 in the console font, and near
 * enough the same proportion in every monospace face the desktop view loads. A
 * border of the same CELL count on all four sides is therefore a border twice
 * as thick in PIXELS at the top and bottom as it is at the sides — heavier to
 * look at than the sides it dwarfs, and no easier to aim at than they are. TWO
 * COLUMNS AND ONE ROW is the square border: sixteen pixels of edge to take
 * hold of on every side of the window, which is the thickness a hand can find
 * without hunting for it.
 *
 * AND ROWS ARE THE SCARCE AXIS. The shipped grid is 240x67 — 1920x1080 over
 * an 8x16 cell — so a row is three and a half times the share of its own axis
 * that a column is of its: a second row top and bottom takes 3% of every
 * window's height, where a second column each side takes 0.8% of its width.
 * The vertical tolerance is bought with CON_GRAB_CORNER instead, which costs
 * no cells at all.
 *
 * WHICH SIDE OF THE BORDER PAYS FOR IT IS DECIDED BY WHICH RECTANGLE IS FIXED,
 * and both directions are in this desktop.
 *
 * A WINDOW WHOSE CONTENT IS CHOSEN KEEPS EVERY CELL OF IT. `geom` is the
 * content and win_frame() inflates it, so a window landed by the placement
 * search, dragged, or put at a rectangle a layout or a saved session names
 * takes these two numbers MORE of the desk on each side and its program is
 * told the size it asked for.
 *
 * A WINDOW WHOSE OUTER RECTANGLE IS FIXED PAYS OUT OF ITS CONTENT. Maximised,
 * snapped, tiled and the scratchpad's drop-down all come from win_tile_rect():
 * the work area, or a division of it, is the FRAME, and what the program gets
 * is 2 * CON_FRAME_X columns and 2 * CON_FRAME_Y rows less than that. A window
 * too big for the grid is the same case — win_fit_area() deflates the grid by
 * the border, because there the frame is the thing that has to stay on it.
 * Raising either number takes cells from every one of those at once.
 *
 * THE TITLE SITS IN THE TOP ROW, which is why the content rect and the frame
 * rect differ by exactly these two numbers.
 */
#define CON_FRAME_X 2
#define CON_FRAME_Y 1

/*
 * HOW MANY COLUMNS ONE TITLE-ROW CHIP OWNS: plate, mark, plate.
 *
 * THE MARK NEEDS PLATE ON ALL FOUR SIDES. The palette's dark slots are one
 * colour to the eye — KT_BG measures 1.00:1 to 1.20:1 against KT_SURFACE
 * across the seven schemes — so a chip's ink cannot be told from the window
 * body below the title row by its colour. What tells them apart is the plate
 * AROUND the mark, and an odd width is the only one that can centre it.
 *
 * AND A ONE-CELL TARGET IS THE FAILURE A PERSON FEELS: a mouse has to be aimed
 * at it and a finger cannot land on it at all, and the miss goes to the title
 * row underneath, which arms a move.
 *
 * READ BY btn_run() AND draw_buttons() AND NOTHING ELSE. The stride living in
 * two places is the bug the one-place rule above btn_run() exists to prevent.
 */
#define CON_CHIP_W 3

/*
 * THE NARROWEST A TAB MAY BE DRAWN, in cells: a space, four columns of title,
 * a space.
 *
 * A TAB IS READ BY THE PLATE AROUND ITS TEXT, exactly as a chip is — the
 * palette's dark slots are one colour to the eye, so the fill either side is
 * the whole of what separates one tab from its neighbour. Four columns of
 * title is the shortest run that tells two programs apart at a glance; below
 * it the strip stops being names and becomes a row of initials.
 *
 * A STRIP THAT CANNOT GIVE EVERY TAB THIS MUCH COLLAPSES TO A COUNTER instead
 * of shrinking further. See draw_tabs().
 */
#define CON_TAB_MIN 6


/*
 * HOW FAR ALONG A SIDE IS STILL THE CORNER, in cells.
 *
 * A corner resizes both axes, and the block where the two bands actually cross
 * is CON_FRAME_X by CON_FRAME_Y — two cells, which is a target a mouse has to
 * be aimed at and a finger cannot hit at all. The ends of every side take both
 * axes instead, for this many cells.
 *
 * FOUR AND NOT TWO, BECAUSE THE ARMS ARE THE VERTICAL TOLERANCE. The top and
 * bottom bands are one row each and cannot be more without costing every
 * window a row, so the four rows of arm running up each side column are where
 * a hand that wants to drag a bottom corner finds one. Along the top and
 * bottom rows the same four cells are pure surplus and cost nothing.
 *
 * Clamped to a third of the side in corner_arm(): a frame twelve cells wide
 * with four cells of corner at each end has four cells of side left to drag,
 * and a shorter one would have none at all — every press on it would take both
 * axes and the window could not be resized in one.
 */
#define CON_GRAB_CORNER 4

/*
 * HOW LONG A WINDOW IS SHOWN FLASHING, in milliseconds. 120 is long enough to
 * be seen and short enough that a program ringing in a loop is a flicker
 * rather than a window that stays lit. The bell and the answer to a click on a
 * window a modal has blocked are the same flash, because they are the same
 * sentence: this window wants you.
 */
#define CON_FLASH_MS 120

/*
 * THE STARTUP CARD A BOXED APPLICATION OPENS AS, in cells of content.
 *
 * A launch is a window on the desktop from the moment the cage is forked,
 * seconds before the application behind it can draw anything — and the
 * rectangle that window is going to BE is the rectangle the cage was forked
 * with, which is half the work area or whatever the person last left this
 * program at. Standing that rectangle up empty is a large blank frame that is
 * then repainted by an application laying itself out inside it; standing a
 * card up instead says the same thing in a shape that is obviously not the
 * application yet.
 *
 * IT IS THE WINDOW'S RECTANGLE AND NEVER THE GUEST'S OUTPUT. The output stays
 * cut at the full rectangle for the whole of the launch, so nothing is
 * reallocated and nothing is reconfigured when the card opens out — see
 * `Win.starting`. Shrink the output to the card and the application comes up
 * the size of the card, which is the failure this exists to avoid.
 *
 * Three rows: the name, the bar, the stage. Clamped to the rectangle it opens
 * out to, so a launch remembered at a tiny rectangle gets a card that fits
 * inside it rather than one larger than the window it precedes.
 */
#define CON_CARD_W 30
#define CON_CARD_H 3

typedef struct Win {
	struct Win *next;
	int id;
	int kind;

	KwmRect geom;		/* the CONTENT, in cells */
	KwmRect restore;	/* what an untile returns to */
	unsigned tiled;
	int minimised;

	/*
	 * HOW MUCH OF THIS WINDOW'S OWN BACKGROUND SURVIVES, per cent, or 0
	 * for "whatever con.conf says". Zero is a real answer here and 100 is
	 * a different one: a window set to 100 by hand stays opaque through a
	 * configuration that made every other window see-through.
	 */
	int opacity;

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
	 * IT ASKED TO OPEN WHERE THE EYE IS, at the size it attached with. A
	 * terminal application with a fixed shape — a monitor, a mixer — is
	 * asking to be looked at and dismissed rather than to be one pane
	 * among others. It is a window like any other once it is open: the
	 * tiling chord takes it with the rest, and only where it OPENS and
	 * whether that is remembered differ.
	 */
	int floating;

	/*
	 * THIS WINDOW IS A STARTUP CARD AND NOT THE APPLICATION YET, which is
	 * a boxed guest between the fork and its first frame.
	 *
	 * WHILE IT IS SET, `geom` AND THE GUEST'S OUTPUT ARE TWO DIFFERENT
	 * RECTANGLES. `geom` is the card — CON_CARD_W by CON_CARD_H, centred
	 * on the rectangle the window will open out to — and the output is
	 * that larger rectangle, already allocated and already named in the
	 * cage's `--embed`. embed_resized() reads the larger one for as long
	 * as this stands, because reflowing the guest to the card is the
	 * application coming up the size of the card.
	 *
	 * AND NOTHING REMEMBERS A CARD'S RECTANGLE. geo_worth() refuses it for
	 * the reason it refuses a splash: a launch that dies before it draws
	 * would otherwise write the card as the size this program opens at
	 * next time, to disk, for every session after this one.
	 */
	int starting;

	/*
	 * THE WINDOW THIS ONE BELONGS TO, a window id, 0 for a window of its
	 * own — and whether the owner may be used while it is open.
	 *
	 * A DIALOG IS NOT A SECOND APPLICATION. It opens over the window that
	 * raised it, rides that window's raises, carries no taskbar row of its
	 * own and is remembered nowhere, because what a person opened is the
	 * owner and the dialog is a question about it. `modal` adds that the
	 * owner cannot be raised, focused or closed while the question stands:
	 * a raise aimed at the owner lands on the modal and flashes it, which
	 * is the only honest answer a desktop can give to a click on a window
	 * its own application has stopped answering.
	 *
	 * PLAIN FIELDS, read by the window model and written by whoever built
	 * the window. Every kind of window can have an owner — the rules are
	 * about the relation and not about what is inside either window.
	 */
	int owner;
	int modal;

	/*
	 * THE STACK THIS WINDOW IS A TAB OF — the window id of the tab that is
	 * ON SCREEN, and 0 for a window in no stack. THE SESSION'S SECOND
	 * INTER-WINDOW RELATION, and the only other one there is.
	 *
	 * THE HEAD IS THE VISIBLE MEMBER AND IT NAMES THE STACK. Every member
	 * carries the same value, the head's included — `w->stack == w->id` is
	 * what makes a window the one on screen — so the id changes each time
	 * a different tab is brought up, and a set with one member left is not
	 * a stack at all. The head holds the geometry, the tile state and the
	 * workspace; every other member is HIDDEN, which is what buys one
	 * taskbar row, one ring entry, one hit rectangle, one window-list row
	 * and a sleeping guest with no code of its own.
	 *
	 * THE STRIP IS ORDERED BY ID AND NOT BY THE LIST. S.wins is the
	 * z-order and a tab switch moves the incoming member to the front of
	 * it, so a strip drawn in list order would reshuffle every time
	 * somebody changed tab. Ids only ever go up, so id order is the one
	 * order a person can point at twice.
	 *
	 * IT IS SESSION STATE AND NOT A libkwm CONCEPT: libkwm computes
	 * rectangles and knows nothing about a neighbour, so nothing about a
	 * stack is shared with kdos-comp and nothing here is written to the
	 * session record.
	 */
	int stack;

	/*
	 * A ROW IN THE TASKBAR IS FOR SOMETHING A PERSON OPENED. A tool
	 * palette and a splash are not, and neither is a dialog: one
	 * application is one row, and a row per dialog is a bar that grows a
	 * button every time a file chooser opens.
	 */
	int no_task;

	/* Its child has gone and the modes it set have been put back. A
	 * terminal window OUTLIVES its program here — it stays showing how the
	 * program finished — so the reset happens once, when the death is
	 * first seen, and not on every pump after it. */
	int term_reset;

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

	/*
	 * THE LAST WHOLE FRAME THIS TERMINAL WAS COMPOSED FROM, and the grid
	 * it was taken at. For WIN_TERM only, and NULL until the program
	 * inside opens a synchronized-output bracket — see term_cells().
	 *
	 * IT IS WHAT A HELD WINDOW IS DRAWN WITH. The session clears the grid
	 * and repaints every window on one tick, so a held window that drew
	 * nothing would be a hole showing the backdrop, and a held window that
	 * delayed the tick would stop the whole desktop for one program's
	 * frame. A terminal that never opens a bracket pays one pointer test
	 * per compose for the whole mechanism and not a byte of memory.
	 */
	KtuiCell *sync_cells;
	int sync_w, sync_h;
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
	/*
	 * WHAT THIS SURFACE WAS LAST TOLD ABOUT ITS FRAME, or -1 before it has
	 * been told anything. The session draws chrome round an ordinary
	 * window and round nothing else, and a client that is not told draws a
	 * second box inside the first. See publish_decor().
	 */
	int decor_told;

	char title[128];
	char app_id[64];

	/*
	 * THE PROGRAM THIS WINDOW WAS OPENED FOR, written once and never
	 * again. Neither field above can answer that question: `title` is the
	 * guest's to rewrite the moment it emits an OSC, and `app_id` groups
	 * windows of a kind — every WIN_TERM is "terminal", and a guest on a
	 * terminal of its own is "kdos-cage" — so a run-or-raise matching on
	 * either would find the wrong window or none. An EMBEDDED guest's app
	 * id is the application's own name, which is what the taskbar groups
	 * its chips by and what the box collector reads.
	 *
	 * AN EMBEDDED GUEST'S `prog` IS THE PROGRAM IT EXECS, NOT ITS BOX —
	 * `gimp` and not `app.gimp`. It is the desktop entry's own stem, so a
	 * configured taskbar row, a run-or-raise chord and the geometry table
	 * all match the name a person would write; the box name is the
	 * launcher's and names no window. Empty when nothing named a program,
	 * and an empty `prog` matches nothing.
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
	struct EmbedWin *em;	/* WIN_EMBED */

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

	/*
	 * THE FACES AN ATTACHED DISPLAY OFFERED, and which is in force.
	 *
	 * THE LIST IS THE DISPLAY'S AND NOT THE SESSION'S. A view is the only
	 * end with a font stack and it may be at the far end of an ssh link,
	 * so what a session gathered would be this machine's fonts offered to
	 * that screen. These are held to hand to a picker and to bound the
	 * index it sends back; nothing here parses one.
	 *
	 * Empty where no attached view rasterises its own glyphs, which is a
	 * `--tty` view inside somebody else's terminal — the honest answer to
	 * a picker, and the same one `font_step()` puts on the bar.
	 */
	char fonts[KCON_MAX_FONTS][KCON_FONT_NAME];
	int nfonts, font_cur;

	/*
	 * THE SCREENS AN ATTACHED DISPLAY IS DRIVING, and what each can show.
	 *
	 * Relayed, never gathered: the display is the end with the modes and
	 * it may be somewhere else. Held so a picker can be answered at once
	 * and so an index it sends back can be bounded before it goes out.
	 */
	KconOut outs[KCON_MAX_OUTS];
	int nouts;
	/*
	 * THE FASTEST MODE ANY ATTACHED DISPLAY IS WEARING, in millihertz,
	 * and 0 where none has said.
	 *
	 * SEPARATE FROM `outs` BECAUSE THEY ANSWER DIFFERENT QUESTIONS. A
	 * picker lists ONE display's monitors and the seam snap measures ONE
	 * display's columns, so `outs` is whichever view answered last;
	 * frame_floor_ms() paces a session that has several, so it needs the
	 * MAXIMUM over all of them. Held in one field, the session's compose
	 * rate would be decided by reply order.
	 *
	 * Dropped to 0 whenever the screens are asked for and raised by each
	 * answer, so it is a union over one round of asking — see
	 * outs_ask_views().
	 */
	int pace_mhz;
	/* The shell waiting for a list a view has not answered with yet. */
	KconSurface *outs_for;
	/* The shell waiting for a list a view has not answered with yet, or
	 * NULL. One slot: a person opens one picker. */
	KconSurface *fonts_for;
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
/*
 * TO THE FRONT, WITH WHATEVER IT OWNS IN FRONT OF IT — and a raise aimed at
 * the owner of a modal lands on the modal instead. Both rules live here and
 * nowhere else: the ring, the directional search, a number chord and a click
 * all end in this call, and a rule repeated at four of them is four chances
 * for a dialog to be left behind the window it is asking about.
 */
void win_raise(int id);
/*
 * AND THE INVERSE: TO THE BACK, WITH WHATEVER IT OWNS STILL OVER IT.
 *
 * The list is the stack, so this is a move to the tail — the family lowered
 * together and the owner last of all, or a dialog would end up under the
 * window it is asking about. Two windows of the same size fully overlapped
 * have nothing else to swap them with: the ring raises, so it can only ever
 * bring the same one forward.
 *
 * THE FOCUS GOES WITH THE FRONT when the lowered window had it. A keyboard
 * left on a window now behind another is one whose keys land where nobody is
 * looking.
 */
void win_lower(Win *w);
/* The rectangle moved and kept its size: fitted like any other placement, and
 * nobody is told a size they already have. */
void win_moved(Win *w);
/*
 * STEP THIS WINDOW'S OWN TRANSPARENCY. `step` is in per cent and 0 gives the
 * window back to `con.conf`. Clamped at both ends: a window at zero is one
 * nobody can find again, and there is no chord to bring back a window that
 * cannot be seen.
 */
void win_opacity_step(Win *w, int step);
/* And straight to a rung of the ladder, which is what the menu's pane picks.
 * Clamped at both ends like a step. */
void win_opacity_set(Win *w, int pct);
/* What it is drawing at, per cent — its own or the configured default. */
int win_opacity_pct(const Win *w);
/* The window a modal question is being asked in, for this window id, or NULL.
 * A window with one may not be raised, focused or closed. */
Win *win_modal_for(int id);
void win_close(Win *w);				/* ask */
void win_drop(Win *w);				/* and take it out */
/*
 * WHERE A NEW WINDOW GOES. A window that names an owner opens CENTRED ON IT at
 * the size it asked for, because a dialog belongs to the window that raised it
 * and the minimal-overlap search would put it wherever there happened to be
 * room. Everything else goes through the search. See windows.c.
 */
void win_place(Win *w, int want_w, int want_h);
void win_place_corner(Win *w, int want_w, int want_h, int corner, int mx,
		      int my);
KwmRect win_frame(const Win *w);		/* the content rect, inflated */
KwmRect win_workarea(void);
Win *win_at(int x, int y);
void win_snap(Win *w, unsigned edge, int combine);
void win_resized(Win *w);
/* A window exactly where it was, with its FRAME clamped onto the grid it comes
 * back on — the rectangle named here is the content, and a border off the grid
 * is a window with no edge to grab. What a restored session and a named layout
 * use; `win_place` is for a window that has no place yet. */
void win_place_at(Win *w, int x, int y, int cw, int ch);
void win_maximise(Win *w);
void win_fullscreen(Win *w);
void win_minimise(Win *w);
void win_restore(Win *w);
Win *win_last_minimised(void);
void win_restore_all(void);
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

/*
 * ── THE STACK ──────────────────────────────────────────────────────────
 *
 * Tabs on a window frame, and stacking only: two windows in a stack are ONE
 * rectangle showing one of them at a time. Nothing here tiles a group, drags
 * one window onto another or reorders a strip — see `Win.stack`.
 */
/* How many tabs this window's stack has, and where this one sits in the strip
 * (1-based). Both answer 0 for a window in no stack, which is what the frame
 * tests before it draws a strip at all. */
int win_stack_n(const Win *w);
int win_stack_index(const Win *w);
/*
 * `a` JOINS `b`'S STACK, and `b` is the tab left on screen. `a` — with every
 * tab it was already carrying — takes the head's rectangle through
 * win_place_at() and is hidden: a member configured to its old size draws the
 * old size into the new rectangle the moment it is brought up.
 */
void win_stack_join(Win *a, Win *b);
/* Bring this member up and put the one on screen away. The rectangle, the tile
 * state and the workspace go with the head, so a stack is one window wherever
 * it is put. */
void win_stack_show(Win *m);
/* One tab along the strip, forward or back, wrapping. */
void win_stack_step(Win *w, int dir);
/* Fold the ring's next window into this one as a tab. */
void win_stack_with_next(Win *w);
/* Take the group apart: every member un-hidden and placed by the ordinary
 * search, the one on screen left where it is. */
void win_stack_unstack(Win *w);
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
/* Fit every window to the work area as it now is — what a change to the area
 * itself owes, as against a change to the grid. */
void win_refit(void);
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
/*
 * IS ANYBODY LOOKING AT IT — the one answer, so the draw loop and the embedded
 * guests cannot disagree about which windows are worth rendering.
 */
int win_is_on_screen(const Win *w);
/*
 * IS THIS CELL OF THIS WINDOW UNDER SOMETHING DRAWN AFTER IT.
 *
 * ANYTHING PAINTED ONTO A WINDOW AFTER THE WALK HAS TO ASK. win_draw_all()
 * lays the desktop down back to front and whatever it drew last owns the
 * cell, so a later pass that writes a window's own rectangle unasked puts its
 * marks over whatever is in FRONT of that window there — and a window is
 * topmost only where the pointer is, never along the whole of its border.
 *
 * THE DRAW ORDER AND NOT THE HIT ORDER. win_at() answers which window a press
 * belongs to and therefore skips a surface that takes no input — a tooltip, a
 * toast, the candidate list — and every one of those is drawn and does cover
 * cells.
 */
int win_covered_at(const Win *w, int x, int y);
void win_draw_all(void);
/* The window list: Turbo Vision's Alt+0, drawn by the session until Task
 * 6.4 lets kdos-teams read the list over libkdisp. */
void win_list_toggle(void);
int win_list_active(void);
void win_list_draw(void);
int win_list_key(int key);
/*
 * THE WINDOW MENU — every frame verb, named, with the chord beside it.
 *
 * WHAT A POINTER CAN REACH IS OTHERWISE WHAT THE FRAME DRAWS: three chips and
 * a row to drag. Fullscreen, lower, the scratchpad mark and send-to-workspace
 * are chords and nothing else, and the shipped `taskbar = windows` does not
 * draw the function-key row that names any of them — so a person who has not
 * read the book cannot find them at all.
 *
 * EVERY ROW PRINTS ITS OWN CHORD, read out of the bind table rather than
 * written here, so the menu teaches the keyboard and cannot teach a chord
 * `keys.conf` has moved.
 *
 * `win_menu_ptr` and `win_menu_key` answer 1 when the event was the menu's; it
 * owns both while it is up, for the window list's reason.
 */
void win_menu_open(Win *w, int x, int y);
int win_menu_active(void);
void win_menu_draw(void);
int win_menu_key(const KtuiEvent *ev);
int win_menu_ptr(const KtuiEvent *ev);
/* Is this cell the window's TITLE ROW — the top band of its frame, chips
 * included? What the right and middle buttons are answered from. */
int win_on_title(const Win *w, int x, int y);
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

/*
 * FORK A CAGE AND PUT ITS FIRST WINDOW ON THE DESKTOP.
 *
 * The window it answers with is a PLACEHOLDER: it says "starting…" and it is
 * what the guest's first ordinary toplevel claims when it maps, so a one-window
 * application is on screen from the moment it is launched and is never a second
 * window that replaces the first. Every further toplevel the guest maps becomes
 * a window of its own with no fork behind it, and a guest that hands its
 * document off to an instance that is already running maps nothing here at all
 * — its placeholder goes quietly and the window opens in the other cage.
 */
Win *embed_open(const char *const argv[], const char *title);
void embed_pump(void);
void embed_reap(void);
void embed_resized(Win *w);
void embed_view_attached(void);
/* A display could not keep the picture in `slot`: the block is owed to that
 * display again, bounded — see the definition. */
void embed_sprite_lost(KconSurface *v, int slot);
/*
 * ASK THIS WINDOW TO GO, and this window alone. The guest decides what that
 * means, and A GUEST THAT ANSWERS IS TAKEN BY NO TIMER AFTERWARDS: a save
 * prompt opened on the asked window is a window being used, and calling this
 * again before the force is offered re-sends the question and takes nothing,
 * because the dialog is already drawn and has nothing new to commit. Only an
 * ask covering the last window a person can reach, and only while that window
 * has answered nothing, starts the clock that escalates to a signal, because a
 * process signalled over one dialog is unsaved work in every other window it
 * had open.
 *
 * AND CALLING IT AGAIN ON A WINDOW THAT WAS OFFERED THE FORCE TAKES IT. Ten
 * seconds after the latest ask the bar tells the person that closing again
 * quits the application, which is the only route to a guest that answers and
 * will not go. From there the window goes at once and the process is signalled
 * and killed on a schedule no frame can clear — see the rungs above
 * embed_close() in embed.c.
 */
void embed_close(Win *w);
void embed_close_all(void);
void embed_free(Win *w);
int embed_alive(const Win *w);
int embed_fds(int *fds, int max);
void embed_draw(const Win *w);
int embed_key(Win *w, const KtuiEvent *ev);
int embed_ptr(Win *w, const KtuiEvent *ev);
/*
 * THE SAME PHYSICAL INPUT, AS THE DEVICE REPORTED IT. The two calls above are
 * a character and a cell, which is what a view inside somebody else's terminal
 * can send and all a cell desktop wants; an embedded guest is the one thing
 * here that holds a key down, aims below a cell and scrolls sideways. The
 * COOKED event for the same input has already been routed when any of these
 * runs — see the input section of embed.c.
 */
int embed_key_raw(Win *w, const KconKeyRaw *k);
int embed_ptr_raw(Win *w, const KconPtrRaw *p);
int embed_axis_raw(Win *w, const KconAxisRaw *a);
/* The modifier state the keyboard is in, kept for the resync a window is sent
 * when it takes the keyboard and whenever a lock or a group changes under
 * it. */
void embed_mods_note(const KconKeyRaw *k);
/*
 * THE RAW STREAM STARTED OR STOPPED. A release and the modifier mask travel on
 * it and on nothing else, so every guest is released here and the mask the
 * session holds stops counting as known: called from the gate that asks for
 * the stream, which is the one place either edge exists.
 */
void embed_raw_reset(void);
/* The layout every guest is given; `text` is xkb's text format and is copied
 * here. The last view to say what its keyboard is wins. */
void embed_keymap(int format, const char *text, size_t len);
/* The pointer is no longer over this window. */
void embed_leave(Win *w);

/*
 * ── A DRAG ACROSS AN EMBEDDED WINDOW ────────────────────────────────────
 *
 * The session owns the drag — it hit-tests, it draws the pointer and it holds
 * the payload — and these four hand a guest the part of it a toolkit needs.
 * `lx`/`ly` are window-relative CELLS, the same numbers drag_local() clamps,
 * and are converted to the guest's pixels here.
 *
 * ONLY THE TYPE CROSSES ON THE ENTER AND ONLY THE BYTES ON THE DROP, which is
 * the rule the session already keeps for its own surfaces: a drag passing over
 * six windows must not hand its payload to all six. Each answers non-zero when
 * the message reached the cage.
 */
int embed_drag_enter(Win *w, int lx, int ly, const char *mime);
int embed_drag_motion(Win *w, int lx, int ly);
int embed_drag_leave(Win *w);
int embed_drag_drop(Win *w, int lx, int ly, const char *data, size_t len);

/*
 * AND THE OTHER DIRECTION: a guest began a drag, so the session takes it over.
 * Implemented in main.c, which owns the drag state; called from embed.c, which
 * owns the channel. `src` is the window it came from, for the bar.
 *
 * A TYPE THE SESSION CANNOT CARRY IS REFUSED HERE, in the one place that
 * decides what a drag on this desktop may be.
 */
void con_drag_from_guest(const char *mime, const char *data, size_t len,
			 unsigned src);
/* The window whose guest has taken the pointer, or NULL for none. */
Win *embed_grab_win(void);
/* Is a guest anybody can see asking for the screen to stay on. */
int embed_inhibited(void);

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
/*
 * THE CELLS THIS TERMINAL IS COMPOSED FROM THIS TICK — the live grid, or the
 * last whole frame while the program inside is holding one open with DECSET
 * 2026. `buf` is the caller's scratch, at least `cols * rows` cells, and the
 * answer is either it or a buffer the window owns; neither outlives the
 * compose that asked. NULL when there is nothing to draw.
 */
const KtuiCell *term_cells(Win *w, KtuiCell *buf, int cols, int rows);
/* The frame kept for a hold, released with the window. The terminal itself is
 * win_close()'s, which runs first and can run without this. */
void term_free(Win *w);

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
 * WHICH CHIP IS HELD DOWN, so that a press can be taken back.
 *
 * A PRESS IS NOT A CLICK. A chip that fired on the press gave a person no way
 * to change their mind, and the one that destroys the window is the one they
 * most need to: arming on the press and acting on a release over the same chip
 * makes a slip recoverable by moving the hand off before letting go.
 *
 * `win_button_armed` answers WIN_BTN_NONE when nothing is held.
 */
void win_button_arm(int id, int kind);
int win_button_armed(int *id);
void win_button_disarm(void);

/*
 * WHERE THE POINTER IS, in cells, or off the grid for nowhere.
 *
 * CHROME UNDER THE POINTER SAYS SO. A frame button that looked the same
 * whether or not it was about to be pressed is one a person tests by pressing
 * it, and on `X` that is a window they did not mean to close. The router hands
 * the position over on every pointer event including the leave, which libkwl
 * reports as an off-grid position — a highlight nothing retracted would stay
 * lit for the rest of the session.
 */
void win_ptr_at(int x, int y);

/*
 * WHAT A PRESS ON A WINDOW ARMS, and which edges a resize is to move.
 *
 * THE ONE PLACE THE ANSWER IS WORKED OUT. The router owns the grab and this
 * owns the geometry: a second reading of where the border is would be a second
 * thing to get wrong, on the frames nobody tests.
 */
enum {
	WIN_GRAB_NONE = 0,
	WIN_GRAB_MOVE,
	WIN_GRAB_RESIZE
};

int win_grab_at(const Win *w, int x, int y, int btn, int mods,
		unsigned *edges);

/* See the definition: the corner arms `win_grab_at` measures a press against,
 * for the one caller that has to light exactly what a press would arm. */
void win_grab_arms(const Win *w, int *ax, int *ay);

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
/*
 * HOW MANY TIMES THE SELECTION HAS CHANGED, the clipboard and the primary
 * counted together. A reader that mirrors the selection somewhere else keeps
 * the count it last mirrored and compares that, rather than comparing up to
 * KEMBED_CLIP_MAX bytes to find out whether it has anything to send —
 * embed.c's channel mirror is the one that does.
 */
unsigned long clip_gen(void);
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
/* ONE FRAME OF THE GREETER, OFFSCREEN, from a fixture file naming the accounts
 * and the sessions — the machine's own /etc/passwd would make the golden
 * change whenever somebody adds an account. Composites and returns: this path
 * never asks for a password and never starts a session. */
int con_greet_dump(int cols, int rows, const char *fixture, const char *msg);

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
/* The chord bound to an action NAME, after the keys.conf overlay — empty for
 * an action no row names. What the window menu prints beside each verb: a
 * string written at the menu instead would teach a chord a rebinding has
 * moved. */
void keys_chord_for(const char *action, char *out, size_t n);
/*
 * THE CHORD A VERB IS BOUND TO, by the verb's own name. 0 when the bind table
 * has no such name — which is what a surface asking for a verb this build does
 * not have must get.
 *
 * A NAME REACHES AN ACTION THROUGH ITS CHORD and not through a second table:
 * the chord is fed to the one key handler, so a verb asked for by a menu and
 * one pressed on a keyboard cannot take different paths.
 */
int keys_chord_of(const char *action, int *key, int *mods);

/* main.c */
void con_quit(void);
/* Begin the keyboard's move-and-size mode on this window. The chord and the
 * window menu's row both end here, so what can be rearranged is decided once;
 * see con_rearranging(). */
void con_rearrange(Win *w);
/* One key into whatever holds the focus, past the chord table. A replay uses
 * it so a recorded chord cannot fire the session's own actions. */
void con_key_to_window(const KtuiEvent *ev);

#endif /* CON_H */
