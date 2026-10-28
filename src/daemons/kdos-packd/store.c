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
 * INSTALLING IS A RENAME. A pack is copied out of the staging directory into a
 * private, root-owned file in the store, verified there and renamed into
 * place; there is no unpack step to interrupt and no half-installed state to
 * recover from. The previous version is not deleted — it is
 * what `rollback` renames back, and per-app rollback is the property this
 * whole format exists for.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

	/* Clamped like the file's value, so a negative count means `0`
	 * whichever of the two said it. */
	if (env && *env) {
		int v = atoi(env);
		return v < 0 ? 0 : v;
	}
	if (cached >= 0)
		return cached;
	cached = 1;
	/* `> 0`, not `== 0`: kb_read_file answers the BYTE COUNT, so `== 0`
	 * would parse the file only when it is empty, and every configured
	 * `retain =` would be ignored in favour of the default. */
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
 * A KEPT VERSION OF `id` is `<id>-<version>.kpack` in the store whose own
 * metadata names `id`. The name alone cannot say so: an id may contain `-`,
 * so `foo-bar.kpack` reads as a kept `bar` of foo by its name and is the
 * installed pack foo-bar by its contents, and a sweep or a rollback of foo
 * that trusted the name would delete or install the other application.
 * Answers 1 with the version the name carries, 0 for any other file.
 */
