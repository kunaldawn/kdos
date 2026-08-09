/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — the snapshot inventory and what a restore would extract
 *
 * The decidable half of script/buildlib/snapshot.py: reading manifests,
 * deciding which snapshots exist, and choosing which archive supplies each
 * path for a restore. Creating and extracting them runs tar as root and stays
 * where it is for now — this is the part that DECIDES, and a wrong decision
 * here restores the wrong tree under the right name.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbuild.h"

/* ──────────────────────────────────────────────────────────────────────── */
/* Codec — the manifest records it, so an archive written by one build is
 * still readable by a build whose host lost zstd. */

const char *kbuild_snap_suffix(const char *codec)
{
	if (codec && !strcmp(codec, "zstd"))
		return ".tar.zst";
	if (codec && !strcmp(codec, "gzip"))
		return ".tar.gz";
	return ".tar";
}

void kbuild_snap_decompressor(const char *codec, KbArgv *a)
{
	if (codec && !strcmp(codec, "zstd")) {
		kb_argv_add(a, "zstd");
		kb_argv_add(a, "-dc");
	} else if (codec && !strcmp(codec, "gzip")) {
		kb_argv_add(a, "gzip");
		kb_argv_add(a, "-dc");
	} else {
		kb_argv_add(a, "cat");
	}
	kb_argv_end(a);
}

void kbuild_snap_archive_name(const char *path, const char *codec, char *out,
			      size_t cap)
{
	char tmp[256];
	kb_strlcpy(tmp, path, sizeof(tmp));
	for (char *c = tmp; *c; c++)
		if (*c == '/')
			*c = '_';
	snprintf(out, cap, "%s%s", tmp, kbuild_snap_suffix(codec));
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Inventory                                                                */

void kbuild_snap_dir(const char *root, const char *dir_name, char *out,
		     size_t cap)
{
	snprintf(out, cap, "%s/%s", root, dir_name);
}

int kbuild_snap_load(const char *root, const char *dir_name, KbuildSnapshot *sn)
{
	memset(sn, 0, sizeof(*sn));
	kb_strlcpy(sn->dir_name, dir_name, sizeof(sn->dir_name));

	char dir[512];
	kbuild_snap_dir(root, dir_name, dir, sizeof(dir));

	char path[640];
	snprintf(path, sizeof(path), "%s/%s", dir, KBUILD_MANIFEST);
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	if (!text)
		return -1;

	KjNode *m = kj_parse(text);
	free(text);
	if (!m || m->type != KJ_OBJ) {
		kj_free(m);
		return -1;
	}

	const KjNode *entries = kj_get(m, "entries");
	if (!entries || entries->type != KJ_ARR) {
		kj_free(m);
		return -1;		/* not a manifest at all */
	}

	kb_strlcpy(sn->phase, kj_str(m, "phase", ""), sizeof(sn->phase));
	kb_strlcpy(sn->phase_dir, kj_str(m, "phase_dir", dir_name),
		   sizeof(sn->phase_dir));
	kb_strlcpy(sn->title, kj_str(m, "title", ""), sizeof(sn->title));
	kb_strlcpy(sn->codec, kj_str(m, "codec", ""), sizeof(sn->codec));
	kb_strlcpy(sn->created_iso, kj_str(m, "created_iso", ""),
		   sizeof(sn->created_iso));
	kb_strlcpy(sn->git_commit, kj_str(m, "git_commit", ""),
		   sizeof(sn->git_commit));
	sn->git_dirty = kj_bool(m, "git_dirty", 0);
	sn->created = kj_num(m, "created", 0);
	sn->duration_s = kj_num(m, "duration_s", 0);
	sn->snapshot_s = kj_num(m, "snapshot_s", 0);
	sn->schema = (int)kj_num(m, "schema", 0);
	sn->steps = (int)kj_num(m, "steps", 0);
	sn->total_steps = (int)kj_num(m, "total_steps", sn->steps);
	/* Snapshots written before schema 3 are always whole-phase snapshots. */
	sn->complete = kj_bool(m, "complete", 1);

	for (const KjNode *e = entries->child; e; e = e->next) {
		if (sn->nentries == KBUILD_MAX_PATHS)
			break;
		KbuildSnapEntry *slot = &sn->entry[sn->nentries];
		kb_strlcpy(slot->path, kj_str(e, "path", ""), sizeof(slot->path));
		kb_strlcpy(slot->archive, kj_str(e, "archive", ""),
			   sizeof(slot->archive));
		slot->bytes_raw = (long long)kj_num(e, "bytes_raw", 0);
		slot->bytes_compressed =
			(long long)kj_num(e, "bytes_compressed", 0);
		slot->files = (long long)kj_num(e, "files", 0);

		/* An entry whose archive is gone means the snapshot is
		 * INCOMPLETE, and an incomplete snapshot is treated as absent
		 * rather than restored from partially. */
		char ap[900];
		snprintf(ap, sizeof(ap), "%s/%s", dir, slot->archive);
		if (!kb_path_exists(ap) || kb_is_dir(ap)) {
			kj_free(m);
			return -1;
		}
		sn->nentries++;
	}

	kj_free(m);
	return 0;
}

int kbuild_snap_list(const char *root, KbuildSnapshot *out, int max)
{
	char **names = kb_listdir(root, NULL);
	if (!names)
		return 0;

	int n = 0;
	for (char **e = names; *e && n < max; e++) {
		char dir[512];
		kbuild_snap_dir(root, *e, dir, sizeof(dir));
		if (!kb_is_dir(dir))
			continue;
		if (kbuild_snap_load(root, *e, &out[n]) == 0)
			n++;
	}
	kb_strv_free(names);
	return n;
}

const KbuildSnapshot *kbuild_snap_find(const KbuildSnapshot *snaps, int n,
				       const char *dir_name)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(snaps[i].dir_name, dir_name))
			return &snaps[i];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Restore selection
 *
 * Layered and newest-wins: each declared path is taken from the newest
 * snapshot at or below the target, so restoring phase 5 can still get `ports`
 * from phase 3 if that is the last phase that declared it. */

