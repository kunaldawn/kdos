/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgdepends — the install order, and nothing else
 *
 * ONE line on stdout, space-separated, newline-terminated. Nothing else, ever.
 * `script/buildlib/phases.py` splits this and validates every token against
 * ^[A-Za-z0-9][A-Za-z0-9._+-]*$, and treats empty output as a hard error;
 * `script/chroot_exec.sh` writes its own diagnostics to a file for exactly
 * this reason. A banner, a version line or a progress bar here becomes a
 * bogus package name in a build.
 *
 * Errors go to stderr. There is no --help and no --version, because the shell
 * version had none and something may be parsing the absence.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kdos-kpkg.h"

int depends_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	char *names[KP_MAX_ORDER];
	int n = 0;

	for (int i = 1; i < argc && n < KP_MAX_ORDER; i++) {
		if (!strcmp(argv[i], "--root") && i + 1 < argc) {
			kb_strlcpy(c.root, argv[++i], sizeof(c.root));
			continue;
		}
		names[n++] = argv[i];
	}

	KpOrder order;
	kp_resolve(&c, names, n, &order);

	for (int i = 0; i < order.n; i++)
		printf("%s%s", i ? " " : "", order.order[i]);
	putchar('\n');
	return 0;
}
