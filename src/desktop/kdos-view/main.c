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
#include <unistd.h>

#include <signal.h>

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
 * THE CELL RASTERISER IS SHARED BY TWO MODES. `--kms` puts cells on a screen
 * and `--shot` puts the same cells in a file, and both go through libkcell —
 * so the painter, the sprite table and the picture path are all behind this
 * one name, and only the KMS driver itself is behind the narrower guard.
 *
 * A BUILD WITHOUT IT DRAWS PICTURES AS CHARACTERS, which is what a terminal
 * view and a `--dump` do anyway. What it must not do is claim
 * `KCON_VIEW_PIXELS` and then have nothing to put the pixels in.
 */
#if defined(KDOS_VIEW_KMS) || defined(KDOS_VIEW_SHOT)
#define KDOS_VIEW_PIXELS 1
#endif

#ifdef KDOS_VIEW_PIXELS
#include <pixman.h>

#include "kcell.h"
#endif
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
"  --tty              draw in this terminal\n"
"  --shot FILE.png    take one frame and write it as a picture\n"
"  --dump [COLSxROWS] take one frame and write it as cells; without a\n"
"                     size it takes the session's own grid\n"
"  --cast             rasterise into a PipeWire stream instead of onto a\n"
"                     screen — a view nobody looks at. Prints its node id\n"
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
static unsigned cap_flags;

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

	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_VIEW);
	kcon_put_u16(&b, (uint16_t)cap_cell_w);
	kcon_put_u16(&b, (uint16_t)cap_cell_h);
	kcon_put_u16(&b, (uint16_t)cap_flags);
	kcon_send(conn, KCON_OP_HELLO, &b);

	/* THE VIEW DECIDES THE GRID. The session composites to whatever the
	 * first view says it can show. */
	kcon_buf_reset(&b);
	kcon_put_u16(&b, (uint16_t)cols);
	kcon_put_u16(&b, (uint16_t)rows);
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
	if (key < KCON_MAX_SPRITE_MAP)
		view_slot[key] = -1;
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
typedef struct {
	int cw, ch;		/* the picture's size in cells */
	uint32_t *cp;		/* cw*ch matched codepoints */
	uint8_t *fg;		/* cw*ch palette slots */
} AsciiPic;

static AsciiPic *ascii_pic[KCON_MAX_SPRITE_MAP];
/* Set when the view has no pixels of its own: --tty, and --dump. */
static int ascii_mode;

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
	if (!ascii_mode || !KTUI_IS_SPRITE(*ch))
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

static void take_sprite(const KconMsg *m)
{
	KconRd r;
	int slot, cw, ch, pw, ph;
	uint32_t fallback;
	const void *argb;

	kcon_rd_init(&r, m->payload, m->len);
	slot = (int)kcon_get_u16(&r);
	cw = (int)kcon_get_u16(&r);
	ch = (int)kcon_get_u16(&r);
	fallback = kcon_get_u32(&r);
	pw = (int)kcon_get_u16(&r);
	ph = (int)kcon_get_u16(&r);
	if (r.err || cw < 1 || ch < 1)
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

	if (ascii_mode) {
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
	int dw = cw * kcell_w();
	int dh = ch * kcell_h();

	if (dw <= 0 || dh <= 0)
		return;

	uint32_t *bits = calloc((size_t)dw * (size_t)dh, 4);

	if (!bits)
		return;

	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, dw, dh,
						       bits, dw * 4);

	if (!img) {
		free(bits);
		return;
	}
	pixman_image_set_destroy_function(img, free_bits, bits);

	pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, pw, ph,
						       (uint32_t *)argb,
						       pw * 4);

	if (!src) {
		pixman_image_unref(img);
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

	int vs = ktui_sprite_put((uint64_t)slot, img, cw, ch, fallback);

	if (vs < 0)
		pixman_image_unref(img);
	view_slot[slot] = vs;
	redraw_slot((unsigned)slot);
#else
	(void)slot;
	(void)fallback;
	(void)argb;
#endif
}

#ifdef KDOS_VIEW_PIXELS
/* The session's slot in the cell, rewritten to this view's. A slot with no
 * picture behind it becomes one the table has never heard of, and the backend
 * draws the fallback mark. */
