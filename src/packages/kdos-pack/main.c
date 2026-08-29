/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-pack — build, inspect, sign and index packs
 * ---------------------------------
 *
 *     $ kdos-pack build ./diff meta.txt app.gimp.kpack
 *     app.gimp.kpack  92.0M  (image 91.9M, 3 requires, 1 desktop entry)
 *     $ kdos-pack info app.gimp.kpack
 *     $ kdos-pack index ./packs --sign builder.key
 *
 * IT SHIPS AS WELL AS RUNNING ON THE HOST, which is why it is in
 * `src/packages/` rather than beside kdos-portup in `src/tools/`:
 * `kdos-box freeze` builds a pack out of a box's writable layer on the machine
 * somebody is working on, and mkfs.erofs and zstd are both on the target.
 * `ports/appbox/packs` still compiles its own host copy on demand, the way
 * `ports/fetch` compiles kpkg — it links libkbase, libksig, libkpkg and
 * libkpack, all of which link nothing but libc, so that is a two-second `cc`.
 *
 * `mkfs.erofs` is EXEC'D rather than linked, the same shape kdos-appbox keeps
 * around `zstd`, and every argument goes through KbArgv — there is no
 * `system()` here and no shell anywhere near a directory name that came off a
 * container's diff layer.
 *
 * REPRODUCIBILITY IS A PROPERTY OF THIS FILE, not of whoever runs it. A pack
 * built twice from one tree must be byte-identical or a delta is meaningless
 * and a second builder can never be checked against the first. Four things
 * make it so and each is a real source of drift:
 *
 *   -T $SOURCE_DATE_EPOCH   otherwise every inode carries the second it was
 *                           packed. --all-time is mkfs.erofs's default and is
 *                           passed anyway, because a default is not a promise
 *   --force-uid/-gid=1000   the builder's own uid would otherwise ride along,
 *                           and 1000 is what presents as root inside a
 *                           rootless container
 *   -b 4096                 mkfs.erofs takes its block size from the PAGE
 *                           SIZE, so the same tree on a 16K-page machine is a
 *                           different image
 *   -U <derived>            a random UUID per build, otherwise. It is derived
 *                           from the pack's own identity, so two builds of one
 *                           pack agree and two different packs never collide
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kpack.h"
#include "kpkg.h"
#include "ksig.h"

static const char *keydir(void)
{
	const char *p = getenv("KDOS_KEYS");
	/* The PACK ring, not kpkg's binhost ring — see KD_KEYS in packd.h. */
	return p && *p ? p : "/etc/kdos/keys/packs";
}

/*
 * The image UUID, derived from the pack's identity rather than generated.
 * Version 4 bits are set so it is a well-formed UUID; the point is not
 * randomness, it is that the same pack is always the same sixteen bytes.
 *
 * THE VERSION IS DELIBERATELY NOT IN IT. This lands in the EROFS superblock,
 * so anything that goes in here is part of the image — and a version derived
 * from the bake's clock would make every rebuild a different image even when
 * not one file inside had moved. `kdos-pack imagehash` could then never answer
 * "unchanged", and a bake would rewrite all 47 packs, 7.2 GB, for nothing.
 *
 * The cost is that two VERSIONS of one pack share a UUID. Nothing here mounts
 * by UUID — kdos-packd mounts a path, through a loop device or the file
 * backend — so what is given up is the ability to tell two generations of the
 * same pack apart by superblock alone, which nothing asks for.
 */
static void derive_uuid(const KpkMeta *m, char out[37])
{
	char sub[256], hex[65];
	KbSha256 s;

	snprintf(sub, sizeof(sub), "kdos-pack-uuid-1\n%s\n", m->id);
	kb_sha256_init(&s);
	kb_sha256_update(&s, sub, strlen(sub));
	kb_sha256_final(&s, hex);
	snprintf(out, 37, "%.8s-%.4s-4%.3s-8%.3s-%.12s", hex, hex + 8, hex + 12,
		 hex + 16, hex + 20);
}

