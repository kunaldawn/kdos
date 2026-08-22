/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-teams — the Team Monitor
 *
 *   ╔═ teams ════════════════════════════════════════════════╗
 *   ║ ▶ firefox-esr   Wikipedia   (appbox kdos-apps)  ws 2   ║
 *   ║   foot          ~           4231                ws 1   ║
 *   ╚════════════════════════════════════════════════════════╝
 *
 * Haiku's Team Monitor, and the escape hatch this desktop did not have: a
 * fullscreen alien app that has stopped answering owns the screen, and the
 * only way out was a tty. Enter is the POLITE close the protocol already has —
 * an editor still gets to ask about unsaved work — and `k` is SIGTERM to the
 * process behind it, which is the answer when the polite one is being ignored.
 *
 * The window list comes from kdos-comp's command socket rather than from
 * wlr-foreign-toplevel, because this surface needs what that protocol does not
 * carry: the PID. Without it there is no route from a toplevel to the box it
 * runs in and no process to signal — the list would be the panel's taskbar
 * with fewer manners.
 *
 * The box identity is the ppid walk `kdos stutter` documents: rootless podman
 * gets no cgroup delegation without systemd, so a boxed process frequently
 * sits in `0::/` and says nothing at all, while the conmon that supervises it
 * carries `-n <name>` in its argv. A few file reads, and no podman call — this
 * runs while somebody is already looking at a wedged screen.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "kwl.h"
#include "kproc.h"
#include "shell.h"

#define TEAMS_COLS 78
#define TEAMS_ROWS 20
#define TEAMS_MAX  64
#define TEAMS_REPLY (256 * 1024)

struct tview {
	long id;
	int pid;
	int ws;
	int focused, minimized, maximized, fullscreen, shaded;
	char app[64];
	char title[128];
	char box[48];		/* the appbox name, or empty */
};

static struct tview views[TEAMS_MAX];
static int nviews;

/* What went wrong, in the words the user gets. Empty means the list is real. */
static char errline[120];
/* Whether the last request reached the socket at all. An empty list means two
 * different things — nothing is open, or nothing answered — and the surface
 * has to say which, or "the desktop has no windows" is what a broken socket
 * looks like. */
static int sock_up;
/* The result of the last action, one line under the list. */
static char status[120];

/* ── a read-only JSON scanner ──────────────────────────────────────────── */

/*
 * Exactly enough of JSON to read one reply from one program on this machine.
 * It is here rather than borrowed because kdos-shell links libktui, libkcolor
 * and libkwl, and libkbuild's scanner (kj_*) belongs to the build host.
 *
 * It is string-aware rather than a strstr for `"pid":`, and that is not
 * pedantry: a window TITLE is arbitrary text chosen by somebody else, and a
 * text editor showing this very protocol would otherwise be able to make the
 * monitor read a key out of its own title bar.
 */
enum { JV_NONE = 0, JV_STR, JV_NUM, JV_LIT };

struct jv {
	const char *s;
	size_t n;
	int t;
};

