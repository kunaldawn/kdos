// SPDX-License-Identifier: GPL-2.0-only
/*
 * The desktop background, ported from the pre-fork kdos-comp.
 *
 * labwc has no wallpaper — the usual answer is swaybg, and it is the
 * wrong one here: libkwl paints CELLS, so a wallpaper client would be
 * the one program in this desktop that is not a character grid. The
 * compositor owns a scene graph; the image goes straight in.
 *
 * Three rules carried over, each measured once:
 * - There is no public "wlr_buffer from memory", so this file carries
 *   the smallest wlr_buffer_impl that works: data-ptr access only,
 *   which is what both the GLES2 and pixman renderers use to upload.
 * - One node per OUTPUT, not one for the layout: wlr_scene_buffer
 *   scales to one destination size and two monitors differ.
 * - libpng reports a bad file by longjmp'ing back AFTER the allocation,
 *   so only an initialised buffer may leave decode_png().
 *
 * The nodes live in the scene ROOT (not a per-output tree) and are
 * re-lowered to the bottom on every arrange — handle_new_output()
 * lowers each output's background layer_tree to the bottom after us,
 * and the arrange that follows it through do_output_layout_change()
 * puts the wallpaper back underneath.
 */
#include <drm_fourcc.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "kdos.h"
#include "labwc.h"
#include "output.h"

struct kdos_wp_buffer {
	struct wlr_buffer base;
	uint32_t *data;			/* ARGB8888, premultiplied (opaque) */
	size_t stride;
};

static struct kdos_wp_buffer *wp_buf;

struct kdos_wp_node {
	struct wl_list link;
	struct wlr_scene_buffer *scene_buffer;
};
static struct wl_list wp_nodes = { .prev = &wp_nodes, .next = &wp_nodes };

static void
wp_buffer_destroy(struct wlr_buffer *b)
{
	struct kdos_wp_buffer *w = (struct kdos_wp_buffer *)b;
	wlr_buffer_finish(b);
	free(w->data);
	free(w);
}

static bool
wp_begin_data_ptr_access(struct wlr_buffer *b, uint32_t flags, void **data,
		uint32_t *format, size_t *stride)
{
	struct kdos_wp_buffer *w = (struct kdos_wp_buffer *)b;
	/* read-only: decoded once, never written again */
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = w->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = w->stride;
	return true;
}

static void
wp_end_data_ptr_access(struct wlr_buffer *b)
{
	(void)b;
}

static const struct wlr_buffer_impl wp_buffer_impl = {
	.destroy = wp_buffer_destroy,
	.begin_data_ptr_access = wp_begin_data_ptr_access,
	.end_data_ptr_access = wp_end_data_ptr_access,
};

/*
 * libpng with every transform asked for up front: 8-bit RGBA whatever
 * the file is, so there is one pixel layout below this call.
 */
static struct kdos_wp_buffer *
decode_png(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;
	/* volatile: assigned between setjmp and a possible longjmp, read
	 * after — without it the cleanup below may see stale registers */
	struct kdos_wp_buffer *volatile w = NULL;
	png_bytep *volatile rows = NULL;
	volatile int ok = 0;

	if (!png || !info || setjmp(png_jmpbuf(png))) {
		goto out;
	}

	png_init_io(png, f);
	png_read_info(png, info);

	png_uint_32 iw = 0, ih = 0;
	int depth = 0, ctype = 0;
	png_get_IHDR(png, info, &iw, &ih, &depth, &ctype, NULL, NULL, NULL);
	if (iw == 0 || ih == 0 || iw > 16384 || ih > 16384) {
		goto out;
	}

	if (ctype == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}
	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png);
	}
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}
	if (depth == 16) {
		png_set_strip_16(png);
	}
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png);
	}
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);

	w = calloc(1, sizeof(*w));
	if (!w) {
		goto out;
	}
	w->stride = (size_t)iw * 4;
	w->data = malloc(w->stride * ih);
	rows = malloc(sizeof(png_bytep) * ih);
	if (!w->data || !rows) {
		free(w->data);
		free(w);
		w = NULL;
		goto out;
	}
	for (png_uint_32 y = 0; y < ih; y++) {
		rows[y] = (png_bytep)((char *)w->data + y * w->stride);
	}
	png_read_image(png, rows);

	/*
	 * libpng hands back R,G,B,A in memory order; DRM_FORMAT_ARGB8888
	 * is a little-endian word wanting B,G,R,A. Swap in place.
	 */
	for (png_uint_32 y = 0; y < ih; y++) {
		uint8_t *p = (uint8_t *)rows[y];
		for (png_uint_32 x = 0; x < iw; x++, p += 4) {
			uint8_t r = p[0];
			p[0] = p[2];
			p[2] = r;
		}
	}

	wlr_buffer_init(&w->base, &wp_buffer_impl, (int)iw, (int)ih);
	ok = 1;

