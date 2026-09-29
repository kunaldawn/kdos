// SPDX-License-Identifier: GPL-2.0-only
/*
 * MOTION — the compositor's fades: a menu, a toast, a tooltip or an OSD on the
 * top or overlay layer fades in when it maps and out when it unmaps, peek
 * (kdos-peek.c) eases instead of jumping, and, with `window_motion`, windows
 * fade and slide as they open, close, minimise and change workspace
 * (kdos-winmotion.c, which drives the tree fades below).
 *
 * It lives here rather than in the client because here it costs the client
 * nothing: the surface commits its first buffer once, and the compositor
 * re-blends that buffer at a rising alpha for a few frames. No client repaints,
 * no client clock, no cell changes — so tty1, --dump and every golden are
 * untouched, and a surface that knows nothing about any of this fades anyway.
 *
 * ONE CLOCK. kdos_motion_tick() runs at the top of every output frame, before
 * the frame is built, and moves every fade to where the wall clock says it is
 * now. A fade is a function of time, not of how many frames were drawn, so a
 * dropped frame is a skipped step rather than a slower fade. Setting an alpha
 * or a position damages the node's area, and that damage is what asks for the
 * next frame; when the last fade ends, nothing is damaged and the output goes
 * idle again.
 *
 * `motion = no` in comp.conf ends every running fade at its end state on the
 * next tick and starts none: a surface appears and vanishes in one frame, as
 * it would with no compositor-side motion at all.
 *
 * OCCLUSION. wlroots treats a buffer as opaque only while its opacity is
 * exactly 1, so a fading surface hides nothing beneath it and everything under
 * it is drawn for as long as it fades. Every fade therefore ends on the exact
 * value it was heading for — 1.0f, never 0.999 — or the surface would cost that
 * blend for the rest of its life. A fade that moves its node ends on the exact
 * resting position for the same kind of reason: an offset left behind is a
 * window drawn a few pixels away from where input and placement think it is.
 */

#include <assert.h>
#include <drm_fourcc.h>
#include <math.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

#include "labwc.h"
#include "output.h"
#include "kdos.h"

/*
 * Durations. An open is long enough to be seen and short enough that nobody
 * waits for it: a menu is readable from its first frame and fully there in
 * about eight frames at 60 Hz. A close is shorter, because what closes is
 * already dismissed and anything longer reads as lag.
 */
#define MOTION_OPEN_NS	(120 * 1000000LL)
#define MOTION_CLOSE_NS	(90 * 1000000LL)

struct fade {
	struct wl_list link;			/* fades */
	struct wlr_scene_tree *tree;
	float from, to;
	int64_t start_ns, dur_ns;
	/*
	 * A close fade's tree is a SNAPSHOT this file built and owns: copies
	 * of the unmapped surface's last buffers, each holding its own lock on
	 * the buffer, so the picture outlives the client. It is destroyed at
	 * the end of the fade, or with the tree it was placed in, or with the
	 * output's layer tree it stands above (`layer_destroy`).
	 */
	bool snapshot;
	/*
	 * A fade that also MOVES its tree: the offset runs from (ox0,oy0) to
	 * (ox1,oy1) on the same curve as the alpha. The resting position is
	 * not stored, because the tree's owner may move it mid-fade (labwc
	 * places a window again after it maps): (wx,wy) is the position this
	 * file last wrote and (cdx,cdy) the offset in it, so a tree found
	 * anywhere else has been moved by its owner, and where it was put IS
	 * the new resting position.
	 */
	bool moves;
	int ox0, oy0, ox1, oy1;
	int wx, wy, cdx, cdy;
	/*
	 * The window tree a close snapshot was taken of, or NULL. A window
	 * shown again while its snapshot still fades takes the snapshot's
	 * alpha and offset over (kdos_motion_tree_reclaim), or it would be
	 * drawn twice: once opening, once as its own ghost leaving.
	 */
	struct wlr_scene_tree *origin;
	struct wl_listener destroy;
	struct wl_listener layer_destroy;
	struct wl_listener origin_destroy;
};

