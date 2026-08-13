/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   deco.c — window frames, drawn as characters
 *
 *   ╔═[■]═ GIMP ─ photo.png ═══════════════[_][↑]═╗       focused
 *   ║                                             ║▒
 *   ║            < real GIMP pixels >             ║▒
 *   ╚═════════════════════════════════════════════◢▒
 *    ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
 *
 *   ┌──[■]─ mc ────────────────────────────[_][↑]─┐       unfocused
 *
 * Turbo Vision, in libkcolor's palette, on the same glyph grid as tty1 and the
 * boot splash. FOCUS IS SHOWN BY THE FRAME WEIGHT, not by a colour alone —
 * double lines when focused, single when not. That is how Turbo Vision did it,
 * it survives a monochrome screen, and the two glyph sets are the same width so
 * it costs no relayout.
 *
 * SIX SCENE NODES, NOT ONE BUFFER. The obvious implementation is one cell grid
 * the size of the whole frame with a transparent hole in the middle, and it
 * uploads w*h*4 bytes on every repaint — 8 MB at 1080p, for a focus change. So
 * a frame is four strips of cells (top, left, right, bottom) plus two
 * wlr_scene_rects for the shadow. The shadow is rects rather than cells on
 * purpose: a Turbo Vision shadow is a DARKENING of what is underneath, which is
 * exactly what a rect with alpha is, and a grid of ▒ would be a worse imitation
 * of the same thing at a hundred times the cost.
 *
 * THE NODES ARE CHILDREN OF t->scene_tree, and that is the whole integration
 * with the window management that already exists. wlr_scene_node_set_position()
 * on the toplevel moves the frame, raise_to_top() raises it, a workspace switch
 * hides it, and the window's own destroy path is where it is freed. A sibling
 * tree would need every one of those kept in agreement by hand.
 *
 * THE LAST CELL OF A STRIP IS CLIPPED. Windows sit at arbitrary pixel positions
 * and a strip's width is rarely a multiple of the cell width, so a strip renders
 * ceil(w / cell_w) cells and lets the last one run off the end. On a run of ═
 * that is invisible. This is what removes the need for a global grid, and with
 * it every class of bug where two grids disagree about where a cell starts.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kdos-comp.h"

/*
 * The shadow offset, in cells: two across, one down.
 *
 * Turbo Vision's shadow was two characters right and one row down, which on an
 * 8x16 VGA cell is 16 by 16 — square. A KDOS cell is 16x32, so the same two-and-
 * one lands on 32 by 32 and is square again. Copying the pixel count instead of
 * the cell count would give a shadow twice as tall as it is wide.
 */
#define SHADOW_CELLS_X 2
#define SHADOW_CELLS_Y 1

/* Frames narrower than this lose their furniture rather than overlapping it. */
#define MIN_COLS_BUTTONS 22
#define MIN_COLS_CLOSE   10

enum { STRIP_TOP = 0, STRIP_LEFT, STRIP_RIGHT, STRIP_BOTTOM, STRIP_N };

struct kc_deco {
	struct kc_toplevel *t;
	struct wlr_scene_buffer *strip[STRIP_N];
	/* One typed descriptor per strip, pointed at by the strip node's
	 * `data`. The scene hit test returns the node; the tag says whose
	 * chrome it is. See struct kc_deco_part in kdos-comp.h. */
	struct kc_deco_part part[STRIP_N];
	struct wlr_scene_rect *shadow[2];
	/* What the strips were last painted FOR. A repaint that would produce
	 * identical pixels is skipped, which matters because commit fires far
	 * more often than the frame changes. */
	int last_w, last_h, last_scale;
	bool last_focused;
	char *last_title;
	bool hidden;			/* fullscreen: frame and shadow off */
};

/* ── the glyph budget ──────────────────────────────────────────────────────
 *
 * Resolved once against the font that is actually loaded, with a fallback per
 * slot. A missing glyph is a BLANK CELL and nothing else — no warning, no
 * substitute box — so a titlebar silently losing its close box is exactly the
 * kind of defect that ships. `kc_deco_check_glyphs()` is what turns that into a
 * line in the log at startup instead.
 */
