// SPDX-License-Identifier: GPL-2.0-only
/*
 * Supervised chrome children, ported from the pre-fork kdos-comp.
 *
 * labwc's spawn_async_no_shell() double-forks so the compositor never
 * reaps — right for a terminal a keybinding opened, wrong for the SHELL:
 * `kdos-shell` is the taskbar, `kdos-desk` the desktop icons, `kdos-slit`
 * the dockapp column, and `kdos-notifyd` owns
 * org.freedesktop.Notifications. A session that loses one keeps running
 * with no chrome and no way to get any back. So these are
 * spawned with ONE fork, their pid is kept, and the reap arrives through
 * labwc's existing SIGCHLD source (a second signalfd on SIGCHLD would
 * race the first for the event — hence the hook in handle_sigchld, not a
 * second wl_event_loop_add_signal).
 *
 * A child that keeps dying is not respawned forever: RESPAWN_MAX
 * failures inside RESPAWN_WINDOW_S stop it — a crash loop hides the log
 * line that explains it.
 *
 * The signal MASK survives exec and so does SIG_IGN, so the child gets
 * both put back before execvp — same lesson labwc's own spawn.c encodes.
 */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "common/spawn.h"
#include "kdos.h"
/* For server.outputs at startup: the chrome for a screen that already exists
 * when the children are first started cannot come from the output-created
 * hook, because those outputs were created before the event loop was running. */
#include "labwc.h"
#include "output.h"

#define RESPAWN_MAX		5
#define RESPAWN_WINDOW_S	30

/*
 * `argv` rather than a bare command, because most of these are the SAME binary
 * told which surface it is: a layer-shell surface has one exclusive zone, so
 * the top and bottom panels cannot be one process, and it takes an `--output`
 * so it can be the panel of a PARTICULAR screen.
 *
 * ONE SET OF CHROME PER OUTPUT, and that is what makes a second monitor a
 * desktop rather than a wallpaper. An unnamed layer surface is placed by the
 * compositor on ONE output — so a machine with two screens had a panel, a
 * window list and a row of desktop icons on the first and nothing at all on
 * the second, with no setting anywhere that could have changed it. libktui has
 * a single cell buffer, so a second screen cannot be a second surface of the
 * same process; it has to be a second process, which is exactly what the two
 * panels already are.
 *
 * `want` is read once at startup from comp.conf. A child that is switched off
 * is never spawned and never reaped; it is not respawned into existence by a
 * reconfigure either, because a panel appearing under the user's pointer
 * halfway through a session is worse than one that waits for the next login.
 */
#define KDOS_MAX_CHILDREN	40
#define KDOS_OUTPUT_NAME_MAX	64

/*
 * `want` is a `const bool *` into kdos_conf, and the panel's switch is an
 * ENUM there rather than a bool — so it is mirrored here, once, at the point
 * the children are started. A pointer into a struct field of the wrong type is
 * the sort of thing that compiles.
 */
static bool kdos_want_panel = true;
static char kdos_panel_cells_arg[16] = "2";

static const struct {
	const char *cmd;
	const char *arg;		/* NULL for none */
	const bool *want;		/* NULL = always */
	bool per_output;
} TEMPLATES[] = {
	/*
	 * ONE taskbar. There were two panels here — a menu bar at the top and
	 * a second panel at the bottom — which is two exclusive zones and the
	 * window list drawn twice. `panel = top|off` in comp.conf moves it or
	 * turns it off; the edge is an ARGUMENT rather than a second row,
	 * because two rows here would be two panels again.
	 */
	{ "kdos-shell", NULL, &kdos_want_panel, true },
	{ "kdos-desk", NULL, &kdos_conf.desktop_icons, true },
	/*
	 * The dockapp column. Off by default — a slit nobody configured is a
	 * column of dim `!` marks — and it had no row here at all for a
	 * release, which meant writing ~/.config/kdos/slit.conf did nothing
	 * whatever the file's own comment claimed.
	 */
	{ "kdos-slit", NULL, &kdos_conf.slit, true },
	/*
	 * The notification daemon is NOT per-output: it owns
	 * org.freedesktop.Notifications, which is one bus name, and a second
	 * instance would simply fail to take it. Toasts land on whichever
	 * screen the compositor puts them on, which is the honest limit of a
	 * single-owner protocol.
	 */
	{ "kdos-notifyd", NULL, NULL, false },
	/*
	 * The clipboard history. NOT per-output either: it owns one socket in
	 * $XDG_RUNTIME_DIR and holds the history in memory, and a second
	 * instance would be a second history nobody could reach.
	 */
	{ "kdos-clip", NULL, &kdos_conf.clipboard, false },
};
#define NTEMPLATES ((int)(sizeof(TEMPLATES) / sizeof(TEMPLATES[0])))

