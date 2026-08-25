// SPDX-License-Identifier: GPL-2.0-only
/*
 * `kdos hey` — the command socket. $XDG_RUNTIME_DIR/kdos-cmd.sock, one
 * NDJSON request line in, one NDJSON response line out, close. Haiku's
 * scripting soul in the shape this distro already speaks: every other
 * KDOS channel between two of its own programs is a unix socket carrying
 * one JSON object per line, and `kdos stutter` proved the shape.
 *
 *   {"cmd":"list"}      -> {"ok":true,"views":[...]}
 *   {"cmd":"run","action":"Close","id":7} -> {"ok":true}
 *   {"cmd":"outputs"}   -> {"ok":true,"outputs":[...]}
 *   {"cmd":"boxes"}     -> {"ok":true,"boxes":["arch","kdos-apps"]}
 *
 * Rules, each one a way a control socket usually takes a compositor down
 * with it:
 *
 * - It NEVER blocks the frame loop. Both ends are non-blocking, the read
 *   buffer is per connection and small, a partial response goes out
 *   through a WRITABLE source rather than a blocking write, and a client
 *   that stalls mid-request is dropped on a timer — one that connects
 *   and says nothing, or asks and never reads the answer, would
 *   otherwise hold an fd and a growing buffer for the whole session.
 *   KC_MAX_CLIENTS bounds it from the other side.
 * - The peer's uid is checked with SO_PEERCRED, the way kdos-powerd and
 *   kdos-energyd already do it, because this socket runs ACTIONS: the
 *   mode is not the authorisation. It cannot separate a boxed app from
 *   a host one — the appbox shares $XDG_RUNTIME_DIR and runs as the
 *   same user — so this is a multi-user gate, and the box's own reach
 *   is the compositor's sandbox question, not this one's.
 * - A request line over KC_REQ_MAX is refused rather than grown. The
 *   protocol has no request that needs a kilobyte.
 * - There is no history and no subscription. A consumer that connects
 *   late has missed nothing, because nothing is remembered; anything that
 *   wants a stream of events wants kdos-frames.sock, which is that.
 * - An unknown or incompletely-specified action is answered `ok:false`
 *   with a reason. Silently doing nothing is how a script comes to
 *   believe it worked.
 *
 * The `id` is the view's creation_id: monotonic, unique for the life of
 * the session, and — unlike a pointer — meaningless to hand back at the
 * wrong moment.
 */
/* _GNU_SOURCE rather than the _POSIX_C_SOURCE its siblings use: struct
 * ucred is a GNU extension, and it is a superset of what that asked for.
 * Guarded, because an unguarded one collides with a -D on the command
 * line — that cost kdos-boxsock a build. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

#include "action.h"
#include "common/buf.h"
#include "common/list.h"
#include "kdos.h"
#include "labwc.h"
#include "output.h"
#include "view.h"
#include "workspaces.h"

#define KC_REQ_MAX 4096
/* one request, one response, close — a handful of those in flight at
 * once is already more than anything real does */
#define KC_MAX_CLIENTS 8
#define KC_IDLE_MS 5000

struct kc_client {
	struct wl_list link;		/* struct kc.clients */
	int fd;
	struct wl_event_source *src;
	struct wl_event_source *idle;	/* drops a peer that stops talking */
	char in[KC_REQ_MAX + 1];
	size_t in_len;
	struct buf out;
	size_t out_sent;
	bool writing;
};

struct kc {
	int listen_fd;
	struct wl_event_source *listen_src;
	char path[256];
	struct wl_list clients;
	int nclients;
};

static struct kc *kc;

/* ── JSON out ───────────────────────────────────────────────────── */

/*
 * A title is whatever the application put there — quotes, backslashes and
 * control bytes included — and one unescaped byte turns the whole line
 * into something no reader can parse. Anything below 0x20 goes out as
 * \uXXXX; the rest passes through, so UTF-8 survives untouched.
 */
static void
json_str(struct buf *b, const char *s)
{
	buf_add_char(b, '"');
	for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
		switch (*p) {
		case '"':
			buf_add(b, "\\\"");
			break;
		case '\\':
			buf_add(b, "\\\\");
			break;
		case '\n':
			buf_add(b, "\\n");
			break;
		case '\r':
			buf_add(b, "\\r");
			break;
		case '\t':
			buf_add(b, "\\t");
			break;
		default:
			if (*p < 0x20) {
				buf_add_fmt(b, "\\u%04x", *p);
			} else {
				buf_add_char(b, (char)*p);
			}
		}
	}
	buf_add_char(b, '"');
}

