/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kvt_term — a screen, a state machine and a child, as one thing
 *
 * What a window actually holds. Upstream ships the three pieces separately
 * because kmscon wires them itself; a KDOS surface wants one object with a
 * descriptor to poll and a grid to draw.
 *
 * NO SHELL AND NO system(). The argument vector arrives built — libkbase owns
 * the one correct way to turn a command line into one — and is executed
 * directly, because a terminal opens names that came from a desktop entry.
 * ---------------------------------
 */


#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kvt.h"
#include "kvt_int.h"
#include "kvt_keysyms.h"
#include "kvt_pty.h"

struct kvt_term {
	struct kvt_screen *screen;
	struct kvt_vte *vte;
	struct kvt_shl_pty *pty;

	pid_t child;
	int alive;
	int status;		/* kept after the child is gone */

	int cols, rows;

	/* When synchronized output went on, for the watchdog. Zero when it is
	 * off. See kvt_term_sync_hold(). */
	unsigned long long sync_since;
};

/* The child's output. Straight into the state machine; the screen is what
 * changed by the time this returns. */
static void on_output(struct kvt_shl_pty *pty, void *data, char *u8, size_t len)
{
	struct kvt_term *t = data;

	(void)pty;
	kvt_vte_input(t->vte, u8, len);
}

/* The terminal's own replies — a cursor-position report, a device attribute —
 * go back to the child exactly as a key would. */
static void on_reply(struct kvt_vte *vte, const char *u8, size_t len, void *data)
{
	struct kvt_term *t = data;

	(void)vte;
	if (t->pty)
		kvt_shl_pty_write(t->pty, u8, len);
}

struct kvt_term *
kvt_term_open(const char *const argv[], int cols, int rows)
{
	if (!argv || !argv[0] || cols <= 0 || rows <= 0)
		return NULL;

	struct kvt_term *t = calloc(1, sizeof(*t));

	if (!t)
		return NULL;

	t->cols = cols;
	t->rows = rows;
	t->status = -1;

	if (kvt_screen_new(&t->screen, NULL, NULL) != 0)
		goto fail;
	if (kvt_screen_resize(t->screen, (unsigned)cols, (unsigned)rows) != 0)
		goto fail;
	if (kvt_vte_new(&t->vte, t->screen, on_reply, t, NULL, NULL) != 0)
		goto fail;

	pid_t pid = kvt_shl_pty_open(&t->pty, on_output, t,
				     (unsigned short)cols, (unsigned short)rows);

	if (pid < 0)
		goto fail;

	if (pid == 0) {
		/*
		 * THE CHILD. Nothing above this line is safe to keep — it is
		 * the parent's — and nothing below it returns.
		 *
		 * TERM is xterm-256color because that is what this state
		 * machine implements and what ncurses already ships an entry
		 * for. A private TERM breaks the first time somebody types
		 * ssh.
		 */
		setenv("TERM", "xterm-256color", 1);
		/*
		 * AND COLORTERM, which is the only way a program learns that
		 * the sixteen-colour entry `TERM` names is an understatement.
		 * `libktui`'s own `detect_caps()` reads it, so a KDOS surface
		 * running inside a KDOS terminal detected 256 colours and drew
		 * the theme approximately — the palette is truecolour at both
		 * ends and the variable is what says so.
		 */
		setenv("COLORTERM", "truecolor", 1);
		/*
		 * WHICH TERMINAL, BY NAME, which is the third tier of every
		 * picture program's capability probe — after the kitty query
		 * and after DA1 — and the only one that works over a pipe. A
		 * terminal that answers neither of the first two and does not
		 * name itself is one such a program treats as a teletype.
		 *
		 * ONE NAME FOR BOTH TERMINALS. `kdos-term` and the console
		 * session's own windows are the same engine with the same
		 * capabilities, so a program that changed behaviour between
		 * them would be reacting to a difference that is not there.
		 */
		setenv("TERM_PROGRAM", "kdos-term", 1);
		setenv("TERM_PROGRAM_VERSION", KVT_TERM_VERSION, 1);
		unsetenv("COLUMNS");
		unsetenv("LINES");
		execvp(argv[0], (char *const *)argv);
		/* 127 is what a shell reports for a command it cannot find,
		 * and the window shows it rather than closing on a blank. */
		_exit(127);
	}

	t->child = pid;
	t->alive = 1;
	return t;

fail:
	if (t->vte)
		kvt_vte_unref(t->vte);
	if (t->screen)
		kvt_screen_unref(t->screen);
	free(t);
	return NULL;
}