struct kdos_child {
	bool live;
	int tmpl;			/* index into TEMPLATES */
	char output[KDOS_OUTPUT_NAME_MAX];	/* "" for the global ones */
	/* cmd + --output N + --font N + --top/--bottom + --cells N +
	 * --clock N + --autohide + --no-icons + NULL = 13 at the widest, and
	 * an overrun here writes past the slot into the next child's. */
	const char *argv[20];
	pid_t pid;
	int fails;
	time_t since;
	bool stopping;			/* its output went away; do not respawn */
};

static struct kdos_child children[KDOS_MAX_CHILDREN];
static bool children_started;
/* one crash-loop dialog per reconfigure cycle — a prompt spawned per give-up
 * would itself be a loop when several children die together */
static bool gaveup_prompted;
static struct wl_event_source *display_apply_timer;

/* For the log lines: `kdos-shell --bottom on HDMI-A-1` rather than four rows
 * all saying kdos-shell, which is what a crash-loop message has to tell
 * apart. */
static const char *
child_label(const struct kdos_child *c)
{
	static char buf[160];
	snprintf(buf, sizeof(buf), "%s%s%s%s%s", TEMPLATES[c->tmpl].cmd,
		 TEMPLATES[c->tmpl].arg ? " " : "",
		 TEMPLATES[c->tmpl].arg ? TEMPLATES[c->tmpl].arg : "",
		 c->output[0] ? " on " : "", c->output);
	return buf;
}

/*
 * argv is built from the template and the slot's own `output` buffer, so every
 * pointer in it outlives the exec — the same rule kb_argv_add taught the rest
 * of the tree. Nothing here is freed: a slot is reused in place.
 */
static void
child_build_argv(struct kdos_child *c)
{
	int n = 0;
	c->argv[n++] = TEMPLATES[c->tmpl].cmd;
	if (TEMPLATES[c->tmpl].arg) {
		c->argv[n++] = TEMPLATES[c->tmpl].arg;
	}
	if (c->output[0]) {
		c->argv[n++] = "--output";
		c->argv[n++] = c->output;
	}
	/*
	 * One font for all of the chrome, from comp.conf. kdos_conf outlives
	 * every child, so pointing into it is safe — the same rule the output
	 * name follows by living in the slot.
	 */
	if (kdos_conf.chrome_font[0]) {
		c->argv[n++] = "--font";
		c->argv[n++] = kdos_conf.chrome_font;
	}
	/* clock_format and panel_autohide are the panel's, not the desk's or
	 * the notifyd's */
	if (!strcmp(TEMPLATES[c->tmpl].cmd, "kdos-shell")) {
		c->argv[n++] = kdos_conf.panel_edge == KDOS_PANEL_TOP
			? "--top" : "--bottom";
		c->argv[n++] = "--cells";
		c->argv[n++] = kdos_panel_cells_arg;
		if (kdos_conf.clock_format[0]) {
			c->argv[n++] = "--clock";
			c->argv[n++] = kdos_conf.clock_format;
		}
		if (kdos_conf.panel_autohide) {
			c->argv[n++] = "--autohide";
		}
	}
	/*
	 * `icons = no` reaches every surface that can draw one — and ONLY
	 * those. A flag handed to a child that does not parse it is a usage
	 * error and a child that never starts, which under a respawn loop is a
	 * session that never settles.
	 */
	if (!kdos_conf.icons
			&& (!strcmp(TEMPLATES[c->tmpl].cmd, "kdos-shell")
				|| !strcmp(TEMPLATES[c->tmpl].cmd, "kdos-desk"))) {
		c->argv[n++] = "--no-icons";
	}
	c->argv[n] = NULL;
}

/*
 * The mask and every SIG_IGN survive exec, so both have to go back before it.
 * EVERY disposition, not just SIGPIPE the way labwc's own spawn.c does: labwc
 * assumes it inherited nothing, and a session started under anything that
 * ignores a signal — `nohup kdos-desktop` is the obvious one — hands that
 * SIG_IGN all the way down to kdos-shell. Measured: SigIgn 0x1 on a running
 * panel, so `kdos theme <accent>` SIGHUPed a process that could not receive
 * it and the live retint silently did nothing.
 */
