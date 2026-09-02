/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcon — a surface, over a socket
 *
 * The console desktop has no compositor: only the process holding the display
 * can draw. This is how a separate program gets a window anyway — it commits a
 * grid of cells and receives input, and it keeps its sprites, its colours and
 * its clipboard, which a program driven through a pty does not.
 *
 * BOTH ENDS LIVE HERE. The client is a KDispImpl and a KtuiBackend; the server
 * is what kdos-con composites. One file describing the wire means the two
 * cannot drift.
 *
 * NO FILE DESCRIPTORS CROSS IT, EVER. That is what makes the socket
 * forwardable: a desktop reached over ssh is the same desktop. A sprite
 * travels as its pixels, once, and is cached by key.
 *
 * LINKS libktui AND libkdisp AND NOTHING ELSE. A surface that gains this gains
 * a socket and a vtable, not a font renderer — the glyphs are rasterised once,
 * in whatever is holding the display.
 * ---------------------------------
 */

#ifndef KCON_H
#define KCON_H

#include <stddef.h>
#include <stdint.h>

#include "kdisp.h"
#include "ktui.h"

/*
 * The version on the wire. A mismatch is refused at hello with BOTH numbers in
 * the message, because "protocol error" tells the person nothing about which
 * half to rebuild.
 */
#define KCON_VERSION 3

/*
 * A length field is an allocation request from an untrusted peer, so it is
 * refused at the header before anything is reserved. A megabyte is far above
 * the largest legitimate message — a full 240x67 grid is under 130 kB — and far
 * below anything that matters if it is refused.
 */
#define KCON_MAX_PAYLOAD (1u << 20)

/* Sending more than this without the peer draining is a peer that has stopped
 * reading. The connection is dropped rather than the server blocking on it. */
#define KCON_MAX_QUEUE (4u << 20)

enum {
	KCON_OP_NONE = 0,
	KCON_OP_HELLO,		/* both ways: version, kind                */

	/* client -> server */
	KCON_OP_ATTACH,
	KCON_OP_COMMIT,		/* cell runs                               */
	KCON_OP_SPRITE,		/* key, w, h, argb — sent once             */
	KCON_OP_SPRITE_DROP,
	KCON_OP_TITLE,
	KCON_OP_CLOSE,
	KCON_OP_CLIP_OFFER,
	KCON_OP_CLIP_REQUEST,
	KCON_OP_DRAG_START,
	KCON_OP_ACTIVATE,	/* management: raise that toplevel         */
	KCON_OP_CLOSE_REQUEST,
	KCON_OP_VIEW_SIZE,	/* a view: this is the grid I can show    */

	/*
	 * THERE IS NOTHING TO SHOW RIGHT NOW. An overlay — a candidate window,
	 * a stack of toasts — is up for a fraction of the time its program is
	 * running, and a surface that cannot say so parks an empty box on
	 * somebody's desktop for the rest of the session. It is not a close:
	 * the connection, the sprites and the clipboard all survive it, and the
	 * next show is a resize rather than a reconnection.
	 */
	KCON_OP_HIDE,

	/*
	 * ONE OP, TWO SENDERS, and the sender is what it means. From a VIEW it
	 * is "I am leaving" — the display goes and every window stays. From a
	 * SHELL surface it is "detach every view", which is how `kdos con
	 * detach` reaches a display it is not. A plain surface sending it is
	 * ignored: a program with a window in the session has no business
	 * taking the screen away from the person using it.
	 */
	KCON_OP_DETACH,

	/*
	 * THE PASSWORD WAS ACCEPTED. The one message that lifts a lock, and it
	 * is separate from KCON_OP_CLOSE on purpose: closing is what a client
	 * does when it exits for any reason at all, and a lock that lifted on
	 * that would lift when the lock program crashed.
	 */
	KCON_OP_UNLOCK,

