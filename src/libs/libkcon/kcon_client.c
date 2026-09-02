/* libkcon — the client half: a KDispImpl and a KtuiBackend over a socket.
 * See kcon.h.
 *
 * Everything above this is the toolkit, unchanged. A surface that ran under a
 * compositor runs on the console because this file answers the same questions
 * libkwl does, over a socket instead of a Wayland connection.
 */

#include <errno.h>
#include <poll.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdio.h>

#include "kcon.h"

static struct {
	KconConn *conn;
	int cols, rows;
	int configured;
	int should_close;

	/*
	 * WHAT THE ATTACH SAID, kept so a resize can repeat it. A re-attach is
	 * how an overlay asks for a different size here — there is no separate
	 * resize message, because the session already reads a size out of an
	 * attach and a second op would be a second place for the two to
	 * disagree.
	 */
	int role, edge, cells, exclusive;
	char app_id[128], title[256], output[64];

	/*
	 * WHICH PICTURE WAS LAST SENT FOR EACH SLOT, as the pointer the sprite
	 * table holds. A flag saying "sent" would be wrong for an animation:
	 * every frame registers new pixels under the SAME key and therefore in
	 * the same slot, without touching a single cell — so the cells never
	 * change, the diff finds nothing, and a display would hold the first
	 * frame for ever.
	 */
	const void *sent_pix[KTUI_MAX_SPRITES];
	KconSpriteBits bits_fn;
	void *bits_user;

	/* Events decoded but not yet handed up. The toolkit asks for one at a
	 * time and a single read can carry many. */
	KtuiEvent q[64];
	int qhead, qtail;
} C;

static int connect_to(const char *path)
{
	struct sockaddr_un sa;

	if (!path || !*path || strlen(path) >= sizeof(sa.sun_path))
		return -1;

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
		return -1;

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, strlen(path));

	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static int kcon_probe(void)
{
	const char *p = getenv("KDOS_CON");

	/* Cheap and without side effects: kdisp_init probes implementations it
	 * will not go on to use, so this must not connect. */
	return p && *p;
}

static void push(const KtuiEvent *ev)
{
	int next = (C.qtail + 1) % (int)(sizeof(C.q) / sizeof(C.q[0]));

	/* A full queue drops the OLDEST: a surface that stopped draining is
	 * better shown the newest state than the state it fell behind at. */
	if (next == C.qhead)
		C.qhead = (C.qhead + 1) % (int)(sizeof(C.q) / sizeof(C.q[0]));
	C.q[C.qtail] = *ev;
	C.qtail = next;
}

static int pop(KtuiEvent *ev)
{
	if (C.qhead == C.qtail)
		return 0;
	*ev = C.q[C.qhead];
	C.qhead = (C.qhead + 1) % (int)(sizeof(C.q) / sizeof(C.q[0]));
	return 1;
}

