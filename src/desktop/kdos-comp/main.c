/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — outputs, windows, pointer, keyboard, VT switching,
 *               workspaces, alt-tab, interactive move/resize with snapping
 * ---------------------------------
 */

#include <assert.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kcell.h"
#include "kdos-comp.h"

/* ── spawning ──────────────────────────────────────────────────────────── */

/*
 * Everything a child must be handed back before it execs, and both halves cost
 * a debug cycle each:
 *
 * - **The signal MASK survives exec.** `wl_event_loop_add_signal` blocks the
 *   signal it watches — that is how it turns it into a signalfd event — so
 *   kdos-comp runs with SIGHUP, SIGINT and SIGTERM blocked and EVERY child it
 *   spawned inherited the block. Measured: `kdos theme amber` SIGHUPs
 *   kdos-shell to retint the panel live, the signal was never delivered, and
 *   the accent switch silently did nothing to the running session. `kill` on a
 *   launcher-started app did nothing either.
 * - **SIG_IGN survives exec too** (SIG_DFL and handlers do not). The
 *   compositor ignores SIGPIPE so a dead client cannot kill it; a shell that
 *   inherits that ignore never dies on a closed pipe, so `foo | head` runs foo
 *   to completion.
 */
static void child_reset_signals(void)
{
	sigset_t none;
	sigemptyset(&none);
	sigprocmask(SIG_SETMASK, &none, NULL);
	signal(SIGPIPE, SIG_DFL);
}

