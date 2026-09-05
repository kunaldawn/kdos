/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos places — the column, from a prompt
 *
 * THE SAME READER THE SURFACES USE. `kxdg_places()` answers the desktop's
 * Places menu, the Start menu's column and the chooser's `Ctrl+P`; a command
 * that walked `user-dirs.dirs` itself would be a fourth answer to where a
 * person's directories are, which is the thing that library call exists to
 * have stopped.
 *
 * `add` IS WHY THIS EXISTS. The desktop can add a folder from its context
 * menu and `mc` cannot, so `F2` in the file manager had no way to say "keep
 * this one" — and the whole point of the column is that it is the places
 * somebody said, rather than the ones a program guessed.
 * ---------------------------------
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kdos-tools.h"
#include "kxdg.h"

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos places                 the column, one per line\n"
		"       kdos places add DIR [NAME]  keep one\n"
		"\nThe same list kdos-start, kdos-menu and kdos-pick show.\n");
	return 2;
}

int kdt_places(int argc, char **argv)
{
	KxdgPlace p[KXDG_PLACES_MAX];
	int n;

	if (argc && !strcmp(argv[0], "add")) {
		/* PATH_MAX and not a smaller round number: realpath writes up
		 * to that whatever the argument's length, and glibc's fortified
		 * form aborts on a buffer that cannot hold it. */
		char real[PATH_MAX];
		const char *dir, *name;
		int r;

		if (argc < 2)
			return usage();
		/*
		 * ABSOLUTE, because the row is read back by a program with a
		 * different working directory. `mc` hands `%d`, which already
		 * is one, but a person typing `kdos places add .` means the
		 * directory they are standing in.
		 */
		if (!realpath(argv[1], real)) {
			fprintf(stderr, "kdos places: %s: no such directory\n",
				argv[1]);
			return 1;
		}
		name = argc > 2 ? argv[2] : kb_basename(real);
		dir = real;
		r = kxdg_places_add(name, dir);
		if (r == 0)
			printf("%s is now a place\n", name);
		else if (r == 1)
			printf("%s is already a place\n", dir);
		else
			fprintf(stderr, "kdos places: cannot add %s\n", dir);
		return r < 0 ? 1 : 0;
	}
	if (argc)
		return usage();

	n = kxdg_places(p, KXDG_PLACES_MAX);
	for (int i = 0; i < n; i++)
		printf("%-20s %s\n", p[i].name, p[i].path);
	return 0;
}
