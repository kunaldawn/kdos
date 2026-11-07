/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the windows KWin has open, for `kdos hey` and `kdos appid`
 *
 * KWin's window list is the Wayland global org_kde_plasma_window_management:
 * bound, it names every window by uuid, and each window asked for by uuid
 * reports its title, app id, pid, state and resource name, and takes a state
 * change or a close. KWin offers it to every client outside a sandbox; a
 * boxed client's security context is refused it, which is why this is asked
 * on the host and never from inside a box.
 *
 * THE WAYLAND WIRE PROTOCOL BY HAND, NOT libwayland-client. The conversation
 * is a registry, one bind, one request per window and a few roundtrips: a
 * hundred lines of framing. A generated client would make kdos-tools wait in
 * its build for the Plasma protocol files, which come a phase later, and put
 * a library under the multi-call binary the initramfs carries as
 * kdos-bootctl. The opcodes below are those of plasma-wayland-protocols'
 * plasma-window-management.xml (interface version 21); a request or event
 * added after it is never sent here and is skipped by its length when it
 * arrives.
 *
 * WHERE: $WAYLAND_DISPLAY, absolute or under $XDG_RUNTIME_DIR, else
 * $XDG_RUNTIME_DIR/wayland-0 — `kdos-box gc` runs from the timer service,
 * which has no WAYLAND_DISPLAY.
 * ---------------------------------
 */

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "kdos-tools.h"

#define PWM_IFACE "org_kde_plasma_window_management"
/* The highest version whose events are all known below. */
#define PWM_VERSION 21
/* "This global must be bound with a version at least 17." */
#define PWM_MIN 17

/* wl_display, wl_registry, wl_callback */
enum { DISPLAY_SYNC = 0, DISPLAY_GET_REGISTRY = 1 };
enum { DISPLAY_ERROR = 0, DISPLAY_DELETE_ID = 1 };
enum { REGISTRY_BIND = 0 };
enum { REGISTRY_GLOBAL = 0 };
enum { CALLBACK_DONE = 0 };
/* org_kde_plasma_window_management */
enum { PWM_GET_WINDOW_BY_UUID = 2 };
enum { PWM_WINDOW = 1, PWM_WINDOW_WITH_UUID = 4 };
/* org_kde_plasma_window */
enum { WIN_SET_STATE = 0, WIN_CLOSE = 4 };
enum {
	WIN_TITLE = 0, WIN_APP_ID = 1, WIN_STATE = 2, WIN_UNMAPPED = 5,
	WIN_INITIAL_STATE = 6, WIN_PID = 10, WIN_RESOURCE_NAME = 16,
};
/* org_kde_plasma_window_management.state */
enum {
	ST_ACTIVE = 0x1, ST_MINIMIZED = 0x2, ST_MAXIMIZED = 0x4,
	ST_FULLSCREEN = 0x8, ST_KEEP_ABOVE = 0x10,
};

#define MAX_WIN 1024

typedef struct {
	char uuid[128];
	char title[512];
	char app_id[256];
	char resource[256];
	uint32_t state;
	uint32_t pid;
	int unmapped;
	int obj;		/* its object id, or 0 before the request */
} Win;

static int sock = -1;
/*
 * A CLIENT'S OBJECT IDS ARE HANDED OUT IN ORDER, from 2 (1 is the display),
 * with no gap: libwayland-server refuses an id past the next free one as
 * "not a valid new object id" and drops the connection. So every object —
 * the registry, each sync callback, the bind, each window — takes the next
 * id at the moment it is created.
 */
static uint32_t registry_id, sync_id, pwm_id;
static uint32_t pwm_name, pwm_version;
static uint32_t next_id = 2;
static int synced, failed;
static Win wins[MAX_WIN];
static int nwin;