void kc_spawn(const char *const *argv)
{
	pid_t p = fork();
	if (p < 0)
		return;
	if (p == 0) {
		/* Double fork so the compositor never has to reap. The
		 * intermediate exits immediately and init adopts the child. */
		if (fork() == 0) {
			setsid();
			child_reset_signals();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	}
	waitpid(p, NULL, 0);
}

/*
 * ── supervising the startup children ───────────────────────────────────────
 *
 * kc_spawn double-forks so the compositor never reaps, which is right for a
 * terminal a keybinding opened and wrong for the SHELL: kdos-shell draws the
 * panel, the window list and the launcher, and kdos-notifyd owns
 * org.freedesktop.Notifications. When one of those exits the session keeps
 * running with no chrome and no way to get any back except a keybinding the
 * user has to already know — measured in QEMU, where kdos-notifyd was gone by
 * the time anything asked it for a notification and nothing said so.
 *
 * So startup entries are spawned with ONE fork, their pid is kept, and SIGCHLD
 * arrives through the event loop like every other event. A child that keeps
 * dying is not respawned forever: RESPAWN_MAX failures inside RESPAWN_WINDOW_S
 * stop it, because a crash loop that repaints the screen sixty times a second
 * is worse than a missing panel and hides the log line that explains it.
 */
#define RESPAWN_MAX	  5
#define RESPAWN_WINDOW_S  30

static void spawn_startup_one(struct kc_server *s, int i)
{
	pid_t p = fork();
	if (p < 0) {
		wlr_log(WLR_ERROR, "cannot fork for %s", s->startup[i][0]);
		return;
	}
	if (p == 0) {
		setsid();
		child_reset_signals();
		execvp(s->startup[i][0], s->startup[i]);
		_exit(127);
	}
	s->startup_pid[i] = p;
}

static int handle_sigchld(int sig, void *data)
{
	struct kc_server *s = data;
	pid_t p;
	int st;
	(void)sig;

	while ((p = waitpid(-1, &st, WNOHANG)) > 0) {
		for (int i = 0; i < s->nstartup; i++) {
			if (s->startup_pid[i] != p)
				continue;
			s->startup_pid[i] = 0;

			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (now.tv_sec - s->startup_since[i] > RESPAWN_WINDOW_S) {
				s->startup_since[i] = now.tv_sec;
				s->startup_fails[i] = 0;
			}
			if (++s->startup_fails[i] > RESPAWN_MAX) {
				wlr_log(WLR_ERROR,
					"%s died %d times in %ds — not "
					"restarting it again",
					s->startup[i][0], s->startup_fails[i],
					RESPAWN_WINDOW_S);
				break;
			}
			wlr_log(WLR_INFO, "%s exited (status %d) — restarting",
				s->startup[i][0], st);
			spawn_startup_one(s, i);
			break;
		}
	}
	return 0;
}

/* ── focus ─────────────────────────────────────────────────────────────── */

static struct kc_toplevel *focused_toplevel(struct kc_server *s);

/*
 * `reorder` is what separates a real focus change from an alt-tab preview.
 * Moving the window to the head of the MRU list is the thing that must NOT
 * happen while a cycle is running — see kc_server.cycling.
 */
static void focus_toplevel(struct kc_toplevel *t, struct wlr_surface *surface,
			   bool reorder)
{
	if (!t)
		return;
	struct kc_server *s = t->server;
	/*
	 * While the session is locked NOTHING below the lock tree may take the
	 * keyboard — not a window that just mapped, not a client that asked to
	 * be activated. One check, here, because every focus change in the
	 * compositor goes through this function.
	 */
	if (kc_locked(s))
		return;
	struct wlr_surface *prev = s->seat->keyboard_state.focused_surface;
	if (prev == surface)
		return;

	struct kc_toplevel *prev_t = NULL;
	if (prev) {
		/* Deactivate the old one so it repaints as unfocused. Popups
		 * are not toplevels and must not be touched here. */
		struct wlr_xdg_toplevel *pt =
			wlr_xdg_toplevel_try_from_wlr_surface(prev);
		if (pt)
			wlr_xdg_toplevel_set_activated(pt, false);
		/* Found before the seat's focus moves, because that is what
		 * kc_deco_refocus() reads to decide the frame weight. */
		prev_t = focused_toplevel(s);
	}

	wlr_scene_node_raise_to_top(&t->scene_tree->node);
	if (reorder) {
		wl_list_remove(&t->link);
		wl_list_insert(&s->toplevels, &t->link);
	}
	wlr_xdg_toplevel_set_activated(t->xdg_toplevel, true);

	/*
	 * The `enter` happens EITHER WAY. Skipping it when the seat has no
	 * keyboard looks like a harmless guard and is the difference between a
	 * desktop you can type into and one you cannot: keyboard focus is a
	 * property of the SEAT, and without the enter the seat has no focused
	 * surface, so `wlr_seat_keyboard_notify_key` later delivers every
	 * keystroke to nobody. Compositor bindings still fire — they read the
	 * device event before the seat is involved — so the symptom is a
	 * session where Super+Return opens a terminal that then ignores the
	 * keyboard entirely. lock.c has always had both branches; this one
	 * had only the first.
	 */
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		wlr_seat_keyboard_notify_enter(s->seat,
			t->xdg_toplevel->base->surface, kb->keycodes,
			kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(s->seat,
			t->xdg_toplevel->base->surface, NULL, 0, NULL);

	/* Both frames change: one loses its double rule, the other gains it.
	 * The old one is repainted AFTER the seat has moved on, because the
	 * frame weight is read off the seat's focused surface. */
	if (prev_t && prev_t != t)
		kc_deco_refocus(prev_t);
	kc_deco_refocus(t);

	/* The taskbar highlights the focused entry, so a focus change is a
	 * change to BOTH windows' reported state. */
	kc_shellsvc_refresh(s);
}

void kc_focus(struct kc_toplevel *t, struct wlr_surface *surface)
{
	focus_toplevel(t, surface, true);
}

static struct kc_toplevel *focused_toplevel(struct kc_server *s)
{
	struct wlr_surface *f = s->seat->keyboard_state.focused_surface;
	if (!f)
		return NULL;
	struct kc_toplevel *t;
	wl_list_for_each(t, &s->toplevels, link)
		if (t->xdg_toplevel->base->surface == f)
			return t;
	return NULL;
}

/* ── workspaces ────────────────────────────────────────────────────────── */

/*
 * Focus the most recent window on the current workspace, or nothing at all.
 *
 * The `nothing at all` half is not optional: a seat whose keyboard focus is
 * still on a window that has just been hidden delivers every keystroke to a
 * surface the user cannot see.
 */
static void focus_top_of_ws(struct kc_server *s)
{
	struct kc_toplevel *t;
	wl_list_for_each(t, &s->toplevels, link) {
		if (t->ws == s->cur_ws) {
			focus_toplevel(t, t->xdg_toplevel->base->surface, true);
			return;
		}
	}
	wlr_seat_keyboard_notify_clear_focus(s->seat);
	wlr_seat_pointer_clear_focus(s->seat);
}

static void end_interactive(struct kc_server *s);

/* Give the keyboard back to the session — what an unlock ends with. Public
 * because lock.c is the only caller and the MRU list is main.c's. */
void kc_refocus(struct kc_server *s)
{
	focus_top_of_ws(s);
}

void kc_ws_switch(struct kc_server *s, int n)
{
	if (n < 0 || n >= KC_WORKSPACES || n == s->cur_ws)
		return;
	/* A drag that survives the switch is a grab on a window nobody can see:
	 * the pointer keeps moving something invisible and the next click does
	 * nothing. Switching workspaces ends it. */
	if (s->cursor_mode != KC_CURSOR_PASSTHROUGH)
		end_interactive(s);
	wlr_scene_node_set_enabled(&s->ws_tree[s->cur_ws]->node, false);
	s->cur_ws = n;
	wlr_scene_node_set_enabled(&s->ws_tree[n]->node, true);
	focus_top_of_ws(s);
	/* Every window's reported visibility is relative to the current
	 * workspace, so the taskbar's whole view changes here, not just the two
	 * windows that swapped. */
	kc_shellsvc_refresh(s);
}

static void ws_move(struct kc_server *s, int n)
{
	struct kc_toplevel *t = focused_toplevel(s);
	if (!t || n < 0 || n >= KC_WORKSPACES || n == t->ws)
		return;
	t->ws = n;
	wlr_scene_node_reparent(&t->scene_tree->node, s->ws_tree[n]);
	kc_shellsvc_update(s, t);
	/* The window is gone from here, so something else has to take the
	 * keyboard — otherwise focus follows it to a workspace nobody is
	 * looking at. */
	focus_top_of_ws(s);
}

/* ── alt-tab ───────────────────────────────────────────────────────────── */

/*
 * The next window on this workspace, walking the MRU list from `from`.
 *
 * The walk is bounded by returning to where it started rather than by a count:
 * the list is circular and the head is a sentinel, so the only two ways out are
 * a match or a full lap.
 */
static struct kc_toplevel *ws_next(struct kc_server *s, struct kc_toplevel *from,
				   bool back)
{
	struct wl_list *start = from ? &from->link : &s->toplevels;
	struct wl_list *p = start;
	do {
		p = back ? p->prev : p->next;
		if (p == &s->toplevels)		/* skip the head sentinel */
			p = back ? p->prev : p->next;
		if (p == start)
			break;
		struct kc_toplevel *t = wl_container_of(p, t, link);
		if (t->ws == s->cur_ws)
			return t;
	} while (p != start);
	return NULL;
}

static void cycle_step(struct kc_server *s, bool back)
{
	struct kc_toplevel *from = s->cycling ? s->cycle_at : focused_toplevel(s);
	struct kc_toplevel *t = ws_next(s, from, back);
	if (!t)
		return;
	s->cycling = true;
	s->cycle_at = t;
	/* Raise the candidate as well as listing it: the switcher says which
	 * window is selected, and raising it says what that window contains. */
	focus_toplevel(t, t->xdg_toplevel->base->surface, false);
	kc_switcher_show(s);
}

/* Super came up: the selection is now the choice, so it takes the MRU head. */
static void cycle_commit(struct kc_server *s)
{
	struct kc_toplevel *t = s->cycle_at;
	s->cycling = false;
	s->cycle_at = NULL;
	kc_switcher_hide(s);
	if (!t)
		return;
	wl_list_remove(&t->link);
	wl_list_insert(&s->toplevels, &t->link);
}

/* ── keyboard ──────────────────────────────────────────────────────────── */

/* Defined below, with the rest of the modal grab. */
static void kbgrab_end(struct kc_server *s, bool keep);

static void run_action(struct kc_server *s, const struct kc_bind *b)
{
	switch (b->action) {
	case KC_ACT_SPAWN:
		kc_spawn((const char *const *)b->argv);
		break;
	case KC_ACT_CLOSE: {
		/* The FOCUSED window, not the head of the list. While a cycle is
		 * running those are different, and closing the wrong one is not
		 * a mistake the user can undo. */
		struct kc_toplevel *t = focused_toplevel(s);
		if (t)
			wlr_xdg_toplevel_send_close(t->xdg_toplevel);
		break;
	}
	case KC_ACT_QUIT:
		wl_display_terminate(s->display);
		break;
	case KC_ACT_CYCLE:
		cycle_step(s, false);
		break;
	case KC_ACT_CYCLE_BACK:
		cycle_step(s, true);
		break;
	case KC_ACT_WORKSPACE:
		kc_ws_switch(s, b->arg);
		break;
	case KC_ACT_MOVE_TO_WORKSPACE:
		ws_move(s, b->arg);
		break;
	case KC_ACT_LOCK: {
		/* The lock CLIENT is spawned; the compositor does not lock
		 * itself. A lock nobody can type a password into is a reboot,
		 * so the state only changes once the client asks for it. */
		static const char *const argv[] = { "kdos-lock", NULL };
		if (!kc_locked(s))
			kc_spawn(argv);
		break;
	}
	/*
	 * The keyboard half of the window frame.
	 *
	 * Every control the titlebar offers has one, which is not symmetry for
	 * its own sake: a window can be dragged off the bottom of the screen, a
	 * pointer can be missing, and an unframed CSD window has no titlebar to
	 * aim at at all. These reach it anyway.
	 */
	case KC_ACT_MAXIMIZE: {
		struct kc_toplevel *t = focused_toplevel(s);
		if (t)
			kc_window_maximize(t, !t->maximized);
		break;
	}
	case KC_ACT_FULLSCREEN: {
		struct kc_toplevel *t = focused_toplevel(s);
		if (t)
			kc_window_fullscreen(t, !t->fullscreen);
		break;
	}
	case KC_ACT_SHADE: {
		struct kc_toplevel *t = focused_toplevel(s);
		if (t)
			kc_window_shade(t, !t->shaded);
		break;
	}
	case KC_ACT_MINIMIZE: {
		struct kc_toplevel *t = focused_toplevel(s);
		if (t)
			kc_window_minimize(t, true);
		break;
	}
	/*
	 * Keyboard move and resize.
	 *
	 * The pointer grab is a drag; this one is MODAL — it takes the arrow
	 * keys until Enter or Escape ends it, exactly as Alt+F7 has worked
	 * since GNOME 1. It exists for the cases the mouse cannot reach: a
	 * window dragged mostly off the screen, a machine with no pointer, and
	 * a CSD window with no titlebar to aim at.
	 */
	case KC_ACT_ASCII:
		kc_ascii_toggle(s);
		break;
	case KC_ACT_MOVE_KB:
	case KC_ACT_RESIZE_KB: {
		struct kc_toplevel *t = focused_toplevel(s);
		if (!t || t->fullscreen)
			break;
		/* Pressing it twice cancels, which kc_window_kbgrab handles. */
		kc_window_kbgrab(t, b->action == KC_ACT_RESIZE_KB);
		break;
	}
	}
}

/*
 * Compositor bindings, from the config (which has already been given its
 * defaults — Super, not Alt, because Alt belongs to the applications).
 *
 * The mask comparison is EQUALITY, not `contains`: Super+Shift+1 must not also
 * fire the Super+1 binding, and those two mean different things.
 *
 * Returns true when the key was consumed and must not reach the client.
 */
/* ── keyboard move / resize ────────────────────────────────────────────── */

/*
 * A modal grab, ended by Enter (keep) or Escape (put it back).
 *
 * Putting it back is the whole reason the starting box is saved: a user who
 * nudges a window twenty times and then thinks better of it has no other way to
 * undo it, and "Escape leaves it where you last pushed it" is the behaviour
 * that makes people stop using the feature.
 */
static void kbgrab_end(struct kc_server *s, bool keep)
{
	struct kc_toplevel *t = s->kbgrab;
	if (!t)
		return;
	s->kbgrab = NULL;

	if (!keep) {
		t->x = s->kbgrab_from.x;
		t->y = s->kbgrab_from.y;
		wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
		if (s->kbgrab_resize)
			wlr_xdg_toplevel_set_size(t->xdg_toplevel,
						  s->kbgrab_from.width,
						  s->kbgrab_from.height);
	}
	kc_deco_arrange(t);
}

/*
 * Begin a modal move or resize. Public because the window menu's Move and
 * Resize items are the other way in, and the grab state is main.c's.
 */
void kc_window_kbgrab(struct kc_toplevel *t, bool resize)
{
	struct kc_server *s = t->server;
	if (t->fullscreen)
		return;
	if (s->kbgrab) {
		kbgrab_end(s, false);
		return;
	}
	s->kbgrab = t;
	s->kbgrab_resize = resize;
	s->kbgrab_from.x = t->x;
	s->kbgrab_from.y = t->y;
	struct wlr_box g = t->xdg_toplevel->base->geometry;
	s->kbgrab_from.width = g.width;
	s->kbgrab_from.height = g.height;
}

static bool kbgrab_key(struct kc_server *s, xkb_keysym_t sym)
{
	struct kc_toplevel *t = s->kbgrab;
	if (!t)
		return false;

	/* One cell a press, a whole eight with Shift — the same relationship
	 * the arrow keys have to a text field, and in the unit this desktop
	 * measures everything else in. */
	int step = kc_deco_enabled(s) ? kc_deco_border_h(t) : 20;
	uint32_t mods = 0;
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		mods = wlr_keyboard_get_modifiers(kb);
	if (mods & WLR_MODIFIER_SHIFT)
		step *= 8;

	int dx = 0, dy = 0;
	switch (sym) {
	case XKB_KEY_Left:   dx = -step; break;
	case XKB_KEY_Right:  dx =  step; break;
	case XKB_KEY_Up:     dy = -step; break;
	case XKB_KEY_Down:   dy =  step; break;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		kbgrab_end(s, true);
		return true;
	case XKB_KEY_Escape:
		kbgrab_end(s, false);
		return true;
	default:
		/* Anything else ends the grab and is NOT swallowed: a user who
		 * starts typing has stopped moving the window, and eating the
		 * first character would be a keystroke lost with no explanation. */
		kbgrab_end(s, true);
		return false;
	}

	if (s->kbgrab_resize) {
		struct wlr_box g = t->xdg_toplevel->base->geometry;
		int w = g.width + dx, h = g.height + dy;
		if (w < step)
			w = step;
		if (h < step)
			h = step;
		wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
	} else {
		t->x += dx;
		t->y += dy;
		wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
		kc_deco_arrange(t);
	}
	return true;
}

static bool handle_binding(struct kc_server *s, xkb_keysym_t sym, uint32_t mods)
{
	struct kc_bind *b;
	wl_list_for_each(b, &s->binds, link) {
		if (b->sym == sym && (mods & KC_MOD_MASK) == b->mods) {
			run_action(s, b);
			return true;
		}
	}
	return false;
}

/*
 * VT switching, which tinywl does not implement.
 *
 * Without this the session owns the only VT you can see and Ctrl+Alt+F2 does
 * nothing — on real hardware that means no way back to a root shell when the
 * desktop misbehaves, which is exactly when you need one. XKB reports the
 * combination as XF86Switch_VT_1..12 and libseat performs the switch.
 *
 * Under the headless backend there is no session at all, so this is a no-op
 * rather than a crash.
 */
static bool handle_vt_switch(struct kc_server *s, xkb_keysym_t sym)
{
	if (sym < XKB_KEY_XF86Switch_VT_1 || sym > XKB_KEY_XF86Switch_VT_12)
		return false;
	if (s->session)
		wlr_session_change_vt(s->session, sym - XKB_KEY_XF86Switch_VT_1 + 1);
	return true;
}

static void keyboard_modifiers(struct wl_listener *l, void *data)
{
	struct kc_keyboard *kb = wl_container_of(l, kb, modifiers);
	(void)data;
	/* Releasing Super is what ends an alt-tab. There is no key event for a
	 * modifier going up that handle_binding could see — this is the only
	 * place the release is observable. */
	if (kb->server->cycling &&
	    !(wlr_keyboard_get_modifiers(kb->wlr_keyboard) & WLR_MODIFIER_LOGO))
		cycle_commit(kb->server);
	/* An input method holding the keyboard grab gets the modifiers too, and
	 * INSTEAD of the client: a Shift the application also saw would be
	 * applied twice, once by the IME's own state machine and once by the
	 * text field. */
	if (!kb->virt && kc_im_modifiers(kb->server, kb->wlr_keyboard))
		return;
	wlr_seat_set_keyboard(kb->server->seat, kb->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(kb->server->seat,
					   &kb->wlr_keyboard->modifiers);
}

static void keyboard_key(struct wl_listener *l, void *data)
{
	struct kc_keyboard *kb = wl_container_of(l, kb, key);
	struct kc_server *s = kb->server;
	struct wlr_keyboard_key_event *ev = data;

	uint32_t keycode = ev->keycode + 8;	/* libinput -> xkb */
	const xkb_keysym_t *syms;
	int n = xkb_state_key_get_syms(kb->wlr_keyboard->xkb_state, keycode,
				       &syms);

	/*
	 * Bindings are matched against the UNSHIFTED keysym, taken from level 0
	 * of the current layout rather than from the keyboard state. Shift
	 * rewrites the state's keysym — on a us layout Super+Shift+1 arrives as
	 * `exclam`, not as `1` — so a binding table written in digits would
	 * never see the shifted half of Super+Shift+N at all. The shift is read
	 * from the modifier mask instead, where it means the same thing on every
	 * layout.
	 *
	 * VT switching deliberately keeps using the state-derived keysym: the
	 * keymap only produces XF86Switch_VT_n at the Ctrl+Alt level, so at
	 * level 0 there is nothing to match.
	 */
	struct xkb_keymap *keymap = xkb_state_get_keymap(kb->wlr_keyboard->xkb_state);
	xkb_layout_index_t layout =
		xkb_state_key_get_layout(kb->wlr_keyboard->xkb_state, keycode);
	const xkb_keysym_t *raw;
	int nraw = xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0,
						    &raw);

	bool handled = false;
	uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

	kc_idle_activity(s);

	if (ev->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* VT switching is checked FIRST and without a modifier test:
		 * the keymap only produces XF86Switch_VT_n for the full
		 * Ctrl+Alt+Fn combination anyway, and it must work even if a
		 * client has taken every other grab. */
		for (int i = 0; i < n && !handled; i++)
			handled = handle_vt_switch(s, syms[i]);

		/*
		 * No compositor binding fires while the session is locked. VT
		 * switching above is the deliberate exception — it is the way
		 * out of a wedged session and must survive everything — but
		 * Super+Q on a lock screen must not quit the compositor, and
		 * Super+1 must not reveal workspace 1 behind it.
		 */
		if (kc_locked(s))
			handled = false;
		/*
		 * A keyboard move or resize owns the keyboard while it runs —
		 * BEFORE the bindings, so Escape cancels it rather than
		 * reaching whatever Escape is bound to, and after the lock
		 * check, so it cannot run on a locked screen.
		 */
		else if (!handled && s->menu)
			for (int i = 0; i < nraw && !handled; i++)
				handled = kc_menu_key(s, raw[i]);
		else if (!handled && s->kbgrab)
			for (int i = 0; i < nraw && !handled; i++)
				handled = kbgrab_key(s, raw[i]);
		/* Any modifier at all is enough to be worth a lookup — the
		 * binding itself decides which. A modifier-less key is never a
		 * binding, because the config refuses to create one. */
		else if (!handled && (mods & KC_MOD_MASK))
			for (int i = 0; i < nraw && !handled; i++)
				handled = handle_binding(s, raw[i], mods);
	}

	/*
	 * The input method's keyboard grab comes AFTER the bindings and after
	 * the VT keys. An IME that could swallow Super+Q or Ctrl+Alt+F2 would be
	 * an IME that can trap the session, and a candidate window is exactly
	 * the state a user is in when they want out.
	 */
	if (!handled && !kb->virt && kc_im_key(s, kb->wlr_keyboard, ev))
		handled = true;

	if (!handled) {
		wlr_seat_set_keyboard(s->seat, kb->wlr_keyboard);
		wlr_seat_keyboard_notify_key(s->seat, ev->time_msec,
					     ev->keycode, ev->state);
	}
}

static void keyboard_destroy(struct wl_listener *l, void *data)
{
	struct kc_keyboard *kb = wl_container_of(l, kb, destroy);
	(void)data;
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	wl_list_remove(&kb->link);
	free(kb);
}

void kc_keyboard_add(struct kc_server *s, struct wlr_input_device *dev,
		     bool virt)
{
	struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(dev);
	struct kc_keyboard *kb = calloc(1, sizeof(*kb));
	if (!kb)
		return;
	kb->server = s;
	kb->wlr_keyboard = wlr_kb;
	kb->virt = virt;

	/*
	 * The keymap comes from the environment (XKB_DEFAULT_LAYOUT and
	 * friends), which is what /etc/profile.d sets and what kinstall
	 * writes. Compiling a hardcoded "us" here would silently ignore the
	 * layout the installer just asked the user to choose.
	 */
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap =
		xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (keymap) {
		wlr_keyboard_set_keymap(wlr_kb, keymap);
		xkb_keymap_unref(keymap);
	} else {
		wlr_log(WLR_ERROR, "no xkb keymap — keyboard will be unusable");
	}
	xkb_context_unref(ctx);
	wlr_keyboard_set_repeat_info(wlr_kb, s->repeat_rate, s->repeat_delay);

	kb->modifiers.notify = keyboard_modifiers;
	wl_signal_add(&wlr_kb->events.modifiers, &kb->modifiers);
	kb->key.notify = keyboard_key;
	wl_signal_add(&wlr_kb->events.key, &kb->key);
	kb->destroy.notify = keyboard_destroy;
	wl_signal_add(&dev->events.destroy, &kb->destroy);

	wlr_seat_set_keyboard(s->seat, wlr_kb);
	wl_list_insert(&s->keyboards, &kb->link);

	/*
	 * A seat with no keyboard capability makes clients ignore key events
	 * entirely — they never even ask for a wl_keyboard. This has to happen
	 * HERE rather than only in new_input, because a virtual keyboard arrives
	 * through a protocol rather than through the backend: measured with
	 * imtest, where the input method's own virtual keyboard was the only
	 * keyboard on the seat and every key it sent went nowhere.
	 */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(s->seat, caps);
}

static void new_input(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_input);
	struct wlr_input_device *dev = data;

	switch (dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		kc_keyboard_add(s, dev, false);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		wlr_cursor_attach_input_device(s->cursor, dev);
		break;
	default:
		break;
	}

	/* The advertised set tracks what is attached; kc_keyboard_add does the
	 * same for keyboards that arrive over a protocol rather than a backend. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&s->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(s->seat, caps);
}

/* ── interactive move and resize ───────────────────────────────────────── */

/*
 * The window under the cursor, walked back from the scene node.
 *
 * A scene node is a buffer deep inside a surface tree; only the toplevel's own
 * tree carries our pointer in node.data, so this climbs until it finds one —
 * and then CHECKS THE TAG, because an X11 surface's tree carries a
 * kc_xsurface* in exactly the same field. Returning that as a kc_toplevel*
 * would read `xdg_toplevel` out of the middle of an unrelated struct, and the
 * first Super+drag on an X11 window would hand a garbage pointer to
 * wlr_xdg_toplevel_set_size.
 */
static struct kc_toplevel *toplevel_at(struct kc_server *s, double lx, double ly,
				       struct wlr_surface **surface,
				       double *sx, double *sy)
{
	struct wlr_scene_node *node =
		wlr_scene_node_at(&s->scene->tree.node, lx, ly, sx, sy);
	if (!node || node->type != WLR_SCENE_NODE_BUFFER)
		return NULL;
	struct wlr_scene_buffer *b = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(b);
	if (!ss)
		return NULL;
	if (surface)
		*surface = ss->surface;
	struct wlr_scene_tree *tree = node->parent;
	while (tree && !tree->node.data)
		tree = tree->node.parent;
	if (!tree)
		return NULL;
	enum kc_node_type *tag = tree->node.data;
	return *tag == KC_NODE_TOPLEVEL ? (struct kc_toplevel *)tag : NULL;
}

/*
 * Which window's FRAME is under the pointer, if any.
 *
 * Walked in stacking order — s->toplevels is the MRU list and focus always
 * raises, so its head is the topmost window — and it stops at the first window
 * whose own rectangle contains the cursor. Without that stop, pointing at the
 * middle of a window would find the frame of something BEHIND it and put a
 * resize cursor on a border nobody can see.
 */
static struct kc_toplevel *deco_at_cursor(struct kc_server *s,
					  enum kc_deco_region *out)
{
	struct kc_toplevel *t;

	if (s->cursor_mode != KC_CURSOR_PASSTHROUGH || kc_locked(s))
		return NULL;

	wl_list_for_each(t, &s->toplevels, link) {
		if (t->minimized)
			continue;
		if (t->ws != s->cur_ws && !t->fullscreen)
			continue;

		enum kc_deco_region r = kc_deco_at(t, s->cursor->x, s->cursor->y);
		if (r != KC_DECO_NONE) {
			if (out)
				*out = r;
			return t;
		}

		struct wlr_box g = t->xdg_toplevel->base->geometry;
		int gh = t->shaded ? 0 : g.height;
		if (s->cursor->x >= t->x && s->cursor->y >= t->y &&
		    s->cursor->x < t->x + g.width && s->cursor->y < t->y + gh)
			return NULL;		/* the client's own pixels */
	}
	return NULL;
}

static void begin_interactive(struct kc_toplevel *t, enum kc_cursor_mode mode,
			      uint32_t edges)
{
	struct kc_server *s = t->server;

	/* Refuse a grab on a window that does not have keyboard focus. Without
	 * this a click-through on an unfocused window starts dragging it, which
	 * is how you move a window you meant to click a button in. */
	if (t->xdg_toplevel->base->surface !=
	    s->seat->keyboard_state.focused_surface)
		return;

	s->grabbed = t;
	s->cursor_mode = mode;
	s->grab_x = s->cursor->x;
	s->grab_y = s->cursor->y;
	s->grab_geo = t->xdg_toplevel->base->geometry;
	s->grab_geo.x = t->x;
	s->grab_geo.y = t->y;
	s->resize_edges = edges;
}

static void end_interactive(struct kc_server *s)
{
	s->cursor_mode = KC_CURSOR_PASSTHROUGH;
	s->grabbed = NULL;
}

/*
 * Snap on release. Dragging to an edge fills that half; a corner fills a
 * quarter; the top maximises.
 *
 * The test is on the CURSOR, not on the window box, because that is what the
 * user is aiming with — a window dragged by its right-hand side would
 * otherwise snap when its left edge is nowhere near the screen.
 */
/*
 * Where an edge-drag would put the window, or false if it would not.
 *
 * Split out of snap_on_release() so the PREVIEW and the SNAP cannot disagree.
 * Two functions that each computed the box would eventually differ by a pixel,
 * and the one thing a preview must never do is show a rectangle the window then
 * does not land in.
 */
static bool snap_target(struct kc_server *s, struct wlr_box *out)
{
	int snap = s->snap_px;
	if (snap <= 0)
		return false;

	struct wlr_box ob;
	if (!kc_usable_at(s, s->cursor->x, s->cursor->y, &ob))
		return false;

	int left   = s->cursor->x <= ob.x + snap;
	int right  = s->cursor->x >= ob.x + ob.width  - snap;
	int top    = s->cursor->y <= ob.y + snap;
	int bottom = s->cursor->y >= ob.y + ob.height - snap;

	if (!left && !right && !top && !bottom)
		return false;

	struct wlr_box g = ob;
	if (left || right) {
		g.width = ob.width / 2;
		g.x = left ? ob.x : ob.x + ob.width - g.width;
		if (top || bottom) {			/* corner -> quarter */
			g.height = ob.height / 2;
			g.y = top ? ob.y : ob.y + ob.height - g.height;
		}
	} else if (top) {
		;					/* the whole usable box */
	} else {
		return false;
	}
	*out = g;
	return true;
}

/*
 * Show it while the drag is still running.
 *
 * Snapping used to happen silently on release, so an edge drag gave no warning
 * of what it was about to do — you found out by having it done. The preview is
 * one translucent rect in the accent, created on demand and hidden the moment
 * the cursor leaves the edge.
 */
static void snap_preview(struct kc_server *s)
{
	struct wlr_box g;
	bool show = s->cursor_mode == KC_CURSOR_MOVE && snap_target(s, &g);

	if (!show) {
		if (s->snap_rect)
			wlr_scene_node_set_enabled(&s->snap_rect->node, false);
		return;
	}

	if (!s->snap_rect) {
		KRgb a = ktui_theme->slot[KT_ACCENT];
		float rgba[4] = { a.r / 255.f * 0.35f, a.g / 255.f * 0.35f,
				  a.b / 255.f * 0.35f, 0.35f };
		/* In layer_above so it is over the windows it is describing, and
		 * created lazily so a session that never drags never pays. */
		s->snap_rect = wlr_scene_rect_create(s->layer_above, 1, 1, rgba);
		if (!s->snap_rect)
			return;
	}
	wlr_scene_rect_set_size(s->snap_rect, g.width, g.height);
	wlr_scene_node_set_position(&s->snap_rect->node, g.x, g.y);
	wlr_scene_node_set_enabled(&s->snap_rect->node, true);
	wlr_scene_node_raise_to_top(&s->snap_rect->node);
}

static void snap_on_release(struct kc_server *s, struct kc_toplevel *t)
{
	/*
	 * The same box the preview drew, from the same function. Two
	 * computations would eventually differ by a pixel and the window would
	 * land somewhere the preview did not promise.
	 *
	 * The USABLE box, not the output's, and the test is on the CURSOR
	 * rather than on the window: a window dragged by its right-hand side
	 * would otherwise snap when its left edge is nowhere near the screen,
	 * and snapping to the output box paints a maximised window straight
	 * over the panel.
	 */
	struct wlr_box g;
	if (s->snap_rect)
		wlr_scene_node_set_enabled(&s->snap_rect->node, false);
	if (!snap_target(s, &g))
		return;

	t->x = g.x;
	t->y = g.y;
	wlr_scene_node_set_position(&t->scene_tree->node, g.x, g.y);
	wlr_xdg_toplevel_set_size(t->xdg_toplevel, g.width, g.height);
	/* Deliberately NOT marked maximised, even for the top-edge snap that
	 * fills the whole usable box: `maximized` owns the saved box that
	 * restores the window, and a snap has no "before" to go back to. */
}

static void process_move(struct kc_server *s)
{
	struct kc_toplevel *t = s->grabbed;
	t->x = (int)(s->grab_geo.x + (s->cursor->x - s->grab_x));
	t->y = (int)(s->grab_geo.y + (s->cursor->y - s->grab_y));
	wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
	/* The candidate window is anchored to this window's cursor rectangle,
	 * so dragging the window has to drag it too. */
	kc_im_moved(s);
	/* And show where an edge would put it, before it is put there. */
	snap_preview(s);
}

static void process_resize(struct kc_server *s)
{
	struct kc_toplevel *t = s->grabbed;
	double dx = s->cursor->x - s->grab_x;
	double dy = s->cursor->y - s->grab_y;

	int x = s->grab_geo.x, y = s->grab_geo.y;
	int w = s->grab_geo.width, h = s->grab_geo.height;

	if (s->resize_edges & WLR_EDGE_TOP) {
		y = (int)(s->grab_geo.y + dy);
		h = (int)(s->grab_geo.height - dy);
	} else if (s->resize_edges & WLR_EDGE_BOTTOM) {
		h = (int)(s->grab_geo.height + dy);
	}
	if (s->resize_edges & WLR_EDGE_LEFT) {
		x = (int)(s->grab_geo.x + dx);
		w = (int)(s->grab_geo.width - dx);
	} else if (s->resize_edges & WLR_EDGE_RIGHT) {
		w = (int)(s->grab_geo.width + dx);
	}

	/* A client asked for a size it cannot honour will clamp it and then
	 * disagree with us about where its edges are, so clamp here too. */
	if (w < 40) w = 40;
	if (h < 30) h = 30;

	t->x = x;
	t->y = y;
	wlr_scene_node_set_position(&t->scene_tree->node, x, y);
	wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
}

/* ── pointer ───────────────────────────────────────────────────────────── */

static void pointer_motion_common(struct kc_server *s, uint32_t time)
{
	/* A grab owns the pointer completely: no enter/leave, no motion to any
	 * client, until the button comes back up. */
	if (s->cursor_mode == KC_CURSOR_MOVE) {
		process_move(s);
		return;
	}
	if (s->cursor_mode == KC_CURSOR_RESIZE) {
		process_resize(s);
		return;
	}

	/* A menu owns the pointer completely while it is open. */
	if (kc_menu_motion(s, s->cursor->x, s->cursor->y))
		return;

	/*
	 * The frame is asked FIRST, and its answer is final.
	 *
	 * A frame strip is a wlr_scene_buffer with no wlr_surface behind it, so
	 * the surface lookup below would simply not find one and the pointer
	 * would fall through to whatever is underneath — which on a stack of
	 * windows is the window BEHIND the frame you are pointing at. Asking
	 * the deco first is also what puts a resize cursor on the edges.
	 */
	{
		struct kc_toplevel *dt = deco_at_cursor(s, NULL);
		if (dt) {
			const char *shape = kc_deco_cursor(
				kc_deco_at(dt, s->cursor->x, s->cursor->y));
			wlr_cursor_set_xcursor(s->cursor, s->cursor_mgr,
					       shape ? shape : "default");
			/* The pointer is on the compositor's own chrome, so no
			 * client may have it — and the one that had it must be
			 * told, or it keeps drawing a hover it cannot see. */
			wlr_seat_pointer_clear_focus(s->seat);
			return;
		}
	}

	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wlr_scene_node *node = wlr_scene_node_at(
		&s->scene->tree.node, s->cursor->x, s->cursor->y, &sx, &sy);

	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *b = wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *ss =
			wlr_scene_surface_try_from_buffer(b);
		if (ss)
			surface = ss->surface;
	}

	if (surface) {
		wlr_seat_pointer_notify_enter(s->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(s->seat, time, sx, sy);
	} else {
		/* Nothing under the pointer: fall back to the default image
		 * and drop the client's focus, or the last surface keeps
		 * receiving motion it cannot see. */
		wlr_cursor_set_xcursor(s->cursor, s->cursor_mgr, "default");
		wlr_seat_pointer_clear_focus(s->seat);
	}
}

static void cursor_motion(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, cursor_motion);
	struct wlr_pointer_motion_event *ev = data;
	kc_idle_activity(s);
	wlr_cursor_move(s->cursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
	pointer_motion_common(s, ev->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *ev = data;
	kc_idle_activity(s);
	wlr_cursor_warp_absolute(s->cursor, &ev->pointer->base, ev->x, ev->y);
	pointer_motion_common(s, ev->time_msec);
}

/*
 * A click that landed on a window frame.
 *
 * Returns true when it was handled, in which case the button never reaches a
 * client — the frame is the compositor's, and a client that saw the press
 * would also see no release.
 *
 * The double-click window is deliberately generous: the titlebar is one CELL
 * tall, which at scale 1 is 32 pixels, and a user aiming at a 32-pixel strip
 * twice is not aiming precisely.
 */
#define DOUBLE_CLICK_MS 400

static bool deco_button(struct kc_server *s, struct kc_toplevel *t,
			enum kc_deco_region region,
			struct wlr_pointer_button_event *ev)
{
	static struct kc_toplevel *last_t;
	static uint32_t last_ms;
	static enum kc_deco_region last_region;

	if (ev->button == BTN_LEFT) {
		switch (region) {
		case KC_DECO_CLOSE:
			kc_window_close(t);
			return true;
		case KC_DECO_MIN:
			kc_window_minimize(t, true);
			return true;
		case KC_DECO_MAX:
			kc_window_maximize(t, !t->maximized);
			return true;
		case KC_DECO_TITLE: {
			bool dbl = last_t == t && last_region == KC_DECO_TITLE &&
				   ev->time_msec - last_ms < DOUBLE_CLICK_MS;
			last_t = t;
			last_ms = ev->time_msec;
			last_region = region;
			if (dbl) {
				/* Consume it, so a third click is a fresh
				 * first click rather than another toggle. */
				last_t = NULL;
				kc_window_maximize(t, !t->maximized);
				return true;
			}
			/* A maximised window is not draggable — there is
			 * nowhere for it to go, and unmaximise-on-drag is a
			 * gesture, not a click. */
			if (!t->maximized && !t->fullscreen)
				begin_interactive(t, KC_CURSOR_MOVE, 0);
			return true;
		}
		default: {
			uint32_t edges = kc_deco_edges(region);
			if (edges && !t->maximized && !t->fullscreen && !t->shaded)
				begin_interactive(t, KC_CURSOR_RESIZE, edges);
			return true;
		}
		}
	}

	/* Right-click the titlebar opens the window menu, where it has been on
	 * every stacking desktop for thirty years. */
	if (ev->button == BTN_RIGHT && region == KC_DECO_TITLE) {
		kc_menu_open(t, s->cursor->x, s->cursor->y);
		return true;
	}

	/* Anything else on the frame is swallowed rather than passed on: the
	 * client has no idea this area exists. */
	(void)s;
	return true;
}

static void cursor_button(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, cursor_button);
	struct wlr_pointer_button_event *ev = data;

	kc_idle_activity(s);

	/*
	 * A locked session has no windows to focus and no grabs to start. The
	 * button still reaches the seat, because the lock surface has the
	 * pointer focus and its own buttons to click.
	 */
	if (kc_locked(s)) {
		wlr_seat_pointer_notify_button(s->seat, ev->time_msec,
					       ev->button, ev->state);
		return;
	}

	if (kc_menu_button(s, s->cursor->x, s->cursor->y,
			   ev->state == WL_POINTER_BUTTON_STATE_PRESSED))
		return;

	if (ev->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (s->cursor_mode != KC_CURSOR_PASSTHROUGH) {
			struct kc_toplevel *t = s->grabbed;
			if (t && s->cursor_mode == KC_CURSOR_MOVE)
				snap_on_release(s, t);
			end_interactive(s);
			return;	/* the release ended the grab, not a click */
		}
		wlr_seat_pointer_notify_button(s->seat, ev->time_msec,
					       ev->button, ev->state);
		return;
	}

	/*
	 * The frame first, and it never reaches a client.
	 *
	 * The focus guard inside begin_interactive() refuses a grab on a window
	 * that does not already have keyboard focus, which is right for the
	 * CLIENT-initiated move — it is what stops a click-through from
	 * dragging a window you meant to click a button in. It is wrong for a
	 * titlebar: clicking the titlebar of a background window must focus,
	 * raise and drag in ONE gesture, which is what every desktop with
	 * titlebars does. So this path focuses first and then grabs, and the
	 * guard stays exactly where it is for the client path.
	 */
	enum kc_deco_region region = KC_DECO_NONE;
	struct kc_toplevel *dt = deco_at_cursor(s, &region);
	if (dt) {
		kc_focus(dt, dt->xdg_toplevel->base->surface);
		if (deco_button(s, dt, region, ev))
			return;
	}

	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct kc_toplevel *t = toplevel_at(s, s->cursor->x, s->cursor->y,
					    &surface, &sx, &sy);
	if (t)
		kc_focus(t, surface);

	/*
	 * Super+drag moves, Super+right-drag resizes. Binding the grab to a
	 * modifier rather than to a titlebar is what lets a window be moved
	 * from anywhere in it — which matters here more than on other
	 * desktops, because most KDOS windows are alien apps drawing their own
	 * client-side decorations and there may be no titlebar to aim at.
	 */
	uint32_t mods = 0;
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		mods = wlr_keyboard_get_modifiers(kb);

	if (t && (mods & WLR_MODIFIER_LOGO)) {
		if (ev->button == BTN_LEFT) {
			begin_interactive(t, KC_CURSOR_MOVE, 0);
			return;
		}
		if (ev->button == BTN_RIGHT) {
			/* Resize from the corner the cursor is nearest, so the
			 * edge that follows the pointer is the one being
			 * dragged rather than an arbitrary fixed corner. */
			uint32_t edges = 0;
			edges |= (s->cursor->x < t->x + t->xdg_toplevel->base->geometry.width / 2)
				 ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
			edges |= (s->cursor->y < t->y + t->xdg_toplevel->base->geometry.height / 2)
				 ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
			begin_interactive(t, KC_CURSOR_RESIZE, edges);
			return;
		}
	}

	wlr_seat_pointer_notify_button(s->seat, ev->time_msec, ev->button,
				       ev->state);
}

static void cursor_axis(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, cursor_axis);
	struct wlr_pointer_axis_event *ev = data;
	kc_idle_activity(s);

	/* Wheel on a titlebar shades and unshades — the gesture every stacking
	 * window manager since the 1990s has had there, and the one place a
	 * scroll over the compositor's own chrome means anything. */
	enum kc_deco_region region = KC_DECO_NONE;
	struct kc_toplevel *dt = deco_at_cursor(s, &region);
	if (dt && region == KC_DECO_TITLE) {
		if (ev->delta != 0)
			kc_window_shade(dt, ev->delta > 0);
		return;
	}

	wlr_seat_pointer_notify_axis(s->seat, ev->time_msec, ev->orientation,
				     ev->delta, ev->delta_discrete, ev->source,
				     ev->relative_direction);
}

static void cursor_frame(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, cursor_frame);
	(void)data;
	wlr_seat_pointer_notify_frame(s->seat);
}

