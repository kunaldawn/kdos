/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkicon — name in, sprite slot out
 *
 * Five decisions, and each is a way an icon layer usually ruins a text-mode
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
 *   decode, no allocation. When the table has taken the slot back but this
 *   file still holds the picture, the answer is a re-registration: a decode,
 *   a tint of every pixel and a bilinear rescale to produce the picture that
 *   is already in memory is the frame.
 *
 * - **A picture handed to the sprite table is REFERENCE-COUNTED and frees its
 *   own pixels.** The table's evictor is one per process and shared by every
 *   owner in it, so an unref arriving from somebody else's evictor has to be
 *   a complete free — and this file has to keep a reference of its own, or
 *   an eviction would leave the cache naming a freed image.
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

/*
 * NAMES THAT RESOLVE TO NOTHING ARE REMEMBERED TOO.
 *
 * A name with no picture ran the whole search on EVERY draw: the atlas, then
 * the data directories, then a stat of six sizes under each. A panel or a
 * desktop draws the same names every frame, so a single missing icon was a
 * few dozen failed stat() calls per frame for the life of the session, and a
 * theme where several are missing is the frame time.
 *
 * The key is the same one the picture cache uses. The table lives as long as
 * the library is initialised and is emptied only by kicon_init()/
 * kicon_finish(), so a name that starts resolving mid-session — a package
 * installed under a running desktop — is picked up at the program's next
 * start.
 */
#define KI_MAX_MISS 256

struct ki_pic {
	uint64_t key;
	pixman_image_t *img;
	unsigned long used;	/* the LRU clock, for eviction */
};

static struct ki_pic cache[KI_MAX_CACHE];
static int ncache;
static unsigned long ki_clock;

/*
 * AN APPLICATION ID TO AN ICON NAME, REMEMBERED.
 *
 * Answering it reads and parses a desktop entry from every data directory,
 * and the panel asks twice per task chip on every redraw — so a bar with six
 * windows on it re-read a dozen files a frame to draw icons that had not
 * changed. The answer for an id with no entry is remembered too, which is the
 * case that paid the full search every time.
 */
#define KI_APP_CACHE 64

static struct {
	uint64_t key;
	char icon[KI_NAME_MAX];
	int set;
} app_cache[KI_APP_CACHE];
static int napp, app_cursor;

/*
 * A FILE'S PATH TO THE SLOT ITS ICON IS IN.
 *
 * Answering it from scratch is a stat() and a walk of the MIME glob table —
 * a thousand suffix comparisons — and the desktop asks it for every entry it
 * draws on every frame. Thirty icons at the session's tick rate is thirty
 * syscalls and thirty thousand string comparisons a frame to decide pictures
 * that did not change.
 *
 * THE MEMO IS DROPPED BY kicon_forget_paths(), which the consumer calls when
 * it re-reads its directory. A file replaced by one of another type is
 * exactly that event, and the library cannot see it.
 */
#define KI_PATH_CACHE 256

static struct {
	uint64_t key;
	int cw, ch;
	int slot;
	int set;
} path_cache[KI_PATH_CACHE];
static int npath, path_cursor;

static uint64_t id_key(const char *id)
{
	uint64_t h = 1469598103934665603ULL;

	for (const unsigned char *p = (const unsigned char *)id; *p; p++)
		h = (h ^ *p) * 1099511628211ULL;
	return h;
}

static uint64_t miss[KI_MAX_MISS];
static int nmiss;

static int miss_known(uint64_t key)
{
	for (int i = 0; i < nmiss; i++)
		if (miss[i] == key)
			return 1;
	return 0;
}

static void miss_add(uint64_t key)
{
	/* A ring rather than a cap that stops recording: a table that filled
	 * and then refused would put every later name back on the slow path,
	 * which is the case this exists for. */
	if (nmiss < KI_MAX_MISS) {
		miss[nmiss++] = key;
		return;
	}
	miss[(int)(key % KI_MAX_MISS)] = key;
}

