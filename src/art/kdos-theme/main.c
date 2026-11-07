/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — one binary, six generators
 *
 * Run by the identity ports' build.sh, at build time, never by a session:
 * every file it writes is installed by a package, so what a machine shows is
 * what the image carries and an accent switch is a choice between files that
 * already exist.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

static void usage(FILE *to)
{
	fprintf(to,
		"kdos-theme — generate the KDOS identity kit from the palette\n"
		"\n"
		"usage: kdos-theme kde-colors      <outdir> [accent]\n"
		"       kdos-theme konsole         <outdir> [accent]\n"
		"       kdos-theme qt5ct-colors    <outdir> [accent]\n"
		"       kdos-theme wallpaper       <outdir> [accent] --src MARK.png"
		" [--size WxH]...\n"
		"       kdos-theme greeter-globals <file|-> [accent]\n"
		"       kdos-theme greeter-globals --sddm <outdir> [accent]\n"
		"       kdos-theme contrast        [-q] FILE...\n"
		"       kdos-theme accents         [--long | --default]\n"
		"\n"
		"With no accent a generator writes all of them. greeter-globals\n"
		"defaults to " KCOL_DEFAULT_NAME ", the default accent, and always\n"
		"writes the dark scheme the greeter shows for the accent named.\n");
}

static void accents(const char *flag)
{
	if (flag && !strcmp(flag, "--default")) {
		printf("%s\n", kcol_default()->name);
		return;
	}
	for (int i = 0; i < kcol_nscheme; i++) {
		const KcolScheme *sc = &kcol_schemes[i];

		if (flag && !strcmp(flag, "--long"))
			printf("%s\t%s\t%s\t%s\n", sc->name, kth_title(sc),
			       kth_is_light(sc) ? "light" : "dark",
			       kth_dark_partner(sc)->name);
		else
			printf("%s\n", sc->name);
	}
}

static int parse_size(const char *s, KthSize *out)
{
	char *x;
	long w = strtol(s, &x, 10);

	if (x == s || *x != 'x')
		return -1;
	char *end;
	long h = strtol(x + 1, &end, 10);

	if (end == x + 1 || *end || w < 1 || h < 1 || w > 16384 || h > 16384)
		return -1;
	out->w = (int)w;
	out->h = (int)h;
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-theme");

	if (argc < 2) {
		usage(stderr);
		return 2;
	}

	const char *cmd = argv[1];

	if (!strcmp(cmd, "--help") || !strcmp(cmd, "help")) {
		usage(stdout);
		return 0;
	}
	if (!strcmp(cmd, "accents")) {
		if (argc > 3 || (argc == 3 && strcmp(argv[2], "--long") &&
				 strcmp(argv[2], "--default")))
			kb_die("usage: kdos-theme accents [--long | --default]");
		accents(argc == 3 ? argv[2] : NULL);
		return 0;
	}
	if (!strcmp(cmd, "contrast")) {
		int quiet = argc > 2 && !strcmp(argv[2], "-q");
		int first = 2 + quiet;

		if (first >= argc)
			kb_die("usage: kdos-theme contrast [-q] FILE...");
		return kth_contrast(argc - first, argv + first, quiet);
	}

	const char *pos[2] = { NULL, NULL };
	const char *src = NULL;
	int npos = 0, sddm = 0, nsize = 0;
	KthSize sizes[16];

	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--src") && i + 1 < argc) {
			src = argv[++i];
		} else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
			if (nsize == (int)(sizeof(sizes) / sizeof(sizes[0])))
				kb_die("too many --size options");
			if (parse_size(argv[++i], &sizes[nsize++]) < 0)
				kb_die("--size wants WxH, not '%s'", argv[i]);
		} else if (!strcmp(argv[i], "--sddm")) {
			sddm = 1;
		} else if (argv[i][0] == '-' && argv[i][1]) {
			kb_die("unknown option %s", argv[i]);
		} else if (npos < 2) {
			pos[npos++] = argv[i];
		} else {
			kb_die("unexpected argument %s", argv[i]);
		}
	}

	if (!pos[0]) {
		usage(stderr);
		return 2;
	}
	if ((src || nsize) && strcmp(cmd, "wallpaper"))
		kb_die("--src and --size belong to wallpaper");
	if (sddm && strcmp(cmd, "greeter-globals"))
		kb_die("--sddm belongs to greeter-globals");

	if (!strcmp(cmd, "kde-colors"))
		return kth_kde_colors(pos[0], pos[1]);
	if (!strcmp(cmd, "konsole"))
		return kth_konsole(pos[0], pos[1]);
	if (!strcmp(cmd, "qt5ct-colors"))
		return kth_qt5ct(pos[0], pos[1]);
	if (!strcmp(cmd, "greeter-globals"))
		return sddm ? kth_greeter_sddm(pos[0], pos[1])
			    : kth_greeter_merge(pos[0], pos[1]);
	if (!strcmp(cmd, "wallpaper")) {
		if (!src)
			kb_die("wallpaper needs --src MARK.png");
		return kth_wallpaper(pos[0], pos[1], src, sizes, nsize);
	}

	kb_die("unknown command '%s' (try: kdos-theme --help)", cmd);
}
