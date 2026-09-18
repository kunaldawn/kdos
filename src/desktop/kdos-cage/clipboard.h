#ifndef CG_CLIPBOARD_H
#define CG_CLIPBOARD_H

#include "kembed.h"
#include "server.h"

/*
 * The embedded mode's selection bridge. See clipboard.c's header for the
 * ownership rule and kembed.h for the two ops it speaks.
 *
 * A cage that is not embedded has no parent to bridge to, so both calls are
 * no-ops there: the guest's own seat is the whole desktop and its selection
 * belongs where it already is.
 */
void clipboard_init(struct cg_server *server);
void clipboard_finish(struct cg_server *server);

/*
 * THE SESSION'S SELECTION, OUT OF THE DESCRIPTOR IT ARRIVED IN. It is
 * KEMBED_CLIP_SET and nothing else.
 *
 * The descriptor stays the CALLER'S: the dispatch that read the message closes
 * every descriptor it received, so this maps and copies within the call and
 * keeps nothing that outlives it. `fd` is -1 on a message that carried none,
 * which with a length of zero is the selection being cleared.
 */
void clipboard_take(struct cg_server *server, const KembedMsg *m, int fd);

#endif