static const char *epoch(void)
{
	const char *e = getenv("SOURCE_DATE_EPOCH");
	/* The same pinned instant every phase env file already exports, so a
	 * pack built by the ISO build and one built by hand agree. */
	return e && *e ? e : "1735689600";
}

static int mkfs(const char *dir, const char *out, const KpkMeta *m)
{
	KbArgv a = {0};
	char uuid[37];

	derive_uuid(m, uuid);
	kb_argv_add(&a, "mkfs.erofs");
	kb_argv_add(&a, "-zzstd");
	kb_argv_add(&a, "-b");
	kb_argv_add(&a, "4096");
	kb_argv_add(&a, "-T");
	kb_argv_add(&a, epoch());
	kb_argv_add(&a, "--all-time");
	kb_argv_add(&a, "--force-uid=1000");
	kb_argv_add(&a, "--force-gid=1000");
	kb_argv_addf(&a, "-U");
	kb_argv_add(&a, uuid);
	kb_argv_add(&a, out);
	kb_argv_add(&a, dir);
	kb_argv_end(&a);
	return kb_run(&a);
}

/* ── build ─────────────────────────────────────────────────────────────── */

/*
 * `build` runs mkfs.erofs and then wraps; `assemble` wraps an image somebody
 * else already made. The split is not a convenience: mkfs.erofs must run as
 * ROOT to preserve overlay whiteouts and trusted.overlay.* xattrs, while
 * wrapping is an ordinary file operation — so the bake makes images under the
 * privilege it needs and wraps them under the privilege it does not.
 */
static int build_or_assemble(int argc, char **argv, int have_image)
{
	const char *dir = NULL, *metapath = NULL, *out = NULL, *iconpath = NULL;
	char err[256];
	KpkMeta m;
	size_t mlen = 0, iconlen = 0;
	char *mtext, *icon = NULL;
	char tmp[KPK_PATH * 2];
	struct stat st;
	int rc;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--icon") && i + 1 < argc)
			iconpath = argv[++i];
		else if (!dir)
			dir = argv[i];
		else if (!metapath)
			metapath = argv[i];
		else if (!out)
			out = argv[i];
	}
	if (!dir || !metapath || !out) {
		fprintf(stderr, have_image
			? "usage: kdos-pack assemble <image> <meta> <out.kpack> "
			  "[--icon FILE]\n"
			: "usage: kdos-pack build <dir> <meta> <out.kpack> "
			  "[--icon FILE]\n");
		return 2;
	}

	mtext = kb_read_all(metapath, &mlen);
	if (!mtext)
		kb_die("cannot read %s", metapath);
	kpk_meta_parse(mtext, mlen, &m);
	free(mtext);
	if (kpk_meta_valid(&m, err, sizeof(err)) != 0)
		kb_die("%s: %s", metapath, err);

	if (have_image) {
		if (kb_is_dir(dir) || !kb_path_exists(dir))
			kb_die("%s is not an image file", dir);
	} else if (!kb_is_dir(dir)) {
		kb_die("%s is not a directory", dir);
	}

	if (iconpath) {
		icon = kb_read_all(iconpath, &iconlen);
		if (!icon)
			kb_die("cannot read %s", iconpath);
		/*
		 * A pack's icon is drawn by a launcher, a taskbar and a Start
		 * menu row. Refusing a non-PNG here rather than at draw time is
		 * the genatlas lesson: an unusable picture still takes the
		 * slot, and the caller never falls back to its glyph.
		 */
		if (iconlen < 8 || memcmp(icon, "\x89PNG\r\n\x1a\n", 8) != 0)
			kb_die("%s is not a PNG", iconpath);
	}

	if (have_image) {
		kb_strlcpy(tmp, dir, sizeof(tmp));
	} else {
		snprintf(tmp, sizeof(tmp), "%s.erofs.tmp", out);
		if (mkfs(dir, tmp, &m) != 0) {
			unlink(tmp);
			kb_die("mkfs.erofs failed");
		}
	}

	rc = kpk_write(out, tmp, &m, icon, iconlen);
	if (!have_image)
		unlink(tmp);
	free(icon);
	if (rc != 0)
		kb_die("cannot write %s", out);

	if (stat(out, &st) == 0)
		printf("%s  %s  (%s, %d requires, %d desktop)\n", out,
		       kb_human_size((unsigned long long)st.st_size),
		       kpk_kind_name(m.kind), m.nreq, m.ndesktop);
	return 0;
}