static void
child_reset_signals(void)
{
	sigset_t none;
	sigemptyset(&none);
	sigprocmask(SIG_SETMASK, &none, NULL);
	/* The 31 standard signals: NSIG is not visible under
	 * _POSIX_C_SOURCE, and an inherited SIG_IGN comes from a shell or a
	 * nohup, neither of which ignores a real-time signal. */
	for (int sig = 1; sig <= 31; sig++) {
		if (sig == SIGKILL || sig == SIGSTOP) {
			continue;
		}
		struct sigaction old;
		if (sigaction(sig, NULL, &old) == 0
				&& old.sa_handler == SIG_IGN) {
			signal(sig, SIG_DFL);
		}
	}
}

static void
spawn_one(struct kdos_child *c)
{
	pid_t p = fork();
	if (p < 0) {
		wlr_log(WLR_ERROR, "cannot fork for %s", child_label(c));
		return;
	}
	if (p == 0) {
		setsid();
		child_reset_signals();
		execvp(c->argv[0], (char *const *)c->argv);
		_exit(127);
	}
	c->pid = p;
}

/* A free slot, or NULL when the table is full (40 = ten outputs' worth). */
static struct kdos_child *
child_alloc(void)
{
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		if (!children[i].live) {
			memset(&children[i], 0, sizeof(children[i]));
			children[i].live = true;
			return &children[i];
		}
	}
	wlr_log(WLR_ERROR, "too many supervised children");
	return NULL;
}

static void
start_template(int tmpl, const char *output)
{
	if (TEMPLATES[tmpl].want && !*TEMPLATES[tmpl].want) {
		return;
	}
	struct kdos_child *c = child_alloc();
	if (!c) {
		return;
	}
	c->tmpl = tmpl;
	if (output) {
		snprintf(c->output, sizeof(c->output), "%s", output);
	}
	child_build_argv(c);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	c->since = now.tv_sec;
	spawn_one(c);
}

/*
 * DISPLAY APPLY contract: an output arrived, so the saved layout in
 * ~/.config/kdos/displays.conf gets re-asserted — `kdos-display --apply`
 * matches heads by name, skips unknowns and exits. Debounced one second
 * so a dock plugging three heads spawns one apply, not three; not
 * supervised, because a one-shot with nothing to respawn is not chrome.
 */
static int
display_apply_fire(void *data)
{
	(void)data;
	spawn_async_no_shell("kdos-display --apply");
	return 0;
}

static void
display_apply_debounce(void)
{
	if (!display_apply_timer) {
		display_apply_timer = wl_event_loop_add_timer(
			wl_display_get_event_loop(server.wl_display),
			display_apply_fire, NULL);
	}
	if (display_apply_timer) {
		wl_event_source_timer_update(display_apply_timer, 1000);
	}
}

/*
 * A screen appeared. Give it its own panel, window list and desktop.
 *
 * Before the compositor has started its children at all this does nothing:
 * outputs are created during server_start(), before the wayland socket is
 * being serviced, and a panel spawned there would fail to connect and burn a
 * respawn attempt for every output on the machine.
 */
void
kdos_children_output_add(const char *name)
{
	if (!children_started || !name || !*name) {
		return;
	}
	display_apply_debounce();
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		/* a `stopping` slot is the SAME name on its way out — a
		 * replugged monitor (DP retrain on resume) re-creates the
		 * output before the old chrome is reaped, and matching it
		 * here left the new screen bare for the session */
		if (children[i].live && !children[i].stopping
				&& !strcmp(children[i].output, name)) {
			return;	/* already has its set */
		}
	}
	wlr_log(WLR_INFO, "output %s: starting its chrome", name);
	for (int t = 0; t < NTEMPLATES; t++) {
		if (TEMPLATES[t].per_output) {
			start_template(t, name);
		}
	}
}

/*
 * A screen went away. Its chrome is told to go with it — SIGTERM, and the slot
 * is marked `stopping` so the reap frees it instead of respawning it. Left
 * alone, a panel whose output has been unplugged is a client whose layer
 * surface the compositor has closed: it exits, gets respawned, exits again,
 * and five of those in thirty seconds is how the supervisor stops trusting a
 * program that was working perfectly.
 */
void
kdos_children_output_remove(const char *name)
{
	if (!name || !*name) {
		return;
	}
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (!c->live || strcmp(c->output, name)) {
			continue;
		}
		c->stopping = true;
		if (c->pid > 0) {
			kill(c->pid, SIGTERM);
		} else {
			c->live = false;
		}
	}
}

