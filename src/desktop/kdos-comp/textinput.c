/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   text-input-v3 + input-method-v2 — the relay
 *
 * An input method is three parties that never speak to each other: the
 * APPLICATION says "I am a text field, here is my cursor and what surrounds
 * it" over text-input-v3, the INPUT METHOD says "here is a preedit, here is a
 * string to commit" over input-method-v2, and the COMPOSITOR is the only thing
 * that knows which application has the keyboard. Neither protocol can reach
 * the other; the compositor is the wire between them, and until this file
 * existed kdos-comp advertised neither global, so no input method could work
 * at all — the same shape as the capture globals in capture.c.
 *
 * Six rules, and every one of them is a way this goes wrong:
 *
 *   - ONE input method per seat. The protocol says a second `get_input_method`
 *     gets `unavailable`, and this is not a formality: two input methods both
 *     believing they own the preedit produce interleaved garbage in the text
 *     field, and the second one is as likely to be a mistake (a stale fcitx5)
 *     as a second user.
 *   - A text input hears `enter` only if ITS CLIENT owns the focused surface.
 *     text-input-v3 is bound per seat, not per surface, so without that check
 *     every text field in every application would be told it had focus.
 *   - The keyboard GRAB is what makes typing work, and it is the dangerous
 *     part: while an input method holds it, every keystroke goes to the input
 *     method instead of the application. That is why the manager is a
 *     privileged global — see the security-context table, where it sits beside
 *     virtual-keyboard.
 *   - Compositor bindings are matched BEFORE the grab. Super+Q must quit the
 *     session while a candidate window is open; an input method that could
 *     swallow the compositor's own keys could trap you in it.
 *   - The grab does not survive a lock. `kc_locked()` is asked on every path
 *     here for the same reason it is asked in main.c: a lock screen whose
 *     keystrokes reach a boxed input method is a lock screen that leaks the
 *     password.
 *   - The popup follows the CURSOR RECTANGLE, in surface coordinates, so it
 *     has to be re-placed when the window moves as well as when the rectangle
 *     changes. A candidate list parked over the text it is completing is the
 *     complaint every IME on Wayland has had at some point.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kdos-comp.h"

/*
 * All three structs live here rather than in the header: nothing outside this
 * file has any business reaching into the relay, and `struct kc_im *im` in
 * kc_server is an incomplete type everywhere else.
 */
struct kc_text_input {
	struct wl_list link;
	struct kc_im *relay;
	struct wlr_text_input_v3 *ti;
	struct wl_listener enable;
	struct wl_listener commit;
	struct wl_listener disable;
	struct wl_listener destroy;
};

struct kc_im_popup {
	struct wl_list link;
	struct kc_im *relay;
	struct wlr_input_popup_surface_v2 *popup;
	struct wlr_scene_tree *tree;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct kc_im {
	struct kc_server *s;
	struct wlr_text_input_manager_v3 *ti_mgr;
	struct wlr_input_method_manager_v2 *im_mgr;
	struct wl_listener new_text_input;
	struct wl_listener new_input_method;
	struct wl_listener focus_change;
	struct wl_list text_inputs;		/* struct kc_text_input */

	/* At most one, by protocol. */
	struct wlr_input_method_v2 *im;
	struct wl_listener im_commit;
	struct wl_listener im_new_popup;
	struct wl_listener im_grab_keyboard;
	struct wl_listener im_destroy;
	struct wl_list popups;			/* struct kc_im_popup */

	struct wlr_input_method_keyboard_grab_v2 *grab;
	struct wl_listener grab_destroy;