void
kvt_term_close(struct kvt_term *t)
{
	if (!t)
		return;

	if (t->pty) {
		kvt_shl_pty_close(t->pty);
		kvt_shl_pty_unref(t->pty);
	}
	if (t->vte)
		kvt_vte_unref(t->vte);
	if (t->screen)
		kvt_screen_unref(t->screen);
	free(t);
}

int
kvt_term_fd(struct kvt_term *t)
{
	return t && t->pty ? kvt_shl_pty_get_fd(t->pty) : -1;
}

/*
 * Reap without blocking, and KEEP THE STATUS. A window whose program has
 * finished stays on screen saying how it finished; one that vanished would
 * take the error message with it.
 */
static void reap(struct kvt_term *t)
{
	if (!t->alive || t->child <= 0)
		return;

	int st = 0;
	pid_t r = waitpid(t->child, &st, WNOHANG);

	if (r != t->child)
		return;

	t->alive = 0;
	if (WIFEXITED(st))
		t->status = WEXITSTATUS(st);
	else if (WIFSIGNALED(st))
		/* Shell convention, so a caller need not know which it was. */
		t->status = 128 + WTERMSIG(st);
	else
		t->status = -1;
}

int
kvt_term_pump(struct kvt_term *t)
{
	if (!t)
		return -1;

	int r = 0;

	if (t->pty)
		r = kvt_shl_pty_dispatch(t->pty);

	reap(t);

	/* The descriptor going quiet is not the child being gone: it may have
	 * closed its output and still be running. Only waitpid decides. */
	return r;
}

void
kvt_term_write(struct kvt_term *t, const char *u8, size_t len)
{
	if (t && t->pty && len)
		kvt_shl_pty_write(t->pty, u8, len);
}

/*
 * How many scrolled-off lines the screen keeps. Set by the caller rather than
 * chosen here: the number is a desktop's configuration and a library that read
 * a program's configuration file would answer differently in every consumer.
 */
void kvt_term_scrollback(struct kvt_term *t, unsigned int lines)
{
	if (t && t->screen)
		kvt_screen_set_max_sb(t->screen, lines);
}

void
kvt_term_resize(struct kvt_term *t, int cols, int rows)
{
	if (!t || cols <= 0 || rows <= 0)
		return;
	if (cols == t->cols && rows == t->rows)
		return;

	t->cols = cols;
	t->rows = rows;
	kvt_screen_resize(t->screen, (unsigned)cols, (unsigned)rows);
	if (t->pty)
		kvt_shl_pty_resize(t->pty, (unsigned short)cols,
				   (unsigned short)rows);
}

kvt_age_t
kvt_term_render(struct kvt_term *t, KtuiCell *cells, int w, int h)
{
	return t ? kvt_grid_render(t->screen, cells, w, h) : 0;
}

int
kvt_term_alive(struct kvt_term *t)
{
	return t && t->alive;
}

int
kvt_term_status(struct kvt_term *t)
{
	return t ? t->status : -1;
}

int
kvt_term_signal(struct kvt_term *t, int sig)
{
	return t && t->pty ? kvt_shl_pty_signal(t->pty, sig) : -1;
}

void
kvt_term_scroll(struct kvt_term *t, int lines)
{
	if (!t || !lines)
		return;

	if (lines < 0)
		kvt_screen_sb_up(t->screen, (unsigned)-lines);
	else
		kvt_screen_sb_down(t->screen, (unsigned)lines);
}

struct kvt_screen *
kvt_term_screen(struct kvt_term *t)
{
	return t ? t->screen : NULL;
}