static struct wl_list fades = { &fades, &fades };

float
kdos_motion_ease(int64_t start_ns, int64_t dur_ns, float from, float to,
		int64_t now_ns, bool *done)
{
	double t = dur_ns > 0 ? (double)(now_ns - start_ns) / dur_ns : 1.0;

	if (!kdos_conf.motion || t >= 1.0) {
		*done = true;
		return to;
	}
	*done = false;
	if (t < 0.0) {
		t = 0.0;
	}
	/* Ease out (cubic): most of the change in the first frames, so the
	 * surface is legible at once and settles rather than arrives. */
	double u = 1.0 - t;
	return from + (to - from) * (float)(1.0 - u * u * u);
}

/*
 * EVERY buffer under `node`, enabled or not. wlr_scene_node_for_each_buffer()
 * skips disabled subtrees, and two of this file's callers need the ones it
 * would skip: a layer surface's tree is already disabled by the time its
 * unmap listener runs, and a view on another workspace sits in a disabled
 * tree while peek fades the rest. An alpha on a hidden buffer is a number the
 * next showing uses; an alpha left behind on one is a surface that comes back
 * translucent.
 */
void
kdos_motion_set_alpha(struct wlr_scene_node *node, float alpha)
{
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		wlr_scene_buffer_set_opacity(wlr_scene_buffer_from_node(node),
			alpha);
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			kdos_motion_set_alpha(child, alpha);
		}
	}
}

/* Frames on every output: a fade that damaged nothing yet (peek, whose first
 * step equals where it starts) still needs its first tick. */
