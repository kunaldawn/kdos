/* libkcon — the server half: connections, and the surfaces on them.
 * See kcon.h.
 *
 * What this owns is the transport and each surface's grid. What it does not
 * own is where a window goes or which is on top: that is the window model's,
 * and a display reads the surfaces out of here and arranges them itself.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kcon.h"

/*
 * A ceiling on clients, because every one is a descriptor and a grid. A
 * display that has run out says so and keeps serving the ones it has, rather
 * than accepting until it cannot open a font.
 */
#define KCON_MAX_CLIENTS 128

struct KconSurface {
	KconServer *server;
	KconConn *conn;

	int hello;		/* the handshake completed */
	int kind_fixed;		/* the listener decided; the hello cannot */
	int no_view;		/* reached the surface socket: never a display */
	int attached;
	unsigned kind;
	unsigned role;
	int edge, want_cells, exclusive;
	int hidden;

	char app_id[128];
	char title[256];

	int cols, rows;

	/*
	 * WHAT A VIEW ASKED FOR, which is not `cols`/`rows` above: for a view
	 * those hold the size of the frame it was last sent. Zero is "I impose
	 * nothing" and has to stay zero, or a view that asked for no size would
	 * start imposing whatever it was first handed the moment it received a
	 * frame.
	 */
	int view_cols, view_rows;

	/*
	 * A VIEW'S PIXEL GEOMETRY, from its hello. Zero is a view with none —
	 * a terminal — and is not an error: it is the answer that makes the
	 * session use its own default rather than refuse to size a guest.
	 */
	int cell_w, cell_h;
	unsigned caps;

	/*
	 * A SURFACE'S SLOT NUMBERS ARE ITS OWN, and two surfaces both using
	 * slot 0 is the normal case. The session slot is assigned here on
	 * first sight, so a compositing session can rewrite the slot in every
	 * sprite cell it copies out and two pictures never become one.
	 */
	int slotmap[KCON_MAX_SPRITE_MAP];
	/* A surface's committed grid. For a VIEW this is its PREVIOUS frame
	 * instead — the thing the next send is diffed against. */
	KtuiCell *cells;
	int dirty;
	int have_prev;

	int gone;
};

/*
 * A LISTENER, AND THE KIND OF CLIENT IT ADMITS. Which socket a peer reached is
 * evidence; what it says in its hello is a claim. A view socket may be
 * forwarded over ssh — that is what makes a remote desktop fall out of this
 * design rather than be built — and a forwarded socket that admitted surfaces
 * would let the far end place windows in your session. So the kind comes from
 * the listener, and the hello's is overridden.
 */
typedef struct {
	int fd;
	int kind;		/* KCON_LISTEN_* */
	char path[108];
} KconListen;

struct KconServer {
	KconListen l[KCON_MAX_LISTEN];
	int nl;
	unsigned next_slot;
	KconSurface *s[KCON_MAX_CLIENTS];
	int n;
	KconServerHooks hooks;
	void *user;
};

/* ── surfaces ────────────────────────────────────────────────────────── */

static void blank(KtuiCell *c, int n)
{
	KtuiCell b = { ' ', KT_TEXT, KT_BG, KT_A_NONE };

	for (int i = 0; i < n; i++)
		c[i] = b;
}

static int resize(KconSurface *f, int cols, int rows)
{
	if (cols <= 0 || rows <= 0 || cols > 4096 || rows > 4096)
		return -1;
	if (f->cells && cols == f->cols && rows == f->rows)
		return 0;

	KtuiCell *c = calloc((size_t)cols * rows, sizeof(*c));

	if (!c)
		return -1;

	blank(c, cols * rows);
	free(f->cells);
	f->cells = c;
	f->cols = cols;
	f->rows = rows;
	f->dirty = 1;
	return 0;
}

static void surface_free(KconSurface *f)
{
	if (!f)
		return;
	kcon_conn_free(f->conn);
	free(f->cells);
	free(f);
}

/* ── the wire, inbound ───────────────────────────────────────────────── */

