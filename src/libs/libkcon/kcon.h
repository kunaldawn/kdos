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
 *
 * IT IS BUMPED BY ANY CHANGE TO THE ENUM BELOW, not only by a change to a
 * payload. The opcodes are positions, so an entry inserted anywhere but the
 * end renumbers every one after it — and a peer built before the insertion
 * would not fail, which is the dangerous outcome: it would act on the wrong
 * verb.
 *
 * A RECORDING OUTLIVES THE REBUILD. `kdos-view --record` writes the op NUMBER
 * into its file and the replay hands it straight back to the message reader,
 * so an entry added anywhere but the end also turns every recording ever made
 * into a different session — and that one no version check can catch, because
 * the file carries no version at all. Append, whatever group the new op
 * belongs to by meaning.
 */
#define KCON_VERSION 21

/*
 * A length field is an allocation request from an untrusted peer, so it is
 * refused at the header before anything is reserved.
 *
 * THE CAP BOUNDS ONE CHUNK OF A FRAME, NOT A FRAME. A frame is as large as
 * the grid — 4096x4096 is admitted, and a full one of those is tens of
 * megabytes — so a sender walks the grid and flushes a message every
 * KCON_CHUNK_BYTES rather than building the frame and discovering it cannot
 * be encoded. A megabyte is far above any single chunk and far below anything
 * that matters if it is refused.
 */
#define KCON_MAX_PAYLOAD (1u << 20)

/*
 * Where a sender cuts a frame into messages. Well under KCON_MAX_PAYLOAD on
 * purpose: a buffer grows by doubling, so one allowed to approach the cap
 * reallocs to exactly the ceiling and then refuses the run that follows. At a
 * quarter of a megabyte an ordinary frame is still one message and one write,
 * and the largest grid is a handful.
 */
#define KCON_CHUNK_BYTES (256u << 10)

/* Sending more than this without the peer draining is a peer that has stopped
 * reading. The connection is dropped rather than the server blocking on it. */
#define KCON_MAX_QUEUE (4u << 20)

/*
 * HOW LARGE A FILL BUFFER IS KEPT BETWEEN MESSAGES. See kcon_buf_retire().
 *
 * A message is encoded into a KconBuf and the buffer is then kept for the next
 * one, because a fresh allocation the size of a sprite block is an mmap under
 * a size-class allocator and the matching free is an munmap: every page of
 * every block is faulted in and zeroed by the kernel on every frame, which on
 * a fullscreen guest is three quarters of the session's frame time spent in
 * the allocator rather than on pixels.
 *
 * Above this mark the buffer is released instead of kept, so one outsized
 * message does not pin its memory for the life of the connection. It sits
 * above both shapes that repeat: a 1080p sprite block is about 120 KiB, and a
 * frame chunk is KCON_CHUNK_BYTES plus at most the one run that overshot it.
 * Lower it below either and the allocation comes back every frame.
 *
 * IT MUST STAY AT OR ABOVE TWICE KCON_CHUNK_BYTES, because the buffer grows by
 * DOUBLING: a chunk one byte over a power of two rounds the capacity up to the
 * next one, and a capacity above this mark is released. Without the margin the
 * frame buffers fall out of retention silently — the allocator cost returns in
 * full and nothing says why — so the relation is asserted rather than trusted.
 */
#define KCON_BUF_KEEP (512u << 10)

_Static_assert(KCON_BUF_KEEP >= 2u * KCON_CHUNK_BYTES,
	       "KCON_BUF_KEEP must cover a doubled frame chunk or the frame "
	       "buffers are freed and remapped every frame");

/*
 * WHAT THE SOCKET IS ASKED FOR, both directions, on every connection.
 *
 * A frame the sender cannot place in one turn of its loop is presented
 * partially: the display shows what arrived and the window fills in
 * horizontal bands over several frames. A fullscreen guest is dozens of
 * sprite blocks of over a hundred kilobytes each, so the default socket
 * buffer holds only a handful of them and a whole window takes a dozen turns
 * to appear.
 *
 * THE KERNEL DOUBLES THE REQUEST AND CLAMPS IT to net.core.wmem_max /
 * net.core.rmem_max, so what is granted is not what is asked for and must be
 * read back rather than assumed. A clamp is not an error: a smaller buffer is
 * a slower desktop, not a broken one, and the request never fails a
 * connection.
 *
 * SIGNED, unlike the caps above it: setsockopt reads an int through a void
 * pointer and takes whatever bit pattern is there.
 */
#define KCON_SOCK_BUF (2 << 20)

/*
 * A DISPLAY THAT IS BEHIND IS SENT NOTHING, AND IS NEVER DROPPED FOR IT.
 *
 * A view is the one peer whose messages are a STREAM OF PICTURES: the newest
 * frame makes every older one pointless, so the answer to a view that cannot
 * keep up is to skip a frame, not to queue it. Above this mark the session
 * stops sending that view cells and sprites; because a skipped frame also
 * leaves the view's previous-frame copy alone, the next one it does take
 * carries everything it missed.
 *
 * Far enough below KCON_MAX_QUEUE that one more whole message on top of it
 * cannot reach the cap — a message is bounded by KCON_MAX_PAYLOAD and a grid
 * is cut into KCON_CHUNK_BYTES pieces — so a display that reads at all never
 * trips the peer-is-gone guard. Without it a guest repainting its whole
 * window, or a full-screen animation in a terminal, kills the desktop's only
 * display and with it the only source of input the session has.
 *
 * AND IT IS ALSO WHERE A DISPLAY STOPS BEING READY, which is what makes it
 * the wrong mark to fill a queue up to. kcon_view_ready() returns 0 above it,
 * and a session whose displays are none of them ready composes no frame at
 * all — so a publisher that filled a queue to this mark would buy one
 * window's pixels with the panel, the pointer and the clock. An embedded
 * window is paced against a softer mark of its own for that reason; see
 * EM_VIEW_SOFT in kdos-con's embed.c.
 */
#define KCON_VIEW_HIGH (1u << 20)
/* The same mark, read from the other end: a surface whose own queue is past
 * it skips its frame rather than letting the queue reach the cap. */

enum {
	KCON_OP_NONE = 0,
	KCON_OP_HELLO,		/* both ways: version, kind                */

	/* client -> server */
	KCON_OP_ATTACH,
	KCON_OP_COMMIT,		/* cell runs                               */
	KCON_OP_SPRITE,		/* key, w, h, argb — sent once             */
	/*
	 * A PICTURE'S NUMBER GOES BACK, AND IT TRAVELS IN BOTH DIRECTIONS
	 * BECAUSE BOTH ENDS HOLD PIXELS UNDER IT.
	 *
	 * Client -> session: the client has finished with one of its own slot
	 * numbers and the session slot behind it returns to the rotation.
	 * Session -> view: that session number has gone back to the rotation,
	 * which will hand it out again when the search comes round to it, so
	 * the display must forget the picture it is holding under it. Until
	 * the number comes round nobody owns it and nothing will send a
	 * picture under it, so a display that is never told keeps those pixels
	 * in its own byte budget with nothing that will ever replace them, and
	 * the eviction that eventually takes them is reported as a loss of a
	 * slot that by then belongs to a live window, which spends that
	 * window's repair allowance on a picture it never lost.
	 *
	 * The payload is one u16 slot either way: the sender's own numbering.
	 */
	KCON_OP_SPRITE_DROP,	/* both ways: slot                         */
	KCON_OP_TITLE,
	KCON_OP_CLOSE,
	KCON_OP_CLIP_OFFER,
	KCON_OP_CLIP_REQUEST,
	KCON_OP_DRAG_START,
	KCON_OP_ACTIVATE,	/* management: raise that toplevel         */
	KCON_OP_CLOSE_REQUEST,

	/*
	 * PUT THAT TOPLEVEL INTO A STATE: one KCON_TL_ bit and the value
	 * wanted. Minimise, maximise and fullscreen are the same request with
	 * a different bit, and three ops would be three chances for the two
	 * ends to disagree about a payload they share.
	 *
	 * THE STATE ASKED FOR, NEVER A TOGGLE. A panel draws the list it was
	 * sent and a person clicks the row they can see; a toggle acts on
	 * whatever the session believed a round trip later, which on a busy
	 * desktop is the opposite of what they clicked.
	 */
	KCON_OP_WIN_STATE,
	/*
	 * A VIEW: THIS IS THE GRID I CAN SHOW, and optionally the pixel size
	 * of one cell after it. Sent when it attaches and again whenever
	 * either changes — a font step is a resize, because that is what
	 * dividing the same screen by a different cell is.
	 */
	KCON_OP_VIEW_SIZE,

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
	 * END THE SESSION. From a SHELL surface only, the same rule
	 * KCON_OP_DETACH keeps.
	 *
	 * A QUIT IS A MESSAGE, NOT A MISSING SOCKET. Unlinking the two socket
	 * files leaves the serve loop running on listeners it still holds, so
	 * every attached view keeps its display, the supervisor keeps waiting,
	 * and the session is unreachable and alive. There is no pid in a
	 * socket path either, and looking one up by name would end whichever
	 * process happened to match.
	 */
	KCON_OP_QUIT,

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
	/*
	 * THE WINDOW LIST, to a SHELL surface only.
	 *
	 * This is the channel that carries on the console what
	 * `wlr-foreign-toplevel-management` and `ext-workspace-v1` carry on
	 * Wayland: a panel cannot draw a taskbar, and nothing can raise or
	 * close another program's window, without it.
	 *
	 * A plain surface is not told. A program with a window in the session
	 * has no business knowing what else is open, and the two-socket split
	 * means a forwarded display cannot ask either.
	 *
	 * ADD carries the id, the app id and the title; STATE the id, a flag
	 * set and the workspace; REMOVE the id alone. STATE rather than a
	 * fresh ADD because a title that changes is not a window that closed
	 * and reopened, and a taskbar that redrew its whole row on every
	 * keystroke in a terminal would flicker on every keystroke.
	 */
	KCON_OP_TOPLEVEL_ADD,
	KCON_OP_TOPLEVEL_STATE,
	KCON_OP_TOPLEVEL_REMOVE,
	KCON_OP_WORKSPACE,
	KCON_OP_CURSOR,		/* where the caret is, for a view          */

