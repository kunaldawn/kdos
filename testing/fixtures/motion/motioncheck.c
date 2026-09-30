/*
 * The compositor's fades (kdos-motion.c) against wlroots' real scene graph.
 *
 * A fade is a handful of alpha writes, and what can go wrong with it is not
 * the arithmetic but the scene: a close fade copies the unmapped surface's
 * buffers into a snapshot that must stand directly above the output's layer
 * tree WITHOUT being inside it (layers.c takes every child of that tree for a
 * layer surface), must keep the buffers alive after the client is gone, must
 * let the pointer through, must leave out subsurfaces that were not showing,
 * and must go with the output. An open fade must end on exactly 1.0, or
 * wlroots never treats the surface as opaque again. A window transition also
 * moves the window's tree, and must follow labwc placing the window again
 * mid-open and end exactly at rest; its snapshot must carry the window's
 * border rectangles as buffers that refuse the pointer, and leave out the
 * decoration that was switched off; and a window shown again while that
 * snapshot still leaves must take it over, or it is drawn twice. None of that is visible to a compiler,
 * and the rig cannot time a 90 ms fade.
 *
 * So this compiles kdos-motion.c itself (#included, so the fade list can be
 * counted) against an installed wlroots, drives the clock by hand, and plays
 * the orders wlroots really uses: the subsurface tree is disabled BEFORE
 * layers.c's unmap listener runs, so every unmap here disables it first.
 *
 *   motioncheck     exit 0 when every check holds, 1 naming each that does not
 *
 * Not shipped. testing/selftest.sh compiles it where wlroots-0.20 is
 * installed and nothing else does.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>

#include "kdos-motion.c"

struct server server;
struct kdos_conf kdos_conf;

/* the fade clock, driven by hand */
static int64_t clk;

int64_t
kdos_frames_now(void)
{
	return clk;
}

void
kdos_peek_tick(int64_t now_ns)
{
	(void)now_ns;
}

bool
output_is_usable(struct output *output)
{
	(void)output;
	return true;
}

static int fails, checks;

#define CHECK(cond, ...) do {						\
	checks++;							\
	if (!(cond)) {							\
		fails++;						\
		printf("FAIL %d: ", __LINE__);				\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)

/* A buffer that says when it is gone: the snapshot's locks are measured by
 * whether its last reference really went. */
struct tbuf {
	struct wlr_buffer base;
	bool *gone;
};

static void
tbuf_destroy(struct wlr_buffer *buffer)
{
	struct tbuf *t = wl_container_of(buffer, t, base);

	*t->gone = true;
	wlr_buffer_finish(buffer);
	free(t);
}

static const struct wlr_buffer_impl tbuf_impl = { .destroy = tbuf_destroy };

/* A scene buffer under `parent` holding the only reference to a new buffer. */
static struct wlr_scene_buffer *
add_buffer(struct wlr_scene_tree *parent, bool *gone)
{
	struct tbuf *t = calloc(1, sizeof(*t));

	*gone = false;
	t->gone = gone;
	wlr_buffer_init(&t->base, &tbuf_impl, 40, 30);
	struct wlr_scene_buffer *sb = wlr_scene_buffer_create(parent, &t->base);
	wlr_buffer_drop(&t->base);
	return sb;
}

/*
 * The shape layers.c and wlroots build for one layer surface:
 *
 *   root ─ below at 105,65                (a buffer under the surface)
 *        ─ layer_tree at 100,50           (an output's layer tree)
 *            └ surf at 10,20              (scene_layer_surface->tree)
 *                └ sub                    (the surface's subsurface tree)
 *                    ├ a                  (the surface's own buffer)
 *                    ├ t2 at 5,7 ─ b      (a showing subsurface, 20x10)
 *                    └ t3 (disabled) ─ c  (a hidden subsurface)
 *        ─ other                          (whatever stacks above)
 */
struct fix {
	struct wlr_scene *scene;
	struct wlr_scene_tree *layer_tree, *surf, *sub, *t2, *t3;
	struct wlr_scene_buffer *a, *b, *c, *below;
	bool ga, gb, gc, gbelow;
};