static int kept_version(const char *name, const char *id, char *ver,
			size_t cap)
{
	size_t l = strlen(name), idlen = strlen(id);
	char *path;
	KpkPack p;
	int ok;

	if (l < idlen + 8 || strcmp(name + l - 6, ".kpack"))
		return 0;
	if (strncmp(name, id, idlen) || name[idlen] != '-')
		return 0;
	if (l - 6 - idlen - 1 >= cap)
		return 0;
	kb_strlcpy(ver, name + idlen + 1, l - 6 - idlen);
	path = kb_path_join(kd_store_dir(), name);
	ok = kpk_open(path, &p) == 0 && !strcmp(p.meta.id, id);
	free(path);
	return ok;
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

	if (retain > 8)
		retain = 8;
	if (!names)
		return;

	/* The newest `retain` versions, by kp_vercmp so `1.10` beats `1.9`. */
	for (char **nm = names; *nm; nm++) {
		char ver[96];

		if (!kept_version(*nm, id, ver, sizeof(ver)))
			continue;
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
		int kept = 0;

		if (!kept_version(*nm, id, ver, sizeof(ver)))
			continue;
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
		 * IN THE STORE THE ROW IS `<id>.kpack` AND NOTHING ELSE. An
		 * install names the version it installs so and renames the one
		 * it supersedes `<id>-<version>.kpack`, which is what rollback
		 * reads and never a row: `-` sorts before `.`, so a scan that
		 * took the first file naming an id would list and mount the
		 * kept version in place of the installed one.
		 */
		if (origin == KD_ORIGIN_STORE) {
			size_t il = strlen(p.meta.id);
			if (strncmp(*n, p.meta.id, il) ||
			    strcmp(*n + il, ".kpack")) {
				free(path);
				continue;
			}
		}

		/*
		 * An id already in the table wins if it is INSTALLED: the store
		 * shadows the medium, so a pack the user updated is the one
		 * that runs even while the older one is still on the stick.
		 * Of two files on the medium naming one id, the first read is
		 * the row.
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
 * A PRIVATE COPY of a file in the staging directory, made by this daemon and
 * owned by it, mode 0644. Everything after it reads the copy: the staged file
 * belongs to whoever put it there, in a directory every user may write, so a
 * check on it proves nothing about the bytes a later read or a rename would
 * find — its owner can rewrite it, or hold it open for writing, after the
 * verification passed. `O_NOFOLLOW` and the regular-file test keep a staged
 * symlink or device from being read as root.
 */
static int take_private(const char *from, const char *to)
{
	char buf[65536];
	struct stat st;
	int in, out, rc = -1;

	in = open(from, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (in < 0)
		return -1;
	if (fstat(in, &st) != 0 || !S_ISREG(st.st_mode)) {
		close(in);
		return -1;
	}
	unlink(to);
	out = open(to, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
		   0644);
	if (out < 0) {
		close(in);
		return -1;
	}
	for (;;) {
		ssize_t r = read(in, buf, sizeof(buf));
		if (r == 0) {
			rc = 0;
			break;
		}
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		ssize_t off = 0;
		while (off < r) {
			ssize_t w = write(out, buf + off, (size_t)(r - off));
			if (w < 0 && errno == EINTR)
				continue;
			if (w <= 0)
				break;
			off += w;
		}
		if (off < r)
			break;
	}
	close(in);
	if (fchmod(out, 0644) != 0)
		rc = -1;
	if (close(out) != 0)
		rc = -1;
	if (rc != 0)
		unlink(to);
	return rc;
}

static int install_private(const char *src, const char *idxpath,
			   const char *staged, char *msg, size_t n);

/* A staged name is a FILENAME in the staging directory: no slash, no `..`,
 * nothing outside [A-Za-z0-9._-]. */
static int staged_name_ok(const char *staged, char *msg, size_t n)
{
	if (strchr(staged, '/') || !strcmp(staged, "..") || !*staged) {
		snprintf(msg, n, "a staged name is a filename, not a path");
		return 0;
	}
	for (const char *c = staged; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-') {
			snprintf(msg, n, "a staged name is [A-Za-z0-9._-]");
			return 0;
		}
	return 1;
}

/*
 * `staged` is a FILENAME in the staging directory and nothing else — not a
 * path, not something with a slash in it. That is rule 3: a daemon reachable
 * from `wheel` that takes a path is `mount /dev/sda2 /etc` from any shell, and
 * the staging directory is the one place an unprivileged download may land.
 *
 * `index`, when not NULL, is a second staged filename: a signed set's index,
 * with its signature staged beside it as `<index>.sig`. THE INDEX IS NAMED BY
 * THE REQUEST, NEVER FOUND. One fixed name in a directory every user writes to
 * would be state shared across every install while it sat there: an import's
 * index would refuse another caller's install of a different build of a pack
 * it lists, and a stale one would outlive the import that staged it.
 *
 * The pack, and the index and its signature when named, are verified as
 * private copies in the store (take_private), and the pack copy is what is
 * renamed into place. The staged pack is removed only once that has worked;
 * the index is the caller's to remove, since one index covers many installs.
 */
int kd_install(const char *staged, const char *index, char *msg, size_t n)
{
	char from[KD_PATH * 2], src[KD_PATH * 2];
	char sidx[KD_PATH * 2], idxpath[KD_PATH * 2];
	char ssig[KD_PATH * 2 + 4], idxsig[KD_PATH * 2 + 4];
	const char *idx = NULL;
	int rc;

	if (!staged_name_ok(staged, msg, n))
		return -1;
	if (index && !staged_name_ok(index, msg, n))
		return -1;
	snprintf(from, sizeof(from), "%s/%s", kd_staging_dir(), staged);
	if (!kb_path_exists(from)) {
		snprintf(msg, n, "%s is not in the staging directory", staged);
		return -1;
	}
	kb_mkdir_p(kd_store_dir());
	/* Not `.kpack`, so a scan never lists a copy that is half written. */
	snprintf(src, sizeof(src), "%s/.install.part", kd_store_dir());
	if (take_private(from, src) != 0) {
		snprintf(msg, n, "cannot read %s from the staging directory",
			 staged);
		return -1;
	}
	snprintf(sidx, sizeof(sidx), "%s/%s", kd_staging_dir(),
		 index ? index : "");
	snprintf(idxpath, sizeof(idxpath), "%s/.install-index.part",
		 kd_store_dir());
	/* kpk_index_verify reads the signature from `<index>.sig`, so the
	 * copy of the index needs a copy of the staged signature beside it
	 * under the matching name; without it the signed-index route never
	 * answers GOOD and a pack the index contradicts would not be
	 * refused. */
	snprintf(ssig, sizeof(ssig), "%s.sig", sidx);
	snprintf(idxsig, sizeof(idxsig), "%s.sig", idxpath);
	unlink(idxsig);
	if (index && kb_path_exists(sidx) && take_private(sidx, idxpath) == 0) {
		idx = idxpath;
		if (kb_path_exists(ssig))
			take_private(ssig, idxsig);
	}
	rc = install_private(src, idx, staged, msg, n);
	if (idx)
		unlink(idxpath);
	unlink(idxsig);
	if (rc != 0)
		unlink(src);
	else
		unlink(from);
	return rc;
}

static int install_private(const char *src, const char *idxpath,
			   const char *staged, char *msg, size_t n)
{
	char dst[KD_PATH * 2], old[KD_PATH * 2];
	KpkPack p;
	KsigRing ring = {0};
	char who[KSIG_ID_HEX], err[256];
	KpkSigState sig;
	KpkIndex ix = {0};

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
	if (idxpath &&
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
		 * Allowed, and SAID. A pack reaches here when it carries no
		 * signature block and no signed index beside it vouches for it
		 * — an imported set, a `kdos-box freeze` — and refusing those
		 * outright would refuse the artefact this system can make for
		 * itself. A pack mounted straight off the medium does not
		 * pass through here; kd_mount checks it.
		 *
		 * THE KEY IN KD_KEYS SIGNS NOTHING THE BUILD MAKES: the build
		 * does not hold its secret half, so on a fresh system it
		 * attributes nothing. Add the `.pub` of a key from `kdos-pack
		 * keygen` to make a signature say something about you.
		 * KDOS_REQUIRE_SIG is the stricter rule for a machine that
		 * installs only what it can attribute.
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
	/*
	 * A MOUNTED SUPERSEDED VERSION MUST NOT OUTLIVE THE INSTALL. The mount
	 * table is keyed by id and survives the rescan, so leaving the
	 * superseded version mounted puts every later compose's NEW app diff
	 * over the OLD runtime's bytes, and an app then fails to load a
	 * library the new runtime carries and the mounted one does not. Idle,
	 * the stale mount is dropped here so the next mount reads the new
	 * file; composed into a box, the install is refused by the same rule
	 * `remove` applies, and the refusal says how many boxes to stop.
	 */
	{
		int cur = kd_find(p.meta.id);
		if (cur >= 0 && kd_pack[cur].origin == KD_ORIGIN_STORE &&
		    kd_pack[cur].mnt[0]) {
			if (kd_pack[cur].refs) {
				snprintf(msg, n, "%s is composed into %d box(es) — "
					 "stop them, then install", p.meta.id,
					 kd_pack[cur].refs);
				return -1;
			}
			if (kd_unmount(cur, msg, n) != 0)
				return -1;
		}
	}
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

	/* The private copy is already in the store directory, so this is
	 * a rename on one filesystem and cannot be half done. */
	if (rename(src, dst) != 0) {
		snprintf(msg, n, "cannot move %s into the store", staged);
		return -1;
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

	snprintf(cur, sizeof(cur), "%s/%s.kpack", kd_store_dir(), id);
	if (kpk_open(cur, &p) != 0) {
		snprintf(msg, n, "%s is unreadable", cur);
		return -1;
	}

	/* The newest kept version, by kp_vercmp as the sweep keeps them, so
	 * `1.10` is newer than `1.9`. One equal to the installed version is
	 * skipped: the installed file is renamed to that same name below, so
	 * taking it would overwrite the kept copy and restore nothing. */
	names = kb_listdir(kd_store_dir(), NULL);
	for (char **nm = names; nm && *nm; nm++) {
		char ver[sizeof(best)];
		if (!kept_version(*nm, id, ver, sizeof(ver)))
			continue;
		if (!strcmp(ver, p.meta.version))
			continue;
		if (!best[0] || kp_vercmp(ver, best) > 0)
			kb_strlcpy(best, ver, sizeof(best));
	}
	kb_strv_free(names);
	if (!best[0]) {
		snprintf(msg, n, "no earlier %s is kept", id);
		return -1;
	}

	snprintf(prev, sizeof(prev), "%s/%s-%s.kpack", kd_store_dir(), id, best);
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
