/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos toggle — the switches a desktop needs at hand
 *
 *   kdos toggle                 list them and their state
 *   kdos toggle <name>          flip it
 *   kdos toggle <name> on|off   set it
 *
 * A FLAG FILE, NOT A CONFIGURATION KEY. `con.conf` documents itself as read
 * once when the session starts, so a runtime writer would make half its
 * answers come from before an edit and half from after. These are state: they
 * live under `~/.local/state/kdos/toggles/` and the session stats them on the
 * tick it already has.
 *
 * PRESENT MEANS ON. There is no file format and nothing to parse — a toggle is
 * a name and whether the file exists, which is the smallest thing that can be
 * read by a shell script, a chord and a surface without any of them agreeing
 * on a syntax first.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kdos-tools.h"

/*
 * The whole set. A toggle nobody reads is a switch that does nothing, so each
 * row names what consumes it — and a name added here without a consumer is the
 * thing this table exists to make visible.
 */
static const struct {
	const char *name;
	const char *what;
	const char *who;
} TOGGLES[] = {
	{ "stay-awake", "never save, lock or blank on idle",
	  "kdos-con's idle tick" },
	{ "night-light", "warm the palette",
	  "kdos-con and kdos-view, on the retint signal" },
	{ "dnd", "hold notifications back", "kdos-notifyd" },
};

#define NTOGGLES ((int)(sizeof(TOGGLES) / sizeof(TOGGLES[0])))

static int toggle_dir(char *out, size_t n)
{
	const char *state = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");

	if (state && *state)
		return snprintf(out, n, "%s/kdos/toggles", state) < (int)n;
	if (home && *home)
		return snprintf(out, n, "%s/.local/state/kdos/toggles",
				home) < (int)n;
	return 0;
}

static int toggle_path(const char *name, char *out, size_t n)
{
	char dir[512];

	if (!toggle_dir(dir, sizeof(dir)))
		return 0;
	return snprintf(out, n, "%s/%s", dir, name) < (int)n;
}

int kdt_toggle_on(const char *name)
{
	char path[600];

	if (!name || !toggle_path(name, path, sizeof(path)))
		return 0;
	return kb_path_exists(path);
}

static int known(const char *name)
{
	for (int i = 0; i < NTOGGLES; i++)
		if (!strcmp(TOGGLES[i].name, name))
			return 1;
	return 0;
}

/*
 * Setting one is creating or removing a file, and the directory is made on the
 * way. No temp-and-rename: an empty file either exists or does not, and a
 * half-written nothing is still nothing.
 */
static int toggle_set(const char *name, int on)
{
	char dir[512], path[600];

	if (!toggle_dir(dir, sizeof(dir)) ||
	    !toggle_path(name, path, sizeof(path)))
		return -1;
	if (on) {
		kb_mkdir_p(dir);

		FILE *f = fopen(path, "w");

		if (!f)
			return -1;
		fclose(f);
	} else {
		unlink(path);
	}
	return 0;
}

int cmd_toggle(int argc, char **argv)
{
	if (argc < 1) {
		for (int i = 0; i < NTOGGLES; i++)
			printf("%-14s %-3s  %s\n", TOGGLES[i].name,
			       kdt_toggle_on(TOGGLES[i].name) ? "on" : "off",
			       TOGGLES[i].what);
		return 0;
	}

	const char *name = argv[0];

	if (!known(name)) {
		fprintf(stderr, "kdos toggle: no such toggle '%s' — try: "
				"kdos toggle\n", name);
		return 2;
	}

	int on;

	if (argc < 2)
		on = !kdt_toggle_on(name);
	else if (!strcmp(argv[1], "on"))
		on = 1;
	else if (!strcmp(argv[1], "off"))
		on = 0;
	else {
		fprintf(stderr, "kdos toggle: %s takes on or off\n", name);
		return 2;
	}

	if (toggle_set(name, on) != 0) {
		fprintf(stderr, "kdos toggle: cannot write the state file\n");
		return 1;
	}
	printf("%s %s\n", name, on ? "on" : "off");
	return 0;
}