static void on_msg(KconSurface *f, const KconMsg *m)
{
	KconServer *s = f->server;
	KconRd r;

	kcon_rd_init(&r, m->payload, m->len);

	/*
	 * NOTHING BEFORE HELLO. A peer that starts committing cells without
	 * agreeing a version is either a different protocol or a probe, and
	 * either way it is not talked to.
	 */
	if (!f->hello && m->op != KCON_OP_HELLO) {
		f->gone = 1;
		return;
	}

	switch (m->op) {
	case KCON_OP_HELLO: {
		unsigned ver = kcon_get_u16(&r);

		unsigned claimed = kcon_get_u16(&r);

		/* THE LISTENER DECIDES, not the peer. A socket that admits
		 * only views is the whole of the remote-attach security
		 * model; honouring a hello that said "surface" would undo
		 * it. A single-listener server has nothing to decide and
		 * takes the claim. */
		/*
		 * A claim of VIEW on the surface socket is refused and the
		 * client stays a surface: that socket admits programs that
		 * place windows and programs that drive the session, and
		 * nothing that shows one.
		 */
		if (!f->kind_fixed &&
		    !(f->no_view && claimed == KCON_KIND_VIEW))
			f->kind = claimed;
		if (r.err || ver != KCON_VERSION) {
			/*
			 * BOTH numbers go back, because "protocol error" tells
			 * the person nothing about which half to rebuild.
			 */
			KconBuf b = { 0 };

			kcon_put_u16(&b, KCON_VERSION);
			kcon_put_u16(&b, (uint16_t)ver);
			kcon_send(f->conn, KCON_OP_BYE, &b);
			kcon_buf_free(&b);
			f->gone = 1;
			return;
		}
		f->hello = 1;

		/*
		 * OPTIONAL, AND ONLY FROM A VIEW. Everything before this point
		 * is what every peer sends; a view adds what it can show. A
		 * message without them is a view that has no pixel geometry,
		 * which is the answer a terminal gives.
		 */
		if (f->kind == KCON_KIND_VIEW && kcon_rd_left(&r) >= 6) {
			int cw = (int)kcon_get_u16(&r);
			int chh = (int)kcon_get_u16(&r);
			unsigned caps = kcon_get_u16(&r);

			if (!r.err && cw >= 0 && cw <= 256 && chh >= 0 &&
			    chh <= 256) {
				f->cell_w = cw;
				f->cell_h = chh;
				f->caps = caps;
			}
		}
		break;
	}
	case KCON_OP_DETACH:
		if (f->kind == KCON_KIND_VIEW) {
			f->gone = 1;
		} else if (f->kind == KCON_KIND_SHELL) {
			for (int i = 0; i < s->n; i++) {
				KconSurface *v = s->s[i];

				if (v->kind != KCON_KIND_VIEW)
					continue;
				/* A REASON, so a view's log says why it lost
				 * the session rather than reporting a closed
				 * socket. */
				KconBuf b = { 0 };

				kcon_put_u16(&b, 0);
				kcon_send(v->conn, KCON_OP_BYE, &b);
				kcon_buf_free(&b);
				v->gone = 1;
			}
		}
		break;

	case KCON_OP_ACTIVATE: {
		unsigned id = kcon_get_u32(&r);

		/* A SHELL SURFACE ONLY, the rule every management verb keeps:
		 * a program with a window in the session must not be able to
		 * raise or close another one. */
		if (!r.err && f->kind == KCON_KIND_SHELL && s->hooks.activate)
			s->hooks.activate(f, id, s->user);
		break;
	}
	case KCON_OP_CLOSE_REQUEST: {
		unsigned id = kcon_get_u32(&r);

		if (!r.err && f->kind == KCON_KIND_SHELL &&
		    s->hooks.close_request)
			s->hooks.close_request(f, id, s->user);
		break;
	}
	case KCON_OP_QUIT:
		/*
		 * A SHELL SURFACE ONLY, the same rule KCON_OP_DETACH keeps: a
		 * client with a window in the session has no business ending
		 * it for the person using it.
		 */
		if (f->kind == KCON_KIND_SHELL && s->hooks.quit)
			s->hooks.quit(f, s->user);
		break;

	case KCON_OP_PASTE: {
		const char *text = kcon_get_str(&r);

		/*
		 * ONLY FROM A DISPLAY, and only text. A surface with a paste
		 * verb could type into whatever has the focus without the
		 * person touching a key, which is the one thing a client on
		 * this socket must never be able to do.
		 */
		if (r.err || f->kind != KCON_KIND_VIEW || !*text)
			return;
		if (s->hooks.paste)
			s->hooks.paste(f, text, s->user);
		break;
	}

	case KCON_OP_VIEW_SIZE: {
		int cols = (int)kcon_get_u16(&r);
		int rows = (int)kcon_get_u16(&r);

		if (r.err || f->kind != KCON_KIND_VIEW)
			return;
		if (cols < 0 || rows < 0 || cols > 4096 || rows > 4096)
			return;

		/*
		 * ZERO IS "I IMPOSE NOTHING", AND IT STILL ATTACHES. A
		 * screenshot and a screencast both ask for no size so that
		 * taking one does not resize the desktop being taken — a view
		 * that refused to attach on that answer would never be sent a
		 * frame, and would then report that the session never said how
		 * big it is.
		 */
		f->view_cols = cols;
		f->view_rows = rows;
		f->cols = cols;
		f->rows = rows;
		/* Its previous frame is meaningless at a new size, so the next
		 * send is a whole one. */
		f->have_prev = 0;
		f->attached = 1;
		if (s->hooks.attached)
			s->hooks.attached(f, s->user);
		break;
	}
	case KCON_OP_ATTACH: {
		f->role = kcon_get_u16(&r);
		f->edge = (int)kcon_get_u16(&r);
		f->want_cells = (int)kcon_get_u16(&r);

		int cols = (int)kcon_get_u16(&r);
		int rows = (int)kcon_get_u16(&r);

		f->exclusive = (int)kcon_get_u8(&r);
		snprintf(f->app_id, sizeof(f->app_id), "%s", kcon_get_str(&r));
		snprintf(f->title, sizeof(f->title), "%s", kcon_get_str(&r));
		(void)kcon_get_str(&r);		/* output, chosen by the display */

		/*
		 * A SIZE OF ZERO IS A QUESTION, AND ONLY WHERE THE SESSION
		 * OWNS THE ANSWER. A saver covers the screen and a docked panel
		 * spans its edge; neither can know how big the screen is, so
		 * they ask for nothing — the panel naming only its thickness —
		 * and the session answers with a CONFIGURE, which is what
		 * allocates the cells.
		 *
		 * FROM ANY OTHER ROLE IT IS STILL FATAL. Nothing is going to
		 * tell them a size, so a surface let through would wait for a
		 * configure that never comes — a client hung with no message,
		 * where refusing the attach says which half is wrong.
		 */
		int nosize = cols <= 0 || rows <= 0;
		int asks = f->role == KDISP_ROLE_SAVER ||
			   (f->role == KDISP_ROLE_PANEL && f->want_cells > 0);

		if (r.err || (nosize && !asks) ||
		    (!nosize && resize(f, cols, rows) != 0)) {
			f->gone = 1;
			return;
		}
		f->attached = 1;
		if (s->hooks.attached)
			s->hooks.attached(f, s->user);
		break;
	}
	case KCON_OP_HIDE: {
		int on = (int)kcon_get_u8(&r);

		if (!r.err)
			f->hidden = on != 0;
		break;
	}
	case KCON_OP_COMMIT: {
		/*
		 * A run is clipped to the surface's own grid rather than
		 * refused: a client whose configure crossed with its draw is
		 * behind, not hostile, and the next frame corrects it.
		 */
		while (r.pos < r.len && !r.err) {
			uint16_t x, y;
			KtuiCell run[4096];
			int n = kcon_get_run(&r, &x, &y, run, 4096);

			if (n < 0)
				break;
			if ((int)y >= f->rows)
				continue;
			for (int i = 0; i < n; i++) {
				int cx = (int)x + i;

				if (cx >= f->cols)
					break;
				f->cells[(int)y * f->cols + cx] = run[i];
			}
			f->dirty = 1;
		}
		break;
	}
	case KCON_OP_KEY:
		/* Only a view sends input. A surface doing so is talking the
		 * wrong direction and is ignored rather than trusted. */
		if (f->kind == KCON_KIND_VIEW && s->hooks.view_key) {
			int key = kcon_get_i32(&r);
			int mods = (int)kcon_get_u8(&r);

			if (!r.err)
				s->hooks.view_key(f, key, mods, s->user);
		}
		break;
	case KCON_OP_PTR:
		if (f->kind == KCON_KIND_VIEW && s->hooks.view_ptr) {
			int x = kcon_get_i32(&r);
			int y = kcon_get_i32(&r);
			int btn = (int)kcon_get_u8(&r);
			int press = (int)kcon_get_u8(&r);
			/* THE CENTRE OF THE CELL is what a view that cannot say
			 * means. Zero would be its top-left corner, which on a
			 * scrollbar one cell wide is the pixel beside it. */
			int subx = 128, suby = 128;

			if (kcon_rd_left(&r) >= 2) {
				subx = (int)kcon_get_u8(&r);
				suby = (int)kcon_get_u8(&r);
			}

			if (!r.err)
				s->hooks.view_ptr(f, x, y, subx, suby, btn,
						  press, s->user);
		}
		break;
	case KCON_OP_TITLE:
		snprintf(f->title, sizeof(f->title), "%s", kcon_get_str(&r));
		break;
	case KCON_OP_CLIP_OFFER: {
		int primary = (int)kcon_get_u8(&r);
		uint32_t n = kcon_get_u32(&r);
		const void *p = kcon_get_blob(&r, n);

		if (!r.err && s->hooks.clip_offer)
			s->hooks.clip_offer(f, p, n, primary, s->user);
		break;
	}
	case KCON_OP_CLIP_REQUEST: {
		int primary = (int)kcon_get_u8(&r);

		if (!r.err && s->hooks.clip_request)
			s->hooks.clip_request(f, primary, s->user);
		break;
	}
	case KCON_OP_DRAG_START: {
		const char *mime = kcon_get_str(&r);
		char keep[64];

		snprintf(keep, sizeof(keep), "%s", mime);

		uint32_t n = kcon_get_u32(&r);
		const void *p = kcon_get_blob(&r, n);

		if (!r.err && s->hooks.drag_start)
			s->hooks.drag_start(f, keep, p, n, s->user);
		break;
	}
	case KCON_OP_SPRITE: {
		unsigned cslot = kcon_get_u16(&r);
		int cw = (int)kcon_get_u16(&r);
		int ch = (int)kcon_get_u16(&r);
		uint32_t fallback = kcon_get_u32(&r);
		int pw = (int)kcon_get_u16(&r);
		int ph = (int)kcon_get_u16(&r);
		const uint32_t *argb = NULL;

		if (r.err || cslot >= KCON_MAX_SPRITE_MAP || cw < 1 ||
		    ch < 1 || cw > 16 || ch > 16)
			break;

		if (pw > 0 && ph > 0) {
			/* The declared size is an allocation request from an
			 * untrusted peer, so it is bounded by what a sprite
			 * can be before the blob is read. */
			if (pw > 16 * 256 || ph > 16 * 256)
				break;
			argb = kcon_get_blob(&r, (size_t)pw * (size_t)ph * 4);
			if (r.err || !argb)
				break;
		}

		/* Assign a session slot the first time this surface names one.
		 * Sequential from the server's counter, so it is unique across
		 * every client rather than only within one. */
		if (f->slotmap[cslot] < 0)
			f->slotmap[cslot] = (int)(s->next_slot++ %
						  KCON_MAX_SPRITE_MAP);

		if (s->hooks.sprite)
			s->hooks.sprite(f, f->slotmap[cslot], cw, ch,
					fallback, argb, pw, ph, s->user);
		break;
	}
	case KCON_OP_CLOSE:
		/*
		 * A LOCK SURFACE CLOSING IS NOT AN UNLOCK. Closing is what a
		 * client does when it exits for any reason, including an error
		 * it did not expect, and a screen that unlocked on that path
		 * would be a lock screen worth nothing. Only KCON_OP_UNLOCK
		 * lifts the lock.
		 */
		f->gone = 1;
		break;

	case KCON_OP_RUN: {
		/*
		 * A SHELL SURFACE ONLY, the same rule KCON_OP_DETACH keeps: a
		 * program with a window in this session has no business
		 * starting one on a terminal the person is not looking at.
		 */
		const char *title = kcon_get_str(&r);
		unsigned flags = kcon_get_u16(&r);
		unsigned n = kcon_get_u16(&r);
		const char *av[KCON_MAX_ARGV + 1];
		char abuf[KCON_MAX_ARGV][256];
		char tbuf[128];

		snprintf(tbuf, sizeof(tbuf), "%s", title);

		/* The count is an allocation request from a peer, so it is
		 * bounded before anything is read against it. */
		if (r.err || n == 0 || n > KCON_MAX_ARGV)
			break;
		/* COPIED, one at a time. kcon_get_str hands back one shared
		 * buffer, so keeping the pointers would leave every argument
		 * equal to the last one. */
		for (unsigned i = 0; i < n; i++) {
			snprintf(abuf[i], sizeof(abuf[i]), "%s",
				 kcon_get_str(&r));
			av[i] = abuf[i];
		}
		av[n] = NULL;
		if (r.err)
			break;

		int vt = -1;

		if (f->kind == KCON_KIND_SHELL && s->hooks.run)
			vt = s->hooks.run(f, av, tbuf, flags, s->user);

		KconBuf b = { 0 };

		/*
		 * TWO FIELDS, BECAUSE ZERO IS AN ANSWER. A guest that became an
		 * ordinary window is on no terminal at all, and a single number
		 * cannot say that and "it did not start" both.
		 */
		kcon_put_u16(&b, vt >= 0 ? 1 : 0);
		kcon_put_u16(&b, (uint16_t)(vt > 0 ? vt : 0));
		kcon_send(f->conn, KCON_OP_RUN_REPLY, &b);
		kcon_buf_free(&b);
		break;
	}

	case KCON_OP_UNLOCK:
		/*
		 * THE ONE MESSAGE THAT UNLOCKS ANYTHING, and only from a
		 * surface that took the lock role. A window sending it is a
		 * window asking to dismiss a lock screen it does not own.
		 */
		if (f->role == KDISP_ROLE_LOCK && s->hooks.unlock)
			s->hooks.unlock(f, s->user);
		break;
	default:
		/* An opcode this version does not know is ignored rather than
		 * fatal: the handshake already refused a peer we cannot talk
		 * to, and within a version an unknown message is an addition. */
		break;
	}
}