/* ── sending ───────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t w[512];
	int n;
} Msg;

static void m_begin(Msg *m, uint32_t obj, uint16_t op)
{
	m->n = 2;
	m->w[0] = obj;
	m->w[1] = op;
}

static void m_uint(Msg *m, uint32_t v)
{
	m->w[m->n++] = v;
}

static void m_str(Msg *m, const char *s)
{
	uint32_t len = (uint32_t)strlen(s) + 1;
	uint32_t words = (len + 3) / 4;

	m->w[m->n++] = len;
	memset(&m->w[m->n], 0, words * 4);
	memcpy(&m->w[m->n], s, len);
	m->n += (int)words;
}

static int m_send(Msg *m)
{
	size_t bytes = (size_t)m->n * 4, off = 0;

	m->w[1] |= (uint32_t)bytes << 16;
	while (off < bytes) {
		ssize_t r = send(sock, (char *)m->w + off, bytes - off,
				 MSG_NOSIGNAL);

		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)r;
	}
	return 0;
}

static int sync_request(void)
{
	Msg m;

	sync_id = next_id++;
	synced = 0;
	m_begin(&m, 1, DISPLAY_SYNC);
	m_uint(&m, sync_id);
	return m_send(&m);
}

/* ── receiving ─────────────────────────────────────────────────────────── */

static unsigned char rbuf[1 << 17];
static size_t rlen;

/* A string argument at *p within an event body of `end`: copied with any tab
 * or newline as a space, so a field can never break a line. */
static const unsigned char *get_str(const unsigned char *p,
				    const unsigned char *end, char *out,
				    size_t n)
{
	uint32_t len;

	out[0] = '\0';
	if (p + 4 > end)
		return end;
	memcpy(&len, p, 4);
	p += 4;
	if (len == 0)
		return p;
	if ((size_t)(end - p) < len)
		return end;
	{
		size_t k = len - 1 < n - 1 ? len - 1 : n - 1;

		for (size_t i = 0; i < k; i++) {
			char c = (char)p[i];

			out[i] = (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
		}
		out[k] = '\0';
	}
	return p + ((len + 3) & ~3u);
}

static uint32_t get_u(const unsigned char *p, const unsigned char *end)
{
	uint32_t v = 0;

	if (p + 4 <= end)
		memcpy(&v, p, 4);
	return v;
}

static Win *win_of(uint32_t obj)
{
	for (int i = 0; i < nwin; i++)
		if ((uint32_t)wins[i].obj == obj)
			return &wins[i];
	return NULL;
}

static void event(uint32_t obj, uint16_t op, const unsigned char *p,
		  const unsigned char *end)
{
	char s[512];

	if (obj == 1 && op == DISPLAY_ERROR) {
		get_str(p + 8, end, s, sizeof(s));
		fprintf(stderr, "kdos: the compositor refused: %s\n", s);
		failed = 1;
		return;
	}
	if (obj == registry_id && op == REGISTRY_GLOBAL) {
		uint32_t name = get_u(p, end);
		const unsigned char *q = get_str(p + 4, end, s, sizeof(s));

		if (!strcmp(s, PWM_IFACE)) {
			pwm_name = name;
			pwm_version = get_u(q, end);
		}
		return;
	}
	if (obj == sync_id && op == CALLBACK_DONE) {
		synced = 1;
		return;
	}
	if (pwm_id && obj == pwm_id && op == PWM_WINDOW_WITH_UUID) {
		if (nwin < MAX_WIN) {
			memset(&wins[nwin], 0, sizeof(wins[0]));
			get_str(p + 4, end, wins[nwin].uuid,
				sizeof(wins[nwin].uuid));
			nwin++;
		}
		return;
	}
	{
		Win *w = win_of(obj);

		if (!w)
			return;
		switch (op) {
		case WIN_TITLE:
			get_str(p, end, w->title, sizeof(w->title));
			break;
		case WIN_APP_ID:
			get_str(p, end, w->app_id, sizeof(w->app_id));
			break;
		case WIN_RESOURCE_NAME:
			get_str(p, end, w->resource, sizeof(w->resource));
			break;
		case WIN_STATE:
			w->state = get_u(p, end);
			break;
		case WIN_PID:
			w->pid = get_u(p, end);
			break;
		case WIN_UNMAPPED:
			w->unmapped = 1;
			break;
		default:
			break;
		}
	}
}

/* Read until the pending sync's callback is done. Any descriptor the
 * compositor passes (none of these events carries one) is closed. */
static int roundtrip(void)
{
	if (sync_request() != 0)
		return -1;
	while (!synced && !failed) {
		struct pollfd pf = { .fd = sock, .events = POLLIN };
		char cbuf[CMSG_SPACE(sizeof(int) * 28)];
		struct iovec iov = { rbuf + rlen, sizeof(rbuf) - rlen };
		struct msghdr mh = { 0 };
		ssize_t r;
		size_t off = 0;

		if (poll(&pf, 1, 5000) <= 0) {
			fprintf(stderr, "kdos: the compositor did not "
					"answer\n");
			return -1;
		}
		mh.msg_iov = &iov;
		mh.msg_iovlen = 1;
		mh.msg_control = cbuf;
		mh.msg_controllen = sizeof(cbuf);
		r = recvmsg(sock, &mh, MSG_CMSG_CLOEXEC);
		if (r <= 0) {
			if (r < 0 && errno == EINTR)
				continue;
			fprintf(stderr, "kdos: the compositor closed "
					"the connection\n");
			return -1;
		}
		for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c;
		     c = CMSG_NXTHDR(&mh, c))
			if (c->cmsg_level == SOL_SOCKET &&
			    c->cmsg_type == SCM_RIGHTS) {
				int nfd = (int)((c->cmsg_len - CMSG_LEN(0)) /
						sizeof(int));
				int fds[28];

				memcpy(fds, CMSG_DATA(c), (size_t)nfd * sizeof(int));
				for (int i = 0; i < nfd; i++)
					close(fds[i]);
			}
		rlen += (size_t)r;
		while (rlen - off >= 8) {
			uint32_t obj, hdr, size;

			memcpy(&obj, rbuf + off, 4);
			memcpy(&hdr, rbuf + off + 4, 4);
			size = hdr >> 16;
			if (size < 8) {
				fprintf(stderr, "kdos: a malformed "
						"message\n");
				return -1;
			}
			if (rlen - off < size)
				break;
			event(obj, (uint16_t)(hdr & 0xffff), rbuf + off + 8,
			      rbuf + off + size);
			off += size;
		}
		if (failed)
			return -1;
		memmove(rbuf, rbuf + off, rlen - off);
		rlen -= off;
		if (rlen == sizeof(rbuf)) {
			fprintf(stderr, "kdos: a message too large\n");
			return -1;
		}
	}
	return 0;
}

