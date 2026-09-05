/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The three image protocols, joined to the decoder.
 *
 * libkvt delimits and hands over a payload; libkimg turns bytes into a picture
 * or into nothing; this file is the only thing between them, and it does four
 * jobs: strip the transport encoding, decide how many CELLS the picture
 * occupies, scale it to them, and write the sprite cells into the screen.
 *
 * NOTHING HERE PARSES AN IMAGE FORMAT. Base64 and the key=value control blocks
 * are transport, and they are bounded here; the moment a byte could be part of
 * a picture it goes to libkimg, which is the one place in KDOS that decodes
 * one.
 *
 * A BUILD WITHOUT libkimg LEAVES THE PROTOCOLS OFF ENTIRELY, rather than
 * parsing them and dropping the result: with no callback registered libkvt
 * ignores a sixel dump exactly as it always did, and an APC keeps going
 * nowhere. Parsing bytes nobody can use is a buffer somebody can fill.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "term.h"

#ifdef HAVE_KIMG
#include <pixman.h>

#include "kimg.h"

/*
 * A CELL'S PIXEL SIZE, WHERE THERE IS ONE. Under kdos-comp the backend knows;
 * under kdos-con this program has no pixels at all — the display it is
 * eventually drawn on does, and it scales what arrives. So a cell client
 * renders at a nominal size, which bounds what goes on the wire without
 * pretending to know the font somebody else is using.
 */
#define NOMINAL_CW 10
#define NOMINAL_CH 20

/* Sprites are megabytes where an icon is kilobytes, so the table is given a
 * budget and an evictor; without them the second picture in a session would
 * find the table full and draw its fallback forever. */
#define SPRITE_BUDGET (32u << 20)

/* Kitty transmits a picture and places it later, so a decoded image outlives
 * the sequence that carried it. Bounded, because the peer chooses both how
 * many and how big. */
#define KITTY_STORE 8

/*
 * ANIMATION. A frame is a picture with a delay after it, which is how the
 * protocol describes one and is all this needs to hold: the frames are
 * transmitted whole or composed onto an earlier one, and playback is a timer
 * and an index.
 *
 * A FRAME REPLACES THE PICTURE UNDER THE SAME SPRITE KEY, so the screen is
 * never rewritten. The cells naming the slots stay exactly as they are and only
 * the pixels behind them change — which is why an animation costs no damage in
 * the cell grid at all, and why it needs no extra slots however many frames it
 * has.
 */
#define KITTY_FRAMES 32

/*
 * THE BUDGET IS ON THE FRAMES, not on the slots. Frames are our own memory —
 * the sprite table only ever holds the one that is showing — so an animation
 * that would exceed this drops the frames that do not fit and plays the ones
 * that do, rather than pushing the desktop's own icons out of the table.
 */
#define KITTY_ANIM_BYTES (16u << 20)

struct kitty_frame {
	pixman_image_t *img;
	int gap_ms;			/* after this frame */
};

/*
 * `gen` is what makes a re-transmitted picture a DIFFERENT picture. The sprite
 * key has to change when the pixels behind an id change, or the table answers
 * the new id with the old image's tiles.
 */
static struct {
	uint32_t id;
	uint32_t gen;
	pixman_image_t *img;

	struct kitty_frame fr[KITTY_FRAMES];
	int nfr;			/* frames AFTER the root one */
	int cur;			/* 0 is the root frame */
	int loops;			/* -1 forever, 0 stopped */
	int running;
	unsigned long long due_ms;

	/* Where it was placed, so a frame can replace the pixels behind cells
	 * that are already on the screen. Zero cells means never placed. */
	uint64_t key;
	int cw, ch;
} store[KITTY_STORE];

static uint32_t store_gen;
static size_t anim_bytes;

/* The chunked form: `m=1` says more is coming, and the control block of the
 * first chunk is the one that describes the picture. */
static struct {
	int active;
	char ctl[512];
	uint8_t *buf;
	size_t len, cap;
} chunk;

static int cell_w(void)
{
	int w = kdisp_cell_w();

	return w > 1 ? w : NOMINAL_CW;
}

static int cell_h(void)
{
	int h = kdisp_cell_h();

	return h > 1 ? h : NOMINAL_CH;
}

/* ── transport ─────────────────────────────────────────────────────────── */

static int b64_val(unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

/*
 * Base64, in place of the caller's buffer and never larger than it: three
 * bytes out for every four in, so the output cannot outgrow the input and the
 * cap the parser already enforced still holds. Whitespace is skipped —
 * a payload wrapped at 76 columns is the normal case — and any other
 * character ends the decode, because a picture is not worth guessing at.
 */
static size_t b64_decode(const uint8_t *in, size_t len, uint8_t *out)
{
	size_t n = 0;
	int acc = 0, bits = 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = in[i];
		int v;

		if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;
		if (c == '=')
			break;
		v = b64_val(c);
		if (v < 0)
			break;
		acc = (acc << 6) | v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out[n++] = (uint8_t)((acc >> bits) & 0xff);
		}
	}
	return n;
}

