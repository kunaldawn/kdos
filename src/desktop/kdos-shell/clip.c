/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-clip — the clipboard, remembered
 *
 *   ┌ clipboard ───────────────────────────────────┐
 *   │▸ 1  ssh kdos@192.168.1.40                    │
 *   │  2  https://wayland.freedesktop.org/docs/    │
 *   │  3  /media/kdos/KDOSSTICK/notes.txt          │
 *   ├──────────────────────────────────────────────┤
 *   │ Enter put it back   d forget   c clear   Esc │
 *   └──────────────────────────────────────────────┘
 *
 * THE PROTOCOL IS THE WHOLE DESIGN. A clipboard manager cannot use
 * `wl_data_device`: that delivers a selection event only to the client with
 * KEYBOARD FOCUS, so a history built on it would record exactly the copies you
 * made while the history window was open — which is none of them.
 * `zwlr_data_control_manager_v1` exists for this and this alone, and labwc
 * carries both generations of it. It is also on the compositor's
 * sandbox DENY list, so a boxed application cannot be a clipboard manager,
 * which is the correct half of the same decision.
 *
 * NOTHING IS WRITTEN TO DISK. The history is a clipboard's history: every
 * password anybody copied is in it. It lives in this process and in a socket
 * under $XDG_RUNTIME_DIR, which is a tmpfs that does not survive a reboot,
 * and there is no `persist` option to turn on later — if one is ever wanted it
 * should have to be argued for, not defaulted away from.
 *
 * TEXT ONLY. An image on the clipboard is megabytes per copy and a history of
 * them is a memory leak with a UI; the offer is taken only when it carries a
 * text mime type, and a copied image passes through untouched because the
 * SOURCE application still owns it.
 *
 * TWO PROGRAMS IN ONE. `kdos-clip` is the daemon the compositor supervises;
 * `kdos-clip --pick` is the picker a keybind opens, and it talks to the daemon
 * over the socket. The daemon never draws and the picker never touches the
 * protocol — which is what lets the picker be an ordinary libkwl overlay while
 * the daemon is a hundred lines of wayland-client with no surface at all.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-data-control-unstable-v1-client-protocol.h"

#include "kwl.h"
#include "shell.h"

#define CL_MAX 32		/* entries kept                            */
#define CL_TEXT_MAX (64 * 1024)	/* per entry                               */
#define CL_PREVIEW 96
#define CL_COLS 68
#define CL_ROWS 18

struct cl_entry {
	char *text;
	size_t len;
	char preview[CL_PREVIEW];
};

/* ── the daemon ────────────────────────────────────────────────────────── */

static struct cl_entry hist[CL_MAX];
static int nhist;

static struct wl_display *dpy;
static struct wl_seat *seat;
static struct zwlr_data_control_manager_v1 *dcm;
static struct zwlr_data_control_device_v1 *dcd;
static struct zwlr_data_control_source_v1 *own_src;
/* What we are offering, so the `send` handler has something to write. */
static char *own_text;
static size_t own_len;

/* The mime types worth taking, best first. */
static const char *const TEXT_MIME[] = {
	"text/plain;charset=utf-8", "UTF8_STRING", "text/plain", "STRING",
	"TEXT",
};
#define NMIME ((int)(sizeof(TEXT_MIME) / sizeof(TEXT_MIME[0])))

/*
 * One line of what an entry says, for the picker.
 *
 * Every run of whitespace becomes ONE space and control bytes are dropped: a
 * copied shell command with a newline in it must not be able to draw a second
 * row, and a copied terminal escape must not be able to draw anything at all.
 */
static void make_preview(struct cl_entry *e)
{
	size_t k = 0;
	int space = 0;

	for (size_t i = 0; i < e->len && k + 1 < sizeof(e->preview); i++) {
		unsigned char c = (unsigned char)e->text[i];
		if (c == '\n' || c == '\t' || c == '\r' || c == ' ') {
			if (!k || space)
				continue;
			space = 1;
			e->preview[k++] = ' ';
			continue;
		}
		if (c < 0x20 || c == 0x7f)
			continue;
		space = 0;
		e->preview[k++] = (char)c;
	}
	while (k && e->preview[k - 1] == ' ')
		k--;
	e->preview[k] = '\0';
	if (!k)
		snprintf(e->preview, sizeof(e->preview), "(%zu bytes)", e->len);
}

