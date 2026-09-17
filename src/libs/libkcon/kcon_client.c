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
	/* Where an overlay asked to sit, and its margins from the two edges
	 * that corner names — kept for the same reason as the rest of the
	 * attach: a resize repeats it. */
	int corner, margin_x, margin_y;
	int min_cols, min_rows;
	/* Asked to open unanchored, where the eye is — kept for the same
	 * reason as the rest of the attach: a resize repeats it, and an
	 * attach that contradicted the first would be two answers. */
	int floating;
	/* Where a drag is over this surface, and what it is carrying. Kept
	 * rather than delivered, the way libkwl keeps the compositor's. */
	int drag_in, drag_x, drag_y;
	char drag_mime[64];

	/*
	 * THE FACES THE DISPLAY OFFERED, and which is in force. Gathered by
	 * the VIEW and relayed by the session, so these are the display's own
	 * names: they are drawn and indexed into, never parsed here.
	 *
	 * Zero until the list is asked for AND the answer has arrived, which
	 * is some pumps later — a caller re-reads rather than believing the
	 * first count, the rule the window list keeps for the same reason.
	 */
	char fonts[KCON_MAX_FONTS][KCON_FONT_NAME];
	int nfonts, font_cur;

	/* The screens the display is driving, and their modes. The display's
	 * own, relayed by the session — read and drawn, never parsed. */
	KconOut outs[KCON_MAX_OUTS];
	int nouts;

	/* Close when the keyboard focus goes elsewhere — an overlay's own
	 * choice, kept here because the surface made it. */
	int dismiss_on_unfocus;
	/* Whether this surface asked for the keyboard. An overlay that did not
	 * is never focused, so it cannot take the focus off what it covers. */
	int keyboard;
	/*
	 * Whether the SESSION drew this surface's frame — its answer, never
	 * this side's guess. Starts 0, which is "nobody has drawn one", so a
	 * surface that draws its own box has one from its first frame and
	 * stops when the session says it is framing it. See KCON_OP_DECORATED.
	 */
	int decorated;
	int focused;
	char app_id[128], title[256], output[64];

	/*
	 * WHETHER THE SESSION CONFIRMED THE LOCK. Both start at 0 and neither
	 * is inferred: a lock client refuses every keystroke until `engaged`
	 * arrives, because until then the desktop is still on screen and a
	 * password typed at it goes to whatever has the focus. `finished` is
	 * the refusal — the session was already locked by somebody else — and
	 * is read on the way out to decide the exit status.
	 */
	int lock_engaged, lock_finished;

	/*
	 * THE WINDOW LIST, as the session has told it. Kept by the library so
	 * every consumer in a process sees one list rather than each keeping
	 * its own and drifting.
	 */
	KconToplevel tl[64];
	int ntl;
	int ws_current, ws_count;
	unsigned ws_occupied;

	/*
	 * WHICH PICTURE WAS LAST SENT FOR EACH SLOT, as the sprite table's put
	 * counter. A flag saying "sent" would be wrong for an animation: every
	 * frame registers new pixels under the SAME key and therefore in the
	 * same slot, without touching a single cell — so the cells never
	 * change, the diff finds nothing, and a display would hold the first
	 * frame for ever.
	 *
	 * THE COUNTER AND NOT THE POINTER, because the pointer is reused. The
	 * evictor frees the previous frame at the moment the next one is
	 * registered, and the allocator hands the same block straight back —
	 * so a pointer comparison says "already sent" for every frame after
	 * the first, and the animation plays everywhere except over the wire.
	 */
	unsigned long sent_gen[KTUI_MAX_SPRITES];
	/* One past the highest slot ever sent. The scan for pictures the
	 * program has dropped is bounded by this and not by the toolkit's
	 * slot count, which a ktui_sprite_clear() takes back to zero while
	 * the session still holds every slot it was given. */
	int sent_hi;
	KconSpriteBits bits_fn;
	void *bits_user;

	/* Events decoded but not yet handed up. The toolkit asks for one at a
	 * time and a single read can carry many. */
	KtuiEvent q[64];
	int qhead, qtail;

	/*
	 * THE FRAME CONTRACT, this end. `frame_wait` is set when a commit or
	 * a picture goes out and cleared by the session's KCON_OP_FRAME;
	 * while it is set and not yet KCON_FRAME_STALL_MS old, a flush is
	 * stashed in `pend` instead of sent, and the newest stash goes out
	 * when the answer lands. One commit per composed frame, carrying the
	 * newest cells, is what a program that draws faster than the desktop
	 * shows then costs — the contract libkwl keeps with a frame callback.
	 *
	 * A FRAME THE DISPLAY IS TOO FAR BEHIND TO TAKE GOES INTO THE SAME
	 * STASH, and is retried by every drain until the backlog falls back
	 * under KCON_VIEW_HIGH. Dropping it instead leaves nothing to resend
	 * it: the toolkit clears its dirty flag the moment it hands a frame
	 * over, so a surface that paints once and waits — a toast, a menu
	 * after its last paint, a dialog — would keep the stale frame on
	 * screen until somebody pressed a key.
	 */
	int frame_wait;
	int64_t frame_at;
	/* Whether the last flush reached the session. A stashed frame is not
	 * on a screen, and the only thing the toolkit still owes one is the
	 * repaint bit — the cells themselves are held here. */
	int presented;
	KtuiCell *pend;
	/*
	 * WHICH ROWS OF THE STASH HOLD ANYTHING. A stash copies only the rows
	 * that differ from what the display has, so an unmarked row of `pend`
	 * is uninitialised memory and must never be read. The mark is
	 * cumulative and a marked row is re-copied by every later stash: a row
	 * that changed in one stash and changed back in the next would
	 * otherwise release the older cells and poison the toolkit's
	 * last-presented buffer with them.
	 */
	unsigned char *pend_row;
	int pend_w, pend_h, pend_full, pend_valid;
	/*
	 * THE NEXT FRAME MUST BE SENT WHOLE. Set when a frame could not be
	 * encoded or could not be sent whole: the toolkit's last-presented
	 * buffer then describes cells the session never received, and a diff
	 * against it would leave them wrong on screen until something else
	 * happened to change them. Read by cl_flush() and not by cl_present(),
	 * because only a flush covers the whole grid — a stash release is
	 * given the rows the stash marked, and the rows it did not mark are
	 * exactly the ones a diff would skip.
	 */
	int need_full;
	/* Bumped by every configure, equal size or not: a resize waits for
	 * an ANSWER, and an answer that repeats the old size is one. */
	unsigned configure_seq;

	/*
	 * THE FILL BUFFERS FOR EVERYTHING THIS SURFACE SENDS, kept between
	 * messages rather than allocated per message. See KCON_BUF_KEEP: a
	 * picture's pixels are over a hundred kilobytes and a frame chunk is
	 * a quarter of a megabyte, so a per-message buffer is an mmap and an
	 * munmap whose every page the kernel faults in and zeroes on every
	 * frame.
	 *
	 * THEY BELONG TO THE CONNECTION, which is what `C` is: one process
	 * holds one session connection, and these die with it in
	 * kcon_shutdown. They are safe to refill the instant a send returns
	 * because kcon_send copies the payload into the connection's own out
	 * queue — nothing downstream holds a pointer in here.
	 *
	 * THREE, BECAUSE THREE ARE FILLED AT ONCE. `bcells` carries the cells
	 * of a frame and `bcolor` the colour records patching them; the
	 * pictures those cells reference are sent from `bsprite` during the
	 * same walk. Any two of them sharing one allocation would have the
	 * second filler send the first one's bytes, and the coupling would be
	 * invisible until somebody reordered the walk.
	 */
	KconBuf bcells, bcolor, bsprite;
} C;

