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
 * Present only where the rasteriser is (`KDOS_VIEW_SHOT`), the same rule the
 * KMS and cast modes keep.
 */
#ifdef KDOS_VIEW_SHOT
int view_shot_png(const char *path, int scale);
#endif

#endif /* KDOS_VIEW_H */