	/*
	 * PUT THIS ON THE HOST TERMINAL'S CLIPBOARD, for a view that is one.
	 *
	 * A view inside `foot`, or one at the far end of `ssh`, is a window on
	 * somebody's own desktop — and a copy inside this session that did not
	 * reach that desktop's clipboard is a copy they cannot paste into
	 * their editor. The view writes OSC 52; the session encodes nothing.
	 *
	 * It buys nothing on `tty1`: `ktui_clip_copy()` is a deliberate no-op
	 * on a Linux console, which has no clipboard to write to. There the
	 * session's own selection is the whole answer.
	 */
	KCON_OP_VIEW_CLIP,

	/*
	 * WHETHER THE SESSION IS LOCKED, told to the lock surface itself.
	 *
	 * A lock client must not accept a keystroke before the session has
	 * confirmed the lock: until then the desktop is still on screen and
	 * still taking input, and a password typed into that is a password
	 * typed into whatever has the focus. The client cannot know on its
	 * own — attaching is a request, and the session is what grants it.
	 *
	 * One byte of flags: KCON_LOCK_ENGAGED means the session is locked by
	 * this surface, KCON_LOCK_FINISHED that it will not be — the session
	 * was already locked by another client. The two are separate because
	 * a client that is refused must exit non-zero rather than sit on a
	 * screen it does not own.
	 */
	KCON_OP_LOCK_STATE,

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

	/*
	 * A TERMINAL RANG. It carries nothing: what a bell means is the
	 * display's to decide, and the two displays decide differently — a
	 * view in somebody's terminal writes BEL and lets that terminal do
	 * whatever it is configured to do, and a view on a screen has no
	 * sound to make. The VISIBLE half is the session's, because inverting
	 * a window means owning its cells.
	 */
	KCON_OP_BELL,

	/*
	 * A VIEW WAS HANDED TEXT. A view in somebody's terminal is the only
	 * thing that can be: the host terminal owns that machine's clipboard
	 * and reports a paste in brackets, and this desktop never sees the
	 * menu it came from. The session decides where it goes, which is the
	 * focused window and not the view — a display holds no window state,
	 * so it cannot know what it would be pasting into.
	 */
	KCON_OP_PASTE,

	/*
	 * MAKE YOUR FONT BIGGER, SMALLER, OR WHAT IT WAS. One signed step,
	 * and never a font name: the font is the VIEW'S — it was given one on
	 * its command line or by its environment, and a session that named
	 * fontconfig syntax would be a session deciding what a display it has
	 * never seen can render.
	 *
	 * Sent only to a view that said KCON_VIEW_FONT. The grid comes back
	 * as an ordinary KCON_OP_VIEW_SIZE, because a font that changes the
	 * cell changes how many cells fit and that is the same event as a
	 * screen being resized.
	 */
	KCON_OP_VIEW_FONT,

	/*
	 * THE COLOURS A RUN'S CELLS NAMED THEMSELVES, sent only to a view that
	 * said KCON_VIEW_COLOR and only for a run that has any.
	 *
	 * It follows the KCON_OP_COMMIT of the SAME run and repeats its
	 * position and count, so the view patches cells it already has rather
	 * than holding a frame back waiting for a second message that may
	 * never come. A view that declined never sees it and draws the slots,
	 * which is the whole point of the split: eight bytes a cell down a
	 * terminal link, and the literals only where somebody can show them.
	 */
	KCON_OP_COLOR,

	/*
	 * WHAT IS ON THE SCREEN, AS TEXT — asked by a shell surface and
	 * answered on the same op.
	 *
	 * A shell only, like every other management verb: a view is trusted
	 * with the cells it was handed and the events it reports, and reading
	 * back a whole session is neither. The request carries a window's ring
	 * number, or 0 for the screen; the answer is one string, because a
	 * capture is something a person reads or pipes and a cell run would
	 * make every consumer of it re-implement the renderer.
	 */
	KCON_OP_CAPTURE,

	/*
	 * WHAT A WIDGET SAYS IT IS, out to the readers. Role, position in its
	 * set, the focused window's rectangle where the record is a window,
	 * then the label and the value.
	 *
	 * It goes only to a11y clients: every view is sent the cells, and a
	 * display that draws them has no use for a second description of what
	 * it is already showing.
	 */
	KCON_OP_ANNOUNCE,

	KCON_OP_BYE,		/* with a reason, so a log says why        */

	/*
	 * SAVE OR LOAD A LAYOUT — the windows a session has open, by name.
	 * Client to session, despite sitting among the answers: an op is
	 * appended wherever it belongs by meaning, for the reason KCON_VERSION
	 * gives above.
	 *
	 * FROM A SHELL SURFACE ONLY, which is KCON_OP_RUN's rule and is kept
	 * for KCON_OP_RUN's reason: not a privilege boundary, but so the op
	 * has one caller and one meaning.
	 *
	 * IT CARRIES A NAME AND NEVER A COMMAND. A layout names roles the
	 * session resolves through `con.conf` — the indirection the chords
	 * keep — so nothing that reaches this socket can choose what a session
	 * starts, only which of the person's own arrangements to put back.
	 */
	KCON_OP_LAYOUT,

	/*
	 * ASK THE SESSION FOR THE COLOUR UNDER THE POINTER.
	 *
	 * Nothing on the wire in either direction. The session takes the whole
	 * interaction — it draws the prompt, it reads the click, and it puts
	 * the answer on its own clipboard — because the answer arrives when a
	 * person clicks and a socket a caller blocked on for that is a caller
	 * hung for as long as somebody hesitates.
	 *
	 * FROM A SHELL SURFACE ONLY, the rule KCON_OP_RUN keeps and for the
	 * same reason: one caller and one meaning.
	 */
	KCON_OP_PICK,

	/*
	 * WHERE THIS SURFACE'S CARET IS, in its own cells — one signed pair,
	 * and a negative x means it has none.
	 *
	 * A view holds no window state and cannot know where the text cursor
	 * of the focused window is, so the session tells it; and the session
	 * knows only its own terminals, because a surface knew its caret and
	 * had no message to say so. This is that message. The position is
	 * SURFACE-LOCAL: only the session knows where the window sits, and a
	 * surface that sent screen coordinates would be a surface guessing.
	 *
	 * Sent whenever it moves and once when it goes away. The session
	 * forwards the focused window's, so a surface without the focus is
	 * stored and not published.
	 */
	KCON_OP_CARET,

	/*
	 * THE FACES THIS VIEW CAN RENDER, asked and answered on one op.
	 *
	 * THE VIEW ENUMERATES AND NOTHING ELSE MAY. A view is the only end
	 * with a font stack, and it is the end that may be somewhere else
	 * entirely — a forwarded display has its own machine's fonts, and a
	 * list gathered here would be this machine's, offered to a screen
	 * that has never had one of them. That is the same rule
	 * KCON_OP_VIEW_FONT states about a step, said about a list.
	 *
	 * Empty from a view that did not claim KCON_VIEW_FONT, and never
	 * asked of one: the terminal it runs in owns the font.
	 *
	 * Request (session to view): nothing.
	 * Answer (view to session): u16 count, u16 the one in force, then
	 * that many strings — each a name the VIEW produced and only it has
	 * to understand.
	 */
	KCON_OP_VIEW_FONTS,

