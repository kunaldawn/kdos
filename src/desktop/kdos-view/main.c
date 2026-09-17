/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view — a display, and nothing else
 *
 * It holds NO window state: no window list, no focus, no workspaces, no
 * clipboard. Cells arrive, input leaves. That is what makes a view that
 * crashes lose nothing, a view that is remote trustworthy with nothing, and
 * detach and reattach fall out rather than be built.
 *
 * FOUR MODES, EXACTLY ONE PER RUN. Each dispatches on its own flag rather than
 * on the absence of the others: a chain that ends in a fall-through catches
 * whichever mode is added next and sends it somewhere it was never meant to go.
 *
 *   --kms     a screen of its own: modeset, seat and input, through libkkms
 *   --tty     inside foot, tmux, or over ssh
 *   --dump    the rig: exact cells, no PPM and no VNC
 *   --cast    a PipeWire stream — a view nobody looks at
 * ---------------------------------
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <signal.h>

#include "kbase.h"
#include "kcolor.h"
#include "kcon.h"
#include "ktui.h"

/*
 * THE KMS MODE IS COMPILED IN ONLY WHERE ITS LIBRARIES ARE. libkkms brings
 * drm, input, seat and a font renderer with it; the shipped recipe defines
 * this and the self-test does not, so the tty and dump modes stay buildable
 * anywhere — which is what keeps the console desktop's goldens checkable on a
 * bare host.
 *
 * A build without it says so when asked for --kms rather than pretending.
 */
/*
 * THE CELL RASTERISER IS SHARED BY EVERY SINK THAT HOLDS PIXELS. `--kms` puts
 * cells on a screen, `--shot` puts the same cells in a file and a `--tty` view
 * hands them to the terminal it is running in — all three keep a picture's
 * pixels, and all three go through libkcell's sprite table to do it. Only the
 * KMS driver itself is behind the narrower guard.
 *
 * A BUILD WITHOUT IT DRAWS PICTURES AS CHARACTERS, which is what a `--dump`
 * does anyway. What it must not do is claim `KCON_VIEW_PIXELS` and then have
 * nothing to put the pixels in.
 */
#if defined(KDOS_VIEW_KMS) || defined(KDOS_VIEW_SHOT) || \
    defined(KDOS_VIEW_TTYPIX)
#define KDOS_VIEW_PIXELS 1
#endif

#ifdef KDOS_VIEW_PIXELS
#include <pixman.h>

#include "kcell.h"
#endif
#include "record.h"
#include "view.h"
#ifdef KDOS_VIEW_KMS
#include "kkms.h"
#endif

/*
 * THE CAST MODE IS COMPILED IN ONLY WHERE PIPEWIRE IS, for the same reason the
 * KMS mode is guarded: the self-test builds this without either, so the tty and
 * dump modes stay checkable on a bare host.
 */
#include "cast.h"

/*
 * THE CAP ON A RECORDING'S FRAME RATE. Thirty is more than a character grid
 * ever changes at and far less than a compositor's; the rate the frames
 * actually go out at is the desktop's own, because a frame is pushed only when
 * cells changed.
 */
#define KDOS_VIEW_CAST_FPS 30


static KconConn *conn;

/*
 * THE GRID AS IT WAS SENT, before any picture was substituted into it.
 *
 * A sprite may arrive AFTER the cells that name it — a view that attached late
 * gets the whole grid and the pictures only when their pixels next change — and
 * a cell is only ever drawn when the session sends it again. Without a copy of
 * what was sent there is nothing to redraw those cells from, so the picture
 * would appear one frame late at best and never at worst.
 */
static KtuiCell *shadow;
static int shadow_w, shadow_h;

/*
 * WHERE EACH SESSION SLOT'S CELLS ARE, as a box into the shadow above.
 *
 * A PICTURE REPAINTS ONLY THE CELLS THAT NAME IT, and an embedded application
 * publishes one 16x16-cell block at a time — so searching the grid for those
 * cells costs the whole desktop per block, and a maximised guest lands dozens
 * of blocks per frame on the display's own thread. The box grows wherever a
 * sprite cell lands in the shadow and shrinks to what each repaint finds, so
 * a block whose window moves is covered by the union of both places until the
 * repaint that tightens it onto the new one.
 *
 * x1 < 0 IS A SLOT WITH NO CELLS ON THIS GRID, which is also what a picture
 * arriving before the cells that name it looks like: there is nothing drawn
 * to repaint, and the commit that names the slot draws it itself. The boxes
 * index the shadow, so they are emptied with it.
 */
static short slot_x0[KCON_MAX_SPRITE_MAP], slot_y0[KCON_MAX_SPRITE_MAP];
static short slot_x1[KCON_MAX_SPRITE_MAP], slot_y1[KCON_MAX_SPRITE_MAP];

#ifdef KDOS_VIEW_PIXELS
/* Defined below, once the substitutions it draws through exist. A build with no
 * pixel library is never sent a picture, so it has nothing to repaint. */
static void redraw_slot(unsigned slot);
#endif

static void shadow_fit(int w, int h)
{
	if (w == shadow_w && h == shadow_h && shadow)
		return;
	free(shadow);
	shadow = calloc((size_t)w * (size_t)h, sizeof(*shadow));
	shadow_w = shadow ? w : 0;
	shadow_h = shadow ? h : 0;
	for (int i = 0; i < KCON_MAX_SPRITE_MAP; i++)
		slot_x1[i] = -1;
}

/*
 * A CELL GOING INTO THE SHADOW, TAKEN NOTE OF. The box only grows here: a
 * slot leaving a cell is not seen, so this is an upper bound on where the
 * slot is, and redraw_slot() replaces it with the slot's real extent.
 */
static void slot_box_note(int x, int y, const KtuiCell *c)
{
	unsigned s;

	if (!KTUI_IS_SPRITE(c->ch))
		return;
	s = KTUI_SPRITE_SLOT(c->ch);
	if (s >= KCON_MAX_SPRITE_MAP)
		return;
	if (slot_x1[s] < 0) {
		slot_x0[s] = slot_x1[s] = (short)x;
		slot_y0[s] = slot_y1[s] = (short)y;
		return;
	}
	if (x < slot_x0[s])
		slot_x0[s] = (short)x;
	if (x > slot_x1[s])
		slot_x1[s] = (short)x;
	if (y < slot_y0[s])
		slot_y0[s] = (short)y;
	if (y > slot_y1[s])
		slot_y1[s] = (short)y;
}

static void usage(FILE *f)
{
	fprintf(f,
"kdos-view — a display for a kdos-con session\n"
"\n"
"  --socket PATH      the session to attach to; $KDOS_CON otherwise\n"
"  --kms              take a screen: modeset, seat and input of its own\n"
"  --kms-only         --kms, and a failure to take the screen is an error\n"
"                     rather than a fall back to this terminal\n"
"  --card PATH        which DRM device to take, for a machine with more\n"
"                     than one; without it the first with a connected\n"
"                     output wins\n"
"  --buffers N        scanout buffers a screen may hold, 1 to 3. Three\n"
"                     lets the next frame be composed while a flip is\n"
"                     still in flight; a driver with no memory for it\n"
"                     gets fewer\n"
"  --fastest-mode     take the highest refresh at the size the monitor\n"
"                     asked for, instead of the monitor's own choice\n"
"  --tearing          present each frame as it is composed instead of at\n"
"                     the vblank: less latency, and a moving edge is cut\n"
"                     across the screen\n"
"  --tty              draw in this terminal\n"
"  --shot FILE.png    take one frame and write it as a picture\n"
"  --crop X,Y,W,H     the part of the grid a shot covers, in cells\n"
"  --dump [COLSxROWS] take one frame and write it as cells; without a\n"
"                     size it takes the session's own grid\n"
"  --cast             rasterise into a PipeWire stream instead of onto a\n"
"                     screen — a view nobody looks at. Prints its node id\n"
"  --observe          watch and do not type: no key and no pointer is sent,\n"
"                     and the session refuses both from this view\n"
"  --record FILE      write everything the session sends to FILE while it\n"
"                     draws. KDOS's own format, not an asciicast\n"
"  --replay FILE      draw a recording instead of attaching to a session\n"
"  --help\n");
}

/*
 * WHAT THIS VIEW CAN SHOW, told to the session at hello.
 *
 * A view that rasterises has a cell size in pixels and can put a sprite's
 * bytes on a screen; a terminal view has neither, and says so. The session
 * uses the numbers to size an embedded pixel guest and the flag to decide how
 * often it may send one — a window of pixels at a compositor's frame rate down
 * an ssh link is a link that does nothing else.
 */
static int cap_cell_w, cap_cell_h;

/*
 * A VIEW THAT WATCHES AND DOES NOT TYPE.
 *
 * It sends no key and no pointer, and the session refuses both from it anyway:
 * over a forwarded socket this is the difference between showing somebody a
 * problem and handing them the machine, and a promise the client keeps by
 * itself is decorative.
 */
static int observe;
static unsigned cap_flags;

/*
 * WHETHER THE SESSION HAS ASKED FOR THE RAW STREAM, and which keymap it has
 * already been told about.
 *
 * OFF IS WHERE EVERY VIEW STARTS. The raw stream is one message per device
 * event where the cell stream is one per cell crossed, so it is sent only
 * while the session says something can use it — an embedded pixel guest holds
 * the focus — and a view that is never asked sends none of it, which is the
 * whole of what an unasked view does.
 */
static int raw_on;
static unsigned raw_keymap_gen;

#ifdef KDOS_VIEW_KMS
/* The font this view was STARTED with — its flag, its environment, or the
 * built-in default as an empty string. A reset goes back to this rather than
 * to a size this file believes in, so `con.conf` stays the answer to what the
 * console's font is. */
static char font_base[192];
#endif

static int attach(const char *path, int cols, int rows)
{
	struct sockaddr_un sa;

	if (!path || !*path || strlen(path) >= sizeof(sa.sun_path))
		return -1;

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
		return -1;

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, strlen(path));

	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		close(fd);
		return -1;
	}

	conn = kcon_conn_new(fd);
	if (!conn)
		return -1;

	KconBuf b = { 0 };

	/*
	 * A REAL KEYBOARD AND A REAL POINTING DEVICE, claimed only by a
	 * backend that holds one. A view drawing in somebody's terminal is
	 * handed characters and a cell — there is no evdev code and no keymap
	 * behind them — so it never claims this and the session never asks.
	 *
	 * AN OBSERVER NEVER CLAIMS IT EITHER. It sends no input at all, and a
	 * capability it would be refused on is a capability it should not have
	 * announced.
	 */
	if (!observe && ktui_backend() && ktui_backend()->poll_raw)
		cap_flags |= KCON_VIEW_RAW;

	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_VIEW);
	kcon_put_u16(&b, (uint16_t)cap_cell_w);
	kcon_put_u16(&b, (uint16_t)cap_cell_h);
	kcon_put_u16(&b, (uint16_t)cap_flags);
	/* AND WHAT THIS VIEW MAY DO. A driver says so by saying nothing —
	 * every view was one before an observer existed — and an observer
	 * says it here, where the server can hold it to it. */
	kcon_put_u16(&b, (uint16_t)(observe ? KCON_RIGHTS_OBSERVE
					    : KCON_RIGHTS_DRIVE));
	kcon_send(conn, KCON_OP_HELLO, &b);

	/* THE VIEW DECIDES THE GRID. The session composites to whatever the
	 * first view says it can show. */
	kcon_buf_reset(&b);
	kcon_put_u16(&b, (uint16_t)cols);
	kcon_put_u16(&b, (uint16_t)rows);
	kcon_put_u16(&b, (uint16_t)cap_cell_w);
	kcon_put_u16(&b, (uint16_t)cap_cell_h);
	kcon_send(conn, KCON_OP_VIEW_SIZE, &b);
	kcon_flush(conn);
	kcon_buf_free(&b);
	return 0;
}

/*
 * Cells go into libktui's own buffer rather than a private one, so the dump
 * and the terminal output are the toolkit's single implementation of what a
 * cell looks like — wide-glyph continuations and sprite fallbacks included.
 */
/*
 * Read until the session says how big its grid is. Called only by a view that
 * asked for no size of its own — the cells cannot be stored before the buffer
 * exists, so this consumes nothing but the CONFIGURE, and the session resends
 * the whole frame to a view it has no previous frame for.
 */
static int wait_for_grid(int *cols, int *rows)
{
	KconMsg m;

	for (int spin = 0; spin < 400; spin++) {
		int r;

		while ((r = kcon_recv(conn, &m)) == 1) {
			if (m.op == KCON_OP_BYE)
				return -1;
			if (m.op != KCON_OP_CONFIGURE)
				continue;

			KconRd rd;

			kcon_rd_init(&rd, m.payload, m.len);
			*cols = (int)kcon_get_u16(&rd);
			*rows = (int)kcon_get_u16(&rd);
			if (rd.err || *cols <= 0 || *rows <= 0)
				return -1;
			return 0;
		}
		if (r < 0)
			return -1;

		struct pollfd p = { .fd = kcon_conn_fd(conn),
				    .events = POLLIN, .revents = 0 };

		poll(&p, 1, 10);
	}
	return -1;
}

