/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkicon — name in, sprite slot out
 *
 * Four decisions, and each is a way an icon layer usually ruins a text-mode
 * desktop:
 *
 * - **A picture is a SQUARE centred in its cell box, never a stretch.** The
 *   cell here is 16x32, so "one icon" is two cells wide and one tall and comes
 *   out at 32x32. Asking for one cell gets a 16x16 picture floating in the
 *   middle of a 16x32 box, which is correct and is what a taskbar wants; a
 *   16x32 Firefox logo is not an icon, it is a smear.
 *
 * - **The theme's own artwork is TINTED, the applications' is not.** A folder
 *   goes through kcol_remap into the accent like every other KDOS artefact;
 *   a phosphor Firefox mark is vandalism. That is the same split
 *   `kdos-theme icons` already keeps, for the same reason, and this file
 *   reaches it by SOURCE — the atlas is tinted, /usr/share/icons/hicolor is
 *   not — rather than by guessing from the name.
 *
 * - **The cache is keyed by CONTENT and so is the sprite slot.** Two frames
 *   drawing the same icon at the same size must produce byte-identical cells,
 *   or the row diff repaints the whole panel sixty times a second. The hot
 *   path is therefore a ktui_sprite_find() and nothing else — no stat, no
 *   decode, no allocation.
 *
 * - **Every failure is -1 and every -1 is a glyph.** No icon theme, no atlas,
 *   an unreadable PNG, a full sprite table, `icons = off`, a tty: one answer,
 *   and every consumer already knows how to draw the desktop without pictures
 *   because that is how it drew it last week.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kcolor.h"
#include "kicon_int.h"
#include "ktui.h"
#include "kxdg.h"

#define KI_MAX_CACHE 192
#define KI_NAME_MAX 128

struct ki_pic {
	uint64_t key;
	pixman_image_t *img;
	unsigned long used;	/* the LRU clock, for eviction */
};

static struct ki_pic cache[KI_MAX_CACHE];
static int ncache;
static unsigned long ki_clock;

static int ki_cw, ki_ch, ki_scale;
static int ki_on = 1;
static int ki_ready;
static int ki_have_atlas;

/* The sizes 06_packaging/01_appbox.sh flattens the alien apps' icons into, and
 * the ones hicolor themes conventionally carry. Ascending, so the first at or
 * above the wanted size is the one to take. */
static const int hicolor_sizes[] = { 16,  22,  24,	32,  48,
				     64,  72,  96,	128, 192,
				     256, 384, 512 };
#define NHICOLOR ((int)(sizeof(hicolor_sizes) / sizeof(hicolor_sizes[0])))

/* ── the tint ──────────────────────────────────────────────────────────── */

/*
 * kcol_remap is six HLS conversions per call and an icon is thousands of
 * pixels of a handful of colours — flat SVG artwork, which is exactly why
 * Papirus was chosen. Memoised the same way write_wallpaper() memoises, and
 * for the same measured reason.
 */
struct tint_memo {
	uint32_t in, out;
	int valid;
};
#define TINT_SLOTS 1024
static struct tint_memo tint_memo[TINT_SLOTS];
static const KcolScheme *tint_scheme;

static void tint_reset(const KcolScheme *sc)
{
	memset(tint_memo, 0, sizeof(tint_memo));
	tint_scheme = sc;
}