/*
 * One `key=value` out of a control block, which is `a=T,f=100,s=10` for kitty
 * and `name=x;width=4;inline=1` for iTerm2 — the same shape with a different
 * separator. Returns the value's length, or -1.
 */
static int ctl_get(const char *ctl, const char *key, char sep,
		   char *out, size_t cap)
{
	size_t klen = strlen(key);
	const char *p = ctl;

	while (*p) {
		const char *eq = strchr(p, '=');
		const char *end = strchr(p, sep);

		if (!end)
			end = p + strlen(p);
		if (eq && eq < end && (size_t)(eq - p) == klen &&
		    !strncmp(p, key, klen)) {
			size_t n = (size_t)(end - eq - 1);

			if (n >= cap)
				n = cap - 1;
			memcpy(out, eq + 1, n);
			out[n] = 0;
			return (int)n;
		}
		if (!*end)
			break;
		p = end + 1;
	}
	return -1;
}

/* ── the picture becomes cells ─────────────────────────────────────────── */

/* pixman hands the image back to its destroy function and free() does not
 * take one; a cast between the two signatures is undefined behaviour. */
static void free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

/* WHAT A PICTURE LOOKS LIKE WHERE THERE ARE NO PIXELS — a tty, a view built
 * without a pixel library, a dump. Something rather than nothing: a photograph
 * that rendered as blank cells is indistinguishable from output that never
 * arrived. The same shade libkicon falls back to, and the same reason its
 * ASCII form exists: a Linux VT has no UTF-8. */
static uint32_t fallback_cp(void)
{
	return (ktui_caps & KT_CAP_UTF8) ? 0x2593u : (uint32_t)'#';
}

static void sprite_free(uint64_t key, const void *pix, void *user)
{
	(void)key;
	(void)user;
	pixman_image_unref((pixman_image_t *)pix);
}

/* FNV-1a over the payload, which is what makes the same picture sent twice
 * reuse its slots instead of taking a second set. */
static uint64_t hash_bytes(const uint8_t *p, size_t n, int cw, int ch)
{
	uint64_t h = 0xcbf29ce484222325ULL;

	for (size_t i = 0; i < n; i++) {
		h ^= p[i];
		h *= 0x100000001b3ULL;
	}
	h ^= (uint64_t)cw << 32 | (uint64_t)ch;
	h *= 0x100000001b3ULL;
	return h;
}

/* The whole picture, scaled to the cell grid it was given. Kept for the length
 * of the tiling and unreffed after: every tile is a copy of a piece of it. */
static pixman_image_t *scaled;

static const void *tile_of(void *user, int cell_x, int cell_y, int tw, int th)
{
	(void)user;

	int pw = tw * cell_w(), ph = th * cell_h();
	uint32_t *bits = calloc((size_t)pw * (size_t)ph, 4);

	if (!bits)
		return NULL;

	pixman_image_t *t = pixman_image_create_bits(PIXMAN_a8r8g8b8, pw, ph,
						     bits, pw * 4);

	if (!t) {
		free(bits);
		return NULL;
	}
	/* The tile owns its bits: the sprite table holds the image and the
	 * evictor unrefs it, which is what frees them. */
	pixman_image_set_destroy_function(t, free_bits, bits);
	pixman_image_composite32(PIXMAN_OP_SRC, scaled, NULL, t,
				 cell_x * cell_w(), cell_y * cell_h(), 0, 0,
				 0, 0, pw, ph);
	return t;
}

/*
 * Scale a picture to `cw` by `ch` cells and register its tiles under `key`.
 * Returns what the table said: > 0 when every tile took a slot.
 *
 * A KEY THAT IS ALREADY REGISTERED IS REPLACED, which is the whole of playing
 * an animation: the cells on the screen name these slots and go on naming them,
 * and the picture behind them becomes the next frame.
 */