static const char *j_ws(const char *p, const char *e)
{
	while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

/* Past the end of the value starting at `p`, or NULL if it does not close. */
static const char *j_skip(const char *p, const char *e)
{
	int depth = 0;

	if (p >= e)
		return NULL;
	if (*p == '"') {
		for (p++; p < e; p++) {
			if (*p == '\\') {
				p++;
				continue;
			}
			if (*p == '"')
				return p + 1;
		}
		return NULL;
	}
	if (*p == '{' || *p == '[') {
		for (; p < e; p++) {
			if (*p == '"') {
				const char *q = j_skip(p, e);
				if (!q)
					return NULL;
				p = q - 1;
				continue;
			}
			if (*p == '{' || *p == '[')
				depth++;
			else if (*p == '}' || *p == ']') {
				if (--depth == 0)
					return p + 1;
			}
		}
		return NULL;
	}
	while (p < e && *p != ',' && *p != '}' && *p != ']' &&
	       *p != ' ' && *p != '\n' && *p != '\t' && *p != '\r')
		p++;
	return p;
}

/* One member of the object at `obj`. Returns 1 when found. */
static int j_member(const char *obj, const char *e, const char *key,
		    struct jv *out)
{
	const char *p = j_ws(obj, e);

	if (p >= e || *p != '{')
		return 0;
	p = j_ws(p + 1, e);
	while (p < e && *p != '}') {
		const char *kend;
		if (*p != '"')
			return 0;
		kend = j_skip(p, e);
		if (!kend)
			return 0;
		size_t klen = (size_t)(kend - p) - 2;
		const char *kp = p + 1;

		p = j_ws(kend, e);
		if (p >= e || *p != ':')
			return 0;
		p = j_ws(p + 1, e);
		const char *vend = j_skip(p, e);
		if (!vend)
			return 0;

		if (klen == strlen(key) && !memcmp(kp, key, klen)) {
			out->t = *p == '"' ? JV_STR :
				 (*p == '-' || (*p >= '0' && *p <= '9')) ? JV_NUM
									 : JV_LIT;
			out->s = out->t == JV_STR ? p + 1 : p;
			out->n = out->t == JV_STR ? (size_t)(vend - p) - 2
						  : (size_t)(vend - p);
			return 1;
		}
		p = j_ws(vend, e);
		if (p < e && *p == ',')
			p = j_ws(p + 1, e);
	}
	return 0;
}

/* The first element of the array at `arr`, and the one after `cur`. */
static const char *j_first(const char *arr, const char *e)
{
	const char *p = j_ws(arr, e);

	if (p >= e || *p != '[')
		return NULL;
	p = j_ws(p + 1, e);
	return (p < e && *p != ']') ? p : NULL;
}

static const char *j_next(const char *cur, const char *e)
{
	const char *p = j_skip(cur, e);

	if (!p)
		return NULL;
	p = j_ws(p, e);
	if (p >= e || *p != ',')
		return NULL;
	p = j_ws(p + 1, e);
	return (p < e && *p != ']') ? p : NULL;
}

static void utf8_put(char **w, const char *end, unsigned cp)
{
	char buf[4];
	int n;

	if (cp < 0x80) {
		n = 1;
		buf[0] = (char)cp;
	} else if (cp < 0x800) {
		n = 2;
		buf[0] = (char)(0xc0 | (cp >> 6));
		buf[1] = (char)(0x80 | (cp & 0x3f));
	} else if (cp < 0x10000) {
		n = 3;
		buf[0] = (char)(0xe0 | (cp >> 12));
		buf[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		buf[2] = (char)(0x80 | (cp & 0x3f));
	} else {
		n = 4;
		buf[0] = (char)(0xf0 | (cp >> 18));
		buf[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
		buf[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
		buf[3] = (char)(0x80 | (cp & 0x3f));
	}
	if (*w + n >= end)
		return;
	memcpy(*w, buf, (size_t)n);
	*w += n;
}

static unsigned hex4(const char *p)
{
	unsigned v = 0;

	for (int i = 0; i < 4; i++) {
		char c = p[i];
		v <<= 4;
		if (c >= '0' && c <= '9')
			v |= (unsigned)(c - '0');
		else if (c >= 'a' && c <= 'f')
			v |= (unsigned)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v |= (unsigned)(c - 'A' + 10);
		else
			return 0;
	}
	return v;
}

static void j_str(const struct jv *v, char *out, size_t n)
{
	const char *p = v->s, *e = v->s + v->n;
	char *w = out, *wend = out + n - 1;

	if (v->t != JV_STR) {
		out[0] = '\0';
		return;
	}
	while (p < e && w < wend) {
		if (*p != '\\') {
			*w++ = *p++;
			continue;
		}
		if (++p >= e)
			break;
		switch (*p) {
		case 'n': *w++ = '\n'; p++; break;
		case 't': *w++ = '\t'; p++; break;
		case 'r': *w++ = '\r'; p++; break;
		case 'b': *w++ = '\b'; p++; break;
		case 'f': *w++ = '\f'; p++; break;
		case 'u':
			if (e - p >= 5) {
				unsigned cp = hex4(p + 1);
				p += 5;
				/* A surrogate pair is two escapes; a lone
				 * surrogate is not a codepoint and would be
				 * encoded as one if this fell through. */
				if (cp >= 0xd800 && cp < 0xdc00 &&
				    e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
					unsigned lo = hex4(p + 2);
					if (lo >= 0xdc00 && lo < 0xe000) {
						cp = 0x10000 +
						     ((cp - 0xd800) << 10) +
						     (lo - 0xdc00);
						p += 6;
					}
				}
				if (cp >= 0xd800 && cp < 0xe000)
					cp = '?';
				utf8_put(&w, wend, cp);
			} else {
				p = e;
			}
			break;
		default:
			*w++ = *p++;
			break;
		}
	}
	*w = '\0';
	/* A control character in a window title would move the cursor or eat
	 * the row it is drawn on; the grid takes text, not commands. */
	for (char *q = out; *q; q++)
		if ((unsigned char)*q < 0x20)
			*q = ' ';
}

static long j_int(const struct jv *v, long dflt)
{
	char buf[32];
	size_t n = v->n < sizeof(buf) - 1 ? v->n : sizeof(buf) - 1;

	if (v->t != JV_NUM)
		return dflt;
	memcpy(buf, v->s, n);
	buf[n] = '\0';
	return strtol(buf, NULL, 10);
}

static int j_bool(const struct jv *v)
{
	return v->t == JV_LIT && v->n >= 4 && !memcmp(v->s, "true", 4);
}

/* ── the command socket ────────────────────────────────────────────────── */

/*
 * One request, one reply, close — the whole protocol. Both timeouts are set
 * because this surface is drawn by the same loop that is waiting on the
 * answer: a compositor wedged badly enough not to reply is exactly the
 * situation the monitor is opened in, and a blocking read there would hang the
 * one program that could still fix it.
 */
static int cmd_call(const char *req, char *out, size_t n)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un a = { 0 };
	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	size_t len = strlen(req), sent = 0, got = 0;
	int fd;

	if (!rt || !*rt) {
		snprintf(errline, sizeof(errline),
			 "no XDG_RUNTIME_DIR — there is no session to ask");
		return -1;
	}
	a.sun_family = AF_UNIX;
	if ((size_t)snprintf(a.sun_path, sizeof(a.sun_path),
			     "%s/kdos-cmd.sock", rt) >= sizeof(a.sun_path)) {
		snprintf(errline, sizeof(errline), "socket path too long");
		return -1;
	}

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		snprintf(errline, sizeof(errline), "socket: %s",
			 strerror(errno));
		return -1;
	}
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
		snprintf(errline, sizeof(errline),
			 "the compositor does not expose the command socket");
		close(fd);
		return -1;
	}
	while (sent < len) {
		ssize_t w = write(fd, req + sent, len - sent);
		if (w <= 0) {
			snprintf(errline, sizeof(errline), "write: %s",
				 strerror(errno));
			close(fd);
			return -1;
		}
		sent += (size_t)w;
	}
	/* One line: the reply ends at the newline the server appends, and
	 * waiting for EOF instead would wait out the whole close. */
	while (got < n - 1) {
		ssize_t r = read(fd, out + got, n - 1 - got);
		if (r <= 0)
			break;
		got += (size_t)r;
		if (memchr(out, '\n', got))
			break;
	}
	close(fd);
	out[got] = '\0';
	if (!got) {
		snprintf(errline, sizeof(errline), "the socket answered nothing");
		return -1;
	}
	return 0;
}

/* ── who owns a pid ────────────────────────────────────────────────────── */

static size_t proc_read(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "rb");
	size_t got;

	if (!f)
		return 0;
	got = fread(buf, 1, n - 1, f);
	fclose(f);
	buf[got] = '\0';
	return got;
}