static void handle(const KconMsg *m)
{
	KconRd r;
	KtuiEvent ev;

	kcon_rd_init(&r, m->payload, m->len);
	memset(&ev, 0, sizeof(ev));

	switch (m->op) {
	case KCON_OP_CONFIGURE: {
		int cols = (int)kcon_get_u16(&r);
		int rows = (int)kcon_get_u16(&r);

		if (r.err || cols <= 0 || rows <= 0)
			return;
		if (cols == C.cols && rows == C.rows)
			return;
		C.cols = cols;
		C.rows = rows;
		C.configured = 1;
		/*
		 * THE FLAG AS WELL AS THE EVENT. A loop that drains events acts
		 * on the event; a loop that does not — every overlay in this
		 * tree — watches the flag, and a backend that set only one of
		 * the two leaves half its consumers drawing at the old size for
		 * ever.
		 */
		ktui_resized = 1;
		ev.type = KT_EVT_RESIZE;
		push(&ev);
		break;
	}
	case KCON_OP_KEY:
		ev.type = KT_EVT_KEY;
		ev.key = kcon_get_i32(&r);
		ev.mods = (int)kcon_get_u8(&r);
		if (!r.err)
			push(&ev);
		break;
	case KCON_OP_PTR:
		ev.type = KT_EVT_MOUSE;
		ev.mx = (int)kcon_get_i32(&r);
		ev.my = (int)kcon_get_i32(&r);
		ev.btn = (int)kcon_get_u8(&r);
		ev.press = (int)kcon_get_u8(&r);
		if (!r.err)
			push(&ev);
		break;
	case KCON_OP_TOUCH:
		ev.type = KT_EVT_TOUCH;
		ev.mx = (int)kcon_get_i32(&r);
		ev.my = (int)kcon_get_i32(&r);
		ev.slot = (int)kcon_get_u8(&r);
		ev.phase = (int)kcon_get_u8(&r);
		ev.ms = kcon_get_u32(&r);
		ev.gesture = (int)kcon_get_u8(&r);
		if (!r.err)
			push(&ev);
		break;
	case KCON_OP_CLIP_DATA: {
		const char *t = kcon_get_str(&r);

		if (!r.err && *t)
			ktui_paste_push(t, strlen(t));
		break;
	}
	case KCON_OP_DRAG_DROP: {
		ev.type = KT_EVT_DROP;
		ev.mx = (int)kcon_get_i32(&r);
		ev.my = (int)kcon_get_i32(&r);
		const char *t = kcon_get_str(&r);

		if (r.err)
			break;
		ktui_drop_push(t, strlen(t));
		push(&ev);
		break;
	}
	case KCON_OP_SPRITE_RESEND:
		/*
		 * A DISPLAY ATTACHED AND HAS NONE OF THEM. Forgetting what was
		 * sent is the whole of it: the next flush walks the table, sees
		 * every picture as new, and sends them all before any cell.
		 */
		memset(C.sent_pix, 0, sizeof(C.sent_pix));
		ktui_draw_invalidate();
		break;
	case KCON_OP_CLOSE:
	case KCON_OP_BYE:
		C.should_close = 1;
		break;
	default:
		/* An opcode from a newer server is ignored, not fatal: the
		 * version handshake already refused a peer we cannot talk to,
		 * and within a version an unknown message is an addition. */
		break;
	}
}

/* ── the toolkit's backend ───────────────────────────────────────────── */

/*
 * The diff is HERE rather than handed over as a damage list, for the reason
 * ktui.h gives: a backend decides for itself what changed. A run is a row's
 * worth of contiguous differing cells, which is the shape the wire wants and
 * the shape a redraw actually has.
 */
/*
 * A picture whose pixels the display has not got. Sent BEFORE any cells, so a
 * cell referencing a slot is never ahead of the picture behind it.
 */
static void send_sprite(int slot, const KtuiSprite *sp)
{
	KconBuf sb = { 0 };
	const uint32_t *argb = NULL;
	int pw = 0, ph = 0, stride = 0;

	kcon_put_u16(&sb, (uint16_t)slot);
	kcon_put_u16(&sb, (uint16_t)sp->w);
	kcon_put_u16(&sb, (uint16_t)sp->h);
	kcon_put_u32(&sb, sp->fallback);

	/*
	 * THE PIXELS, if this consumer has a pixel library and said so.
	 *
	 * Row by row rather than as one block: the source stride is the image's
	 * and need not equal its width, and sending the padding would put
	 * whatever is in it on the wire.
	 */
	if (C.bits_fn &&
	    C.bits_fn(sp->pix, &argb, &pw, &ph, &stride, C.bits_user) == 0 &&
	    argb && pw > 0 && ph > 0) {
		kcon_put_u16(&sb, (uint16_t)pw);
		kcon_put_u16(&sb, (uint16_t)ph);
		/* RAW ROWS, no length before any of them. The reader knows the
		 * size from pw and ph, and a length per row would put four
		 * bytes between every row of every picture. */
		for (int ry = 0; ry < ph; ry++)
			kcon_put_bytes(&sb, argb + (size_t)ry * stride,
				       (size_t)pw * 4);
	} else {
		kcon_put_u16(&sb, 0);
		kcon_put_u16(&sb, 0);
	}

	kcon_send(C.conn, KCON_OP_SPRITE, &sb);
	kcon_buf_free(&sb);
}