static int register_tiles(pixman_image_t *img, uint64_t key, int cw, int ch)
{
	int sw = pixman_image_get_width(img);
	int sh = pixman_image_get_height(img);
	int dw = cw * cell_w(), dh = ch * cell_h();

	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return -1;

	uint32_t *bits = calloc((size_t)dw * (size_t)dh, 4);

	if (!bits)
		return -1;
	scaled = pixman_image_create_bits(PIXMAN_a8r8g8b8, dw, dh, bits,
					  dw * 4);
	if (!scaled) {
		free(bits);
		return -1;
	}

	if (sw != dw || sh != dh) {
		/* 16.16 fixed point, and the transform maps DESTINATION back
		 * to source — so the ratio is source over destination.
		 * Inverting it scales by the reciprocal. */
		struct pixman_transform t;

		pixman_transform_init_scale(&t,
			(pixman_fixed_t)(((int64_t)sw << 16) / dw),
			(pixman_fixed_t)(((int64_t)sh << 16) / dh));
		pixman_image_set_transform(img, &t);
		pixman_image_set_filter(img, PIXMAN_FILTER_BILINEAR, NULL, 0);
	}
	pixman_image_composite32(PIXMAN_OP_SRC, img, NULL, scaled,
				 0, 0, 0, 0, 0, 0, dw, dh);
	pixman_image_set_transform(img, NULL);

	int r = ktui_sprite_put_tiled(key, cw, ch, fallback_cp(), tile_of,
				      NULL);

	pixman_image_unref(scaled);
	scaled = NULL;
	free(bits);
	return r;
}

/*
 * Put a decoded picture on the screen, `cw` by `ch` cells.
 *
 * ALL OR NOTHING, and the sprite table enforces it: a picture that registered
 * two thirds of its tiles would draw two thirds of itself over whatever the
 * cells beneath it held. A refusal draws the fallback codepoint instead, which
 * is what a terminal with no pixel path shows anyway.
 */
static void place(pixman_image_t *img, uint64_t key, int cw, int ch)
{
	int sw = pixman_image_get_width(img);
	int sh = pixman_image_get_height(img);
	int dw = cw * cell_w(), dh = ch * cell_h();

	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return;

	/*
	 * ALREADY REGISTERED, and every tile of it: the same picture drawn
	 * twice must not be scaled twice. Every tile, because the table evicts
	 * one slot at a time and a picture missing one of them would be drawn
	 * with a hole in it.
	 */
	int cols = (cw + 15) / 16, rows = (ch + 15) / 16, have = 1;

	for (int i = 0; i < cols * rows; i++)
		if (ktui_sprite_find(key ^ ((uint64_t)i * KTUI_TILE_STRIDE)) < 0) {
			have = 0;
			break;
		}
	if (have) {
		kvt_term_place(T.t, key, cw, ch);
		return;
	}

	if (register_tiles(img, key, cw, ch) > 0)
		kvt_term_place(T.t, key, cw, ch);
}

/*
 * HOW BIG IS IT, IN CELLS. A size the sequence asked for wins; otherwise the
 * picture's own pixels divided by a cell, rounded up so the last row of pixels
 * has a cell to be in.
 *
 * Clamped to the screen and to `image_cells`, because the number in the
 * sequence came off a pty: a picture asked to be nine thousand cells wide is a
 * request to scale one to nine thousand cells' worth of pixels.
 */
static void size_in_cells(pixman_image_t *img, int want_w, int want_h,
			  int *cw, int *ch)
{
	int sw = pixman_image_get_width(img);
	int sh = pixman_image_get_height(img);

	/*
	 * CLAMPED BEFORE ANY ARITHMETIC, not after. The numbers came off a
	 * pty, and the aspect-ratio branch below multiplies one of them by a
	 * cell size — clamping the result would be clamping a value that had
	 * already overflowed on the way there.
	 */
	if (want_w < 0 || want_w > TC.image_cells)
		want_w = want_w < 0 ? 0 : TC.image_cells;
	if (want_h < 0 || want_h > TC.image_cells)
		want_h = want_h < 0 ? 0 : TC.image_cells;

	*cw = want_w > 0 ? want_w : (sw + cell_w() - 1) / cell_w();
	*ch = want_h > 0 ? want_h : (sh + cell_h() - 1) / cell_h();

	/* One dimension given and not the other keeps the aspect ratio, which
	 * is what every one of these protocols means by it. */
	if (want_w > 0 && want_h <= 0 && sw > 0)
		*ch = (want_w * cell_w() * sh) / (sw * cell_h());
	if (want_h > 0 && want_w <= 0 && sh > 0)
		*cw = (want_h * cell_h() * sw) / (sh * cell_w());

	if (*cw < 1)
		*cw = 1;
	if (*ch < 1)
		*ch = 1;
	if (*cw > TC.image_cells)
		*cw = TC.image_cells;
	if (*ch > TC.image_cells)
		*ch = TC.image_cells;
	if (*cw > T.cols)
		*cw = T.cols;
	if (*ch > T.rows)
		*ch = T.rows;
}

static KimgBudget budget(void)
{
	KimgBudget b;

	b.max_w = TC.image_cells * 64;
	b.max_h = TC.image_cells * 64;
	b.max_bytes = 64u << 20;
	return b;
}

/*
 * A number out of a control block, bounded before it is used in arithmetic.
 * The `%` and `px` forms below multiply it by a cell size or a column count,
 * and a peer that wrote two billion would overflow the multiply rather than
 * the clamp. 100000 is far past any picture and far short of that.
 */