void
kdos_motion_kick(void)
{
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		if (output_is_usable(output)) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

static void
fade_free(struct fade *f)
{
	wl_list_remove(&f->link);
	wl_list_remove(&f->destroy.link);
	wl_list_remove(&f->layer_destroy.link);
	wl_list_remove(&f->origin_destroy.link);
	free(f);
}

/*
 * Put a moving fade's tree at its resting position plus (dx,dy). The resting
 * position is where the tree's owner last put it: where this file left it
 * minus the offset this file added, unless the owner has moved it since.
 */
static void
fade_offset(struct fade *f, int dx, int dy)
{
	struct wlr_scene_node *node = &f->tree->node;
	int bx = node->x, by = node->y;

	if (node->x == f->wx && node->y == f->wy) {
		bx -= f->cdx;
		by -= f->cdy;
	}
	f->wx = bx + dx;
	f->wy = by + dy;
	f->cdx = dx;
	f->cdy = dy;
	wlr_scene_node_set_position(node, f->wx, f->wy);
}

/*
 * End a live fade where it stands without finishing it: the tree goes back
 * to its resting position (its alpha is the caller's to set). Every path that
 * drops a fade early goes through here, or a window stays offset.
 */
static void
fade_drop(struct fade *f)
{
	if (f->moves && !f->snapshot) {
		fade_offset(f, 0, 0);
	}
	fade_free(f);
}

/* The output went, and its layer trees with it: the snapshot standing above
 * one goes too, or it would be left on whatever output takes its place in
 * the layout. Destroying the tree frees `f` through its destroy listener. */
static void
handle_layer_tree_destroy(struct wl_listener *listener, void *data)
{
	struct fade *f = wl_container_of(listener, f, layer_destroy);

	(void)data;
	wlr_scene_node_destroy(&f->tree->node);
}

/* The window went while its snapshot fades: nothing can reclaim it now. */
static void
handle_origin_destroy(struct wl_listener *listener, void *data)
{
	struct fade *f = wl_container_of(listener, f, origin_destroy);

	(void)data;
	f->origin = NULL;
	wl_list_remove(&f->origin_destroy.link);
	wl_list_init(&f->origin_destroy.link);
}

static void
handle_fade_tree_destroy(struct wl_listener *listener, void *data)
{
	struct fade *f = wl_container_of(listener, f, destroy);

	(void)data;
	fade_free(f);
}

static struct fade *
fade_find(struct wlr_scene_tree *tree)
{
	struct fade *f;
	wl_list_for_each(f, &fades, link) {
		if (f->tree == tree) {
			return f;
		}
	}
	return NULL;
}

static struct fade *
fade_start(struct wlr_scene_tree *tree, float from, float to, int64_t dur_ns,
		bool snapshot)
{
	struct fade *f = calloc(1, sizeof(*f));
	if (!f) {
		return NULL;
	}
	f->tree = tree;
	f->from = from;
	f->to = to;
	f->start_ns = kdos_frames_now();
	f->dur_ns = dur_ns;
	f->snapshot = snapshot;
	f->wx = tree->node.x;
	f->wy = tree->node.y;
	f->destroy.notify = handle_fade_tree_destroy;
	wl_signal_add(&tree->node.events.destroy, &f->destroy);
	wl_list_init(&f->layer_destroy.link);	/* a close adds it */
	wl_list_init(&f->origin_destroy.link);	/* a window close adds it */
	wl_list_insert(&fades, &f->link);
	return f;
}

/* Make `f` move its tree from offset (ox0,oy0) to (ox1,oy1); the tree is put
 * at the first one now, so the frame that shows the first alpha shows the
 * first position too. */
static void
fade_move(struct fade *f, int ox0, int oy0, int ox1, int oy1)
{
	if (ox0 == 0 && oy0 == 0 && ox1 == 0 && oy1 == 0) {
		return;
	}
	f->moves = true;
	f->ox0 = ox0;
	f->oy0 = oy0;
	f->ox1 = ox1;
	f->oy1 = oy1;
	fade_offset(f, ox0, oy0);
}

/*
 * Where a fade is at `now_ns`: its alpha, with *p the eased progress the
 * offset follows. The end is the exact end value, not from + (to - from) * 1,
 * which a float may round to a hair short of it.
 */
static float
fade_at(struct fade *f, int64_t now_ns, float *p, bool *done)
{
	*p = kdos_motion_ease(f->start_ns, f->dur_ns, 0.0f, 1.0f, now_ns,
		done);
	return *done ? f->to : f->from + (f->to - f->from) * *p;
}

/* The alpha a fade shows now, without advancing it. */
static float
fade_now(struct fade *f)
{
	float p;
	bool done;
	return fade_at(f, kdos_frames_now(), &p, &done);
}

void
kdos_motion_layer_map(struct wlr_scene_tree *tree, bool above_toplevels)
{
	/* An unmap ends the tree's open fade, so none should be running;
	 * a stale one would fight this one for the alpha. */
	struct fade *f = fade_find(tree);
	if (f) {
		fade_drop(f);
	}
	/* With motion off there is nothing to do: the unmap left every
	 * buffer of this tree at 1. */
	if (!above_toplevels || !kdos_conf.motion) {
		return;
	}
	if (fade_start(tree, 0.0f, 1.0f, MOTION_OPEN_NS, false)) {
		kdos_motion_set_alpha(&tree->node, 0.0f);
	}
}

/*
 * A snapshot is a picture, not a surface: the pointer passes through it to
 * whatever is beneath, so a click in the fade's last frames lands where the
 * person sees the desktop becoming, not on something already closed.
 */
static bool
snapshot_refuses_input(struct wlr_scene_buffer *buffer, double *sx, double *sy)
{
	(void)buffer;
	(void)sx;
	(void)sy;
	return false;
}

/*
 * One premultiplied ARGB pixel, stretched by the scene to a rectangle's size.
 * A snapshot draws a window's border rectangles with these rather than with
 * scene rectangles, because a scene rectangle takes the pointer (only a
 * buffer can refuse it) and has no opacity of its own to fade.
 */
struct px_buffer {
	struct wlr_buffer base;
	uint32_t px;
};

static void
px_destroy(struct wlr_buffer *buffer)
{
	struct px_buffer *p = wl_container_of(buffer, p, base);

	wlr_buffer_finish(buffer);
	free(p);
}

static bool
px_begin(struct wlr_buffer *buffer, uint32_t flags, void **data,
		uint32_t *format, size_t *stride)
{
	struct px_buffer *p = wl_container_of(buffer, p, base);

	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = &p->px;
	*format = DRM_FORMAT_ARGB8888;
	*stride = sizeof(p->px);
	return true;
}

static void
px_end(struct wlr_buffer *buffer)
{
	(void)buffer;
}

static const struct wlr_buffer_impl px_impl = {
	.destroy = px_destroy,
	.begin_data_ptr_access = px_begin,
	.end_data_ptr_access = px_end,
};

static uint32_t
px_channel(float v)
{
	v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
	return (uint32_t)lroundf(v * 255.0f);
}

/* A snapshot's copy of a scene rectangle. wlroots' rectangle colours are
 * premultiplied, and so is ARGB8888 in a buffer, so the channels copy over
 * as they are. A fully transparent rectangle (labwc's invisible grab
 * extents) is left out. */
static struct wlr_scene_buffer *
snapshot_rect(struct wlr_scene_tree *snap, struct wlr_scene_rect *rect)
{
	if (rect->width <= 0 || rect->height <= 0 || rect->color[3] <= 0.0f) {
		return NULL;
	}
	struct px_buffer *p = calloc(1, sizeof(*p));
	if (!p) {
		return NULL;
	}
	wlr_buffer_init(&p->base, &px_impl, 1, 1);
	p->px = px_channel(rect->color[3]) << 24
		| px_channel(rect->color[0]) << 16
		| px_channel(rect->color[1]) << 8
		| px_channel(rect->color[2]);
	struct wlr_scene_buffer *b = wlr_scene_buffer_create(snap, &p->base);
	/* the scene buffer holds the one lock that is left */
	wlr_buffer_drop(&p->base);
	if (!b) {
		return NULL;
	}
	wlr_scene_buffer_set_dest_size(b, rect->width, rect->height);
	wlr_scene_buffer_set_filter_mode(b, WLR_SCALE_FILTER_NEAREST);
	return b;
}

/*
 * Copy what `node` shows into `snap`, flattened, at the position it had
 * relative to the tree being snapshotted. A disabled node was not on screen
 * and is left out — except `forced` and its direct children, which are walked
 * whatever their state: a surface's own subsurface tree has been switched off
 * by wlroots before the unmap hook runs (its unmap listener was registered
 * first), while the subsurfaces under it are still in the state they were
 * shown in, because wlroots unmaps them after the parent's unmap signal.
 */
static void
snapshot_copy(struct wlr_scene_tree *snap, struct wlr_scene_node *node,
		int x, int y, struct wlr_scene_tree *forced, float alpha)
{
	bool force = forced && (node == &forced->node
		|| node->parent == forced);
	if (!node->enabled && !force) {
		return;
	}
	x += node->x;
	y += node->y;
	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			snapshot_copy(snap, child, x, y, forced, alpha);
		}
		return;
	}
	struct wlr_scene_buffer *b = NULL;
	if (node->type == WLR_SCENE_NODE_RECT) {
		b = snapshot_rect(snap, wlr_scene_rect_from_node(node));
	} else if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *src = wlr_scene_buffer_from_node(node);
		if (!src->buffer) {
			return;
		}
		/* wlr_scene_buffer_create() takes its own lock on the
		 * buffer: the client may destroy the surface, and exit,
		 * before the fade ends. */
		b = wlr_scene_buffer_create(snap, src->buffer);
		if (!b) {
			return;
		}
		wlr_scene_buffer_set_source_box(b, &src->src_box);
		wlr_scene_buffer_set_dest_size(b, src->dst_width,
			src->dst_height);
		wlr_scene_buffer_set_transform(b, src->transform);
		wlr_scene_buffer_set_filter_mode(b, src->filter_mode);
		wlr_scene_buffer_set_transfer_function(b,
			src->transfer_function);
		wlr_scene_buffer_set_primaries(b, src->primaries);
		wlr_scene_buffer_set_color_encoding(b, src->color_encoding);
		wlr_scene_buffer_set_color_range(b, src->color_range);
	}
	if (!b) {
		return;
	}
	wlr_scene_node_set_position(&b->node, x, y);
	wlr_scene_buffer_set_opacity(b, alpha);
	b->point_accepts_input = snapshot_refuses_input;
}