	/*
	 * RUN THIS ON A VT OF ITS OWN. The console desktop composites character
	 * cells and a Wayland client's surface is pixels, so a graphical
	 * application cannot be a window here — it gets a terminal to itself
	 * and the session goes on existing on the one it was already on.
	 *
	 * From a SHELL surface only, and not because it is a privilege
	 * boundary: a client that can reach this socket is already the
	 * session's own user and can fork and exec whatever it likes. It is so
	 * the op has one caller and one meaning, the way KCON_OP_DETACH does.
	 */
	KCON_OP_RUN,

	/* server -> client */
	/* The VT the guest was given, or 0 — a machine with every terminal in
	 * use has to say so, and the requester is the only thing that can tell
	 * the person. */
	KCON_OP_RUN_REPLY,
	KCON_OP_CONFIGURE,	/* cols, rows                              */
	KCON_OP_KEY,
	KCON_OP_PTR,
	KCON_OP_WHEEL,
	KCON_OP_TOUCH,
	KCON_OP_ENTER,
	KCON_OP_LEAVE,
	KCON_OP_FOCUS,
	KCON_OP_CLIP_DATA,
	KCON_OP_DRAG_ENTER,
	KCON_OP_DRAG_MOTION,
	KCON_OP_DRAG_LEAVE,
	KCON_OP_DRAG_DROP,
	KCON_OP_TOPLEVEL_ADD,
	KCON_OP_TOPLEVEL_STATE,
	KCON_OP_TOPLEVEL_REMOVE,
	KCON_OP_WORKSPACE,
	KCON_OP_CURSOR,		/* where the caret is, for a view          */

	/*
	 * POWER THE SCREEN DOWN. Sent to a view, because the device belongs to
	 * the view and the idle policy belongs to the session — which is the
	 * same split as everything else here: the session decides, the display
	 * acts. A view with no power control ignores it and stays lit, which is
	 * a screensaver that does not save power rather than a failure.
	 */
	/*
	 * SEND YOUR PICTURES AGAIN. A surface's sprites cross once, so a
	 * display that attached after one was sent has the cells that name it
	 * and none of its pixels — and the cells do not change, so nothing ever
	 * repaints them. This is the session asking every surface to forget
	 * what the display has and start again.
	 */
	KCON_OP_SPRITE_RESEND,

	KCON_OP_BLANK,
	KCON_OP_BYE,		/* with a reason, so a log says why        */

	KCON_OP_N
};

/* What a peer says it is at hello. The management messages go only to a
 * surface that asked to be a shell, and only when the peer's credentials match
 * the session's owner. */
enum {
	KCON_KIND_SURFACE = 0,
	KCON_KIND_SHELL,
	/*
	 * A DISPLAY, not a window. It sends no cells and receives the composited
	 * grid — the same socket and the same framing, read the other way round.
	 * A view holds no window state at all, so one that dies loses nothing
	 * and one that is remote is trusted with nothing.
	 */
	KCON_KIND_VIEW,
};

/*
 * WHAT A VIEW CAN DO, sent in its hello beside the version.
 *
 * KCON_VIEW_PIXELS says the view can turn a sprite's bytes into pixels on
 * whatever it is drawing on. A view without it still RECEIVES sprites — a
 * terminal view turns them into characters by shape, which is the only thing
 * a picture can be over ssh — but the session throttles what it sends, because
 * a full window of pixels at a compositor's frame rate down a terminal link is
 * a link that does nothing else.
 */
#define KCON_VIEW_PIXELS 0x1u

/*
 * libkcon as a libkdisp implementation. A consumer hands the ADDRESS of this
 * to kdisp_init(); naming it is what links the console client into that
 * program, which is why libkdisp itself never names it.
 */
extern const KDispImpl kcon_impl;

/* ── buffers ─────────────────────────────────────────────────────────────
 *
 * Little-endian on the wire and read a field at a time. A struct written
 * whole is a struct whose padding and alignment become protocol, and the two
 * ends of a forwarded socket are not always the same build.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	unsigned char *b;
	size_t len, cap;
	int err;		/* set once; every later put is a no-op */
} KconBuf;