static void cl_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		     int force_full)
{
	if (!C.conn || kcon_conn_dead(C.conn))
		return;

	KconBuf buf = { 0 };

	/*
	 * EVERY PICTURE ON THE SCREEN WHOSE PIXELS CHANGED, whether or not a
	 * cell did — which is what makes an animation arrive at all, because an
	 * animation changes pixels and never cells.
	 *
	 * ON THE SCREEN, not everything the table holds: a program's icons are
	 * sprites too, and sending the ones nothing is drawing would put the
	 * whole icon set on the wire the first time anything is flushed. One
	 * pass over the grid collects what is referenced, which costs what the
	 * diff below already costs.
	 */
	static unsigned char used[KTUI_MAX_SPRITES];

	memset(used, 0, sizeof(used));
	for (long i = 0, n = (long)w * h; i < n; i++) {
		uint32_t ch = cur[i].ch;

		if (KTUI_IS_SPRITE(ch)) {
			unsigned slot = KTUI_SPRITE_SLOT(ch);

			if (slot < KTUI_MAX_SPRITES)
				used[slot] = 1;
		}
	}

	for (int slot = 0; slot < ktui_sprite_slots(); slot++) {
		const KtuiSprite *sp = ktui_sprite_get(slot);

		if (!sp) {
			C.sent_pix[slot] = NULL;
			continue;
		}
		if (!used[slot] || sp->pix == C.sent_pix[slot])
			continue;
		send_sprite(slot, sp);
		C.sent_pix[slot] = sp->pix;
	}

	for (int y = 0; y < h; y++) {
		int x = 0;

		while (x < w) {
			const KtuiCell *c = &cur[y * w + x];

			if (!force_full && !memcmp(c, &prev[y * w + x],
						   sizeof(*c))) {
				x++;
				continue;
			}

			int start = x;

			while (x < w &&
			       (force_full || memcmp(&cur[y * w + x],
						     &prev[y * w + x],
						     sizeof(KtuiCell))))
				x++;

			kcon_buf_reset(&buf);
			kcon_put_run(&buf, (uint16_t)start, (uint16_t)y,
				     &cur[y * w + start], (uint16_t)(x - start));
			kcon_send(C.conn, KCON_OP_COMMIT, &buf);

			memcpy(&prev[y * w + start], &cur[y * w + start],
			       sizeof(KtuiCell) * (size_t)(x - start));
		}
	}

	kcon_buf_free(&buf);
	kcon_flush(C.conn);
}

static int cl_poll(KtuiEvent *ev, int timeout_ms)
{
	if (pop(ev))
		return 1;

	if (!C.conn || kcon_conn_dead(C.conn)) {
		C.should_close = 1;
		ev->type = KT_EVT_TICK;
		return 0;
	}

	struct pollfd p = { .fd = kcon_conn_fd(C.conn), .events = POLLIN };

	/* Writable is only interesting while something is queued; asking for
	 * it always would spin. */
	if (kcon_flush(C.conn) > 0)
		p.events |= POLLOUT;

	poll(&p, 1, timeout_ms);

	KconMsg m;
	int r;

	while ((r = kcon_recv(C.conn, &m)) == 1)
		handle(&m);

	if (r < 0)
		C.should_close = 1;

	if (pop(ev))
		return 1;

	ev->type = KT_EVT_TICK;
	return 0;
}

static void cl_size(int *w, int *h)
{
	*w = C.cols > 0 ? C.cols : 80;
	*h = C.rows > 0 ? C.rows : 24;
}