static void
fix_make(struct fix *f)
{
	f->scene = wlr_scene_create();
	f->below = add_buffer(&f->scene->tree, &f->gbelow);
	wlr_scene_node_set_position(&f->below->node, 105, 65);
	f->layer_tree = wlr_scene_tree_create(&f->scene->tree);
	wlr_scene_node_set_position(&f->layer_tree->node, 100, 50);
	wlr_scene_tree_create(&f->scene->tree);
	f->surf = wlr_scene_tree_create(f->layer_tree);
	wlr_scene_node_set_position(&f->surf->node, 10, 20);
	f->sub = wlr_scene_tree_create(f->surf);
	f->a = add_buffer(f->sub, &f->ga);
	f->t2 = wlr_scene_tree_create(f->sub);
	wlr_scene_node_set_position(&f->t2->node, 5, 7);
	f->b = add_buffer(f->t2, &f->gb);
	wlr_scene_buffer_set_dest_size(f->b, 20, 10);
	f->t3 = wlr_scene_tree_create(f->sub);
	wlr_scene_node_set_enabled(&f->t3->node, false);
	f->c = add_buffer(f->t3, &f->gc);
}

/* wlroots' own unmap listener runs before layers.c's, and disables the
 * surface's subsurface tree; every unmap here does the same first. */
static void
unmap(struct fix *f, bool exclusive)
{
	wlr_scene_node_set_enabled(&f->sub->node, false);
	kdos_motion_layer_unmap(f->surf, true, exclusive);
}

static void
remap(struct fix *f)
{
	wlr_scene_node_set_enabled(&f->sub->node, true);
	kdos_motion_layer_map(f->surf, true);
}

static int
nfades(void)
{
	return wl_list_length(&fades);
}

static int
nchildren(struct wlr_scene_tree *tree)
{
	return wl_list_length(&tree->children);
}

/* The sibling stacked directly above `node`, or NULL. */
static struct wlr_scene_node *
above(struct wlr_scene_node *node)
{
	struct wlr_scene_node *next;

	if (node->link.next == &node->parent->children) {
		return NULL;
	}
	return wl_container_of(node->link.next, next, link);
}

#define MS	1000000LL

/*
 * A window's tree, as labwc and wlroots build it, in a workspace's view tree:
 *
 *   ws ─ under                               (a buffer beneath the window)
 *      ─ view at 200,100                     (view->scene_tree)
 *          ├ ssd
 *          │  ├ act                          (the active decoration)
 *          │  │  ├ border 300x4, a colour    (a scene rectangle)
 *          │  │  └ extents 10x10, invisible  (labwc's grab margin)
 *          │  └ inact (disabled) ─ rect      (the inactive decoration)
 *          └ content at 0,4                  (view->content_tree)
 *              └ vsub                        (the surface's subsurface tree)
 *                  └ vbuf                    (the window's buffer)
 *      ─ next                                (a window stacked above)
 */
struct win {
	struct wlr_scene *scene;
	struct wlr_scene_tree *ws, *view, *content, *vsub, *next;
	struct wlr_scene_rect *border;
	struct wlr_scene_buffer *vbuf, *under;
	bool gv, gu;
};

static const float border_colour[4] = { 0.5f, 0.25f, 0.0f, 0.5f };

