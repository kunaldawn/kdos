/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the binary repository: an index, a signature, and three equality tests
 *
 *   kpkg keygen <name>            make a signing key
 *   kpkg index <dir> [--sign K]   write PACKAGES (+ PACKAGES.sig, + sidecars)
 *   kpkg verify-index <dir>       check it against the keyring
 *   kpkg binhost <dir> <port>     install the prebuilt package IF it matches
 *
 * The index format is Alpine's shape — single-character keys, one stanza per
 * package, blank line between — because it parses in sixty lines of C and reads
 * fine in a pager. What is NOT Alpine's is the key: two hashes replace the whole
 * USE-flag matching problem, since KDOS has no USE flags.
 *
 *   A:  architecture
 *   B:  build-config hash   (flags, target, libc, compiler)
 *   E:  recipe hash         (kpkgbuild, build.sh, postinstall.sh, patches)
 *
 * A client uses a prebuilt package when all three equal its own, and builds
 * from source otherwise. There is no BUILD_ID counter and no "close enough".
 *
 * SIGN THE INDEX, NOT 353 PACKAGES. The index carries each package's SHA-256,
 * so one signature transitively covers every file it names — and the per-package
 * `.sig` sidecar exists for the other case, a package that travels on a USB
 * stick with no index beside it.
 *
 * Two rules this file exists to keep:
 *
 *   - A signature is verified against the LOCAL KEYRING or not at all. The key
 *     id inside a signature file selects which key to try; it can never supply
 *     one. `--insecure` is spelled out on the command line every time, and says
 *     so in the output.
 *   - The index is checked BEFORE the package, and the package's hash is checked
 *     before it is unpacked. Verifying after install is verifying nothing.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kdos-kpkg.h"
#include "ksig.h"

#define INDEX_NAME "PACKAGES"
#define INDEX_SIG  "PACKAGES.sig"

/* Where a machine keeps the keys it trusts. One directory of `*.pub`; adding a
 * key is copying a file in, which is the whole administration story. */
static const char *keyring_dir(void)
{
	const char *e = getenv("KPKG_KEYRING");
	return (e && *e) ? e : "/etc/kdos/keys";
}

/* ── the index ─────────────────────────────────────────────────────────── */

typedef struct {
	char name[128];
	char version[64];
	char release[32];
	char arch[32];
	char file[256];
	char sha[65];
	char bhash[65];
	char ehash[65];
	/* Set only on a DELTA stanza: the package file this one applies to. Its
	 * presence is what makes a stanza a delta — there is no type field,
	 * because "it names what it patches" is the same statement. */
	char from[256];
	long long size;
} Stanza;

/* Alpine's shape: `K:value` lines, a blank line between stanzas. Sixty lines of
 * parser and no library, which is the whole reason for choosing it. */
static int index_parse(const char *data, Stanza *out, int max)
{
	Stanza cur = {0};
	int n = 0, have = 0;

	for (const char *line = data; line && *line;) {
		const char *nl = strchr(line, '\n');
		size_t len = nl ? (size_t)(nl - line) : strlen(line);
		char buf[1024];

		if (len >= sizeof(buf))
			len = sizeof(buf) - 1;
		memcpy(buf, line, len);
		buf[len] = 0;
		line = nl ? nl + 1 : NULL;

		if (!buf[0]) {			/* stanza boundary */
			if (have && n < max)
				out[n++] = cur;
			memset(&cur, 0, sizeof(cur));
			have = 0;
			continue;
		}
		if (buf[1] != ':')
			continue;		/* header lines and comments */
		const char *v = buf + 2;
		switch (buf[0]) {
		case 'P': kb_strlcpy(cur.name, v, sizeof(cur.name)); have = 1; break;
		case 'V': kb_strlcpy(cur.version, v, sizeof(cur.version)); break;
		case 'R': kb_strlcpy(cur.release, v, sizeof(cur.release)); break;
		case 'A': kb_strlcpy(cur.arch, v, sizeof(cur.arch)); break;
		case 'F': kb_strlcpy(cur.file, v, sizeof(cur.file)); break;
		case 'C': kb_strlcpy(cur.sha, v, sizeof(cur.sha)); break;
		case 'B': kb_strlcpy(cur.bhash, v, sizeof(cur.bhash)); break;
		case 'E': kb_strlcpy(cur.ehash, v, sizeof(cur.ehash)); break;
		case 'O': kb_strlcpy(cur.from, v, sizeof(cur.from)); break;
		case 'S': cur.size = atoll(v); break;
		default: break;
		}
	}
	if (have && n < max)
		out[n++] = cur;
	return n;
}

