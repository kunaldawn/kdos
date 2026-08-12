/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — ext-session-lock-v1
 *
 * THE WHOLE REASON THIS PROTOCOL EXISTS is the failure mode of every lock
 * screen that came before it: a screensaver that is just a fullscreen window
 * unlocks the machine when it crashes. Here the COMPOSITOR owns the locked
 * state. `kdos-lock` asks to lock, and if it then segfaults, `s->locked` stays
 * true, the screen stays blank, and no client below ever sees another key.
 *
 * Which is why `locked` is a field and not `s->lock != NULL`. A boolean derived
 * from the client's liveness is a lock you defeat with SIGKILL.
 *
 * Three things the protocol requires and one it does not:
 *
 *   - An output with no lock surface must show NOTHING of the session. That is
 *     the black rect under the whole lock tree: a client that covers one screen
 *     of three leaves the other two blank rather than showing your mail.
 *   - `locked` is only sent once EVERY output is covered. Sending it early is
 *     telling the client the screen is safe while a window is still visible on
 *     the second monitor.
 *   - The lock tree is created above every other layer, including
 *     layer-shell's overlay, so a panel cannot draw over the lock screen.
 *   - Not required, and done anyway: the compositor refuses a SECOND lock
 *     while one is live. wlroots would let two clients hold locks at once; the
 *     unlock from either would then have to be reasoned about.
 */

#include <stdlib.h>

#include "kdos-comp.h"

bool kc_locked(const struct kc_server *s)
{
	return s->locked;
}

/* ── the blank floor ───────────────────────────────────────────────────── */

/*
 * One rect the size of the whole output layout, at the bottom of the lock
 * tree. It is what an uncovered output shows, and it is also what remains if
 * the lock client dies — so it is created when the lock starts and destroyed
 * only when the session is genuinely unlocked.
 */
static void blank_ensure(struct kc_server *s)
{
	struct wlr_box box = {0};
	wlr_output_layout_get_box(s->output_layout, NULL, &box);
	/* A layout with no outputs has a zero box; a zero-size rect is not
	 * worth creating and would have to be resized the moment one arrives
	 * anyway. */
	if (box.width <= 0 || box.height <= 0)
		return;

	const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	if (!s->lock_blank) {
		s->lock_blank = wlr_scene_rect_create(s->layer_lock, box.width,
						     box.height, black);
		if (!s->lock_blank)
			return;
	} else {
		wlr_scene_rect_set_size(s->lock_blank, box.width, box.height);
	}
	wlr_scene_node_set_position(&s->lock_blank->node, box.x, box.y);
	wlr_scene_node_lower_to_bottom(&s->lock_blank->node);
}

static void blank_drop(struct kc_server *s)
{
	if (!s->lock_blank)
		return;
	wlr_scene_node_destroy(&s->lock_blank->node);
	s->lock_blank = NULL;
}

/* ── surfaces ──────────────────────────────────────────────────────────── */

/* Every lock surface is exactly its output's size and sits at its output's
 * position in the layout. It has no other geometry to negotiate: it is not a
 * window and cannot be moved. */
static void surface_place(struct kc_lock_surface *ls)
{
	struct wlr_output *out = ls->surface->output;
	if (!out)
		return;

	struct wlr_box box = {0};
	wlr_output_layout_get_box(ls->parent->server->output_layout, out, &box);
	wlr_scene_node_set_position(&ls->scene_tree->node, box.x, box.y);
	wlr_session_lock_surface_v1_configure(ls->surface, (uint32_t)box.width,
					      (uint32_t)box.height);
}

void kc_lock_arrange(struct kc_server *s)
{
	/* Gated on `locked`, not on `lock`: an ABANDONED lock has no client and
	 * no surfaces, and it is exactly then that the blank rect has to keep
	 * covering an output that was just plugged in. */
	if (!s->locked)
		return;
	if (s->lock) {
		struct kc_lock_surface *ls;
		wl_list_for_each(ls, &s->lock->surfaces, link)
			surface_place(ls);
	}
	blank_ensure(s);
}

