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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kcolor.h"
#include "kbuild.h"
#include "kpkg.h"

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
	kp_owned_free(ow);
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

int main(void)
{
	kb_set_progname("selftest");

	test_base();
	test_colour();
	test_pkg();
	test_build();

	printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
