/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-ports-helper — one host port into this slot, prebuilt or built here
 *
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <port>
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-builder install \
 *       [--build-deps=prebuilt|source] <port>
 *
 * Discover's KDOS backend runs one of these, through pkexec, when somebody
 * installs host software the running system lacks. Each is the whole of the
 * root side of its button, so it takes one verb and one port name, and the
 * name is checked before it means anything: pkexec's grant names a program,
 * never its arguments, and /usr/bin/kdos under the same grant would reach
 * every subcommand.
 *
 * ONE SOURCE, TWO PROGRAMS, TWO GRANTS. kdos-ports-helper takes the port
 * prebuilt from the signed binhost, under io.kdos.ports.install.
 * kdos-ports-builder, this file compiled with KPH_BUILD, builds the port on
 * this computer under io.kdos.ports.build, which asks for the password every
 * time: it runs a recipe's build.sh as root, for minutes or hours, and a kept
 * grant for the first would otherwise cover it. pkexec picks the action by
 * the program's path, so the two routes are two programs.
 *
 * THE INSTALL IS `kdos update install`'S, never a second copy of it. That
 * command owns everything the route decides: the signed index `kdos update
 * fetch` keeps in /var/lib/kdos/binhost, downloading each missing package and
 * checking it against that index, the build environment of
 * /etc/kdos/chroot.env, fetching and building the sources of a source
 * install, recording each package's route in /var/lib/kdos/origin, and adding
 * the port to /var/lib/kdos/world so the next update keeps it. A copy here
 * would be a second answer to each of those, and the first to drift. The
 * builder passes --build-deps only when asked; without it, `build-deps =` in
 * /etc/kdos/update.conf decides.
 *
 * THE CATALOGUE IS WRITTEN AGAIN after an install, so Discover lists the new
 * program as installed host software at its next refresh. A catalogue that
 * could not be written leaves the port installed and is said, not failed.
 *
 * The environment pkexec hands over is replaced by a fixed PATH and the C
 * locale before anything runs, so nothing the caller set reaches a root
 * process.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"

#define SAFE_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

/* A kpkg package name: the characters `kdos update` accepts for the world
 * set, never starting with `.` or `-`, so the name cannot be read as an
 * option by anything it is handed to. */
static int kph_name_ok(const char *s)
{
	if (!*s || strlen(s) >= 128 || *s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '+' || *c == '.' ||
		      *c == '_' || *c == '-'))
			return 0;
	return 1;
}

#ifdef KPH_BUILD
#define KPH_NAME "kdos-ports-builder"
#define KPH_USAGE "install [--build-deps=prebuilt|source] <port>"
#else
#define KPH_NAME "kdos-ports-helper"
#define KPH_USAGE "install <port>"
#endif

int main(int argc, char **argv)
{
	KbArgv install = {0}, cat = {0};
	const char *port;

#ifdef KPH_BUILD
	const char *deps = NULL;
	if (argc == 4 && (!strcmp(argv[2], "--build-deps=prebuilt") ||
			  !strcmp(argv[2], "--build-deps=source"))) {
		deps = argv[2];
		argv[2] = argv[3];
		argc = 3;
	}
#endif
	if (argc != 3 || strcmp(argv[1], "install")) {
		fprintf(stderr, "usage: " KPH_NAME " " KPH_USAGE "\n");
		return 2;
	}
	port = argv[2];
	if (!kph_name_ok(port)) {
		fprintf(stderr, KPH_NAME ": '%s' is not a port name\n", port);
		return 2;
	}
	if (geteuid() != 0) {
		fprintf(stderr, KPH_NAME ": run through pkexec\n");
		return 2;
	}

	clearenv();
	setenv("PATH", SAFE_PATH, 1);
	setenv("LC_ALL", "C", 1);

	kb_argv_add(&install, "kdos");
	kb_argv_add(&install, "update");
	kb_argv_add(&install, "install");
#ifdef KPH_BUILD
	kb_argv_add(&install, "--source");
	if (deps)
		kb_argv_add(&install, deps);
#else
	kb_argv_add(&install, "--prebuilt");
#endif
	kb_argv_add(&install, port);
	kb_argv_end(&install);
	if (kb_run_tty(&install) != 0)
		return 1;

	kb_argv_add(&cat, "kdos-discover-catalog");
	kb_argv_end(&cat);
	if (kb_run_tty(&cat) != 0)
		fprintf(stderr, KPH_NAME ": %s is installed and the AppStream "
				"catalogue was not rewritten; Discover lists it "
				"after the next system update\n",
			port);
	printf("==> %s installed\n", port);
	return 0;
}
