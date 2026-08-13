/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — the KDOS compositor
 *
 * wlroots does outputs, input, buffers and the protocol implementations;
 * everything above that is ours. M1 is the smallest thing that is honestly a
 * session: one or more outputs, xdg-shell windows, a pointer, a keyboard, and
 * a VT you can still switch away from.
 *
 * Structured after wlroots' own tinywl, which is the only reference that
 * tracks the API — wlroots breaks it every release, so this was written
 * against the 0.20.2 headers rather than from memory.
 *
 * Three deliberate departures from tinywl:
 *
 *   - No shell, anywhere. tinywl spawns its startup command with
 *     execl("/bin/sh", "-c", cmd). Every KDOS program builds argv and execs
 *     it directly; a shell in the middle turns any string that reaches it
 *     into an injection point, and the same rule already governs kpkg,
 *     kinstall and kdos-appbox.
 *   - Super, not Alt, is the compositor modifier. Alt belongs to the
 *     applications — Alt+F in a terminal application is not ours to take.
 *   - VT switching is implemented. tinywl has none, so on real hardware it
 *     traps you in the session; Ctrl+Alt+F2 reaching the root tty is the
 *     acceptance criterion for this milestone and the debug path for every
 *     one after it.
 * ---------------------------------
 */

#ifndef KDOS_COMP_H
#define KDOS_COMP_H

#include <sys/types.h>
#include <time.h>

#include <pixman.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
/* struct kc_cellbuf embeds a wlr_buffer, so the full definition is needed here
 * rather than a forward declaration. */
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_workspace_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/version.h>
#include <xkbcommon/xkbcommon.h>

/* KtuiCell — struct kc_popup holds a grid of them. libktui links nothing but
 * musl, so this costs the compositor no dependency. */
#include "ktui.h"

enum kc_cursor_mode { KC_CURSOR_PASSTHROUGH = 0, KC_CURSOR_MOVE, KC_CURSOR_RESIZE };

/*
 * Four workspaces, fixed. Not a placeholder for a configurable count: a
 * workspace is only useful if the key that reaches it is muscle memory, and
 * Super+1..4 is reachable without looking. A dynamic count buys a feature
 * nobody can find.
 */
#define KC_WORKSPACES 4
#define KC_MAX_STARTUP 8

/*
 * A scene node's `data` is a void* and every kind of thing puts its own struct
 * there. Both a toplevel and an X11 surface own a scene tree, so a lookup that
 * walks up to the first non-NULL `data` can land on EITHER — and casting an
 * xsurface to a toplevel reads `xdg_toplevel` out of the middle of a different
 * struct. Every taggable thing therefore starts with this enum, and the lookup
 * checks the tag before it believes the pointer.
 */
enum kc_node_type { KC_NODE_TOPLEVEL = 1, KC_NODE_XSURFACE, KC_NODE_DECO };

enum kc_action {
	KC_ACT_SPAWN = 1,
	KC_ACT_CLOSE,
	KC_ACT_QUIT,
	KC_ACT_CYCLE,
	KC_ACT_CYCLE_BACK,
	KC_ACT_WORKSPACE,
	KC_ACT_MOVE_TO_WORKSPACE,
	KC_ACT_LOCK,
	/* The keyboard half of the window frame — every titlebar control has
	 * one, because a CSD window has no titlebar to aim at. */
	KC_ACT_MAXIMIZE,
	KC_ACT_FULLSCREEN,
	KC_ACT_SHADE,
	KC_ACT_MINIMIZE,
	/* Modal, arrow-driven, ended by Enter or Escape — for the window that
	 * has been dragged off the screen and the machine with no pointer. */
	KC_ACT_MOVE_KB,
	KC_ACT_RESIZE_KB,
	KC_ACT_ASCII,
};

/*
 * The modifiers a binding is allowed to name. A binding matches on the WHOLE
 * masked set, not on "contains" — otherwise Super+Shift+1 would also fire the
 * Super+1 binding, and the two mean different things.
 */
#define KC_MOD_MASK (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT | \
		     WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)

struct kc_bind {
	struct wl_list link;
	uint32_t mods;
	xkb_keysym_t sym;
	enum kc_action action;
	int arg;			/* workspace index, 0-based */
	char **argv;			/* KC_ACT_SPAWN only, NULL-terminated */
};

