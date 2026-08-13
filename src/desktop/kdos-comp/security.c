/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   security-context-v1 — the box a client came from decides what it may bind
 *
 * The sandbox profile and the compositor policy are THE SAME RECORD.
 * `~/.config/kdos/boxes/<name>.conf` already maps 1:1 onto distrobox flags at
 * container-create time; the same file's `wayland.*` keys are read here and
 * enforced at protocol-bind time. One file, two enforcement points, nothing to
 * keep in sync and no manifest for an application author to write about itself.
 *
 * How a client gets an identity: kdos-appbox binds a Wayland socket per box and
 * hands it to the compositor through wp_security_context_manager_v1, tagged
 * `io.kdos.appbox` / the box name / the container id. Every client that then
 * connects on that socket is tagged by the compositor itself. The client cannot
 * choose, forge or drop its own tag — it never sees it.
 *
 * That last property is the whole point, and it is why the identity is the
 * socket and not the process. KWin threw out sha256-of-/proc/pid/exe in 2023
 * (MR !4630) precisely because a sandboxed app has its own mount namespace and
 * can therefore lie about its executable path. That argument applies verbatim
 * to podman, and Hyprland's permission engine — the only shipping per-app one —
 * still keys on /proc/pid/exe today.
 *
 * WHAT THIS IS NOT. KDOS boxes share $HOME. A truthful capability display reads
 * "filesystem: entire home", and that is unconfined. This is not a hardening
 * story; it is the first desktop whose permission display is derived from
 * enforced state rather than from a file the application author wrote about
 * itself. Anything the compositor cannot measure must be reported as UNKNOWN,
 * never as denied — see kdos-comp's title bar, which reads this table.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-comp.h"

#define KC_SANDBOX_ENGINE "io.kdos.appbox"

enum kc_cap {
	KC_CAP_SCREENCOPY = 0,
	KC_CAP_CLIPBOARD,
	KC_CAP_LAYER_SHELL,
	KC_CAP_VIRTUAL_INPUT,
	KC_CAP_FOREIGN_TOPLEVEL,
	KC_CAP_SHORTCUTS_INHIBIT,
	KC_CAP_OUTPUT_MANAGEMENT,
	KC_CAP_INPUT_METHOD,
	KC_CAP_COUNT
};

static const char *const cap_names[KC_CAP_COUNT] = {
	[KC_CAP_SCREENCOPY]        = "screencopy",
	[KC_CAP_CLIPBOARD]         = "clipboard",
	[KC_CAP_LAYER_SHELL]       = "layer_shell",
	[KC_CAP_VIRTUAL_INPUT]     = "virtual_input",
	[KC_CAP_FOREIGN_TOPLEVEL]  = "foreign_toplevel",
	[KC_CAP_SHORTCUTS_INHIBIT] = "shortcuts_inhibit",
	[KC_CAP_OUTPUT_MANAGEMENT] = "output_management",
	[KC_CAP_INPUT_METHOD]      = "input_method",
};

/*
 * Interface name -> capability.
 *
 * Written out by NAME rather than against the wlroots types on purpose: most of
 * these globals do not exist in kdos-comp yet (layer-shell and
 * foreign-toplevel arrive with the shell in M3, screencopy with kdos-shot).
 * The table being ahead of the compositor is the safe direction — a capability
 * that is denied before its protocol exists cannot be forgotten on the day it
 * lands, whereas a table written to match today's globals silently grants
 * every one added afterwards.
 *
 * Both the wlr_ and the ext_ spelling of each is listed. They are different
 * protocols doing the same thing, and denying only one of a pair is the same as
 * denying neither.
 */
