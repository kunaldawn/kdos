/* kdos-view --record / --replay — a session's own messages, timestamped.
 *
 * See record.c. Compiled in only where zstd is (KDOS_VIEW_RECORD); a build
 * without it refuses both flags by name rather than pretending, the same way
 * --kms and --cast do.
 */

#ifndef KDOS_VIEW_RECORD_H
#define KDOS_VIEW_RECORD_H

#include <stddef.h>

/* Start recording. Returns 0, or -1 with a message on stderr. The grid and the
 * cell are the header's: a replay has to know how big the screen was before
 * the first message arrives. */
int krec_open(const char *path, int cols, int rows, int cell_w, int cell_h);
/* One message as it arrived. Silently nothing when no recording is open, so
 * the receive loop needs no flag of its own. */
void krec_msg(unsigned op, const char *payload, size_t len);
void krec_close(void);

/* What a replay hands back: the same three arguments the receive loop had. */
typedef int (*KrecFn)(unsigned op, const char *payload, size_t len, void *user);

/*
 * Play a recording back through `fn`, waiting between messages the way the
 * clock did. Returns 0, or -1 with a message on stderr — including for a
 * recording of a protocol version this build does not speak, which is refused
 * rather than drawn wrong.
 *
 * `cols`, `rows` and the cell come back out of the header so the caller can
 * size a screen before anything is drawn on it.
 */
int krec_replay(const char *path, int *cols, int *rows, int *cell_w,
		int *cell_h, KrecFn fn, void *user);

#endif /* KDOS_VIEW_RECORD_H */