static int64_t now_ms(void);
static int cl_present(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full, const unsigned char *rows);
static void release_stash(void);

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

/*
 * ASK THE SESSION FOR THE SELECTION. The answer comes back as
 * KCON_OP_CLIP_DATA and is pushed into libktui's paste queue, so every widget
 * that already handles a paste handles this one with no change.
 */
static void clip_request(int primary)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u8(&b, (uint8_t)(primary ? 1 : 0));
	kcon_send(C.conn, KCON_OP_CLIP_REQUEST, &b);
	kcon_buf_free(&b);
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
		C.configure_seq++;
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
		if (!r.err) {
			/*
			 * Ctrl+V ARRIVES AS U+0016 and asks the session for
			 * the clipboard. The same rule libkwl keeps: the key
			 * is still delivered, so a client with nothing on the
			 * clipboard sees exactly what it always saw, and the
			 * text arrives later as KCON_OP_CLIP_DATA rather than
			 * as a reply — a client must not block on a selection
			 * whose owner may be busy compositing.
			 */
			if (ev.key == 0x16)
				clip_request(0);
			push(&ev);
		}
		break;
	case KCON_OP_PTR:
		ev.type = KT_EVT_MOUSE;
		ev.mx = (int)kcon_get_i32(&r);
		ev.my = (int)kcon_get_i32(&r);
		ev.btn = (int)kcon_get_u8(&r);
		ev.press = (int)kcon_get_u8(&r);
		if (!r.err) {
			/* Middle-click pastes the primary selection, the X11
			 * tradition every terminal keeps. */
			if (ev.btn == KT_MB_MIDDLE && ev.press == KT_MP_PRESS)
				clip_request(1);
			push(&ev);
		}
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
	case KCON_OP_VIEW_FONTS: {
		/*
		 * THE LIST THE DISPLAY OFFERED. Stored and not announced: a
		 * caller re-reads it on its own turn, because an event
		 * delivered from inside a pump would arrive from whichever of
		 * the two read paths happened to read the socket.
		 */
		int n = (int)kcon_get_u16(&r);
		int cur = (int)(int16_t)kcon_get_u16(&r);

		if (r.err)
			break;
		if (n < 0)
			n = 0;
		if (n > KCON_MAX_FONTS)
			n = KCON_MAX_FONTS;
		C.nfonts = 0;
		for (int i = 0; i < n; i++) {
			const char *one = kcon_get_str(&r);

			if (r.err)
				break;
			snprintf(C.fonts[C.nfonts], KCON_FONT_NAME, "%s", one);
			C.nfonts++;
		}
		C.font_cur = cur >= 0 && cur < C.nfonts ? cur : -1;
		break;
	}

	case KCON_OP_VIEW_OUTPUTS: {
		/* THE SCREENS THE DISPLAY OFFERED. Stored and not announced,
		 * the rule the font list keeps: a caller re-reads on its own
		 * turn rather than being called back from inside a pump. */
		int n = (int)kcon_get_u16(&r);

		if (r.err)
			break;
		if (n < 0)
			n = 0;
		if (n > KCON_MAX_OUTS)
			n = KCON_MAX_OUTS;
		C.nouts = 0;
		for (int i = 0; i < n; i++) {
			KconOut *o = &C.outs[C.nouts];

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
			C.nouts++;
		}
		break;
	}

	case KCON_OP_CLIP_DATA: {
		/*
		 * READ AS A BLOB AND NOT THROUGH kcon_get_str, which copies
		 * into a 1023-byte scratch buffer: a clipboard is a document as
		 * often as it is a word, and a paste silently missing its tail
		 * is worse than one that fails.
		 */
		uint32_t n = kcon_get_u32(&r);
		const char *t = kcon_get_blob(&r, n);

		if (!r.err && t && n)
			ktui_paste_push(t, n);
		break;
	}
	/*
	 * WHERE A DRAG IS, TRACKED AND NOT ANNOUNCED — which is exactly what
	 * `libkwl` does with the same three on the compositor: `dd_enter`,
	 * `dd_motion` and `dd_leave` keep the position and accept the offer,
	 * and only the DROP becomes a KtuiEvent. One event contract on both
	 * desktops, so a surface written against one works on the other.
	 */
	case KCON_OP_DRAG_ENTER:
		C.drag_x = (int)kcon_get_i32(&r);
		C.drag_y = (int)kcon_get_i32(&r);
		snprintf(C.drag_mime, sizeof(C.drag_mime), "%s",
			 kcon_get_str(&r));
		C.drag_in = !r.err;
		break;
	case KCON_OP_DRAG_MOTION:
		C.drag_x = (int)kcon_get_i32(&r);
		C.drag_y = (int)kcon_get_i32(&r);
		break;
	case KCON_OP_DRAG_LEAVE:
		C.drag_in = 0;
		C.drag_mime[0] = '\0';
		break;
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
		memset(C.sent_gen, 0, sizeof(C.sent_gen));
		C.sent_hi = 0;
		ktui_draw_invalidate();
		break;
	case KCON_OP_TOPLEVEL_ADD: {
		unsigned id = kcon_get_u32(&r);
		const char *app = kcon_get_str(&r);
		char keep[64];

		/* kcon_get_str returns ONE shared scratch buffer, so the first
		 * string is copied before the second is read. */
		snprintf(keep, sizeof(keep), "%s", app);

		const char *ttl = kcon_get_str(&r);

		if (r.err || C.ntl >= (int)(sizeof(C.tl) / sizeof(C.tl[0])))
			break;
		for (int i = 0; i < C.ntl; i++)
			if (C.tl[i].id == id)
				goto tl_done;
		memset(&C.tl[C.ntl], 0, sizeof(C.tl[0]));
		C.tl[C.ntl].id = id;
		snprintf(C.tl[C.ntl].app_id, sizeof(C.tl[0].app_id), "%s", keep);
		snprintf(C.tl[C.ntl].title, sizeof(C.tl[0].title), "%s", ttl);
		C.ntl++;
tl_done:
		break;
	}
	case KCON_OP_TOPLEVEL_STATE: {
		unsigned id = kcon_get_u32(&r);
		unsigned fl = kcon_get_u16(&r);
		int ws = (int)kcon_get_u16(&r);

		if (r.err)
			break;
		for (int i = 0; i < C.ntl; i++)
			if (C.tl[i].id == id) {
				C.tl[i].flags = fl;
				C.tl[i].workspace = ws;
				break;
			}
		break;
	}
	case KCON_OP_TOPLEVEL_REMOVE: {
		unsigned id = kcon_get_u32(&r);

		if (r.err)
			break;
		for (int i = 0; i < C.ntl; i++)
			if (C.tl[i].id == id) {
				C.tl[i] = C.tl[--C.ntl];
				break;
			}
		break;
	}
	case KCON_OP_WORKSPACE: {
		int cur = (int)kcon_get_u16(&r);
		int cnt = (int)kcon_get_u16(&r);
		unsigned occ = kcon_get_u32(&r);

		if (r.err)
			break;
		C.ws_current = cur;
		C.ws_count = cnt;
		C.ws_occupied = occ;
		break;
	}
	case KCON_OP_LOCK_STATE: {
		unsigned fl = kcon_get_u8(&r);

		if (r.err)
			break;
		C.lock_engaged = (fl & KCON_LOCK_ENGAGED) != 0;
		C.lock_finished = (fl & KCON_LOCK_FINISHED) != 0;
		break;
	}
	case KCON_OP_DECORATED: {
		int on = kcon_get_u8(&r) != 0;

		if (r.err || on == C.decorated)
			break;
		C.decorated = on;
		/*
		 * THE FRAME IS PART OF THE PICTURE, so the next frame has to
		 * be a whole one: what changed is which cells are the
		 * surface's to draw, and a diff against the last frame would
		 * leave the box it just stopped drawing on the screen.
		 */
		ktui_draw_invalidate();
		ev.type = KT_EVT_RESIZE;
		push(&ev);
		break;
	}
	case KCON_OP_FOCUS: {
		int in = kcon_get_u8(&r) != 0;

		if (r.err)
			break;
		C.focused = in;
		/*
		 * AN OVERLAY THAT ASKED TO BE DISMISSED IS DISMISSED — the
		 * same rule `libkwl` keeps on `wl_keyboard.leave`, and the
		 * reason it is the client's rather than the session's: the
		 * config field belongs to the surface, and a session deciding
		 * for it would be a second copy of that decision.
		 *
		 * Without this, clicking a window while the Start menu was
		 * open left the menu floating over it on the console until
		 * somebody found Escape, while the same menu closed correctly
		 * on the compositor.
		 */
		if (!in && C.dismiss_on_unfocus)
			C.should_close = 1;
		break;
	}
	case KCON_OP_FRAME:
		/*
		 * THE SESSION HAS COMPOSED THE LAST COMMIT. Whatever was
		 * stashed meanwhile is the newest frame and goes out now.
		 */
		C.frame_wait = 0;
		release_stash();
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
 * cell referencing a slot is never ahead of the picture behind it. Returns 0
 * when it reached the queue; a send that failed must not be recorded as sent,
 * or the slot keeps the fallback mark for the life of the picture.
 */