/* ── info / extract-meta ───────────────────────────────────────────────── */

static void print_list(const char *label, const char list[][128], int n)
{
	for (int i = 0; i < n; i++)
		printf("%-12s%s\n", i ? "" : label, list[i]);
}

static int cmd_info(int argc, char **argv)
{
	KpkPack p;
	KsigRing ring = {0};
	char who[KSIG_ID_HEX];
	KpkSigState st;

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-pack info <pack>\n");
		return 2;
	}
	if (kpk_open(argv[0], &p) != 0)
		kb_die("%s is not a pack", argv[0]);

	printf("id          %s\n", p.meta.id);
	printf("kind        %s\n", kpk_kind_name(p.meta.kind));
	if (p.meta.name[0])
		printf("name        %s\n", p.meta.name);
	printf("version     %s-%s\n", p.meta.version, p.meta.release);
	printf("arch        %s\n", p.meta.arch);
	if (p.meta.summary[0])
		printf("summary     %s\n", p.meta.summary);
	if (p.meta.category[0])
		printf("category    %s\n", p.meta.category);
	if (p.meta.licence[0])
		printf("licence     %s\n", p.meta.licence);
	printf("size        %s\n", kb_human_size(p.fsize));
	printf("image       %s\n", kb_human_size(p.foot.erofs_len));
	if (p.meta.installed)
		printf("installed   %s\n", kb_human_size(p.meta.installed));
	if (p.meta.launch_cold)
		printf("cold launch %d ms\n", p.meta.launch_cold);
	for (int i = 0; i < p.meta.nreq; i++)
		printf("%-12s%s%s%s%s%s\n", i ? "" : "requires",
		       p.meta.req[i].name, p.meta.req[i].op[0] ? " " : "",
		       p.meta.req[i].op, p.meta.req[i].op[0] ? " " : "",
		       p.meta.req[i].ver);
	for (int i = 0; i < p.meta.nprov; i++)
		printf("%-12s%s\n", i ? "" : "provides", p.meta.provides[i]);
	print_list("desktop", p.meta.desktop, p.meta.ndesktop);
	print_list("mime", p.meta.mime, p.meta.nmime);
	for (int i = 0; i < p.meta.ncmd; i++)
		printf("%-12s%s\n", i ? "" : "command", p.meta.command[i]);
	for (int i = 0; i < p.meta.nneeds; i++)
		printf("%-12s%s\n", i ? "" : "needs", p.meta.needs[i]);
	for (int i = 0; i < p.meta.ngraft; i++)
		printf("%-12s%s -> /usr/share/%s\n", i ? "" : "graft",
		       p.meta.graft[i].from, p.meta.graft[i].to);
	for (int i = 0; i < p.meta.nboxgraft; i++)
		printf("%-12s%s -> %s\n", i ? "" : "boxgraft",
		       p.meta.boxgraft[i].from, p.meta.boxgraft[i].to);
	for (int i = 0; i < p.meta.nenv; i++)
		printf("%-12s%s\n", i ? "" : "env", p.meta.env[i]);
	printf("icon        %s\n", p.foot.icon_len ? "yes" : "none");

	ksig_ring_load(&ring, keydir());
	st = kpk_verify(&p, &ring, who);
	printf("signature   %s%s%s\n", kpk_sig_state_name(st),
	       who[0] ? " by " : "", who);
	if (p.meta.description[0])
		printf("\n%s\n", p.meta.description);
	return 0;
}

