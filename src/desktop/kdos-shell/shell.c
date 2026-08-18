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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
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
 * At most `cells` display columns of `src`, cut on a codepoint boundary.
 *
 * `%.14s` cuts by BYTES, and every string this panel truncates is somebody
 * else's: an app's `application.name` from the PipeWire graph, a workspace name
 * out of the user's own rc.xml. A cut in the middle of a sequence leaves a lone
 * continuation byte for ktui_utf8_width to measure and ktui_draw_text to
 * decode. Bounded by the destination as well, so a wide script cannot overrun a
 * buffer sized in cells.
 */
void sh_utf8_trunc(char *dst, size_t n, const char *src, int cells)
{
	size_t w = 0;
	int used = 0;

	if (!n)
		return;
	for (const char *p = src; *p;) {
		uint32_t cp = 0;
		const char *next = ktui_utf8_next(p, &cp);
		size_t len = (size_t)(next - p);
		int cw = ktui_wcwidth(cp);

		if (used + cw > cells || w + len >= n)
			break;
		memcpy(dst + w, p, len);
		w += len;
		used += cw;
		p = next;
	}
	dst[w] = '\0';
}

/*
 * `kdos restarts --quiet` exits 1 when something needs restarting and 0 when
 * nothing does, which is why it has that exit code: the panel needs no parser
 * and no shared format, just a status.
 *
 * Spawned rather than reimplemented. The join between the package manifest and
 * /proc lives in one place, and a second copy in the panel would be a second
 * thing to keep correct.
 *
 * Never waited for synchronously: the command walks every process's maps, and
 * blocking in waitpid here froze the panel for the whole walk once a minute.
 * `begin` forks, `poll` collects with WNOHANG on later ticks.
 */
static pid_t restart_pid = -1;
static int restart_val = -1;

void sh_restart_begin(void)
{
	if (restart_pid > 0)
		return;			/* one in flight is enough */
	pid_t p = fork();
	if (p < 0)
		return;
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
	restart_pid = p;
}

int sh_restart_poll(void)
{
	if (restart_pid > 0) {
		int st = 0;
		pid_t r = waitpid(restart_pid, &st, WNOHANG);
		if (r == restart_pid) {
			restart_pid = -1;
			if (!WIFEXITED(st) || WEXITSTATUS(st) == 127)
				restart_val = -1;  /* no kdos: unknown, not zero */
			else
				restart_val = WEXITSTATUS(st) == 1 ? 1 : 0;
		} else if (r < 0) {
			restart_pid = -1;
		}
	}
	return restart_val;
}

/*
 * Field codes, out of an Exec line: `%f` is a file, `%U` a URL list, `%i` the
 * icon option. Launching with them still in argv passes the LITERAL "%U" to
 * the program, which browsers open as a search and everything else reports as
 * a missing file. `%%` is the escape for a percent the program should SEE.
 * The whitespace a dropped code leaves behind is collapsed, so the result
 * still splits into clean argv — "app  --flag" is two words, not three.
 */
