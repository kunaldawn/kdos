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

#include <stddef.h>

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
/*
 * THE FONT CHORDS' ARITHMETIC, and the only part of them that runs without a
 * screen.
 *
 * `view_font_state_path()` writes `~/.local/state/kdos/con-font`, the stepped
 * size's override; non-zero when it fits.
 *
 * `view_font_stepped()` writes `base` with its size moved by `step`. The size
 * rides in a fontconfig name as `:size=N` points or `:pixelsize=N`; a name
 * carrying neither is treated as the default's `size=11` and gains it, and
 * whatever follows the size is kept because it is what the person wrote.
 * CLAMPED, because fontconfig will happily return a two-pixel face and a
 * screen of unreadable specks is not a step a chord can undo. Non-zero when
 * the result fits.
 */
int view_font_state_path(char *out, size_t n);
int view_font_stepped(const char *base, int step, char *out, size_t n);

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