static struct {
	uint32_t tl, tr, bl, br, h, v;		/* focused — double */
	uint32_t stl, str, sbl, sbr, sh, sv;	/* unfocused — single */
	uint32_t close, min, max, grip;
	uint32_t ellipsis;
} G;

static uint32_t pick(const uint32_t *want, int n)
{
	for (int i = 0; i < n; i++)
		if (kcell_has(want[i]))
			return want[i];
	return want[n - 1];		/* the last one is always ASCII */
}

#define PICK(...) pick((const uint32_t[]){ __VA_ARGS__ }, \
		       (int)(sizeof((const uint32_t[]){ __VA_ARGS__ }) / \
			     sizeof(uint32_t)))

void kc_deco_glyphs_init(void)
{
	G.tl = PICK(0x2554, 0x250c, '+');	/* ╔ ┌ + */
	G.tr = PICK(0x2557, 0x2510, '+');	/* ╗ ┐ + */
	G.bl = PICK(0x255a, 0x2514, '+');	/* ╚ └ + */
	G.br = PICK(0x255d, 0x2518, '+');	/* ╝ ┘ + */
	G.h  = PICK(0x2550, 0x2500, '-');	/* ═ ─ - */
	G.v  = PICK(0x2551, 0x2502, '|');	/* ║ │ | */

	G.stl = PICK(0x250c, '+');
	G.str = PICK(0x2510, '+');
	G.sbl = PICK(0x2514, '+');
	G.sbr = PICK(0x2518, '+');
	G.sh  = PICK(0x2500, '-');
	G.sv  = PICK(0x2502, '|');

	G.close = PICK(0x25a0, 0x25aa, 'x');	/* ■ ▪ x */
	G.min   = '_';
	G.max   = PICK(0x2191, '^');		/* ↑ ^ */
	/*
	 * The resize grip. ◢ is the one glyph in this budget that Terminus may
	 * genuinely not carry — it is not in the console's 512 and it is not a
	 * box-drawing character — so the fallback chain matters here more than
	 * anywhere else, and it ends on the corner the frame would have drawn
	 * anyway.
	 */
	G.grip = PICK(0x25e2, 0x25e5, 0x2593, 0x255d);	/* ◢ ◥ ▓ ╝ */
	G.ellipsis = PICK(0x2026, '.');			/* … . */
}

/*
 * Report anything the font could not supply. Called once, at startup, because
 * the alternative is finding out from a screenshot.
 */
void kc_deco_check_glyphs(void)
{
	static const struct { uint32_t cp; const char *name; } budget[] = {
		{ 0x2554, "╔" }, { 0x2557, "╗" }, { 0x255a, "╚" }, { 0x255d, "╝" },
		{ 0x2550, "═" }, { 0x2551, "║" }, { 0x250c, "┌" }, { 0x2510, "┐" },
		{ 0x2514, "└" }, { 0x2518, "┘" }, { 0x2500, "─" }, { 0x2502, "│" },
		{ 0x25a0, "■" }, { 0x2191, "↑" }, { 0x25e2, "◢" }, { 0x2026, "…" },
		{ 0x2591, "░" }, { 0x2592, "▒" }, { 0x2593, "▓" }, { 0x2588, "█" },
	};
	char missing[256] = { 0 };
	size_t n = 0;
	for (size_t i = 0; i < sizeof(budget) / sizeof(budget[0]); i++) {
		if (kcell_has(budget[i].cp))
			continue;
		size_t l = strlen(budget[i].name);
		if (n + l + 2 < sizeof(missing)) {
			if (n)
				missing[n++] = ' ';
			memcpy(missing + n, budget[i].name, l);
			n += l;
			missing[n] = 0;
		}
	}
	if (n)
		wlr_log(WLR_ERROR,
			"deco: the font is missing %s — those cells will be "
			"drawn with a fallback", missing);
	else
		wlr_log(WLR_INFO, "deco: the full glyph budget is present");
}

