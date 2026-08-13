/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   pointer.c — capturing the pointer, and two output knobs
 *
 * WHAT WAS BROKEN. kdos-comp created neither pointer-constraints nor
 * relative-pointer, and without that pair NO application can take the mouse:
 * a first-person game, an emulator, a CAD orbit, a VNC or RDP client, anything
 * in wine. The cursor simply walks out of the window mid-aim. The appbox ships
 * games, emulators and wine, so this was the largest functional hole in the
 * compositor — and it is invisible until you try, because nothing errors.
 *
 * The two protocols are one feature and neither is useful alone:
 *
 *   pointer-constraints  the client says "lock the pointer here" (it stops
 *                        moving, and the cursor is hidden) or "confine it to
 *                        this region" (it moves, but cannot leave)
 *   relative-pointer     the client is told how far the mouse MOVED, as a
 *                        delta, with the unaccelerated delta beside it —
 *                        which is the only thing a locked pointer can send,
 *                        since its absolute position never changes
 *
 * THREE RULES, each of which is how this goes wrong:
 *
 *   - A CONSTRAINT ONLY APPLIES TO THE FOCUSED SURFACE. Honouring one for an
 *     unfocused client lets a background window hold the mouse hostage.
 *   - THE ESCAPE HATCH IS NOT OPTIONAL. A locked pointer with no way out is a
 *     session you reboot, so Super breaks any constraint — the same modifier
 *     that already owns window management.
 *   - NOTHING SURVIVES A LOCK SCREEN. kc_locked() is asked before a constraint
 *     is ever applied; a game holding the pointer across a lock would be a game
 *     reading the mouse while the machine is supposed to be secured.
 * ---------------------------------
 */

#include <stdlib.h>

#include "kdos-comp.h"

/* ── constraints ───────────────────────────────────────────────────────── */

static void constraint_destroy(struct wl_listener *l, void *data)
{
	struct kc_pointer_constraint *c = wl_container_of(l, c, destroy);
	struct kc_server *s = c->server;
	(void)data;

	if (s->active_constraint == c->constraint) {
		s->active_constraint = NULL;
		/* The cursor was parked while the pointer was locked. Put it
		 * back where the surface said it should reappear, or the mouse
		 * teleports to wherever it was when the lock began. */
		s->constraint_committed = false;
	}
	wl_list_remove(&c->destroy.link);
	free(c);
}

static void new_constraint(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_constraint);
	struct wlr_pointer_constraint_v1 *wlr_constraint = data;

	struct kc_pointer_constraint *c = calloc(1, sizeof(*c));
	if (!c)
		return;
	c->server = s;
	c->constraint = wlr_constraint;
	c->destroy.notify = constraint_destroy;
	wl_signal_add(&wlr_constraint->events.destroy, &c->destroy);

	/* It may already be the focused surface — a client that locks the
	 * pointer the moment it maps is the ordinary case for a game. */
	kc_pointer_check_constraint(s);
}

/*
 * Decide which constraint, if any, is live right now.
 *
 * Called on every focus change and whenever a constraint appears or goes away,
 * because "the focused surface" is the only thing that decides it.
 */
