/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   data packs — a dataset that a program can find
 * ---------------------------------
 *
 * A dataset is a pack with `kind = data`. It gets the same image, the same
 * footer, the same signatures and the same daemon; what is specific to it is
 * what happens once it is mounted, and that is three rules:
 *
 *   - Mounted ro,nosuid,nodev,NOEXEC. A data pack that ships a binary cannot
 *     run it. That is in mount.c, where the mount is.
 *   - A DATA PACK CONTAINS NO SCRIPTS. Everything it wants done is DECLARED as
 *     graft / boxgraft / env lines that this file interprets. There is no
 *     shell near an untrusted artefact, and no path in a pack is ever handed
 *     to one.
 *   - TWO GRAFT NAMESPACES, because /usr/share is invisible inside a box.
 *     `graft` symlinks into /usr/share for host consumers; `boxgraft` lands
 *     under ~/.local/share/kdos/packs/<name>, because a box shares $HOME and
 *     nothing else — the same reason `kdos theme` writes GTK and Qt themes
 *     into $HOME rather than into /usr/share.
 *
 * EVERY GRAFT IS RECORDED, so ungraft removes exactly what it added. That is
 * the fs-manifest lesson applied to data: a sync that only ever adds leaves a
 * dropped path behind for ever, and here that would be a dangling symlink in
 * /usr/share pointing at a mountpoint nothing mounts any more.
 *
 * THE RECORD IS THE STORE'S, NOT THE SLOT'S. It is `pack-manifest` inside
 * the pack store, which on an A/B machine is /home/.kdos/packs bound onto
 * /var/lib/kdos/packs in both roots, so the two slots read one record of
 * what is grafted. A `graft` line lands in the root this daemon runs, and the
 * other slot's /usr/share never received it: at start (kd_regraft) every
 * host graft the record names and this root lacks is made again, so a data
 * pack installed while the other slot was being filled is in place after the
 * switch. A boxgraft is under $HOME, which both slots share, and needs none.
 *
 * An `ungraft` removes the links of the root it runs in, and the other
 * slot's stay until that slot starts. So the paths it drops are kept in
 * `pack-manifest.ungrafted` beside the record, and at start every one of
 * them the record does not name is unlinked when it is a link into the
 * store's mountpoints; a `graft` of the same path takes it off that list.
 *
 * EVERY WRITE IS WHOLE AND SERIALISED. The record and the list are rewritten
 * to a temporary file and renamed over the old one, under an flock of
 * `pack-manifest.lock` beside them, so a write that is interrupted leaves the
 * previous file and two writers never interleave lines.
 */

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "packd.h"

/* The record's lock, beside it: held for every read-modify-write, released
 * by closing it. -1 when it cannot be taken, and nothing is then written. */
static int manifest_lock(void)
{
	char lp[KD_PATH + 8];
	snprintf(lp, sizeof(lp), "%s.lock", kd_manifest());
	return kb_lock_file(lp, 0);
}

/* The paths an `ungraft` dropped, one per line, beside the record. */
static const char *dropped_list(void)
{
	static char b[KD_PATH + 16];
	snprintf(b, sizeof(b), "%s.ungrafted", kd_manifest());
	return b;
}

/* 1 when `text` holds the line `want` (without its newline). */
static int has_line(const char *text, const char *want)
{
	size_t wl = strlen(want);
	for (const char *l = text; l && *l;) {
		size_t len = strcspn(l, "\n");
		if (len == wl && !strncmp(l, want, len))
			return 1;
		l += len + (l[len] == '\n');
	}
	return 0;
}

/* The dropped list with `add` (newline-separated paths) put on and `take`
 * taken off; written only when it changes. Called with the lock held. */
