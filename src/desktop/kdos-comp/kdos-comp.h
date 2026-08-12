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

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
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
#include <wlr/types/wlr_ext_workspace_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
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
enum kc_node_type { KC_NODE_TOPLEVEL = 1, KC_NODE_XSURFACE };

enum kc_action {
	KC_ACT_SPAWN = 1,
	KC_ACT_CLOSE,
	KC_ACT_QUIT,
	KC_ACT_CYCLE,
	KC_ACT_CYCLE_BACK,
	KC_ACT_WORKSPACE,
	KC_ACT_MOVE_TO_WORKSPACE,
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
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_surface;
	struct wl_list layers;		/* struct kc_layer */

	/* What a panel needs to know. Standard protocols, so waybar and friends
	 * work here too — see shellsvc.c. */
	struct wlr_foreign_toplevel_manager_v1 *ftl_mgr;
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
	int32_t repeat_rate, repeat_delay;
	int snap_px;

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
	double grab_x, grab_y;		/* cursor at grab time */
	struct wlr_box grab_geo;	/* window box at grab time */
	uint32_t resize_edges;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
};

struct kc_output {
	struct wl_list link;
	struct kc_server *server;
	struct wlr_output *wlr_output;
	/* What is left after the panels have taken their exclusive zones. */
	struct wlr_box usable;
	struct wl_listener frame;
	struct wl_listener request_state;
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
	int x, y;			/* scene position, ours to own */
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
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

struct kc_keyboard {
	struct wl_list link;
	struct kc_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

void kc_focus(struct kc_toplevel *t, struct wlr_surface *surface);

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

void kc_appid_init(struct kc_server *s);
void kc_appid_observe(struct kc_server *s, const char *app_id);
void kc_appid_free(struct kc_server *s);

#endif /* KDOS_COMP_H */