static void
json_kv_str(struct buf *b, const char *key, const char *value)
{
	json_str(b, key);
	buf_add_char(b, ':');
	json_str(b, value);
}

/* ── JSON in ────────────────────────────────────────────────────── */

/*
 * A scanner, not a parser. The request grammar is three keys and two
 * value shapes; a real JSON reader here would be more code than the
 * whole protocol and would still have to refuse everything this refuses.
 * A key matches only where a key can appear — after `{` or `,` — so a
 * title containing the word "cmd" cannot be mistaken for one.
 */
static const char *
find_key(const char *line, const char *key)
{
	size_t klen = strlen(key);
	for (const char *p = line; (p = strchr(p, '"')); p++) {
		const char *before = p;
		while (before > line && isspace((unsigned char)before[-1])) {
			before--;
		}
		if (before == line || (before[-1] != '{' && before[-1] != ',')) {
			continue;
		}
		if (strncmp(p + 1, key, klen) || p[1 + klen] != '"') {
			continue;
		}
		const char *v = p + 2 + klen;
		while (isspace((unsigned char)*v)) {
			v++;
		}
		if (*v != ':') {
			continue;
		}
		v++;
		while (isspace((unsigned char)*v)) {
			v++;
		}
		return v;
	}
	return NULL;
}

/* No escape decoding: every value this protocol carries is an action or
 * command name, and one that needs an escape is one we would refuse. */
static bool
req_str(const char *line, const char *key, char *out, size_t len)
{
	const char *v = find_key(line, key);
	if (!v || *v != '"') {
		return false;
	}
	v++;
	size_t n = 0;
	while (*v && *v != '"' && *v != '\\') {
		if (n + 1 >= len) {
			return false;
		}
		out[n++] = *v++;
	}
	if (*v != '"') {
		return false;
	}
	out[n] = '\0';
	return true;
}

static bool
req_int(const char *line, const char *key, uint64_t *out)
{
	const char *v = find_key(line, key);
	if (!v || !isdigit((unsigned char)*v)) {
		return false;
	}
	char *end = NULL;
	unsigned long long n = strtoull(v, &end, 10);
	if (end == v) {
		return false;
	}
	*out = (uint64_t)n;
	return true;
}

/* ── the commands ───────────────────────────────────────────────── */

static int
workspace_index(struct workspace *workspace)
{
	int i = 1;
	struct workspace *w;
	wl_list_for_each(w, &server.workspaces.all, link) {
		if (w == workspace) {
			return i;
		}
		i++;
	}
	return 0;
}

static void
cmd_list(struct buf *b)
{
	buf_add(b, "{\"ok\":true,\"views\":[");
	bool first = true;
	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		if (!first) {
			buf_add_char(b, ',');
		}
		first = false;

		buf_add_fmt(b, "{\"id\":%" PRIu64 ",", view->creation_id);
		json_kv_str(b, "app_id", view->app_id);
		buf_add_char(b, ',');
		json_kv_str(b, "title", view->title);
		buf_add_fmt(b, ",\"workspace\":%d", workspace_index(view->workspace));
		buf_add_fmt(b, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d",
			view->current.x, view->current.y,
			view->current.width, view->current.height);
		/*
		 * The pid is what makes a client able to say WHOSE window
		 * this is — kdos-teams walks the ppid chain to the conmon
		 * that supervises a boxed app, the same walk `kdos stutter`
		 * does, and there is no other route from a toplevel to a box
		 * name.
		 */
		pid_t pid = view->impl->get_pid ? view->impl->get_pid(view) : -1;
		buf_add_fmt(b, ",\"pid\":%d", (int)pid);
		/*
		 * THE BOX, and it is not derived from the pid. The ppid walk to
		 * conmon is what a client OUTSIDE the compositor has to do;
		 * here the security context says it outright, which is both
		 * cheaper and correct for a client that is tagged but not
		 * containerised. Empty for a host window.
		 */
		buf_add_char(b, ',');
		json_kv_str(b, "box", kdos_view_box(view));
		buf_add_char(b, ',');
		json_kv_str(b, "instance", kdos_view_instance(view));
		buf_add_fmt(b, ",\"focused\":%s",
			view == server.active_view ? "true" : "false");
		buf_add_fmt(b, ",\"minimized\":%s", view->minimized ? "true" : "false");
		buf_add_fmt(b, ",\"maximized\":%s",
			view->maximized != VIEW_AXIS_NONE ? "true" : "false");
		buf_add_fmt(b, ",\"fullscreen\":%s", view->fullscreen ? "true" : "false");
		buf_add_fmt(b, ",\"shaded\":%s}", view->shaded ? "true" : "false");
	}
	buf_add(b, "]}");
}