/* ── metrics ───────────────────────────────────────────────────────────────
 *
 * One border cell on every side, so the titlebar IS the top border — Turbo
 * Vision again, and the reason the close box sits inside the corner rather than
 * on a bar of its own.
 */

/*
 * The scale is the OUTPUT's, not the session's.
 *
 * A 4K panel beside a 1080p one is the case that decides this: one global
 * number makes the chrome either tiny on one screen or enormous on the other.
 * Every frame carries its own cell grid anyway — there is no global grid to
 * keep in agreement — so asking per window costs one layout lookup and gets a
 * mixed-DPI desk right.
 *
 * `deco_scale` in comp.conf forces it; 0 (the default) means follow the output.
 */
int kc_deco_scale(struct kc_toplevel *t)
{
	struct kc_server *s = t->server;
	int sc = s->deco_scale;

	if (sc <= 0) {
		struct wlr_output *o = wlr_output_layout_output_at(
			s->output_layout, t->x, t->y);
		sc = o ? (int)(o->scale + 0.5f) : 1;
	}
	if (sc < 1)
		sc = 1;
	if (sc > KCELL_MAX_SCALE)
		sc = KCELL_MAX_SCALE;
	return sc;
}

int kc_deco_border_w(struct kc_toplevel *t)
{
	return kcell_w() * kc_deco_scale(t);
}

int kc_deco_border_h(struct kc_toplevel *t)
{
	return kcell_h() * kc_deco_scale(t);
}

/* The window's own size, as the client last committed it. */
static void window_size(struct kc_toplevel *t, int *w, int *h)
{
	struct wlr_box g = t->xdg_toplevel->base->geometry;
	*w = g.width > 0 ? g.width : 1;
	*h = g.height > 0 ? g.height : 1;
}

/* ── painting ──────────────────────────────────────────────────────────── */

static void set_cell(KtuiCell *c, uint32_t ch, uint8_t fg, uint8_t bg)
{
	c->ch = ch;
	c->fg = fg;
	c->bg = bg;
	c->attr = KT_A_NONE;
}

/*
 * Where the buttons sit, in cells from the left edge of the top strip.
 *
 * Returned rather than recomputed at the hit-test, because a titlebar whose
 * drawn close box and clickable close box disagree is a desktop that closes the
 * wrong window. One function, two callers.
 */
struct button_layout {
	int close_at;		/* -1 when it did not fit */
	int min_at, max_at;	/* -1 when they did not fit */
	int title_at, title_w;
};

static struct button_layout layout_top(int cols)
{
	struct button_layout b = { -1, -1, -1, 1, 0 };

	if (cols >= MIN_COLS_CLOSE)
		b.close_at = 2;			/* ╔ ═ [ ■ ] */
	if (cols >= MIN_COLS_BUTTONS) {
		b.min_at = cols - 8;		/* [_][↑] ═ ╗ */
		b.max_at = cols - 5;
	}

	b.title_at = b.close_at >= 0 ? b.close_at + 5 : 2;
	int end = b.min_at >= 0 ? b.min_at - 1 : cols - 2;
	b.title_w = end - b.title_at;
	if (b.title_w < 0)
		b.title_w = 0;
	return b;
}

/* Minimal UTF-8 decode. The title comes from a client and may be anything; a
 * malformed sequence becomes one replacement cell rather than a desync that
 * eats the rest of the string. */
static uint32_t utf8_next(const char **p)
{
	const unsigned char *s = (const unsigned char *)*p;
	uint32_t cp;
	int n;

	if (s[0] < 0x80) {
		cp = s[0];
		n = 1;
	} else if ((s[0] & 0xe0) == 0xc0) {
		cp = s[0] & 0x1f;
		n = 2;
	} else if ((s[0] & 0xf0) == 0xe0) {
		cp = s[0] & 0x0f;
		n = 3;
	} else if ((s[0] & 0xf8) == 0xf0) {
		cp = s[0] & 0x07;
		n = 4;
	} else {
		*p += 1;
		return 0xfffd;
	}
	for (int i = 1; i < n; i++) {
		if ((s[i] & 0xc0) != 0x80) {
			*p += 1;
			return 0xfffd;
		}
		cp = (cp << 6) | (s[i] & 0x3f);
	}
	*p += n;
	return cp;
}