static void request_cursor(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *ev = data;
	/* Only the client with pointer focus may set the image — otherwise
	 * any client could change the cursor at any time. */
	if (ev->seat_client == s->seat->pointer_state.focused_client)
		wlr_cursor_set_surface(s->cursor, ev->surface, ev->hotspot_x,
				       ev->hotspot_y);
}

static void request_set_selection(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, request_set_selection);
	struct wlr_seat_request_set_selection_event *ev = data;
	wlr_seat_set_selection(s->seat, ev->source, ev->serial);
}

/* Middle-click paste. Same shape as the clipboard above, and separate from it
 * on purpose: the two selections are independent, and a compositor that
 * forwarded one to the other would make copying overwrite the highlight. */
static void request_set_primary(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, request_set_primary);
	struct wlr_seat_request_set_primary_selection_event *ev = data;
	wlr_seat_set_primary_selection(s->seat, ev->source, ev->serial);
}

static void request_cursor_shape(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, request_cursor_shape);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *ev = data;

	/* Only the client with pointer focus may set the image, exactly as for
	 * request_cursor() — otherwise any client could change the cursor at
	 * any time, from any workspace. */
	if (ev->seat_client != s->seat->pointer_state.focused_client)
		return;
	/* A grab owns the pointer image: a client repainting its cursor in the
	 * middle of a resize would fight the resize arrow for it. */
	if (s->cursor_mode != KC_CURSOR_PASSTHROUGH)
		return;
	wlr_cursor_set_xcursor(s->cursor, s->cursor_mgr,
			       wlr_cursor_shape_v1_name(ev->shape));
}