static int clamp_num(const char *v)
{
	long n = strtol(v, NULL, 10);

	if (n < 0)
		return 0;
	return n > 100000 ? 100000 : (int)n;
}

/* ── iTerm2: OSC 1337 ──────────────────────────────────────────────────── */

/*
 * `File=<key=value;...>:<base64>`. Only an INLINE file is a picture: the same
 * sequence without it is a download, and this terminal does not write files
 * somebody else named.
 */
static void do_osc1337(const uint8_t *p, size_t len)
{
	const uint8_t *colon = memchr(p, ':', len);

	if (!colon)
		return;

	size_t hlen = (size_t)(colon - p);
	char head[512];

	if (hlen >= sizeof(head))
		return;
	memcpy(head, p, hlen);
	head[hlen] = 0;

	if (strncmp(head, "File=", 5))
		return;

	char v[64];

	if (ctl_get(head + 5, "inline", ';', v, sizeof(v)) < 0 || atoi(v) != 1)
		return;

	const uint8_t *b64 = colon + 1;
	size_t b64len = len - hlen - 1;
	uint8_t *raw = malloc(b64len ? b64len : 1);

	if (!raw)
		return;

	size_t n = b64_decode(b64, b64len, raw);
	pixman_image_t *img = NULL;
	KimgBudget b = budget();

	if (n)
		img = kimg_decode(raw, n, KIMG_AUTO, &b);
	if (img) {
		int want_w = 0, want_h = 0;

		/*
		 * `auto`, `N`, `Npx` and `N%`. Cells are the bare number,
		 * which is the unit this grid is in; the other two are
		 * converted here so nothing downstream carries a unit.
		 */
		if (ctl_get(head + 5, "width", ';', v, sizeof(v)) > 0 &&
		    strcmp(v, "auto")) {
			int n2 = clamp_num(v);
			const char *u = v + strspn(v, "0123456789");

			want_w = !strcmp(u, "px") ? (n2 + cell_w() - 1) / cell_w()
				 : !strcmp(u, "%") ? T.cols * n2 / 100 : n2;
		}
		if (ctl_get(head + 5, "height", ';', v, sizeof(v)) > 0 &&
		    strcmp(v, "auto")) {
			int n2 = clamp_num(v);
			const char *u = v + strspn(v, "0123456789");

			want_h = !strcmp(u, "px") ? (n2 + cell_h() - 1) / cell_h()
				 : !strcmp(u, "%") ? T.rows * n2 / 100 : n2;
		}

		int cw, ch;

		size_in_cells(img, want_w, want_h, &cw, &ch);
		place(img, hash_bytes(raw, n, cw, ch), cw, ch);
		pixman_image_unref(img);
	}
	free(raw);
}

/* ── kitty: APC G ──────────────────────────────────────────────────────── */

/* Every frame of one entry, and the bytes they were charged for. */
static void frames_free(int i)
{
	for (int f = 0; f < store[i].nfr; f++) {
		pixman_image_t *im = store[i].fr[f].img;

		if (!im)
			continue;
		anim_bytes -= (size_t)pixman_image_get_width(im) *
			      (size_t)pixman_image_get_height(im) * 4;
		pixman_image_unref(im);
		store[i].fr[f].img = NULL;
	}
	store[i].nfr = 0;
	store[i].cur = 0;
	store[i].running = 0;
	store[i].loops = 0;
}

static int store_slot(uint32_t id)
{
	for (int i = 0; i < KITTY_STORE; i++)
		if (store[i].img && store[i].id == id)
			return i;
	return -1;
}

static uint32_t store_put(uint32_t id, pixman_image_t *img)
{
	uint32_t gen = ++store_gen;

	for (int i = 0; i < KITTY_STORE; i++) {
		if (store[i].img && store[i].id == id) {
			/* New pixels under an id are a new picture, so its
			 * frames are somebody else's animation. */
			frames_free(i);
			pixman_image_unref(store[i].img);
			store[i].img = img;
			store[i].gen = gen;
			store[i].cw = store[i].ch = 0;
			return gen;
		}
	}
	for (int i = 0; i < KITTY_STORE; i++) {
		if (!store[i].img) {
			store[i].id = id;
			store[i].gen = gen;
			store[i].img = img;
			return gen;
		}
	}
	/* Full. The oldest goes, because a peer that transmits nine pictures
	 * without placing any is a peer whose ninth is the one it wants. */
	frames_free(0);
	pixman_image_unref(store[0].img);
	memmove(&store[0], &store[1], sizeof(store[0]) * (KITTY_STORE - 1));
	store[KITTY_STORE - 1].id = id;
	store[KITTY_STORE - 1].gen = gen;
	store[KITTY_STORE - 1].img = img;
	return gen;
}

