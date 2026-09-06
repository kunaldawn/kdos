/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   What the view's own files hand each other.
 * ---------------------------------
 */

#ifndef KDOS_VIEW_H
#define KDOS_VIEW_H

/*
 * Rasterise the frame libktui is holding and write it as a PNG.
 *
 * A FONT MUST BE LOADED before this is called — it rasterises through
 * `kcell_paint()`, the same painter a screen uses, and there is nothing to
 * draw with otherwise. Returns 0, or -1 with the reason on stderr.
 *
 * `cx`, `cy`, `cw`, `ch` are a rectangle in CELLS, and a zero width or height
 * means the whole grid. The unit is cells because the caller is the session,
 * which has no other; the pixels are worked out here, where the font is.
 *
 * Present only where the rasteriser is (`KDOS_VIEW_SHOT`), the same rule the
 * KMS and cast modes keep.
 */
#ifdef KDOS_VIEW_SHOT
int view_shot_png(const char *path, int scale, int cx, int cy, int cw, int ch);
#endif

/*
 * PICTURES OUT OF A TERMINAL VIEW, when the terminal it runs in will draw them.
 *
 * `view_ttypix_probe()` asks, consuming every reply byte — one left behind is
 * decoded as keystrokes and typed into the session being viewed. It returns
 * non-zero when a protocol was found, and writes the terminal's cell size in
 * pixels, which is what every picture is then scaled to.
 *
 * `view_ttypix_install()` must be given the backend that is installed NOW and
 * its result handed to `ktui_backend_set()`: it wraps rather than replaces, and
 * capturing the pointer afterwards captures itself.
 *
 * Present only where the encoders are (`KDOS_VIEW_TTYPIX`), the same rule the
 * KMS, cast and shot modes keep.
 */
#ifdef KDOS_VIEW_TTYPIX
int view_ttypix_probe(int *cell_w, int *cell_h);
const KtuiBackend *view_ttypix_install(const KtuiBackend *base);
void view_ttypix_pointer(int x, int y);
void view_ttypix_caret(int x, int y);
void view_ttypix_forget(int view_slot);
void view_ttypix_shutdown(void);
#endif

#endif /* KDOS_VIEW_H */
