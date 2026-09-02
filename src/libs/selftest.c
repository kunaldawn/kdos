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
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kwm.h"
#include "kvt.h"
#include "kcon.h"
#ifdef HAVE_KIMG
#include "kimg.h"
#endif
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
	 * A STATE FILE IS REPLACED, NEVER TRUNCATED. `kb_write_file` opens
	 * O_TRUNC and then writes, so the file is zero bytes in between and any
	 * failure there leaves it that way — measured as a box profile found
	 * empty, which loses `base` and makes the box unstartable. The atomic
	 * writer must land the whole content, keep 0644, shrink cleanly when
	 * the new content is shorter, and leave no temp file behind.
	 */
	{
		struct stat st;
		char rd[64];
		snprintf(p, sizeof(p), "%s/state.conf", work);
		ok(kb_write_file_atomic(p, "base=pack:kdos\nimage=x\n") == 0,
		   "atomic write lands");
		ok(stat(p, &st) == 0 && st.st_size == 23,
		   "the whole content is there");
		ok((st.st_mode & 0777) == 0644, "and it is 0644");
		ok(kb_write_file_atomic(p, "x\n") == 0, "replace with less");
		ok(stat(p, &st) == 0 && st.st_size == 2,
		   "no tail of the old content survives");
		ok(kb_read_file(p, rd, sizeof(rd)) == 2, "and it reads back");
		{
			/* the temp file is named <path>.tmpXXXXXX */
			char g[512];
			snprintf(g, sizeof(g), "%s.tmp", p);
			ok(!kb_path_exists(g), "no temp file left behind");
		}
		unlink(p);
	}

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

		/*
		 * THREE WAYS TO FAIL A SIGNATURE AND THEY ARE DIFFERENT
		 * QUESTIONS. An EMPTY ring is a fact about this machine — it
		 * holds no key that could check anything — and reporting it as
		 * a bad signature sends the reader to inspect the artefact
		 * instead of the drawer. A ring holding SOMEBODY ELSE'S key is
		 * a fact about the pack: a block that no trusted key verifies.
		 * Both are refused; only one of them is about the pack.
		 */
		KsigRing empty = {0};
		eq_int(kpk_verify(&p, &empty, who), KPK_SIG_NOKEY,
		       "an empty ring is 'no key here', not a bad signature");

		uint8_t oseed[KSIG_SEED_LEN], opub[KSIG_PUB_LEN];
		if (ksig_keygen(oseed, opub) == 0) {
			char *odir = kb_path_join(dir, "otherkeys");
			kb_mkdir_p(odir);
			char *of = kb_path_join(odir, "other.pub");
			ksig_write_public(of, opub, "stranger");
			KsigRing other = {0};
			ksig_ring_load(&other, odir);
			eq_int(kpk_verify(&p, &other, who), KPK_SIG_BAD,
			       "a signature by a key nobody trusts is refused");
			free(of);
			free(odir);
		}

		/* One byte of the filesystem, flipped. The hash is checked
		 * BEFORE the signature, so this is KPK_SIG_HASH — a caller
		 * that saw KPK_SIG_BAD here would go looking for the wrong
		 * problem. */
		poke(pack, 4, 'X');
		ok(kpk_open(pack, &p) == 0, "a tampered pack still opens");
		eq_int(kpk_verify(&p, &ring, who), KPK_SIG_HASH,
		       "one flipped payload byte fails the hash, not the signature");

		/*
		 * A SUBDIRECTORY IS A REAL BOUNDARY BETWEEN KEYRINGS, and two
		 * of them depend on it: /etc/kdos/keys is kpkg's binhost ring
		 * and /etc/kdos/keys/packs is kdos-packd's. A key that attests
		 * "these packs came off this medium" must not thereby become a
		 * trusted publisher of HOST packages, and the only thing
		 * keeping those apart is that the loader does not descend.
		 */
		char *subdir = kb_path_join(keydir, "packs");
		kb_mkdir_p(subdir);
		char *sub = kb_path_join(subdir, "scoped.pub");
		ksig_write_public(sub, pub, "scoped");
		KsigRing outer = {0}, inner = {0};
		char *bare = kb_path_join(dir, "keys2");
		kb_mkdir_p(bare);
		char *bsub = kb_path_join(bare, "packs");
		kb_mkdir_p(bsub);
		char *bkey = kb_path_join(bsub, "scoped.pub");
		ksig_write_public(bkey, pub, "scoped");
		eq_int(ksig_ring_load(&outer, bare), 0,
		       "a keyring does not descend into a subdirectory");
		eq_int(ksig_ring_load(&inner, bsub), 1,
		       "and the subdirectory is a ring of its own");
		free(bkey); free(bsub); free(bare); free(sub); free(subdir);
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

		/* env is NOT a data-pack key: which QT_QPA_PLATFORMTHEME works
		 * is a fact about the runtime that installed the platform
		 * theme, so the runtime is what declares it. graft and
		 * boxgraft stay data-only. */
		const char *renv = "id = rt-kde\nkind = runtime\nversion = 1\n"
				   "env = QT_QPA_PLATFORMTHEME=kde\n";
		kpk_meta_parse(renv, strlen(renv), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) == 0,
		   "a runtime may declare env");
		eq_str(m.env[0], "QT_QPA_PLATFORMTHEME=kde",
		       "and it survives the parse");

		const char *rgr = "id = rt-x\nkind = runtime\nversion = 1\n"
				  "graft = share here\n";
		kpk_meta_parse(rgr, strlen(rgr), &m);
		ok(kpk_meta_valid(&m, err, sizeof(err)) != 0,
		   "but a runtime may not graft");
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

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * libkwm, against testing/fixtures/wm/geometry.txt.
 *
 * THE FIXTURE IS THE CONTRACT AND THIS ONLY REPLAYS IT. Every row there was
 * derived from a named line of kdos-comp, so a failure here means the library
 * and the compositor have parted company — which is the one thing sharing a
 * window model between two desktops exists to prevent. Adding a case means
 * adding a row and citing its line, never writing an assertion here.
 */

static unsigned wm_edge(const char *s)
{
	unsigned e = 0;

	if (!strcmp(s, "N"))
		return KWM_EDGE_NONE;
	if (!strcmp(s, "C"))
		return KWM_EDGE_CENTER;

	for (; *s; s++)
		switch (*s) {
		case 'T':
			e |= KWM_EDGE_TOP;
			break;
		case 'B':
			e |= KWM_EDGE_BOTTOM;
			break;
		case 'L':
			e |= KWM_EDGE_LEFT;
			break;
		case 'R':
			e |= KWM_EDGE_RIGHT;
			break;
		}

	return e;
}

/* UNB is either bound: which one it means is decided by the comparison, not
 * by the token, so both map to INT_MAX and no row relies on the difference. */
static int wm_num(const char *s)
{
	if (!strcmp(s, "UNB") || !strcmp(s, "INT_MAX"))
		return INT_MAX;
	if (!strcmp(s, "INT_MIN"))
		return INT_MIN;
	if (!strcmp(s, "INT_MAX-1"))
		return INT_MAX - 1;
	return atoi(s);
}

/*
 * The gap rule, asserted against what the search hands its validator rather
 * than against a rectangle it returns — the rule IS which of the two candidate
 * edges carries the padding.
 *
 * The moving box arrives ALREADY PADDED by the gap (kdos-comp builds it that
 * way in src/edges.c:28-31), so an unpadded opposing edge leaves that padding
 * standing as the space between two windows placed beside each other, and an
 * aligned edge padded outward by the same amount cancels it so the two line up
 * exactly. Swap the two pads and both rows fail.
 */
typedef struct {
	int want_cur;		/* the mover edge offset that picks one call */
	int seen;
	int oppose;
	int align;
} WmGapProbe;

static void wm_gap_probe(int *best, KwmEdge cur, KwmEdge tgt, KwmEdge oppose,
			 KwmEdge align, int lesser, void *user)
{
	WmGapProbe *p = user;

	(void)best;
	(void)tgt;
	(void)lesser;

	if (cur.offset != p->want_cur)
		return;

	p->seen++;
	p->oppose = oppose.offset;
	p->align = align.offset;
}

