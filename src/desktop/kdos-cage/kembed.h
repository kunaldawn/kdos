/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kembed — the private channel between kdos-con and its kdos-cage child
 *
 * THE ONE FILE TWO PORTS SHARE, and it is a header with no code: kdos-cage
 * links no KDOS library but libkcolor, and kdos-con links no Wayland. Neither
 * may grow a dependency on the other, so the protocol they speak is a struct
 * definition and nothing else.
 *
 * IT IS NOT EITHER PUBLISHED PROTOCOL, and that is the point. The surface
 * socket and the view socket carry no file descriptors, which is exactly what
 * lets the view socket be forwarded over ssh. This one passes a shared-memory
 * descriptor — so it is private, local, and parent-to-child only. A process
 * that is not the parent cannot reach it: it is a socketpair created before
 * the fork and inherited, never a path anybody can connect to.
 *
 * NO SERIALISATION. Both ends are the same build on the same machine, one the
 * other's child, and the socket is SOCK_SEQPACKET so the kernel frames every
 * message. A wire codec here would be a second format to keep in step for a
 * channel with exactly two implementations that ship together.
 * ---------------------------------
 */

#ifndef KEMBED_H
#define KEMBED_H

#include <stdint.h>

/* The inherited descriptors. Numbered rather than passed, because a parent
 * that has already forked can simply dup2 them into place. */
#define KEMBED_FD 3

#define KEMBED_MAGIC 0x4b454d42u	/* "KEMB" */

/*
 * TWO FRAMES IN ONE MAPPING, PER WINDOW, and `slot` says which is current.
 * The child renders into the half the parent is not reading, then flips — so
 * a frame is never half old and half new, which single-buffering cannot
 * promise and which on a photograph reads as the compositor being broken.
 *
 * A MAPPING IS ONE WINDOW'S AND NEVER THE CHANNEL'S. Every mapping is
 * announced under the `win` it belongs to and every flip of it names that
 * window, so a window that redrew costs one message about itself and a window
 * that did not costs none — which one mapping shared across a process cannot
 * express, because every flip of it would be about every window. A guest
 * showing one window announces one mapping, under that window.
 */
#define KEMBED_SLOTS 2

/*
 * THE LONGEST NAME THAT TRAVELS, bytes, terminator included.
 *
 * A message may simply be LONGER than the struct: the socket is SOCK_SEQPACKET
 * and the kernel frames every datagram, so a string after the struct needs no
 * length field and no codec — the datagram boundary is its end. This is what
 * kdos-con's `Win.title` holds, so a name cut here is one that would be cut
 * there a copy later.
 */
#define KEMBED_TITLE_MAX 128

/*
 * THE LONGEST TAIL ANY OP CARRIES, and what BOTH ends size their receive
 * buffer at. A datagram that does not fit the buffer is truncated by the
 * kernel with no error anywhere, and a receiver that then compares the length
 * against the struct reads a truncated tail as a whole one.
 */
#define KEMBED_TAIL_MAX 256

/*
 * THE MOST SELECTION A TRANSFER CARRIES, and it is the session's own bound.
 * kdos-con's clipboard truncates at this constant itself rather than at a copy
 * of it, so a selection is cut once or not at all.
 */
#define KEMBED_CLIP_MAX (64u * 1024u)

/*
 * WHAT KIND OF WINDOW THE GUEST JUST OPENED, in KEMBED_OPEN's `d`.
 *
 * The window model belongs to the parent, and this end decides only whether a
 * toplevel is maximised into its own output or sized to it — so these are what
 * a placement, a stacking order and a focus decision are made from: a toolbox
 * that took the keyboard from a terminal, or a modal a person could click
 * behind, is a window model decided in the wrong process.
 *
 * ONLY DIALOG REACHES THE PARENT FROM A WAYLAND CLIENT, and the rules that read
 * these have to degrade to "owned or not" without the other three. xdg-shell's
 * toplevel state carries maximised, fullscreen, resizing, activated,
 * suspended, tiled, constrained and sizes — there is no modal, no utility and
 * no splash in the protocol to report — so a Wayland toplevel is a DIALOG when
 * it names an owner and nothing when it does not. X11 has all three, in
 * _NET_WM_STATE and _NET_WM_WINDOW_TYPE, so MODAL, UTILITY and SPLASH arrive
 * from an Xwayland guest alone.
 */
