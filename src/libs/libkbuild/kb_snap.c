/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — the snapshot inventory, its chains, and what a restore extracts
 *
 * The decidable half of snapshotting: reading manifests, deciding which
 * snapshots exist, which chain of archives supplies each path for a restore,
 * what a layer holds, and which held snapshots nothing needs any more.
 * Creating and extracting them runs tar as root and belongs to the driver —
 * this is the part that DECIDES, and a wrong decision here restores the wrong
 * tree under the right name.
 * ---------------------------------
 */

#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

void kbuild_snap_gone_name(const char *path, char *out, size_t cap)
{
	char tmp[256];
	kb_strlcpy(tmp, path, sizeof(tmp));
	for (char *c = tmp; *c; c++)
		if (*c == '/')
			*c = '_';
	snprintf(out, cap, "%s.gone", tmp);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Inventory                                                                */

void kbuild_snap_dir(const char *root, const char *dir_name, char *out,
		     size_t cap)
{
	snprintf(out, cap, "%s/%s", root, dir_name);
}

/* A file beside the manifest that the manifest names. A name with a '/' in
 * it would reach outside the snapshot directory, so it is refused. */
static int member_present(const char *dir, const char *name)
{
	if (!*name || strchr(name, '/') || !strcmp(name, ".") ||
	    !strcmp(name, ".."))
		return 0;
	char p[1024];
	snprintf(p, sizeof(p), "%s/%s", dir, name);
	return kb_path_exists(p) && !kb_is_dir(p);
}

int kbuild_snap_load_dir(const char *root, const char *dir_name,
			 KbuildSnapshot *sn)
{
	memset(sn, 0, sizeof(*sn));
	kb_strlcpy(sn->dir_name, dir_name, sizeof(sn->dir_name));
	sn->held = !strncmp(dir_name, KBUILD_HELD_DIR "/",
			    sizeof(KBUILD_HELD_DIR));

	char dir[768];
	kbuild_snap_dir(root, dir_name, dir, sizeof(dir));

	char path[900];
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

	/* Schema 4 names its array `paths`, so a kdosbuild that knows only
	 * `entries` finds none and reads a layered snapshot as absent rather
	 * than restoring a layer as if it were the whole tree. */
	sn->schema = (int)kj_num(m, "schema", 0);
	const KjNode *entries = kj_get(m, "paths");
	int legacy = 0;
	if (!entries || entries->type != KJ_ARR) {
		entries = kj_get(m, "entries");
		legacy = 1;
		if (sn->schema >= KBUILD_SNAP_SCHEMA)
			entries = NULL;
	}
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
	sn->steps = (int)kj_num(m, "steps", 0);
	sn->total_steps = (int)kj_num(m, "total_steps", sn->steps);
	/* Snapshots written before schema 3 are always whole-phase snapshots. */
	sn->complete = kj_bool(m, "complete", 1);

	if (legacy) {
		snprintf(sn->id, sizeof(sn->id), "legacy-%.60s-%lld",
			 sn->phase_dir, (long long)(sn->created * 10));
	} else {
		const char *id = kj_str(m, "id", "");
		if (!*id || strlen(id) >= sizeof(sn->id)) {
			kj_free(m);
			return -1;
		}
		kb_strlcpy(sn->id, id, sizeof(sn->id));
	}

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
		slot->tree_bytes = (long long)kj_num(e, "tree_bytes",
						     (double)slot->bytes_raw);
		slot->tree_files = (long long)kj_num(e, "tree_files",
						     (double)slot->files);
		if (!legacy) {
			const char *kind = kj_str(e, "kind", "");
			if (!strcmp(kind, "layer"))
				slot->layer = 1;
			else if (strcmp(kind, "full"))
				goto absent;
			kb_strlcpy(slot->base_id, kj_str(e, "base", ""),
				   sizeof(slot->base_id));
			kb_strlcpy(slot->removed, kj_str(e, "removed", ""),
				   sizeof(slot->removed));
			slot->removed_count =
				(long long)kj_num(e, "removed_count", 0);
			if (slot->layer && !slot->base_id[0])
				goto absent;
		}

		/* An entry whose archive or removal list is gone means the
		 * snapshot is INCOMPLETE, and an incomplete snapshot is
		 * treated as absent rather than restored from partially. */
		if (!kbuild_safe_relpath(slot->path) ||
		    !member_present(dir, slot->archive) ||
		    (slot->removed[0] && !member_present(dir, slot->removed)))
			goto absent;
		sn->nentries++;
	}

	kj_free(m);
	return 0;

absent:
	kj_free(m);
	return -1;
}