static void dropped_update(const char *add, const char *take)
{
	char *text = kb_read_all(dropped_list(), NULL);
	KbBuf b = {0};
	int changed = 0;

	for (const char *l = text; l && *l;) {
		size_t len = strcspn(l, "\n");
		if (take && len == strlen(take) && !strncmp(l, take, len))
			changed = 1;
		else if (len)
			kb_buf_printf(&b, "%.*s\n", (int)len, l);
		l += len + (l[len] == '\n');
	}
	for (const char *l = add; l && *l;) {
		size_t len = strcspn(l, "\n");
		char one[KD_PATH * 3];
		snprintf(one, sizeof(one), "%.*s", (int)len, l);
		if (len && !has_line(b.p, one)) {
			kb_buf_printf(&b, "%s\n", one);
			changed = 1;
		}
		l += len + (l[len] == '\n');
	}
	if (changed)
		kb_write_file_atomic(dropped_list(), b.p ? b.p : "");
	kb_buf_free(&b);
	free(text);
}

/* `<id>\t<absolute path>` per line. Flat, because the reader is `ungraft` and
 * a person with `cat`. A line already there is not written twice, so making
 * a graft again (kd_regraft, a second `graft`) leaves one line for it. */
static int manifest_add(const char *id, const char *path)
{
	KbBuf b = {0};
	char *text, line[KD_PATH * 4];
	int lock = manifest_lock(), rc = 0;

	if (lock < 0)
		return -1;
	dropped_update(NULL, path);
	snprintf(line, sizeof(line), "%s\t%s\n", id, path);
	text = kb_read_all(kd_manifest(), NULL);
	if (text)
		for (char *l = text; *l;) {
			size_t len = strcspn(l, "\n");
			if (len + 1 == strlen(line) && !strncmp(l, line, len))
				goto out;
			l += len + (l[len] == '\n');
		}
	kb_buf_printf(&b, "%s%s%s", text ? text : "",
		      text && *text && text[strlen(text) - 1] != '\n' ? "\n" : "",
		      line);
	rc = kb_write_file_atomic(kd_manifest(), b.p);
out:
	free(text);
	kb_buf_free(&b);
	close(lock);
	return rc;
}

/* Remove every line for `id`, calling `each` with the path first. */
static int manifest_drop(const char *id, void (*each)(const char *))
{
	size_t len = 0;
	char *text;
	KbBuf keep = {0}, gone = {0};
	char *line, *save;
	int found = 0, lock = manifest_lock();

	if (lock < 0)
		return 0;
	text = kb_read_all(kd_manifest(), &len);
	if (!text) {
		close(lock);
		return 0;
	}
	for (line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		if (!tab) {
			continue;
		}
		*tab = 0;
		if (!strcmp(line, id)) {
			found++;
			if (each)
				each(tab + 1);
			kb_buf_printf(&gone, "%s\n", tab + 1);
		} else {
			kb_buf_printf(&keep, "%s\t%s\n", line, tab + 1);
		}
	}
	free(text);
	if (found) {
		kb_write_file_atomic(kd_manifest(), keep.p ? keep.p : "");
		dropped_update(gone.p, NULL);
	}
	kb_buf_free(&keep);
	kb_buf_free(&gone);
	close(lock);
	return found;
}

static void unlink_one(const char *path)
{
	/*
	 * Only a SYMLINK is removed. The manifest names what this daemon
	 * created, and if what is there now is a real directory then somebody
	 * else owns it — removing it would be the manifest deleting something
	 * it did not put there.
	 */
	if (kb_is_link(path))
		unlink(path);
}

static const char *share_dir(void)
{
	const char *p = getenv("KDOS_PACK_SHARE");
	return p && *p ? p : KD_SHARE;
}

/* The host grafts of pack `idx`: links into /usr/share, made in the root
 * this daemon runs. With `missing_only`, a destination that is already a
 * link is left as it is. The count made, or -1 with the reason in `msg`. */