/* ── the server ──────────────────────────────────────────────────────── */

int kcon_server_listen(KconServer *s, const char *path, int kind)
{
	struct sockaddr_un sa;

	if (!s || !path || !*path || strlen(path) >= sizeof(sa.sun_path))
		return -1;
	if (s->nl >= KCON_MAX_LISTEN)
		return -1;

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);

	if (fd < 0)
		return -1;

	/* A socket left behind by a session that died would make this look
	 * like a display that is already running. */
	unlink(path);

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, strlen(path));

	/*
	 * 0600, and the directory above it is 0700. The peer-credential check
	 * is the real gate, but a socket another account can connect to at all
	 * is one it can hold open and probe; the two together mean only this
	 * user's processes ever reach the handshake.
	 */
	mode_t old = umask(0177);
	int rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));

	umask(old);

	if (rc != 0 || listen(fd, 16) != 0) {
		close(fd);
		return -1;
	}

	/*
	 * A VIEW LISTENER MAKES THE OTHERS EXPLICIT. Once a session has a
	 * socket that admits only views, the one that admits "anything" must
	 * become the surface socket — leaving it open to both would let a peer
	 * reach the surface socket and be whatever it claimed, which is the
	 * separation this pair of sockets exists to make.
	 */
	if (kind == KCON_LISTEN_VIEW)
		for (int i = 0; i < s->nl; i++)
			if (s->l[i].kind == KCON_LISTEN_ANY)
				s->l[i].kind = KCON_LISTEN_SURFACE;

	s->l[s->nl].fd = fd;
	s->l[s->nl].kind = kind;
	snprintf(s->l[s->nl].path, sizeof(s->l[s->nl].path), "%s", path);
	s->nl++;
	return 0;
}