static uint32_t present(uint32_t ch)
{
	if (!KTUI_IS_SPRITE(ch))
		return ch;

	unsigned slot = KTUI_SPRITE_SLOT(ch);
	int vs = slot < KCON_MAX_SPRITE_MAP ? view_slot[slot] : -1;

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
	uint32_t cp = c->ch;
	int fg = c->fg;

#ifdef KDOS_VIEW_PIXELS
	if (!ascii_cell(&cp, &fg))
		cp = present(cp);
#endif
	ktui_draw_cell(x, y, cp, fg, c->bg, c->attr);
}

#ifdef KDOS_VIEW_PIXELS
/*
 * A PICTURE ARRIVED FOR CELLS THAT ARE ALREADY DRAWN. Only those cells are
 * repainted, out of the copy of what the session sent — the alternative is a
 * full repaint on every frame of an animation, which is the one thing a cell
 * grid is supposed to avoid.
 */
static void redraw_slot(unsigned slot)
{
	if (!shadow)
		return;
	for (int y = 0; y < shadow_h; y++)
		for (int x = 0; x < shadow_w; x++) {
			const KtuiCell *c = &shadow[y * shadow_w + x];

			if (!KTUI_IS_SPRITE(c->ch) ||
			    KTUI_SPRITE_SLOT(c->ch) != slot)
				continue;
			draw_one(x, y, c);
		}
}
#endif