	/*
	 * WEAR THE NTH OF THEM. A u16 INDEX into the list this view last
	 * sent — 0xffff for the one it started with — and a u8 saying whether
	 * to KEEP it.
	 *
	 * A PREVIEW WRITES NO STATE FILE. Arrows in a picker walk a list and
	 * every step is a real font on a real screen; a step that persisted
	 * would make the last face the highlight passed over the one the next
	 * login comes up in, whether or not anybody chose it. Only `keep`
	 * writes, and putting it back removes.
	 *
	 * AN INDEX AND NEVER A NAME, for the reason above: the only names on
	 * this wire are the view's own, travelling outward. A session that
	 * sent fontconfig syntax back would be a session deciding what a
	 * display it has never seen can render, which is what the step op
	 * exists not to do.
	 *
	 * The grid comes back as an ordinary KCON_OP_VIEW_SIZE, because a
	 * font that changes the cell changes how many cells fit.
	 */
	KCON_OP_VIEW_SETFONT,

	/*
	 * THE SCREENS THIS VIEW IS DRIVING, asked and answered on one op —
	 * the shape KCON_OP_VIEW_FONTS uses, and for the same reason: a
	 * request and its answer are one verb asked in two directions.
	 *
	 * THE VIEW ENUMERATES, as it does for faces. A session composes one
	 * grid and has no idea there are screens under it; the display is the
	 * end that knows, and it may be somewhere else entirely.
	 *
	 * Request (session to view): nothing.
	 * Answer (view to session): u16 count, then per screen — a name, the
	 * columns it shows as two u16s, its pixel size as two u16s, the index
	 * of the mode it is wearing, and then its modes as width, height and
	 * millihertz.
	 */
	KCON_OP_VIEW_OUTPUTS,

	/*
	 * WEAR THE NTH MODE ON THE NTH SCREEN. Two u16 INDICES into the list
	 * this view last sent, and a u8 saying whether to keep it.
	 *
	 * INDICES AND NEVER A MODE, for the reason a face is never a name: the
	 * only modes on this wire are the ones the view itself published,
	 * travelling outward. A session that sent a resolution back would be
	 * deciding what a display it has never seen can show.
	 *
	 * The grid comes back as an ordinary KCON_OP_VIEW_SIZE.
	 */
	KCON_OP_VIEW_SETMODE,

	/*
	 * WHICH OF A SURFACE'S CELLS ANSWER THE POINTER, and the default is
	 * all of them.
	 *
	 * A tooltip, a toast, the candidate window and the screen saver are
	 * all drawn over the desktop and all take NOTHING: the thing under
	 * them is what a click is aimed at. Without this the topmost surface
	 * under the pointer wins every time, so a tooltip that opened over
	 * the Start button swallowed the click on it — the pointer saw a
	 * button, the session saw a tip, and the menu never opened.
	 *
	 * u16 count, then that many rectangles as four u16s each, in the
	 * surface's OWN cells. A count of 0xffff is "all of me", which is
	 * what a surface starts with and what wl_surface's NULL region means
	 * on the other transport.
	 *
	 * MORE RECTANGLES THAN KCON_INPUT_RECTS ARE REFUSED WHOLE, back to
	 * "all of me": a region silently cut short would leave part of a
	 * surface answering clicks it said it would not, which is the failure
	 * this op exists to prevent pointed the other way.
	 */
	KCON_OP_INPUT_REGION,

	/*
	 * WHETHER THE SESSION IS DRAWING THIS SURFACE'S FRAME. One u8, session
	 * to client, sent when the answer changes and whenever a surface is
	 * adopted.
	 *
	 * `kdisp_decorated()` means "somebody else drew my furniture, so I must
	 * not" — three programs ask it (kdos-term, kdos-res and every
	 * kdos-shell window through sh_frame()) and all three draw their own
	 * box when the answer is no. libkcon answered a flat NO, so on the
	 * console every one of them drew a second box inside the session's
	 * frame, with the title written twice. A photograph of a terminal
	 * running btop shows all three: the session's frame, the client's box,
	 * and btop's own.
	 *
	 * THE SESSION ANSWERS IT AND THE CLIENT DOES NOT DERIVE IT. The window
	 * model already decides this — a panel, a layer, a background and a
	 * FULLSCREEN window are drawn without chrome — and a client that
	 * re-derived it from its role alone would be wrong the moment a window
	 * went fullscreen and right again when it came back. One fact, one
	 * owner, one message.
	 *
	 * NOT A FIELD ON KCON_OP_CONFIGURE: a configure that does not change
	 * the size is dropped by the client on purpose, and a decoration that
	 * changed without the size changing would be dropped with it.
	 */
	KCON_OP_DECORATED,

	/*
	 * A FRAME BOUNDARY, three ways, and the direction is what it means.
	 *
	 * Session -> view: everything this frame changed has been sent — the
	 * cell runs and the colour runs that patch them — so the view may
	 * present. A view that painted at every message boundary showed the
	 * slots of a colour run before its literals arrived, one frame in
	 * eight slots and the next in the right ones.
	 *
	 * View -> session: the frame before it has been painted. The session
	 * composes the next frame when a view says so and not before, which
	 * is what paces the whole console path at the rate the display can
	 * show — the same contract a Wayland frame callback gives libkwl. A
	 * view that never answers is one built without the capability, and
	 * the session falls back to its own clock after KCON_FRAME_STALL_MS.
	 *
	 * Session -> surface: the frame that carried this surface's last
	 * commit or picture has been composed. A surface holds its next
	 * commit until it arrives, so a program drawing faster than the
	 * desktop can show puts one commit on the wire per composed frame,
	 * carrying the newest cells, instead of every intermediate one. A
	 * picture counts: an animation changes pixels and no cells, so a leg
	 * answered only for cells would leave it unpaced.
	 *
	 * IT IS THE CELL PATH'S BOUNDARY AND NOT THE PICTURE PATH'S: the
	 * session -> view message is emitted by kcon_view_send() and only when
	 * that frame carried runs, so pictures forwarded with
	 * kcon_view_sprite() fall outside it. A window whose pixels change and
	 * whose cells do not is published with no boundary at all, and a view
	 * that presents on each picture shows part of one window frame beside
	 * part of the last.
	 */
	KCON_OP_FRAME,

	/*
	 * VIEW -> SESSION: PICTURES THAT ARRIVED AND COULD NOT BE KEPT.
	 *
	 * A view's sprite table has a byte budget of its own, so pixels that
	 * crossed the wire may still be refused at the far end. The cells
	 * naming that slot are already drawn and the sender has already
	 * cleared what it owed — kcon_view_sprite() answers for the WIRE —
	 * so without this the block is a hole in the window for as long as the
	 * window lives: nothing re-sends a picture nobody knows was lost.
	 *
	 * A SET PER PAINTED FRAME, NOT A MESSAGE PER LOSS. A view short of
	 * budget loses one picture per block per frame, and a message for each
	 * would spend the queue the pictures themselves need. The sender
	 * gathers the slots it refused, drops the repeats and sends one
	 * message when it presents: u16 count, then that many u16 slots.
	 *
	 * The owner of those slots owes them again, and must bound how often
	 * it pays PER UNIT OF TIME: a table too small for the window refuses
	 * every re-send, and a repair with no limit is the same bytes for
	 * ever. A total, restored only when the owner next draws, is not a
	 * bound but an expiry — an owner that has finished drawing never
	 * restores it, and the first picture the display loses after that is a
	 * hole nothing fills.
	 */
	KCON_OP_SPRITE_LOST,

	/*
	 * VIEW -> SESSION: THE KEY AS A SWITCH, BESIDE THE CHARACTER.
	 *
	 * KCON_OP_KEY is a resolved character and a cell desktop wants nothing
	 * else: a text field is typed into with characters, and a view that
	 * resolved the person's own layout is the only thing that can. A PIXEL
	 * GUEST IS NOT A TEXT FIELD. It holds keys down, repeats from its own
	 * keymap, reads a modifier that produces no character at all, and
	 * resolves the layout itself — none of which survives a round trip
	 * through a codepoint. So the switch travels too, and the two streams
	 * are both true: the session routes chords and cells from the cooked
	 * one and hands the raw one to whichever window is a guest.
	 *
	 * ONLY WHILE IT IS ASKED FOR, and a view that was never asked sends
	 * nothing. See KCON_OP_VIEW_RAW. All four raw verbs are refused from
	 * a view that did not claim KCON_VIEW_RAW and from one that attached
	 * to observe, the same guard KCON_OP_KEY keeps.
	 *
	 * u16 keycode — evdev, NOT +8, refused above KCON_KEYCODE_MAX;
	 * u8 state, 1 pressed 0 released; u32 depressed; u32 latched;
	 * u32 locked; u32 group; u32 time (the backend's own clock,
	 * milliseconds).
	 *
	 * THE COOKED MESSAGE FOR THE SAME PHYSICAL KEY GOES FIRST. The session
	 * decides from the cooked one whether a chord ate the key, and a raw
	 * key that arrived before that decision would be one the guest
	 * receives and the desktop also acts on.
	 *
	 * AND A KEY IS NEVER COALESCED, however far behind the link is. A
	 * dropped press is a letter that never arrives; a dropped release is a
	 * key held down for ever. Only motion compresses — see
	 * KCON_OP_PTR_RAW.
	 */
	KCON_OP_KEY_RAW,