/*
 * OSC, for the two codes that name a window. A terminal is the only thing that
 * can act on them, and it needs the vte it does not otherwise hold.
 */
void
kvt_term_osc_cb(struct kvt_term *t, kvt_vte_osc_cb cb, void *user)
{
	if (t)
		kvt_vte_set_osc_cb(t->vte, cb, user);
}

void kvt_term_clip_cb(struct kvt_term *t, kvt_vte_clip_cb cb, void *user)
{
	if (t)
		kvt_vte_set_clip_cb(t->vte, cb, user);
}

void kvt_term_bell_cb(struct kvt_term *t, kvt_vte_bell_cb cb, void *user)
{
	if (t)
		kvt_vte_set_bell_cb(t->vte, cb, user);
}

void kvt_term_sync_cb(struct kvt_term *t, kvt_vte_sync_cb cb, void *user)
{
	if (t)
		kvt_vte_set_sync_cb(t->vte, cb, user);
}

void kvt_term_notify_cb(struct kvt_term *t, kvt_vte_notify_cb cb, void *user)
{
	if (t)
		kvt_vte_set_notify_cb(t->vte, cb, user);
}

/*
 * The focus moved. Both desktops call this from the one place each decides
 * focus, so a window on a workspace somebody left is told it lost it — which
 * it did, and an editor that is not told does not reload a changed file.
 */
void kvt_term_focus(struct kvt_term *t, int in)
{
	if (t)
		kvt_vte_focus(t->vte, in != 0);
}

int kvt_term_sync_output(struct kvt_term *t)
{
	return t ? kvt_vte_sync_output(t->vte) : 0;
}

static unsigned long long kvt_mono_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000 +
	       (unsigned long long)ts.tv_nsec / 1000000;
}

/*
 * SHOULD THIS FRAME BE HELD BACK?
 *
 * True while the child has synchronized output on and has not held it too
 * long. A frame drawn as an unbracketed row diff tears on a slow link, which
 * is the link this desktop is sold on, so a program that brackets its screen
 * is never seen half-drawn.
 *
 * THE WATCHDOG IS WHY THIS IS A FUNCTION RATHER THAN A FLAG. A child that sets
 * the mode and dies would freeze its window forever, and the terminal cannot
 * tell that from a program taking its time — 150 ms is longer than any frame
 * and shorter than a person notices. It lives here rather than in each
 * renderer because both terminals need the same rule and two copies of a
 * timeout is two timeouts.
 */
int kvt_term_sync_hold(struct kvt_term *t)
{
	if (!t)
		return 0;
	if (!kvt_vte_sync_output(t->vte)) {
		t->sync_since = 0;
		return 0;
	}

	unsigned long long now = kvt_mono_ms();

	if (!t->sync_since)
		t->sync_since = now;
	return now - t->sync_since < 150;
}

/*
 * The three image protocols, switched on for this terminal. Off is the
 * default and stays the default: a consumer that links no decoder must parse
 * exactly what it parsed before, or a sixel dump would stop being ignored and
 * start being a buffer somebody fills.
 */
void
kvt_term_img_cb(struct kvt_term *t, kvt_vte_img_cb cb, size_t max_bytes,
		void *user)
{
	if (t)
		kvt_vte_set_img_cb(t->vte, cb, max_bytes, user);
}

void kvt_term_img_geom(struct kvt_term *t, int max_w_px, int max_h_px)
{
	if (t)
		kvt_vte_set_img_geom(t->vte, max_w_px, max_h_px);
}

/*
 * A REGISTERED PICTURE BECOMES CELLS IN THE SCREEN, at the cursor, and that is
 * the whole of how an image lives in a terminal here.
 *
 * Writing the sprite codepoints into the screen rather than keeping an overlay
 * beside it is what makes a picture scroll with its output, disappear on
 * `clear`, and land in the scrollback — three behaviours an overlay would have
 * to reimplement against a screen that is already doing all three.
 *
 * The caller registered the tiles under `key`; this only names them. A tile
 * the table has since dropped becomes a blank, because a sprite cell pointing at
 * an evicted slot would draw whatever took the slot next.
 */
