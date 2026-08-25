// SPDX-License-Identifier: GPL-2.0-only
/*
 * The box chip — which box a window came from, on the frame itself.
 *
 * Every fat application on this distro runs in its own container, and the
 * panel already says so: a task button reads `GIMP (arch)` when the same
 * app_id exists in more than one box. The FRAME said nothing, so the moment a
 * window was raised over the bar the answer went with it.
 *
 * The chip is a square of the box's own accent at the left of the title area,
 * the height of the titlebar — two cells on the shipped 16x32 grid, which is
 * the size every other picture on this desktop is drawn at.
 *
 * Four decisions, and the first is what keeps it from being noise:
 *
 * - **A box with the session's own accent draws NO CHIP.** The colour comes
 *   from `accent =` in `~/.config/kdos/boxes/<name>.conf` and from nowhere
 *   else; a box that never declared one wears the session's, and a marker on
 *   every window on a machine where every window is boxed says nothing at all.
 *   So on a default install there are no chips, and the first one appears the
 *   moment somebody gives a box a colour to tell it apart by.
 * - **The width is added to the title's LEFT OFFSET**, in `get_title_offsets`,
 *   which is the single place both the title's wrapping width and its position
 *   are computed from. A chip drawn at a coordinate of its own would be
 *   correct until a title grew long enough to run under it.
 * - **It is rebuilt, never patched** — kdos-group's rule, for the same reason:
 *   it hangs off the titlebar's own subtrees and is thrown away with them, so
 *   nothing in it caches a geometry that can disagree with a resize.
 * - **The profile is read once per view**, when the box name is first resolved,
 *   and cached on the view. `ssd_update_title` fires on every resize and on
 *   every title change; a file read there would be a stat per frame of a drag.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/types/wlr_scene.h>

#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "kdos.h"
#include "labwc.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

/*
 * One entry per view that has a chip. A list rather than a field on `struct
 * view` for the reason every graft here keeps: upstream's struct is upstream's,
 * and a fork that grows fields into it is a fork that cannot be read against
 * labwc any more.
 */
struct kbc_entry {
	struct wl_list link;
	struct view *view;
	char box[64];
	float colour[4];	/* pre-multiplied, as every theme colour is */
	bool has_chip;
	/* One tree per active state, dropped and rebuilt on every update.
	 * `ssd_update_title` fires on every resize and every title change, so
	 * a chip that were merely ADDED would stack a rect per frame of a
	 * drag. kdos-group's rule, for the same reason. */
	struct wlr_scene_tree *tree[2];
};

static struct wl_list kbc_entries;
static bool kbc_ready;

static void
kbc_init(void)
{
	if (!kbc_ready) {
		wl_list_init(&kbc_entries);
		kbc_ready = true;
	}
}

static struct kbc_entry *
kbc_find(struct view *view)
{
	struct kbc_entry *e;

	kbc_init();
	wl_list_for_each(e, &kbc_entries, link) {
		if (e->view == view) {
			return e;
		}
	}
	return NULL;
}

/*
 * `accent = amber` out of the box's profile, or NULL. The profile is
 * kdos-appbox's file and this is a READER of it: the writer stays in one
 * place, which is the same rule `~/.config/kdos/favorites` keeps between the
 * panel and the window menu.
 */
static const KcolScheme *
kbc_box_accent(const char *box)
{
	char path[512], line[256];
	const char *home = getenv("HOME");
	const KcolScheme *sc = NULL;
	FILE *f;

	if (!home || !box || !*box) {
		return NULL;
	}
	/* The name arrives from a security context an application chose, so it
	 * is checked before it becomes a path component. */
	for (const char *c = box; *c; c++) {
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
		      (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' ||
		      *c == '_')) {
			return NULL;
		}
	}
	snprintf(path, sizeof(path), "%s/.config/kdos/boxes/%s.conf", home, box);
	f = fopen(path, "r");
	if (!f) {
		return NULL;
	}
	while (fgets(line, sizeof(line), f)) {
		char *eq = strchr(line, '=');
		char *k = line, *v;

		if (!eq) {
			continue;
		}
		*eq = '\0';
		v = eq + 1;
		while (*k == ' ' || *k == '\t') {
			k++;
		}
		for (char *p = k + strlen(k); p > k && (p[-1] == ' ' || p[-1] == '\t'); ) {
			*--p = '\0';
		}
		while (*v == ' ' || *v == '\t') {
			v++;
		}
		v[strcspn(v, " \t\r\n")] = '\0';
		if (!strcmp(k, "accent")) {
			sc = kcol_find(v);
			break;
		}
	}
	fclose(f);
	return sc;
}

/*
 * Resolve, once, what this view's chip is. `has_chip` false is the ordinary
 * answer: no box, no profile, no `accent =`, or an accent that is the one the
 * session is already wearing.
 */
