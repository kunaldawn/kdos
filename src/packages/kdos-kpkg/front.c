/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkg — build and install from ports
 *
 * `kpkg install -f` FORCES ONLY WHAT WAS NAMED. Dependencies pulled in behind
 * a forced package keep the ordinary skip-if-installed behaviour. That is not
 * a nicety: `script/buildlib/phases.py` passes `-f` for exactly the ports a
 * build plan selected, and a blanket force would rebuild all ~350 packages on
 * every run.
 *
 * The mechanism is the one the shell version arrived at. kpkgdepends drops
 * anything already installed, which would make `-f` resolve to nothing at all,
 * so under `-f` resolution runs against an EMPTY database and the decision
 * about what to actually rebuild is taken here, per package.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

#define VERSION "1.0-kdos"

static void usage(void)
{
	printf("kpkg version %s - Minimal package manager for KDOS\n"
	       "\n"
	       "Usage: kpkg <command> [options]\n"
	       "\n"
	       "Options:\n"
	       "  --root <path>      Operate on a different root directory\n"
	       "  --keep-cache       Keep cached packages after installation\n"
	       "\n"
	       "Commands:\n"
	       "  install <pkg>...   Install package(s) with dependencies\n"
	       "  remove <pkg>...    Remove package(s)\n"
	       "  update             Update all installed packages\n"
	       "  list               List installed packages\n"
	       "  info <pkg>         Show package information\n"
	       "  meta <pkg>         Print the recipe's metadata as shell\n"
	       "                     assignments, for ports/fetch to eval\n"
	       "  verify <pkg>       Build with kpkgbuild and kpkgbuild.new,\n"
	       "                     then compare the two packages\n"
	       "  help               Show this help message\n"
	       "\n"
	       "Examples:\n"
	       "  kpkg install bash\n"
	       "  kpkg install --root /mnt/target bash\n"
	       "  kpkg remove bash\n"
	       "  kpkg list\n", VERSION);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* kpkgbuild is invoked with no arguments and cwd == the port directory. */
static int build_port(const char *portdir)
{
	char cwd[1024];
	if (!getcwd(cwd, sizeof(cwd)))
		return -1;
	if (chdir(portdir) != 0) {
		kp_err("Port not found: %s", portdir);
		return -1;
	}
	int rc = build_main(0, NULL);
	if (chdir(cwd) != 0)
		kb_die("cannot return to %s", cwd);
	return rc;
}

static int install_pkgfile(const KpConf *c, const char *file, int force)
{
	char *args[6];
	int n = 0;
	args[n++] = (char *)"kpkgadd";
	if (force)
		args[n++] = (char *)"--force";
	if (c->root[0]) {
		args[n++] = (char *)"--root";
		args[n++] = (char *)c->root;
	}
	args[n++] = (char *)file;
	args[n] = NULL;
	return add_main(n, args);
}

static int cmd_install(KpConf *c, int argc, char **argv)
{
	int force = 0, keep_cache = 0;
	char *want[KP_MAX_ORDER];
	int nwant = 0;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force"))
			force = 1;
		else if (!strcmp(argv[i], "--keep-cache"))
			keep_cache = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc)
			kb_strlcpy(c->root, argv[++i], sizeof(c->root));
		else if (nwant < KP_MAX_ORDER)
			want[nwant++] = argv[i];
	}

	if (!nwant) {
		printf("Usage: kpkg install <package>...\n");
		return 1;
	}

	kp_msg("Resolving dependencies...");

	KpConf resolve = *c;
	if (force)
		kb_strlcpy(resolve.pkgdb_dir, "/dev/null",
			   sizeof(resolve.pkgdb_dir));

	KpOrder order;
	kp_resolve(&resolve, want, nwant, &order);
	if (!order.n) {
		kp_msg("Nothing to do");
		return 0;
	}

	KbBuf list = {0};
	for (int i = 0; i < order.n; i++)
		kb_buf_printf(&list, "%s%s", i ? " " : "", order.order[i]);
	kp_msg("Packages to install: %s", list.p);
	kb_buf_free(&list);

	for (int i = 0; i < order.n; i++) {
		const char *pkg = order.order[i];

		/* Forced only if it was NAMED. A dependency dragged in behind
		 * one keeps the ordinary behaviour. */
		int forced = 0;
		if (force)
			for (int j = 0; j < nwant; j++)
				if (!strcmp(want[j], pkg))
					forced = 1;

		if (!forced && kp_installed(c, pkg)) {
			kp_msg("Skipping %s (already installed)", pkg);
			continue;
		}

		char *dir = kp_port_dir(c, pkg);
		if (!dir) {
			kp_err("Port not found: %s", pkg);
			return 1;
		}
		kp_msg(forced ? "Rebuilding %s (forced)..." : "Building %s...",
		       pkg);
		int rc = build_port(dir);
		free(dir);
		if (rc != 0) {
			kp_err("Failed to build %s", pkg);
			return 1;
		}

		/* kpkgbuild names the package after the recipe, so the file is
		 * whatever landed in PACKAGE_DIR under that name. */
		char **files = kb_listdir(c->package_dir, NULL);
		char *found = NULL;
		size_t plen = strlen(pkg);
		for (char **f = files; f && *f; f++) {
			if (strncmp(*f, pkg, plen) || (*f)[plen] != '-')
				continue;
			free(found);
			found = kb_path_join(c->package_dir, *f);
		}
		kb_strv_free(files);
		if (!found) {
			kp_err("no package produced for %s", pkg);
			return 1;
		}

		if (install_pkgfile(c, found, forced) != 0) {
			kp_err("Failed to install %s", pkg);
			free(found);
			return 1;
		}
		if (!keep_cache) {
			kp_msg("Removing cached package %s...", found);
			unlink(found);
		}
		free(found);
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * `kpkg verify <port>` — build the port with its current recipe AND with the
 * `kpkgbuild.new` / `build.sh.new` sitting beside it, then diff the two
 * packages. The claim being checked is "the new recipe produces the same
 * package", and nothing weaker is worth having when the recipe changes.
 *
 * Neither build happens in the port directory. A scratch directory is filled
 * with symlinks to everything the port carries — tarballs, patches, vendor
 * bundles — and only the files being TESTED are real. The ports tree is never
 * written to, so an interrupted verify leaves nothing behind.
 */
static int stage_recipe(const char *portdir, const char *recipe, char *out,
			size_t cap)
{
	char tmpl[512];
	snprintf(tmpl, sizeof(tmpl), "/tmp/kpkg-verify.XXXXXX");
	if (!mkdtemp(tmpl))
		return -1;
	kb_strlcpy(out, tmpl, cap);

	char **names = kb_listdir(portdir, NULL);
	for (char **n = names; n && *n; n++) {
		/* The recipe and its build script are copied, not linked, so
		 * the `.new` variants can stand in for them. */
		if (!strcmp(*n, "kpkgbuild") || !strcmp(*n, "kpkgbuild.new") ||
		    !strcmp(*n, "build.sh") || !strcmp(*n, "build.sh.new"))
			continue;
		char *src = kb_path_join(portdir, *n);
		char *dst = kb_path_join(tmpl, *n);
		if (symlink(src, dst) != 0)
			kb_warn("cannot link %s", *n);
		free(src);
		free(dst);
	}
	kb_strv_free(names);

	char *from = kb_path_join(portdir, recipe);
	char *to = kb_path_join(tmpl, "kpkgbuild");
	int rc = kb_copy_file(from, to);
	free(from);
	free(to);
	if (rc != 0)
		return rc;

	/* The build script travels with the recipe: `kpkgbuild.new` is tested
	 * against `build.sh.new` when there is one, and against the current
	 * build.sh when the change is metadata-only. */
	int is_new = strcmp(recipe, "kpkgbuild") != 0;
	char *bs = NULL;
	if (is_new) {
		bs = kb_path_join(portdir, "build.sh.new");
		if (!kb_path_exists(bs)) {
			free(bs);
			bs = NULL;
		}
	}
	if (!bs)
		bs = kb_path_join(portdir, "build.sh");
	to = kb_path_join(tmpl, "build.sh");
	rc = kb_path_exists(bs) ? kb_copy_file(bs, to) : 0;
	free(bs);
	free(to);
	return rc;
}

/* The built package for <name>, whatever version it came out as. */
static char *find_package(const KpConf *c, const char *name)
{
	char **files = kb_listdir(c->package_dir, NULL);
	char *found = NULL;
	size_t plen = strlen(name);
	for (char **f = files; f && *f; f++) {
		if (strncmp(*f, name, plen) || (*f)[plen] != '-')
			continue;
		free(found);
		found = kb_path_join(c->package_dir, *f);
	}
	kb_strv_free(files);
	return found;
}

static char *manifest_of(const char *pkgfile)
{
	char *buf = kb_calloc(1, 1 << 20);
	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "-tf");
	kb_argv_add(&a, pkgfile);
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 20) != 0) {
		free(buf);
		return NULL;
	}
	return buf;
}

