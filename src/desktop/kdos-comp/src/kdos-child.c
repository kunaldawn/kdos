// SPDX-License-Identifier: GPL-2.0-only
/*
 * Supervised chrome children, ported from the pre-fork kdos-comp.
 *
 * labwc's spawn_async_no_shell() double-forks so the compositor never
 * reaps — right for a terminal a keybinding opened, wrong for the SHELL:
 * kdos-shell is the panel, window list and launcher, and kdos-notifyd
 * owns org.freedesktop.Notifications. A session that loses one keeps
 * running with no chrome and no way to get any back. So these two are
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
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "kdos.h"

#define RESPAWN_MAX		5
#define RESPAWN_WINDOW_S	30

struct kdos_child {
	const char *cmd;
	pid_t pid;
	int fails;
	time_t since;
};

static struct kdos_child children[] = {
	{ .cmd = "kdos-shell" },
	{ .cmd = "kdos-notifyd" },
};
#define NCHILDREN (sizeof(children) / sizeof(children[0]))

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
		wlr_log(WLR_ERROR, "cannot fork for %s", c->cmd);
		return;
	}
	if (p == 0) {
		setsid();
		child_reset_signals();
		execlp(c->cmd, c->cmd, (char *)NULL);
		_exit(127);
	}
	c->pid = p;
}

void
kdos_children_start(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (size_t i = 0; i < NCHILDREN; i++) {
		children[i].since = now.tv_sec;
		spawn_one(&children[i]);
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
	for (size_t i = 0; i < NCHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (c->pid != pid) {
			continue;
		}
		c->pid = 0;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (now.tv_sec - c->since > RESPAWN_WINDOW_S) {
			c->since = now.tv_sec;
			c->fails = 0;
		}
		if (++c->fails > RESPAWN_MAX) {
			wlr_log(WLR_ERROR,
				"%s died %d times in %ds — not restarting it again",
				c->cmd, c->fails, RESPAWN_WINDOW_S);
			return true;
		}
		wlr_log(WLR_INFO, "%s exited (status %d) — restarting",
			c->cmd, status);
		spawn_one(c);
		return true;
	}
	return false;
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
	for (size_t i = 0; i < NCHILDREN; i++) {
		struct kdos_child *c = &children[i];
		if (c->pid <= 0) {
			continue;
		}
		int st;
		if (waitpid(c->pid, &st, WNOHANG) == c->pid) {
			kdos_child_reap(c->pid, WIFEXITED(st)
				? WEXITSTATUS(st) : -WTERMSIG(st));
		}
	}
}
