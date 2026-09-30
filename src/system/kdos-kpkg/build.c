/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kpkgbuild — turn a port into a package
 *
 * kp_build_port() is the whole interface: it builds the port in one directory
 * and reports the exact package file it wrote. `kpkg install` and `kpkg
 * verify` call it directly; the `kpkgbuild` command is the same call on the
 * current directory.
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
 * a recipe is parsed and never sourced and so cannot define them for itself.
 * They come from the parser, and each is single-quoted on the way in.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-kpkg.h"

typedef struct {
	char name[128];
	char version[128];
	char release[64];
	char source[4096];
	char sha256[4096];
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

/*
 * `sha256 = <64 hex>  <basename>` — find the hash for one file.
 *
 * The accumulated value is alternating <hex> <name> tokens (see decl.c), so
 * this walks them in pairs and matches on the NAME. Returns NULL when the
 * recipe names no hash for this file, which the caller must treat as a
 * refusal rather than a pass.
 */
static const char *hash_for(const Recipe *r, const char *file, char out[65])
{
	char list[4096];
	kb_strlcpy(list, r->sha256, sizeof(list));
	/* strtok_r: this walk runs inside extract_sources' own walk over the
	 * source list, and one shared scanner state cannot serve both. */
	char *hex = NULL, *save = NULL;
	int k = 0;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), k++) {
		if (k % 2 == 0) {
			hex = t;
		} else if (!strcmp(t, file) && hex) {
			kb_strlcpy(out, hex, 65);
			return out;
		}
	}
	return NULL;
}

/*
 * Every extracted source is DECLARED. Its bytes were already checked by
 * verify_declared(), which hashes every `sha256 =` entry found in the port
 * directory or $SOURCE_DIR — the same two places, in the same order, that
 * extract_sources() looks — before the work directory is touched; hashing the
 * file again here would read every source twice for no new answer.
 *
 * A source the recipe names with no hash is a HARD failure, not a warning: a
 * warning here is indistinguishable from a hash that passed, and an
 * unverified tarball must never reach a build.
 *
 * KDOS_ALLOW_UNVERIFIED=1 is the bring-up escape hatch, for adding a port
 * before its hash is known. testing/preflight.sh asserts that no recipe in
 * the tree needs it.
 */
static int verify_source(const Recipe *r, const char *file)
{
	char want[65];
	if (hash_for(r, file, want))
		return 0;
	if (getenv("KDOS_ALLOW_UNVERIFIED")) {
		kp_msg("UNVERIFIED: no sha256 for %s", file);
		return 0;
	}
	kp_err("No sha256 for %s in the recipe — refusing to extract "
	       "an unverified source (KDOS_ALLOW_UNVERIFIED=1 to override)",
	       file);
	return -1;
}

/*
 * EVERY `sha256 =` ENTRY IS CHECKED, not only the ones a `source =` names.
 * A vendor bundle — `<name>-vendor-<version>.tar.xz` beside the recipe, which
 * `build.sh` unpacks itself — is declared with a hash and named by no source
 * entry, so a walk over `source =` alone would build 114 ports out of bytes
 * nothing ever looked at.
 *
 * An entry whose file is in neither the port directory nor $SOURCE_DIR is
 * skipped: a source that must be there is caught by extract_sources(), and
 * refusing a port over a declared file its build never opens would fail builds
 * for a hash that cannot affect them.
 *
 * This is the only place a source's bytes are hashed, so it must look where
 * extract_sources() looks — the port directory first, then $SOURCE_DIR. A
 * lookup that differed would check one file and extract another.
 */
