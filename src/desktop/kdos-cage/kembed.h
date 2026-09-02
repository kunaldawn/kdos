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
 * TWO FRAMES IN ONE MAPPING, and `slot` says which is current. The child
 * renders into the half the parent is not reading, then flips — so a frame is
 * never half old and half new, which single-buffering cannot promise and which
 * on a photograph reads as the compositor being broken.
 */
#define KEMBED_SLOTS 2

enum {
	KEMBED_HELLO = 1,	/* child -> parent: the guest is up          */
	KEMBED_BUF,		/* child -> parent, WITH the memfd: a new
				 * mapping, because the size changed         */
	KEMBED_FRAME,		/* child -> parent: which slot, and what
				 * changed in it                             */
	KEMBED_GONE,		/* child -> parent: the application exited   */

	KEMBED_SIZE = 64,	/* parent -> child: the window is this many
				 * pixels now                                */
	KEMBED_KEY,		/* parent -> child: keycode + pressed        */
	KEMBED_MOTION,		/* parent -> child: pointer, in pixels       */
	KEMBED_BUTTON,
	KEMBED_AXIS,
	KEMBED_FOCUS,		/* parent -> child: the keyboard is yours,
				 * or it is not                              */
	KEMBED_SLEEP,		/* parent -> child: minimised — stop
				 * rendering rather than render unseen       */
	KEMBED_CLOSE,		/* parent -> child: go away                  */
};

/*
 * One message, fixed size. The fields are named per op below rather than by a
 * union: a union in a message a peer chose the tag of is a union whose active
 * member a peer chose.
 *
 *   KEMBED_BUF     a=width b=height c=stride(bytes) d=slot size(bytes)
 *   KEMBED_FRAME   a=slot  b,c,d,e = damage x,y,w,h (pixels)
 *   KEMBED_SIZE    a=width b=height
 *   KEMBED_KEY     a=keycode (evdev, NOT +8) b=pressed
 *   KEMBED_MOTION  a=x b=y (pixels, window-relative) e=time ms
 *   KEMBED_BUTTON  a=x b=y c=button (BTN_*) d=pressed e=time ms
 *   KEMBED_AXIS    a=x b=y c=steps (negative is up) e=time ms
 *   KEMBED_FOCUS   a=1 focused, 0 not
 *   KEMBED_SLEEP   a=1 asleep, 0 awake
 *   KEMBED_GONE    a=exit status
 */
typedef struct {
	uint32_t magic;
	uint32_t op;
	int32_t a, b, c, d;
	uint32_t e;
	uint32_t pad;
} KembedMsg;

#endif /* KEMBED_H */
