#ifndef CG_EMBED_H
#define CG_EMBED_H

#include <stdbool.h>

#include "server.h"

/*
 * The embedded mode's half of the private channel to kdos-con. See kembed.h
 * for the protocol and cage.c's header for what --embed is.
 */
bool embed_init(struct cg_server *server, int fd, int width, int height);
void embed_finish(struct cg_server *server);

/* Called after a frame has been rendered into `buffer`: copies it into the
 * shared mapping and tells the parent what changed. */
void embed_publish(struct cg_server *server, struct wlr_buffer *buffer,
		   const pixman_region32_t *damage);

/* Is this compositor embedded at all? */
bool embed_active(struct cg_server *server);
/* Minimised: the parent is not showing this, so nothing is rendered. */
bool embed_asleep(struct cg_server *server);

#endif
