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
#include <time.h>
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
	/* An overlay's requested corner and its margins from the two edges
	 * that corner names, in cells. See kcon_surface_corner(). */
	int corner, margin_x, margin_y;
	/* The smallest grid this surface can compose on. See
	 * kcon_surface_min_cols(). */
	int min_cols, min_rows;
	/* Asked to open unanchored, where the eye is. See
	 * kcon_surface_floating(). */
	int floating;
	/* Whether this surface asked for the keyboard. See
	 * kcon_surface_keyboard(): an overlay that did not must never be
	 * focused, or it takes the focus off whatever it is drawn over. */
	int keyboard;
	int hidden;

	/*
	 * WHICH OF THIS SURFACE'S CELLS ANSWER THE POINTER, in its own cells.
	 * `in_n < 0` is all of it, which is what every surface starts as; 0 is
	 * none. See kcon_surface_input_n().
	 */
	KRect in_rect[KCON_INPUT_RECTS];
	int in_n;

	/* Where this surface says its caret is, in its own cells. A negative
	 * x is none, which is what a surface with no text field reports and
	 * what every surface reports before it has said anything. See
	 * kcon_surface_caret(). */
	int caret_x, caret_y;

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
	int observe;		/* a view that may watch and not type */
	int a11y;		/* a reader: the one kind sent announcements */

	/*
	 * A SURFACE'S SLOT NUMBERS ARE ITS OWN, and two surfaces both using
	 * slot 0 is the normal case. The session slot is assigned here on
	 * first sight, so a compositing session can rewrite the slot in every
	 * sprite cell it copies out and two pictures never become one.
	 */
	int slotmap[KCON_MAX_SPRITE_MAP];
	/*
	 * THE FILL BUFFERS FOR EVERYTHING THIS SURFACE IS SENT, kept between
	 * messages rather than allocated per message. See KCON_BUF_KEEP: a
	 * sprite block is over a hundred kilobytes and a fullscreen guest is
	 * dozens of them a frame, so a per-message buffer is an mmap and an
	 * munmap per block and the kernel faults in and zeroes every page of
	 * every one of them on every frame.
	 *
	 * PER SURFACE, NEVER FILE-SCOPE. Two surfaces are sent frames in the
	 * same loop and a shared buffer would put one's pixels in the other's
	 * message. They are safe to refill the instant a send returns because
	 * kcon_send copies the payload into the connection's own out queue —
	 * nothing downstream holds a pointer in here.
	 *
	 * THREE, BECAUSE THREE ARE FILLED AT ONCE. `bcells` carries the cells
	 * of a frame and `bcolor` the colour records patching them; the
	 * pictures those cells reference are sent from `bsprite` while the
	 * same frame is being composed. Any two of them sharing one allocation
	 * would have the second filler send the first one's bytes, and the
	 * coupling would be invisible until somebody reordered the walk.
	 *
	 * WHAT THIS COSTS: at most three buffers of KCON_BUF_KEEP per surface,
	 * held until that surface goes, so the ceiling scales with the number
	 * of live surfaces rather than with how much any one of them sends.
	 * `bcells` and `bcolor` only reach what a grid needs, and `bsprite`
	 * only grows for a surface carrying pictures at all.
	 */
	KconBuf bcells, bcolor, bsprite;

	/* A surface's committed grid. For a VIEW this is its PREVIOUS frame
	 * instead — the thing the next send is diffed against. */
	KtuiCell *cells;
	int dirty;
	int have_prev;

	/*
	 * THE CARET TRAVELS WITH THE FRAME IT BELONGS TO. Sent the moment the
	 * session moves it, it arrives before the cells it sits on and the
	 * display parks its cursor on the old picture for a frame — and where
	 * frames are being skipped, for as many frames as are skipped.
	 */
	int cur_x, cur_y, cur_dirty, cur_seen;
	/* The pointer's shape, and whether this view has been told the
	 * current one. `shape_seen` starts 0 so the first frame sends it:
	 * a view that attached mid-session would otherwise draw an arrow
	 * over whatever the pointer is actually on. */
	int shape, shape_dirty, shape_seen;

	/*
	 * THE FRAME CONTRACT, both directions. A surface that sent cells or a
	 * picture since the last composed frame is owed a KCON_OP_FRAME when
	 * the next one is done — a picture counts because an animation
	 * changes pixels and no cells, and a leg answered only for cells
	 * would leave it paced by the stall timeout alone. A view that took a
	 * frame owes one back, and `frame_at` is
	 * when it was sent, so an answer that never comes is given up on
	 * after KCON_FRAME_STALL_MS rather than waited for. See
	 * kcon_view_ready().
	 */
	int committed;
	int frame_owed;
	unsigned long long frame_at;

	int gone;
};

static unsigned long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000 +
	       (unsigned long long)ts.tv_nsec / 1000000;
}

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
	/*
	 * WHICH SESSION SLOTS ARE TAKEN, one bit each.
	 *
	 * The counter alone wrapped onto slots still in use: nothing ever gave
	 * one back, so a session that opened and closed windows for long
	 * enough handed a new surface a number another surface was already
	 * drawing with, and one program's picture appeared inside another's
	 * window. The counter stays as the starting point of the search, so
	 * slots are still handed out in rotation rather than reused the
	 * instant they are freed.
	 */
	unsigned char slot_used[(KCON_MAX_SPRITE_MAP + 7) / 8];
	KconSurface *s[KCON_MAX_CLIENTS];
	int n;
	KconServerHooks hooks;
	void *user;
	/* How many views this session admits at once; 0 is no limit. Set by
	 * the caller from its own configuration. */
	int view_max;
};

/* ── surfaces ────────────────────────────────────────────────────────── */