int kcon_server_unlisten(KconServer *s, const char *path)
{
	if (!s || !path)
		return -1;
	for (int i = 0; i < s->nl; i++) {
		if (strcmp(s->l[i].path, path))
			continue;
		close(s->l[i].fd);
		unlink(s->l[i].path);
		s->l[i] = s->l[--s->nl];
		return 0;
	}
	return -1;
}

KconServer *kcon_server_new(const char *path)
{
	KconServer *s = calloc(1, sizeof(*s));

	if (!s)
		return NULL;

	/*
	 * ONE LISTENER, ADMITTING BOTH KINDS. That is a session whose display
	 * and whose surfaces share a socket — the offscreen and --dump paths,
	 * where there is nothing to forward and nothing to separate. A session
	 * that will be attached to adds a view listener of its own, and from
	 * that point the kinds are decided by which socket a peer reached.
	 */
	if (kcon_server_listen(s, path, KCON_LISTEN_ANY) != 0) {
		free(s);
		return NULL;
	}

	return s;
}

void kcon_server_free(KconServer *s)
{
	if (!s)
		return;
	for (int i = 0; i < s->n; i++)
		surface_free(s->s[i]);
	for (int i = 0; i < s->nl; i++) {
		close(s->l[i].fd);
		unlink(s->l[i].path);
	}
	free(s);
}