/* ── the conversation ──────────────────────────────────────────────────── */

static int connect_display(void)
{
	const char *wd = getenv("WAYLAND_DISPLAY");
	const char *rt = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	int n;

	if (wd && *wd == '/')
		n = snprintf(a.sun_path, sizeof(a.sun_path), "%s", wd);
	else if (rt && *rt)
		n = snprintf(a.sun_path, sizeof(a.sun_path), "%s/%s", rt,
			     wd && *wd ? wd : "wayland-0");
	else {
		fprintf(stderr, "kdos: no XDG_RUNTIME_DIR, so no "
				"session to ask\n");
		return -1;
	}
	if (n < 0 || (size_t)n >= sizeof(a.sun_path))
		return -1;
	sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock < 0 || connect(sock, (struct sockaddr *)&a, sizeof(a)) != 0) {
		fprintf(stderr, "kdos: no compositor at %s\n",
			a.sun_path);
		return -1;
	}
	return 0;
}

static int load(void)
{
	Msg m;
	uint32_t v;

	if (connect_display() != 0)
		return -1;
	registry_id = next_id++;
	m_begin(&m, 1, DISPLAY_GET_REGISTRY);
	m_uint(&m, registry_id);
	if (m_send(&m) != 0 || roundtrip() != 0)
		return -1;
	if (!pwm_name || pwm_version < PWM_MIN) {
		fprintf(stderr, "kdos: the compositor gives this "
				"program no window list\n");
		return -1;
	}
	v = pwm_version < PWM_VERSION ? pwm_version : PWM_VERSION;
	pwm_id = next_id++;
	m_begin(&m, registry_id, REGISTRY_BIND);
	m_uint(&m, pwm_name);
	m_str(&m, PWM_IFACE);
	m_uint(&m, v);
	m_uint(&m, pwm_id);
	if (m_send(&m) != 0 || roundtrip() != 0)
		return -1;
	for (int i = 0; i < nwin; i++) {
		wins[i].obj = (int)next_id++;
		m_begin(&m, pwm_id, PWM_GET_WINDOW_BY_UUID);
		m_uint(&m, (uint32_t)wins[i].obj);
		m_str(&m, wins[i].uuid);
		if (m_send(&m) != 0)
			return -1;
	}
	return roundtrip();
}