static void hist_push(char *text, size_t len)
{
	if (!len) {
		free(text);
		return;
	}
	/* Already at the top: a selection event arrives whenever anything
	 * re-advertises, including our own `set`, and a history that grew a
	 * row each time would fill with one string. */
	if (nhist && hist[0].len == len && !memcmp(hist[0].text, text, len)) {
		free(text);
		return;
	}
	/* Somewhere below the top: promote rather than duplicate. */
	for (int i = 1; i < nhist; i++) {
		if (hist[i].len == len && !memcmp(hist[i].text, text, len)) {
			struct cl_entry tmp = hist[i];
			memmove(&hist[1], &hist[0],
				(size_t)i * sizeof(hist[0]));
			hist[0] = tmp;
			free(text);
			return;
		}
	}
	if (nhist == CL_MAX)
		free(hist[--nhist].text);
	memmove(&hist[1], &hist[0], (size_t)nhist * sizeof(hist[0]));
	nhist++;
	hist[0].text = text;
	hist[0].len = len;
	make_preview(&hist[0]);
}

/*
 * Read an offer to the end, with a deadline.
 *
 * A source that opens the pipe and never writes would otherwise hold this
 * process for the rest of the session; two seconds is far more than any local
 * client needs and is bounded, which is the property that matters.
 */
static void take_offer(struct zwlr_data_control_offer_v1 *offer,
		       const char *mime)
{
	int fds[2];
	char *buf;
	size_t len = 0;

	if (pipe2(fds, O_CLOEXEC) < 0)
		return;
	zwlr_data_control_offer_v1_receive(offer, mime, fds[1]);
	close(fds[1]);
	wl_display_flush(dpy);

	buf = malloc(CL_TEXT_MAX);
	if (!buf) {
		close(fds[0]);
		return;
	}
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	for (int spins = 0; spins < 200 && len < CL_TEXT_MAX;) {
		ssize_t r = read(fds[0], buf + len, CL_TEXT_MAX - len);
		if (r > 0) {
			len += (size_t)r;
			continue;
		}
		if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			struct pollfd p = { .fd = fds[0], .events = POLLIN };
			poll(&p, 1, 10);
			spins++;
			continue;
		}
		break;			/* EOF or error: done */
	}
	close(fds[0]);
	hist_push(buf, len);
}

/* ── the offer ─────────────────────────────────────────────────────────── */

/* The best text mime an offer carries, as an index into TEXT_MIME, or -1.
 * Kept in the proxy's user data — the same trick libkwl's paste path uses,
 * because an offer has no other place to hang state. */
static void offer_mime(void *d, struct zwlr_data_control_offer_v1 *o,
		       const char *mime)
{
	(void)d;
	long rank = (long)(intptr_t)wl_proxy_get_user_data(
		(struct wl_proxy *)o);

	for (int i = 0; i < NMIME; i++)
		if (!strcmp(mime, TEXT_MIME[i])) {
			/* Stored as index+1 so that zero still means "nothing
			 * textual", and only ever IMPROVED. */
			if (!rank || i + 1 < rank)
				wl_proxy_set_user_data(
					(struct wl_proxy *)o,
					(void *)(intptr_t)(i + 1));
			return;
		}
}

static const struct zwlr_data_control_offer_v1_listener offer_listener = {
	.offer = offer_mime,
};

static void dev_data_offer(void *d, struct zwlr_data_control_device_v1 *dev,
			   struct zwlr_data_control_offer_v1 *o)
{
	(void)d;
	(void)dev;
	wl_proxy_set_user_data((struct wl_proxy *)o, (void *)(intptr_t)0);
	zwlr_data_control_offer_v1_add_listener(o, &offer_listener, NULL);
}

