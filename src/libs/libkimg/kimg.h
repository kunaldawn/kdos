/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkimg — bytes from a terminal become a picture, or nothing
 *
 * THE ONLY PLACE IN KDOS WHERE UNTRUSTED IMAGE BYTES ARE DECODED, and it is
 * reachable by anything that can write to a terminal: a shell script, a
 * program inside a box, the output of `cat` on a file somebody sent you. Four
 * decoders, and every one of them is a large C library with a history of
 * memory-safety bugs.
 *
 * That is why this is a library with ONE entry point rather than four calls
 * inside kdos-con. There is one place to audit, one place the budget is
 * enforced, and one place a fifth format would be added.
 *
 * THE BUDGET IS ENFORCED BEFORE ANY ALLOCATION, from the size the format
 * itself declares in its header. A length field is an allocation request from
 * an untrusted peer: a decompression bomb is four lines of sixel, and a PNG
 * that says 65535x65535 is eight bytes on the wire and sixteen gigabytes in
 * memory. Refusing after decoding is not refusing.
 * ---------------------------------
 */

#ifndef KIMG_H
#define KIMG_H

#include <stddef.h>

#include <pixman.h>

/*
 * What the caller will accept. All three are enforced; a picture that fails
 * any of them is refused without being decoded.
 *
 * `max_bytes` bounds the DECODED image — width * height * 4 — not the input.
 * The input is bounded by whoever read it off the pty.
 */
typedef struct {
	int max_w;
	int max_h;
	size_t max_bytes;
} KimgBudget;

/*
 * What the escape sequence said it was carrying. KIMG_AUTO sniffs the magic
 * instead, which is what OSC 1337 needs — it names a file, not a format.
 *
 * A type that disagrees with the payload is a refusal, not a re-sniff: a peer
 * that says PNG and sends sixel is not making a mistake worth accommodating.
 */
enum {
	KIMG_AUTO = 0,
	KIMG_SIXEL,
	KIMG_PNG,
	KIMG_JPEG,
	KIMG_WEBP
};

/*
 * Decode, or NULL. NULL is every failure: an unknown format, a decoder this
 * build does not have, a declared size past the budget, a truncated payload, a
 * zero dimension, a size that disagrees with what arrived. The caller cannot
 * tell them apart on purpose — there is nothing useful to do differently, and
 * a reason string reaching a log is a reason string an attacker chose.
 *
 * The result is an ARGB32 pixman image the caller owns and must unref.
 */
pixman_image_t *kimg_decode(const void *bytes, size_t len, int type,
			    const KimgBudget *budget);

/*
 * Which formats this build can actually decode, as a bitmask of 1 << KIMG_*.
 * A build with none still links: kimg_decode answers NULL, and a terminal that
 * cannot show a picture falls back to characters.
 */
unsigned kimg_formats(void);

#endif /* KIMG_H */