/*
 * xdg-activation: a client asking for a window to be raised.
 *
 * The token is checked by wlroots before this fires, so what arrives here has
 * already been vouched for by a focused client. What is NOT delegated is the
 * decision: an activation while the session is locked is refused outright, and
 * a window on another workspace takes the user there rather than being raised
 * invisibly behind the lock or behind another desktop.
 */
static void request_activate(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, request_activate);
	const struct wlr_xdg_activation_v1_request_activate_event *ev = data;

	if (kc_locked(s))
		return;

	struct wlr_xdg_surface *xdg =
		wlr_xdg_surface_try_from_wlr_surface(ev->surface);
	if (!xdg || xdg->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL || !xdg->data)
		return;
	struct wlr_scene_tree *inner = xdg->data;
	if (!inner->node.parent)
		return;
	struct kc_toplevel *t = inner->node.parent->node.data;
	if (!t || t->node_type != KC_NODE_TOPLEVEL)
		return;

	if (t->minimized)
		kc_window_minimize(t, false);
	if (t->ws != s->cur_ws)
		kc_ws_switch(s, t->ws);
	kc_focus(t, t->xdg_toplevel->base->surface);
}

/* ── xdg-shell ─────────────────────────────────────────────────────────── */

/*
 * Where a new window goes.
 *
 * It used to go nowhere: every toplevel mapped at the scene default of 0x0, so
 * every window on this desktop opened in the top-left corner UNDER the panel,
 * and a second window landed exactly on the first. Centring in the output's
 * USABLE box fixes both — a window is never born behind the panel, and it is
 * never off-screen either, because the clamp is against the same box.
 *
 * The cascade is what stops five terminals from being one terminal. It steps
 * by CASCADE_PX and wraps after CASCADE_N, so an eleventh window is on top of
 * the first rather than off the bottom of the screen.
 */