/*
 * The box a pid belongs to, or nothing. conmon is podman's per-container
 * supervisor and carries the container name in its own argv; the process
 * inside the box is its descendant, so the walk goes up. Eight levels is
 * generous — an app in a box is two or three below conmon — and a bound is
 * what keeps a /proc that is lying to us from spinning here.
 */
static void box_of_pid(int pid, char *out, size_t n)
{
	kpr_box_of_pid(pid, out, n);
}

/* This session's own chrome, which must never be on the receiving end of `k`:
 * SIGTERMing the panel or the compositor from the window list is a monitor
 * that can take the desktop down by aiming one row wrong. */
static int is_chrome(int pid)
{
	static const char *const names[] = {
		"kdos-comp", "kdos-shell", "kdos-desk", "kdos-notifyd",
		"kdos-lock", "kdos-teams", NULL
	};
	char path[64], comm[64];
	size_t got;

	if (pid == (int)getpid() || pid == (int)getppid())
		return 1;
	snprintf(path, sizeof(path), "/proc/%d/comm", pid);
	got = proc_read(path, comm, sizeof(comm));
	if (!got)
		return 0;
	comm[strcspn(comm, "\n")] = '\0';
	for (int i = 0; names[i]; i++)
		if (!strcmp(comm, names[i]))
			return 1;
	return 0;
}

/* ── the list ──────────────────────────────────────────────────────────── */