static int send_sprite(int slot, const KtuiSprite *sp)
{
	/* THE CONNECTION'S OWN BUFFER, EMPTIED FIRST AND NEVER COPIED OUT OF
	 * IT. See C.bsprite: a picture's pixels are the one payload on this
	 * wire large enough that allocating for it is an mmap, and an
	 * animating surface sends one every frame. */
	KconBuf *sb = &C.bsprite;
	const uint32_t *argb = NULL;
	int pw = 0, ph = 0, stride = 0;

	kcon_buf_reset(sb);
	kcon_put_u16(sb, (uint16_t)slot);
	kcon_put_u16(sb, (uint16_t)sp->w);
	kcon_put_u16(sb, (uint16_t)sp->h);
	kcon_put_u32(sb, sp->fallback);

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
		kcon_put_u16(sb, (uint16_t)pw);
		kcon_put_u16(sb, (uint16_t)ph);
		/* RAW ROWS, no length before any of them. The reader knows the
		 * size from pw and ph, and a length per row would put four
		 * bytes between every row of every picture. */
		for (int ry = 0; ry < ph; ry++)
			kcon_put_bytes(sb, argb + (size_t)ry * stride,
				       (size_t)pw * 4);
	} else {
		kcon_put_u16(sb, 0);
		kcon_put_u16(sb, 0);
	}

	int r = kcon_send(C.conn, KCON_OP_SPRITE, sb);

	kcon_buf_retire(sb, KCON_BUF_KEEP);

	return r;
}

/*
 * A PICTURE THE PROGRAM HAS FINISHED WITH, so the session's slot goes back to
 * its rotation. The pool is session-wide and finite, and a slot never given
 * back is one the whole desktop has lost — a pane that closes, an icon the
 * cache evicts and a picture replaced by another all reach here.
 *
 * Sent AFTER the cells of the same flush. Between a drop and the commit that
 * stops naming the slot, the session's cells still reference a number it has
 * already handed back, and the next surface to ask for one is given it: one
 * program's picture inside another's window.
 */
static void send_sprite_drop(int slot)
{
	KconBuf sb = { 0 };

	kcon_put_u16(&sb, (uint16_t)slot);
	kcon_send(C.conn, KCON_OP_SPRITE_DROP, &sb);
	kcon_buf_free(&sb);
}

/*
 * THE LITERAL COLOURS OF A RUN, ONE RECORD PER SPAN THAT CARRIES ANY. The
 * colour record is ten bytes a cell against the commit's eight, and a screen
 * is cells in slots with a handful of literals among them: mirroring the
 * commit's span would put nine zero bytes on the wire for every cell beside
 * the one that named a colour, and a force_full frame, whose runs are whole
 * rows, would pay that for the entire grid.
 *
 * Returns 0 when every span went into the buffer, and -1 when one did not:
 * the caller must not record a cell as delivered that no message carries.
 */
static int put_color_spans(KconBuf *b, uint16_t x, uint16_t y,
			   const KtuiCell *cells, uint16_t n)
{
	const unsigned lit = KT_A_FGRGB | KT_A_BGRGB | KT_A_ULCOLOR |
			     KT_A_ULSTYLE;
	uint16_t i = 0;

	while (i < n) {
		if (!(cells[i].attr & lit)) {
			i++;
			continue;
		}

		uint16_t start = i;

		while (i < n && (cells[i].attr & lit))
			i++;
		if (kcon_put_color_run(b, (uint16_t)(x + start), y,
				       &cells[start],
				       (uint16_t)(i - start)) != 0)
			return -1;
	}

	return 0;
}

/*
 * Sends one frame. `rows`, when given, marks which rows of `cur` hold cells
 * at all — a stash copies only what changed — and an unmarked row is left
 * alone entirely, both in the diff and in `prev`.
 *
 * Returns 1 when the frame went out and 0 when the display was too far behind
 * to take it, in which case `prev` is untouched and the caller still owns the
 * cells: the toolkit has already forgotten them.
 */
