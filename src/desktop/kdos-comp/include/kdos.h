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
#include <stddef.h> /* kcolor.h names size_t and does not include it */

#include "kcolor.h"

struct server;
struct output;
struct view;
struct wl_display;

/* Where the one taskbar goes. */
enum kdos_panel_edge {
	KDOS_PANEL_BOTTOM = 0,
	KDOS_PANEL_TOP,
	KDOS_PANEL_OFF,
};

/* What closing the laptop lid does. `off` is screen off ONLY — the
 * machine keeps running, which is what a clamshell-with-external-screen
 * setup wants. */
enum kdos_lid_close {
	KDOS_LID_OFF = 0,
	KDOS_LID_LOCK,
	KDOS_LID_SUSPEND,
};

struct kdos_conf {
	/*
	 * The CRT pass, percentages. crt = 0 is off. ON by default at the
	 * pre-fork strength (55/60/0) — kdos_conf_load() holds the numbers.
	 * kdos_crt_init() still forces it off on a renderer that is not
	 * GLES2, whatever is configured.
	 */
	int crt, crt_scanlines, crt_curve;

	/*
	 * crt_fullscreen = off skips the pass on an output whose topmost
	 * view is fullscreen — one render instead of two for video and
	 * games. Direct scanout itself stays off for the session (the
	 * scene's switch is creation-time), so this is the battery lever,
	 * not a zero-copy path.
	 */
	bool crt_fullscreen;

	/*
	 * Idle policy, seconds from last activity; 0 = never. idle_configured
	 * says the user wrote ANY idle_ key — the VM default is all-off, and
	 * overriding "off" must include writing a 0 that is meant.
	 */
	int idle_dim_s, idle_lock_s, idle_off_s;
	bool idle_configured;

	/*
	 * The lid switch (enum kdos_lid_close). Same VM gate as the idle
	 * timers: default suspend on hardware, off in a VM unless the key
	 * is written — a VM has no lid, and a default that suspends the
	 * host's nested session is a trap.
	 */
	int lid_close;
	bool lid_configured;

	/* Wallpaper PNG path, or "none". */
	char wallpaper[512];

	/*
	 * WHERE THE ONE BAR IS. There used to be two panels — a menu bar at
	 * the top and a second panel at the bottom — which was two exclusive
	 * zones, two hit maps and the window list drawn twice. It is one
	 * taskbar on the bottom edge now, the shape every desktop of this
	 * lineage settled on, and this key is what moves it or turns it off.
	 *
	 *   0 bottom (the default)   1 top   2 off
	 *
	 * `panel_bottom` is GONE. A comp.conf that still carries it is
	 * reported by name rather than silently ignored, because the file
	 * promises that a line which does not take effect says so.
	 */
	int panel_edge;

	/*
	 * How many CELLS thick. Two by default, and that is what makes a task
	 * button's icon square: a cell is 16x32, so two cells across two rows
	 * is a 32x64 box with a 32x32 picture centred in it. One row is the
	 * bar as it was before the icon layer and still works — nothing on
	 * this desktop requires a picture.
	 */
	int panel_cells;

	/*
	 * Pixel icons at all (libkicon). ON where there is artwork; off makes
	 * every surface fall back to its glyph tier, which is also what a tty,
	 * a missing atlas and an unreadable PNG produce. Passed to every
	 * chrome child as --no-icons, so it is startup-only like the rest of
	 * their argv.
	 */
	bool icons;

	/*
	 * The desktop icon layer. Defaults ON — it is the desktop, not an
	 * extra — and exists as a key because it is the thing a person running
	 * this on a small screen will genuinely want off.
	 */
	bool desktop_icons;

	/*
	 * The dockapp column (kdos-slit) and the desktop's widget zone. Both
	 * OFF by default and both are opt-in for the same reason: a column of
	 * gadgets nobody configured is a column of dim `!` marks.
	 */
	bool slit;

	/*
	 * The clipboard history (kdos-clip). ON: a desktop that forgets what
	 * you copied thirty seconds ago is the complaint every clipboard
	 * manager exists to answer. Nothing is written to disk — the history
	 * lives in that process and dies with the session — so the key is
	 * about whether to keep one at all rather than about where it goes.
	 */
	bool clipboard;

	/*
	 * The top panel hides against the edge until the pointer reaches it.
	 * Off by default: a panel that is not there is a panel nobody can
	 * find, and this desktop's face is the one thing a new user is
	 * looking for. Startup-only, like the rest of the chrome's argv.
	 */
	bool panel_autohide;