/*
 * Split `<name>-<version>-<release>.tar.xz` back apart.
 *
 * The name may itself contain dashes (`gst-plugins-base`), so the split is from
 * the RIGHT: release, then version, then whatever is left is the name.
 */
static int split_pkgfile(const char *file, char *name, size_t ncap, char *ver,
			 size_t vcap, char *rel, size_t rcap)
{
	char base[256];
	kb_strlcpy(base, file, sizeof(base));
	char *ext = strstr(base, ".tar.xz");
	if (!ext)
		return -1;
	*ext = 0;

	char *dash2 = strrchr(base, '-');
	if (!dash2)
		return -1;
	*dash2 = 0;
	char *dash1 = strrchr(base, '-');
	if (!dash1)
		return -1;
	*dash1 = 0;

	kb_strlcpy(rel, dash2 + 1, rcap);
	kb_strlcpy(ver, dash1 + 1, vcap);
	kb_strlcpy(name, base, ncap);
	return 0;
}

int kp_cmd_index(const KpConf *c, int argc, char **argv)
{
	const char *dir = NULL, *keyfile = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--sign") && i + 1 < argc)
			keyfile = argv[++i];
		else if (argv[i][0] != '-')
			dir = argv[i];
	}
	if (!dir) {
		printf("Usage: kpkg index <dir> [--sign <secret-key>]\n");
		return 1;
	}

	char arch[32], bhash[65];
	kp_arch(arch, sizeof(arch));
	kp_buildconfig_hash(bhash, NULL, 0);

	char **files = kb_listdir(dir, NULL);
	if (!files) {
		kp_err("cannot read %s", dir);
		return 1;
	}
	/* Sorted: the index is an artefact that gets diffed and signed, so it
	 * must not depend on readdir order. */
	int nf = 0;
	for (char **p = files; *p; p++)
		nf++;
	for (int i = 0; i < nf; i++)
		for (int j = i + 1; j < nf; j++)
			if (strcmp(files[i], files[j]) > 0) {
				char *t = files[i];
				files[i] = files[j];
				files[j] = t;
			}

	KbBuf b = {0};
	kb_buf_printf(&b, "K:kdos-index-1\nA:%s\nB:%s\n\n", arch, bhash);

	int n = 0, nd = 0;
	for (int i = 0; i < nf; i++) {
		size_t fl = strlen(files[i]);

		/*
		 * Deltas first, so a reader meets the package before the thing
		 * that patches into it. A delta stanza names its SOURCE file and
		 * carries its own checksum; the checksum of the RESULT is the
		 * one already in the package's own stanza, which is what the
		 * client checks the reconstruction against.
		 */
		if (fl > 7 && !strcmp(files[i] + fl - 7, ".kdelta")) {
			char base[300];
			kb_strlcpy(base, files[i], sizeof(base));
			base[strlen(base) - 7] = 0;
			char *sep = strstr(base, "--from--");
			if (!sep) {
				kb_warn("%s: not a delta file name", files[i]);
				continue;
			}
			*sep = 0;
			const char *oldbase = sep + 8;

			char name[128], ver[64], rel[32];
			char target[320];
			snprintf(target, sizeof(target), "%.290s.tar.xz", base);
			if (split_pkgfile(target, name, sizeof(name), ver,
					  sizeof(ver), rel, sizeof(rel)) != 0)
				continue;

			char *dpath = kb_path_join(dir, files[i]);
			char dsha[65] = "";
			struct stat dst;
			if (kb_sha256_file(dpath, dsha) == 0 &&
			    stat(dpath, &dst) == 0) {
				kb_buf_printf(&b,
					      "P:%s\nV:%s\nR:%s\nA:%s\nF:%s\n"
					      "O:%s.tar.xz\nS:%lld\nC:%s\n\n",
					      name, ver, rel, arch, files[i],
					      oldbase, (long long)dst.st_size,
					      dsha);
				nd++;
			}
			free(dpath);
			continue;
		}

		if (fl < 8 || strcmp(files[i] + fl - 7, ".tar.xz"))
			continue;

		char name[128], ver[64], rel[32];
		if (split_pkgfile(files[i], name, sizeof(name), ver, sizeof(ver),
				  rel, sizeof(rel)) != 0) {
			kb_warn("%s: not a package file name", files[i]);
			continue;
		}

		char *path = kb_path_join(dir, files[i]);
		char sha[65] = "";
		struct stat st;
		if (kb_sha256_file(path, sha) != 0 || stat(path, &st) != 0) {
			kb_warn("%s: cannot hash", files[i]);
			free(path);
			continue;
		}

		/* The recipe hash comes from the ports tree this index is built
		 * ON, which is the machine that built the packages. A port that
		 * is no longer in the tree gets an empty E: and can therefore
		 * never satisfy a client's equality test — which is correct: we
		 * cannot say what it was built from. */
		char ehash[65] = "";
		char *portdir = kp_port_dir(c, name);
		if (portdir) {
			kp_recipe_hash(portdir, ehash);
			free(portdir);
		}

		char desc[512] = "";
		portdir = kp_port_dir(c, name);
		if (portdir) {
			kp_description(portdir, desc, sizeof(desc));
			free(portdir);
		}

		kb_buf_printf(&b,
			      "P:%s\nV:%s\nR:%s\nA:%s\nF:%s\nS:%lld\nC:%s\n"
			      "B:%s\nE:%s\nT:%s\n\n",
			      name, ver, rel, arch, files[i],
			      (long long)st.st_size, sha, bhash, ehash, desc);
		n++;
		free(path);
	}
	kb_strv_free(files);

	char *out = kb_path_join(dir, INDEX_NAME);
	int rc = kb_write_all(out, b.p, b.n);
	if (rc != 0) {
		kp_err("cannot write %s", out);
		free(out);
		kb_buf_free(&b);
		return 1;
	}
	kp_msg("Indexed %d package(s) and %d delta(s) into %s", n, nd, out);

	if (keyfile) {
		uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
		int kr = ksig_read_secret(keyfile, seed, pub);
		if (kr == -2) {
			kp_err("%s is readable by other users — refusing to "
			       "sign with it", keyfile);
			free(out);
			kb_buf_free(&b);
			return 1;
		}
		if (kr != 0) {
			kp_err("cannot read the signing key %s", keyfile);
			free(out);
			kb_buf_free(&b);
			return 1;
		}

		char *sigpath = kb_path_join(dir, INDEX_SIG);
		/* Removed first: appending would accumulate signatures from
		 * every past index, and an old signature over new content is
		 * exactly the thing that must not verify. */
		unlink(sigpath);
		if (ksig_sig_append(sigpath, seed, pub, b.p, b.n) != 0) {
			kp_err("cannot write %s", sigpath);
			free(sigpath);
			free(out);
			kb_buf_free(&b);
			return 1;
		}
		char id[KSIG_ID_HEX];
		ksig_keyid(pub, id);
		kp_msg("Signed with key %s", id);
		free(sigpath);

		/*
		 * Per-package sidecars, for the package that travels without
		 * its index — a USB stick, an email. One signature over the
		 * package's own bytes.
		 */
		int sidecars = 0;
		Stanza st[KP_MAX_INDEX];
		int ns = index_parse(b.p, st, KP_MAX_INDEX);
		for (int i = 0; i < ns; i++) {
			/* Not the deltas. A delta is verified by the checksum
			 * of what it RECONSTRUCTS — which the signed index
			 * already carries — so signing it would add a second
			 * trust path to answer a question already answered. */
			if (st[i].from[0])
				continue;
			char *pf = kb_path_join(dir, st[i].file);
			size_t len = 0;
			char *data = kb_read_all(pf, &len);
			if (data) {
				char sp[600];
				snprintf(sp, sizeof(sp), "%s.sig", pf);
				unlink(sp);
				if (ksig_sig_append(sp, seed, pub, data, len) == 0)
					sidecars++;
				free(data);
			}
			free(pf);
		}
		kp_msg("Wrote %d package signature(s)", sidecars);
		/* The seed does not outlive its use. */
		memset(seed, 0, sizeof(seed));
	}

	free(out);
	kb_buf_free(&b);
	return 0;
}