	/*
	 * virtual-keyboard-v1 lives here rather than with the real keyboards
	 * because it exists FOR this: an input method takes the grab, decides a
	 * key is not for it, and forwards it through a virtual keyboard. Without
	 * the global those keys are simply lost — the arrow keys and Escape in
	 * every fcitx5 candidate window.
	 *
	 * It is also raw input injection, which is why the security-context
	 * table denies it to a box by default.
	 */
	struct wlr_virtual_keyboard_manager_v1 *vkbd_mgr;
	struct wl_listener new_vkbd;
};

/* ── helpers ───────────────────────────────────────────────────────────── */

static struct wl_client *surface_client(struct wlr_surface *s)
{
	return s ? wl_resource_get_client(s->resource) : NULL;
}

/*
 * The text input that should be talking to the input method right now: focused,
 * and enabled by its own client. There can be several text inputs on the seat
 * (one per client, and a client may make more than one) and at most one of them
 * is in that state.
 */
static struct kc_text_input *active_text_input(struct kc_im *im)
{
	struct kc_text_input *ti;
	wl_list_for_each(ti, &im->text_inputs, link)
		if (ti->ti->current_enabled && ti->ti->focused_surface)
			return ti;
	return NULL;
}

/* ── the popup ─────────────────────────────────────────────────────────── */

/*
 * Place the candidate window under the text cursor.
 *
 * The rectangle the application sends is in ITS surface's coordinates, so the
 * popup's position is the focused window's scene position plus that rectangle.
 * When the application never sent one (the feature is optional) the whole
 * surface is the fallback, which puts the popup at the window's top-left rather
 * than at a guessed centre — wrong in a way that is obvious rather than subtle.
 */
static void popup_place(struct kc_im_popup *p)
{
	struct kc_im *im = p->relay;
	struct kc_text_input *ti = active_text_input(im);
	if (!ti || !p->tree)
		return;

	struct wlr_surface *focus = ti->ti->focused_surface;
	if (!focus)
		return;

	/* Where the focused surface is on the desktop. Only toplevels carry a
	 * position we own; anything else (a layer surface with a text field)
	 * anchors at the output origin, which is honest and never off-screen. */
	int sx = 0, sy = 0;
	struct kc_toplevel *t;
	wl_list_for_each(t, &im->s->toplevels, link) {
		if (t->xdg_toplevel->base->surface == focus) {
			sx = t->x;
			sy = t->y;
			break;
		}
	}

	struct wlr_box cur = ti->ti->current.cursor_rectangle;
	if (!(ti->ti->active_features & WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE)) {
		cur.x = cur.y = 0;
		cur.width = cur.height = 0;
	}

	int px = sx + cur.x;
	int py = sy + cur.y + cur.height;

	/*
	 * Keep it on the output it lands on. A candidate list that runs off the
	 * bottom of the screen is a candidate list you cannot read, and the
	 * protocol's own answer — text_input_rectangle — only tells the input
	 * method where the text is, not where we put its window.
	 */
	struct wlr_output *o = wlr_output_layout_output_at(im->s->output_layout,
							   px, py);
	if (o) {
		struct wlr_box ob;
		wlr_output_layout_get_box(im->s->output_layout, o, &ob);
		int w = p->popup->surface->current.width;
		int h = p->popup->surface->current.height;
		if (px + w > ob.x + ob.width)
			px = ob.x + ob.width - w;
		if (py + h > ob.y + ob.height)
			py = sy + cur.y - h;	/* flip above the cursor */
		if (px < ob.x)
			px = ob.x;
		if (py < ob.y)
			py = ob.y;
	}

	wlr_scene_node_set_position(&p->tree->node, px, py);

	struct wlr_box rel = { .x = cur.x, .y = cur.y,
			       .width = cur.width, .height = cur.height };
	wlr_input_popup_surface_v2_send_text_input_rectangle(p->popup, &rel);
}

static void popup_commit(struct wl_listener *l, void *data)
{
	struct kc_im_popup *p = wl_container_of(l, p, commit);
	(void)data;
	popup_place(p);
}

static void popup_destroy(struct wl_listener *l, void *data)
{
	struct kc_im_popup *p = wl_container_of(l, p, destroy);
	(void)data;
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	wl_list_remove(&p->link);
	if (p->tree)
		wlr_scene_node_destroy(&p->tree->node);
	free(p);
}

static void im_new_popup(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, im_new_popup);
	struct wlr_input_popup_surface_v2 *ps = data;

	struct kc_im_popup *p = calloc(1, sizeof(*p));
	if (!p)
		return;
	p->relay = im;
	p->popup = ps;

	/*
	 * Above the windows and below the lock tree. A candidate window belongs
	 * on top of the application it is completing for; it does not belong on
	 * top of a lock screen, and putting it in layer_above rather than in the
	 * workspace tree also means it is not hidden by a workspace switch that
	 * happens while it is open.
	 */
	p->tree = wlr_scene_subsurface_tree_create(im->s->layer_above, ps->surface);
	if (!p->tree) {
		free(p);
		return;
	}

	p->commit.notify = popup_commit;
	wl_signal_add(&ps->surface->events.commit, &p->commit);
	p->destroy.notify = popup_destroy;
	wl_signal_add(&ps->events.destroy, &p->destroy);
	wl_list_insert(&im->popups, &p->link);
	popup_place(p);
}

