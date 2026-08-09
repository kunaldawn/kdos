/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgbuild — turn a port into a package
 *
 * Invoked with NO arguments and cwd == the port directory. That is the whole
 * interface, and `kpkg` is its only caller.
 *
 * A recipe is `kpkgbuild` (declarative metadata, parsed — no shell involved)
 * plus `build.sh` beside it, which IS bash. The sources are extracted and the
 * package rolled by this program; bash is asked to do exactly one thing, which
 * is run the build with the five variables a recipe expects.
 *
 * That contract is reproduced exactly, because every recipe in the tree was
 * written against it:
 *
 *   PKGNAME   <name>-<version>-<release>.tar.xz
 *   PORT_SRC  the port directory
 *   SRC_ROOT  $WORK_DIR/<name>
 *   SRC       $WORK_DIR/<name>/<name>-<version>   (cwd when build() runs)
 *   PKG       $WORK_DIR/<name>/pkg
 *
 * They are shell VARIABLES, not exports — a recipe reaches them by
 * interpolating them, and nothing in a child process's environment should
 * suddenly start carrying them. `build.sh` is sourced inside `( set -e )`,
 * with no pipefail and no `set -u`, and its output goes straight to this
 * program's stdout and stderr: that interleaved stream IS the per-port build
 * log.
 *
 * `name`, `version`, `release` and every recipe helper are INJECTED, because
 * the recipe can no longer source itself. They come from the parser, and each
 * is single-quoted on the way in.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

typedef struct {
	char name[128];
	char version[128];
	char release[64];
	char source[2048];
} Recipe;

/* ──────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────── */

static int ends_with(const char *s, const char *suf)
{
	size_t a = strlen(s), b = strlen(suf);
	return a >= b && !strcmp(s + a - b, suf);
}

static int is_tarball(const char *s)
{
	return ends_with(s, ".tar.gz") || ends_with(s, ".tgz") ||
	       ends_with(s, ".tar.bz2") || ends_with(s, ".tbz2") ||
	       ends_with(s, ".tar.xz") || ends_with(s, ".txz") ||
	       ends_with(s, ".tar");
}

/*
 * The filename a source entry is cached under. `file::url` names it outright;
 * a bare URL is its basename, EXCEPT for the first source, which `ports/fetch`
 * saves under the standardised `<name>-<version>.<ext>`.
 */
static void source_file(const Recipe *r, const char *src, int first, char *out,
			size_t cap)
{
	const char *sep = strstr(src, "::");
	if (sep) {
		size_t n = (size_t)(sep - src);
		if (n >= cap)
			n = cap - 1;
		memcpy(out, src, n);
		out[n] = 0;
		return;
	}

	int url = !strncmp(src, "http://", 7) || !strncmp(src, "https://", 8) ||
		  !strncmp(src, "ftp://", 6);
	if (!url) {
		kb_strlcpy(out, src, cap);
		return;
	}

	kb_strlcpy(out, kb_basename(src), cap);
	if (!first)
		return;

	static const struct {
		const char *suffix;
		const char *ext;
	} EXT[] = {
		{ ".tar.gz", "tar.gz" },   { ".tgz", "tar.gz" },
		{ ".tar.bz2", "tar.bz2" }, { ".tbz2", "tar.bz2" },
		{ ".tar.xz", "tar.xz" },   { ".txz", "tar.xz" },
		{ ".tar.zst", "tar.zst" }, { ".zip", "zip" },
		{ NULL, NULL }
	};
	for (int i = 0; EXT[i].suffix; i++)
		if (ends_with(src, EXT[i].suffix)) {
			snprintf(out, cap, "%s-%s.%s", r->name, r->version,
				 EXT[i].ext);
			return;
		}
}