int
kvt_term_place(struct kvt_term *t, uint64_t key, int cw, int ch)
{
	struct kvt_screen_attr a;

	if (!t || !t->screen || cw < 1 || ch < 1)
		return -1;

	unsigned int width = kvt_screen_get_width(t->screen);

	if (cw > (int)width)
		return -1;

	/*
	 * The picture's own colours are in its pixels; these are what a text
	 * backend paints where it cannot draw one, so they are the ordinary
	 * text on the ordinary background rather than anything of the
	 * picture's.
	 */
	memset(&a, 0, sizeof(a));
	a.fccode = 7;
	a.bccode = 0;

	unsigned int x0 = kvt_screen_get_cursor_x(t->screen);

	/* A picture that will not fit to the right of the cursor starts on the
	 * next line instead of being cut in half at the margin. */
	if (x0 + (unsigned int)cw > width) {
		kvt_screen_newline(t->screen);
		x0 = 0;
	}

	for (int y = 0; y < ch; y++) {
		kvt_screen_move_to(t->screen, x0,
				   kvt_screen_get_cursor_y(t->screen));
		for (int x = 0; x < cw; x++) {
			int sx = 0, sy = 0;
			int slot = ktui_sprite_tile_at(key, cw, x, y, &sx, &sy);
			uint32_t cp = ' ';

			if (slot >= 0)
				cp = KTUI_SPRITE_BASE |
				     ((uint32_t)slot << 8) |
				     ((uint32_t)(sy & 0xf) << 4) |
				     (uint32_t)(sx & 0xf);
			kvt_screen_write(t->screen, kvt_symbol_make(cp), &a);
		}
		kvt_screen_newline(t->screen);
	}
	kvt_screen_move_to(t->screen, x0, kvt_screen_get_cursor_y(t->screen));
	return 0;
}

/*
 * Pasted text, through the state machine rather than straight down the pty:
 * bracketed-paste mode is the child's request to be told where a paste starts
 * and ends, and a terminal that ignored it would let a paste run as if it had
 * been typed.
 */
int kvt_term_paste_needs_confirm(struct kvt_term *t, const char *text)
{
	return t ? kvt_vte_paste_needs_confirm(t->vte, text) : 0;
}

void
kvt_term_paste(struct kvt_term *t, const char *text)
{
	if (t && text)
		kvt_vte_paste(t->vte, text);
}

/* libktui's modifiers, as the state machine names them. */
static unsigned int mods_of(int mods)
{
	unsigned int m = 0;

	if (mods & KT_MOD_SHIFT)
		m |= KVT_SHIFT_MASK;
	if (mods & KT_MOD_ALT)
		m |= KVT_ALT_MASK;
	if (mods & KT_MOD_CTRL)
		m |= KVT_CONTROL_MASK;
	if (mods & KT_MOD_SUPER)
		m |= KVT_LOGO_MASK;
	return m;
}

/*
 * A KEY BECOMES A KEYSYM, and the state machine turns that into bytes.
 *
 * The escape a key produces is not a property of the key: it depends on
 * application cursor mode, on keypad mode, and on which modifiers are down,
 * and all three live in the state machine. A caller writing "\x1b[A" for an
 * arrow is a caller that is wrong the moment a program sends DECCKM — which
 * `less` does on its first screen.
 */