static void wm_reset(void)
{
	if (sock >= 0)
		close(sock);
	sock = -1;
	registry_id = sync_id = pwm_id = pwm_name = pwm_version = 0;
	next_id = 2;
	synced = failed = 0;
	nwin = 0;
	rlen = 0;
}

int kdt_windows(KdtWindow **out)
{
	KdtWindow *w;
	int n = 0;

	*out = NULL;
	wm_reset();
	if (load() != 0) {
		wm_reset();
		return -1;
	}
	w = kb_calloc((size_t)nwin + 1, sizeof(*w));
	for (int i = 0; i < nwin; i++) {
		if (wins[i].unmapped)
			continue;
		w[n].uuid = kb_strdup(wins[i].uuid);
		w[n].app_id = kb_strdup(wins[i].app_id);
		w[n].resource = kb_strdup(wins[i].resource);
		w[n].title = kb_strdup(wins[i].title);
		w[n].state = wins[i].state;
		w[n].pid = (long)wins[i].pid;
		n++;
	}
	wm_reset();
	*out = w;
	return n;
}

void kdt_windows_free(KdtWindow *w, int n)
{
	for (int i = 0; i < n; i++) {
		free(w[i].uuid);
		free(w[i].app_id);
		free(w[i].resource);
		free(w[i].title);
	}
	free(w);
}

int kdt_window_act(const char *uuid, const char *action)
{
	static const struct {
		const char *name;
		uint32_t flags, state;
	} A[] = {
		{ "activate", ST_ACTIVE | ST_MINIMIZED, ST_ACTIVE },
		{ "minimize", ST_MINIMIZED, ST_MINIMIZED },
		{ "unminimize", ST_MINIMIZED, 0 },
		{ "maximize", ST_MAXIMIZED, ST_MAXIMIZED },
		{ "unmaximize", ST_MAXIMIZED, 0 },
		{ "fullscreen", ST_FULLSCREEN, ST_FULLSCREEN },
		{ "unfullscreen", ST_FULLSCREEN, 0 },
		{ "keep-above", ST_KEEP_ABOVE, ST_KEEP_ABOVE },
		{ "no-keep-above", ST_KEEP_ABOVE, 0 },
	};
	Win *w = NULL;
	Msg m;
	size_t i = 0;
	int rc;

	if (strcmp(action, "close")) {
		for (i = 0; i < sizeof(A) / sizeof(A[0]); i++)
			if (!strcmp(A[i].name, action))
				break;
		if (i == sizeof(A) / sizeof(A[0]))
			return 2;
	}
	wm_reset();
	if (load() != 0) {
		wm_reset();
		return -1;
	}
	for (int k = 0; k < nwin; k++)
		if (!strcmp(wins[k].uuid, uuid) && !wins[k].unmapped)
			w = &wins[k];
	if (!w) {
		wm_reset();
		return 1;
	}
	if (!strcmp(action, "close")) {
		m_begin(&m, (uint32_t)w->obj, WIN_CLOSE);
	} else {
		m_begin(&m, (uint32_t)w->obj, WIN_SET_STATE);
		m_uint(&m, A[i].flags);
		m_uint(&m, A[i].state);
	}
	rc = m_send(&m) != 0 || roundtrip() != 0 ? -1 : 0;
	wm_reset();
	return rc;
}
