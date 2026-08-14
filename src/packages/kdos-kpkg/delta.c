/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   deltas — an update that ships the difference, not the package
 *
 *   kpkg delta <old.tar.xz> <new.tar.xz> [-o <file>]
 *   kpkg apply-delta <old.tar.xz> <delta> -o <new.tar.xz>
 *
 * `zstd --patch-from` is the whole engine; everything here is the two decisions
 * around it.
 *
 * FIRST: the delta is taken over the UNCOMPRESSED tars. Two `.tar.xz` files
 * built from almost identical trees share almost no bytes — that is what a
 * compressor is for — so a delta between them is the size of the whole package.
 * Decompressing first is what turns a 1.3 MB download into a 29 KB one.
 *
 * SECOND: a delta is never trusted, and it never needs to be. It is applied and
 * the RESULT is hashed against the `C:` the signed index already carries for the
 * target package. A delta that was tampered with produces a package whose hash
 * does not match and is thrown away; a delta cannot make the client install
 * anything the index did not already name. So there is no delta signature, no
 * second trust path, and nothing new to get wrong.
 *
 * The client can only use a delta when it still HAS the old package — KDOS keeps
 * built packages in PACKAGE_DIR, so this is the ordinary case for a machine that
 * has been updating rather than installing fresh. When the old package is gone,
 * the full package is the answer and nothing has been lost.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kdos-kpkg.h"

/* xz -dc <in> > <out>, without a shell: the redirect is a file the child is
 * given, which is what kb_run_out exists for. */
static int decompress(const char *in, const char *out)
{
	KbArgv a = {0};
	kb_argv_add(&a, "xz");
	kb_argv_add(&a, "-dc");
	kb_argv_add(&a, in);
	kb_argv_end(&a);
	return kb_run_to_file(&a, out);
}

static int compress(const char *in, const char *out)
{
	/* The same pinned compressor the packager uses, for the same reason:
	 * a delta that is regenerated must come out identical. */
	KbArgv a = {0};
	kb_argv_add(&a, "xz");
	kb_argv_add(&a, "-9");
	kb_argv_add(&a, "-T1");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, in);
	kb_argv_end(&a);
	return kb_run_to_file(&a, out);
}

static long long file_size(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

/* A scratch directory that cleans up after itself even when a step fails. */
static int scratch(char *out, size_t cap)
{
	const char *tmp = getenv("TMPDIR");
	snprintf(out, cap, "%s/kpkg-delta.XXXXXX", tmp && *tmp ? tmp : "/tmp");
	return mkdtemp(out) ? 0 : -1;
}

int kp_delta_make(const char *oldpkg, const char *newpkg, const char *out)
{
	char dir[512];
	if (scratch(dir, sizeof(dir)) != 0) {
		kp_err("cannot create a scratch directory");
		return 1;
	}

	char oldtar[600], newtar[600];
	snprintf(oldtar, sizeof(oldtar), "%s/old.tar", dir);
	snprintf(newtar, sizeof(newtar), "%s/new.tar", dir);

	int rc = 1;
	if (decompress(oldpkg, oldtar) != 0 || decompress(newpkg, newtar) != 0) {
		kp_err("cannot decompress the packages");
		goto done;
	}

	/*
	 * --patch-from needs its window to cover the whole reference file, and
	 * zstd refuses a reference larger than the default window rather than
	 * silently producing a worse delta. --long=27 is 128 MB, which is more
	 * than any package in this tree and costs memory only while running.
	 */
	KbArgv a = {0};
	char patch_arg[700];
	snprintf(patch_arg, sizeof(patch_arg), "--patch-from=%s", oldtar);
	kb_argv_add(&a, "zstd");
	kb_argv_add(&a, "-19");
	kb_argv_add(&a, "--long=27");
	kb_argv_add(&a, "-q");
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, patch_arg);
	kb_argv_add(&a, newtar);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, (char *)out);
	kb_argv_end(&a);
	if (kb_run(&a) != 0) {
		kp_err("zstd --patch-from failed");
		goto done;
	}
	rc = 0;
done:
	kb_rmtree(dir);
	return rc;
}

