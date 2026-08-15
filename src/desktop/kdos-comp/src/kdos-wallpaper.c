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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "kcolor.h"
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
	struct wlr_scene_buffer *scene_buffer;	/* NULL for a rect-only output */
	struct wlr_scene_rect *rect;
};
static struct wl_list wp_nodes = { .prev = &wp_nodes, .next = &wp_nodes };

/* what the current wp_buf was decoded from, for the SIGHUP reload */
static char wp_src[512];
static time_t wp_mtime;

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

/*
 * The image to decode: `kdos theme` retints the shipped wallpaper into
 * $XDG_CACHE_HOME/kdos/wallpaper.png, and that file — the accent the
 * user actually chose — beats the fixed comp.conf path whenever it
 * exists. `wallpaper = none` stays an honest off and is never
 * overridden by the cache: the user said none.
 * Returns false when the wallpaper is off; *mtime is 0 when the chosen
 * file cannot be statted.
 */
static bool
wp_source(char *buf, size_t len, time_t *mtime)
{
	*mtime = 0;
	const char *conf = kdos_conf.wallpaper;
	if (!*conf || !strcmp(conf, "none")) {
		return false;
	}

	char cachef[512];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	cachef[0] = '\0';
	if (cache && *cache) {
		snprintf(cachef, sizeof(cachef), "%s/kdos/wallpaper.png", cache);
	} else if (home && *home) {
		snprintf(cachef, sizeof(cachef), "%s/.cache/kdos/wallpaper.png",
			home);
	}

	struct stat st;
	if (cachef[0] && stat(cachef, &st) == 0) {
		snprintf(buf, len, "%s", cachef);
		*mtime = st.st_mtime;
		return true;
	}
	snprintf(buf, len, "%s", conf);
	if (stat(buf, &st) == 0) {
		*mtime = st.st_mtime;
	}
	return true;
}

void
kdos_wallpaper_init(void)
{
	char path[512];
	time_t mtime;
	if (!wp_source(path, sizeof(path), &mtime)) {
		/* an honest off, not a missing file — the deep rects from
		 * kdos_wallpaper_arrange() are the background */
		wlr_log(WLR_INFO, "wallpaper off (comp.conf) — the desktop "
			"background is the accent's deep colour");
		return;
	}
	wp_buf = decode_png(path);
	if (!wp_buf) {
		wlr_log(WLR_INFO, "no wallpaper at %s — the desktop "
			"background is the accent's deep colour", path);
		return;
	}
	snprintf(wp_src, sizeof(wp_src), "%s", path);
	wp_mtime = mtime;
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
	if (!server.scene) {
		return;
	}

	struct kdos_wp_node *n, *tmp;
	wl_list_for_each_safe(n, tmp, &wp_nodes, link) {
		if (n->scene_buffer) {
			wlr_scene_node_destroy(&n->scene_buffer->node);
		}
		if (n->rect) {
			wlr_scene_node_destroy(&n->rect->node);
		}
		wl_list_remove(&n->link);
		free(n);
	}

	/*
	 * C8: the very bottom of every output is a rect in the accent's
	 * DEEP colour, image or no image — `wallpaper = none` and a failed
	 * decode now genuinely show the theme colour the log always
	 * claimed, and an image that does not cover (it always does, but
	 * cover is a crop) has the palette under it, not undefined black.
	 */
	const KcolScheme *sc = kdos_accent_scheme();
	KcolRgb deep = kcol_rgb(sc ? sc->deep : 0x000a03);
	const float shade[4] = { deep.r / 255.0f, deep.g / 255.0f,
		deep.b / 255.0f, 1.0f };

	struct output *o;
	wl_list_for_each(o, &server.outputs, link) {
		struct wlr_box box = { 0 };
		wlr_output_layout_get_box(server.output_layout,
			o->wlr_output, &box);
		if (wlr_box_empty(&box)) {
			continue;
		}

		n = calloc(1, sizeof(*n));
		if (!n) {
			continue;
		}

		struct wlr_scene_buffer *sb = NULL;
		if (wp_buf) {
			sb = wlr_scene_buffer_create(&server.scene->tree,
				&wp_buf->base);
		}
		if (sb) {
			/* under everything, incl. background layer clients */
			wlr_scene_node_lower_to_bottom(&sb->node);

			struct wlr_fbox src;
			cover_src_box(wp_buf->base.width, wp_buf->base.height,
				box.width, box.height, &src);
			wlr_scene_buffer_set_source_box(sb, &src);
			wlr_scene_buffer_set_dest_size(sb, box.width,
				box.height);
			wlr_scene_node_set_position(&sb->node, box.x, box.y);
		}

		struct wlr_scene_rect *rect = wlr_scene_rect_create(
			&server.scene->tree, box.width, box.height, shade);
		if (rect) {
			wlr_scene_node_set_position(&rect->node, box.x, box.y);
			/* created after the image, lowered after it: the
			 * rect ends up underneath */
			wlr_scene_node_lower_to_bottom(&rect->node);
		}

		if (!sb && !rect) {
			free(n);
			continue;
		}
		n->scene_buffer = sb;
		n->rect = rect;
		wl_list_insert(&wp_nodes, &n->link);
	}
}

/*
 * SIGHUP: `kdos theme <accent>` just retinted the cache wallpaper and
 * the deep colour moved with the accent. Re-decode only when the chosen
 * file actually changed — a SIGHUP that changed nothing must not decode
 * a 4K PNG for fun — and rebuild either way for the retinted rects.
 */
void
kdos_wallpaper_reload(void)
{
	char path[512];
	time_t mtime;
	struct kdos_wp_buffer *old = wp_buf;

	if (!wp_source(path, sizeof(path), &mtime)) {
		wp_buf = NULL;
		wp_src[0] = '\0';
		wp_mtime = 0;
		if (old) {
			wlr_log(WLR_INFO, "wallpaper off (comp.conf) — the "
				"desktop background is the accent's deep colour");
		}
	} else if (!old || strcmp(path, wp_src) || mtime != wp_mtime) {
		wp_buf = decode_png(path);
		if (wp_buf) {
			snprintf(wp_src, sizeof(wp_src), "%s", path);
			wp_mtime = mtime;
			wlr_log(WLR_INFO, "wallpaper %s (%dx%d)", path,
				wp_buf->base.width, wp_buf->base.height);
		} else {
			wp_src[0] = '\0';
			wp_mtime = 0;
			wlr_log(WLR_INFO, "no wallpaper at %s — the desktop "
				"background is the accent's deep colour", path);
		}
	} else {
		old = NULL;	/* unchanged: keep the decode */
	}

	kdos_wallpaper_arrange();
	if (old && old != wp_buf) {
		/* after arrange: the nodes holding its refs are gone */
		wlr_buffer_drop(&old->base);
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
