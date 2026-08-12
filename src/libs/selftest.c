/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   src/libs/selftest.c — the regression net for libk*
 *
 * Host-only; nothing links this into the ISO. Run it with
 * `testing/selftest.sh`, which compiles it against every library and executes
 * it. It exists because five programs now share these libraries, and a change
 * to one of them can quietly break a distro artefact that is only visible
 * after a full build.
 *
 * The assertions here are the INVARIANTS that were established by diffing
 * against the implementations these libraries replaced — python's colorsys,
 * the shell kpkg, the shell kdos. Each one is a claim that was true when it
 * was measured; this file is what notices when it stops being true.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kcolor.h"
#include "kbuild.h"
#include "kpkg.h"
#include "ktui.h"
#include "portup.h"

static int failures;
static int checks;

static void ok(int cond, const char *what)
{
	checks++;
	if (cond)
		return;
	failures++;
	printf("  FAIL  %s\n", what);
}

static void eq_str(const char *got, const char *want, const char *what)
{
	checks++;
	if (got && want && !strcmp(got, want))
		return;
	failures++;
	printf("  FAIL  %s\n        got  '%s'\n        want '%s'\n", what,
	       got ? got : "(null)", want ? want : "(null)");
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_colour(void)
{
	printf("libkcolor\n");

	uint32_t v;
	ok(kcol_parse("#39ff14", &v) == 0 && v == 0x39ff14, "parse #rrggbb");
	ok(kcol_parse("39ff14", &v) == 0 && v == 0x39ff14, "parse rrggbb");
	/* CSS shorthand doubles each digit — #39f is #3399ff, not #0039f0. */
	ok(kcol_parse("#39f", &v) == 0 && v == 0x3399ff, "parse #rgb doubles");
	ok(kcol_parse("#zz", &v) < 0, "reject non-hex");

	char hex[8];
	kcol_format(0x0a0b0c, hex);
	eq_str(hex, "0a0b0c", "format keeps leading zeroes");

	/* Round-tripping every scheme colour through HLS must be exact — the
	 * generators depend on it. */
	int rt = 1;
	for (int i = 0; i < kcol_nscheme; i++) {
		const uint32_t f[] = { kcol_schemes[i].primary,
				       kcol_schemes[i].secondary,
				       kcol_schemes[i].urgent,
				       kcol_schemes[i].text };
		for (size_t k = 0; k < sizeof(f) / sizeof(f[0]); k++) {
			double h, l, s;
			kcol_to_hls(f[k], &h, &l, &s);
			if (kcol_from_hls(h, l, s) != f[k])
				rt = 0;
		}
	}
	ok(rt, "HLS round-trip is exact for every scheme colour");

	/* Measured against CPython over 8476 colours: black and white are
	 * STRUCTURE and must come back untouched, or a recoloured icon set
	 * turns to mush. */
	const KcolScheme *ph = kcol_find("phosphor");
	ok(ph != NULL, "kcol_find(phosphor)");
	ok(kcol_remap(ph, 0x000000) == 0x000000, "remap leaves pure black");
	ok(kcol_remap(ph, 0xffffff) == 0xffffff, "remap leaves pure white");
	ok(kcol_family(0xff0000) == KCOL_FAM_URGENT, "red is the urgent family");
	ok(kcol_family(0x808080) == KCOL_FAM_NEUTRAL, "grey is neutral");
	ok(kcol_family(0x2277dd) == KCOL_FAM_ACCENT, "blue maps to the accent");

	/* kcol_mix and kcol_mixf are NOT interchangeable, and this is the proof:
	 * halfway between black and white is 0x7f by the integer path (a
	 * truncating divide) and 0x80 by the float one (python's round(), which
	 * sends a half to EVEN). Each generated file was written against exactly
	 * one of them, so swapping them shifts every mixed colour by a unit. */
	ok(kcol_mix(0x000000, 0xffffff, 50) == 0x7f7f7f, "integer mix at 50%");
	ok(kcol_mixf(0x000000, 0xffffff, 0.5) == 0x808080, "float mix at 0.5");
	ok(kcol_mix(0x000000, 0xffffff, 50) != kcol_mixf(0x000000, 0xffffff, 0.5),
	   "the two mixes genuinely disagree");

	/* The hex-token scanner: python's #([0-9a-f]{6}|[0-9a-f]{3})\b, whose
	 * trailing word boundary is what keeps an 8-digit RGBA intact. */
	size_t n = 0;
	char *out = kcol_retint_text("x #39ff14ff y", 13, ph, &n);
	eq_str(out, "x #39ff14ff y", "8 hex digits are left alone");
	free(out);
	out = kcol_retint_text("#000000", 7, ph, &n);
	eq_str(out, "#000000", "declined colours keep their spelling");
	free(out);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_base(void)
{
	printf("libkbase\n");

	char buf[8];
	kb_strlcpy(buf, "abcdefghij", sizeof(buf));
	eq_str(buf, "abcdefg", "strlcpy truncates and terminates");
	kb_strlcpy(buf, "", sizeof(buf));
	eq_str(buf, "", "strlcpy handles empty");

	ok(kb_str_ieq("KDOS", "kdos"), "case-insensitive compare");
	ok(!kb_str_ieq("kdos", "kdosx"), "compare rejects a prefix");
	eq_str(kb_basename("/a/b/c"), "c", "basename");
	eq_str(kb_basename("bare"), "bare", "basename of a bare name");

	eq_str(kb_human_size(0), "0B", "human_size zero");
	eq_str(kb_human_size(1024), "1.0K", "human_size exact K");
	eq_str(kb_human_size(1536), "1.5K", "human_size fractional");

	/* kb_buf_printf must never truncate: a fixed stack buffer here once cut
	 * a generated btop theme in half, and the half-file looked plausible. */
	KbBuf b = {0};
	for (int i = 0; i < 200; i++)
		kb_buf_printf(&b, "%s", "0123456789");
	ok(b.n == 2000, "buf_printf grows past its scratch buffer");
	kb_buf_free(&b);

	KbBuf big = {0};
	char huge[4096];
	memset(huge, 'x', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	kb_buf_printf(&big, "%s", huge);
	ok(big.n == 4095, "buf_printf handles an over-long single write");
	kb_buf_free(&big);

	/* JSON escaping. Every --json output funnels through this, and the
	 * inputs are package descriptions, file paths and window titles — all
	 * of which can carry a quote or a control character. Getting this wrong
	 * emits something that looks like JSON and does not parse. */
	KbBuf j = {0};
	kb_json_str(&j, "plain");
	eq_str(j.p, "\"plain\"", "json plain");
	j.n = 0;
	kb_json_str(&j, "say \"hi\"");
	eq_str(j.p, "\"say \\\"hi\\\"\"", "json escapes quotes");
	j.n = 0;
	kb_json_str(&j, "back\\slash");
	eq_str(j.p, "\"back\\\\slash\"", "json escapes backslash");
	j.n = 0;
	kb_json_str(&j, "a\nb\tc");
	eq_str(j.p, "\"a\\nb\\tc\"", "json escapes newline and tab");
	j.n = 0;
	kb_json_str(&j, "bell\x07");
	eq_str(j.p, "\"bell\\u0007\"", "json escapes other controls as \\u");
	j.n = 0;
	/* UTF-8 passes through: JSON strings are Unicode and the input is
	 * already UTF-8, so escaping high bytes would double-encode them. */
	kb_json_str(&j, "caf\xc3\xa9");
	eq_str(j.p, "\"caf\xc3\xa9\"", "json passes UTF-8 through");
	j.n = 0;
	kb_json_str(&j, NULL);
	eq_str(j.p, "\"\"", "json NULL is an empty string");
	kb_buf_free(&j);

	/* ustar round-trip — the appbox image goes out and comes back through
	 * this, and a wrong header is a `podman load` that fails at 11 GB. */
	char dir[] = "/tmp/kdos-selftest.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	char *tarpath = kb_path_join(dir, "t.tar");
	int fd = open(tarpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ok(fd >= 0, "create a tar");
	const char *payload = "hello tar";
	kb_tar_put_header(fd, "blobs/sha256/deadbeef", (long long)strlen(payload));
	ok(write(fd, payload, strlen(payload)) == (ssize_t)strlen(payload),
	   "write a member");
	kb_tar_pad(fd, (long long)strlen(payload));
	kb_tar_finish(fd);
	close(fd);

	KbTarIn t;
	KbTarEntry e;
	ok(kb_tar_open(&t, tarpath) == 0, "reopen the tar");
	ok(kb_tar_next(&t, &e) == 1, "read a member");
	eq_str(e.name, "blobs/sha256/deadbeef", "member name survives");
	ok(e.size == (long long)strlen(payload), "member size survives");
	char back[32] = {0};
	ok(kb_tar_read(&t, back, sizeof(back) - 1) == (int)strlen(payload),
	   "member payload length");
	eq_str(back, payload, "member payload bytes");
	ok(kb_tar_next(&t, &e) == 0, "end of archive");
	kb_tar_close(&t);

	/* And a real tar has to accept what we wrote. */
	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "-tf");
	kb_argv_add(&a, tarpath);
	kb_argv_end(&a);
	char listing[256];
	ok(kb_run_capture(&a, listing, sizeof(listing)) == 0,
	   "GNU tar reads our archive");
	eq_str(listing, "blobs/sha256/deadbeef", "GNU tar agrees on the name");

	/* Output bigger than the buffer must TRUNCATE, not hang. It used to
	 * hang: the child inherited the pipe's read end, so closing ours left
	 * the pipe writable and the child blocked in write() while we blocked
	 * in waitpid(). `kpkg install zig` died there on a 1.1 MB listing. */
	KbArgv gen = {0};
	kb_argv_add(&gen, "seq");
	kb_argv_add(&gen, "100000");
	kb_argv_end(&gen);
	char small[64];
	kb_run_capture(&gen, small, sizeof(small));
	ok(strlen(small) >= sizeof(small) - 2, "capture truncates at the ceiling");

	KbBuf all = {0};
	ok(kb_run_capture_buf(&gen, &all) == 0, "unbounded capture succeeds");
	ok(all.n > (1 << 19), "unbounded capture keeps every byte");
	eq_str(all.p + all.n - 7, "100000\n", "and the last line is intact");
	kb_buf_free(&all);

	free(tarpath);
	kb_rmtree(dir);
	ok(!kb_path_exists(dir), "rmtree removes the tree");

	/* Landlock. A ruleset is only BUILT here, never enforced — enforcing is
	 * irreversible and would sandbox the rest of this process. */
	int abi = kb_landlock_abi();
	ok(abi > 0 || abi == -ENOSYS || abi == -EOPNOTSUPP,
	   "landlock abi probe answers a version or a known errno");

	if (abi > 0) {
		KbLandlock ll;
		ok(kb_landlock_new(&ll, 0) == 0, "ruleset builds for this ABI");

		/* The regression: a path_beneath rule on a NON-DIRECTORY is
		 * EINVAL if the mask names directory-only accesses. /dev/null
		 * has to take a write rule, or every sandboxed program dies on
		 * its first `>/dev/null` and looks broken rather than confined. */
		ok(kb_landlock_allow(&ll, "/dev/null", 1) == 0,
		   "a write rule on a non-directory is accepted");
		ok(kb_landlock_allow(&ll, "/usr", 0) == 0,
		   "a read rule on a directory is accepted");
		ok(kb_landlock_allow(&ll, "/nonexistent-kdos-selftest", 0) == -ENOENT,
		   "a missing path reports ENOENT rather than being ignored");
		kb_landlock_free(&ll);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_pkg(void)
{
	printf("libkpkg\n");

	char dir[] = "/tmp/kdos-selftest-pkg.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	char *repo = kb_path_join(dir, "repo");
	const char *ports[][2] = {
		{ "a", "depends = b c\n\nname = a\nversion = 1\nrelease = 1\n" },
		{ "b", "depends = c\n\nname = b\nversion = 1\nrelease = 1\n" },
		{ "c", "depends =\n\nname = c\nversion = 1\nrelease = 1\n" },
		/* A cycle. The shell solver terminated on these silently and so
		 * must this one — marking a name visited BEFORE walking it is
		 * the reason. */
		{ "x", "depends = y\n\nname = x\nversion = 1\nrelease = 1\n" },
		{ "y", "depends = x\n\nname = y\nversion = 1\nrelease = 1\n" },
		{ NULL, NULL }
	};
	for (int i = 0; ports[i][0]; i++) {
		char *pd = kb_path_join(repo, ports[i][0]);
		kb_mkdir_p(pd);
		char *rp = kb_path_join(pd, "kpkgbuild");
		kb_write_file(rp, ports[i][1]);
		free(rp);
		free(pd);
	}

	setenv("PORT_REPO", repo, 1);
	setenv("KPKG_CONF", "/nonexistent", 1);
	setenv("PKGDB_DIR", "/dev/null", 1);

	KpConf c;
	kp_conf_load(&c);
	ok(c.nrepos == 1, "PORT_REPO from the environment");

	/* The /dev/null idiom: three callers ask for an empty database this
	 * way, and it works only because /dev/null/<name> cannot be a file. */
	ok(!kp_installed(&c, "a"), "PKGDB_DIR=/dev/null reads as empty");

	char *names[] = { (char *)"a" };
	KpOrder o;
	kp_resolve(&c, names, 1, &o);
	ok(o.n == 3, "a pulls in three packages");
	eq_str(o.order[0], "c", "deepest dependency first");
	eq_str(o.order[o.n - 1], "a", "the named package last");

	char *cyc[] = { (char *)"x" };
	kp_resolve(&c, cyc, 1, &o);
	ok(o.n == 2, "a dependency cycle terminates");

	char *pd = kp_port_dir(&c, "a");
	ok(pd != NULL, "port lookup");
	char deps[KP_MAX_DEPS][128];
	int nd = kp_depends(pd, deps, KP_MAX_DEPS);
	ok(nd == 2, "two dependencies parsed");
	eq_str(deps[0], "b", "first dependency");
	free(pd);

	pd = kp_port_dir(&c, "nope");
	ok(pd == NULL, "a missing port is not found");

	/* Ownership, which is what a file conflict is actually about.
	 *
	 * Phases 0 and 1 install tar, musl, binutils and gcc by hand with
	 * `make DESTDIR=$SYSROOT install`, so those files exist and NO
	 * database entry owns them. Phase 2 — the self-hosting bootstrap —
	 * then rebuilds exactly those packages with kpkg. If an unowned file
	 * counts as a conflict the bootstrap cannot run at all, which is what
	 * `tar` failed on. A file another PACKAGE owns is still a conflict. */
	char *dbdir = kb_path_join(dir, "db");
	kb_mkdir_p(dbdir);
	char *dbfile = kb_path_join(dbdir, "owner");
	kb_write_file(dbfile, "1 1\n./usr/bin/owned\n./usr/share/\n");
	free(dbfile);
	/* The rest of this function relies on PKGDB_DIR=/dev/null meaning
	 * "empty database", so the real one is only pointed at here. */
	setenv("PKGDB_DIR", dbdir, 1);
	KpConf owned_conf;
	kp_conf_load(&owned_conf);
	free(dbdir);

	KpOwned *ow = kp_owned_load(&owned_conf);
	ok(kp_owned_has(ow, "usr/bin/owned"), "a file an installed package owns");
	ok(!kp_owned_has(ow, "usr/bin/stray"),
	   "a file left by a hand install is owned by nobody");
	ok(!kp_owned_has(ow, "usr/share"),
	   "directories are shared and never owned");
	/* An overwrite has to know WHO to take the path from. */
	eq_str(kp_owned_owner(ow, "usr/bin/owned"), "owner",
	       "the claim names its package");
	ok(kp_owned_owner(ow, "usr/bin/stray") == NULL,
	   "an unowned path has no owner to take it from");
	kp_owned_free(ow);

	/* `--overwrite` moves the path out of the old owner's manifest. Left
	 * behind, it is claimed twice and `kpkgdel <old>` deletes a file the
	 * new owner installed. */
	char *drop[] = { (char *)"usr/bin/owned" };
	ok(kp_db_drop_paths(&owned_conf, "owner", drop, 1) == 1,
	   "the path leaves the old owner");
	ok(kp_db_drop_paths(&owned_conf, "owner", drop, 1) == 0,
	   "and dropping it twice is a no-op");
	ow = kp_owned_load(&owned_conf);
	ok(!kp_owned_has(ow, "usr/bin/owned"), "nothing claims it now");
	ok(!kp_owned_has(ow, "usr/share"),
	   "the rest of the manifest survived the rewrite");
	kp_owned_free(ow);
	char *dbcheck = kb_path_join(dir, "db/owner");
	size_t dblen = 0;
	char *dbtext = kb_read_all(dbcheck, &dblen);
	eq_str(dbtext, "1 1\n./usr/share/\n", "version line and all");
	free(dbtext);
	free(dbcheck);
	setenv("PKGDB_DIR", "/dev/null", 1);

	kb_rmtree(dir);
	free(repo);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_build(void)
{
	printf("libkbuild\n");

	char v[128];

	kbuild_unquote("\"Cross Toolchain\"", v, sizeof(v));
	eq_str(v, "Cross Toolchain", "double-quoted literal");
	kbuild_unquote("'single'", v, sizeof(v));
	eq_str(v, "single", "single-quoted literal");
	kbuild_unquote("  bare   # trailing", v, sizeof(v));
	eq_str(v, "bare", "unquoted stops at whitespace and comment");
	/* An unterminated quote is not a literal, and metadata values are
	 * required to be literals — no expansion is ever performed. */
	kbuild_unquote("\"never closed", v, sizeof(v));
	eq_str(v, "", "unterminated quote reads as empty");
	kbuild_unquote("", v, sizeof(v));
	eq_str(v, "", "empty value");

	/* A snapshot path is deleted and re-extracted AS ROOT. Everything that
	 * could escape $BUILD_DIR is refused rather than sanitised. */
	ok(kbuild_safe_relpath("fs"), "a plain relative path is safe");
	ok(kbuild_safe_relpath("fs/usr/bin"), "a nested relative path is safe");
	ok(!kbuild_safe_relpath("/fs"), "absolute is refused");
	ok(!kbuild_safe_relpath("~/fs"), "a home path is refused");
	ok(!kbuild_safe_relpath(""), "empty is refused");
	ok(!kbuild_safe_relpath("."), "'.' is refused");
	ok(!kbuild_safe_relpath("/"), "'/' is refused");
	ok(!kbuild_safe_relpath("../etc"), "a leading .. is refused");
	ok(!kbuild_safe_relpath("fs/../.."), "an embedded .. is refused");
	/* ".." only counts as a whole component: "..foo" is a real name. */
	ok(kbuild_safe_relpath("fs/..foo"), "a name beginning .. is allowed");

	/* The reject path itself — no phase in the tree has a bad entry, so it
	 * is only reachable with a synthetic one. */
	char dir[] = "/tmp/kdos-selftest-bld.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");
	char *pd = kb_path_join(dir, "07_evil");
	kb_mkdir_p(pd);
	char *env = kb_path_join(dir, "evil.env.sh");
	kb_write_file(env,
		"export KDOS_PHASE_TITLE=\"Evil\"\n"
		"export CHROOT=1\n"
		"export KDOS_SNAPSHOT_PATHS=\"fs ../escape /abs\"\n"
		"export KDOS_SNAPSHOT_EXCLUDE=\"fs/tmp/*\"\n"
		"rm -rf /var/cache/kpkg/work\n");

	KbuildPhase ph[KBUILD_MAX_PHASES];
	int n = kbuild_discover(dir, ph, KBUILD_MAX_PHASES);
	ok(n == 1, "one phase discovered");
	if (n == 1) {
		eq_str(ph[0].name, "evil", "phase name drops the numeric prefix");
		eq_str(ph[0].title, "Evil", "declared title wins");
		ok(ph[0].chroot == 1, "CHROOT=1 is read");
		ok(ph[0].nsnap == 1, "only the safe path is kept");
		eq_str(ph[0].snap_path[0], "fs", "the safe path");
		ok(ph[0].nrejected == 2, "both unsafe paths are REPORTED");
		ok(kbuild_snapshottable(&ph[0]), "phase is snapshottable");
		char label[128];
		kbuild_label(&ph[0], label, sizeof(label));
		eq_str(label, "evil (Chroot)", "label marks a chroot phase");
	}

	/* A phase with no declared paths is never snapshotted. */
	char *pd2 = kb_path_join(dir, "08_bare");
	kb_mkdir_p(pd2);
	n = kbuild_discover(dir, ph, KBUILD_MAX_PHASES);
	ok(n == 2, "a phase without an env file still discovers");
	ok(!kbuild_snapshottable(&ph[1]), "no paths means never snapshotted");
	eq_str(ph[1].title, "bare", "title falls back to the tidied name");

	/* ----- the build plan ------------------------------------------- */

	KbuildPlan pl;
	char err[256];

	ok(kbuild_plan_from_cli(&pl, NULL, NULL, NULL, ph, n, err,
				sizeof(err)) == 0, "the empty plan parses");
	ok(!kbuild_plan_custom(&pl), "no arguments is not a custom plan");
	ok(kbuild_plan_phase_selected(&pl, "07_evil"), "everything is selected");
	ok(kbuild_plan_step_selected(&pl, "07_evil", "x.sh"), "every step runs");

	/* Rebuilding a port is custom but does NOT narrow execution — the
	 * distinction is what decides whether snapshots are suppressed. */
	ok(kbuild_plan_from_cli(&pl, NULL, NULL, "zlib, zlib musl", ph, n, err,
				sizeof(err)) == 0, "a rebuild-only plan parses");
	ok(kbuild_plan_custom(&pl), "rebuild makes a plan custom");
	ok(!kbuild_plan_narrows(&pl), "rebuild alone does not narrow execution");
	ok(pl.nrebuild == 2, "a repeated port is forced once");
	ok(kbuild_plan_forced(&pl, "musl") && !kbuild_plan_forced(&pl, "bash"),
	   "only the named ports are forced");

	ok(kbuild_plan_from_cli(&pl, "evil", NULL, NULL, ph, n, err,
				sizeof(err)) == 0, "a phase resolves by short name");
	eq_str(pl.phase[0], "07_evil", "and is stored by directory name");
	ok(kbuild_plan_narrows(&pl), "selecting phases narrows execution");
	ok(!kbuild_plan_phase_selected(&pl, "08_bare"), "the others are skipped");

	ok(kbuild_plan_from_cli(&pl, "nosuch", NULL, NULL, ph, n, err,
				sizeof(err)) < 0, "an unknown phase is refused");
	eq_str(err, "unknown phase: nosuch", "with the phase named");
	ok(kbuild_plan_from_cli(&pl, NULL, "evil", NULL, ph, n, err,
				sizeof(err)) < 0, "--steps without a colon is refused");

	/* A plan file that is missing or not an object reads as NO plan.
	 * Reading it as "run everything" is the dangerous failure. */
	ok(kbuild_plan_load(&pl, dir) < 0, "a missing plan file is not a plan");
	char *pf = kb_path_join(dir, KBUILD_PLAN_FILE);
	kb_write_file(pf, "not json at all\n");
	ok(kbuild_plan_load(&pl, dir) < 0, "a malformed plan file is not a plan");

	KbuildPlan saved;
	kbuild_plan_from_cli(&saved, "evil", NULL, "zlib", ph, n, err, sizeof(err));
	ok(kbuild_plan_save(&saved, dir) == 0, "a plan writes");
	ok(kbuild_plan_load(&pl, dir) == 0, "and reads back");
	ok(pl.nphase == 1 && !strcmp(pl.phase[0], "07_evil"), "phases round-trip");
	ok(pl.nrebuild == 1 && kbuild_plan_forced(&pl, "zlib"),
	   "rebuilds round-trip");
	free(pf);

	/* ----- the JSON manifests carry -------------------------------- */

	KjNode *j = kj_parse("{\"a\": \"x\\u2014y\", \"n\": -1.5e2, "
			     "\"t\": true, \"f\": false, \"z\": null, "
			     "\"arr\": [1, {\"k\": \"v\"}]}");
	ok(j != NULL, "a manifest-shaped document parses");
	eq_str(kj_str(j, "a", ""), "x\xe2\x80\x94y", "\\uXXXX becomes UTF-8");
	ok(kj_num(j, "n", 0) == -150.0, "exponents and signs are read");
	ok(kj_bool(j, "t", 0) && !kj_bool(j, "f", 1), "true and false");
	ok(kj_bool(j, "z", 1) == 1, "null falls back to the default");
	ok(kj_len(kj_get(j, "arr")) == 2, "arrays count their members");
	eq_str(kj_str(kj_get(j, "arr")->child->next, "k", ""), "v",
	       "an object inside an array");
	ok(kj_str(j, "missing", "def") != NULL &&
	   !strcmp(kj_str(j, "missing", "def"), "def"), "a missing key defaults");
	kj_free(j);

	/* Anything that does not parse WHOLE is not a document. A manifest
	 * read as half a manifest would restore half a tree. */
	ok(kj_parse("not json") == NULL, "bare text is refused");
	ok(kj_parse("{\"a\": 1") == NULL, "a truncated object is refused");
	ok(kj_parse("{\"a\": 1} trailing") == NULL, "trailing junk is refused");
	ok(kj_parse("{\"a\": }") == NULL, "a missing value is refused");
	ok(kj_parse("[1, 2,]") == NULL, "a trailing comma is refused");
	ok(kj_parse("{}") != NULL, "an empty object is still a document");

	/* /proc reports st_size 0. A whole-file read that trusts it returns an
	 * empty string, and "no mounts" is the wrong answer to act on. */
	size_t plen = 0;
	char *mounts = kb_read_all("/proc/self/mounts", &plen);
	ok(mounts && plen > 0, "a whole-file read gets past a stat'ed size of 0");
	free(mounts);

	free(pd);
	free(pd2);
	free(env);
	kb_rmtree(dir);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void test_ramps(int caps, int want_levels, const char *tier)
{
	ktui_caps = caps;
	ktui_ramp_init();
	checks++;
	if (ktui_ramp_levels() != want_levels) {
		failures++;
		printf("  FAIL  %s ramp has %d levels, want %d\n", tier,
		       ktui_ramp_levels(), want_levels);
	}

	/* Endpoints are exact: an empty bar must not show a sliver and a full
	 * one must not stop one shade short. */
	eq_str(ktui_ramp_h(0.0), " ", "empty cell is a space");
	eq_str(ktui_ramp_v(0.0), " ", "empty vertical cell is a space");

	/* Monotonic, and never out of range at the edges. */
	int mono = 1;
	const char *prev = ktui_ramp_h(0.0);
	for (int i = 0; i <= 100; i++) {
		const char *now = ktui_ramp_h(i / 100.0);
		if (!now)
			mono = 0;
		prev = now;
	}
	(void)prev;
	ok(mono, "the horizontal ramp answers every input");
	ok(ktui_ramp_h(-5.0) != NULL && ktui_ramp_h(9.0) != NULL,
	   "out-of-range input is clamped, not indexed");
	ok(!strcmp(ktui_ramp_h(9.0), ktui_ramp_h(1.0)),
	   "above 1 is the full cell");

	/* Small non-zero f must show a sliver, not empty: the bar's tip tells
	 * the story of how full it is, and a rounded-down tip lies about that. */
	ok(strcmp(ktui_ramp_h(0.01), ktui_ramp_h(0.0)) != 0,
	   "a small non-zero fraction is never the empty cell");
	ok(strcmp(ktui_ramp_v(0.01), ktui_ramp_v(0.0)) != 0,
	   "and the same vertically");
}

static void test_chart(void)
{
	printf("libktui charts\n");
	int saved = ktui_caps;
	test_ramps(KT_CAP_UTF8, 8, "rich");
	test_ramps(KT_CAP_UTF8 | KT_CAP_LINUXVT, 3, "vt");
	test_ramps(0, 3, "ascii");

	/* The vt tier may only use glyphs the shipped console font carries. */
	ktui_caps = KT_CAP_UTF8 | KT_CAP_LINUXVT;
	ktui_ramp_init();
	eq_str(ktui_ramp_h(1.0), "█", "vt full cell is FULL BLOCK");
	eq_str(ktui_ramp_h(0.5), "▒", "vt mid cell is MEDIUM SHADE");
	eq_str(ktui_ramp_h(0.2), "░", "vt low cell is LIGHT SHADE");

	ktui_caps = KT_CAP_UTF8;
	ktui_ramp_init();
	eq_str(ktui_ramp_h(1.0), "█", "rich full cell is FULL BLOCK");
	eq_str(ktui_ramp_v(0.125), "▁", "rich vertical starts at LOWER ONE EIGHTH");

	ktui_caps = saved;
	ktui_ramp_init();

	/* The integer/tip split. A 40-cell bar at exactly half is 20 solid
	 * cells and NO tip — a tip there would read as 51%. */
	double tip = -1;
	ok(ktui_bar_fill(40, 0.5, &tip) == 20 && tip == 0.0,
	   "half of 40 cells is 20 solid and no tip");
	ok(ktui_bar_fill(40, 0.5125, &tip) == 20 && tip > 0.49 && tip < 0.51,
	   "half a cell past half is 20 solid plus a half tip");
	ok(ktui_bar_fill(40, 0.0, &tip) == 0 && tip == 0.0, "zero is empty");
	ok(ktui_bar_fill(40, 1.0, &tip) == 40 && tip == 0.0, "one is full");
	ok(ktui_bar_fill(40, 1.5, &tip) == 40 && tip == 0.0, "over one clamps");
	ok(ktui_bar_fill(40, -1.0, &tip) == 0 && tip == 0.0, "under zero clamps");

	/* Never w+1 cells: solid + (tip ? 1 : 0) has to fit. */
	int overflow = 0;
	for (int w = 1; w <= 200; w++)
		for (int i = 0; i <= 1000; i++) {
			int solid = ktui_bar_fill(w, i / 1000.0, &tip);
			if (solid + (tip > 0 ? 1 : 0) > w)
				overflow = 1;
		}
	ok(!overflow, "solid plus tip never exceeds the bar width");
}

/* ──────────────────────────────────────────────────────────────────────── */

static void vcmp(const char *a, const char *b, int want, const char *what)
{
	checks++;
	int got = pu_vercmp(a, b);
	if (got == want)
		return;
	failures++;
	printf("  FAIL  %s: vercmp(%s,%s) = %d, want %d\n", what, a, b, got, want);
}

static void vcmp_sym(const char *a, const char *b, int want, const char *what)
{
	vcmp(a, b, want, what);
	vcmp(b, a, -want, what);	/* a comparator that is not antisymmetric
					   is not an ordering */
}

/* Test helper: check if a candidate is present among the extracted ones. */
static int pu_extract_has(char cand[][PU_MAX_VER], int n, const char *want)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(cand[i], want))
			return 1;
	return 0;
}

static void test_portup(void)
{
	printf("kdos-portup\n");

	/* Every one of these is a real version pair from the ports tree. A
	 * comparator that gets any of them wrong proposes a downgrade, and a
	 * downgrade costs a full rebuild to notice. */
	vcmp_sym("1.4rc5", "1.4", -1, "a release candidate precedes its release");
	vcmp_sym("1.3.0rc1", "1.3.0", -1, "rc with a dotted base");
	vcmp_sym("1.0pre4", "1.0", -1, "pre behaves like rc");
	vcmp_sym("10.2p1", "10.2", 1, "OpenSSH's p-suffix is an increment");
	vcmp_sym("1.9.17p2", "1.9.17p1", 1, "sudo's p-suffixes order numerically");
	vcmp_sym("3.6a", "3.6", 1, "tmux's letter suffix is an increment");
	vcmp_sym("20250826", "20250901", -1, "dates order as integers");
	vcmp_sym("1.10.0", "1.9.0", 1, "components are numeric, not lexical");
	vcmp_sym("r62", "r52", 1, "inih's rNN");
	vcmp_sym("8.17.0", "8.17.0", 0, "equal");
	vcmp_sym("1.4.0", "1.4", 1, "more components wins when the prefix ties");
	vcmp_sym("1.4.0", "1.4.0.0", -1, "a trailing component is significant, "
				       "as in dpkg and rpm");
	vcmp_sym("99999999999999999999999", "1.0", 1,
		 "a run too long for a long still orders as greater");
	vcmp_sym("99999999999999999999999", "99999999999999999999998", 1,
		 "two saturating runs order by their digits");
	vcmp_sym("007", "7", 0, "leading zeroes do not change the value");

	char sh[64];
	pu_shape("1.4.0", sh, sizeof(sh));
	eq_str(sh, "N.N.N", "shape of a three-part version");
	pu_shape("20250826", sh, sizeof(sh));
	eq_str(sh, "N8", "a lone digit run records its length");
	pu_shape("1", sh, sizeof(sh));
	eq_str(sh, "N1", "so a date cannot match a bare 1");
	pu_shape("r62", sh, sizeof(sh));
	eq_str(sh, "aN", "letter then digits");
	pu_shape("10.2p1", sh, sizeof(sh));
	eq_str(sh, "N.NaN", "sudo/openssh p-suffix shape");
	pu_shape("1.4rc5", sh, sizeof(sh));
	eq_str(sh, "N.NaN", "rc shares the p-suffix shape, which is fine: "
			    "shape only gates comparability");

	/* Real strings the adapters will hand it. */
	char cand[PU_MAX_CAND][PU_MAX_VER];
	int n;

	n = pu_extract("epoch-1.5.0", cand, PU_MAX_CAND);
	ok(n == 2, "epoch tag yields two (raw and cleaned)");
	ok(pu_extract_has(cand, n, "1.5.0"), "cleaned version is present");

	n = pu_extract("v2.1-20250901", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "v2.1-20250901"), "luajit's tag yields the whole form");
	ok(pu_extract_has(cand, n, "2.1-20250901"), "the form without v prefix");
	ok(pu_extract_has(cand, n, "20250901"), "and the date component");

	n = pu_extract("cacert-2026-01-15.pem", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "2026-01-15"), "a dated pem yields the whole date");

	n = pu_extract("fuse-3.19.0.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "3.19.0"), "a tarball name yields its version");

	n = pu_extract("no-version-here", cand, PU_MAX_CAND);
	ok(n == 0, "a string with no version yields nothing");

	/* The extractor is deliberately generous and the SHAPE FILTER is what
	 * narrows it — that split is why no adapter needs to know about
	 * `epoch-` or `v2.1-` conventions. */
	n = pu_extract("Release_1_16_1", cand, PU_MAX_CAND);
	ok(n == 2, "underscores separate into digit runs");
	ok(pu_extract_has(cand, n, "1"), "including versions with repeated digits");
	ok(pu_extract_has(cand, n, "16"), "and longer ones");

	n = pu_extract("curl-8.12.1.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "8.12.1"), ".tar.xz is trimmed — 129 ports use it");
	n = pu_extract("mesa-25.2.0.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "25.2.0"), ".tar.bz2 too — another 40 ports");
	n = pu_extract("zlib-1.3.1.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.3.1"), "and .tar.gz still works");

	/* r62 is inih's real version; v1.2.3's v is a tag convention. The
	 * extractor emits both forms rather than guessing which letters
	 * belong to the version — the shape filter is what decides. */
	n = pu_extract("r62", cand, PU_MAX_CAND);
	ok(n >= 2, "a letter-prefixed run yields multiple forms");
	ok(pu_extract_has(cand, n, "r62"), "the run as written");
	ok(pu_extract_has(cand, n, "62"), "and without the prefix");

	/* Multi-hyphen package names were yielding prefix+rest (e.g., "conf-1.2.5.1")
	 * instead of the version alone. The new generative rules emit all useful
	 * forms and the shape filter picks the matching one. Cost a debug cycle:
	 * 15+ ports were broken; alsa-topology-conf, gst-plugins-{bad,base,good,ugly},
	 * desktop-file-utils, shared-mime-info, xcb-util-{cursor,image,renderutil}
	 * and others all reported unknown forever. */
	n = pu_extract("alsa-topology-conf-1.2.5.1.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.2.5.1"),
	   "a name with three hyphens still yields its version");
	n = pu_extract("gst-plugins-bad-1.28.0.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.28.0"), "and a two-hyphen name");
	n = pu_extract("cacert-2026-01-15.pem", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "2026-01-15"),
	   "while a hyphenated DATE survives intact");

	/* Rules must compose so that <name>-v<version> tarballs work. node-v22.14.0
	 * needs both "node-" stripping (rule 2) AND "v" stripping (rule 1b) to yield
	 * "22.14.0". Cost a debug cycle: 9 ports regressed (brotli, iso-codes,
	 * libglvnd, libslirp, libuv, lynx, nodejs, spirv-tools, upower) because the
	 * transformations were not iterative. */
	n = pu_extract("node-v22.14.0.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "22.14.0"),
	   "a <name>-v<version> tarball yields the bare version");
	n = pu_extract("upower-v1.90.9.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.90.9"), "and so does a .tar.bz2 one");
	n = pu_extract("alsa-topology-conf-1.2.5.1.tar.bz2", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "1.2.5.1"), "without losing round 2's fix");

	/* LLVM uses .src convention; Debian uses .orig. Both must trim. */
	n = pu_extract("llvm-21.1.8.src.tar.xz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "21.1.8"),
	   "LLVM .src.tar.xz tarballs yield bare version");
	n = pu_extract("libaio_0.3.113.orig.tar.gz", cand, PU_MAX_CAND);
	ok(pu_extract_has(cand, n, "0.3.113"),
	   "Debian .orig.tar.gz tarballs yield bare version");

	/* No candidate may repeat: a duplicate means dedup ran before a
	 * transformation rather than after it. Release_1_16_1 yields repeated "1"
	 * before dedup, so this check genuinely exercises that invariant. */
	n = pu_extract("Release_1_16_1", cand, PU_MAX_CAND);
	ok(n >= 2, "the dedup check needs at least two candidates to mean anything");
	int dup = 0;
	for (int i = 0; i < n; i++)
		for (int k = i + 1; k < n; k++)
			if (!strcmp(cand[i], cand[k]))
				dup = 1;
	ok(!dup, "no candidate repeats");
}

/* ──────────────────────────────────────────────────────────────────────── */

int main(void)
{
	kb_set_progname("selftest");

	test_base();
	test_colour();
	test_pkg();
	test_build();
	test_chart();
	test_portup();

	printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