/* Every popup, after the focused window moved or the rectangle changed. */
static void popups_place(struct kc_im *im)
{
	struct kc_im_popup *p;
	wl_list_for_each(p, &im->popups, link)
		popup_place(p);
}

/* ── text input -> input method ────────────────────────────────────────── */

/*
 * Hand the application's state to the input method.
 *
 * Sent in the protocol's order and always terminated with `done`: input methods
 * treat the surrounding_text/content_type pair as valid only once done arrives,
 * and one missing done leaves fcitx5 composing against the previous field's
 * text — which looks like the IME "remembering" what you typed somewhere else.
 */
static void send_state_to_im(struct kc_im *im, struct kc_text_input *ti)
{
	if (!im->im)
		return;

	if (ti->ti->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
		wlr_input_method_v2_send_surrounding_text(im->im,
			ti->ti->current.surrounding.text,
			ti->ti->current.surrounding.cursor,
			ti->ti->current.surrounding.anchor);

	wlr_input_method_v2_send_text_change_cause(im->im,
		ti->ti->current.text_change_cause);

	if (ti->ti->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
		wlr_input_method_v2_send_content_type(im->im,
			ti->ti->current.content_type.hint,
			ti->ti->current.content_type.purpose);

	wlr_input_method_v2_send_done(im->im);
}

static void ti_enable(struct wl_listener *l, void *data)
{
	struct kc_text_input *ti = wl_container_of(l, ti, enable);
	struct kc_im *im = ti->relay;
	(void)data;

	if (!im->im || kc_locked(im->s))
		return;
	wlr_input_method_v2_send_activate(im->im);
	send_state_to_im(im, ti);
	popups_place(im);
}

static void ti_commit(struct wl_listener *l, void *data)
{
	struct kc_text_input *ti = wl_container_of(l, ti, commit);
	struct kc_im *im = ti->relay;
	(void)data;

	if (!im->im || kc_locked(im->s))
		return;
	if (!ti->ti->current_enabled)
		return;
	send_state_to_im(im, ti);
	popups_place(im);
}

static void ti_disable(struct wl_listener *l, void *data)
{
	struct kc_text_input *ti = wl_container_of(l, ti, disable);
	struct kc_im *im = ti->relay;
	(void)data;

	if (!im->im)
		return;
	wlr_input_method_v2_send_deactivate(im->im);
	wlr_input_method_v2_send_done(im->im);
}

static void ti_destroy(struct wl_listener *l, void *data)
{
	struct kc_text_input *ti = wl_container_of(l, ti, destroy);
	struct kc_im *im = ti->relay;
	(void)data;

	/* A text input that goes away while enabled has to deactivate the input
	 * method on its way out, or the IME stays composing against a field
	 * whose client has gone. */
	if (ti->ti->current_enabled && im->im) {
		wlr_input_method_v2_send_deactivate(im->im);
		wlr_input_method_v2_send_done(im->im);
	}
	wl_list_remove(&ti->enable.link);
	wl_list_remove(&ti->commit.link);
	wl_list_remove(&ti->disable.link);
	wl_list_remove(&ti->destroy.link);
	wl_list_remove(&ti->link);
	free(ti);
}

static void new_text_input(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, new_text_input);
	struct wlr_text_input_v3 *wti = data;

	if (wti->seat != im->s->seat)
		return;

	struct kc_text_input *ti = calloc(1, sizeof(*ti));
	if (!ti)
		return;
	ti->relay = im;
	ti->ti = wti;

	ti->enable.notify = ti_enable;
	wl_signal_add(&wti->events.enable, &ti->enable);
	ti->commit.notify = ti_commit;
	wl_signal_add(&wti->events.commit, &ti->commit);
	ti->disable.notify = ti_disable;
	wl_signal_add(&wti->events.disable, &ti->disable);
	ti->destroy.notify = ti_destroy;
	wl_signal_add(&wti->events.destroy, &ti->destroy);
	wl_list_insert(&im->text_inputs, &ti->link);

	/* It may have been created while its own surface already had focus —
	 * a client that binds text-input lazily, on the first click into a
	 * field, is the common case rather than the exotic one. */
	struct wlr_surface *focus =
		im->s->seat->keyboard_state.focused_surface;
	if (im->im && focus &&
	    surface_client(focus) == wl_resource_get_client(wti->resource))
		wlr_text_input_v3_send_enter(wti, focus);
}

/* ── input method -> text input ────────────────────────────────────────── */

static void im_commit(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, im_commit);
	(void)data;

	struct kc_text_input *ti = active_text_input(im);
	if (!ti || kc_locked(im->s))
		return;

	struct wlr_input_method_v2_state *st = &im->im->current;

	/*
	 * Order matters and is the protocol's: delete first, then commit, then
	 * preedit, then done. Committing before deleting would leave the text
	 * being replaced sitting in front of its replacement — which is what a
	 * broken CJK conversion looks like, one character at a time.
	 */
	if (st->delete.before_length || st->delete.after_length)
		wlr_text_input_v3_send_delete_surrounding_text(ti->ti,
			st->delete.before_length, st->delete.after_length);
	if (st->commit_text)
		wlr_text_input_v3_send_commit_string(ti->ti, st->commit_text);
	if (st->preedit.text)
		wlr_text_input_v3_send_preedit_string(ti->ti, st->preedit.text,
			st->preedit.cursor_begin, st->preedit.cursor_end);
	wlr_text_input_v3_send_done(ti->ti);
}