static int cl_caps(void)
{
	/*
	 * The same answer libkwl gives, and for the same reason: this is our
	 * own renderer at the far end, so "does the terminal support it" has no
	 * meaning. Not LINUXVT, which is what keeps bold usable and evdev out
	 * of the input path.
	 */
	return KT_CAP_TRUECOLOR | KT_CAP_UTF8 | KT_CAP_MOUSE;
}

static const KtuiBackend kcon_backend = {
	.name = "console",
	.flush = cl_flush,
	.poll_event = cl_poll,
	.size = cl_size,
	.caps = cl_caps,
};

/* ── the display implementation ──────────────────────────────────────── */

static int kcon_init(const KDispConfig *cfg)
{
	const char *path = getenv("KDOS_CON");

	memset(&C, 0, sizeof(C));
	C.cols = cfg && cfg->cols > 0 ? cfg->cols : 80;
	C.rows = cfg && cfg->rows > 0 ? cfg->rows : 24;

	int fd = connect_to(path);

	if (fd < 0)
		return -1;

	C.conn = kcon_conn_new(fd);
	if (!C.conn)
		return -1;

	KconBuf b = { 0 };

	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_SURFACE);
	if (kcon_send(C.conn, KCON_OP_HELLO, &b) != 0)
		goto fail;

	C.role = cfg ? (int)cfg->role : 0;
	C.edge = cfg ? cfg->edge : 0;
	C.cells = cfg ? cfg->cells : 0;
	C.exclusive = cfg ? cfg->exclusive : 0;
	snprintf(C.app_id, sizeof(C.app_id), "%s", cfg && cfg->app_id ? cfg->app_id : "");
	snprintf(C.title, sizeof(C.title), "%s", cfg && cfg->title ? cfg->title : "");
	snprintf(C.output, sizeof(C.output), "%s", cfg && cfg->output ? cfg->output : "");

	kcon_buf_reset(&b);
	kcon_put_u16(&b, (uint16_t)C.role);
	kcon_put_u16(&b, (uint16_t)C.edge);
	kcon_put_u16(&b, (uint16_t)C.cells);
	kcon_put_u16(&b, (uint16_t)C.cols);
	kcon_put_u16(&b, (uint16_t)C.rows);
	kcon_put_u8(&b, (uint8_t)C.exclusive);
	kcon_put_str(&b, C.app_id);
	kcon_put_str(&b, C.title);
	kcon_put_str(&b, C.output);
	if (kcon_send(C.conn, KCON_OP_ATTACH, &b) != 0)
		goto fail;

	kcon_buf_free(&b);
	ktui_backend_set(&kcon_backend);
	return 0;

fail:
	kcon_buf_free(&b);
	kcon_conn_free(C.conn);
	C.conn = NULL;
	return -1;
}

void kcon_set_sprite_bits(KconSpriteBits fn, void *user)
{
	C.bits_fn = fn;
	C.bits_user = user;
}

static void kcon_shutdown(void)
{
	if (C.conn) {
		kcon_send(C.conn, KCON_OP_CLOSE, NULL);
		kcon_flush(C.conn);
		kcon_conn_free(C.conn);
		C.conn = NULL;
	}
	ktui_backend_set(NULL);
}

static int kcon_should_close(void)
{
	return C.should_close;
}

static int kcon_fd(void)
{
	return C.conn ? kcon_conn_fd(C.conn) : -1;
}

static void kcon_pump(void)
{
	KtuiEvent ev;

	(void)cl_poll(&ev, 0);
}

static int kcon_copy(const char *text, size_t len, int primary)
{
	if (!C.conn)
		return -1;

	KconBuf b = { 0 };

	kcon_put_u8(&b, (uint8_t)(primary ? 1 : 0));
	kcon_put_blob(&b, text, len);

	int r = kcon_send(C.conn, KCON_OP_CLIP_OFFER, &b);

	kcon_buf_free(&b);
	return r;
}