static int ki_cw, ki_ch, ki_scale;
static int ki_on = 1;
static int ki_ready;
static int ki_have_atlas;

/* The sizes hicolor themes conventionally carry. Ascending, so the first at or
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

static uint64_t hash64_mem(const void *p, size_t n, uint64_t h)
{
	const unsigned char *b = p;

	while (n--) {
		h ^= *b++;
		h *= 1099511628211ull;
	}
	return h;
}

static uint64_t pic_key(const char *name, int cw, int ch, int pad)
{
	uint64_t h = hash64(name, 1469598103934665603ull);
	char tag[64];
	/* The accent is part of the identity: a tinted folder is a different
	 * picture in amber, and a key that ignored it would hand the panel the
	 * previous accent's icon for the life of the process. `pad` for the
	 * same reason — the same name in the same cells at two paddings is two
	 * pictures, and one key for both hands the second caller the first
	 * one's. */
	snprintf(tag, sizeof(tag), "|%d|%d|%d|%d|%s", cw, ch, pad, ki_scale,
		 ktui_theme ? ktui_theme->name : "");
	return hash64(tag, h);
}

/* ── the pixel work ────────────────────────────────────────────────────── */

/* pixman hands the image back to its destroy function and free() does not take
 * one; a cast between the two signatures is undefined behaviour. */
static void kicon_free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

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

	/* pixman_image_create_bits frees only a buffer it allocated itself, so
	 * the destroy function is what releases this one. It must be the image
	 * that owns the pixels: a picture registered as a sprite can be
	 * unref'd by the table's evictor, which belongs to some other owner in
	 * the process and knows nothing about this buffer. */
	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						       px, w * 4);
	if (!img) {
		free(px);
		return NULL;
	}
	pixman_image_set_destroy_function(img, kicon_free_bits, px);
	return img;
}

/* One reference back. The pixels go with the last one, through the destroy
 * function the image was built with. */
static void pic_unref(pixman_image_t *img)
{
	if (img)
		pixman_image_unref(img);
}

/*
 * Scale `src` into a fresh box_w x box_h picture, centred, aspect kept.
 *
 * BILINEAR, and the transform is the INVERSE of the scale — pixman maps
 * DESTINATION coordinates back into the source, which is the direction
 * everybody gets wrong exactly once.
 */
