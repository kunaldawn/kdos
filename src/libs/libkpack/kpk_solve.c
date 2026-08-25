/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the solve — what has to be mounted before what
 * ---------------------------------
 *
 * A stack is composed lowest-first, so the order this produces is the order
 * `lowerdir=` reads in reverse: dependencies come out first and the pack that
 * asked for them last.
 *
 * The version comparison is kp_vercmp out of libkpkg, which is already the one
 * implementation of "is this version newer" that kpkg and kdos-portup share. A
 * third would eventually disagree with the other two, and the disagreement
 * would show up as a runtime that refuses to satisfy an app for no visible
 * reason.
 */

#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "kpack.h"
#include "kpkg.h"

int kpk_req_met(const KpkReq *r, const KpkMeta *m)
{
	int by_id = !strcmp(r->name, m->id);
	int cmp;

	if (!by_id) {
		int found = 0;
		for (int i = 0; i < m->nprov; i++)
			if (!strcmp(r->name, m->provides[i])) {
				found = 1;
				break;
			}
		if (!found)
			return 0;
	}
	if (!r->op[0])
		return 1;

	cmp = kp_vercmp(m->version, r->ver);
	if (!strcmp(r->op, ">="))	return cmp >= 0;
	if (!strcmp(r->op, ">"))	return cmp > 0;
	if (!strcmp(r->op, "="))	return cmp == 0;
	if (!strcmp(r->op, "<="))	return cmp <= 0;
	if (!strcmp(r->op, "<"))	return cmp < 0;
	return 0;
}

static int find_provider(const KpkMeta *const *avail, int navail,
			 const KpkReq *r)
{
	int best = -1;

	for (int i = 0; i < navail; i++) {
		if (!kpk_req_met(r, avail[i]))
			continue;
		/* Several packs may provide one name — the newest wins, which
		 * is the same rule the layered snapshot restore keeps. */
		if (best < 0 ||
		    kp_vercmp(avail[i]->version, avail[best]->version) > 0)
			best = i;
	}
	return best;
}

struct solve {
	const KpkMeta *const *avail;
	int navail;
	int *order;
	int max;
	int n;
	char *err;
	size_t errcap;
	unsigned char *state;	/* 0 unseen, 1 in progress, 2 emitted */
};

static int visit(struct solve *s, int idx, const char *asked_by)
{
	const KpkMeta *m = s->avail[idx];

	if (s->state[idx] == 2)
		return 0;
	if (s->state[idx] == 1) {
		/*
		 * A cycle is refused rather than broken at an arbitrary edge.
		 * Two packs that require each other have no valid lowerdir
		 * order, and picking one would produce a stack whose shadowing
		 * depended on which was named first.
		 */
		snprintf(s->err, s->errcap, "%s: requirement cycle through %s",
			 m->id, asked_by ? asked_by : "itself");
		return -1;
	}
	s->state[idx] = 1;

	for (int i = 0; i < m->nreq; i++) {
		int p = find_provider(s->avail, s->navail, &m->req[i]);
		if (p < 0) {
			const KpkReq *r = &m->req[i];
			if (r->op[0])
				snprintf(s->err, s->errcap,
					 "%s requires %s %s %s, which nothing here provides",
					 m->id, r->name, r->op, r->ver);
			else
				snprintf(s->err, s->errcap,
					 "%s requires %s, which nothing here provides",
					 m->id, r->name);
			return -1;
		}
		if (visit(s, p, m->id) != 0)
			return -1;
	}

	if (s->n >= s->max) {
		snprintf(s->err, s->errcap, "the stack is deeper than %d packs",
			 s->max);
		return -1;
	}
	s->order[s->n++] = idx;
	s->state[idx] = 2;
	return 0;
}

int kpk_solve(const KpkMeta *const *avail, int navail, const char *const *want,
	      int nwant, int *order, int max, char *err, size_t errcap)
{
	unsigned char state[KPK_INDEX_MAX] = {0};
	struct solve s = { avail, navail, order, max, 0, err, errcap, state };

	if (errcap)
		err[0] = 0;
	if (navail > KPK_INDEX_MAX) {
		snprintf(err, errcap, "more than %d packs to solve over",
			 KPK_INDEX_MAX);
		return -1;
	}

	for (int w = 0; w < nwant; w++) {
		int idx = -1;
		for (int i = 0; i < navail; i++)
			if (!strcmp(avail[i]->id, want[w])) {
				idx = i;
				break;
			}
		if (idx < 0) {
			/* Falling back to `provides` here would let `kdos app
			 * install gimp` silently install something else that
			 * claims the name. A request names an id. */
			snprintf(err, errcap, "no pack with id %s", want[w]);
			return -1;
		}
		if (visit(&s, idx, NULL) != 0)
			return -1;
	}
	return s.n;
}