static int verify_declared(const KpConf *c, const Recipe *r, const char *portdir)
{
	char list[4096];
	kb_strlcpy(list, r->sha256, sizeof(list));

	/* Alternating <hex> <name> tokens, as hash_for() walks them. */
	char *hex = NULL, *save = NULL;
	int k = 0;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), k++) {
		if (k % 2 == 0) {
			hex = t;
			continue;
		}
		if (!hex)
			continue;

		char *path = kb_path_join(portdir, t);
		if (!kb_path_exists(path)) {
			free(path);
			path = kb_path_join(c->source_dir, t);
			if (!kb_path_exists(path)) {
				free(path);
				continue;
			}
		}

		char got[65];
		if (kb_sha256_file(path, got) != 0) {
			kp_err("Cannot read %s to verify it", path);
			free(path);
			return -1;
		}
		free(path);
		if (!kb_str_ieq(got, hex)) {
			kp_err("sha256 MISMATCH for %s\n  expected %s\n  got      %s",
			       t, hex, got);
			return -1;
		}
	}
	return 0;
}

static int extract_sources(const KpConf *c, const Recipe *r, const char *portdir,
			   const char *src_dir, const char *src_root)
{
	char list[4096];
	kb_strlcpy(list, r->source, sizeof(list));

	int idx = 0;
	/* strtok_r: verify_source() tokenises the hash list from inside this
	 * loop, so the two walks must not share a scanner state. */
	char *save = NULL;
	for (char *t = strtok_r(list, " \t\n", &save); t;
	     t = strtok_r(NULL, " \t\n", &save), idx++) {
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

		if (verify_source(r, file) != 0) {
			free(path);
			return -1;
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
			 * unpacked here; a recipe that wants one unpacks it
			 * itself. */
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

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * Rolling the package, reproducibly.
 *
 * A package built twice from the same tree must be BYTE-IDENTICAL, and that is
 * a property of this one function rather than of every recipe — which is the
 * whole reason kpkg rolls its own archive instead of letting each build.sh do
 * it. Everything below is a source of nondeterminism a plain `tar -cJf` would
 * carry:
 *
 *   --sort=name        readdir order is filesystem order, and it is not stable
 *                      across machines or even across a copy of the same tree
 *   --mtime            every file carries the second it was installed
 *   --owner/--group    the builder's uid, and its NAME as text in the header
 *   --numeric-owner    ... and the name lookup that would otherwise happen
 *   --format=gnu       pax headers carry atime and ctime, which are wall clock;
 *                      ustar cannot hold a path over 255 bytes and some ports
 *                      have them, so gnu is the only format that is both
 *                      deterministic and sufficient
 *   XZ_OPT             -e or --check here changes the bytes
 *   XZ_DEFAULTS        a memory limit here shrinks the dictionary, or fails
 *                      the run under --no-adjust; both variables are held out
 *                      of the environment while xz runs, and the compressor
 *                      is pinned rather than inherited
 *   the compressor     preset and block size decide the bytes; the thread
 *                      count does not (kp_xz_args below)
 *
 * SOURCE_DATE_EPOCH is honoured when set (the phase env files set it) and 0
 * otherwise — either way the answer does not depend on when the build ran.
 */
static long long source_date_epoch(void)
{
	const char *s = getenv("SOURCE_DATE_EPOCH");
	char *end = NULL;
	long long v;

	if (!s || !*s)
		return 0;
	v = strtoll(s, &end, 10);
	if (end == s || v < 0)
		return 0;
	return v;
}

/*
 * The two compressor settings (kdos-kpkg.h). kept is what every package that
 * outlives its install is made with — the cache, the binhost, a verify, a
 * delta's reconstruction — so changing it changes every package hash, and a
 * published binhost and its deltas are then regenerated together. transient
 * is -0: its package is read once by kpkgadd and deleted, so its size reaches
 * nothing that ships.
 */
static const char *const kp_xz_kept[] = {
	"xz", "-9", "-T0", "--block-size=32MiB", "--no-adjust", NULL
};
static const char *const kp_xz_transient[] = {
	"xz", "-0", "-T0", "--block-size=8MiB", "--no-adjust", NULL
};

int kp_pack_transient;

const char *const *kp_xz_args(int transient)
{
	return transient ? kp_xz_transient : kp_xz_kept;
}

const char *kp_xz_cmd(int transient)
{
	static char cmd[2][96];
	char *s = cmd[!!transient];
	size_t n = 0;
	if (!*s)
		for (const char *const *w = kp_xz_args(transient); *w; w++)
			n += snprintf(s + n, sizeof(cmd[0]) - n, "%s%s",
				      n ? " " : "", *w);
	return s;
}

/* XZ_OPT and XZ_DEFAULTS are taken out of the environment while xz runs and
 * put back afterwards: kpkg install builds every port of an order in one
 * process, and the next port's build.sh must see the environment it was
 * given. */
static const char *const kp_xz_env[] = { "XZ_OPT", "XZ_DEFAULTS" };

void kp_xz_env_hide(char *saved[2])
{
	for (int i = 0; i < 2; i++) {
		const char *v = getenv(kp_xz_env[i]);
		saved[i] = v ? kb_strdup(v) : NULL;
		unsetenv(kp_xz_env[i]);
	}
}

void kp_xz_env_restore(char *saved[2])
{
	for (int i = 0; i < 2; i++) {
		if (saved[i])
			setenv(kp_xz_env[i], saved[i], 1);
		free(saved[i]);
		saved[i] = NULL;
	}
}

/* Bytes of regular files under the staged tree, for the packaging report. */
static long long staged_bytes;

static int staged_add(const char *path, const struct stat *st, int flag,
		      struct FTW *ftw)
{
	(void)path;
	(void)ftw;
	if (flag == FTW_F && S_ISREG(st->st_mode))
		staged_bytes += st->st_size;
	return 0;
}

/*
 * The archive is written to `<out>.part` and renamed to `<out>` only when tar
 * succeeds, and the partial file is removed otherwise: a package file under
 * its final name is always a whole one, never the remains of a failed or
 * interrupted roll that kpkgadd would then read.
 *
 * A change here to what a package contains, or to how the same staged tree is
 * archived, bumps KP_STORE_FORMAT (kpkg.h): the package store keys on inputs,
 * not on this code, and would keep serving packages rolled the old way.
 */
static int roll_package(const char *pkg, const char *out)
{
	char part[1100];
	if ((size_t)snprintf(part, sizeof(part), "%s.part", out) >= sizeof(part))
		return -1;

	char mtime[64];
	snprintf(mtime, sizeof(mtime), "--mtime=@%lld", source_date_epoch());

	/* tar splits the compressor on spaces itself; there is no shell
	 * involved. */
	KbArgv t = {0};
	kb_argv_add(&t, "tar");
	kb_argv_add(&t, "--sort=name");
	kb_argv_add(&t, "--format=gnu");
	kb_argv_add(&t, "--numeric-owner");
	kb_argv_add(&t, "--owner=0");
	kb_argv_add(&t, "--group=0");
	kb_argv_add(&t, mtime);
	char prog[128];
	snprintf(prog, sizeof(prog), "--use-compress-program=%s",
		 kp_xz_cmd(kp_pack_transient));
	kb_argv_add(&t, prog);
	kb_argv_add(&t, "-cf");
	kb_argv_add(&t, part);
	kb_argv_add(&t, "-C");
	kb_argv_add(&t, pkg);
	kb_argv_add(&t, ".");
	kb_argv_end(&t);
	char *saved[2];
	kp_xz_env_hide(saved);
	int rc = kb_run_tty(&t);
	kp_xz_env_restore(saved);
	if (rc == 0 && rename(part, out) != 0) {
		kp_err("cannot rename %s: %s", part, strerror(errno));
		rc = -1;
	}
	if (rc != 0)
		unlink(part);
	return rc;
}

/*
 * `.POSTINSTALL` is a standalone bash script: a shebang, the metadata the hook
 * may read, then `postinstall.sh` verbatim. It is packaged at the root of the
 * tarball and hoisted out again by kpkgadd, so it is never installed.
 *
 * The hook is a FILE, copied through byte for byte: nothing here serialises a
 * shell function, so a hook that runs under `bash postinstall.sh` in the port
 * directory runs identically out of the package.
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

/* The info directory file is an index over every package's pages. Staged, it
 * is a path each package with info pages claims — the second such install is
 * a file conflict — and it lists only that one package. kpkgadd regenerates it
 * from what is installed (triggers.c). */
static void strip_info_dir(const char *pkg)
{
	char *dir = kb_path_join(pkg, "usr/share/info/dir");
	unlink(dir);
	free(dir);
}

/* mimeinfo.cache is an index over every package's desktop entries; kpkgadd
 * writes it from what is installed (triggers.c). Staged, it lists one package,
 * and removing that package deletes the system's copy. */
static void strip_desktop_cache(const char *pkg)
{
	char *f = kb_path_join(pkg, "usr/share/applications/mimeinfo.cache");
	unlink(f);
	free(f);
}

/* The same for each font directory's fonts.dir: X.Org's font packages write
 * one at install time listing only their own faces, and two of them share
 * `misc/`. kpkgadd writes it from the directory (triggers.c). */
static void strip_font_dirs(const char *pkg)
{
	char *top = kb_path_join(pkg, "usr/share/fonts");
	char **dirs = kb_listdir(top, NULL);
	for (char **d = dirs; d && *d; d++) {
		char *sub = kb_path_join(top, *d);
		char *idx = kb_path_join(sub, "fonts.dir");
		unlink(idx);
		free(idx);
		free(sub);
	}
	kb_strv_free(dirs);
	free(top);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The build, in the current directory, which is the port directory. */
static int build_here(char *pkgout, size_t cap)
{
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
	kb_strlcpy(r.sha256, kp_decl_sha256(decl), sizeof(r.sha256));

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

	/*
	 * The umask is part of the package. A file `make install` creates
	 * without an explicit mode takes it, so a builder running with 002 or
	 * 077 produces a package whose modes differ from everyone else's — the
	 * one source of nondeterminism that is not in the tar invocation.
	 */
	umask(022);

	/* Before the work directory is touched: a bad hash must not cost the
	 * tree that is already unpacked under it. */
	if (verify_declared(&c, &r, portdir) != 0)
		return 1;

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
	strip_info_dir(pkg);
	strip_desktop_cache(pkg);
	strip_font_dirs(pkg);

	kb_mkdir_p(c.package_dir);
	char pkgname[512];
	snprintf(pkgname, sizeof(pkgname), "%s-%s-%s.tar.xz", r.name, r.version,
		 r.release);
	char *out = kb_path_join(c.package_dir, pkgname);

	struct timespec t0, t1;
	staged_bytes = 0;
	nftw(pkg, staged_add, 32, FTW_PHYS);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	if (roll_package(pkg, out) != 0) {
		kp_err("Failed to create %s", out);
		return 1;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	kp_msg("Packaged %s: %lld MB in %.1f s", r.name,
	       staged_bytes / (1024 * 1024),
	       (double)(t1.tv_sec - t0.tv_sec) +
		       (double)(t1.tv_nsec - t0.tv_nsec) / 1e9);

	kp_msg("Cleaning up...");
	kb_rmtree(src_root);
	kp_msg("Package created: %s", out);
	if (pkgout)
		kb_strlcpy(pkgout, out, cap);

	free(src_root);
	free(src_dir);
	free(pkg);
	free(out);
	return 0;
}

int kp_build_port(const char *portdir, char *pkgout, size_t cap)
{
	char cwd[1024];
	if (pkgout && cap)
		pkgout[0] = '\0';
	if (!getcwd(cwd, sizeof(cwd)))
		return -1;
	if (chdir(portdir) != 0) {
		kp_err("Port not found: %s", portdir);
		return -1;
	}
	int rc = build_here(pkgout, cap);
	if (chdir(cwd) != 0)
		kb_die("cannot return to %s", cwd);
	return rc;
}

int build_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return kp_build_port(".", NULL, 0) != 0;
}