int kp_cmd_keygen(int argc, char **argv)
{
	if (argc < 1) {
		printf("Usage: kpkg keygen <name>   (writes <name>.key and "
		       "<name>.pub)\n");
		return 1;
	}
	uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
	if (ksig_keygen(seed, pub) != 0) {
		kp_err("no randomness available — refusing to invent a key");
		return 1;
	}

	char sk[512], pk[512];
	snprintf(sk, sizeof(sk), "%s.key", argv[0]);
	snprintf(pk, sizeof(pk), "%s.pub", argv[0]);
	if (ksig_write_secret(sk, seed, pub) != 0) {
		kp_err("cannot write %s (it must not already exist)", sk);
		memset(seed, 0, sizeof(seed));
		return 1;
	}
	memset(seed, 0, sizeof(seed));
	if (ksig_write_public(pk, pub, argv[0]) != 0) {
		kp_err("cannot write %s", pk);
		return 1;
	}
	char id[KSIG_ID_HEX];
	ksig_keyid(pub, id);
	kp_msg("Key %s", id);
	kp_msg("  secret: %s (mode 0600 — this is the whole of the trust)", sk);
	kp_msg("  public: %s (copy into %s on every machine that should trust "
	       "it)", pk, keyring_dir());
	return 0;
}

/* ── verification ──────────────────────────────────────────────────────── */