out:
	free(rows);
	if (png) {
		png_destroy_read_struct(&png, info ? &info : NULL, NULL);
	}
	fclose(f);
	/* only an initialised buffer may leave — see header comment */
	if (!ok && w) {
		free(w->data);
		free(w);
		w = NULL;
	}
	return w;
}

/* Cover: scale by the LARGER ratio and centre; overhang is cropped. */
static void
cover_src_box(int iw, int ih, int ow, int oh, struct wlr_fbox *src)
{
	double sx = (double)ow / iw, sy = (double)oh / ih;
	double s = sx > sy ? sx : sy;
	double vw = ow / s, vh = oh / s;
	src->x = (iw - vw) / 2;
	src->y = (ih - vh) / 2;
	src->width = vw;
	src->height = vh;
}

void
kdos_wallpaper_init(void)
{
	const char *path = kdos_conf.wallpaper;
	/* `wallpaper = none` is an honest off, not a missing file */
	if (!*path || !strcmp(path, "none")) {
		wlr_log(WLR_INFO, "wallpaper off (comp.conf)");
		return;
	}
	wp_buf = decode_png(path);
	if (!wp_buf) {
		wlr_log(WLR_INFO, "no wallpaper at %s — the desktop "
			"background is the theme colour", path);
		return;
	}
	wlr_log(WLR_INFO, "wallpaper %s (%dx%d)", path,
		wp_buf->base.width, wp_buf->base.height);
}

/*
 * Rebuilt rather than adjusted on every layout change — a scene
 * buffer's destination is its geometry and nothing is cheaper to
 * update. Runs from output_update_for_layout_change(), which follows
 * every output add, remove and move.
 */
void
kdos_wallpaper_arrange(void)
{
	if (!wp_buf || !server.scene) {
		return;
	}

	struct kdos_wp_node *n, *tmp;
	wl_list_for_each_safe(n, tmp, &wp_nodes, link) {
		wlr_scene_node_destroy(&n->scene_buffer->node);
		wl_list_remove(&n->link);
		free(n);
	}

	struct output *o;
	wl_list_for_each(o, &server.outputs, link) {
		struct wlr_box box = { 0 };
		wlr_output_layout_get_box(server.output_layout,
			o->wlr_output, &box);
		if (wlr_box_empty(&box)) {
			continue;
		}

		struct wlr_scene_buffer *sb = wlr_scene_buffer_create(
			&server.scene->tree, &wp_buf->base);
		if (!sb) {
			continue;
		}
		/* under everything, including background layer clients */
		wlr_scene_node_lower_to_bottom(&sb->node);

		struct wlr_fbox src;
		cover_src_box(wp_buf->base.width, wp_buf->base.height,
			box.width, box.height, &src);
		wlr_scene_buffer_set_source_box(sb, &src);
		wlr_scene_buffer_set_dest_size(sb, box.width, box.height);
		wlr_scene_node_set_position(&sb->node, box.x, box.y);

		n = calloc(1, sizeof(*n));
		if (!n) {
			wlr_scene_node_destroy(&sb->node);
			continue;
		}
		n->scene_buffer = sb;
		wl_list_insert(&wp_nodes, &n->link);
	}
}

void
kdos_wallpaper_finish(void)
{
	struct kdos_wp_node *n, *tmp;
	wl_list_for_each_safe(n, tmp, &wp_nodes, link) {
		wl_list_remove(&n->link);
		free(n);
	}
	wl_list_init(&wp_nodes);
	if (wp_buf) {
		/* scene nodes held their own refs; dropping ours frees it */
		wlr_buffer_drop(&wp_buf->base);
		wp_buf = NULL;
	}
}