static void test_wm(void)
{
	printf("\n==> libkwm replays the window-model contract\n");

	const char *path = "testing/fixtures/wm/geometry.txt";
	FILE *f = fopen(path, "r");
	if (!f) {
		ok(0, "testing/fixtures/wm/geometry.txt is readable");
		return;
	}

	char line[512];
	int rows = 0;
	/*
	 * Rows the replay deliberately does not drive, and rows it did not
	 * recognise at all. The second must stay zero: a kind with no branch
	 * used to leave the file silently, which is a contract row nothing
	 * checks wearing the appearance of one that passed.
	 */
	int stated = 0, unknown = 0;

	while (fgets(line, sizeof(line), f)) {
		char a[64], b[64], c[64];
		int i2, i3, i4, want;

		if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
			continue;

		/*
		 * A row whose answer is "@unchanged" states what the CALLER
		 * does: kwm_place is never reached when the output is unusable,
		 * so there is no library call to make. It is in the contract
		 * because the contract is the caller's too, and it is counted
		 * here so the file and the replay reconcile.
		 */
		if (strstr(line, "@unchanged")) {
			stated++;
			continue;
		}

		if (!strncmp(line, "tile ", 5)) {
			int combine, across, mv = 0;
			char mark[64];

			mark[0] = '\0';
			if (sscanf(line, "tile %63s %63s %d %d -> %63s %63s",
				   a, b, &combine, &across, c, mark) < 5)
				continue;

			unsigned got = kwm_tile_next(wm_edge(a), wm_edge(b),
						     combine, across, &mv);
			rows++;

			/*
			 * The no-adjacent-output row asserts what the CALLER
			 * must do, not what this returns: the library says
			 * "move outputs" and the caller leaves the view alone
			 * when there is nowhere to move it, which is why the
			 * expected state on that row is the one it started in.
			 */
			if (!strcmp(mark, "@no-adjacent-output-unchanged")) {
				ok(mv != 0, line);
			} else {
				if (!strcmp(mark, "@adjacent"))
					ok(mv != 0, line);
				eq_int(got, wm_edge(c), line);
			}
		} else if (!strncmp(line, "geom ", 5)) {
			KwmRect u;
			KwmBorder m;
			int gap, ml, mt, mr, mb, rx, ry, rw, rh;

			if (sscanf(line,
				   "geom %d,%d,%d,%d %d %d,%d,%d,%d %63s"
				   " -> %d,%d,%d,%d",
				   &u.x, &u.y, &u.w, &u.h, &gap,
				   &ml, &mt, &mr, &mb, a,
				   &rx, &ry, &rw, &rh) != 14)
				continue;

			/* The fixture writes a margin left,top,right,bottom;
			 * KwmBorder is top,right,bottom,left, as struct border
			 * is. Assigning by name is what keeps that straight. */
			m.left = ml;
			m.top = mt;
			m.right = mr;
			m.bottom = mb;

			KwmRect g = kwm_tile_geom(u, gap, m, wm_edge(a));

			rows++;
			ok(g.x == rx && g.y == ry && g.w == rw && g.h == rh,
			   line);
		} else if (!strncmp(line, "place ", 6)) {
			KwmRect u;
			KwmBorder m;
			KwmBox ex[8];
			int gap, ml, mt, mr, mb, ww, hh;
			int nex = 0;
			char rest[256], wnt[64];

			if (sscanf(line,
				   "place %d,%d,%d,%d %d %d,%d,%d,%d %dx%d"
				   " %255[^\n]",
				   &u.x, &u.y, &u.w, &u.h, &gap,
				   &ml, &mt, &mr, &mb, &ww, &hh, rest) != 12)
				continue;

			m.left = ml; m.top = mt; m.right = mr; m.bottom = mb;

			/* The rows list existing windows as x,y,w,h; kwm_place
			 * takes absolute edges, already inflated. */
			char *p = rest;
			while (nex < 8) {
				int x, y, w, h, n = 0;

				if (sscanf(p, " %d,%d,%d,%d%n",
					   &x, &y, &w, &h, &n) != 4)
					break;
				ex[nex].left = x;
				ex[nex].top = y;
				ex[nex].right = x + w;
				ex[nex].bottom = y + h;
				nex++;
				p += n;
			}

			if (sscanf(p, " -> %63s", wnt) != 1)
				continue;

			KwmRect g = kwm_place(u, gap, m, ww, hh, ex, nex);
			int wx, wy;

			rows++;
			if (sscanf(wnt, "%d,%d", &wx, &wy) == 2)
				ok(g.x == wx && g.y == wy, line);
			else if (!strncmp(wnt, "@not-", 5) &&
				 sscanf(wnt + 5, "%d,%d", &wx, &wy) == 2)
				ok(g.x != wx || g.y != wy, line);
			else
				ok(0, line);
		} else if (!strncmp(line, "fit ", 4)) {
			KwmRect work, wnt;
			int rx, ry, rw, rh;

			if (sscanf(line,
				   "fit %d,%d,%d,%d %d,%d,%d,%d"
				   " -> %d,%d,%d,%d",
				   &work.x, &work.y, &work.w, &work.h,
				   &wnt.x, &wnt.y, &wnt.w, &wnt.h,
				   &rx, &ry, &rw, &rh) != 12)
				continue;

			KwmRect g = kwm_fit(wnt, work);

			rows++;
			ok(g.x == rx && g.y == ry && g.w == rw && g.h == rh,
			   line);
		} else if (!strncmp(line, "drag ", 5)) {
			int dx, dy;
			char w[16];

			if (sscanf(line, "drag %d %d -> %15s", &dx, &dy, w) != 3)
				continue;

			rows++;
			eq_int(kwm_drag_threshold(dx, dy), !strcmp(w, "yes"),
			       line);
		} else if (!strncmp(line, "clip ", 5)) {
			if (sscanf(line, "clip %63s %63s -> %63s", a, b, c) != 3)
				continue;

			rows++;
			eq_int(kwm_clip_add(wm_num(a), wm_num(b)), wm_num(c),
			       line);
		} else if (!strncmp(line, "best ", 5)) {
			if (sscanf(line, "best %63s %63s %d -> %63s",
				   a, b, &i2, c) != 4)
				continue;

			rows++;
			eq_int(kwm_edge_best(wm_num(a), wm_num(b), i2),
			       wm_num(c), line);
		} else if (!strncmp(line, "btwn ", 5)) {
			if (sscanf(line, "btwn %d %d %d -> %63s",
				   &i2, &i3, &i4, c) != 4)
				continue;

			rows++;
			eq_int(!!kwm_edge_between(i2, i3, i4),
			       !strcmp(c, "yes"), line);
		} else if (!strncmp(line, "gaprule ", 8)) {
			/* top, right, bottom, left */
			const KwmBox mover = { 50, 1000, 900, 10 };
			const KwmRegion reg = {
				{ 100, 400, 300, 200 },
				KWM_EDGE_TOP | KWM_EDGE_BOTTOM |
				KWM_EDGE_LEFT | KWM_EDGE_RIGHT
			};
			const int gap = 7;
			WmGapProbe probe = { mover.left, 0, 0, 0 };
			KwmBox best;

			if (sscanf(line, "gaprule %63s -> %63s", a, b) != 2)
				continue;

			kwm_edge_init(&best);
			kwm_edge_regions(&best, mover, mover, &reg, 1, gap,
					 wm_gap_probe, &probe);

			/*
			 * What the mover's REAL edge keeps from the region edge
			 * it met, once its own padding is added back.
			 */
			int landed = !strcmp(a, "oppose") ? probe.oppose
							  : probe.align;
			int refer = !strcmp(a, "oppose") ? reg.box.right
							 : reg.box.left;

			rows++;
			ok(probe.seen == 1, "the gap rule's left-edge call ran");
			eq_int((landed + gap) - refer,
			       !strcmp(b, "gap") ? gap : 0, line);
		} else if (!strncmp(line, "ring ", 5)) {
			if (sscanf(line, "ring %d %d %d -> %d",
				   &i2, &i3, &i4, &want) != 4)
				continue;

			rows++;
			eq_int(kwm_ring_next(i2, i3, i4), want, line);
		} else if (!strncmp(line, "wsadj ", 6)) {
			unsigned char bits[64];

			if (sscanf(line, "wsadj %63s %d %d %d -> %d",
				   a, &i2, &i3, &i4, &want) != 5)
				continue;

			int n = (int)strlen(a);
			if (n > (int)sizeof(bits))
				continue;
			for (int k = 0; k < n; k++)
				bits[k] = a[k] == '1';

			rows++;
			eq_int(kwm_ws_adjacent(bits, n, i2, i3, i4), want, line);
		} else {
			unknown++;
			printf("  UNKNOWN ROW  %s", line);
		}
	}

	fclose(f);

	/*
	 * A fixture that stopped being found, or that lost its rows to an
	 * editing accident, must fail rather than report a clean run over
	 * nothing. The floor is well under the current count on purpose: this
	 * asserts the file was read, not how many rows anyone has added.
	 */
	ok(rows >= 80, "the contract file was read and had rows in it");
	ok(unknown == 0, "every row in the contract has a branch that drives it");
	printf("  %d rows replayed, %d stated for the caller\n", rows, stated);
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The gesture recogniser.
 *
 * DRIVEN, NOT SLEPT THROUGH. Every timestamp here is supplied on the event,
 * which is the whole reason the recogniser reads no clock: a long press is
 * asserted in microseconds instead of half a second, and the suite stays a
 * pure function of its input.
 *
 * Both halves are checked on every scenario — the gesture, and the ordinary
 * mouse event synthesised beneath it — because a widget that never heard of
 * touch is supposed to keep working, and that only holds if the synthesis does.
 */

static KtuiEvent touch_ev(int phase, int slot, int x, int y, unsigned ms)
{
	KtuiEvent ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = KT_EVT_TOUCH;
	ev.phase = phase;
	ev.slot = slot;
	ev.mx = x;
	ev.my = y;
	ev.ms = ms;
	return ev;
}

static void test_gesture(void)
{
	printf("\n==> touch becomes a gesture, and a mouse\n");

	KtuiEvent ev, m;
	KtuiGesture g;
	int have = 0;

	/* start_edge() reads the grid, so the scenarios below need one. */
	ktui_w = 80;
	ktui_h = 24;

	/* ── a tap ─────────────────────────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 1000);
	ok(!ktui_gesture_feed(&ev, &g, &m, &have),
	   "a finger down is not yet a gesture");
	ok(have && m.type == KT_EVT_MOUSE && m.btn == KT_MB_LEFT
	   && m.press == KT_MP_PRESS,
	   "a finger down synthesises a left press");
	eq_int(m.mx, 10, "the synthesised press is where the finger is");

	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, 1100);
	ok(ktui_gesture_feed(&ev, &g, &m, &have),
	   "a short press in one cell reports");
	eq_int(g.type, KT_GEST_TAP, "and it is a tap");
	eq_int(g.fingers, 1, "a tap is one finger");
	eq_int(g.x, 10, "the tap is at the cell it went down in");
	ok(have && m.press == KT_MP_RELEASE,
	   "and a release is synthesised under it");

	/* ── held too long is not a tap ────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, KT_TAP_MS);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a press held past the tap window is not a tap");

	/* ── a move inside the cell says nothing ───────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 10, 5, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a move that stays in the cell is not a gesture");

	ev = touch_ev(KT_TOUCH_MOVE, 0, 12, 5, 20);
	ok(ktui_gesture_feed(&ev, &g, &m, &have), "leaving the cell reports");
	eq_int(g.type, KT_GEST_DRAG, "and it is a drag");
	eq_int(g.dx, 2, "the drag carries the cells moved");
	ok(have && m.press == KT_MP_DRAG,
	   "a drag synthesises a dragging pointer");

	/* A drag disqualifies the tap that would otherwise follow. */
	ev = touch_ev(KT_TOUCH_UP, 0, 12, 5, 30);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a finger that moved does not tap when it lifts");

	/* ── an edge swipe is a drag that started against an edge ──────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 0, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 4, 5, 10);
	ok(ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a drag from the edge reports");
	eq_int(g.type, KT_GEST_SWIPE_EDGE, "and it is an edge swipe");
	eq_int(g.edge, KT_K_LEFT, "naming the edge it came from");

	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, ktui_w - 1, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, ktui_w - 5, 5, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	eq_int(g.edge, KT_K_RIGHT, "the far edge is the right one");

	/* ── a long press has no event of its own ──────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ok(!ktui_gesture_tick(KT_LONG_MS - 1, &g),
	   "a press short of the long window is not a long press");
	ok(ktui_gesture_tick(KT_LONG_MS, &g), "held past it, it is");
	eq_int(g.type, KT_GEST_LONG, "and it says so");
	ok(!ktui_gesture_tick(KT_LONG_MS + 100, &g),
	   "a long press is reported once, not once per tick");
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 5, KT_LONG_MS + 200);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "and no tap follows the long press");

	/* A finger that has moved never becomes a long press. */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 14, 5, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ok(!ktui_gesture_tick(KT_LONG_MS, &g),
	   "a finger that moved does not become a long press");

	/* ── two fingers: nothing is decided by one of them ────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);

	ev = touch_ev(KT_TOUCH_MOVE, 0, 10, 14, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "one finger moving while the other has not is undecided");
	ev = touch_ev(KT_TOUCH_MOVE, 1, 20, 14, 20);
	ok(ktui_gesture_feed(&ev, &g, &m, &have),
	   "the second finger decides it");
	eq_int(g.type, KT_GEST_SCROLL, "two fingers the same way is a scroll");
	eq_int(g.fingers, 2, "and it says how many");
	ok(have && m.btn == KT_MB_WHEEL_DOWN,
	   "a downward scroll synthesises a wheel");

	/* ── two fingers apart is a pinch ──────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_MOVE, 0, 6, 10, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL), "still undecided");
	ev = touch_ev(KT_TOUCH_MOVE, 1, 24, 10, 20);
	ok(ktui_gesture_feed(&ev, &g, NULL, NULL), "the pair disagreed");
	eq_int(g.type, KT_GEST_PINCH, "and that is a pinch");
	ok(g.dx > 0, "spreading reports a growing span");

	/* ── a second finger disqualifies the tap ──────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_DOWN, 1, 20, 10, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 1, 20, 10, 10);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_UP, 0, 10, 10, 20);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a sequence that had two fingers in it never taps");

	/* ── cancel is not up ──────────────────────────────────────────── */
	ktui_gesture_reset();
	ev = touch_ev(KT_TOUCH_DOWN, 0, 10, 5, 0);
	ktui_gesture_feed(&ev, &g, NULL, NULL);
	ev = touch_ev(KT_TOUCH_CANCEL, 0, 10, 5, 10);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "a cancelled sequence reports nothing");
	ok(!ktui_gesture_tick(KT_LONG_MS, &g),
	   "and leaves nothing half-recognised behind it");

	/* ── a caller that wants only gestures passes NULL ─────────────── */
	ktui_gesture_reset();
	have = 1;
	ev = touch_ev(KT_TOUCH_DOWN, 0, 3, 3, 0);
	ok(!ktui_gesture_feed(&ev, &g, NULL, NULL),
	   "NULL for the mouse pair is allowed");

	/* ── anything that is not a touch event is not ours ────────────── */
	memset(&ev, 0, sizeof(ev));
	ev.type = KT_EVT_KEY;
	have = 1;
	ok(!ktui_gesture_feed(&ev, &g, &m, &have),
	   "a key event is not a gesture");
	eq_int(have, 0, "and the mouse flag is cleared before anything else");

	ktui_gesture_reset();
	ktui_w = 0;
	ktui_h = 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The sprite table: its cap, its byte budget, its opt-in eviction, and the
 * grid a picture larger than one slot becomes.
 *
 * EVERY PICTURE HERE IS A FAKE POINTER. The table holds an opaque `const void
 * *` and never dereferences it — that is the property that keeps libktui
 * linking nothing but the C library — so a distinct address per picture is a
 * complete test subject, and the suite needs no pixel library to run this.
 */

static uint64_t spr_freed[64];
static const void *spr_freed_pix[64];
static int spr_nfreed;

static void spr_evictor(uint64_t key, const void *pix, void *user)
{
	(void)user;
	if (spr_nfreed < (int)(sizeof(spr_freed) / sizeof(spr_freed[0]))) {
		spr_freed_pix[spr_nfreed] = pix;
		spr_freed[spr_nfreed++] = key;
	}
}

/* The tile callback hands back a distinct address per tile, and records the
 * cell rectangle it was asked for. */
static int spr_tw[64], spr_th[64], spr_tx[64], spr_ty[64];
static int spr_ntiles;
static int spr_fail_at = -1;
static char spr_pixels[64];

static const void *spr_tile(void *user, int cell_x, int cell_y, int cw, int ch)
{
	(void)user;
	if (spr_ntiles >= 64)
		return NULL;
	if (spr_fail_at >= 0 && spr_ntiles == spr_fail_at) {
		spr_ntiles++;
		return NULL;
	}
	spr_tx[spr_ntiles] = cell_x;
	spr_ty[spr_ntiles] = cell_y;
	spr_tw[spr_ntiles] = cw;
	spr_th[spr_ntiles] = ch;
	return &spr_pixels[spr_ntiles++];
}

/*
 * A BACKEND THAT PRESENTS, and the block needs one.
 *
 * The eviction rule is "is anything still drawing this", and ktui_cells()
 * answers it with the PRESENTED frame — `front`, which a backend's flush
 * updates as it presents. Offscreen mode deliberately makes ktui_draw_flush()
 * a no-op, so `front` there is always empty and nothing ever looks on-screen.
 * Presenting into it is the backend's documented job, so the test does the
 * smallest possible one rather than reaching around the seam.
 */
static void spr_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full)
{
	(void)force_full;
	memcpy(prev, cur, (size_t)w * h * sizeof(*prev));
}

