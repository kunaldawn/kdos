/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the two protocols a panel lives on
 *
 * wlr-foreign-toplevel-management for the window list, ext-workspace-v1 for
 * the workspaces. Both are bound on libkwl's EXISTING connection: a second
 * wl_display would mean a second socket, a second seat and a second set of
 * globals, and the panel would be two clients that have to agree with each
 * other about what it is showing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <wayland-client.h>

#include "ext-workspace-v1-client-protocol.h"
#include "kwl.h"
#include "shell.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

/* ── small helpers ─────────────────────────────────────────────────────── */

int sh_read_line(const char *path, char *buf, size_t len)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	if (!fgets(buf, (int)len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

/*
 * `kdos restarts --quiet` exits 1 when something needs restarting and 0 when
 * nothing does, which is why it has that exit code: the panel needs no parser
 * and no shared format, just a status.
 *
 * Spawned rather than reimplemented. The join between the package manifest and
 * /proc lives in one place, and a second copy in the panel would be a second
 * thing to keep correct.
 */
int sh_restart_count(void)
{
	pid_t p = fork();
	if (p < 0)
		return -1;
	if (p == 0) {
		int null = open("/dev/null", O_WRONLY);
		if (null >= 0) {
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
			if (null > STDERR_FILENO)
				close(null);
		}
		execlp("kdos", "kdos", "restarts", "--quiet", (char *)NULL);
		_exit(127);
	}
	int st = 0;
	if (waitpid(p, &st, 0) < 0 || !WIFEXITED(st))
		return -1;
	int rc = WEXITSTATUS(st);
	if (rc == 127)
		return -1;	/* kdos is not installed; say unknown, not zero */
	return rc == 1 ? 1 : 0;
}

void sh_theme_from_cache(void)
{
	char path[512], name[64];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return;

	if (sh_read_line(path, name, sizeof(name)) == 0 && *name)
		ktui_theme_set(name);
	/* No file is not an error: ktui_theme_set already defaulted to
	 * phosphor, which is what a fresh install wears. */
}

/*
 * `kdos theme <accent>` writes the state file and then SIGHUPs kdos-shell and
 * kdos-comp. The compositor half worked from the start (labwc's Reconfigure);
 * this half did not exist, and the failure had two faces depending on how the
 * session was started:
 *
 *   - default disposition: SIGHUP TERMINATES. The panel died and the
 *     compositor's supervisor respawned it, which re-read the state file and
 *     came up in the new accent — so it LOOKED like a live retint. But it is a
 *     crash per accent change, and RESPAWN_MAX is 5 in 30 s: trying four or
 *     five accents to pick one is enough to lose the panel for the session.
 *   - inherited SIG_IGN (a session started under nohup, say): nothing happened
 *     at all. Measured: SigIgn 0x1, SigCgt 0 on a running kdos-shell.
 *
 * sigaction() here settles both — it overrides an inherited SIG_IGN, and a
 * caught signal is not a fatal one. No SA_RESTART: the poll the loops sleep in
 * should come back at once, and both already treat EINTR as "go round again".
 */
volatile sig_atomic_t sh_theme_dirty;

static void on_sighup(int sig)
{
	(void)sig;
	sh_theme_dirty = 1;
}

void sh_theme_watch(void)
{
	struct sigaction sa = { 0 };
	sa.sa_handler = on_sighup;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);
}

static struct sh_task *task_for(struct sh_state *sh, void *handle)
{
	for (int i = 0; i < sh->ntasks; i++)
		if (sh->tasks[i].handle == handle)
			return &sh->tasks[i];
	return NULL;
}

/* ── foreign-toplevel ──────────────────────────────────────────────────── */

static void tl_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
		     const char *title)
{
	struct sh_task *t = task_for(data, h);
	if (t)
		snprintf(t->title, sizeof(t->title), "%s", title);
}

static void tl_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
		      const char *app_id)
{
	struct sh_task *t = task_for(data, h);
	if (t)
		snprintf(t->app_id, sizeof(t->app_id), "%s", app_id);
}

static void tl_output_enter(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
			    struct wl_output *o)
{ (void)d; (void)h; (void)o; }
static void tl_output_leave(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
			    struct wl_output *o)
{ (void)d; (void)h; (void)o; }

/*
 * State arrives as an ARRAY of enum values, not as a bitmask — the protocol
 * sends the complete set every time, so a state that is absent is a state that
 * is off. Reading it as flags to OR together would make a window that was once
 * activated stay highlighted forever.
 */