static int cmd_extract_meta(int argc, char **argv)
{
	KpkPack p;
	int want_icon = 0;

	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], "--icon"))
			want_icon = 1;
	if (argc < 1) {
		fprintf(stderr, "usage: kdos-pack extract-meta <pack> [--icon]\n");
		return 2;
	}
	if (kpk_open(argv[0], &p) != 0)
		kb_die("%s is not a pack", argv[0]);

	if (want_icon) {
		size_t n = 0;
		void *icon = kpk_icon_read(&p, &n);
		if (!icon)
			return 1;
		fwrite(icon, 1, n, stdout);
		free(icon);
		return 0;
	}
	size_t len = 0;
	char *text = kpk_meta_render(&p.meta, &len);
	fwrite(text, 1, len, stdout);
	free(text);
	return 0;
}

/* ── verify / sign ─────────────────────────────────────────────────────── */

static int cmd_verify(int argc, char **argv)
{
	KpkPack p;
	KsigRing ring = {0};
	char who[KSIG_ID_HEX];
	const char *kd = keydir();
	KpkSigState st;

	for (int i = 0; i < argc; i++)
		if (!strcmp(argv[i], "--keys") && i + 1 < argc)
			kd = argv[++i];
	if (argc < 1) {
		fprintf(stderr, "usage: kdos-pack verify <pack> [--keys DIR]\n");
		return 2;
	}
	if (kpk_open(argv[0], &p) != 0) {
		fprintf(stderr, "%s: not a pack\n", argv[0]);
		return 2;
	}
	ksig_ring_load(&ring, kd);
	st = kpk_verify(&p, &ring, who);
	switch (st) {
	case KPK_SIG_GOOD:
		printf("%s: signed by %s\n", p.meta.id, who);
		return 0;
	case KPK_SIG_NONE:
		/*
		 * Exit 0. The packs on a medium you booted are signed by
		 * nobody, and a verifier that failed on them would be a
		 * verifier nobody could run. KDOS_REQUIRE_SIG=1 is how a
		 * machine that installs only from a repository says so.
		 */
		if (getenv("KDOS_REQUIRE_SIG")) {
			fprintf(stderr, "%s: unsigned, and KDOS_REQUIRE_SIG is set\n",
				p.meta.id);
			return 2;
		}
		printf("%s: unsigned\n", p.meta.id);
		return 0;
	default:
		fprintf(stderr, "%s: %s\n", p.meta.id, kpk_sig_state_name(st));
		return 2;
	}
}

static int cmd_sign(int argc, char **argv)
{
	uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];

	if (argc < 2) {
		fprintf(stderr, "usage: kdos-pack sign <pack> <key.sec>\n");
		return 2;
	}
	if (ksig_read_secret(argv[1], seed, pub) != 0)
		kb_die("cannot read the signing key %s", argv[1]);
	if (kpk_sign(argv[0], seed, pub) != 0)
		kb_die("cannot sign %s", argv[0]);
	printf("%s signed\n", argv[0]);
	return 0;
}

/* ── index ─────────────────────────────────────────────────────────────── */

static int has_suffix(const char *s, const char *suf)
{
	size_t a = strlen(s), b = strlen(suf);
	return a > b && !strcmp(s + a - b, suf);
}