/*
 * A PICTURE ARRIVED. Registering it is the DISPLAY's job and not the session's:
 * the session holds no pixel code, and whether these bytes can become pixels
 * at all is a property of this build rather than of the desktop.
 *
 * A build without the pixel libraries drops it and the cells that reference it
 * draw their fallback codepoint — which is what a text backend does anyway,
 * and is why a picture over a plain terminal is characters rather than nothing.
 */
/* The sprite table below is the RASTERISER'S, not the KMS driver's: a shot
 * decodes the same pictures into the same table, so it is compiled in wherever
 * libkcell is. */
#if defined(KDOS_VIEW_KMS) || defined(KDOS_VIEW_SHOT)
/* The loss set is declared below the table it reports on, and the table's
 * eviction hook is above it. */
static void sprite_lost_mark(int slot);

/* pixman hands the image back to its destroy function and free() does not take
 * one; a cast between the two signatures is undefined behaviour. */
static void free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

/*
 * THE SESSION'S SLOT IS NOT THIS TABLE'S SLOT. The session numbers every
 * picture in the desktop; libktui numbers the ones this view is holding, and
 * it reuses and evicts on its own schedule. The cells name the session's
 * number, so the codepoint is rewritten on the way in — the same rewrite
 * kdos-con does when it copies a surface's cells into the grid, for the same
 * reason: two numbering spaces treated as one puts somebody else's picture in
 * a cell.
 *
 * -1 is a picture this view has not been sent, or one the table has taken
 * back, and it draws the fallback mark.
 */
static int view_slot[KCON_MAX_SPRITE_MAP];
static int view_slot_ready;

static void view_slot_init(void)
{
	if (view_slot_ready)
		return;
	for (int i = 0; i < KCON_MAX_SPRITE_MAP; i++)
		view_slot[i] = -1;
	view_slot_ready = 1;
}

/*
 * THE PIXELS BEHIND A SESSION SLOT, KEPT BETWEEN FRAMES, AND BORROWED.
 *
 * An embedded guest republishes every block of its window on every frame, and
 * a block at the shipped cell is 128 KiB. An allocation that size is a fresh
 * mapping: the kernel faults in and zeroes all of its pages the first time the
 * scale writes them, and hands them straight back at the unref — so a fresh
 * image per block per frame spends most of a 1080p frame in the fault handler
 * and nothing else. The buffer is therefore kept and overwritten in place,
 * which is sound because the resample is OP_SRC and covers every pixel of it.
 *
 * REALLOCATED THE MOMENT THE PIXEL SIZE MOVES, which is what a resized block,
 * a font step and a mode change all look like from here: `view_pix_w`/`_h` are
 * the size the kept image really is, never the size that was asked for.
 *
 * THE POINTER IS THE SPRITE TABLE'S, NOT THIS TABLE'S. Exactly one reference
 * exists and ktui_sprite_put() takes it; sprite_free() is the only place it is
 * dropped, and clearing the entry there is what keeps this from naming freed
 * pixels after an eviction, a ktui_sprite_clear() or a refused put.
 */
static pixman_image_t *view_pix[KCON_MAX_SPRITE_MAP];
static int view_pix_w[KCON_MAX_SPRITE_MAP], view_pix_h[KCON_MAX_SPRITE_MAP];

/*
 * How the sprite table hands a picture back when it takes a slot. Registered
 * so the table may evict — without it a full table refuses, and a terminal
 * showing a second picture would show the first one's fallback forever.
 *
 * The key IS the session slot, so this is also where the rewrite above learns
 * that the picture behind one is gone.
 */
static void sprite_free(uint64_t key, const void *pix, void *user)
{
	(void)user;
	if (key < KCON_MAX_SPRITE_MAP) {
#if defined(KDOS_VIEW_TTYPIX)
		/* A placement the terminal is still showing outlives the
		 * table entry that named it; the emitter has to be told. */
		view_ttypix_forget(view_slot[key]);
#endif
		view_slot[key] = -1;
		/*
		 * AND THE KEPT BUFFER GOES WITH IT. The unref below is the
		 * last reference; a slot still naming these pixels would
		 * resample the next frame into freed memory.
		 */
		view_pix[key] = NULL;
		/*
		 * AN EVICTION IS A LOSS LIKE A REFUSAL IS. The cells naming
		 * this key are drawn and the session has already cleared what
		 * it owed, so the block is a hole until the session is told.
		 * A put that replaces this key's own picture marks it here and
		 * clears it on the way out, so only a key left with nothing
		 * behind it stays in the set.
		 */
		sprite_lost_mark((int)key);
	}
	pixman_image_unref((pixman_image_t *)pix);
}
#endif


/* ── a picture on a display with no pixels ──────────────────────────────
 *
 * A VIEW THAT CANNOT SHOW PIXELS STILL RECEIVES THEM, and turns each cell of
 * the picture into the character whose shape covers the same part of a cell —
 * the matcher behind `kdos-ascii` and `Super+A`, run here rather than in the
 * session. The session holds no font and no pixel code; a view holds both or
 * neither, and it is the only end that knows which.
 *
 * That is the whole of "an embedded application over ssh is characters": no
 * special case in the session, and no second path for pictures.
 *
 * THE SHAPE, IN THE PICTURE'S COLOUR REDUCED TO A SLOT. A cell's colour is a
 * palette slot and a photograph's is not, so the average of each cell goes to
 * the nearest slot — the same rule a terminal's SGR colours take, so `kdos
 * theme` moves both.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * TRUE WHEN THIS VIEW HAS A SCREEN OF ITS OWN. A terminal view can place its
 * host terminal's cursor and a screen cannot: on a framebuffer the caret is
 * already a cell in the frame the session sent.
 */
static int own_screen;

#ifdef KDOS_VIEW_PIXELS
/*
 * WHAT THE SPRITE TABLE MAY HOLD, DERIVED FROM THE SCREEN.
 *
 * AN EMBEDDED GUEST IS A GRID OF SPRITES, one per block, and a put the table
 * refuses is a hole: `kcon_view_sprite()` already answered 1, so the session
 * has cleared its owed bit and will never send that block again. A cap below
 * one screenful of pixels therefore damages the largest window on the screen
 * permanently, and any fixed number is right for one screen size and wrong
 * for every other.
 *
 * FOUR SCREENS, because a maximised guest being resized holds blocks cut for
 * two grids at once and a second window is the normal case. The icons are
 * small, numerous and owned for the life of the session, so they get a fixed
 * allowance rather than a share. The floor keeps a grid that has not been
 * sized yet — a shot or a cast, asked before the backend is up — from getting
 * a budget measured against nothing.
 */
enum {
	VIEW_SPRITE_SCREENS = 4,
	VIEW_SPRITE_ICONS = 4u << 20,
	VIEW_SPRITE_FLOOR = 16u << 20,
};

static size_t view_sprite_budget(int cell_w, int cell_h)
{
	size_t px;

	if (ktui_w < 1 || ktui_h < 1 || cell_w < 1 || cell_h < 1)
		return VIEW_SPRITE_FLOOR;

	px = (size_t)ktui_w * (size_t)ktui_h *
	     (size_t)cell_w * (size_t)cell_h;

	px = px * 4 * VIEW_SPRITE_SCREENS + VIEW_SPRITE_ICONS;
	return px < VIEW_SPRITE_FLOOR ? VIEW_SPRITE_FLOOR : px;
}
#endif

/*
 * A SHOT'S CAP IS TAKEN AGAIN ONCE THE OFFSCREEN GRID EXISTS. ktui_draw_init()
 * is what sets ktui_w and ktui_h, and view_sprite_budget() measures nothing
 * else, so the cap taken where the shot sink was chosen is the floor: a
 * photograph of a large session with a maximised guest in it would refuse
 * blocks of that window, and a refused block is a hole the session will never
 * fill again. A `--dump` reaches the same call sites and holds no pixels at
 * all, so only a shot recomputes.
 */
static void view_shot_budget(const char *shot)
{
#ifdef KDOS_VIEW_SHOT
	if (shot)
		ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
				   kcell_w(), kcell_h());
#else
	(void)shot;
#endif
}

#ifdef KDOS_VIEW_KMS
/*
 * THE FACES THIS VIEW LAST OFFERED, and how many. The session sends back an
 * INDEX into exactly this table, so it is what an index is checked against —
 * a list rebuilt shorter between the ask and the answer would otherwise be an
 * index off the end of it.
 */
static char font_names[VIEW_FONT_MAX][VIEW_FONT_NAME];
static int font_nnames;

/*
 * WEAR THIS FACE AND SAY WHAT THE GRID BECAME.
 *
 * The grid is DERIVED — the backend divides the mode by the cell — so a font
 * with a different cell is a different number of columns and rows, and the
 * announcement is an ordinary KCON_OP_VIEW_SIZE because that is the same event
 * as a screen being resized.
 *
 * EVERY PICTURE WAS CUT FOR THE OLD CELL and is dropped rather than stretched:
 * the session resends when it sees the grid move, and a view that scaled what
 * it held would show one sharp desktop and one blurred one on a machine with
 * two screens.
 *
 * `how` says what becomes of it. FONT_RESET is "back to what this view was
 * started with", and it REMOVES the state file rather than writing the same
 * name into it: a person who put the font back has asked for the default, not
 * for a pin on today's default. FONT_PREVIEW writes nothing at all — a
 * picker's arrows walk a list and every step is a real font on a real screen,
 * and a step that persisted would make the last face the highlight passed
 * over the one the next login comes up in.
 */
enum { FONT_PREVIEW = 0, FONT_KEEP, FONT_RESET };

static void font_apply(KconConn *conn, const char *want, int how)
{
	char path[512];

	if (kkms_set_font(want && *want ? want : NULL) != 0)
		return;

	ktui_draw_resize();
	ktui_sprite_clear();
	ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
			   kcell_w(), kcell_h());
	ktui_draw_invalidate();

	cap_cell_w = kcell_w();
	cap_cell_h = kcell_h();

	KconBuf sz = { 0 };

	kcon_put_u16(&sz, (uint16_t)ktui_w);
	kcon_put_u16(&sz, (uint16_t)ktui_h);
	kcon_put_u16(&sz, (uint16_t)cap_cell_w);
	kcon_put_u16(&sz, (uint16_t)cap_cell_h);
	kcon_send(conn, KCON_OP_VIEW_SIZE, &sz);
	kcon_flush(conn);
	kcon_buf_free(&sz);

	/* WRITTEN AFTER THE FONT LOADED, never before: a name that fcft
	 * refuses would otherwise be the name the next login starts with, and
	 * the session would come up on a screen nobody can read. */
	if (how == FONT_PREVIEW || !view_font_state_path(path, sizeof(path)))
		return;
	if (how == FONT_RESET) {
		unlink(path);
		return;
	}

	char line[VIEW_FONT_NAME + 2], *slash;

	slash = strrchr(path, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(path);
		*slash = '/';
	}
	snprintf(line, sizeof(line), "%s\n", kkms_font());
	kb_write_file_atomic(path, line);
}
#endif

#ifdef KDOS_VIEW_PIXELS
typedef struct {
	int cw, ch;		/* the picture's size in cells */
	uint32_t *cp;		/* cw*ch matched codepoints */
	uint8_t *fg;		/* cw*ch palette slots */
} AsciiPic;

static AsciiPic *ascii_pic[KCON_MAX_SPRITE_MAP];

/*
 * WHERE THIS VIEW CAN PUT A PICTURE'S PIXELS, which is the one thing that
 * decides whether a sprite is kept or matched to a shape.
 *
 *   RASTER  a screen, a cast or a file — libkcell paints the cells
 *   TTYPIX  a host terminal that answered the probe — ttypix.c emits them
 *   ASCII   nowhere; the shape matcher is all there is
 *
 * A sink that is not ASCII must reach `ktui_sprite_put()`, or the pixels exist
 * only for the length of the call that delivered them.
 */
enum { SINK_ASCII = 0, SINK_RASTER, SINK_TTYPIX };
static int sink;

/* The sink's cell in pixels. A terminal's comes from its own answer to the
 * probe; everything else is the font libkcell loaded. */
static int pix_cw, pix_ch;

static int sink_cell_w(void)
{
	return sink == SINK_TTYPIX ? pix_cw : kcell_w();
}

static int sink_cell_h(void)
{
	return sink == SINK_TTYPIX ? pix_ch : kcell_h();
}

/* What the SESSION said a picture should look like without pixels. Kept per
 * session slot so a view that cannot draw one still draws its mark. */
static uint32_t sess_fb[KCON_MAX_SPRITE_MAP];

/* 0 untried, 1 measured, -1 refused. A machine with no font at all draws the
 * fallback mark, which is what a picture has always looked like here. */
static int ascii_font;

static void ascii_drop(int slot)
{
	AsciiPic *a = ascii_pic[slot];

	if (!a)
		return;
	ascii_pic[slot] = NULL;
	free(a->cp);
	free(a->fg);
	free(a);
}