static pixman_image_t *fit(pixman_image_t *src, int box_w, int box_h, int pad)
{
	int sw = pixman_image_get_width(src);
	int sh = pixman_image_get_height(src);
	if (sw <= 0 || sh <= 0 || box_w <= 0 || box_h <= 0)
		return NULL;

	/*
	 * `pad` shrinks the SQUARE and not the box: the sprite still covers
	 * the cells it was asked for, so the caller's hit map, its plate and
	 * its layout are all unchanged and only the picture inside gets air.
	 * A padding that would leave no square is IGNORED and the picture is
	 * drawn at the full box size — the caller gets an icon rather than a
	 * dot, and the well it asked for is unchanged either way.
	 */
	int side = box_w < box_h ? box_w : box_h;

	side -= 2 * pad;
	if (side < 1)
		side = box_w < box_h ? box_w : box_h;
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
	/* The image owns its pixels — see to_pixman(). */
	pixman_image_set_destroy_function(dst, kicon_free_bits, px);

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

/*
 * Give the cached picture up, and the sprite slot naming it with it.
 *
 * TWO REFERENCES OR ONE, depending on whether the table still holds the
 * picture: ktui_sprite_drop() forgets a slot without calling the evictor, so
 * the reference taken when the picture was registered is this function's to
 * return. A slot the table already evicted handed that reference back itself.
 */
static void cache_drop(int i)
{
	pixman_image_t *img = cache[i].img;
	int slot = ktui_sprite_find(cache[i].key);
	const KtuiSprite *s = slot >= 0 ? ktui_sprite_get(slot) : NULL;
	int in_tbl = s && s->pix == img;

	ktui_sprite_drop(cache[i].key);
	cache[i].img = NULL;
	cache[i].key = 0;
	if (in_tbl)
		pic_unref(img);
	pic_unref(img);
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

	/*
	 * A KEY OCCUPIES ONE SLOT. Two entries under one key make the eviction
	 * of either call ktui_sprite_drop() on the key the other is still
	 * drawing with, so the live picture silently loses its sprite and is
	 * decoded again from disk on the next frame. The entry being replaced
	 * goes out through cache_drop(), which is safe here because the caller
	 * registers the new picture under the same key before anything draws.
	 */
	for (int i = 0; i < ncache; i++)
		if (cache[i].img && cache[i].key == key) {
			cache_drop(i);
			slot = i;
			break;
		}
	for (int i = 0; slot < 0 && i < ncache; i++)
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
 * An application's own icon, out of the system hicolor tree.
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

	/*
	 * A CELL SMALLER THAN 4x4 PIXELS IS NOT A PIXEL BACKEND. The console
	 * client answers one, because there are no pixels on its side of the
	 * socket; rasterising at it decodes a PNG per name to produce a
	 * picture a pixel or two across, which is a blank cell by a longer
	 * route. Nothing is opened, kicon_enabled() stays false, and every
	 * kicon_slot() answers -1 — the glyph tier, which every caller draws.
	 * A consumer with a nominal cell of its own passes that instead.
	 */
	if (cell_w < 4 || cell_h < 4)
		return -1;

	ki_cw = cell_w;
	ki_ch = cell_h;
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
	/* The names that resolved to nothing go with them, and so do the
	 * desktop entries that were read: both answer a question about what is
	 * installed, and teardown is the only point at which this library can
	 * know the answer has stopped being asked. */
	nmiss = 0;
	napp = 0;
	app_cursor = 0;
	memset(app_cache, 0, sizeof(app_cache));
	kicon_forget_paths();
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
	/* The path memo names sprite slots, and every slot it can name has
	 * just been handed back: a memo kept across this either draws nothing
	 * or draws whichever picture reclaims the index next. */
	kicon_forget_paths();
	tint_reset(ktui_theme ? kcol_find(ktui_theme->name) : NULL);
}

static uint32_t fallback_cp(void)
{
	return (ktui_caps & KT_CAP_UTF8) ? 0x2593u : (uint32_t)'#';
}

int kicon_slot(const char *name, int cw, int ch)
{
	return kicon_slot_pad(name, cw, ch, 0);
}

int kicon_slot_pad(const char *name, int cw, int ch, int pad)
{
	if (!kicon_enabled() || !name || !*name || cw < 1 || ch < 1)
		return -1;
	if (cw > 16 || ch > 16)
		return -1;
	if (pad < 0)
		pad = 0;
	pad *= ki_scale;

	uint64_t key = pic_key(name, cw, ch, pad);

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

	/*
	 * THE TABLE GAVE THE SLOT BACK BUT THE PICTURE IS STILL OURS. A slot
	 * taken back under the byte budget is routine on a surface that also
	 * draws photographs, and re-registering a picture already in memory is
	 * a hash insert where decoding it again is a PNG, a tint of every
	 * pixel and a bilinear rescale. The scan is linear over at most
	 * KI_MAX_CACHE entries and runs only on this miss, never per frame.
	 */
	for (int i = 0; i < ncache; i++) {
		if (!cache[i].img || cache[i].key != key)
			continue;
		slot = ktui_sprite_put(key, cache[i].img, cw, ch,
				       fallback_cp());
		if (slot < 0)
			return -1;	/* a table with no room is the glyph */
		pixman_image_ref(cache[i].img);
		cache[i].used = ++ki_clock;
		return slot;
	}

	/* Asked before any of the searching below, which is the whole point. */
	if (miss_known(key))
		return -1;

	int box_w = cw * ki_cw * ki_scale;
	int box_h = ch * ki_ch * ki_scale;
	/* The size the picture will be DRAWN at, not the well's — asking the
	 * atlas for 40 and then scaling it to 32 is a resample nobody needs,
	 * and the atlas has a 32 to hand. */
	int want = (box_w < box_h ? box_w : box_h) - 2 * pad;

	/* A padding that would leave no square is ignored and the picture is
	 * drawn at the full box size — the same rule fit() keeps, and the two
	 * must agree or the atlas is asked for a blob of one size and the box
	 * is filled at another. */
	if (want < 1)
		want = box_w < box_h ? box_w : box_h;

	/*
	 * THE ATLAS FIRST, and the order is the whole of this lookup.
	 *
	 * hicolor's `apps/` is a shared tree any package may write into, and it
	 * is not restricted to applications — a `folder.png` filed there by
	 * somebody would otherwise win, and every folder on the desktop would
	 * come back as that package's artwork, at 16 pixels upscaled to 32,
	 * with no tint.
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
	if (!rgba) {
		miss_add(key);
		return -1;
	}

	pixman_image_t *src = to_pixman(rgba, w, h, tint);
	free(rgba);
	if (!src)
		return -1;

	pixman_image_t *img = fit(src, box_w, box_h, pad);
	pic_unref(src);
	if (!img)
		return -1;

	int ci = cache_put(key, img);
	if (ci < 0) {
		pic_unref(img);
		return -1;
	}
	slot = ktui_sprite_put(key, img, cw, ch, fallback_cp());
	if (slot < 0) {
		cache_drop(ci);
		return -1;
	}
	/* The table's reference, taken only once it has accepted the picture:
	 * a refused put leaves this file the sole owner, and a reference taken
	 * before the put would never be given back. */
	pixman_image_ref(img);
	return slot;
}

/*
 * A SPRITE FROM BYTES, for a picture that never came from a theme.
 *
 * The body below kicon_slot_pad's search is the same body — decode, to
 * pixman, fit, cache, register — and only the source of the pixels differs,
 * so what is NOT shared is the search: there is nothing to search for. A
 * caller holding the bytes has already resolved the lookup this library
 * exists to do.
 *
 * THE KEY IS THE BYTES. A name identifies a picture only because a theme
 * says so; these have no name, and hashing the PNG means the same picture
 * published twice is one slot and one decode. The cell box and the accent
 * ride the key for pic_key's reasons — the accent because a cached picture
 * outlives a retint, even though nothing here tints.
 *
 * A DECODE FAILURE IS REMEMBERED. The bytes come from another process and a
 * surface redraws many times a second: a truncated PNG that was not memoed
 * would be re-decoded, and re-rejected, on every frame.
 */
int kicon_slot_png(const void *png, size_t len, int cw, int ch)
{
	if (!kicon_enabled() || !png || !len || cw < 1 || ch < 1)
		return -1;
	if (cw > 16 || ch > 16 || len > KICON_PNG_MAX)
		return -1;

	char tag[64];

	snprintf(tag, sizeof(tag), "|png|%d|%d|%d|%s", cw, ch, ki_scale,
		 ktui_theme ? ktui_theme->name : "");

	uint64_t key = hash64(tag, hash64_mem(png, len,
					      1469598103934665603ull));
	int slot = ktui_sprite_find(key);

	if (slot >= 0) {
		for (int i = 0; i < ncache; i++)
			if (cache[i].key == key && cache[i].img) {
				cache[i].used = ++ki_clock;
				break;
			}
		return slot;
	}
	for (int i = 0; i < ncache; i++) {
		if (!cache[i].img || cache[i].key != key)
			continue;
		slot = ktui_sprite_put(key, cache[i].img, cw, ch,
				       fallback_cp());
		if (slot < 0)
			return -1;
		pixman_image_ref(cache[i].img);
		cache[i].used = ++ki_clock;
		return slot;
	}
	if (miss_known(key))
		return -1;

	int box_w = cw * ki_cw * ki_scale;
	int box_h = ch * ki_ch * ki_scale;
	int w = 0, h = 0;
	uint8_t *rgba = ki_png_mem(png, len, &w, &h);

	if (!rgba) {
		miss_add(key);
		return -1;
	}

	pixman_image_t *src = to_pixman(rgba, w, h, 0);

	free(rgba);
	if (!src)
		return -1;

	pixman_image_t *img = fit(src, box_w, box_h, 0);

	pic_unref(src);
	if (!img)
		return -1;

	int ci = cache_put(key, img);

	if (ci < 0) {
		pic_unref(img);
		return -1;
	}
	slot = ktui_sprite_put(key, img, cw, ch, fallback_cp());
	if (slot < 0) {
		cache_drop(ci);
		return -1;
	}
	pixman_image_ref(img);
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

	pixman_image_t *img = fit(src, box_w, box_h, 0);
	pic_unref(src);
	return img;
}

void kicon_pixmap_free(pixman_image_t *img)
{
	pic_unref(img);
}

int kicon_slot_for_path(const char *path, int is_dir, int cw, int ch)
{
	char mime[128];
	char names[4][64];

	if (!kicon_enabled() || !path)
		return -1;
	if (is_dir)
		return kicon_slot("folder", cw, ch);

	uint64_t key = id_key(path);

	for (int i = 0; i < npath; i++)
		if (path_cache[i].set && path_cache[i].key == key &&
		    path_cache[i].cw == cw && path_cache[i].ch == ch)
			return path_cache[i].slot;

	int slot = -1;

	kxdg_mime_for_path(path, mime, sizeof(mime));
	int n = kxdg_mime_icon_names(mime, names, 4);
	for (int i = 0; i < n; i++) {
		slot = kicon_slot(names[i], cw, ch);
		if (slot >= 0)
			break;
	}

	int at;

	if (npath < KI_PATH_CACHE) {
		at = npath++;
	} else {
		at = path_cursor;
		path_cursor = (path_cursor + 1) % KI_PATH_CACHE;
	}
	path_cache[at].key = key;
	path_cache[at].cw = cw;
	path_cache[at].ch = ch;
	path_cache[at].slot = slot;
	path_cache[at].set = 1;
	return slot;
}

/*
 * Forget every path this library has resolved. What is remembered is a sprite
 * slot index, so the memo is only as good as the slot: the consumer calls
 * this when it re-reads the directory it is drawing, or a path whose file was
 * replaced by one of another type keeps its old icon. kicon_retint() calls it
 * for the same reason — it frees every slot the memo could name.
 */
void kicon_forget_paths(void)
{
	npath = 0;
	path_cursor = 0;
	memset(path_cache, 0, sizeof(path_cache));
}

const char *kicon_app_icon(const char *id)
{
	static char icon[KI_NAME_MAX];
	char dirs[8][512];
	char path[1024];

	if (!id || !*id)
		return NULL;

	uint64_t key = id_key(id);

	for (int i = 0; i < napp; i++)
		if (app_cache[i].set && app_cache[i].key == key)
			return app_cache[i].icon[0] ? app_cache[i].icon : NULL;

	int nd = data_dirs(dirs, 8);

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
			break;
	}

	int slot;

	if (napp < KI_APP_CACHE) {
		slot = napp++;
	} else {
		slot = app_cursor;
		app_cursor = (app_cursor + 1) % KI_APP_CACHE;
	}
	app_cache[slot].key = key;
	app_cache[slot].set = 1;
	snprintf(app_cache[slot].icon, sizeof(app_cache[slot].icon), "%s",
		 icon);
	return icon[0] ? app_cache[slot].icon : NULL;
}