static void spr_size(int *w, int *h)
{
	if (w)
		*w = 80;
	if (h)
		*h = 24;
}

static int spr_caps(void) { return 0; }

static int spr_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static const KtuiBackend spr_backend = {
	"selftest", spr_flush, spr_poll, spr_size, spr_caps
};

static void spr_reset(void)
{
	ktui_sprite_clear();
	ktui_sprite_evictor(NULL, NULL);
	ktui_sprite_budget(0, 0, 0);
	spr_nfreed = 0;
	spr_ntiles = 0;
	spr_fail_at = -1;
}

static void test_sprite(void)
{
	printf("\n==> the sprite table: the cap, the budget and the grid\n");

	char a_pix, b_pix, c_pix;

	const KtuiBackend *was = ktui_backend();

	ktui_backend_set(&spr_backend);
	ktui_w = 80;
	ktui_h = 24;
	ktui_draw_resize();
	ktui_draw_clip_none();
	ktui_draw_clear();
	ktui_draw_flush();
	spr_reset();

	/* ── the cap, and where the number comes from ──────────────────── */
	eq_int(KTUI_MAX_SPRITES, 4096, "the slot table holds four thousand");
	ok(((240 + 15) / 16) * ((67 + 15) / 16) <= KTUI_MAX_SPRITES,
	   "a full screen tiled edge to edge fits inside it");

	/* ── the round trip ────────────────────────────────────────────── */
	int sa = ktui_sprite_put(1, &a_pix, 2, 2, 'A');
	ok(sa >= 0, "a picture registers");
	eq_int(ktui_sprite_find(1), sa, "and is found again by its key");
	const KtuiSprite *got = ktui_sprite_get(sa);
	ok(got && got->pix == &a_pix, "the slot holds the pointer it was given");
	eq_int(got ? got->fallback : 0, 'A', "and the fallback beside it");

	eq_int(ktui_sprite_find(999), -1, "an unknown key is not found");
	eq_int(ktui_sprite_put(2, NULL, 2, 2, 'x'), -1,
	       "a null picture is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 17, 2, 'x'), -1,
	       "wider than a slot is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 2, 17, 'x'), -1,
	       "taller than a slot is refused");
	eq_int(ktui_sprite_put(2, &b_pix, 0, 2, 'x'), -1,
	       "a zero dimension is refused");

	/* ── re-registering ────────────────────────────────────────────── */
	ktui_sprite_evictor(spr_evictor, NULL);
	spr_nfreed = 0;
	eq_int(ktui_sprite_put(1, &a_pix, 2, 2, 'A'), sa,
	       "the same picture under the same key keeps its slot");
	eq_int(spr_nfreed, 0, "and hands nothing back");

	eq_int(ktui_sprite_put(1, &b_pix, 2, 2, 'A'), sa,
	       "a different picture under the same key reuses the slot");
	eq_int(spr_nfreed, 1, "and hands the old one back");
	ok(spr_nfreed == 1 && spr_freed_pix[0] == &a_pix,
	   "handing back exactly the picture it replaced");

	/* ── dropping is the owner's, so it calls nobody ───────────────── */
	spr_nfreed = 0;
	ktui_sprite_drop(1);
	eq_int(ktui_sprite_find(1), -1, "a dropped key is gone");
	eq_int(spr_nfreed, 0,
	       "dropping does not call the evictor: the owner is already there");

	/* ── clearing hands everything back ────────────────────────────── */
	spr_reset();
	ktui_sprite_evictor(spr_evictor, NULL);
	ktui_sprite_put(10, &a_pix, 1, 1, 'a');
	ktui_sprite_put(11, &b_pix, 1, 1, 'b');
	spr_nfreed = 0;
	ktui_sprite_clear();
	eq_int(spr_nfreed, 2, "clearing hands back every picture it held");
	eq_int(ktui_sprite_slots(), 0, "and empties the table");

	/* ── the byte budget ───────────────────────────────────────────── */
	spr_reset();
	ktui_sprite_put(20, &a_pix, 2, 2, 'a');
	eq_int((long long)ktui_sprite_bytes(), 0,
	       "with no cell size declared the byte budget is off");

	spr_reset();
	ktui_sprite_budget(1024 * 1024, 8, 16);
	ktui_sprite_put(20, &a_pix, 2, 2, 'a');
	eq_int((long long)ktui_sprite_bytes(), (long long)(2 * 8 * 2 * 16 * 4),
	       "a sprite costs its cells times the cell size, in ARGB");

	/* A cap too small for the next picture, and nobody to hand one back
	 * to: refused, and the total is left where it was. */
	spr_reset();
	ktui_sprite_budget(2 * 8 * 2 * 16 * 4, 8, 16);
	ok(ktui_sprite_put(20, &a_pix, 2, 2, 'a') >= 0,
	   "the first picture fits the cap exactly");
	size_t before = ktui_sprite_bytes();
	eq_int(ktui_sprite_put(21, &b_pix, 2, 2, 'b'), -1,
	       "the second is refused with no evictor to make room");
	eq_int((long long)ktui_sprite_bytes(), (long long)before,
	       "and the refusal left the total alone");

	/* With an evictor, room is made from the least recently used. */
	spr_reset();
	ktui_sprite_budget(2 * (2 * 8 * 2 * 16 * 4), 8, 16);
	ktui_sprite_evictor(spr_evictor, NULL);
	ktui_sprite_put(30, &a_pix, 2, 2, 'a');
	ktui_sprite_put(31, &b_pix, 2, 2, 'b');
	ktui_sprite_find(31);		/* touch 31, so 30 is the oldest */
	spr_nfreed = 0;
	ok(ktui_sprite_put(32, &c_pix, 2, 2, 'c') >= 0,
	   "a third picture is taken once there is an evictor");
	eq_int(spr_nfreed, 1, "exactly one picture was handed back");
	eq_int((long long)spr_freed[0], 30,
	       "and it is the least recently used one");
	eq_int(ktui_sprite_find(31), 1, "the one that was touched survived");

	/* ── what is on the screen is not evictable ────────────────────── */
	spr_reset();
	ktui_draw_clear();
	ktui_sprite_budget(2 * 8 * 2 * 16 * 4, 8, 16);
	ktui_sprite_evictor(spr_evictor, NULL);
	int son = ktui_sprite_put(40, &a_pix, 2, 2, 'a');
	KRect r = { 0, 0, 2, 2 };
	ktui_draw_sprite(r, son, 7, 0);
	ktui_draw_flush();		/* now the frame on screen names it */

	/*
	 * LOUD, BECAUSE THE DEPENDENCY IS INVISIBLE. ktui_offscreen_init() is a
	 * ONE-WAY LATCH — nothing clears it — and it makes ktui_draw_flush() a
	 * no-op, so a block that runs after one of the offscreen blocks would
	 * find `front` empty, every sprite would look evictable, and the two
	 * assertions below would pass for the wrong reason. This is why
	 * test_sprite() runs first in main().
	 */
	int fw = 0, fh = 0;
	const KtuiCell *frame = ktui_cells(&fw, &fh);
	ok(frame && fw > 0 && KTUI_IS_SPRITE(frame[0].ch) &&
	   (int)KTUI_SPRITE_SLOT(frame[0].ch) == son,
	   "a frame was presented, so the on-screen rule can be exercised");

	spr_nfreed = 0;
	eq_int(ktui_sprite_put(41, &b_pix, 2, 2, 'b'), -1,
	       "a sprite the cell buffer still names is not evictable");
	eq_int(spr_nfreed, 0, "so nothing was handed back");
	ktui_draw_clear();
	ktui_draw_flush();
	ok(ktui_sprite_put(41, &b_pix, 2, 2, 'b') >= 0,
	   "and once nothing draws it, it is");

	/* ── a text backend's substitute ───────────────────────────────── */
	spr_reset();
	int st = ktui_sprite_put(50, &a_pix, 2, 2, 0x2588);
	uint32_t tl = KTUI_SPRITE_BASE | ((uint32_t)st << 8);
	eq_int(ktui_sprite_text_cell(tl), 0x2588,
	       "a text backend puts the fallback in the top-left cell");
	eq_int(ktui_sprite_text_cell(tl | 1u), ' ',
	       "and blanks the rest, so two icons do not merge");
	eq_int(ktui_sprite_text_cell('Z'), 'Z',
	       "an ordinary character passes through untouched");

	/* ── a picture larger than a slot is a grid ────────────────────── */
	spr_reset();
	eq_int(ktui_sprite_put_tiled(100, 40, 30, 0x2588, spr_tile, NULL), 6,
	       "a 40x30 picture is six tiles");
	eq_int(spr_ntiles, 6, "and the callback was asked six times");

	int sx = -1, sy = -1;
	int tslot = ktui_sprite_tile_at(100, 40, 17, 3, &sx, &sy);
	ok(tslot >= 0, "cell 17,3 lands in a registered tile");
	eq_int(sx, 1, "at sub-cell x 1 of the second column");
	eq_int(sy, 3, "and sub-cell y 3");
	eq_int(tslot, ktui_sprite_tile_at(100, 40, 16, 0, NULL, NULL),
	       "which is the same tile cell 16,0 is in");

	/* The remainder tile is the remainder, not a padded full one. */
	eq_int(spr_tw[5], 8, "the last tile is the 8-cell remainder wide");
	eq_int(spr_th[5], 14, "and the 14-cell remainder tall");
	eq_int(spr_tw[0], 16, "while the first is a full slot");

	const KtuiSprite *last = ktui_sprite_get(
		ktui_sprite_tile_at(100, 40, 39, 29, NULL, NULL));
	ok(last && last->fallback == 0x2588,
	   "every tile carries the fallback, so a picture leaves a mark");

	/* ── an adjacent key must not collide ──────────────────────────── */
	int slots_before = ktui_sprite_slots();
	spr_ntiles = 0;
	eq_int(ktui_sprite_put_tiled(101, 40, 30, 0x2588, spr_tile, NULL), 6,
	       "a picture with an adjacent key registers six tiles of its own");
	eq_int(ktui_sprite_slots(), slots_before + 6,
	       "in six slots of their own — the stride is XORed, not added");

	/* ── all or nothing ────────────────────────────────────────────── */
	spr_reset();
	ktui_sprite_evictor(spr_evictor, NULL);
	spr_fail_at = 3;
	eq_int(ktui_sprite_put_tiled(200, 40, 30, 0x2588, spr_tile, NULL), -1,
	       "a tile the caller cannot supply abandons the whole picture");
	/* ktui_sprite_slots() is a HIGH-WATER MARK, not a live count: a dropped
	 * slot is emptied in place so the next put can reuse it. What "nothing
	 * is registered" means is that no tile can be found. */
	eq_int(ktui_sprite_tile_at(200, 40, 0, 0, NULL, NULL), -1,
	       "leaving no tile of it registered");
	eq_int(ktui_sprite_tile_at(200, 40, 17, 3, NULL, NULL), -1,
	       "not one of the tiles it had already taken either");
	eq_int(spr_nfreed, 3,
	       "and handing back the tiles it had already taken");

	/* ── dropping a tiled picture drops all of it ──────────────────── */
	spr_reset();
	ktui_sprite_put_tiled(300, 40, 30, 0x2588, spr_tile, NULL);
	ktui_sprite_drop_tiled(300, 40, 30);
	eq_int(ktui_sprite_tile_at(300, 40, 0, 0, NULL, NULL), -1,
	       "dropping a tiled picture drops every tile of it");

	spr_reset();
	ktui_draw_clear();
	ktui_draw_flush();
	ktui_backend_set(was);
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * libkvt's three image protocols, through the real state machine.
 *
 * The library DECODES NOTHING — it is linked by kdos-con, which links no pixel
 * code at all — so what is asserted here is the parse: which protocol, what the
 * introducer said, and exactly which bytes came out. Sixel is a DCS, iTerm2's
 * is an OSC and kitty's is an APC; one callback serves all three because they
 * differ only in how they are delimited.
 *
 * The recorded streams under testing/fixtures/vt/ are the other half of this
 * library's coverage and are replayed by testing/selftest.sh, which builds a
 * renderer of its own for them.
 */

struct vt_seen {
	int calls;
	int kind;
	char params[128];
	char payload[256];
	size_t len;
	int osc_calls;
	char osc[128];
};

static void vt_on_write(struct kvt_vte *vte, const char *u8, size_t len,
			void *data)
{
	/* The terminal's replies to the host. Nothing reads them: a test that
	 * answered would be testing the answer. */
	(void)vte; (void)u8; (void)len; (void)data;
}

static void vt_on_osc(struct kvt_vte *vte, const char *u8, size_t len,
		      void *data)
{
	struct vt_seen *s = data;

	(void)vte;
	s->osc_calls++;
	if (len >= sizeof(s->osc))
		len = sizeof(s->osc) - 1;
	memcpy(s->osc, u8, len);
	s->osc[len] = '\0';
}

static void vt_on_img(struct kvt_vte *vte, enum kvt_img_kind kind,
		      const char *params, const uint8_t *payload, size_t len,
		      void *data)
{
	struct vt_seen *s = data;

	(void)vte;
	s->calls++;
	s->kind = (int)kind;
	s->len = len;
	snprintf(s->params, sizeof(s->params), "%s", params ? params : "");
	size_t n = len < sizeof(s->payload) - 1 ? len : sizeof(s->payload) - 1;
	if (payload && n)
		memcpy(s->payload, payload, n);
	s->payload[n] = '\0';
}

/* Feed one stream through a fresh terminal. `cap` of 0 leaves the image
 * callback unset, which is the default and turns all three protocols off. */
static void vt_feed(struct vt_seen *s, const char *bytes, size_t n, size_t cap,
		    char *row0, size_t row0n)
{
	struct kvt_screen *scr;
	struct kvt_vte *vte;
	KtuiCell cells[40 * 4];

	memset(s, 0, sizeof(*s));
	if (kvt_screen_new(&scr, NULL, NULL) != 0)
		return;
	kvt_screen_resize(scr, 40, 4);
	if (kvt_vte_new(&vte, scr, vt_on_write, NULL, NULL, NULL) != 0) {
		kvt_screen_unref(scr);
		return;
	}
	kvt_vte_set_osc_cb(vte, vt_on_osc, s);
	if (cap)
		kvt_vte_set_img_cb(vte, vt_on_img, cap, s);

	/* A BYTE AT A TIME, because a pty splits a sequence across reads and a
	 * parser that only works on whole ones works only in a test. */
	for (size_t i = 0; i < n; i++)
		kvt_vte_input(vte, bytes + i, 1);

	kvt_grid_render(scr, cells, 40, 4);
	if (row0 && row0n) {
		size_t k = 0;
		for (; k < row0n - 1 && k < 40; k++)
			row0[k] = cells[k].ch >= 32 && cells[k].ch < 127
				? (char)cells[k].ch : '.';
		row0[k] = '\0';
	}
	kvt_vte_unref(vte);
	kvt_screen_unref(scr);
}

static void test_vt_img(void)
{
	printf("\n==> libkvt parses the three image protocols and decodes none\n");

	struct vt_seen s;
	char row[21];		/* twenty columns is plenty */

	/* ── sixel: a DCS ──────────────────────────────────────────────── */
	static const char sixel[] = "A\033P0;1;0q#0;2;0;0;0~-\033\\B";

	vt_feed(&s, sixel, sizeof(sixel) - 1, 4096, row, sizeof(row));
	eq_int(s.calls, 1, "a sixel is delivered once");
	eq_int(s.kind, KVT_IMG_SIXEL, "and says which protocol it was");
	/*
	 * THE INTRODUCER IS REBUILT FROM THE PARSED PARAMETERS, so a trailing
	 * zero has to survive the round trip: the parameter after the last
	 * separator needs the same bump the CSI path makes, or `0;1;0` reads
	 * back as `0;1` and the raster attributes change meaning.
	 */
	eq_str(s.params, "0;1;0", "the introducer's parameters come back whole");
	eq_str(s.payload, "#0;2;0;0;0~-", "and the body is exactly the bytes between");
	eq_int((long long)s.len, 12, "with its own length");
	eq_str(row, "AB                  ",
	       "the text on either side of it still lands");

	/* ── iTerm2: an OSC ────────────────────────────────────────────── */
	static const char it[] = "A\033]1337;File=inline=1:aGVsbG8=\033\\B";

	vt_feed(&s, it, sizeof(it) - 1, 4096, row, sizeof(row));
	eq_int(s.calls, 1, "an OSC 1337 is delivered once");
	eq_int(s.kind, KVT_IMG_OSC1337, "as the iTerm2 protocol");
	eq_str(s.params, "1337", "named by its OSC number");
	eq_int((long long)s.len, 22, "delivering exactly the bytes after it");
	eq_str(s.payload, "File=inline=1:aGVsbG8=",
	       "which are the argument list and the base64 together");
	eq_int(s.osc_calls, 0, "and it does not also reach the title handler");

	/* An ordinary OSC is untouched by any of this. */
	static const char title[] = "A\033]0;a title\033\\B";

	vt_feed(&s, title, sizeof(title) - 1, 4096, row, sizeof(row));
	eq_int(s.calls, 0, "an OSC 0 is not a picture");
	eq_int(s.osc_calls, 1, "it goes to the ordinary handler");
	eq_str(s.osc, "0;a title", "with its payload intact");

	/* ── kitty: an APC, control block split from payload ───────────── */
	static const char kitty[] = "A\033_Ga=T,f=32,s=2,v=2;AAECAwQFBgc=\033\\B";

	vt_feed(&s, kitty, sizeof(kitty) - 1, 4096, row, sizeof(row));
	eq_int(s.calls, 1, "a kitty APC is delivered once");
	eq_int(s.kind, KVT_IMG_KITTY, "as the kitty protocol");
	/*
	 * SPLIT IN THE PARSER, because the split is part of the sequence's
	 * grammar: a consumer would otherwise have to find the semicolon again
	 * inside a buffer that may be megabytes of base64.
	 */
	eq_str(s.params, "a=T,f=32,s=2,v=2", "the control block arrives on its own");
	eq_str(s.payload, "AAECAwQFBgc=", "and the payload separately");

	/* A delete or a query has no payload and is still a message. */
	static const char kdel[] = "A\033_Ga=d\033\\B";

	vt_feed(&s, kdel, sizeof(kdel) - 1, 4096, row, sizeof(row));
	eq_int(s.calls, 1, "a control block with no payload is still delivered");
	eq_str(s.params, "a=d", "carrying what it asked for");
	eq_int((long long)s.len, 0, "with an empty payload");

	/* ── off is the default ────────────────────────────────────────── */
	vt_feed(&s, sixel, sizeof(sixel) - 1, 0, row, sizeof(row));
	eq_int(s.calls, 0, "with no callback a sixel goes nowhere");
	eq_str(row, "AB                  ",
	       "and the text around it still lands");

	vt_feed(&s, kitty, sizeof(kitty) - 1, 0, row, sizeof(row));
	eq_int(s.calls, 0, "an APC is ignored to its terminator");
	eq_str(row, "AB                  ",
	       "leaving the text on either side of it alone");

	/* ── the cap drops a payload whole ─────────────────────────────── */
	vt_feed(&s, kitty, sizeof(kitty) - 1, 4, row, sizeof(row));
	eq_int(s.calls, 0,
	       "a payload past the cap is dropped entirely, not truncated");
	eq_str(row, "AB                  ",
	       "and the terminal carries on with the text");

	/*
	 * THE CAP IS CHECKED ON EVERY BYTE, not only when the buffer grows.
	 * The collector is kept between payloads so a video does not allocate
	 * a megabyte per frame — which means a later payload starts with a
	 * large allocation already in hand, and a check that only fired on
	 * growth would let it through.
	 */
	static const char big[] =
		"A\033_Ga=T;AAECAwQFBgcAAAECAwQFBgcAAAECAwQFBgcA\033\\"
		"\033_Ga=T;AA\033\\B";

	vt_feed(&s, big, sizeof(big) - 1, 8, row, sizeof(row));
	eq_int(s.calls, 1,
	       "an oversized payload is dropped and the next small one is not");
	eq_str(s.payload, "AA", "and the small one arrives whole");
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * libkcon's wire, and a message across a real socketpair.
 *
 * THE WIRE IS WRITTEN A FIELD AT A TIME, little-endian, and never as a struct:
 * a struct written whole makes its padding and its alignment into protocol, and
 * the two ends of a forwarded socket are not always the same build. Everything
 * below is that rule being checked from both directions.
 */

static void test_kcon(void)
{
	printf("\n==> libkcon's wire, and a message over a socketpair\n");

	KconBuf b = { 0 };
	KconRd r;

	/* ── scalars, in the order and the endianness declared ─────────── */
	kcon_put_u8(&b, 0x12);
	kcon_put_u16(&b, 0x3456);
	kcon_put_u32(&b, 0x89abcdefu);
	kcon_put_i32(&b, -2);
	eq_int(b.err, 0, "a run of puts sets no error");
	eq_int((long long)b.len, 1 + 2 + 4 + 4, "and writes exactly its fields");
	ok(b.b && b.b[1] == 0x56 && b.b[2] == 0x34,
	   "a u16 goes out little-endian");

	kcon_rd_init(&r, b.b, b.len);
	eq_int(kcon_get_u8(&r), 0x12, "a u8 comes back");
	eq_int(kcon_get_u16(&r), 0x3456, "a u16 comes back");
	eq_int((long long)kcon_get_u32(&r), 0x89abcdefll, "a u32 comes back");
	eq_int(kcon_get_i32(&r), -2, "and an i32 keeps its sign");
	eq_int((long long)kcon_rd_left(&r), 0, "with nothing left over");
	eq_int(r.err, 0, "and no error along the way");

	/* A read past the end is an error, not a crash and not a guess. */
	eq_int(kcon_get_u32(&r), 0, "a read past the end answers zero");
	ok(r.err != 0, "and sets the error flag");

	/*
	 * OPTIONAL TRAILING FIELDS. A peer that predates a field sends a
	 * shorter message, and kcon_rd_left is how a reader tells that from a
	 * truncated one rather than refusing both.
	 */
	kcon_buf_reset(&b);
	kcon_put_u16(&b, 7);
	kcon_rd_init(&r, b.b, b.len);
	kcon_get_u16(&r);
	eq_int((long long)kcon_rd_left(&r), 0,
	       "a message without its optional tail reads as complete");

	/* ── strings share one scratch buffer, by contract ──────────────── */
	kcon_buf_reset(&b);
	kcon_put_str(&b, "first");
	kcon_put_str(&b, "second");
	kcon_rd_init(&r, b.b, b.len);

	const char *s1 = kcon_get_str(&r);
	eq_str(s1, "first", "a string comes back");
	const char *s2 = kcon_get_str(&r);
	eq_str(s2, "second", "and so does the one after it");
	/*
	 * AND THE FIRST POINTER NOW READS AS THE SECOND. The payload's own
	 * bytes are not NUL-terminated, so the string is copied into one shared
	 * scratch buffer — a caller keeping N pointers keeps N copies of the
	 * LAST one, which is exactly how an argv reached execvp as five copies
	 * of its final argument. Stated here so nobody re-derives it from a
	 * bug report.
	 */
	eq_str(s1, "second",
	       "a string is valid only until the next get: both point at one buffer");

	/* ── a blob carries its length; bytes do not ───────────────────── */
	kcon_buf_reset(&b);
	static const unsigned char raw[4] = { 1, 2, 3, 4 };
	kcon_put_blob(&b, raw, sizeof(raw));
	eq_int((long long)b.len, 4 + (long long)sizeof(raw),
	       "a blob writes a length in front of its bytes");

	kcon_buf_reset(&b);
	kcon_put_bytes(&b, raw, sizeof(raw));
	eq_int((long long)b.len, (long long)sizeof(raw),
	       "raw bytes write no length at all");

	/*
	 * A PICTURE, BYTE FOR BYTE, out of a source whose STRIDE IS WIDER THAN
	 * ITS WIDTH — which is what a pixman image is. The size comes from the
	 * header's pw and ph and nothing else: a second length would be a
	 * second thing that can disagree with the first, and a reader computing
	 * the size from the header would then be four bytes out for every
	 * picture on the desktop.
	 */
	enum { PW = 3, PH = 2, STRIDE = 5 };
	uint32_t src[STRIDE * PH];
	uint32_t packed[PW * PH];

	for (int i = 0; i < STRIDE * PH; i++)
		src[i] = 0xdeadbeefu;			/* the padding */
	for (int y = 0; y < PH; y++)
		for (int x = 0; x < PW; x++)
			src[y * STRIDE + x] = 0x01000000u | (uint32_t)(y * PW + x);

	kcon_buf_reset(&b);
	kcon_put_u16(&b, PW);
	kcon_put_u16(&b, PH);
	for (int y = 0; y < PH; y++)
		kcon_put_bytes(&b, &src[y * STRIDE], (size_t)PW * 4);

	kcon_rd_init(&r, b.b, b.len);
	int gw = kcon_get_u16(&r), gh = kcon_get_u16(&r);
	eq_int(gw, PW, "the picture's width comes off the header");
	eq_int(gh, PH, "and its height");
	const void *pix = kcon_get_blob(&r, (size_t)gw * gh * 4);
	ok(pix != NULL, "and its pixels are exactly that many bytes");
	if (pix)
		memcpy(packed, pix, sizeof(packed));

	int same = 1;
	for (int y = 0; y < PH; y++)
		for (int x = 0; x < PW; x++)
			if (packed[y * PW + x] != src[y * STRIDE + x])
				same = 0;
	ok(same, "every pixel arrives where it started, with no stride padding");
	eq_int((long long)kcon_rd_left(&r), 0,
	       "and nothing is left over — no length crept in per row");

	/* ── a run of cells is the protocol's record, not KtuiCell's ───── */
	KtuiCell cells[3], back[3];

	memset(cells, 0, sizeof(cells));
	cells[0].ch = 'x'; cells[0].fg = 3; cells[0].bg = 1; cells[0].attr = 0;
	cells[1].ch = 0x2588; cells[1].fg = 7; cells[1].bg = 0;
	cells[2].ch = 0x4e2d; cells[2].fg = 2; cells[2].bg = 4;

	kcon_buf_reset(&b);
	kcon_put_run(&b, 4, 9, cells, 3);
	eq_int((long long)b.len, 2 + 2 + 2 + 3 * KCON_CELL_BYTES,
	       "a run is a position, a count, and packed records of a fixed size");

	uint16_t rx = 0, ry = 0;
	kcon_rd_init(&r, b.b, b.len);
	eq_int(kcon_get_run(&r, &rx, &ry, back, 3), 3, "the run reads back");
	eq_int(rx, 4, "at the x it was written at");
	eq_int(ry, 9, "and the y");
	ok(back[0].ch == 'x' && back[1].ch == 0x2588 && back[2].ch == 0x4e2d,
	   "with every codepoint intact");
	ok(back[0].fg == 3 && back[2].bg == 4,
	   "and the colours that travelled with them");

	kcon_buf_free(&b);

	/* ── a real socketpair ─────────────────────────────────────────── */
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		ok(0, "a socketpair for the connection test");
		return;
	}

	KconConn *tx = kcon_conn_new(sv[0]);
	KconConn *rxc = kcon_conn_new(sv[1]);

	ok(tx && rxc, "both ends of the socketpair become connections");

	KconMsg msg;

	eq_int(kcon_recv(rxc, &msg), 0,
	       "nothing has been sent, so nothing is complete yet");

	KconBuf p = { 0 };
	kcon_put_u32(&p, 0xcafebabeu);
	kcon_put_str(&p, "hello");
	eq_int(kcon_send(tx, 42, &p), 0, "a message goes out");
	kcon_flush(tx);

	eq_int(kcon_recv(rxc, &msg), 1, "and arrives whole at the other end");
	eq_int(msg.op, 42, "carrying the op it was sent with");
	kcon_rd_init(&r, msg.payload, msg.len);
	eq_int((long long)kcon_get_u32(&r), 0xcafebabell,
	       "and the payload it was given");
	eq_str(kcon_get_str(&r), "hello", "string and all");

	/* A message with no payload is a message. */
	eq_int(kcon_send(tx, 7, NULL), 0, "an op with no payload is allowed");
	kcon_flush(tx);
	eq_int(kcon_recv(rxc, &msg), 1, "and arrives");
	eq_int(msg.op, 7, "as itself");
	eq_int((long long)msg.len, 0, "carrying nothing");

	kcon_buf_free(&p);
	kcon_conn_free(tx);
	kcon_conn_free(rxc);

	/* ── the caps are what they claim ──────────────────────────────── */
	eq_int((long long)KCON_MAX_PAYLOAD, 1ll << 20,
	       "a payload is refused above a megabyte");
	eq_int(KCON_VERSION, 3, "and the version the two ends agree on");
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * A terminal window: a real child on a real pty, and the selection over what
 * it wrote.
 *
 * THE ARGUMENT VECTOR IS EXECUTED DIRECTLY. There is no shell anywhere in this
 * path — the rule the whole tree is written under — so a program is named by
 * its argv and nothing re-splits it.
 *
 * THE CHILD'S STATUS OUTLIVES IT, in the shell's own convention: the exit code,
 * 128 plus the signal when it was killed, and 127 when the exec never happened.
 * That is what lets a window say how its program finished instead of vanishing
 * with the message.
 */

/* Pump until the child is gone, then a little more for what it left in the
 * pty. Bounded, because a test that hangs is worse than one that fails. */
static int term_settle(struct kvt_term *t)
{
	for (int i = 0; i < 400 && kvt_term_alive(t); i++) {
		kvt_term_pump(t);
		usleep(2000);
	}
	for (int i = 0; i < 20; i++)
		kvt_term_pump(t);
	return kvt_term_status(t);
}

static void kvt_write_cb(struct kvt_vte *vte, const char *u8, size_t len,
			 void *data)
{
	(void)vte; (void)u8; (void)len; (void)data;
}

static void test_kvt_term(void)
{
	printf("\n==> a terminal window: a child on a pty, and the selection\n");

	enum { TW = 20, TH = 4 };
	KtuiCell cells[TW * TH];
	char row[TW + 1];

	/* ── a command runs and its output lands ───────────────────────── */
	static const char *const echo_argv[] = { "/bin/echo", "hello", NULL };
	struct kvt_term *t = kvt_term_open(echo_argv, TW, TH);

	ok(t != NULL, "a terminal opens on a real pty");
	if (!t)
		return;

	eq_int(term_settle(t), 0, "a command that succeeded reports zero");
	eq_int(kvt_term_alive(t), 0, "and is no longer alive");
	eq_int(kvt_term_status(t), 0,
	       "its status is still readable after it has gone");

	kvt_term_render(t, cells, TW, TH);
	for (int x = 0; x < TW; x++)
		row[x] = cells[x].ch >= 32 && cells[x].ch < 127
			? (char)cells[x].ch : ' ';
	row[TW] = '\0';
	ok(!strncmp(row, "hello", 5), "what it printed is on the first row");
	kvt_term_close(t);

	/* ── how a program finished ────────────────────────────────────── */
	static const char *const fail_argv[] = { "/bin/sh", "-c", "exit 3", NULL };

	t = kvt_term_open(fail_argv, TW, TH);
	ok(t != NULL, "a second terminal opens");
	if (t) {
		eq_int(term_settle(t), 3, "an exit code comes back as itself");
		kvt_term_close(t);
	}

	static const char *const kill_argv[] = {
		"/bin/sh", "-c", "kill -TERM $$", NULL
	};

	t = kvt_term_open(kill_argv, TW, TH);
	if (t) {
		eq_int(term_settle(t), 128 + 15,
		       "a killed child reports 128 plus its signal");
		kvt_term_close(t);
	}

	/*
	 * AN EXEC THAT NEVER HAPPENED IS 127, the same answer a shell gives.
	 * The window has to be able to say "there is no such program" rather
	 * than opening empty and closing again with nothing said.
	 */
	static const char *const missing_argv[] = {
		"/nonexistent/kdos-selftest-no-such-binary", NULL
	};

	t = kvt_term_open(missing_argv, TW, TH);
	ok(t != NULL, "a terminal for a program that does not exist still opens");
	if (t) {
		eq_int(term_settle(t), 127, "and reports that the exec failed");
		kvt_term_close(t);
	}

	/* ── the selection, over a line the terminal wrapped itself ────── */
	struct kvt_screen *scr;
	struct kvt_vte *vte;

	if (kvt_screen_new(&scr, NULL, NULL) != 0) {
		ok(0, "a screen for the selection assertions");
		return;
	}
	kvt_screen_resize(scr, 10, 4);
	if (kvt_vte_new(&vte, scr, kvt_write_cb, NULL, NULL, NULL) != 0) {
		ok(0, "a state machine for the selection assertions");
		kvt_screen_unref(scr);
		return;
	}

	static const char wrapped[] = "abcdefghijklmno";	/* wraps at 10 */

	kvt_vte_input(vte, wrapped, sizeof(wrapped) - 1);
	eq_int((int)kvt_screen_get_cursor_y(scr), 1,
	       "fifteen characters into ten columns puts the cursor on row two");

	char *out = NULL;

	kvt_screen_selection_reset(scr);
	kvt_screen_selection_start(scr, 0, 0);
	kvt_screen_selection_target(scr, 4, 1);
	int n = kvt_screen_selection_copy(scr, &out);

	ok(out != NULL, "a selection copies");
	eq_int(n, 16, "spanning both rows of the wrapped line");
	/*
	 * A SOFT WRAP COMES BACK AS A NEWLINE. The screen stores rows and the
	 * copy walks rows, so the point at which the terminal ran out of
	 * columns is indistinguishable from the point at which the program
	 * pressed return. Recorded because it decides what you get when you
	 * copy a long path out of a terminal, and because it is the fork's
	 * behaviour rather than a choice made here.
	 */
	eq_str(out ? out : "", "abcdefghij\nklmno",
	       "and the wrap comes back as a newline");
	free(out);
	out = NULL;

	/* Double click takes the word under the pointer. */
	kvt_screen_selection_reset(scr);
	kvt_screen_selection_word(scr, 2, 0);
	n = kvt_screen_selection_copy(scr, &out);
	eq_int(n, 10, "a word selection takes the run of characters around it");
	eq_str(out ? out : "", "abcdefghij", "which here is the whole row");
	free(out);
	out = NULL;

	/* Reset means reset: nothing is left selected. */
	kvt_screen_selection_reset(scr);
	n = kvt_screen_selection_copy(scr, &out);
	ok(n <= 0 || (out && out[0] == '\0'),
	   "resetting the selection leaves nothing to copy");
	free(out);

	kvt_vte_unref(vte);
	kvt_screen_unref(scr);
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * libkimg: the only place in KDOS where untrusted image bytes are decoded.
 *
 * The corpus under testing/fixtures/img/ is one file per ANSWER rather than one
 * per format: a valid picture, a truncated one, one whose header declares a
 * size past any budget, and one that declares a zero dimension. What is
 * asserted is that every failure is the SAME failure — NULL — because there is
 * nothing useful a caller could do differently, and a reason string reaching a
 * log is a reason string an attacker chose.
 *
 * Guarded on the decoders being present, the way the kdos-shell block is: none
 * of the four is on a bare host, and a library nobody can build is a library
 * nobody runs the assertions for — which is worse than a skip that says so.
 * testing/fixtures/img/fuzz.c is the other half, and it is its own binary so it
 * can be run under the sanitisers on its own.
 */

#ifdef HAVE_KIMG

static unsigned char *img_slurp(const char *name, size_t *len)
{
	char path[256];
	snprintf(path, sizeof(path), "testing/fixtures/img/%s", name);

	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;

	static unsigned char buf[1 << 16];
	size_t n = fread(buf, 1, sizeof(buf), f);

	fclose(f);
	*len = n;
	return n ? buf : NULL;
}

/* The budget every case below is measured against. Small on purpose: the
 * "huge" fixtures declare a size past it in their own headers, which is what
 * lets the refusal happen before a single pixel is allocated. */
static const KimgBudget img_budget = {
	.max_w = 256, .max_h = 256, .max_bytes = 256 * 256 * 4
};

static int img_have(int type)
{
	return (kimg_formats() & (1u << (unsigned)type)) != 0;
}

/* Decode one fixture and say whether anything came back. */
static int img_decodes(const char *name, int type)
{
	size_t n = 0;
	const unsigned char *b = img_slurp(name, &n);

	if (!b)
		return -1;			/* fixture missing */

	pixman_image_t *img = kimg_decode(b, n, type, &img_budget);

	if (!img)
		return 0;
	pixman_image_unref(img);
	return 1;
}

static void test_kimg(void)
{
	printf("\n==> libkimg decodes a picture, and refuses everything else\n");

	static const struct {
		const char *valid, *truncated, *huge, *zero;
		int type;
		const char *what;
	} fmt[] = {
		{ "valid.png",  "truncated.png",  "huge.png",  "zero.png",
		  KIMG_PNG,  "PNG" },
		{ "valid.jpg",  "truncated.jpg",  "huge.jpg",  "zero.jpg",
		  KIMG_JPEG, "JPEG" },
		{ "valid.webp", "truncated.webp", "huge.webp", NULL,
		  KIMG_WEBP, "WebP" },
		{ "valid.six",  NULL,             NULL,        "zero.six",
		  KIMG_SIXEL, "sixel" },
	};

	int ran = 0;

	for (size_t i = 0; i < sizeof(fmt) / sizeof(fmt[0]); i++) {
		char msg[128];

		if (!img_have(fmt[i].type))
			continue;
		ran++;

		snprintf(msg, sizeof(msg), "a valid %s decodes", fmt[i].what);
		eq_int(img_decodes(fmt[i].valid, fmt[i].type), 1, msg);

		if (fmt[i].truncated) {
			snprintf(msg, sizeof(msg),
				 "a truncated %s is refused", fmt[i].what);
			eq_int(img_decodes(fmt[i].truncated, fmt[i].type), 0,
			       msg);
		}
		if (fmt[i].huge) {
			snprintf(msg, sizeof(msg),
				 "a %s declaring a size past the budget is"
				 " refused before it is decoded", fmt[i].what);
			eq_int(img_decodes(fmt[i].huge, fmt[i].type), 0, msg);
		}
		if (fmt[i].zero) {
			snprintf(msg, sizeof(msg),
				 "a %s with a zero dimension is refused",
				 fmt[i].what);
			eq_int(img_decodes(fmt[i].zero, fmt[i].type), 0, msg);
		}
	}

	ok(ran > 0, "at least one decoder was present to check");

	/* A decompression bomb is four lines of sixel. */
	if (img_have(KIMG_SIXEL))
		eq_int(img_decodes("bomb.six", KIMG_SIXEL), 0,
		       "a sixel that expands past the budget is refused");

	/* A WebP that declares no dimensions at all. */
	if (img_have(KIMG_WEBP))
		eq_int(img_decodes("nodims.webp", KIMG_WEBP), 0,
		       "a WebP that declares no dimensions is refused");

	/*
	 * A DECLARED TYPE THAT DISAGREES WITH THE BYTES IS A REFUSAL, NOT A
	 * RE-SNIFF. A peer saying PNG and sending JPEG is not making a mistake
	 * worth accommodating, and re-sniffing would mean its declaration
	 * decided nothing.
	 */
	if (img_have(KIMG_PNG) && img_have(KIMG_JPEG)) {
		eq_int(img_decodes("valid.png", KIMG_JPEG), 0,
		       "PNG bytes declared as JPEG are refused, not re-sniffed");
		eq_int(img_decodes("valid.jpg", KIMG_PNG), 0,
		       "and JPEG bytes declared as PNG likewise");
	}

	/*
	 * KIMG_AUTO sniffs, and it exists for OSC 1337 — which names a FILE,
	 * not a format, so there is nothing for the sender to have declared.
	 */
	if (img_have(KIMG_PNG))
		eq_int(img_decodes("valid.png", KIMG_AUTO), 1,
		       "and with nothing declared, the magic is sniffed");

	/* Nothing at all is still an answer. */
	pixman_image_t *none = kimg_decode(NULL, 0, KIMG_AUTO, &img_budget);

	ok(none == NULL, "no bytes decode to nothing rather than crashing");
	ok(kimg_formats() != 0,
	   "this build reports which formats it can actually decode");
}

#else	/* !HAVE_KIMG */

static void test_kimg(void)
{
	printf("\n==> libkimg: skipped, no pixman on this host\n");
}

#endif

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The server half: a real listening socket, a real client on it, and the two
 * refusals that are the whole of the protocol's safety.
 *
 * A SESSION A VIEW CAN ATTACH TO IS A KEYLOGGER IF ANYTHING CAN ATTACH, so the
 * rules that decide who gets in are worth more assertions than the ones that
 * carry cells: a first message that is not a hello, and a version that does not
 * match, are both refusals rather than best-effort.
 */

/* Connect to a unix socket and wrap it. Returns NULL rather than asserting, so
 * a host that cannot make one skips instead of failing. */
static KconConn *srv_client(const char *path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		return NULL;

	struct sockaddr_un a;

	memset(&a, 0, sizeof(a));
	a.sun_family = AF_UNIX;
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", path);
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
		close(fd);
		return NULL;
	}
	return kcon_conn_new(fd);
}

static void srv_hello(KconConn *c, unsigned ver, unsigned kind)
{
	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)ver);
	kcon_put_u16(&b, (uint16_t)kind);
	kcon_send(c, KCON_OP_HELLO, &b);
	kcon_flush(c);
	kcon_buf_free(&b);
}