static void dev_selection(void *d, struct zwlr_data_control_device_v1 *dev,
			  struct zwlr_data_control_offer_v1 *o)
{
	(void)d;
	(void)dev;
	if (!o)
		return;			/* the selection was cleared */
	long rank = (long)(intptr_t)wl_proxy_get_user_data(
		(struct wl_proxy *)o);
	if (rank > 0)
		take_offer(o, TEXT_MIME[rank - 1]);
	zwlr_data_control_offer_v1_destroy(o);
}

/*
 * The PRIMARY selection is deliberately not recorded.
 *
 * On this desktop primary is "whatever text the pointer last dragged over",
 * which changes constantly and is not a decision anybody made. A history of it
 * is noise, and worse, it would record every password field a mouse happened
 * to sweep. The offer is destroyed so the compositor is not left holding it.
 */
static void dev_primary(void *d, struct zwlr_data_control_device_v1 *dev,
			struct zwlr_data_control_offer_v1 *o)
{
	(void)d;
	(void)dev;
	if (o)
		zwlr_data_control_offer_v1_destroy(o);
}

static void dev_finished(void *d, struct zwlr_data_control_device_v1 *dev)
{
	(void)d;
	(void)dev;
	/* The compositor took the device away — a reconfigure, or a shutdown.
	 * Exiting is right: the supervisor restarts us and we bind a new one. */
	exit(0);
}

static const struct zwlr_data_control_device_v1_listener device_listener = {
	.data_offer = dev_data_offer,
	.selection = dev_selection,
	.finished = dev_finished,
	.primary_selection = dev_primary,
};

/* ── putting one back ──────────────────────────────────────────────────── */

static void src_send(void *d, struct zwlr_data_control_source_v1 *src,
		     const char *mime, int32_t fd)
{
	(void)d;
	(void)src;
	(void)mime;
	if (own_text && own_len) {
		/* Small and local: a clipboard entry is 64 KB at the very most
		 * and the pipe buffer takes it in one or two writes. The fd is
		 * left blocking on purpose — this is the one place where a
		 * short pause is better than a truncated paste. */
		size_t off = 0;
		while (off < own_len) {
			ssize_t w = write(fd, own_text + off, own_len - off);
			if (w <= 0)
				break;
			off += (size_t)w;
		}
	}
	close(fd);
}

static void src_cancelled(void *d, struct zwlr_data_control_source_v1 *src)
{
	(void)d;
	if (src == own_src) {
		zwlr_data_control_source_v1_destroy(own_src);
		own_src = NULL;
	}
}

static const struct zwlr_data_control_source_v1_listener source_listener = {
	.send = src_send,
	.cancelled = src_cancelled,
};

static void offer_entry(int i)
{
	if (i < 0 || i >= nhist || !dcm || !dcd)
		return;
	free(own_text);
	own_text = malloc(hist[i].len);
	if (!own_text)
		return;
	memcpy(own_text, hist[i].text, hist[i].len);
	own_len = hist[i].len;

	if (own_src)
		zwlr_data_control_source_v1_destroy(own_src);
	own_src = zwlr_data_control_manager_v1_create_data_source(dcm);
	if (!own_src)
		return;
	zwlr_data_control_source_v1_add_listener(own_src, &source_listener,
						 NULL);
	for (int k = 0; k < NMIME; k++)
		zwlr_data_control_source_v1_offer(own_src, TEXT_MIME[k]);
	zwlr_data_control_device_v1_set_selection(dcd, own_src);
	wl_display_flush(dpy);
	/* And promote it, so picking the third entry twice does not walk it
	 * back down the list. */
	hist_push(strndup(hist[i].text, hist[i].len), hist[i].len);
}

/* ── the registry ──────────────────────────────────────────────────────── */