#define CASCADE_PX 28
#define CASCADE_N  8

static void place_new_window(struct kc_server *s, struct kc_toplevel *t)
{
	struct wlr_box u;
	if (!kc_usable_at(s, s->cursor->x, s->cursor->y, &u))
		return;

	/* `base->geometry` is the window geometry the client last committed —
	 * wlroots 0.20 keeps it on the surface and has no getter. It is 0x0 for
	 * a client that has not committed one, which the size guard below is
	 * for. */
	struct wlr_box geo = t->xdg_toplevel->base->geometry;
	/* A client that has not committed a size yet gets the centre of the
	 * usable box and will grow from there; guessing a size for it would put
	 * the window somewhere it never asked to be. */
	int w = geo.width > 0 ? geo.width : 0;
	int h = geo.height > 0 ? geo.height : 0;

	static int step;
	int off = (step++ % CASCADE_N) * CASCADE_PX;

	int x = u.x + (u.width - w) / 2 + off;
	int y = u.y + (u.height - h) / 2 + off;

	/*
	 * The FRAME has to fit, not just the window. A window placed flush
	 * against the top of the usable box puts its titlebar under the panel,
	 * which is the same failure the usable box exists to prevent arriving
	 * one level further in. Zero for a client that decorates itself.
	 */
	int bw = 0, bh = 0;
	if (!t->csd && kc_deco_enabled(s)) {
		bw = kcell_w() * (s->deco_scale > 0 ? s->deco_scale : 1);
		bh = kcell_h() * (s->deco_scale > 0 ? s->deco_scale : 1);
	}

	/* Clamp last, so the cascade can never walk a window off the screen —
	 * and clamp the top edge against the USABLE box, which is what keeps the
	 * title bar out from under the panel. */
	if (x + w + bw > u.x + u.width)
		x = u.x + u.width - w - bw;
	if (y + h + bh > u.y + u.height)
		y = u.y + u.height - h - bh;
	if (x < u.x + bw)
		x = u.x + bw;
	if (y < u.y + bh)
		y = u.y + bh;

	t->x = x;
	t->y = y;
	wlr_scene_node_set_position(&t->scene_tree->node, x, y);
}

