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
#include "kproc.h"
#include "kxdg.h"
#include "kpack.h"

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

static void eq_int(long long got, long long want, const char *what)
{
	checks++;
	if (got == want)
		return;
	failures++;
	printf("  FAIL  %s\n        got  %lld\n        want %lld\n", what,
	       got, want);
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

	/*
	 * kcol_contrast is integer and table-driven because libkcolor may not
	 * link libm. The claim is that it agrees with the real WCAG formula to
	 * the two decimals every threshold in this tree is expressed in, and
	 * the value that matters most is the one nobody would guess: `variant`
	 * against `backdrop` is 1.00:1 — the panel's own background is the same
	 * colour as the desktop, which is the whole reason the tone ladder
	 * exists.
	 */
	ok(kcol_contrast(0x04120a, 0x02120a) == 100,
	   "phosphor SURFACE vs BG is 1.00:1 — the bar has no body of its own");
	ok(kcol_contrast(0x39ff14, 0x04120a) == 1413, "accent on surface 14.13:1");
	ok(kcol_contrast(0x12401f, 0x04120a) == 162, "dim is a FILL at 1.62:1");
	ok(kcol_contrast(0x1f8f0c, 0x04120a) == 455, "mid is a LABEL at 4.55:1");
	ok(kcol_contrast(0xffffff, 0xffffff) == 100, "a colour against itself");

	/*
	 * kxdg_recent is a jump list's data, read out of freedesktop's
	 * recently-used.xbel by a SCANNER rather than an XML parser — this
	 * tree ships no XML library and one bookmark file is not the reason to
	 * start. The fixture carries the four things that scanner has to get
	 * right, and each is a way a jump list misleads rather than fails:
	 * newest first, a percent-escaped path decoded, a file that has since
	 * been deleted left out, and another application's entries not
	 * offered under this one's name.
	 */
	{
		char got[8][512];
		int n;

		setenv("XDG_DATA_HOME", "testing/fixtures/recent", 1);
		n = kxdg_recent("nvim", got, 8);
		ok(n == 2, "recent: the deleted destination is not offered");
		ok(n == 2 && !strcmp(got[0], "/etc/os-release"),
		   "recent: newest first, and %2D is decoded");
		ok(n == 2 && !strcmp(got[1], "/etc/hostname"),
		   "recent: the older one follows");
		n = kxdg_recent("gimp", got, 8);
		ok(n == 1 && !strcmp(got[0], "/etc/passwd"),
		   "recent: another application's files are its own");
		n = kxdg_recent("no-such-app", got, 8);
		ok(n == 0, "recent: an application with no history has none");
		unsetenv("XDG_DATA_HOME");
	}
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

	/*
	 * kcol_muted is T9: `dim` was used as a TEXT colour in every scheme and
	 * lands around 1.6:1 against `deep`, which is below any legibility
	 * floor. The claim is not "muted is a colour" — it is that muted sits
	 * strictly between the background and the text, and is farther from the
	 * background than dim is by a wide enough margin to be a different
	 * decision rather than a nudge. Checked for every scheme, because a
	 * palette that only reads in phosphor is the bug this replaces.
	 */
	int muted_ok = 1, muted_far = 1;
	for (int i = 0; i < kcol_nscheme; i++) {
		const KcolScheme *sc = &kcol_schemes[i];
		unsigned m = kcol_muted(sc);
		double ld = kcol_luma(sc->deep), lm = kcol_luma(m);
		if (m == sc->dim || m == sc->text || m == sc->deep)
			muted_ok = 0;
		if (!(ld < kcol_luma(sc->dim) && kcol_luma(sc->dim) < lm &&
		      lm < kcol_luma(sc->text)))
			muted_ok = 0;
		/* Measured: 2.2x in bone, 2.7x in phosphor. */
		if ((lm - ld) <= 2.0 * (kcol_luma(sc->dim) - ld))
			muted_far = 0;
	}
	ok(muted_ok, "muted is its own colour, between deep and text, in every scheme");
	ok(muted_far, "muted is more than twice as far from the background as dim");
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

	/*
	 * A size field that does not fit is a REFUSAL, not a number. GNU
	 * base-256 puts the size in the low bytes of a 12-byte field, so
	 * eleven shifts of 8 overflow a long long — undefined behaviour, and
	 * the value it used to produce was negative. Everything downstream
	 * read that as a length: the GNU-long-name branch computed
	 * `(size_t)size` and asked read() for 2^63 bytes into a 512-byte
	 * stack buffer, and the only thing that stopped it was the kernel
	 * refusing an address range that large. Found by fuzzing kb_tar_next.
	 */
	char *badpath = kb_path_join(dir, "bad.tar");
	unsigned char hdr[512] = {0};
	memcpy(hdr, "longname", 8);
	hdr[124] = 0x80;	/* base-256 marker */
	hdr[128] = 0x80;	/* lands in bit 63 after the shifts */
	hdr[156] = 'L';		/* GNU long name: the payload is a length */
	memcpy(hdr + 257, "ustar\0" "00", 8);
	int bfd = open(badpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ok(bfd >= 0, "create a corrupt tar");
	ok(write(bfd, hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr), "write its header");
	close(bfd);
	KbTarIn bt;
	KbTarEntry be;
	ok(kb_tar_open(&bt, badpath) == 0, "open the corrupt tar");
	ok(kb_tar_next(&bt, &be) == -1, "a size that overflows is refused, not read");
	kb_tar_close(&bt);
	free(badpath);

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

/*
 * The freedesktop trash. It is libkbase's rather than either front end's
 * because `kdos trash` at a prompt and `kdos-desk`'s Delete key have to mean
 * the same thing, and every assertion below is a way the two could quietly
 * come apart if there were two copies.
 */
static void test_trash(void)
{
	char dir[] = "/tmp/kdos-selftest-trash.XXXXXX";
	char oldhome[512] = "", oldcwd[512] = "";
	char work[200], a[240], b[240], p[400];
	const char *home = getenv("HOME");
	KbTrashItem *v;
	char to[KB_TRASH_PATH];
	int n;

	printf("libkbase trash\n");
	if (home)
		kb_strlcpy(oldhome, home, sizeof(oldhome));
	if (!getcwd(oldcwd, sizeof(oldcwd)))
		oldcwd[0] = '\0';
	ok(mkdtemp(dir) != NULL, "scratch directory");
	setenv("HOME", dir, 1);

	snprintf(work, sizeof(work), "%s/w", dir);
	snprintf(a, sizeof(a), "%s/a", work);
	snprintf(b, sizeof(b), "%s/b", work);
	kb_mkdir_p(a);
	kb_mkdir_p(b);

	/*
	 * TWO FILES OF THE SAME NAME FROM DIFFERENT DIRECTORIES is the ordinary
	 * case, and the second silently replacing the first is data loss.
	 */
	snprintf(p, sizeof(p), "%s/notes.txt", a);
	kb_write_file(p, "one\n");
	ok(kb_trash_put(p) == 0, "trash the first notes.txt");
	snprintf(p, sizeof(p), "%s/notes.txt", b);
	kb_write_file(p, "two\n");
	ok(kb_trash_put(p) == 0, "trash the second notes.txt");

	n = kb_trash_list(&v);
	ok(n == 2, "both are in the trash");
	free(v);

	/* And each goes back where IT came from, which is the whole reason the
	 * record exists. */
	ok(kb_trash_restore("notes.txt", to, sizeof(to)) == 0, "restore the first");
	snprintf(p, sizeof(p), "%s/notes.txt", a);
	eq_str(to, p, "the first went back to a/");
	ok(kb_trash_restore("notes.txt.1", to, sizeof(to)) == 0, "restore the second");
	snprintf(p, sizeof(p), "%s/notes.txt", b);
	eq_str(to, p, "the second went back to b/");

	/* A name with a space and a percent in it. The Path= value is a URI, so
	 * it is escaped on the way in and unescaped on the way out; getting one
	 * half wrong restores to a path with `%20` in its name. */
	snprintf(p, sizeof(p), "%s/od%%d file.txt", work);
	kb_write_file(p, "x\n");
	ok(kb_trash_put(p) == 0, "trash an awkward name");
	ok(kb_trash_restore("od%d file.txt", to, sizeof(to)) == 0,
	   "restore an awkward name");
	eq_str(to, p, "the escape round-trips");

	/* Restoring onto something that is there NOW would destroy it. */
	ok(kb_trash_put(p) == 0, "trash it again");
	kb_write_file(p, "in the way\n");
	ok(kb_trash_restore("od%d file.txt", to, sizeof(to)) == -1 &&
	   errno == EEXIST, "restore refuses to overwrite");

	/* Trashing the trash empties it in the worst possible order. */
	{
		char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
		kb_trash_dirs(files, sizeof(files), info, sizeof(info));
		ok(kb_trash_put(files) == -1, "the trash cannot be trashed");
	}

	/* A DIRECTORY comes back too, and empty takes both halves of every
	 * record: an info/ entry outliving its file is the state every other
	 * implementation ignores. */
	snprintf(p, sizeof(p), "%s/adir", work);
	kb_mkdir_p(p);
	snprintf(a, sizeof(a), "%s/adir/f", work);
	kb_write_file(a, "y\n");
	ok(kb_trash_put(p) == 0, "trash a directory");
	ok(kb_trash_empty() >= 1, "empty removes it");
	n = kb_trash_list(&v);
	ok(n == 0, "the trash is empty");
	free(v);
	{
		char files[KB_TRASH_PATH], info[KB_TRASH_PATH];
		int left = 0;
		char **nm;
		kb_trash_dirs(files, sizeof(files), info, sizeof(info));
		nm = kb_listdir(info, &left);
		ok(left == 0, "and so is info/");
		kb_strv_free(nm);
	}

	if (oldhome[0])
		setenv("HOME", oldhome, 1);
	else
		unsetenv("HOME");
	/* The chdir back is best effort: the scratch directory is going next
	 * line either way, and a failure here must not fail the suite. */
	if (oldcwd[0]) {
		int rc = chdir(oldcwd);
		(void)rc;
	}
	kb_rmtree(dir);
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

/*
 * libkproc, against testing/fixtures/res.
 *
 * Every assertion here is a rule that fails SILENTLY when it is wrong: a
 * doubled disk rate, a battery at 100% health, a wrapped counter as a spike,
 * an unreadable io column of zeroes. None of them crashes, and none of them is
 * visible to the compiler.
 */
static void test_proc(void)
{
	printf("libkproc\n");

	const char *base = getenv("KDOS_RES_FIXTURE");
	if (!base)
		base = "testing/fixtures/res";

	char p0[512], s0[512], p1[512], s1[512];
	snprintf(p0, sizeof(p0), "%s/proc", base);
	snprintf(s0, sizeof(s0), "%s/sys", base);
	snprintf(p1, sizeof(p1), "%s/next/proc", base);
	snprintf(s1, sizeof(s1), "%s/next/sys", base);

	if (!kb_path_exists(p0)) {
		printf("  (fixture absent — skipped)\n");
		return;
	}

	kpr_root_set(p0, s0);

	/* ── CPU: topology is read, never counted ───────────────────────── */
	KprCpu c0;
	ok(kpr_cpu_read(&c0) == 0, "cpu snapshot reads");
	eq_int(c0.ncpu, 2, "logical cpus from /proc/stat");
	/* cpuinfo has two blocks and the machine has two CORES; a tree where
	 * these differ is what catches counting cpuinfo and calling it cores. */
	eq_int(c0.ncore, 2, "cores from topology/, not from cpuinfo blocks");
	eq_int(c0.npkg, 1, "packages from topology/");
	eq_int(c0.virt, KPR_VIRT_QEMU, "virtualisation from DMI sys_vendor");
	ok(c0.temp_c > 51.9 && c0.temp_c < 52.1, "cpu temperature from hwmon");
	eq_str(c0.governor, "schedutil", "cpufreq governor");

	kpr_root_set(p1, s1);
	KprCpu c1;
	kpr_cpu_read(&c1);
	double busy = kpr_cpu_busy(&c0.total, &c1.total);
	/* delta: user+sys 500, idle+iowait 500, total 1000 -> exactly half.
	 * Computed over the DELTA; over the absolute it would be ~13%. */
	ok(busy > 0.49 && busy < 0.51, "cpu busy is over the delta, not the total");
	kpr_cpu_free(&c0);
	kpr_cpu_free(&c1);

	kpr_root_set(p0, s0);

	/* ── memory: MemAvailable, never total - free ───────────────────── */
	KprMem m;
	ok(kpr_mem_read(&m) == 0, "meminfo reads");
	ok(m.available == 5242880ULL * 1024ULL, "available is MemAvailable");
	/*
	 * The fixture's available is deliberately NOT half of total: with
	 * total/2 the used figure and the available figure come out equal and
	 * a swapped subtraction renders identically to a correct one.
	 */
	ok(m.total - m.available != m.available, "used and available differ");
	/* total - free would be 15.5 GB of 16 GB — a healthy machine at 97%. */
	ok(m.total - m.available < m.total - m.free, "used is not total - free");

	/* ── pressure: absent is not zero ───────────────────────────────── */
	KprPsi psi;
	ok(kpr_psi_read("io", &psi) == 0 && psi.present, "psi io present");
	ok(psi.some10 > 11.9 && psi.some10 < 12.1, "psi some avg10");
	ok(psi.full10 > 7.9 && psi.full10 < 8.1, "psi full avg10");
	KprPsi none;
	ok(kpr_psi_read("nosuchfile", &none) != 0 || !none.present,
	   "a missing pressure file is absent, not a stall of zero");

	/* ── processes ──────────────────────────────────────────────────── */
	KprSample sm;
	ok(kpr_sample_take(&sm, KPR_WANT_IO | KPR_WANT_CMDLINE |
			       KPR_WANT_STATUS) == 0, "sample taken");
	eq_int(sm.n, 9, "every pid in the fixture");
	eq_int(sm.nkthread, 2, "kernel threads counted, not dropped");

	const KprProc *fx = kpr_find_pid(&sm, 950);
	ok(fx != NULL, "the boxed app is in the sample");
	eq_str(fx ? fx->comm : "", "firefox-esr", "comm parsed");
	const KprProc *wc = kpr_find_pid(&sm, 951);
	/* comm carries a space; the split must be on the LAST ')' or every
	 * field after it is read from the wrong place. */
	eq_str(wc ? wc->comm : "", "Web Content", "a comm with a space in it");
	ok(wc && wc->ppid == 950, "ppid survives a comm with a space");

	const KprProc *bk = kpr_find_pid(&sm, 800);
	ok(bk && bk->state == 'D', "the blocked process is in D");
	ok(bk && bk->rd_bytes != KPR_UNREADABLE && bk->rd_bytes > 0,
	   "a readable io counter is a number");

	/* The trap this sentinel exists for: an unreadable counter must never
	 * reach a column as 0, which would claim root's sshd did no disk io. */
	const KprProc *sd = kpr_find_pid(&sm, 300);
	ok(sd && sd->rd_bytes == KPR_UNREADABLE,
	   "an unreadable io is KPR_UNREADABLE, never 0");
	ok(sd && sd->wr_bytes == KPR_UNREADABLE, "both io counters, not just one");

	/* ── identity: the box, and its boundary ────────────────────────── */
	char box[64];
	ok(kpr_box_of(&sm, 950, box, sizeof(box)) && !strcmp(box, "kdos-apps"),
	   "a process under conmon is in its box");
	ok(kpr_box_of(&sm, 951, box, sizeof(box)) && !strcmp(box, "kdos-apps"),
	   "and so is its child, two hops up");
	ok(!kpr_box_of(&sm, 700, box, sizeof(box)),
	   "a process whose parent chain leaves the box is in none");
	/* conmon runs on the HOST and supervises from outside; reporting it as
	 * a member would put the supervisor in the app's own rollup. */
	ok(!kpr_box_of(&sm, 900, box, sizeof(box)),
	   "conmon itself is not in the box it supervises");

	/* ── cpu% of one core, and the new-process case ─────────────────── */
	kpr_root_set(p1, s1);
	KprSample sm1;
	kpr_sample_take(&sm1, 0);
	const KprProc *a = kpr_find_pid(&sm, 950), *b = kpr_find_pid(&sm1, 950);
	double pc = kpr_proc_cpu(a, b, 1000);
	ok(pc > 0.0, "a process that used cpu reports some");
	/* No previous sample means no interval to divide by; reporting the
	 * whole lifetime as this second's usage is how a fresh process shows
	 * thousands of percent. */
	ok(kpr_proc_cpu(NULL, b, 1000) == 0.0, "a process first seen reports 0");
	kpr_sample_free(&sm1);
	kpr_sample_free(&sm);
	kpr_root_set(p0, s0);

	/* ── block: whole disks only ────────────────────────────────────── */
	KprDisk *d = NULL;
	int nd = kpr_block_list(&d);
	/* sda1 is in diskstats beside sda. Summing both counts every byte
	 * twice, which is the defect this count catches. */
	eq_int(nd, 3, "sda, nvme0n1 and loop0 — sda1 is a partition");
	int seen_sda = 0, seen_part = 0, seen_loop_virt = 0;
	for (int i = 0; i < nd; i++) {
		if (!strcmp(d[i].name, "sda"))
			seen_sda = 1;
		if (!strcmp(d[i].name, "sda1"))
			seen_part = 1;
		if (!strcmp(d[i].name, "loop0") && d[i].virt)
			seen_loop_virt = 1;
	}
	ok(seen_sda, "the whole disk is kept");
	ok(!seen_part, "the partition is not");
	ok(seen_loop_virt, "loop0 is flagged virtual, not dropped");
	for (int i = 0; i < nd; i++)
		if (!strcmp(d[i].name, "sda")) {
			/* A diskstats sector is 512 by definition of that
			 * interface, whatever the drive's own sector size. */
			ok(d[i].size == 976773168ULL * 512ULL,
			   "size is sysfs 512-byte units");
			ok(d[i].rotational == 1, "rotational read from queue/");
		}
	kpr_block_free(d);

	/* ── net: a wrap is a gap, not a negative ───────────────────────── */
	KprIface *n0 = NULL;
	int nn = kpr_net_list(&n0);
	ok(nn == 3, "three interfaces");
	unsigned long long eth_rx0 = 0;
	for (int i = 0; i < nn; i++) {
		if (!strcmp(n0[i].name, "lo")) {
			/* operstate is "unknown" for loopback for ever; IFF_UP
			 * out of the flags word is the real answer. */
			ok(n0[i].up, "lo is up from flags, not from operstate");
			ok(!n0[i].carrier, "and its operstate is not up");
			ok(n0[i].loopback && n0[i].virt, "lo is flagged");
		}
		if (!strcmp(n0[i].name, "eth0")) {
			eth_rx0 = n0[i].rx_bytes;
			ok(n0[i].speed_mbit == 1000, "speed where the driver has one");
			eq_str(n0[i].driver, "", "no driver link in the fixture");
		}
		if (!strcmp(n0[i].name, "wlan0"))
			ok(n0[i].speed_mbit == -1,
			   "no speed is -1, never 0 Mbit");
	}
	kpr_net_free(n0);

	kpr_root_set(p1, s1);
	KprIface *n1 = NULL;
	kpr_net_list(&n1);
	unsigned long long eth_rx1 = 0;
	for (int i = 0; i < 3; i++)
		if (!strcmp(n1[i].name, "eth0"))
			eth_rx1 = n1[i].rx_bytes;
	kpr_net_free(n1);
	/* 4294967000 -> 200 across the interval. A subtraction produces an
	 * enormous negative; the caller must see the decrease and report a
	 * gap. The library's job is to hand back both readings faithfully. */
	ok(eth_rx1 < eth_rx0, "the wrapped counter decreased");
	kpr_root_set(p0, s0);

	/* ── power: health is wear, capacity is charge ──────────────────── */
	KprBattery *bt = NULL;
	int nb = kpr_power_list(&bt);
	ok(nb == 2, "one battery and one adapter");
	for (int i = 0; i < nb; i++) {
		if (!bt[i].is_battery) {
			ok(bt[i].online == 0, "the adapter is unplugged");
			continue;
		}
		ok(bt[i].capacity == 90, "capacity is the charge");
		/* 50 Wh of a design 62 Wh. A monitor printing only capacity
		 * would say this battery is fine. */
		ok(bt[i].health > 0.80 && bt[i].health < 0.81,
		   "health is full / full_design, not capacity");
		ok(bt[i].cycles == 412, "cycle count");
		ok(bt[i].power_uw == 8500000, "draw from power_now");
	}
	kpr_power_free(bt);

	/* ── the ring, the axis and the smoothing ───────────────────────── */
	KprHist h;
	kpr_hist_init(&h, 0);
	for (int i = 0; i < 5; i++)
		kpr_hist_push(&h, 1000.0);
	double sc = kpr_hist_scale(&h);
	ok(sc == 16e3, "a small series sits on the first rung");
	kpr_hist_push(&h, 100000.0);
	ok(kpr_hist_scale(&h) == 256e3, "the axis grows the moment a sample does not fit");
	/*
	 * Hysteresis. A third of 256e3 is 85.3e3, so a series peaking at 100e3
	 * is inside the band and must hold the axis still; one peaking at 1e3
	 * is well under it and must let the axis down. One threshold in each
	 * direction would oscillate between two rungs for a series sitting on
	 * the boundary, which is the same flicker wearing a different hat.
	 */
	for (int i = 0; i < KPR_HIST; i++)
		kpr_hist_push(&h, 100e3);
	ok(kpr_hist_scale(&h) == 256e3, "the axis holds above a third of itself");
	for (int i = 0; i < KPR_HIST; i++)
		kpr_hist_push(&h, 1000.0);
	ok(kpr_hist_scale(&h) < 256e3, "and only comes down below it");

	KprHist pin;
	kpr_hist_init(&pin, 1);
	kpr_hist_push(&pin, 3.0);
	ok(kpr_hist_scale(&pin) == 100.0,
	   "a percentage ring is pinned 0..100 and never rescales");

	KprHist sm3;
	kpr_hist_init(&sm3, 0);
	kpr_hist_push(&sm3, 0.0);
	kpr_hist_push(&sm3, 30.0);
	kpr_hist_push(&sm3, 0.0);
	ok(kpr_hist_smooth(&sm3, 1) == 10.0, "the plotted series is a three-point mean");
	ok(kpr_hist_at(&sm3, 1) == 30.0, "the stored sample is never smoothed");
	ok(kpr_hist_peak(&sm3) == 30.0, "peak is of the raw series");
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
	/* Freed rather than dropped so the whole suite runs clean under
	 * `CC="cc -fsanitize=address,undefined" testing/selftest.sh` — one
	 * leaked node is enough to make LeakSanitizer's verdict useless. */
	KjNode *empty = kj_parse("{}");
	ok(empty != NULL, "an empty object is still a document");
	kj_free(empty);

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

/*
 * The cell grid's own arithmetic: widths, the paste queue, the caret, and one
 * modal rendered offscreen.
 *
 * S3 (no wcwidth anywhere) and S2 (nothing above 0x7f could be typed) were
 * both invisible to the compiler and to a suite with no terminal, and the
 * damage they did — a CJK filename shearing every column after it in pick, a
 * non-ASCII password that could not unlock the session — is arithmetic, so it
 * is checkable here rather than by looking at a screen.
 */

static void feed(KtuiEvent *ev, int key)
{
	memset(ev, 0, sizeof(*ev));
	ev->type = KT_EVT_KEY;
	ev->key = key;
}

/* ktui_draw_dump() writes the cell buffer to stdout; there is no reader for
 * it, and a golden-frame check needs one. Redirect, render, read back. */
static char *capture_dump(void)
{
	char tmpl[] = "/tmp/kdos-selftest-dump.XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		return NULL;
	fflush(stdout);
	int saved = dup(1);
	if (saved < 0 || dup2(fd, 1) < 0) {
		close(fd);
		unlink(tmpl);
		return NULL;
	}
	ktui_draw_dump();
	fflush(stdout);
	dup2(saved, 1);
	close(saved);
	close(fd);
	size_t n = 0;
	char *text = kb_read_all(tmpl, &n);
	unlink(tmpl);
	return text;
}

/*
 * A modal, rendered at a size where it used to break, with nothing outside its
 * own rect.
 *
 * S11: the buttons were placed at `x + w - 40` with no floor, so below 46
 * columns the Yes button started to the LEFT of the dialog and drew over the
 * page behind it. The rect is recomputed here from the same three rules
 * ktui_modal_draw uses (56 wide, ktui_w - 6, floor 20) because the assertion
 * IS "the drawing stayed inside the box the layout claims", and a check that
 * asked the drawing where the box was would agree with itself.
 */
static void check_modal(int sw, int sh)
{
	KtuiEvent ev;
	char what[64];

	ktui_offscreen_init(sw, sh);
	ktui_modal_confirm("Log Out", "End this session?", "Log Out",
			   "Cancel", NULL);
	feed(&ev, 0);
	ev.type = KT_EVT_NONE;
	ktui_frame_begin(&ev);
	ktui_draw_clear();
	ktui_modal_draw();
	ktui_frame_end();

	int w = 56;
	if (w > sw - 6)
		w = sw - 6;
	if (w < 20)
		w = 20;
	int h = 1 + 6;
	int x = (sw - w) / 2, y = (sh - h) / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	char *text = capture_dump();
	if (!text) {
		ok(0, "the modal dump could not be captured");
		return;
	}
	int row = 0, col = 0, outside = 0;
	for (const char *p = text; *p; p++) {
		if (*p == '\n') {
			row++;
			col = 0;
			continue;
		}
		/* The dump is ASCII here: ktui_caps is 0 offscreen, so the box
		 * is drawn in the ascii tier and one byte is one cell. */
		if (*p != ' ' &&
		    (col < x || col >= x + w || row < y || row >= y + h))
			outside = 1;
		col++;
	}
	snprintf(what, sizeof(what),
		 "a modal at %dx%d draws nothing outside its own rect", sw, sh);
	ok(!outside, what);
	snprintf(what, sizeof(what), "a modal at %dx%d keeps both buttons", sw, sh);
	ok(strstr(text, "Log Out") && strstr(text, "Cancel"), what);
	free(text);
}

static void test_grid(void)
{
	printf("libktui grid\n");

	/* The three answers, and nothing else. */
	int only_three = 1;
	for (uint32_t cp = 0; cp <= 0x10ffff; cp++) {
		int w = ktui_wcwidth(cp);
		if (w < 0 || w > 2)
			only_three = 0;
	}
	ok(only_three, "wcwidth answers 0, 1 or 2 for every codepoint");

	ok(ktui_wcwidth('A') == 1 && ktui_wcwidth(' ') == 1 &&
	   ktui_wcwidth(0x7f) == 1, "ASCII is one cell");
	ok(ktui_wcwidth(0x00e9) == 1 && ktui_wcwidth(0x0410) == 1,
	   "Latin-1 and Cyrillic are one cell");
	ok(ktui_wcwidth(0x2500) == 1 && ktui_wcwidth(0x2591) == 1,
	   "the box-drawing and shade glyphs the console font carries are one cell");
	ok(ktui_wcwidth(0x0301) == 0, "a combining acute takes no cell");
	ok(ktui_wcwidth(0x4e2d) == 2 && ktui_wcwidth(0xac00) == 2,
	   "CJK and Hangul are two cells");
	ok(ktui_wcwidth(0xff21) == 2, "a fullwidth Latin capital is two cells");
	ok(ktui_wcwidth(0x1f600) == 2, "an emoji is two cells");

	/*
	 * The bsearch precondition.
	 *
	 * cp_in() is a binary search, so both range tables must be sorted and
	 * disjoint — an out-of-order entry makes the ranges after it
	 * UNREACHABLE and the failure is silent, a CJK name that shears one
	 * row and not another. The tables are static to ktui_draw.c and cannot
	 * be walked from here, so the precondition is asserted the only way it
	 * can be from outside: one codepoint from the first, the last and
	 * several middle ranges of each table, which is what an unsorted table
	 * stops finding. A gap between two ranges is checked too, or a table
	 * that answered 2 for everything would pass.
	 */
	static const uint32_t two[] = {
		0x1100, 0x231a, 0x2648, 0x267f, 0x2b50, 0x3000, 0x3400,
		0x4e00, 0xa000, 0xac00, 0xf900, 0xfe30, 0xff01, 0xffe0,
		0x17000, 0x1f004, 0x1f191, 0x1f300, 0x1f680, 0x1f9d1,
		0x20000, 0x3fffd,
	};
	static const uint32_t zero[] = {
		0x0300, 0x0591, 0x06df, 0x0e31, 0x102d, 0x135f, 0x1712,
		0x1dc0, 0x200b, 0x20d0, 0x302a, 0xa806, 0xaab0, 0xfe00,
		0xfeff, 0x101fd, 0x11038, 0x1d167, 0xe0100,
	};
	/* Neither table claims these: the gaps a "return 2 for everything"
	 * table would swallow. */
	static const uint32_t one[] = {
		0x1160, 0x2000, 0x2318, 0x2600, 0x303f, 0xa4d0, 0xd800,
		0xfb00, 0xff61, 0x1f000, 0x1f700, 0x40000,
	};
	int t_ok = 1, z_ok = 1, o_ok = 1;
	for (size_t i = 0; i < sizeof(two) / sizeof(*two); i++)
		if (ktui_wcwidth(two[i]) != 2)
			t_ok = 0;
	for (size_t i = 0; i < sizeof(zero) / sizeof(*zero); i++)
		if (ktui_wcwidth(zero[i]) != 0)
			z_ok = 0;
	for (size_t i = 0; i < sizeof(one) / sizeof(*one); i++)
		if (ktui_wcwidth(one[i]) != 1)
			o_ok = 0;
	ok(t_ok, "every sampled wide range is still reachable by the bsearch");
	ok(z_ok, "and every sampled zero-width one");
	ok(o_ok, "while the gaps between them stay one cell");

	/* ktui_utf8_width is what every column in this toolkit measures with. */
	ok(ktui_utf8_width("abc") == 3, "utf8_width counts ASCII");
	ok(ktui_utf8_width("中文") == 4, "two CJK glyphs are four columns");
	/* e + combining acute is ONE column, not two: a name typed with a dead
	 * key must not push the column after it out of line. */
	ok(ktui_utf8_width("e\xcc\x81") == 1,
	   "a combining mark adds no column");
	ok(ktui_utf8_width("e\xcc\x81t\xc3\xa9") == 3,
	   "and a mix of composed and decomposed measures the same either way");
	ok(ktui_utf8_width("") == 0, "an empty string is no columns");

	/* ── the paste queue ─────────────────────────────────────────────── */

	/*
	 * There is no reader for the queue: the focused ktui_input takes it.
	 * So every assertion below pushes, draws one input, and reads the
	 * buffer — which is also the only path that matters.
	 */
	KtuiEvent ev;
	char buf[256];
	KRect r = krect(0, 0, 40, 1);

	ktui_offscreen_init(40, 3);
	feed(&ev, 0);
	ev.type = KT_EVT_NONE;

#define PASTE_INTO(src, len) do {                                        \
		buf[0] = 0;                                              \
		ktui_paste_push((src), (len));                           \
		ktui_frame_begin(&ev);                                   \
		ktui_focus_set(0);                                       \
		ktui_input(r, buf, sizeof(buf), 0, NULL);                \
		ktui_frame_end();                                        \
	} while (0)

	PASTE_INTO("hello", 5);
	eq_str(buf, "hello", "a plain paste reaches the focused input");

	/* A multi-line paste must not be able to fake an Enter. */
	PASTE_INTO("one\ntwo", 7);
	eq_str(buf, "one two", "a newline in a paste becomes a space");

	PASTE_INTO("a\x01\x02\tb\x7f", 6);
	eq_str(buf, "ab", "control bytes and DEL are stripped at the door");

	/* Stripping bytes below 0x20 can never split a UTF-8 sequence — a
	 * continuation byte is >= 0x80 — and this is the check that says so. */
	PASTE_INTO("caf\xc3\xa9\x01!", 7);
	eq_str(buf, "café!", "stripping a control byte leaves a sequence intact");

	/* The 4 KB cap, and the fragment it can leave behind. The queue is
	 * filled to one byte short of full with ASCII and then handed a
	 * three-byte sequence: two bytes fit, and a lead byte with no body
	 * would be a trailing fragment every later edit trips over. */
	{
		static char big[8192];
		memset(big, 'x', sizeof(big));
		PASTE_INTO(big, sizeof(big));
		size_t n = strlen(buf);
		ok(n == sizeof(buf) - 1,
		   "an oversized paste fills the input and stops");
		int clean = 1;
		for (size_t i = 0; i < n; i++)
			if (buf[i] != 'x')
				clean = 0;
		ok(clean, "and what it inserted is the paste, not the overflow");

		static char edge[4200];
		memset(edge, 'y', sizeof(edge));
		memcpy(edge + 4090, "\xe4\xb8\xad", 3);	/* 中, on the seam */
		buf[0] = 0;
		ktui_paste_push(edge, sizeof(edge));
		char sink[8192];
		sink[0] = 0;
		ktui_frame_begin(&ev);
		ktui_focus_set(0);
		ktui_input(krect(0, 0, 40, 1), sink, sizeof(sink), 0, NULL);
		ktui_frame_end();
		size_t m = strlen(sink);
		int trailing_lead = m > 0 && (unsigned char)sink[m - 1] >= 0xc0;
		ok(!trailing_lead,
		   "a sequence cut by the 4 KB cap is dropped, not half-kept");
	}
#undef PASTE_INTO

	/* ── the caret walks sequences, not bytes ────────────────────────── */

	/*
	 * S2's other half. The caret is a BYTE index into a UTF-8 buffer, so
	 * LEFT and BACKSPACE have to move a whole sequence or they leave a
	 * half-codepoint behind — which is what a lock screen refusing a
	 * password typed with a dead key looked like.
	 */
	kb_strlcpy(buf, "", sizeof(buf));
	ktui_paste_push("héllo中", strlen("héllo中"));
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "héllo中", "multibyte text reaches the buffer whole");

	/* One BACKSPACE removes the three-byte 中, not one byte of it. */
	feed(&ev, KT_K_BACKSPACE);
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "héllo", "backspace deletes a whole sequence");

	/* LEFT over the two-byte é, then BACKSPACE, must take the 'l' before
	 * it and leave the é standing. */
	for (int i = 0; i < 4; i++) {
		feed(&ev, KT_K_LEFT);
		ktui_frame_begin(&ev);
		ktui_focus_set(0);
		ktui_input(r, buf, sizeof(buf), 0, NULL);
		ktui_frame_end();
	}
	feed(&ev, KT_K_BACKSPACE);
	ktui_frame_begin(&ev);
	ktui_focus_set(0);
	ktui_input(r, buf, sizeof(buf), 0, NULL);
	ktui_frame_end();
	eq_str(buf, "éllo", "left moves by sequence, so the delete lands on one");

	/* ── modals ──────────────────────────────────────────────────────── */

	check_modal(44, 12);
	check_modal(30, 8);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void vcmp(const char *a, const char *b, int want, const char *what)
{
	checks++;
	int got = kp_vercmp(a, b);
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


/* ──────────────────────────────────────────────────────────────────────── *
 * libkpack — a pack is a filesystem with a footer, and the footer is the
 * only thing standing between a corrupt file and a mount as root.
 *
 * THE ONE RULE UNDER TEST IS "ABSENT, NEVER PARTIAL". Every malformed shape
 * below must read as "there is no pack here" rather than as a pack with some
 * of its fields filled in: a half-read description that looks complete is
 * what gets mounted.
 * ──────────────────────────────────────────────────────────────────────── */

/* A pack whose "filesystem" is arbitrary bytes. Nothing in libkpack looks
 * inside the image — mkfs.erofs is not a dependency of the library and is not
 * on the host running this. */
static char *fake_pack(const char *dir, const char *name, const char *meta,
		       const char *fs, const char *icon)
{
	char *img = kb_path_join(dir, "img.bin");
	char *out = kb_path_join(dir, name);
	KpkMeta m;

	kb_write_file(img, fs);
	kpk_meta_parse(meta, strlen(meta), &m);
	ok(kpk_write(out, img, &m, icon, icon ? strlen(icon) : 0) == 0,
	   "kpk_write assembles a pack");
	free(img);
	return out;
}

static void poke(const char *path, long long off, unsigned char v)
{
	FILE *f = fopen(path, "r+b");
	if (!f)
		return;
	fseeko(f, (off_t)off, off < 0 ? SEEK_END : SEEK_SET);
	fwrite(&v, 1, 1, f);
	fclose(f);
}

static void test_pack(void)
{
	printf("libkpack\n");

	char dir[] = "/tmp/kdos-selftest-pack.XXXXXX";
	ok(mkdtemp(dir) != NULL, "scratch directory");

	const char *meta =
		"id          = app.gimp\n"
		"kind        = app\n"
		"name        = GNU Image Manipulation Program\n"
		"version     = 3.0.4\n"
		"release     = 1\n"
		"summary     = Create images and edit photographs\n"
		"description = A raster editor.\n"
		"description = It has layers.\n"
		"category    = Graphics\n"
		"licence     = GPL-3.0-or-later\n"
		"requires    = rt-gtk >= 1\n"
		"requires    = base\n"
		"provides    = gimp\n"
		"desktop     = gimp.desktop\n"
		"mime        = image/png\n"
		"mime        = image/xcf\n"
		"command     = gimp\n"
		"launch_cold = 18300\n";

	/* ── round trip ─────────────────────────────────────────────── */
	char *pack = fake_pack(dir, "app.gimp.kpack", meta,
			       "not really erofs, and libkpack must not care",
			       "\x89PNG\r\n\x1a\n" "fake");
	KpkPack p;
	ok(kpk_open(pack, &p) == 0, "a written pack opens");
	eq_str(p.meta.id, "app.gimp", "id survives the round trip");
	eq_int(p.meta.kind, KPK_KIND_APP, "kind parses");
	eq_str(p.meta.version, "3.0.4", "version survives");
	eq_int(p.meta.nreq, 2, "both requirements parsed");
	eq_str(p.meta.req[0].name, "rt-gtk", "requirement name");
	eq_str(p.meta.req[0].op, ">=", "requirement operator");
	eq_str(p.meta.req[0].ver, "1", "requirement version");
	eq_str(p.meta.req[1].op, "", "a bare requirement has no operator");
	eq_int(p.meta.nmime, 2, "repeated keys accumulate");
	eq_str(p.meta.description, "A raster editor.\nIt has layers.",
	       "repeated description lines become paragraphs");
	eq_int(p.meta.launch_cold, 18300, "the measured cold launch");
	eq_int((long long)p.meta.size, (long long)p.fsize,
	       "size is what the file measures, not what it claims");

	size_t ilen = 0;
	void *icon = kpk_icon_read(&p, &ilen);
	ok(icon && ilen == 12 && !memcmp(icon, "\x89PNG", 4),
	   "the icon comes back byte for byte");
	free(icon);

	/* Rendering and re-parsing must yield the same struct, or a pack
	 * cannot survive `extract-meta` and a rebuild. */
	size_t rlen = 0;
	char *rendered = kpk_meta_render(&p.meta, &rlen);
	KpkMeta again;
	kpk_meta_parse(rendered, rlen, &again);
	eq_str(again.id, p.meta.id, "render/parse round trip: id");
	eq_int(again.nmime, p.meta.nmime, "render/parse round trip: mime list");
	eq_str(again.description, p.meta.description,
	       "render/parse round trip: description");
	free(rendered);

	/* ── the payload hash and the signature ─────────────────────── */
	KsigRing ring = {0};
	char who[KSIG_ID_HEX];
	eq_int(kpk_verify(&p, &ring, who), KPK_SIG_NONE,
	       "a pack nobody signed is unsigned, not bad");

	uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
	if (ksig_keygen(seed, pub) == 0) {
		char *keydir = kb_path_join(dir, "keys");
		kb_mkdir_p(keydir);
		char *kf = kb_path_join(keydir, "b.pub");
		ksig_write_public(kf, pub, "builder");
		ok(kpk_sign(pack, seed, pub) == 0, "a pack signs");
		ok(kpk_open(pack, &p) == 0, "a signed pack still opens");
		ksig_ring_load(&ring, keydir);
		eq_int(kpk_verify(&p, &ring, who), KPK_SIG_GOOD,
		       "and verifies against the ring");

		/* A key the machine does not trust is a BAD signature, not an
		 * absent one — the distinction kpkgadd already keeps. */
		KsigRing empty = {0};
		eq_int(kpk_verify(&p, &empty, who), KPK_SIG_BAD,
		       "a signature by a key nobody trusts is refused");

		/* One byte of the filesystem, flipped. The hash is checked
		 * BEFORE the signature, so this is KPK_SIG_HASH — a caller
		 * that saw KPK_SIG_BAD here would go looking for the wrong
		 * problem. */
		poke(pack, 4, 'X');
		ok(kpk_open(pack, &p) == 0, "a tampered pack still opens");
		eq_int(kpk_verify(&p, &ring, who), KPK_SIG_HASH,
		       "one flipped payload byte fails the hash, not the signature");
		free(kf);
		free(keydir);
	}
	free(pack);

	/* ── absent, never partial ──────────────────────────────────── */
	char *p2 = fake_pack(dir, "b.kpack", "id = base\nkind = base\nversion = 1\n",
			     "fs", NULL);
	KpkFooter f;

	ok(kpk_footer_read(p2, &f, NULL) == 0, "a good footer reads");

	/* the magic */
	poke(p2, -512, 'X');
	ok(kpk_footer_read(p2, &f, NULL) != 0, "a wrong magic is refused");
	poke(p2, -512, 'K');
	ok(kpk_footer_read(p2, &f, NULL) == 0, "and reads again once restored");

	/* a footer whose offsets point past the end of the file. Without the
	 * consistency check this is a read of the whole address space — the
	 * kb_tar base-256 lesson on a different field. */
	{
		uint8_t buf[KPK_FOOTER_LEN];
		KpkFooter bad = f;
		bad.meta_len = (uint64_t)1 << 62;
		kpk_footer_pack(&bad, buf);
		FILE *fp = fopen(p2, "r+b");
		fseeko(fp, -(off_t)KPK_FOOTER_LEN, SEEK_END);
		fwrite(buf, 1, sizeof(buf), fp);
		fclose(fp);
		ok(kpk_footer_read(p2, &f, NULL) != 0,
		   "an offset past the end of the file is refused");
	}

	/* a truncated footer */
	{
		char *t = kb_path_join(dir, "short.kpack");
		kb_write_file(t, "KDOSPACK");
		ok(kpk_footer_read(t, &f, NULL) != 0,
		   "a file too short to hold a footer is absent, not partial");
		free(t);
	}

	/* a format from the future */
	{
		char *fut = kb_path_join(dir, "future.kpack");
		char *img = kb_path_join(dir, "img.bin");
		KpkMeta m;
		uint8_t buf[KPK_FOOTER_LEN];
		KpkFooter ff;
		kpk_meta_parse("id = x\nkind = app\nversion = 1\n", 30, &m);
		kb_write_file(img, "fs");
		kpk_write(fut, img, &m, NULL, 0);
		kpk_footer_read(fut, &ff, NULL);
		ff.format = KPK_FORMAT + 1;
		kpk_footer_pack(&ff, buf);
		FILE *fp = fopen(fut, "r+b");
		fseeko(fp, -(off_t)KPK_FOOTER_LEN, SEEK_END);
		fwrite(buf, 1, sizeof(buf), fp);
		fclose(fp);
		ok(kpk_footer_read(fut, &ff, NULL) != 0,
		   "a format this build does not know is not guessed at");
		free(img);
		free(fut);
	}
	free(p2);

	/* ── the metadata parser's own edges ────────────────────────── */
	{
		KpkMeta m;
		/* NOT NUL-terminated: the blob on disk is a span, and the
		 * parser is bounded by its length. Passing a shorter length
		 * must cut the value, never read past it. */
		const char *blob = "id = a\nversion = 9.9.9\nkind = app\n";
		kpk_meta_parse(blob, 6, &m);
		eq_str(m.id, "a", "a length-bounded parse stops where told");
		eq_str(m.version, "", "and does not see past the span");

		char big[2048];
		memset(big, 'x', sizeof(big));
		memcpy(big, "summary = ", 10);
		big[sizeof(big) - 1] = '\n';
		kpk_meta_parse(big, sizeof(big), &m);
		eq_str(m.summary, "",
		       "a line longer than the buffer is dropped, not halved");

		const char *unk = "id = a\nkind = app\nversion = 1\n"
				  "nonsense = 3\nno-equals-here\n";
		kpk_meta_parse(unk, strlen(unk), &m);
		eq_str(m.id, "a", "an unknown key is ignored, not fatal");

		/* An env line is a variable a daemon will export. */
		const char *envs = "id = d\nkind = data\nversion = 1\n"
				   "env = PROJ_DATA=/x\nenv = BAD NAME=1\n"
				   "env = =nope\n";
		kpk_meta_parse(envs, strlen(envs), &m);
		eq_int(m.nenv, 1, "only a well-formed env line is kept");
		eq_str(m.env[0], "PROJ_DATA=/x", "and it is the right one");
	}

	/* ── what kpk_meta_valid refuses ────────────────────────────── */
	{
		KpkMeta m;
		char err[256];

		kpk_meta_parse("id = ../etc\nkind = app\nversion = 1\n",
			       strlen("id = ../etc\nkind = app\nversion = 1\n"), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "an id that is a path is refused");

		kpk_meta_parse("id = app.x\nkind = app\nversion = 1\n",
			       strlen("id = app.x\nkind = app\nversion = 1\n"), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) == 0, "a plain app is valid");

		/* A data pack is mounted noexec, so a command in one names
		 * something that can never run. */
		const char *dcmd = "id = d.x\nkind = data\nversion = 1\n"
				   "command = thing\n";
		kpk_meta_parse(dcmd, strlen(dcmd), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "a data pack carrying a command is refused");

		const char *dgr = "id = d.y\nkind = data\nversion = 1\n"
				  "graft = share ../../etc\n";
		kpk_meta_parse(dgr, strlen(dgr), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "a graft that escapes its root is refused");
	}

	/* ── the solve ──────────────────────────────────────────────── */
	{
		KpkMeta store[5];
		const KpkMeta *av[5];
		const char *src[5] = {
			"id = base\nkind = base\nversion = 1\n",
			"id = rt-gtk\nkind = runtime\nversion = 2\nrequires = base\n",
			"id = app.gimp\nkind = app\nversion = 3\n"
				"requires = rt-gtk >= 1\nrequires = base\n",
			"id = app.old\nkind = app\nversion = 1\n"
				"requires = rt-gtk >= 9\n",
			"id = rt-alias\nkind = runtime\nversion = 1\n"
				"provides = toolkit\n",
		};
		for (int i = 0; i < 5; i++) {
			kpk_meta_parse(src[i], strlen(src[i]), &store[i]);
			av[i] = &store[i];
		}

		int order[8];
		char err[256];
		const char *want1[] = { "app.gimp" };
		int n = kpk_solve(av, 5, want1, 1, order, 8, err, sizeof(err));
		eq_int(n, 3, "the solve pulls in both runtimes");
		eq_str(av[order[0]]->id, "base", "base is mounted first");
		eq_str(av[order[2]]->id, "app.gimp", "and the app last");

		const char *want2[] = { "app.old" };
		eq_int(kpk_solve(av, 5, want2, 1, order, 8, err, sizeof(err)), -1,
		       "a requirement nothing satisfies is refused");
		ok(strstr(err, "rt-gtk") != NULL, "and the message names it");

		/* `provides` satisfies a requirement; an INSTALL REQUEST does
		 * not resolve through it, or `install gimp` could quietly
		 * install whatever claimed the name. */
		KpkReq r = { .name = "toolkit" };
		ok(kpk_req_met(&r, av[4]), "a provides name satisfies a requirement");
		const char *want3[] = { "toolkit" };
		eq_int(kpk_solve(av, 5, want3, 1, order, 8, err, sizeof(err)), -1,
		       "but a request names an id, never a provides");
	}

	/* ── the index ──────────────────────────────────────────────── */
	{
		KpkIndex ix = {0};
		char *ipath = kb_path_join(dir, "PACKAGES");

		ix.n = 2;
		kb_strlcpy(ix.ent[0].id, "rt-gtk", KPK_ID_MAX);
		kb_strlcpy(ix.ent[0].version, "2", 64);
		kb_strlcpy(ix.ent[0].release, "1", 16);
		kb_strlcpy(ix.ent[0].file, "rt-gtk.kpack", KPK_PATH);
		memset(ix.ent[0].sha256, 'a', 64);
		ix.ent[0].size = 410;
		kb_strlcpy(ix.ent[1].id, "app.gimp", KPK_ID_MAX);
		kb_strlcpy(ix.ent[1].version, "3.0.4", 64);
		kb_strlcpy(ix.ent[1].release, "1", 16);
		kb_strlcpy(ix.ent[1].file, "app.gimp.kpack", KPK_PATH);
		memset(ix.ent[1].sha256, 'b', 64);
		ix.ent[1].size = 96;

		ix.ent[1].recommended = 1;
		ok(kpk_index_write(&ix, ipath) == 0, "an index writes");
		KpkIndex back = {0};
		eq_int(kpk_index_load(&back, ipath), 2, "and reads back");
		eq_str(back.ent[0].id, "app.gimp",
		       "sorted by id, so the same set is always the same bytes");
		ok(kpk_index_find(&back, "rt-gtk") != NULL, "lookup by id");
		/* `R:` is in the index and not only in the pack, so kinstall —
		 * which links libkbase and libktui and nothing else — can
		 * answer "what does KDOS suggest" from a flat file. */
		ok(back.ent[0].recommended && !back.ent[1].recommended,
		   "the recommended flag round-trips through the index");
		ok(kpk_index_find(&back, "nope") == NULL, "and a miss is a miss");

		/* A version carrying a dash: the release is after the LAST one. */
		kb_write_file(ipath, "P:x\nV:1.2-rc1-4\nC:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\nF:x.kpack\n\n");
		kpk_index_load(&back, ipath);
		eq_str(back.ent[0].version, "1.2-rc1", "version keeps its dash");
		eq_str(back.ent[0].release, "4", "the release is the last field");

		/* A stanza with no hash cannot be verified, so it is dropped
		 * rather than recorded as an entry nothing can check. */
		kb_write_file(ipath, "P:x\nV:1-1\nF:x.kpack\n\nP:y\nV:1-1\n"
				     "C:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\nF:y.kpack\n\n");
		eq_int(kpk_index_load(&back, ipath), 1,
		       "a stanza with no hash is dropped, not half-recorded");
		eq_str(back.ent[0].id, "y", "and it is the one that had one");
		free(ipath);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

int main(void)
{
	kb_set_progname("selftest");

	test_base();
	test_trash();
	test_colour();
	test_pkg();
	test_build();
	test_proc();
	test_chart();
	test_grid();
	test_pack();
	test_portup();

	printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