static void test_kcon_server(void)
{
	printf("\n==> the session socket: who gets in, and who is dropped\n");

	char dir[] = "/tmp/kdos-selftest-con.XXXXXX";

	if (!mkdtemp(dir)) {
		ok(0, "a directory for the session socket");
		return;
	}

	char path[256];

	snprintf(path, sizeof(path), "%s/session.sock", dir);

	KconServer *s = kcon_server_new(path);

	ok(s != NULL, "a session listens on a unix socket");
	if (!s) {
		rmdir(dir);
		return;
	}

	/* ── a peer that says hello properly is admitted ───────────────── */
	KconConn *good = srv_client(path);

	ok(good != NULL, "a client connects to it");
	if (good) {
		srv_hello(good, KCON_VERSION, 0);
		kcon_server_pump(s);
		eq_int(kcon_server_count(s), 1, "and the session counts it");
	}

	/* ── a version that does not match is refused, with both numbers ── */
	KconConn *old = srv_client(path);

	if (old) {
		srv_hello(old, KCON_VERSION + 1, 0);
		kcon_server_pump(s);

		/*
		 * BOTH NUMBERS COME BACK. "Protocol error" tells the person
		 * nothing about which half to rebuild, and the two ends of a
		 * forwarded socket are not always the same build.
		 */
		KconMsg m;
		int got = 0;

		for (int i = 0; i < 50 && !got; i++) {
			if (kcon_recv(old, &m) == 1)
				got = 1;
			else
				usleep(1000);
		}
		ok(got, "a version mismatch is answered rather than dropped");
		if (got) {
			KconRd r;

			kcon_rd_init(&r, m.payload, m.len);
			eq_int(kcon_get_u16(&r), KCON_VERSION,
			       "the answer carries the version the session speaks");
			eq_int(kcon_get_u16(&r), KCON_VERSION + 1,
			       "and the one the peer claimed");
		}
		kcon_conn_free(old);
	}

	/* ── a first message that is not a hello ends the connection ───── */
	KconConn *rude = srv_client(path);

	if (rude) {
		int before = kcon_server_count(s);
		KconBuf b = { 0 };

		kcon_put_u16(&b, 1);
		kcon_send(rude, KCON_OP_COMMIT, &b);	/* before saying hello */
		kcon_flush(rude);
		kcon_buf_free(&b);

		for (int i = 0; i < 20; i++) {
			kcon_server_pump(s);
			usleep(1000);
		}
		ok(kcon_server_count(s) <= before,
		   "a peer whose first message is not a hello is dropped");
		kcon_conn_free(rude);
	}

	if (good)
		kcon_conn_free(good);
	kcon_server_free(s);
	unlink(path);
	rmdir(dir);

	/* ── a peer that stops reading is dropped, not waited for ──────── */
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		ok(0, "a socketpair for the queue-cap assertion");
		return;
	}

	KconConn *tx = kcon_conn_new(sv[0]);

	/*
	 * THE FAR END NEVER READS. sv[1] is left open and undrained, so the
	 * socket buffer fills and everything after it queues in the sender.
	 * Past KCON_MAX_QUEUE that is not backpressure to wait out — it is a
	 * peer that has stopped — and the session drops it rather than
	 * blocking the display behind it.
	 */
	KconBuf big = { 0 };
	static unsigned char filler[32 * 1024];

	kcon_put_bytes(&big, filler, sizeof(filler));

	int died = 0;

	for (int i = 0; i < 4096 && !died; i++) {
		if (kcon_send(tx, 99, &big) != 0 || kcon_conn_dead(tx))
			died = 1;
		kcon_flush(tx);
	}
	ok(died, "a peer that stops reading is dropped past the queue cap");
	ok(kcon_conn_dead(tx), "and the connection says so");
	eq_int((long long)KCON_MAX_QUEUE, 4ll << 20,
	       "which is four megabytes of unread output");

	kcon_buf_free(&big);
	kcon_conn_free(tx);
	close(sv[1]);
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * Driving a real full-screen program, interactively.
 *
 * The recorded streams under testing/fixtures/vt/ prove the state machine
 * against what these programs actually wrote. What a recording cannot show is
 * the other direction — that a keystroke REACHES the child, and that a program
 * REFLOWS when its window changes size — because both are things the program
 * does in response to us rather than things it once did.
 *
 * Neither needs a booted desktop: a pty and a child are the whole apparatus.
 * What the rig still owes after this is the integration — the same program in a
 * kdos-term window on a console desktop somebody is looking at — and not the
 * mechanism, which is here.
 *
 * EVERY PROGRAM IS OPTIONAL. An exec that did not happen reports 127, so a host
 * without one skips it by name instead of failing, exactly as the decoder
 * blocks do.
 */