/*
 * BOXES. `{"cmd":"boxes"}` -> the distinct boxes with a window on the screen.
 * `kdos-box gc` asks this before stopping anything: a box whose window is
 * mapped is not idle, whatever its clock says, and deriving that from `list`
 * in the client would put the same reduction in two places.
 */
static void
cmd_boxes(struct buf *b)
{
	const char *seen[64];
	int nseen = 0;
	struct view *view;

	buf_add(b, "{\"ok\":true,\"boxes\":[");
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		const char *box = kdos_view_box(view);
		int i;

		if (!box || !*box) {
			continue;
		}
		for (i = 0; i < nseen; i++) {
			if (!strcmp(seen[i], box)) {
				break;
			}
		}
		if (i < nseen || nseen >= 64) {
			continue;
		}
		if (nseen) {
			buf_add_char(b, ',');
		}
		seen[nseen++] = box;
		buf_add_char(b, '"');
		buf_add(b, box);
		buf_add_char(b, '"');
	}
	buf_add(b, "]}");
}

static void
cmd_outputs(struct buf *b)
{
	buf_add(b, "{\"ok\":true,\"outputs\":[");
	bool first = true;
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		if (!output->wlr_output) {
			continue;
		}
		struct wlr_box box = {0};
		wlr_output_layout_get_box(server.output_layout,
			output->wlr_output, &box);
		if (!first) {
			buf_add_char(b, ',');
		}
		first = false;
		buf_add_char(b, '{');
		json_kv_str(b, "name", output->wlr_output->name);
		buf_add_fmt(b, ",\"w\":%d,\"h\":%d,\"scale\":%.3f,\"x\":%d,\"y\":%d",
			box.width, box.height, output->wlr_output->scale,
			box.x, box.y);
		buf_add_fmt(b, ",\"enabled\":%s}",
			output->wlr_output->enabled ? "true" : "false");
	}
	buf_add(b, "]}");
}

static void
resp_err(struct buf *b, const char *reason)
{
	buf_add(b, "{\"ok\":false,");
	json_kv_str(b, "err", reason);
	buf_add_char(b, '}');
}

static struct view *
view_by_id(uint64_t id)
{
	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		if (view->creation_id == id) {
			return view;
		}
	}
	return NULL;
}

/*
 * PEEK. `{"cmd":"peek","on":true}` fades every window; false restores them.
 *
 * A verb rather than an action, because it is neither: an action is a thing a
 * keybind does once, and this is a state that lasts exactly as long as a
 * pointer dwells somewhere. It takes no view and names nothing, so there is
 * nothing here to aim.
 *
 * ABSENT `on` IS OFF. A caller that says "peek" and nothing else is asking for
 * something it did not specify, and the safe reading of that is the state that
 * cannot get stuck — a peek left on is a desktop whose windows have all gone
 * transparent with no obvious way back.
 */
static void
cmd_peek(struct buf *b, const char *line)
{
	uint64_t on = 0;

	req_int(line, "on", &on);
	if (!on && strstr(line, "\"on\":true")) {
		on = 1;
	}
	kdos_peek_set(on != 0);
	buf_add(b, "{\"ok\":true}");
}

/*
 * THUMB. `{"cmd":"thumb","app_id":"foot","w":64,"h":36}` writes a picture of
 * that application's front window and answers with the path.
 *
 * THE PATH IS THE COMPOSITOR'S TO CHOOSE and is never taken from the request.
 * A verb that wrote wherever a caller pointed it would be a file-write
 * primitive with a compositor's privileges, on a socket whose only gate is the
 * peer's uid — and the appbox shares that uid. One fixed name in
 * $XDG_RUNTIME_DIR, overwritten every time.
 *
 * Answering ok:false for a window whose pixels cannot be read is not a failure
 * to report loudly: a dmabuf client simply has no host-readable buffer, which
 * is the common case on real hardware, and the caller's job is to carry on
 * without a picture.
 */
static void
cmd_thumb(struct buf *b, const char *line)
{
	char app[128];
	uint64_t w = 0, h = 0;
	const char *run = getenv("XDG_RUNTIME_DIR");
	char path[512];

	if (!req_str(line, "app_id", app, sizeof(app))) {
		resp_err(b, "thumb needs an \"app_id\"");
		return;
	}
	if (!run || !*run) {
		resp_err(b, "no XDG_RUNTIME_DIR");
		return;
	}
	req_int(line, "w", &w);
	req_int(line, "h", &h);
	snprintf(path, sizeof(path), "%s/kdos-thumb.ppm", run);
	if (!kdos_thumb_write(app, (int)w, (int)h, path)) {
		resp_err(b, "no readable buffer for that window");
		return;
	}
	buf_add(b, "{\"ok\":true,");
	json_kv_str(b, "path", path);
	buf_add_char(b, '}');
}

