// SPDX-License-Identifier: GPL-2.0-only
/*
 * Click-away for on-demand layer surfaces — the second half of a rule
 * labwc only implements one half of.
 *
 * labwc's press path FOCUSES a layer surface that asks for keyboard
 * interactivity when you press on it. Nothing gives that focus up when you
 * press somewhere else, because upstream's on-demand clients (labnag,
 * lxqt-runner) are modal things you answer rather than dismiss.
 *
 * This desktop is built the other way round: the menus, the launcher, the
 * run box, the file chooser and the settings window are all on-demand layer
 * surfaces, and every one of them closes when it loses the keyboard
 * (libkwl's `dismiss_on_unfocus`). Pressing a VIEW already did that for
 * free — focusing the view moves the keyboard, the overlay gets
 * wl_keyboard.leave and exits, which is why launching an application closed
 * the menu it was launched from. Pressing the DESKTOP did not: no view is
 * involved, no focus changes, and the menu sat there over a desktop the
 * user had visibly clicked away to. Same for a press on the panel, which
 * takes no keyboard of its own.
 *
 * So: any press that is not on the focused on-demand layer surface releases
 * it. Deliberately narrow — it does nothing at all unless such a surface
 * currently holds the keyboard, so an ordinary click on the desktop with no
 * menu open behaves exactly as it did.
 *
 * EXCLUSIVE layers are left alone on purpose. Exclusive means "this surface
 * keeps the keyboard until it goes away" and a lock screen or a panel that
 * asked for it means it; dismissing one on a stray click is the bug this
 * file is fixing, pointed the other way.
 */

#include <string.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "kdos.h"
#include "labwc.h"
#include "layers.h"
#include "node.h"
#include "output.h"
#include "session-lock.h"

void
kdos_layer_release_on_demand(struct seat *seat,
		struct wlr_layer_surface_v1 *pressed)
{
	if (!seat || !seat->focused_layer) {
		return;
	}
	if (seat->focused_layer->current.keyboard_interactive
			!= ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
		return;
	}
	if (pressed == seat->focused_layer) {
		return;
	}

	/*
	 * NULL hands the keyboard back to the topmost view, or clears it when
	 * there is none — either way the overlay sees a leave, which is the
	 * event it is waiting for.
	 */
	seat_set_focus_layer(seat, NULL);
}

/*
 * The saver goes on the first key, button or touch, and that event is spent
 * on it. kdos-saver takes no keyboard and claims no pointer region, so
 * without this the press would land on whatever window it hides — a click
 * the user cannot see the target of. Closing the layer surface sends
 * `closed`, on which libkwl ends kdos-saver's loop.
 *
 * A LOCKED SESSION STILL CLOSES IT BUT KEEPS THE EVENT. The lock screen is
 * above the saver and holds the keyboard, so the key belongs to the password
 * field; spending it here would drop the password's first character.
 */
bool
kdos_saver_dismiss(void)
{
	bool any = false;
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		struct wlr_scene_tree *tree =
			output->layer_tree[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY];
		struct wlr_scene_node *node, *tmp;
		wl_list_for_each_safe(node, tmp, &tree->children, link) {
			struct lab_layer_surface *layer =
				node_layer_surface_from_node(node);
			struct wlr_layer_surface_v1 *ls =
				layer->scene_layer_surface->layer_surface;
			if (!ls->surface->mapped || !ls->namespace
					|| strcmp(ls->namespace, "kdos-saver")) {
				continue;
			}
			wlr_layer_surface_v1_destroy(ls);
			any = true;
		}
	}
	return any && !server.session_lock_manager->locked;
}
