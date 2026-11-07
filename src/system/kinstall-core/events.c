/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall --unattended --events json — the install as a stream
 *
 * The graphical installer runs this binary through pkexec and reads its
 * standard output, one JSON object per line. The first line is the plan, so a
 * front end draws the same steps the install will run, skips included; every
 * line the install's child sends after it is the same event in JSON; the
 * last is the result:
 *
 *   {"t":"plan","steps":[{"index":0,"title":"Prepare","detail":"…",
 *                         "state":"pending"}, …]}
 *   {"t":"S","step":4,"title":"Copy system"}    a step begins
 *   {"t":"K","step":1}                          a step is skipped
 *   {"t":"P","step":4,"frac":0.42}              its progress, -1 unknown
 *   {"t":"N","step":4,"text":"1.2G copied …"}   its one-line note
 *   {"t":"L","text":"$ mkfs.ext4 …"}            a log line
 *   {"t":"W","text":"…"}                        a warning; the install goes on
 *   {"t":"F","step":9,"text":"…"}               the failure, with its reason
 *   {"t":"D"}                                   every step finished
 *   {"t":"exit","status":0}                     the last line, always
 *
 * Status 0 is a finished install, 1 a failed one, 2 answers refused before
 * anything was written (the reason in an `F` line with step -1). The process
 * exits with the same status.
 *
 * A LINE IS WRITTEN WHOLE AND FLUSHED, because the reader is a pipe and a
 * half line held in stdio's buffer is a progress bar that stops until the
 * next step. Nothing but these lines reaches stdout; the child's commands
 * write into the log, never here.
 * ---------------------------------
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kinstall-core.h"

static void out(KbBuf *b)
{
	kb_buf_str(b, "\n");
	fwrite(b->p, 1, b->n, stdout);
	fflush(stdout);
	kb_buf_free(b);
}

static void plan_line(void)
{
	KbBuf b = {0};

	kb_buf_str(&b, "{\"t\":\"plan\",\"steps\":[");
	for (int i = 0; i < inst.nsteps; i++) {
		kb_buf_printf(&b, "%s{\"index\":%d,\"title\":", i ? "," : "", i);
		kb_json_str(&b, inst.step[i].title);
		kb_buf_str(&b, ",\"detail\":");
		kb_json_str(&b, inst.step[i].detail);
		kb_buf_printf(&b, ",\"state\":\"%s\"}",
			      ki_state_name(inst.step[i].state));
	}
	kb_buf_str(&b, "]}");
	out(&b);
}

static void fail_line(int step, const char *text)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "{\"t\":\"F\",\"step\":%d,\"text\":", step);
	kb_json_str(&b, text);
	kb_buf_str(&b, "}");
	out(&b);
}

/*
 * ONE PROTOCOL LINE, AS JSON. Called before the step table takes the line in,
 * so `inst.cur` is still the step a P, N or F line belongs to; an S line names
 * its own. A step number outside the table is passed through as sent: the
 * table ignores it, and so may the reader.
 */
static void on_line(const char *s)
{
	KbBuf b = {0};
	const char *arg = s + 1;

	switch (s[0]) {
	case 'S': {
		int n = atoi(arg);

		kb_buf_printf(&b, "{\"t\":\"S\",\"step\":%d,\"title\":", n);
		kb_json_str(&b, n >= 0 && n < inst.nsteps ? inst.step[n].title
							  : "");
		kb_buf_str(&b, "}");
		break;
	}
	case 'K':
		kb_buf_printf(&b, "{\"t\":\"K\",\"step\":%d}", atoi(arg));
		break;
	case 'P': {
		double f = atof(arg);

		if (f > 1)
			f = 1;
		kb_buf_printf(&b, "{\"t\":\"P\",\"step\":%d,\"frac\":%.4f}",
			      inst.cur, f < 0 ? -1.0 : f);
		break;
	}
	case 'N':
		kb_buf_printf(&b, "{\"t\":\"N\",\"step\":%d,\"text\":", inst.cur);
		kb_json_str(&b, arg);
		kb_buf_str(&b, "}");
		break;
	case 'L':
	case 'W':
		kb_buf_printf(&b, "{\"t\":\"%c\",\"text\":", s[0]);
		kb_json_str(&b, arg);
		kb_buf_str(&b, "}");
		break;
	case 'F':
		fail_line(inst.cur, arg);
		return;
	case 'D':
		kb_buf_str(&b, "{\"t\":\"D\"}");
		break;
	default:
		return;
	}
	out(&b);
}

static void exit_line(int status)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "{\"t\":\"exit\",\"status\":%d}", status);
	out(&b);
}

/*
 * A front end that closes the pipe has gone away, and with it the only
 * reader of the stream; the install goes on to its end — stopping a copy
 * half way leaves the disk with no operating system — and its log is still
 * /var/log/kinstall.log. SIGPIPE would kill this process instead and orphan
 * the child, so it is ignored and a failed write is not an error.
 */
int ki_events_run(void)
{
	char why[256];

	signal(SIGPIPE, SIG_IGN);

	/* The payload walk is the one slow probe, and only the erase plan's
	 * A/B decision reads it; the rest of the plan does not depend on it. */
	probe_system();
	probe_disks();
	if (cfg.plan == PLAN_WIPE && cfg.lvm && disk_by_path(cfg.disk))
		probe_payload();
	ki_apps_enter();
	install_plan();
	plan_line();

	if (ki_answers_check(why, sizeof(why))) {
		fail_line(-1, why);
		exit_line(2);
		return 2;
	}

	inst.fd = -1;
	inst.logfd = 0;
	ki_line_hook = on_line;
	install_start(0);
	if (!inst.running) {
		fail_line(-1, inst.failmsg[0] ? inst.failmsg
					      : "the install could not start");
		exit_line(1);
		return 1;
	}

	while (inst.running) {
		struct pollfd p = { .fd = inst.fd, .events = POLLIN };

		if (poll(&p, 1, -1) < 0 && errno != EINTR)
			break;
		install_pump();
	}
	ki_line_hook = NULL;

	/* A child that died without its F line still failed, and the reader is
	 * told why rather than seeing the stream simply end. */
	if (inst.failed && !inst.done &&
	    strstr(inst.failmsg, "exited unexpectedly"))
		fail_line(inst.cur, inst.failmsg);
	exit_line(inst.done && !inst.failed ? 0 : 1);
	return inst.done && !inst.failed ? 0 : 1;
}