static void
cmd_run(struct buf *b, const char *line)
{
	char name[64];
	if (!req_str(line, "action", name, sizeof(name))) {
		resp_err(b, "run needs an \"action\"");
		return;
	}

	struct view *activator = NULL;
	uint64_t id = 0;
	if (req_int(line, "id", &id)) {
		activator = view_by_id(id);
		if (!activator) {
			resp_err(b, "no view with that id");
			return;
		}
	}

	/*
	 * The name is resolved by the same table rc.xml is parsed against,
	 * so `kdos hey` can reach exactly the actions a keybind can and
	 * nothing else. An action needing an argument this protocol has no
	 * way to carry (Execute's command, SnapToEdge's direction) fails
	 * action_is_valid() and is refused HERE — actions_run() would run
	 * it as a no-op and the caller would never learn.
	 */
	struct action *action = action_create(name);
	if (!action) {
		resp_err(b, "unknown action");
		return;
	}
	if (!action_is_valid(action)) {
		action_free(action);
		resp_err(b, "action needs arguments this socket cannot carry");
		return;
	}

	struct wl_list actions;
	wl_list_init(&actions);
	wl_list_append(&actions, &action->link);
	actions_run(activator, &actions, /*ctx*/ NULL);
	action_list_free(&actions);

	buf_add(b, "{\"ok\":true}");
}

static void
dispatch(struct buf *b, const char *line)
{
	char cmd[32];
	if (!req_str(line, "cmd", cmd, sizeof(cmd))) {
		resp_err(b, "no \"cmd\" in request");
		return;
	}
	if (!strcmp(cmd, "list")) {
		cmd_list(b);
	} else if (!strcmp(cmd, "boxes")) {
		cmd_boxes(b);
	} else if (!strcmp(cmd, "outputs")) {
		cmd_outputs(b);
	} else if (!strcmp(cmd, "run")) {
		cmd_run(b, line);
	} else if (!strcmp(cmd, "peek")) {
		cmd_peek(b, line);
	} else if (!strcmp(cmd, "thumb")) {
		cmd_thumb(b, line);
	} else {
		resp_err(b,
			"unknown cmd — list, run, outputs, peek or thumb");
	}
	buf_add_char(b, '\n');
}

/* ── the socket ─────────────────────────────────────────────────── */

static void
client_drop(struct kc_client *c)
{
	if (c->src) {
		wl_event_source_remove(c->src);
	}
	if (c->idle) {
		wl_event_source_remove(c->idle);
	}
	close(c->fd);
	buf_reset(&c->out);
	wl_list_remove(&c->link);
	kc->nclients--;
	free(c);
}

/* Re-armed on every byte in either direction: the deadline is on being
 * STUCK, not on the exchange taking a moment. */
static void
client_touch(struct kc_client *c)
{
	if (c->idle) {
		wl_event_source_timer_update(c->idle, KC_IDLE_MS);
	}
}

static int
client_idle_out(void *data)
{
	client_drop(data);
	return 0;
}

/* Returns false when the client is gone. */
static bool
client_flush(struct kc_client *c)
{
	while (c->out_sent < (size_t)c->out.len) {
		ssize_t n = write(c->fd, c->out.data + c->out_sent,
			(size_t)c->out.len - c->out_sent);
		if (n > 0) {
			c->out_sent += (size_t)n;
			client_touch(c);
			continue;
		}
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0 && errno == EAGAIN) {
			/* the answer waits on the loop, never on us — and on
			 * the idle timer, or a peer that never reads keeps
			 * this buffer for the session */
			if (!c->writing) {
				c->writing = true;
				wl_event_source_fd_update(c->src, WL_EVENT_WRITABLE);
			}
			return true;
		}
		client_drop(c);
		return false;
	}
	client_drop(c);	/* one request, one response, close */
	return false;
}

