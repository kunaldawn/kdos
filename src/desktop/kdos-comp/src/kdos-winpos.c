// SPDX-License-Identifier: GPL-2.0-only
/*
 * Window memory — Haiku's "windows reopen where you left them".
 *
 * At unmap a view's app_id, geometry, workspace and shaded state go into
 * $XDG_STATE_HOME/kdos/winpos; at map the record for that app_id is put
 * back. It is the realistic subset of session restore: the compositor
 * cannot relaunch anything, but it can stop every application opening in
 * the middle of the screen at the size its author picked.
 *
 * Four rules, each one a way this feature usually becomes an annoyance:
 *
 * - It applies only to a view that would otherwise be PLACED by us. A
 *   client that positioned itself (an X11 window with USPosition), a
 *   window rule that placed it, a fixedPosition rule, and anything that
 *   maps maximized, tiled or fullscreen are all left alone: overriding
 *   an explicit intent is worse than having no memory at all.
 * - The restored box is clamped into the output's USABLE area — labwc
 *   already computes one, panels included. A window remembered from a
 *   1920-wide screen must not open off the edge of a 1366-wide one.
 * - The file is written the way kdos-bootctl writes the boot state:
 *   temp file, fsync the FILE, fsync the DIRECTORY, rename. The
 *   directory fsync is the step people leave out, and without it the
 *   rename can be lost while the data survives.
 * - It is capped at KW_MAX lines, most recent first. An unbounded
 *   history of every application ever run is a file nobody can read and
 *   a linear scan on every map.
 *
 * `window_memory = off` in comp.conf turns it off.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "common/list.h"
#include "common/mem.h"
#include "kdos.h"
#include "labwc.h"
#include "output.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

#define KW_MAX 200

struct kw_record {
	struct wl_list link;		/* struct kwin.records, most recent first */
	char *app_id;
	int x, y, w, h;
	int workspace;
	bool shaded;
};

struct kw {
	struct wl_list records;
	char path[512];
	char dir[512];
	bool loaded;
};

static struct kw kwin;
/*
 * A view is marked "the client placed this one" before it maps and the
 * mark is spent at map. view_destroy() drops one that was never spent,
 * so this cannot leak — and it deliberately has no ceiling: an X11
 * client that configures a dozen USPosition windows before mapping any
 * of them would lose the oldest to a fixed ring, and applying our
 * memory over a position the client asked for is the one thing this
 * file's rules say must not happen.
 */
static struct view **kw_marks;
static size_t kw_marks_len, kw_marks_cap;

/* ── the file ───────────────────────────────────────────────────── */

static bool
kw_paths(void)
{
	const char *state = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");
	if (state && *state) {
		if (snprintf(kwin.dir, sizeof(kwin.dir), "%s/kdos", state)
				>= (int)sizeof(kwin.dir)) {
			return false;
		}
	} else if (home && *home) {
		if (snprintf(kwin.dir, sizeof(kwin.dir), "%s/.local/state/kdos", home)
				>= (int)sizeof(kwin.dir)) {
			return false;
		}
	} else {
		return false;
	}
	return snprintf(kwin.path, sizeof(kwin.path), "%s/winpos", kwin.dir)
		< (int)sizeof(kwin.path);
}

static struct kw_record *
kw_find(const char *app_id)
{
	struct kw_record *r;
	wl_list_for_each(r, &kwin.records, link) {
		if (!strcmp(r->app_id, app_id)) {
			return r;
		}
	}
	return NULL;
}

static void
kw_load(void)
{
	if (kwin.loaded) {
		return;
	}
	kwin.loaded = true;
	wl_list_init(&kwin.records);
	if (!kw_paths()) {
		return;
	}

	FILE *f = fopen(kwin.path, "r");
	if (!f) {
		return;	/* absent is the normal case on a fresh system */
	}
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		char app_id[256];
		int x, y, w, h, ws, shaded;
		if (sscanf(line, "%255s %d %d %d %d %d %d", app_id, &x, &y, &w,
				&h, &ws, &shaded) != 7) {
			continue;	/* a line that does not parse is absent */
		}
		if (w <= 0 || h <= 0 || kw_find(app_id)) {
			continue;
		}
		struct kw_record *r = znew(*r);
		r->app_id = xstrdup(app_id);
		r->x = x;
		r->y = y;
		r->w = w;
		r->h = h;
		r->workspace = ws;
		r->shaded = shaded != 0;
		wl_list_append(&kwin.records, &r->link);
		if (wl_list_length(&kwin.records) >= KW_MAX) {
			break;
		}
	}
	fclose(f);
}