static void ascii_take(int slot, int cw, int ch, const uint32_t *argb,
		       int pw, int ph)
{
	ascii_drop(slot);

	if (ascii_font == 0) {
		/*
		 * The candidate set is measured against a font, and the
		 * matcher needs one loaded to know which glyphs the font
		 * carries. It is never rasterised onto anything: this view
		 * draws characters, and the font is here to decide which.
		 */
		ascii_font = kcell_font_load(NULL) == 0 &&
			     kcell_ascii_init() > 0 ? 1 : -1;
	}
	if (ascii_font < 0)
		return;

	/* How many pixels of the picture fall in one of its cells. Under two
	 * there is no shape left to match and the fallback mark says more. */
	int mcw = pw / cw, mch = ph / ch;

	if (mcw < 2 || mch < 2)
		return;

	AsciiPic *a = calloc(1, sizeof(*a));

	if (!a)
		return;

	uint32_t *cp = calloc((size_t)cw * ch, sizeof(*cp));
	uint32_t *tint = calloc((size_t)cw * ch, sizeof(*tint));
	uint8_t *fg = calloc((size_t)cw * ch, 1);

	if (!a || !cp || !tint || !fg) {
		free(a);
		free(cp);
		free(tint);
		free(fg);
		return;
	}

	int oc = 0, orow = 0;

	if (kcell_ascii_image(argb, mcw * cw, mch * ch, pw, mcw, mch, cp, tint,
			      &oc, &orow) != 0 || oc < cw || orow < ch) {
		free(a);
		free(cp);
		free(tint);
		free(fg);
		return;
	}

	for (int i = 0; i < cw * ch; i++)
		fg[i] = (uint8_t)ktui_theme_nearest(tint[i] & 0xffffffu);
	free(tint);

	a->cw = cw;
	a->ch = ch;
	a->cp = cp;
	a->fg = fg;
	ascii_pic[slot] = a;
}

/*
 * Substitute in place. Answers 0 for a sprite cell with no picture behind it
 * yet, which leaves the codepoint alone and lets the backend draw the
 * fallback mark — a picture that renders as nothing is indistinguishable from
 * output that never arrived.
 */
static int ascii_cell(uint32_t *ch, int *fg)
{
	if (sink == SINK_RASTER || !KTUI_IS_SPRITE(*ch))
		return 0;

	unsigned slot = KTUI_SPRITE_SLOT(*ch);

	if (slot >= KCON_MAX_SPRITE_MAP || !ascii_pic[slot])
		return 0;

	AsciiPic *a = ascii_pic[slot];
	int sx = (int)KTUI_SPRITE_SX(*ch), sy = (int)KTUI_SPRITE_SY(*ch);

	if (sx >= a->cw || sy >= a->ch)
		return 0;

	*ch = a->cp[sy * a->cw + sx];
	*fg = a->fg[sy * a->cw + sx];
	return 1;
}
#endif

#ifdef KDOS_VIEW_PIXELS
/*
 * PICTURES THAT ARRIVED AND COULD NOT BE KEPT, one bit per session slot.
 *
 * kcon_view_sprite() ANSWERS FOR THE WIRE AND NOT FOR THE SCREEN: the cells
 * naming a refused slot are already drawn and the session has already cleared
 * what it owed, so a refusal at this end is a hole in the window for the life
 * of the window unless this view says so. KCON_OP_SPRITE_LOST is how it says
 * so, and the session owes those blocks again.
 *
 * A SET PER PRESENTED FRAME, NEVER A MESSAGE PER LOSS. A table short of
 * budget refuses a picture per block per frame, and a message each would
 * spend the queue the pictures themselves need — the coalescing here is the
 * flood bound, and the protocol makes it the sender's. A slot stored
 * successfully leaves the set, or the view asks for a picture it is holding.
 *
 * ONLY A VIEW THAT KEEPS PIXELS REPORTS. A build with no pixel library draws
 * a sprite cell's fallback mark by design, which is not a loss, and reporting
 * it would ask the session to re-send for ever.
 */
static uint32_t sprite_lost[KCON_MAX_SPRITE_MAP / 32];
static int sprite_nlost;

static void sprite_lost_mark(int slot)
{
	uint32_t bit = 1u << (slot & 31);

	if (sprite_lost[slot >> 5] & bit)
		return;
	sprite_lost[slot >> 5] |= bit;
	sprite_nlost++;
}

static void sprite_lost_drop(int slot)
{
	uint32_t bit = 1u << (slot & 31);

	if (!(sprite_lost[slot >> 5] & bit))
		return;
	sprite_lost[slot >> 5] &= ~bit;
	sprite_nlost--;
}

static void sprite_lost_flush(void)
{
	KconBuf b = { 0 };

	if (!sprite_nlost)
		return;
	if (conn && !kcon_conn_dead(conn)) {
		kcon_put_u16(&b, (uint16_t)sprite_nlost);
		for (int w = 0; w < KCON_MAX_SPRITE_MAP / 32; w++) {
			if (!sprite_lost[w])
				continue;
			for (int i = 0; i < 32; i++)
				if (sprite_lost[w] & (1u << i))
					kcon_put_u16(&b,
						(uint16_t)(w * 32 + i));
		}
		kcon_send(conn, KCON_OP_SPRITE_LOST, &b);
		kcon_buf_free(&b);
	}
	memset(sprite_lost, 0, sizeof(sprite_lost));
	sprite_nlost = 0;
}
#endif

static void take_sprite(const unsigned char *payload, size_t len)
{
	KconRd r;
	int slot, cw, ch, pw, ph;
	uint32_t fallback;
	const void *argb;

	kcon_rd_init(&r, payload, len);
	slot = (int)kcon_get_u16(&r);
	cw = (int)kcon_get_u16(&r);
	ch = (int)kcon_get_u16(&r);
	fallback = kcon_get_u32(&r);
	pw = (int)kcon_get_u16(&r);
	ph = (int)kcon_get_u16(&r);
	if (r.err || cw < 1 || ch < 1)
		return;

	/* THE SLOT INDEXES THREE TABLES IN HERE, so it is bounded once at the
	 * top rather than at each of them: the far end of a view socket is
	 * not always this machine or even this build, and a slot the session
	 * could never hand out is exactly what a forwarded link delivers. */
	if (slot < 0 || slot >= KCON_MAX_SPRITE_MAP)
		return;

	/* The declared size is an allocation request from the session, which
	 * got it from a client: bounded before the blob is read, and against
	 * what a sprite can be rather than against what will fit. */
	if (pw < 0 || ph < 0 || pw > 16 * 256 || ph > 16 * 256)
		return;

	size_t npx = (size_t)pw * (size_t)ph;

	argb = npx ? kcon_get_blob(&r, npx * 4) : NULL;
	if (r.err)
		return;

#ifdef KDOS_VIEW_PIXELS
	if (!argb || !npx) {
		(void)slot;
		return;
	}

	sess_fb[slot] = fallback;

	if (sink == SINK_ASCII) {
		ascii_take(slot, cw, ch, argb, pw, ph);
		redraw_slot((unsigned)slot);
		return;
	}

	/*
	 * THE DISPLAY IS WHAT KNOWS HOW MANY PIXELS A CELL IS, so the picture
	 * arrives at whatever size its sender had and is resampled here. A
	 * cell client has no pixel size of its own, two views of one session
	 * can be running different fonts, and a view over ssh is a third
	 * answer — so scaling at the sender could only ever be right for one
	 * of them.
	 */
	int dw = cw * sink_cell_w();
	int dh = ch * sink_cell_h();

	if (dw <= 0 || dh <= 0) {
		sprite_lost_mark(slot);
		return;
	}

	/*
	 * THE BUFFER THIS SLOT ALREADY HAS IS WRITTEN AGAIN, and a new one is
	 * taken only when the pixel size has moved. The resample below is
	 * OP_SRC over the whole image, so every pixel of it is replaced and
	 * nothing of the last frame can show through — which is the entire
	 * licence for reusing it. A guest at 60 fps publishing a screenful of
	 * 128 KiB blocks otherwise maps, faults, zeroes and unmaps every one
	 * of them on every frame, and that churn is the frame budget.
	 *
	 * `reused` also says WHO OWNS `img` on the way out: a kept image is
	 * the sprite table's and must never be unref'd here, a fresh one is
	 * this function's until the put takes it.
	 */
	int reused = view_pix[slot] && view_pix_w[slot] == dw &&
		     view_pix_h[slot] == dh;
	pixman_image_t *img = reused ? view_pix[slot] : NULL;

	if (!img) {
		uint32_t *bits = calloc((size_t)dw * (size_t)dh, 4);

		if (!bits) {
			sprite_lost_mark(slot);
			return;
		}

		img = pixman_image_create_bits(PIXMAN_a8r8g8b8, dw, dh, bits,
					       dw * 4);

		if (!img) {
			free(bits);
			sprite_lost_mark(slot);
			return;
		}
		pixman_image_set_destroy_function(img, free_bits, bits);
	}

	pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, pw, ph,
						       (uint32_t *)argb,
						       pw * 4);

	if (!src) {
		if (!reused)
			pixman_image_unref(img);
		sprite_lost_mark(slot);
		return;
	}

	if (pw != dw || ph != dh) {
		/*
		 * 16.16 fixed point, and the transform maps DESTINATION back
		 * to source — so the ratio is source over destination, not the
		 * other way round. Inverting it scales by the reciprocal and
		 * looks like a decoder bug.
		 */
		struct pixman_transform t;

		pixman_transform_init_scale(&t,
			(pixman_fixed_t)(((int64_t)pw << 16) / dw),
			(pixman_fixed_t)(((int64_t)ph << 16) / dh));
		pixman_image_set_transform(src, &t);
		pixman_image_set_filter(src, PIXMAN_FILTER_BILINEAR, NULL, 0);
	}
	pixman_image_composite32(PIXMAN_OP_SRC, src, NULL, img,
				 0, 0, 0, 0, 0, 0, dw, dh);
	pixman_image_unref(src);

	/*
	 * The session's slot IS the key: it already namespaces every surface,
	 * so two windows showing different pictures cannot collide, and a
	 * surface re-sending a slot replaces its own picture.
	 */
	view_slot_init();

	/*
	 * A TERMINAL'S PLACEMENTS GO BACK WHENEVER THE PIXELS UNDER THEM DO.
	 * The emitter keeps, per table slot, what it has already drawn and
	 * where; a picture rewritten in place is a placement whose contents no
	 * longer match it. The sprite table announces a picture it REPLACED
	 * through its evictor, and one rewritten in the buffer it already
	 * holds is the single case the table cannot see — so it is said here.
	 */
#if defined(KDOS_VIEW_TTYPIX)
	if (reused)
		view_ttypix_forget(view_slot[slot]);
#endif

	int vs = ktui_sprite_put((uint64_t)slot, img, cw, ch, fallback);

	/*
	 * A REFUSAL IS NOT A SLOT AND IT DOES NOT CLEAR THE ONE HELD. The
	 * table makes room before it writes, so a refused put leaves whatever
	 * this key already holds in place — storing the answer would blank a
	 * block that still has its last frame behind it. The picture the table
	 * turns away is this view's to free, and the session is told so the
	 * block is owed again — unless the picture the table turned away is
	 * the one it is already holding for this key, which is still its own
	 * and whose last frame is still on the screen.
	 */
	if (vs < 0) {
		if (!reused)
			pixman_image_unref(img);
		sprite_lost_mark(slot);
		return;
	}
	sprite_lost_drop(slot);
	view_slot[slot] = vs;
	/*
	 * RECORDED ONLY ONCE THE TABLE HAS TAKEN IT. A put that replaced a
	 * differently sized picture has already cleared this entry through
	 * sprite_free(), so the write has to come after it and not before.
	 */
	view_pix[slot] = img;
	view_pix_w[slot] = dw;
	view_pix_h[slot] = dh;
	redraw_slot((unsigned)slot);
#else
	(void)slot;
	(void)fallback;
	(void)argb;
#endif
}

/*
 * A SESSION SLOT WENT BACK TO THE ROTATION, SO THE PICTURE BEHIND IT GOES NOW.
 *
 * The rotation will hand the number to another window's block when the search
 * comes round to it, and until it does nobody owns the number and nothing
 * sends a picture under it. Pixels still held here would sit in this view's
 * byte budget with nothing that could ever replace them, they would be the
 * next owner's picture until it published its own, and the eviction that
 * eventually took them would be reported as a loss of a slot that owner never
 * lost — spending the repair allowance of a window with nothing wrong with it.
 *
 * DROPPED, NOT EVICTED. ktui_sprite_drop() does not call the evictor, because
 * the owner of the pixels is the caller and a callback into it would be a free
 * from inside its own call — so the unref is this function's, and so is every
 * table entry sprite_free() would have cleared.
 *
 * AND IT IS NOT A LOSS. A loss asks the session for the picture again; nothing
 * owns this number any more, and asking would be asking for ever.
 */
