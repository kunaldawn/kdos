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
#include <strings.h>

#include <dirent.h>
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

/*
 * The name a person would recognise, from the app's own desktop entry.
 *
 * A taskbar that shows `org.gnome.Meld` is showing an implementation detail:
 * an app_id is a reverse-DNS identifier chosen so it cannot collide, and it is
 * the last thing that should be on screen when the entry that identifier
 * belongs to carries the word "Meld" three lines down. The launcher, the menu
 * and the dock all read that entry already; this is the fourth consumer of it
 * and the one the user looks at most.
 *
 * Empty when there is no entry — an alien app launched by a shim, a client
 * with an invented app_id — and the caller then falls back to the title and to
 * the app_id, in that order.
 */
static void desktop_name(const char *app_id, char *out, size_t n)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	char bases[8][512];
	int nb = 0;

	*out = '\0';
	if (!app_id || !*app_id || strchr(app_id, '/'))
		return;			/* not a desktop id; do not build a path */

	if (home && *home)
		snprintf(bases[nb++], sizeof(bases[0]), "%s", home);
	else if (getenv("HOME"))
		snprintf(bases[nb++], sizeof(bases[0]), "%s/.local/share",
			 getenv("HOME"));
	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *p = dirs; *p && nb < 8;) {
		const char *sep = strchr(p, ':');
		size_t len = sep ? (size_t)(sep - p) : strlen(p);
		while (len > 1 && p[len - 1] == '/')
			len--;
		if (len && len < sizeof(bases[0])) {
			memcpy(bases[nb], p, len);
			bases[nb][len] = '\0';
			nb++;
		}
		if (!sep)
			break;
		p = sep + 1;
	}

	for (int i = 0; i < nb; i++) {
		char path[1024];
		KxdgEntry e;
		snprintf(path, sizeof(path), "%.400s/applications/%.100s.desktop",
			 bases[i], app_id);
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			continue;
		const char *name = kxdg_get(&e, "Name", NULL);
		if (name && *name)
			snprintf(out, n, "%s", name);
		kxdg_free(&e);
		if (*out)
			return;
	}

	/*
	 * THEN StartupWMClass, which is the other half of the answer and was
	 * only ever the documented half.
	 *
	 * A Wayland app_id is usually the desktop id and this stops at the
	 * lookup above; an X11 client under Xwayland reports its WM_CLASS
	 * instead, and the entry that owns it says so in StartupWMClass rather
	 * than in its file name — GIMP's entry says `gimp-3.0` while its
	 * Wayland toplevel says `gimp`. Without this pass those windows fell
	 * through to the raw id, which is the reverse-DNS-in-the-taskbar defect
	 * one step down.
	 *
	 * A directory scan, and it is affordable because it only runs when the
	 * cheap lookup MISSED, and then once per window rather than per frame.
	 */
	for (int i = 0; i < nb; i++) {
		char dir[600];
		snprintf(dir, sizeof(dir), "%.500s/applications", bases[i]);
		DIR *d = opendir(dir);
		struct dirent *de;
		if (!d)
			continue;
		while ((de = readdir(d))) {
			char path[1200];
			KxdgEntry e;
			size_t len = strlen(de->d_name);
			if (len < 9 || strcmp(de->d_name + len - 8, ".desktop"))
				continue;
			snprintf(path, sizeof(path), "%.600s/%.500s", dir,
				 de->d_name);
			if (kxdg_load(&e, path, "Desktop Entry") != 0)
				continue;
			const char *wm = kxdg_get(&e, "StartupWMClass", NULL);
			const char *name = kxdg_get(&e, "Name", NULL);
			if (wm && !strcasecmp(wm, app_id) && name && *name)
				snprintf(out, n, "%s", name);
			kxdg_free(&e);
			if (*out)
				break;
		}
		closedir(d);
		if (*out)
			return;
	}
}

static void tl_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
		      const char *app_id)
{
	struct sh_task *t = task_for(data, h);
	if (!t)
		return;
	snprintf(t->app_id, sizeof(t->app_id), "%s", app_id);
	/* Resolved once, here, rather than per frame: this fires when a window
	 * maps and when it changes its id, which is the only time the answer
	 * can change, and the panel redraws every second. */
	desktop_name(app_id, t->name, sizeof(t->name));
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

/*
 * Close, for the middle click every taskbar since the nineties has answered
 * that way. This is the protocol's polite close — the same request the window's
 * own close box sends, so an editor with unsaved work still gets to ask.
 */
void sh_close_task(struct sh_state *sh, int i)
{
	if (i < 0 || i >= sh->ntasks)
		return;
	zwlr_foreign_toplevel_handle_v1_close(sh->tasks[i].handle);
	wl_display_flush(sh->display);
}

/*
 * What a LEFT click on a task entry should do, which is not simply "activate".
 * Clicking the window you are already in minimises it and clicking it again
 * brings it back — the behaviour every taskbar has, and the reason the entry
 * is worth clicking at all once the window is on screen.
 */
void sh_toggle_task(struct sh_state *sh, int i)
{
	if (i < 0 || i >= sh->ntasks)
		return;
	if (sh->tasks[i].minimized) {
		zwlr_foreign_toplevel_handle_v1_unset_minimized(sh->tasks[i].handle);
		sh_activate_task(sh, i);
		return;
	}
	if (sh->tasks[i].activated) {
		zwlr_foreign_toplevel_handle_v1_set_minimized(sh->tasks[i].handle);
		wl_display_flush(sh->display);
		return;
	}
	sh_activate_task(sh, i);
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