static void refresh(void)
{
	static char reply[TEAMS_REPLY];
	struct jv v;
	const char *e, *arr;

	nviews = 0;
	errline[0] = '\0';
	sock_up = 0;
	if (cmd_call("{\"cmd\":\"list\"}\n", reply, sizeof(reply)) != 0)
		return;
	sock_up = 1;

	e = reply + strlen(reply);
	if (!j_member(reply, e, "ok", &v) || !j_bool(&v)) {
		if (j_member(reply, e, "err", &v))
			j_str(&v, errline, sizeof(errline));
		else
			snprintf(errline, sizeof(errline),
				 "the socket refused the list");
		return;
	}
	if (!j_member(reply, e, "views", &v)) {
		snprintf(errline, sizeof(errline), "no window list in the reply");
		return;
	}

	arr = j_first(v.s, e);
	for (; arr && nviews < TEAMS_MAX; arr = j_next(arr, e)) {
		struct tview *t = &views[nviews];
		const char *oe = j_skip(arr, e);

		if (!oe)
			break;
		memset(t, 0, sizeof(*t));
		t->id = j_member(arr, oe, "id", &v) ? j_int(&v, -1) : -1;
		t->pid = j_member(arr, oe, "pid", &v) ? (int)j_int(&v, -1) : -1;
		t->ws = j_member(arr, oe, "workspace", &v) ? (int)j_int(&v, 0) : 0;
		if (j_member(arr, oe, "app_id", &v))
			j_str(&v, t->app, sizeof(t->app));
		if (j_member(arr, oe, "title", &v))
			j_str(&v, t->title, sizeof(t->title));
		t->focused = j_member(arr, oe, "focused", &v) && j_bool(&v);
		t->minimized = j_member(arr, oe, "minimized", &v) && j_bool(&v);
		t->maximized = j_member(arr, oe, "maximized", &v) && j_bool(&v);
		t->fullscreen = j_member(arr, oe, "fullscreen", &v) && j_bool(&v);
		t->shaded = j_member(arr, oe, "shaded", &v) && j_bool(&v);
		if (t->id < 0)
			continue;
		if (t->pid > 1)
			box_of_pid(t->pid, t->box, sizeof(t->box));
		nviews++;
	}
}

static void run_action(const char *action, long id)
{
	char req[128], reply[1024];
	struct jv v;

	snprintf(req, sizeof(req), "{\"cmd\":\"run\",\"action\":\"%s\","
				   "\"id\":%ld}\n", action, id);
	if (cmd_call(req, reply, sizeof(reply)) != 0) {
		snprintf(status, sizeof(status), "%s", errline);
		return;
	}
	const char *e = reply + strlen(reply);
	if (j_member(reply, e, "ok", &v) && j_bool(&v)) {
		snprintf(status, sizeof(status), "%s sent to window %ld",
			 action, id);
		return;
	}
	if (j_member(reply, e, "err", &v)) {
		char why[80];
		j_str(&v, why, sizeof(why));
		snprintf(status, sizeof(status), "%s refused: %s", action, why);
	} else {
		snprintf(status, sizeof(status), "%s refused", action);
	}
}