static void store_drop(uint32_t id, int all)
{
	for (int i = 0; i < KITTY_STORE; i++) {
		if (!store[i].img)
			continue;
		if (!all && store[i].id != id)
			continue;
		frames_free(i);
		pixman_image_unref(store[i].img);
		store[i].img = NULL;
		store[i].cw = store[i].ch = 0;
	}
}

/*
 * RAW RGB AND RGBA, which are not a format and are not decoded: the payload IS
 * the pixels, and the only thing that can go wrong is a declared size that
 * disagrees with how many arrived. That is exactly the check libkimg exists to
 * make, so it is made here in the same shape and before anything allocates.
 */
static pixman_image_t *raw_pixels(const uint8_t *p, size_t n, int w, int h,
				  int comp)
{
	KimgBudget b = budget();

	if (w <= 0 || h <= 0 || w > b.max_w || h > b.max_h)
		return NULL;
	if ((size_t)w * (size_t)h * 4 > b.max_bytes)
		return NULL;
	if (n != (size_t)w * (size_t)h * (size_t)comp)
		return NULL;

	uint32_t *bits = malloc((size_t)w * (size_t)h * 4);

	if (!bits)
		return NULL;

	for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
		const uint8_t *s = p + i * (size_t)comp;
		uint32_t a = comp == 4 ? s[3] : 0xff;

		/* Premultiplied, because PIXMAN_a8r8g8b8 is. Compositing an
		 * unpremultiplied buffer as if it were one puts a bright halo
		 * round everything transparent. */
		bits[i] = (a << 24) |
			  ((uint32_t)(s[0] * a / 255) << 16) |
			  ((uint32_t)(s[1] * a / 255) << 8) |
			  (uint32_t)(s[2] * a / 255);
	}

	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						      bits, w * 4);

	if (!img) {
		free(bits);
		return NULL;
	}
	return img;
}

static unsigned long long anim_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ull +
	       (unsigned long long)(ts.tv_nsec / 1000000);
}

/* The picture that should be on the screen for this entry right now. */
static pixman_image_t *anim_image(int i)
{
	if (store[i].cur <= 0 || store[i].cur > store[i].nfr)
		return store[i].img;
	return store[i].fr[store[i].cur - 1].img;
}

static int anim_gap(int i)
{
	if (store[i].cur <= 0 || store[i].cur > store[i].nfr)
		return store[i].nfr ? store[i].fr[0].gap_ms : 0;
	return store[i].fr[store[i].cur - 1].gap_ms;
}

/*
 * TRANSMIT A FRAME — `a=f`. The payload is a whole picture at the root's size,
 * or a rectangle composed onto an earlier frame at `x`,`y`; `c` names the frame
 * to compose onto and `z` is the delay after this one.
 *
 * A frame that will not fit the budget is DROPPED, and the ones already
 * accepted still play. An animation that is half there is worth more than an
 * animation that pushed every icon on the desktop out of the sprite table.
 */
static void kitty_frame(int i, pixman_image_t *img, int base, int gap,
			int at_x, int at_y)
{
	int rw = pixman_image_get_width(store[i].img);
	int rh = pixman_image_get_height(store[i].img);
	size_t cost = (size_t)rw * (size_t)rh * 4;

	if (store[i].nfr >= KITTY_FRAMES || anim_bytes + cost > KITTY_ANIM_BYTES) {
		pixman_image_unref(img);
		return;
	}

	uint32_t *bits = calloc((size_t)rw * (size_t)rh, 4);
	pixman_image_t *fr = bits ? pixman_image_create_bits(PIXMAN_a8r8g8b8,
							    rw, rh, bits,
							    rw * 4)
				  : NULL;

	if (!fr) {
		free(bits);
		pixman_image_unref(img);
		return;
	}
	pixman_image_set_destroy_function(fr, free_bits, bits);

	/*
	 * THE BASE FIRST, THEN THE NEW PIXELS OVER IT. `c=0` and a frame that
	 * covers everything are the same thing here — the base is copied and
	 * then painted over — so composition needs no separate path.
	 */
	pixman_image_t *under = (base > 0 && base <= store[i].nfr)
				? store[i].fr[base - 1].img
				: store[i].img;

	pixman_image_composite32(PIXMAN_OP_SRC, under, NULL, fr, 0, 0, 0, 0,
				 0, 0, rw, rh);
	pixman_image_composite32(PIXMAN_OP_OVER, img, NULL, fr, 0, 0, 0, 0,
				 at_x, at_y, pixman_image_get_width(img),
				 pixman_image_get_height(img));
	pixman_image_unref(img);

	store[i].fr[store[i].nfr].img = fr;
	store[i].fr[store[i].nfr].gap_ms = gap > 0 ? gap : 100;
	store[i].nfr++;
	anim_bytes += cost;
}

