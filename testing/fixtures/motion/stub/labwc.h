/*
 * Stands in for kdos-comp's labwc.h when motioncheck compiles kdos-motion.c
 * and schedcheck compiles kdos-sched.c: the two fields of `server` those
 * files read. The real header reaches rcxml.h, libxml2, cairo and pango, none
 * of which a fade or a timer touches.
 */
#include <stdbool.h>
#include <wayland-server-core.h>

struct server {
	struct wl_display *wl_display;
	struct wl_list outputs;
};
extern struct server server;