void kcon_buf_free(KconBuf *b);
void kcon_buf_reset(KconBuf *b);
int kcon_put_u8(KconBuf *b, uint8_t v);
int kcon_put_u16(KconBuf *b, uint16_t v);
int kcon_put_u32(KconBuf *b, uint32_t v);
int kcon_put_i32(KconBuf *b, int32_t v);
int kcon_put_blob(KconBuf *b, const void *p, size_t n);
/*
 * RAW, WITH NO LENGTH IN FRONT. For a payload whose size the message header
 * already gives — a sprite's pixels are pw*ph*4 and nothing else — because a
 * second length is a second thing that can disagree with the first, and a
 * reader that computed the size from the header would then be four bytes out
 * for every picture on the desktop.
 */
int kcon_put_bytes(KconBuf *b, const void *p, size_t n);
int kcon_put_str(KconBuf *b, const char *s);

/* A cursor over a received payload. Every get is bounds-checked and sets
 * `err` once; a caller may read a whole message and test `err` at the end. */
typedef struct {
	const unsigned char *b;
	size_t len, pos;
	int err;
} KconRd;

void kcon_rd_init(KconRd *r, const void *p, size_t n);
uint8_t kcon_get_u8(KconRd *r);
uint16_t kcon_get_u16(KconRd *r);
uint32_t kcon_get_u32(KconRd *r);
int32_t kcon_get_i32(KconRd *r);
const void *kcon_get_blob(KconRd *r, size_t n);
/*
 * NUL-terminated, and valid only until THE NEXT GET. The payload's own bytes
 * are not terminated, so the string is copied into one scratch buffer shared
 * by every call — a caller reading several strings must copy each before
 * reading the next, or it ends up with one string several times.
 */
const char *kcon_get_str(KconRd *r);
/* What is left unread. A message may carry OPTIONAL trailing fields — a peer
 * that predates them sends a shorter one — and this is how a reader tells the
 * two apart without treating the short message as an error. */
size_t kcon_rd_left(const KconRd *r);

/* ── cells ───────────────────────────────────────────────────────────────
 *
 * A run is a position and a count, then that many packed records. The record
 * is the PROTOCOL's, not KtuiCell's: a wire format that is a struct dump
 * breaks the day a field is added to the cell.
 * ──────────────────────────────────────────────────────────────────────── */

#define KCON_CELL_BYTES 8

int kcon_put_run(KconBuf *b, uint16_t x, uint16_t y,
		 const KtuiCell *cells, uint16_t n);
/* Reads one run into `out`, which must hold at least `max` cells. Returns the
 * count, or -1. */
int kcon_get_run(KconRd *r, uint16_t *x, uint16_t *y, KtuiCell *out,
		 uint16_t max);

/* ── connections ─────────────────────────────────────────────────────────
 *
 * Non-blocking both ways. A send that cannot go out now is queued; a queue
 * past KCON_MAX_QUEUE means the peer stopped reading and the connection is
 * marked dead rather than blocking whatever is holding the display.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct KconConn KconConn;

/* Takes ownership of `fd` and sets it non-blocking. */
KconConn *kcon_conn_new(int fd);
void kcon_conn_free(KconConn *c);
int kcon_conn_fd(const KconConn *c);
int kcon_conn_dead(const KconConn *c);

int kcon_send(KconConn *c, uint16_t op, const KconBuf *payload);
/* Push whatever is queued. 0 when the queue is empty, 1 when more is waiting,
 * -1 when the connection died. */
int kcon_flush(KconConn *c);

typedef struct {
	uint16_t op;
	uint16_t flags;
	const unsigned char *payload;
	size_t len;
} KconMsg;

/*
 * One message, or 0 when none is complete yet, or -1 when the connection died.
 * `out->payload` is borrowed and valid until the next call.
 */
int kcon_recv(KconConn *c, KconMsg *out);