struct kc_server {
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_session *session;	/* NULL under the headless backend */
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_manager_v1 *output_mgr;
	struct wl_listener output_mgr_apply;
	struct wl_listener output_mgr_test;
	struct wl_listener layout_change;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
	/* Most-recently-used order, head first. This IS the alt-tab order and
	 * the "what gets focus when this window goes away" order; keeping one
	 * list for both is what makes those two agree. */
	struct wl_list toplevels;

	/* A workspace is a scene tree that is either enabled or not. Switching
	 * is two set_enabled calls, and every window kind — xdg, X11, and the
	 * layer surfaces that arrive with the shell — is covered by being
	 * parented into one, rather than by each growing its own hide path. */
	struct wlr_scene_tree *ws_tree[KC_WORKSPACES];
	int cur_ws;

	/*
	 * Layer-shell lives OUTSIDE the workspace trees: a panel belongs to the
	 * screen, not to workspace 2, and must survive a switch. The two trees
	 * sandwich the workspaces, which is the protocol's layer order —
	 * background/bottom, then windows, then top/overlay.
	 */
	struct wlr_scene_tree *layer_below, *layer_above;
	/* The desktop background. One decoded image, one scene node per output
	 * (wallpaper.c). NULL when there is no wallpaper, which is a black
	 * desktop rather than an error. */
	struct kc_wallpaper_buffer *wallpaper_buf;
	char *wallpaper;		/* comp.conf `wallpaper =`, or NULL */
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_surface;
	struct wl_list layers;		/* struct kc_layer */

	/* What a panel needs to know. Standard protocols, so waybar and friends
	 * work here too — see shellsvc.c. Two foreign-toplevel protocols: the
	 * wlr one carries the state and the requests a taskbar needs, the ext
	 * one carries nothing but identity and is what a capture source is named
	 * by (capture.c). Neither is a superset of the other. */
	struct wlr_foreign_toplevel_manager_v1 *ftl_mgr;
	struct wlr_ext_foreign_toplevel_list_v1 *ext_ftl_list;
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
		*ftl_capture_mgr;
	struct wl_listener ftl_capture_request;
	struct wlr_ext_workspace_manager_v1 *ws_mgr;
	struct wlr_ext_workspace_group_handle_v1 *ws_group;
	struct wlr_ext_workspace_handle_v1 *ws_handle[KC_WORKSPACES];
	struct wl_listener ws_commit;

	/* Alt-tab in progress. The MRU list is deliberately NOT reordered until
	 * the modifier comes up: reordering on every Tab would make the second
	 * press return to where it started, which is the classic broken
	 * alt-tab. */
	bool cycling;
	struct kc_toplevel *cycle_at;

	/*
	 * ext-session-lock-v1. `locked` is the compositor's own state and is
	 * deliberately NOT derived from `lock` being non-NULL: if the lock
	 * client crashes, the protocol says the session STAYS locked and the
	 * compositor keeps the screen blank. A boolean that came from the
	 * client's liveness would unlock the machine by crashing it.
	 */
	struct wlr_session_lock_manager_v1 *lock_mgr;
	struct wl_listener new_lock;
	struct kc_lock *lock;
	struct wlr_scene_tree *layer_lock;
	struct wlr_scene_rect *lock_blank;
	bool locked;

	/*
	 * Idle: the ext-idle-notify global clients use, the idle-inhibit global
	 * a video player uses to stop us, and our own dim/lock/DPMS timer.
	 */
	struct wlr_idle_notifier_v1 *idle_notifier;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit;
	struct wl_listener new_inhibitor;
	int ninhibit;
	struct wl_event_source *idle_timer;
	struct wlr_scene_rect *idle_dim_rect;
	int idle_dim_s, idle_lock_s, idle_off_s;
	bool idle_configured;		/* a comp.conf line set one of them */
	int idle_stage;			/* 0 awake, 1 dimmed, 2 locked, 3 off */

	/*
	 * The CRT pass (crt.c). Three knobs, all percentages, and `crt = 0` is
	 * an honest off rather than a quiet minimum: the pass costs fill rate,
	 * and on a software renderer it is forced to 0 whatever the config says.
	 * `crt_gl` is opaque here so GLES2 headers stay inside crt.c.
	 */
	int crt_intensity, crt_scan, crt_curve;
	struct kc_crt_gl *crt_gl;

	/* Super+A: the whole screen as characters (ascii.c). Off by default and
	 * built on first use — most sessions never ask for it. */
	struct kc_ascii *ascii;
	bool ascii_on;
	bool ascii_mono;		/* `ascii_color = accent` */