/* Count the outputs that are actually usable. An output that has been switched
 * off — DPMS, or unplugged mid-lock — cannot receive a lock surface, so
 * counting it would leave `locked` unsent forever. */
static int outputs_live(struct kc_server *s)
{
	int n = 0;
	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link)
		if (o->wlr_output->enabled)
			n++;
	return n;
}

/*
 * Send `locked` once every live output has a surface that is actually SHOWING
 * something.
 *
 * Two traps, both found by locking a two-output session, which aborted the
 * compositor and took the client with it:
 *
 *   - It may be sent EXACTLY ONCE. wlroots asserts `!locked_sent`, so a second
 *     call is not a redundant message, it is `Assertion failed` and a dead
 *     compositor with a lock client that dies alongside it.
 *   - The count must be of MAPPED surfaces, not created ones. A client creates
 *     all of its lock surfaces up front, so counting them fires on the first
 *     map — telling the client the screen is covered while the second monitor
 *     is still showing the session.
 */
static void maybe_send_locked(struct kc_lock *lk)
{
	if (lk->locked_sent)
		return;

	int mapped = 0;
	struct kc_lock_surface *ls;
	wl_list_for_each(ls, &lk->surfaces, link)
		if (ls->surface->surface->mapped)
			mapped++;

	if (mapped < outputs_live(lk->server))
		return;
	lk->locked_sent = true;
	wlr_session_lock_v1_send_locked(lk->lock);
}

static void lock_surface_map(struct wl_listener *l, void *data)
{
	struct kc_lock_surface *ls = wl_container_of(l, ls, map);
	(void)data;
	/* Focus follows the first surface that is actually showing something:
	 * a keyboard handed to a surface with no content is a password typed
	 * into nothing. */
	kc_lock_focus(ls->parent->server);
	maybe_send_locked(ls->parent);
}

static void lock_surface_destroy(struct wl_listener *l, void *data)
{
	struct kc_lock_surface *ls = wl_container_of(l, ls, destroy);
	(void)data;

	wl_list_remove(&ls->map.link);
	wl_list_remove(&ls->destroy.link);
	wl_list_remove(&ls->output_destroy.link);
	wl_list_remove(&ls->link);
	ls->parent->nsurfaces--;
	/* The scene tree belongs to the surface's own scene helper and is
	 * destroyed with it; dropping it here would be a double free. */
	free(ls);
}

/* An output that goes away takes its lock surface with it, and the remaining
 * surfaces still cover what is left — so the session stays locked and the
 * count simply falls. */
static void lock_surface_output_destroy(struct wl_listener *l, void *data)
{
	struct kc_lock_surface *ls = wl_container_of(l, ls, output_destroy);
	(void)data;
	wlr_scene_node_set_enabled(&ls->scene_tree->node, false);
}

static void lock_new_surface(struct wl_listener *l, void *data)
{
	struct kc_lock *lk = wl_container_of(l, lk, new_surface);
	struct wlr_session_lock_surface_v1 *surf = data;

	struct kc_lock_surface *ls = calloc(1, sizeof(*ls));
	if (!ls)
		return;
	ls->parent = lk;
	ls->surface = surf;
	ls->scene_tree = wlr_scene_subsurface_tree_create(lk->server->layer_lock,
							 surf->surface);
	if (!ls->scene_tree) {
		free(ls);
		return;
	}

	ls->map.notify = lock_surface_map;
	wl_signal_add(&surf->surface->events.map, &ls->map);
	ls->destroy.notify = lock_surface_destroy;
	wl_signal_add(&surf->events.destroy, &ls->destroy);
	ls->output_destroy.notify = lock_surface_output_destroy;
	wl_signal_add(&surf->output->events.destroy, &ls->output_destroy);

	wl_list_insert(&lk->surfaces, &ls->link);
	lk->nsurfaces++;
	surface_place(ls);
}

/* ── focus ─────────────────────────────────────────────────────────────── */