static int kcon_drag_start(const char *mime, const char *data, size_t len)
{
	if (!C.conn)
		return -1;

	KconBuf b = { 0 };

	kcon_put_str(&b, mime);
	kcon_put_blob(&b, data, len);

	int r = kcon_send(C.conn, KCON_OP_DRAG_START, &b);

	kcon_buf_free(&b);
	return r;
}

/*
 * A cell is a cell here: there are no pixels on this side of the socket, so
 * the size of one is the far end's business and one is the honest answer.
 */
static int kcon_cell_w(void) { return 1; }
static int kcon_cell_h(void) { return 1; }
static int kcon_scale(void) { return 1; }
/* The server draws the furniture, as a compositor does. */
static int kcon_decorated(void) { return 0; }

/*
 * The password was accepted. Sent as its own message rather than inferred from
 * the exit that follows it: the session cannot tell a clean exit from a crash
 * by the socket closing, and it must not guess — see KCON_OP_UNLOCK.
 */
static void kcon_unlock(void)
{
	if (!C.conn)
		return;
	kcon_send(C.conn, KCON_OP_UNLOCK, NULL);
	kcon_flush(C.conn);
}

/*
 * A DIFFERENT SIZE IS A SECOND ATTACH. The session reads a requested size out
 * of one already; sending it again is how a surface that has grown asks for the
 * room, and the answer comes back as the ordinary configure.
 */
static int kcon_overlay_resize(int cols, int rows)
{
	KconBuf b = { 0 };
	int was_cols = C.cols, was_rows = C.rows;

	if (!C.conn || cols < 1 || rows < 1)
		return -1;
	if (cols == C.cols && rows == C.rows)
		return 0;

	/*
	 * C.cols and C.rows ARE NOT SET HERE. They are what the session last
	 * configured, and the configure handler ignores one that matches them —
	 * so setting them to the request first makes the answer look like no
	 * change at all, and the surface draws for ever at the size it had.
	 */
	kcon_put_u16(&b, (uint16_t)C.role);
	kcon_put_u16(&b, (uint16_t)C.edge);
	kcon_put_u16(&b, (uint16_t)C.cells);
	kcon_put_u16(&b, (uint16_t)cols);
	kcon_put_u16(&b, (uint16_t)rows);
	kcon_put_u8(&b, (uint8_t)C.exclusive);
	kcon_put_str(&b, C.app_id);
	kcon_put_str(&b, C.title);
	kcon_put_str(&b, C.output);
	kcon_send(C.conn, KCON_OP_ATTACH, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);

	/*
	 * WAIT FOR THE CONFIGURE, bounded. The caller draws immediately after
	 * this returns, and a draw against the old size is a frame at the wrong
	 * size that nothing ever repaints. The session may answer with a size
	 * that is not the one asked for — it places windows and it has an edge
	 * to fit them inside — so the wait ends on ANY answer.
	 */
	for (int spin = 0; spin < 200; spin++) {
		struct pollfd p = { .fd = kcon_conn_fd(C.conn),
				    .events = POLLIN, .revents = 0 };

		if (C.cols != was_cols || C.rows != was_rows)
			break;
		poll(&p, 1, 5);
		kcon_pump();
	}
	return 0;
}