/* ── the server half ─────────────────────────────────────────────────────
 *
 * Connections and the surfaces on them. WHAT THIS DOES NOT DO IS COMPOSITE:
 * where a window goes and which is on top is the window model's, and a display
 * server reads the surfaces out of here and arranges them itself.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct KconServer KconServer;
typedef struct KconSurface KconSurface;

/*
 * Listen on `path`. The socket is unlinked first — a stale one from a session
 * that died would otherwise make this look like a display that is already
 * running — and created inside a directory the caller has already made 0700.
 */
/*
 * WHICH SOCKET A PEER REACHED IS THE EVIDENCE for what it is allowed to be.
 * `kcon_server_new` opens one listener admitting both kinds, which is a
 * session with nothing to separate — offscreen, or --dump. A session that will
 * be attached to adds a KCON_LISTEN_VIEW socket, and only that socket is ever
 * forwarded: a forwarded surface socket would let the far end place windows in
 * your session, which is a different thing entirely from showing you yours.
 */
enum { KCON_LISTEN_ANY = 0, KCON_LISTEN_SURFACE, KCON_LISTEN_VIEW };

#define KCON_MAX_LISTEN 4

/* How many sprite slots one surface may name. The same sixteen bits the cell
 * encoding carries, capped where a table of ints is still small. */
#define KCON_MAX_SPRITE_MAP 4096

/*
 * The longest argument vector KCON_OP_RUN carries. A desktop entry's Exec with
 * its file arguments is a handful of words; the cap is here because the count
 * on the wire is an allocation request from a peer.
 */
#define KCON_MAX_ARGV 32

/*
 * KCON_OP_RUN's flags.
 *
 * KCON_RUN_BARE says the guest IS a compositor and must not be put inside one.
 * The graphical session is the case: kdos-desktop starts kdos-comp, and a
 * compositor inside a kiosk compositor is a screen inside a screen. Everything
 * else is an application, and an application on a VT needs something to hold
 * the display for it.
 */
#define KCON_RUN_BARE 0x1u

/*
 * KCON_RUN_VT pins the guest to a terminal of its own even though embedding is
 * what everything else gets. It is for an application that needs acceleration a
 * software renderer cannot give it: an embedded guest is composited by pixman
 * on the CPU, which is fine for a text editor and is not a way to play a game.
 *
 * The session decides this from the guest's box profile as well, so a caller
 * that knows nothing about the policy sends 0 and still gets the right answer.
 */
#define KCON_RUN_VT 0x2u

KconServer *kcon_server_new(const char *path);
int kcon_server_listen(KconServer *s, const char *path, int kind);
int kcon_server_unlisten(KconServer *s, const char *path);
int kcon_server_nfds(const KconServer *s);
int kcon_server_fd_at(const KconServer *s, int i);
void kcon_server_free(KconServer *s);
int kcon_server_fd(const KconServer *s);

/*
 * Accept what is waiting and read every client. Returns the number of surfaces
 * whose cells changed, so a caller knows whether it has anything to redraw.
 * Never blocks, and never blocks ON a client: one that stopped reading is
 * dropped.
 */
int kcon_server_pump(KconServer *s);

int kcon_server_count(const KconServer *s);
KconSurface *kcon_server_at(KconServer *s, int i);

unsigned kcon_surface_id(const KconSurface *f);
/*
 * KCON_KIND_*. A VIEW IS NOT A WINDOW, and it lives in the same client list —
 * so anything walking that list to find windows must ask. A session that
 * adopted its own display would draw its last frame inside itself.
 */
unsigned kcon_surface_kind(const KconSurface *f);
unsigned kcon_surface_role(const KconSurface *f);
const char *kcon_surface_app_id(const KconSurface *f);
const char *kcon_surface_title(const KconSurface *f);
int kcon_surface_cols(const KconSurface *f);
int kcon_surface_rows(const KconSurface *f);
int kcon_surface_edge(const KconSurface *f);
int kcon_surface_exclusive(const KconSurface *f);
/* True while the surface says it has nothing to show. A display draws it
 * nowhere and lists it nowhere; it is still a client and still attached. */