#define DRV_W 60
#define DRV_H 12

/* The rendered screen as printable text, rows joined by newlines. */
static void drv_text(struct kvt_term *t, char *out, size_t cap, int w, int h)
{
	KtuiCell cells[DRV_W * DRV_H];
	size_t o = 0;

	if (w > DRV_W)
		w = DRV_W;
	if (h > DRV_H)
		h = DRV_H;
	kvt_term_render(t, cells, w, h);
	for (int y = 0; y < h && o + 1 < cap; y++) {
		for (int x = 0; x < w && o + 1 < cap; x++) {
			uint32_t ch = cells[y * w + x].ch;

			out[o++] = ch >= 32 && ch < 127 ? (char)ch : ' ';
		}
		if (o + 1 < cap)
			out[o++] = '\n';
	}
	out[o] = '\0';
}

/*
 * A signature over the WHOLE cell, not just its character.
 *
 * Comparing rendered text alone misses a change that is only colour or
 * attribute — a file manager moving its panel highlight redraws no glyph at
 * all — and "the keystroke did nothing" and "the keystroke changed something I
 * am not looking at" are not the same answer.
 */
static unsigned long drv_sig(struct kvt_term *t, int w, int h)
{
	KtuiCell cells[DRV_W * DRV_H];
	unsigned long g = 1469598103934665603UL;

	if (w > DRV_W)
		w = DRV_W;
	if (h > DRV_H)
		h = DRV_H;
	kvt_term_render(t, cells, w, h);
	for (int i = 0; i < w * h; i++) {
		unsigned long v = cells[i].ch ^ ((unsigned long)cells[i].fg << 24)
			^ ((unsigned long)cells[i].bg << 32)
			^ ((unsigned long)cells[i].attr << 40);

		g = (g ^ v) * 1099511628211UL;
	}
	return g;
}