static void grab_destroy(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, grab_destroy);
	(void)data;
	wl_list_remove(&im->grab_destroy.link);
	im->grab = NULL;
}

static void im_grab_keyboard(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, im_grab_keyboard);
	struct wlr_input_method_keyboard_grab_v2 *grab = data;

	/* The grab has to be told which keyboard it is grabbing, or it sends
	 * the client no keymap and every key it forwards is uninterpretable. */
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(im->s->seat);
	if (kb)
		wlr_input_method_keyboard_grab_v2_set_keyboard(grab, kb);

	im->grab = grab;
	im->grab_destroy.notify = grab_destroy;
	wl_signal_add(&grab->events.destroy, &im->grab_destroy);
}

static void im_destroy(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, im_destroy);
	(void)data;

	wl_list_remove(&im->im_commit.link);
	wl_list_remove(&im->im_new_popup.link);
	wl_list_remove(&im->im_grab_keyboard.link);
	wl_list_remove(&im->im_destroy.link);
	im->im = NULL;
	im->grab = NULL;

	/*
	 * The input method died — probably crashed. Every text input that
	 * thought it had one has to be told its preedit is gone, or the last
	 * half-composed string stays on screen in a field nothing is driving
	 * any more.
	 */
	struct kc_text_input *ti;
	wl_list_for_each(ti, &im->text_inputs, link) {
		if (!ti->ti->current_enabled)
			continue;
		wlr_text_input_v3_send_preedit_string(ti->ti, NULL, 0, 0);
		wlr_text_input_v3_send_done(ti->ti);
	}
}

static void new_input_method(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, new_input_method);
	struct wlr_input_method_v2 *wim = data;

	if (wim->seat != im->s->seat)
		return;

	/* One per seat. The second is told so rather than left waiting — an
	 * input method that binds and then hears nothing looks like a
	 * compositor with no IM support at all, which is the wrong diagnosis
	 * and sends people to rebuild things that were fine. */
	if (im->im) {
		wlr_log(WLR_INFO, "a second input method asked for the seat — "
				  "sending unavailable");
		wlr_input_method_v2_send_unavailable(wim);
		return;
	}

	im->im = wim;
	im->im_commit.notify = im_commit;
	wl_signal_add(&wim->events.commit, &im->im_commit);
	im->im_new_popup.notify = im_new_popup;
	wl_signal_add(&wim->events.new_popup_surface, &im->im_new_popup);
	im->im_grab_keyboard.notify = im_grab_keyboard;
	wl_signal_add(&wim->events.grab_keyboard, &im->im_grab_keyboard);
	im->im_destroy.notify = im_destroy;
	wl_signal_add(&wim->events.destroy, &im->im_destroy);

	/* An input method that starts AFTER the field is focused — the ordinary
	 * case, since the shell launches it and the user clicks later — must be
	 * activated for whatever is already enabled. */
	struct kc_text_input *ti = active_text_input(im);
	if (ti && !kc_locked(im->s)) {
		wlr_input_method_v2_send_activate(wim);
		send_state_to_im(im, ti);
	}
}

/* ── focus ─────────────────────────────────────────────────────────────── */

/*
 * One listener on the seat rather than a call in every focus path. The seat
 * emits this for `notify_enter` AND for `clear_focus`, which is the half that
 * is easy to forget: a workspace switch away from a text field leaves the field
 * with no focus at all, and a text input still holding `enter` there would keep
 * feeding the input method a window the user cannot see.
 */