static void toplevel_map(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, map);
	struct kc_server *s = t->server;
	(void)data;
	wl_list_insert(&s->toplevels, &t->link);
	/* t->x/t->y are the compositor's record of where the window IS. They
	 * start at the scene default (0,0) and must be kept in step with every
	 * set_position, or the first drag computes its delta from a position
	 * the window never had and the window jumps. */
	t->x = 0;
	t->y = 0;
	/*
	 * Whether this window gets a frame is decided HERE, at map, and not at
	 * decoration time: app_id is only reliable once the window has mapped,
	 * and `csd_apps` matches on it. A client that explicitly asked for
	 * client-side mode has already set t->csd by now.
	 */
	if (!t->csd && kc_config_is_csd(s, t->xdg_toplevel->app_id))
		t->csd = true;
	place_new_window(s, t);
	kc_deco_create(t);
	/* At map, not at creation: app_id is set before the first commit but a
	 * client is free to set it later, and map is the first moment the value
	 * is the one a shell would actually match against. */
	kc_appid_observe(t->server, t->xdg_toplevel->app_id);
	/* At map, not at creation: before this the window has no title and no
	 * app_id, and a taskbar entry that appears blank then renames itself is
	 * the startup flicker every desktop has. */
	kc_shellsvc_add(t->server, t);
	kc_focus(t, t->xdg_toplevel->base->surface);
}

static void toplevel_unmap(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, unmap);
	struct kc_server *s = t->server;
	(void)data;
	/* Both of these outlive the window if they are not cleared here. A
	 * window can close mid-drag (a dialog that dismisses itself) and mid
	 * alt-tab (anything with a timeout), and the next pointer motion or
	 * modifier release would then walk a freed pointer. */
	if (s->grabbed == t)
		end_interactive(s);
	if (s->cycle_at == t) {
		s->cycle_at = NULL;
		s->cycling = false;
		kc_switcher_hide(s);
	}
	/* A window menu belongs to a window. Closing it here rather than
	 * letting it hold a freed pointer is the same rule as the two above,
	 * and the window it names can close itself at any moment. */
	if (s->menu_for == t)
		kc_menu_close(s);
	kc_shellsvc_remove(t);
	wl_list_remove(&t->link);
	/* Focus does not fall anywhere by itself: without this the seat keeps
	 * pointing at a surface that is gone and the keyboard goes dead. */
	if (s->seat->keyboard_state.focused_surface ==
	    t->xdg_toplevel->base->surface)
		focus_top_of_ws(s);
}

static void toplevel_commit(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, commit);
	(void)data;
	/* The first commit is the client asking what size it should be. Answer
	 * 0x0 — "you choose" — or it waits forever and never maps. */
	if (t->xdg_toplevel->base->initial_commit)
		wlr_xdg_toplevel_set_size(t->xdg_toplevel, 0, 0);

	/*
	 * THE FRAME RESIZES HERE, ON COMMIT, AND NOT IN process_resize().
	 *
	 * process_resize() calls wlr_xdg_toplevel_set_size(), and the size it
	 * asks for is not the size the window HAS until the client acknowledges
	 * it and commits. Repainting the strips from the motion handler makes
	 * the frame lead the window by a frame, which on screen looks like the
	 * border tearing away from the window while you drag it.
	 *
	 * kc_deco_arrange() compares against what it last drew, so calling it
	 * on every commit — sixty times a second from a video player — costs a
	 * few comparisons and no pixels.
	 */
	kc_deco_arrange(t);
}

static void toplevel_destroy(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, destroy);
	(void)data;
	/* Before the free: the capture source outlives this handler — it hangs
	 * off the scene node — and its destroy listener would land on freed
	 * memory. */
	kc_capture_toplevel_free(t);
	/* The frame's nodes hang off t->scene_tree, which xdg-shell is about to
	 * take away. Destroying them here rather than letting the tree do it is
	 * what keeps the two shadow rects and four strips from outliving the
	 * struct they point back at. */
	kc_deco_destroy(t);
	wl_list_remove(&t->map.link);
	wl_list_remove(&t->unmap.link);
	wl_list_remove(&t->commit.link);
	wl_list_remove(&t->destroy.link);
	wl_list_remove(&t->request_maximize.link);
	wl_list_remove(&t->request_fullscreen.link);
	wl_list_remove(&t->request_minimize.link);
	free(t);
}

static void new_xdg_toplevel(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	struct kc_toplevel *t = calloc(1, sizeof(*t));
	if (!t)
		return;
	t->node_type = KC_NODE_TOPLEVEL;
	t->server = s;
	t->xdg_toplevel = xdg_toplevel;
	/* Born on the workspace the user is looking at, and parented into that
	 * workspace's tree rather than into the scene root — which is what makes
	 * it disappear with the rest of them on a switch. */
	t->ws = s->cur_ws;
	/*
	 * An OUTER tree that is ours, with the client's subtree inside it.
	 *
	 * It used to be the client's subtree directly. The frame changes that:
	 * scene_tree now holds the shadow, the client and the four frame strips
	 * as siblings, which is what lets a shade disable the client's pixels
	 * with the titlebar still on screen — and, more importantly, means
	 * set_position moves the frame, raise_to_top raises it and a workspace
	 * switch hides it, with no second thing to keep in agreement.
	 */
	t->scene_tree = wlr_scene_tree_create(s->ws_tree[t->ws]);
	if (!t->scene_tree) {
		free(t);
		return;
	}
	t->surface_tree = wlr_scene_xdg_surface_create(t->scene_tree,
						       xdg_toplevel->base);
	if (!t->surface_tree) {
		wlr_scene_node_destroy(&t->scene_tree->node);
		free(t);
		return;
	}
	/* node.data is how a scene node is walked back to its toplevel when
	 * the pointer lands on it. On the OUTER tree, so the walk up from a
	 * surface node finds it — and explicitly NULL on the inner one, so a
	 * frame strip is never mistaken for a window. */
	t->scene_tree->node.data = t;
	t->surface_tree->node.data = NULL;
	/* Popups parent into the client's subtree, not into ours: a menu is
	 * positioned against the window's geometry, not against its frame. */
	xdg_toplevel->base->data = t->surface_tree;

	t->map.notify = toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &t->map);
	t->unmap.notify = toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &t->unmap);
	t->commit.notify = toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &t->commit);
	t->destroy.notify = toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &t->destroy);
	/* Without these three a video player asking for the screen was ignored
	 * and F11 did nothing anywhere — see window.c. */
	t->request_maximize.notify = kc_window_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		      &t->request_maximize);
	t->request_fullscreen.notify = kc_window_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		      &t->request_fullscreen);
	t->request_minimize.notify = kc_window_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize,
		      &t->request_minimize);
}