void
kdos_children_start(void)
{
	children_started = true;

	/* The enum-to-bool mirror, and the cell count as the string it will be
	 * passed as: kb_argv-style tables store POINTERS, so every argument a
	 * child gets has to outlive the exec. */
	kdos_want_panel = kdos_conf.panel_edge != KDOS_PANEL_OFF;
	snprintf(kdos_panel_cells_arg, sizeof(kdos_panel_cells_arg), "%d",
		 kdos_conf.panel_cells > 0 ? kdos_conf.panel_cells : 2);

	for (int t = 0; t < NTEMPLATES; t++) {
		if (TEMPLATES[t].want && !*TEMPLATES[t].want) {
			wlr_log(WLR_INFO, "%s%s%s: off in comp.conf",
				TEMPLATES[t].cmd, TEMPLATES[t].arg ? " " : "",
				TEMPLATES[t].arg ? TEMPLATES[t].arg : "");
			continue;
		}
		if (!TEMPLATES[t].per_output) {
			start_template(t, NULL);
		}
	}

	/* And one set for every output that already exists. */
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		if (output->wlr_output && output->wlr_output->name) {
			kdos_children_output_add(output->wlr_output->name);
		}
	}
}

/*
 * Called from labwc's handle_sigchld() AFTER the consuming waitid().
 * Returns true when the pid was one of ours (already handled — the
 * caller should not log it as a stray child).
 */
bool
kdos_child_reap(pid_t pid, int status)
{
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (!c->live || c->pid != pid) {
			continue;
		}
		c->pid = 0;

		/* Its output is gone; the slot goes with it. */
		if (c->stopping) {
			c->live = false;
			return true;
		}

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec - c->since > RESPAWN_WINDOW_S) {
			c->since = now.tv_sec;
			c->fails = 0;
		}
		if (++c->fails > RESPAWN_MAX) {
			wlr_log(WLR_ERROR,
				"%s died %d times in %ds — not restarting it again",
				child_label(c), c->fails, RESPAWN_WINDOW_S);
			/* the log line is invisible from inside the session;
			 * this is the one place the user can be told */
			if (!gaveup_prompted) {
				gaveup_prompted = true;
				char msg[256];
				snprintf(msg, sizeof(msg), "%s crashed "
					"repeatedly and was given up on — "
					"Reconfigure (W-Escape menu) retries",
					child_label(c));
				const char *argv[] = { "kdos-prompt",
					"--message", msg, NULL };
				pid_t p = fork();
				if (p == 0) {
					setsid();
					child_reset_signals();
					execvp(argv[0], (char *const *)argv);
					_exit(127);
				}
			}
			return true;
		}
		wlr_log(WLR_INFO, "%s exited (status %d) — restarting",
			child_label(c), status);
		spawn_one(c);
		return true;
	}
	return false;
}

/*
 * Reconfigure re-arms supervision: the counters reset and a child that
 * crossed RESPAWN_MAX gets exactly one more run of attempts. That is
 * the recovery the give-up dialog names — before it, a crashed-out
 * panel was gone until the next login.
 */
void
kdos_children_reconfigure(void)
{
	if (!children_started) {
		return;
	}
	gaveup_prompted = false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (!c->live || c->stopping) {
			continue;
		}
		c->fails = 0;
		c->since = now.tv_sec;
		if (c->pid == 0) {
			wlr_log(WLR_INFO, "%s: retrying after reconfigure",
				child_label(c));
			spawn_one(c);
		}
	}
}

/*
 * SIGCHLD is not queued and signalfd coalesces it: two children dying
 * close together deliver ONE event, and labwc's handle_sigchld() reaps
 * exactly one zombie per event. For labwc's own fire-and-forget spawns
 * that is a cosmetic zombie until the next SIGCHLD; for a supervised
 * child it is a dead panel that never respawns. So the handler also
 * polls OUR pids directly — targeted waitpid(WNOHANG), so it can never
 * steal a zombie that is not ours (Xwayland's included).
 */
void
kdos_children_poll(void)
{
	for (int i = 0; i < KDOS_MAX_CHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (!c->live || c->pid <= 0) {
			continue;
		}
		int st;
		if (waitpid(c->pid, &st, WNOHANG) == c->pid) {
			kdos_child_reap(c->pid, WIFEXITED(st)
				? WEXITSTATUS(st) : -WTERMSIG(st));
		}
	}
}
