/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkicon — the icon, resolved to a sprite slot
 *
 * The one job: a NAME (or a file path) becomes a libktui sprite slot, or -1.
 * Minus one is not a failure — it is a tty, an install with no artwork,
 * `icons = off`, and a name nothing on this machine has a picture for. Every
 * caller draws its glyph tier then, exactly as it did before this library
 * existed, which is the rule the whole icon layer is built under.
 *
 * NOTHING PIXMAN OR PNG APPEARS IN THIS HEADER. The dump harness that renders
 * the shell's front ends offscreen links neither, and it stubs this file's
 * five functions to "no icons" — so a golden frame is the character grid, and
 * a layout that only lines up when the icons load is a layout that is broken.
 *
 * Two sources, and they are different in kind:
 *
 *   /usr/share/icons/hicolor/<size>/apps/<name>.png   the alien apps' own
 *       icons, installed by 06_packaging/01_appbox.sh. Already PNG, already
 *       at fixed sizes, and NEVER recoloured — a phosphor Firefox logo is
 *       vandalism, the same rule kdos-theme icons already keeps.
 *
 *   /usr/share/kdos/icons/atlas.kia                   the theme's own set
 *       (mimetypes, places, devices, status, actions), rasterised from the
 *       vendored Papirus SVGs on the maintainer's HOST by
 *       kdos-icons/genatlas.py, because there is no SVG parser anywhere in
 *       this tree and there is not going to be one. Shipped in upstream's own
 *       colours and tinted at load through kcol_remap — the wallpaper's
 *       pipeline — so one atlas serves all four accents and `kdos theme amber`
 *       retints it live.
 * ---------------------------------
 */

#ifndef KICON_H
#define KICON_H

#include <stdint.h>

/*
 * DECLARED, NOT INCLUDED. kicon.h is read by consumers that draw only cells
 * and do not link pixman — the offscreen dump harness is one, and pulling
 * pixman's header in here made every one of them fail to compile. C11 permits
 * a typedef repeated identically, so a translation unit that includes
 * <pixman.h> as well is unaffected.
 *
 * pixman's own type is a UNION, not a struct — declaring it wrong is a hard
 * error the moment a translation unit sees both this and <pixman.h>.
 */
typedef union pixman_image pixman_image_t;

/*
 * `cell_w`/`cell_h` are the backend's cell size in pixels and `scale` its
 * integer output scale — an icon is chosen and rasterised at cell*scale, or a
 * HiDPI panel gets a blurred one. Returns 0 when at least one source answered,
 * -1 when there is no artwork at all (which is a working desktop, not an
 * error). Safe to call twice; the second call is a re-scan.
 */
int kicon_init(int cell_w, int cell_h, int scale);
void kicon_finish(void);

/* `icons = off` in comp.conf, and the `--no-icons` flag every front end takes.
 * Off makes kicon_slot() answer -1 for everything without unloading anything,
 * so it can be toggled live. */
void kicon_set_enabled(int on);
int kicon_enabled(void);

/*
 * Resolve `name` into a sprite covering `cw` x `ch` CELLS, or -1.
 *
 * The picture is drawn as the largest SQUARE that fits the cell box and is
 * centred in it — a cell is 16x32 on this desktop, so "one icon" is two cells
 * wide and one tall, and asking for a 1x1 icon gets a 16x16 picture in the
 * middle of a 16x32 box rather than a stretched one. Nothing here ever changes
 * an icon's aspect.
 */
int kicon_slot(const char *name, int cw, int ch);

/* A file's icon, by MIME type — the same resolution `kdos-appbox open` does
 * (/usr/share/mime/globs, LONGEST matching suffix wins, or every .tar.gz gets
 * the decompressor's icon). `is_dir` short-circuits to the folder icon. */
int kicon_slot_for_path(const char *path, int is_dir, int cw, int ch);

/* The `Icon=` of a desktop entry id or an app_id, resolved through the XDG
 * data dirs. Returns NULL when there is no entry or it names no icon. The
 * buffer is static and valid until the next call. */
const char *kicon_app_icon(const char *id);

/* The accent changed: drop every tinted picture and every sprite slot. The
 * next kicon_slot() re-tints from the atlas. App icons are not recoloured, so
 * they survive — this is a re-TINT, not a re-scan. */
void kicon_retint(void);

/* How many pictures are cached, for `--dump` and the selftest. */
int kicon_cached(void);

/*
 * The icon as PIXELS, scaled into box_w x box_h and owned by the caller.
 *
 * `kicon_slot` is what a consumer wants when the icon occupies cells of its
 * own; this is what a kcell canvas wants, where the icon and the text beside
 * it are placed at pixel positions rather than cell ones. Same lookup order —
 * the atlas, then the hicolor tree — because two answers to what a name means
 * would put one picture in a tile and a different one next to it.
 */
pixman_image_t *kicon_pixmap(const char *name, int box_w, int box_h);
void kicon_pixmap_free(pixman_image_t *img);

#endif /* KICON_H */