static const struct {
	const char *iface;
	enum kc_cap cap;
} iface_caps[] = {
	{ "zwlr_screencopy_manager_v1",                KC_CAP_SCREENCOPY },
	{ "zwlr_export_dmabuf_manager_v1",             KC_CAP_SCREENCOPY },
	{ "ext_image_copy_capture_manager_v1",         KC_CAP_SCREENCOPY },
	{ "ext_output_image_capture_source_manager_v1", KC_CAP_SCREENCOPY },
	{ "ext_foreign_toplevel_image_capture_source_manager_v1", KC_CAP_SCREENCOPY },

	{ "zwlr_data_control_manager_v1",              KC_CAP_CLIPBOARD },
	{ "ext_data_control_manager_v1",               KC_CAP_CLIPBOARD },

	{ "zwlr_layer_shell_v1",                       KC_CAP_LAYER_SHELL },

	{ "zwlr_virtual_pointer_manager_v1",           KC_CAP_VIRTUAL_INPUT },
	{ "zwp_virtual_keyboard_manager_v1",           KC_CAP_VIRTUAL_INPUT },

	{ "zwlr_foreign_toplevel_manager_v1",          KC_CAP_FOREIGN_TOPLEVEL },
	{ "ext_foreign_toplevel_list_v1",              KC_CAP_FOREIGN_TOPLEVEL },

	{ "zwp_keyboard_shortcuts_inhibit_manager_v1", KC_CAP_SHORTCUTS_INHIBIT },

	{ "zwlr_output_manager_v1",                    KC_CAP_OUTPUT_MANAGEMENT },
	{ "zwlr_output_power_manager_v1",              KC_CAP_OUTPUT_MANAGEMENT },

	/*
	 * BEING an input method is a keylogger by design: the keyboard grab
	 * delivers every keystroke on the seat to whoever holds it, including
	 * the ones typed into other applications. So the manager sits here,
	 * beside virtual-keyboard, and is denied to a box by default.
	 *
	 * zwp_text_input_manager_v3 is deliberately NOT in this table. That is
	 * the application half — "I am a text field" — and every boxed app that
	 * accepts typing needs it; denying it would be denying input methods to
	 * exactly the applications that need one most.
	 */
	{ "zwp_input_method_manager_v2",               KC_CAP_INPUT_METHOD },
};

struct kc_policy {
	struct wl_list link;
	char *box;			/* the profile name, == app_id */
	bool allow[KC_CAP_COUNT];
	/* One line per denied interface per box, not per bind attempt. A GTK
	 * app retries its registry walk on every window. */
	bool logged[KC_CAP_COUNT];
};

/* ── the profile ───────────────────────────────────────────────────────── */

/*
 * A box name reaches us from kdos-appbox, which took it from argv. It is
 * interpolated into a path, so it is checked rather than trusted: a plain name,
 * no separators, no dots-only. The same rule ksvc applies to a service name,
 * and for the same reason — that one was a glob injection.
 */