static int title_cells(const char *title, uint32_t *out, int max)
{
	int n = 0;
	const char *p = title;
	while (*p && n < max)
		out[n++] = utf8_next(&p);
	if (*p && n > 0)
		out[n - 1] = G.ellipsis;	/* elide, never overrun */
	return n;
}

static void paint_top(KtuiCell *row, int cols, bool focused, const char *title)
{
	uint8_t line = focused ? KT_ACCENT : KT_DIM;
	uint8_t text = focused ? KT_TEXT : KT_DIM;
	uint8_t bg = KT_SURFACE;
	uint32_t h = focused ? G.h : G.sh;

	for (int i = 0; i < cols; i++)
		set_cell(&row[i], h, line, bg);
	set_cell(&row[0], focused ? G.tl : G.stl, line, bg);
	set_cell(&row[cols - 1], focused ? G.tr : G.str, line, bg);

	struct button_layout b = layout_top(cols);

	if (b.close_at >= 0) {
		set_cell(&row[b.close_at], '[', line, bg);
		/* The close box is the one control that is destructive, so it
		 * takes the urgent slot — the same colour the panel uses to say
		 * something is wrong. Dim when unfocused with everything else. */
		set_cell(&row[b.close_at + 1], G.close,
			 focused ? KT_ERR : KT_DIM, bg);
		set_cell(&row[b.close_at + 2], ']', line, bg);
	}
	if (b.min_at >= 0) {
		set_cell(&row[b.min_at], '[', line, bg);
		set_cell(&row[b.min_at + 1], G.min, focused ? KT_MID : KT_DIM, bg);
		set_cell(&row[b.min_at + 2], ']', line, bg);
		set_cell(&row[b.max_at], '[', line, bg);
		set_cell(&row[b.max_at + 1], G.max, focused ? KT_MID : KT_DIM, bg);
		set_cell(&row[b.max_at + 2], ']', line, bg);
	}

	if (title && *title && b.title_w > 2) {
		uint32_t buf[256];
		int max = b.title_w - 2 < 256 ? b.title_w - 2 : 256;
		int n = title_cells(title, buf, max);
		if (n > 0) {
			/* A space either side so the title never touches the
			 * rule it sits in. */
			set_cell(&row[b.title_at], ' ', line, bg);
			for (int i = 0; i < n; i++)
				set_cell(&row[b.title_at + 1 + i], buf[i], text, bg);
			set_cell(&row[b.title_at + 1 + n], ' ', line, bg);
		}
	}
}

static void paint_bottom(KtuiCell *row, int cols, bool focused)
{
	uint8_t line = focused ? KT_ACCENT : KT_DIM;
	uint8_t bg = KT_SURFACE;
	uint32_t h = focused ? G.h : G.sh;

	for (int i = 0; i < cols; i++)
		set_cell(&row[i], h, line, bg);
	set_cell(&row[0], focused ? G.bl : G.sbl, line, bg);
	/* The grip replaces the corner rather than sitting beside it: a control
	 * that needs its own cell would make the frame one cell wider than the
	 * window on one side only. */
	set_cell(&row[cols - 1], focused ? G.grip : G.sbr, line, bg);
}

static void paint_side(KtuiCell *col, int rows, bool focused)
{
	uint8_t line = focused ? KT_ACCENT : KT_DIM;
	uint32_t v = focused ? G.v : G.sv;
	for (int i = 0; i < rows; i++)
		set_cell(&col[i], v, line, KT_SURFACE);
}

/*
 * Paint one strip and hand it to the scene.
 *
 * create -> paint -> set_buffer -> drop, which is the ownership rule cellbuf.c
 * exists to make easy: after the drop the scene holds the only lock, so nothing
 * here can be written while the renderer is reading it.
 */