static int extract_sources(const KpConf *c, const Recipe *r, const char *portdir,
			   const char *src_dir, const char *src_root)
{
	char list[2048];
	kb_strlcpy(list, r->source, sizeof(list));

	int idx = 0;
	for (char *t = strtok(list, " \t\n"); t; t = strtok(NULL, " \t\n"), idx++) {
		char file[512];
		source_file(r, t, idx == 0, file, sizeof(file));

		char *path = kb_path_join(portdir, file);
		if (!kb_path_exists(path)) {
			free(path);
			path = kb_path_join(c->source_dir, file);
			if (!kb_path_exists(path)) {
				kp_err("Source not found: %s (checked %s and %s)",
				       file, portdir, c->source_dir);
				free(path);
				return -1;
			}
		}

		KbArgv a = {0};
		if (is_tarball(path)) {
			/* Only the FIRST source is stripped into $SRC; every
			 * later one lands unstripped beside it in $SRC_ROOT.
			 * That is how a port carries a second tarball. */
			kp_msg(idx == 0 ? "Extracting %s with stripping..."
					: "Extracting %s without stripping...",
			       file);
			kb_argv_add(&a, "tar");
			kb_argv_add(&a, "-xf");
			kb_argv_add(&a, path);
			kb_argv_add(&a, "-C");
			kb_argv_add(&a, idx == 0 ? src_dir : src_root);
			if (idx == 0)
				kb_argv_add(&a, "--strip-components=1");
		} else {
			/* .zip and .tar.zst are recognised for naming but not
			 * unpacked here — same as before; a recipe that wants
			 * one unpacks it itself. */
			kb_argv_add(&a, "cp");
			kb_argv_add(&a, path);
			kb_argv_add(&a, src_dir);
		}
		kb_argv_end(&a);
		kb_proc_verbose = 1;
		if (kb_run_tty(&a) != 0) {
			kp_err("Failed to unpack %s", file);
			free(path);
			return -1;
		}
		free(path);
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The prelude `build.sh` is sourced under. Everything before the last line is
 * the environment the recipe was written against. */
static int run_build(const KpConf *c, const KpDecl *d, const Recipe *r,
		     const char *portdir, const char *src_root,
		     const char *src_dir, const char *pkg)
{
	KbBuf s = {0};
	kb_buf_str(&s, "[ -f \"$KPKG_CONF\" ] && . \"$KPKG_CONF\" || true\n");
	kp_decl_prelude(d, &s);
	kb_buf_printf(&s,
		"PKGNAME=%s-%s-%s.tar.xz\n"
		"PORT_SRC=%s\n"
		"SRC_ROOT=%s\n"
		"SRC=%s\n"
		"PKG=%s\n"
		"cd \"$SRC\"\n"
		"( set -e; . \"$PORT_SRC/build.sh\" )\n",
		r->name, r->version, r->release, portdir, src_root, src_dir, pkg);

	KbArgv a = {0};
	kb_argv_add(&a, "bash");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, s.p);
	kb_argv_end(&a);
	setenv("KPKG_CONF", c->conf, 1);
	int rc = kb_run_tty(&a);
	kb_buf_free(&s);
	return rc;
}

/*
 * `.POSTINSTALL` is a standalone bash script: a shebang, the metadata the hook
 * may read, then `postinstall.sh` verbatim. It is packaged at the root of the
 * tarball and hoisted out again by kpkgadd, so it is never installed.
 *
 * It used to be bash's own `declare -f postinstall` dump plus a call. The hook
 * is a file now, so there is nothing to serialise.
 */
static void write_postinstall(const KpDecl *d, const char *portdir,
			      const char *pkg)
{
	char *hook = kb_path_join(portdir, "postinstall.sh");
	size_t len = 0;
	char *body = kb_read_all(hook, &len);
	free(hook);
	if (!body)
		return;

	KbBuf b = {0};
	kb_buf_str(&b, "#!/bin/bash\n");
	kp_decl_prelude(d, &b);
	kb_buf_add(&b, body, len);
	free(body);

	char *p = kb_path_join(pkg, ".POSTINSTALL");
	kb_write_all(p, b.p, b.n);
	chmod(p, 0755);
	free(p);
	kb_buf_free(&b);
	kp_msg("Added postinstall hook");
}

/* libtool archives name build-time paths that do not exist on the target and
 * make every consumer's link line wrong. */
static void strip_la(const char *dir)
{
	KbArgv a = {0};
	kb_argv_add(&a, "find");
	kb_argv_add(&a, dir);
	kb_argv_add(&a, "-name");
	kb_argv_add(&a, "*.la");
	kb_argv_add(&a, "-delete");
	kb_argv_end(&a);
	kb_run(&a);
}

/* ──────────────────────────────────────────────────────────────────────── */

int build_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	KpConf c;
	kp_conf_load(&c);

	char portdir[1024];
	if (!getcwd(portdir, sizeof(portdir)))
		kb_die("cannot read the current directory");

	if (!kb_path_exists("./kpkgbuild")) {
		kp_err("kpkgbuild not found in current directory");
		return 1;
	}

	KpDecl *decl = kp_decl_parse("./kpkgbuild");
	if (!decl) {
		kp_err("not a recipe: name, version or release is missing");
		return 1;
	}

	Recipe r;
	memset(&r, 0, sizeof(r));
	kb_strlcpy(r.name, kp_decl_name(decl), sizeof(r.name));
	kb_strlcpy(r.version, kp_decl_version(decl), sizeof(r.version));
	kb_strlcpy(r.release, kp_decl_release(decl), sizeof(r.release));
	kb_strlcpy(r.source, kp_decl_source(decl), sizeof(r.source));

	if (!kb_path_exists("./build.sh")) {
		kp_err("build.sh not found beside ./kpkgbuild");
		return 1;
	}
	if (!r.name[0]) {
		kp_err("'name' is not set");
		return 1;
	}
	if (!r.version[0]) {
		kp_err("'version' is not set");
		return 1;
	}
	if (!r.release[0]) {
		kp_err("'release' is not set");
		return 1;
	}
	/* WORK_DIR is validated before anything under it is removed. The shell
	 * version ran `rm -rf "$WORK_DIR/$name"` first and asked questions
	 * afterwards, which with an empty WORK_DIR is `rm -rf /<name>`. */
	if (!c.work_dir[0] || c.work_dir[0] != '/' || !strcmp(c.work_dir, "/")) {
		kp_err("WORK_DIR is not an absolute path: '%s'", c.work_dir);
		return 1;
	}

	char *src_root = kb_path_join(c.work_dir, r.name);
	char verdir[512];
	snprintf(verdir, sizeof(verdir), "%s-%s", r.name, r.version);
	char *src_dir = kb_path_join(src_root, verdir);
	char *pkg = kb_path_join(src_root, "pkg");

	kp_msg("Preparing...");
	kb_rmtree(src_root);
	/* $SRC is created even for a source-less port, which is what lets a
	 * `source=""` recipe cd into it and build out of $PORT_SRC. */
	kb_mkdir_p(src_dir);
	kb_mkdir_p(pkg);

	if (r.source[0] &&
	    extract_sources(&c, &r, portdir, src_dir, src_root) != 0)
		return 1;

	kp_msg("Building %s-%s-%s...", r.name, r.version, r.release);
	KpPaths paths = { portdir, src_root, src_dir, pkg };
	(void)paths;
	int brc = run_build(&c, decl, &r, portdir, src_root, src_dir, pkg);
	if (brc != 0) {
		kp_err("Build failed");
		return 1;
	}

	if (!kb_is_dir(pkg)) {
		kp_err("the build left no $PKG directory");
		return 1;
	}

	kp_msg("Creating package...");
	write_postinstall(decl, portdir, pkg);
	strip_la(pkg);

	kb_mkdir_p(c.package_dir);
	char pkgname[512];
	snprintf(pkgname, sizeof(pkgname), "%s-%s-%s.tar.xz", r.name, r.version,
		 r.release);
	char *out = kb_path_join(c.package_dir, pkgname);

	KbArgv t = {0};
	kb_argv_add(&t, "tar");
	kb_argv_add(&t, "-cJf");
	kb_argv_add(&t, out);
	kb_argv_add(&t, "-C");
	kb_argv_add(&t, pkg);
	kb_argv_add(&t, ".");
	kb_argv_end(&t);
	if (kb_run_tty(&t) != 0) {
		kp_err("Failed to create %s", out);
		return 1;
	}

	kp_msg("Cleaning up...");
	kb_rmtree(src_root);
	kp_msg("Package created: %s", out);

	free(src_root);
	free(src_dir);
	free(pkg);
	free(out);
	return 0;
}