static void drop_sprite(const unsigned char *payload, size_t len)
{
	KconRd r;
	int slot;

	kcon_rd_init(&r, payload, len);
	slot = (int)kcon_get_u16(&r);
	if (r.err || slot < 0 || slot >= KCON_MAX_SPRITE_MAP)
		return;

#ifdef KDOS_VIEW_PIXELS
	view_slot_init();
	ascii_drop(slot);
	sess_fb[slot] = 0;
#if defined(KDOS_VIEW_TTYPIX)
	view_ttypix_forget(view_slot[slot]);
#endif
	ktui_sprite_drop((uint64_t)slot);
	if (view_pix[slot])
		pixman_image_unref(view_pix[slot]);
	view_pix[slot] = NULL;
	view_pix_w[slot] = 0;
	view_pix_h[slot] = 0;
	view_slot[slot] = -1;
	sprite_lost_drop(slot);
	/*
	 * THE CELLS NAMING IT ARE REPAINTED HERE. They are still on the screen
	 * over pixels that have just been freed, and the frame that replaces
	 * them is a message away — a display that waited for it would show a
	 * picture whose memory is gone.
	 */
	redraw_slot((unsigned)slot);
#endif
}

#ifdef KDOS_VIEW_PIXELS
/*
 * The session's slot in the cell, rewritten to this view's.
 *
 * A SLOT WITH NO PICTURE BEHIND IT BECOMES THE SESSION'S OWN MARK rather than
 * a table entry nobody has. Naming a slot the table has never heard of makes
 * the backend write a space — indistinguishable from a picture that never
 * arrived — and a whole pane of them is what "nothing appeared" looks like.
 * The mark goes in the top-left cell only, so a 2x2 icon does not become four
 * identical blocks.
 *
 * Only where a picture was expected: a `--dump` writes what it has always
 * written, because its output is a golden.
 */
static uint32_t present(uint32_t ch)
{
	if (!KTUI_IS_SPRITE(ch))
		return ch;

	unsigned slot = KTUI_SPRITE_SLOT(ch);
	int vs = slot < KCON_MAX_SPRITE_MAP ? view_slot[slot] : -1;

	if (vs < 0 && sink == SINK_TTYPIX) {
		uint32_t fb = slot < KCON_MAX_SPRITE_MAP ? sess_fb[slot] : 0;

		if (KTUI_SPRITE_SX(ch) || KTUI_SPRITE_SY(ch))
			return ' ';
		return fb ? fb : 0x2593u;
	}
	if (vs < 0)
		vs = 0xffff;
	return (ch & ~(0xffffu << 8)) | ((uint32_t)vs << 8);
}
#endif

#if defined(KDOS_VIEW_CAST) && defined(KDOS_VIEW_KMS)
/* ── the cast backend ────────────────────────────────────────────────────
 *
 * A view that rasterises into a PipeWire stream instead of onto a screen. Its
 * flush is the KMS one's without the scanout: the same cell painter, the same
 * row diff, the same glyph cache.
 * ──────────────────────────────────────────────────────────────────────── */

static pixman_image_t *cast_img;
static uint32_t *cast_bits;
static int cast_cols, cast_rows, cast_pw, cast_ph;

static void cast_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		       int force_full)
{
	if (!cast_img)
		return;
	kcell_paint(cast_img, cur, prev, w, h, force_full, 1, cast_pw,
		    cast_ph);
	kcast_push(cast_bits);
}

/* A RECORDING IS NOT A SEAT. Nothing is typed into a cast and nothing is
 * pointed at one, so it never sends input to the session. */
static int cast_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)timeout_ms;
	ev->type = KT_EVT_TICK;
	return 0;
}

static void cast_size(int *w, int *h)
{
	*w = cast_cols;
	*h = cast_rows;
}

static int cast_caps(void)
{
	return KT_CAP_TRUECOLOR | KT_CAP_UTF8;
}

static const KtuiBackend cast_backend = {
	.name = "cast",
	.flush = cast_flush,
	.poll_event = cast_poll,
	.size = cast_size,
	.caps = cast_caps,
};
#endif

static void draw_one(int x, int y, const KtuiCell *c)
{
	KtuiCell out = *c;
	uint32_t cp = c->ch;
	int fg = c->fg;

#ifdef KDOS_VIEW_PIXELS
	if (!ascii_cell(&cp, &fg))
		cp = present(cp);
#endif
	/* The cell goes on whole: the fallback path may rewrite the codepoint
	 * and the slot, and everything else — the attributes, and a literal
	 * colour where the session sent one — is the session's to decide. */
	out.ch = cp;
	out.fg = (uint8_t)fg;
	ktui_draw_put(x, y, &out);
}

#ifdef KDOS_VIEW_PIXELS
/*
 * A PICTURE ARRIVED FOR CELLS THAT ARE ALREADY DRAWN. Only those cells are
 * repainted, out of the copy of what the session sent.
 *
 * AND THOSE CELLS ARE MARKED FOR REPAINT THOUGH NOTHING IN THEM CHANGED. A
 * sprite cell encodes the SLOT, not the picture — so an animation's next frame
 * writes byte-identical cells, the flush's diff finds nothing to send, and the
 * screen holds the first frame for ever.
 *
 * THE RECTANGLE THE SLOT COVERS, NEVER THE SCREEN — neither to search for its
 * cells nor to repaint them. A slot is one 16x16-cell block of one window and
 * an embedded application publishes a block at a time, so a whole-screen
 * repaint here costs every glyph on the desktop and a whole framebuffer
 * upload dozens of times per guest frame, and a whole-screen SEARCH costs the
 * cell count of the desktop as many times again. The slot's box is kept where
 * the cells are written, and what this walk finds replaces it — so a block
 * that moved costs the union of both places once and its own size thereafter.
 *
 * NOT FOR A PIXEL EMITTER, which compares `KtuiSprite.gen` and needs no help.
 */
static void redraw_slot(unsigned slot)
{
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
	int bx0, by0, bx1, by1;

	if (!shadow || slot >= KCON_MAX_SPRITE_MAP || slot_x1[slot] < 0)
		return;
	bx0 = slot_x0[slot];
	by0 = slot_y0[slot];
	bx1 = slot_x1[slot];
	by1 = slot_y1[slot];
	for (int y = by0; y <= by1; y++)
		for (int x = bx0; x <= bx1; x++) {
			const KtuiCell *c = &shadow[y * shadow_w + x];

			if (!KTUI_IS_SPRITE(c->ch) ||
			    KTUI_SPRITE_SLOT(c->ch) != slot)
				continue;
			draw_one(x, y, c);
			if (x1 < 0) {
				x0 = x1 = x;
				y0 = y1 = y;
				continue;
			}
			if (x < x0)
				x0 = x;
			if (x > x1)
				x1 = x;
			if (y < y0)
				y0 = y;
			if (y > y1)
				y1 = y;
		}
	slot_x0[slot] = (short)x0;
	slot_y0[slot] = (short)y0;
	slot_x1[slot] = (short)x1;
	slot_y1[slot] = (short)y1;
	if (x1 >= 0 && sink != SINK_TTYPIX)
		ktui_draw_dirty(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}
#endif

/*
 * ONE MESSAGE FROM THE SESSION, DRAWN.
 *
 * Split out of the receive loop because a REPLAY feeds the same handler from a
 * file: a recording is this protocol's own messages with timestamps, so the
 * drawing has to be reachable from something that is not a socket. Returns 1
 * when the screen changed, 0 when nothing did, -1 when the session said
 * goodbye.
 */
/*
 * WHY THIS VIEW STOPPED, AND IT DECIDES THE EXIT CODE.
 *
 * A supervisor reads a clean exit as "a person asked for the screen back" and
 * stops supervising; it reads a failure as "try again". Those are two
 * different endings and the socket cannot tell them apart — a session that
 * said goodbye and a session that dropped this view both leave a closed
 * connection behind. Only KCON_OP_BYE means the first, so only KCON_OP_BYE
 * ends the process successfully; anything else is a display that went away on
 * its own and must come back, or the screen keeps the last frame it flipped
 * for the rest of the login with nothing able to type at it.
 */
static int said_bye;

/*
 * THE FRAME CONTRACT, this end. A frame is open from its first cell run
 * until the session's KCON_OP_FRAME closes it, and it is presented then and
 * not at a message boundary inside it — a colour run patches the run before
 * it, and a view that painted between the two showed a frame in eight slots
 * and the next in the right colours. `frame_done` is the boundary having
 * landed, and the answer the session is owed goes back after the paint, so
 * the session's next frame is paced by this display and not by a timer.
 *
 * A frame left open longer than this is presented anyway: a session under
 * load must not hold the screen, and what it sends later closes the next.
 */
#define VIEW_FRAME_HOLD_MS 50
static int frame_open, frame_done;
static unsigned long long frame_open_at;

static unsigned long long mono_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000 +
	       (unsigned long long)ts.tv_nsec / 1000000;
}

static void frame_ack(void)
{
	if (!conn || kcon_conn_dead(conn) || !(cap_flags & KCON_VIEW_FRAME))
		return;
	kcon_send(conn, KCON_OP_FRAME, NULL);
}

/*
 * Present what has arrived, unless a frame is still open and young: then the
 * rest of it is a read away and the paint waits for the boundary. The answer
 * goes back after the paint, which is the whole point of answering at all.
 *
 * AND THIS IS WHERE THE PICTURES THAT COULD NOT BE KEPT GO BACK, one message
 * for the whole frame's set. A view with no frame contract still presents, so
 * the report rides the paint rather than the acknowledgement — a display that
 * never answers a boundary would otherwise never report a loss at all.
 */
static void view_present(void)
{
	if (frame_open && !frame_done &&
	    mono_ms() - frame_open_at < VIEW_FRAME_HOLD_MS)
		return;
	ktui_draw_flush();
#ifdef KDOS_VIEW_PIXELS
	sprite_lost_flush();
#endif
	frame_open = 0;
	if (frame_done) {
		frame_done = 0;
		frame_ack();
	}
}

/*
 * ── the raw stream ──────────────────────────────────────────────────────
 *
 * THE SAME PHYSICAL INPUT AS send_key() AND send_ptr(), UNRESOLVED. Those two
 * carry a character and a cell, which is the whole of what a cell desktop
 * wants and none of what a pixel guest embedded in a window can use: it holds
 * a key down, repeats from its own keymap, reads a modifier that produces no
 * character, and aims at controls smaller than the grid pointing at them.
 *
 * THE COOKED MESSAGE FOR ONE PHYSICAL INPUT ALWAYS GOES FIRST, and its raw
 * partner follows it rather than following the whole batch: that is what lets
 * the session decide whether a chord ate THAT key, and which window the
 * pointer is over, before the guest is handed it. Two keys drained as two
 * cooked messages and then two raw ones leave the session no way to say which
 * press its chord swallowed, so the two queues are drained TOGETHER, against
 * the cooked count each raw event carries in KtuiRaw.after.
 *
 * THE VIEW DOES NOT ROUTE, and nothing here reads a window. It reports what
 * the device did; where it lands is the session's single answer to give.
 * ──────────────────────────────────────────────────────────────────────── */

/* libktui's numbers are its own and so are libkcon's; neither is the other's,
 * so the two are joined by a switch rather than by an equality nobody can
 * see. */
static int wire_axis_of(int axis)
{
	return axis == KT_RAW_HORIZ ? KCON_AXIS_HORIZ : KCON_AXIS_VERT;
}

static int wire_src_of(int src)
{
	switch (src) {
	case KT_RAW_SRC_FINGER:
		return KCON_AXIS_SRC_FINGER;
	case KT_RAW_SRC_CONTINUOUS:
		return KCON_AXIS_SRC_CONTINUOUS;
	case KT_RAW_SRC_WHEEL_TILT:
		return KCON_AXIS_SRC_WHEEL_TILT;
	default:
		return KCON_AXIS_SRC_WHEEL;
	}
}

/*
 * THE LAYOUT THIS VIEW'S KEYBOARD IS RUNNING. A keycode means nothing without
 * it: a guest handed codes alone reads US positions, and a French keyboard
 * types the wrong letters.
 *
 * SENT WHEN THE RAW STREAM IS FIRST ASKED FOR AND WHENEVER IT CHANGES, never
 * at hello — a session that opens no guest would have paid tens of kilobytes
 * on a link that may be ssh for nothing. As BYTES and never a descriptor,
 * which is what keeps this socket forwardable.
 */
static void send_keymap(void)
{
	const KtuiBackend *b = ktui_backend();
	const char *text;
	unsigned gen = 0;
	size_t n;

	if (!b || !b->keymap)
		return;
	text = b->keymap(&gen);
	if (!text || gen == raw_keymap_gen)
		return;

	n = strlen(text) + 1;		/* the terminator travels with it */
	if (n > KCON_KEYMAP_MAX)
		return;

	KconBuf buf = { 0 };

	kcon_put_u8(&buf, KCON_KEYMAP_XKB_V1);
	kcon_put_u32(&buf, (uint32_t)n);
	kcon_put_bytes(&buf, text, n);
	kcon_send(conn, KCON_OP_KEYMAP, &buf);
	kcon_buf_free(&buf);
	raw_keymap_gen = gen;
}

/*
 * HOW MANY COOKED EVENTS THIS VIEW HAS TAKEN FROM THE BACKEND, and the one
 * raw event taken from the queue whose turn has not come.
 *
 * The backend counts the same events, so the two numbers name the same event:
 * a raw event is sent once this view has sent every cooked event its `after`
 * counts. The peek is what makes that decidable — poll_raw pops, so an event
 * whose turn has not come is held here until it has.
 */
static unsigned long long cooked_taken;
static KtuiRaw raw_held;
static int raw_have;