static int take_frame(int timeout_ms)
{
	KconMsg m;
	int got = 0;
	int r;

	while ((r = kcon_recv(conn, &m)) == 1) {
		if (m.op == KCON_OP_BYE)
			return -1;

		/*
		 * WHAT THE SESSION COPIED, onto the clipboard of the desktop
		 * this view is running on. `ktui_clip_copy` writes OSC 52 and
		 * is a deliberate no-op on a Linux console, so this does
		 * nothing on tty1 and everything in `foot` or over ssh — which
		 * is where a person has another desktop to paste into.
		 */
		if (m.op == KCON_OP_VIEW_CLIP) {
			KconRd b;

			kcon_rd_init(&b, m.payload, m.len);

			const char *text = kcon_get_str(&b);

			if (!b.err && *text)
				ktui_clip_copy(text);
			continue;
		}

		/*
		 * WHERE THE CARET IS. A view holds no window state, so it is
		 * told; a terminal view puts its own cursor there, which is
		 * the one thing the person's own terminal can draw better
		 * than this desktop can paint. A view with a screen of its own
		 * ignores it — the caret is already a cell in the frame it was
		 * sent.
		 */
		if (m.op == KCON_OP_CURSOR) {
			KconRd b;

			kcon_rd_init(&b, m.payload, m.len);

			int cx = (int)kcon_get_i32(&b);
			int cy = (int)kcon_get_i32(&b);

			if (!b.err && !own_screen)
				ktui_term_caret(cx, cy);
			continue;
		}

		/*
		 * A BELL RINGS WHERE THE PERSON IS. A view in somebody's
		 * terminal writes BEL and lets that terminal do whatever it is
		 * configured to do — a sound, a flash, or nothing; a view with
		 * a screen of its own has no sound to make, and the session
		 * has already inverted the window that rang.
		 */
		if (m.op == KCON_OP_BELL) {
			if (!own_screen) {
				ssize_t r = write(1, "\a", 1);

				(void)r;
			}
			continue;
		}

		if (m.op == KCON_OP_BLANK) {
			KconRd b;

			kcon_rd_init(&b, m.payload, m.len);

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
			continue;
		}

		if (m.op == KCON_OP_SPRITE) {
			take_sprite(&m);
			continue;
		}

		if (m.op != KCON_OP_COMMIT)
			continue;

		KconRd rd;
		KtuiCell run[4096];
		uint16_t x, y;

		kcon_rd_init(&rd, m.payload, m.len);
		while (rd.pos < rd.len && !rd.err) {
			int n = kcon_get_run(&rd, &x, &y, run, 4096);

			if (n < 0)
				break;
			shadow_fit(ktui_w, ktui_h);
			for (int i = 0; i < n; i++) {
				int cx = (int)x + i, cy = (int)y;

				if (shadow && cx < shadow_w && cy < shadow_h)
					shadow[cy * shadow_w + cx] = run[i];
				draw_one(cx, cy, &run[i]);
			}
			got = 1;
		}
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
 */
static volatile sig_atomic_t g_retint;

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
	ktui_term_repalette();
	ktui_draw_invalidate();
}

int main(int argc, char **argv)
{
	const char *sock = getenv("KDOS_CON");
	const char *font = getenv("KDOS_CON_FONT");
	int cols = 0, rows = 0, tty = 0, kms = 0, dump = 0, cast = 0;
	int kms_only = 0;
	const char *shot = NULL;

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

	if (!sock) {
		fprintf(stderr,
			"kdos-view: no session. Set $KDOS_CON or pass --socket.\n");
		return 2;
	}

	/* The font is the KMS mode's; a build without it still accepts --font
	 * so a script need not know which build it is talking to. */
	(void)font;

	if (!tty && !kms && !dump && !cast) {
		fprintf(stderr,
			"kdos-view: choose --kms, --tty, --shot, --dump or "
			"--cast\n");
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
		ktui_sprite_budget(16u << 20, kcell_w(), kcell_h());
		cap_cell_w = kcell_w();
		cap_cell_h = kcell_h();
		cap_flags = KCON_VIEW_PIXELS;
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
		ktui_sprite_budget(16u << 20, kcell_w(), kcell_h());
		cap_cell_w = kcell_w();
		cap_cell_h = kcell_h();
		cap_flags = KCON_VIEW_PIXELS;
#else
		fprintf(stderr, "kdos-view: this build has no cast mode "
				"(built without PipeWire)\n");
		return 1;
#endif
	}

	if (kms) {
#ifdef KDOS_VIEW_KMS
		if (kkms_init(NULL, font) == 0) {
			ktui_draw_init();

			/*
			 * PICTURES ARE EVICTABLE HERE and nowhere else on this
			 * path: this is the only build that turns a blob into
			 * real pixels, so it is the only one holding memory
			 * worth capping. Sixteen megabytes is several
			 * full-screen photographs and no more.
			 */
			ktui_sprite_evictor(sprite_free, NULL);
			ktui_sprite_budget(16u << 20, kcell_w(), kcell_h());
			cap_cell_w = kcell_w();
			cap_cell_h = kcell_h();
			cap_flags = KCON_VIEW_PIXELS;
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
		}
#else
		(void)kms_only;
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
	}

#ifdef KDOS_VIEW_PIXELS
	view_slot_init();

	/*
	 * NO SCREEN OF ITS OWN MEANS PICTURES BECOME CHARACTERS. A terminal and
	 * a dump both draw glyphs and nothing else, so a sprite that arrives
	 * for either is matched to a shape rather than dropped.
	 */
	ascii_mode = !kms && !cast;
#endif

	/*
	 * ZERO IS "I IMPOSE NOTHING". The session ignores a size that is not
	 * positive and keeps the grid its primary view decided, which is what
	 * makes a screenshot a screenshot rather than a resize.
	 */
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
			int rc = view_shot_png(shot, 1);

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
			struct pollfd p[3];
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

			/* A bounded wait even with nothing readable: a seat
			 * event can arrive with no input, and a VT switch must
			 * not wait for a keypress that cannot happen while the
			 * session is inactive. */
			poll(p, (nfds_t)n, 20);
			kkms_pump();

			if (take_frame(0) < 0)
				break;

			/* Nothing is drawn while switched away: the devices
			 * are gone and the framebuffer is somebody else's. */
			if (kkms_active())
				ktui_draw_flush();

			KtuiEvent ev;

			while (ktui_backend()->poll_event(&ev, 0)) {
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
					 * THE POINTER IS A REVERSED CELL, on
					 * this screen exactly as on a terminal
					 * and over ssh. It is the pointer every
					 * text mode has drawn, it needs no
					 * artwork and no pixels, and it is the
					 * same picture wherever the desktop is
					 * looked at.
					 */
					ktui_draw_cursor(ev.mx, ev.my);
					send_ptr(&ev);
				}
			}

			if (kcon_conn_dead(conn))
				break;
		}

		kkms_shutdown();
		kcon_conn_free(conn);
		return 0;
	}
#endif

	for (;;) {
		if (g_retint) {
			g_retint = 0;
			retint();
		}

		if (take_frame(0) < 0)
			break;		/* the session went away */
		ktui_draw_flush();

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
				 * terminal on the far end of ssh can show.
				 */
				ktui_draw_cursor(ev.mx, ev.my);
				send_ptr(&ev);
			}
			else if (ev.type == KT_EVT_RESIZE)
				break;	/* the grid is the session's to remake */
		}
		/* A paste produces no event of its own — the backend queues
		 * it and whoever wants it takes it. */
		send_paste();

		if (kcon_conn_dead(conn))
			break;
	}

	ktui_term_shutdown();
	kcon_conn_free(conn);
	return 0;
}
