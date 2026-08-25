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

/* `<id>\t<absolute path>` per line. Flat, because the reader is `ungraft` and
 * a person with `cat`. */
static int manifest_add(const char *id, const char *path)
{
	FILE *f = fopen(kd_manifest(), "a");

	if (!f)
		return -1;
	fprintf(f, "%s\t%s\n", id, path);
	return fclose(f) == 0 ? 0 : -1;
}

/* Remove every line for `id`, calling `each` with the path first. */
static int manifest_drop(const char *id, void (*each)(const char *))
{
	size_t len = 0;
	char *text = kb_read_all(kd_manifest(), &len);
	KbBuf keep = {0};
	char *line, *save;
	int found = 0;

	if (!text)
		return 0;
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
		} else {
			kb_buf_printf(&keep, "%s\t%s\n", line, tab + 1);
		}
	}
	free(text);
	kb_write_all(kd_manifest(), keep.p ? keep.p : "", keep.n);
	kb_buf_free(&keep);
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

int kd_graft(int idx, uid_t uid, char *msg, size_t n)
{
	KdPack *p = &kd_pack[idx];
	struct passwd *pw;
	char home[KD_PATH];
	int made = 0;

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

	for (int i = 0; i < p->meta.ngraft; i++) {
		char src[KD_PATH * 3], dst[KD_PATH * 3];

		snprintf(src, sizeof(src), "%s/%s", p->mnt, p->meta.graft[i].from);
		snprintf(dst, sizeof(dst), "%s/%s", share_dir(),
			 p->meta.graft[i].to);
		if (kd_fixture) {
			kd_log("graft %s -> %s", src, dst);
			made++;
			continue;
		}
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

	for (int i = 0; i < p->meta.nboxgraft; i++) {
		char src[KD_PATH * 3], dst[KD_PATH * 3], dir[KD_PATH * 2];

		snprintf(src, sizeof(src), "%s/%s", p->mnt,
			 p->meta.boxgraft[i].from);
		snprintf(dir, sizeof(dir), "%s/.local/share/kdos/packs", home);
		snprintf(dst, sizeof(dst), "%s/%s", dir, p->meta.boxgraft[i].to);
		if (kd_fixture) {
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
	gone = manifest_drop(id, kd_fixture ? NULL : unlink_one);
	snprintf(msg, n, "%s: %d graft(s) removed", id, gone);
	return 0;
}