static void raw_send(const KtuiRaw *rp, int forward)
{
	KconBuf buf = { 0 };
	KtuiRaw r = *rp;

	if (!forward)
		return;

	if (r.type == KT_RAW_KEY) {
		/* A KEYCODE IS AN INDEX at the far end — what holds a bit per
		 * key is sized from this bound — and the server refuses one
		 * past it, so a device reporting a code nothing can hold is
		 * dropped here rather than silently at the socket. */
		if (r.code < 0 || r.code > KCON_KEYCODE_MAX)
			return;
		kcon_put_u16(&buf, (uint16_t)r.code);
		kcon_put_u8(&buf, (uint8_t)(r.state ? 1 : 0));
		kcon_put_u32(&buf, r.depressed);
		kcon_put_u32(&buf, r.latched);
		kcon_put_u32(&buf, r.locked);
		kcon_put_u32(&buf, r.group);
		kcon_put_u32(&buf, r.ms);
		kcon_send(conn, KCON_OP_KEY_RAW, &buf);
	} else if (r.type == KT_RAW_PTR) {
		/* THE CELL IS A DIVISOR at the session, which derives the same
		 * cell this view would from the position and these two
		 * numbers. A zero is refused there, so it is never sent. */
		if (r.cell_w <= 0 || r.cell_h <= 0 ||
		    r.cell_w > 0xffff || r.cell_h > 0xffff ||
		    r.code < 0 || r.code > KCON_KEYCODE_MAX)
			return;
		kcon_put_i32(&buf, r.x);
		kcon_put_i32(&buf, r.y);
		kcon_put_u16(&buf, (uint16_t)r.cell_w);
		kcon_put_u16(&buf, (uint16_t)r.cell_h);
		kcon_put_i32(&buf, r.dx);
		kcon_put_i32(&buf, r.dy);
		kcon_put_i32(&buf, r.dx_un);
		kcon_put_i32(&buf, r.dy_un);
		kcon_put_u16(&buf, (uint16_t)r.code);
		kcon_put_u8(&buf, (uint8_t)(r.state ? 1 : 0));
		kcon_put_u8(&buf, (uint8_t)r.mods);
		kcon_put_u32(&buf, r.ms);
		kcon_send(conn, KCON_OP_PTR_RAW, &buf);
	} else if (r.type == KT_RAW_AXIS) {
		kcon_put_i32(&buf, r.value);
		kcon_put_i32(&buf, r.value120);
		kcon_put_u8(&buf, (uint8_t)wire_axis_of(r.axis));
		kcon_put_u8(&buf, (uint8_t)wire_src_of(r.source));
		kcon_put_u8(&buf, (uint8_t)(r.flags & KT_RAW_INVERTED
						    ? KCON_AXIS_INVERTED
						    : 0));
		kcon_put_u8(&buf, (uint8_t)r.mods);
		kcon_put_u32(&buf, r.ms);
		kcon_send(conn, KCON_OP_AXIS_RAW, &buf);
	}
	kcon_buf_free(&buf);
}

/*
 * SEND THE RAW EVENTS THIS VIEW HAS ALREADY SENT THE COOKED EVENTS FOR, and
 * with `all` set the rest of the queue behind them.
 *
 * IT IS DRAINED EVEN WHEN NOTHING IS LISTENING. The queue is deep and the
 * backend evicts its oldest entry when it fills, so a view that left it alone
 * would hand the session a burst of motion from before the guest had the focus
 * the moment it was asked.
 */
static void raw_drain_to(int forward, int all)
{
	const KtuiBackend *b = ktui_backend();

	if (!b || !b->poll_raw)
		return;
	if (observe || !conn || kcon_conn_dead(conn))
		forward = 0;
	if (forward)
		send_keymap();

	for (;;) {
		if (!raw_have) {
			if (!b->poll_raw(&raw_held))
				return;
			raw_have = 1;
		}
		/* A raw event whose cooked partner this view has not sent yet
		 * WAITS, and waits in hand: poll_raw pops, so putting it back
		 * is not available and holding it is how the order is kept. */
		if (!all && raw_held.after > cooked_taken)
			return;
		raw_have = 0;
		raw_send(&raw_held, forward);
	}
}

/*
 * EVERYTHING STILL QUEUED, wherever the cooked count has reached. The end of a
 * pump: what is left has no cooked partner coming, and a raw event held for
 * one that never arrives is a guest that stops moving.
 */
static void raw_drain(int forward)
{
	raw_drain_to(forward, 1);
}

static int handle_msg(unsigned op, const unsigned char *payload, size_t len)
{
	int got = 0;

	if (op == KCON_OP_BYE) {
		said_bye = 1;
		return -1;
	}
	if (op == KCON_OP_FRAME) {
		frame_done = 1;
		return 1;
	}
	if ((op == KCON_OP_COMMIT || op == KCON_OP_COLOR) && !frame_open) {
		frame_open = 1;
		frame_open_at = mono_ms();
	}

	/*
	 * WHAT THE SESSION COPIED, onto the clipboard of the desktop
	 * this view is running on. `ktui_clip_copy` writes OSC 52 and
	 * is a deliberate no-op on a Linux console, so this does
	 * nothing on tty1 and everything in `foot` or over ssh — which
	 * is where a person has another desktop to paste into.
	 */
	if (op == KCON_OP_VIEW_CLIP) {
		KconRd b;

		kcon_rd_init(&b, payload, len);

		const char *text = kcon_get_str(&b);

		if (!b.err && *text)
			ktui_clip_copy(text);
		return got;
	}

	/*
	 * WHERE THE CARET IS. A view holds no window state, so it is
	 * told; a terminal view puts its own cursor there, which is
	 * the one thing the person's own terminal can draw better
	 * than this desktop can paint. A view with a screen of its own
	 * ignores it — the caret is already a cell in the frame it was
	 * sent.
	 */
	if (op == KCON_OP_CURSOR) {
		KconRd b;

		kcon_rd_init(&b, payload, len);

		int cx = (int)kcon_get_i32(&b);
		int cy = (int)kcon_get_i32(&b);

		if (!b.err && !own_screen) {
			ktui_term_caret(cx, cy);
#ifdef KDOS_VIEW_TTYPIX
			view_ttypix_caret(cx, cy);
#endif
		}
		return got;
	}

	/*
	 * A BELL RINGS WHERE THE PERSON IS. A view in somebody's
	 * terminal writes BEL and lets that terminal do whatever it is
	 * configured to do — a sound, a flash, or nothing; a view with
	 * a screen of its own has no sound to make, and the session
	 * has already inverted the window that rang.
	 */
	if (op == KCON_OP_BELL) {
		if (!own_screen) {
			ssize_t r = write(1, "\a", 1);

			(void)r;
		}
		return got;
	}

	/*
	 * START, OR STOP, SENDING RAW INPUT. The session asks only while
	 * something can use it, and anything already captured is dropped on
	 * the transition: a queue filled before a guest had the focus is a
	 * burst of stale motion delivered the moment it gets it.
	 */
	if (op == KCON_OP_VIEW_RAW) {
		KconRd b;

		kcon_rd_init(&b, payload, len);
		raw_on = kcon_get_u8(&b) ? 1 : 0;
		if (!b.err)
			raw_drain(0);
		return got;
	}

	if (op == KCON_OP_BLANK) {
		KconRd b;

		kcon_rd_init(&b, payload, len);

		int on = (int)kcon_get_u16(&b);

		/*
		 * THE SESSION DECIDES, THE DISPLAY ACTS. A view that
		 * cannot power its screen down ignores this and stays
		 * lit — a screensaver that saves no power, rather than
		 * a session that fails because its display is a
		 * terminal.
		 */
#ifdef KDOS_VIEW_KMS
		kkms_blank(on);
#else
		(void)on;
#endif
		return got;
	}

	/*
	 * A FONT STEP, and only a view with a screen of its own is
	 * sent one — the session checks KCON_VIEW_FONT before it
	 * asks, so there is nothing to refuse here.
	 *
	 * The grid goes back as an ordinary KCON_OP_VIEW_SIZE: a cell
	 * of a different size is a different number of columns, which
	 * is the same event as a screen being resized and is already
	 * the one the session knows how to handle.
	 */
	if (op == KCON_OP_VIEW_FONT) {
#ifdef KDOS_VIEW_KMS
		KconRd b;

		kcon_rd_init(&b, payload, len);

		int step = (int)(int16_t)kcon_get_u16(&b);
		char want[192];

		if (b.err || !own_screen)
			return got;
		if (step == 0)
			snprintf(want, sizeof(want), "%s", font_base);
		else if (!view_font_stepped(kkms_font(), step, want,
					    sizeof(want)))
			return got;
		font_apply(conn, want, step == 0 ? FONT_RESET : FONT_KEEP);
#endif
		return got;
	}

	if (op == KCON_OP_VIEW_FONTS) {
#ifdef KDOS_VIEW_KMS
		/*
		 * WHAT THIS DISPLAY CAN RENDER, gathered HERE because this is
		 * the end with the font stack — and the end that may be on
		 * another machine, whose fonts are its own.
		 *
		 * The list is rebuilt on every ask rather than cached: a font
		 * installed while the session ran is a font the next opening
		 * of the picker should offer, and the ask happens when a
		 * person opens a window, not per frame.
		 */
		if (!own_screen)
			return got;

		int n = view_font_list(kkms_font(), font_names,
				       VIEW_FONT_MAX);
		int cur = -1;

		for (int i = 0; i < n; i++)
			if (!strcmp(font_names[i], kkms_font()))
				cur = i;

		KconBuf out = { 0 };

		kcon_put_u16(&out, (uint16_t)n);
		kcon_put_u16(&out, (uint16_t)(int16_t)cur);
		for (int i = 0; i < n; i++)
			kcon_put_str(&out, font_names[i]);
		kcon_send(conn, KCON_OP_VIEW_FONTS, &out);
		kcon_flush(conn);
		kcon_buf_free(&out);
		font_nnames = n;
#endif
		return got;
	}

	if (op == KCON_OP_VIEW_OUTPUTS) {
#ifdef KDOS_VIEW_KMS
		/*
		 * THE SCREENS THIS VIEW IS DRIVING, gathered where the modes
		 * are. A session composes one grid and has no idea there are
		 * screens under it.
		 */
		if (!own_screen)
			return got;

		KconBuf out = { 0 };
		int n = kkms_outputs();

		if (n > KCON_MAX_OUTS)
			n = KCON_MAX_OUTS;
		kcon_put_u16(&out, (uint16_t)n);
		for (int i = 0; i < n; i++) {
			KkmsOutput o;

			if (!kkms_output(i, &o))
				break;

			int nm = kkms_modes(i);

			if (nm > KCON_MAX_MODES)
				nm = KCON_MAX_MODES;
			kcon_put_str(&out, o.name);
			kcon_put_u16(&out, (uint16_t)o.col);
			kcon_put_u16(&out, (uint16_t)o.cols);
			kcon_put_u16(&out, (uint16_t)o.width);
			kcon_put_u16(&out, (uint16_t)o.height);
			kcon_put_u16(&out,
				     (uint16_t)(int16_t)kkms_mode_current(i));
			kcon_put_u16(&out, (uint16_t)nm);
			for (int m = 0; m < nm; m++) {
				KkmsMode md;

				if (!kkms_mode(i, m, &md))
					break;
				kcon_put_u16(&out, (uint16_t)md.width);
				kcon_put_u16(&out, (uint16_t)md.height);
				kcon_put_u32(&out, (uint32_t)md.refresh);
			}
		}
		kcon_send(conn, KCON_OP_VIEW_OUTPUTS, &out);
		kcon_flush(conn);
		kcon_buf_free(&out);
#endif
		return got;
	}

	if (op == KCON_OP_VIEW_SETMODE) {
#ifdef KDOS_VIEW_KMS
		KconRd b;

		kcon_rd_init(&b, payload, len);

		int oi = (int)(int16_t)kcon_get_u16(&b);
		int mi = (int)(int16_t)kcon_get_u16(&b);

		(void)kcon_get_u8(&b);	/* `keep` is the picker's countdown;
					 * a mode is not persisted anywhere on
					 * this desktop, so nothing here reads
					 * it — see known-gaps. */
		if (b.err || !own_screen)
			return got;
		if (kkms_set_mode(oi, mi) != 0)
			return got;

		/* THE GRID IS DERIVED, so the announcement is the same one a
		 * font step and a hotplug make. */
		ktui_draw_resize();
		ktui_sprite_clear();
		ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
				   kcell_w(), kcell_h());
		ktui_draw_invalidate();

		cap_cell_w = kcell_w();
		cap_cell_h = kcell_h();

		KconBuf sz = { 0 };

		kcon_put_u16(&sz, (uint16_t)ktui_w);
		kcon_put_u16(&sz, (uint16_t)ktui_h);
		kcon_put_u16(&sz, (uint16_t)cap_cell_w);
		kcon_put_u16(&sz, (uint16_t)cap_cell_h);
		kcon_send(conn, KCON_OP_VIEW_SIZE, &sz);
		kcon_flush(conn);
		kcon_buf_free(&sz);
#endif
		return got;
	}

	if (op == KCON_OP_VIEW_SETFONT) {
#ifdef KDOS_VIEW_KMS
		KconRd b;

		kcon_rd_init(&b, payload, len);

		unsigned idx = kcon_get_u16(&b);
		int keep = (int)kcon_get_u8(&b);

		if (b.err || !own_screen)
			return got;
		/*
		 * 0xffff IS THE ONE IT STARTED WITH, which is what leaving the
		 * picker asks for. Any other index past the list this view
		 * sent is refused HERE: the session counts rows it was given
		 * and cannot know that a rebuild shortened them.
		 */
		if (idx == 0xffffu)
			font_apply(conn, font_base, FONT_RESET);
		else if ((int)idx < font_nnames)
			font_apply(conn, font_names[idx],
				   keep ? FONT_KEEP : FONT_PREVIEW);
#endif
		return got;
	}


	if (op == KCON_OP_SPRITE) {
		take_sprite(payload, len);
		return got;
	}

	if (op == KCON_OP_SPRITE_DROP) {
		drop_sprite(payload, len);
		return got;
	}

	/*
	 * THE LITERALS OF THE RUN THAT CAME BEFORE. The shadow already
	 * holds those cells, so this patches them there and redraws
	 * exactly them — which is why the session may send it as a
	 * second message without a frame ever being shown half
	 * coloured.
	 */
	if (op == KCON_OP_COLOR) {
		KconRd cr;
		KtuiCell patch[4096];
		uint16_t cx, cy;

		kcon_rd_init(&cr, payload, len);
		while (cr.pos < cr.len && !cr.err) {
			int n = kcon_get_color_run(&cr, &cx, &cy,
						   patch, 4096);

			if (n < 0)
				break;
			shadow_fit(ktui_w, ktui_h);
			for (int i = 0; i < n; i++) {
				int px = (int)cx + i, py = (int)cy;

				if (!shadow || px >= shadow_w ||
				    py >= shadow_h)
					return got;

				KtuiCell *sc =
					&shadow[py * shadow_w + px];

				/* The low byte is the commit's and
				 * stays the commit's; this message
				 * owns the bits above it and the
				 * three colours they describe. */
				sc->attr = (uint16_t)
					((sc->attr & 0xffu) |
					 (patch[i].attr & ~0xffu));
				sc->fgc = patch[i].fgc;
				sc->bgc = patch[i].bgc;
				sc->ulc = patch[i].ulc;
				draw_one(px, py, sc);
			}
			got = 1;
		}
		return got;
	}

	if (op != KCON_OP_COMMIT)
		return got;

	KconRd rd;
	KtuiCell run[4096];
	uint16_t x, y;

	kcon_rd_init(&rd, payload, len);
	while (rd.pos < rd.len && !rd.err) {
		int n = kcon_get_run(&rd, &x, &y, run, 4096);

		if (n < 0)
			break;
		shadow_fit(ktui_w, ktui_h);
		for (int i = 0; i < n; i++) {
			int cx = (int)x + i, cy = (int)y;

			if (shadow && cx < shadow_w && cy < shadow_h) {
				shadow[cy * shadow_w + cx] = run[i];
				slot_box_note(cx, cy, &run[i]);
			}
			draw_one(cx, cy, &run[i]);
		}
		got = 1;
	}
	return got;
}