int kcon_server_nfds(const KconServer *s)
{
	return s ? s->nl : 0;
}

int kcon_server_fd_at(const KconServer *s, int i)
{
	return s && i >= 0 && i < s->nl ? s->l[i].fd : -1;
}

int kcon_server_fd(const KconServer *s)
{
	return kcon_server_fd_at(s, 0);
}

void kcon_server_hooks(KconServer *s, const KconServerHooks *h, void *user)
{
	if (!s)
		return;
	if (h)
		s->hooks = *h;
	else
		memset(&s->hooks, 0, sizeof(s->hooks));
	s->user = user;
}

/*
 * THE GATE. Filesystem permissions already keep the socket inside a 0700
 * directory, and this is the second half: the peer's own credentials, checked
 * by the kernel and unforgeable, must be this session's owner. A forwarded
 * socket does not weaken it — ssh already authenticated, and what comes out
 * the far end still runs as somebody.
 */
static int peer_ok(int fd)
{
	struct ucred cr;
	socklen_t n = sizeof(cr);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &n) != 0)
		return 0;
	return cr.uid == getuid();
}

static void drop(KconServer *s, int i)
{
	KconSurface *f = s->s[i];

	if (s->hooks.gone && f->attached)
		s->hooks.gone(f, s->user);
	surface_free(f);
	s->s[i] = s->s[--s->n];
}