static int cmd_index(int argc, char **argv)
{
	const char *dir = NULL, *key = NULL;
	KpkIndex ix = {0};
	char **names;
	int nn = 0;
	char out[KPK_PATH * 2 + 16];

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--sign") && i + 1 < argc)
			key = argv[++i];
		else if (!dir)
			dir = argv[i];
	}
	if (!dir) {
		fprintf(stderr, "usage: kdos-pack index <dir> [--sign key.sec]\n");
		return 2;
	}

	names = kb_listdir(dir, &nn);
	for (char **n = names; n && *n; n++) {
		char *path = kb_path_join(dir, *n);
		struct stat st;
		KpkIndexEnt *e;

		if (!has_suffix(*n, ".kpack") && !has_suffix(*n, ".kdelta")) {
			free(path);
			continue;
		}
		if (ix.n >= KPK_INDEX_MAX) {
			ix.truncated = 1;
			free(path);
			continue;
		}
		e = &ix.ent[ix.n];
		memset(e, 0, sizeof(*e));

		if (has_suffix(*n, ".kdelta")) {
			/*
			 * A delta is an ordinary stanza with O:, because "it
			 * names what it patches" already says everything a type
			 * field would. Its id and version are the ones it
			 * RECONSTRUCTS, read off the target pack rather than
			 * off the filename — a client looking for app.gimp
			 * 3.0.4 must find both routes to it in one lookup, and
			 * a filename stem is not an id.
			 */
			char base[KPK_PATH], target[KPK_PATH * 2];
			char *dd;
			KpkPack tp;

			kb_strlcpy(base, *n, sizeof(base));
			base[strlen(base) - 7] = 0;
			dd = strstr(base, "--from--");
			if (!dd) {
				fprintf(stderr, "%s: a delta is named "
						"<new>--from--<old>.kdelta\n", *n);
				free(path);
				continue;
			}
			*dd = 0;
			snprintf(e->from, sizeof(e->from), "%s.kpack", dd + 8);
			snprintf(target, sizeof(target), "%s/%s.kpack", dir, base);
			if (kpk_open(target, &tp) != 0) {
				/* Without the pack it rebuilds there is nothing
				 * to check the result against, so the delta is
				 * unusable rather than merely unidentified. */
				fprintf(stderr, "%s: %s.kpack is not here, skipped\n",
					*n, base);
				free(path);
				continue;
			}
			kb_strlcpy(e->id, tp.meta.id, sizeof(e->id));
			kb_strlcpy(e->version, tp.meta.version, sizeof(e->version));
			kb_strlcpy(e->release, tp.meta.release, sizeof(e->release));
			kb_strlcpy(e->arch, tp.meta.arch, sizeof(e->arch));
			kb_strlcpy(e->kind, kpk_kind_name(tp.meta.kind),
				   sizeof(e->kind));
		} else {
			KpkPack p;
			if (kpk_open(path, &p) != 0) {
				fprintf(stderr, "%s: not a pack, skipped\n", *n);
				free(path);
				continue;
			}
			kb_strlcpy(e->id, p.meta.id, sizeof(e->id));
			kb_strlcpy(e->version, p.meta.version, sizeof(e->version));
			kb_strlcpy(e->release, p.meta.release, sizeof(e->release));
			kb_strlcpy(e->arch, p.meta.arch, sizeof(e->arch));
			kb_strlcpy(e->kind, kpk_kind_name(p.meta.kind),
				   sizeof(e->kind));
			e->recommended = p.meta.recommended;
			kb_strlcpy(e->name, p.meta.name, sizeof(e->name));
			kb_strlcpy(e->category, p.meta.category,
				   sizeof(e->category));
			/* Names only: the index is read by programs that
			 * cannot evaluate a version constraint. */
			{
				size_t o = 0;
				e->requires[0] = '\0';
				for (int r = 0; r < p.meta.nreq; r++) {
					int w = snprintf(e->requires + o,
							 sizeof(e->requires) - o,
							 "%s%s", o ? " " : "",
							 p.meta.req[r].name);
					if (w < 0 ||
					    (size_t)w >= sizeof(e->requires) - o)
						break;
					o += (size_t)w;
				}
			}
			kb_strlcpy(e->summary, p.meta.summary,
				   sizeof(e->summary));
		}
		kb_strlcpy(e->file, *n, sizeof(e->file));
		if (stat(path, &st) == 0)
			e->size = (unsigned long long)st.st_size;
		if (kb_sha256_file(path, e->sha256) != 0) {
			fprintf(stderr, "%s: cannot hash, skipped\n", *n);
			free(path);
			continue;
		}
		ix.n++;
		free(path);
	}
	kb_strv_free(names);

	snprintf(out, sizeof(out), "%s/PACKAGES", dir);
	if (kpk_index_write(&ix, out) != 0)
		kb_die("cannot write %s", out);
	if (ix.truncated)
		kb_warn("more than %d packs in %s — the rest are not indexed",
			KPK_INDEX_MAX, dir);

	if (key) {
		uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
		char sig[KPK_PATH * 2 + 32];
		size_t len = 0;
		char *text;

		if (ksig_read_secret(key, seed, pub) != 0)
			kb_die("cannot read the signing key %s", key);
		text = kb_read_all(out, &len);
		if (!text)
			kb_die("cannot re-read %s", out);
		snprintf(sig, sizeof(sig), "%s.sig", out);
		/*
		 * ONE signature over the index covers every pack in it, because
		 * the index carries each pack's hash. Signing 200 packs
		 * individually would be 200 chances for one to be missed.
		 */
		unlink(sig);
		if (ksig_sig_append(sig, seed, pub, text, len) != 0)
			kb_die("cannot write %s", sig);
		free(text);
		printf("%s  %d entries, signed\n", out, ix.n);
	} else {
		printf("%s  %d entries\n", out, ix.n);
	}
	return 0;
}

