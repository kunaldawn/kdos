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
#include "kcon.h"	/* kcon_impl */
#include "kwl.h"	/* kwl_impl — naming these is what links each one in */

/* See the declaration: naming kwl_impl is what links Wayland into this
 * program. A console-only build would name a different one, or none. */
const KDispImpl *const kdos_disp[] = { &kcon_impl, &kwl_impl };
const int kdos_disp_n = 2;


static const struct {
	const char *name;
	int (*fn)(int, char **);
} TOOLS[] = {
	{ "kdos-shell", panel_main },
	{ "kdos-start", start_main },
	{ "kdos-launcher", launcher_main },
	{ "kdos-menu", menu_main },
	{ "kdos-desk", desk_main },
	{ "kdos-pick", pick_main },
	{ "kdos-ascii", asciicmd_main },
	{ "kdos-run", run_main },
	{ "kdos-prompt", prompt_main },
	{ "kdos-notifyd", notifyd_main },
	{ "kdos-notify", notify_main },
	{ "kdos-osd", osd_main },
	{ "kdos-cal", cal_main },
	{ "kdos-display", display_main },
	{ "kdos-keys", keys_main },
	{ "kdos-teams", teams_main },
	{ "kdos-saver", saver_main },
	{ "kdos-slit", slit_main },
	{ "kdos-doc", doc_main },
	{ "kdos-settings", settings_main },
	{ "kdos-openwith", openwith_main },
	{ "kdos-audio", audio_main },
	{ "kdos-net", net_main },
	{ "kdos-bt", bt_main },
	{ "kdos-devices", devices_main },
	{ "kdos-clip", clip_main },
	{ "kdos-status", status_main },
	{ "kdos-tip", tip_main },
	{ "kdos-ime", ime_main },
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