static uint32_t keysym_of(int key, int mods, uint32_t *unicode)
{
	*unicode = KVT_VTE_INVALID;

	switch (key) {
	case KT_K_ESC: return XKB_KEY_Escape;
	case KT_K_ENTER: return XKB_KEY_Return;
	case KT_K_TAB: return XKB_KEY_Tab;
	case KT_K_BACKSPACE: return XKB_KEY_BackSpace;
	case KT_K_UP: return XKB_KEY_Up;
	case KT_K_DOWN: return XKB_KEY_Down;
	case KT_K_LEFT: return XKB_KEY_Left;
	case KT_K_RIGHT: return XKB_KEY_Right;
	case KT_K_HOME: return XKB_KEY_Home;
	case KT_K_END: return XKB_KEY_End;
	case KT_K_PGUP: return XKB_KEY_Prior;
	case KT_K_PGDN: return XKB_KEY_Next;
	case KT_K_INS: return XKB_KEY_Insert;
	case KT_K_DEL: return XKB_KEY_Delete;
	case KT_K_BTAB: return XKB_KEY_ISO_Left_Tab;
	case KT_K_F1: return XKB_KEY_F1;
	case KT_K_F2: return XKB_KEY_F2;
	case KT_K_F3: return XKB_KEY_F3;
	case KT_K_F4: return XKB_KEY_F4;
	case KT_K_F5: return XKB_KEY_F5;
	case KT_K_F6: return XKB_KEY_F6;
	case KT_K_F7: return XKB_KEY_F7;
	case KT_K_F8: return XKB_KEY_F8;
	case KT_K_F9: return XKB_KEY_F9;
	case KT_K_F10: return XKB_KEY_F10;
	case KT_K_F11: return XKB_KEY_F11;
	case KT_K_F12: return XKB_KEY_F12;
	default:
		break;
	}

	if (key >= KT_K_SPECIAL || key <= 0)
		return XKB_KEY_NoSymbol;

	/*
	 * A control code with Ctrl held is the LETTER, arriving already
	 * folded: xkb hands Ctrl+C over as U+0003 and the state machine wants
	 * the keysym so it can apply its own rule — which is not the same rule
	 * for every key, and re-folding a folded code would send Ctrl+Ctrl+C.
	 */
	if ((mods & KT_MOD_CTRL) && key > 0 && key < 0x20)
		return (uint32_t)key | 0x60u;

	*unicode = (uint32_t)key;
	if (key < 0x80)
		return (uint32_t)key;
	return XKB_KEY_NoSymbol;
}

int
kvt_term_key(struct kvt_term *t, int key, int mods)
{
	uint32_t unicode;
	uint32_t sym;

	if (!t)
		return 0;

	sym = keysym_of(key, mods, &unicode);
	if (sym == XKB_KEY_NoSymbol && unicode == KVT_VTE_INVALID)
		return 0;	/* a key with no meaning to a terminal */

	return kvt_vte_handle_keyboard(t->vte, sym,
				       sym < 0x80 ? sym : XKB_KEY_NoSymbol,
				       mods_of(mods), unicode) ? 1 : 0;
}

/*
 * The mouse, reported to the child. Answers whether it was wanted: a program
 * that has not asked for mouse reports leaves the pointer to the terminal, and
 * that is what makes selection possible at all.
 *
 * Pixel coordinates are the cell's, because a cell surface has no others —
 * SGR-pixel reporting is a mode nothing on this desktop turns on, and feeding
 * it cell numbers is more honest than feeding it a guessed pixel size.
 */
int
kvt_term_mouse(struct kvt_term *t, int cell_x, int cell_y, int btn, int mods,
	       int event)
{
	if (!t || cell_x < 0 || cell_y < 0)
		return 0;
	return kvt_vte_handle_mouse(t->vte, (unsigned int)cell_x,
				    (unsigned int)cell_y, (unsigned int)cell_x,
				    (unsigned int)cell_y, (unsigned int)btn,
				    (unsigned int)event,
				    (unsigned char)mods_of(mods)) ? 1 : 0;
}

/* Has the child asked for mouse reports at all? What decides whether the
 * pointer selects text or is handed over. */
int
kvt_term_mouse_mode(struct kvt_term *t)
{
	return t ? (int)kvt_vte_get_mouse_mode(t->vte) : 0;
}

/*
 * The selection, onto the clipboard the rest of the desktop uses. One
 * implementation of "what is on the clipboard", so a copy out of a terminal
 * pastes into a KDOS surface and the other way round.
 */
int
kvt_term_copy_selection(struct kvt_term *t)
{
	if (!t)
		return -1;

	char *text = NULL;
	int len = kvt_screen_selection_copy(t->screen, &text);

	if (len < 0 || !text)
		return -1;

	int r = ktui_clip_copy(text);

	free(text);
	return r;
}
