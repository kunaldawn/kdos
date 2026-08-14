/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shell — basename dispatch
 *
 * Installed under every name it answers to, like kdos-tools and kdos-appbox.
 * The launcher is a separate PROCESS but not a separate program: it shares the
 * font cache, the palette and the whole cell-grid toolkit with the panel, and
 * having one binary is what keeps them from drifting into two different
 * pictures of the same desktop.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "shell.h"

static const struct {
	const char *name;
	int (*fn)(int, char **);
} TOOLS[] = {
	{ "kdos-shell", panel_main },
	{ "kdos-launcher", launcher_main },
	{ "kdos-menu", menu_main },
	{ "kdos-desk", desk_main },
	{ "kdos-pick", pick_main },
	{ "kdos-ascii", asciicmd_main },
	{ "kdos-run", run_main },
	{ "kdos-prompt", prompt_main },
	{ "kdos-notifyd", notifyd_main },
	{ "kdos-osd", osd_main },
	{ "kdos-cal", cal_main },
	{ "kdos-display", display_main },
};
#define NTOOLS ((int)(sizeof(TOOLS) / sizeof(TOOLS[0])))

int main(int argc, char **argv)
{
	const char *self = strrchr(argv[0], '/');
	self = self ? self + 1 : argv[0];

	for (int i = 0; i < NTOOLS; i++)
		if (!strcmp(TOOLS[i].name, self))
			return TOOLS[i].fn(argc, argv);

	/* Invoked under an unexpected name: the first argument selects, so
	 * `kdos-shell kdos-launcher` works before the symlink exists. */
	if (argc > 1)
		for (int i = 0; i < NTOOLS; i++)
			if (!strcmp(TOOLS[i].name, argv[1]))
				return TOOLS[i].fn(argc - 1, argv + 1);

	fprintf(stderr, "kdos-shell: no tool named '%s'\navailable:", self);
	for (int i = 0; i < NTOOLS; i++)
		fprintf(stderr, " %s", TOOLS[i].name);
	fputc('\n', stderr);
	return 2;
}
