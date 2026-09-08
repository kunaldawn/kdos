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
 * live under `~/.local/state/kdos/toggles/`, and their readers either stat
 * them on a tick they already run or are sent the retint signal below.
 *
 * THE PATH IS libkbase'S. `kb_toggle_on()` and `kb_toggle_set()` are the only
 * two places it is spelled, so a surface and this command cannot disagree
 * about where a switch lives.
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
	/* Its consumer reads it on the retint signal rather than on a tick, so
	 * setting it has to send that signal or nothing on screen moves. */
	int retint;
} TOGGLES[] = {
	{ "stay-awake", "never save, lock or blank on idle",
	  "kdos-con's idle tick", 0 },
	{ "night-light", "warm the palette",
	  "kdos-con and kdos-view, on the retint signal", 1 },
	{ "dnd", "hold notifications back", "kdos-notifyd", 0 },
};

#define NTOGGLES ((int)(sizeof(TOGGLES) / sizeof(TOGGLES[0])))

static int find(const char *name)
{
	for (int i = 0; i < NTOGGLES; i++)
		if (!strcmp(TOGGLES[i].name, name))
			return i;
	return -1;
}

int cmd_toggle(int argc, char **argv)
{
	if (argc < 1) {
		for (int i = 0; i < NTOGGLES; i++)
			printf("%-14s %-3s  %s\n", TOGGLES[i].name,
			       kb_toggle_on(TOGGLES[i].name) ? "on" : "off",
			       TOGGLES[i].what);
		return 0;
	}

	const char *name = argv[0];
	int which = find(name);

	if (which < 0) {
		fprintf(stderr, "kdos toggle: no such toggle '%s' — try: "
				"kdos toggle\n", name);
		return 2;
	}

	int on;

	if (argc < 2)
		on = !kb_toggle_on(name);
	else if (!strcmp(argv[1], "on"))
		on = 1;
	else if (!strcmp(argv[1], "off"))
		on = 0;
	else {
		fprintf(stderr, "kdos toggle: %s takes on or off\n", name);
		return 2;
	}

	if (kb_toggle_set(name, on) != 0) {
		fprintf(stderr, "kdos toggle: cannot write the state file\n");
		return 1;
	}
	/* AFTER the file: the surfaces re-read it the moment the signal
	 * lands, so a signal sent first is one they answer with the old
	 * state. */
	if (TOGGLES[which].retint)
		kdt_reload_session();
	printf("%s %s\n", name, on ? "on" : "off");
	return 0;
}
