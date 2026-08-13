/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * wallpaper.c — the desktop background.
 *
 * KDOS shipped a wallpaper and nothing drew it: `fs/usr/share/backgrounds/` was
 * installed, the plan referred to "a wallpaper stays under the windows", and
 * every session came up on a black screen. The usual answer is a separate
 * layer-shell client (swaybg), and it is the wrong one here — libkwl paints
 * CELLS, so a wallpaper client would be the one program in the desktop that is
 * not a character grid, carrying its own Wayland connection and its own shm
 * pool to blit one static image. The compositor already owns a scene graph with
 * a background tree in it; putting the image straight in costs one node.
 *
 * Three things worth knowing:
 *
 * - **The buffer is ours, so wlroots needs an impl for it.** There is no public
 *   "wlr_buffer from memory", so this file has the smallest one that works:
 *   data-ptr access only, which is what both the GLES2 and the pixman renderer
 *   use to upload a texture. It is created once and shared by every output.
 * - **One node per OUTPUT, not one for the layout.** `wlr_scene_buffer` scales
 *   to a destination size, and two monitors of different sizes need two
 *   destinations. The node is positioned in layout coordinates so it lands on
 *   the output it belongs to.
 * - **Cover, not stretch.** The image is scaled to the larger of the two axis
 *   ratios and centred, so a 16:10 wallpaper on a 4:3 screen crops rather than
 *   distorting. wlr_scene_buffer's source box is what does the cropping.
 */

#include <drm_fourcc.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/interfaces/wlr_buffer.h>

#include "kdos-comp.h"

struct kc_wallpaper_buffer {
	struct wlr_buffer base;
	uint32_t *data;			/* ARGB8888, premultiplied (opaque) */
	size_t stride;
};

static void wp_buffer_destroy(struct wlr_buffer *b)
{
	struct kc_wallpaper_buffer *w = (struct kc_wallpaper_buffer *)b;
	/*
	 * Same omission cellbuf.c had, and latent here rather than harmless:
	 * this buffer is created once and destroyed at shutdown, so the dangling
	 * addon links never get walked again. See cellbuf_destroy() for what it
	 * costs when a buffer is destroyed while the session is still running.
	 */
	wlr_buffer_finish(b);
	free(w->data);
	free(w);
}

static bool wp_begin_data_ptr_access(struct wlr_buffer *b, uint32_t flags,
				     void **data, uint32_t *format,
				     size_t *stride)
{
	struct kc_wallpaper_buffer *w = (struct kc_wallpaper_buffer *)b;
	/* Read-only: the wallpaper is decoded once and never written again, and
	 * a renderer that asked to write into it would be a bug we want to hear
	 * about rather than a copy we want to make. */
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = w->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = w->stride;
	return true;
}

static void wp_end_data_ptr_access(struct wlr_buffer *b)
{
	(void)b;
}

static const struct wlr_buffer_impl wp_buffer_impl = {
	.destroy = wp_buffer_destroy,
	.begin_data_ptr_access = wp_begin_data_ptr_access,
	.end_data_ptr_access = wp_end_data_ptr_access,
};

/*
 * libpng, and every transform asked for up front: the decoder is told to hand
 * back 8-bit RGBA whatever the file is (palette, 16-bit, grey, tRNS), so there
 * is one pixel layout below this call instead of six.
 */