/* ── delta ─────────────────────────────────────────────────────────────── */

static int cmd_delta(int argc, char **argv)
{
	char out[KPK_PATH * 2];
	KbArgv a = {0};
	struct stat so, sn, sd;

	if (argc < 2) {
		fprintf(stderr, "usage: kdos-pack delta <old.kpack> <new.kpack> "
				"[out.kdelta]\n");
		return 2;
	}
	if (argc > 2)
		kb_strlcpy(out, argv[2], sizeof(out));
	else {
		/*
		 * `<new>--from--<old>.kdelta`, which is what `index` parses
		 * back. The name IS the metadata, so a delta that travels
		 * without its index still says what it patches.
		 */
		char nb[KPK_PATH], ob[KPK_PATH];
		kb_strlcpy(nb, kb_basename(argv[1]), sizeof(nb));
		kb_strlcpy(ob, kb_basename(argv[0]), sizeof(ob));
		if (has_suffix(nb, ".kpack"))
			nb[strlen(nb) - 6] = 0;
		if (has_suffix(ob, ".kpack"))
			ob[strlen(ob) - 6] = 0;
		snprintf(out, sizeof(out), "%s--from--%s.kdelta", nb, ob);
	}

	/*
	 * A pack is already compressed, so a delta over two of them only works
	 * because EROFS compresses per-cluster: blocks whose contents did not
	 * change come out as identical compressed bytes. That is a property
	 * worth MEASURING rather than claiming, which is why the ratio is
	 * printed with every delta.
	 */
	kb_argv_add(&a, "zstd");
	kb_argv_add(&a, "-19");
	kb_argv_add(&a, "-q");
	kb_argv_add(&a, "--force");
	kb_argv_addf(&a, "--patch-from=%s", argv[0]);
	kb_argv_add(&a, argv[1]);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		kb_die("zstd --patch-from failed");

	if (stat(argv[0], &so) == 0 && stat(argv[1], &sn) == 0 &&
	    stat(out, &sd) == 0)
		printf("%s: %lld bytes, %.1fx smaller than the pack (%lld bytes)\n",
		       out, (long long)sd.st_size,
		       sd.st_size ? (double)sn.st_size / (double)sd.st_size : 0.0,
		       (long long)sn.st_size);
	return 0;
}

/* ── apply ─────────────────────────────────────────────────────────────── */