	/*
	 * VIEW -> SESSION: WHERE THE POINTER IS IN PIXELS, AND HOW FAR IT
	 * MOVED.
	 *
	 * KCON_OP_PTR is a cell and an offset inside it, reported when the
	 * cell changes, which is everything a grid needs and nothing a
	 * scrollbar two pixels wide can use. This carries the view's own pixel
	 * position, the cell size those pixels were measured in — so the
	 * session derives the same cell the view would, with no rounding and
	 * no disagreement across a font step — and the deltas the device
	 * actually reported, which are the only thing a guest that grabbed the
	 * pointer can read.
	 *
	 * i32 x; i32 y — absolute, this view's pixels;
	 * u16 cell_w; u16 cell_h — what x and y were measured in, and never
	 *   zero: the session derives its cell by DIVIDING by them, so a zero
	 *   is refused here rather than at the consumer;
	 * i32 dx; i32 dy — 1/256 pixel, accelerated;
	 * i32 dx_unaccel; i32 dy_unaccel — 1/256 pixel, as the device said;
	 * u16 button — evdev BTN_*, 0 for a motion that pressed nothing,
	 *   refused above KCON_KEYCODE_MAX;
	 * u8 state — 1 pressed 0 released, meaningless when button is 0;
	 * u8 mods — KT_MOD_*, which is the session's and not the guest's:
	 *   a guest reads its modifiers from its own keyboard, and the session
	 *   reads them from here because Super+drag is a chord on a pointer;
	 * u32 time.
	 *
	 * THE FULL BUTTON CODE AND NOT A NARROWED ONE. A mouse with side
	 * buttons drives Back and Forward in a browser, and KT_MB_* has three
	 * names in it.
	 *
	 * MOTION COALESCES TO THE NEWEST; NOTHING ELSE COALESCES. A device at
	 * a thousand hertz on a socket that also carries a window of pixels is
	 * a socket that carries no pixels, and only the newest position is
	 * true — so a view merges a motion into the pending one when no
	 * button, key or axis message sits between them, and SUMS the deltas
	 * it merges, because a delta is a distance and dropping one shortens
	 * the movement a grabbed guest sees. A message carrying a button is
	 * the boundary and is sent whole: a click coalesced away is a click
	 * that never happened, and a click merged into a later position is a
	 * click on the wrong thing.
	 */
	KCON_OP_PTR_RAW,

	/*
	 * VIEW -> SESSION: A SCROLL WITH A SECOND AXIS AND A REAL VALUE.
	 *
	 * The cell path has no horizontal axis at all — KT_MB_WHEEL_UP and
	 * _DOWN are the whole vocabulary — and a detent quantised to one step
	 * is a page that jumps. A guest wants the continuous value, the
	 * high-resolution count and which device made it, because a wheel and
	 * a finger scroll differently and a toolkit that is told which will
	 * behave like every other desktop.
	 *
	 * i32 value — 1/256 pixel; i32 value120; u8 axis — KCON_AXIS_*;
	 * u8 source — KCON_AXIS_SRC_*; u8 flags — KCON_AXIS_INVERTED;
	 * u8 mods — KT_MOD_*; u32 time.
	 *
	 * BOTH NUMBERS, because a client reads one or the other and never
	 * both: value120 is what a modern toolkit steps by and `value` is what
	 * one that predates it scrolls by. A value of zero ends the gesture,
	 * which is what lets a guest stop its kinetic scrolling — so a zero is
	 * a message like any other and is never coalesced away.
	 *
	 * AN AXIS OR A SOURCE OUTSIDE ITS ENUM IS REFUSED. The far end maps
	 * these in a switch, and a scroll it had to guess the direction of is
	 * a page that moves the wrong way.
	 */
	KCON_OP_AXIS_RAW,

	/*
	 * VIEW -> SESSION: THE LAYOUT THIS VIEW'S KEYBOARD IS RUNNING.
	 *
	 * A keycode means nothing without one. The session holds it, hands it
	 * to every embedded guest, and a guest then reads the person's own
	 * letters instead of the ones printed on an American keyboard.
	 *
	 * u8 format — KCON_KEYMAP_XKB_V1, xkb's text format and the only one
	 * either end speaks; u32 length including the terminator, refused
	 * above KCON_KEYMAP_MAX and refused when the last byte is not one;
	 * then that many bytes.
	 *
	 * IT TRAVELS AS BYTES AND NOT AS A DESCRIPTOR, which is what keeps
	 * this socket forwardable: a view may be at the far end of an ssh
	 * link, where a descriptor cannot cross at all. It is sent when raw
	 * input is first asked for and again whenever the layout changes — not
	 * at hello, because a session that never opens a guest would have paid
	 * for it on the link for nothing.
	 */
	KCON_OP_KEYMAP,

	/*
	 * SESSION -> VIEW: START, OR STOP, SENDING RAW INPUT.
	 *
	 * u8, 1 on and 0 off. Off is where every view starts, so a view that
	 * never hears this sends nothing.
	 *
	 * THE SESSION ASKS ONLY WHEN SOMETHING CAN USE IT — an embedded pixel
	 * guest has the focus — because the raw stream is one message per
	 * device event and everything else on this desktop is drawn in cells.
	 */
	KCON_OP_VIEW_RAW,

	/*
	 * SHELL -> SESSION: RUN A NAMED VERB OF THE SESSION'S OWN.
	 *
	 * The payload is the verb's name as it appears in `keys.conf`: `tile`,
	 * `cascade`, `show-desktop`, `windows`, `lock`. Not a window's — those
	 * are KCON_OP_ACTIVATE, KCON_OP_CLOSE_REQUEST and KCON_OP_WIN_STATE,
	 * which name one toplevel — but the ones that act on the desktop, and
	 * which until now a pointer could not ask for at all: they were bound
	 * to chords and to nothing else, so a person using a mouse had no way
	 * to tile their windows.
	 *
	 * A NAME AND NOT A CHORD. A surface that could send a chord could send
	 * ANY chord, including whichever one the machine's `keys.conf` happens
	 * to have put a shell command on; a name is looked up in the bind table
	 * and anything the session does not recognise is dropped.
	 *
	 * A SHELL SURFACE ONLY, the gate every management verb keeps.
	 */
	KCON_OP_ACTION,

	KCON_OP_N
};

/*
 * How long a frame boundary is waited for before the wait is given up: a
 * session under load, or a peer that predates the op, must not freeze the
 * other end for good. The same figure libkwl gives a compositor's frame
 * callback.
 */
#define KCON_FRAME_STALL_MS 100

/* What KCON_OP_KEYMAP's bytes are written in. One value, because xkb's text
 * format is the one thing both a view's keyboard and a guest's compiler
 * speak. */
enum { KCON_KEYMAP_XKB_V1 = 1 };

/*
 * The most rectangles an input region may name. Every surface in this tree
 * asks for all of itself or for none of itself; the list exists so a surface
 * with a hole in it is expressible, not because one has been built.
 */
#define KCON_INPUT_RECTS 16

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
 * KCON_VIEW_FONT says the view rasterises its own glyphs and can be asked to
 * change their size. A view inside somebody's terminal cannot: that terminal
 * owns the font, the session is a guest in it, and the only honest answer to
 * the chord is to say so — which the session can only do because this flag
 * arrives before the chord is ever pressed.
 */
#define KCON_VIEW_FONT 0x2u

/*
 * KCON_VIEW_COLOR says the view can draw a colour outside the theme's eight
 * slots, so the session may send the literals a terminal's own cells carry.
 *
 * It is asked for rather than assumed because the answer is not the session's
 * to guess: a view inside somebody's sixteen-colour terminal would have to
 * reduce every literal back to a slot after paying for it on the wire, and a
 * link slow enough to make that hurt is exactly the link a remote view is on.
 */
#define KCON_VIEW_COLOR 0x4u

/*
 * KCON_VIEW_FRAME says the view answers every frame with KCON_OP_FRAME once it
 * has painted it. The session then sends the next frame when this view is
 * ready for it, which is what makes an animation arrive at the display's own
 * rate rather than at a timer's. A view without it is paced by the session's
 * clock and its queue depth.
 */
#define KCON_VIEW_FRAME 0x8u

/*
 * KCON_VIEW_RAW says the view holds a real keyboard and a real pointing
 * device — evdev codes, an xkb keymap and libinput's own deltas — and can
 * report them BESIDE the cells it already reports. A view inside somebody's
 * terminal has none of that and never claims it: what reaches that terminal
 * is characters and a cell, so a character and a cell is all it can forward.
 *
 * IT IS NOT A PROMISE TO SEND. The session asks, with KCON_OP_VIEW_RAW, and
 * asks only while an embedded window has the focus — raw motion is one
 * message per device event, and a view that sent it unasked would put a
 * thousand messages a second down a link that may be ssh.
 *
 * A VIEW THAT NEVER CLAIMS IT IS DRIVEN ENTIRELY BY THE COOKED STREAM. It is
 * never asked, so it sends no raw message and no keymap; its input arrives as
 * KCON_OP_KEY and KCON_OP_PTR and the session drives a pixel guest from those
 * exactly as it drives a cell window. Every capability here is asked for and
 * none is assumed, which is what makes a view that claims nothing a view the
 * session can still drive.
 */
