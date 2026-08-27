/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The verbs: end, kill, stop, continue, renice.
 *
 * A process the caller OWNS goes through kill(2) and setpriority(2) in this
 * process. Anything else — and any renice below zero — goes through
 * kdos-resctl, and when that helper is missing or has lost its setuid bit the
 * button is DISABLED WITH THE REASON ON IT rather than present and failing.
 * A control that looks available and does nothing teaches people to distrust
 * the whole surface.
 *
 * NO SHELL AND NO system(). Process names and box names arrive from /proc and
 * are attacker-controlled in the ordinary case: any user can name a process
 * anything. Everything execs through KbArgv, and the helper takes no path at
 * all.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "res.h"

#define RESCTL "/usr/bin/kdos-resctl"

/*
 * Why the helper cannot be used, or NULL when it can. The answer is the text
 * a disabled button carries, so it has to be a sentence rather than an errno.
 */
const char *res_act_helper_why(void)
{
	struct stat st;
	/*
	 * A FIXTURE IS A RECORDED MACHINE AND THIS IS A QUESTION ABOUT THE
	 * RUNNING ONE. Stat'ing the real filesystem here makes the detail
	 * page's footer — and therefore its golden frame — depend on whether
	 * the host happens to have the helper installed, so the same dump
	 * renders differently on two machines and the golden belongs to
	 * whoever generated it. Nothing is executed on a fixture anyway: the
	 * verbs are drawn, never run.
	 */
	if (R.fixture)
		return NULL;
	if (stat(RESCTL, &st) != 0)
		return "kdos-resctl is not installed";
	if (!(st.st_mode & S_ISUID))
		return "kdos-resctl has lost its setuid bit";
	return NULL;
}

static int own(const KprProc *p)
{
	return p && (uid_t)p->uid == getuid();
}

static int via_helper(const char *verb, int pid, const char *arg)
{
	if (res_act_helper_why())
		return -1;

	char spid[16];
	snprintf(spid, sizeof(spid), "%d", pid);

	KbArgv a = { 0 };
	kb_argv_add(&a, RESCTL);
	kb_argv_add(&a, verb);
	kb_argv_add(&a, spid);
	kb_argv_add(&a, arg);
	kb_argv_end(&a);
	int rc = kb_run(&a);
	return rc == 0 ? 0 : -1;
}

int res_act_signal(const KprProc *p, int sig)
{
	if (!p || p->pid <= 1)
		return -1;

	if (own(p)) {
		if (kill(p->pid, sig) == 0)
			return 0;
		if (errno != EPERM)
			return -1;
		/* Owned by uid and still refused: fall through to the helper
		 * rather than reporting a failure the user can do nothing
		 * about. */
	}

	const char *name = sig == SIGKILL ? "KILL"
			   : sig == SIGSTOP ? "STOP"
			   : sig == SIGCONT ? "CONT" : "TERM";
	return via_helper("signal", p->pid, name);
}

int res_act_renice(const KprProc *p, int nice)
{
	if (!p || p->pid <= 1 || nice < -20 || nice > 19)
		return -1;

	/*
	 * Lowering a nice value is a privileged operation even on your own
	 * process, so it goes to the helper regardless of ownership. Raising
	 * it is not, and does not.
	 */
	if (own(p) && nice >= 0) {
		if (setpriority(PRIO_PROCESS, (id_t)p->pid, nice) == 0)
			return 0;
		if (errno != EPERM && errno != EACCES)
			return -1;
	}
	char s[16];
	snprintf(s, sizeof(s), "%d", nice);
	return via_helper("renice", p->pid, s);
}

/*
 * Whether a verb can be offered at all, and why not. A caller draws the button
 * disabled with this string on it.
 */
const char *res_act_why_disabled(const KprProc *p)
{
	if (!p)
		return "nothing selected";
	if (p->pid <= 1)
		return "pid 1 is not a task-manager target";
	if (own(p))
		return NULL;
	return res_act_helper_why();
}