static void tl_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
		     struct wl_array *states)
{
	struct sh_task *t = task_for(data, h);
	if (!t)
		return;
	t->activated = 0;
	t->minimized = 0;
	uint32_t *st;
	wl_array_for_each(st, states) {
		if (*st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
			t->activated = 1;
		else if (*st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			t->minimized = 1;
	}
}

static void tl_done(void *d, struct zwlr_foreign_toplevel_handle_v1 *h)
{ (void)d; (void)h; }

static void tl_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
	struct sh_state *sh = data;
	for (int i = 0; i < sh->ntasks; i++) {
		if (sh->tasks[i].handle != h)
			continue;
		/* Compact the array so the click map stays dense: position N in
		 * the panel must always be tasks[N]. */
		memmove(&sh->tasks[i], &sh->tasks[i + 1],
			(size_t)(sh->ntasks - i - 1) * sizeof(sh->tasks[0]));
		sh->ntasks--;
		break;
	}
	zwlr_foreign_toplevel_handle_v1_destroy(h);
}

static void tl_parent(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
		      struct zwlr_foreign_toplevel_handle_v1 *p)
{ (void)d; (void)h; (void)p; }

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_listener = {
	.title = tl_title,
	.app_id = tl_app_id,
	.output_enter = tl_output_enter,
	.output_leave = tl_output_leave,
	.state = tl_state,
	.done = tl_done,
	.closed = tl_closed,
	.parent = tl_parent,
};

static void ftl_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *m,
			 struct zwlr_foreign_toplevel_handle_v1 *h)
{
	struct sh_state *sh = data;
	(void)m;
	if (sh->ntasks >= SH_MAX_TASKS) {
		/* Dropped rather than wrapped. A panel that silently replaces
		 * one window's entry with another's is worse than one that
		 * stops adding at 64 — and nobody has 64 windows open. */
		zwlr_foreign_toplevel_handle_v1_destroy(h);
		return;
	}
	struct sh_task *t = &sh->tasks[sh->ntasks++];
	memset(t, 0, sizeof(*t));
	t->handle = h;
	zwlr_foreign_toplevel_handle_v1_add_listener(h, &toplevel_listener, sh);
}

static void ftl_finished(void *d, struct zwlr_foreign_toplevel_manager_v1 *m)
{ (void)d; (void)m; }

static const struct zwlr_foreign_toplevel_manager_v1_listener ftl_listener = {
	.toplevel = ftl_toplevel,
	.finished = ftl_finished,
};

/* ── ext-workspace ─────────────────────────────────────────────────────── */

static void ws_id(void *d, struct ext_workspace_handle_v1 *h, const char *id)
{ (void)d; (void)h; (void)id; }

static void ws_name(void *d, struct ext_workspace_handle_v1 *h, const char *name)
{ (void)d; (void)h; (void)name; }

static void ws_coordinates(void *d, struct ext_workspace_handle_v1 *h,
			   struct wl_array *coords)
{ (void)d; (void)h; (void)coords; }