/*
 * Server-side decorations.
 *
 * KDOS draws window frames as CHARACTER CELLS, in the same palette and the
 * same glyph tier as the panel and the installer — a double-line box with the
 * title in the accent. That is only possible for clients that let the
 * compositor decorate them, so every toplevel is asked for SERVER_SIDE.
 *
 * The answer is not ours to force. GTK clients insist on client-side
 * decorations and will keep drawing their own headerbar whatever this says;
 * fighting them produces a window with two titlebars, which is worse than one
 * that does not match. Offer, accept the reply, do not argue — the same rule
 * kdos-appbox follows for the box's own theming.
 *
 * Nothing is painted yet: this milestone establishes WHO decorates. The cell
 * grid arrives with libkwl in M3, because the frames are drawn with the same
 * toolkit as everything else rather than with a second ad-hoc renderer.
 *
 * THE ANSWER IS DEFERRED TO THE SURFACE'S FIRST COMMIT, AND THAT IS NOT A
 * REFINEMENT — it is the whole difference between a desktop and a compositor
 * that aborts. A client creates its decoration object BEFORE it first commits
 * the xdg_surface, so at `new_toplevel_decoration` time the surface is not yet
 * `initialized`; `set_mode()` schedules a configure, and since wlroots 0.18
 * `wlr_xdg_surface_schedule_configure()` asserts on exactly that:
 *
 *     Assertion failed: surface->initialized
 *       (types/xdg_shell/wlr_xdg_surface.c: wlr_xdg_surface_schedule_configure)
 *
 * Answering at once killed kdos-comp the instant its FIRST client appeared —
 * foot, launched by kdos-desktop — and took the session with it. Every client
 * that speaks xdg-decoration did it, which is most of them. Found on the first
 * QEMU boot of an ISO that had the desktop in it; no headless test reached it,
 * because none of them used xdg-decoration.
 */