static void push_strip(int scale, struct wlr_scene_buffer *node,
		       const KtuiCell *cells, int cols, int rows, int px_w,
		       int px_h)
{
	if (px_w <= 0 || px_h <= 0)
		return;

	struct kc_cellbuf *cb = kc_cellbuf_create(px_w, px_h, true, true);
	if (!cb)
		return;

	kcell_paint(kc_cellbuf_image(cb), cells, NULL, cols, rows, 1,
		    scale, px_w, px_h);

	wlr_scene_buffer_set_buffer(node, kc_cellbuf_base(cb));
	wlr_scene_buffer_set_dest_size(node, px_w, px_h);
	kc_cellbuf_drop(cb);
}

/* ── the public half ───────────────────────────────────────────────────── */

bool kc_deco_enabled(const struct kc_server *s)
{
	return s->deco_frames && kcell_w() > 0;
}

void kc_deco_create(struct kc_toplevel *t)
{
	struct kc_server *s = t->server;

	if (!kc_deco_enabled(s) || t->csd || t->deco)
		return;

	struct kc_deco *d = calloc(1, sizeof(*d));
	if (!d)
		return;
	d->t = t;
	d->last_w = d->last_h = -1;
	d->last_focused = false;

	/*
	 * The shadow first, so it is UNDER everything else in this tree. Order
	 * within a scene tree is creation order, and a shadow created after the
	 * strips would darken the window it belongs to.
	 */
	float black[4] = { 0.f, 0.f, 0.f, 0.5f };
	for (int i = 0; i < 2; i++) {
		d->shadow[i] = wlr_scene_rect_create(t->scene_tree, 1, 1, black);
		if (d->shadow[i])
			wlr_scene_node_lower_to_bottom(&d->shadow[i]->node);
	}

	for (int i = 0; i < STRIP_N; i++) {
		d->strip[i] = wlr_scene_buffer_create(t->scene_tree, NULL);
		if (!d->strip[i]) {
			kc_deco_destroy(t);
			return;
		}
		/*
		 * The typed tag. The scene hit test is the ONLY input router
		 * now, and this is how it knows a node is chrome and whose:
		 * a strip has no wlr_surface, so without the tag a hit on it
		 * would read as "nothing", and with a geometric second answer
		 * it read as whatever that walk decided — which is the bug the
		 * first live boot shipped.
		 */
		d->part[i].type = KC_NODE_DECO;
		d->part[i].toplevel = t;
		d->strip[i]->node.data = &d->part[i];
	}

	t->deco = d;
	kc_deco_arrange(t);
}

void kc_deco_destroy(struct kc_toplevel *t)
{
	struct kc_deco *d = t->deco;
	if (!d)
		return;

	/*
	 * The nodes go before the buffers they hold, which is the same destroy
	 * order capture.c had to learn: a scene node that outlives what it
	 * points at fires its destroy handler on freed memory.
	 */
	for (int i = 0; i < STRIP_N; i++)
		if (d->strip[i])
			wlr_scene_node_destroy(&d->strip[i]->node);
	for (int i = 0; i < 2; i++)
		if (d->shadow[i])
			wlr_scene_node_destroy(&d->shadow[i]->node);

	free(d->last_title);
	free(d);
	t->deco = NULL;
}

void kc_deco_hide(struct kc_toplevel *t, bool hide)
{
	struct kc_deco *d = t->deco;
	if (!d || d->hidden == hide)
		return;
	d->hidden = hide;
	for (int i = 0; i < STRIP_N; i++)
		if (d->strip[i])
			wlr_scene_node_set_enabled(&d->strip[i]->node, !hide);
	for (int i = 0; i < 2; i++)
		if (d->shadow[i])
			wlr_scene_node_set_enabled(&d->shadow[i]->node, !hide);
}

