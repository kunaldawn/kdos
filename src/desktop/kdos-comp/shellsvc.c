/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   what a shell needs to know: windows and workspaces
 *
 * kdos-shell is ours and so is kdos-comp, which makes a private side channel
 * between them tempting and wrong. Both of these are STANDARD protocols —
 * wlr-foreign-toplevel-management and ext-workspace-v1 — so `wlr-taskbar`,
 * `waybar` and anything else a user prefers work against our compositor
 * unchanged, and our shell has no privileged access it did not earn.
 *
 * They are the two questions a panel asks: what windows exist, and which
 * workspace am I on. Neither is answerable from xdg-shell, because xdg-shell
 * deliberately tells a client only about ITSELF.
 *
 * Both are on the security-context deny list. A boxed app that could enumerate
 * every window's title has a keylogger's view of the session, and one that
 * could activate them can raise a fake password prompt over the real one.
 */

#include <stdio.h>
#include <stdlib.h>

#include "kdos-comp.h"

/* ── foreign-toplevel: the window list ─────────────────────────────────── */

/*
 * The request signals live on the HANDLE, not on the manager — one set per
 * window — so these listeners are per-toplevel and reach their window through
 * wl_container_of rather than by searching for it.
 */
static void ftl_request_activate(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, ftl_activate);
	struct kc_server *s = t->server;
	(void)data;
	/*
	 * Clicking a taskbar entry for a window on another workspace has to
	 * TAKE YOU THERE. Raising it on the current workspace instead would
	 * either show nothing (its scene tree is disabled) or drag one window
	 * out of the set the user arranged.
	 */
	if (t->ws != s->cur_ws)
		kc_ws_switch(s, t->ws);
	kc_focus(t, t->xdg_toplevel->base->surface);
}

static void ftl_request_close(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, ftl_close);
	(void)data;
	wlr_xdg_toplevel_send_close(t->xdg_toplevel);
}

/*
 * Called whenever a window appears, is focused, or changes its title.
 *
 * The handle is created at MAP rather than at creation: a toplevel that has
 * not mapped has no title and no app_id yet, and a taskbar entry that appears
 * blank and then renames itself a moment later is the flicker every desktop
 * has at startup.
 */
void kc_shellsvc_add(struct kc_server *s, struct kc_toplevel *t)
{
	if (!s->ftl_mgr || t->ftl)
		return;
	t->ftl = wlr_foreign_toplevel_handle_v1_create(s->ftl_mgr);
	if (!t->ftl)
		return;
	t->ftl_activate.notify = ftl_request_activate;
	wl_signal_add(&t->ftl->events.request_activate, &t->ftl_activate);
	t->ftl_close.notify = ftl_request_close;
	wl_signal_add(&t->ftl->events.request_close, &t->ftl_close);
	kc_shellsvc_update(s, t);
}

void kc_shellsvc_update(struct kc_server *s, struct kc_toplevel *t)
{
	if (!t->ftl)
		return;
	const char *title = t->xdg_toplevel->title;
	const char *app_id = t->xdg_toplevel->app_id;
	if (title)
		wlr_foreign_toplevel_handle_v1_set_title(t->ftl, title);
	if (app_id)
		wlr_foreign_toplevel_handle_v1_set_app_id(t->ftl, app_id);
	wlr_foreign_toplevel_handle_v1_set_activated(
		t->ftl, s->seat->keyboard_state.focused_surface ==
				t->xdg_toplevel->base->surface);
	/*
	 * A window on another workspace is reported MINIMIZED. There is no
	 * "on another workspace" state in this protocol, and minimized is the
	 * closest true thing: not visible, still exists, click to get it back.
	 * Saying nothing would make every panel draw all four workspaces'
	 * windows as if they were all on screen.
	 */
	wlr_foreign_toplevel_handle_v1_set_minimized(t->ftl, t->ws != s->cur_ws);
}

void kc_shellsvc_remove(struct kc_toplevel *t)
{
	if (!t->ftl)
		return;
	wl_list_remove(&t->ftl_activate.link);
	wl_list_remove(&t->ftl_close.link);
	wlr_foreign_toplevel_handle_v1_destroy(t->ftl);
	t->ftl = NULL;
}

/* Every window's reported state depends on the CURRENT workspace, so a switch
 * re-reports all of them rather than just the two that changed visibility. */
void kc_shellsvc_refresh(struct kc_server *s)
{
	struct kc_toplevel *t;
	wl_list_for_each(t, &s->toplevels, link)
		kc_shellsvc_update(s, t);
	for (int i = 0; i < KC_WORKSPACES; i++)
		if (s->ws_handle[i])
			wlr_ext_workspace_handle_v1_set_active(s->ws_handle[i],
							       i == s->cur_ws);
}

/* ── ext-workspace: which desk am I on ─────────────────────────────────── */

/*
 * A client asks to switch by sending a request and then a commit; the
 * compositor is free to refuse. Requests arrive as a LIST because the protocol
 * is transactional — a client may ask to activate one workspace and deactivate
 * another in the same commit, and applying them one at a time as they arrive
 * would flash through an intermediate state that was never asked for.
 */
static void ws_commit(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, ws_commit);
	const struct wlr_ext_workspace_v1_commit_event *ev = data;
	struct wlr_ext_workspace_v1_request *req;

	int target = -1;
	wl_list_for_each(req, ev->requests, link) {
		if (req->type != WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE)
			continue;
		for (int i = 0; i < KC_WORKSPACES; i++)
			if (s->ws_handle[i] == req->activate.workspace)
				target = i;
	}
	if (target >= 0)
		kc_ws_switch(s, target);
}

void kc_shellsvc_init(struct kc_server *s)
{
	s->ftl_mgr = wlr_foreign_toplevel_manager_v1_create(s->display);

	s->ws_mgr = wlr_ext_workspace_manager_v1_create(s->display, 1);
	if (!s->ws_mgr)
		return;
	s->ws_commit.notify = ws_commit;
	wl_signal_add(&s->ws_mgr->events.commit, &s->ws_commit);

	/*
	 * One group covering every output. KDOS's workspaces are global rather
	 * than per-monitor — Super+2 means the same thing whichever screen the
	 * pointer is on — and a group per output would say the opposite.
	 */
	/* caps 0: the shell may look and may activate, but may not create or
	 * remove workspaces. Four fixed workspaces is the design (see
	 * KC_WORKSPACES), so advertising the capability would be a lie the
	 * protocol lets us tell. */
	s->ws_group = wlr_ext_workspace_group_handle_v1_create(s->ws_mgr, 0);
	for (int i = 0; i < KC_WORKSPACES; i++) {
		char name[8];
		snprintf(name, sizeof(name), "%d", i + 1);
		s->ws_handle[i] = wlr_ext_workspace_handle_v1_create(
			s->ws_mgr, name,
			EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE);
		if (!s->ws_handle[i])
			continue;
		wlr_ext_workspace_handle_v1_set_name(s->ws_handle[i], name);
		if (s->ws_group)
			wlr_ext_workspace_handle_v1_set_group(s->ws_handle[i],
							      s->ws_group);
		wlr_ext_workspace_handle_v1_set_active(s->ws_handle[i], i == 0);
	}
}