int kcon_server_pump(KconServer *s)
{
	if (!s)
		return 0;

	for (int li = 0; li < s->nl; li++) {
		for (;;) {
			int fd = accept(s->l[li].fd, NULL, NULL);

			if (fd < 0)
				break;

			if (!peer_ok(fd) || s->n >= KCON_MAX_CLIENTS) {
				close(fd);
				continue;
			}

			KconSurface *f = calloc(1, sizeof(*f));

			if (!f) {
				close(fd);
				continue;
			}

			f->conn = kcon_conn_new(fd);
			if (!f->conn) {
				close(fd);
				free(f);
				continue;
			}

			f->server = s;
			for (int k = 0; k < KCON_MAX_SPRITE_MAP; k++)
				f->slotmap[k] = -1;
			/*
			 * THE LISTENER DECIDES WHAT A CLIENT MAY BE, and the
			 * two sockets do not decide it the same way.
			 *
			 * A VIEW listener is absolute: whatever a client
			 * claims, it is a display. That socket is the one that
			 * may be forwarded, and the far end must never be able
			 * to place a window or drive the session.
			 *
			 * A SURFACE listener sets the DEFAULT and no more. It
			 * never leaves the machine, so a program that reached
			 * it is already this session's own user — and a shell
			 * surface is exactly that: `kdos con run`, `detach`
			 * and `kill`, the panel asking for the window list.
			 * Fixing the kind here made KCON_KIND_SHELL
			 * unreachable on any session that had a view socket,
			 * which is every real one, and every shell-only verb
			 * silently did nothing.
			 *
			 * What it must still refuse is a claim of VIEW: a
			 * client on the surface socket is not a display.
			 */
			if (s->l[li].kind == KCON_LISTEN_VIEW) {
				f->kind = KCON_KIND_VIEW;
				f->kind_fixed = 1;
			} else if (s->l[li].kind == KCON_LISTEN_SURFACE) {
				f->kind = KCON_KIND_SURFACE;
				f->no_view = 1;
			}
			s->s[s->n++] = f;
		}
	}

	int changed = 0;

	for (int i = 0; i < s->n;) {
		KconSurface *f = s->s[i];
		KconMsg m;
		int r;

		while ((r = kcon_recv(f->conn, &m)) == 1)
			on_msg(f, &m);

		if (r < 0 || f->gone || kcon_flush(f->conn) < 0) {
			drop(s, i);
			continue;
		}

		if (f->dirty)
			changed++;
		i++;
	}

	return changed;
}

int kcon_server_count(const KconServer *s)
{
	return s ? s->n : 0;
}

KconSurface *kcon_server_at(KconServer *s, int i)
{
	return s && i >= 0 && i < s->n ? s->s[i] : NULL;
}

/* ── views ───────────────────────────────────────────────────────────── */

int kcon_server_view_count(const KconServer *s)
{
	int n = 0;

	for (int i = 0; s && i < s->n; i++)
		if (s->s[i]->kind == KCON_KIND_VIEW && s->s[i]->attached)
			n++;
	return n;
}

KconSurface *kcon_server_view_at(KconServer *s, int i)
{
	for (int k = 0; s && k < s->n; k++) {
		KconSurface *f = s->s[k];

		if (f->kind != KCON_KIND_VIEW || !f->attached)
			continue;
		if (i-- == 0)
			return f;
	}
	return NULL;
}

int kcon_view_cols(const KconSurface *v) { return v ? v->view_cols : 0; }
int kcon_view_rows(const KconSurface *v) { return v ? v->view_rows : 0; }

int kcon_view_cell_w(const KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW ? v->cell_w : 0;
}

int kcon_view_cell_h(const KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW ? v->cell_h : 0;
}

unsigned kcon_view_caps(const KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW ? v->caps : 0u;
}

int kcon_surface_map_slot(const KconSurface *f, int client_slot)
{
	if (!f || client_slot < 0 || client_slot >= KCON_MAX_SPRITE_MAP)
		return -1;
	return f->slotmap[client_slot];
}

/*
 * ONE ROTATION FOR EVERY PICTURE IN THE SESSION, whoever owns it. A session
 * that numbered its own sprites separately would eventually hand a view a
 * number a surface is already using, and a view keys its cache on nothing else
 * — so one window's frame would appear inside another's cell.
 */
int kcon_server_alloc_slot(KconServer *s)
{
	if (!s)
		return -1;
	return (int)(s->next_slot++ % KCON_MAX_SPRITE_MAP);
}

/*
 * Forward a sprite to a view, as it arrives and to whatever is attached then.
 *
 * THE PIXEL SIZE TRAVELS WITH THE PICTURE, and the view scales it to its own
 * cells. A client cannot know how many pixels a cell is: a cell client has
 * none of its own, two views of one session can disagree, and a view attached
 * over ssh is a third answer. So the client sends the picture at whatever size
 * it has and the display — which is the only thing that knows — resamples it.
 *
 * A view that attaches AFTER a picture was sent does not have it: nothing here
 * keeps the blob, and a session that cached every sprite would be a session
 * holding megabytes of pixels it is otherwise built never to touch. Those
 * cells draw the fallback codepoint until the surface sends the slot again.
 */
void kcon_view_sprite(KconSurface *v, int slot, int w, int h,
		      uint32_t fallback, const uint32_t *argb, int pw, int ph)
{
	if (!v || v->kind != KCON_KIND_VIEW || slot < 0 ||
	    slot >= KCON_MAX_SPRITE_MAP)
		return;

	KconBuf b = { 0 };
	size_t npx = (argb && pw > 0 && ph > 0)
		     ? (size_t)pw * (size_t)ph : 0;

	kcon_put_u16(&b, (uint16_t)slot);
	kcon_put_u16(&b, (uint16_t)w);
	kcon_put_u16(&b, (uint16_t)h);
	kcon_put_u32(&b, fallback);
	kcon_put_u16(&b, (uint16_t)(npx ? pw : 0));
	kcon_put_u16(&b, (uint16_t)(npx ? ph : 0));
	if (npx)
		kcon_put_bytes(&b, argb, npx * 4);
	kcon_send(v->conn, KCON_OP_SPRITE, &b);
	kcon_buf_free(&b);
}