static void focus_change(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, focus_change);
	struct wlr_seat_keyboard_focus_change_event *ev = data;

	struct kc_text_input *ti;
	wl_list_for_each(ti, &im->text_inputs, link) {
		struct wl_client *c = wl_resource_get_client(ti->ti->resource);

		if (ti->ti->focused_surface &&
		    surface_client(ev->old_surface) == c) {
			if (ti->ti->current_enabled && im->im) {
				wlr_input_method_v2_send_deactivate(im->im);
				wlr_input_method_v2_send_done(im->im);
			}
			wlr_text_input_v3_send_leave(ti->ti);
		}

		if (ev->new_surface && surface_client(ev->new_surface) == c &&
		    !kc_locked(im->s))
			wlr_text_input_v3_send_enter(ti->ti, ev->new_surface);
	}
}

/* ── the virtual keyboard ──────────────────────────────────────────────── */

static void new_vkbd(struct wl_listener *l, void *data)
{
	struct kc_im *im = wl_container_of(l, im, new_vkbd);
	struct wlr_virtual_keyboard_v1 *vk = data;
	kc_keyboard_add(im->s, &vk->keyboard.base, true);
}

/* ── the public surface ────────────────────────────────────────────────── */

void kc_im_init(struct kc_server *s)
{
	struct kc_im *im = calloc(1, sizeof(*im));
	if (!im)
		return;
	im->s = s;
	wl_list_init(&im->text_inputs);
	wl_list_init(&im->popups);

	im->ti_mgr = wlr_text_input_manager_v3_create(s->display);
	im->im_mgr = wlr_input_method_manager_v2_create(s->display);
	if (!im->ti_mgr || !im->im_mgr) {
		wlr_log(WLR_ERROR, "input method globals unavailable — no IME");
		free(im);
		return;
	}

	im->new_text_input.notify = new_text_input;
	wl_signal_add(&im->ti_mgr->events.new_text_input, &im->new_text_input);
	im->new_input_method.notify = new_input_method;
	wl_signal_add(&im->im_mgr->events.new_input_method, &im->new_input_method);
	im->focus_change.notify = focus_change;
	wl_signal_add(&s->seat->keyboard_state.events.focus_change,
		      &im->focus_change);

	im->vkbd_mgr = wlr_virtual_keyboard_manager_v1_create(s->display);
	if (im->vkbd_mgr) {
		im->new_vkbd.notify = new_vkbd;
		wl_signal_add(&im->vkbd_mgr->events.new_virtual_keyboard,
			      &im->new_vkbd);
	}

	s->im = im;
}

bool kc_im_key(struct kc_server *s, struct wlr_keyboard *kb,
	       struct wlr_keyboard_key_event *ev)
{
	struct kc_im *im = s->im;
	if (!im || !im->grab || kc_locked(s))
		return false;
	wlr_input_method_keyboard_grab_v2_set_keyboard(im->grab, kb);
	wlr_input_method_keyboard_grab_v2_send_key(im->grab, ev->time_msec,
						   ev->keycode, ev->state);
	return true;
}

bool kc_im_modifiers(struct kc_server *s, struct wlr_keyboard *kb)
{
	struct kc_im *im = s->im;
	if (!im || !im->grab || kc_locked(s))
		return false;
	wlr_input_method_keyboard_grab_v2_set_keyboard(im->grab, kb);
	wlr_input_method_keyboard_grab_v2_send_modifiers(im->grab,
							 &kb->modifiers);
	return true;
}

void kc_im_moved(struct kc_server *s)
{
	if (s->im)
		popups_place(s->im);
}

void kc_im_free(struct kc_server *s)
{
	struct kc_im *im = s->im;
	if (!im)
		return;
	wl_list_remove(&im->new_text_input.link);
	wl_list_remove(&im->new_input_method.link);
	wl_list_remove(&im->focus_change.link);
	if (im->vkbd_mgr)
		wl_list_remove(&im->new_vkbd.link);
	if (im->im) {
		wl_list_remove(&im->im_commit.link);
		wl_list_remove(&im->im_new_popup.link);
		wl_list_remove(&im->im_grab_keyboard.link);
		wl_list_remove(&im->im_destroy.link);
	}
	if (im->grab)
		wl_list_remove(&im->grab_destroy.link);
	free(im);
	s->im = NULL;
}