static void kitty_apply(const char *ctl, const uint8_t *payload, size_t len)
{
	char v[64];
	char action = 'T';
	uint32_t id = 0;
	int fmt = 32, sw = 0, sh = 0;
	int cols = 0, rows = 0;
	int gap = 0, at_x = 0, at_y = 0, base = 0, anim_state = 0, loops = 0;
	int have_loops = 0;

	if (ctl_get(ctl, "a", ',', v, sizeof(v)) > 0)
		action = v[0];
	if (ctl_get(ctl, "i", ',', v, sizeof(v)) > 0)
		id = (uint32_t)strtoul(v, NULL, 10);
	if (ctl_get(ctl, "f", ',', v, sizeof(v)) > 0)
		fmt = atoi(v);
	if (ctl_get(ctl, "s", ',', v, sizeof(v)) > 0)
		sw = clamp_num(v);
	if (ctl_get(ctl, "v", ',', v, sizeof(v)) > 0)
		sh = clamp_num(v);
	if (ctl_get(ctl, "c", ',', v, sizeof(v)) > 0)
		cols = clamp_num(v);
	if (ctl_get(ctl, "r", ',', v, sizeof(v)) > 0)
		rows = clamp_num(v);
	if (ctl_get(ctl, "z", ',', v, sizeof(v)) > 0)
		gap = clamp_num(v);
	if (ctl_get(ctl, "x", ',', v, sizeof(v)) > 0)
		at_x = clamp_num(v);
	if (ctl_get(ctl, "y", ',', v, sizeof(v)) > 0)
		at_y = clamp_num(v);
	/*
	 * `s` AND `v` MEAN DIFFERENT THINGS PER ACTION, which is the protocol's
	 * doing and not a shortcut here: on a transmission they are the source
	 * width and height, and on an animation control they are the state and
	 * the loop count. The action is read first, so there is no ambiguity.
	 */
	if (action == 'a') {
		anim_state = sw;
		if (ctl_get(ctl, "v", ',', v, sizeof(v)) > 0) {
			loops = clamp_num(v);
			have_loops = 1;
		}
	}

	base = cols;	/* `c` is the base frame for a=f and the frame to show
			 * for a=a; it is the column count for everything else,
			 * which is what the protocol does with one letter. */

	if (action == 'd') {
		store_drop(id, id == 0);
		return;
	}

	/*
	 * ANIMATION CONTROL — `a=a`. `S` is stop, run-and-wait or run; `v` is
	 * how many times round, where zero is for ever; `c` selects the frame
	 * to show; `z` sets the delay after it.
	 */
	if (action == 'a') {
		int i = store_slot(id);

		if (i < 0)
			return;
		if (base > 0 && base <= store[i].nfr + 1)
			store[i].cur = base - 1;
		/* `r` names the frame `z` applies to, counting the root as 1. */
		if (gap > 0 && rows > 1 && rows <= store[i].nfr + 1)
			store[i].fr[rows - 2].gap_ms = gap;
		if (have_loops)
			store[i].loops = loops > 0 ? loops : -1;
		if (anim_state == 1) {
			store[i].running = 0;
		} else if (anim_state >= 2) {
			if (!store[i].loops)
				store[i].loops = -1;
			store[i].running = store[i].nfr > 0;
			store[i].due_ms = anim_now() +
					  (unsigned long long)anim_gap(i);
		}
		return;
	}

	if (action == 'p') {
		int i = store_slot(id);
		int cw, ch;

		if (i < 0)
			return;
		size_in_cells(store[i].img, cols, rows, &cw, &ch);

		uint64_t key = hash_bytes((const uint8_t *)&store[i].gen,
					  sizeof(store[i].gen), cw, ch);

		place(store[i].img, key, cw, ch);
		/* WHERE IT LANDED, so a frame can replace the pixels behind
		 * cells that are already on the screen. */
		store[i].key = key;
		store[i].cw = cw;
		store[i].ch = ch;
		return;
	}

	/*
	 * `a=q` IS A QUESTION, AND SILENCE IS THE ONE ANSWER THAT BREAKS IT.
	 *
	 * A picture program sends a tiny transmission with `a=q` and waits: an
	 * `OK` means it may use this protocol, an error code means it must
	 * fall back, and NOTHING means it waits for its own timeout and then
	 * draws as though the terminal were a teletype. Dropping the query is
	 * therefore the most expensive way to not support something.
	 *
	 * NOTHING IS STORED. A query names an id so the reply can be matched
	 * to it, and it must not leave an image behind — which is exactly why
	 * this returns here rather than falling into the transmit path.
	 */
	if (action == 'q') {
		char reply[64];
		int n;

		/*
		 * The id is echoed so a program with several in flight knows
		 * which it is hearing about. A query that named none is
		 * answered anyway, with i=0: the alternative is silence, which
		 * is the failure this branch exists to prevent.
		 */
		n = snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\",
			     (unsigned)id);
		kvt_term_write(T.t, reply, (size_t)n);
		return;
	}

	if (action != 'T' && action != 't' && action != 'f')
		return;		/* a query, or an action this does not have */

	uint8_t *raw = malloc(len ? len : 1);

	if (!raw)
		return;

	size_t n = b64_decode(payload, len, raw);
	pixman_image_t *img = NULL;

	if (n) {
		KimgBudget b = budget();

		if (fmt == 24 || fmt == 32)
			img = raw_pixels(raw, n, sw, sh, fmt == 24 ? 3 : 4);
		else
			img = kimg_decode(raw, n, KIMG_AUTO, &b);
	}
	free(raw);
	if (!img)
		return;

	/*
	 * A FRAME BELONGS TO A PICTURE THAT ALREADY EXISTS. There is nothing to
	 * animate otherwise, and storing it as a picture in its own right would
	 * put a frame of somebody's animation in the store under their id.
	 */
	if (action == 'f') {
		int i = store_slot(id);

		if (i < 0) {
			pixman_image_unref(img);
			return;
		}
		kitty_frame(i, img, base, gap, at_x, at_y);
		return;
	}

	/* Stored either way, and stored FIRST: `a=T` is transmit AND display,
	 * a peer that displayed one may place it again by id without
	 * re-sending it, and the generation the store hands back is what makes
	 * this picture's sprite key different from the last one under this
	 * id. */
	uint32_t gen = store_put(id, img);

	if (action == 'T') {
		int cw, ch;

		size_in_cells(img, cols, rows, &cw, &ch);

		uint64_t key = hash_bytes((const uint8_t *)&gen, sizeof(gen),
					  cw, ch);

		place(img, key, cw, ch);

		int i = store_slot(id);

		if (i >= 0) {
			store[i].key = key;
			store[i].cw = cw;
			store[i].ch = ch;
		}
	}
}