void kcon_server_resend_sprites(KconServer *s)
{
	for (int i = 0; s && i < s->n; i++) {
		KconSurface *f = s->s[i];

		if (f->kind == KCON_KIND_VIEW || !f->hello)
			continue;
		kcon_send(f->conn, KCON_OP_SPRITE_RESEND, NULL);
	}
}

void kcon_view_blank(KconSurface *v, int on)
{
	if (!v || v->kind != KCON_KIND_VIEW)
		return;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)(on ? 1 : 0));
	kcon_send(v->conn, KCON_OP_BLANK, &b);
	kcon_buf_free(&b);
}

void kcon_view_send(KconSurface *v, const KtuiCell *cells, int w, int h)
{
	if (!v || v->kind != KCON_KIND_VIEW || !cells || w <= 0 || h <= 0)
		return;

	/*
	 * EVERY VIEW KEEPS ITS OWN PREVIOUS FRAME. A view that attached a
	 * moment ago has seen nothing, and handing it the diff another view is
	 * up to date with would draw it a screen made of holes.
	 */
	int full = 0;

	if (!v->cells || v->cols != w || v->rows != h) {
		KtuiCell *c = calloc((size_t)w * h, sizeof(*c));

		if (!c)
			return;
		free(v->cells);
		v->cells = c;
		v->cols = w;
		v->rows = h;
		full = 1;

		/*
		 * TELL THE VIEW HOW BIG THE GRID IS. A view that imposed a
		 * size knows already, but one that attached asking for
		 * nothing — a screenshot, a second display on a session whose
		 * size another view decided — would otherwise have to infer
		 * the extent from which cells happened to be written, and a
		 * blank right-hand column is indistinguishable from a narrower
		 * screen.
		 */
		KconBuf cb = { 0 };

		kcon_put_u16(&cb, (uint16_t)w);
		kcon_put_u16(&cb, (uint16_t)h);
		kcon_send(v->conn, KCON_OP_CONFIGURE, &cb);
		kcon_buf_free(&cb);
	}
	if (!v->have_prev)
		full = 1;

	KconBuf b = { 0 };

	for (int y = 0; y < h; y++) {
		int x = 0;

		while (x < w) {
			if (!full && !memcmp(&cells[y * w + x],
					     &v->cells[y * w + x],
					     sizeof(KtuiCell))) {
				x++;
				continue;
			}

			int start = x;

			while (x < w &&
			       (full || memcmp(&cells[y * w + x],
					       &v->cells[y * w + x],
					       sizeof(KtuiCell))))
				x++;

			kcon_buf_reset(&b);
			kcon_put_run(&b, (uint16_t)start, (uint16_t)y,
				     &cells[y * w + start],
				     (uint16_t)(x - start));
			if (kcon_send(v->conn, KCON_OP_COMMIT, &b) != 0) {
				kcon_buf_free(&b);
				return;
			}
		}
	}

	memcpy(v->cells, cells, sizeof(KtuiCell) * (size_t)w * h);
	v->have_prev = 1;
	kcon_buf_free(&b);
}

/*
 * TELL EVERY VIEW WHAT WAS COPIED, so a view that is itself a terminal can put
 * it on the clipboard of the desktop it is running on.
 *
 * Every view rather than one: a session may be looked at from two places, and
 * a copy that reached only the first is a copy that depends on which display
 * happened to attach first.
 */
/*
 * OUT TO EVERY SHELL SURFACE, and to no other kind.
 *
 * One helper because the four messages differ only in their payload, and
 * because "who is told" is a rule that must be stated once: a plain surface
 * has no business knowing the window list, and a view is a display and asks
 * nothing about what it is showing.
 */
static void mgmt_send(KconServer *s, uint16_t op, KconBuf *b)
{
	if (!s)
		return;
	for (int i = 0; i < s->n; i++) {
		KconSurface *f = s->s[i];

		if (f->kind != KCON_KIND_SHELL)
			continue;
		kcon_send(f->conn, op, b);
	}
}

void kcon_mgmt_add(KconServer *s, unsigned id, const char *app_id,
		   const char *title)
{
	KconBuf b = { 0 };

	kcon_put_u32(&b, id);
	kcon_put_str(&b, app_id ? app_id : "");
	kcon_put_str(&b, title ? title : "");
	mgmt_send(s, KCON_OP_TOPLEVEL_ADD, &b);
	kcon_buf_free(&b);
}

void kcon_mgmt_state(KconServer *s, unsigned id, unsigned flags, int workspace)
{
	KconBuf b = { 0 };

	kcon_put_u32(&b, id);
	kcon_put_u16(&b, (uint16_t)flags);
	kcon_put_u16(&b, (uint16_t)(workspace < 0 ? 0 : workspace));
	mgmt_send(s, KCON_OP_TOPLEVEL_STATE, &b);
	kcon_buf_free(&b);
}