void kc_pointer_check_constraint(struct kc_server *s)
{
	if (!s->pointer_constraints)
		return;

	struct wlr_surface *focused = s->seat->keyboard_state.focused_surface;
	struct wlr_pointer_constraint_v1 *want = NULL;

	if (focused && !kc_locked(s) && !s->constraint_broken) {
		struct wlr_pointer_constraint_v1 *c;
		wl_list_for_each(c, &s->pointer_constraints->constraints, link)
			if (c->surface == focused) {
				want = c;
				break;
			}
	}

	if (want == s->active_constraint)
		return;

	if (s->active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(s->active_constraint);
	s->active_constraint = want;
	s->constraint_committed = false;
	if (want)
		wlr_pointer_constraint_v1_send_activated(want);
}

/*
 * Break whatever constraint is live and refuse to reapply it until the surface
 * loses and regains focus. Super is the escape hatch.
 */
void kc_pointer_break_constraint(struct kc_server *s)
{
	if (!s->active_constraint)
		return;
	s->constraint_broken = true;
	kc_pointer_check_constraint(s);
}

void kc_pointer_unbreak_constraint(struct kc_server *s)
{
	if (!s->constraint_broken)
		return;
	s->constraint_broken = false;
	kc_pointer_check_constraint(s);
}

/*
 * Apply the constraint to a motion that is about to happen.
 *
 * Returns true when the cursor must NOT be moved — a lock. For a confine, the
 * delta is clipped so the cursor stays inside the region and the caller moves
 * it as usual. The relative motion is sent either way, because that is the only
 * thing a locked client ever receives.
 */
bool kc_pointer_constrain(struct kc_server *s, double *dx, double *dy)
{
	struct wlr_pointer_constraint_v1 *c = s->active_constraint;
	if (!c)
		return false;

	if (c->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
		return true;

	/*
	 * Confined: clamp the destination into the region the client gave, in
	 * SURFACE coordinates. Without the surface's position on screen there is
	 * nothing to clamp against, so a confine we cannot place is a confine we
	 * do not enforce — rather than one we enforce in the wrong place.
	 */
	struct wlr_box box;
	if (!pixman_region32_not_empty(&c->region))
		return false;
	pixman_region32_t *r = &c->region;
	pixman_box32_t *ext = pixman_region32_extents(r);
	box.x = ext->x1;
	box.y = ext->y1;
	box.width = ext->x2 - ext->x1;
	box.height = ext->y2 - ext->y1;

	double sx, sy;
	if (!kc_surface_position(s, c->surface, &sx, &sy))
		return false;

	double nx = s->cursor->x + *dx, ny = s->cursor->y + *dy;
	double lo_x = sx + box.x, hi_x = sx + box.x + box.width - 1;
	double lo_y = sy + box.y, hi_y = sy + box.y + box.height - 1;

	if (nx < lo_x)
		*dx = lo_x - s->cursor->x;
	else if (nx > hi_x)
		*dx = hi_x - s->cursor->x;
	if (ny < lo_y)
		*dy = lo_y - s->cursor->y;
	else if (ny > hi_y)
		*dy = hi_y - s->cursor->y;
	return false;
}

void kc_pointer_relative(struct kc_server *s, uint64_t time_usec,
			 double dx, double dy, double dx_unaccel,
			 double dy_unaccel)
{
	if (s->relative_pointer)
		wlr_relative_pointer_manager_v1_send_relative_motion(
			s->relative_pointer, s->seat, time_usec, dx, dy,
			dx_unaccel, dy_unaccel);
}

/* Where a toplevel's surface sits in layout coordinates. Only toplevels: a
 * confine on a layer surface has no meaning here, and answering false makes
 * the caller not enforce it rather than enforce it in the wrong place. */
bool kc_surface_position(struct kc_server *s, struct wlr_surface *surface,
			 double *lx, double *ly)
{
	struct kc_toplevel *t;
	wl_list_for_each(t, &s->toplevels, link)
		if (t->xdg_toplevel->base->surface == surface) {
			*lx = t->x;
			*ly = t->y;
			return true;
		}
	return false;
}

/* ── gestures ──────────────────────────────────────────────────────────── */

/*
 * Touchpad pinch, swipe and hold, forwarded verbatim.
 *
 * The compositor has no gesture bindings of its own, so there is nothing to
 * intercept and every event goes to the client — which is what makes
 * pinch-to-zoom work in a document viewer. If bindings are ever added they go
 * in front of these, the same way keyboard bindings sit in front of the seat.
 */
static void gesture_swipe_begin(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, swipe_begin);
	struct wlr_pointer_swipe_begin_event *e = data;
	wlr_pointer_gestures_v1_send_swipe_begin(s->gestures, s->seat,
						 e->time_msec, e->fingers);
}

static void gesture_swipe_update(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, swipe_update);
	struct wlr_pointer_swipe_update_event *e = data;
	wlr_pointer_gestures_v1_send_swipe_update(s->gestures, s->seat,
						  e->time_msec, e->dx, e->dy);
}

static void gesture_swipe_end(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, swipe_end);
	struct wlr_pointer_swipe_end_event *e = data;
	wlr_pointer_gestures_v1_send_swipe_end(s->gestures, s->seat,
					       e->time_msec, e->cancelled);
}

static void gesture_pinch_begin(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, pinch_begin);
	struct wlr_pointer_pinch_begin_event *e = data;
	wlr_pointer_gestures_v1_send_pinch_begin(s->gestures, s->seat,
						 e->time_msec, e->fingers);
}

static void gesture_pinch_update(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, pinch_update);
	struct wlr_pointer_pinch_update_event *e = data;
	wlr_pointer_gestures_v1_send_pinch_update(s->gestures, s->seat,
						  e->time_msec, e->dx, e->dy,
						  e->scale, e->rotation);
}

static void gesture_pinch_end(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, pinch_end);
	struct wlr_pointer_pinch_end_event *e = data;
	wlr_pointer_gestures_v1_send_pinch_end(s->gestures, s->seat,
					       e->time_msec, e->cancelled);
}

static void gesture_hold_begin(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, hold_begin);
	struct wlr_pointer_hold_begin_event *e = data;
	wlr_pointer_gestures_v1_send_hold_begin(s->gestures, s->seat,
						e->time_msec, e->fingers);
}