/* `kpkg meta` — the recipe's fields as shell assignments, single-quoted.
 * ports/fetch used to `. ./kpkgbuild` for these; a recipe stopped being a
 * shell script, so it asks for them instead. Accepts a port NAME or a
 * directory, because fetch walks directories. */
static int cmd_meta(const KpConf *c, const char *who)
{
	char path[1040];
	char *dir = NULL;

	if (strchr(who, '/') || kb_is_dir(who))
		snprintf(path, sizeof(path), "%s/kpkgbuild", who);
	else {
		dir = kp_port_dir(c, who);
		if (!dir) {
			kp_err("no port named %s", who);
			return 1;
		}
		snprintf(path, sizeof(path), "%s/kpkgbuild", dir);
		free(dir);
	}

	KpDecl *d = kp_decl_parse(path);
	if (!d) {
		kp_err("%s: not a recipe", path);
		return 1;
	}
	kp_decl_meta(d);
	kp_decl_free(d);
	return 0;
}

static int cmd_verify(const KpConf *c, const char *name)
{
	char *portdir = kp_port_dir(c, name);
	if (!portdir) {
		kp_err("Port not found: %s", name);
		return 1;
	}

	char *newp = kb_path_join(portdir, "kpkgbuild.new");
	if (!kb_path_exists(newp)) {
		kp_err("%s has no kpkgbuild.new to verify against", name);
		free(newp);
		free(portdir);
		return 1;
	}
	free(newp);

	char *pkg[2] = { NULL, NULL };
	const char *which[2] = { "kpkgbuild", "kpkgbuild.new" };

	for (int i = 0; i < 2; i++) {
		char stage[512];
		if (stage_recipe(portdir, which[i], stage, sizeof(stage)) != 0) {
			kp_err("cannot stage %s", which[i]);
			return 1;
		}
		kp_msg("Building with %s...", which[i]);
		if (build_port(stage) != 0) {
			kp_err("build with %s failed", which[i]);
			kb_rmtree(stage);
			return 1;
		}
		kb_rmtree(stage);

		char *built = find_package(c, name);
		if (!built) {
			kp_err("no package produced by %s", which[i]);
			return 1;
		}
		char keep[600];
		snprintf(keep, sizeof(keep), "/tmp/kpkg-verify-%d.tar.xz", i);
		if (rename(built, keep) != 0 && kb_copy_file(built, keep) != 0) {
			kp_err("cannot set aside %s", built);
			free(built);
			return 1;
		}
		unlink(built);
		free(built);
		pkg[i] = kb_strdup(keep);
	}

	char *a = manifest_of(pkg[0]);
	char *b = manifest_of(pkg[1]);
	int same = a && b && !strcmp(a, b);

	printf("\n");
	if (same) {
		kp_msg("%s: the two recipes produce the same file list", name);
	} else {
		kp_err("%s: the file lists DIFFER", name);
		printf("  old: %s\n  new: %s\n", pkg[0], pkg[1]);
	}
	free(a);
	free(b);
	free(pkg[0]);
	free(pkg[1]);
	free(portdir);
	return same ? 0 : 1;
}