/*
 * Advance every running animation whose frame is due, and say how long until
 * the next one is — or -1 when nothing is animating, so a caller with nothing
 * else to do waits on its descriptors instead of on a clock.
 *
 * THE SCREEN IS NOT TOUCHED. Each frame re-registers the SAME sprite key, so
 * the cells that name its slots go on naming them and only the pixels change.
 */
int term_pic_tick(void)
{
	unsigned long long now = anim_now();
	long long next = -1;

	for (int i = 0; i < KITTY_STORE; i++) {
		if (!store[i].img || !store[i].running || !store[i].nfr)
			continue;
		if (!store[i].cw || !store[i].ch)
			continue;	/* transmitted but never placed */

		if (now >= store[i].due_ms) {
			store[i].cur++;
			if (store[i].cur > store[i].nfr) {
				store[i].cur = 0;
				if (store[i].loops > 0 &&
				    --store[i].loops == 0) {
					store[i].running = 0;
					continue;
				}
			}

			pixman_image_t *im = anim_image(i);

			if (im)
				register_tiles(im, store[i].key, store[i].cw,
					       store[i].ch);
			store[i].due_ms = now +
					  (unsigned long long)anim_gap(i);
		}

		long long wait = (long long)store[i].due_ms - (long long)now;

		if (wait < 0)
			wait = 0;
		if (next < 0 || wait < next)
			next = wait;
	}
	return (int)next;
}

static void chunk_reset(void)
{
	chunk.active = 0;
	chunk.len = 0;
	chunk.ctl[0] = 0;
}

static void do_kitty(const char *ctl, const uint8_t *payload, size_t len)
{
	char v[64];
	int more = 0;

	if (ctl_get(ctl, "m", ',', v, sizeof(v)) > 0)
		more = atoi(v);

	if (!more && !chunk.active) {
		kitty_apply(ctl, payload, len);
		return;
	}

	/* The FIRST chunk's control block describes the picture; the ones
	 * after it carry `m` and nothing worth keeping. */
	if (!chunk.active) {
		chunk.active = 1;
		chunk.len = 0;
		snprintf(chunk.ctl, sizeof(chunk.ctl), "%s", ctl);
	}

	size_t cap = (size_t)TC.image_max * 1024;

	if (chunk.len + len > cap) {
		chunk_reset();
		return;
	}
	if (chunk.len + len > chunk.cap) {
		size_t want = chunk.cap ? chunk.cap * 2 : 8192;

		while (want < chunk.len + len)
			want *= 2;
		if (want > cap)
			want = cap;

		uint8_t *nb = realloc(chunk.buf, want);

		if (!nb) {
			chunk_reset();
			return;
		}
		chunk.buf = nb;
		chunk.cap = want;
	}
	memcpy(chunk.buf + chunk.len, payload, len);
	chunk.len += len;

	if (!more) {
		kitty_apply(chunk.ctl, chunk.buf, chunk.len);
		chunk_reset();
	}
}