static int graft_host(int idx, int missing_only, char *msg, size_t n)
{
	KdPack *p = &kd_pack[idx];
	int made = 0;

	for (int i = 0; i < p->meta.ngraft; i++) {
		char src[KD_PATH * 3], dst[KD_PATH * 3];

		snprintf(src, sizeof(src), "%s/%s", p->mnt, p->meta.graft[i].from);
		snprintf(dst, sizeof(dst), "%s/%s", share_dir(),
			 p->meta.graft[i].to);
		if (kd_dry) {
			kd_log("graft %s -> %s", src, dst);
			made++;
			continue;
		}
		if (missing_only && kb_is_link(dst))
			continue;
		if (kb_path_exists(dst) && !kb_is_link(dst)) {
			kd_log("%s: %s is already there and is not ours", p->meta.id,
			       dst);
			continue;
		}
		unlink(dst);
		if (symlink(src, dst) != 0) {
			snprintf(msg, n, "%s: %s", dst, strerror(errno));
			return -1;
		}
		manifest_add(p->meta.id, dst);
		made++;
	}
	return made;
}

int kd_graft(int idx, uid_t uid, char *msg, size_t n)
{
	KdPack *p = &kd_pack[idx];
	struct passwd *pw;
	char home[KD_PATH];
	int made;

	if (p->meta.kind != KPK_KIND_DATA) {
		snprintf(msg, n, "%s is not a data pack", p->meta.id);
		return -1;
	}
	if (!p->mnt[0] && kd_mount(idx, msg, n) != 0)
		return -1;

	pw = getpwuid(uid);
	kb_strlcpy(home, pw && pw->pw_dir ? pw->pw_dir : "/root", sizeof(home));
	{
		const char *h = getenv("KDOS_PACK_HOME");
		if (h && *h)
			kb_strlcpy(home, h, sizeof(home));
	}

	made = graft_host(idx, 0, msg, n);
	if (made < 0)
		return -1;

	for (int i = 0; i < p->meta.nboxgraft; i++) {
		char src[KD_PATH * 3], dst[KD_PATH * 3], dir[KD_PATH * 2];

		snprintf(src, sizeof(src), "%s/%s", p->mnt,
			 p->meta.boxgraft[i].from);
		/*
		 * `to` is a name under ~/.local/share/kdos/packs, where a
		 * program told by an `env` line finds it — or, beginning with
		 * `~/`, a place in the home the program looks in by itself:
		 * stellarium reads ~/.stellarium/stars and takes no variable
		 * naming another directory. The parent is made; the link is
		 * the only thing written, and only a link is ever removed.
		 */
		if (!strncmp(p->meta.boxgraft[i].to, "~/", 2)) {
			char *slash;
			snprintf(dst, sizeof(dst), "%s/%s", home,
				 p->meta.boxgraft[i].to + 2);
			kb_strlcpy(dir, dst, sizeof(dir));
			slash = strrchr(dir, '/');
			if (slash)
				*slash = 0;
		} else {
			snprintf(dir, sizeof(dir), "%s/.local/share/kdos/packs",
				 home);
			snprintf(dst, sizeof(dst), "%s/%s", dir,
				 p->meta.boxgraft[i].to);
		}
		if (kd_dry) {
			kd_log("boxgraft %s -> %s", src, dst);
			made++;
			continue;
		}
		kb_mkdir_p(dir);
		(void)!chown(dir, uid, (gid_t)-1);
		if (kb_path_exists(dst) && !kb_is_link(dst))
			continue;
		unlink(dst);
		if (symlink(src, dst) != 0) {
			snprintf(msg, n, "%s: %s", dst, strerror(errno));
			return -1;
		}
		manifest_add(p->meta.id, dst);
		made++;
	}

	snprintf(msg, n, "%s: %d graft(s)", p->meta.id, made);
	return 0;
}

int kd_ungraft(const char *id, char *msg, size_t n)
{
	int gone;

	if (!kd_id_ok(id)) {
		snprintf(msg, n, "not an id");
		return -1;
	}
	gone = manifest_drop(id, kd_dry ? NULL : unlink_one);
	snprintf(msg, n, "%s: %d graft(s) removed", id, gone);
	return 0;
}