int kp_delta_apply(const char *oldpkg, const char *delta, const char *out)
{
	char dir[512];
	if (scratch(dir, sizeof(dir)) != 0)
		return 1;

	char oldtar[600], newtar[600];
	snprintf(oldtar, sizeof(oldtar), "%s/old.tar", dir);
	snprintf(newtar, sizeof(newtar), "%s/new.tar", dir);

	int rc = 1;
	if (decompress(oldpkg, oldtar) != 0)
		goto done;

	KbArgv a = {0};
	char patch_arg[700];
	snprintf(patch_arg, sizeof(patch_arg), "--patch-from=%s", oldtar);
	kb_argv_add(&a, "zstd");
	kb_argv_add(&a, "-d");
	kb_argv_add(&a, "--long=27");
	kb_argv_add(&a, "-q");
	kb_argv_add(&a, "-f");
	kb_argv_add(&a, patch_arg);
	kb_argv_add(&a, (char *)delta);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, newtar);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		goto done;

	/*
	 * Recompressed with the packager's own settings, so the reconstructed
	 * file is byte-identical to the package the builder signed — which is
	 * what lets the caller check it against the index's SHA-256 rather than
	 * having to trust the delta.
	 */
	if (compress(newtar, out) != 0)
		goto done;
	rc = 0;
done:
	kb_rmtree(dir);
	return rc;
}

/* ── the commands ──────────────────────────────────────────────────────── */

int kp_cmd_delta(int argc, char **argv)
{
	const char *oldp = NULL, *newp = NULL, *out = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc)
			out = argv[++i];
		else if (!oldp)
			oldp = argv[i];
		else if (!newp)
			newp = argv[i];
	}
	if (!oldp || !newp) {
		printf("Usage: kpkg delta <old.tar.xz> <new.tar.xz> "
		       "[-o <file.kdelta>]\n");
		return 1;
	}

	char def[700];
	if (!out) {
		/* Named for BOTH ends: a delta is only usable by someone
		 * holding exactly the old package it was made against, so the
		 * name has to say which one that is. */
		char base[256];
		kb_strlcpy(base, kb_basename(newp), sizeof(base));
		char *ext = strstr(base, ".tar.xz");
		if (ext)
			*ext = 0;
		char oldbase[256];
		kb_strlcpy(oldbase, kb_basename(oldp), sizeof(oldbase));
		ext = strstr(oldbase, ".tar.xz");
		if (ext)
			*ext = 0;
		snprintf(def, sizeof(def), "%s--from--%s.kdelta", base, oldbase);
		out = def;
	}

	if (kp_delta_make(oldp, newp, out) != 0)
		return 1;

	long long ds = file_size(out), ns = file_size(newp);
	if (ds > 0 && ns > 0)
		kp_msg("%s: %lld bytes, %.1fx smaller than the package "
		       "(%lld bytes)", out, ds, (double)ns / (double)ds, ns);
	return 0;
}

int kp_cmd_apply_delta(int argc, char **argv)
{
	const char *oldp = NULL, *delta = NULL, *out = NULL, *want = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--expect") && i + 1 < argc)
			want = argv[++i];
		else if (!oldp)
			oldp = argv[i];
		else if (!delta)
			delta = argv[i];
	}
	if (!oldp || !delta || !out) {
		printf("Usage: kpkg apply-delta <old.tar.xz> <delta> "
		       "-o <new.tar.xz> [--expect <sha256>]\n");
		return 1;
	}

	if (kp_delta_apply(oldp, delta, out) != 0) {
		kp_err("could not apply %s", delta);
		return 1;
	}

	if (want) {
		char sha[65] = "";
		if (kb_sha256_file(out, sha) != 0 || strcmp(sha, want)) {
			/* The reconstruction is the thing being checked, and a
			 * failed check leaves nothing behind: half a package on
			 * disk is worse than none. */
			unlink(out);
			kp_err("the reconstructed package does not match the "
			       "expected checksum — discarded");
			return 1;
		}
		kp_msg("%s: reconstructed and verified (%.16s...)", out, sha);
	} else {
		kp_msg("%s: reconstructed (no checksum given to verify it "
		       "against)", out);
	}
	return 0;
}