#define KEMBED_ROLE_DIALOG 0x1u
#define KEMBED_ROLE_MODAL 0x2u
#define KEMBED_ROLE_UTILITY 0x4u
#define KEMBED_ROLE_SPLASH 0x8u

/*
 * WHETHER THE GUEST HOLDS THE POINTER, in KEMBED_GRAB's `a`. A game and a
 * three-dimensional editor take the pointer away from the desktop and read
 * motion as a delta; the parent has to know, because while one is held the
 * console stops moving its own arrow, stops changing which window is hovered
 * and sends relative motion alone.
 */
enum {
	KEMBED_GRAB_NONE = 0,
	KEMBED_GRAB_LOCKED,	/* the pointer does not move at all      */
	KEMBED_GRAB_CONFINED,	/* it moves, inside the window only      */
};
/* `b`,`c` carry the position the guest asked the pointer to be left at. */
#define KEMBED_GRAB_HINT 0x1u

/*
 * WHERE A SCROLL CAME FROM, in KEMBED_AXIS's `d`, and the numbers are this
 * protocol's own. They are NOT wl_pointer's: kdos-con links no Wayland, so a
 * value that silently happened to equal an upstream enum would be a coupling
 * neither end could see, and the cage maps them in a switch.
 */
enum {
	KEMBED_AXIS_WHEEL = 0,
	KEMBED_AXIS_FINGER,
	KEMBED_AXIS_CONTINUOUS,
	KEMBED_AXIS_WHEEL_TILT,
};
/* The device reverses the direction itself — natural scrolling. It is the
 * guest's to know, because the guest draws the scrollbar. */
#define KEMBED_AXIS_INVERTED 0x100u

/* What KEMBED_KEYMAP's memfd holds. One value, because xkb's text format is
 * the one thing both a view's keyboard and the cage's compiler can speak. */
enum { KEMBED_KEYMAP_XKB_V1 = 1 };

enum {
	KEMBED_HELLO = 1,	/* child -> parent: the guest is up          */
	KEMBED_BUF,		/* child -> parent, WITH the memfd: a new
				 * mapping for this window, because its
				 * size changed                              */
	KEMBED_FRAME,		/* child -> parent: which slot, and what
				 * changed in it                             */
	KEMBED_GONE,		/* child -> parent: the application exited   */
	KEMBED_DAMAGE,		/* child -> parent: ANOTHER box of the frame
				 * just announced                            */
	KEMBED_TITLE,		/* child -> parent: the guest renamed a
				 * window                                    */
	KEMBED_FULLSCREEN,	/* child -> parent: the guest asked for the
				 * screen, or gave it back                   */
	KEMBED_OPEN,		/* child -> parent: a toplevel mapped, and
				 * this is what kind it is                   */
	KEMBED_CLOSE_WIN,	/* child -> parent: that toplevel is gone    */
	KEMBED_INHIBIT,		/* child -> parent: this window is playing
				 * something — do not blank the screen       */
	KEMBED_GRAB,		/* child -> parent: the guest took the
				 * pointer, or gave it back                  */
	KEMBED_CLIP_OFFER,	/* child -> parent, WITH a memfd: the guest
				 * copied                                    */
	KEMBED_DRAG_OFFER,	/* child -> parent, WITH a memfd: the guest
				 * began dragging something out              */

	KEMBED_SIZE = 64,	/* parent -> child: the window is this many
				 * pixels now                                */
	KEMBED_KEY,		/* parent -> child: a key went down or up    */
	KEMBED_MOTION,		/* parent -> child: pointer, in pixels       */
	KEMBED_BUTTON,
	KEMBED_AXIS,
	KEMBED_FOCUS,		/* parent -> child: the keyboard is this
				 * window's, or it is nobody's               */
	KEMBED_SLEEP,		/* parent -> child: nobody can see this
				 * window — stop rendering rather than
				 * render unseen                             */
	KEMBED_CLOSE,		/* parent -> child: go away                  */
	KEMBED_KEYMAP,		/* parent -> child, WITH the memfd: the
				 * layout the person is actually typing on   */
	KEMBED_MODS,		/* parent -> child: the locks and the group,
				 * which a key stream cannot establish       */
	KEMBED_REL,		/* parent -> child: how far the device moved,
				 * which a position cannot say               */
	KEMBED_LEAVE,		/* parent -> child: the pointer left this
				 * window                                    */
	KEMBED_FULLSCREEN_SET,	/* parent -> child: the window IS fullscreen
				 * now, or is not                            */
	KEMBED_CLIP_SET,	/* parent -> child, WITH a memfd: the
				 * session's selection                       */
	KEMBED_DRAG_ENTER,	/* parent -> child: something is being
				 * dragged over this window                  */
	KEMBED_DRAG_MOTION,
	KEMBED_DRAG_LEAVE,
	KEMBED_DROP,		/* parent -> child, WITH a memfd: it was
				 * dropped here                              */
};