static void reg_global(void *d, struct wl_registry *r, uint32_t name,
		       const char *iface, uint32_t ver)
{
	(void)d;
	if (!strcmp(iface, wl_seat_interface.name) && !seat) {
		seat = wl_registry_bind(r, name, &wl_seat_interface,
					ver < 5 ? ver : 5);
	} else if (!strcmp(iface,
			   zwlr_data_control_manager_v1_interface.name)) {
		/* Version 2 for the primary-selection event; a compositor at
		 * version 1 simply never sends it, which is fine because this
		 * ignores it anyway. */
		dcm = wl_registry_bind(r, name,
				       &zwlr_data_control_manager_v1_interface,
				       ver < 2 ? ver : 2);
	}
}

static void reg_remove(void *d, struct wl_registry *r, uint32_t name)
{
	(void)d;
	(void)r;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = reg_global,
	.global_remove = reg_remove,
};

/* ── the socket ────────────────────────────────────────────────────────── */

/*
 * A PATH THAT DOES NOT FIT `sun_path` IS REFUSED, NOT TRUNCATED — kdos-packd's
 * rule, and this is the only place that builds the name so it is the only
 * place that has to keep it. The bound is `sun_path`'s 108 bytes and NOT the
 * caller's buffer: bounding against the buffer accepts a 200-byte path that
 * both `bind` and `connect` then quietly cut down, which binds a socket nobody
 * asked for and makes two different XDG_RUNTIME_DIRs collide on one name.
 */
static int sock_path(char *out, size_t n)
{
	const char *run = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un probe;
	int len;

	if (!run || !*run)
		return -1;
	len = snprintf(out, n, "%s/kdos-clip.sock", run);
	if (len < 0 || (size_t)len >= n)
		return -1;
	if ((size_t)len >= sizeof(probe.sun_path)) {
		fprintf(stderr, "kdos-clip: socket path is longer than %zu "
				"bytes: %s\n", sizeof(probe.sun_path) - 1, out);
		return -1;
	}
	return 0;
}

static void serve_client(int c)
{
	char buf[64] = {0};
	ssize_t n = read(c, buf, sizeof(buf) - 1);

	if (n <= 0)
		return;
	buf[n] = '\0';
	buf[strcspn(buf, "\r\n")] = '\0';

	if (!strcmp(buf, "list")) {
		for (int i = 0; i < nhist; i++)
			dprintf(c, "%d\t%zu\t%s\n", i, hist[i].len,
				hist[i].preview);
		(void)!write(c, "ok\n", 3);
	} else if (!strcmp(buf, "count")) {
		dprintf(c, "%d\n", nhist);
	} else if (!strncmp(buf, "get ", 4)) {
		int i = atoi(buf + 4);
		if (i >= 0 && i < nhist)
			(void)!write(c, hist[i].text, hist[i].len);
	} else if (!strncmp(buf, "set ", 4)) {
		offer_entry(atoi(buf + 4));
		(void)!write(c, "ok\n", 3);
	} else if (!strncmp(buf, "forget ", 7)) {
		int i = atoi(buf + 7);
		if (i >= 0 && i < nhist) {
			free(hist[i].text);
			memmove(&hist[i], &hist[i + 1],
				(size_t)(nhist - i - 1) * sizeof(hist[0]));
			nhist--;
		}
		(void)!write(c, "ok\n", 3);
	} else if (!strcmp(buf, "clear")) {
		for (int i = 0; i < nhist; i++)
			free(hist[i].text);
		nhist = 0;
		(void)!write(c, "ok\n", 3);
	} else {
		(void)!write(c, "err unknown command\n", 20);
	}
}