	/*
	 * Window memory (kdos-winpos.c): app_id -> geometry/workspace/shaded,
	 * recorded at unmap and applied at map. ON, because a window that
	 * comes back where it was is what everyone expects and nobody
	 * configures; the key exists for the person who wants every launch to
	 * be placed by the policy instead.
	 */
	bool window_memory;

	/*
	 * The font every piece of supervised chrome is drawn in, as a
	 * fontconfig name. Empty means libkwl's default, which is
	 * `Terminus:pixelsize=32` — the console's own cell, and the reason the
	 * panel, the boot splash and tty1 look like one machine.
	 *
	 * It exists because that default is a PIXEL size and libkwl does no
	 * HiDPI: on a 4K panel the chrome comes out half the height it should
	 * be, and there was no knob anywhere in the system to say so. The
	 * compositor's own titlebars are rc.xml's <theme><font>, which is the
	 * matching setting on the labwc side.
	 */
	char chrome_font[128];

	/*
	 * strftime format for the panel clock, passed as `--clock`.
	 * Empty means the panel's own default (%H:%M). Startup-only, like
	 * the rest of the chrome's command line.
	 */
	char clock_format[64];

	/*
	 * THE BAR'S OWN FONT, and it is separate from chrome_font on purpose.
	 *
	 * A cell is half as wide as the font is tall, so the panel's thickness
	 * IS this number: Terminus at 20 gives a 10x20 cell and a two-row bar
	 * 40 pixels tall, which is what a taskbar has been since Windows 7.
	 * chrome_font stays at the console's own 32 for the menus and popups,
	 * which are read rather than glanced at. Empty falls back to
	 * chrome_font, and then to libkwl's default.
	 *
	 * Terminus is a BITMAP face: name a size it actually has
	 * (12/14/16/18/20/22/24/28/32) or fcft answers with the nearest strike
	 * and the bar is a pixel off what every arithmetic here assumes.
	 */
	char panel_font[128];

	/*
	 * The gap between the bar and the screen edge, in pixels. 0 is the
	 * edge-to-edge bar; non-zero floats it, and the exclusive zone grows
	 * by the same amount so a maximized window does not slide underneath.
	 */
	int panel_margin;

	/*
	 * The bar's body opacity, in percent. Below 100 the panel's background
	 * slot is painted translucent and the desktop shows through; the text
	 * and the fills stay ink.
	 */
	int panel_opacity;
};

extern struct kdos_conf kdos_conf;

/* Fill defaults, then overlay ~/.config/kdos/comp.conf. */
void kdos_conf_load(void);

/*
 * SIGHUP/Reconfigure: re-parse comp.conf. The crt, idle, lid_close and
 * wallpaper keys apply live; the chrome keys (panel_bottom, desktop_icons, chrome_font,
 * clock_format) are a child's command line and stay startup-only — a
 * change is LOGGED, never half-applied.
 */
void kdos_conf_reload(void);

/* The accent, from $XDG_CACHE_HOME/kdos/theme — one reader for every
 * graft. Never NULL; absent or unknown means phosphor. */
const KcolScheme *kdos_accent_scheme(void);

/* The idle policy's DMI/session VM test, shared with the lid gate. */
bool kdos_in_vm(void);

/*
 * Supervised chrome children: the two panels and the desktop icons, ONE SET
 * PER OUTPUT, plus the notification daemon, which is one for the session
 * because it owns a bus name. The output pair is called from labwc's own
 * output create/destroy paths.
 */
#include <sys/types.h>
void kdos_children_start(void);
void kdos_children_output_add(const char *name);
void kdos_children_output_remove(const char *name);
bool kdos_child_reap(pid_t pid, int status);
void kdos_children_poll(void);
/* Reconfigure re-arms supervision: fail counters reset, a given-up child
 * gets another chance. */
void kdos_children_reconfigure(void);

/* Compositor-owned wallpaper (no swaybg on a cell-grid desktop). */
void kdos_wallpaper_init(void);
void kdos_wallpaper_arrange(void);
/* SIGHUP: prefer $XDG_CACHE_HOME/kdos/wallpaper.png (kdos theme's
 * retint), re-decode when the source changed, retint the deep rects. */
void kdos_wallpaper_reload(void);
void kdos_wallpaper_finish(void);