static bool box_name_ok(const char *name)
{
	if (!name || !*name || strlen(name) > 64)
		return false;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return false;
	for (const char *p = name; *p; p++) {
		bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
			  (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
			  *p == '.';
		if (!ok)
			return false;
	}
	return true;
}

/*
 * $HOME/.config, deliberately NOT $XDG_CONFIG_HOME.
 *
 * kdos-appbox's profile_path() resolves it exactly this way, and this file's
 * entire claim is that the two programs read THE SAME RECORD. Honouring XDG
 * here and not there means a user who sets XDG_CONFIG_HOME gets a container
 * built from one file and a capability policy read from another — which is
 * precisely the drift the one-record design exists to make impossible, arriving
 * silently and in the direction that grants rather than denies.
 *
 * kdos-comp's own comp.conf does follow XDG_CONFIG_HOME. That is not an
 * inconsistency to tidy up: comp.conf is one program's config, and this is a
 * shared record whose location is pinned by the other program.
 */
static bool profile_path(char *buf, size_t len, const char *box)
{
	const char *home = getenv("HOME");
	if (!home || !*home)
		return false;
	return snprintf(buf, len, "%s/.config/kdos/boxes/%s.conf", home, box) < (int)len;
}

static void profile_line(const char *key, char *value, const char *path,
			 int lineno, void *user)
{
	struct kc_policy *p = user;

	/*
	 * The wayland.* namespace is this file's. Everything else in a box
	 * profile belongs to kdos-appbox (network, ipc, devices, processes,
	 * home) and is NOT an error here — the two programs read the same file
	 * and each ignores the other's keys. Reporting them would fill the
	 * session log with complaints about a correct config.
	 */
	if (strncmp(key, "wayland.", 8))
		return;
	const char *cap_name = key + 8;

	for (int i = 0; i < KC_CAP_COUNT; i++) {
		if (strcmp(cap_name, cap_names[i]))
			continue;
		if (!strcmp(value, "allow") || !strcmp(value, "yes") ||
		    !strcmp(value, "true")) {
			p->allow[i] = true;
		} else if (!strcmp(value, "deny") || !strcmp(value, "no") ||
			   !strcmp(value, "false")) {
			p->allow[i] = false;
		} else {
			/* Left DENIED. An unreadable value must never read as a
			 * grant — that is the direction in which a typo becomes a
			 * capability. */
			wlr_log(WLR_ERROR, "%s:%d: %s must be allow or deny — "
					   "leaving it denied", path, lineno, key);
		}
		return;
	}
	wlr_log(WLR_ERROR, "%s:%d: unknown capability `%s`", path, lineno, cap_name);
}

/*
 * The policy for a box, loaded once and cached.
 *
 * Cached because the filter runs on every global for every client — a toolkit
 * walks the whole registry at startup, so an uncached read would be a stat and
 * an open per interface per client. The cost of that cache is that editing a
 * profile does not affect a box that is already running, which is the same
 * rule the profile already has for namespaces: a profile applies at create
 * time, and changing one tells you to recreate the box.
 */
static struct kc_policy *policy_for(struct kc_server *s, const char *box)
{
	struct kc_policy *p;
	wl_list_for_each(p, &s->policies, link)
		if (!strcmp(p->box, box))
			return p;

	p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;
	p->box = strdup(box);
	if (!p->box) {
		free(p);
		return NULL;
	}
	/* Everything denied until a profile says otherwise. A box with no
	 * profile at all is the most restricted case, not the least. */

	char path[512];
	if (profile_path(path, sizeof(path), box))
		kc_config_read(path, profile_line, p);

	wl_list_insert(&s->policies, &p->link);
	return p;
}

/* ── the filter ────────────────────────────────────────────────────────── */

static bool global_filter(const struct wl_client *client,
			  const struct wl_global *global, void *data)
{
	struct kc_server *s = data;
	const char *iface = wl_global_get_interface(global)->name;

	const struct wlr_security_context_v1_state *ctx =
		wlr_security_context_manager_v1_lookup_client(s->security_mgr,
							      client);

	/*
	 * Only an unsandboxed client may hand out sandbox identities. Without
	 * this the whole scheme is decorative: a boxed app binds the manager,
	 * creates a context of its own and connects through it wearing whatever
	 * app_id it likes.
	 */
	if (!strcmp(iface, "wp_security_context_manager_v1"))
		return ctx == NULL;

	if (!ctx)
		return true;		/* a host client; not our business */

	/*
	 * A context from an engine we do not know is not ours to interpret, and
	 * its app_id does not name a KDOS box. Treated as the most restricted
	 * case rather than the least: deny everything on the capability list.
	 */
	bool ours = ctx->sandbox_engine &&
		    !strcmp(ctx->sandbox_engine, KC_SANDBOX_ENGINE);

	for (size_t i = 0; i < sizeof(iface_caps) / sizeof(iface_caps[0]); i++) {
		if (strcmp(iface, iface_caps[i].iface))
			continue;
		enum kc_cap cap = iface_caps[i].cap;

		struct kc_policy *p = NULL;
		if (ours && box_name_ok(ctx->app_id))
			p = policy_for(s, ctx->app_id);
		if (p && p->allow[cap])
			return true;

		if (p && !p->logged[cap]) {
			p->logged[cap] = true;
			wlr_log(WLR_INFO, "box %s: %s denied (%s)", p->box,
				cap_names[cap], iface);
		}
		return false;
	}

	/* Not on the list: allowed. The list is what is dangerous, and a
	 * default-deny over every global would break a boxed client on the day
	 * wlroots adds a new one — including the ones it needs to draw. */
	return true;
}

/* ── wiring ────────────────────────────────────────────────────────────── */

static void security_commit(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, security_commit);
	const struct wlr_security_context_v1_commit_event *ev = data;
	(void)s;
	wlr_log(WLR_INFO, "security context: engine=%s app_id=%s instance=%s",
		ev->state->sandbox_engine ? ev->state->sandbox_engine : "(none)",
		ev->state->app_id ? ev->state->app_id : "(none)",
		ev->state->instance_id ? ev->state->instance_id : "(none)");
}

void kc_security_init(struct kc_server *s)
{
	wl_list_init(&s->policies);
	s->security_mgr = wlr_security_context_manager_v1_create(s->display);
	if (!s->security_mgr) {
		wlr_log(WLR_ERROR, "no security-context manager — boxed clients "
				   "will be indistinguishable from host ones");
		return;
	}
	s->security_commit.notify = security_commit;
	wl_signal_add(&s->security_mgr->events.commit, &s->security_commit);
	wl_display_set_global_filter(s->display, global_filter, s);
}

void kc_security_free(struct kc_server *s)
{
	/* The manager asserts its listener list is empty when the display tears
	 * it down, so this has to go before that and not after. */
	if (s->security_mgr)
		wl_list_remove(&s->security_commit.link);
	struct kc_policy *p, *tmp;
	wl_list_for_each_safe(p, tmp, &s->policies, link) {
		wl_list_remove(&p->link);
		free(p->box);
		free(p);
	}
}