static int daemon_main(void)
{
	char path[SH_SOCK_MAX];
	int srv;

	dpy = wl_display_connect(NULL);
	if (!dpy) {
		fprintf(stderr, "kdos-clip: no compositor\n");
		return 1;
	}
	struct wl_registry *reg = wl_display_get_registry(dpy);
	wl_registry_add_listener(reg, &registry_listener, NULL);
	wl_display_roundtrip(dpy);

	if (!dcm || !seat) {
		/* Said out loud: without data-control this program can watch
		 * nothing, and a clipboard manager that silently records
		 * nothing is worse than one that is not running. */
		fprintf(stderr, "kdos-clip: the compositor has no "
				"zwlr_data_control_manager_v1 — no history\n");
		return 1;
	}
	dcd = zwlr_data_control_manager_v1_get_data_device(dcm, seat);
	zwlr_data_control_device_v1_add_listener(dcd, &device_listener, NULL);
	wl_display_roundtrip(dpy);

	if (sock_path(path, sizeof(path)) != 0) {
		fprintf(stderr, "kdos-clip: no XDG_RUNTIME_DIR\n");
		return 1;
	}
	srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0)
		return 1;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-clip: bind %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	/* 0600, and no credential check: $XDG_RUNTIME_DIR is already 0700 and
	 * this socket hands out a clipboard history, which is exactly as
	 * private as the session it belongs to and no more. The appbox shares
	 * that directory and runs as the same user — a boxed application can
	 * read this, and it could read the clipboard through the compositor
	 * too if it were not sandboxed, so the boundary is the box's, not
	 * this socket's. */
	chmod(path, 0600);
	listen(srv, 4);

	signal(SIGPIPE, SIG_IGN);
	for (;;) {
		struct pollfd p[2] = {
			{ .fd = wl_display_get_fd(dpy), .events = POLLIN },
			{ .fd = srv, .events = POLLIN },
		};
		/* The documented non-blocking read sequence, the same one
		 * kwl_pump uses: prepare, poll, read — anything else spins at
		 * 100% the moment the compositor sends something nobody
		 * dispatched. */
		while (wl_display_prepare_read(dpy) != 0)
			wl_display_dispatch_pending(dpy);
		wl_display_flush(dpy);
		if (poll(p, 2, -1) < 0) {
			wl_display_cancel_read(dpy);
			if (errno == EINTR)
				continue;
			break;
		}
		if (p[0].revents & POLLIN) {
			if (wl_display_read_events(dpy) < 0)
				break;
		} else {
			wl_display_cancel_read(dpy);
		}
		if (wl_display_dispatch_pending(dpy) < 0)
			break;
		if (p[1].revents & POLLIN) {
			int c = accept(srv, NULL, NULL);
			if (c >= 0) {
				serve_client(c);
				close(c);
			}
		}
	}
	unlink(path);
	return 1;
}

/* ── the picker ────────────────────────────────────────────────────────── */

struct pick_row {
	int idx;
	size_t len;
	char preview[CL_PREVIEW];
};

static struct pick_row rows[CL_MAX];
static int nrows;
/* The viewport follows the SELECTION only when the selection is what moved —
 * see kch_list_wheel. A clamp that followed unconditionally would undo a page
 * scroll on the very next frame. */
static int sel, top, sel_follow = 1;
static char why[128];

/* One request, one answer. The daemon is in the same session and answers in
 * microseconds; a picker that is up for as long as somebody is reading it can
 * afford a blocking round trip. */
static int ask(const char *req, char *out, size_t n)
{
	char path[SH_SOCK_MAX];
	int fd;
	size_t got = 0;
	ssize_t r;

	if (out)
		out[0] = '\0';
	if (sock_path(path, sizeof(path)) != 0)
		return -1;
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", req);
	while (out && got + 1 < n && (r = read(fd, out + got, n - got - 1)) > 0)
		got += (size_t)r;
	if (out)
		out[got] = '\0';
	close(fd);
	return 0;
}