/*
 * A snapshot of `src` in `anchor`'s parent, stacked directly above or below
 * `anchor`, fading from `from` to nothing while it moves by (dx,dy). NULL
 * when nothing of `src` was showing, or on allocation failure.
 */
static struct fade *
snapshot_fade(struct wlr_scene_tree *src, struct wlr_scene_tree *forced,
		struct wlr_scene_node *anchor, bool above, float from,
		int dx, int dy, int64_t dur_ns)
{
	struct wlr_scene_tree *parent = anchor->parent;
	if (!parent) {
		return NULL;
	}
	struct wlr_scene_tree *snap = wlr_scene_tree_create(parent);
	if (!snap) {
		return NULL;
	}
	int sx = 0, sy = 0, px = 0, py = 0;
	wlr_scene_node_coords(&src->node, &sx, &sy);
	wlr_scene_node_coords(&parent->node, &px, &py);
	wlr_scene_node_set_position(&snap->node, sx - px, sy - py);
	if (above) {
		wlr_scene_node_place_above(&snap->node, anchor);
	} else {
		wlr_scene_node_place_below(&snap->node, anchor);
	}
	struct wlr_scene_node *child;
	wl_list_for_each(child, &src->children, link) {
		snapshot_copy(snap, child, 0, 0, forced, from);
	}
	struct fade *f = NULL;
	if (!wl_list_empty(&snap->children)) {
		f = fade_start(snap, from, 0.0f, dur_ns, true);
	}
	if (!f) {
		wlr_scene_node_destroy(&snap->node);
		return NULL;
	}
	fade_move(f, 0, 0, dx, dy);
	return f;
}