static void kill_view(const struct tview *t)
{
	if (t->pid <= 1) {
		snprintf(status, sizeof(status),
			 "no pid for that window — Enter asks it to close");
		return;
	}
	if (is_chrome(t->pid)) {
		snprintf(status, sizeof(status),
			 "%d is this session's own chrome — refusing", t->pid);
		return;
	}
	if (kill(t->pid, SIGTERM) != 0)
		snprintf(status, sizeof(status), "SIGTERM %d: %s", t->pid,
			 strerror(errno));
	else
		snprintf(status, sizeof(status), "SIGTERM sent to %d", t->pid);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const char *state_word(const struct tview *t)
{
	if (t->minimized)
		return "min";
	if (t->fullscreen)
		return "full";
	if (t->shaded)
		return "shaded";
	if (t->maximized)
		return "max";
	return "";
}

static int list_rows(void)
{
	/* The rows run from y=2 to y=h-4: the box takes 0 and h-1, the header
	 * gap takes 1, the status line h-3 and the hint h-2. One row more and
	 * the last window is painted over by the status of the action that was
	 * just taken on it — and the mouse test below could never reach it. */
	int n = ktui_h - 5;
	return n < 1 ? 1 : n;
}

static void draw(int sel, int top)
{
	int w = ktui_w, h = ktui_h;
	int rowsv = list_rows();

	if (w < 24 || h < 6)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " teams ", KT_ACCENT, KT_SURFACE, 1);

	if (!nviews) {
		/* A desktop with nothing open is a healthy answer, and the whole
		 * point of sock_up is to be able to tell it from a socket that
		 * did not reply. Painting it in the error colour would make the
		 * monitor look broken on every freshly logged-in machine. */
		int failed = errline[0] != 0;

		ktui_draw_text(2, 2, w - 4,
			       failed ? errline : "no windows are open",
			       failed ? KT_ERR : KT_DIM, KT_SURFACE, KT_A_NONE);
		if (!sock_up)
			ktui_draw_text(2, 4, w - 4,
				       "kdos-comp serves "
				       "$XDG_RUNTIME_DIR/kdos-cmd.sock",
				       KT_DIM, KT_SURFACE, KT_A_NONE);
		/* Closing the LAST window lands here, and the answer to the
		 * action that emptied the list belongs on the screen with it. */
		if (status[0])
			ktui_draw_text(2, h - 3, w - 4, status, KT_WARN,
				       KT_SURFACE, KT_A_NONE);
		ktui_draw_text(2, h - 2, w - 4, "r retry   Esc close", KT_DIM,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_flush();
		return;
	}

	for (int i = 0; i < rowsv && top + i < nviews; i++) {
		const struct tview *t = &views[top + i];
		int y = 2 + i;
		int is_sel = top + i == sel;
		/* Swapped slots, never KT_A_REVERSE over the text: the
		 * attribute inverts only the cells the glyphs cover and the
		 * row comes out as one lit block per word. */
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : KT_SURFACE;
		char right[64], label[192];

		if (is_sel)
			ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		if (t->focused)
			ktui_draw_text(1, y, 1, ktui_glyph[KT_G_RIGHT],
				       is_sel ? fg : KT_ACCENT, bg, KT_A_NONE);

		/* The identity first, then what the window calls itself: an
		 * app_id is what the taskbar matches on and a title is what
		 * tells two windows of one app apart. */
		snprintf(label, sizeof(label), "%s%s%s",
			 t->app[0] ? t->app : "(no app_id)",
			 t->title[0] ? "  " : "", t->title);

		const char *st = state_word(t);
		if (t->box[0])
			snprintf(right, sizeof(right), "(appbox %s)  ws %d %s",
				 t->box, t->ws, st);
		else if (t->pid > 1)
			snprintf(right, sizeof(right), "%d  ws %d %s", t->pid,
				 t->ws, st);
		else
			snprintf(right, sizeof(right), "ws %d %s", t->ws, st);

		/* A window in no particular state leaves the word empty, and
		 * the space in front of it would push the whole column one
		 * cell left of every other row. */
		for (int k = (int)strlen(right); k > 0 && right[k - 1] == ' '; k--)
			right[k - 1] = '\0';

		/* Two cells of gap, computed from where the right column will
		 * start: a title long enough to reach it reads as one word
		 * with the pid, and it is the narrow screen that does it. */
		int rw = (int)strlen(right);
		ktui_draw_text(3, y, w - 6 - rw, label, fg, bg, KT_A_NONE);
		/* Inside the left border even when it has to truncate: x is
		 * where a too-long string starts, not where it ends. */
		ktui_draw_text_right(1, y, w - 3, right, is_sel ? fg : KT_MID,
				     bg, KT_A_NONE);
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE, on the frame's own right edge —
	 * see kch_scrollbar. It matters more since the wheel started
	 * moving the PAGE rather than the cursor: without it the content
	 * slides for no visible reason.
	 */
	kch_scrollbar(0, w - 1, 2, rowsv, nviews, top, KT_SURFACE);

	if (status[0])
		ktui_draw_text(2, h - 3, w - 4, status, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	ktui_draw_text(2, h - 2, w - 4,
		       "Enter close   k SIGTERM   r refresh   Esc quit",
		       KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_flush();
}

/*
 * `--dump-cells`: codepoint and colour SLOT per painted cell — a plain text
 * dump proves the layout and says nothing about the palette. It goes through a
 * backend because libktui's cell buffer is private and the backend vtable is
 * its documented seam; offscreen mode short-circuits the flush entirely.
 */
static int cap_w = TEAMS_COLS, cap_h = TEAMS_ROWS;

static void cap_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)prev;
	(void)ff;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const KtuiCell *c = &cur[y * w + x];
			if (!c->ch || c->ch == ' ' || c->ch == KTUI_WIDE_CONT)
				continue;
			printf("%d %d U+%04X %d %d %d\n", y, x, c->ch, c->fg,
			       c->bg, c->attr);
		}
}

static int cap_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static void cap_size(int *w, int *h)
{
	*w = cap_w;
	*h = cap_h;
}

static int cap_caps(void)
{
	return KT_CAP_UTF8 | KT_CAP_TRUECOLOR;
}

static const KtuiBackend cap_backend = {
	"dump-cells", cap_flush, cap_poll, cap_size, cap_caps
};

/* ── main ──────────────────────────────────────────────────────────────── */

int teams_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, dump_cells = 0;
	int at_x = -1, at_y = 0, at_bottom = 0;

	for (int i = 1; i < argc; i++) {
		/*
		 * ANCHORED WHEN THE PANEL OPENS IT. The window list is what
		 * the taskbar's `+N` cell now leads to — the overflow used to
		 * step one window per click, which on a bar with four hidden
		 * windows is a guessing game — and a popup belonging to a bar
		 * has to grow out of that bar. Typed by name it is still a
		 * centred window: the same one flag decides both, exactly as
		 * it does for the device managers.
		 */
		if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			dump_cells = 1;
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 23 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		} else {
			fprintf(stderr, "usage: kdos-teams "
					"[--at X Y] [--at-bottom X Y] "
					"[--dump|--dump-cells] [--dump-size WxH] "
					"[--font NAME]\n");
			return 2;
		}
	}

	if (dump || dump_cells) {
		sh_theme_from_cache();
		refresh();
		if (dump_cells) {
			ktui_backend_set(&cap_backend);
			ktui_draw_init();
			draw(0, 0);
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw(0, 0);
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = TEAMS_COLS,
		.rows = TEAMS_ROWS,
		.corner = at_x < 0	? KWL_CORNER_CENTER
			  : at_bottom	? KWL_CORNER_BOTTOM_LEFT
					: KWL_CORNER_TOP_LEFT,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-teams",
		.font = font,
		.keyboard = 1,
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-teams: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	refresh();

	int sel = 0, top = 0;
	/* Set by everything that moves the CURSOR, cleared by everything that
	 * moves the PAGE — see kch_list_clamp. */
	int sel_follow = 1;

	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int rowsv = list_rows();

		if (sel >= nviews)
			sel = nviews ? nviews - 1 : 0;
		if (sel < 0)
			sel = 0;
		/* The viewport follows the SELECTION only when the selection is
		 * what moved — the rule kch_list_clamp exists to state once. A
		 * hand-rolled pull here undid every page scroll on the frame
		 * after it. */
		kch_list_clamp(&top, sel, nviews, rowsv, sel_follow);

		draw(sel, top);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 2 + top;
			int on_row = ev.my >= 2 && ev.my < ktui_h - 3 &&
				     row >= 0 && row < nviews;
			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar.
				 * A drag is a press that is still down, and
				 * Wayland says nothing about that, so the
				 * grab is what remembers it. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				if (on_row) {
					sel = row;
					sel_follow = 1;
				}
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt = kch_scrollbar_press(0, ev.mx,
							     ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel--;
				sel_follow = 1;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel++;
				sel_follow = 1;
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT || !on_row)
				continue;
			/* A click on the row that is already selected activates
			 * it — pick.c's rule, so one hand learns one thing. It
			 * matters more here than anywhere: motion selects, so a
			 * plain first click would close whatever window the
			 * pointer happened to be crossing. */
			if (row == sel) {
				run_action("Close", views[row].id);
				refresh();
			}
			sel = row;
			sel_follow = 1;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		sel_follow = 1;	/* a key moves the cursor; the view follows */

		switch (ev.key) {
		case KT_K_ESC:
		case 'q':
			goto done;
		case KT_K_ENTER:
			/* The protocol's polite close: an editor still gets to
			 * ask about unsaved work. `k` is the other answer. */
			if (sel < nviews) {
				run_action("Close", views[sel].id);
				refresh();
			}
			break;
		case 'k':
			if (sel < nviews)
				kill_view(&views[sel]);
			break;
		case 'r':
			status[0] = '\0';
			refresh();
			break;
		case KT_K_UP:
			sel--;
			break;
		case KT_K_DOWN:
			sel++;
			break;
		case KT_K_PGUP:
			sel -= rowsv;
			break;
		case KT_K_PGDN:
			sel += rowsv;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
			sel = nviews - 1;
			break;
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	return 0;
}