int kbuild_snap_load(const char *root, const char *dir_name, KbuildSnapshot *sn)
{
	return kbuild_snap_load_dir(root, dir_name, sn);
}

int kbuild_snap_list_all(const char *root, KbuildSnapshot *out, int max)
{
	int n = 0;
	char **names = kb_listdir(root, NULL);
	if (names) {
		for (char **e = names; *e && n < max; e++) {
			if ((*e)[0] == '.')
				continue;
			char dir[768];
			kbuild_snap_dir(root, *e, dir, sizeof(dir));
			if (!kb_is_dir(dir))
				continue;
			if (kbuild_snap_load_dir(root, *e, &out[n]) == 0)
				n++;
		}
		kb_strv_free(names);
	}

	char held[768];
	snprintf(held, sizeof(held), "%s/%s", root, KBUILD_HELD_DIR);
	names = kb_listdir(held, NULL);
	if (names) {
		for (char **e = names; *e && n < max; e++) {
			char rel[192];
			snprintf(rel, sizeof(rel), "%s/%.180s", KBUILD_HELD_DIR,
				 *e);
			char dir[1024];
			kbuild_snap_dir(root, rel, dir, sizeof(dir));
			if (!kb_is_dir(dir))
				continue;
			if (kbuild_snap_load_dir(root, rel, &out[n]) == 0)
				n++;
		}
		kb_strv_free(names);
	}
	return n;
}