static int cl_present(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full, const unsigned char *rows)
{
	/*
	 * A SURFACE THAT IS BEHIND SKIPS THE FRAME, the rule the session
	 * keeps for a display: cells are a stream of pictures and the newest
	 * makes the older ones pointless. A queue allowed to grow instead
	 * reaches KCON_MAX_QUEUE, which marks the connection dead, and the
	 * window is gone with no signal and no line in any log — a
	 * full-screen animation in a terminal window is what fills it.
	 *
	 * The backlog is pushed out first, so the decision is made on what is
	 * still queued rather than on what was queued last turn.
	 */
	kcon_flush(C.conn);
	if (kcon_conn_pending(C.conn) > KCON_VIEW_HIGH)
		return 0;
	C.presented = 1;

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
	int any = 0;

	memset(used, 0, sizeof(used));
	for (int y = 0; y < h; y++) {
		/*
		 * A ROW THE STASH DID NOT COPY IS READ OUT OF `prev`. It holds
		 * nothing in `cur`, and `prev` is its content by construction
		 * — and this scan must see it, because a picture whose cells
		 * never change is exactly the case an animation is.
		 */
		const KtuiCell *row = rows && !rows[y] ? &prev[(size_t)y * w]
						       : &cur[(size_t)y * w];

		for (int x = 0; x < w; x++) {
			uint32_t ch = row[x].ch;

			if (KTUI_IS_SPRITE(ch)) {
				unsigned slot = KTUI_SPRITE_SLOT(ch);

				if (slot < KTUI_MAX_SPRITES)
					used[slot] = 1;
			}
		}
	}

	for (int slot = 0; slot < ktui_sprite_slots(); slot++) {
		const KtuiSprite *sp = ktui_sprite_get(slot);

		if (!sp || !used[slot] || sp->gen == C.sent_gen[slot])
			continue;
		if (send_sprite(slot, sp) < 0)
			continue;
		C.sent_gen[slot] = sp->gen;
		if (slot >= C.sent_hi)
			C.sent_hi = slot + 1;
		/*
		 * A PICTURE ARMS THE FRAME CONTRACT TOO, and the session
		 * answers one as it answers a commit — KCON_OP_SPRITE marks
		 * the surface as owing a boundary there. An animation changes
		 * no cells by design, so a leg paced only by commits is a leg
		 * with no pacing at all: every tick of a full-screen GIF would
		 * go out whether or not the session had composed the last one,
		 * and the surface's own text updates would starve behind it.
		 */
		any = 1;
	}

	/*
	 * ONE MESSAGE FOR THE CELLS AND ONE FOR THE COLOURS PER CHUNK,
	 * however many runs the frame has: a message is a socket write, and
	 * an animation whose every row changed is a syscall per row per
	 * frame otherwise. The literals a terminal named ride the second
	 * message, which the session patches over the first — without it
	 * every colour outside the sixteen reaches the screen reduced to a
	 * slot.
	 *
	 * THE CHUNK IS WHAT LETS A FRAME BE LARGER THAN A MESSAGE. A buffer
	 * refuses the run that would take it past KCON_MAX_PAYLOAD and
	 * latches the refusal, so a frame built whole and sent at the end
	 * fails to encode, silently and for good, at every size past about
	 * 131k cells — the buffers go out whenever the next run would cross
	 * KCON_CHUNK_BYTES instead. Cells first: a colour record patches a
	 * cell the commit placed, so a COLOR that overtook its COMMIT would
	 * be undone by it.
	 *
	 * The colour estimate is the run's, while a run split into spans
	 * pays a six-byte header per span; the chunk sits far enough under
	 * KCON_MAX_PAYLOAD to absorb that overshoot, which is bounded by one
	 * run and so by one row.
	 *
	 * A run is copied into `prev` only once it is IN a buffer, and a
	 * frame that did not go whole arms `need_full`: a copy claiming cells
	 * the session never got is a screen that stays wrong until something
	 * else happens to overwrite it, and this end has no next diff that
	 * would find them.
	 *
	 * THE BUFFERS ARE THE CONNECTION'S OWN AND ARE EMPTIED, NOT
	 * ALLOCATED. See C.bcells: a chunk is a quarter of a megabyte, so a
	 * pair allocated per frame is two mmaps and two munmaps at the
	 * surface's frame rate. They are held by pointer so that no exit from
	 * this function can leave a second owner of either allocation behind.
	 */
	KconBuf *buf = &C.bcells, *cbuf = &C.bcolor;

	kcon_buf_reset(buf);
	kcon_buf_reset(cbuf);

	for (int y = 0; y < h; y++) {
		const KtuiCell *row = &cur[(size_t)y * w];
		KtuiCell *prow = &prev[(size_t)y * w];
		int x = 0;

		if (rows && !rows[y])
			continue;
		if (!force_full &&
		    !memcmp(row, prow, sizeof(KtuiCell) * (size_t)w))
			continue;

		while (x < w) {
			if (!force_full && !memcmp(&row[x], &prow[x],
						   sizeof(KtuiCell))) {
				x++;
				continue;
			}

			int start = x;

			while (x < w &&
			       (force_full || memcmp(&row[x], &prow[x],
						     sizeof(KtuiCell))))
				x++;

			uint16_t n = (uint16_t)(x - start);

			if (buf->len + 6 + (size_t)n * KCON_CELL_BYTES >
				KCON_CHUNK_BYTES ||
			    cbuf->len + 6 + (size_t)n * KCON_COLOR_BYTES >
				KCON_CHUNK_BYTES) {
				if (buf->len && kcon_send(C.conn,
							  KCON_OP_COMMIT,
							  buf) != 0)
					goto fail;
				kcon_buf_reset(buf);
				if (cbuf->len && kcon_send(C.conn,
							   KCON_OP_COLOR,
							   cbuf) != 0)
					goto fail;
				kcon_buf_reset(cbuf);
			}
			if (kcon_put_run(buf, (uint16_t)start, (uint16_t)y,
					 &row[start], n) != 0)
				goto fail;
			if (put_color_spans(cbuf, (uint16_t)start,
					    (uint16_t)y, &row[start], n) != 0)
				goto fail;
			memcpy(&prow[start], &row[start],
			       sizeof(KtuiCell) * (size_t)n);
			any = 1;
		}
	}

	if (buf->len && kcon_send(C.conn, KCON_OP_COMMIT, buf) != 0)
		goto fail;
	if (cbuf->len && kcon_send(C.conn, KCON_OP_COLOR, cbuf) != 0)
		goto fail;
	goto done;
fail:
	/*
	 * A FRAME THAT DID NOT GO WHOLE LEAVES NOTHING CLAIMED. The next
	 * flush is encoded full, and the toolkit is told the frame never
	 * reached a screen so a repaint it was carrying is asked for again.
	 * The tail below still runs: part of the frame is on the wire, so
	 * the session will answer it, and a wait that was not armed would
	 * let the next flush overtake the answer.
	 */
	C.need_full = 1;
	C.presented = 0;
done:
	/* Kept for the next frame unless one of them grew past the mark; see
	 * KCON_BUF_KEEP. */
	kcon_buf_retire(&C.bcells, KCON_BUF_KEEP);
	kcon_buf_retire(&C.bcolor, KCON_BUF_KEEP);

	/* The drops go last, after the commit that stopped naming the slot.
	 * See send_sprite_drop. */
	for (int slot = 0; slot < C.sent_hi; slot++) {
		if (!C.sent_gen[slot] || ktui_sprite_get(slot))
			continue;
		send_sprite_drop(slot);
		C.sent_gen[slot] = 0;
	}

	kcon_flush(C.conn);

	/* The session answers a commit with a frame; nothing was sent, so
	 * nothing is waited for. A surface with no view attached gets no
	 * answer at all and is paced instead by KCON_FRAME_STALL_MS. */
	if (any) {
		C.frame_wait = 1;
		C.frame_at = now_ms();
	}

	return 1;
}

/*
 * THE STASHED FRAME GOES OUT. Diffed against the toolkit's own last-presented
 * buffer — fetched here rather than remembered, because a resize since the
 * stash has replaced it, and a stash of the old size is a frame nobody wants.
 *
 * A display still too far behind keeps the frame stashed rather than losing
 * it. The pictures a stashed frame names are read at this moment and not at
 * the stash, which is what makes ktui_sprite_drop() before freeing a
 * registered picture load-bearing rather than tidy.
 */
static void release_stash(void)
{
	int w, h;
	KtuiCell *prev;

	if (!C.pend_valid)
		return;

	prev = (KtuiCell *)ktui_cells(&w, &h);
	if (prev && w == C.pend_w && h == C.pend_h &&
	    !cl_present(C.pend, prev, w, h, C.pend_full, C.pend_row))
		return;		/* still behind: the frame stays stashed */

	C.pend_valid = 0;
	C.pend_full = 0;
	if (C.pend_row)
		memset(C.pend_row, 0, (size_t)C.pend_h);
}