static void load(void)
{
	char buf[16384];

	nrows = 0;
	why[0] = '\0';
	if (ask("list", buf, sizeof(buf)) != 0) {
		snprintf(why, sizeof(why),
			 "kdos-clip is not running — nothing is being kept");
		return;
	}
	for (char *p = buf; *p && nrows < CL_MAX;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		struct pick_row *r = &rows[nrows];
		unsigned long len = 0;
		if (sscanf(p, "%d\t%lu\t%95[^\n]", &r->idx, &len,
			   r->preview) == 3) {
			r->len = len;
			nrows++;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	int body = h - 4;

	if (w < 24 || h < 6)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), "clipboard", KT_ACCENT, KT_BG, 1);

	if (why[0])
		ktui_draw_text(2, 2, w - 4, why, KT_ERR, KT_BG, KT_A_NONE);
	else if (!nrows)
		ktui_draw_text(2, 2, w - 4, "nothing has been copied yet",
			       KT_MID, KT_BG, KT_A_NONE);

	kch_list_clamp(&top, sel, nrows, body, sel_follow);
	sel_follow = 0;
	if (top < 0)
		top = 0;

	for (int i = 0; i < body && top + i < nrows; i++) {
		const struct pick_row *r = &rows[top + i];
		int y = 1 + i;
		int on = top + i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		ktui_draw_textf(2, y, 4, on ? KT_SURFACE : KT_DIM, bg,
				KT_A_NONE, "%d", r->idx + 1);
		ktui_draw_text(6, y, w - 8, r->preview, fg, bg, KT_A_NONE);
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE, on the frame's own right edge —
	 * see kch_scrollbar. It matters more since the wheel started
	 * moving the PAGE rather than the cursor: without it the content
	 * slides for no visible reason.
	 */
	kch_scrollbar(0, w - 1, 1, body, nrows, top, KT_BG);

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	ktui_draw_text(2, h - 2, w - 4,
		       "Enter put it back   d forget   c clear   Esc",
		       KT_MID, KT_BG, KT_A_NONE);
	ktui_draw_flush();
}

/* Named for what it is rather than `pick_main`, which is kdos-pick's. */
static int show_picker(const char *font, int at_x, int at_y, int dump)
{
	load();

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(CL_COLS, CL_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = CL_COLS,
		.rows = CL_ROWS,
		.corner = at_x >= 0 ? KWL_CORNER_BOTTOM_LEFT : KWL_CORNER_CENTER,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-clip",
		.font = font,
		.keyboard = 1,
		/* A menu: clicking elsewhere closes it, because the thing you
		 * want to paste into is elsewhere. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-clip: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_BG);

	while (!kwl_should_close()) {
		sh_theme_poll();
		draw_frame();

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
			int idx = top + ev.my - 1;
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
				if (ev.my >= 1 && idx >= 0 && idx < nrows) {
					sel = idx;
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
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;
				int body = ktui_h - 2 > 0 ? ktui_h - 2 : 1;
				if (!kch_list_wheel(up, &top, nrows, body)) {
					if (up ? sel > 0 : sel + 1 < nrows)
						sel += up ? -1 : 1;
					sel_follow = 1;
				}
			} else if (ev.btn == KT_MB_RIGHT)
				break;
			else if (ev.btn == KT_MB_LEFT && idx >= 0 &&
				 idx < nrows) {
				char req[32];
				snprintf(req, sizeof(req), "set %d",
					 rows[idx].idx);
				ask(req, NULL, 0);
				break;
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		sel_follow = 1;
		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nrows)
				sel++;
			break;
		case KT_K_ENTER: {
			char req[32];
			if (sel < nrows) {
				snprintf(req, sizeof(req), "set %d",
					 rows[sel].idx);
				ask(req, NULL, 0);
			}
			goto done;
		}
		case 'd': {
			char req[32];
			if (sel < nrows) {
				snprintf(req, sizeof(req), "forget %d",
					 rows[sel].idx);
				ask(req, NULL, 0);
				load();
				if (sel >= nrows)
					sel = nrows ? nrows - 1 : 0;
			}
			break;
		}
		case 'c':
			ask("clear", NULL, 0);
			load();
			sel = top = 0;
			break;
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	return 0;
}

int clip_main(int argc, char **argv)
{
	const char *font = NULL;
	int pick = 0, dump = 0, at_x = -1, at_y = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--pick"))
			pick = 1;
		else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
			pick = 1;
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[i + 1]);
			at_y = atoi(argv[i + 2]);
			i += 2;
		} else {
			fprintf(stderr,
				"usage: kdos-clip                 (the daemon)\n"
				"       kdos-clip --pick [--at-bottom X Y]\n"
				"       kdos-clip --dump\n");
			return 2;
		}
	}
	if (pick)
		return show_picker(font, at_x, at_y, dump);
	return daemon_main();
}
