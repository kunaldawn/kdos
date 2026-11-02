/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-ports-helper — one host port from the binhost into this slot
 *
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <port>
 *
 * Discover's KDOS backend runs this, through pkexec and the polkit action
 * io.kdos.ports.install, when somebody installs host software the running
 * system lacks. It is the whole of the root side of that button, so it takes
 * one verb and one port name, and the name is checked before it means
 * anything: pkexec's grant names a program, never its arguments, and
 * /usr/bin/kdos under the same grant would reach every subcommand.
 *
 * THE INSTALL IS `kdos update install`'S, never a second copy of it. That
 * command owns everything the binhost path decides: the signed index
 * `kdos update fetch` keeps in /var/lib/kdos/binhost, downloading each
 * missing package and checking it against that index, the build environment
 * of /etc/kdos/chroot.env that kpkg matches packages under, refusing a
 * package the binhost does not hold instead of building it, and adding the
 * port to /var/lib/kdos/world so the next update keeps it. A copy here would
 * be a second answer to each of those, and the first to drift.
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

int main(int argc, char **argv)
{
	KbArgv install = {0}, cat = {0};

	if (argc != 3 || strcmp(argv[1], "install")) {
		fprintf(stderr, "usage: kdos-ports-helper install <port>\n");
		return 2;
	}
	if (!kph_name_ok(argv[2])) {
		fprintf(stderr, "kdos-ports-helper: '%s' is not a port name\n",
			argv[2]);
		return 2;
	}
	if (geteuid() != 0) {
		fprintf(stderr, "kdos-ports-helper: run through pkexec\n");
		return 2;
	}

	clearenv();
	setenv("PATH", SAFE_PATH, 1);
	setenv("LC_ALL", "C", 1);

	kb_argv_add(&install, "kdos");
	kb_argv_add(&install, "update");
	kb_argv_add(&install, "install");
	kb_argv_add(&install, argv[2]);
	kb_argv_end(&install);
	if (kb_run_tty(&install) != 0)
		return 1;

	kb_argv_add(&cat, "kdos-discover-catalog");
	kb_argv_end(&cat);
	if (kb_run_tty(&cat) != 0)
		fprintf(stderr, "kdos-ports-helper: %s is installed and the "
				"AppStream catalogue was not rewritten; Discover "
				"lists it after the next system update\n",
			argv[2]);
	printf("==> %s installed\n", argv[2]);
	return 0;
}