	/* Frame timing, reported on $XDG_RUNTIME_DIR/kdos-frames.sock — the
	 * compositor's half of stutter attribution (frames.c). Opaque: nothing
	 * outside that file needs its shape. */
	struct kc_frames *frames;

	/* The input-method relay (textinput.c). Opaque here; nothing outside
	 * that file needs its shape. NULL when the globals could not be made. */
	struct kc_im *im;

	struct wlr_security_context_manager_v1 *security_mgr;
	struct wl_listener security_commit;
	struct wl_list policies;	/* struct kc_policy, one per box seen */

	struct wl_list seen_app_ids;	/* struct kc_seen */

	struct wl_list binds;		/* struct kc_bind */
	/* Programs launched once the socket exists. Default: kdos-shell.
	 * Bounded rather than unbounded — a config that spawns 500 processes at
	 * login is a mistake, not a feature. */
	char **startup[KC_MAX_STARTUP];
	int nstartup;
	int startup_overridden;
	/* Supervision state, one slot per startup entry: the live pid (0 when
	 * it is not running), how many times it has died and when that count
	 * started. A startup child is the desktop's chrome and is restarted; a
	 * `spawn` binding's child is not, and is not tracked here. */
	pid_t startup_pid[KC_MAX_STARTUP];
	int startup_fails[KC_MAX_STARTUP];
	time_t startup_since[KC_MAX_STARTUP];
	int32_t repeat_rate, repeat_delay;
	int snap_px;

	/*
	 * The cell-grid window frames (deco.c).
	 *
	 * `deco_font` is asked for by name and is Terminus by default, which is
	 * the same rasterisation tty1 and the boot splash use — the whole point
	 * of a bitmap font here rather than a nicer-looking scalable one.
	 * `deco_scale` of 0 means follow each OUTPUT's scale, which is what a
	 * 4K panel beside a 1080p one needs.
	 */
	bool deco_frames;
	char *deco_font;
	int deco_scale;
	/* app_ids that draw their own titlebar without ever saying so over
	 * xdg-decoration. A `*` suffix matches a prefix. */
	char **csd_apps;
	int ncsd_apps;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_xdg_decoration_manager_v1 *deco_mgr;
	struct wl_listener new_decoration;

	struct wlr_xwayland *xwayland;
	struct wl_listener new_xwayland_surface;
	struct wl_listener xwayland_ready;

	/* Interactive move/resize. A grab is compositor-wide by nature: while
	 * one is running the pointer belongs to the WM, not to any client. */
	enum kc_cursor_mode cursor_mode;
	struct kc_toplevel *grabbed;
	/* Where an edge-drag would put the window, shown while the drag is
	 * still running. Created on the first drag that needs it. */
	struct wlr_scene_rect *snap_rect;
	/* The KEYBOARD move/resize grab (Alt+F7 / Alt+F8), which is modal
	 * rather than a drag: it owns the arrow keys until Enter or Escape, and
	 * Escape puts the window back where it started. */
	struct kc_toplevel *kbgrab;
	bool kbgrab_resize;
	struct wlr_box kbgrab_from;

	/*
	 * A titlebar press that has not yet become a drag. The grab starts only
	 * after the pointer moves DRAG_THRESHOLD_PX, so a slightly sloppy click
	 * is a click — every modern desktop does this, and its absence is one
	 * of the things that made v2 feel wrong under the hand.
	 */
	struct kc_toplevel *drag_pending;
	double drag_px, drag_py;


	/* The window menu and the alt-tab switcher (cellui.c). */
	struct kc_popup *menu;
	struct kc_toplevel *menu_for;
	int menu_sel;
	struct kc_popup *switcher;
	double grab_x, grab_y;		/* cursor at grab time */
	struct wlr_box grab_geo;	/* window box at grab time */
	uint32_t resize_edges;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	/* Middle-click paste, one pointer image for the desktop, and "raise my
	 * other window". All three were missing globals that foot names on
	 * startup; the first is why middle-click paste worked nowhere. */
	struct wl_listener request_set_primary;
	struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
	struct wl_listener request_cursor_shape;
	struct wlr_xdg_activation_v1 *activation;
	struct wl_listener request_activate;

	/*
	 * Capturing the pointer (pointer.c). Without these two protocols NO
	 * game, emulator, 3D application or remote-desktop client can take the
	 * mouse — the cursor walks out of the window mid-aim and nothing errors.
	 */
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wl_listener new_constraint;
	struct wlr_pointer_constraint_v1 *active_constraint;
	bool constraint_committed;
	/* Super breaks a constraint; it stays broken until focus moves. A locked
	 * pointer with no escape hatch is a session you reboot. */
	bool constraint_broken;
	struct wlr_relative_pointer_manager_v1 *relative_pointer;