static int cmd_list(const KpConf *c)
{
	char *db = kp_db_dir(c);
	char **v = kb_listdir(db, NULL);
	int width = 0;
	for (char **p = v; p && *p; p++) {
		int n = (int)strlen(*p);
		if (n > width)
			width = n;
	}
	for (char **p = v; p && *p; p++) {
		char ver[128], rel[128];
		if (kp_installed_version(c, *p, ver, sizeof(ver), rel,
					 sizeof(rel)) == 0)
			printf("%-*s  %s-%s\n", width, *p, ver, rel);
	}
	kb_strv_free(v);
	free(db);
	return 0;
}

static int cmd_info(const KpConf *c, const char *name)
{
	char ver[128], rel[128];
	if (kp_installed_version(c, name, ver, sizeof(ver), rel, sizeof(rel)) == 0) {
		char *db = kp_db_dir(c);
		char *f = kb_path_join(db, name);
		size_t n = 0;
		char *data = kb_read_all(f, &n);
		int files = 0;
		for (char *p = data; p && *p; p++)
			if (*p == '\n')
				files++;
		free(data);
		free(f);
		free(db);
		printf("Package: %s\nVersion: %s-%s\nFiles: %d\n", name, ver, rel,
		       files > 0 ? files - 1 : 0);
		return 0;
	}

	char *dir = kp_port_dir(c, name);
	if (!dir) {
		kp_err("Package not found or not installed: %s", name);
		return 1;
	}

	/* The description is READ now. The shell version printed `$1` here,
	 * which inside its function was the package NAME — so every uninstalled
	 * package described itself as its own name, and `# description :` was
	 * parsed by nothing at all. */
	char desc[256];
	kp_description(dir, desc, sizeof(desc));

	char deps[KP_MAX_DEPS][128];
	int nd = kp_depends(dir, deps, KP_MAX_DEPS);

	printf("Package: %s\n", name);
	if (desc[0])
		printf("Description: %s\n", desc);
	if (nd) {
		printf("Depends:");
		for (int i = 0; i < nd; i++)
			printf(" %s", deps[i]);
		putchar('\n');
	}
	printf("Status: not installed\n");
	free(dir);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int front_main(int argc, char **argv)
{
	KpConf c;
	kp_conf_load(&c);

	if (argc < 2) {
		usage();
		return 1;
	}

	const char *cmd = argv[1];
	int rest = argc - 2;
	char **restv = argv + 2;

	if (!strcmp(cmd, "install") || !strcmp(cmd, "i"))
		return cmd_install(&c, rest, restv);

	if (!strcmp(cmd, "remove") || !strcmp(cmd, "r")) {
		char *args[KP_MAX_ORDER + 4];
		int n = 0;
		args[n++] = (char *)"kpkgdel";
		for (int i = 0; i < rest && n < KP_MAX_ORDER; i++)
			args[n++] = restv[i];
		args[n] = NULL;
		return del_main(n, args);
	}

	if (!strcmp(cmd, "list") || !strcmp(cmd, "l")) {
		for (int i = 0; i < rest; i++)
			if (!strcmp(restv[i], "--root") && i + 1 < rest)
				kb_strlcpy(c.root, restv[++i], sizeof(c.root));
		return cmd_list(&c);
	}

	if (!strcmp(cmd, "meta")) {
		if (!rest) {
			printf("Usage: kpkg meta <package|portdir>\n");
			return 1;
		}
		return cmd_meta(&c, restv[0]);
	}

	if (!strcmp(cmd, "verify")) {
		if (!rest) {
			printf("Usage: kpkg verify <package>\n");
			return 1;
		}
		return cmd_verify(&c, restv[0]);
	}

	if (!strcmp(cmd, "info")) {
		const char *who = NULL;
		for (int i = 0; i < rest; i++) {
			if (!strcmp(restv[i], "--root") && i + 1 < rest)
				kb_strlcpy(c.root, restv[++i], sizeof(c.root));
			else if (!who)
				who = restv[i];
		}
		if (!who) {
			printf("Usage: kpkg info <package>\n");
			return 1;
		}
		return cmd_info(&c, who);
	}

	if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") ||
	    !strcmp(cmd, "-h")) {
		usage();
		return 0;
	}

	usage();
	return 1;
}