static void blank(KtuiCell *c, int n)
{
	KtuiCell b = { ' ', KT_TEXT, KT_BG, KT_A_NONE, 0, 0, 0 };

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

static int slot_take(KconServer *s);
static void slot_give(KconServer *s, int n);

/*
 * EVERY DISPLAY FORGETS A PICTURE WHOSE NUMBER IS GOING BACK.
 *
 * A number given back is a number the rotation will hand out again, but not at
 * once: slot_take() takes every free number at or after the rotation point
 * first, so a number freed behind that point waits until the scan wraps onto
 * it. A caller gives back its highest numbers first — the blocks above a grid
 * that shrank — and those are exactly the ones the scan reaches last, a lap of
 * the whole map later.
 *
 * Until it comes round nobody owns the number, so nothing sends a picture
 * under it and nothing replaces what a display is holding under it — and a
 * view keys its sprite table on that number alone. A display that is never
 * told keeps those pixels in its own byte budget until something evicts them;
 * by then the number is the next owner's, and the loss is reported against a
 * slot that owner never lost, spending its repair allowance on somebody else's
 * picture.
 *
 * BEST EFFORT, AND THAT IS ENOUGH. A drop that cannot be queued leaves the
 * display holding a stale picture until the slot's next owner sends its own,
 * which replaces it under the same key — the same state the wire has always
 * had for a display that attached late.
 *
 * NOT CALLED WHILE THE SERVER IS BEING TORN DOWN: the surfaces are freed in
 * one pass there and this walks the very list that pass is emptying.
 */
static void view_forget(KconServer *s, int slot)
{
	KconBuf b = { 0 };

	if (!s || slot < 0 || slot >= KCON_MAX_SPRITE_MAP)
		return;
	kcon_put_u16(&b, (uint16_t)slot);
	for (int i = 0; i < s->n; i++) {
		KconSurface *v = s->s[i];

		if (v->kind != KCON_KIND_VIEW || !v->hello)
			continue;
		kcon_send(v->conn, KCON_OP_SPRITE_DROP, &b);
	}
	kcon_buf_free(&b);
}

static void surface_free(KconSurface *f)
{
	if (!f)
		return;
	/* Every session slot this surface was given goes back, or the
	 * numbering eventually wraps onto one somebody is still drawing. */
	if (f->server)
		for (int i = 0; i < KCON_MAX_SPRITE_MAP; i++)
			if (f->slotmap[i] >= 0)
				slot_give(f->server, f->slotmap[i]);
	kcon_conn_free(f->conn);
	kcon_buf_free(&f->bcells);
	kcon_buf_free(&f->bcolor);
	kcon_buf_free(&f->bsprite);
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

	/*
	 * A VIEW MAY SAY ONLY WHAT A DISPLAY HAS TO SAY, whatever rights it
	 * asked for: its hello, the input it carries, the frame it painted,
	 * what it was pasted, what it can show, and that it is leaving.
	 *
	 * The view socket is the one that may be FORWARDED, so the far end of
	 * it is not this machine and may not be this person. The header's
	 * promise is that a view is trusted with nothing, and that promise
	 * held for keys, pointer and touch and for the shell verbs — but every
	 * other client verb was reached by whoever was on the other end of an
	 * ssh link: offering a clipboard, asking to READ one, starting a drag,
	 * registering pictures in the session's own slot table. An observer,
	 * which exists precisely to be shown a desktop without being given it,
	 * could do all four.
	 *
	 * Stated as what is allowed rather than what is refused: a verb added
	 * later is then refused until somebody decides it belongs, which is
	 * the safe direction for a list whose whole job is to be complete.
	 */
	if (f->kind == KCON_KIND_VIEW) {
		switch (m->op) {
		case KCON_OP_HELLO:
		case KCON_OP_DETACH:
		case KCON_OP_FRAME:
		case KCON_OP_KEY:
		case KCON_OP_PTR:
		case KCON_OP_TOUCH:
		case KCON_OP_KEY_RAW:
		case KCON_OP_PTR_RAW:
		case KCON_OP_AXIS_RAW:
		case KCON_OP_KEYMAP:
		case KCON_OP_PASTE:
		case KCON_OP_VIEW_SIZE:
		case KCON_OP_VIEW_FONTS:
		case KCON_OP_VIEW_OUTPUTS:
		case KCON_OP_SPRITE_LOST:
			break;
		default:
			return;
		}
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
			/* AND WHAT IT MAY DO, if it said. A hello that stops
			 * at the capabilities is a driver: that is what every
			 * view was before an observer existed, and a client
			 * cannot gain rights by saying nothing. */
			if (kcon_rd_left(&r) >= 2 &&
			    kcon_get_u16(&r) == KCON_RIGHTS_OBSERVE && !r.err)
				f->observe = 1;
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

	case KCON_OP_ACTION: {
		/*
		 * A SHELL SURFACE ONLY, the rule every management verb keeps.
		 * The payload is the WHOLE message and carries no terminator,
		 * so it is copied into a bounded buffer rather than read as a
		 * string — a name longer than the buffer is a name no bind
		 * table has, and is dropped.
		 */
		char verb[64];
		size_t n = m->len;

		if (f->kind != KCON_KIND_SHELL || n < 1 || n >= sizeof(verb))
			break;
		memcpy(verb, m->payload, n);
		verb[n] = '\0';
		if (s->hooks.action)
			s->hooks.action(f, verb, s->user);
		break;
	}
	case KCON_OP_ACTIVATE: {
		unsigned id = kcon_get_u32(&r);

		/* A SHELL SURFACE ONLY, the rule every management verb keeps:
		 * a program with a window in the session must not be able to
		 * raise or close another one. */
		if (!r.err && f->kind == KCON_KIND_SHELL && s->hooks.activate)
			s->hooks.activate(f, id, s->user);
		break;
	}
	case KCON_OP_CAPTURE: {
		int win = (int)(int16_t)kcon_get_u16(&r);
		char *txt = NULL;

		/* A SHELL SURFACE ONLY. Reading back a whole session is the
		 * management right this socket exists to gate: a program with
		 * a window in it must not be able to photograph another's. */
		if (r.err || f->kind != KCON_KIND_SHELL)
			break;
		if (s->hooks.capture)
			txt = s->hooks.capture(f, win, s->user);

		KconBuf cb = { 0 };

		/* ANSWERED EVEN WHEN THERE IS NOTHING, because a caller that
		 * asked and heard nothing cannot tell a session with an empty
		 * screen from one that is not listening, and would wait out
		 * its timeout to find out. */
		kcon_put_str(&cb, txt ? txt : "");
		kcon_send(f->conn, KCON_OP_CAPTURE, &cb);
		kcon_buf_free(&cb);
		free(txt);
		break;
	}
	case KCON_OP_CLOSE_REQUEST: {
		unsigned id = kcon_get_u32(&r);

		if (!r.err && f->kind == KCON_KIND_SHELL &&
		    s->hooks.close_request)
			s->hooks.close_request(f, id, s->user);
		break;
	}
	case KCON_OP_WIN_STATE: {
		unsigned id = kcon_get_u32(&r);
		unsigned flag = kcon_get_u16(&r);
		int on = kcon_get_u8(&r) != 0;

		if (!r.err && f->kind == KCON_KIND_SHELL && s->hooks.win_state)
			s->hooks.win_state(f, id, flag, on, s->user);
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
		/*
		 * AS A BLOB, NOT A STRING: the string reader's scratch holds a
		 * kilobyte, and a paste is whatever a person had on the
		 * clipboard. The copy is what terminates it.
		 */
		uint32_t n = kcon_get_u32(&r);
		const char *p = kcon_get_blob(&r, n);

		/*
		 * ONLY FROM A DISPLAY, and only text. A surface with a paste
		 * verb could type into whatever has the focus without the
		 * person touching a key, which is the one thing a client on
		 * this socket must never be able to do.
		 */
		if (r.err || !p || !n || f->kind != KCON_KIND_VIEW)
			return;
		if (s->hooks.paste) {
			char *text = malloc((size_t)n + 1);

			if (!text)
				return;
			memcpy(text, p, n);
			text[n] = '\0';
			s->hooks.paste(f, text, s->user);
			free(text);
		}
		break;
	}

	case KCON_OP_VIEW_FONTS: {
		/*
		 * ONE VERB, ASKED IN TWO DIRECTIONS — the shape
		 * KCON_OP_CAPTURE already uses. A SHELL sending it is asking
		 * for the list; a VIEW sending it is answering with one.
		 *
		 * The names mean nothing on this side: they are shown and
		 * indexed into, never parsed and never sent back.
		 */
		if (f->kind == KCON_KIND_SHELL) {
			if (s->hooks.fonts_ask)
				s->hooks.fonts_ask(f, s->user);
			break;
		}

		int n = (int)kcon_get_u16(&r);
		int cur = (int)(int16_t)kcon_get_u16(&r);

		if (r.err || f->kind != KCON_KIND_VIEW)
			return;
		if (n < 0 || n > KCON_MAX_FONTS)
			n = n < 0 ? 0 : KCON_MAX_FONTS;

		const char *names[KCON_MAX_FONTS];
		/*
		 * COPIED OUT OF THE READER, because kcon_get_str() answers
		 * into ONE static scratch buffer: an array of its return value
		 * is an array of the same pointer, holding the last name n
		 * times.
		 */
		static char store[KCON_MAX_FONTS][KCON_FONT_NAME];
		int got = 0;

		for (int i = 0; i < n; i++) {
			const char *one = kcon_get_str(&r);

			if (r.err)
				break;
			snprintf(store[got], sizeof(store[0]), "%s", one);
			names[got] = store[got];
			got++;
		}
		if (s->hooks.view_fonts)
			s->hooks.view_fonts(f, names, got,
					    cur >= 0 && cur < got ? cur : -1,
					    s->user);
		break;
	}

	case KCON_OP_VIEW_SETFONT: {
		/*
		 * A SHELL ONLY, the rule KCON_OP_RUN keeps: changing the
		 * screen's font is a management verb, and a window that could
		 * send one could resize every other window on the desktop by
		 * changing the cell under them.
		 */
		int idx = (int)(int16_t)kcon_get_u16(&r);
		int keep = (int)kcon_get_u8(&r);

		if (r.err || f->kind != KCON_KIND_SHELL)
			return;
		/* A NEGATIVE INDEX IS "PUT IT BACK", which is what leaving
		 * the picker asks for — the signed read above is what carries
		 * it, so the sentinel is a value and not a magic number. */
		if (s->hooks.font_set)
			s->hooks.font_set(f, idx, keep, s->user);
		break;
	}

	case KCON_OP_VIEW_OUTPUTS: {
		/*
		 * ONE VERB, ASKED IN TWO DIRECTIONS, the shape the font list
		 * uses. A SHELL sending it asks for the screens; a VIEW
		 * sending it answers with them.
		 */
		if (f->kind == KCON_KIND_SHELL) {
			if (s->hooks.outputs_ask)
				s->hooks.outputs_ask(f, s->user);
			break;
		}

		int n = (int)kcon_get_u16(&r);

		if (r.err || f->kind != KCON_KIND_VIEW)
			return;
		if (n < 0 || n > KCON_MAX_OUTS)
			n = n < 0 ? 0 : KCON_MAX_OUTS;

		/* STATIC, because a KconOut is a kilobyte and this runs on the
		 * session's own stack inside its frame loop. One buffer: the
		 * hook is called before the next message is read. */
		static KconOut outs[KCON_MAX_OUTS];
		int got = 0;

		for (int i = 0; i < n; i++) {
			KconOut *o = &outs[got];

			snprintf(o->name, sizeof(o->name), "%s",
				 kcon_get_str(&r));
			o->col = (int)kcon_get_u16(&r);
			o->cols = (int)kcon_get_u16(&r);
			o->width = (int)kcon_get_u16(&r);
			o->height = (int)kcon_get_u16(&r);
			o->cur_mode = (int)(int16_t)kcon_get_u16(&r);
			o->nmodes = (int)kcon_get_u16(&r);
			if (r.err)
				break;
			if (o->nmodes < 0 || o->nmodes > KCON_MAX_MODES)
				o->nmodes = o->nmodes < 0 ? 0
							  : KCON_MAX_MODES;
			for (int m = 0; m < o->nmodes; m++) {
				o->mode[m].width = (int)kcon_get_u16(&r);
				o->mode[m].height = (int)kcon_get_u16(&r);
				o->mode[m].refresh = (int)kcon_get_u32(&r);
			}
			if (r.err)
				break;
			got++;
		}
		if (s->hooks.view_outputs)
			s->hooks.view_outputs(f, outs, got, s->user);
		break;
	}

	case KCON_OP_VIEW_SETMODE: {
		/*
		 * A SHELL ONLY, the rule the font set keeps: a mode change
		 * re-cuts the grid under every window on the desktop.
		 */
		int out = (int)(int16_t)kcon_get_u16(&r);
		int mode = (int)(int16_t)kcon_get_u16(&r);
		int keep = (int)kcon_get_u8(&r);

		if (r.err || f->kind != KCON_KIND_SHELL)
			return;
		if (s->hooks.mode_set)
			s->hooks.mode_set(f, out, mode, keep, s->user);
		break;
	}

	case KCON_OP_VIEW_SIZE: {
		int cols = (int)kcon_get_u16(&r);
		int rows = (int)kcon_get_u16(&r);

		if (r.err || f->kind != KCON_KIND_VIEW)
			return;

		/*
		 * THE CAP IS COUNTED AT THE ATTACH, not at the connection: a
		 * view is a display only once it has said what it can show,
		 * and a connection that never got that far is not one of the
		 * displays a person is looking at. Told why, so a view that
		 * was refused says so instead of reporting a closed socket.
		 */
		if (!f->attached && s->view_max > 0 &&
		    kcon_server_view_count(s) >= s->view_max) {
			KconBuf b = { 0 };

			kcon_put_u16(&b, 0);
			kcon_send(f->conn, KCON_OP_BYE, &b);
			kcon_buf_free(&b);
			f->gone = 1;
			return;
		}
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
		/*
		 * THE CELL, OPTIONALLY AND ONLY AFTER THE GRID. A font step
		 * changes how big a cell is as well as how many fit, and the
		 * cell is what an embedded guest is sized in — so it rides the
		 * message that announces the new grid rather than waiting for
		 * a second hello. A view that sends only the grid keeps the
		 * cell it declared when it arrived.
		 */
		if (kcon_rd_left(&r) >= 4) {
			int cw = (int)kcon_get_u16(&r);
			int chh = (int)kcon_get_u16(&r);

			if (!r.err && cw >= 0 && cw <= 256 && chh >= 0 &&
			    chh <= 256) {
				f->cell_w = cw;
				f->cell_h = chh;
			}
		}

		f->view_cols = cols;
		f->view_rows = rows;
		/*
		 * `cols`/`rows` ARE NOT TOUCHED. For a view those two say how
		 * big the frame in `cells` IS, and nothing here allocates one
		 * — so writing the requested size into them tells
		 * kcon_view_send() that a buffer of the new size already
		 * exists. The first frame at a larger grid is then copied into
		 * the smaller allocation, which is a heap overflow of exactly
		 * the difference and kills the session on the first window
		 * that grows.
		 */
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
		f->corner = (int)kcon_get_u16(&r);
		f->margin_x = (int)kcon_get_u16(&r);
		f->margin_y = (int)kcon_get_u16(&r);
		f->min_cols = (int)kcon_get_u16(&r);
		f->min_rows = (int)kcon_get_u16(&r);
		/* OPTIONALLY AND ONLY AT THE END, the way every other field
		 * that arrived after its message did. */
		f->floating = kcon_rd_left(&r) >= 1 ?
			(int)kcon_get_u8(&r) : 0;
		/*
		 * A PEER THAT DOES NOT SAY TAKES THE KEYBOARD, because that is
		 * what every surface did before the field existed: the default
		 * has to be the old behaviour or an unrebuilt client's menu
		 * stops answering keys.
		 */
		f->keyboard = kcon_rd_left(&r) >= 1 ?
			(int)kcon_get_u8(&r) : 1;

		/*
		 * A SIZE OF ZERO IS A QUESTION, AND ONLY WHERE THE SESSION
		 * OWNS THE ANSWER. A saver covers the screen, the icon layer
		 * IS the screen, and a docked panel spans its edge; none of
		 * them can know how big the screen is, so they ask for nothing
		 * — the panel naming only its thickness — and the session
		 * answers with a CONFIGURE, which is what allocates the cells.
		 *
		 * FROM ANY OTHER ROLE IT IS STILL FATAL. Nothing is going to
		 * tell them a size, so a surface let through would wait for a
		 * configure that never comes — a client hung with no message,
		 * where refusing the attach says which half is wrong.
		 */
		int nosize = cols <= 0 || rows <= 0;
		int asks = f->role == KDISP_ROLE_SAVER ||
			   f->role == KDISP_ROLE_BACKGROUND ||
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
	case KCON_OP_INPUT_REGION: {
		unsigned n = kcon_get_u16(&r);
		KRect got[KCON_INPUT_RECTS];

		if (r.err)
			break;
		/*
		 * A REGION THAT DOES NOT FIT IS ALL OF THE SURFACE, never the
		 * part that fitted: a list cut short leaves the rest of the
		 * surface answering clicks the client said it would not, which
		 * is the failure this op exists to prevent pointed backwards.
		 */
		if (n == 0xffff || n > KCON_INPUT_RECTS) {
			f->in_n = -1;
			break;
		}
		for (unsigned i = 0; i < n; i++) {
			got[i].x = (int)kcon_get_u16(&r);
			got[i].y = (int)kcon_get_u16(&r);
			got[i].w = (int)kcon_get_u16(&r);
			got[i].h = (int)kcon_get_u16(&r);
		}
		if (r.err)
			break;
		for (unsigned i = 0; i < n; i++)
			f->in_rect[i] = got[i];
		f->in_n = (int)n;
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
			/*
			 * THE BOUNDARY IS OWED FOR THE MESSAGE, NOT FOR THE
			 * CELLS THAT SURVIVED THE CLIP. The client arms its
			 * frame wait on the send; a commit whose every run
			 * falls outside the surface — which is what a resize
			 * in flight looks like — would otherwise be answered
			 * by nothing, and the client would pay the whole
			 * KCON_FRAME_STALL_MS before drawing again. `dirty`
			 * stays where it is: it means cells moved, and none
			 * did.
			 */
			f->committed = 1;
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
	case KCON_OP_COLOR: {
		/*
		 * THE LITERALS A SURFACE'S CELLS NAMED, patched over the
		 * commit they follow: the slot run has already landed, so this
		 * writes only the high attribute bits and the three colours,
		 * and clips exactly as the commit did. Without it a terminal
		 * on the console shows every colour outside the sixteen
		 * reduced to eight slots while the same program in a session
		 * window does not.
		 */
		if (f->kind == KCON_KIND_VIEW)
			return;
		while (r.pos < r.len && !r.err) {
			uint16_t x, y;
			KtuiCell run[4096];
			int n = kcon_get_color_run(&r, &x, &y, run, 4096);

			if (n < 0)
				break;
			/* Owed for the message, as the commit is: see the
			 * KCON_OP_COMMIT case. */
			f->committed = 1;
			if ((int)y >= f->rows)
				continue;
			for (int i = 0; i < n; i++) {
				int cx = (int)x + i;

				if (cx >= f->cols)
					break;

				KtuiCell *c = &f->cells[(int)y * f->cols + cx];

				c->attr = (uint16_t)((c->attr & 0xffu) |
						     (run[i].attr & ~0xffu));
				c->fgc = run[i].fgc;
				c->bgc = run[i].bgc;
				c->ulc = run[i].ulc;
			}
			f->dirty = 1;
		}
		break;
	}
	case KCON_OP_FRAME:
		/* A view has painted the frame it was sent. From anything
		 * else it means nothing and is dropped. */
		if (f->kind == KCON_KIND_VIEW)
			f->frame_owed = 0;
		break;
	case KCON_OP_KEY:
		/* Only a view sends input. A surface doing so is talking the
		 * wrong direction and is ignored rather than trusted.
		 *
		 * AND NOT AN OBSERVER. Over a forwarded socket that is the
		 * difference between showing somebody a problem and handing
		 * them the machine, so it is refused HERE — a view that asked
		 * to observe and then sent a key is exactly the case the field
		 * exists for, and trusting the client to keep its own promise
		 * would make the promise decorative. */
		if (f->kind == KCON_KIND_VIEW && !f->observe &&
		    s->hooks.view_key) {
			int key = kcon_get_i32(&r);
			int mods = (int)kcon_get_u8(&r);

			if (!r.err)
				s->hooks.view_key(f, key, mods, s->user);
		}
		break;
	case KCON_OP_PTR:
		if (f->kind == KCON_KIND_VIEW && !f->observe &&
		    s->hooks.view_ptr) {
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
	case KCON_OP_TOUCH:
		/*
		 * A FINGER, AND WHAT THE RECOGNISER MADE OF IT. The gesture is
		 * decided at the VIEW — one recogniser, fed by whichever
		 * backend has the touch device — so what crosses is its
		 * verdict and not the raw geometry a second recogniser here
		 * would disagree with.
		 *
		 * The same guard KCON_OP_PTR keeps: a display only, and never
		 * one that attached to watch.
		 */
		if (f->kind == KCON_KIND_VIEW && !f->observe &&
		    s->hooks.view_touch) {
			int x = kcon_get_i32(&r);
			int y = kcon_get_i32(&r);
			int slot = (int)kcon_get_u8(&r);
			int phase = (int)kcon_get_u8(&r);
			unsigned ms = kcon_get_u32(&r);
			int gest = (int)kcon_get_u8(&r);

			if (!r.err)
				s->hooks.view_touch(f, x, y, slot, phase, ms,
						    gest, s->user);
		}
		break;
	case KCON_OP_KEY_RAW: {
		/*
		 * THE SWITCH BESIDE THE CHARACTER, for the one thing on this
		 * desktop that is not cells. The cooked KCON_OP_KEY for the
		 * same physical key has already been delivered, so the session
		 * has decided whether a chord ate it before this arrives.
		 *
		 * The same guard KCON_OP_KEY keeps: a display only, and never
		 * one that attached to watch. AND ONLY FROM A VIEW THAT SAID
		 * IT HAS THE DEVICE — a view without KCON_VIEW_RAW is never
		 * asked for any of this, so one that sends it is reporting a
		 * keyboard it told the session it does not have. The three
		 * arms below keep the same gate.
		 */
		KconKeyRaw k = { 0 };
		unsigned code;

		if (f->kind != KCON_KIND_VIEW || f->observe ||
		    !(f->caps & KCON_VIEW_RAW) || !s->hooks.view_key_raw)
			break;
		code = kcon_get_u16(&r);
		k.state = kcon_get_u8(&r) ? 1 : 0;
		k.depressed = kcon_get_u32(&r);
		k.latched = kcon_get_u32(&r);
		k.locked = kcon_get_u32(&r);
		k.group = kcon_get_u32(&r);
		k.ms = kcon_get_u32(&r);
		/*
		 * A KEYCODE FROM A PEER IS AN INDEX. What holds a bit per key
		 * — the presses a window is owed a release for — is sized from
		 * KCON_KEYCODE_MAX, so a higher code is a write outside it.
		 */
		if (r.err || code > KCON_KEYCODE_MAX)
			break;
		k.code = (int)code;
		s->hooks.view_key_raw(f, &k, s->user);
		break;
	}
	case KCON_OP_PTR_RAW: {
		KconPtrRaw p = { 0 };
		unsigned cw, ch, btn;

		if (f->kind != KCON_KIND_VIEW || f->observe ||
		    !(f->caps & KCON_VIEW_RAW) || !s->hooks.view_ptr_raw)
			break;
		p.x = kcon_get_i32(&r);
		p.y = kcon_get_i32(&r);
		cw = kcon_get_u16(&r);
		ch = kcon_get_u16(&r);
		p.dx = kcon_get_i32(&r);
		p.dy = kcon_get_i32(&r);
		p.dx_un = kcon_get_i32(&r);
		p.dy_un = kcon_get_i32(&r);
		btn = kcon_get_u16(&r);
		p.state = kcon_get_u8(&r) ? 1 : 0;
		p.mods = (int)kcon_get_u8(&r);
		p.ms = kcon_get_u32(&r);
		/*
		 * A CELL SIZE IS A DIVISOR and a button is an index. The
		 * session divides by the cell to derive the same cell the view
		 * would have, so a zero is refused here rather than faulting
		 * one caller later; a button shares the key number space and
		 * is bounded with it.
		 */
		if (r.err || !cw || !ch || btn > KCON_KEYCODE_MAX)
			break;
		p.cell_w = (int)cw;
		p.cell_h = (int)ch;
		p.button = (int)btn;
		s->hooks.view_ptr_raw(f, &p, s->user);
		break;
	}
	case KCON_OP_AXIS_RAW: {
		KconAxisRaw a = { 0 };
		unsigned axis, src;

		if (f->kind != KCON_KIND_VIEW || f->observe ||
		    !(f->caps & KCON_VIEW_RAW) || !s->hooks.view_axis_raw)
			break;
		a.value = kcon_get_i32(&r);
		a.value120 = kcon_get_i32(&r);
		axis = kcon_get_u8(&r);
		src = kcon_get_u8(&r);
		a.flags = (int)(kcon_get_u8(&r) & KCON_AXIS_INVERTED);
		a.mods = (int)kcon_get_u8(&r);
		a.ms = kcon_get_u32(&r);
		/*
		 * AN AXIS OR A SOURCE OUTSIDE ITS ENUM IS REFUSED. The far end
		 * maps both in a switch, and a scroll whose direction it had
		 * to guess is a page that moves the wrong way.
		 */
		if (r.err || axis > KCON_AXIS_HORIZ || src >= KCON_AXIS_SRC_N)
			break;
		a.axis = (int)axis;
		a.source = (int)src;
		s->hooks.view_axis_raw(f, &a, s->user);
		break;
	}
	case KCON_OP_KEYMAP: {
		/*
		 * WHAT THIS VIEW'S KEYBOARD IS RUNNING, as bytes. No
		 * descriptor crosses this socket — that is what lets a view be
		 * forwarded — so the text itself travels and the session seals
		 * its own copy of it.
		 */
		int fmt;
		uint32_t n;
		const char *text;

		if (f->kind != KCON_KIND_VIEW || f->observe ||
		    !(f->caps & KCON_VIEW_RAW) || !s->hooks.view_keymap)
			break;
		fmt = (int)kcon_get_u8(&r);
		n = kcon_get_u32(&r);
		/*
		 * A LENGTH FROM A PEER IS AN ALLOCATION REQUEST, and this peer
		 * may be a machine at the other end of an ssh link. A format
		 * neither end compiles is refused with it: the bytes would be
		 * handed to an xkb compiler that reads only this one.
		 */
		if (r.err || fmt != KCON_KEYMAP_XKB_V1 || !n ||
		    n > KCON_KEYMAP_MAX)
			break;
		text = kcon_get_blob(&r, n);
		/*
		 * AND A COMPILER READS TO A NUL. `n` counts the terminator, so
		 * a text that does not end in one is a text the consumer would
		 * read past the end of.
		 */
		if (r.err || !text || text[n - 1] != '\0')
			break;
		s->hooks.view_keymap(f, fmt, text, n, s->user);
		break;
	}
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

		/*
		 * A PICTURE OWES ITS SENDER A BOUNDARY, exactly as a commit
		 * does, and it is owed before the message is judged: the
		 * client arms its frame wait on the send and not on
		 * acceptance, so a sprite dropped below for a bad size or an
		 * empty slot pool must still be answered. An animation
		 * changes pixels and no cells, so a picture-only frame that
		 * went unanswered here would wait out KCON_FRAME_STALL_MS on
		 * every tick, and any text the surface committed within it
		 * would wait behind the same stall.
		 */
		f->committed = 1;

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
		 * Taken from the server's own free map, so it is unique across
		 * every client rather than only within one — and given back
		 * when the surface goes, so the numbering cannot wrap onto a
		 * slot somebody is still drawing with. */
		if (f->slotmap[cslot] < 0)
			f->slotmap[cslot] = slot_take(s);
		if (f->slotmap[cslot] < 0)
			break;		/* none left: the fallback mark */

		if (s->hooks.sprite)
			s->hooks.sprite(f, f->slotmap[cslot], cw, ch,
					fallback, argb, pw, ph, s->user);
		break;
	}
	case KCON_OP_SPRITE_DROP: {
		/*
		 * A PICTURE THE CLIENT HAS FINISHED WITH. The slot goes back
		 * to the session's rotation; without this nothing was ever
		 * given back and the counter wrapped onto slots in use.
		 */
		int cslot = (int)kcon_get_u16(&r);

		if (r.err || cslot < 0 || cslot >= KCON_MAX_SPRITE_MAP)
			break;
		if (f->slotmap[cslot] >= 0) {
			view_forget(s, f->slotmap[cslot]);
			slot_give(s, f->slotmap[cslot]);
			f->slotmap[cslot] = -1;
		}
		break;
	}
	case KCON_OP_SPRITE_LOST: {
		/*
		 * PICTURES A DISPLAY COULD NOT KEEP, so whatever owns them can
		 * owe them again. One message carries the whole set the view
		 * refused since it last presented; see KCON_OP_SPRITE_LOST.
		 */
		unsigned n = kcon_get_u16(&r);

		if (r.err || f->kind != KCON_KIND_VIEW)
			return;
		/*
		 * A COUNT FROM A PEER IS A LOOP BOUND. No display can lose
		 * more slots than the map holds, so a larger claim is a peer
		 * spending this loop rather than reporting a loss.
		 */
		if (n > KCON_MAX_SPRITE_MAP)
			n = KCON_MAX_SPRITE_MAP;

		for (unsigned i = 0; i < n; i++) {
			unsigned slot = kcon_get_u16(&r);

			if (r.err)
				break;
			/*
			 * AND A SLOT FROM A PEER IS AN INDEX. The hook hands
			 * it straight to a table of the session's own; one
			 * past the map is a write outside it.
			 */
			if (slot >= KCON_MAX_SPRITE_MAP)
				continue;
			if (s->hooks.view_sprite_lost)
				s->hooks.view_sprite_lost(f, (int)slot,
							  s->user);
		}
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

	case KCON_OP_LAYOUT: {
		/*
		 * A NAME AND A DIRECTION, and nothing else on the wire. The
		 * session resolves the name to a file and the file's rows to
		 * roles con.conf names, so what a peer can ask for here is
		 * "put back one of this person's own arrangements" and never
		 * "start this".
		 */
		char nbuf[64];
		int save;

		snprintf(nbuf, sizeof(nbuf), "%s", kcon_get_str(&r));
		save = (int)kcon_get_u16(&r) != 0;
		if (r.err || !nbuf[0])
			break;

		int done = -1;

		/* A SHELL SURFACE ONLY, the rule KCON_OP_RUN keeps. */
		if (f->kind == KCON_KIND_SHELL && s->hooks.layout)
			done = s->hooks.layout(f, nbuf, save, s->user);

		KconBuf b = { 0 };

		kcon_put_u16(&b, done >= 0 ? 1 : 0);
		kcon_put_u16(&b, (uint16_t)(done > 0 ? done : 0));
		kcon_send(f->conn, KCON_OP_RUN_REPLY, &b);
		kcon_buf_free(&b);
		break;
	}

	case KCON_OP_PICK:
		/*
		 * NOTHING ON THE WIRE AND NOTHING BACK. The session draws the
		 * prompt, reads the click and puts the answer on its own
		 * clipboard; a reply would mean holding this connection open
		 * for as long as somebody hesitates.
		 *
		 * A SHELL SURFACE ONLY, the rule KCON_OP_RUN keeps.
		 */
		if (f->kind == KCON_KIND_SHELL && s->hooks.pick)
			s->hooks.pick(f, s->user);
		break;

	case KCON_OP_CARET: {
		/*
		 * WHERE THE CARET IS, in this surface's own cells. Stored and
		 * never forwarded from here: the session publishes the FOCUSED
		 * window's, and it is the only thing that knows which that is
		 * or where the window sits.
		 */
		int cx = (int)(int16_t)kcon_get_u16(&r);
		int cy = (int)(int16_t)kcon_get_u16(&r);

		if (r.err)
			break;
		f->caret_x = cx;
		f->caret_y = cy;
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
	if (kind == KCON_LISTEN_VIEW || kind == KCON_LISTEN_A11Y)
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
	/*
	 * THE DISPLAYS FORGET THIS SURFACE'S PICTURES BEFORE ITS NUMBERS GO
	 * BACK. surface_free() returns them to the rotation, and a number back
	 * in the rotation is a number the next surface is given.
	 */
	for (int k = 0; k < KCON_MAX_SPRITE_MAP; k++)
		if (f->slotmap[k] >= 0)
			view_forget(s, f->slotmap[k]);
	surface_free(f);
	s->s[i] = s->s[--s->n];
}

int kcon_server_pump(KconServer *s)
{
	if (!s)
		return 0;

	for (int li = 0; li < s->nl; li++) {
		for (;;) {
			/* Close-on-exec from birth: the session forks guests,
			 * and a client socket inherited by one is a peer that
			 * never hangs up. */
			int fd = accept4(s->l[li].fd, NULL, NULL,
					 SOCK_CLOEXEC | SOCK_NONBLOCK);

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
			/* NO CARET UNTIL ONE IS SENT. Zero is a cell, and a
			 * surface that never says would otherwise park the
			 * screen's cursor in its top-left corner. */
			f->caret_x = f->caret_y = -1;
			/* ALL OF IT UNTIL IT SAYS OTHERWISE, which is what
			 * wl_surface's NULL input region means and what every
			 * surface that never calls the op needs. */
			f->in_n = -1;
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
			} else if (s->l[li].kind == KCON_LISTEN_A11Y) {
				/*
				 * A READER IS A VIEW THAT MAY NOT DRIVE, and
				 * that is the socket's decision rather than
				 * the client's: rights it could choose would
				 * make this socket the same as the view one
				 * with a flag, and the whole point of a third
				 * path is that reaching it grants less.
				 */
				f->kind = KCON_KIND_VIEW;
				f->kind_fixed = 1;
				f->observe = 1;
				f->a11y = 1;

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

		/* Counted and cleared: the answer is what changed since the
		 * last pump, or a surface that ever drew counts for ever. */
		if (f->dirty) {
			changed++;
			f->dirty = 0;
		}
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

int kcon_server_a11y_count(const KconServer *s)
{
	int n = 0;

	for (int i = 0; s && i < s->n; i++)
		if (s->s[i]->a11y && s->s[i]->attached)
			n++;
	return n;
}

void kcon_a11y_announce(KconServer *s, int role, const char *label,
			const char *value, int index, int count, int x, int y,
			int w, int h)
{
	KconBuf b = { 0 };
	int any = 0;

	if (!s)
		return;
	for (int i = 0; i < s->n; i++)
		if (s->s[i]->a11y && s->s[i]->attached)
			any = 1;
	/* NOTHING IS BUILT WHEN NOBODY IS READING. A desktop with no reader
	 * attached — which is nearly every desktop — pays this comparison and
	 * not a message. */
	if (!any)
		return;

	kcon_put_u16(&b, (uint16_t)role);
	kcon_put_u16(&b, (uint16_t)(index < 0 ? 0 : index));
	kcon_put_u16(&b, (uint16_t)(count < 0 ? 0 : count));
	kcon_put_u16(&b, (uint16_t)(int16_t)x);
	kcon_put_u16(&b, (uint16_t)(int16_t)y);
	kcon_put_u16(&b, (uint16_t)(int16_t)w);
	kcon_put_u16(&b, (uint16_t)(int16_t)h);
	kcon_put_str(&b, label ? label : "");
	kcon_put_str(&b, value ? value : "");

	for (int i = 0; i < s->n; i++)
		if (s->s[i]->a11y && s->s[i]->attached)
			kcon_send(s->s[i]->conn, KCON_OP_ANNOUNCE, &b);
	kcon_buf_free(&b);
}

int kcon_view_observing(const KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW && v->observe;
}

void kcon_server_view_max(KconServer *s, int n)
{
	if (s)
		s->view_max = n > 0 ? n : 0;
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
/* The lowest free slot at or after the rotation point, or -1 when every one is
 * taken — the caller then draws the fallback mark, which is what a picture
 * that could not be sent has always looked like. */
static int slot_take(KconServer *s)
{
	for (unsigned i = 0; i < KCON_MAX_SPRITE_MAP; i++) {
		unsigned n = (s->next_slot + i) % KCON_MAX_SPRITE_MAP;

		if (!(s->slot_used[n / 8] & (1u << (n % 8)))) {
			s->slot_used[n / 8] |= (unsigned char)(1u << (n % 8));
			s->next_slot = n + 1;
			return (int)n;
		}
	}
	return -1;
}

static void slot_give(KconServer *s, int n)
{
	if (n >= 0 && n < KCON_MAX_SPRITE_MAP)
		s->slot_used[n / 8] &= (unsigned char)~(1u << (n % 8));
}

int kcon_server_alloc_slot(KconServer *s)
{
	if (!s)
		return -1;
	return slot_take(s);
}

void kcon_server_free_slot(KconServer *s, int slot)
{
	if (!s)
		return;
	view_forget(s, slot);
	slot_give(s, slot);
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
int kcon_view_sprite(KconSurface *v, int slot, int w, int h,
		     uint32_t fallback, const uint32_t *argb, int pw, int ph)
{
	if (!v || v->kind != KCON_KIND_VIEW || slot < 0 ||
	    slot >= KCON_MAX_SPRITE_MAP)
		return 0;

	/*
	 * A DISPLAY THAT IS BEHIND IS OFFERED THE PICTURE AGAIN, NOT SENT IT
	 * NOW. One block is over a hundred kilobytes and a window is dozens of
	 * them; queued without asking, a single whole-window repaint reaches
	 * KCON_MAX_QUEUE and the session drops the display it is drawing on.
	 */
	if (kcon_conn_pending(v->conn) > KCON_VIEW_HIGH)
		return 0;

	/*
	 * THE SURFACE'S OWN BUFFER, EMPTIED FIRST AND NEVER COPIED OUT OF IT.
	 * See KconSurface::bsprite: the pixels of a block are the one payload
	 * on this wire large enough that allocating for it is an mmap, and
	 * this is the path a fullscreen guest takes dozens of times a frame.
	 * Held by pointer so that no exit from here can leave a second owner
	 * of the same allocation behind.
	 */
	KconBuf *b = &v->bsprite;
	size_t npx = (argb && pw > 0 && ph > 0)
		     ? (size_t)pw * (size_t)ph : 0;

	kcon_buf_reset(b);
	kcon_put_u16(b, (uint16_t)slot);
	kcon_put_u16(b, (uint16_t)w);
	kcon_put_u16(b, (uint16_t)h);
	kcon_put_u32(b, fallback);
	kcon_put_u16(b, (uint16_t)(npx ? pw : 0));
	kcon_put_u16(b, (uint16_t)(npx ? ph : 0));
	if (npx)
		kcon_put_bytes(b, argb, npx * 4);

	/*
	 * WHAT THE SEND DID, NOT WHAT IT WAS ASKED TO DO. A picture has no
	 * previous copy to diff against, so a caller that took an unsent
	 * block for a sent one would clear its dirty bit and never offer the
	 * tile again — the cell shows the fallback mark for the life of the
	 * window with nothing in any log. A block whose pixels pass
	 * KCON_MAX_PAYLOAD fails here exactly as a full queue does, so the
	 * side that CHOOSES the pixel size is the side that must keep a tile
	 * inside the cap.
	 */
	int rc = kcon_send(v->conn, KCON_OP_SPRITE, b);

	kcon_buf_retire(b, KCON_BUF_KEEP);
	return rc == 0;
}

size_t kcon_view_pending(const KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW ? kcon_conn_pending(v->conn) : 0;
}

int kcon_view_flush(KconSurface *v)
{
	return v && v->kind == KCON_KIND_VIEW ? kcon_flush(v->conn) : -1;
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

void kcon_surface_decorated(KconSurface *f, int on)
{
	if (!f || f->kind == KCON_KIND_VIEW)
		return;

	KconBuf b = { 0 };

	kcon_put_u8(&b, (uint8_t)(on ? 1 : 0));
	kcon_send(f->conn, KCON_OP_DECORATED, &b);
	kcon_buf_free(&b);
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

/*
 * ASK THIS VIEW FOR RAW INPUT, or stop. Silently nothing on a view that did
 * not claim KCON_VIEW_RAW, the rule every other capability-gated call keeps —
 * a display inside somebody's terminal has no device to report and is never
 * asked.
 *
 * ASKED ONLY WHILE SOMETHING READS IT: the stream is one message per device
 * event, so a session that left it on with no pixel guest focused would spend
 * a display's link on input nothing consumes.
 */
void kcon_view_raw(KconSurface *v, int on)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_RAW))
		return;

	KconBuf b = { 0 };

	kcon_put_u8(&b, (uint8_t)(on ? 1 : 0));
	kcon_send(v->conn, KCON_OP_VIEW_RAW, &b);
	kcon_buf_free(&b);
}

void kcon_view_font(KconSurface *v, int step)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_FONT))
		return;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)(int16_t)(step > 0 ? 1 : step < 0 ? -1 : 0));
	kcon_send(v->conn, KCON_OP_VIEW_FONT, &b);
	kcon_buf_free(&b);
}

void kcon_surface_fonts(KconSurface *f, const char *const *names, int n,
			int cur)
{
	if (!f)
		return;
	if (n < 0)
		n = 0;
	if (n > KCON_MAX_FONTS)
		n = KCON_MAX_FONTS;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)n);
	kcon_put_u16(&b, (uint16_t)(int16_t)(cur >= 0 && cur < n ? cur : -1));
	for (int i = 0; i < n; i++)
		kcon_put_str(&b, names && names[i] ? names[i] : "");
	kcon_send(f->conn, KCON_OP_VIEW_FONTS, &b);
	kcon_buf_free(&b);
}

void kcon_view_outputs_ask(KconSurface *v)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_FONT))
		return;

	KconBuf b = { 0 };

	kcon_send(v->conn, KCON_OP_VIEW_OUTPUTS, &b);
	kcon_buf_free(&b);
}