/*
 * A RECORDED MESSAGE, DRAWN. The same handler the socket path uses, which is
 * the whole point of splitting it out: a replay that had its own drawing code
 * would drift from the desktop it claims to be replaying.
 */
static int on_replay(unsigned op, const char *payload, size_t len, void *user)
{
	(void)user;
	if (handle_msg(op, (const unsigned char *)payload, len) > 0)
		ktui_draw_flush();
	return 0;
}

static int take_frame(int timeout_ms)
{
	KconMsg m;
	int got = 0;
	int r;

	while ((r = kcon_recv(conn, &m)) == 1) {
		int h = handle_msg(m.op, m.payload, m.len);

		/*
		 * RECORDED BEFORE IT IS ACTED ON, and recorded whatever it
		 * was: a recording that only kept the ops this build knows
		 * would lose an op added later, and the version in its header
		 * is what says which protocol these bytes are.
		 */
		krec_msg(m.op, (const char *)m.payload, m.len);
		if (h < 0)
			return -1;
		if (h > 0)
			got = 1;
	}

	if (r < 0)
		return -1;

	if (!got && timeout_ms) {
		struct pollfd p = { .fd = kcon_conn_fd(conn),
				    .events = POLLIN };

		poll(&p, 1, timeout_ms);
	}

	return got;
}

static void send_key(const KtuiEvent *ev)
{
	KconBuf b = { 0 };

	/* AN OBSERVER SENDS NOTHING, here as well as at the server. Two
	 * places, because the local one is what makes the view honest and the
	 * remote one is what makes it safe. */
	if (observe)
		return;

	kcon_put_i32(&b, ev->key);
	kcon_put_u8(&b, (uint8_t)ev->mods);
	kcon_send(conn, KCON_OP_KEY, &b);
	kcon_buf_free(&b);
}

/*
 * TEXT THE HOST TERMINAL HANDED THIS VIEW. It is forwarded rather than typed:
 * a paste turned back into keystrokes is a pasted line that runs its own first
 * word, and a view decides nothing about where text goes in any case.
 */
static void send_paste(void)
{
	const char *text = NULL;
	size_t n = ktui_paste_take(&text);

	if (!n || !text)
		return;

	KconBuf b = { 0 };
	char *z = malloc(n + 1);

	if (!z)
		return;
	memcpy(z, text, n);
	z[n] = 0;
	kcon_put_str(&b, z);
	kcon_send(conn, KCON_OP_PASTE, &b);
	kcon_buf_free(&b);
	free(z);
}

static void send_ptr(const KtuiEvent *ev)
{
	if (observe)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, ev->mx);
	kcon_put_i32(&b, ev->my);
	kcon_put_u8(&b, (uint8_t)ev->btn);
	kcon_put_u8(&b, (uint8_t)ev->press);
	/* Biased into a byte: the backend's zero is the centre of the cell, and
	 * the centre is also what a session reading a message without these
	 * takes. */
	kcon_put_u8(&b, (uint8_t)(ev->subx + 128));
	kcon_put_u8(&b, (uint8_t)(ev->suby + 128));
	kcon_send(conn, KCON_OP_PTR, &b);
	kcon_buf_free(&b);
}

#ifdef KDOS_VIEW_KMS
/*
 * A FINGER, AND WHAT THIS VIEW'S RECOGNISER MADE OF IT.
 *
 * THE GESTURE IS DECIDED HERE AND NOT AT THE SESSION. There is one recogniser
 * and it lives where the touch device is; a second one at the far end would be
 * fed a message rather than a device and would disagree the first time a link
 * was slow. The session is told the verdict.
 *
 * The mouse event the recogniser synthesises beside it goes as an ordinary
 * KCON_OP_PTR, so every surface that has never heard of touch still gets a
 * click.
 */
static void send_touch(const KtuiEvent *ev)
{
	if (observe)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, ev->mx);
	kcon_put_i32(&b, ev->my);
	kcon_put_u8(&b, (uint8_t)ev->slot);
	kcon_put_u8(&b, (uint8_t)ev->phase);
	kcon_put_u32(&b, ev->ms);
	kcon_put_u8(&b, (uint8_t)ev->gesture);
	kcon_send(conn, KCON_OP_TOUCH, &b);
	kcon_buf_free(&b);
}
#endif	/* a view with no screen has no touch device to hear from */

/*
 * SIGHUP is the live retint, the same signal `kdos theme` sends to every
 * long-lived surface. A view holds no window state, but it does hold the
 * palette its backend paints with — a KMS view rasterises glyphs itself — so
 * it has to be told, and a flag rather than the work is what keeps allocation
 * out of a signal handler.
 *
 * The default disposition for SIGHUP is DEATH: a program on
 * reload_session()'s list that does not handle it is one `kdos theme amber`
 * kills, after which the supervisor restarts it and it looks retinted.
 *
 * SET AT STARTUP, so the first turn of whichever loop this view runs applies
 * the accent and the night-light toggle. There is no second place that reads
 * them: a view that only ever retinted on the signal painted in the table's
 * first scheme until somebody ran `kdos theme` again.
 */
static volatile sig_atomic_t g_retint = 1;

static void on_hup(int sig)
{
	(void)sig;
	g_retint = 1;
}

static void retint(void)
{
	char name[64];

	if (kcol_theme_name(name, sizeof(name)) && *name)
		ktui_theme_set(name);
	/* AFTER the scheme, because it transforms whatever the scheme just
	 * became — and unconditionally, because turning the toggle off is a
	 * retint too. */
	ktui_theme_night(kb_toggle_on("night-light"));
	ktui_term_repalette();
	ktui_draw_invalidate();
}

int main(int argc, char **argv)
{
	const char *sock = getenv("KDOS_CON");
	const char *font = getenv("KDOS_CON_FONT");
	int cols = 0, rows = 0, tty = 0, kms = 0, dump = 0, cast = 0;
	int kms_only = 0;
	const char *card = NULL;
	/*
	 * HOW THE SCREEN IS DRIVEN — see KkmsTune, which these three become in
	 * the KMS block below. Zero is every default: three buffers, the
	 * monitor's preferred mode and presentation locked to the vblank. Each
	 * is an opt-in that trades something a person can see for something
	 * else a person can see, so none of them is a default this program
	 * picks on their behalf.
	 *
	 * READ IN EVERY BUILD, acted on only in the one that takes a screen —
	 * the rule `--font` already keeps, so a script that passes them need
	 * not know which build it is talking to.
	 */
	int want_bufs = 0, want_fastest = 0, want_tearing = 0;
#ifdef KDOS_VIEW_PIXELS
	/* Only a build that can hold pixels can have a terminal sink. */
	int tty_pix = 0;
#endif
	const char *shot = NULL;
	const char *record = NULL, *replay = NULL;
	int crop[4] = { 0, 0, 0, 0 };

	signal(SIGHUP, on_hup);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage(stdout);
			return 0;
		}
		if (!strcmp(argv[i], "--socket") && i + 1 < argc) {
			sock = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--tty")) {
			tty = 1;
			continue;
		}
		if (!strcmp(argv[i], "--card") && i + 1 < argc) {
			/* WHICH DRM DEVICE, for a machine with more than one.
			 * Without it the sweep takes the first card with a
			 * connected output, which on a machine with an
			 * emulated display beside a virtual one is the
			 * emulated one every time — and that is what made a
			 * second screen impossible to test. */
			card = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--buffers") && i + 1 < argc) {
			/* A CEILING AND NOT A PROMISE: a driver with no memory
			 * for the third gives two and the desktop comes up
			 * either way. Out of range is the default, because a
			 * typo here must not be a machine with no display. */
			want_bufs = atoi(argv[++i]);
			continue;
		}
		if (!strcmp(argv[i], "--fastest-mode")) {
			/* THE HIGHEST REFRESH AT THE SIZE THE MONITOR CHOSE,
			 * never a different size: a scaled desktop is a blur
			 * nobody asked for. */
			want_fastest = 1;
			continue;
		}
		if (!strcmp(argv[i], "--tearing")) {
			/* PRESENT WITHOUT WAITING FOR THE VBLANK. It costs up
			 * to a refresh period of latency and it TEARS: a
			 * moving edge is cut across the screen, because the
			 * raster is inside the buffer when the CRTC is
			 * pointed at the next one. */
			want_tearing = 1;
			continue;
		}
		if (!strcmp(argv[i], "--kms")) {
			kms = 1;
			continue;
		}
		if (!strcmp(argv[i], "--kms-only")) {
			kms = 1;
			kms_only = 1;
			continue;
		}
		if (!strcmp(argv[i], "--cast")) {
			cast = 1;
			continue;
		}
		if (!strcmp(argv[i], "--observe")) {
			observe = 1;
			continue;
		}
		if (!strcmp(argv[i], "--record") && i + 1 < argc) {
			record = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--replay") && i + 1 < argc) {
			replay = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
			/*
			 * A PICTURE OF THE SAME FRAME `--dump` PRINTS. It
			 * settles the same way and takes the same one; the
			 * only difference is that it rasterises rather than
			 * writing codepoints, so a person can look at it.
			 */
			shot = argv[++i];
			dump = 1;
			continue;
		}
		if (!strcmp(argv[i], "--crop") && i + 1 < argc) {
			/*
			 * CELLS, because the session that asks for a crop has
			 * no other unit; the view turns them into pixels
			 * because it is the end that knows the font.
			 */
			if (sscanf(argv[++i], "%d,%d,%d,%d", &crop[0],
				   &crop[1], &crop[2], &crop[3]) != 4) {
				fprintf(stderr,
					"kdos-view: --crop wants X,Y,W,H in cells\n");
				return 2;
			}
			continue;
		}
		if (!strcmp(argv[i], "--dump")) {
			dump = 1;
			/*
			 * A SIZE IS OPTIONAL, and leaving it out is the right
			 * answer when the session already has one: the view
			 * asks for nothing, is told the grid, and the desktop
			 * is not resized by having its picture taken.
			 */
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				if (sscanf(argv[++i], "%dx%d", &cols,
					   &rows) != 2) {
					fprintf(stderr,
						"kdos-view: --dump wants COLSxROWS\n");
					return 2;
				}
			}
			continue;
		}
		fprintf(stderr, "kdos-view: unknown option '%s'\n", argv[i]);
		usage(stderr);
		return 2;
	}

	/* A REPLAY NEEDS NO SESSION, so it is not asked for one: the messages
	 * come from a file and connecting would put a second view on somebody
	 * else's desktop. */
	if (!sock && !replay) {
		fprintf(stderr,
			"kdos-view: no session. Set $KDOS_CON or pass --socket.\n");
		return 2;
	}

	/* The font and the three screen-driving flags are the KMS mode's; a
	 * build without it still accepts them so a script need not know which
	 * build it is talking to. */
	(void)font;
	(void)want_bufs;
	(void)want_fastest;
	(void)want_tearing;

	/* `--record` is not a mode: it rides whichever one is drawing, because
	 * a recording is what a view was sent and the view still has to be
	 * looking at something. `--replay` IS one — it draws a file. */
	if (!tty && !kms && !dump && !cast && !replay) {
		fprintf(stderr,
			"kdos-view: choose --kms, --tty, --shot, --dump or "
			"--cast\n");
		return 2;
	}
	/*
	 * ONE MODE. The blocks below are independent `if`s that each load a
	 * font and set cap_cell_w/cap_flags, so a pair given together has the
	 * second silently overwrite the first's setup and the first's output
	 * never arrives.
	 */
	if ((shot != NULL) + (cast != 0) + (kms != 0) > 1) {
		fprintf(stderr, "kdos-view: %s%s%sare more than one mode; "
				"pick one\n",
			shot ? "--shot " : "", cast ? "--cast " : "",
			kms ? "--kms " : "");
		return 2;
	}

	/*
	 * A SHOT HAS PIXELS AND A DUMP DOES NOT, which is the whole difference
	 * between them on the wire: the session sends a view with pixels the
	 * pictures a program drew, and a view that claimed none would take a
	 * photograph with the pictures missing. The font is loaded for the same
	 * reason the cast path loads one — there is nothing to rasterise with
	 * otherwise.
	 */
	if (shot) {
#ifndef KDOS_VIEW_SHOT
		fprintf(stderr, "kdos-view: this build has no shot mode "
				"(built without the cell rasteriser)\n");
		return 1;
#else
		if (kcell_font_load(font) != 0) {
			fprintf(stderr,
				"kdos-view: no font to rasterise a shot with\n");
			return 1;
		}
		ktui_sprite_evictor(sprite_free, NULL);
		ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
				   kcell_w(), kcell_h());
		cap_cell_w = kcell_w();
		cap_cell_h = kcell_h();
		cap_flags = KCON_VIEW_PIXELS | KCON_VIEW_COLOR;