static void deco_commit(struct wl_listener *l, void *data)
{
	struct kc_decoration *d = wl_container_of(l, d, commit);
	(void)data;
	if (d->deco->toplevel->base->initial_commit)
		wlr_xdg_toplevel_decoration_v1_set_mode(d->deco,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

/*
 * The client answering "I will decorate myself".
 *
 * This is the ONLY thing that makes a window unframed by its own request —
 * silence does not. The wlroots convention reads "no decoration object means
 * the client decorates itself", and following it to the letter gives an SDL
 * game and a bare Qt dialog no titlebar and no close box at all, which is worse
 * than the double titlebar it was avoiding. So silence is framed, and the
 * handful that draw their own bar without ever saying so are named in
 * comp.conf's `csd_apps` — see toplevel_map().
 */
static void deco_mode(struct wl_listener *l, void *data)
{
	struct kc_decoration *d = wl_container_of(l, d, mode);
	(void)data;

	struct wlr_scene_tree *tree = d->deco->toplevel->base->data;
	if (!tree || !tree->node.parent)
		return;
	/* base->data is the client's inner subtree; the toplevel is tagged on
	 * the outer tree that holds it. */
	struct kc_toplevel *t = tree->node.parent->node.data;
	if (!t || t->node_type != KC_NODE_TOPLEVEL)
		return;

	bool csd = d->deco->requested_mode ==
		   WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	if (csd == t->csd)
		return;
	t->csd = csd;
	if (csd)
		kc_deco_destroy(t);
	else
		kc_deco_create(t);
}

static void deco_destroy(struct wl_listener *l, void *data)
{
	struct kc_decoration *d = wl_container_of(l, d, destroy);
	(void)data;
	wl_list_remove(&d->commit.link);
	wl_list_remove(&d->mode.link);
	wl_list_remove(&d->destroy.link);
	free(d);
}

static void new_decoration(struct wl_listener *l, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	(void)l;
	struct kc_decoration *d = calloc(1, sizeof(*d));
	if (!d)
		return;
	d->deco = deco;
	d->commit.notify = deco_commit;
	wl_signal_add(&deco->toplevel->base->surface->events.commit, &d->commit);
	d->mode.notify = deco_mode;
	wl_signal_add(&deco->events.request_mode, &d->mode);
	d->destroy.notify = deco_destroy;
	wl_signal_add(&deco->events.destroy, &d->destroy);
}

static void new_xdg_popup(struct wl_listener *l, void *data)
{
	struct wlr_xdg_popup *popup = data;
	(void)l;
	/* A popup is positioned relative to its parent, so it only needs to be
	 * put in the scene under the parent's tree; wlroots does the rest. */
	struct wlr_xdg_surface *parent =
		wlr_xdg_surface_try_from_wlr_surface(popup->parent);
	if (!parent || !parent->data)
		return;
	struct wlr_scene_tree *ptree = parent->data;
	popup->base->data = wlr_scene_xdg_surface_create(ptree, popup->base);
}

/* ── main ──────────────────────────────────────────────────────────────── */

/* SIGHUP is `kdos theme <accent>` telling the session the accent changed. It is
 * NOT a config reload: comp.conf is read once, and a binding that changed under
 * a running session would be a surprise rather than a feature. */
static int handle_hup_signal(int sig, void *data)
{
	struct kc_server *s = data;
	(void)sig;
	kc_crt_reload(s);
	return 0;
}

static int handle_quit_signal(int sig, void *data)
{
	struct wl_display *display = data;
	wlr_log(WLR_INFO, "signal %d — ending the session", sig);
	wl_display_terminate(display);
	return 0;
}

int main(int argc, char **argv)
{
	/* INFO by default; DEBUG when asked. The frame-timing lines and the
	 * idle policy's decisions are DEBUG, and "why did my screen do that"
	 * should not need a rebuild to answer. */
	wlr_log_init(getenv("KDOS_COMP_DEBUG") ? WLR_DEBUG : WLR_INFO, NULL);

	bool spawn_startup = true;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--no-startup"))
			spawn_startup = false;
		else if (!strcmp(argv[i], "--version")) {
			printf("kdos-comp (wlroots %s)\n", WLR_VERSION_STR);
			return 0;
		} else {
			fprintf(stderr, "usage: kdos-comp [--no-startup]\n");
			return 2;
		}
	}

	/* A dead client must not take the compositor with it. */
	signal(SIGPIPE, SIG_IGN);

	struct kc_server s = {0};
	/* Before anything that reads a setting — new_keyboard wants the repeat
	 * rate, and a keyboard can arrive the instant the backend starts. */
	kc_config_load(&s);
	kc_appid_init(&s);

	s.display = wl_display_create();
	/*
	 * SIGTERM and SIGINT end the session the same way Super+Escape does,
	 * through the event loop rather than from a signal handler — wayland's
	 * add_signal delivers them as ordinary events, so the teardown that
	 * follows wl_display_run() is the SAME teardown either way. Without this a
	 * `kill` left the compositor to die on the default action, which is also
	 * the only path the CRT pass's swapchains and the renderer's textures are
	 * released on.
	 */
	wl_event_loop_add_signal(wl_display_get_event_loop(s.display), SIGTERM,
				 handle_quit_signal, s.display);
	wl_event_loop_add_signal(wl_display_get_event_loop(s.display), SIGINT,
				 handle_quit_signal, s.display);
	wl_event_loop_add_signal(wl_display_get_event_loop(s.display), SIGHUP,
				 handle_hup_signal, &s);
	s.backend = wlr_backend_autocreate(wl_display_get_event_loop(s.display),
					   &s.session);
	if (!s.backend) {
		wlr_log(WLR_ERROR, "no backend — no DRM master and no "
				   "WAYLAND_DISPLAY to nest in");
		return 1;
	}

	s.renderer = wlr_renderer_autocreate(s.backend);
	if (!s.renderer) {
		wlr_log(WLR_ERROR, "no renderer");
		return 1;
	}
	wlr_renderer_init_wl_display(s.renderer, s.display);

	s.allocator = wlr_allocator_autocreate(s.backend, s.renderer);
	if (!s.allocator) {
		wlr_log(WLR_ERROR, "no allocator");
		return 1;
	}

	/* Before any global a boxed client must not reach. The filter applies to
	 * globals registered either side of this call, so the ordering is for
	 * the reader, not for correctness. */
	kc_security_init(&s);

	struct wlr_compositor *compositor =
		wlr_compositor_create(s.display, 5, s.renderer);
	wlr_subcompositor_create(s.display);
	wlr_data_device_manager_create(s.display);

	/* The renderer exists, so this can decide whether there is going to be a
	 * CRT pass at all and compile the shader once, instead of finding out per
	 * frame. It only ever turns the pass OFF, so it is safe this early. */
	kc_crt_init(&s);
	/* Allocates the handle and nothing else; the atlas and the programs are
	 * built the first time Super+A is pressed. */
	kc_ascii_init(&s);

	/*
	 * The font for the window frames.
	 *
	 * Failing to load one turns the frames OFF and leaves a working session
	 * with undecorated windows — exactly how this compositor behaved before
	 * they existed. A desktop with no titlebars is a limitation; a desktop
	 * that will not start because fontconfig could not find Terminus is an
	 * unbootable machine.
	 */
	if (s.deco_frames) {
		if (kcell_font_load(s.deco_font) != 0) {
			wlr_log(WLR_ERROR,
				"deco: cannot load `%s` — window frames are off",
				s.deco_font ? s.deco_font : "(default)");
			s.deco_frames = false;
		} else {
			kc_deco_glyphs_init();
			kc_deco_check_glyphs();
			wlr_log(WLR_INFO, "deco: %s, cell %dx%d",
				s.deco_font, kcell_w(), kcell_h());
		}
	}

	s.output_layout = wlr_output_layout_create(s.display);
	s.scene = wlr_scene_create();
	s.scene_layout = wlr_scene_attach_output_layout(s.scene, s.output_layout);
	/* After the scene layout exists, because attaching an output needs both.
	 * The backend has not been started yet, so no output can arrive early. */
	kc_output_init(&s);

	/*
	 * Z-order is creation order, so these three groups are built in exactly
	 * the order the layer-shell protocol defines: background and bottom
	 * below every window, the workspaces in the middle, top and overlay
	 * above. Create them in any other order and a panel is behind the
	 * windows it is meant to sit over.
	 */
	s.layer_below = wlr_scene_tree_create(&s.scene->tree);

	/* The wallpaper is decoded HERE and not earlier: its scene nodes are
	 * parented into layer_below, and kc_output_init above only registers a
	 * listener — no output exists until the backend starts, which is further
	 * down. Decoding after that point would show a black background on the
	 * first output until something moved it. */
	kc_wallpaper_init(&s);

	/* One tree per workspace, all but the first disabled. Everything a
	 * window kind has to do to gain workspaces is be parented into one of
	 * these. */
	for (int i = 0; i < KC_WORKSPACES; i++) {
		s.ws_tree[i] = wlr_scene_tree_create(&s.scene->tree);
		wlr_scene_node_set_enabled(&s.ws_tree[i]->node, i == 0);
	}

	s.layer_above = wlr_scene_tree_create(&s.scene->tree);
	kc_layer_init(&s);

	/*
	 * Above everything, including layer-shell's overlay: a panel that could
	 * draw over the lock screen would be a panel that could show you the
	 * window titles of a locked session. The idle dim lives in here too,
	 * below the lock surfaces.
	 */
	s.layer_lock = wlr_scene_tree_create(&s.scene->tree);
	kc_lock_init(&s);
	kc_shellsvc_init(&s);
	/* After shellsvc: per-window capture names its window through
	 * ext-foreign-toplevel-list, which shellsvc creates. */
	kc_capture_init(&s);

	wl_list_init(&s.toplevels);
	s.xdg_shell = wlr_xdg_shell_create(s.display, 3);
	s.new_xdg_toplevel.notify = new_xdg_toplevel;
	wl_signal_add(&s.xdg_shell->events.new_toplevel, &s.new_xdg_toplevel);
	s.new_xdg_popup.notify = new_xdg_popup;
	wl_signal_add(&s.xdg_shell->events.new_popup, &s.new_xdg_popup);

	s.deco_mgr = wlr_xdg_decoration_manager_v1_create(s.display);
	s.new_decoration.notify = new_decoration;
	wl_signal_add(&s.deco_mgr->events.new_toplevel_decoration,
		      &s.new_decoration);

	s.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(s.cursor, s.output_layout);
	/* NULL theme = whatever XCURSOR_THEME says, which 10-wayland.sh sets
	 * to KDOS-cursors. */
	s.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	s.cursor_motion.notify = cursor_motion;
	wl_signal_add(&s.cursor->events.motion, &s.cursor_motion);
	s.cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&s.cursor->events.motion_absolute, &s.cursor_motion_absolute);
	s.cursor_button.notify = cursor_button;
	wl_signal_add(&s.cursor->events.button, &s.cursor_button);
	s.cursor_axis.notify = cursor_axis;
	wl_signal_add(&s.cursor->events.axis, &s.cursor_axis);
	s.cursor_frame.notify = cursor_frame;
	wl_signal_add(&s.cursor->events.frame, &s.cursor_frame);

	wl_list_init(&s.keyboards);
	s.new_input.notify = new_input;
	wl_signal_add(&s.backend->events.new_input, &s.new_input);
	s.seat = wlr_seat_create(s.display, "seat0");
	s.request_cursor.notify = request_cursor;
	wl_signal_add(&s.seat->events.request_set_cursor, &s.request_cursor);
	s.request_set_selection.notify = request_set_selection;
	wl_signal_add(&s.seat->events.request_set_selection, &s.request_set_selection);
	/*
	 * PRIMARY SELECTION — middle-click paste.
	 *
	 * Without this global, selecting text in one window and middle-clicking
	 * in another does NOTHING, anywhere on this machine. It is not a
	 * preference and it is not a nicety: it is the oldest interaction on
	 * Unix, every terminal and every text field implements it, and a
	 * compositor that does not create the manager silently breaks all of
	 * them at once. foot names it on startup as missing.
	 */
	wlr_primary_selection_v1_device_manager_create(s.display);
	s.request_set_primary.notify = request_set_primary;
	wl_signal_add(&s.seat->events.request_set_primary_selection,
		      &s.request_set_primary);

	/*
	 * CURSOR SHAPE — one pointer image for the whole desktop.
	 *
	 * Without it every client loads its OWN cursor theme at its own size,
	 * so the pointer visibly changes shape and size as it crosses between
	 * windows. With it, a client names a shape and the compositor draws it
	 * from the one theme — which is also what makes the resize cursors on
	 * the window frames match the ones inside applications.
	 */
	s.cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(s.display, 1);
	if (s.cursor_shape_mgr) {
		s.request_cursor_shape.notify = request_cursor_shape;
		wl_signal_add(&s.cursor_shape_mgr->events.request_set_shape,
			      &s.request_cursor_shape);
	}

	/*
	 * ACTIVATION — "raise my other window".
	 *
	 * A single-instance application asked to open a second document raises
	 * the window it already has through this. Without it the click appears
	 * to do nothing, which is indistinguishable from the application being
	 * hung — and every alien app in the box is single-instance, because
	 * that is what the shared session bus makes them.
	 */
	s.activation = wlr_xdg_activation_v1_create(s.display);
	if (s.activation) {
		s.request_activate.notify = request_activate;
		wl_signal_add(&s.activation->events.request_activate,
			      &s.request_activate);
	}

	/* After the seat exists: xwayland_ready hands the seat over, and a
	 * lazy server can become ready as soon as it is created. */
	kc_xwayland_init(&s, compositor);

	/* After the seat: the relay listens to the seat's own focus_change, and
	 * a text input is bound per seat. */
	kc_im_init(&s);

	/* After the seat, which the idle notifier reports activity against. */
	kc_idle_init(&s);
	/* After the outputs exist and before the session does anything: the
	 * socket has to be there when kdos-shell starts, or the panel's first
	 * connection attempt is the one that fails. */
	kc_frames_init(&s);

	const char *socket = wl_display_add_socket_auto(s.display);
	if (!socket) {
		wlr_backend_destroy(s.backend);
		return 1;
	}
	if (!wlr_backend_start(s.backend)) {
		wlr_backend_destroy(s.backend);
		wl_display_destroy(s.display);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	wlr_log(WLR_INFO, "kdos-comp running on WAYLAND_DISPLAY=%s", socket);

	/*
	 * The shell, and whatever else comp.conf lists. After the socket exists
	 * and WAYLAND_DISPLAY is exported, or the first thing it does is fail to
	 * connect to the compositor that just started it.
	 */
	if (spawn_startup) {
		wl_event_loop_add_signal(wl_display_get_event_loop(s.display),
					 SIGCHLD, handle_sigchld, &s);
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		for (int i = 0; i < s.nstartup; i++) {
			s.startup_since[i] = now.tv_sec;
			spawn_startup_one(&s, i);
		}
	}

	wl_display_run(s.display);

	kc_wallpaper_free(&s);
	kc_appid_free(&s);
	kc_im_free(&s);
	kc_security_free(&s);
	kc_idle_free(&s);
	kc_config_free(&s);

	/*
	 * Every listener that lives as long as the session, taken off before the
	 * thing it listens to is destroyed. wlroots ASSERTS on a non-empty listener
	 * list in several of these destructors — the security-context manager and
	 * wlr_cursor were the two that aborted here — so this is not tidiness, it is
	 * the difference between a clean logout and a core dump. Each is guarded the
	 * same way its `wl_signal_add` was: a global that failed to create was never
	 * listened to.
	 */
	if (s.layer_shell)
		wl_list_remove(&s.new_layer_surface.link);
	if (s.lock_mgr)
		wl_list_remove(&s.new_lock.link);
	if (s.ws_mgr)
		wl_list_remove(&s.ws_commit.link);
	kc_capture_free(&s);
	if (s.output_mgr) {
		wl_list_remove(&s.output_mgr_apply.link);
		wl_list_remove(&s.output_mgr_test.link);
	}
	wl_list_remove(&s.layout_change.link);
	wl_list_remove(&s.new_xdg_toplevel.link);
	wl_list_remove(&s.new_xdg_popup.link);
	if (s.deco_mgr)
		wl_list_remove(&s.new_decoration.link);
	wl_list_remove(&s.request_cursor.link);
	wl_list_remove(&s.request_set_selection.link);
	/* Same rule as every other listener here: wlroots ASSERTS that nothing
	 * is still listening when it destroys an object, so a global that was
	 * created must have its listener removed, and one that failed to be
	 * created was never listened to. */
	kc_cellui_free(&s);
	kc_ascii_free(&s);
	wl_list_remove(&s.request_set_primary.link);
	if (s.cursor_shape_mgr)
		wl_list_remove(&s.request_cursor_shape.link);
	if (s.activation)
		wl_list_remove(&s.request_activate.link);
	if (s.xwayland) {
		wl_list_remove(&s.new_xwayland_surface.link);
		wl_list_remove(&s.xwayland_ready.link);
	}

	if (s.xwayland)
		wlr_xwayland_destroy(s.xwayland);
	wl_display_destroy_clients(s.display);
	wlr_scene_node_destroy(&s.scene->tree.node);
	/* Before the renderer: the CRT pass's swapchains and imported textures
	 * belong to it, and the backend — which is what destroys the outputs, and
	 * so would otherwise run kc_crt_output_free — is torn down after it. */
	struct kc_output *out;
	wl_list_for_each(out, &s.outputs, link)
		kc_crt_output_free(out);
	kc_crt_free(&s);
	kc_frames_free(&s);
	wlr_xcursor_manager_destroy(s.cursor_mgr);
	/*
	 * wlr_cursor_destroy() ASSERTS that nothing is still listening to it, and
	 * these five listeners are ours. Leaving them attached ended the session
	 * with
	 *
	 *   wlr_cursor_destroy: Assertion `wl_list_empty(&cur->events.motion...)'
	 *
	 * which nobody had seen because nothing reached this code: `quit` was the
	 * only way here until SIGTERM became the other one.
	 */
	wl_list_remove(&s.cursor_motion.link);
	wl_list_remove(&s.cursor_motion_absolute.link);
	wl_list_remove(&s.cursor_button.link);
	wl_list_remove(&s.cursor_axis.link);
	wl_list_remove(&s.cursor_frame.link);
	wlr_cursor_destroy(s.cursor);
	/* And the backend asserts the same thing about its own two signals. */
	wl_list_remove(&s.new_input.link);
	wl_list_remove(&s.new_output.link);
	wlr_allocator_destroy(s.allocator);
	wlr_renderer_destroy(s.renderer);
	wlr_backend_destroy(s.backend);
	wl_display_destroy(s.display);
	return 0;
}