static int cmd_apply(int argc, char **argv)
{
	KbArgv a = {0};

	if (argc < 3) {
		fprintf(stderr, "usage: kdos-pack apply <old.kpack> <delta> "
				"<out.kpack>\n");
		return 2;
	}
	kb_argv_add(&a, "zstd");
	kb_argv_add(&a, "-d");
	kb_argv_add(&a, "-q");
	kb_argv_add(&a, "--force");
	kb_argv_addf(&a, "--patch-from=%s", argv[0]);
	kb_argv_add(&a, argv[1]);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, argv[2]);
	kb_argv_end(&a);
	if (kb_run(&a) != 0)
		kb_die("zstd could not apply the delta");
	/*
	 * A DELTA IS NEVER TRUSTED AND NEVER NEEDS TO BE. What comes out is
	 * hashed against the C: the signed index already carries; a tampered
	 * delta produces a pack whose hash does not match and is discarded. So
	 * there is no delta signature and no second trust path.
	 */
	printf("%s written — check it against the index's C: before using it\n",
	       argv[2]);
	return 0;
}

/* ── keys ──────────────────────────────────────────────────────────────── */

static int cmd_keygen(int argc, char **argv)
{
	uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
	char sec[KPK_PATH], pubp[KPK_PATH], id[KSIG_ID_HEX];

	if (argc < 1) {
		fprintf(stderr, "usage: kdos-pack keygen <name>\n");
		return 2;
	}
	if (ksig_keygen(seed, pub) != 0)
		kb_die("the system would not give randomness");
	snprintf(sec, sizeof(sec), "%s.key", argv[0]);
	snprintf(pubp, sizeof(pubp), "%s.pub", argv[0]);
	if (ksig_write_secret(sec, seed, pub) != 0)
		kb_die("cannot write %s", sec);
	if (ksig_write_public(pubp, pub, argv[0]) != 0)
		kb_die("cannot write %s", pubp);
	ksig_keyid(pub, id);
	printf("%s  %s\n%s  (0600)\n", pubp, id, sec);
	return 0;
}

static int usage(void);

/*
 * The sha256 of the EROFS extent alone — the filesystem, without the metadata
 * blob, the icon, the signature or the footer.
 *
 * IT ANSWERS "DID THE CONTENTS CHANGE", WHICH THE FOOTER'S OWN HASH CANNOT.
 * That one covers the metadata too, and the metadata carries the version — so
 * two bakes of an identical filesystem always differ by it, and a bake would
 * rewrite every pack in the set whether or not a single file inside had moved.
 * At 7.2 GB a set, published as release assets or committed, that is the
 * difference between a re-bake costing nothing and costing the whole archive.
 */
static int cmd_imagehash(int argc, char **argv)
{
	KpkFooter f;
	char hex[65];
	KbSha256 sh;
	unsigned char buf[65536];
	uint64_t left;
	FILE *fp;

	if (argc < 1)
		return usage();
	if (kpk_footer_read(argv[0], &f, NULL) != 0) {
		kb_warn("%s: no pack footer here", argv[0]);
		return 1;
	}
	if (!(fp = fopen(argv[0], "rb"))) {
		kb_warn("%s: cannot open", argv[0]);
		return 1;
	}
	kb_sha256_init(&sh);
	for (left = f.erofs_len; left; ) {
		size_t want = left < sizeof(buf) ? (size_t)left : sizeof(buf);
		size_t got = fread(buf, 1, want, fp);
		if (!got)
			break;
		kb_sha256_update(&sh, buf, got);
		left -= got;
	}
	fclose(fp);
	if (left) {
		kb_warn("%s: short read — the image extent is not all there",
			argv[0]);
		return 1;
	}
	kb_sha256_final(&sh, hex);
	printf("%s\n", hex);
	return 0;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-pack build <dir> <meta> <out.kpack> [--icon FILE]\n"
		"       kdos-pack assemble <image> <meta> <out.kpack> [--icon FILE]\n"
		"       kdos-pack info <pack>\n"
	      "       kdos-pack imagehash <pack>\n"
		"       kdos-pack extract-meta <pack> [--icon]\n"
		"       kdos-pack verify <pack> [--keys DIR]\n"
		"       kdos-pack sign <pack> <key.sec>\n"
		"       kdos-pack index <dir> [--sign key.sec]\n"
		"       kdos-pack delta <old> <new> [out.kdelta]\n"
		"       kdos-pack apply <old> <delta> <out.kpack>\n"
		"       kdos-pack keygen <name>\n");
	return 2;
}

