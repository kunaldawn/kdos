/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — one binary, three generators
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

static const char *pick(const char *flag, const char *env, const char *def)
{
	if (flag)
		return flag;
	const char *e = getenv(env);
	return (e && *e) ? e : def;
}

static void usage(void)
{
	printf("kdos-theme — regenerate the KDOS artwork for an accent\n\n"
	       "usage: kdos-theme gtk     <out-dir> [accent] [--src DIR]\n"
	       "       kdos-theme icons   <out-dir> [accent] [--src DIR] [--marks DIR]\n"
	       "       kdos-theme cursors <out-dir> [accent] [--src DIR]\n"
	       "       kdos-theme accents\n\n"
	       "The accent defaults to phosphor. --src defaults to the\n"
	       "installed artwork under /usr/share/kdos.\n");
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-theme");

	if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
		usage();
		return argc < 2 ? 1 : 0;
	}

	const char *cmd = argv[1];

	if (!strcmp(cmd, "accents")) {
		for (int i = 0; i < kcol_nscheme; i++)
			printf("%s\n", kcol_schemes[i].name);
		return 0;
	}

	const char *out = NULL, *src = NULL, *marks = NULL, *accent = "phosphor";
	int positional = 0;

	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--src") && i + 1 < argc) {
			src = argv[++i];
		} else if (!strcmp(argv[i], "--marks") && i + 1 < argc) {
			marks = argv[++i];
		} else if (argv[i][0] == '-') {
			kb_die("unknown option %s", argv[i]);
		} else if (positional == 0) {
			out = argv[i];
			positional++;
		} else if (positional == 1) {
			accent = argv[i];
			positional++;
		} else {
			kb_die("unexpected argument %s", argv[i]);
		}
	}

	if (!out) {
		usage();
		return 1;
	}

	const KcolScheme *sc = kcol_find(accent);
	if (!sc)
		kb_die("unknown accent '%s' (try: kdos-theme accents)", accent);

	/* --src wins, then the environment, then the installed location. The
	 * env hooks are what let 06_packaging and a sandbox run this against a
	 * tree that is not /usr/share yet. */
	if (!strcmp(cmd, "gtk"))
		return gen_gtk(pick(src, "KDOS_GTK_SRC", GTK_SRC_DEFAULT), out, sc);
	if (!strcmp(cmd, "icons"))
		return gen_icons(pick(src, "KDOS_ICON_ART", ICON_ART_DEFAULT),
				 pick(marks, "KDOS_ICON_MARKS", ICON_MARKS_DEFAULT),
				 out, sc);
	if (!strcmp(cmd, "cursors"))
		return gen_cursors(pick(src, "KDOS_CURSOR_ART", CURSOR_ART_DEFAULT),
				   out, sc);

	kb_die("unknown command '%s'", cmd);
}
