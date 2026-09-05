/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * ~/.config/kdos/term.conf — `key = value`, the res.conf shape.
 *
 * An unknown key is REPORTED BY NAME rather than ignored: a line that does not
 * take effect and says nothing is indistinguishable from a setting that does
 * nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"

TermConf TC = {
	.shell = "",
	.font = "",
	.cols = 80,
	.rows = 24,
	.scrollback = 2000,
	.images = 1,
	/*
	 * A megabyte for one picture. The cap is what makes a payload
	 * bounded before anything allocates for it, and a terminal is
	 * reachable by `cat` on a file somebody sent you.
	 */
	.image_max = 1024,
	/* Wider than a screen is wider than anything can be seen at, and it
	 * bounds what one sequence can ask this program to scale. */
	.image_cells = 200,
	/* On: an unbracketed paste carrying a newline executes, and a person
	 * who meant it is one keystroke away from agreeing. */
	.paste_guard = 1,
};

const char *term_conf_path(void)
{
	static char path[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/term.conf", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/term.conf", home);
	else
		return NULL;
	return path;
}

static int yes(const char *v)
{
	return !strcmp(v, "yes") || !strcmp(v, "1") || !strcmp(v, "true") ||
	       !strcmp(v, "on");
}

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

void term_conf_load(void)
{
	const char *path = term_conf_path();

	if (!path)
		return;

	char *data = kb_read_all(path, NULL);

	if (!data)
		return;		/* absent is a working default */

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			goto cont;

		char *eq = strchr(line, '=');

		if (!eq)
			goto cont;
		*eq = 0;

		char *k = line, *v = eq + 1;

		for (char *e = k + strlen(k); e > k && (e[-1] == ' ' || e[-1] == '\t'); e--)
			e[-1] = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		for (char *e = v + strlen(v); e > v && (e[-1] == ' ' || e[-1] == '\t'); e--)
			e[-1] = 0;

		if (!strcmp(k, "shell")) {
			kb_strlcpy(TC.shell, v, sizeof(TC.shell));
		} else if (!strcmp(k, "font")) {
			kb_strlcpy(TC.font, v, sizeof(TC.font));
		} else if (!strcmp(k, "columns")) {
			TC.cols = clamp(atoi(v), 20, 1000);
		} else if (!strcmp(k, "rows")) {
			TC.rows = clamp(atoi(v), 4, 1000);
		} else if (!strcmp(k, "scrollback")) {
			TC.scrollback = clamp(atoi(v), 0, 200000);
		} else if (!strcmp(k, "images")) {
			TC.images = yes(v);
		} else if (!strcmp(k, "image_max")) {
			TC.image_max = clamp(atoi(v), 4, 65536);
		} else if (!strcmp(k, "image_cells")) {
			TC.image_cells = clamp(atoi(v), 1, 1000);
		} else if (!strcmp(k, "paste_guard")) {
			TC.paste_guard = yes(v);
		} else {
			fprintf(stderr, "kdos-term: %s: unknown key '%s'\n",
				path, k);
		}
cont:
		if (nl)
			*nl = '\n';
	}
	free(data);
}
