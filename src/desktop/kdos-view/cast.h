/* kdos-view --cast — the composited grid, into a PipeWire stream. See cast.c.
 *
 * Compiled in only where PipeWire is (KDOS_VIEW_CAST). A build without it
 * refuses --cast by name rather than pretending, the same way --kms does. */

#ifndef KDOS_VIEW_CAST_H
#define KDOS_VIEW_CAST_H

#include <stdint.h>

int kcast_init(int w, int h, int fps);
int kcast_fd(void);
void kcast_pump(void);
/*
 * Offer a frame. `argb` is w*h pixels, tightly packed, and is BORROWED until
 * the next call — the caller owns one buffer for the life of the stream and
 * paints into it in place. It goes out on the stream's next cycle; a second
 * offer before that one replaces it, because a consumer wants the current
 * frame and not a queue of stale ones.
 */
int kcast_push(const uint32_t *argb);
/* How many frames have actually gone out. */
unsigned long kcast_frames(void);
/*
 * True once after every format negotiation — that is, once for each consumer
 * that connects. A consumer that just started has seen nothing, and a desktop
 * nobody is typing at produces no damage, so without this a recording of an
 * idle console is a recording of nothing. The same rule the view protocol
 * already applies to a view that has just attached.
 */
int kcast_hungry(void);
uint32_t kcast_node_id(void);
void kcast_finish(void);

#endif /* KDOS_VIEW_CAST_H */