void
kdos_motion_layer_unmap(struct wlr_scene_tree *tree, bool above_toplevels,
		bool exclusive_keyboard)
{
	float from = 1.0f;
	struct fade *f = fade_find(tree);
	if (f) {
		from = fade_now(f);
		fade_drop(f);
	}

	/*
	 * No close fade for a surface that held the keyboard EXCLUSIVELY. That
	 * is a selection overlay (slurp, which kdos-shot runs before grim) or a
	 * picker, and what follows it is usually a capture of the screen: a
	 * ghost of the dimmed overlay fading out would be in the screenshot.
	 */
	struct wlr_scene_tree *layer_tree = tree->node.parent;
	/*
	 * Nor for one that was not on screen: labwc disables an output's whole
	 * top layer under a fullscreen window, and a snapshot is not inside
	 * that layer tree, so it would show a ghost of something nobody saw.
	 */
	int lx = 0, ly = 0;
	bool shown = wlr_scene_node_coords(&tree->node, &lx, &ly);
	bool fade = above_toplevels && !exclusive_keyboard && kdos_conf.motion
		&& shown && from > 0.0f && layer_tree && layer_tree->node.parent;
	if (fade) {
		/*
		 * NOT inside the output's layer tree: every child of that is
		 * taken for a layer surface (arrange_one_layer() and the focus
		 * search in layers.c look each one up as one), and the unmap
		 * handler arranges the layer right after this returns. A
		 * sibling of the layer tree, stacked directly above it, draws
		 * in the same place in the stack.
		 */
		struct fade *close = snapshot_fade(tree, tree,
			&layer_tree->node, true, from, 0, 0, MOTION_CLOSE_NS);
		if (close) {
			close->layer_destroy.notify = handle_layer_tree_destroy;
			wl_signal_add(&layer_tree->node.events.destroy,
				&close->layer_destroy);
		}
	}

	/* The surface itself goes back to 1 while it is hidden, so a remap
	 * starts from a known alpha whatever `motion` says by then. */
	kdos_motion_set_alpha(&tree->node, 1.0f);
}