/*
 * The links an `ungraft` in the other root dropped from the record, removed
 * from this one. Only a path under the share directory that the record no
 * longer names, and only while it is a symlink into the store's mountpoints,
 * which is what a graft made; anything else there is somebody else's. The
 * number removed.
 */
static int drop_stale(void)
{
	char sharep[KD_PATH + 2], mntp[KD_PATH + 2];
	int lock = manifest_lock(), gone = 0;

	if (lock < 0)
		return 0;
	char *list = kb_read_all(dropped_list(), NULL);
	char *rec = kb_read_all(kd_manifest(), NULL);
	snprintf(sharep, sizeof(sharep), "%s/", share_dir());
	snprintf(mntp, sizeof(mntp), "%s/", kd_mnt_dir());
	for (const char *l = list; l && *l;) {
		size_t len = strcspn(l, "\n");
		char path[KD_PATH * 3], want[KD_PATH * 4], to[KD_PATH * 3];
		ssize_t tn;
		int named = 0;

		snprintf(path, sizeof(path), "%.*s", (int)len, l);
		l += len + (l[len] == '\n');
		if (!len || strncmp(path, sharep, strlen(sharep)) ||
		    strstr(path, "/../") || !kb_is_link(path))
			continue;
		for (const char *r = rec; r && *r && !named;) {
			size_t rl = strcspn(r, "\n");
			const char *tab = memchr(r, '\t', rl);
			snprintf(want, sizeof(want), "%.*s", (int)rl, r);
			named = tab && !strcmp(want + (tab - r) + 1, path);
			r += rl + (r[rl] == '\n');
		}
		tn = readlink(path, to, sizeof(to) - 1);
		if (named || tn <= 0)
			continue;
		to[tn] = 0;
		if (strncmp(to, mntp, strlen(mntp)))
			continue;
		if (kd_dry)
			kd_log("ungraft %s", path);
		else if (unlink(path) != 0)
			continue;
		gone++;
	}
	free(list);
	free(rec);
	close(lock);
	return gone;
}

/*
 * THE HOST GRAFTS OF EVERY PACK THE RECORD NAMES, made again where this root
 * lacks them: run once at start, before the first request. The destinations
 * are this root's, from the pack's own `graft` lines, and a destination that
 * is already a link is left alone. A pack the store no longer holds, or one
 * that is not a data pack, is skipped and its lines are kept for its
 * `ungraft`. The links the other root's `ungraft`s dropped go first
 * (drop_stale). The number of packs that needed a link made.
 */
int kd_regraft(void)
{
	char *text;
	char *line, *save, msg[512];
	char done[KD_PACKS];
	int again = 0, stale = drop_stale();

	if (stale)
		kd_log("removed %d graft(s) an ungraft in the other root dropped",
		       stale);
	text = kb_read_all(kd_manifest(), NULL);
	if (!text)
		return 0;
	memset(done, 0, sizeof(done));
	for (line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		int i, made;

		if (!tab)
			continue;
		*tab = 0;
		if (!kd_id_ok(line) || (i = kd_find(line)) < 0 || done[i])
			continue;
		done[i] = 1;
		if (kd_pack[i].meta.kind != KPK_KIND_DATA ||
		    kd_pack[i].origin != KD_ORIGIN_STORE || !kd_pack[i].meta.ngraft)
			continue;
		if (!kd_pack[i].mnt[0] && kd_mount(i, msg, sizeof(msg)) != 0) {
			kd_log("regraft %s: %s", line, msg);
			continue;
		}
		made = graft_host(i, 1, msg, sizeof(msg));
		if (made < 0)
			kd_log("regraft %s: %s", line, msg);
		else if (made > 0)
			again++;
	}
	free(text);
	return again;
}
