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
 * ~/.config/kdos/res.conf — `key = value`, the panel.conf shape.
 *
 * An unknown key is REPORTED BY NAME rather than ignored, which is the promise
 * comp.conf and panel.conf already make: a line that does not take effect and
 * says nothing is indistinguishable from a setting that does nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "res.h"

ResConf RC = {
	.interval_ms = 1000,
	.units_1024 = 1,
	.fahrenheit = 0,
	.cpu_of_machine = 0,
	.memory_pss = 0,
	.kernel_threads = 0,
	.virtual_drives = 0,
	.virtual_net = 0,
	.icons = 1,
	.sort = "cpu",
	.columns = "",
};

const char *res_conf_path(void)
{
	static char path[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/res.conf", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/res.conf", home);
	else
		return NULL;
	return path;
}

static int yes(const char *v)
{
	return !strcmp(v, "yes") || !strcmp(v, "1") || !strcmp(v, "true") ||
	       !strcmp(v, "on");
}

void res_conf_load(void)
{
	const char *path = res_conf_path();
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

		if (!strcmp(k, "interval")) {
			int ms = atoi(v);
			/*
			 * Floored rather than trusted. A monitor asked to
			 * sample every 10 ms becomes the load it is measuring,
			 * and the rates it prints are then mostly its own.
			 */
			RC.interval_ms = ms < 200 ? 200 : ms > 60000 ? 60000 : ms;
		} else if (!strcmp(k, "units")) {
			RC.units_1024 = strcmp(v, "1000") != 0;
		} else if (!strcmp(k, "temperature")) {
			RC.fahrenheit = (*v == 'f' || *v == 'F');
		} else if (!strcmp(k, "cpu_percent")) {
			RC.cpu_of_machine = !strcmp(v, "machine");
		} else if (!strcmp(k, "memory")) {
			RC.memory_pss = !strcmp(v, "pss");
		} else if (!strcmp(k, "kernel_threads")) {
			RC.kernel_threads = yes(v);
		} else if (!strcmp(k, "virtual_drives")) {
			RC.virtual_drives = yes(v);
		} else if (!strcmp(k, "virtual_net")) {
			RC.virtual_net = yes(v);
		} else if (!strcmp(k, "icons")) {
			RC.icons = yes(v);
		} else if (!strcmp(k, "sort")) {
			kb_strlcpy(RC.sort, v, sizeof(RC.sort));
		} else if (!strcmp(k, "columns")) {
			kb_strlcpy(RC.columns, v, sizeof(RC.columns));
		} else {
			fprintf(stderr, "kdos-res: %s: unknown key '%s'\n",
				path, k);
		}
cont:
		if (nl)
			*nl = '\n';
	}
	free(data);
}