static int load_ring(KsigRing *ring)
{
	ksig_ring_load(ring, keyring_dir());
	if (ring->n == 0)
		kp_err("no trusted keys in %s", keyring_dir());
	return ring->n;
}

int kp_cmd_verify_index(int argc, char **argv)
{
	if (argc < 1) {
		printf("Usage: kpkg verify-index <dir>\n");
		return 1;
	}
	KsigRing ring;
	if (!load_ring(&ring))
		return 2;

	char *idx = kb_path_join(argv[0], INDEX_NAME);
	char *sig = kb_path_join(argv[0], INDEX_SIG);
	char who[KSIG_ID_HEX];
	int rc = ksig_verify_path(&ring, idx, sig, who);
	if (rc == 0)
		kp_msg("%s: signature good (key %s)", idx, who);
	else
		kp_err("%s: NO trusted signature", idx);
	free(idx);
	free(sig);
	return rc == 0 ? 0 : 1;
}

/*
 * The sidecar path: verify one package file on its own.
 *
 * This is what makes the sidecars more than decoration — a package that arrived
 * on a USB stick has no index beside it, and `<file>.sig` is the only thing that
 * can say where it came from.
 */
int kp_verify_package(const char *path, int required, char who[KSIG_ID_HEX])
{
	char sigpath[1024];
	snprintf(sigpath, sizeof(sigpath), "%s.sig", path);

	if (who)
		who[0] = 0;
	if (!kb_path_exists(sigpath)) {
		/* No signature at all. Refusing by default would break every
		 * package kpkg builds locally, which are the overwhelming
		 * majority and never signed; KPKG_REQUIRE_SIG=1 is for the
		 * machine that wants the stricter rule. */
		return required ? -1 : 1;
	}

	KsigRing ring;
	ksig_ring_load(&ring, keyring_dir());
	if (ring.n == 0)
		return required ? -1 : 1;
	return ksig_verify_path(&ring, path, sigpath, who) == 0 ? 0 : -1;
}

int kp_cmd_verify_pkg(int argc, char **argv)
{
	if (argc < 1) {
		printf("Usage: kpkg verify-pkg <package.tar.xz>\n");
		return 1;
	}
	char who[KSIG_ID_HEX];
	int rc = kp_verify_package(argv[0], 1, who);
	if (rc == 0)
		kp_msg("%s: signature good (key %s)", argv[0], who);
	else
		kp_err("%s: NO trusted signature", argv[0]);
	return rc == 0 ? 0 : 1;
}

/* ── the client ────────────────────────────────────────────────────────── */