static void cl_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		     int force_full)
{
	if (!C.conn || kcon_conn_dead(C.conn))
		return;

	C.presented = 0;

	/*
	 * A FRAME THE ENCODER COULD NOT FINISH IS MADE GOOD HERE AND NOWHERE
	 * ELSE. Joined in before the stash decision so either path carries
	 * it: a stash marks every row once `pend_full` is set, and a direct
	 * present walks the whole grid. Applying it inside cl_present()
	 * instead would apply it to a stash release too, whose unmarked rows
	 * cannot be sent — and those are exactly the rows the failed frame
	 * may have left wrong.
	 */
	force_full |= C.need_full;
	C.need_full = 0;

	/*
	 * FRAME THROTTLING, the shape libkwl gives a compositor's callback:
	 * while the session has not yet composed the last commit, or while
	 * the display is too far behind to take another frame, the newest
	 * cells are stashed instead of sent and go out when the session
	 * answers or the backlog drains. A program that redraws on every pty
	 * read then puts one commit per composed frame on the wire instead of
	 * one per read — and the commit it does send carries the newest
	 * frame, not the oldest.
	 *
	 * The stash is a copy: `cur` is the toolkit's back buffer and is
	 * redrawn the moment this returns.
	 */
	kcon_flush(C.conn);

	int behind = kcon_conn_pending(C.conn) > KCON_VIEW_HIGH;

	if (behind ||
	    (C.frame_wait && now_ms() - C.frame_at < KCON_FRAME_STALL_MS)) {
		if (!C.pend || C.pend_w != w || C.pend_h != h) {
			free(C.pend);
			free(C.pend_row);
			C.pend = malloc((size_t)w * h * sizeof(KtuiCell));
			C.pend_row = calloc((size_t)h, 1);
			if (!C.pend || !C.pend_row) {
				free(C.pend);
				free(C.pend_row);
				C.pend = NULL;
				C.pend_row = NULL;
				C.pend_w = C.pend_h = 0;
				C.pend_valid = C.pend_full = 0;
				/* The frame is gone with the stash, so the
				 * next one carries the whole grid rather
				 * than a diff against cells nothing sent. */
				C.need_full = 1;
				return;
			}
			C.pend_w = w;
			C.pend_h = h;
			C.pend_full = 1;
		}

		/*
		 * ONLY THE ROWS THAT DIFFER FROM WHAT THE DISPLAY HAS, plus
		 * every row an earlier stash marked. A surface redrawing
		 * several times per composed frame copies its whole grid on
		 * each one otherwise, and a panel whose cells did not change
		 * pays that for a stash the release then finds nothing in.
		 *
		 * The mark is sticky because the stash is a frame and not a
		 * delta: a row that changed in one stash and changed back in
		 * the next must carry its current cells, not the older ones.
		 */
		int marked = 0;

		C.pend_full |= force_full;
		for (int y = 0; y < h; y++) {
			size_t off = (size_t)y * w;
			size_t len = sizeof(KtuiCell) * (size_t)w;

			if (C.pend_full ||
			    memcmp(&cur[off], &prev[off], len))
				C.pend_row[y] = 1;
			if (!C.pend_row[y])
				continue;
			memcpy(&C.pend[off], &cur[off], len);
			marked = 1;
		}
		if (marked)
			C.pend_valid = 1;
		return;
	}
	/*
	 * The stash is superseded by `cur`, which is newer by construction,
	 * and its `full` is joined into this commit: ktui_draw_flush() clears
	 * force_full after ANY flush, stashed ones included, so dropping it
	 * here would lose a repaint the consumer has already forgotten about.
	 */
	if (C.pend_valid) {
		force_full |= C.pend_full;
		C.pend_valid = 0;
		C.pend_full = 0;
		if (C.pend_row)
			memset(C.pend_row, 0, (size_t)C.pend_h);
	}
	/* A present the display was too far behind to take sent nothing, so
	 * the repaint this flush was carrying is still owed. */
	if (!cl_present(cur, prev, w, h, force_full, NULL))
		C.need_full |= force_full;
}

/*
 * READ THE SOCKET INTO THE QUEUE, and nothing more. The poll below pops from
 * the queue; a pump that went through the poll instead popped an event into
 * a local and dropped it, which is a keystroke lost by every overlay loop on
 * this desktop once per turn.
 *
 * Reading stops while the queue has fewer than a few free slots: what is not
 * read stays in the socket for the next turn, where dropping it would have
 * lost the oldest event — a keystroke under a burst of pointer motion.
 */
static int queue_room(void)
{
	int n = (int)(sizeof(C.q) / sizeof(C.q[0]));

	return (C.qhead - C.qtail - 1 + n) % n;
}

static void cl_drain(int timeout_ms)
{
	if (!C.conn || kcon_conn_dead(C.conn)) {
		C.should_close = 1;
		return;
	}

	/*
	 * A STASH THE FRAME CONTRACT IS NOT HOLDING GOES OUT BEFORE THE WAIT,
	 * and before the poll is built, so what it queues is what decides
	 * whether writability is interesting. What held such a stash was the
	 * display being behind, and a surface that painted once and waits —
	 * a toast, a menu, a dialog — has no later flush to carry the cells
	 * instead.
	 */
	if (!C.frame_wait)
		release_stash();

	/*
	 * A STASHED FRAME HAS A DEADLINE: the session's answer normally
	 * releases it, but one that never comes — the surface is not on any
	 * view, the session is busy — must not hold the frame past the
	 * stall, so the wait is shortened to it.
	 */
	if (C.pend_valid && C.frame_wait) {
		int64_t rem = KCON_FRAME_STALL_MS - (now_ms() - C.frame_at);

		if (rem < 0)
			rem = 0;
		if (timeout_ms < 0 || rem < timeout_ms)
			timeout_ms = (int)rem;
	}

	struct pollfd p = { .fd = kcon_conn_fd(C.conn), .events = POLLIN };

	/* Writable is only interesting while something is queued; asking for
	 * it always would spin. */
	if (kcon_flush(C.conn) > 0)
		p.events |= POLLOUT;

	if (timeout_ms != 0 || queue_room() > 4)
		poll(&p, 1, timeout_ms);

	KconMsg m;
	int r = 0;

	while (queue_room() > 4 && (r = kcon_recv(C.conn, &m)) == 1)
		handle(&m);

	if (r < 0)
		C.should_close = 1;

	/* The answer arrived and was handled above, or the stall passed with
	 * none: either way the stash goes out on this side's own clock, and
	 * no stash outlives KCON_FRAME_STALL_MS whatever is holding it. */
	if (!C.frame_wait || now_ms() - C.frame_at >= KCON_FRAME_STALL_MS) {
		C.frame_wait = 0;
		release_stash();
	}
}