static void
win_make(struct win *w)
{
	static const float invisible[4] = { 0, 0, 0, 0 };
	static const float grey[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

	w->scene = wlr_scene_create();
	w->ws = wlr_scene_tree_create(&w->scene->tree);
	w->under = add_buffer(w->ws, &w->gu);
	wlr_scene_node_set_position(&w->under->node, 195, 95);
	w->view = wlr_scene_tree_create(w->ws);
	wlr_scene_node_set_position(&w->view->node, 200, 100);
	struct wlr_scene_tree *ssd = wlr_scene_tree_create(w->view);
	struct wlr_scene_tree *act = wlr_scene_tree_create(ssd);
	w->border = wlr_scene_rect_create(act, 300, 4, border_colour);
	wlr_scene_rect_create(act, 10, 10, invisible);
	struct wlr_scene_tree *inact = wlr_scene_tree_create(ssd);
	wlr_scene_rect_create(inact, 300, 4, grey);
	wlr_scene_node_set_enabled(&inact->node, false);
	w->content = wlr_scene_tree_create(w->view);
	wlr_scene_node_set_position(&w->content->node, 0, 4);
	w->vsub = wlr_scene_tree_create(w->content);
	w->vbuf = add_buffer(w->vsub, &w->gv);
	w->next = wlr_scene_tree_create(w->ws);
}

static void
check_windows(void)
{
	struct win w;
	float mid;

	/* An open: rising 12 px into place while it fades up, following
	 * labwc when it places the window again part way through, and
	 * ending exactly where labwc put it, exactly opaque. */
	kdos_conf.motion = true;
	clk = 10000 * MS;
	win_make(&w);
	CHECK(kdos_motion_tree_open(w.view, 0.0f, 0, 12, 150 * MS),
		"the open did not start");
	CHECK(w.view->node.x == 200 && w.view->node.y == 112,
		"the open starts at %d,%d, not 200,112", w.view->node.x,
		w.view->node.y);
	CHECK(w.vbuf->opacity == 0.0f, "the open starts at %f",
		w.vbuf->opacity);
	clk += 50 * MS;
	kdos_motion_tick();
	CHECK(w.view->node.y > 100 && w.view->node.y < 112
		&& w.vbuf->opacity > 0 && w.vbuf->opacity < 1,
		"not half way at 50 ms: y %d, alpha %f", w.view->node.y,
		w.vbuf->opacity);
	wlr_scene_node_set_position(&w.view->node, 300, 150);
	clk += 20 * MS;
	kdos_motion_tick();
	CHECK(w.view->node.x == 300 && w.view->node.y > 150
		&& w.view->node.y < 162,
		"a window placed mid-open is at %d,%d, not following 300,150",
		w.view->node.x, w.view->node.y);
	clk += 100 * MS;
	kdos_motion_tick();
	CHECK(w.view->node.x == 300 && w.view->node.y == 150,
		"the open ends at %d,%d, not 300,150", w.view->node.x,
		w.view->node.y);
	CHECK(w.vbuf->opacity == 1.0f, "the open ends at %f, not 1",
		w.vbuf->opacity);
	CHECK(nfades() == 0, "the open outlived its end");
	wlr_scene_node_set_position(&w.view->node, 200, 100);

	/* Settled mid-open: at rest and opaque at once, answering the alpha
	 * it had; with nothing running, 1. */
	kdos_motion_tree_open(w.view, 0.0f, 0, 12, 150 * MS);
	clk += 40 * MS;
	kdos_motion_tick();
	mid = kdos_motion_tree_settle(w.view);
	CHECK(mid > 0 && mid < 1, "the settle answered %f", mid);
	CHECK(w.view->node.x == 200 && w.view->node.y == 100
		&& w.vbuf->opacity == 1.0f && nfades() == 0,
		"the settle left %d,%d at %f, %d fades", w.view->node.x,
		w.view->node.y, w.vbuf->opacity, nfades());
	CHECK(kdos_motion_tree_settle(w.view) == 1.0f,
		"a settle with nothing running is not 1");

	/* A close: wlroots has switched the subsurface tree off and labwc
	 * the view's tree; the snapshot copies the window's buffer and its
	 * showing border, stands directly above the view, lets the pointer
	 * through and sinks away. */
	wlr_scene_node_set_enabled(&w.vsub->node, false);
	wlr_scene_node_set_enabled(&w.view->node, false);
	CHECK(kdos_motion_tree_close(w.view, w.content, &w.view->node, true,
		mid, 0, 12, 120 * MS), "the close did not start");
	CHECK(nfades() == 1, "not one close (%d)", nfades());
	if (nfades() != 1) {
		return;
	}
	struct fade *close = wl_container_of(fades.next, close, link);
	struct wlr_scene_tree *snap = close->tree;
	CHECK(snap->node.parent == w.ws && above(&w.view->node) == &snap->node,
		"the snapshot is not directly above the window");
	CHECK(snap->node.x == 200 && snap->node.y == 100,
		"the snapshot is at %d,%d, not 200,100", snap->node.x,
		snap->node.y);
	CHECK(nchildren(snap) == 2, "%d nodes copied, not the border and the "
		"buffer (the inactive border and the invisible extents stay "
		"out)", nchildren(snap));
	struct wlr_scene_node *c0 = wl_container_of(snap->children.next, c0,
		link);
	struct wlr_scene_node *c1 = wl_container_of(c0->link.next, c1, link);
	CHECK(c0->type == WLR_SCENE_NODE_BUFFER
		&& c1->type == WLR_SCENE_NODE_BUFFER,
		"a copy is not a buffer");
	struct wlr_scene_buffer *cb = wlr_scene_buffer_from_node(c0);
	struct wlr_scene_buffer *cv = wlr_scene_buffer_from_node(c1);
	CHECK(cv->buffer == w.vbuf->buffer && c1->x == 0 && c1->y == 4,
		"the window's buffer copy is wrong, or at %d,%d", c1->x, c1->y);
	CHECK(cb->dst_width == 300 && cb->dst_height == 4 && c0->x == 0
		&& c0->y == 0, "the border copy is %dx%d at %d,%d",
		cb->dst_width, cb->dst_height, c0->x, c0->y);
	void *data;
	uint32_t format;
	size_t stride;
	if (cb->buffer && wlr_buffer_begin_data_ptr_access(cb->buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format,
			&stride)) {
		uint32_t px = *(uint32_t *)data;
		wlr_buffer_end_data_ptr_access(cb->buffer);
		CHECK(format == DRM_FORMAT_ARGB8888 && px == 0x80804000u,
			"the border pixel is %08x, not the rectangle's "
			"premultiplied 80804000", px);
	} else {
		CHECK(false, "the border copy has no pixel to read");
	}
	CHECK(cb->opacity == mid && cv->opacity == mid,
		"the close starts at %f/%f, the window was at %f", cb->opacity,
		cv->opacity, mid);
	double sx, sy;
	struct wlr_scene_node *hit = wlr_scene_node_at(&w.scene->tree.node,
		201, 101, &sx, &sy);
	CHECK(hit == &w.under->node,
		"the pointer on the border's snapshot does not reach beneath");
	clk += 60 * MS;
	kdos_motion_tick();
	CHECK(snap->node.y > 100 && snap->node.y < 112
		&& cv->opacity < mid && cv->opacity > 0,
		"not sinking at 60 ms: y %d, alpha %f", snap->node.y,
		cv->opacity);
	clk += 70 * MS;
	kdos_motion_tick();
	CHECK(nfades() == 0 && nchildren(w.ws) == 3,
		"the snapshot outlived its close");

	/* Below an anchor, the way a workspace switch stacks the old
	 * windows under the new workspace; nothing forced, so the window's
	 * own switched-off subsurface tree stays out. */
	wlr_scene_node_set_enabled(&w.view->node, true);
	CHECK(kdos_motion_tree_close(w.view, NULL, &w.next->node, false, 1.0f,
		-48, 0, 150 * MS), "the workspace close did not start");
	snap = NULL;
	if (nfades() == 1) {
		close = wl_container_of(fades.next, close, link);
		snap = close->tree;
	}
	CHECK(snap && above(&snap->node) == &w.next->node,
		"the snapshot is not directly below its anchor");
	CHECK(snap && nchildren(snap) == 1,
		"a switched-off subtree was copied without being forced");
	clk += 200 * MS;
	kdos_motion_tick();
	CHECK(nfades() == 0, "the workspace close outlived its end");

	/* A switch straight back while the ghost still leaves: the window
	 * takes over the ghost's alpha and offset, and the ghost goes, or
	 * the window is drawn twice. Only a snapshot of that window. */
	CHECK(!kdos_motion_tree_reclaim(w.view, &mid, &(int){0}, &(int){0}),
		"a reclaim with no ghost answered one");
	kdos_motion_tree_close(w.view, NULL, &w.next->node, false, 1.0f,
		-48, 0, 150 * MS);
	clk += 40 * MS;
	kdos_motion_tick();
	float ga = -1.0f;
	int gx = 0, gy = 1;
	CHECK(!kdos_motion_tree_reclaim(w.next, &ga, &gx, &gy) && ga == -1.0f,
		"another window's ghost was reclaimed");
	CHECK(kdos_motion_tree_reclaim(w.view, &ga, &gx, &gy),
		"the window's ghost was not reclaimed");
	CHECK(ga > 0 && ga < 1 && gx < 0 && gx > -48 && gy == 0,
		"the ghost answered alpha %f at %d,%d", ga, gx, gy);
	CHECK(nfades() == 0 && nchildren(w.ws) == 3,
		"the reclaimed ghost is still in the scene");

	/* A ghost whose window has gone cannot be reclaimed, and still
	 * fades out. */
	bool gt;
	struct wlr_scene_tree *gone = wlr_scene_tree_create(w.ws);
	add_buffer(gone, &gt);
	kdos_motion_tree_close(gone, NULL, &gone->node, true, 1.0f, 0, 12,
		120 * MS);
	wlr_scene_node_destroy(&gone->node);
	close = nfades() == 1 ? wl_container_of(fades.next, close, link)
		: NULL;
	CHECK(close && close->origin == NULL,
		"a ghost kept the tree of a window that has gone");
	clk += 200 * MS;
	kdos_motion_tick();
	CHECK(nfades() == 0 && gt, "the orphaned ghost outlived its close");
	wlr_scene_node_set_enabled(&w.vsub->node, true);

	/* Nothing to close from nothing, and nothing at all with motion
	 * off: the window is at rest and opaque at once. */
	CHECK(!kdos_motion_tree_close(w.view, NULL, &w.view->node, true,
		0.0f, 0, 12, 120 * MS) && nfades() == 0,
		"a close from alpha 0 started");
	kdos_conf.motion = false;
	CHECK(!kdos_motion_tree_open(w.view, 0.0f, 0, 12, 150 * MS),
		"an open started with motion off");
	CHECK(w.view->node.x == 200 && w.view->node.y == 100
		&& w.vbuf->opacity == 1.0f && nfades() == 0,
		"motion off left the window at %d,%d, %f", w.view->node.x,
		w.view->node.y, w.vbuf->opacity);
	CHECK(!kdos_motion_tree_close(w.view, w.content, &w.view->node, true,
		1.0f, 0, 12, 120 * MS) && nfades() == 0,
		"a close started with motion off");

	/* Motion switched off mid-open: the next tick puts it at rest. */
	kdos_conf.motion = true;
	kdos_motion_tree_open(w.view, 0.0f, 48, 0, 150 * MS);
	kdos_conf.motion = false;
	clk += 1 * MS;
	kdos_motion_tick();
	CHECK(w.view->node.x == 200 && w.view->node.y == 100
		&& w.vbuf->opacity == 1.0f && nfades() == 0,
		"motion off mid-open left %d,%d at %f", w.view->node.x,
		w.view->node.y, w.vbuf->opacity);
	kdos_conf.motion = true;

	/* Teardown mid-open: at rest and opaque. */
	kdos_motion_tree_open(w.view, 0.0f, 0, 12, 150 * MS);
	kdos_motion_finish();
	CHECK(w.view->node.x == 200 && w.view->node.y == 100
		&& w.vbuf->opacity == 1.0f && nfades() == 0,
		"the teardown left the window at %d,%d, %f", w.view->node.x,
		w.view->node.y, w.vbuf->opacity);
	wlr_scene_node_destroy(&w.scene->tree.node);
	CHECK(w.gv && w.gu, "buffers outlived the scene");
}

int
main(void)
{
	struct fix f;

	wlr_log_init(WLR_ERROR, NULL);
	wl_list_init(&server.outputs);
	kdos_conf.motion = true;

	/* An open: from 0, rising, ending on exactly 1. */
	clk = 1000 * MS;
	fix_make(&f);
	kdos_motion_layer_map(f.surf, true);
	CHECK(f.a->opacity == 0 && f.b->opacity == 0 && f.c->opacity == 0,
		"the map sets every buffer to 0 (%f)", f.a->opacity);
	CHECK(nfades() == 1, "one open fade (%d)", nfades());
	float last = 0;
	for (int ms = 10; ms <= 130; ms += 10) {
		clk = 1000 * MS + ms * MS;
		kdos_motion_tick();
		CHECK(f.a->opacity >= last, "falls back at %d ms", ms);
		if (ms < 120) {
			CHECK(f.a->opacity > 0 && f.a->opacity < 1,
				"not between 0 and 1 at %d ms: %f", ms,
				f.a->opacity);
		}
		last = f.a->opacity;
	}
	CHECK(f.a->opacity == 1.0f && f.b->opacity == 1.0f
		&& f.c->opacity == 1.0f, "does not end on exactly 1");
	CHECK(nfades() == 0, "the open fade outlived its end");

	/* Below the toplevels nothing fades. */
	kdos_motion_layer_map(f.surf, false);
	CHECK(nfades() == 0 && f.a->opacity == 1.0f,
		"a bottom-layer surface faded");

	/* An unmap half way through the open: the snapshot. */
	clk = 2000 * MS;
	kdos_motion_layer_map(f.surf, true);
	clk += 60 * MS;
	kdos_motion_tick();
	float mid = f.a->opacity;
	unmap(&f, false);
	CHECK(nfades() == 1, "one close fade (%d)", nfades());
	if (nfades() != 1) {
		printf("motioncheck: %d FAILED\n", fails);
		return 1;
	}
	struct fade *close = wl_container_of(fades.next, close, link);
	struct wlr_scene_tree *snap = close->tree;
	CHECK(close->snapshot, "the close fade is not a snapshot");
	CHECK(snap->node.parent == &f.scene->tree,
		"the snapshot is not a sibling of the layer tree");
	CHECK(nchildren(f.layer_tree) == 1,
		"the layer tree gained a child (%d)", nchildren(f.layer_tree));
	CHECK(above(&f.layer_tree->node) == &snap->node,
		"the snapshot is not stacked directly above the layer tree");
	CHECK(snap->node.x == 110 && snap->node.y == 70,
		"the snapshot is at %d,%d, not 110,70", snap->node.x,
		snap->node.y);
	CHECK(nchildren(snap) == 2,
		"%d buffers copied, not a and b (c was hidden)", nchildren(snap));
	struct wlr_scene_node *n0 = wl_container_of(snap->children.next, n0,
		link);
	struct wlr_scene_node *n1 = wl_container_of(n0->link.next, n1, link);
	struct wlr_scene_buffer *s0 = wlr_scene_buffer_from_node(n0);
	struct wlr_scene_buffer *s1 = wlr_scene_buffer_from_node(n1);
	CHECK(n0->x == 0 && n0->y == 0 && n1->x == 5 && n1->y == 7,
		"copies at %d,%d and %d,%d", n0->x, n0->y, n1->x, n1->y);
	CHECK(s1->dst_width == 20 && s1->dst_height == 10,
		"the destination size was not copied");
	CHECK(s0->buffer == f.a->buffer, "the copy shows another buffer");
	CHECK(s0->opacity == mid && mid > 0 && mid < 1,
		"the close starts at %f, the open was at %f", s0->opacity, mid);
	CHECK(f.a->opacity == 1.0f && f.c->opacity == 1.0f,
		"the unmapped surface was not put back to 1");
	double sx, sy;
	struct wlr_scene_node *hit = wlr_scene_node_at(&f.scene->tree.node,
		112, 72, &sx, &sy);
	CHECK(hit != n0 && hit != n1, "the snapshot takes the pointer");
	CHECK(hit == &f.below->node,
		"the pointer does not reach what is beneath");

	/* The client exits: only the snapshot holds a and b now. */
	wlr_scene_node_destroy(&f.surf->node);
	CHECK(!f.ga && !f.gb, "the snapshot did not keep its buffers");
	CHECK(f.gc, "the hidden subsurface's buffer was kept");
	last = s0->opacity;
	clk += 45 * MS;
	kdos_motion_tick();
	CHECK(s0->opacity < last && s0->opacity > 0,
		"not fading out at 45 ms: %f", s0->opacity);
	clk += 50 * MS;
	kdos_motion_tick();
	CHECK(nfades() == 0, "the close fade outlived its end");
	CHECK(nchildren(&f.scene->tree) == 3,
		"the snapshot is still in the scene");
	CHECK(f.ga && f.gb, "the buffers outlived the fade");
	wlr_scene_node_destroy(&f.scene->tree.node);

	/* An exclusive-keyboard surface (a selection overlay) vanishes at once,
	 * and so does one whose layer was hidden (a fullscreen window disables
	 * the top layer), and everything with motion off. */
	clk = 3000 * MS;
	fix_make(&f);
	unmap(&f, true);
	CHECK(nfades() == 0, "an exclusive surface got a close fade");
	remap(&f);
	clk += 200 * MS;
	kdos_motion_tick();
	wlr_scene_node_set_enabled(&f.layer_tree->node, false);
	unmap(&f, false);
	CHECK(nfades() == 0, "a surface on a hidden layer got a close fade");
	wlr_scene_node_set_enabled(&f.layer_tree->node, true);
	kdos_conf.motion = false;
	remap(&f);
	CHECK(nfades() == 0 && f.a->opacity == 1.0f,
		"an open fade with motion off");
	unmap(&f, false);
	CHECK(nfades() == 0, "a close fade with motion off");

	/* Motion switched off with a close and an open running: both end on
	 * the next tick, at their end states. */
	kdos_conf.motion = true;
	remap(&f);
	clk += 30 * MS;
	kdos_motion_tick();
	unmap(&f, false);
	clk += 1 * MS;
	remap(&f);
	CHECK(nfades() == 2, "not a close and an open (%d)", nfades());
	kdos_conf.motion = false;
	clk += 1 * MS;
	kdos_motion_tick();
	CHECK(nfades() == 0 && f.a->opacity == 1.0f,
		"motion off did not end every fade at its end");
	kdos_conf.motion = true;

	/* The output goes mid-close: its layer tree, and the snapshot. */
	unmap(&f, false);
	CHECK(nfades() == 1, "no close fade (%d)", nfades());
	wlr_scene_node_destroy(&f.layer_tree->node);
	CHECK(nfades() == 0, "the snapshot outlived the output's layer tree");
	CHECK(f.ga && f.gb && f.gc, "buffers outlived the output");
	wlr_scene_node_destroy(&f.scene->tree.node);

	/* Teardown with an open and a close running. */
	fix_make(&f);
	struct wlr_scene_tree *surf2 = wlr_scene_tree_create(f.layer_tree);
	bool g2;
	struct wlr_scene_buffer *b2 = add_buffer(surf2, &g2);
	kdos_motion_layer_map(surf2, true);
	unmap(&f, false);
	CHECK(nfades() == 2, "not two fades running (%d)", nfades());
	kdos_motion_finish();
	CHECK(nfades() == 0 && b2->opacity == 1.0f,
		"the teardown left a fade, or a surface translucent");
	wlr_scene_node_destroy(&f.scene->tree.node);
	CHECK(f.ga && g2, "buffers outlived the scene");

	check_windows();

	/* The curve's two ends. */
	bool done;
	float e = kdos_motion_ease(0, 100, 0.2f, 0.8f, 100, &done);
	CHECK(done && e == 0.8f, "the curve does not end on its target");
	e = kdos_motion_ease(0, 100, 0.2f, 0.8f, -5, &done);
	CHECK(!done && e == 0.2f, "the curve moves before its start (%f)", e);

	if (fails) {
		printf("motioncheck: %d FAILED\n", fails);
	} else {
		printf("motioncheck: %d checks passed\n", checks);
	}
	return fails != 0;
}