static uint32_t tint_rgb(uint32_t rgb)
{
	if (!tint_scheme)
		return rgb;
	unsigned h = (rgb * 2654435761u) >> 20;
	h &= TINT_SLOTS - 1;
	if (tint_memo[h].valid && tint_memo[h].in == rgb)
		return tint_memo[h].out;
	uint32_t out = kcol_remap(tint_scheme, rgb);
	tint_memo[h].in = rgb;
	tint_memo[h].out = out;
	tint_memo[h].valid = 1;
	return out;
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static uint64_t hash64(const char *s, uint64_t h)
{
	while (*s) {
		h ^= (unsigned char)*s++;
		h *= 1099511628211ull;
	}
	return h;
}

static uint64_t pic_key(const char *name, int cw, int ch)
{
	uint64_t h = hash64(name, 1469598103934665603ull);
	char tag[64];
	/* The accent is part of the identity: a tinted folder is a different
	 * picture in amber, and a key that ignored it would hand the panel the
	 * previous accent's icon for the life of the process. */
	snprintf(tag, sizeof(tag), "|%d|%d|%d|%s", cw, ch, ki_scale,
		 ktui_theme ? ktui_theme->name : "");
	return hash64(tag, h);
}

/* ── the pixel work ────────────────────────────────────────────────────── */

/*
 * RGBA8888 (straight) -> a premultiplied a8r8g8b8 pixman image, tinted on the
 * way if asked.
 *
 * PREMULTIPLY LAST. The tint runs on the colour, and a colour that has already
 * been multiplied by its alpha is not the colour — that is the halo bug
 * kdos-cursors paid for once, arriving here in a different shape.
 */
static pixman_image_t *to_pixman(const uint8_t *rgba, int w, int h, int tint)
{
	uint32_t *px = malloc((size_t)w * h * 4);
	if (!px)
		return NULL;

	for (int i = 0; i < w * h; i++) {
		uint32_t r = rgba[i * 4 + 0];
		uint32_t g = rgba[i * 4 + 1];
		uint32_t b = rgba[i * 4 + 2];
		uint32_t a = rgba[i * 4 + 3];

		if (tint && a) {
			uint32_t c = tint_rgb((r << 16) | (g << 8) | b);
			r = (c >> 16) & 0xff;
			g = (c >> 8) & 0xff;
			b = c & 0xff;
		}
		r = (r * a + 127) / 255;
		g = (g * a + 127) / 255;
		b = (b * a + 127) / 255;
		px[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}

	/* pixman_image_create_bits does NOT take ownership of the pixels — it
	 * only frees them when it allocated them itself (data == NULL). Every
	 * image built here is freed through free_bits(), which unrefs and then
	 * frees the buffer it was handed. */
	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						       px, w * 4);
	if (!img) {
		free(px);
		return NULL;
	}
	return img;
}

static void free_bits(pixman_image_t *img)
{
	if (!img)
		return;
	uint32_t *data = pixman_image_get_data(img);
	pixman_image_unref(img);
	free(data);
}

/*
 * Scale `src` into a fresh box_w x box_h picture, centred, aspect kept.
 *
 * BILINEAR, and the transform is the INVERSE of the scale — pixman maps
 * DESTINATION coordinates back into the source, which is the direction
 * everybody gets wrong exactly once.
 */
static pixman_image_t *fit(pixman_image_t *src, int box_w, int box_h)
{
	int sw = pixman_image_get_width(src);
	int sh = pixman_image_get_height(src);
	if (sw <= 0 || sh <= 0 || box_w <= 0 || box_h <= 0)
		return NULL;

	int side = box_w < box_h ? box_w : box_h;
	if (side < 1)
		return NULL;

	uint32_t *px = calloc((size_t)box_w * box_h, 4);
	if (!px)
		return NULL;
	pixman_image_t *dst = pixman_image_create_bits(PIXMAN_a8r8g8b8, box_w,
						       box_h, px, box_w * 4);
	if (!dst) {
		free(px);
		return NULL;
	}

	pixman_transform_t t;
	pixman_transform_init_scale(&t,
				    pixman_double_to_fixed((double)sw / side),
				    pixman_double_to_fixed((double)sh / side));
	pixman_image_set_transform(src, &t);
	pixman_image_set_filter(src, PIXMAN_FILTER_BILINEAR, NULL, 0);
	pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);

	int ox = (box_w - side) / 2, oy = (box_h - side) / 2;
	pixman_image_composite32(PIXMAN_OP_SRC, src, NULL, dst, 0, 0, 0, 0, ox,
				 oy, side, side);

	/* Leave the source as it was found: it is freed straight after here
	 * today, and a transform left on a cached image would be a bug waiting
	 * for the day it is not. */
	pixman_image_set_transform(src, NULL);
	return dst;
}