int kcon_surface_hidden(const KconSurface *f);
/* The surface's own grid, cols by rows. Borrowed, and valid until the next
 * pump — a configure reallocates it. */
const KtuiCell *kcon_surface_cells(const KconSurface *f);
/* True once per change, and clears. */
int kcon_surface_take_dirty(KconSurface *f);

void kcon_surface_configure(KconSurface *f, int cols, int rows);
void kcon_surface_key(KconSurface *f, int key, int mods);
void kcon_surface_ptr(KconSurface *f, int x, int y, int btn, int press);
void kcon_surface_drop(KconSurface *f, int x, int y, const char *text);
void kcon_surface_clip_data(KconSurface *f, const char *text);
/* Ask the surface to go away. It closes itself; a display that killed the
 * connection instead would lose whatever the program wanted to say first. */
void kcon_surface_close(KconSurface *f);

/* ── views ───────────────────────────────────────────────────────────────
 *
 * A display that attached. The session composites into one grid and hands it
 * to every view; each keeps its OWN previous frame, so a view that attaches
 * late gets a whole one rather than the tail of somebody else's diff.
 * ──────────────────────────────────────────────────────────────────────── */

int kcon_server_view_count(const KconServer *s);
KconSurface *kcon_server_view_at(KconServer *s, int i);

/* What the view says it can show. 0 until it has said. */
int kcon_view_cols(const KconSurface *v);
int kcon_view_rows(const KconSurface *v);

/*
 * HOW MANY PIXELS ONE CELL IS on this view, and what it can show. Zero cell
 * dimensions mean a view with no pixel geometry at all — a terminal — and a
 * session sizing a pixel guest for it uses its own default rather than
 * refusing to have one.
 */
int kcon_view_cell_w(const KconSurface *v);
int kcon_view_cell_h(const KconSurface *v);
unsigned kcon_view_caps(const KconSurface *v);

/* Diff against this view's own last frame and send what changed. */
void kcon_view_send(KconSurface *v, const KtuiCell *cells, int w, int h);
void kcon_view_cursor(KconSurface *v, int x, int y);

/* Ask a view to power its screen down (1) or back up (0). */
void kcon_view_blank(KconSurface *v, int on);

/*
 * Ask every surface to send its pictures again. What a display does when one
 * attaches: sprites cross once and the cells naming them do not change, so a
 * view that arrived late would show the fallback mark for ever.
 */
void kcon_server_resend_sprites(KconServer *s);

/*
 * What a surface asked the display to do. A display sets this and acts on it;
 * `text` is borrowed for the length of the call.
 */
typedef struct {
	void (*clip_offer)(KconSurface *f, const char *text, size_t len,
			   int primary, void *user);
	void (*drag_start)(KconSurface *f, const char *mime, const char *data,
			   size_t len, void *user);
	void (*clip_request)(KconSurface *f, int primary, void *user);
	void (*attached)(KconSurface *f, void *user);
	void (*gone)(KconSurface *f, void *user);

	/*
	 * A LOCK SURFACE ASKED TO BE DISMISSED. Not the same event as `gone`,
	 * and the difference is the whole of a lock screen: `gone` is the
	 * client crashing and must leave the session locked, while this is the
	 * password having been accepted.
	 */
	void (*unlock)(KconSurface *f, void *user);

	/* A VIEW's input, which is the session's to route: it decides which
	 * window a key belongs to, and that is not a transport question. */
	void (*view_key)(KconSurface *v, int key, int mods, void *user);
	/*
	 * `subx` and `suby` are the position INSIDE the cell, in 1/256ths. A
	 * cell desktop routes on the cell and ignores them; a pixel guest
	 * embedded in a window is the one thing on this desktop that can be
	 * pointed at more finely than a cell, and a view that knows its own
	 * pixel geometry is the only thing that can say where.
	 */
	void (*view_ptr)(KconSurface *v, int x, int y, int subx, int suby,
			 int btn, int press, void *user);

	/*
	 * A surface sent a picture. `slot` is the SESSION's, already mapped;
	 * `argb` is borrowed and is `pw` by `ph` pixels. A session with no
	 * pixel code forwards it to its views and looks at nothing.
	 */
	void (*sprite)(KconSurface *f, int slot, int w, int h,
		       uint32_t fallback, const uint32_t *argb, int pw, int ph,
		       void *user);

	/*
	 * Run `argv` as a graphical application. Returns the terminal it was
	 * given, 0 when it became an ordinary window, or -1 when it could not
	 * be started at all — which the server sends back, because the
	 * requester is the only thing in the chain that can put a message in
	 * front of a person.
	 *
	 * `argv` is borrowed and NULL-terminated; `title` is what a taskbar
	 * should call it.
	 */
	int (*run)(KconSurface *f, const char *const argv[], const char *title,
		   unsigned flags, void *user);
} KconServerHooks;