void kcon_view_set_mode(KconSurface *v, int out, int mode, int keep)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_FONT))
		return;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)(int16_t)out);
	kcon_put_u16(&b, (uint16_t)(int16_t)mode);
	kcon_put_u8(&b, (uint8_t)(keep ? 1 : 0));
	kcon_send(v->conn, KCON_OP_VIEW_SETMODE, &b);
	kcon_buf_free(&b);
}

void kcon_surface_outputs(KconSurface *f, const KconOut *outs, int n)
{
	if (!f)
		return;
	if (n < 0)
		n = 0;
	if (n > KCON_MAX_OUTS)
		n = KCON_MAX_OUTS;

	KconBuf b = { 0 };

	kcon_put_u16(&b, (uint16_t)n);
	for (int i = 0; i < n; i++) {
		const KconOut *o = &outs[i];
		int nm = o->nmodes < 0 ? 0
		       : o->nmodes > KCON_MAX_MODES ? KCON_MAX_MODES
						    : o->nmodes;

		kcon_put_str(&b, o->name);
		kcon_put_u16(&b, (uint16_t)o->col);
		kcon_put_u16(&b, (uint16_t)o->cols);
		kcon_put_u16(&b, (uint16_t)o->width);
		kcon_put_u16(&b, (uint16_t)o->height);
		kcon_put_u16(&b, (uint16_t)(int16_t)o->cur_mode);
		kcon_put_u16(&b, (uint16_t)nm);
		for (int m = 0; m < nm; m++) {
			kcon_put_u16(&b, (uint16_t)o->mode[m].width);
			kcon_put_u16(&b, (uint16_t)o->mode[m].height);
			kcon_put_u32(&b, (uint32_t)o->mode[m].refresh);
		}
	}
	kcon_send(f->conn, KCON_OP_VIEW_OUTPUTS, &b);
	kcon_buf_free(&b);
}