int kbuild_snap_plan_restore(const char *root, const KbuildPhase *ph, int nph,
			     int target_index, KbuildRestoreItem *out, int max)
{
	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int nsnap = kbuild_snap_list(root, snaps, KBUILD_MAX_PHASES);

	int n = 0;
	for (int i = 0; i < nph; i++) {
		if (ph[i].index > target_index)
			break;
		const KbuildSnapshot *sn =
			kbuild_snap_find(snaps, nsnap, ph[i].dir_name);
		if (!sn)
			continue;

		for (int k = 0; k < sn->nentries; k++) {
			const KbuildSnapEntry *ent = &sn->entry[k];
			int at = -1;
			for (int j = 0; j < n; j++)
				if (!strcmp(out[j].path, ent->path)) {
					at = j;
					break;
				}
			if (at < 0) {
				if (n == max)
					continue;
				at = n++;
			}
			KbuildRestoreItem *it = &out[at];
			kb_strlcpy(it->path, ent->path, sizeof(it->path));
			snprintf(it->archive, sizeof(it->archive), "%s/%s/%s",
				 root, ph[i].dir_name, ent->archive);
			kb_strlcpy(it->source, ph[i].dir_name, sizeof(it->source));
			kb_strlcpy(it->codec, sn->codec, sizeof(it->codec));
			it->bytes_compressed = ent->bytes_compressed;
			it->bytes_raw = ent->bytes_raw;
			it->files = ent->files;
		}
	}
	free(snaps);

	/* python returns `[chosen[k] for k in sorted(chosen)]` — path order, not
	 * discovery order. Extraction order is observable in the progress UI. */
	for (int i = 1; i < n; i++) {
		KbuildRestoreItem key = out[i];
		int k = i - 1;
		while (k >= 0 && strcmp(out[k].path, key.path) > 0) {
			out[k + 1] = out[k];
			k--;
		}
		out[k + 1] = key;
	}
	return n;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Guards                                                                   */

int kbuild_snap_interrupted(const char *build_dir, char *target, size_t cap)
{
	if (target && cap)
		target[0] = 0;

	char path[640];
	snprintf(path, sizeof(path), "%s/%s", build_dir, KBUILD_RESTORE_MARKER);
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	if (!text)
		return 0;

	KjNode *m = kj_parse(text);
	free(text);
	/* An empty object is not a marker: python returns the dict and every
	 * caller tests it for truth, so `{}` means no interrupted restore. */
	if (!m || m->type != KJ_OBJ || !m->child) {
		kj_free(m);
		return 0;		/* python: ValueError -> None */
	}
	if (target && cap)
		kb_strlcpy(target, kj_str(m, "target", ""), cap);
	kj_free(m);
	return 1;
}

/* /proc/mounts, longest first — chroot_exec.sh's own bind mounts have to be
 * released before build/ can be deleted, and a snapshot taken over a live
 * bind mount would archive the host's /dev. */
int kbuild_snap_mounts_under(const char *path, char out[][256], int max)
{
	size_t len = 0;
	char *data = kb_read_all("/proc/mounts", &len);
	if (!data)
		return 0;

	size_t plen = strlen(path);
	while (plen > 1 && path[plen - 1] == '/')
		plen--;

	int n = 0;
	for (char *line = data, *next; line && *line && n < max; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		char *sp = strchr(line, ' ');
		if (!sp)
			continue;
		char *mnt = sp + 1;
		char *end = strchr(mnt, ' ');
		if (!end)
			continue;
		*end = 0;

		/* getmntent escaping: a space in a mount point is "\040". */
		char decoded[256];
		size_t d = 0;
		for (char *c = mnt; *c && d < sizeof(decoded) - 1; c++) {
			if (c[0] == '\\' && c[1] == '0' && c[2] == '4' &&
			    c[3] == '0') {
				decoded[d++] = ' ';
				c += 3;
			} else {
				decoded[d++] = *c;
			}
		}
		decoded[d] = 0;

		if (strncmp(decoded, path, plen))
			continue;
		if (decoded[plen] && decoded[plen] != '/')
			continue;
		kb_strlcpy(out[n++], decoded, 256);
	}
	free(data);

	for (int i = 1; i < n; i++) {
		char key[256];
		memcpy(key, out[i], 256);
		size_t kl = strlen(key);
		int k = i - 1;
		while (k >= 0 && strlen(out[k]) < kl) {
			memcpy(out[k + 1], out[k], 256);
			k--;
		}
		memcpy(out[k + 1], key, 256);
	}
	return n;
}