static int
client_ready(int fd, uint32_t mask, void *data)
{
	struct kc_client *c = data;
	(void)fd;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		client_drop(c);
		return 0;
	}
	if (c->writing) {
		client_flush(c);
		return 0;
	}
	if (!(mask & WL_EVENT_READABLE)) {
		return 0;
	}

	ssize_t n = read(c->fd, c->in + c->in_len, sizeof(c->in) - 1 - c->in_len);
	if (n < 0) {
		if (errno == EAGAIN || errno == EINTR) {
			return 0;
		}
		client_drop(c);
		return 0;
	}
	if (n == 0) {
		client_drop(c);	/* half-close before a whole request */
		return 0;
	}
	c->in_len += (size_t)n;
	c->in[c->in_len] = '\0';
	client_touch(c);

	char *nl = memchr(c->in, '\n', c->in_len);
	if (!nl) {
		if (c->in_len >= sizeof(c->in) - 1) {
			resp_err(&c->out, "request too long");
			buf_add_char(&c->out, '\n');
			client_flush(c);
		}
		return 0;	/* still arriving */
	}
	*nl = '\0';
	dispatch(&c->out, c->in);
	client_flush(c);
	return 0;
}

/*
 * The kernel fills these in and the peer cannot lie about them, which is
 * why this is on the connection rather than in the protocol. Only this
 * session's own user may run actions in it.
 */
static bool
peer_allowed(int cfd)
{
	struct ucred cred = { 0 };
	socklen_t len = sizeof(cred);
	if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0) {
		return false;
	}
	return cred.uid == geteuid();
}

static int
listen_ready(int fd, uint32_t mask, void *data)
{
	(void)mask;
	(void)data;
	int cfd = accept(fd, NULL, NULL);
	if (cfd < 0) {
		return 0;
	}
	if (!peer_allowed(cfd)) {
		wlr_log(WLR_INFO, "cmd: refused a connection from another user");
		close(cfd);
		return 0;
	}
	if (kc->nclients >= KC_MAX_CLIENTS) {
		wlr_log(WLR_INFO, "cmd: %d connections already open, refusing",
			kc->nclients);
		close(cfd);
		return 0;
	}
	int fl = fcntl(cfd, F_GETFL, 0);
	fcntl(cfd, F_SETFL, (fl < 0 ? 0 : fl) | O_NONBLOCK);
	fcntl(cfd, F_SETFD, FD_CLOEXEC);

	struct kc_client *c = calloc(1, sizeof(*c));
	if (!c) {
		close(cfd);
		return 0;
	}
	c->fd = cfd;
	struct wl_event_loop *loop =
		wl_display_get_event_loop(server.wl_display);
	c->src = wl_event_loop_add_fd(loop, cfd, WL_EVENT_READABLE,
		client_ready, c);
	c->idle = wl_event_loop_add_timer(loop, client_idle_out, c);
	wl_list_insert(&kc->clients, &c->link);
	kc->nclients++;
	client_touch(c);
	return 0;
}

/* ── setup ──────────────────────────────────────────────────────── */

void
kdos_cmd_init(void)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt || !*rt) {
		wlr_log(WLR_INFO, "cmd: no XDG_RUNTIME_DIR, `kdos hey` is off");
		return;
	}

	struct kc *k = calloc(1, sizeof(*k));
	if (!k) {
		return;
	}
	k->listen_fd = -1;
	wl_list_init(&k->clients);
	snprintf(k->path, sizeof(k->path), "%s/kdos-cmd.sock", rt);

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	if (strlen(k->path) >= sizeof(addr.sun_path)) {
		wlr_log(WLR_ERROR, "cmd: %s is too long for a unix socket",
			k->path);
		free(k);
		return;
	}
	strcpy(addr.sun_path, k->path);

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		free(k);
		return;
	}
	/* per-user and per-boot: a stale socket from an unclean shutdown
	 * must not stop this session answering */
	unlink(k->path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0
			|| listen(fd, 4) < 0) {
		wlr_log(WLR_ERROR, "cmd: cannot listen on %s: %s",
			k->path, strerror(errno));
		close(fd);
		free(k);
		return;
	}
	k->listen_fd = fd;
	kc = k;
	k->listen_src = wl_event_loop_add_fd(
		wl_display_get_event_loop(server.wl_display), fd,
		WL_EVENT_READABLE, listen_ready, k);
	wlr_log(WLR_INFO, "cmd: `kdos hey` on %s", k->path);
}

void
kdos_cmd_finish(void)
{
	if (!kc) {
		return;
	}
	struct kc_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &kc->clients, link) {
		client_drop(c);
	}
	if (kc->listen_src) {
		wl_event_source_remove(kc->listen_src);
	}
	if (kc->listen_fd >= 0) {
		close(kc->listen_fd);
	}
	if (*kc->path) {
		unlink(kc->path);
	}
	free(kc);
	kc = NULL;
}