void kc_deco_arrange(struct kc_toplevel *t)
{
	struct kc_deco *d = t->deco;
	if (!d || d->hidden)
		return;

	struct kc_server *s = t->server;
	const int scale = kc_deco_scale(t);
	const int bw = kcell_w() * scale, bh = kcell_h() * scale;
	int gw, gh;
	window_size(t, &gw, &gh);

	/*
	 * A shaded window is its titlebar and nothing else. The client is not
	 * resized to do it — it is not told at all — because a shade is a
	 * compositor decision about what is on screen, and asking a client to
	 * be zero pixels tall is a request most of them answer badly.
	 */
	const bool shaded = t->shaded;
	const int gh_eff = shaded ? 0 : gh;

	const char *title = t->xdg_toplevel->title;
	bool focused = t->xdg_toplevel->base->surface ==
		       s->seat->keyboard_state.focused_surface;

	/*
	 * Skip a repaint that would produce identical pixels. `commit` fires on
	 * every frame a client draws — a video player commits sixty times a
	 * second — and repainting the chrome each time would make a still
	 * window cost as much as a moving one.
	 */
	bool same = d->last_w == gw && d->last_h == gh_eff &&
		    d->last_scale == scale && d->last_focused == focused &&
		    ((!title && !d->last_title) ||
		     (title && d->last_title && !strcmp(title, d->last_title)));

	/* Positions are relative to t->scene_tree, whose origin is the window's
	 * own geometry origin — so moving the window moves all of this with no
	 * further work. */
	wlr_scene_node_set_position(&d->strip[STRIP_TOP]->node, -bw, -bh);
	wlr_scene_node_set_position(&d->strip[STRIP_LEFT]->node, -bw, 0);
	wlr_scene_node_set_position(&d->strip[STRIP_RIGHT]->node, gw, 0);
	wlr_scene_node_set_position(&d->strip[STRIP_BOTTOM]->node, -bw, gh_eff);

	wlr_scene_node_set_enabled(&d->strip[STRIP_LEFT]->node, !shaded);
	wlr_scene_node_set_enabled(&d->strip[STRIP_RIGHT]->node, !shaded);

	const int fw = gw + 2 * bw, fh = gh_eff + 2 * bh;
	const int sx = SHADOW_CELLS_X * bw, sy = SHADOW_CELLS_Y * bh;

	/* The shadow is the frame rect translated by (sx, sy) minus the frame
	 * rect: an L, and two rectangles that do not overlap. Overlapping them
	 * would double the alpha along the corner and draw a darker square
	 * there. */
	if (d->shadow[0]) {
		wlr_scene_node_set_position(&d->shadow[0]->node, -bw + fw, -bh + sy);
		wlr_scene_rect_set_size(d->shadow[0], sx, fh);
	}
	if (d->shadow[1]) {
		wlr_scene_node_set_position(&d->shadow[1]->node, -bw + sx, -bh + fh);
		wlr_scene_rect_set_size(d->shadow[1], fw - sx, sy);
	}

	if (same)
		return;

	d->last_w = gw;
	d->last_h = gh_eff;
	d->last_scale = scale;
	d->last_focused = focused;
	free(d->last_title);
	d->last_title = title ? strdup(title) : NULL;

	/* ceil, so the strip covers its whole pixel width and the last cell is
	 * clipped rather than leaving a gap. */
	int top_cols = (fw + bw - 1) / bw;
	int side_rows = (gh_eff + bh - 1) / bh;
	if (top_cols < 2)
		top_cols = 2;
	if (side_rows < 1)
		side_rows = 1;

	KtuiCell *row = calloc((size_t)top_cols, sizeof(*row));
	KtuiCell *col = calloc((size_t)side_rows, sizeof(*col));
	if (!row || !col) {
		free(row);
		free(col);
		return;
	}

	paint_top(row, top_cols, focused, title);
	push_strip(scale, d->strip[STRIP_TOP], row, top_cols, 1, fw, bh);

	paint_bottom(row, top_cols, focused);
	push_strip(scale, d->strip[STRIP_BOTTOM], row, top_cols, 1, fw, bh);

	if (!shaded) {
		paint_side(col, side_rows, focused);
		push_strip(scale, d->strip[STRIP_LEFT], col, 1, side_rows, bw, gh_eff);
		push_strip(scale, d->strip[STRIP_RIGHT], col, 1, side_rows, bw, gh_eff);
	}

	free(row);
	free(col);
}

