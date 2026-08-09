/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — basename dispatch
 *
 * Installed under every name it answers to. Same trick kdos-appbox uses for
 * the alien-app shims, and for the same reason: one binary, no shell wrapper
 * anywhere in the chain.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kdos-tools.h"

static const struct {
	const char *name;
	int (*fn)(int, char **);
} TOOLS[] = {
	{ "kdos", kdos_main },
	{ "ksvc", ksvc_main },
	{ "service", ksvc_main },
	{ "kdos-getty", getty_main },
	{ "kdos-shot", shot_main },
	{ "kdos-banner", banner_main },
	{ "kdos-fetch-app", fetch_app_main },
	{ "kdos-fetch-static", fetch_static_main },
};
#define NTOOLS ((int)(sizeof(TOOLS) / sizeof(TOOLS[0])))

int main(int argc, char **argv)
{
	const char *self = strrchr(argv[0], '/');
	self = self ? self + 1 : argv[0];

	kb_set_progname(self);

	for (int i = 0; i < NTOOLS; i++)
		if (!strcmp(TOOLS[i].name, self))
			return TOOLS[i].fn(argc, argv);

	/* Invoked under its own name: the first argument selects the tool, so
	 * `kdos-tools service list` works without the symlinks existing yet. */
	if (argc > 1) {
		for (int i = 0; i < NTOOLS; i++)
			if (!strcmp(TOOLS[i].name, argv[1])) {
				kb_set_progname(argv[1]);
				return TOOLS[i].fn(argc - 1, argv + 1);
			}
	}

	fprintf(stderr, "kdos-tools: no tool named '%s'\navailable:", self);
	for (int i = 0; i < NTOOLS; i++)
		fprintf(stderr, " %s", TOOLS[i].name);
	fputc('\n', stderr);
	return 1;
}