void kcon_mgmt_remove(KconServer *s, unsigned id)
{
	KconBuf b = { 0 };

	kcon_put_u32(&b, id);
	mgmt_send(s, KCON_OP_TOPLEVEL_REMOVE, &b);
	kcon_buf_free(&b);
}

void kcon_mgmt_workspace(KconServer *s, int current, int count,
			 unsigned occupied)
{
	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)(current < 0 ? 0 : current));
	kcon_put_u16(&b, (uint16_t)(count < 0 ? 0 : count));
	kcon_put_u32(&b, occupied);
	mgmt_send(s, KCON_OP_WORKSPACE, &b);
	kcon_buf_free(&b);
}

int kcon_surface_fd(const KconSurface *f)
{
	return f ? kcon_conn_fd(f->conn) : -1;
}

void kcon_view_clip(KconServer *s, const char *text)
{
	if (!s || !text)
		return;

	for (int i = 0; i < s->n; i++) {
		KconSurface *v = s->s[i];
		KconBuf b = { 0 };

		if (v->kind != KCON_KIND_VIEW)
			continue;
		kcon_put_str(&b, text);
		kcon_send(v->conn, KCON_OP_VIEW_CLIP, &b);
		kcon_buf_free(&b);
	}
}

void kcon_view_bell(KconServer *s)
{
	if (!s)
		return;

	for (int i = 0; i < s->n; i++)
		if (s->s[i]->kind == KCON_KIND_VIEW)
			kcon_send(s->s[i]->conn, KCON_OP_BELL, NULL);
}

void kcon_view_cursor(KconSurface *v, int x, int y)
{
	if (!v || v->kind != KCON_KIND_VIEW)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_send(v->conn, KCON_OP_CURSOR, &b);
	kcon_buf_free(&b);
}

unsigned kcon_surface_kind(const KconSurface *f)
{
	return f ? f->kind : (unsigned)KCON_KIND_SURFACE;
}
unsigned kcon_surface_role(const KconSurface *f) { return f ? f->role : 0; }
const char *kcon_surface_app_id(const KconSurface *f) { return f ? f->app_id : ""; }
const char *kcon_surface_title(const KconSurface *f) { return f ? f->title : ""; }
int kcon_surface_cols(const KconSurface *f) { return f ? f->cols : 0; }
int kcon_surface_rows(const KconSurface *f) { return f ? f->rows : 0; }
int kcon_surface_edge(const KconSurface *f) { return f ? f->edge : 0; }
int kcon_surface_want_cells(const KconSurface *f) { return f ? f->want_cells : 0; }
int kcon_surface_hidden(const KconSurface *f)
{
	return f ? f->hidden : 0;
}

int kcon_surface_exclusive(const KconSurface *f) { return f ? f->exclusive : 0; }
const KtuiCell *kcon_surface_cells(const KconSurface *f) { return f ? f->cells : NULL; }

void kcon_surface_configure(KconSurface *f, int cols, int rows)
{
	if (!f || resize(f, cols, rows) != 0)
		return;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)cols);
	kcon_put_u16(&b, (uint16_t)rows);
	kcon_send(f->conn, KCON_OP_CONFIGURE, &b);
	kcon_buf_free(&b);
}

void kcon_surface_key(KconSurface *f, int key, int mods)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, key);
	kcon_put_u8(&b, (uint8_t)mods);
	kcon_send(f->conn, KCON_OP_KEY, &b);
	kcon_buf_free(&b);
}

void kcon_surface_ptr(KconSurface *f, int x, int y, int btn, int press)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_put_u8(&b, (uint8_t)btn);
	kcon_put_u8(&b, (uint8_t)press);
	kcon_send(f->conn, KCON_OP_PTR, &b);
	kcon_buf_free(&b);
}

void kcon_surface_drop(KconSurface *f, int x, int y, const char *text)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_put_str(&b, text);
	kcon_send(f->conn, KCON_OP_DRAG_DROP, &b);
	kcon_buf_free(&b);
}

void kcon_surface_clip_data(KconSurface *f, const char *text)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_str(&b, text);
	kcon_send(f->conn, KCON_OP_CLIP_DATA, &b);
	kcon_buf_free(&b);
}

/*
 * TELL A LOCK SURFACE WHETHER IT HOLDS THE SESSION.
 *
 * Flushed rather than queued: the client refuses every keystroke until this
 * arrives, so a byte sitting in a send buffer is a lock screen that cannot be
 * answered.
 */
void kcon_surface_lock_state(KconSurface *f, unsigned flags)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_u8(&b, (uint8_t)flags);
	kcon_send(f->conn, KCON_OP_LOCK_STATE, &b);
	kcon_buf_free(&b);
	kcon_flush(f->conn);
}

void kcon_surface_close(KconSurface *f)
{
	if (!f)
		return;
	kcon_send(f->conn, KCON_OP_CLOSE, NULL);
	kcon_flush(f->conn);
}