static void ws_state(void *data, struct ext_workspace_handle_v1 *h,
		     uint32_t state)
{
	struct sh_state *sh = data;
	for (int i = 0; i < sh->nws; i++) {
		if (sh->ws[i] != h)
			continue;
		if (state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
			sh->active_ws = i;
		/* `urgent` is the protocol's word for "something here wants
		 * you"; the panel shows it the same way it shows occupancy,
		 * because on one row there is no space for a third state. */
		sh->ws_occupied[i] =
			(state & EXT_WORKSPACE_HANDLE_V1_STATE_URGENT) ? 1
			: sh->ws_occupied[i];
		break;
	}
}

static void ws_capabilities(void *d, struct ext_workspace_handle_v1 *h,
			    uint32_t caps)
{ (void)d; (void)h; (void)caps; }

static void ws_removed(void *data, struct ext_workspace_handle_v1 *h)
{
	struct sh_state *sh = data;
	for (int i = 0; i < sh->nws; i++) {
		if (sh->ws[i] != h)
			continue;
		memmove(&sh->ws[i], &sh->ws[i + 1],
			(size_t)(sh->nws - i - 1) * sizeof(sh->ws[0]));
		sh->nws--;
		break;
	}
	ext_workspace_handle_v1_destroy(h);
}

static const struct ext_workspace_handle_v1_listener workspace_listener = {
	.id = ws_id,
	.name = ws_name,
	.coordinates = ws_coordinates,
	.state = ws_state,
	.capabilities = ws_capabilities,
	.removed = ws_removed,
};

static void wsm_workspace_group(void *d, struct ext_workspace_manager_v1 *m,
				struct ext_workspace_group_handle_v1 *g)
{ (void)d; (void)m; (void)g; }

static void wsm_workspace(void *data, struct ext_workspace_manager_v1 *m,
			  struct ext_workspace_handle_v1 *h)
{
	struct sh_state *sh = data;
	(void)m;
	if (sh->nws >= SH_MAX_WS) {
		ext_workspace_handle_v1_destroy(h);
		return;
	}
	sh->ws[sh->nws++] = h;
	ext_workspace_handle_v1_add_listener(h, &workspace_listener, sh);
}

static void wsm_done(void *d, struct ext_workspace_manager_v1 *m)
{ (void)d; (void)m; }
static void wsm_finished(void *d, struct ext_workspace_manager_v1 *m)
{ (void)d; (void)m; }

static const struct ext_workspace_manager_v1_listener wsm_listener = {
	.workspace_group = wsm_workspace_group,
	.workspace = wsm_workspace,
	.done = wsm_done,
	.finished = wsm_finished,
};

/* ── registry ──────────────────────────────────────────────────────────── */

static void reg_global(void *data, struct wl_registry *r, uint32_t name,
		       const char *iface, uint32_t version)
{
	struct sh_state *sh = data;
	(void)version;
	if (!strcmp(iface, zwlr_foreign_toplevel_manager_v1_interface.name)) {
		sh->ftl_mgr = wl_registry_bind(
			r, name, &zwlr_foreign_toplevel_manager_v1_interface, 3);
		zwlr_foreign_toplevel_manager_v1_add_listener(sh->ftl_mgr,
							      &ftl_listener, sh);
	} else if (!strcmp(iface, ext_workspace_manager_v1_interface.name)) {
		sh->ws_mgr = wl_registry_bind(
			r, name, &ext_workspace_manager_v1_interface, 1);
		ext_workspace_manager_v1_add_listener(sh->ws_mgr, &wsm_listener,
						      sh);
	}
}

static void reg_remove(void *d, struct wl_registry *r, uint32_t name)
{ (void)d; (void)r; (void)name; }

static const struct wl_registry_listener registry_listener = {
	.global = reg_global,
	.global_remove = reg_remove,
};

int sh_connect(struct sh_state *sh)
{
	sh->display = kwl_display();
	if (!sh->display)
		return -1;

	struct wl_registry *r = wl_display_get_registry(sh->display);
	wl_registry_add_listener(r, &registry_listener, sh);
	wl_display_roundtrip(sh->display);	/* the globals */
	wl_display_roundtrip(sh->display);	/* and what they then send us */

	/*
	 * The window list is what a panel IS. Without it there is a clock and a
	 * row of workspace numbers, which is not worth a layer-shell surface
	 * and an exclusive zone taken off every other window.
	 */
	return sh->ftl_mgr ? 0 : -1;
}

void sh_dispatch(struct sh_state *sh)
{
	if (sh->display)
		wl_display_dispatch_pending(sh->display);
}

void sh_disconnect(struct sh_state *sh)
{
	/* The connection is libkwl's; kwl_shutdown() closes it. Only the
	 * objects bound here are this file's to release. */
	sh->ftl_mgr = NULL;
	sh->ws_mgr = NULL;
	sh->display = NULL;
}

/*
 * Minimise, which is what show-desktop is made of.
 *
 * No seat argument, and none is wanted: minimising is not a focus grab, so the
 * compositor has nothing to protect against here. A window that is already
 * minimised is left alone rather than toggled — show-desktop pressed twice must
 * not un-minimise half the screen.
 */
void sh_minimize_task(struct sh_state *sh, int i)
{
	if (i < 0 || i >= sh->ntasks || sh->tasks[i].minimized)
		return;
	zwlr_foreign_toplevel_handle_v1_set_minimized(sh->tasks[i].handle);
	wl_display_flush(sh->display);
}

void sh_activate_task(struct sh_state *sh, int i)
{
	if (i < 0 || i >= sh->ntasks)
		return;
	/*
	 * The seat is required: the compositor uses it to decide whether the
	 * request came from something the user is actually driving, which is
	 * what stops a background client raising itself over what you are
	 * typing into.
	 */
	struct wl_seat *seat = kwl_seat();
	if (!seat)
		return;
	zwlr_foreign_toplevel_handle_v1_activate(sh->tasks[i].handle, seat);
	wl_display_flush(sh->display);
}

void sh_activate_workspace(struct sh_state *sh, int i)
{
	if (i < 0 || i >= sh->nws || !sh->ws_mgr)
		return;
	/* Request then commit: ext-workspace is transactional, and a request
	 * with no commit is a request the compositor never sees. */
	ext_workspace_handle_v1_activate(sh->ws[i]);
	ext_workspace_manager_v1_commit(sh->ws_mgr);
	wl_display_flush(sh->display);
}