#endif
	}

	if (cast) {
#if defined(KDOS_VIEW_CAST) && defined(KDOS_VIEW_KMS)
		/*
		 * THE FONT IS LOADED HERE and nowhere else on this path: a cast
		 * rasterises through the same cell painter a screen does, so it
		 * needs the same glyph cache. Without a font there is nothing
		 * to record.
		 */
		if (kcell_font_load(font) != 0) {
			fprintf(stderr, "kdos-view: no font to rasterise with\n");
			return 1;
		}
		ktui_sprite_evictor(sprite_free, NULL);
		ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
				   kcell_w(), kcell_h());
		cap_cell_w = kcell_w();
		cap_cell_h = kcell_h();
		cap_flags = KCON_VIEW_PIXELS | KCON_VIEW_COLOR;
#else
		fprintf(stderr, "kdos-view: this build has no cast mode "
				"(built without PipeWire)\n");
		return 1;
#endif
	}

	if (kms) {
#ifdef KDOS_VIEW_KMS
		/*
		 * THE STEPPED FONT IS AN OVERRIDE AND ONLY THAT. It is read
		 * here, after the flag and the environment have had their
		 * say, so `kdos-view --font` and `$KDOS_CON_FONT` still name
		 * the font — a state file that beat them would be a chord
		 * that quietly disabled the configuration.
		 *
		 * `font_base` is what a reset returns to, which is why it is
		 * taken before the file is read.
		 */
		char fs_path[512];
		char *fs_name = NULL;
		size_t fs_len = 0;

		snprintf(font_base, sizeof(font_base), "%s", font ? font : "");
		if (!font && view_font_state_path(fs_path, sizeof(fs_path)) &&
		    (fs_name = kb_read_whole(fs_path, &fs_len)) != NULL) {
			/* Held for the life of the process: `font` points into
			 * it and the screen is opened from it. */
			fs_name[strcspn(fs_name, "\r\n")] = '\0';
			if (fs_name[0])
				font = fs_name;
		}

		KkmsTune tune = {
			.buffers = want_bufs,
			.mode = want_fastest ? KKMS_MODE_FASTEST
					     : KKMS_MODE_PREFERRED,
			.tearing = want_tearing,
		};

		if (kkms_init(NULL, card, font, &tune) == 0) {
			ktui_draw_init();

			/*
			 * PICTURES ARE EVICTABLE HERE and nowhere else on this
			 * path: this is the only build that turns a blob into
			 * real pixels, so it is the only one holding memory
			 * worth capping. The cap is taken from the grid the
			 * backend just sized, so it has to be set after
			 * ktui_draw_init() and again wherever the grid moves.
			 */
			ktui_sprite_evictor(sprite_free, NULL);
			ktui_sprite_budget(view_sprite_budget(kcell_w(),
							      kcell_h()),
					   kcell_w(), kcell_h());
			cap_cell_w = kcell_w();
			cap_cell_h = kcell_h();
			/* THIS VIEW RASTERISES ITS OWN GLYPHS, so it is the
			 * one kind that can be asked to change their size. */
			cap_flags = KCON_VIEW_PIXELS | KCON_VIEW_FONT |
				    KCON_VIEW_COLOR | KCON_VIEW_FRAME;
			cols = ktui_w;
			rows = ktui_h;
			own_screen = 1;
		} else if (kms_only) {
			/*
			 * A SUPERVISED VIEW MUST NOT FALL BACK. It has no
			 * terminal to fall back into — its stdout is a log
			 * file — so the fallback draws a desktop nobody can
			 * see and then exits 0 in milliseconds. A clean exit
			 * reads as a detach, so the supervisor's crash cap
			 * never fires and the screen keeps whatever the
			 * framebuffer console last drew, with every check
			 * reporting success.
			 */
			fprintf(stderr, "kdos-view: no screen to take — %s\n",
				kkms_reason());
			return 1;
		} else {
			/*
			 * NO DRM DEVICE, OR THE SEAT REFUSED. Falling back is
			 * the whole point of this design: a desktop that will
			 * not start on a machine whose GPU driver is broken is
			 * the case the console exists for.
			 */
			fprintf(stderr,
				"kdos-view: no screen to take (%s) — falling back to this terminal\n",
				kkms_reason());
			kms = 0;
			tty = 1;
			/* WHICH MODE WAS CHOSEN, said on both paths. The KMS
			 * path names the outputs it took; without this line a
			 * probe that fell back said only what it could NOT
			 * do, and a person reading a log could not tell a
			 * terminal view from a view that never started. */
			fprintf(stderr,
				"kdos-view: mode terminal (this terminal's own cells)\n");
		}
#else
		(void)kms_only;
		(void)card;
		fprintf(stderr,
			"kdos-view: this build has no KMS mode (built without libkkms)\n");
		return 1;
#endif
	}

	if (cast) {
		/*
		 * ZERO ROWS AND COLUMNS ON PURPOSE: the grid is taken from the
		 * session below, so starting a recording does not resize the
		 * desktop being recorded.
		 */
		cols = rows = 0;
	} else if (tty) {
		if (ktui_term_init(1) != 0) {
			fprintf(stderr, "kdos-view: no terminal\n");
			return 1;
		}
		ktui_draw_init();
		/*
		 * ASKED FOR ONLY WHERE THE TERMINAL CAN SHOW IT, and asked
		 * after ktui_draw_init because `ktui_caps` is the backend's
		 * answer and is not set until then. A view on a sixteen-colour
		 * terminal that took the literals would pay for them on the
		 * link and then reduce every one of them back to the slot it
		 * was already sent.
		 */
		if (ktui_caps & KT_CAP_TRUECOLOR)
			cap_flags |= KCON_VIEW_COLOR;
		/* A display, so its paints pace the session's frames. */
		cap_flags |= KCON_VIEW_FRAME;
#ifdef KDOS_VIEW_TTYPIX
		/*
		 * ASKED AFTER ktui_draw_init AND BEFORE THE FIRST FRAME.
		 * After, because `ktui_caps` is the backend's answer and is
		 * not set until then — a probe that ran first would not know
		 * it was on a Linux console. Before, because the answer
		 * decides whether this view keeps a picture's pixels at all,
		 * and because the reply has to be read while nothing else is
		 * reading the terminal.
		 *
		 * THE THROTTLE IS NOT LIFTED. A view that declared
		 * KCON_VIEW_PIXELS would have the session stop rate-limiting
		 * every embedded guest for every view, and this one writes to
		 * a terminal that can block: while it is blocked it is not
		 * reading its session socket, and a session drops a peer whose
		 * queue overflows. The emitter rate-limits itself instead,
		 * which is the end that knows what the link is.
		 */
		if (view_ttypix_probe(&pix_cw, &pix_ch) > 0) {
			tty_pix = 1;
			ktui_backend_set(view_ttypix_install(ktui_backend()));
			ktui_sprite_evictor(sprite_free, NULL);
			ktui_sprite_budget(view_sprite_budget(pix_cw, pix_ch),
					   pix_cw, pix_ch);
		}
#endif
		cols = ktui_w;
		rows = ktui_h;
	} else if (kms) {
		/*
		 * ALREADY INITIALISED, AND IT MUST NOT BE DONE AGAIN HERE.
		 * kkms_init() installed the backend and ktui_draw_init() sized
		 * the grid from the screen it took, which is what set `cols`
		 * — so a chain that ends in `cols > 0` catches --kms and sends
		 * it offscreen. That is a ONE-WAY LATCH: every later
		 * ktui_draw_flush() returns without drawing, the desktop is
		 * painted into a buffer nothing presents, and the screen stays
		 * the colour the modeset left it with every check reporting
		 * success.
		 */
	} else if (cols > 0) {
		if (ktui_offscreen_init(cols, rows) != 0) {
			fprintf(stderr, "kdos-view: cannot render offscreen\n");
			return 1;
		}
		ktui_draw_init();
		view_shot_budget(shot);
	}

#ifdef KDOS_VIEW_PIXELS
	view_slot_init();

	/*
	 * WHICH SINK THIS VIEW HAS. A dump draws glyphs and nothing else, so a
	 * sprite that arrives for it is matched to a shape; everything else
	 * keeps the pixels, and a terminal keeps them only once its host has
	 * said it will draw them.
	 */
	sink = (kms || cast || shot) ? SINK_RASTER
	     : tty_pix		     ? SINK_TTYPIX
				     : SINK_ASCII;
#endif

	/*
	 * ZERO IS "I IMPOSE NOTHING". The session ignores a size that is not
	 * positive and keeps the grid its primary view decided, which is what
	 * makes a screenshot a screenshot rather than a resize.
	 */
	/*
	 * A REPLAY ATTACHES TO NOTHING. Its messages come from a file, so it
	 * neither needs a session nor may take one: a player that connected
	 * would be a second view on somebody's desktop, resizing it to
	 * whatever the recording was made at.
	 */
	if (replay) {
		int rc, rw = 0, rh = 0, rcw = 0, rch = 0;

		if (krec_replay(replay, &rw, &rh, &rcw, &rch, NULL, NULL) != 0)
			return 1;
		if (rw > 0 && rh > 0 && !tty && !kms) {
			if (ktui_offscreen_init(rw, rh) != 0) {
				fprintf(stderr,
					"kdos-view: cannot render offscreen\n");
				return 1;
			}
			ktui_draw_init();
		}
		rc = krec_replay(replay, NULL, NULL, NULL, NULL, on_replay,
				 NULL);
		if (dump)
			ktui_draw_dump();
		if (tty)
			ktui_term_shutdown();
		return rc == 0 ? 0 : 1;
	}

	if (attach(sock, cols, rows) != 0) {
		fprintf(stderr, "kdos-view: cannot attach to %s\n", sock);
		return 1;
	}


	if (!tty && !kms && !cast && cols <= 0) {
		if (wait_for_grid(&cols, &rows) != 0) {
			fprintf(stderr,
				"kdos-view: the session never said how big it is\n");
			return 1;
		}
		if (ktui_offscreen_init(cols, rows) != 0) {
			fprintf(stderr, "kdos-view: cannot render offscreen\n");
			return 1;
		}
		ktui_draw_init();
		view_shot_budget(shot);
	}