#define KCON_VIEW_RAW 0x10u

/*
 * The most keymap a view may send. Far above any real layout — a full xkb
 * text keymap is tens of kilobytes — and far below KCON_MAX_PAYLOAD, because
 * a length field is an allocation request from a peer that may be remote.
 */
#define KCON_KEYMAP_MAX (256u << 10)

/*
 * THE HIGHEST EVDEV CODE THAT CROSSES, keys and pointer buttons alike: they
 * share one number space and BTN_LEFT is a key code like any other. It is
 * evdev's own KEY_MAX, written out rather than included, because libkcon
 * links no Linux input header and the view that sends it may be another
 * machine.
 *
 * A higher code is refused at the server. Whatever holds a bit per key — the
 * set of presses a window is owed a release for — is sized from this, so a
 * code the wire admitted and that table cannot hold is a write outside it.
 */
#define KCON_KEYCODE_MAX 767

/*
 * Which way a scroll went, and what made it. The session forwards these to a
 * pixel guest; nothing drawn in cells reads them, because a cell scrolls by
 * whole lines and has no second axis to scroll along.
 */
enum { KCON_AXIS_VERT = 0, KCON_AXIS_HORIZ = 1 };
enum {
	KCON_AXIS_SRC_WHEEL = 0,
	KCON_AXIS_SRC_FINGER,
	KCON_AXIS_SRC_CONTINUOUS,
	KCON_AXIS_SRC_WHEEL_TILT,
	KCON_AXIS_SRC_N
};
/* The device reverses the direction itself — natural scrolling. It is the
 * guest's to know, because the guest draws the scrollbar. */
#define KCON_AXIS_INVERTED 0x1u

/*
 * WHAT A VIEW MAY DO, sent after its capabilities and separate from them: a
 * capability is what a display can show and this is what it is allowed to
 * send. A view that says nothing is a driver, which is what every view was
 * before an observer existed.
 *
 * A CLIENT CAN ONLY ASK FOR LESS. Saying "observe" is a client holding itself
 * to something, and the server holds it there — the refusal is on the server's
 * side of the socket, so a view that changed its mind after the hello gets
 * nothing through.
 *
 * AND A VIEW MAY SAY ONLY WHAT A DISPLAY HAS TO SAY, whatever rights it asked
 * for: its hello, the input it carries, the frame it painted, what it was
 * pasted, what it can show, and that it is leaving. Every other client verb is
 * dropped on arrival — the clipboard, drags and the session's picture slots
 * included. The view socket is the one that may be forwarded, so the far end
 * of it is not this machine, and a list of what is ALLOWED is what keeps a
 * verb added later from reaching it by default.
 */
enum { KCON_RIGHTS_DRIVE = 0, KCON_RIGHTS_OBSERVE = 1 };


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
/*
 * DONE WITH A MESSAGE, AND THE BUFFER IS KEPT FOR THE NEXT ONE. Empties it as
 * kcon_buf_reset does and releases its memory only if it grew past `keep`.
 *
 * What a sender calls where it would otherwise free: kcon_send COPIES the
 * payload into the connection's own queue, so nothing holds a pointer into a
 * KconBuf once the send returns and the same buffer may be refilled at once.
 * A buffer retired instead of freed BELONGS TO ONE CONNECTION: on the server
 * it is a field of the KconSurface, and in a client it is a field of the one
 * connection that process holds. What breaks the rule is a buffer SHARED
 * between two connections, or between two senders filling at once — the
 * second filler would send the first one's bytes — not the storage class it
 * happens to have.
 */
void kcon_buf_retire(KconBuf *b, size_t keep);
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

/*
 * The literals of the same run: three bytes each for the foreground, the
 * background and the underline, then one byte of the attribute bits that say
 * which of them mean anything and what shape the underline is.
 *
 * TEN BYTES A CELL, PAID ONLY BY A VIEW THAT ASKED. The record is separate
 * from the cell's rather than an eight-byte record grown to eighteen, because
 * the cell record is what every commit costs and most cells on a desktop are
 * chrome in slots.
 */
#define KCON_COLOR_BYTES 10

/* Whether any cell in the run carries a literal at all — a run of none is not
 * sent. */
int kcon_run_has_color(const KtuiCell *cells, uint16_t n);
int kcon_put_color_run(KconBuf *b, uint16_t x, uint16_t y,
		       const KtuiCell *cells, uint16_t n);
/*
 * Reads a colour run into `out`: the three colours, and in `attr` ONLY the
 * bits above the wire's byte. The caller merges them into the cells it already
 * has, because the low byte is the commit's and this message must not be able
 * to change it — a run that could rewrite an attribute would be a second
 * sender for the same field. Returns the count, or -1.
 */