/*
 * One message, fixed size. The fields are named per op below rather than by a
 * union: a union in a message a peer chose the tag of is a union whose active
 * member a peer chose.
 *
 * `win` IS WHICH WINDOW, AND ZERO IS THE CHANNEL. A guest is as many
 * toplevels as it maps and each one is a window on the parent's desktop, so
 * every op that is about a window carries which. Zero belongs to the channel
 * itself — HELLO, GONE, the keymap, the selection — and on an op that
 * addresses a window it names the front one, which is what lets a close asked
 * of the process reach the toplevel a person is looking at, and what a guest
 * showing one window sends and answers throughout. An end that holds more than
 * one window answers each op on the window `win` names, and drops one naming a
 * window it does not have while still closing any descriptor that op carried:
 * a window can go while a message about it is in flight, and a descriptor left
 * unclosed is one leaked per frame.
 *
 *   KEMBED_OPEN    a=natural width b=natural height (pixels, 0 for none)
 *                  c=the owner's win, 0 for a window of its own
 *                  d=KEMBED_ROLE_* ; the UTF-8 title follows the struct
 *   KEMBED_CLOSE_WIN  — the window named by `win` is gone
 *   KEMBED_BUF     a=width b=height c=stride(bytes) d=slot size(bytes)
 *   KEMBED_FRAME   a=slot  b,c,d,e = damage x,y,w,h (pixels)
 *   KEMBED_DAMAGE          b,c,d,e = damage x,y,w,h (pixels)
 *
 * A WINDOW OPENS BEFORE ITS PIXELS DO. KEMBED_OPEN precedes every BUF, FRAME,
 * DAMAGE, TITLE and FULLSCREEN naming that window, so the parent never has to
 * invent a window to hold a frame — and a frame for a window that has not
 * opened is a message from a cage that is out of step, dropped rather than
 * guessed at.
 *
 * A FRAME'S DAMAGE IS AS MANY BOXES AS IT TAKES, and that is why the second op
 * exists. A guest that scrolled its page and ticked a clock in its title bar
 * has damaged two small rectangles at opposite corners; the box that contains
 * both is the whole window, and the parent — which rounds damage out to the
 * blocks it touches and re-cuts every one of them — would pay for a full
 * repaint to carry two. KEMBED_FRAME announces the flip and the first box;
 * every box after it follows immediately in a KEMBED_DAMAGE, before the next
 * KEMBED_FRAME FOR THAT WINDOW. Boxes carry no window of their own beyond
 * `win`, so two windows publishing in one turn interleave safely. A region
 * with more boxes than a frame may carry is collapsed to its bounding box,
 * because past that the messages cost more than the blocks they save.
 *
 *   KEMBED_TITLE   the UTF-8 name follows the struct, at most
 *                  KEMBED_TITLE_MAX bytes including the terminator
 *   KEMBED_FULLSCREEN a=1 the guest asked for the screen, 0 gave it back
 *   KEMBED_INHIBIT a=1 hold the screen awake, 0 let it sleep
 *   KEMBED_GRAB    a=KEMBED_GRAB_* b,c=cursor hint (window pixels)
 *                  d=KEMBED_GRAB_HINT when b,c mean anything
 *   KEMBED_CLIP_OFFER  a=1 the primary selection, 0 the clipboard
 *                  b=bytes, WITH a sealed memfd of exactly that length
 *   KEMBED_DRAG_OFFER  b=bytes, WITH a sealed memfd; the MIME type follows
 *                  the struct
 *   KEMBED_GONE    a=exit status
 *
 * A WINDOW'S NAME AND ITS FULLSCREEN ARE THE WINDOW'S, not the process's.
 * A title honoured only inside the cage is a name nothing draws — the frame
 * and the taskbar entry a person reads are the parent's — and a fullscreen
 * honoured only inside it fills the window the guest is already filling. The
 * toplevel that spoke names itself in `win`, so an export dialog cannot
 * rename the image window it opened over.
 *
 *   KEMBED_SIZE    a=width b=height
 *   KEMBED_KEYMAP  a=bytes b=KEMBED_KEYMAP_* , WITH a sealed memfd
 *   KEMBED_MODS    a=depressed b=latched c=locked d=group
 *   KEMBED_KEY     a=keycode (evdev, NOT +8) b=1 pressed 0 released
 *                  e=time ms
 *   KEMBED_MOTION  a=x b=y (pixels, window-relative) e=time ms
 *   KEMBED_REL     a=dx b=dy c=dx unaccelerated d=dy unaccelerated
 *                  (1/256 pixel) e=time ms
 *   KEMBED_BUTTON  a=x b=y c=button (BTN_*) d=pressed e=time ms
 *   KEMBED_AXIS    a=value (1/256 pixel) b=value120 c=0 vertical 1
 *                  horizontal d=KEMBED_AXIS_* | flags e=time ms
 *   KEMBED_FOCUS   a=1 `win` has the keyboard, 0 nothing here has
 *   KEMBED_LEAVE   — the pointer is no longer over `win`
 *   KEMBED_SLEEP   a=1 asleep, 0 awake
 *   KEMBED_FULLSCREEN_SET a=1 the window is fullscreen, 0 it is not
 *   KEMBED_CLOSE   `win`, or 0 for every toplevel this guest has
 *   KEMBED_CLIP_SET a=1 primary 0 clipboard b=bytes, WITH a sealed memfd;
 *                  b=0 and no descriptor means the selection was cleared
 *   KEMBED_DRAG_ENTER a=x b=y (window pixels); the MIME type follows
 *   KEMBED_DRAG_MOTION a=x b=y e=time ms
 *   KEMBED_DROP    a=x b=y c=bytes, WITH a sealed memfd
 *
 * A KEY IS A SWITCH AND NOT A CHARACTER, which is the whole of why these
 * carry an evdev code. A press and a release are two messages; a guest holds
 * W until the release arrives, repeats from its own keymap, and reads a
 * modifier because the modifier's own key was a message. A press with no
 * release is a key held for ever, so the parent releases every key it has
 * sent when the window loses focus.
 *
 * THE KEYMAP IS THE PERSON'S, NOT AN ASSUMPTION. Without it the guest reads
 * US positions and a French keyboard types the wrong letters; with it the
 * guest compiles the same layout the view's own keyboard is running. It is a
 * descriptor because a keymap is tens of kilobytes and this channel already
 * passes one — chunking it through a 32-byte message would be thousands of
 * datagrams through the loop that also pumps frames.
 *
 * KEMBED_MODS IS A RESYNC AND NOT A PER-KEY EVENT. The key stream drives the
 * guest's xkb state, and xkb's own rule is that a state driven by keys must
 * not also be set by mask — the two disagree about which keys are down and the
 * next key then resolves under the wrong one. It is sent at focus-in, where a
 * lock or a layout group set before the guest existed cannot be derived from
 * any key; on a keymap change; and BEHIND a key that left the locks or the
 * layout group somewhere other than where that window was last told they were.
 * Behind such a key and never ahead of one: ahead, the guest applies the key on
 * top of the mask and toggles the lock straight back off. A session that holds
 * no mask of its own sends none.
 *
 * A POSITION CANNOT SAY HOW FAR THE DEVICE MOVED, which is why KEMBED_REL
 * exists beside KEMBED_MOTION. A guest that grabbed the pointer reads deltas
 * and nothing else, and deltas reconstructed from two positions are the
 * pointer's speed after acceleration, clamping and rounding to the desktop's
 * own grid. While a grab is held the parent sends REL alone; otherwise it
 * sends REL and then MOTION for the same physical event, and the cage spends
 * the pending delta on that motion.
 *
 * KEMBED_AXIS CARRIES NO POSITION and the parent sends a KEMBED_MOTION first
 * whenever the pointer moved. A scroll is not a place, an axis needs six
 * numbers of its own, and a wheel event that also warped the cursor is how a
 * detent over a frame button became a click on it. A value of zero on both
 * axes is the end of a gesture — the finger left the pad — which is what lets
 * a guest stop its kinetic scrolling.
 */
typedef struct {
	uint32_t magic;
	uint32_t op;
	int32_t a, b, c, d;
	uint32_t e;
	uint32_t win;
} KembedMsg;

#endif /* KEMBED_H */