void sh_strip_field_codes(char *exec)
{
	char *w = exec;
	int sp = 1;	/* at start: also swallows leading whitespace */

	for (char *r = exec; *r; r++) {
		if (r[0] == '%' && r[1]) {
			if (r[1] == '%') {
				*w++ = '%';
				sp = 0;
				r++;
				continue;
			}
			r++;		/* drop the code */
			continue;
		}
		if (*r == ' ' || *r == '\t') {
			if (!sp)
				*w++ = ' ';
			sp = 1;
			continue;
		}
		*w++ = *r;
		sp = 0;
	}
	while (w > exec && w[-1] == ' ')
		w--;
	*w = '\0';
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
 * The same retint for the front ends nobody signals — and the reason it polls
 * a file instead of taking the SIGHUP the panel takes.
 *
 * `kdos theme` signals FOUR names by design (kdos-shell, kdos-desk,
 * kdos-notifyd, kdos-comp), because SIGHUP's default disposition is death and
 * a name on that list whose program installs no handler would be KILLED by a
 * theme change. That made the list a thing two files have to agree about, and
 * they already disagreed: kdos-slit calls sh_theme_watch() and is on nobody's
 * list, so the slit never retinted at all.
 *
 * A dialog is not short-lived just because it is modal. Photographed on a
 * booted ISO: `kdos theme amber` repainted the wallpaper, both panels and the
 * desktop icons, and left the open `kdos-openwith` in phosphor — and the most
 * likely way to change an accent at all is kdos-settings, which is one of
 * these surfaces and so was left wearing the accent it had just replaced.
 *
 * Polling the state file's mtime needs no signal, no handler and no second
 * list: a front end that draws is a front end that follows. It is one stat per
 * wake-up of a loop that is already blocked on input the rest of the time.
 */
void sh_theme_poll(void)
{
	static time_t seen;
	static int primed;
	char path[512];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	struct stat st;

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return;

	if (stat(path, &st) != 0)
		return;
	if (!primed) {
		primed = 1;
		seen = st.st_mtime;
		return;
	}
	if (st.st_mtime == seen)
		return;
	seen = st.st_mtime;
	sh_theme_from_cache();
	ktui_draw_invalidate();
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

/* The XDG data dirs, most specific first — shared by every lookup below. */
static int data_dirs(char bases[8][512])
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int nb = 0;

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
	return nb;
}

static void desktop_name(const char *app_id, char *out, size_t n)
{
	char bases[8][512];
	int nb;

	*out = '\0';
	if (!app_id || !*app_id || strchr(app_id, '/'))
		return;			/* not a desktop id; do not build a path */
	nb = data_dirs(bases);

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

/*
 * Name and Exec for a desktop-entry id — the favorites row's lookup, through
 * the same directories the taskbar label search reads, so the two can never
 * disagree about which entry an id means.
 */
int sh_desktop_entry(const char *id, char *name, size_t nname,
		     char *exec, size_t nexec)
{
	char bases[8][512];
	int nb, found = -1;

	if (name && nname)
		*name = '\0';
	if (exec && nexec)
		*exec = '\0';
	if (!id || !*id || strchr(id, '/'))
		return -1;
	nb = data_dirs(bases);

	for (int i = 0; i < nb && found < 0; i++) {
		char path[1024];
		KxdgEntry e;
		snprintf(path, sizeof(path), "%.400s/applications/%.100s.desktop",
			 bases[i], id);
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			continue;
		const char *v = kxdg_get(&e, "Name", NULL);
		if (name && nname && v && *v)
			snprintf(name, nname, "%s", v);
		v = kxdg_get(&e, "Exec", NULL);
		if (exec && nexec && v && *v)
			snprintf(exec, nexec, "%s", v);
		kxdg_free(&e);
		found = 0;
	}
	return found;
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

/* labwc publishes workspace names over ext-workspace-v1; a strip that shows
 * synthesized digits over a user's own `<names>` is showing the wrong thing.
 * Truncated to what a strip cell row can afford; the draw clamps further. */
static void ws_name(void *data, struct ext_workspace_handle_v1 *h,
		    const char *name)
{
	struct sh_state *sh = data;
	for (int i = 0; i < sh->nws; i++) {
		if (sh->ws[i] != h)
			continue;
		sh_utf8_trunc(sh->ws_name[i], sizeof(sh->ws_name[i]),
			      name ? name : "", (int)sizeof(sh->ws_name[i]) - 1);
		break;
	}
}

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
		/* The protocol sends the COMPLETE state every time, so urgency
		 * is set and CLEARED here — its own bit, never folded into
		 * occupancy: folding made it invisible (same colour as
		 * occupied) and sticky (nothing ever unset it). */
		sh->ws_urgent[i] =
			(state & EXT_WORKSPACE_HANDLE_V1_STATE_URGENT) ? 1 : 0;
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
		/* Every parallel array moves with the handle, or workspace N's
		 * name and urgency become workspace N+1's for the session. */
		int rest = sh->nws - i - 1;
		memmove(&sh->ws[i], &sh->ws[i + 1],
			(size_t)rest * sizeof(sh->ws[0]));
		memmove(&sh->ws_occupied[i], &sh->ws_occupied[i + 1],
			(size_t)rest * sizeof(sh->ws_occupied[0]));
		memmove(&sh->ws_urgent[i], &sh->ws_urgent[i + 1],
			(size_t)rest * sizeof(sh->ws_urgent[0]));
		memmove(sh->ws_name[i], sh->ws_name[i + 1],
			(size_t)rest * sizeof(sh->ws_name[0]));
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
	sh->ws_occupied[sh->nws] = 0;
	sh->ws_urgent[sh->nws] = 0;
	sh->ws_name[sh->nws][0] = '\0';
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

/*
 * Spawn something no surface here must wait for.
 *
 * DOUBLE FORK: the intermediate child exits at once and init reaps the
 * grandchild, so a panel that never calls waitpid() cannot accumulate zombies
 * — and a launcher that blocked on a boxed app's eighteen-second cold start
 * would be a launcher nobody uses twice.
 *
 * The waitpid on the INTERMEDIATE child is interruptible on purpose: the
 * SIGHUP `kdos theme` sends is caught without SA_RESTART (see sh_theme_watch),
 * and a waitpid it interrupts leaves that child a zombie for the life of the
 * process — one per menu click.
 *
 * No shell. argv is exec'd as given.
 */
void sh_spawn(const char *const argv[])
{
	pid_t pid;

	if (!argv || !argv[0])
		return;
	pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
			;
	}
}
