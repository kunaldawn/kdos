/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   a wlr_buffer over plain memory
 *
 * wlroots has no public "make me a wlr_buffer from a malloc", so anything that
 * wants to put its OWN pixels in the scene graph has to supply the smallest
 * wlr_buffer_impl that works. wallpaper.c carried one; deco.c needs the same
 * thing for every window frame, and a second copy would be a second answer to
 * what a buffer is. This is that one copy.
 *
 * Data-pointer access only, which is not a shortcut: it is what both renderers
 * use to upload a texture, and implementing dmabuf here would mean allocating
 * through the GPU for a picture the CPU just drew.
 *
 * OWNERSHIP, because it is the part that goes wrong. wlr_scene_buffer LOCKS the
 * buffer it holds. The rule this file exists to make easy:
 *
 *      create -> paint -> wlr_scene_buffer_set_buffer() -> kc_cellbuf_drop()
 *
 * After the drop the scene holds the only lock, and the buffer is freed when
 * the scene lets go of it. A buffer kept around and repainted in place would be
 * written while the renderer was still reading it — tearing that only appears
 * under load, which is the worst kind. A frame repaint is rare (focus, title,
 * size) and a fresh half-megabyte allocation is cheaper than being clever about
 * it.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

#include "kdos-comp.h"

static void cellbuf_destroy(struct wlr_buffer *b)
{
	struct kc_cellbuf *c = (struct kc_cellbuf *)b;

	/*
	 * wlr_buffer_finish() FIRST, and leaving it out is a general protection
	 * fault in musl's allocator with a backtrace that points nowhere near
	 * here. It cost a boot to find.
	 *
	 * It does two things this file cannot do without: it emits
	 * events.destroy, so anything watching this buffer learns it is going
	 * away, and it runs wlr_addon_set_finish() — and the addon set is the
	 * part that bites. wlroots attaches addons to a buffer (the scene's
	 * texture cache among them) and an addon holds LIST LINKS INTO THIS
	 * ALLOCATION. Freeing the buffer without finishing the set leaves those
	 * links pointing at freed memory; the next list operation writes through
	 * them and corrupts the heap. The fault then lands on some unrelated
	 * free() several frames later, which is why the crash appeared inside
	 * pixman_image_unref with nothing obviously wrong nearby.
	 *
	 * Every wlr_buffer_impl in wlroots does this, including the
	 * examples/cairo-buffer.c this file and wallpaper.c were modelled on.
	 * Both of ours had missed it; wallpaper.c never showed it because its
	 * buffer is created once and destroyed at shutdown, while a window frame
	 * allocates and destroys one on every focus change.
	 */
	wlr_buffer_finish(b);

	if (c->img)
		pixman_image_unref(c->img);
	free(c->data);
	free(c);
}

static bool cellbuf_begin_data_ptr_access(struct wlr_buffer *b, uint32_t flags,
					  void **data, uint32_t *format,
					  size_t *stride)
{
	struct kc_cellbuf *c = (struct kc_cellbuf *)b;
	/*
	 * A read-only buffer refuses a write rather than quietly allowing one.
	 * The wallpaper is decoded once and never written again, and a renderer
	 * that asked to write into it would be a bug worth hearing about rather
	 * than a copy worth making.
	 */
	if ((flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) && !c->writable)
		return false;
	*data = c->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = c->stride;
	return true;
}

static void cellbuf_end_data_ptr_access(struct wlr_buffer *b)
{
	(void)b;
}

static const struct wlr_buffer_impl cellbuf_impl = {
	.destroy = cellbuf_destroy,
	.begin_data_ptr_access = cellbuf_begin_data_ptr_access,
	.end_data_ptr_access = cellbuf_end_data_ptr_access,
};

struct kc_cellbuf *kc_cellbuf_create(int w, int h, bool writable, bool want_img)
{
	if (w <= 0 || h <= 0)
		return NULL;

	struct kc_cellbuf *c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;

	c->stride = (size_t)w * 4;
	c->data = calloc(1, c->stride * (size_t)h);
	if (!c->data) {
		free(c);
		return NULL;
	}
	c->writable = writable;

	if (want_img) {
		/*
		 * DRM_FORMAT_ARGB8888 and PIXMAN_a8r8g8b8 are the same bytes on
		 * a little-endian machine — both are a uint32 of 0xAARRGGBB —
		 * so the painter and the renderer agree with no conversion.
		 */
		c->img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						  c->data, (int)c->stride);
		if (!c->img) {
			free(c->data);
			free(c);
			return NULL;
		}
	}

	wlr_buffer_init(&c->base, &cellbuf_impl, w, h);
	return c;
}

pixman_image_t *kc_cellbuf_image(struct kc_cellbuf *c)
{
	return c ? c->img : NULL;
}

uint32_t *kc_cellbuf_data(struct kc_cellbuf *c)
{
	return c ? c->data : NULL;
}

size_t kc_cellbuf_stride(struct kc_cellbuf *c)
{
	return c ? c->stride : 0;
}

struct wlr_buffer *kc_cellbuf_base(struct kc_cellbuf *c)
{
	return c ? &c->base : NULL;
}

void kc_cellbuf_drop(struct kc_cellbuf *c)
{
	if (c)
		wlr_buffer_drop(&c->base);
}
