/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the pack store — what this machine can see, and what it has
 * ---------------------------------
 *
 * Two directories. `/var/lib/kdos/packs` is what is INSTALLED, and on a live
 * session `/mnt/iso/packs` is what is AVAILABLE — readable the moment the
 * image is up, costing nothing until mounted, which is the whole reason the
 * packs sit on ISO9660 beside `system.sfs` rather than inside it.
 *
 * INSTALLING IS A RENAME. A pack is verified in the staging directory and then
 * moved into the store; there is no unpack step to interrupt and no half-
 * installed state to recover from. The previous version is not deleted — it is
 * what `rollback` renames back, and per-app rollback is the property this
 * whole format exists for.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kpkg.h"
#include "ksig.h"
#include "packd.h"

KdPack kd_pack[KD_PACKS];
int kd_npack;

static char store_buf[KD_PATH];
static char medium_buf[KD_PATH];

const char *kd_store_dir(void)
{
	const char *p = getenv("KDOS_PACK_STORE");
	if (p && *p)
		return p;
	kb_strlcpy(store_buf, KD_STORE, sizeof(store_buf));
	return store_buf;
}

const char *kd_medium_dir(void)
{
	const char *p = getenv("KDOS_PACK_MEDIUM");
	if (p && *p)
		return p;
	kb_strlcpy(medium_buf, KD_ISO, sizeof(medium_buf));
	return medium_buf;
}

const char *kd_mnt_dir(void)
{
	static char b[KD_PATH];
	snprintf(b, sizeof(b), "%.480s/mnt", kd_store_dir());
	return b;
}

const char *kd_manifest(void)
{
	const char *p = getenv("KDOS_PACK_MANIFEST");
	return p && *p ? p : KD_MANIFEST;
}

const char *kd_staging_dir(void)
{
	static char b[KD_PATH];
	snprintf(b, sizeof(b), "%.480s/staging", kd_store_dir());
	return b;
}

/*
 * HOW MANY SUPERSEDED VERSIONS THE STORE KEEPS, and it is a setting because
 * the two right answers are different machines. `rollback` is a rename of a
 * file that is still there, so a store that kept none could not roll anything
 * back; a store that kept every version an application ever had would fill a
 * disk with copies of a 400 MB pack nobody will launch again.
 *
 * One is the default: the update you just took, undone. `retain = 0` is an
 * honest off and says so — rollback then answers "no earlier version is kept"
 * rather than failing at a rename.
 */