/*
 * ANSWER A KITTY QUERY WITH A REFUSAL. Used only where pictures are off; the
 * reply shape is the protocol's own `<code>:<message>`, which a client reads
 * as "do not use this protocol here" rather than as a transient failure.
 *
 * Only a QUERY is answered. Every other action is silently dropped, because a
 * transmission that was never going to be shown has no reply in the protocol
 * and a client sending one is not waiting for one.
 */
static void kitty_refuse(const char *ctl)
{
	char v[64], reply[96];
	uint32_t id = 0;
	int n;

	if (!ctl || ctl_get(ctl, "a", ',', v, sizeof(v)) <= 0 || v[0] != 'q')
		return;
	if (ctl_get(ctl, "i", ',', v, sizeof(v)) > 0)
		id = (uint32_t)strtoul(v, NULL, 10);
	n = snprintf(reply, sizeof(reply),
		     "\033_Gi=%u;ENOTSUP:pictures are off in this terminal"
		     "\033\\", (unsigned)id);
	kvt_term_write(T.t, reply, (size_t)n);
}

/* ── the one callback ──────────────────────────────────────────────────── */

static void on_image(struct kvt_vte *vte, enum kvt_img_kind kind,
		     const char *params, const uint8_t *payload, size_t len,
		     void *data)
{
	(void)vte;
	(void)data;

	if (!T.t)
		return;
	/*
	 * PICTURES OFF STILL ANSWERS THE KITTY QUESTION, and refuses it.
	 *
	 * `a=q` asks whether this terminal will take a picture. A terminal
	 * with pictures turned off that says nothing is one the program waits
	 * on and then times out against — the same cost as not implementing
	 * the protocol at all, paid by a person who turned pictures off on
	 * purpose. An error reply is instant and is what the fallback path is
	 * written for.
	 */
	if (!TC.images) {
		if (kind == KVT_IMG_KITTY)
			kitty_refuse(params);
		return;
	}

	switch (kind) {
	case KVT_IMG_SIXEL: {
		/*
		 * The body, re-framed by libkimg — libsixel will not decode a
		 * bare one. The introducer's parameters are aspect ratio and
		 * background handling, which libsixel reads out of the frame
		 * it is given rather than out of anything this file passes.
		 */
		KimgBudget b = budget();
		pixman_image_t *img = kimg_decode(payload, len, KIMG_SIXEL, &b);
		int cw, ch;

		(void)params;
		if (!img)
			return;
		size_in_cells(img, 0, 0, &cw, &ch);
		place(img, hash_bytes(payload, len, cw, ch), cw, ch);
		pixman_image_unref(img);
		break;
	}
	case KVT_IMG_OSC1337:
		do_osc1337(payload, len);
		break;
	case KVT_IMG_KITTY:
		do_kitty(params, payload, len);
		break;
	}
}

/*
 * WHAT A PROGRAM IS TOLD IT MAY SEND, in pixels, and it is the same bound
 * `fit()` above actually enforces: `image_cells` cells in each direction, and
 * never more of them than the grid has. Reporting the decoder's own pixel
 * budget instead would name a size this terminal then clamps, and a picture
 * clipped after the terminal said it would fit is worse than one refused.
 *
 * RE-STATED WHENEVER THE GRID CHANGES, because half the bound is the number of
 * columns and rows — a geometry answered from the size the window opened at is
 * wrong for every size after the first.
 */
void term_pic_geom(void)
{
	int cw = TC.image_cells < T.cols ? TC.image_cells : T.cols;
	int ch = TC.image_cells < T.rows ? TC.image_cells : T.rows;

	if (!TC.images) {
		/* A build or a session with pictures off answers the query
		 * with a failure, which is what "no geometry" means. */
		kvt_term_img_geom(T.t, 0, 0);
		return;
	}
	kvt_term_img_geom(T.t, cw * cell_w(), ch * cell_h());
}

void term_pic_init(void)
{
	if (!TC.images) {
		term_pic_geom();
		return;
	}

	ktui_sprite_evictor(sprite_free, NULL);
	ktui_sprite_budget(SPRITE_BUDGET, cell_w(), cell_h());
	kvt_term_img_cb(T.t, on_image, (size_t)TC.image_max * 1024, NULL);
	term_pic_geom();
}

void term_pic_shutdown(void)
{
	store_drop(0, 1);
	free(chunk.buf);
	chunk.buf = NULL;
	chunk.cap = 0;
	ktui_sprite_clear();
}

#else	/* no libkimg in this build */

/* Nothing decodes, so there is no geometry to report and the query says so. */
void term_pic_geom(void)
{
	kvt_term_img_geom(T.t, 0, 0);
}

void term_pic_init(void)
{
	term_pic_geom();
}

void term_pic_shutdown(void)
{
}

/* Nothing decodes, so nothing animates: -1 is "wait on the descriptors". */
int term_pic_tick(void)
{
	return -1;
}

#endif
