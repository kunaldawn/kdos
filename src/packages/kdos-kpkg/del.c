/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgdel — remove installed packages
 *
 *   kpkgdel [--root <path>] <package>...
 *
 * The manifest is walked in REVERSE, so a directory is only reached after
 * everything inside it: an entry ending in `/` is a directory and gets rmdir
 * (which fails harmlessly while it still holds files another package owns),
 * anything else gets unlink.
 *
 * There is deliberately no reverse-dependency check. `kpkgdel bash` will
 * remove bash. That is the distro this is.
 *
 * One fix: a name that is not installed no longer aborts the whole run. The
 * shell version had the check inside the loop with an `exit 1`, so
 * `kpkgdel a bogus c` removed `a`, then stopped and never touched `c` — while
 * reporting only that `bogus` was not installed.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

static int remove_one(const KpConf *c, const char *name)
{
	char *db = kp_db_dir(c);
	char *dbfile = kb_path_join(db, name);
	free(db);

	size_t len = 0;
	char *data = kb_read_all(dbfile, &len);
	if (!data) {
		kp_err("Package '%s' not installed", name);
		free(dbfile);
		return 1;
	}

	kp_msg("Removing %s", name);

	const char *root = c->root[0] ? c->root : "/";

	/* Skip line 1 — that is the version, not a path. */
	char *body = strchr(data, '\n');
	char *paths[16384];
	int n = 0;
	for (char *l = body ? body + 1 : data; l && *l && n < 16384;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (*l)
			paths[n++] = l;
		l = nl ? nl + 1 : NULL;
	}

	for (int i = n - 1; i >= 0; i--) {
		char *full = kb_path_join(root, paths[i]);
		size_t pl = strlen(paths[i]);
		if (pl && paths[i][pl - 1] == '/')
			rmdir(full);	/* only when it is already empty */
		else
			unlink(full);
		free(full);
	}

	free(data);
	unlink(dbfile);
	free(dbfile);
	kp_msg("Package '%s' removed", name);
	return 0;
}

int del_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	const char *names[256];
	int n = 0;

	for (int i = 1; i < argc && n < 256; i++) {
		if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c.root, argv[++i], sizeof(c.root));
		else if (argv[i][0] == '-' && argv[i][1]) {
			/* Unknown options used to be taken as package names, so
			 * a stray flag read as "Package 'x' not installed". */
			kp_err("unknown option: %s", argv[i]);
			return 1;
		} else
			names[n++] = argv[i];
	}

	if (!n) {
		printf("Usage: kpkgdel [--root <path>] <package>...\n");
		return 1;
	}

	const char *root = c.root[0] ? c.root : "/";
	if (access(root, W_OK) != 0) {
		kp_err("cannot write to %s — run as root", root);
		return 1;
	}

	/* Every named package is attempted; the worst status is the exit code. */
	int rc = 0;
	for (int i = 0; i < n; i++)
		if (remove_one(&c, names[i]))
			rc = 1;
	return rc;
}
