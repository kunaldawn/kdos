/*
 * The shell's front ends, drawn OFFSCREEN with no compositor.
 *
 * `kdos-menu --dump`, `kdos-pick --dump`, `kdos-launcher --dump` and
 * `kdos-cal --dump` render one frame into libktui's cell buffer and print it as
 * text — the same seam `kdos-shell --dump` and `kdosbuild --preview` already
 * have, and it exists for the same reason: every geometry defect this toolkit
 * has shipped (text over a box border, a button on top of a hint, a column
 * drifted out from under its header) was invisible to the compiler and to a
 * test suite with no terminal.
 *
 * That path touches NO Wayland and NO fcft, which is what lets this harness
 * exist: libkwl is stubbed out here, so the front ends link against libktui
 * alone and the layouts can be looked at on any host — including one with
 * neither wlroots nor fcft installed, which is most of them.
 *
 * Not shipped. testing/selftest.sh compiles it and nothing else does.
 */
#include <stdio.h>
#include <string.h>

#include "shell.h"

/* libkwl, stubbed. A dump never reaches any of it; the symbols exist only
 * because the same translation units carry the interactive path. */
int kwl_init(const void *cfg) { (void)cfg; return -1; }
void kwl_shutdown(void) {}
int kwl_should_close(void) { return 1; }
int kwl_cell_w(void) { return 16; }
int kwl_cell_h(void) { return 32; }
void kwl_input_cells(const KRect *r, int n) { (void)r; (void)n; }
void *kwl_display(void) { return NULL; }
void *kwl_seat(void) { return NULL; }
void kwl_overlay_hide(void) {}
int kwl_overlay_show(int c, int r) { (void)c; (void)r; return 0; }
void kwl_overlay_resize(int c, int r) { (void)c; (void)r; }
int kwl_fd(void) { return -1; }
void kwl_pump(void) {}

/* The accent is read from a file the test's HOME does not have; the default is
 * phosphor, which is what a fresh install wears. */
void sh_theme_from_cache(void) {}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: dumpcheck <cal|menu|launcher|pick> …\n");
		return 2;
	}
	if (!strcmp(argv[1], "cal"))
		return cal_main(argc - 1, argv + 1);
	if (!strcmp(argv[1], "menu"))
		return menu_main(argc - 1, argv + 1);
	if (!strcmp(argv[1], "launcher"))
		return launcher_main(argc - 1, argv + 1);
	if (!strcmp(argv[1], "pick"))
		return pick_main(argc - 1, argv + 1);
	fprintf(stderr, "dumpcheck: no front end named '%s'\n", argv[1]);
	return 2;
}