static void gesture_hold_end(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, hold_end);
	struct wlr_pointer_hold_end_event *e = data;
	wlr_pointer_gestures_v1_send_hold_end(s->gestures, s->seat,
					      e->time_msec, e->cancelled);
}

/* ── gamma and output power ────────────────────────────────────────────── */

/*
 * wlr-gamma-control: what wlsunset and gammastep use to warm the screen at
 * dusk. The manager does the protocol; this applies the ramp to the output and
 * tells the client when the hardware refused, which is the half a compositor
 * cannot skip — a silent failure here looks like the tool being broken.
 */
static void set_gamma(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, set_gamma);
	const struct wlr_gamma_control_manager_v1_set_gamma_event *ev = data;
	(void)s;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	if (!wlr_gamma_control_v1_apply(ev->control, &state)) {
		wlr_output_state_finish(&state);
		return;
	}
	if (!wlr_output_commit_state(ev->output, &state)) {
		/* The client is told rather than left believing it worked. */
		wlr_gamma_control_v1_send_failed_and_destroy(ev->control);
	}
	wlr_output_state_finish(&state);
}

/*
 * wlr-output-power-management: an external tool blanking one monitor. The idle
 * policy already does this for itself; this is the same switch, exposed.
 */
static void set_output_power(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, set_output_power);
	const struct wlr_output_power_v1_set_mode_event *ev = data;
	(void)s;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state,
		ev->mode == ZWLR_OUTPUT_POWER_V1_MODE_ON);
	wlr_output_commit_state(ev->output, &state);
	wlr_output_state_finish(&state);
}

/* ── setup ─────────────────────────────────────────────────────────────── */

void kc_pointer_init(struct kc_server *s)
{
	s->pointer_constraints = wlr_pointer_constraints_v1_create(s->display);
	if (s->pointer_constraints) {
		s->new_constraint.notify = new_constraint;
		wl_signal_add(&s->pointer_constraints->events.new_constraint,
			      &s->new_constraint);
	}
	s->relative_pointer = wlr_relative_pointer_manager_v1_create(s->display);
	s->gestures = wlr_pointer_gestures_v1_create(s->display);

	if (s->gestures) {
		struct {
			struct wl_listener *l;
			struct wl_signal *sig;
			wl_notify_func_t fn;
		} g[] = {
			{ &s->swipe_begin, &s->cursor->events.swipe_begin, gesture_swipe_begin },
			{ &s->swipe_update, &s->cursor->events.swipe_update, gesture_swipe_update },
			{ &s->swipe_end, &s->cursor->events.swipe_end, gesture_swipe_end },
			{ &s->pinch_begin, &s->cursor->events.pinch_begin, gesture_pinch_begin },
			{ &s->pinch_update, &s->cursor->events.pinch_update, gesture_pinch_update },
			{ &s->pinch_end, &s->cursor->events.pinch_end, gesture_pinch_end },
			{ &s->hold_begin, &s->cursor->events.hold_begin, gesture_hold_begin },
			{ &s->hold_end, &s->cursor->events.hold_end, gesture_hold_end },
		};
		for (size_t i = 0; i < sizeof(g) / sizeof(g[0]); i++) {
			g[i].l->notify = g[i].fn;
			wl_signal_add(g[i].sig, g[i].l);
		}
	}

	s->gamma_control = wlr_gamma_control_manager_v1_create(s->display);
	if (s->gamma_control) {
		s->set_gamma.notify = set_gamma;
		wl_signal_add(&s->gamma_control->events.set_gamma, &s->set_gamma);
	}
	s->output_power = wlr_output_power_manager_v1_create(s->display);
	if (s->output_power) {
		s->set_output_power.notify = set_output_power;
		wl_signal_add(&s->output_power->events.set_mode,
			      &s->set_output_power);
	}
}

void kc_pointer_free(struct kc_server *s)
{
	/* Same rule as every other listener here: wlroots asserts that nothing
	 * is still listening when it destroys an object, and a global that
	 * failed to be created was never listened to. */
	if (s->pointer_constraints)
		wl_list_remove(&s->new_constraint.link);
	if (s->gestures) {
		wl_list_remove(&s->swipe_begin.link);
		wl_list_remove(&s->swipe_update.link);
		wl_list_remove(&s->swipe_end.link);
		wl_list_remove(&s->pinch_begin.link);
		wl_list_remove(&s->pinch_update.link);
		wl_list_remove(&s->pinch_end.link);
		wl_list_remove(&s->hold_begin.link);
		wl_list_remove(&s->hold_end.link);
	}
	if (s->gamma_control)
		wl_list_remove(&s->set_gamma.link);
	if (s->output_power)
		wl_list_remove(&s->set_output_power.link);
}
