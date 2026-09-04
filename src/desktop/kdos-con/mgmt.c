/* kdos-con — the window list, out to the shell. See con.h.
 *
 * PUBLISHED BY DIFF, not by hooking every place window state changes.
 *
 * A window is raised, minimised, maximised, sent to another workspace, given a
 * new title by the program inside it, closed by a chord, closed by its own
 * client, garbage-collected after its client went away — and every one of
 * those is a place a `kcon_mgmt_*` call could have been put and forgotten. A
 * snapshot compared against the last one has ONE place to be wrong, and it
 * cannot miss a path that did not exist when it was written.
 *
 * It runs once per turn of the session loop, which is the same cadence the
 * frame goes out at: a panel cannot see a change faster than it can be drawn.
 */

#include <stdio.h>
#include <string.h>

#include "con.h"

typedef struct {
	int id;
	unsigned flags;
	int workspace;
	char title[128];
} Snap;

static Snap prev[64];
static int nprev;
static int prev_ws = -1, prev_nws = -1;
static unsigned prev_occ;

/*
 * WHICH WINDOWS A SHELL IS TOLD ABOUT. The same set the taskbar draws: a
 * layer, a guest's placeholder and a surface that says it has nothing to show
 * are not things a person switches between, and a panel that listed them would
 * offer rows that do nothing.
 */
static int listed(const Win *w)
{
	if (w->panel || w->overlay || w->background)
		return 0;
	if (w == S.saver || w == S.lock)
		return 0;
	if (w->kind == WIN_SURFACE && w->surf &&
	    kcon_surface_hidden(w->surf))
		return 0;
	return 1;
}

static unsigned flags_of(const Win *w)
{
	unsigned f = 0;

	if (w->id == S.focus)
		f |= KCON_TL_FOCUSED;
	if (w->minimised)
		f |= KCON_TL_MINIMISED;
	if (w->tiled == KWM_EDGES_CARDINAL)
		f |= KCON_TL_MAXIMISED;
	if (w->full)
		f |= KCON_TL_FULLSCREEN;
	return f;
}

void mgmt_publish(int force)
{
	Snap cur[64];
	int n = 0;

	for (Win *w = S.wins; w && n < 64; w = w->next) {
		if (!listed(w))
			continue;
		cur[n].id = w->id;
		cur[n].flags = flags_of(w);
		cur[n].workspace = w->workspace;
		snprintf(cur[n].title, sizeof(cur[n].title), "%s", w->title);
		n++;
	}

	/* Gone, before added: an id reused inside one turn would otherwise be
	 * removed after it was added. Nothing reuses one today — `S.next_id`
	 * only counts up — and the order costs nothing to keep right. */
	for (int i = 0; i < nprev; i++) {
		int still = 0;

		for (int j = 0; j < n; j++)
			if (cur[j].id == prev[i].id) {
				still = 1;
				break;
			}
		if (!still)
			kcon_mgmt_remove(S.server, (unsigned)prev[i].id);
	}

	for (int i = 0; i < n; i++) {
		const Snap *was = NULL;

		for (int j = 0; j < nprev; j++)
			if (prev[j].id == cur[i].id) {
				was = &prev[j];
				break;
			}

		Win *w = win_find(cur[i].id);

		if (!was || force) {
			kcon_mgmt_add(S.server, (unsigned)cur[i].id,
				      w ? w->app_id : "", cur[i].title);
			kcon_mgmt_state(S.server, (unsigned)cur[i].id,
					cur[i].flags, cur[i].workspace);
			continue;
		}

		/*
		 * A TITLE CHANGE IS AN ADD AGAIN, because the protocol has no
		 * message for it: ADD carries the strings and STATE does not.
		 * The client keeps the first entry for an id it already has,
		 * so a re-ADD is how it learns the new title without the row
		 * disappearing and coming back.
		 */
		if (strcmp(was->title, cur[i].title))
			kcon_mgmt_add(S.server, (unsigned)cur[i].id,
				      w ? w->app_id : "", cur[i].title);
		if (was->flags != cur[i].flags ||
		    was->workspace != cur[i].workspace)
			kcon_mgmt_state(S.server, (unsigned)cur[i].id,
					cur[i].flags, cur[i].workspace);
	}

	memcpy(prev, cur, sizeof(Snap) * (size_t)n);
	nprev = n;

	unsigned occ = 0;

	for (Win *w = S.wins; w; w = w->next)
		if (listed(w) && !w->minimised && w->workspace >= 0 &&
		    w->workspace < 32)
			occ |= 1u << w->workspace;

	if (force || prev_ws != S.workspace || prev_nws != S.nworkspace ||
	    prev_occ != occ) {
		kcon_mgmt_workspace(S.server, S.workspace, S.nworkspace, occ);
		prev_ws = S.workspace;
		prev_nws = S.nworkspace;
		prev_occ = occ;
	}
}

/*
 * A SHELL THAT HAS JUST ATTACHED KNOWS NOTHING. It missed every ADD that went
 * out before it connected, so the whole list is sent again — which every other
 * shell also receives, and re-sending an entry a client already has is what
 * the id is for.
 */
void mgmt_resend(void)
{
	nprev = 0;
	prev_ws = prev_nws = -1;
	prev_occ = 0;
	mgmt_publish(1);
}