static void kcon_hide(int on)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u8(&b, (uint8_t)(on ? 1 : 0));
	kcon_send(C.conn, KCON_OP_HIDE, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static int kcon_overlay_show(int cols, int rows)
{
	kcon_hide(0);
	return kcon_overlay_resize(cols, rows);
}

static void kcon_overlay_hide(void)
{
	kcon_hide(1);
}

const KDispImpl kcon_impl = {
	.name = "console",
	.probe = kcon_probe,
	.init = kcon_init,
	.shutdown = kcon_shutdown,
	.should_close = kcon_should_close,
	.fd = kcon_fd,
	.pump = kcon_pump,
	.copy = kcon_copy,
	.drag_start = kcon_drag_start,
	.cell_w = kcon_cell_w,
	.cell_h = kcon_cell_h,
	.scale = kcon_scale,
	.decorated = kcon_decorated,
	.overlay_resize = kcon_overlay_resize,
	.overlay_show = kcon_overlay_show,
	.overlay_hide = kcon_overlay_hide,
	.unlock = kcon_unlock,
};

/*
 * `kdos con detach`, from a process that is not a view.
 *
 * A one-shot connection: say hello as a shell, ask, close. It is on the
 * SURFACE socket rather than the view socket because the right to take the
 * screen away belongs to the session's owner, not to whatever is displaying
 * it — and the surface socket is the one that never leaves the machine.
 */
/*
 * How long the wait above may be. Long enough for a session that is busy
 * compositing a frame, short enough that a launcher stuck behind a dead session
 * is a launcher somebody can still close.
 */
#define KCON_RUN_WAIT_MS 3000

static int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/*
 * Ask the session to run something on a VT of its own, and WAIT for the answer.
 *
 * Every other one-shot here sends and closes. This one waits because the answer
 * is the point: a machine with every terminal already in use has to say so, and
 * the launcher is the only thing in the chain with a person in front of it. The
 * wait is bounded — a session that has stopped answering must not hang the
 * launcher that asked.
 */
int kcon_run(const char *sock, const char *const argv[], const char *title,
		unsigned flags)
{
	int n = 0;

	if (!argv || !argv[0])
		return -1;
	while (argv[n] && n < KCON_MAX_ARGV)
		n++;
	if (argv[n])
		return -1;	/* longer than the wire carries: refuse, do
				 * not send a truncated command line */

	int fd = connect_to(sock);

	if (fd < 0)
		return -1;

	KconConn *c = kcon_conn_new(fd);

	if (!c) {
		close(fd);
		return -1;
	}

	KconBuf b = { 0 };

	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_SHELL);
	kcon_send(c, KCON_OP_HELLO, &b);
	kcon_buf_reset(&b);

	kcon_put_str(&b, title ? title : "");
	kcon_put_u16(&b, (uint16_t)flags);
	kcon_put_u16(&b, (uint16_t)n);
	for (int i = 0; i < n; i++)
		kcon_put_str(&b, argv[i]);
	kcon_send(c, KCON_OP_RUN, &b);
	kcon_buf_free(&b);

	int vt = -1;

	if (kcon_flush(c) >= 0) {
		int64_t deadline = now_ms() + KCON_RUN_WAIT_MS;

		for (;;) {
			KconMsg m;
			int r = kcon_recv(c, &m);

			if (r < 0)
				break;
			if (r == 1) {
				if (m.op != KCON_OP_RUN_REPLY)
					continue;

				KconRd rd;

				kcon_rd_init(&rd, m.payload, m.len);
				int ok = (int)kcon_get_u16(&rd);
				int got = (int)kcon_get_u16(&rd);

				vt = (!rd.err && ok) ? got : -1;
				break;
			}

			int left = (int)(deadline - now_ms());

			if (left <= 0)
				break;

			struct pollfd p = { kcon_conn_fd(c), POLLIN, 0 };

			if (poll(&p, 1, left) <= 0)
				break;
		}
	}

	kcon_conn_free(c);
	return vt;
}

int kcon_detach_all(const char *sock)
{
	int fd = connect_to(sock);

	if (fd < 0)
		return -1;

	KconConn *c = kcon_conn_new(fd);

	if (!c) {
		close(fd);
		return -1;
	}

	KconBuf b = { 0 };

	kcon_put_u16(&b, KCON_VERSION);
	kcon_put_u16(&b, KCON_KIND_SHELL);
	kcon_send(c, KCON_OP_HELLO, &b);
	kcon_buf_reset(&b);
	kcon_send(c, KCON_OP_DETACH, &b);
	kcon_buf_free(&b);

	int rc = kcon_flush(c) < 0 ? -1 : 0;

	kcon_conn_free(c);
	return rc;
}