/* Pump for a while, whether or not the child has exited. */
static void drv_settle(struct kvt_term *t, int ms)
{
	for (int i = 0; i < ms / 5; i++) {
		kvt_term_pump(t);
		usleep(5000);
	}
}

static int drv_nonblank(const char *s)
{
	for (; *s; s++)
		if (*s != ' ' && *s != '\n')
			return 1;
	return 0;
}

static void test_kvt_drive(void)
{
	printf("\n==> a real program, driven: a keystroke lands and a resize reflows\n");

	/* Something with more lines than the window, for the pager. */
	const char *big = "/tmp/kdos-selftest-pager.txt";
	FILE *bf = fopen(big, "w");

	if (bf) {
		for (int i = 1; i <= 400; i++)
			fprintf(bf, "line %03d ------------------------------\n", i);
		fclose(bf);
	}

	static const struct {
		const char *name;
		const char *argv[5];
		int key;		/* what to press */
		const char *want;	/* what pressing it must put on screen */
	} progs[] = {
		/* A pager: `j` moves one line, so line 001 leaves the top. */
		{ "less",  { "less", "-X", "/tmp/kdos-selftest-pager.txt", NULL },
		  'j', "line 002" },
		/*
		 * vi: `i` is insert mode and it says so on the last row — but
		 * ONLY with -N. `-u NONE` alone implies 'compatible', which
		 * turns showmode off, and then the keystroke lands and changes
		 * nothing anybody can see. Measured, not assumed.
		 */
		{ "vim",   { "vim", "-N", "-u", "NONE", NULL }, 'i', "INSERT" },
		/* A process monitor redraws on any key; `h` opens its help. */
		{ "htop",  { "htop", NULL }, 'h', NULL },
		{ "top",   { "top", NULL }, 'h', NULL },
		/* A file manager: Tab moves to the other panel. */
		{ "mc",    { "mc", "-d", NULL }, KT_K_TAB, NULL },
		/* A multiplexer draws a status bar and passes keys to a shell. */
		{ "tmux",  { "tmux", "-f", "/dev/null", NULL }, 'e', NULL },
		{ "nano",  { "nano", NULL }, 'Z', "Z" },
	};

	int ran = 0;
	char cwd[4096];

	/*
	 * Drive them from a scratch directory. A program interrupted mid-edit
	 * writes its rescue file into the CURRENT one — nano leaves
	 * `nano.<pid>.save` — and the current one here is the source tree.
	 */
	if (!getcwd(cwd, sizeof(cwd)))
		cwd[0] = '\0';
	mkdir("/tmp/kdos-selftest-drive", 0700);
	if (chdir("/tmp/kdos-selftest-drive") < 0) {
		puts("  (no scratch directory to drive them from — skipped)");
		return;
	}

	for (size_t i = 0; i < sizeof(progs) / sizeof(progs[0]); i++) {
		char before[DRV_W * DRV_H + DRV_H + 1];
		char after[sizeof(before)];
		char wide[sizeof(before)];
		char msg[160];

		struct kvt_term *t = kvt_term_open(progs[i].argv, DRV_W, DRV_H);

		if (!t)
			continue;

		drv_settle(t, 600);

		/* 127 is an exec that never happened: this host has not got it. */
		if (!kvt_term_alive(t) && kvt_term_status(t) == 127) {
			printf("  %s: not on this host, skipped\n", progs[i].name);
			kvt_term_close(t);
			continue;
		}
		if (!kvt_term_alive(t)) {
			printf("  %s: exited before it could be driven, skipped\n",
			       progs[i].name);
			kvt_term_close(t);
			continue;
		}
		ran++;

		drv_text(t, before, sizeof(before), DRV_W, DRV_H);
		unsigned long sig0 = drv_sig(t, DRV_W, DRV_H);

		snprintf(msg, sizeof(msg), "%s draws something to begin with",
			 progs[i].name);
		ok(drv_nonblank(before), msg);

		/* ── a keystroke reaches the child ─────────────────────────── */
		kvt_term_key(t, progs[i].key, 0);
		drv_settle(t, 700);
		drv_text(t, after, sizeof(after), DRV_W, DRV_H);

		snprintf(msg, sizeof(msg),
			 "%s: a keystroke reaches the child and changes the screen",
			 progs[i].name);
		ok(drv_sig(t, DRV_W, DRV_H) != sig0, msg);

		if (progs[i].want) {
			snprintf(msg, sizeof(msg), "%s: and it did the thing —"
				 " '%s' is on screen", progs[i].name,
				 progs[i].want);
			ok(strstr(after, progs[i].want) != NULL, msg);
		}

		/* ── the window resizes with the program in it ─────────────── */
		kvt_term_resize(t, DRV_W - 20, DRV_H);
		drv_settle(t, 700);
		drv_text(t, wide, sizeof(wide), DRV_W - 20, DRV_H);

		snprintf(msg, sizeof(msg),
			 "%s: it is still drawing after the window changed size",
			 progs[i].name);
		ok(drv_nonblank(wide), msg);

		snprintf(msg, sizeof(msg),
			 "%s: and reflowed into the narrower window",
			 progs[i].name);
		ok(strcmp(wide, after) != 0, msg);
		(void)after;

		/*
		 * CLOSED BY A SIGNAL, not by a quit key. Every one of these
		 * quits differently and a test that typed the wrong one would
		 * hang waiting for a program that is still asking whether to
		 * save.
		 */
		kvt_term_signal(t, SIGTERM);
		drv_settle(t, 300);
		kvt_term_close(t);
	}

	if (cwd[0] && chdir(cwd) < 0)
		puts("  (could not return to the directory this started in)");

	ok(ran > 0, "at least one catalogue program was here to drive");
	printf("  %d program(s) driven\n", ran);
	unlink(big);
}

int main(void)
{
	kb_set_progname("selftest");

	/* FIRST, and it has to be: it needs a presented frame, and
	 * ktui_offscreen_init() below is a one-way latch that makes a flush a
	 * no-op for the rest of the process. Its own guard says so if this
	 * moves. */
	test_sprite();

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
	test_wm();
	test_gesture();
	test_vt_img();
	test_kvt_term();
	test_kvt_drive();
	test_kimg();
	test_kcon();
	test_kcon_server();

	printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
