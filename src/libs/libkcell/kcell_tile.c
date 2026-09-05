/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   A decoded picture becomes sprite tiles
 *
 * libktui owns the sprite TABLE and does no pixel work; the picture arrives as
 * a pixman image at whatever size its format declared. Between them sits one
 * scale and one cut into tiles, and it lives here because libkcell is the
 * archive that already links pixman and is already linked by everything that
 * puts pixels in a cell grid.
 *
 * ONE COPY, because two would disagree about the filter. A terminal showing an
 * inline picture and a viewer showing a page are the same operation, and a
 * second implementation of it is a second answer to how a photograph is
 * resampled.
 * ---------------------------------
 */

#include <stdlib.h>

#include "kcell.h"

/* pixman hands the image back to its destroy function and free() does not take
 * one; a cast between the two signatures is undefined behaviour. */
static void free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

void kcell_tile_free(uint64_t key, const void *pix, void *user)
{
	(void)key;
	(void)user;
	if (pix)
		pixman_image_unref((pixman_image_t *)pix);
}

/* The whole picture, scaled to the cell grid it was given. Held for the length
 * of one tiling and unreffed after: every tile is a copy of a piece of it. */
static pixman_image_t *scaled;
static int tile_cw, tile_ch;

static const void *tile_of(void *user, int cell_x, int cell_y, int tw, int th)
{
	(void)user;

	int pw = tw * tile_cw, ph = th * tile_ch;
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
				 cell_x * tile_cw, cell_y * tile_ch, 0, 0,
				 0, 0, pw, ph);
	return t;
}

int kcell_tile_picture(pixman_image_t *img, uint64_t key, int cw, int ch,
		       int cell_w, int cell_h, uint32_t fallback)
{
	int sw = img ? pixman_image_get_width(img) : 0;
	int sh = img ? pixman_image_get_height(img) : 0;
	int dw = cw * cell_w, dh = ch * cell_h;
	uint32_t *bits;
	int r;

	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return -1;

	bits = calloc((size_t)dw * (size_t)dh, 4);
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

	tile_cw = cell_w;
	tile_ch = cell_h;
	r = ktui_sprite_put_tiled(key, cw, ch, fallback, tile_of, NULL);

	pixman_image_unref(scaled);
	scaled = NULL;
	free(bits);
	return r;
}
