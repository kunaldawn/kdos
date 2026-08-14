/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * KDOS graft layer over the labwc fork. Everything KDOS adds to the
 * compositor enters through this header; upstream files carry only
 * one-line hooks marked with a KDOS comment.
 *
 * KDOS-only configuration lives in ~/.config/kdos/comp.conf (parsed,
 * never sourced) — rc.xml owns everything labwc.
 */
#ifndef KDOS_H
#define KDOS_H

#include <stdbool.h>

struct server;
struct output;
struct wl_display;

struct kdos_conf {
	/*
	 * The CRT pass, percentages. crt = 0 is off. ON by default at the
	 * pre-fork strength (55/60/0) — kdos_conf_load() holds the numbers.
	 * kdos_crt_init() still forces it off on a renderer that is not
	 * GLES2, whatever is configured.
	 */
	int crt, crt_scanlines, crt_curve;

	/*
	 * Idle policy, seconds from last activity; 0 = never. idle_configured
	 * says the user wrote ANY idle_ key — the VM default is all-off, and
	 * overriding "off" must include writing a 0 that is meant.
	 */
	int idle_dim_s, idle_lock_s, idle_off_s;
	bool idle_configured;

	/* Wallpaper PNG path, or "none". */
	char wallpaper[512];
};

extern struct kdos_conf kdos_conf;

/* Fill defaults, then overlay ~/.config/kdos/comp.conf. */
void kdos_conf_load(void);

/* Supervised chrome children (kdos-shell, kdos-notifyd). */
#include <sys/types.h>
void kdos_children_start(void);
bool kdos_child_reap(pid_t pid, int status);
void kdos_children_poll(void);

/* Compositor-owned wallpaper (no swaybg on a cell-grid desktop). */
void kdos_wallpaper_init(void);
void kdos_wallpaper_arrange(void);
void kdos_wallpaper_finish(void);

/* Frame-timing reports on $XDG_RUNTIME_DIR/kdos-frames.sock. */
#include <stdint.h>
void kdos_frames_init(void);
void kdos_frames_finish(void);
void kdos_frames_output_add(struct output *output);
void kdos_frames_frame(struct output *output);
void kdos_frames_render_ns(struct output *output, int64_t ns);
int64_t kdos_frames_now(void);

/* Idle policy: dim -> lock -> outputs off, from last activity. */
void kdos_idle_init(void);
void kdos_idle_finish(void);
void kdos_idle_activity(void);
void kdos_idle_inhibit(bool add);

/*
 * The CRT pass. kdos_crt_early_init() BEFORE wlr_scene_create() (it
 * owns the scanout switch); kdos_crt_init() after the renderer
 * exists. kdos_crt_frame() returns true when it committed the frame
 * itself; false means "not my frame" and the caller commits the
 * plain way — that contract is what keeps a failure here from
 * becoming a black screen.
 */
struct wlr_scene_output;
void kdos_crt_early_init(void);
void kdos_crt_init(void);
void kdos_crt_reload(void);
void kdos_crt_finish(void);
bool kdos_crt_frame(struct output *output, struct wlr_scene_output *so);

#endif /* KDOS_H */