	struct wlr_pointer_gestures_v1 *gestures;
	struct wl_listener swipe_begin, swipe_update, swipe_end;
	struct wl_listener pinch_begin, pinch_update, pinch_end;
	struct wl_listener hold_begin, hold_end;

	struct wlr_gamma_control_manager_v1 *gamma_control;
	struct wl_listener set_gamma;
	struct wlr_output_power_manager_v1 *output_power;
	struct wl_listener set_output_power;
	struct wl_list keyboards;
};

struct kc_output {
	struct wl_list link;
	struct kc_server *server;
	struct wlr_output *wlr_output;
	/* What is left after the panels have taken their exclusive zones. */
	struct wlr_box usable;
	/* This output's wallpaper node, sized and positioned by
	 * kc_wallpaper_arrange(). Owned by the scene, not by us. */
	struct wlr_scene_buffer *wallpaper;
	/* The CRT pass's two swapchains and its texture cache, created on the
	 * first CRT frame. NULL means the plain path. */
	struct kc_crt *crt;
	/* Frame timing. Two clocks, kept apart on purpose: `present` is what the
	 * user saw and `frame` is what the compositor was given, and interleaving
	 * them would manufacture gaps neither one had. */
	int64_t last_present_ns, last_frame_ns, refresh_ns, render_ns;
	bool presenting;		/* this backend emits presentation events */
	struct wl_listener present;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

/* One per xdg-decoration object. It exists only to hold the two listeners that
 * let the SERVER_SIDE answer wait for the surface's initial commit — see
 * new_decoration() in main.c for why answering immediately aborts wlroots. */
struct kc_decoration {
	struct wlr_xdg_toplevel_decoration_v1 *deco;
	struct wl_listener commit;
	/* The client answering. An explicit CLIENT_SIDE is the ONLY thing that
	 * makes a window unframed by its own request — silence is framed. */
	struct wl_listener mode;
	struct wl_listener destroy;
};

struct kc_toplevel {
	enum kc_node_type node_type;	/* KC_NODE_TOPLEVEL — must stay first */
	struct wl_list link;
	struct kc_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	int ws;				/* which workspace tree it hangs under */
	struct wlr_foreign_toplevel_handle_v1 *ftl;	/* the taskbar's view */
	struct wl_listener ftl_activate;		/* per handle, not per manager */
	struct wl_listener ftl_close;
	/* The same window under ext-foreign-toplevel-list, and the capture
	 * source built on its scene node. The handle's `data` points back here,
	 * which is how a capture request finds the window it names. */
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_ftl;
	struct wlr_ext_image_capture_source_v1 *capture_src;
	struct wl_listener capture_src_destroy;
	int x, y;			/* scene position, ours to own */

	/*
	 * The client's own subtree, INSIDE scene_tree rather than being it.
	 * scene_tree is ours and holds the frame and the shadow beside this;
	 * separating them is what lets a shade disable the client's pixels
	 * while the titlebar stays on screen, and what keeps the frame moving
	 * and raising with the window for free.
	 */
	struct wlr_scene_tree *surface_tree;
	/* The cell-grid frame (deco.c). NULL when the client decorates itself
	 * or frames are off. */
	struct kc_deco *deco;
	/* The client asked for client-side decorations, or its app_id is in
	 * comp.conf's `csd_apps`. Frames are never forced onto it. */
	bool csd;

