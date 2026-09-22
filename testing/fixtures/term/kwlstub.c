/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kwlstub — libkwl's three symbols, for a build with no Wayland
 *
 * `kdos-term --dump` is the whole terminal short of a display: the state
 * machine, the frame, the child on its pty and the picture. Every one of those
 * must be asserted on a bare host, and a bare host has no fcft, no pixman and
 * no wayland-client — so the goldens are rendered by a build that links this
 * instead of libkwl.
 *
 * THE PROBE ANSWERS NO, which is what makes it a stub rather than a fake.
 * `kdisp_init` then selects nothing, `kdisp_current()` is NULL, and every
 * `kwl_surface()` test in the terminal is false — exactly the state a `--dump`
 * is in on the shipped binary. A stub that claimed a display would send the
 * dump down the window path and the golden would be of something else.
 *
 * NOTHING HERE IS REACHED. The three entries exist so the link succeeds; a
 * call would mean the terminal took a path a dump cannot be on, so each one
 * aborts rather than returning a plausible value.
 * ---------------------------------
 */

#include <stdlib.h>

#include "kdisp.h"

int kwl_font_step(int step);
int kwl_frame_throttled(void);

static int stub_probe(void) { return 0; }
static int stub_init(const KDispConfig *cfg) { (void)cfg; abort(); }

const KDispImpl kwl_impl = {
	.name = "wayland (stub)",
	.probe = stub_probe,
	.init = stub_init,
};

int kwl_font_step(int step) { (void)step; abort(); }
int kwl_frame_throttled(void) { return 0; }