/* ── the cache ─────────────────────────────────────────────────────────── */

static void cache_drop(int i)
{
	ktui_sprite_drop(cache[i].key);
	free_bits(cache[i].img);
	cache[i].img = NULL;
	cache[i].key = 0;
}

static void cache_evict_one(void)
{
	int worst = -1;
	for (int i = 0; i < ncache; i++)
		if (cache[i].img &&
		    (worst < 0 || cache[i].used < cache[worst].used))
			worst = i;
	if (worst >= 0)
		cache_drop(worst);
}

static int cache_put(uint64_t key, pixman_image_t *img)
{
	int slot = -1;
	for (int i = 0; i < ncache; i++)
		if (!cache[i].img) {
			slot = i;
			break;
		}
	if (slot < 0 && ncache < KI_MAX_CACHE)
		slot = ncache++;
	if (slot < 0) {
		cache_evict_one();
		for (int i = 0; i < ncache; i++)
			if (!cache[i].img) {
				slot = i;
				break;
			}
	}
	if (slot < 0)
		return -1;
	cache[slot].key = key;
	cache[slot].img = img;
	cache[slot].used = ++ki_clock;
	return slot;
}

/* ── the sources ───────────────────────────────────────────────────────── */

static int file_ok(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

static int data_dirs(char out[][512], int max)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *hdir = getenv("HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int n = 0;

	if (home && *home && n < max)
		snprintf(out[n++], 512, "%s", home);
	else if (hdir && *hdir && n < max)
		snprintf(out[n++], 512, "%s/.local/share", hdir);

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	while (*dirs && n < max) {
		const char *c = strchr(dirs, ':');
		size_t l = c ? (size_t)(c - dirs) : strlen(dirs);
		if (l && l < 512) {
			snprintf(out[n], 512, "%.*s", (int)l, dirs);
			n++;
		}
		if (!c)
			break;
		dirs = c + 1;
	}
	return n;
}

/*
 * An application's own icon, out of the hicolor tree that 01_appbox.sh fills.
 * Never tinted. `want` is the pixel side.
 */
static uint8_t *load_hicolor(const char *name, int want, int *w, int *h)
{
	char dirs[8][512];
	int nd = data_dirs(dirs, 8);
	char path[1024];

	/* An `Icon=` may be an absolute path — several Debian entries are. */
	if (name[0] == '/')
		return file_ok(name) ? ki_png_file(name, w, h) : NULL;

	for (int d = 0; d < nd; d++) {
		int best = -1;
		for (int i = 0; i < NHICOLOR; i++) {
			snprintf(path, sizeof(path),
				 "%s/icons/hicolor/%dx%d/apps/%s.png", dirs[d],
				 hicolor_sizes[i], hicolor_sizes[i], name);
			if (!file_ok(path))
				continue;
			best = i;
			if (hicolor_sizes[i] >= want)
				break;	/* smallest at or above: stop here */
		}
		if (best >= 0) {
			snprintf(path, sizeof(path),
				 "%s/icons/hicolor/%dx%d/apps/%s.png", dirs[d],
				 hicolor_sizes[best], hicolor_sizes[best],
				 name);
			return ki_png_file(path, w, h);
		}
		/* pixmaps/ is where Debian's older packages still put them. */
		snprintf(path, sizeof(path), "%s/pixmaps/%s.png", dirs[d],
			 name);
		if (file_ok(path))
			return ki_png_file(path, w, h);
	}
	return NULL;
}

/* ── the public half ───────────────────────────────────────────────────── */

int kicon_enabled(void)
{
	return ki_on && ki_ready;
}

void kicon_set_enabled(int on)
{
	ki_on = on;
}

int kicon_cached(void)
{
	int n = 0;
	for (int i = 0; i < ncache; i++)
		if (cache[i].img)
			n++;
	return n;
}

int kicon_init(int cell_w, int cell_h, int scale)
{
	char path[1024];
	char dirs[8][512];

	kicon_finish();

	ki_cw = cell_w > 0 ? cell_w : 8;
	ki_ch = cell_h > 0 ? cell_h : 16;
	ki_scale = scale > 0 ? scale : 1;
	tint_reset(ktui_theme ? kcol_find(ktui_theme->name) : NULL);

	int nd = data_dirs(dirs, 8);
	for (int d = 0; d < nd && !ki_have_atlas; d++) {
		snprintf(path, sizeof(path), "%s/kdos/icons/atlas.kia",
			 dirs[d]);
		if (ki_atlas_open(path) == 0)
			ki_have_atlas = 1;
	}

	/* Ready when at least one source can answer. A machine with the atlas
	 * missing but the appbox installed still gets application icons, which
	 * is most of what a taskbar and a start menu draw. */
	int have_apps = 0;
	for (int d = 0; d < nd && !have_apps; d++) {
		struct stat st;
		snprintf(path, sizeof(path), "%s/icons/hicolor", dirs[d]);
		have_apps = stat(path, &st) == 0 && S_ISDIR(st.st_mode);
	}

	ki_ready = ki_have_atlas || have_apps;
	return ki_ready ? 0 : -1;
}

void kicon_finish(void)
{
	for (int i = 0; i < ncache; i++)
		if (cache[i].img)
			cache_drop(i);
	ncache = 0;
	ki_atlas_close();
	ki_have_atlas = 0;
	ki_ready = 0;
}

void kicon_retint(void)
{
	for (int i = 0; i < ncache; i++)
		if (cache[i].img)
			cache_drop(i);
	ncache = 0;
	tint_reset(ktui_theme ? kcol_find(ktui_theme->name) : NULL);
}

static uint32_t fallback_cp(void)
{
	return (ktui_caps & KT_CAP_UTF8) ? 0x2593u : (uint32_t)'#';
}

int kicon_slot(const char *name, int cw, int ch)
{
	if (!kicon_enabled() || !name || !*name || cw < 1 || ch < 1)
		return -1;
	if (cw > 16 || ch > 16)
		return -1;

	uint64_t key = pic_key(name, cw, ch);

	/* The hot path: the same icon at the same size was drawn last frame,
	 * so there is nothing to do but hand back the slot. */
	int slot = ktui_sprite_find(key);
	if (slot >= 0) {
		for (int i = 0; i < ncache; i++)
			if (cache[i].key == key && cache[i].img) {
				cache[i].used = ++ki_clock;
				break;
			}
		return slot;
	}

	int box_w = cw * ki_cw * ki_scale;
	int box_h = ch * ki_ch * ki_scale;
	int want = box_w < box_h ? box_w : box_h;

	/*
	 * THE ATLAS FIRST, and this order is a bug that was already shipped.
	 *
	 * `01_appbox.sh` flattens every context of the appbox image's icon
	 * theme into hicolor's `apps/` — so `/usr/share/icons/hicolor/16x16/
	 * apps/folder.png` exists, and looking there first meant every folder
	 * on the desktop came out as Debian's blue one, at 16 pixels upscaled
	 * to 32, with no tint. Photographed on a booted ISO.
	 *
	 * The atlas can never shadow an application: it carries the theme's
	 * places, devices, mimetypes, status, actions and emblems, and
	 * Papirus's `apps/` is deliberately not vendored (an accented Firefox
	 * mark is vandalism, which is the same reason app icons are not
	 * tinted here).
	 */
	int w = 0, h = 0, tint = 0;
	uint8_t *rgba = NULL;

	if (ki_have_atlas) {
		size_t len = 0;
		const void *blob = ki_atlas_find(name, want, &len, NULL);
		if (blob) {
			rgba = ki_png_mem(blob, len, &w, &h);
			tint = 1;
		}
	}
	if (!rgba)
		rgba = load_hicolor(name, want, &w, &h);
	if (!rgba)
		return -1;

	pixman_image_t *src = to_pixman(rgba, w, h, tint);
	free(rgba);
	if (!src)
		return -1;

	pixman_image_t *img = fit(src, box_w, box_h);
	free_bits(src);
	if (!img)
		return -1;

	int ci = cache_put(key, img);
	if (ci < 0) {
		free_bits(img);
		return -1;
	}
	slot = ktui_sprite_put(key, img, cw, ch, fallback_cp());
	if (slot < 0) {
		cache_drop(ci);
		return -1;
	}
	return slot;
}

/*
 * The same lookup, as a PICTURE the caller owns.
 *
 * `kicon_slot` is the whole story for an icon that occupies cells of its own;
 * a canvas needs the pixels themselves, because the point of a canvas is that
 * the icon and the text beside it are composed at pixel positions rather than
 * at cell ones — which is exactly what a Start button whose word is centred
 * over two rows requires. Not cached: a canvas rasterises when its content
 * changes, which is rare, and a second cache keyed by pixel box would be a
 * second thing to invalidate on a retint.
 *
 * The caller frees it with kicon_pixmap_free().
 */
pixman_image_t *kicon_pixmap(const char *name, int box_w, int box_h)
{
	if (!kicon_enabled() || !name || !*name || box_w < 1 || box_h < 1)
		return NULL;

	int want = box_w < box_h ? box_w : box_h;
	int w = 0, h = 0, tint = 0;
	uint8_t *rgba = NULL;

	/* The atlas first, exactly as kicon_slot does — see the argument
	 * there. Two lookups that disagreed about which picture a name means
	 * would show one icon in a tile and another beside it. */
	if (ki_have_atlas) {
		size_t len = 0;
		const void *blob = ki_atlas_find(name, want, &len, NULL);
		if (blob) {
			rgba = ki_png_mem(blob, len, &w, &h);
			tint = 1;
		}
	}
	if (!rgba)
		rgba = load_hicolor(name, want, &w, &h);
	if (!rgba)
		return NULL;

	pixman_image_t *src = to_pixman(rgba, w, h, tint);
	free(rgba);
	if (!src)
		return NULL;

	pixman_image_t *img = fit(src, box_w, box_h);
	free_bits(src);
	return img;
}

void kicon_pixmap_free(pixman_image_t *img)
{
	if (img)
		free_bits(img);
}

int kicon_slot_for_path(const char *path, int is_dir, int cw, int ch)
{
	char mime[128];
	char names[4][64];

	if (!kicon_enabled() || !path)
		return -1;
	if (is_dir)
		return kicon_slot("folder", cw, ch);

	kxdg_mime_for_path(path, mime, sizeof(mime));
	int n = kxdg_mime_icon_names(mime, names, 4);
	for (int i = 0; i < n; i++) {
		int s = kicon_slot(names[i], cw, ch);
		if (s >= 0)
			return s;
	}
	return -1;
}

const char *kicon_app_icon(const char *id)
{
	static char icon[KI_NAME_MAX];
	char dirs[8][512];
	char path[1024];
	int nd = data_dirs(dirs, 8);

	if (!id || !*id)
		return NULL;
	icon[0] = '\0';

	for (int d = 0; d < nd; d++) {
		KxdgEntry e = { 0 };
		snprintf(path, sizeof(path), "%s/applications/%s.desktop",
			 dirs[d], id);
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			continue;
		const char *v = kxdg_get(&e, "Icon", NULL);
		if (v && *v)
			snprintf(icon, sizeof(icon), "%s", v);
		kxdg_free(&e);
		if (icon[0])
			return icon;
	}
	return NULL;
}