static struct kbc_entry *
kbc_resolve(struct view *view)
{
	const char *box = kdos_view_box(view);
	struct kbc_entry *e = kbc_find(view);
	const KcolScheme *sc, *session;
	uint32_t rgb;

	if (!box || !*box) {
		return e;	/* an existing entry stays; the box cannot change */
	}
	if (e && !strcmp(e->box, box)) {
		return e;
	}
	if (!e) {
		e = znew(*e);
		e->view = view;
		wl_list_insert(&kbc_entries, &e->link);
	}
	snprintf(e->box, sizeof(e->box), "%s", box);
	e->has_chip = false;

	sc = kbc_box_accent(box);
	session = kdos_accent_scheme();
	if (!sc || !session || sc == session) {
		return e;
	}
	rgb = sc->primary;
	/*
	 * PRE-MULTIPLIED, like every colour the theme hands the scene graph:
	 * rgb above alpha is not a colour any renderer agrees about. Opaque
	 * here, so the multiply is the identity and the alpha is stated rather
	 * than left to be inferred.
	 */
	e->colour[0] = (float)((rgb >> 16) & 0xff) / 255.0f;
	e->colour[1] = (float)((rgb >> 8) & 0xff) / 255.0f;
	e->colour[2] = (float)(rgb & 0xff) / 255.0f;
	e->colour[3] = 1.0f;
	e->has_chip = true;
	return e;
}

int
kdos_boxchip_width(struct view *view)
{
	struct kbc_entry *e;

	if (!view || !rc.show_title) {
		return 0;
	}
	e = kbc_resolve(view);
	if (!e || !e->has_chip) {
		return 0;
	}
	/* A square the height of the titlebar: two cells at the shipped
	 * 16x32, and still a square on any other cell size. */
	return rc.theme->titlebar_height;
}

static void
kbc_drop(struct kbc_entry *e)
{
	for (int a = 0; a < 2; a++) {
		if (e->tree[a]) {
			wlr_scene_node_destroy(&e->tree[a]->node);
			e->tree[a] = NULL;
		}
	}
}

void
kdos_boxchip_ssd_update(struct ssd *ssd, int x)
{
	struct kbc_entry *e;
	int h;

	if (!ssd || !ssd->titlebar.tree) {
		return;
	}
	e = kbc_resolve(ssd->view);
	if (!e) {
		return;
	}
	kbc_drop(e);
	if (!e->has_chip) {
		return;
	}
	h = rc.theme->titlebar_height;

	for (int a = 0; a < 2; a++) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[a];
		struct wlr_scene_rect *chip;

		if (!subtree->tree) {
			continue;
		}
		/*
		 * Inactive windows get the same colour at a third of it. The
		 * chip is an identity, not a focus indicator, and the frame
		 * around it is already saying which window is active — but a
		 * chip at full strength on every window on the screen is four
		 * bright squares competing with the one that matters.
		 */
		float c[4] = { e->colour[0], e->colour[1], e->colour[2],
			       e->colour[3] };
		if (!a) {
			for (int i = 0; i < 4; i++) {
				c[i] *= 0.35f;
			}
		}
		e->tree[a] = lab_wlr_scene_tree_create(subtree->tree);
		wlr_scene_node_set_position(&e->tree[a]->node, x, 0);
		chip = lab_wlr_scene_rect_create(e->tree[a], h, h, c);
		wlr_scene_node_set_position(&chip->node, 0, 0);
	}
}

/*
 * From ssd_titlebar_destroy(): the titlebar tree took the children with it,
 * so only the handles are ours to drop. Freeing them here would be a double
 * destroy of nodes that are already gone.
 */
void
kdos_boxchip_ssd_clear(struct view *view)
{
	struct kbc_entry *e = kbc_find(view);

	if (e) {
		e->tree[0] = NULL;
		e->tree[1] = NULL;
	}
}

void
kdos_boxchip_forget(struct view *view)
{
	struct kbc_entry *e = kbc_find(view);

	if (e) {
		wl_list_remove(&e->link);
		free(e);
	}
}

/*
 * `kdos theme <accent>` changes the SESSION's accent, and the chip exists only
 * where the box's differs from it — so the resolved answer is dropped and
 * every view re-reads its profile on the SSD reload that follows.
 */
void
kdos_boxchip_reload(void)
{
	struct kbc_entry *e;

	kbc_init();
	wl_list_for_each(e, &kbc_entries, link) {
		e->box[0] = '\0';
		e->has_chip = false;
	}
}

void
kdos_boxchip_finish(void)
{
	struct kbc_entry *e, *tmp;

	kbc_init();
	wl_list_for_each_safe(e, tmp, &kbc_entries, link) {
		wl_list_remove(&e->link);
		free(e);
	}
}