int kcon_get_color_run(KconRd *r, uint16_t *x, uint16_t *y, KtuiCell *out,
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
/* The socket send buffer the kernel GRANTED, in bytes, or 0 if it could not
 * be read. Not KCON_SOCK_BUF: the kernel doubles the request and clamps it to
 * net.core.wmem_max, so the only honest source for the number is the socket. */
int kcon_conn_sndbuf(const KconConn *c);
/* Bytes queued and not yet written. What a caller with something optional to
 * send asks before sending it. */
size_t kcon_conn_pending(const KconConn *c);

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
enum { KCON_LISTEN_ANY = 0, KCON_LISTEN_SURFACE, KCON_LISTEN_VIEW,
       /*
	* A READER'S SOCKET. Its clients are views that may not drive — the
	* rights field is not theirs to choose here, it is what this socket
	* means — and they are the only ones sent KCON_OP_ANNOUNCE. A reader
	* is handed the composed cells like any display, so reading the screen
	* is a loop over a buffer that already exists rather than a tree of
	* objects somebody hopes matches what was drawn.
	*/
       KCON_LISTEN_A11Y };

#define KCON_MAX_LISTEN 4

/* How many sprite slots one surface may name. The same sixteen bits the cell
 * encoding carries, capped where a table of ints is still small. */
#define KCON_MAX_SPRITE_MAP 4096

/*
 * HOW MANY FACES A VIEW MAY LIST, and how long one name may be.
 *
 * A person picks from a list they can read, and a screen shows a few dozen
 * rows; the cap is here because the count on the wire is an allocation request
 * from a peer, and because the picker's own row store is a fixed table. A view
 * with more faces than this sends the first of them, which is a shorter list
 * and not a broken one.
 *
 * The name length is fontconfig's own worst case with room to spare: a family
 * plus a size plus a style is far inside it.
 */
#define KCON_MAX_FONTS 64
#define KCON_FONT_NAME 192

/*
 * HOW MANY SCREENS ONE SESSION LIGHTS, and how many modes one of them may
 * offer. The first matches libkkms's own cap; the second is a monitor's list
 * with room to spare, and a longer one is truncated — a shorter picker, not a
 * broken one. The name is a connector's, `HDMI-A-1` and the like.
 */
#define KCON_MAX_OUTS 8
#define KCON_MAX_MODES 64
#define KCON_OUT_NAME 32

/*
 * ONE SCREEN AND ITS MODES TOGETHER. A mode list without the screen it belongs
 * to is a picker that can draw a list it cannot say which monitor is for.
 */
typedef struct {
	char name[KCON_OUT_NAME];
	int col, cols;		/* its slice of the shared grid, in cells  */
	int width, height;	/* its mode, in pixels                     */
	int cur_mode, nmodes;
	struct {
		int width, height;
		int refresh;	/* millihertz                              */
	} mode[KCON_MAX_MODES];
} KconOut;

/*
 * A KEY, A POINTER AND A SCROLL AS THE DEVICE REPORTED THEM. Structs rather
 * than fourteen arguments, and prefixed because they are exported.
 *
 * Every field is already bounded when a hook sees one: the server refuses a
 * keycode or a button above KCON_KEYCODE_MAX, a cell size of zero and an axis
 * or a source outside its enum, so the session may divide by a cell size and
 * index by a code without checking either again.
 */
typedef struct {
	int code;		/* evdev, NOT +8                          */
	int state;		/* 1 pressed, 0 released                  */
	unsigned depressed, latched, locked, group;
	unsigned ms;
} KconKeyRaw;

typedef struct {
	int x, y;		/* the view's own pixels, absolute        */
	int cell_w, cell_h;	/* the cell those pixels were measured in */
	int dx, dy;		/* 1/256 pixel, accelerated               */
	int dx_un, dy_un;	/* 1/256 pixel, as the device reported    */
	int button;		/* evdev BTN_*, 0 for motion alone        */
	int state;		/* 1 pressed, 0 released                  */
	int mods;		/* KT_MOD_*, for the SESSION's routing    */
	unsigned ms;
} KconPtrRaw;

typedef struct {
	int value;		/* 1/256 pixel                            */
	int value120;
	int axis;		/* KCON_AXIS_*                            */
	int source;		/* KCON_AXIS_SRC_*                        */
	int flags;		/* KCON_AXIS_INVERTED                     */
	int mods;
	unsigned ms;
} KconAxisRaw;

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
 * what everything else gets. It is for an application the card cannot be given
 * to through a window — one that sets its own full-screen mode, or whose driver
 * will not run against a headless output. An embedded guest is composited by
 * the software renderer unless its box profile says `render = gpu`.
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

/*
 * A CLIENT'S OWN DESCRIPTOR, so a caller's poll can wake on a commit rather
 * than on its next tick. Without it a session polls its listeners only and a
 * window's redraw waits for the timeout — which is a window that updates at
 * the tick rate however fast the program inside it is writing.
 */
int kcon_surface_fd(const KconSurface *f);
void kcon_server_free(KconServer *s);
int kcon_server_fd(const KconServer *s);

/*
 * Accept what is waiting and read every client. Returns the number of surfaces
 * whose cells changed SINCE THE LAST PUMP, so a caller knows whether it has
 * anything to redraw; the count is cleared by the pump that reports it. Never
 * blocks, and never blocks ON a client: one that stopped reading is dropped.
 */
int kcon_server_pump(KconServer *s);

/*
 * The frame just composed is done: every surface whose commit or picture it
 * carried is sent a KCON_OP_FRAME. Called by the session once per composed
 * frame, after the flush that sent it. See KCON_OP_FRAME.
 *
 * A VIEW IS MARKED AS OWING ONE BY kcon_view_send(), not here, and only when
 * the frame it was sent actually carried runs. Pairing the mark with the send
 * is what keeps frame_at — and so kcon_view_ready()'s stall timeout — honest;
 * marking a view that was sent nothing would withhold the next frame from an
 * idle desktop for KCON_FRAME_STALL_MS.
 */
void kcon_server_frame_done(KconServer *s);

int kcon_server_count(const KconServer *s);
KconSurface *kcon_server_at(KconServer *s, int i);

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
/*
 * WHERE AN OVERLAY ASKED TO SIT, as `enum kdisp_corner`, with its margins from
 * the two edges that corner names.
 *
 * The unit is a CELL on this transport and a pixel on Wayland, and the two are
 * the same number in a caller: `kdisp_cell_w()` answers 1 here, so a surface
 * computing "x cells from the left" in the toolkit's own units produces cells
 * here and pixels there without branching. A corner is how layer-shell says
 * "at x" — it has no coordinates — and carrying the same field means a menu
 * asks once and lands beside its button on both desktops.
 */
int kcon_surface_corner(const KconSurface *f);
/* True when this surface asked to open unanchored, at the size it attached
 * with, where the eye is. Toplevel only — an overlay is already unanchored. */
int kcon_surface_floating(const KconSurface *f);
/*
 * WHERE THIS SURFACE SAYS ITS CARET IS, in its own cells. Returns 0 and
 * touches nothing when it has none — which is every surface until it sends
 * one, so a session that forwards this without checking would park a cursor
 * at a corner nobody is typing in.
 */
int kcon_surface_caret(const KconSurface *f, int *x, int *y);
int kcon_surface_margin_x(const KconSurface *f);
int kcon_surface_margin_y(const KconSurface *f);

/*
 * THE SMALLEST GRID THIS SURFACE CAN COMPOSE ON, in cells, or zero when it
 * did not say. Reported at attach because the session is the only thing that
 * can act on it: a window manager that hands a surface fewer cells than it
 * needs gets a frame that was never composed, and the cells under it keep the
 * last program's picture — a hole in the desktop rather than a clipped window.
 */
int kcon_surface_min_cols(const KconSurface *f);
int kcon_surface_min_rows(const KconSurface *f);
/*
 * A PANEL'S THICKNESS ACROSS ITS EDGE, in cells, and zero from anything else.
 * It is the whole size a docked surface asks for: the extent along the edge is
 * the screen's, which the client cannot know, so the session answers with a
 * configure. A surface that named a thickness attaches with no size.
 */
int kcon_surface_want_cells(const KconSurface *f);
int kcon_surface_exclusive(const KconSurface *f);
/*
 * WHETHER THIS SURFACE ASKED FOR THE KEYBOARD. A surface that predates the
 * field is reported as wanting it, which is what every surface did then.
 *
 * A session focuses an overlay that says yes and never one that says no: a
 * tooltip or a toast that took the focus would pull it off the window under
 * it, and anything that closes when it loses the focus — every menu on this
 * desktop — would close the moment a tip appeared beside it.
 */
int kcon_surface_keyboard(const KconSurface *f);
/*
 * TELL A SURFACE WHETHER THE SESSION DREW ITS FRAME. Sent on change only —
 * the session decides, the client draws its own box when the answer is no.
 * See KCON_OP_DECORATED.
 */
void kcon_surface_decorated(KconSurface *f, int on);
/*
 * THE CELLS OF THIS SURFACE THAT ANSWER THE POINTER.
 *
 * `kcon_surface_input_n()` is -1 for all of it (the default), 0 for none, or
 * the count of rectangles `kcon_surface_input_at()` reports, in the surface's
 * OWN cells — the caller adds the window's origin. A hit test that ignores
 * this makes a tooltip swallow the click on the button it is describing.
 */
int kcon_surface_input_n(const KconSurface *f);
int kcon_surface_input_at(const KconSurface *f, int i, KRect *out);
/* True while the surface says it has nothing to show. A display draws it
 * nowhere and lists it nowhere; it is still a client and still attached. */
int kcon_surface_hidden(const KconSurface *f);
/* The surface's own grid, cols by rows. Borrowed, and valid until the next
 * pump — a configure reallocates it. */
const KtuiCell *kcon_surface_cells(const KconSurface *f);

void kcon_surface_configure(KconSurface *f, int cols, int rows);
void kcon_surface_key(KconSurface *f, int key, int mods);
/* The keyboard focus arrived (1) or left (0). See kcon_surface_focus(). */
void kcon_surface_focus(KconSurface *f, int in);
void kcon_surface_ptr(KconSurface *f, int x, int y, int btn, int press);
/*
 * A FINGER ON THIS SURFACE, in its cells, carrying the gesture the VIEW'S
 * recogniser named — `phase` is KT_TOUCH_*, `gesture` is KT_GEST_*. A surface
 * that wants only a pointer needs none of it: the view synthesises the mouse
 * event beside the touch, so every widget already handles a tap.
 */
void kcon_surface_touch(KconSurface *f, int x, int y, int slot, int phase,
			unsigned ms, int gesture);
/*
 * A DRAG ACROSS THE SESSION, in four verbs.
 *
 * The session is the only half that knows what is under the pointer, so it is
 * the half that sends all four: a surface says it has picked something up with
 * KCON_OP_DRAG_START and hears nothing more until the drag is over it.
 *
 * Where a drag is, before it is dropped. `enter` carries the MIME type because
 * that is what a target refuses on; the payload waits for the drop, so a drag
 * crossing six windows does not hand its bytes to all six.
 */
void kcon_surface_drag_enter(KconSurface *f, int x, int y, const char *mime);
void kcon_surface_drag_motion(KconSurface *f, int x, int y);
void kcon_surface_drag_leave(KconSurface *f);
void kcon_surface_drop(KconSurface *f, int x, int y, const char *text);
void kcon_surface_clip_data(KconSurface *f, const char *text);
/* Tell a lock surface whether it holds the session. Flushed rather than
 * queued: the client refuses every keystroke until this arrives, so a byte
 * sitting in a send buffer is a lock screen that cannot be answered. */
void kcon_surface_lock_state(KconSurface *f, unsigned flags);
void kcon_view_clip(KconServer *s, const char *text);
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
/* True for a view that asked to observe. Input from one is dropped by the
 * server, so a view that changed its mind after the hello gets nothing
 * through. */
int kcon_view_observing(const KconSurface *v);
/*
 * Bytes queued for this view, and a push of whatever is queued.
 *
 * A caller cutting a picture into pieces asks between them: the queue is what
 * says whether the display is keeping up, and flushing between pieces is what
 * lets it drain instead of the whole picture arriving as one lump. Above
 * KCON_VIEW_HIGH the send calls below do nothing, which is the caller's cue to
 * keep the piece and offer it again.
 */
size_t kcon_view_pending(const KconSurface *v);
int kcon_view_flush(KconSurface *v);
/*
 * How many views this session admits at once; 0 is no limit. The NUMBER is the
 * caller's — a desktop's configuration, not a library's opinion — and the
 * enforcement is the server's, because it is the end that sees a view arrive.
 */
void kcon_server_view_max(KconServer *s, int n);

/*
 * One announcement, to every reader attached. The role and the counts are
 * libktui's `KtuiA11y`; the rectangle is the focused window's and is zero for
 * a record that is not one.
 *
 * Nothing is sent when no reader is listening, which is the usual case: a
 * desktop nobody is reading pays a comparison per frame.
 */
void kcon_a11y_announce(KconServer *s, int role, const char *label,
			const char *value, int index, int count, int x, int y,
			int w, int h);
/* How many readers are attached. */
int kcon_server_a11y_count(const KconServer *s);

/*
 * Diff against this view's own last frame and send what changed: the cell
 * runs as KCON_OP_COMMIT messages, each followed by the KCON_OP_COLOR
 * carrying the same runs' literals, then one KCON_OP_FRAME. A message per
 * KCON_CHUNK_BYTES rather than one per run, because a run is a socket write
 * and an animation is hundreds of runs a frame — and cells before colours,
 * because a colour record patches a cell the commit placed.
 *
 * Nothing is sent, and the view's copy of the previous frame is untouched,
 * when kcon_view_ready() says no; the next frame the view can take carries
 * everything the skipped one would have. A reader — an accessibility
 * listener — is sent nothing at all: it reads only KCON_OP_ANNOUNCE.
 *
 * A frame that could not be encoded or could not be sent whole disowns the
 * previous-frame copy, so the next one is sent full rather than as a diff
 * against cells the display never received.
 */
void kcon_view_send(KconSurface *v, const KtuiCell *cells, int w, int h);
/*
 * Whether this view may be sent a frame now: not further behind than
 * KCON_VIEW_HIGH, and — for a view that answers frames — not still painting
 * the last one, unless that answer is more than KCON_FRAME_STALL_MS overdue.
 * The session composes when some view says yes and not otherwise, which is
 * what keeps its frame rate the display's.
 *
 * A READER IS NEVER READY. It is a view for counting, for gating and for
 * detach, but it is sent no cells at all — so counting it here would answer
 * yes for ever and there would be no pacing left.
 */
int kcon_view_ready(const KconSurface *v);
/*
 * Where the caret is. It is recorded, not sent: the message goes out from
 * kcon_view_send behind that frame's cells, so the caret never lands on a
 * picture the view has not been given, and a skipped frame skips it too.
 */
void kcon_view_cursor(KconSurface *v, int x, int y);

/* Ask a view to power its screen down (1) or back up (0). */
void kcon_view_blank(KconSurface *v, int on);

/*
 * ASK THIS VIEW FOR RAW INPUT, or stop. Silently nothing on a view that did
 * not claim KCON_VIEW_RAW, the rule every other capability-gated call keeps.
 *
 * ASK ONLY WHILE SOMETHING READS IT. The stream is one message per device
 * event — a pointer at a thousand hertz is a thousand messages a second — so
 * a session that left it on after the last pixel guest lost the focus spends
 * a display's link on input nothing consumes.
 */
void kcon_view_raw(KconSurface *v, int on);

/*
 * Ask a view to step its font: +1 bigger, -1 smaller, 0 back to the one it
 * started with. Silently nothing on a view that did not claim KCON_VIEW_FONT,
 * so a caller that checked the flag and one that did not behave alike.
 */
void kcon_view_font(KconSurface *v, int step);

/*
 * ASK A VIEW WHAT FACES IT HAS, and tell one to wear the nth of them.
 *
 * Silently nothing on a view that did not claim KCON_VIEW_FONT, the rule
 * kcon_view_font() keeps. The answer arrives on the `view_fonts` hook, once,
 * whenever the view feels like sending it — a caller that blocked for it would
 * be a session stopped on a display that may be at the far end of an ssh
 * link.
 */
void kcon_view_fonts_ask(KconSurface *v);
void kcon_view_set_font(KconSurface *v, int index, int keep);

/* The same pair for screens. Silently nothing on a view that did not claim
 * KCON_VIEW_FONT — a display that does not rasterise its own glyphs is a
 * display inside somebody's terminal, which has no modes to offer. */
void kcon_view_outputs_ask(KconSurface *v);
void kcon_view_set_mode(KconSurface *v, int out, int mode, int keep);
/* The list, back to the shell that asked. Empty is the honest answer where no
 * attached display drives a screen. */
void kcon_surface_outputs(KconSurface *f, const KconOut *outs, int n);

/*
 * THE LIST, BACK TO THE SHELL THAT ASKED. `cur` is which of them is in force
 * or -1, and an empty list is the honest answer where no attached display
 * rasterises its own glyphs — the terminal it runs in owns the font.
 */
void kcon_surface_fonts(KconSurface *f, const char *const *names, int n,
			int cur);

/*
 * Ring every attached view. A bell is not addressed to one display: the person
 * is sitting at whichever of them they are sitting at.
 */
void kcon_view_bell(KconServer *s);

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
	 * THE SAME PHYSICAL INPUT, UNRESOLVED. A cell desktop reads the hooks
	 * above and nothing else; these exist for the one thing on it that is
	 * not cells. The cooked hook for the same event has already run, so
	 * the session has decided which window the pointer is over and whether
	 * a chord swallowed the key before either of these is called.
	 *
	 * They are called only for a view that claimed KCON_VIEW_RAW and was
	 * asked with kcon_view_raw(); a display that claimed neither reaches
	 * the session through the cooked hooks alone.
	 */
	void (*view_key_raw)(KconSurface *v, const KconKeyRaw *k, void *user);
	void (*view_ptr_raw)(KconSurface *v, const KconPtrRaw *p, void *user);
	void (*view_axis_raw)(KconSurface *v, const KconAxisRaw *a,
			      void *user);
	/*
	 * A VIEW SAID WHAT ITS KEYBOARD IS. `text` is borrowed for the length
	 * of the call, is NUL-terminated and is xkb's text format; `len`
	 * counts the terminator. One session is one keyboard, so the last view
	 * to say wins.
	 */
	void (*view_keymap)(KconSurface *v, int format, const char *text,
			    size_t len, void *user);

	/*
	 * A surface sent a picture. `slot` is the SESSION's, already mapped;
	 * `argb` is borrowed and is `pw` by `ph` pixels. A session with no
	 * pixel code forwards it to its views and looks at nothing.
	 */
	void (*sprite)(KconSurface *f, int slot, int w, int h,
		       uint32_t fallback, const uint32_t *argb, int pw, int ph,
		       void *user);

	/*
	 * A MANAGING SURFACE ASKED FOR ONE OF THE SESSION'S OWN VERBS, by the
	 * name `keys.conf` gives it. `verb` is borrowed for the length of the
	 * call. A session with no such verb does nothing, which is what a
	 * surface built against a newer bind table must get.
	 */
	void (*action)(KconSurface *f, const char *verb, void *user);

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

	/*
	 * Save or load a layout by name. `save` is non-zero to write what is
	 * open and zero to put a saved arrangement back. Returns how many
	 * windows were written or opened, or -1 when the name could not be
	 * used at all — the session decides both, because it is the half that
	 * holds the windows.
	 */
	int (*layout)(KconSurface *f, const char *name, int save, void *user);
	/* Enter the colour pick. Nothing is answered: the session finishes the
	 * interaction itself. */
	void (*pick)(KconSurface *f, void *user);

	/*
	 * END THE SESSION. The server does not decide this: it holds the
	 * listeners and the surfaces, and what a quit means — draining, saying
	 * goodbye, leaving the run directory clean — belongs to whoever runs
	 * the loop.
	 */
	void (*quit)(KconSurface *f, void *user);

	/*
	 * A SHELL ASKED FOR A WINDOW. Raising and closing are the session's to
	 * do — it owns the stack and the lifetime — so these carry the request
	 * rather than performing it.
	 */
	void (*activate)(KconSurface *f, unsigned id, void *user);
	void (*close_request)(KconSurface *f, unsigned id, void *user);
	void (*win_state)(KconSurface *f, unsigned id, unsigned flag, int on,
			  void *user);

	/*
	 * TEXT ARRIVED AT A DISPLAY. `text` is borrowed and NUL-terminated;
	 * where it lands is the session's, because only the session knows
	 * which window has the focus.
	 */
	void (*paste)(KconSurface *v, const char *text, void *user);

	/*
	 * WHAT IS ON THE SCREEN RIGHT NOW, as text. `window` is 0 for the
	 * whole grid or a window's ring number; the session composes it,
	 * because it owns the frame and the rectangles, and returns a
	 * malloc'd string THE SERVER FREES. NULL is "there is nothing to
	 * show", which is what a number naming no window means.
	 */
	char *(*capture)(KconSurface *f, int window, void *user);

	/*
	 * A FINGER ARRIVED AT A DISPLAY, with the gesture ITS recogniser
	 * named. There is one recogniser and it lives where the touch device
	 * is, so what reaches here is a verdict rather than geometry a second
	 * one would disagree with. `phase` is KT_TOUCH_*, `gesture` KT_GEST_*.
	 */
	void (*view_touch)(KconSurface *v, int x, int y, int slot, int phase,
			   unsigned ms, int gesture, void *user);

	/*
	 * A DISPLAY COULD NOT KEEP A PICTURE IT WAS SENT. `slot` is the
	 * session's, already bounded against KCON_MAX_SPRITE_MAP, and the hook
	 * is called once per slot however many one message carried.
	 *
	 * Whatever owns that slot owes it to this view again, and must bound
	 * how often it pays BY TIME. See KCON_OP_SPRITE_LOST: a view whose
	 * table is too small for the picture loses the replacement as well, so
	 * a repair with no limit is the same bytes for ever — while a limit
	 * restored only by the owner drawing again is not a limit but an
	 * expiry, and a picture lost after a window has settled is then a hole
	 * for the life of that window.
	 */
	void (*view_sprite_lost)(KconSurface *v, int slot, void *user);

	/*
	 * A VIEW LISTED THE FACES IT CAN RENDER. `names` is borrowed and is
	 * `n` NUL-terminated strings; `cur` is which of them is in force, or
	 * -1 when the view could not say.
	 *
	 * The names are the VIEW'S and mean nothing here — the session stores
	 * them to show and sends back an INDEX, never one of them, because a
	 * forwarded display's fonts are its own machine's.
	 */
	void (*view_fonts)(KconSurface *v, const char *const *names, int n,
			   int cur, void *user);

	/*
	 * A VIEW LISTED THE SCREENS IT IS DRIVING. `outs` is borrowed and is
	 * `n` records; the session stores them to show and sends back INDICES,
	 * never a mode.
	 */
	void (*view_outputs)(KconSurface *v, const KconOut *outs, int n,
			     void *user);
	/* A shell asked for that list, on the same op. */
	void (*outputs_ask)(KconSurface *f, void *user);
	/* And wear the nth mode on the nth screen; `keep` is whether it
	 * survives the logout, so a picker's countdown passes 0 until a person
	 * says it is readable. */
	void (*mode_set)(KconSurface *f, int out, int mode, int keep,
			 void *user);

	/*
	 * A SHELL ASKED FOR THE SAME LIST, on the same op — the shape
	 * KCON_OP_CAPTURE already uses, because a request and its answer are
	 * one verb asked in two directions.
	 *
	 * The session answers with kcon_surface_fonts() out of what a view
	 * last told it. It does NOT gather anything itself: the list belongs
	 * to the display, and a session with no view attached has no list to
	 * give, which is an empty one and not an error.
	 */
	void (*fonts_ask)(KconSurface *f, void *user);
	/* And wear the nth of them. -1 is back to the one the view started
	 * with; `keep` is whether it survives the logout, so a picker's
	 * arrows pass 0 and only its Enter passes 1. */
	void (*font_set)(KconSurface *f, int index, int keep, void *user);
} KconServerHooks;