	bool maximized, fullscreen, shaded, minimized;
	/* Where to go back to. Written ONCE on the way into maximise or
	 * fullscreen — writing it again while already there is how a window
	 * becomes impossible to restore. */
	struct wlr_box saved;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
};

/* A layer-shell surface: a panel, a wallpaper, an OSD or a lock screen. */
struct kc_layer {
	struct wl_list link;
	struct kc_server *server;
	struct kc_output *output;
	struct wlr_layer_surface_v1 *layer_surface;
	struct wlr_scene_layer_surface_v1 *scene;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
};

/* An X11 window. Not an xdg toplevel: it positions itself, it may be
 * override-redirect, and its wl_surface arrives separately (associate). */
struct kc_xsurface {
	enum kc_node_type node_type;	/* KC_NODE_XSURFACE — must stay first */
	struct kc_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wlr_scene_tree *scene_tree;
	int ws;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener request_configure;
	struct wl_listener destroy;
};

/* The active session lock, and one scene tree per lock surface. */
struct kc_lock {
	struct kc_server *server;
	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
	struct wl_list surfaces;	/* struct kc_lock_surface */
	int nsurfaces;
	/* `locked` may be sent EXACTLY once — wlroots asserts on the second —
	 * and the trigger is a surface mapping, of which there is one per
	 * output. */
	bool locked_sent;
};

struct kc_lock_surface {
	struct wl_list link;
	struct kc_lock *parent;
	struct wlr_session_lock_surface_v1 *surface;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener destroy;
	struct wl_listener output_destroy;
};

struct kc_keyboard {
	struct wl_list link;
	struct kc_server *server;
	struct wlr_keyboard *wlr_keyboard;
	/* A virtual-keyboard-v1 device rather than real hardware. Its keys never
	 * reach the input method's grab: an input method forwards the keys it
	 * does not want through a virtual keyboard, and feeding those back to the
	 * grab is an infinite loop with the key still not delivered. */
	bool virt;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

void kc_focus(struct kc_toplevel *t, struct wlr_surface *surface);

/* Focus the most recent window on the current workspace, or nothing. What an
 * unlock ends with — the MRU list is main.c's, the unlock is lock.c's. */
void kc_refocus(struct kc_server *s);

/* argv, never a command string — see the header comment. Double-forks so the
 * compositor never has to reap, and never blocks on a child it did not want
 * to wait for. */
void kc_spawn(const char *const *argv);

/* Rootless X11. Safe to call unconditionally: a failure logs and leaves the
 * session working, minus X11-only apps. */
void kc_xwayland_init(struct kc_server *s, struct wlr_compositor *compositor);

/* Outputs, plus wlr-output-management and xdg-output so anything else can
 * arrange them. Call after the scene layout exists and before the backend
 * starts. */
void kc_output_init(struct kc_server *s);

/* Defaults, then ~/.config/kdos/comp.conf over the top. Never fails: a missing
 * or broken config leaves a session with working keys. */
void kc_config_load(struct kc_server *s);
void kc_config_free(struct kc_server *s);

/* The shared `key = value` reader. `value` is mutable so callers may tokenise
 * it in place; it does not survive the callback. Returns false when the file
 * could not be opened, which is not by itself an error. */
typedef void (*kc_kv_fn)(const char *key, char *value, const char *path,
			 int lineno, void *user);
bool kc_config_read(const char *path, kc_kv_fn fn, void *user);
char *kc_trim(char *s);

/*
 * security-context-v1 — the box a client came from decides what it may bind.
 *
 * Call before any global is created that a sandboxed client must not see: the
 * filter is consulted at bind time, but a global registered before the filter
 * is installed is still filtered, so ordering is about clarity rather than
 * correctness.
 */
void kc_security_init(struct kc_server *s);
void kc_security_free(struct kc_server *s);

/*
 * Record an app_id that was actually seen on a mapped window. Records only —
 * the comparison against installed desktop entries is `kdos appid`'s, because
 * an app_id must be measured rather than guessed and the compositor is the
 * only thing that can measure it.
 */
/* Windows and workspaces, as the panel sees them. */
void kc_shellsvc_init(struct kc_server *s);
void kc_shellsvc_add(struct kc_server *s, struct kc_toplevel *t);
void kc_shellsvc_update(struct kc_server *s, struct kc_toplevel *t);
void kc_shellsvc_remove(struct kc_toplevel *t);
void kc_shellsvc_refresh(struct kc_server *s);

/* Switching is public because a taskbar click on a window elsewhere has to
 * take you to it. */
void kc_ws_switch(struct kc_server *s, int n);

/* Panels, wallpapers and OSDs. Arrange recomputes every output's usable area,
 * and must run whenever the outputs change. */
void kc_layer_init(struct kc_server *s);
void kc_layer_arrange(struct kc_server *s);
/* The box a WINDOW may have on the output under (x, y) — the output's own box
 * minus every panel's exclusive zone. Everything that positions a toplevel goes
 * through this; using the raw layout box is how a maximised window ends up
 * painted over the panel. False when there is no output there. */
bool kc_usable_at(struct kc_server *s, double x, double y, struct wlr_box *out);

/* The desktop background — wallpaper.c. `init` decodes once, `arrange` places
 * one node per output and is called again on every layout change. */
#define KC_WALLPAPER_DEFAULT "/usr/share/backgrounds/kdos/default-wallpaper.png"
struct kc_wallpaper_buffer;
void kc_wallpaper_init(struct kc_server *s);
void kc_wallpaper_arrange(struct kc_server *s);
void kc_wallpaper_free(struct kc_server *s);

/*
 * ext-session-lock-v1. Create the global after layer_lock exists.
 *
 * `kc_locked()` is what every input path asks before it lets an event reach a
 * client: while the session is locked NOTHING below the lock tree may see a
 * key or a click, and that has to be one question with one answer rather than a
 * condition each path remembers to check.
 */
void kc_lock_init(struct kc_server *s);
bool kc_locked(const struct kc_server *s);
/* Focus the lock surface, or nothing at all when the client has abandoned us. */
void kc_lock_focus(struct kc_server *s);
/* Resize every lock surface — after an output arrives, leaves or is resized. */
void kc_lock_arrange(struct kc_server *s);

/*
 * Idle: ext-idle-notify + idle-inhibit + the dim/lock/DPMS policy.
 *
 * `kc_idle_activity()` must be called from every input path, which is why it is
 * cheap and idempotent. Everything else is timers.
 */
void kc_idle_init(struct kc_server *s);
void kc_idle_activity(struct kc_server *s);
void kc_idle_free(struct kc_server *s);

/*
 * The CRT pass. `kc_crt_init` decides once whether there is going to be one at
 * all — a renderer that is not GLES2 turns it off and says so — and compiles the
 * shader; call it after the renderer exists.
 *
 * `kc_crt_frame` returns true when it has COMMITTED the frame itself. False
 * means "not my frame": the pass is off, or this output has fallen back, and the
 * caller must commit the scene the plain way. That contract is what keeps a
 * failure in here from turning into a black screen.
 */
void kc_crt_init(struct kc_server *s);
/* SIGHUP: re-read the accent from $XDG_CACHE_HOME/kdos/theme. The tint only —
 * the intensity and the rest are comp.conf's, and reloading a config is a
 * different operation to retinting a running session. */
void kc_crt_reload(struct kc_server *s);
bool kc_crt_frame(struct kc_output *o, struct wlr_scene_output *so);
void kc_crt_output_free(struct kc_output *o);
void kc_crt_free(struct kc_server *s);

/*
 * The ASCII effect (ascii.c) — Super+A.
 *
 * The algorithm is libkcell's; this is the same table and the same arithmetic
 * on the GPU. `kc_ascii_render` returns false when it declined the frame, in
 * which case the caller carries on with the untouched composite: a renderer
 * that cannot do it is a desktop without the effect, never a black screen.
 */
struct kc_ascii;
void kc_ascii_init(struct kc_server *s);
bool kc_ascii_on(const struct kc_server *s);
void kc_ascii_toggle(struct kc_server *s);
bool kc_ascii_render(struct kc_server *s, unsigned tex, unsigned target,
		     int w, int h, const float tint[3], unsigned *out_tex);
void kc_ascii_free(struct kc_server *s);

/*
 * Frame timing (frames.c). `kc_frames_frame` and `kc_frames_present` are the two
 * clocks; `kc_frames_rendered` records what the compositor's own render cost,
 * which is what separates "the desktop is slow" from "something else has the
 * machine". Everything is best-effort: no XDG_RUNTIME_DIR, or a consumer that
 * cannot keep up, costs lines and never a frame.
 */
void kc_frames_init(struct kc_server *s);
void kc_frames_frame(struct kc_output *o);
void kc_frames_present(struct kc_output *o,
		       const struct wlr_output_event_present *ev);
void kc_frames_rendered(struct kc_output *o, int64_t ns);
int64_t kc_frames_now(void);
void kc_frames_free(struct kc_server *s);

/*
 * Screen capture and the clipboard (capture.c) — grim, wl-clipboard and
 * xdg-desktop-portal-wlr. Call after kc_shellsvc_init: per-window capture needs
 * the ext-foreign-toplevel list to name windows by.
 */
void kc_capture_init(struct kc_server *s);
void kc_capture_free(struct kc_server *s);
/* From the toplevel's destroy handler, before it is freed. */
void kc_capture_toplevel_free(struct kc_toplevel *t);

/*
 * text-input-v3 + input-method-v2 (textinput.c). Call after the seat exists.
 *
 * `kc_im_key` and `kc_im_modifiers` return true when the input method's
 * keyboard grab has TAKEN the event, in which case the seat must not also see
 * it. They are asked AFTER compositor bindings on purpose — an input method
 * that could swallow Super+Q could trap the session.
 *
 * `kc_im_moved` re-places the candidate window: it is anchored to the focused
 * window's cursor rectangle, so moving the window moves it too.
 */
void kc_im_init(struct kc_server *s);
/* Attach a keyboard to the seat. Public because the virtual-keyboard global
 * lives in textinput.c and its devices come from a protocol, not a backend. */
void kc_keyboard_add(struct kc_server *s, struct wlr_input_device *dev,
		     bool virt);
bool kc_im_key(struct kc_server *s, struct wlr_keyboard *kb,
	       struct wlr_keyboard_key_event *ev);
bool kc_im_modifiers(struct kc_server *s, struct wlr_keyboard *kb);
void kc_im_moved(struct kc_server *s);
void kc_im_free(struct kc_server *s);

/* One tracked pointer constraint — the wrapper that hears its destroy. */
struct kc_pointer_constraint {
	struct kc_server *server;
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
};

/* Pointer capture, gestures, gamma and output power (pointer.c). */
void kc_pointer_init(struct kc_server *s);
void kc_pointer_free(struct kc_server *s);
/* Re-evaluate which constraint is live. A constraint only ever applies to the
 * FOCUSED surface, so this runs on every focus change. */
void kc_pointer_check_constraint(struct kc_server *s);
void kc_pointer_break_constraint(struct kc_server *s);
void kc_pointer_unbreak_constraint(struct kc_server *s);
/* True when the cursor must not move (a lock). A confine clips the deltas. */
bool kc_pointer_constrain(struct kc_server *s, double *dx, double *dy);
void kc_pointer_relative(struct kc_server *s, uint64_t time_usec, double dx,
			 double dy, double dx_unaccel, double dy_unaccel);
/* Where a surface sits in layout coordinates. False when it is not on screen —
 * a confine we cannot place is one we do not enforce. */
bool kc_surface_position(struct kc_server *s, struct wlr_surface *surface,
			 double *lx, double *ly);

void kc_appid_init(struct kc_server *s);
void kc_appid_observe(struct kc_server *s, const char *app_id);
void kc_appid_free(struct kc_server *s);

/* ── a wlr_buffer over plain memory (cellbuf.c) ─────────────────────────
 *
 * wlroots has no public "wlr_buffer from a malloc", so anything putting its own
 * pixels in the scene supplies the smallest impl that works. One copy, shared
 * by the wallpaper and by every window frame.
 *
 * Ownership: create -> paint -> wlr_scene_buffer_set_buffer() -> drop. After
 * the drop the scene holds the only lock and frees it when it lets go.
 */
struct kc_cellbuf {
	struct wlr_buffer base;
	uint32_t *data;			/* ARGB8888, premultiplied */
	size_t stride;
	bool writable;
	pixman_image_t *img;		/* NULL when the caller writes raw */
};

struct kc_cellbuf *kc_cellbuf_create(int w, int h, bool writable, bool want_img);
pixman_image_t *kc_cellbuf_image(struct kc_cellbuf *c);
uint32_t *kc_cellbuf_data(struct kc_cellbuf *c);
size_t kc_cellbuf_stride(struct kc_cellbuf *c);
struct wlr_buffer *kc_cellbuf_base(struct kc_cellbuf *c);
void kc_cellbuf_drop(struct kc_cellbuf *c);

/* ── window frames, drawn as characters (deco.c) ──────────────────────── */

struct kc_deco;

/*
 * The typed descriptor a frame strip's scene node carries — the labwc pattern,
 * adopted after the first live boot proved the alternative wrong.
 *
 * v2 answered "what did the user click" TWICE: once with the scene hit test
 * and once with a hand-rolled geometric walk (`deco_at_cursor`), consulted
 * first. The moment anything stacked above a frame region — the panel, a
 * kdos-menu, a GTK right-click menu, another window — the two answers diverged
 * and the click went to the wrong place or died. That was "mouse not working"
 * on the screenshot. Now every strip node carries this tag, ALL pointer input
 * goes through one wlr_scene_node_at(), and there is no second answer to keep
 * in agreement.
 */
struct kc_deco_part {
	enum kc_node_type type;		/* KC_NODE_DECO — must stay first */
	struct kc_toplevel *toplevel;
};

enum kc_deco_region {
	KC_DECO_NONE = 0,
	KC_DECO_TITLE,			/* drag = move, double = maximise */
	KC_DECO_CLOSE, KC_DECO_MIN, KC_DECO_MAX,
	KC_DECO_W, KC_DECO_E, KC_DECO_S,
	KC_DECO_SW, KC_DECO_SE,
};

/* Resolve the glyph budget against the loaded font, then say what is missing.
 * Both are called once, after the font is loaded — a missing glyph is a blank
 * cell and nothing else, so this is the only warning there will be. */
void kc_deco_glyphs_init(void);
void kc_deco_check_glyphs(void);

bool kc_deco_enabled(const struct kc_server *s);
void kc_deco_create(struct kc_toplevel *t);
void kc_deco_destroy(struct kc_toplevel *t);
/* Reposition and, if anything visible changed, repaint. Cheap to call on every
 * commit — it compares against what it last drew and returns. */
void kc_deco_arrange(struct kc_toplevel *t);
void kc_deco_refocus(struct kc_toplevel *t);
void kc_deco_hide(struct kc_toplevel *t, bool hide);
/* Per OUTPUT, not per session: a 4K panel beside a 1080p one needs two answers,
 * and every frame carries its own cell grid anyway. */
int kc_deco_scale(struct kc_toplevel *t);
int kc_deco_border_w(struct kc_toplevel *t);
int kc_deco_border_h(struct kc_toplevel *t);

enum kc_deco_region kc_deco_at(struct kc_toplevel *t, double lx, double ly);
uint32_t kc_deco_edges(enum kc_deco_region r);
const char *kc_deco_cursor(enum kc_deco_region r);

/* ── window state (window.c) ─────────────────────────────────────────────
 *
 * maximise takes the USABLE box inset by the frame; fullscreen takes the OUTPUT
 * box with the frame hidden. Confusing the two is how a maximised window ends
 * up painted over the panel.
 */
void kc_window_maximize(struct kc_toplevel *t, bool on);
void kc_window_fullscreen(struct kc_toplevel *t, bool on);
void kc_window_shade(struct kc_toplevel *t, bool on);
void kc_window_minimize(struct kc_toplevel *t, bool on);
void kc_window_close(struct kc_toplevel *t);
void kc_window_request_maximize(struct wl_listener *l, void *data);
void kc_window_request_fullscreen(struct wl_listener *l, void *data);
void kc_window_request_minimize(struct wl_listener *l, void *data);

/* ── the compositor's own popups (cellui.c) ─────────────────────────────
 *
 * The window menu and the alt-tab switcher. They are the compositor's rather
 * than the shell's for two reasons: a window menu belongs to a titlebar the
 * compositor drew, and a window switcher must still work when the shell is
 * dead, because it is how you reach another window to fix things from.
 */

struct kc_popup {
	struct kc_server *s;
	struct wlr_scene_buffer *node;
	struct wlr_scene_rect *shadow[2];
	KtuiCell *cells;
	int cols, rows, scale;
	int x, y;			/* layout coords of the top-left cell */
};

enum kc_menu_action {
	KC_MENU_NONE = 0,
	KC_MENU_MOVE, KC_MENU_RESIZE, KC_MENU_MINIMIZE,
	KC_MENU_MAXIMIZE, KC_MENU_SHADE, KC_MENU_CLOSE,
};

void kc_menu_open(struct kc_toplevel *t, double x, double y);
void kc_menu_close(struct kc_server *s);
bool kc_menu_open_p(const struct kc_server *s);
/* Each returns true when it consumed the event. While a menu is up the pointer
 * and the keyboard belong to the compositor — letting motion through would
 * light up hover states in a window the click cannot reach. */
bool kc_menu_key(struct kc_server *s, xkb_keysym_t sym);
bool kc_menu_motion(struct kc_server *s, double x, double y);
bool kc_menu_button(struct kc_server *s, double x, double y, bool pressed);

void kc_switcher_show(struct kc_server *s);
void kc_switcher_hide(struct kc_server *s);
void kc_cellui_free(struct kc_server *s);

/* Start the modal keyboard move/resize on a window — what the menu's Move and
 * Resize items do. In main.c, where the grab state lives. */
void kc_window_kbgrab(struct kc_toplevel *t, bool resize);
/* Drop any cached reference to a toplevel that is going away — the double-click
 * memory holds a raw pointer that a reallocated toplevel would alias. */
void kc_deco_forget(struct kc_toplevel *t);

/* Whether this app_id is on comp.conf's `csd_apps` list — the clients that
 * draw their own titlebar without ever saying so over xdg-decoration. */
bool kc_config_is_csd(const struct kc_server *s, const char *app_id);

#endif /* KDOS_COMP_H */