static struct kc_wallpaper_buffer *decode_png(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
						 NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;
	struct kc_wallpaper_buffer *w = NULL;
	png_bytep *rows = NULL;
	int ok = 0;

	if (!png || !info || setjmp(png_jmpbuf(png)))
		goto out;

	png_init_io(png, f);
	png_read_info(png, info);

	png_uint_32 iw = 0, ih = 0;
	int depth = 0, ctype = 0;
	png_get_IHDR(png, info, &iw, &ih, &depth, &ctype, NULL, NULL, NULL);
	if (iw == 0 || ih == 0 || iw > 16384 || ih > 16384)
		goto out;

	if (ctype == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (depth == 16)
		png_set_strip_16(png);
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);

	w = calloc(1, sizeof(*w));
	if (!w)
		goto out;
	w->stride = (size_t)iw * 4;
	w->data = malloc(w->stride * ih);
	rows = malloc(sizeof(png_bytep) * ih);
	if (!w->data || !rows) {
		free(w->data);
		free(w);
		w = NULL;
		goto out;
	}
	for (png_uint_32 y = 0; y < ih; y++)
		rows[y] = (png_bytep)((char *)w->data + y * w->stride);
	png_read_image(png, rows);

	/*
	 * libpng hands back R,G,B,A in memory order; DRM_FORMAT_ARGB8888 is a
	 * little-endian 32-bit word, so the bytes it wants are B,G,R,A. Swap in
	 * place rather than asking png_set_bgr() for it, because the alpha
	 * filler above has already fixed the channel COUNT and this keeps the
	 * one byte-order decision in one place.
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
	if (png)
		png_destroy_read_struct(&png, info ? &info : NULL, NULL);
	fclose(f);
	/* libpng reports a bad file by longjmp'ing back to the setjmp above, and
	 * it can do that AFTER the allocation — returning `w` then would hand
	 * back a struct wlr_buffer that was never initialised, which the scene
	 * would happily take a reference to. Only an initialised buffer leaves
	 * this function. */
	if (!ok && w) {
		free(w->data);
		free(w);
		w = NULL;
	}
	return w;
}

/* Cover: scale by the LARGER ratio and centre, so the image fills the output
 * and the overhang is cropped. Stretching to the output's aspect is the other
 * option and it is the one that makes a photograph look wrong. */
static void cover_src_box(int iw, int ih, int ow, int oh, struct wlr_fbox *src)
{
	double sx = (double)ow / iw, sy = (double)oh / ih;
	double s = sx > sy ? sx : sy;
	double vw = ow / s, vh = oh / s;
	src->x = (iw - vw) / 2;
	src->y = (ih - vh) / 2;
	src->width = vw;
	src->height = vh;
}

void kc_wallpaper_init(struct kc_server *s)
{
	const char *path = s->wallpaper && *s->wallpaper
				   ? s->wallpaper
				   : KC_WALLPAPER_DEFAULT;
	/* `wallpaper = none` in comp.conf is an honest off, not a missing file:
	 * a black desktop somebody asked for must not print an error every
	 * login. */
	if (!strcmp(path, "none")) {
		wlr_log(WLR_INFO, "wallpaper off (comp.conf)");
		return;
	}

	s->wallpaper_buf = decode_png(path);
	if (!s->wallpaper_buf) {
		wlr_log(WLR_INFO, "no wallpaper at %s — the desktop background "
				  "is the theme colour", path);
		return;
	}
	wlr_log(WLR_INFO, "wallpaper %s (%dx%d)", path,
		s->wallpaper_buf->base.width, s->wallpaper_buf->base.height);
}

/*
 * Called from the output layout change handler, so it also runs when a monitor
 * is plugged, unplugged or moved — the node is rebuilt rather than adjusted,
 * because a scene buffer's destination is its geometry and there is nothing
 * cheaper to update.
 */
void kc_wallpaper_arrange(struct kc_server *s)
{
	if (!s->wallpaper_buf || !s->layer_below)
		return;

	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		struct wlr_box box = {0};
		wlr_output_layout_get_box(s->output_layout, o->wlr_output, &box);
		if (wlr_box_empty(&box))
			continue;

		if (!o->wallpaper) {
			o->wallpaper = wlr_scene_buffer_create(
				s->layer_below, &s->wallpaper_buf->base);
			if (!o->wallpaper)
				continue;
			/* Below every layer-shell surface in the same tree —
			 * `background` is a layer a client may use, and a
			 * wallpaper is under one, not over it. */
			wlr_scene_node_lower_to_bottom(&o->wallpaper->node);
		}

		struct wlr_fbox src;
		cover_src_box(s->wallpaper_buf->base.width,
			      s->wallpaper_buf->base.height,
			      box.width, box.height, &src);
		wlr_scene_buffer_set_source_box(o->wallpaper, &src);
		wlr_scene_buffer_set_dest_size(o->wallpaper, box.width,
					       box.height);
		wlr_scene_node_set_position(&o->wallpaper->node, box.x, box.y);
	}
}

void kc_wallpaper_free(struct kc_server *s)
{
	if (!s->wallpaper_buf)
		return;
	/* The scene nodes hold their own reference; dropping ours is what lets
	 * the buffer go when the last node does. */
	wlr_buffer_drop(&s->wallpaper_buf->base);
	s->wallpaper_buf = NULL;
}