void kc_lock_focus(struct kc_server *s)
{
	if (!s->locked)
		return;

	struct wlr_surface *target = NULL;
	if (s->lock) {
		struct kc_lock_surface *ls;
		wl_list_for_each(ls, &s->lock->surfaces, link)
			if (ls->surface->surface->mapped) {
				target = ls->surface->surface;
				break;
			}
	}

	if (!target) {
		/*
		 * Nothing to type into — either no surface yet, or the client
		 * is gone. Clear the focus rather than leaving it where it was:
		 * the keyboard would otherwise still be pointed at the window
		 * that was focused when the lock started.
		 */
		wlr_seat_keyboard_notify_clear_focus(s->seat);
		return;
	}

	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		wlr_seat_keyboard_notify_enter(s->seat, target, kb->keycodes,
					       kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(s->seat, target, NULL, 0, NULL);
}

/* ── lock lifetime ─────────────────────────────────────────────────────── */

static void lock_free(struct kc_lock *lk)
{
	wl_list_remove(&lk->new_surface.link);
	wl_list_remove(&lk->unlock.link);
	wl_list_remove(&lk->destroy.link);
	lk->server->lock = NULL;
	free(lk);
}

static void lock_unlock(struct wl_listener *l, void *data)
{
	struct kc_lock *lk = wl_container_of(l, lk, unlock);
	struct kc_server *s = lk->server;
	(void)data;

	s->locked = false;
	blank_drop(s);
	wlr_log(WLR_INFO, "session unlocked");
	/*
	 * The client destroys its own lock object after unlocking, which is
	 * what tears the surfaces down; all this has to do is give the keyboard
	 * back to the most recent window on this workspace.
	 */
	kc_refocus(s);
}

/*
 * The client went away. If it never sent `unlock`, the session STAYS LOCKED —
 * that is the entire guarantee of this protocol, and the black rect is what
 * enforces it. Recovery is a login on another VT, exactly as it would be for a
 * wedged session.
 */
static void lock_destroy(struct wl_listener *l, void *data)
{
	struct kc_lock *lk = wl_container_of(l, lk, destroy);
	struct kc_server *s = lk->server;
	(void)data;

	lock_free(lk);
	if (s->locked) {
		wlr_log(WLR_ERROR, "the lock client died while locked — the "
				   "session stays locked (switch VT to recover)");
		blank_ensure(s);
		kc_lock_focus(s);
	}
}

static void new_lock(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_lock);
	struct wlr_session_lock_v1 *lock = data;

	if (s->lock) {
		/* One lock at a time. Two live locks would mean an unlock from
		 * either had to be reasoned about, and the second client is
		 * always the one with less claim. */
		wlr_log(WLR_ERROR, "refusing a second session lock");
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	struct kc_lock *lk = calloc(1, sizeof(*lk));
	if (!lk) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}
	lk->server = s;
	lk->lock = lock;
	wl_list_init(&lk->surfaces);

	lk->new_surface.notify = lock_new_surface;
	wl_signal_add(&lock->events.new_surface, &lk->new_surface);
	lk->unlock.notify = lock_unlock;
	wl_signal_add(&lock->events.unlock, &lk->unlock);
	lk->destroy.notify = lock_destroy;
	wl_signal_add(&lock->events.destroy, &lk->destroy);

	s->lock = lk;
	s->locked = true;
	blank_ensure(s);
	/*
	 * Drop the keyboard NOW, before any surface exists. Between the lock
	 * request and the first lock surface there is a window in which the
	 * previously focused client is still the keyboard focus — and that
	 * window is exactly where the first characters of a password would go.
	 */
	wlr_seat_keyboard_notify_clear_focus(s->seat);
	/* And end any interactive move/resize: the pointer belongs to the lock
	 * screen now, and a grab that outlived it would resize a window the
	 * user cannot see. */
	s->cursor_mode = KC_CURSOR_PASSTHROUGH;
	s->grabbed = NULL;
	wlr_log(WLR_INFO, "session locked");
}

void kc_lock_init(struct kc_server *s)
{
	s->lock_mgr = wlr_session_lock_manager_v1_create(s->display);
	if (!s->lock_mgr) {
		wlr_log(WLR_ERROR, "no session-lock global — the screen cannot "
				   "be locked");
		return;
	}
	s->new_lock.notify = new_lock;
	wl_signal_add(&s->lock_mgr->events.new_lock, &s->new_lock);
}
