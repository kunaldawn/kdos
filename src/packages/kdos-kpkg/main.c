/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-kpkg — basename dispatch
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "kdos-kpkg.h"

/*
 * The format is GUARDED in both of these, as it is in libkbase's pair. A null
 * format is undefined in vprintf, and the sanitiser build's interprocedural
 * pass cannot prove one non-null across a whole program compiled in a single
 * line — so without the guard `-Wformat-overflow` refuses to build at all.
 */
void kp_msg(const char *fmt, ...)
{
	va_list ap;
	fputs("==> ", stdout);
	va_start(ap, fmt);
	if (fmt)
		vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

void kp_err(const char *fmt, ...)
{
	va_list ap;
	fputs("ERROR: ", stderr);
	va_start(ap, fmt);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static const struct {
	const char *name;
	int (*fn)(int, char **);
} TOOLS[] = {
	{ "kpkgdepends", depends_main },
	{ "kpkgadd", add_main },
	{ "kpkgdel", del_main },
	{ "kpkgbuild", build_main },
	{ "kpkg", front_main },
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

	if (argc > 1)
		for (int i = 0; i < NTOOLS; i++)
			if (!strcmp(TOOLS[i].name, argv[1])) {
				kb_set_progname(argv[1]);
				return TOOLS[i].fn(argc - 1, argv + 1);
			}

	fprintf(stderr, "kdos-kpkg: no tool named '%s'\n", self);
	return 1;
}