/* `uuid <id>` prints what mkfs() would pass to -U for a pack of that id.
 *
 * It exists because 01_packs.sh builds the KDOS base pack with mkfs.erofs
 * DIRECTLY — `assemble` wraps an image somebody else made, and the excludes
 * that keep the repository bind mounts out of it are that script's, not this
 * program's. Everything else in mkfs()'s flag set is a literal a script can
 * repeat; the UUID is a derivation, and a second copy of it in shell would be
 * a second thing to drift from `derive_uuid`.
 */
static int cmd_uuid(int n, char **a)
{
	KpkMeta m = {0};
	char uuid[37];

	if (n < 1) {
		fprintf(stderr, "usage: kdos-pack uuid <id>\n");
		return 2;
	}
	snprintf(m.id, sizeof(m.id), "%s", a[0]);
	derive_uuid(&m, uuid);
	printf("%s\n", uuid);
	return 0;
}

/*
 * `image <pack> <out.erofs>` writes the pack's EROFS bytes and nothing else —
 * [0, erofs_len) as the footer records it. It exists for the one place that
 * has to read INSIDE a pack without a mount: the build, which is a chroot in an
 * unprivileged container and cannot mount anything, and which needs the
 * desktop entries out of the recommended packs to write the ISO's launchers.
 * `fsck.erofs --extract` then reads a plain image, which the .kpack with its
 * footer is not.
 */
static int cmd_image(int n, char **a)
{
	KpkFooter f;
	uint64_t fsize;
	FILE *in, *out;
	char buf[1 << 16];
	uint64_t left;

	if (n < 2) {
		fprintf(stderr, "usage: kdos-pack image <pack> <out.erofs>\n");
		return 2;
	}
	if (kpk_footer_read(a[0], &f, &fsize) != 0)
		kb_die("%s is not a pack", a[0]);
	in = fopen(a[0], "rb");
	out = fopen(a[1], "wb");
	if (!in || !out)
		kb_die("%s: %s", in ? a[1] : a[0], strerror(errno));
	for (left = f.erofs_len; left; ) {
		size_t want = left < sizeof(buf) ? (size_t)left : sizeof(buf);
		size_t got = fread(buf, 1, want, in);
		if (!got || fwrite(buf, 1, got, out) != got)
			kb_die("%s: short copy", a[1]);
		left -= got;
	}
	fclose(in);
	if (fclose(out) != 0)
		kb_die("%s: %s", a[1], strerror(errno));
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-pack");
	if (argc < 2)
		return usage();

	const char *cmd = argv[1];
	int n = argc - 2;
	char **a = argv + 2;

	if (!strcmp(cmd, "build"))		return build_or_assemble(n, a, 0);
	if (!strcmp(cmd, "assemble"))		return build_or_assemble(n, a, 1);
	if (!strcmp(cmd, "info"))		return cmd_info(n, a);
	if (!strcmp(cmd, "imagehash"))		return cmd_imagehash(n, a);
	if (!strcmp(cmd, "uuid"))		return cmd_uuid(n, a);
	if (!strcmp(cmd, "image"))		return cmd_image(n, a);
	if (!strcmp(cmd, "extract-meta"))	return cmd_extract_meta(n, a);
	if (!strcmp(cmd, "verify"))		return cmd_verify(n, a);
	if (!strcmp(cmd, "sign"))		return cmd_sign(n, a);
	if (!strcmp(cmd, "index"))		return cmd_index(n, a);
	if (!strcmp(cmd, "delta"))		return cmd_delta(n, a);
	if (!strcmp(cmd, "apply"))		return cmd_apply(n, a);
	if (!strcmp(cmd, "keygen"))		return cmd_keygen(n, a);
	return usage();
}