int kp_cmd_binhost(const KpConf *c, int argc, char **argv)
{
	const char *dir = NULL, *want = NULL;
	int insecure = 0, dry = 0;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--insecure"))
			insecure = 1;
		else if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!dir)
			dir = argv[i];
		else if (!want)
			want = argv[i];
	}
	if (!dir || !want) {
		printf("Usage: kpkg binhost <dir> <port> [--dry-run] "
		       "[--insecure]\n");
		return 1;
	}

	char *idx = kb_path_join(dir, INDEX_NAME);
	size_t len = 0;
	char *data = kb_read_all(idx, &len);
	if (!data) {
		kp_err("no index at %s", idx);
		free(idx);
		return 2;
	}

	/*
	 * The index is verified BEFORE anything in it is believed. Skipping
	 * that is what `--insecure` means, and it says so out loud every time —
	 * a flag that prints nothing is a flag that gets left in a script.
	 */
	if (insecure) {
		kp_err("--insecure: the index was NOT verified");
	} else {
		KsigRing ring;
		if (!load_ring(&ring)) {
			free(data);
			free(idx);
			return 2;
		}
		char *sig = kb_path_join(dir, INDEX_SIG);
		char who[KSIG_ID_HEX];
		int vr = ksig_verify_file(&ring, sig, data, len, who);
		free(sig);
		if (vr != 0) {
			kp_err("%s: no trusted signature — refusing to use it",
			       idx);
			free(data);
			free(idx);
			return 2;
		}
		kp_msg("Index signature good (key %s)", who);
	}

	static Stanza st[KP_MAX_INDEX];
	int n = index_parse(data, st, KP_MAX_INDEX);
	free(data);
	free(idx);

	/* What THIS machine would build. */
	char arch[32], bhash[65], ehash[65] = "";
	kp_arch(arch, sizeof(arch));
	kp_buildconfig_hash(bhash, NULL, 0);
	char *portdir = kp_port_dir(c, want);
	if (portdir) {
		kp_recipe_hash(portdir, ehash);
		free(portdir);
	}

	const Stanza *hit = NULL, *near = NULL;
	for (int i = 0; i < n; i++) {
		if (strcmp(st[i].name, want))
			continue;
		if (st[i].from[0])
			continue;	/* a delta, considered after the match */
		near = &st[i];
		/* The three equality tests, and nothing else. */
		if (strcmp(st[i].arch, arch) || strcmp(st[i].bhash, bhash) ||
		    !ehash[0] || strcmp(st[i].ehash, ehash))
			continue;
		hit = &st[i];
		break;
	}

	if (!hit) {
		if (!near)
			kp_msg("%s: not in this binhost — build from source",
			       want);
		else if (strcmp(near->arch, arch))
			kp_msg("%s: built for %s, this machine is %s — build "
			       "from source", want, near->arch, arch);
		else if (strcmp(near->bhash, bhash))
			kp_msg("%s: different build config (%.12s... vs "
			       "%.12s...) — build from source", want,
			       near->bhash, bhash);
		else
			kp_msg("%s: different recipe (%.12s... vs %.12s...) — "
			       "build from source", want, near->ehash,
			       ehash[0] ? ehash : "unknown");
		return 1;
	}

	/*
	 * A delta, when one applies and we still have what it applies TO.
	 *
	 * The delta is checked against its own checksum from the signed index,
	 * applied, and then the RESULT is checked against the package's
	 * checksum — which is the only test that matters and the reason a delta
	 * needs no signature of its own. A failure at any step falls back to the
	 * full package rather than to nothing.
	 */
	char *pkgfile = NULL;
	char rebuilt[1024] = "";
	for (int i = 0; i < n && !pkgfile; i++) {
		if (st[i].from[0] == 0 || strcmp(st[i].name, hit->name) ||
		    strcmp(st[i].version, hit->version) ||
		    strcmp(st[i].release, hit->release))
			continue;

		char *have = kb_path_join(c->package_dir, st[i].from);
		if (!kb_path_exists(have)) {
			free(have);
			continue;	/* the old package is gone: no delta */
		}

		char *dpath = kb_path_join(dir, st[i].file);
		char dsha[65] = "";
		if (kb_sha256_file(dpath, dsha) != 0 || strcmp(dsha, st[i].sha)) {
			kp_err("%s: the delta does not match the index — "
			       "using the full package", st[i].file);
			free(dpath);
			free(have);
			continue;
		}

		snprintf(rebuilt, sizeof(rebuilt), "%s/%s", c->package_dir,
			 hit->file);
		kp_msg("%s: applying a %lld-byte delta instead of a %lld-byte "
		       "package", hit->file, st[i].size, hit->size);
		if (kp_delta_apply(have, dpath, rebuilt) != 0) {
			kp_err("the delta would not apply — using the full "
			       "package");
			rebuilt[0] = 0;
		} else {
			char got[65] = "";
			if (kb_sha256_file(rebuilt, got) != 0 ||
			    strcmp(got, hit->sha)) {
				/* Reconstruction is only trustworthy because it
				 * is checked; a mismatch leaves nothing behind. */
				kp_err("the reconstructed package does not "
				       "match the index — discarded");
				unlink(rebuilt);
				rebuilt[0] = 0;
			} else {
				pkgfile = kb_strdup(rebuilt);
				kp_msg("Reconstructed %s from the delta, "
				       "checksum good", hit->file);
			}
		}
		free(dpath);
		free(have);
	}

	if (!pkgfile) {
		pkgfile = kb_path_join(dir, hit->file);
		char sha[65] = "";
		if (kb_sha256_file(pkgfile, sha) != 0 || strcmp(sha, hit->sha)) {
			kp_err("%s: checksum does NOT match the index",
			       hit->file);
			free(pkgfile);
			return 2;
		}
	}
	kp_msg("%s-%s-%s: matches this machine, checksum good", hit->name,
	       hit->version, hit->release);

	if (dry) {
		free(pkgfile);
		return 0;
	}

	/* Installed through kpkgadd's own entry point rather than by exec'ing
	 * it: same process, same config, and no second copy of the install
	 * path to keep in step with this one. */
	char *av[2] = { (char *)"kpkgadd", pkgfile };
	int rc = add_main(2, av);
	free(pkgfile);
	return rc;
}