void kcon_server_hooks(KconServer *s, const KconServerHooks *h, void *user);

enum {
	KCON_LOCK_ENGAGED = 1u << 0,
	KCON_LOCK_FINISHED = 1u << 1,
};

/* What a toplevel is doing, carried by KCON_OP_TOPLEVEL_STATE. */
enum {
	KCON_TL_FOCUSED = 1u << 0,
	KCON_TL_MINIMISED = 1u << 1,
	KCON_TL_MAXIMISED = 1u << 2,
	KCON_TL_FULLSCREEN = 1u << 3,
};

/*
 * The window list, out to every shell surface. Sent by the session when its
 * own state changes; a shell that has just attached is sent the whole list.
 */
void kcon_mgmt_add(KconServer *s, unsigned id, const char *app_id,
		   const char *title);
void kcon_mgmt_state(KconServer *s, unsigned id, unsigned flags,
		     int workspace);
void kcon_mgmt_remove(KconServer *s, unsigned id);
void kcon_mgmt_workspace(KconServer *s, int current, int count,
			 unsigned occupied);

/*
 * The client half: what a panel has been told, and what it can ask for. The
 * list is the library's, so every consumer sees the same one.
 */
typedef struct {
	unsigned id;
	unsigned flags;
	int workspace;
	char app_id[64];
	char title[128];
} KconToplevel;