#if defined(KDOS_VIEW_CAST) && defined(KDOS_VIEW_KMS)
	if (cast) {
		/*
		 * The session's grid, then a stream the size that grid
		 * rasterises to. The stream cannot be created before the size
		 * is known — a consumer negotiates against it — which is why
		 * this waits rather than guessing.
		 */
		if (wait_for_grid(&cast_cols, &cast_rows) != 0) {
			fprintf(stderr,
				"kdos-view: the session never said how big it is\n");
			return 1;
		}

		cast_pw = cast_cols * kcell_w();
		cast_ph = cast_rows * kcell_h();

		uint32_t *bits = calloc((size_t)cast_pw * (size_t)cast_ph, 4);

		cast_img = bits ? pixman_image_create_bits(PIXMAN_a8r8g8b8,
							   cast_pw, cast_ph,
							   bits, cast_pw * 4)
				: NULL;
		if (!cast_img) {
			free(bits);
			fprintf(stderr, "kdos-view: cannot allocate a frame\n");
			return 1;
		}
		cast_bits = bits;

		/*
		 * A BACKEND, not a buffer read from the side. The cast
		 * rasterises in the toolkit's own present step, so it gets the
		 * row diff for free and there is one answer to what a frame
		 * of this desktop looks like.
		 */
		ktui_backend_set(&cast_backend);
		if (ktui_draw_init() != 0) {
			fprintf(stderr, "kdos-view: cannot start the cast\n");
			return 1;
		}

		/* THE BUDGET IS MEASURED FROM ktui_w BY ktui_h, which
		 * ktui_draw_init() sets and nothing before it does: a cap
		 * computed earlier on this path is the floor, and a cast of a
		 * large session would refuse blocks of the biggest window in
		 * the recording. */
		ktui_sprite_budget(view_sprite_budget(kcell_w(), kcell_h()),
				   kcell_w(), kcell_h());

		if (kcast_init(cast_pw, cast_ph, KDOS_VIEW_CAST_FPS) != 0)
			return 1;

		/*
		 * THE NODE, AND THE SIZE, on one line. Whatever started this
		 * needs both — a portal has to tell an application how big the
		 * stream is and cannot know it, because it never rasterises.
		 */
		printf("%u %d %d\n", kcast_node_id(), cast_pw, cast_ph);
		fflush(stdout);

		/*
		 * DAMAGE DRIVES THE FRAMES. A still desktop sends no cells, so
		 * nothing is rasterised and the stream's cycles carry an empty
		 * chunk — which is the rule the rig already lives under and is
		 * what makes recording an idle console cost nothing.
		 */
		for (;;) {
			struct pollfd p[2];
			int n = 0;

			if (g_retint) {
				g_retint = 0;
				retint();
			}

			p[n].fd = kcon_conn_fd(conn);
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
			if (kcast_fd() >= 0) {
				p[n].fd = kcast_fd();
				p[n].events = POLLIN;
				p[n].revents = 0;
				n++;
			}
			poll(p, (nfds_t)n, 1000 / KDOS_VIEW_CAST_FPS);
			kcast_pump();

			int r = take_frame(0);

			if (r < 0)
				break;

			/* A consumer that has just connected has seen nothing,
			 * and an idle desktop will not produce a frame for it
			 * on its own. */
			if (kcast_hungry()) {
				ktui_draw_invalidate();
				r = 1;
			}
			if (r)
				ktui_draw_flush();
			sprite_lost_flush();

			if (kcon_conn_dead(conn))
				break;
		}

		kcast_finish();
		pixman_image_unref(cast_img);
		kcon_conn_free(conn);
		return 0;
	}
#endif

	/*
	 * RECORDING STARTS ONCE A MODE IS UP AND THE GRID IS KNOWN, because
	 * the header names the grid: a player sizes a screen from it before
	 * the first message is drawn on one. Started here rather than at the
	 * attach for that reason — at the attach the size is still whatever
	 * the defaults are.
	 */
	if (record) {
		if (krec_open(record, ktui_w, ktui_h, cap_cell_w,
			      cap_cell_h) != 0)
			return 1;
		/*
		 * CLOSED ON EVERY WAY OUT, and there are four: the cast
		 * branch, the dump branch, the loop and an error. A zstd
		 * stream is not a file with the data already in it — the tail
		 * is what makes it readable — so a mode that returned without
		 * closing wrote a recording of nothing at all. That is what
		 * `--dump --record` did.
		 */
		atexit(krec_close);
	}

	/*
	 * DUMP, AND ONLY DUMP. The four modes are exclusive and every one of
	 * the other three has its own loop below, so this has to name its own
	 * rather than test for the absence of one of them: `--kms` is also
	 * not a tty, and a `!tty` test swallows it here — the screen is taken
	 * and the mode is set, one frame goes to stdout, and the view exits 0
	 * without ever reaching the loop that would keep drawing on it.
	 */
	if (dump) {
		/*
		 * SETTLE, THEN DUMP. A frame taken while the session is still
		 * sending is a different frame every time it is taken, so this
		 * reads until the session has been quiet for a beat.
		 */
		int quiet = 0;

		for (int spin = 0; spin < 400 && quiet < 12; spin++) {
			int r = take_frame(10);

			if (r < 0)
				break;
			quiet = r ? 0 : quiet + 1;
		}
#ifdef KDOS_VIEW_SHOT
		if (shot) {
			int rc = view_shot_png(shot, 1, crop[0],
					       crop[1], crop[2], crop[3]);

			kcon_conn_free(conn);
			return rc == 0 ? 0 : 1;
		}
#endif
		ktui_draw_dump();
		kcon_conn_free(conn);
		return 0;
	}

#ifdef KDOS_VIEW_KMS
	if (kms) {
		for (;;) {
			struct pollfd p[5];
			int n = 0;

			if (g_retint) {
				g_retint = 0;
				retint();
			}

			p[n].fd = kcon_conn_fd(conn);
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
			p[n].fd = kkms_seat_fd();
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
			p[n].fd = kkms_input_fd();
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
			/* A SCREEN PLUGGED IN IS A DESCRIPTOR LIKE ANY OTHER.
			 * -1 where there is no monitor, which poll ignores. */
			p[n].fd = kkms_hotplug_fd();
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
			/*
			 * AND THE VBLANK. A page flip completes on the DRM
			 * descriptor, so waiting on it is waiting for the
			 * screen: the next frame is painted when the last one
			 * is actually being shown, which is what makes an
			 * animation's rate the monitor's refresh rate rather
			 * than this loop's timeout.
			 */
			p[n].fd = kkms_drm_fd();
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;

			/* A bounded wait even with nothing readable: a seat
			 * event can arrive with no input, and a VT switch must
			 * not wait for a keypress that cannot happen while the
			 * session is inactive. */
			poll(p, (nfds_t)n, 20);
			kkms_pump();

			/*
			 * A SCREEN ARRIVED OR WENT, and the grid moved with
			 * it. Announced as an ordinary KCON_OP_VIEW_SIZE for
			 * the reason a font step is: a different number of
			 * cells is the same event as a screen being resized,
			 * and the session already knows how to handle one.
			 * The pictures go with it — every one was cut for a
			 * grid that has changed shape.
			 */
			if (kkms_hotplug_pump()) {
				ktui_draw_resize();
				ktui_sprite_clear();
				ktui_sprite_budget(
					view_sprite_budget(kcell_w(),
							   kcell_h()),
					kcell_w(), kcell_h());
				ktui_draw_invalidate();

				cap_cell_w = kcell_w();
				cap_cell_h = kcell_h();

				KconBuf sz = { 0 };

				kcon_put_u16(&sz, (uint16_t)ktui_w);
				kcon_put_u16(&sz, (uint16_t)ktui_h);
				kcon_put_u16(&sz, (uint16_t)cap_cell_w);
				kcon_put_u16(&sz, (uint16_t)cap_cell_h);
				kcon_send(conn, KCON_OP_VIEW_SIZE, &sz);
				kcon_flush(conn);
				kcon_buf_free(&sz);
			}

			if (take_frame(0) < 0)
				break;

			/*
			 * Nothing is drawn while switched away: the devices
			 * are gone and the framebuffer is somebody else's.
			 * Nothing is drawn while a flip is outstanding
			 * either — the buffer to paint is the one the screen
			 * is about to show.
			 */
			if (kkms_active() && kkms_ready())
				view_present();

			KtuiEvent ev;

			while (ktui_backend()->poll_event(&ev, 0)) {
				/*
				 * EVERY RAW EVENT ITS COOKED PARTNER HAS
				 * ALREADY GONE FOR, BEFORE THIS ONE DOES.
				 * The two queues are drained together rather
				 * than one after the other, so the session
				 * still holds the verdict for the key whose
				 * switch it is about to be handed.
				 */
				raw_drain_to(raw_on, 0);
				cooked_taken++;
				if (ev.type == KT_EVT_KEY) {
					/*
					 * Ctrl+Alt+F<n> NEVER ARRIVES HERE.
					 * xkb resolves it to a switch keysym
					 * and libkkms acts on it where the
					 * keysym is, because that is the only
					 * place it exists.
					 */
					send_key(&ev);
				} else if (ev.type == KT_EVT_MOUSE) {
					/*
					 * THE POINTER IS AN ARROW HERE, drawn
					 * in pixels by libkkms: it answers
					 * KtuiBackend.pointer and claims the
					 * job, so the flush leaves the cell
					 * alone. The same call on a view with
					 * no screen of its own reverses the
					 * cell instead, and this arm says
					 * nothing about which — the named cell
					 * is the whole of what a view decides.
					 */
					/* AN OBSERVER DRAWS NO POINTER. The
					 * cell is a promise that clicking
					 * there will do something, and for a
					 * view that may not click it is a
					 * lie. This is the one caller of the
					 * hide in the tree. */
					if (observe)
						ktui_draw_hide_cursor();
					else
						ktui_draw_cursor(ev.mx, ev.my);
					send_ptr(&ev);
				} else if (ev.type == KT_EVT_TOUCH) {
					/* THE POINTER CELL IS NOT DRAWN HERE.
					 * The recogniser synthesises a mouse
					 * event beside the touch and the arm
					 * above draws it; drawing from both
					 * would put the cursor at the finger
					 * and then at the pointer in one
					 * frame. */
					send_touch(&ev);
				}
			}

			/*
			 * AND WHAT IS LEFT OF THE RAW STREAM. The cooked queue
			 * is empty, so nothing still held is waiting for a
			 * message that is coming: an input that crossed no
			 * cell, or a button the cells have no name for, has no
			 * cooked partner at all and would otherwise sit here
			 * until one happened along.
			 */
			raw_drain(raw_on);

			if (kcon_conn_dead(conn))
				break;
		}

		kkms_shutdown();
		kcon_conn_free(conn);
		return said_bye ? 0 : 1;
	}
#endif

	for (;;) {
		if (g_retint) {
			g_retint = 0;
			retint();
		}

		if (take_frame(0) < 0)
			break;		/* the session went away */
		view_present();

		KtuiEvent ev;

		/* The terminal's own input path, which is what this backend
		 * reads — and it is forwarded rather than acted on, because a
		 * view decides nothing. */
		if (ktui_input_next(&ev, 20)) {
			if (ev.type == KT_EVT_KEY)
				send_key(&ev);
			else if (ev.type == KT_EVT_MOUSE) {
				/*
				 * THE POINTER AT THE RESOLUTION THIS VIEW HAS.
				 * A screen of its own gets an arrow drawn in
				 * pixels by libkkms; here there are only
				 * characters, so the cell under the pointer is
				 * reversed — which is the pointer every text
				 * mode has ever drawn, and the only one a
				 * terminal on the far end of ssh, a --dump or
				 * a braille display reading /dev/vcsa can
				 * show.
				 */
				/* An observer draws none, for the reason the
				 * screen path gives: a pointer that cannot
				 * click is a lie about what this view is. */
				if (observe) {
					ktui_draw_hide_cursor();
				} else {
					ktui_draw_cursor(ev.mx, ev.my);
				}
				send_ptr(&ev);
			}
			else if (ev.type == KT_EVT_RESIZE)
				break;	/* the grid is the session's to remake */
		}
		/* AND THE RAW STREAM, AFTER THE COOKED ONE. A terminal's
		 * backend has no device behind it and offers none of this, so
		 * here the call finds nothing — the order is kept in one place
		 * rather than in whichever loop happens to have a screen. */
		raw_drain(raw_on);

		/* A paste produces no event of its own — the backend queues
		 * it and whoever wants it takes it. */
		send_paste();

		if (kcon_conn_dead(conn))
			break;
		/*
		 * AND THE OTHER END OF THE LINK. The socket dying is a session
		 * that ended; the TERMINAL dying is an `ssh` connection that
		 * dropped, and the session is still there. Both leave through
		 * here, so both put the host back — a view that noticed only
		 * the socket would paint frames into a hung-up pty for as long
		 * as the session ran.
		 *
		 * NOT ON SIGHUP. `kdos theme` sends that signal to every view
		 * to retint it, so tearing down on it would make an accent
		 * change end every `--tty` view on the machine. The write
		 * failing is the fact; the signal is ambiguous.
		 */
		if (ktui_term_hungup()) {
			/* The host terminal IS this view's screen, so there is
			 * nothing to restart into: that ending is as clean as
			 * a goodbye. */
			said_bye = 1;
			break;
		}
	}

#ifdef KDOS_VIEW_TTYPIX
	if (tty_pix)
		view_ttypix_shutdown();
#endif
	ktui_term_shutdown();
	kcon_conn_free(conn);
	return said_bye ? 0 : 1;
}