int kd_retain(void)
{
	static int cached = -1;
	const char *env = getenv("KDOS_PACK_RETAIN");
	char buf[4096];

	if (env && *env)
		return atoi(env);
	if (cached >= 0)
		return cached;
	cached = 1;
	/* `> 0`, not `== 0`: kb_read_file answers the BYTE COUNT, so `== 0`
	 * parses the file only when it is empty — `retain =` was read from a
	 * zero-byte packd.conf and from nothing else, and rollback silently
	 * kept the default on every machine that had configured it. */
	if (kb_read_file("/etc/kdos/packd.conf", buf, sizeof(buf)) > 0) {
		char *line, *save;
		for (line = strtok_r(buf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *eq = strchr(line, '=');
			if (!eq || strncmp(line, "retain", 6))
				continue;
			cached = atoi(eq + 1);
			if (cached < 0)
				cached = 0;
			break;
		}
	}
	return cached;
}

/*
 * Drop superseded copies of one id down to `kd_retain()`, newest kept.
 * Run after an install, which is the only moment a new one appears — a sweep
 * on a timer would be a background job deleting somebody's rollback while
 * they were deciding whether to use it.
 */
static void kd_sweep(const char *id)
{
	char **names = kb_listdir(kd_store_dir(), NULL);
	char keep[8][96];
	int nkeep = 0, retain = kd_retain();
	size_t idlen = strlen(id);

	if (retain > 8)
		retain = 8;
	if (!names)
		return;

	/* The newest `retain` versions, by kp_vercmp so `1.10` beats `1.9`. */
	for (char **nm = names; *nm; nm++) {
		char ver[96];
		size_t l = strlen(*nm);

		if (l < idlen + 8 || strcmp(*nm + l - 6, ".kpack"))
			continue;
		if (strncmp(*nm, id, idlen) || (*nm)[idlen] != '-')
			continue;
		if (l - 6 - idlen - 1 >= sizeof(ver))
			continue;
		kb_strlcpy(ver, *nm + idlen + 1, l - 6 - idlen);
		if (nkeep < retain) {
			kb_strlcpy(keep[nkeep++], ver, sizeof(keep[0]));
		} else if (retain > 0) {
			int worst = 0;
			for (int i = 1; i < nkeep; i++)
				if (kp_vercmp(keep[i], keep[worst]) < 0)
					worst = i;
			if (kp_vercmp(ver, keep[worst]) > 0)
				kb_strlcpy(keep[worst], ver, sizeof(keep[0]));
		}
	}

	for (char **nm = names; *nm; nm++) {
		char path[KD_PATH * 2], ver[96];
		size_t l = strlen(*nm);
		int kept = 0;

		if (l < idlen + 8 || strcmp(*nm + l - 6, ".kpack"))
			continue;
		if (strncmp(*nm, id, idlen) || (*nm)[idlen] != '-')
			continue;
		if (l - 6 - idlen - 1 >= sizeof(ver))
			continue;
		kb_strlcpy(ver, *nm + idlen + 1, l - 6 - idlen);
		for (int i = 0; i < nkeep; i++)
			if (!strcmp(keep[i], ver))
				kept = 1;
		if (kept)
			continue;
		snprintf(path, sizeof(path), "%s/%s", kd_store_dir(), *nm);
		if (unlink(path) == 0)
			kd_log("%s: aged out %s", id, ver);
	}
	kb_strv_free(names);
}

int kd_id_ok(const char *s)
{
	if (!s || !*s || strlen(s) >= KPK_ID_MAX)
		return 0;
	if (*s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-')
			return 0;
	return 1;
}

static int has_suffix(const char *s, const char *suf)
{
	size_t a = strlen(s), b = strlen(suf);
	return a > b && !strcmp(s + a - b, suf);
}

static void scan_dir(const char *dir, KdOrigin origin)
{
	char **names = kb_listdir(dir, NULL);

	for (char **n = names; n && *n; n++) {
		char *path;
		KpkPack p;
		struct stat st;
		int at;

		if (!has_suffix(*n, ".kpack"))
			continue;
		if (kd_npack >= KD_PACKS)
			break;
		path = kb_path_join(dir, *n);
		if (kpk_open(path, &p) != 0) {
			/* Not a pack, or a pack this build cannot read. Either
			 * way it is absent rather than half-listed. */
			free(path);
			continue;
		}

		/*
		 * An id already in the table wins if it is INSTALLED: the store
		 * shadows the medium, so a pack the user updated is the one
		 * that runs even while the older one is still on the stick.
		 * A second copy in the same directory is the older version kept
		 * for rollback and is not a row of its own.
		 */
		at = kd_find(p.meta.id);
		if (at >= 0) {
			if (kd_pack[at].origin == KD_ORIGIN_STORE) {
				free(path);
				continue;
			}
			if (origin != KD_ORIGIN_STORE) {
				free(path);
				continue;
			}
		} else {
			at = kd_npack++;
			memset(&kd_pack[at], 0, sizeof(kd_pack[at]));
			kd_pack[at].loop = -1;
		}
		kd_pack[at].meta = p.meta;
		kd_pack[at].origin = origin;
		kb_strlcpy(kd_pack[at].file, path, sizeof(kd_pack[at].file));
		kd_pack[at].size = stat(path, &st) == 0
					   ? (unsigned long long)st.st_size
					   : 0;
		free(path);
	}
	kb_strv_free(names);
}

void kd_scan(void)
{
	/*
	 * The mount state and the refcounts survive a rescan; only the
	 * catalogue is re-read. Losing a refcount here would let `unmount`
	 * pull a running box's lowerdir out from under it.
	 *
	 * What is carried over is FOUR FIELDS, not a second copy of the
	 * catalogue: a KpkMeta is tens of kilobytes and an array of them is
	 * not something a function puts on its stack.
	 */
	static struct kd_keep {
		char id[KPK_ID_MAX];
		char mnt[KD_PATH];
		int refs, loop;
	} keep[KD_PACKS];
	int nkeep = kd_npack;

	for (int i = 0; i < nkeep; i++) {
		kb_strlcpy(keep[i].id, kd_pack[i].meta.id, sizeof(keep[i].id));
		kb_strlcpy(keep[i].mnt, kd_pack[i].mnt, sizeof(keep[i].mnt));
		keep[i].refs = kd_pack[i].refs;
		keep[i].loop = kd_pack[i].loop;
	}
	kd_npack = 0;
	memset(kd_pack, 0, sizeof(kd_pack));

	scan_dir(kd_store_dir(), KD_ORIGIN_STORE);
	scan_dir(kd_medium_dir(), KD_ORIGIN_MEDIUM);

	for (int i = 0; i < kd_npack; i++) {
		kd_pack[i].loop = -1;
		for (int k = 0; k < nkeep; k++)
			if (!strcmp(keep[k].id, kd_pack[i].meta.id)) {
				kb_strlcpy(kd_pack[i].mnt, keep[k].mnt,
					   sizeof(kd_pack[i].mnt));
				kd_pack[i].refs = keep[k].refs;
				kd_pack[i].loop = keep[k].loop;
				break;
			}
	}
}

int kd_find(const char *id)
{
	for (int i = 0; i < kd_npack; i++)
		if (!strcmp(kd_pack[i].meta.id, id))
			return i;
	return -1;
}

/* ── install ───────────────────────────────────────────────────────────── */

/*
 * `staged` is a FILENAME in the staging directory and nothing else — not a
 * path, not something with a slash in it. That is rule 3: a daemon reachable
 * from `wheel` that takes a path is `mount /dev/sda2 /etc` from any shell, and
 * the staging directory is the one place an unprivileged download may land.
 */
int kd_install(const char *staged, char *msg, size_t n)
{
	char src[KD_PATH * 2], dst[KD_PATH * 2], old[KD_PATH * 2];
	KpkPack p;
	KsigRing ring = {0};
	char who[KSIG_ID_HEX], err[256];
	KpkSigState sig;
	KpkIndex ix = {0};
	char idxpath[KD_PATH * 2];

	if (strchr(staged, '/') || !strcmp(staged, "..") || !*staged) {
		snprintf(msg, n, "a staged name is a filename, not a path");
		return -1;
	}
	for (const char *c = staged; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-') {
			snprintf(msg, n, "a staged name is [A-Za-z0-9._-]");
			return -1;
		}
	snprintf(src, sizeof(src), "%s/%s", kd_staging_dir(), staged);
	if (!kb_path_exists(src)) {
		snprintf(msg, n, "%s is not in the staging directory", staged);
		return -1;
	}
	if (kpk_open(src, &p) != 0) {
		snprintf(msg, n, "%s is not a pack", staged);
		return -1;
	}
	if (kpk_meta_valid(&p.meta, err, sizeof(err)) != 0) {
		snprintf(msg, n, "%s", err);
		return -1;
	}

	/*
	 * VERIFICATION HAPPENS WHERE THE MOUNT HAPPENS. A client that verified
	 * and then asked this daemon to install has verified nothing — the same
	 * rule kpkg keeps about checking an index before believing it.
	 *
	 * The index, when there is one, is the stronger claim: it is signed
	 * once and carries every pack's hash, so a pack matching the index is
	 * covered transitively. The pack's own block is for the pack that
	 * travels on a stick with no index beside it.
	 */
	ksig_ring_load(&ring, KD_KEYS);
	snprintf(idxpath, sizeof(idxpath), "%s/PACKAGES", kd_staging_dir());
	if (kb_path_exists(idxpath) &&
	    kpk_index_verify(idxpath, &ring, who) == KPK_SIG_GOOD &&
	    kpk_index_load(&ix, idxpath) > 0) {
		const KpkIndexEnt *e = kpk_index_find(&ix, p.meta.id);
		char have[65];
		if (e && kb_sha256_file(src, have) == 0 &&
		    !strcmp(have, e->sha256)) {
			kd_log("%s: matches the index signed by %s", p.meta.id, who);
			goto accepted;
		}
		if (e) {
			snprintf(msg, n, "%s does not match the signed index",
				 p.meta.id);
			return -1;
		}
	}

	sig = kpk_verify(&p, &ring, who);
	switch (sig) {
	case KPK_SIG_GOOD:
		kd_log("%s: signed by %s", p.meta.id, who);
		break;
	case KPK_SIG_NONE:
		/*
		 * Allowed, and SAID. The medium's own index IS signed and the
		 * public half of that key ships in /etc/kdos/keys, so a pack
		 * that came off the medium takes the branch above and never
		 * reaches here. What reaches here is a pack from somewhere
		 * else — a stick, a `kdos-box freeze`, a directory in
		 * pack-sources — and refusing those outright would refuse the
		 * artefact this system can make for itself.
		 *
		 * SHIPPING A KEY IS A CLAIM ABOUT WHO BAKED THE MEDIUM, not
		 * about who wrote KDOS: it says the packs beside it came from
		 * the same bake. Replace it and re-sign to make it say
		 * something about you. KDOS_REQUIRE_SIG is the stricter rule
		 * for a machine that installs only what it can attribute.
		 */
		if (getenv("KDOS_REQUIRE_SIG")) {
			snprintf(msg, n, "%s is unsigned and KDOS_REQUIRE_SIG is set",
				 p.meta.id);
			return -1;
		}
		kd_log("%s: unsigned", p.meta.id);
		break;
	default:
		snprintf(msg, n, "%s: %s", p.meta.id, kpk_sig_state_name(sig));
		return -1;
	}

accepted:
	kb_mkdir_p(kd_store_dir());
	snprintf(dst, sizeof(dst), "%s/%s.kpack", kd_store_dir(), p.meta.id);

	/*
	 * The version already installed is kept under its own version, and
	 * `rollback` is what renames it back. Per-app rollback is atomic
	 * because both files are just files: nothing is unpacked, so there is
	 * nothing to half-undo.
	 */
	if (kb_path_exists(dst)) {
		KpkPack cur;
		if (kpk_open(dst, &cur) == 0) {
			snprintf(old, sizeof(old), "%s/%s-%s.kpack",
				 kd_store_dir(), cur.meta.id, cur.meta.version);
			if (strcmp(old, dst))
				rename(dst, old);
		}
	}

	if (rename(src, dst) != 0) {
		/* A cross-device rename fails, and the staging directory is
		 * inside the store precisely so that it does not. */
		if (kb_copy_file(src, dst) != 0) {
			snprintf(msg, n, "cannot move %s into the store", staged);
			return -1;
		}
		unlink(src);
	}
	kd_sweep(p.meta.id);
	snprintf(msg, n, "%s %s", p.meta.id, p.meta.version);
	return 0;
}

int kd_remove(const char *id, char *msg, size_t n)
{
	int i = kd_find(id);

	if (i < 0) {
		snprintf(msg, n, "no pack %s", id);
		return -1;
	}
	if (kd_pack[i].origin != KD_ORIGIN_STORE) {
		snprintf(msg, n, "%s is on the medium, not installed", id);
		return -1;
	}
	if (kd_pack[i].refs) {
		snprintf(msg, n, "%s is composed into %d box(es)", id,
			 kd_pack[i].refs);
		return -1;
	}
	if (kd_pack[i].mnt[0] && kd_unmount(i, msg, n) != 0)
		return -1;
	if (unlink(kd_pack[i].file) != 0) {
		snprintf(msg, n, "cannot remove %s", kd_pack[i].file);
		return -1;
	}
	snprintf(msg, n, "%s removed", id);
	return 0;
}

/*
 * The previous version is still a file, so rolling back is a pair of renames.
 * Bluefin rolls the whole operating system back to undo one bad application;
 * this rolls the application back.
 */
int kd_rollback(const char *id, char *msg, size_t n)
{
	char cur[KD_PATH * 2], prev[KD_PATH * 2], keep[KD_PATH * 2];
	char **names;
	char best[64] = "";
	KpkPack p;
	int i = kd_find(id);

	if (i < 0 || kd_pack[i].origin != KD_ORIGIN_STORE) {
		snprintf(msg, n, "no installed pack %s", id);
		return -1;
	}
	if (kd_pack[i].refs) {
		snprintf(msg, n, "%s is composed into %d box(es)", id,
			 kd_pack[i].refs);
		return -1;
	}

	/* The newest kept version that is not the one installed. */
	names = kb_listdir(kd_store_dir(), NULL);
	for (char **nm = names; nm && *nm; nm++) {
		char idpart[KPK_ID_MAX + 96];
		size_t l = strlen(*nm);
		if (l < 8 || strcmp(*nm + l - 6, ".kpack"))
			continue;
		kb_strlcpy(idpart, *nm, sizeof(idpart));
		idpart[l - 6] = 0;
		if (strncmp(idpart, id, strlen(id)) || idpart[strlen(id)] != '-')
			continue;
		if (!best[0] || strcmp(idpart + strlen(id) + 1, best) > 0)
			kb_strlcpy(best, idpart + strlen(id) + 1, sizeof(best));
	}
	kb_strv_free(names);
	if (!best[0]) {
		snprintf(msg, n, "no earlier %s is kept", id);
		return -1;
	}

	snprintf(cur, sizeof(cur), "%s/%s.kpack", kd_store_dir(), id);
	snprintf(prev, sizeof(prev), "%s/%s-%s.kpack", kd_store_dir(), id, best);
	if (kpk_open(cur, &p) != 0) {
		snprintf(msg, n, "%s is unreadable", cur);
		return -1;
	}
	snprintf(keep, sizeof(keep), "%s/%s-%s.kpack", kd_store_dir(), id,
		 p.meta.version);

	if (kd_pack[i].mnt[0] && kd_unmount(i, msg, n) != 0)
		return -1;
	if (rename(cur, keep) != 0 || rename(prev, cur) != 0) {
		snprintf(msg, n, "cannot roll %s back", id);
		return -1;
	}
	snprintf(msg, n, "%s %s -> %s", id, p.meta.version, best);
	return 0;
}