int kcon_toplevel_count(void);
const KconToplevel *kcon_toplevel_at(int i);
int kcon_workspace_current(void);
int kcon_workspace_count(void);
unsigned kcon_workspace_occupied(void);

/* Raise it, or ask it to close. Both are requests: the session decides. */
/* Ask the session to run one of ITS OWN verbs, by the name `keys.conf` gives
 * it — `tile`, `cascade`, `show-desktop`, `windows`, `lock`. A name and never
 * a chord: see KCON_OP_ACTION. Silently nothing off a shell connection. */
void kcon_session_action(const char *verb);

void kcon_toplevel_activate(unsigned id);
void kcon_toplevel_close(unsigned id);

/* One KCON_TL_ bit, and the value wanted rather than a toggle. See
 * KCON_OP_WIN_STATE. */
void kcon_toplevel_state(unsigned id, unsigned flag, int on);

/* Ask a session to release every view. Connects, asks and closes. */
int kcon_detach_all(const char *sock);

/*
 * Ask a session what is on its screen. Connects as a shell, asks, waits for
 * the answer and closes; `*out` is a malloc'd string the caller frees.
 *
 * `window` is 0 for the whole grid or a window's ring number — the same number
 * the title bar and `Super+Alt+N` use, because a person capturing "window 2"
 * means the one labelled 2.
 */
int kcon_capture(const char *sock, int window, char **out);

/* Ask a session to end. Connects, asks and closes; the session drains its own
 * clients. Returns -1 when nothing is listening on `sock`. */
int kcon_quit_session(const char *sock);

/*
 * Ask a session to save what is open under `name`, or to put that arrangement
 * back. Returns how many windows were written or opened, or -1. `sock` is the
 * session's surface socket — $KDOS_CON.
 *
 * It waits for the same reason kcon_run does: "there is no layout by that
 * name" is the whole of what a person needs told, and only the session knows.
 */
int kcon_layout(const char *sock, const char *name, int save);

/*
 * Ask a session to pick the colour under the pointer. Connects, asks and
 * closes; nothing is answered, because the answer arrives when a person clicks
 * and the session puts it on its own clipboard. Returns -1 when nothing is
 * listening on `sock`.
 */
int kcon_pick_colour(const char *sock);

/*
 * Take a session's clipboard from a program that is not a surface. The console
 * has no `wl-paste` — its clipboard is the session's — so this is the other
 * half of kcon_clip_offer(). `*out` is a malloc'd string the caller frees.
 *
 * It waits, bounded, because the answer is the whole of what was asked. An
 * EMPTY clipboard is an answer and comes back as an empty string; only a
 * session that never replied is -1.
 */
int kcon_clip_take(const char *sock, char **out);

/*
 * Put text on a session's clipboard from a program that is not a surface. The
 * console has no `wl-copy` — its clipboard is the session's — so this is the
 * one way a command-line tool copies. Answers 0 when the offer was sent; there
 * is nothing to wait for, because an offer has no reply.
 */
int kcon_clip_offer(const char *sock, const char *text, size_t len);

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
/*
 * Give one back. The session's slots are a FREE MAP and not a counter: a
 * counter wrapped onto numbers still being drawn with, and one program's
 * picture then appeared inside another's window. A surface's slots go back
 * when it goes; a session that cuts its own pictures gives them back here.
 *
 * AND EVERY ATTACHED DISPLAY IS TOLD, because a number back in the rotation is
 * a number the rotation will hand out again — when the search comes round to
 * it, not when it is freed. A display keys its sprite table on the session
 * slot alone, and until the number comes round nothing will send a picture
 * under it, so a display that was never told holds those pixels in its own
 * byte budget with nothing that will ever replace them — and the eviction that
 * eventually takes them is reported as a loss of a slot that by then belongs
 * to somebody else, which spends that owner's repair allowance on a picture it
 * never lost.
 */
void kcon_server_free_slot(KconServer *s, int slot);

/*
 * Forward a sprite's pixels to a view. The session holds no pixel code and
 * does not look at them; it moves the blob it was given.
 *
 * Answers 0 whenever the picture did not reach the wire — the view is already
 * more than KCON_VIEW_HIGH bytes behind, the pixels pass KCON_MAX_PAYLOAD, or
 * the connection is gone. A picture has no previous copy to diff against, so
 * the caller must keep the piece and offer it again rather than treat it as
 * delivered; a caller that chooses the pixel size must also keep a picture
 * inside KCON_MAX_PAYLOAD, because a block too large to encode is refused
 * every time it is offered.
 *
 * AND 1 IS THE WIRE, NOT THE SCREEN. The view has a byte budget for its own
 * sprite table and may refuse a picture whose bytes it took; it says so with
 * KCON_OP_SPRITE_LOST, which reaches the caller as the `view_sprite_lost`
 * hook. A caller that treats 1 as final leaves that block blank for the life
 * of the window.
 */
int kcon_view_sprite(KconSurface *v, int slot, int w, int h,
		     uint32_t fallback, const uint32_t *argb, int pw, int ph);

/*
 * con.conf — /etc/kdos/con.conf, overridden by ~/.config/kdos-con/con.conf.
 * Read once on the first lookup; a key in neither file takes `def`.
 */
const char *kcon_conf_str(const char *key, const char *def);
int kcon_conf_int(const char *key, int def);
int kcon_conf_bool(const char *key, int def);

#endif /* KCON_H */