bool
kdos_motion_tree_open(struct wlr_scene_tree *tree, float from, int dx, int dy,
		int64_t dur_ns)
{
	struct fade *f = fade_find(tree);
	if (f) {
		fade_drop(f);
	}
	if (!kdos_conf.motion) {
		kdos_motion_set_alpha(&tree->node, 1.0f);
		return false;
	}
	f = fade_start(tree, from, 1.0f, dur_ns, false);
	if (!f) {
		kdos_motion_set_alpha(&tree->node, 1.0f);
		return false;
	}
	kdos_motion_set_alpha(&tree->node, from);
	fade_move(f, dx, dy, 0, 0);
	kdos_motion_kick();
	return true;
}

float
kdos_motion_tree_settle(struct wlr_scene_tree *tree)
{
	struct fade *f = fade_find(tree);
	if (!f || f->snapshot) {
		return 1.0f;
	}
	float shown = fade_now(f);
	fade_drop(f);
	kdos_motion_set_alpha(&tree->node, 1.0f);
	return shown;
}

bool
kdos_motion_tree_close(struct wlr_scene_tree *src, struct wlr_scene_tree *forced,
		struct wlr_scene_node *anchor, bool above, float from,
		int dx, int dy, int64_t dur_ns)
{
	if (!kdos_conf.motion || from <= 0.0f) {
		return false;
	}
	struct fade *f = snapshot_fade(src, forced, anchor, above, from, dx,
		dy, dur_ns);
	if (!f) {
		return false;
	}
	f->origin = src;
	f->origin_destroy.notify = handle_origin_destroy;
	wl_signal_add(&src->node.events.destroy, &f->origin_destroy);
	kdos_motion_kick();
	return true;
}

bool
kdos_motion_tree_reclaim(struct wlr_scene_tree *src, float *alpha, int *dx,
		int *dy)
{
	bool found = false;
	struct fade *f, *tmp;
	/* newest first: the one taken last is the one showing on top */
	wl_list_for_each_safe(f, tmp, &fades, link) {
		if (!f->snapshot || f->origin != src) {
			continue;
		}
		if (!found) {
			float p;
			bool done;
			*alpha = fade_at(f, kdos_frames_now(), &p, &done);
			*dx = f->moves ? (int)lroundf(f->ox0
				+ (f->ox1 - f->ox0) * p) : 0;
			*dy = f->moves ? (int)lroundf(f->oy0
				+ (f->oy1 - f->oy0) * p) : 0;
			found = true;
		}
		/* the destroy listener frees `f` */
		wlr_scene_node_destroy(&f->tree->node);
	}
	return found;
}

void
kdos_motion_tick(void)
{
	int64_t now = kdos_frames_now();

	kdos_peek_tick(now);

	struct fade *f, *tmp;
	wl_list_for_each_safe(f, tmp, &fades, link) {
		bool done;
		float p;
		float alpha = fade_at(f, now, &p, &done);
		if (done && f->snapshot) {
			/* the destroy listener frees `f` */
			wlr_scene_node_destroy(&f->tree->node);
			continue;
		}
		kdos_motion_set_alpha(&f->tree->node, alpha);
		if (f->moves) {
			fade_offset(f, done ? f->ox1 : (int)lroundf(f->ox0
				+ (f->ox1 - f->ox0) * p), done ? f->oy1
				: (int)lroundf(f->oy0 + (f->oy1 - f->oy0) * p));
		}
		if (done) {
			fade_free(f);
		}
	}
}

/*
 * Teardown, before the scene goes: every snapshot destroyed (each holds a
 * buffer lock) and every surface mid-fade left opaque and at rest.
 */
void
kdos_motion_finish(void)
{
	struct fade *f, *tmp;
	wl_list_for_each_safe(f, tmp, &fades, link) {
		if (f->snapshot) {
			wlr_scene_node_destroy(&f->tree->node);
		} else {
			kdos_motion_set_alpha(&f->tree->node, 1.0f);
			fade_drop(f);
		}
	}
	assert(wl_list_empty(&fades));
}