int kbuild_snap_list(const char *root, KbuildSnapshot *out, int max)
{
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(root, all, KBUILD_MAX_SNAPS);
	int n = 0;
	for (int i = 0; i < nall && n < max; i++)
		if (!all[i].held && kbuild_snap_usable(all, nall, &all[i]))
			out[n++] = all[i];
	free(all);
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

const KbuildSnapshot *kbuild_snap_by_id(const KbuildSnapshot *all, int n,
					const char *id)
{
	if (!id || !*id)
		return NULL;
	for (int i = 0; i < n; i++)
		if (!strcmp(all[i].id, id))
			return &all[i];
	return NULL;
}

const KbuildSnapEntry *kbuild_snap_entry(const KbuildSnapshot *sn,
					 const char *path)
{
	for (int i = 0; i < sn->nentries; i++)
		if (!strcmp(sn->entry[i].path, path))
			return &sn->entry[i];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Chains                                                                   */

int kbuild_snap_chain(const KbuildSnapshot *all, int n,
		      const KbuildSnapshot *top, const char *path,
		      const KbuildSnapshot **out, int max)
{
	const KbuildSnapshot *rev[KBUILD_MAX_CHAIN];
	int len = 0;
	for (const KbuildSnapshot *cur = top; cur;) {
		if (len == KBUILD_MAX_CHAIN || len == max)
			return -1;
		for (int k = 0; k < len; k++)
			if (rev[k] == cur)
				return -1;	/* a loop of bases */
		rev[len++] = cur;
		const KbuildSnapEntry *e = kbuild_snap_entry(cur, path);
		if (!e)
			return -1;
		if (!e->layer)
			break;
		cur = kbuild_snap_by_id(all, n, e->base_id);
		if (!cur)
			return -1;
	}
	for (int i = 0; i < len; i++)
		out[i] = rev[len - 1 - i];
	return len;
}

int kbuild_snap_usable(const KbuildSnapshot *all, int n,
		       const KbuildSnapshot *sn)
{
	const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
	for (int i = 0; i < sn->nentries; i++)
		if (kbuild_snap_chain(all, n, sn, sn->entry[i].path, chain,
				      KBUILD_MAX_CHAIN) < 0)
			return 0;
	return 1;
}

/* Follows bases from `sn` for every path without needing the chain to be
 * whole, so a snapshot whose chain is broken further down still counts as
 * needing what it can reach. Each snapshot reached is marked in `reached`. */
static void walk_bases(const KbuildSnapshot *all, int n,
		       const KbuildSnapshot *sn, char *reached)
{
	for (int i = 0; i < sn->nentries; i++) {
		const KbuildSnapshot *cur = sn;
		const char *path = sn->entry[i].path;
		for (int step = 0; cur && step < KBUILD_MAX_CHAIN; step++) {
			const KbuildSnapEntry *e = kbuild_snap_entry(cur, path);
			if (!e || !e->layer)
				break;
			cur = kbuild_snap_by_id(all, n, e->base_id);
			if (cur)
				reached[cur - all] = 1;
		}
	}
}

int kbuild_snap_dependants(const KbuildSnapshot *all, int n, const char *id,
			   int *out, int max)
{
	char *reached = kb_calloc((size_t)(n ? n : 1), 1);
	int count = 0;
	for (int i = 0; i < n && count < max; i++) {
		if (!strcmp(all[i].id, id))
			continue;
		memset(reached, 0, (size_t)n);
		walk_bases(all, n, &all[i], reached);
		for (int k = 0; k < n; k++)
			if (reached[k] && !strcmp(all[k].id, id)) {
				out[count++] = i;
				break;
			}
	}
	free(reached);
	return count;
}

int kbuild_snap_gc_set(const KbuildSnapshot *all, int n, int *out, int max)
{
	char *reached = kb_calloc((size_t)(n ? n : 1), 1);
	for (int i = 0; i < n; i++)
		if (!all[i].held)
			walk_bases(all, n, &all[i], reached);
	int count = 0;
	for (int i = 0; i < n && count < max; i++)
		if (all[i].held && !reached[i])
			out[count++] = i;
	free(reached);
	return count;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Paths                                                                    */

int kbuild_snap_exclude_match(const KbuildPhase *p, const char *relpath)
{
	for (int i = 0; i < p->nexclude; i++)
		if (fnmatch(p->snap_exclude[i], relpath, 0) == 0)
			return 1;
	return 0;
}

static int walk_rank(unsigned char c)
{
	return c == 0 ? 0 : c == '/' ? 1 : (int)c + 1;
}

int kbuild_snap_path_cmp(const char *a, const char *b)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	while (*x && *x == *y) {
		x++;
		y++;
	}
	return walk_rank(*x) - walk_rank(*y);
}

int kbuild_snap_removal_ok(const char *path, const char *entry)
{
	if (!kbuild_safe_relpath(entry))
		return 0;
	size_t pl = strlen(path);
	if (strncmp(entry, path, pl) || (entry[pl] && entry[pl] != '/'))
		return 0;
	for (const char *s = entry; s;) {
		const char *slash = strchr(s, '/');
		size_t len = slash ? (size_t)(slash - s) : strlen(s);
		if (!len || (len == 1 && s[0] == '.'))
			return 0;
		s = slash ? slash + 1 : NULL;
	}
	return 1;
}

/* 1 when `child` lies inside `dir`. */
static int path_under(const char *dir, const char *child)
{
	size_t n = strlen(dir);
	return !strncmp(dir, child, n) && child[n] == '/';
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The index                                                                */

void kbuild_snap_idx_file(const char *build_dir, const char *path, char *out,
			  size_t cap)
{
	char tmp[256];
	kb_strlcpy(tmp, path, sizeof(tmp));
	for (char *c = tmp; *c; c++)
		if (*c == '/')
			*c = '_';
	snprintf(out, cap, "%s/%s/%s.idx", build_dir, KBUILD_LINEAGE_DIR, tmp);
}

void kbuild_snap_idx_add(KbuildSnapIndex *ix, unsigned mode,
			 unsigned long long ino, long long ctime_ns,
			 long long alloc, const char *path)
{
	if (ix->n == ix->cap) {
		ix->cap = ix->cap ? ix->cap * 2 : 4096;
		ix->rec = kb_realloc(ix->rec, ix->cap * sizeof(*ix->rec));
	}
	KbuildIdxRec *r = &ix->rec[ix->n++];
	r->mode = mode;
	r->ino = ino;
	r->ctime_ns = ctime_ns;
	r->alloc = alloc;
	r->off = ix->pool.n;
	kb_buf_add(&ix->pool, path, strlen(path) + 1);
}

void kbuild_snap_idx_free(KbuildSnapIndex *ix)
{
	free(ix->rec);
	kb_buf_free(&ix->pool);
	memset(ix, 0, sizeof(*ix));
}

/* One '\n'-terminated header line starting with `key` and a space. */
static const char *header_line(const char *p, const char *end, const char *key,
			       char *val, size_t cap)
{
	const char *nl = memchr(p, '\n', (size_t)(end - p));
	if (!nl)
		return NULL;
	size_t line = (size_t)(nl - p), kl = strlen(key);
	if (line <= kl || strncmp(p, key, kl) || p[kl] != ' ')
		return NULL;
	size_t vl = line - kl - 1;
	if (vl >= cap)
		return NULL;
	memcpy(val, p + kl + 1, vl);
	val[vl] = 0;
	return nl + 1;
}

int kbuild_snap_idx_read(const char *file, KbuildSnapIndex *ix)
{
	memset(ix, 0, sizeof(*ix));
	size_t len = 0;
	char *text = kb_read_all(file, &len);
	if (!text)
		return -1;
	const char *p = text, *end = text + len;
	char v[256];

	p = header_line(p, end, "kdos-snap-index", v, sizeof(v));
	if (!p || strcmp(v, "1"))
		goto bad;
	p = header_line(p, end, "head", ix->head, sizeof(ix->head));
	if (!p || !ix->head[0])
		goto bad;
	p = header_line(p, end, "phase", v, sizeof(v));
	if (!p || sscanf(v, "%d %63s", &ix->phase_index, ix->phase_dir) != 2)
		goto bad;
	p = header_line(p, end, "partial", v, sizeof(v));
	if (!p || (strcmp(v, "0") && strcmp(v, "1")))
		goto bad;
	ix->partial = v[0] == '1';
	p = header_line(p, end, "root", v, sizeof(v));
	if (!p || sscanf(v, "%llu %llu", &ix->root_dev, &ix->root_ino) != 2)
		goto bad;

	while (p < end) {
		const char *nul = memchr(p, '\0', (size_t)(end - p));
		const char *tab = nul ? memchr(p, '\t', (size_t)(nul - p)) : NULL;
		if (!tab || tab + 1 == nul)
			goto bad;
		unsigned mode;
		unsigned long long ino;
		long long ct, alloc;
		char meta[128];
		size_t ml = (size_t)(tab - p);
		if (ml >= sizeof(meta))
			goto bad;
		memcpy(meta, p, ml);
		meta[ml] = 0;
		if (sscanf(meta, "%o %llu %lld %lld", &mode, &ino, &ct,
			   &alloc) != 4)
			goto bad;
		const char *path = tab + 1;
		if (ix->n && kbuild_snap_path_cmp(kbuild_idx_path(ix, ix->n - 1),
						  path) >= 0)
			goto bad;	/* out of walk order: not ours */
		kbuild_snap_idx_add(ix, mode, ino, ct, alloc, path);
		p = nul + 1;
	}
	free(text);
	return 0;

bad:
	free(text);
	kbuild_snap_idx_free(ix);
	return -1;
}

int kbuild_snap_idx_write(const char *file, const KbuildSnapIndex *ix)
{
	KbBuf b = {0};
	kb_buf_printf(&b, "kdos-snap-index 1\nhead %s\nphase %d %s\n"
		      "partial %d\nroot %llu %llu\n", ix->head,
		      ix->phase_index, ix->phase_dir[0] ? ix->phase_dir : "-",
		      ix->partial ? 1 : 0, ix->root_dev, ix->root_ino);
	for (size_t i = 0; i < ix->n; i++) {
		const KbuildIdxRec *r = &ix->rec[i];
		kb_buf_printf(&b, "%o %llu %lld %lld\t", r->mode, r->ino,
			      r->ctime_ns, r->alloc);
		const char *path = kbuild_idx_path(ix, i);
		kb_buf_add(&b, path, strlen(path) + 1);
	}

	char tmp[1100];
	snprintf(tmp, sizeof(tmp), "%s.tmp", file);
	int rc = kb_write_all(tmp, b.p ? b.p : "", b.n);
	kb_buf_free(&b);
	if (rc < 0 || rename(tmp, file) < 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The diff                                                                 */

void kbuild_snap_diff(const KbuildSnapIndex *base, const KbuildSnapIndex *cur,
		      KbBuf *add, KbBuf *gone, KbuildSnapDiff *st)
{
	memset(st, 0, sizeof(*st));
	char *mark = kb_calloc(cur->n ? cur->n : 1, 1);
	/* The directories of `cur` enclosing the merge position, innermost
	 * last; the top is the parent of whatever is compared next. */
	size_t *stack = kb_calloc(cur->n ? cur->n : 1, sizeof(*stack));
	size_t depth = 0;
	const char *last_gone = NULL;

	size_t i = 0, j = 0;
	while (i < cur->n || j < base->n) {
		const char *cp = i < cur->n ? kbuild_idx_path(cur, i) : NULL;
		const char *bp = j < base->n ? kbuild_idx_path(base, j) : NULL;
		int c = !cp ? 1 : !bp ? -1 : kbuild_snap_path_cmp(cp, bp);
		const char *here = c > 0 ? bp : cp;

		while (depth && !path_under(kbuild_idx_path(cur,
							    stack[depth - 1]),
					    here))
			depth--;
		long parent = depth ? (long)stack[depth - 1] : -1;

		if (c > 0) {
			/* Removed. Its directory is marked, and only the
			 * top of a removed subtree is listed. */
			if (parent >= 0)
				mark[parent] = 1;
			if (!last_gone || !path_under(last_gone, bp)) {
				kb_buf_add(gone, bp, strlen(bp) + 1);
				st->removed++;
				last_gone = bp;
			}
			j++;
			continue;
		}

		int changed = 1;
		if (c == 0) {
			const KbuildIdxRec *a = &base->rec[j], *b = &cur->rec[i];
			if (a->mode != b->mode) {
				kb_buf_add(gone, cp, strlen(cp) + 1);
				st->removed++;
				last_gone = cp;
			} else if (a->ino == b->ino &&
				   a->ctime_ns == b->ctime_ns) {
				changed = 0;
			}
			j++;
		}
		if (changed) {
			mark[i] = 1;
			if (parent >= 0)
				mark[parent] = 1;
		}
		if (cur->rec[i].mode == S_IFDIR)
			stack[depth++] = i;
		i++;
	}

	if (cur->n)
		mark[0] = 1;
	for (size_t k = 0; k < cur->n; k++) {
		if (!mark[k])
			continue;
		const char *p = kbuild_idx_path(cur, k);
		kb_buf_add(add, p, strlen(p) + 1);
		st->listed++;
		st->listed_alloc += cur->rec[k].alloc;
	}
	free(stack);
	free(mark);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Restore selection
 *
 * Newest-wins per path, then expanded into its chain: restoring 60_kernel
 * takes `fs` from 60_kernel and `cross` and `mark` from 10_bootstrap, the last
 * phase that declares them, and each of those becomes the full archive at the
 * bottom of its chain followed by every layer up to the chosen snapshot. */

int kbuild_snap_plan_restore(const char *root, const KbuildPhase *ph, int nph,
			     int target_index, KbuildRestoreItem *out, int max)
{
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(root, all, KBUILD_MAX_SNAPS);

	/* The newest usable phase snapshot for each path. */
	char path[KBUILD_MAX_PATHS][128];
	const KbuildSnapshot *top[KBUILD_MAX_PATHS];
	int npath = 0;
	for (int i = 0; i < nph; i++) {
		if (ph[i].index > target_index)
			break;
		const KbuildSnapshot *sn = NULL;
		for (int k = 0; k < nall && !sn; k++)
			if (!all[k].held &&
			    !strcmp(all[k].dir_name, ph[i].dir_name))
				sn = &all[k];
		if (!sn || !kbuild_snap_usable(all, nall, sn))
			continue;
		for (int k = 0; k < sn->nentries; k++) {
			int at = -1;
			for (int j = 0; j < npath; j++)
				if (!strcmp(path[j], sn->entry[k].path)) {
					at = j;
					break;
				}
			if (at < 0) {
				if (npath == KBUILD_MAX_PATHS)
					continue;
				at = npath++;
				kb_strlcpy(path[at], sn->entry[k].path,
					   sizeof(path[at]));
			}
			top[at] = sn;
		}
	}

	/* Path order, not discovery order: extraction order is observable in
	 * the progress UI and has to be the same on every run. */
	for (int a = 1; a < npath; a++) {
		char key[128];
		const KbuildSnapshot *kt = top[a];
		kb_strlcpy(key, path[a], sizeof(key));
		int k = a - 1;
		while (k >= 0 && strcmp(path[k], key) > 0) {
			memcpy(path[k + 1], path[k], sizeof(path[0]));
			top[k + 1] = top[k];
			k--;
		}
		memcpy(path[k + 1], key, sizeof(path[0]));
		top[k + 1] = kt;
	}

	int n = 0;
	const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
	for (int p = 0; p < npath; p++) {
		int len = kbuild_snap_chain(all, nall, top[p], path[p], chain,
					    KBUILD_MAX_CHAIN);
		if (len < 0 || n + len > max)
			continue;
		for (int s = 0; s < len; s++) {
			const KbuildSnapshot *sn = chain[s];
			const KbuildSnapEntry *ent = kbuild_snap_entry(sn, path[p]);
			KbuildRestoreItem *it = &out[n++];
			memset(it, 0, sizeof(*it));
			kb_strlcpy(it->path, path[p], sizeof(it->path));
			snprintf(it->archive, sizeof(it->archive), "%s/%s/%s",
				 root, sn->dir_name, ent->archive);
			if (ent->removed[0])
				snprintf(it->removed, sizeof(it->removed),
					 "%s/%s/%s", root, sn->dir_name,
					 ent->removed);
			kb_strlcpy(it->source, sn->phase_dir, sizeof(it->source));
			kb_strlcpy(it->id, sn->id, sizeof(it->id));
			kb_strlcpy(it->codec, sn->codec, sizeof(it->codec));
			it->layer = ent->layer;
			it->seq = s;
			it->nseq = len;
			it->complete = sn->complete;
			it->bytes_compressed = ent->bytes_compressed;
			it->bytes_raw = ent->bytes_raw;
			it->files = ent->files;
		}
	}
	free(all);
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
	/* An interrupted restore names its target, so `{}` is not a marker: an
	 * empty object means no restore to resume and the build goes on. */
	if (!m || m->type != KJ_OBJ || !m->child) {
		kj_free(m);
		return 0;		/* unparsable reads as absent */
	}
	if (target && cap)
		kb_strlcpy(target, kj_str(m, "target", ""), cap);
	kj_free(m);
	return 1;
}

/* /proc/mounts, longest first. script/chroot/exec.sh mounts in a private
 * namespace and leaves nothing here, so a mount found under build/ belongs to
 * something else; it has to be released before build/ can be deleted, and a
 * snapshot taken over a live bind mount would archive the host's /dev. */
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