/*
 * The bootctl discipline. A torn winpos file costs nobody their machine,
 * but half a record that still parses is exactly how a window comes back
 * one pixel wide, so it is written whole or not at all.
 */
static void
kw_save(void)
{
	if (!*kwin.path) {
		return;
	}
	if (mkdir(kwin.dir, 0700) < 0 && errno != EEXIST) {
		wlr_log(WLR_ERROR, "winpos: cannot create %s: %s", kwin.dir,
			strerror(errno));
		return;
	}

	char tmp[540];
	snprintf(tmp, sizeof(tmp), "%s.tmp", kwin.path);
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0) {
		return;
	}
	FILE *f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		unlink(tmp);
		return;
	}
	struct kw_record *r;
	wl_list_for_each(r, &kwin.records, link) {
		fprintf(f, "%s %d %d %d %d %d %d\n", r->app_id, r->x, r->y,
			r->w, r->h, r->workspace, r->shaded ? 1 : 0);
	}
	if (fflush(f) != 0 || fsync(fd) != 0) {
		fclose(f);
		unlink(tmp);
		return;
	}
	fclose(f);
	if (rename(tmp, kwin.path) < 0) {
		unlink(tmp);
		return;
	}
	int dfd = open(kwin.dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dfd >= 0) {
		fsync(dfd);
		close(dfd);
	}
}

/* ── what may be remembered, and what may be applied ─────────────── */

static bool
kw_enabled(void)
{
	return kdos_conf.window_memory;
}

/* An app_id with whitespace in it cannot round-trip through a
 * space-separated line, and no real one has any. */
static bool
kw_usable_app_id(const char *app_id)
{
	if (!app_id || !*app_id) {
		return false;
	}
	for (const char *p = app_id; *p; p++) {
		if ((unsigned char)*p <= ' ') {
			return false;
		}
	}
	return true;
}

static bool
kw_take_mark(struct view *view)
{
	for (size_t i = 0; i < kw_marks_len; i++) {
		if (kw_marks[i] == view) {
			kw_marks[i] = kw_marks[--kw_marks_len];
			return true;
		}
	}
	return false;
}

void
kdos_winpos_client_positioned(struct view *view)
{
	for (size_t i = 0; i < kw_marks_len; i++) {
		if (kw_marks[i] == view) {
			return;	/* a second configure is the same intent */
		}
	}
	if (kw_marks_len == kw_marks_cap) {
		kw_marks_cap = kw_marks_cap ? kw_marks_cap * 2 : 8;
		kw_marks = xrealloc(kw_marks, kw_marks_cap * sizeof(*kw_marks));
	}
	kw_marks[kw_marks_len++] = view;
}

void
kdos_winpos_forget(struct view *view)
{
	kw_take_mark(view);
}

/* ── the hooks ──────────────────────────────────────────────────── */

void
kdos_winpos_record(struct view *view)
{
	if (!kw_enabled() || !kw_usable_app_id(view->app_id)) {
		return;
	}
	kw_load();
	if (!*kwin.path) {
		return;
	}

	/*
	 * A maximized, tiled or fullscreen view's `current` box is the
	 * state, not the window: what comes back after unmaximizing is
	 * natural_geometry, and that is what is worth remembering.
	 */
	struct wlr_box geo = view_is_floating(view)
		? view->current : view->natural_geometry;
	if (geo.width <= 0 || geo.height <= 0) {
		return;
	}

	int workspace = 0;
	if (view->workspace) {
		int i = 1;
		struct workspace *w;
		wl_list_for_each(w, &server.workspaces.all, link) {
			if (w == view->workspace) {
				workspace = i;
				break;
			}
			i++;
		}
	}

	struct kw_record *r = kw_find(view->app_id);
	if (r) {
		/*
		 * Nothing changed and it is already the newest record — so
		 * there is nothing to write, and the fsync this save would
		 * cost is a hitch on every window close of an application
		 * opened and closed in the same place all day.
		 */
		if (kwin.records.next == &r->link && r->x == geo.x
				&& r->y == geo.y && r->w == geo.width
				&& r->h == geo.height && r->workspace == workspace
				&& r->shaded == view->shaded) {
			return;
		}
		wl_list_remove(&r->link);
	} else {
		r = znew(*r);
		r->app_id = xstrdup(view->app_id);
	}
	r->workspace = workspace;
	r->x = geo.x;
	r->y = geo.y;
	r->w = geo.width;
	r->h = geo.height;
	r->shaded = view->shaded;
	/* most recent first, so the cap drops what nobody has opened in
	 * longest */
	wl_list_insert(&kwin.records, &r->link);

	while (wl_list_length(&kwin.records) > KW_MAX) {
		struct kw_record *last = wl_container_of(
			kwin.records.prev, last, link);
		wl_list_remove(&last->link);
		zfree(last->app_id);
		free(last);
	}
	kw_save();
}

