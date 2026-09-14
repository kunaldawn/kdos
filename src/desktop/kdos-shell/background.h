/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The console desktop's character-art background — see background.c
 *
 * A HEADER OF ITS OWN, and not two lines in `shell.h`: that header pulls in
 * Wayland, the icon layer and the chrome band, none of which turning a text
 * file into cells has any use for. Keeping the interface here is what lets the
 * loader be compiled and driven on a host with no compositor at all, which is
 * where the parse and the colour reduction are actually checked.
 * ---------------------------------
 */

#ifndef SH_BACKGROUND_H
#define SH_BACKGROUND_H

#include <stddef.h>

#include "ktui.h"

/* The art as cells, through libkvt's parser and its render boundary, so SGR
 * colour reduces to the theme's slots and follows `kdos theme`. NULL when
 * there is nothing to draw; the caller frees. */
KtuiCell *sh_bg_load(const char *path, int *out_w, int *out_h);

/* The file to load: the person's own `background.<ext>`, else the shipped
 * piece the state file names. 0 when there is none, which is the theme's
 * ground. The shipped pieces are `KB_BACKGROUND_DIR/<name>.<ext>`, and the
 * state file holds a NAME and never a path: a separator in it would be a path
 * somebody chose.
 *
 * `*is_image` says which of the two kinds it found: character art to be put
 * through sh_bg_load(), or a picture for the caller's own decoder. DECIDED BY
 * THE EXTENSION AND NOTHING ELSE, because this file links no pixel code at
 * all — see the note above — and the extension is what the person named the
 * file. A caller with no pixel path draws neither and gets the theme's
 * ground. */
int sh_bg_path(char *out, size_t n, int *is_image);

#endif /* SH_BACKGROUND_H */