static int cl_poll(KtuiEvent *ev, int timeout_ms)
{
	if (pop(ev))
		return 1;

	cl_drain(timeout_ms);

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

/* See KtuiBackend.presented: a stashed frame is not on a screen. libkcon
 * holds the cells itself and sends them when the display is ready, so the
 * only thing a stash still owes the toolkit is the repaint bit. */
static int cl_presented(void)
{
	return C.presented;
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

/*
 * WHERE THIS SURFACE'S CARET IS, out to the session.
 *
 * In our OWN cells: the session places it, because only the session knows
 * where this window sits. Nothing is written to the terminal — this surface's
 * stdout is not the screen it is drawn on, and an escape sent down it would
 * move the cursor of whatever is reading that instead.
 */
static void cl_caret(int x, int y)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u16(&b, (uint16_t)(int16_t)x);
	kcon_put_u16(&b, (uint16_t)(int16_t)y);
	kcon_send(C.conn, KCON_OP_CARET, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

/* ── the screens, as libkdisp asks for them ──────────────────────────── */

static void kcon_out_ask(void)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_send(C.conn, KCON_OP_VIEW_OUTPUTS, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static int kcon_out_count(void)
{
	return C.nouts;
}

static int kcon_out_at(int i, KDispOut *out)
{
	if (i < 0 || i >= C.nouts)
		return 0;
	snprintf(out->name, sizeof(out->name), "%s", C.outs[i].name);
	out->col = C.outs[i].col;
	out->cols = C.outs[i].cols;
	out->width = C.outs[i].width;
	out->height = C.outs[i].height;
	out->cur_mode = C.outs[i].cur_mode;
	out->nmodes = C.outs[i].nmodes;
	return 1;
}

static int kcon_out_mode_at(int i, int m, KDispMode *mode)
{
	if (i < 0 || i >= C.nouts || m < 0 || m >= C.outs[i].nmodes)
		return 0;
	mode->width = C.outs[i].mode[m].width;
	mode->height = C.outs[i].mode[m].height;
	mode->refresh = C.outs[i].mode[m].refresh;
	return 1;
}

static void kcon_out_set_mode(int i, int m, int keep)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u16(&b, (uint16_t)(int16_t)i);
	kcon_put_u16(&b, (uint16_t)(int16_t)m);
	kcon_put_u8(&b, (uint8_t)(keep ? 1 : 0));
	kcon_send(C.conn, KCON_OP_VIEW_SETMODE, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static const KtuiBackend kcon_backend = {
	.name = "console",
	.flush = cl_flush,
	.poll_event = cl_poll,
	.size = cl_size,
	.caps = cl_caps,
	.caret = cl_caret,
	.presented = cl_presented,
};

/* ── the display implementation ──────────────────────────────────────── */

/*
 * THE ATTACH PAYLOAD, WRITTEN IN ONE PLACE.
 *
 * There are two senders — the first attach and the resize that is a second one
 * — and a field added to only one of them is a message the server reads three
 * u16s past the end of. It refuses that attach and drops the surface, so the
 * symptom is a toast that vanishes the moment it grows rather than anything
 * that looks like a protocol error. That happened; this is why there is a
 * function.
 *
 * A MARGIN IS A DISTANCE FROM AN EDGE AND IS NEVER NEGATIVE. A caller that has
 * not worked out a position says so with the corner — KDISP_CORNER_CENTER —
 * and passes whatever it likes here; clamping rather than widening the wire to
 * a signed field keeps one meaning for the number the session reads.
 */
static void put_attach(KconBuf *b, int cols, int rows)
{
	kcon_put_u16(b, (uint16_t)C.role);
	kcon_put_u16(b, (uint16_t)C.edge);
	kcon_put_u16(b, (uint16_t)C.cells);
	kcon_put_u16(b, (uint16_t)cols);
	kcon_put_u16(b, (uint16_t)rows);
	kcon_put_u8(b, (uint8_t)C.exclusive);
	kcon_put_str(b, C.app_id);
	kcon_put_str(b, C.title);
	kcon_put_str(b, C.output);
	kcon_put_u16(b, (uint16_t)C.corner);
	kcon_put_u16(b, (uint16_t)(C.margin_x > 0 ? C.margin_x : 0));
	kcon_put_u16(b, (uint16_t)(C.margin_y > 0 ? C.margin_y : 0));
	kcon_put_u16(b, (uint16_t)(C.min_cols > 0 ? C.min_cols : 0));
	kcon_put_u16(b, (uint16_t)(C.min_rows > 0 ? C.min_rows : 0));
	/* APPENDED, and read behind kcon_rd_left: a peer that predates it
	 * sends a shorter message, which is the shape every optional field
	 * here already has. */
	kcon_put_u8(b, (uint8_t)(C.floating ? 1 : 0));
	/*
	 * WHETHER THIS SURFACE WANTS THE KEYBOARD, appended behind `floating`
	 * for the reason that field gives. A tooltip, a toast and the
	 * candidate window all say no: an overlay that took the focus would
	 * pull it off whatever it was drawn over, and a menu that closes when
	 * it loses the focus would close the moment a tip appeared beside it.
	 */
	kcon_put_u8(b, (uint8_t)(C.keyboard ? 1 : 0));
}

static int kcon_init(const KDispConfig *cfg)
{
	const char *path = getenv("KDOS_CON");
	/* Registered before or after this, a consumer's choice: the pixel
	 * reader survives the reset, or a picture registered by a program
	 * that called kcon_set_sprite_bits() first is a blank pane. */
	KconSpriteBits bits_fn = C.bits_fn;
	void *bits_user = C.bits_user;

	free(C.pend);
	free(C.pend_row);
	memset(&C, 0, sizeof(C));
	C.bits_fn = bits_fn;
	C.bits_user = bits_user;
	/*
	 * A SURFACE WHOSE EXTENT THE SESSION OWNS ATTACHES WITH NO SIZE. A
	 * saver covers the screen, the desktop's icon layer is the screen, and
	 * a docked panel spans its edge; no dimension of any of them is the
	 * client's to pick — a made-up 80x24 would be a panel a tenth of the
	 * screen long, drawn where it was never asked for, and it WAS an icon
	 * layer eighty columns wide placed by the window search, with the
	 * desktop's own menu and its hint row stranded in the middle of the
	 * screen and every click outside that rectangle reaching nothing. The
	 * configure that answers the attach sets both fields; until it lands
	 * cl_size() reports the fallback grid, which is what the first frame
	 * is drawn into and immediately resized out of.
	 */
	int sized = !cfg || !(cfg->role == KDISP_ROLE_SAVER ||
			      cfg->role == KDISP_ROLE_BACKGROUND ||
			      (cfg->role == KDISP_ROLE_PANEL &&
			       cfg->cells > 0));

	C.cols = sized ? (cfg && cfg->cols > 0 ? cfg->cols : 80) : 0;
	C.rows = sized ? (cfg && cfg->rows > 0 ? cfg->rows : 24) : 0;

	int fd = connect_to(path);

	if (fd < 0)
		return -1;

	C.conn = kcon_conn_new(fd);
	if (!C.conn) {
		close(fd);
		return -1;
	}

	KconBuf b = { 0 };

	kcon_put_u16(&b, KCON_VERSION);
	/* A SHELL claim is what the management verbs and the window list are
	 * gated on, and it is made here because the kind is fixed at hello and
	 * cannot be raised afterwards. See KDispConfig.manage. */
	kcon_put_u16(&b, cfg && cfg->manage ? KCON_KIND_SHELL
					    : KCON_KIND_SURFACE);
	if (kcon_send(C.conn, KCON_OP_HELLO, &b) != 0)
		goto fail;

	C.role = cfg ? (int)cfg->role : 0;
	C.edge = cfg ? cfg->edge : 0;
	C.cells = cfg ? cfg->cells : 0;
	C.exclusive = cfg ? cfg->exclusive : 0;
	C.corner = cfg ? cfg->corner : 0;
	C.dismiss_on_unfocus = cfg ? cfg->dismiss_on_unfocus : 0;
	C.keyboard = cfg ? cfg->keyboard : 0;
	C.margin_x = cfg ? cfg->margin_x : 0;
	C.margin_y = cfg ? cfg->margin_y : 0;
	C.floating = cfg ? cfg->floating : 0;
	C.min_cols = cfg ? cfg->min_cols : 0;
	C.min_rows = cfg ? cfg->min_rows : 0;
	snprintf(C.app_id, sizeof(C.app_id), "%s", cfg && cfg->app_id ? cfg->app_id : "");
	snprintf(C.title, sizeof(C.title), "%s", cfg && cfg->title ? cfg->title : "");
	snprintf(C.output, sizeof(C.output), "%s", cfg && cfg->output ? cfg->output : "");

	kcon_buf_reset(&b);
	put_attach(&b, C.cols, C.rows);
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
	kcon_buf_free(&C.bcells);
	kcon_buf_free(&C.bcolor);
	kcon_buf_free(&C.bsprite);
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
	cl_drain(0);
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
static int kcon_focused(void) { return C.focused; }

static int kcon_cell_w(void) { return 1; }
static int kcon_cell_h(void) { return 1; }
static int kcon_scale(void) { return 1; }
/*
 * WHETHER THE SESSION DREW THE FRAME, as the session last said.
 *
 * Not a constant and not derived from the role: a panel, a layer, a background
 * and a FULLSCREEN window are all drawn without chrome, and only the window
 * model knows which of those a surface is at this moment. A flat NO here made
 * every console window draw a second box inside the session's — the title
 * written twice, photographed on a terminal running btop.
 */
static int kcon_decorated(void) { return C.decorated; }

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
	unsigned seq = C.configure_seq;

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
	put_attach(&b, cols, rows);
	kcon_send(C.conn, KCON_OP_ATTACH, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);

	/*
	 * WAIT FOR THE CONFIGURE, bounded. The caller draws immediately after
	 * this returns, and a draw against the old size is a frame at the wrong
	 * size that nothing ever repaints. The session may answer with a size
	 * that is not the one asked for — it places windows and it has an edge
	 * to fit them inside — so the wait ends on ANY answer, the size it
	 * had included: the configure counter moves on every one, where the
	 * size fields move only on a change.
	 */
	for (int spin = 0; spin < 200 && C.configure_seq == seq; spin++)
		cl_drain(5);
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

/*
 * WHICH OF THIS SURFACE'S CELLS ANSWER THE POINTER.
 *
 * `n < 0` is all of them, which is what a surface starts with; `n == 0` is
 * none, which is what a tooltip, a toast, the candidate window and the saver
 * all want — the thing UNDER them is what a click is aimed at. Sent in the
 * surface's own cells, because that is the only coordinate system a client
 * has; the session adds the origin.
 *
 * A LIST TOO LONG TO CARRY IS REFUSED HERE rather than cut short: a region
 * missing its last rectangle is a surface swallowing clicks it said it would
 * not, which is the whole failure this call exists to prevent.
 */
/*
 * RENAME THIS WINDOW. The session draws the frame here, so the name it prints
 * is the session's copy — and `C.title` is updated with it, or the next
 * re-attach (a resize IS one) would put the attach name back.
 */
static void kcon_set_title(const char *title)
{
	KconBuf b = { 0 };

	if (!C.conn || !title)
		return;
	if (!strcmp(title, C.title))
		return;
	snprintf(C.title, sizeof(C.title), "%s", title);
	kcon_put_str(&b, C.title);
	kcon_send(C.conn, KCON_OP_TITLE, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static void kcon_input_cells(const KRect *rects, int n)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	if (n > KCON_INPUT_RECTS || (n > 0 && !rects))
		n = -1;
	kcon_put_u16(&b, (uint16_t)(n < 0 ? 0xffff : n));
	for (int i = 0; i < n; i++) {
		kcon_put_u16(&b, (uint16_t)(rects[i].x > 0 ? rects[i].x : 0));
		kcon_put_u16(&b, (uint16_t)(rects[i].y > 0 ? rects[i].y : 0));
		kcon_put_u16(&b, (uint16_t)(rects[i].w > 0 ? rects[i].w : 0));
		kcon_put_u16(&b, (uint16_t)(rects[i].h > 0 ? rects[i].h : 0));
	}
	kcon_send(C.conn, KCON_OP_INPUT_REGION, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

int kcon_toplevel_count(void)
{
	return C.ntl;
}

const KconToplevel *kcon_toplevel_at(int i)
{
	return (i >= 0 && i < C.ntl) ? &C.tl[i] : NULL;
}

int kcon_workspace_current(void)
{
	return C.ws_current;
}

int kcon_workspace_count(void)
{
	return C.ws_count;
}

unsigned kcon_workspace_occupied(void)
{
	return C.ws_occupied;
}

/*
 * A REQUEST, NOT AN ACTION. The session owns the stack and the lifetime of
 * every window; a panel says what it wants and reads the answer out of the
 * list it is then sent, which is the same shape the Wayland side has.
 */
static void mgmt_ask(uint16_t op, unsigned id)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u32(&b, id);
	kcon_send(C.conn, op, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

void kcon_session_action(const char *verb)
{
	KconBuf b = { 0 };

	if (!C.conn || !verb || !*verb)
		return;
	/* THE NAME AND NOTHING ELSE. It is matched against the session's bind
	 * table, so a length is what bounds it rather than a terminator this
	 * end promises. */
	kcon_put_bytes(&b, verb, strlen(verb));
	kcon_send(C.conn, KCON_OP_ACTION, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

void kcon_toplevel_activate(unsigned id)
{
	mgmt_ask(KCON_OP_ACTIVATE, id);
}

void kcon_toplevel_close(unsigned id)
{
	mgmt_ask(KCON_OP_CLOSE_REQUEST, id);
}

void kcon_toplevel_state(unsigned id, unsigned flag, int on)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u32(&b, id);
	kcon_put_u16(&b, (uint16_t)flag);
	kcon_put_u8(&b, (uint8_t)(on ? 1 : 0));
	kcon_send(C.conn, KCON_OP_WIN_STATE, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static int kcon_lock_engaged(void)
{
	return C.lock_engaged;
}

static int kcon_lock_finished(void)
{
	return C.lock_finished;
}

/*
 * THE WINDOW LIST, AS libkdisp ASKS FOR IT.
 *
 * The two flag sets are the same numbers and are checked here rather than
 * translated: a mapping loop would be four lines that silently pass the wrong
 * bit the day either enum grows a member in the middle. If this fails, the
 * enums have diverged and the fix is to realign them, not to add a switch.
 */
_Static_assert((unsigned)KDISP_WIN_FOCUSED == (unsigned)KCON_TL_FOCUSED &&
	       (unsigned)KDISP_WIN_MINIMISED == (unsigned)KCON_TL_MINIMISED &&
	       (unsigned)KDISP_WIN_MAXIMISED == (unsigned)KCON_TL_MAXIMISED &&
	       (unsigned)KDISP_WIN_FULLSCREEN == (unsigned)KCON_TL_FULLSCREEN,
	       "KDISP_WIN_* and KCON_TL_* must be the same bits");

static int kcon_win_count(void)
{
	return kcon_toplevel_count();
}

static int kcon_win_at(int i, KDispWin *out)
{
	const KconToplevel *t = kcon_toplevel_at(i);

	if (!t)
		return 0;
	out->id = t->id;
	out->flags = t->flags;
	out->workspace = t->workspace;
	snprintf(out->app_id, sizeof(out->app_id), "%s", t->app_id);
	snprintf(out->title, sizeof(out->title), "%s", t->title);
	return 1;
}

static void kcon_win_activate(unsigned id)
{
	kcon_toplevel_activate(id);
}

static void kcon_win_close(unsigned id)
{
	kcon_toplevel_close(id);
}

static void kcon_win_set_state(unsigned id, unsigned flag, int on)
{
	kcon_toplevel_state(id, flag, on);
}

/*
 * THE SCREEN'S FONT, AS libkdisp ASKS FOR IT.
 *
 * The list is the DISPLAY'S: the view gathers it, the session relays it, and
 * what goes back is an index into it. A surface never sees a fontconfig name
 * it could act on, which is the point — the display may be on another machine
 * whose fonts are its own.
 */
static void kcon_font_ask(void)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_send(C.conn, KCON_OP_VIEW_FONTS, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
}

static int kcon_font_count(void)
{
	return C.nfonts;
}

static int kcon_font_at(int i, char *out, int cap)
{
	if (i < 0 || i >= C.nfonts)
		return 0;
	snprintf(out, (size_t)cap, "%s", C.fonts[i]);
	return 1;
}

static int kcon_font_current(void)
{
	return C.font_cur;
}

static void kcon_font_set(int index, int keep)
{
	KconBuf b = { 0 };

	if (!C.conn)
		return;
	kcon_put_u16(&b, (uint16_t)(int16_t)index);
	kcon_put_u8(&b, (uint8_t)(keep ? 1 : 0));
	kcon_send(C.conn, KCON_OP_VIEW_SETFONT, &b);
	kcon_buf_free(&b);
	kcon_flush(C.conn);
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
	.focused = kcon_focused,
	.cell_w = kcon_cell_w,
	.cell_h = kcon_cell_h,
	.scale = kcon_scale,
	.decorated = kcon_decorated,
	.overlay_resize = kcon_overlay_resize,
	.overlay_show = kcon_overlay_show,
	.overlay_hide = kcon_overlay_hide,
	.input_cells = kcon_input_cells,
	.set_title = kcon_set_title,
	.unlock = kcon_unlock,
	.lock_engaged = kcon_lock_engaged,
	.lock_finished = kcon_lock_finished,
	.win_count = kcon_win_count,
	.win_at = kcon_win_at,
	.win_activate = kcon_win_activate,
	.session_action = kcon_session_action,
	.win_close = kcon_win_close,
	.win_set_state = kcon_win_set_state,
	.out_ask = kcon_out_ask,
	.out_count = kcon_out_count,
	.out_at = kcon_out_at,
	.out_mode_at = kcon_out_mode_at,
	.out_set_mode = kcon_out_set_mode,
	.font_ask = kcon_font_ask,
	.font_count = kcon_font_count,
	.font_at = kcon_font_at,
	.font_current = kcon_font_current,
	.font_set = kcon_font_set,
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
/* A session settling a terminal answers a capture in milliseconds; one that
 * has wedged answers never, and a capture that hung would be one nobody could
 * interrupt from a script. */
#define KCON_CAPTURE_WAIT_MS 2000

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

/*
 * A LAYOUT, SAVED OR PUT BACK. The same shape kcon_run has and for the same
 * reason: the answer — how many windows, or none by that name — is the whole
 * of what the person asked, and only the session can say it.
 */
int kcon_layout(const char *sock, const char *name, int save)
{
	if (!name || !*name)
		return -1;

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

	kcon_put_str(&b, name);
	kcon_put_u16(&b, (uint16_t)(save ? 1 : 0));
	kcon_send(c, KCON_OP_LAYOUT, &b);
	kcon_buf_free(&b);

	int done = -1;

	if (kcon_flush(c) >= 0) {
		int64_t deadline = now_ms() + KCON_RUN_WAIT_MS;

		for (;;) {
			KconMsg m;
			int r = kcon_recv(c, &m);

			if (r < 0)
				break;
			if (r == 1) {
				/* The answer rides KCON_OP_RUN_REPLY: it is
				 * already "did it work, and what came of it",
				 * which is the same two questions. A second
				 * reply op would be a second thing to keep in
				 * step with this one. */
				if (m.op != KCON_OP_RUN_REPLY)
					continue;

				KconRd rd;

				kcon_rd_init(&rd, m.payload, m.len);
				int ok = (int)kcon_get_u16(&rd);
				int got = (int)kcon_get_u16(&rd);

				done = (!rd.err && ok) ? got : -1;
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
	return done;
}

/*
 * PUT TEXT ON THE SESSION'S CLIPBOARD, from a program that is not a surface.
 *
 * The console has no `wl-copy`: its clipboard is the session's own, and the
 * only way in is the offer a surface makes for its selection. This is that
 * message sent by a one-shot — `kcon_run`'s shape — so a command-line tool can
 * copy without becoming a window first.
 *
 * NOT WAITED ON. An offer has no answer: the session either has the text or
 * the connection failed, and there is nothing it could tell us that we could
 * do anything about.
 */
int kcon_clip_offer(const char *sock, const char *text, size_t len)
{
	if (!text)
		return -1;

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

	/* 0: the CLIPBOARD, not the primary selection. A copy somebody asked
	 * for is not a selection they happened to drag over. */
	kcon_put_u8(&b, 0);
	kcon_put_blob(&b, text, len);
	kcon_send(c, KCON_OP_CLIP_OFFER, &b);
	kcon_buf_free(&b);

	int ok = kcon_flush(c) >= 0 ? 0 : -1;

	kcon_conn_free(c);
	return ok;
}

/*
 * THE SELECTION, TAKEN. The ask-and-wait shape kcon_capture has, over the ops
 * a surface already uses for a paste: the session answers KCON_OP_CLIP_REQUEST
 * with KCON_OP_CLIP_DATA whoever asked, so no op is added for this.
 *
 * 0: THE CLIPBOARD, not the primary selection — the half kcon_clip_offer
 * writes, and the one a person means by "the clipboard".
 */
int kcon_clip_take(const char *sock, char **out)
{
	if (!out)
		return -1;
	*out = NULL;

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
	kcon_put_u8(&b, 0);
	kcon_send(c, KCON_OP_CLIP_REQUEST, &b);
	kcon_buf_free(&b);

	int rc = -1;

	if (kcon_flush(c) >= 0) {
		int64_t deadline = now_ms() + KCON_RUN_WAIT_MS;

		while (now_ms() < deadline) {
			KconMsg m;
			int r = kcon_recv(c, &m);

			if (r < 0)
				break;
			if (r == 1) {
				if (m.op == KCON_OP_BYE)
					break;
				if (m.op != KCON_OP_CLIP_DATA)
					continue;

				KconRd rd;

				kcon_rd_init(&rd, m.payload, m.len);

				/* THE BLOB, not kcon_get_str: that helper
				 * copies into a 1023-byte scratch buffer, and
				 * a clipboard cut short at a thousand bytes is
				 * a file sent with its tail missing. */
				uint32_t len = kcon_get_u32(&rd);
				const char *t = kcon_get_blob(&rd, len);

				if (!rd.err && (t || !len)) {
					*out = malloc((size_t)len + 1);
					if (*out) {
						if (len)
							memcpy(*out, t, len);
						(*out)[len] = '\0';
					}
					rc = *out ? 0 : -1;
				}
				break;
			}

			struct pollfd p = { kcon_conn_fd(c), POLLIN, 0 };

			poll(&p, 1, 50);
		}
	}

	kcon_conn_free(c);
	return rc;
}

/*
 * THE COLOUR UNDER THE POINTER. Asked and not waited for: the answer arrives
 * when a person clicks, and the session puts it on its own clipboard rather
 * than sending it back — so this returns whether the ASK landed and nothing
 * more.
 */
int kcon_pick_colour(const char *sock)
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

	kcon_send(c, KCON_OP_PICK, &b);
	kcon_buf_free(&b);

	int ok = kcon_flush(c) >= 0 ? 0 : -1;

	kcon_conn_free(c);
	return ok;
}

int kcon_quit_session(const char *sock)
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
	kcon_send(c, KCON_OP_QUIT, &b);
	kcon_buf_free(&b);

	int rc = kcon_flush(c) < 0 ? -1 : 0;

	kcon_conn_free(c);
	return rc;
}

/*
 * WHAT IS ON A SESSION'S SCREEN, over the surface socket.
 *
 * Connect, say what we are, ask, wait. The wait is BOUNDED: a session busy
 * settling a terminal answers in milliseconds and one that has wedged answers
 * never, and a capture that hung would be a capture nobody could interrupt
 * from a script.
 */
int kcon_capture(const char *sock, int window, char **out)
{
	if (!out)
		return -1;
	*out = NULL;

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
	kcon_put_u16(&b, (uint16_t)(int16_t)window);
	kcon_send(c, KCON_OP_CAPTURE, &b);
	kcon_buf_free(&b);

	int rc = -1;

	if (kcon_flush(c) >= 0) {
		int64_t deadline = now_ms() + KCON_CAPTURE_WAIT_MS;

		while (now_ms() < deadline) {
			KconMsg m;
			int r = kcon_recv(c, &m);

			if (r < 0)
				break;
			if (r == 1) {
				if (m.op == KCON_OP_BYE)
					break;
				if (m.op != KCON_OP_CAPTURE)
					continue;

				KconRd rd;

				kcon_rd_init(&rd, m.payload, m.len);

				/* A blob, not a string: the string reader's
				 * scratch is a kilobyte and a screen is not. */
				uint32_t n = kcon_get_u32(&rd);
				const char *t = kcon_get_blob(&rd, n);

				if (!rd.err && t) {
					*out = malloc((size_t)n + 1);
					if (*out) {
						memcpy(*out, t, n);
						(*out)[n] = '\0';
					}
					rc = *out ? 0 : -1;
				}
				break;
			}

			struct pollfd p = { kcon_conn_fd(c), POLLIN, 0 };

			poll(&p, 1, 50);
		}
	}

	kcon_conn_free(c);
	return rc;
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
