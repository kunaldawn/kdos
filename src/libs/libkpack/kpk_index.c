/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   PACKAGES — one signature over a whole catalogue
 * ---------------------------------
 *
 * The index carries every pack's sha256, so ONE signature over the index
 * covers all of them transitively. That is kpkg's argument for signing an
 * index rather than 353 packages, and it is the same here: a per-pack sidecar
 * exists only for the pack that travels on a stick with no index beside it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kpack.h"
#include "kpkg.h"

static int ent_cmp(const void *a, const void *b)
{
	return strcmp(((const KpkIndexEnt *)a)->id, ((const KpkIndexEnt *)b)->id);
}

static void stanza_end(KpkIndex *ix, KpkIndexEnt *e)
{
	/* A stanza with no id or no hash is DROPPED. Recording half of one
	 * would put an entry in the catalogue that nothing can verify, and the
	 * verification is the only reason the index is worth having. */
	if (!e->id[0] || !e->sha256[0]) {
		memset(e, 0, sizeof(*e));
		return;
	}
	if (ix->n < KPK_INDEX_MAX)
		ix->ent[ix->n++] = *e;
	else
		ix->truncated = 1;
	memset(e, 0, sizeof(*e));
}

int kpk_index_load(KpkIndex *ix, const char *path)
{
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	KpkIndexEnt e = {0};
	size_t off = 0;

	memset(ix, 0, sizeof(*ix));
	if (!text)
		return -1;

	while (off <= len) {
		char line[1024];
		size_t end = off, n;

		while (end < len && text[end] != '\n')
			end++;
		n = end - off;
		if (n >= sizeof(line))
			n = 0;			/* an over-long line is noise */
		memcpy(line, text + off, n);
		line[n] = 0;
		line[strcspn(line, "\r")] = 0;

		if (!line[0]) {
			stanza_end(ix, &e);
		} else if (line[1] == ':') {
			const char *v = line + 2;
			switch (line[0]) {
			case 'P': kb_strlcpy(e.id, v, sizeof(e.id)); break;
			case 'V': {
				/* `1.2.3-1` — the release is after the LAST
				 * dash, because a version may carry one. */
				const char *d = strrchr(v, '-');
				if (d) {
					size_t vn = (size_t)(d - v);
					if (vn >= sizeof(e.version))
						vn = sizeof(e.version) - 1;
					memcpy(e.version, v, vn);
					e.version[vn] = 0;
					kb_strlcpy(e.release, d + 1,
						   sizeof(e.release));
				} else {
					kb_strlcpy(e.version, v, sizeof(e.version));
					kb_strlcpy(e.release, "1", sizeof(e.release));
				}
				break;
			}
			case 'A': kb_strlcpy(e.arch, v, sizeof(e.arch)); break;
			case 'K': kb_strlcpy(e.kind, v, sizeof(e.kind)); break;
			case 'F': kb_strlcpy(e.file, v, sizeof(e.file)); break;
			case 'C': if (strlen(v) == 64)
					kb_strlcpy(e.sha256, v, sizeof(e.sha256));
				  break;
			case 'O': kb_strlcpy(e.from, v, sizeof(e.from)); break;
			case 'S': e.size = strtoull(v, NULL, 10); break;
			case 'R': e.recommended = !strcmp(v, "yes"); break;
			case 'T': kb_strlcpy(e.summary, v, sizeof(e.summary)); break;
			case 'D': kb_strlcpy(e.requires, v, sizeof(e.requires));
				  break;
			default: break;
			}
		}
		if (end >= len)
			break;
		off = end + 1;
	}
	stanza_end(ix, &e);
	free(text);
	return ix->n;
}

/*
 * The NEWEST entry with that id. An index legitimately carries several
 * versions of one pack — that is what makes rollback a file that is still
 * there — so a lookup that returned whichever came first would hand back an
 * old version whenever the sort happened to put it there.
 */
const KpkIndexEnt *kpk_index_find(const KpkIndex *ix, const char *id)
{
	const KpkIndexEnt *best = NULL;

	for (int i = 0; i < ix->n; i++) {
		const KpkIndexEnt *e = &ix->ent[i];
		if (strcmp(e->id, id))
			continue;
		/* A delta is not a route to a pack you do not have; `find` is
		 * asked "where is this pack", and the answer is a whole one. */
		if (e->from[0])
			continue;
		if (!best || kp_vercmp(e->version, best->version) > 0)
			best = e;
	}
	return best;
}

/* The delta that reconstructs `id` at `version` from a pack you still have. */
const KpkIndexEnt *kpk_index_delta(const KpkIndex *ix, const char *id,
				   const char *version, const char *havefile)
{
	for (int i = 0; i < ix->n; i++) {
		const KpkIndexEnt *e = &ix->ent[i];
		if (!e->from[0] || strcmp(e->id, id))
			continue;
		if (version && strcmp(e->version, version))
			continue;
		if (havefile && strcmp(e->from, havefile))
			continue;
		return e;
	}
	return NULL;
}

int kpk_index_write(const KpkIndex *ix, const char *path)
{
	KpkIndex s = *ix;
	KbBuf b = {0};
	int rc;

	/* Sorted, so the same set of packs is always the same bytes and
	 * therefore the same signature: an index that reordered itself with
	 * the filesystem would need re-signing for no change. */
	qsort(s.ent, (size_t)s.n, sizeof(s.ent[0]), ent_cmp);
	for (int i = 0; i < s.n; i++) {
		const KpkIndexEnt *e = &s.ent[i];
		kb_buf_printf(&b, "P:%s\n", e->id);
		kb_buf_printf(&b, "V:%s-%s\n", e->version,
			      e->release[0] ? e->release : "1");
		kb_buf_printf(&b, "A:%s\n", e->arch[0] ? e->arch : "x86_64");
		if (e->kind[0])
			kb_buf_printf(&b, "K:%s\n", e->kind);
		kb_buf_printf(&b, "S:%llu\n", e->size);
		kb_buf_printf(&b, "C:%s\n", e->sha256);
		kb_buf_printf(&b, "F:%s\n", e->file);
		if (e->from[0])
			kb_buf_printf(&b, "O:%s\n", e->from);
		if (e->recommended)
			kb_buf_str(&b, "R:yes\n");
		if (e->summary[0])
			kb_buf_printf(&b, "T:%s\n", e->summary);
		if (e->requires[0])
			kb_buf_printf(&b, "D:%s\n", e->requires);
		kb_buf_str(&b, "\n");
	}
	rc = kb_write_all(path, b.p ? b.p : "", b.n);
	kb_buf_free(&b);
	return rc;
}

KpkSigState kpk_index_verify(const char *path, const KsigRing *ring,
			     char who[KSIG_ID_HEX])
{
	char sig[KPK_PATH * 2];
	size_t len = 0;
	char *text;
	int rc;

	if (who)
		who[0] = 0;
	text = kb_read_all(path, &len);
	if (!text)
		return KPK_SIG_HASH;	/* no index at all is not "unsigned" */

	snprintf(sig, sizeof(sig), "%s.sig", path);
	if (!kb_path_exists(sig)) {
		free(text);
		return KPK_SIG_NONE;
	}
	rc = ksig_verify_file(ring, sig, text, len, who);
	free(text);
	return rc == 0 ? KPK_SIG_GOOD : KPK_SIG_BAD;
}