void kc_deco_refocus(struct kc_toplevel *t)
{
	if (t && t->deco)
		kc_deco_arrange(t);
}

/* ── hit testing ───────────────────────────────────────────────────────── */

enum kc_deco_region kc_deco_at(struct kc_toplevel *t, double lx, double ly)
{
	struct kc_deco *d = t->deco;
	if (!d || d->hidden)
		return KC_DECO_NONE;

	const int scale = kc_deco_scale(t);
	const int bw = kcell_w() * scale, bh = kcell_h() * scale;
	int gw, gh;
	window_size(t, &gw, &gh);
	if (t->shaded)
		gh = 0;

	const int fx = t->x - bw, fy = t->y - bh;
	const int fw = gw + 2 * bw, fh = gh + 2 * bh;

	if (lx < fx || ly < fy || lx >= fx + fw || ly >= fy + fh)
		return KC_DECO_NONE;
	/* Inside the window itself is the client's, not ours. */
	if (lx >= t->x && ly >= t->y && lx < t->x + gw && ly < t->y + gh)
		return KC_DECO_NONE;

	const bool top = ly < fy + bh;
	const bool bottom = ly >= fy + fh - bh;
	const bool left = lx < fx + bw;
	const bool right = lx >= fx + fw - bw;

	if (top) {
		/*
		 * Buttons before corners, and no resize from the top edge at
		 * all. In Turbo Vision the top row IS the titlebar and the only
		 * resize handle is the bottom-right corner; offering an N-edge
		 * resize here would mean a one-cell strip that is a drag target
		 * for two different things.
		 */
		int cols = (fw + bw - 1) / bw;
		int cell = (int)((lx - fx) / bw);
		struct button_layout b = layout_top(cols);
		if (b.close_at >= 0 && cell >= b.close_at && cell <= b.close_at + 2)
			return KC_DECO_CLOSE;
		if (b.min_at >= 0) {
			if (cell >= b.min_at && cell <= b.min_at + 2)
				return KC_DECO_MIN;
			if (cell >= b.max_at && cell <= b.max_at + 2)
				return KC_DECO_MAX;
		}
		return KC_DECO_TITLE;
	}

	if (bottom) {
		if (right)
			return KC_DECO_SE;
		if (left)
			return KC_DECO_SW;
		return KC_DECO_S;
	}
	/* The bottom cell of a side strip belongs to the corner, so the grip is
	 * reachable from either direction rather than only along the bottom. */
	if (left)
		return ly >= fy + fh - 2 * bh ? KC_DECO_SW : KC_DECO_W;
	if (right)
		return ly >= fy + fh - 2 * bh ? KC_DECO_SE : KC_DECO_E;
	return KC_DECO_NONE;
}

uint32_t kc_deco_edges(enum kc_deco_region r)
{
	switch (r) {
	case KC_DECO_W:  return WLR_EDGE_LEFT;
	case KC_DECO_E:  return WLR_EDGE_RIGHT;
	case KC_DECO_S:  return WLR_EDGE_BOTTOM;
	case KC_DECO_SW: return WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
	case KC_DECO_SE: return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	default:         return 0;
	}
}

const char *kc_deco_cursor(enum kc_deco_region r)
{
	switch (r) {
	case KC_DECO_W:  return "w-resize";
	case KC_DECO_E:  return "e-resize";
	case KC_DECO_S:  return "s-resize";
	case KC_DECO_SW: return "sw-resize";
	case KC_DECO_SE: return "se-resize";
	case KC_DECO_TITLE:
	case KC_DECO_CLOSE:
	case KC_DECO_MIN:
	case KC_DECO_MAX: return "default";
	default:          return NULL;
	}
}