void kcon_view_fonts_ask(KconSurface *v)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_FONT))
		return;

	KconBuf b = { 0 };

	kcon_send(v->conn, KCON_OP_VIEW_FONTS, &b);
	kcon_buf_free(&b);
}

void kcon_view_set_font(KconSurface *v, int index, int keep)
{
	if (!v || v->kind != KCON_KIND_VIEW || !(v->caps & KCON_VIEW_FONT))
		return;

	KconBuf b = { 0 };

	/* 0xffff IS "THE ONE YOU STARTED WITH", which is what leaving the
	 * picker asks for. Any other out-of-range index is the view's to
	 * refuse, because only the view knows how long its own list is. */
	kcon_put_u16(&b, (uint16_t)(index < 0 ? 0xffff : index));
	kcon_put_u8(&b, (uint8_t)(keep ? 1 : 0));
	kcon_send(v->conn, KCON_OP_VIEW_SETFONT, &b);
	kcon_buf_free(&b);
}

void kcon_view_send(KconSurface *v, const KtuiCell *cells, int w, int h)
{
	if (!v || v->kind != KCON_KIND_VIEW || !cells || w <= 0 || h <= 0)
		return;

	/*
	 * A READER IS NOT A DISPLAY. An accessibility listener is a view kind
	 * so that it is counted, gated and detached like one, but it draws
	 * nothing and reads only KCON_OP_ANNOUNCE. Handing it the frame would
	 * push a full-screen animation's whole diff down a second socket to
	 * be discarded, and would allocate it a copy of the grid to diff
	 * against that nothing ever looks at.
	 */
	if (v->a11y)
		return;

	/*
	 * A FRAME IS SKIPPED WHOLE WHERE THE DISPLAY IS NOT READY FOR ONE,
	 * and its copy of the previous frame is left exactly as it was.
	 *
	 * That is what makes skipping safe: the diff below is taken against
	 * that copy, so everything this frame would have carried is still
	 * pending and goes out with the next frame the view can take. A
	 * display is a stream of pictures and the newest one makes the others
	 * pointless — queueing them instead is how a full-screen animation in
	 * a terminal fills KCON_MAX_QUEUE and the session drops the only
	 * display, and with it the only source of input it has.
	 *
	 * Not ready covers the view that still owes the frame it was last
	 * sent, and that is what makes the KCON_OP_FRAME contract pace EVERY
	 * attached display rather than the quickest one: a session composes
	 * as soon as any view answers, so a second display sent a frame on
	 * each of those composites would have its answer reset before it
	 * could give one and its queue would grow until whole frames were
	 * lost. kcon_view_ready()'s stall clause keeps a display that never
	 * answers from being starved instead.
	 */
	if (!kcon_view_ready(v))
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

	/*
	 * ONE MESSAGE FOR THE CELLS AND ONE FOR THE COLOURS PER CHUNK,
	 * however many runs the frame has. A run was a message and a message
	 * is a socket write, so an animation whose every row changed cost a
	 * syscall per row per frame at this hop and again at the display. A
	 * run here is appended, and the buffers go out once the grid has been
	 * walked — or sooner, when the next run would take one past
	 * KCON_CHUNK_BYTES.
	 *
	 * THE CHUNK IS WHAT LETS A FRAME BE LARGER THAN A MESSAGE. A grid may
	 * be up to 4096x4096 and a whole frame of one is tens of megabytes,
	 * far past the KCON_MAX_PAYLOAD a length field is checked against; an
	 * unchunked frame would simply fail to encode, silently, at every
	 * size past about 131k cells.
	 *
	 * BOTH BUFFERS GO OUT TOGETHER AND CELLS GO FIRST. A colour record
	 * patches a cell the commit already placed, so a COLOR message that
	 * overtook the COMMIT carrying its cells would be undone by it.
	 *
	 * The previous-frame copy is updated run by run, not by copying the
	 * whole grid afterwards: the runs are exactly the cells that differ.
	 * A run is recorded only once it is IN a buffer, and the copy is
	 * disowned outright if any send fails, because a copy claiming cells
	 * the display never got is a screen that stays wrong until something
	 * else happens to overwrite it.
	 *
	 * THE BUFFERS ARE THE SURFACE'S OWN AND ARE EMPTIED, NOT ALLOCATED.
	 * See KconSurface::bcells: a chunk is a quarter of a megabyte, so a
	 * pair allocated per frame is two mmaps and two munmaps at the
	 * session's tick rate. They are held by pointer so that no exit from
	 * this function can leave a second owner of either allocation behind.
	 */
	KconBuf *b = &v->bcells, *cb = &v->bcolor;
	int want_color = (v->caps & KCON_VIEW_COLOR) != 0;
	int any = 0;

	kcon_buf_reset(b);
	kcon_buf_reset(cb);

	for (int y = 0; y < h; y++) {
		const KtuiCell *row = cells + (size_t)y * w;
		KtuiCell *prow = v->cells + (size_t)y * w;
		int x = 0;

		/* A row that did not change is one memcmp, not w of them. */
		if (!full && !memcmp(row, prow, sizeof(KtuiCell) * (size_t)w))
			continue;

		while (x < w) {
			if (!full && !memcmp(&row[x], &prow[x],
					     sizeof(KtuiCell))) {
				x++;
				continue;
			}

			int start = x;

			while (x < w && (full || memcmp(&row[x], &prow[x],
							sizeof(KtuiCell))))
				x++;

			uint16_t n = (uint16_t)(x - start);

			if (b->len + 6 + (size_t)n * KCON_CELL_BYTES >
				KCON_CHUNK_BYTES ||
			    cb->len + 6 + (size_t)n * KCON_COLOR_BYTES >
				KCON_CHUNK_BYTES) {
				if (b->len && kcon_send(v->conn,
							KCON_OP_COMMIT,
							b) != 0)
					goto fail;
				kcon_buf_reset(b);
				if (cb->len && kcon_send(v->conn,
							 KCON_OP_COLOR,
							 cb) != 0)
					goto fail;
				kcon_buf_reset(cb);
			}
			if (kcon_put_run(b, (uint16_t)start, (uint16_t)y,
					 &row[start], n) != 0)
				goto fail;
			/*
			 * AND THE LITERALS, ONLY IF THIS VIEW ASKED AND ONLY
			 * IF THIS RUN HAS ANY. It repeats the position of the
			 * run it patches, so a view that declined has the
			 * slots and is a frame behind nothing.
			 */
			if (want_color && kcon_run_has_color(&row[start], n) &&
			    kcon_put_color_run(cb, (uint16_t)start,
					       (uint16_t)y, &row[start],
					       n) != 0)
				goto fail;
			memcpy(&prow[start], &row[start],
			       sizeof(KtuiCell) * (size_t)n);
			any = 1;
		}
	}

	if (b->len && kcon_send(v->conn, KCON_OP_COMMIT, b) != 0)
		goto fail;
	if (cb->len && kcon_send(v->conn, KCON_OP_COLOR, cb) != 0)
		goto fail;
	v->have_prev = 1;

	/*
	 * BEFORE THE CARET AND FOR THE SAME REASON THE CARET IS BEHIND THE
	 * CELLS: a shape is about what the pointer is over, and the cells
	 * that say so have just been sent. Sending it first would have the
	 * view draw a resize arrow over a border that has not arrived yet.
	 */
	if (v->shape_dirty) {
		KconBuf sb = { 0 };

		kcon_put_u8(&sb, (uint8_t)v->shape);
		if (kcon_send(v->conn, KCON_OP_PTRSHAPE, &sb) == 0)
			v->shape_dirty = 0;
		kcon_buf_free(&sb);
	}

	/* Behind the cells, so the caret is never on a picture that has not
	 * arrived, and inside the same skip rule. */
	if (v->cur_dirty) {
		KconBuf pb = { 0 };

		kcon_put_i32(&pb, v->cur_x);
		kcon_put_i32(&pb, v->cur_y);
		if (kcon_send(v->conn, KCON_OP_CURSOR, &pb) == 0)
			v->cur_dirty = 0;
		kcon_buf_free(&pb);
	}

	/*
	 * THE BOUNDARY, ONLY BEHIND A FRAME THAT SENT SOMETHING: a view opens
	 * a frame on the first run it receives and presents on this, so a
	 * frame that changed nothing has nothing to close — and sending one
	 * anyway would have an idle desktop and its display exchanging eight
	 * bytes each way at the session's tick rate.
	 */
	if (any && (v->caps & KCON_VIEW_FRAME)) {
		if (kcon_send(v->conn, KCON_OP_FRAME, NULL) != 0)
			goto fail;
		v->frame_owed = 1;
		v->frame_at = now_ms();
	}
	goto out;
fail:
	/*
	 * A FRAME THAT DID NOT GO WHOLE LEAVES NOTHING CLAIMED. The next
	 * frame is then encoded full, against a copy the display is known not
	 * to hold, rather than as a diff against cells it never received —
	 * which would leave every cell of the failed frame wrong on screen
	 * until something else happened to change it.
	 */
	v->have_prev = 0;
out:
	/* Kept for the next frame unless one of them grew past the mark; see
	 * KCON_BUF_KEEP. */
	kcon_buf_retire(&v->bcells, KCON_BUF_KEEP);
	kcon_buf_retire(&v->bcolor, KCON_BUF_KEEP);
}

