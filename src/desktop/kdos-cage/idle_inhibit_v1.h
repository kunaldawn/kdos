#ifndef CG_IDLE_INHIBIT_H
#define CG_IDLE_INHIBIT_H

#include <wayland-server-core.h>

struct cg_server;
struct cg_view;

void handle_idle_inhibitor_v1_new(struct wl_listener *listener, void *data);

/*
 * THE WINDOW IS GONE AND NO INHIBITOR MAY STILL NAME IT. An inhibitor outlives
 * the toplevel it was created on — a toolkit destroys the surface and leaves
 * the wl_resource for its own destructor — and the destroy handler forwards
 * the window it was for. One still naming a freed view is that view
 * dereferenced after the free, on a road the parent triggers by closing a
 * dialog while a video plays.
 */
void idle_inhibit_forget_view(struct cg_server *server, struct cg_view *view);

#endif