void kcon_server_hooks(KconServer *s, const KconServerHooks *h, void *user);

/* Ask a session to release every view. Connects, asks and closes. */
int kcon_detach_all(const char *sock);

/*
 * Ask a session to run `argv` as a graphical application, and wait for the
 * answer. Returns the terminal it was given, 0 when it became an ordinary
 * window, or -1 when it could not be started. `sock` is the session's surface
 * socket — $KDOS_CON.
 *
 * It waits, unlike every other one-shot here, because "it could not be started"
 * is the whole reason this returns anything at all. The wait is bounded: a
 * session that has stopped answering must not hang a launcher.
 */
int kcon_run(const char *sock, const char *const argv[], const char *title,
	     unsigned flags);

/*
 * WHERE A SPRITE'S PIXELS COME FROM, without this library linking a pixel
 * library.
 *
 * A sprite's picture is a `pixman_image_t *` the client owns, and libkcon must
 * not know what that is — it is linked by kdos-con, which links no pixel code
 * at all, and by kinstall's toolkit underneath it. So a consumer that HAS
 * pixman registers this and libkcon sends the bytes it is handed. A consumer
 * that does not register one sends sprite metadata only, and the display draws
 * the fallback codepoint — which is what a text backend does anyway.
 *
 * Return 0 on success. `argb` is borrowed for the length of the call.
 */
typedef int (*KconSpriteBits)(const void *pix, const uint32_t **argb,
			      int *w, int *h, int *stride_px, void *user);

void kcon_set_sprite_bits(KconSpriteBits fn, void *user);

/*
 * A SURFACE'S SLOT NUMBERS ARE ITS OWN. Two surfaces both using slot 0 is the
 * normal case, not a collision to prevent — so the server assigns a session
 * slot when it first sees one, and this is the mapping. A compositing session
 * rewrites the slot in every sprite cell it copies out of a surface.
 *
 * Answers -1 for a slot the surface never sent, which a display draws as the
 * fallback rather than as somebody else's picture.
 */
int kcon_surface_map_slot(const KconSurface *f, int client_slot);

/*
 * A SESSION SLOT FOR A PICTURE THE SESSION ITSELF OWNS. An embedded guest's
 * frame has no surface behind it — the session holds the mapping and cuts the
 * tiles — so it takes slots from the same rotation surfaces do, which is what
 * stops one of its blocks appearing inside somebody's terminal.
 */
int kcon_server_alloc_slot(KconServer *s);

/* Forward a sprite's pixels to a view. The session holds no pixel code and
 * does not look at them; it moves the blob it was given. */
void kcon_view_sprite(KconSurface *v, int slot, int w, int h,
		      uint32_t fallback, const uint32_t *argb, int pw, int ph);

/*
 * con.conf — /etc/kdos/con.conf, overridden by ~/.config/kdos-con/con.conf.
 * Read once on the first lookup; a key in neither file takes `def`.
 */
const char *kcon_conf_str(const char *key, const char *def);
int kcon_conf_int(const char *key, int def);
int kcon_conf_bool(const char *key, int def);

#endif /* KCON_H */