int kcon_view_ready(const KconSurface *v)
{
	if (!v || v->kind != KCON_KIND_VIEW || !v->conn ||
	    kcon_conn_dead(v->conn))
		return 0;
	/* A READER IS NEVER A REASON TO COMPOSE. kcon_view_send refuses it a
	 * frame, so a session that counted it as ready would walk and diff a
	 * whole grid at the loop's floor rate for a frame nobody is sent —
	 * and the pacing this answer exists to give would be gone whenever a
	 * screen reader is attached. */
	if (v->a11y)
		return 0;
	if (kcon_conn_pending(v->conn) > KCON_VIEW_HIGH)
		return 0;
	if (!(v->caps & KCON_VIEW_FRAME) || !v->frame_owed)
		return 1;
	/* Still painting, unless it has been too long to believe that. */
	return now_ms() - v->frame_at >= KCON_FRAME_STALL_MS;
}

void kcon_server_frame_done(KconServer *s)
{
	if (!s)
		return;
	for (int i = 0; i < s->n; i++) {
		KconSurface *f = s->s[i];

		if (f->kind == KCON_KIND_VIEW || !f->committed)
			continue;
		f->committed = 0;
		kcon_send(f->conn, KCON_OP_FRAME, NULL);
	}
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

void kcon_view_pointer_shape(KconSurface *v, int shape)
{
	if (!v || v->kind != KCON_KIND_VIEW)
		return;

	/* A shape that did not change is not news — the whole reason this is
	 * an op rather than a field on every commit. */
	if (v->shape_seen && v->shape == shape)
		return;
	v->shape_seen = 1;
	v->shape = shape;
	v->shape_dirty = 1;
}

void kcon_view_cursor(KconSurface *v, int x, int y)
{
	if (!v || v->kind != KCON_KIND_VIEW)
		return;

	/* A caret that did not move is not news. */
	if (v->cur_seen && v->cur_x == x && v->cur_y == y)
		return;
	v->cur_seen = 1;
	v->cur_x = x;
	v->cur_y = y;
	v->cur_dirty = 1;
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
int kcon_surface_corner(const KconSurface *f) { return f ? f->corner : 0; }
int kcon_surface_floating(const KconSurface *f) { return f ? f->floating : 0; }
int kcon_surface_keyboard(const KconSurface *f) { return f ? f->keyboard : 1; }
int kcon_surface_input_n(const KconSurface *f) { return f ? f->in_n : -1; }

int kcon_surface_input_at(const KconSurface *f, int i, KRect *out)
{
	if (!f || !out || i < 0 || i >= f->in_n)
		return 0;
	*out = f->in_rect[i];
	return 1;
}

int kcon_surface_caret(const KconSurface *f, int *x, int *y)
{
	if (!f || f->caret_x < 0 || f->caret_y < 0)
		return 0;
	if (x)
		*x = f->caret_x;
	if (y)
		*y = f->caret_y;
	return 1;
}
int kcon_surface_margin_x(const KconSurface *f) { return f ? f->margin_x : 0; }
int kcon_surface_margin_y(const KconSurface *f) { return f ? f->margin_y : 0; }
int kcon_surface_min_cols(const KconSurface *f) { return f ? f->min_cols : 0; }
int kcon_surface_min_rows(const KconSurface *f) { return f ? f->min_rows : 0; }
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

/*
 * THE KEYBOARD FOCUS ARRIVED OR LEFT.
 *
 * Sent on every transition the session makes — a raise, a minimise, a
 * workspace switch, a window closing — so a surface that asked to be dismissed
 * when it loses the focus is, and so a terminal can tell its child (`CSI I` /
 * `CSI O`) that a file it has open may have changed underneath it.
 *
 * A WINDOW ON A WORKSPACE YOU LEFT HAS LOST THE FOCUS and is told so. It is
 * not on the screen and cannot be typed into, and a client that believed it
 * still had the focus would keep drawing a caret nobody can see.
 */
void kcon_surface_focus(KconSurface *f, int in)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_u8(&b, (uint8_t)(in ? 1 : 0));
	kcon_send(f->conn, KCON_OP_FOCUS, &b);
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

/*
 * A FINGER ON THIS SURFACE, in its own cells, with the gesture the recogniser
 * named. The order and the widths are the client's decoder's, and a field added
 * to one without the other is a message read three ways.
 */
void kcon_surface_touch(KconSurface *f, int x, int y, int slot, int phase,
			unsigned ms, int gesture)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_put_u8(&b, (uint8_t)slot);
	kcon_put_u8(&b, (uint8_t)phase);
	kcon_put_u32(&b, ms);
	kcon_put_u8(&b, (uint8_t)gesture);
	kcon_send(f->conn, KCON_OP_TOUCH, &b);
	kcon_buf_free(&b);
}

/*
 * A DRAG IS OVER THIS SURFACE, IS MOVING, OR HAS GONE.
 *
 * The three that say where a drag is, so a target can light up before anything
 * is dropped on it. They carry the MIME type on the way in, because that is
 * what a target refuses on: a folder that takes `text/uri-list` and not
 * `text/plain` has to know before the release, or the highlight is a promise
 * it cannot keep.
 *
 * THE PAYLOAD IS NOT SENT UNTIL THE DROP. A drag that crossed six windows
 * would otherwise hand its bytes to all six, and one of them is a window the
 * person was only passing over.
 */
void kcon_surface_drag_enter(KconSurface *f, int x, int y, const char *mime)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_put_str(&b, mime);
	kcon_send(f->conn, KCON_OP_DRAG_ENTER, &b);
	kcon_buf_free(&b);
}

void kcon_surface_drag_motion(KconSurface *f, int x, int y)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_put_i32(&b, x);
	kcon_put_i32(&b, y);
	kcon_send(f->conn, KCON_OP_DRAG_MOTION, &b);
	kcon_buf_free(&b);
}

void kcon_surface_drag_leave(KconSurface *f)
{
	if (!f)
		return;

	KconBuf b = { 0 };

	kcon_send(f->conn, KCON_OP_DRAG_LEAVE, &b);
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