/* Frame-timing reports on $XDG_RUNTIME_DIR/kdos-frames.sock. */
#include <stdint.h>
void kdos_frames_init(void);
void kdos_frames_finish(void);
void kdos_frames_output_add(struct output *output);
void kdos_frames_frame(struct output *output);
/* Whether this frame event actually submitted a buffer — the difference
 * between "late" and "nothing wanted drawing". */
void kdos_frames_submit(struct output *output, bool submitted);
void kdos_frames_render_ns(struct output *output, int64_t ns);
int64_t kdos_frames_now(void);

/*
 * `kdos hey` — the command socket on $XDG_RUNTIME_DIR/kdos-cmd.sock. One
 * NDJSON request line in, one response line out, close: no history and no
 * subscriptions, which is what kdos-frames.sock is for.
 */
void kdos_cmd_init(void);
void kdos_cmd_finish(void);

/*
 * Window memory. _record() at unmap, _apply() at map AFTER the window
 * rules have run; _client_positioned() is how a shell tells us the client
 * placed this window itself (X11 USPosition), which is the one case that
 * must never be overridden. _forget() drops that mark when a view dies
 * before it maps.
 */
void kdos_winpos_record(struct view *view);
void kdos_winpos_apply(struct view *view);
void kdos_winpos_client_positioned(struct view *view);
void kdos_winpos_forget(struct view *view);
void kdos_winpos_finish(void);

/*
 * Window grouping — Haiku's Stack & Tile, the stack half. A hidden
 * member is a MINIMIZED view (show-desktop.c's pattern), which is what
 * makes the panel stay truthful with no second graft.
 */
struct ssd;
void kdos_group_add(struct view *view);
void kdos_group_remove(struct view *view);
void kdos_group_next(struct view *view, bool reverse);
/* From the focus path: this member is now the one showing. */
void kdos_group_activate(struct view *view);
int kdos_group_size(struct view *view);
/* From ssd_update_title(): (re)draw the tab strip over the title area. */
void kdos_group_ssd_update(struct ssd *ssd);
/* From ssd_titlebar_destroy(): the strip went with the titlebar. */
void kdos_group_ssd_clear(struct view *view);
void kdos_group_finish(void);

/* Idle policy: dim -> lock -> outputs off, from last activity. */
void kdos_idle_init(void);
void kdos_idle_finish(void);
void kdos_idle_activity(void);
void kdos_idle_inhibit(bool add);
/* The dim rect follows the layout, next to the wallpaper's arrange. */
void kdos_idle_arrange(void);
void kdos_idle_reconfigure(void);
/* The lid policy shares the idle policy's output blanking. */
void kdos_idle_outputs_power(bool on);

/* The lid switch (wlr_switch): lid_close = off|lock|suspend. */
struct wlr_input_device;
void kdos_lid_init(void);
void kdos_lid_finish(void);
void kdos_lid_reconfigure(void);

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
/*
 * The CRT power-down: the last composite collapses to a bright line,
 * then a dot. Called from main() AFTER wl_display_run() returns and
 * BEFORE any graft teardown; hard 600 ms deadline, GLES2-only, a clean
 * no-op everywhere else — it must never block shutdown.
 */
void kdos_crt_powerdown(void);

/*
 * Click-away for on-demand layer surfaces.
 *
 * Every front end on this desktop that takes a keyboard — the menus, the
 * launcher, the run box, the file chooser — is an on-demand layer surface
 * that dismisses itself when it loses keyboard focus. Focusing a VIEW does
 * that for free, so launching an app closes a menu; a press on the desktop,
 * the wallpaper or a keyboard-less layer surface like the panel changes no
 * focus at all, and the menu stayed open. Called from the button-press path
 * with the layer surface that was pressed, or NULL when it was not one.
 */
struct seat;
struct wlr_layer_surface_v1;
void kdos_layer_release_on_demand(struct seat *seat,
	struct wlr_layer_surface_v1 *pressed);

/*
 * PEEK — fade every window so the desktop behind them can be seen, for as
 * long as the pointer dwells on Show Desktop. Transient: it changes no
 * toplevel state and nothing a client can observe. See kdos-peek.c.
 */
void kdos_peek_set(bool on);
void kdos_peek_finish(void);

/*
 * A THUMBNAIL of the most recently active window with this app_id, written to
 * `path` as a binary PPM. False when there is no such window, or when its
 * pixels are not host-readable — a dmabuf client has none, and every caller
 * must be built to do without. See kdos-thumb.c.
 */
bool kdos_thumb_write(const char *app_id, int w, int h, const char *path);

#endif /* KDOS_H */