/*
 * A record is per app_id, so without this every instance of an
 * application takes the same box and a second window opens exactly on
 * top of the first — three foots stacked pixel-for-pixel, with labwc's
 * own cascade overwritten before it could run. The origin is the test:
 * a second instance of a different size in the same corner reads as
 * just as broken.
 */
static bool
kw_origin_taken(struct view *view, const struct wlr_box *geo)
{
	struct view *other;
	wl_list_for_each(other, &server.views, link) {
		if (other == view || !other->mapped || !other->app_id
				|| other->workspace != view->workspace
				|| strcmp(other->app_id, view->app_id)) {
			continue;
		}
		if (other->current.x == geo->x && other->current.y == geo->y) {
			return true;
		}
	}
	return false;
}

static struct workspace *
workspace_by_index(int index)
{
	if (index < 1) {
		return NULL;
	}
	int i = 1;
	struct workspace *w;
	wl_list_for_each(w, &server.workspaces.all, link) {
		if (i == index) {
			return w;
		}
		i++;
	}
	return NULL;
}

void
kdos_winpos_apply(struct view *view)
{
	bool client_placed = kw_take_mark(view);
	if (!kw_enabled() || client_placed
			|| !kw_usable_app_id(view->app_id)) {
		return;
	}
	/*
	 * Anything that is not a plain floating window has geometry that
	 * belongs to its state, and a rule that pinned it has an opinion
	 * older than ours.
	 */
	if (!view_is_floating(view)
			|| window_rules_get_property(view, "fixedPosition")
				== LAB_PROP_TRUE) {
		return;
	}
	kw_load();
	struct kw_record *r = kw_find(view->app_id);
	if (!r) {
		return;
	}

	struct workspace *workspace = workspace_by_index(r->workspace);
	if (workspace && workspace != view->workspace
			&& !view->visible_on_all_workspaces) {
		view_move_to_workspace(view, workspace);
		/*
		 * view_impl_map() focused this view before we moved it, and
		 * the move only reparents the scene tree — so the keyboard
		 * would be left on a window that is no longer rendered and
		 * every keystroke would go somewhere invisible. This is
		 * labwc's own SendToDesktop follow="no" case, answered the
		 * same way.
		 */
		if (server.active_view == view) {
			desktop_focus_topmost_view();
		}
	}

	/*
	 * Clamp into the usable area of whichever output the remembered
	 * position lands nearest — a window remembered on a screen that is
	 * no longer there must still come back somewhere reachable.
	 */
	struct wlr_box geo = { .x = r->x, .y = r->y, .width = r->w, .height = r->h };
	struct output *output = output_nearest_to(geo.x + geo.width / 2,
		geo.y + geo.height / 2);
	if (!output_is_usable(output)) {
		return;
	}
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	if (geo.width > usable.width) {
		geo.width = usable.width;
	}
	if (geo.height > usable.height) {
		geo.height = usable.height;
	}
	if (geo.x + geo.width > usable.x + usable.width) {
		geo.x = usable.x + usable.width - geo.width;
	}
	if (geo.y + geo.height > usable.y + usable.height) {
		geo.y = usable.y + usable.height - geo.height;
	}
	if (geo.x < usable.x) {
		geo.x = usable.x;
	}
	if (geo.y < usable.y) {
		geo.y = usable.y;
	}

	if (kw_origin_taken(view, &geo)) {
		return;	/* labwc's placement has the better answer here */
	}

	view_set_output(view, output);
	view_move_resize(view, geo);
	view_save_last_placement(view);
	if (r->shaded) {
		view_set_shade(view, true);
	}
	wlr_log(WLR_DEBUG, "winpos: %s restored to %d,%d %dx%d", view->app_id,
		geo.x, geo.y, geo.width, geo.height);
}

void
kdos_winpos_finish(void)
{
	struct kw_record *r, *tmp;
	zfree(kw_marks);
	kw_marks_len = kw_marks_cap = 0;
	if (!kwin.loaded) {
		return;
	}
	wl_list_for_each_safe(r, tmp, &kwin.records, link) {
		wl_list_remove(&r->link);
		zfree(r->app_id);
		free(r);
	}
	kwin.loaded = false;
}
