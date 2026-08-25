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
 * One subject, full screen.
 *
 * A table row is four columns wide and a process has thirty facts, so the
 * list pages answer "which one" and this answers "what is it". Enter opens
 * it, Esc goes back with the selection where it was.
 *
 * THE RINGS START WHEN THE SUBJECT IS OPENED, and that is deliberate rather
 * than a shortcut. Keeping a history for every process on the machine would
 * be a ring per pid — hundreds of them, most for processes nobody will ever
 * look at — and the page would still be unable to show anything from before
 * it was asked. A chart that begins at the moment you asked is honest about
 * what it knows; one back-filled with zeroes is not.
 *
 * THE VERBS LIVE HERE, and only here. `act.c` can end and renice a process,
 * and the one thing this program must never do is put that on a key that a
 * list page reads while the cursor happens to be on a row. A verb needs its
 * subject named on the screen and a confirm that says what will happen, and
 * that is exactly what this page is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "res.h"

enum { DS_NONE = 0, DS_PROC, DS_APP, DS_FACTS };

/* The verbs, in the order the button bar drops them: most useful first, so a
 * narrow window keeps End and loses Nice. */
enum { B_END = 0, B_KILL, B_NICE_DOWN, B_NICE_UP, B_CLOSE, B_N };

#define D_FACTS 12
#define D_FACT_LEN 128

static struct {
	int kind;
	int pid;			/* DS_PROC                        */
	char title[96], sub[128];
	char box[64];
	char facts[D_FACTS][D_FACT_LEN];	/* DS_FACTS               */
	int nfacts;
	KprHist h_cpu, h_mem;
	int focus;			/* which button has the caret     */
	int top;			/* the fact list's scroll         */
} D;

/* The subject of a pending confirm. The modal takes a void(*)(void), so what
 * it is about has to be here rather than in the closure C does not have. */
static int g_act_pid;
static int g_act_sig;
static int g_act_nice;

int res_detail_active(void) { return D.kind != DS_NONE; }
const char *res_detail_title(void) { return D.title; }
const char *res_detail_subtitle(void) { return D.sub[0] ? D.sub : NULL; }

void res_detail_close(void)
{
	D.kind = DS_NONE;
	D.pid = 0;
	D.nfacts = 0;
	D.focus = 0;
	D.top = 0;
}

static void open_common(void)
{
	/* Percent rings are PINNED 0..100 and the memory ring is not: a CPU
	 * chart that rescaled to its own peak would draw 3% as a full band. */
	kpr_hist_init(&D.h_cpu, 1);
	kpr_hist_init(&D.h_mem, 0);
	D.focus = 0;
	D.top = 0;
	D.nfacts = 0;
}

void res_detail_open_proc(int pid)
{
	const KprProc *p = kpr_find_pid(&R.sample, pid);

	if (!p)
		return;
	open_common();
	D.kind = DS_PROC;
	D.pid = pid;
	kb_strlcpy(D.title, p->comm, sizeof(D.title));
	kb_strlcpy(D.box, p->box, sizeof(D.box));
	if (p->box[0])
		snprintf(D.sub, sizeof(D.sub), "pid %d  %s  appbox %s", pid,
			 kpr_user_of(p->uid), p->box);
	else
		snprintf(D.sub, sizeof(D.sub), "pid %d  %s", pid,
			 kpr_user_of(p->uid));
}

void res_detail_open_app(const char *name, const char *box, int pid)
{
	open_common();
	D.kind = DS_APP;
	D.pid = pid;
	kb_strlcpy(D.title, name, sizeof(D.title));
	kb_strlcpy(D.box, box ? box : "", sizeof(D.box));
	if (box && *box)
		snprintf(D.sub, sizeof(D.sub), "application  appbox %s", box);
	else
		kb_strlcpy(D.sub, "application", sizeof(D.sub));
}

void res_detail_open_facts(const char *title, const char *sub,
			   const char *const *lines, int n)
{
	open_common();
	D.kind = DS_FACTS;
	D.pid = 0;
	kb_strlcpy(D.title, title, sizeof(D.title));
	kb_strlcpy(D.sub, sub ? sub : "", sizeof(D.sub));
	for (int i = 0; i < n && D.nfacts < D_FACTS; i++)
		kb_strlcpy(D.facts[D.nfacts++], lines[i], D_FACT_LEN);
}

/* ── the rings ───────────────────────────────────────────────────────────
 *
 * Fed from res_sample(), so the subject's history advances on the program's
 * own clock rather than on how often this page happens to be drawn.
 */
static double proc_cpu(const KprProc *p)
{
	const KprProc *prev = R.have_prev ? kpr_find_pid(&R.prev, p->pid)
					  : NULL;
	double pc = kpr_proc_cpu(prev, p, R.sample.wall_ms - R.prev.wall_ms);

	if (RC.cpu_of_machine && R.cpu.ncpu > 0)
		pc /= (double)R.cpu.ncpu;
	return pc;
}

void res_detail_sample(void)
{
	if (D.kind == DS_NONE || D.kind == DS_FACTS)
		return;
	if (!R.have_prev)
		return;

	double cpu = 0;
	unsigned long long rss = 0;

	if (D.kind == DS_PROC) {
		const KprProc *p = kpr_find_pid(&R.sample, D.pid);
		if (!p)
			return;		/* it exited; the page says so   */
		cpu = proc_cpu(p);
		rss = p->rss;
	} else {
		/*
		 * An application is its processes. The box is the group key,
		 * exactly as the Applications page derives it — a second rule
		 * for what belongs to an app would put two different numbers
		 * on two screens for one thing.
		 */
		for (int i = 0; i < R.sample.n; i++) {
			const KprProc *p = &R.sample.p[i];
			if (D.box[0]) {
				if (strcmp(p->box, D.box))
					continue;
			} else if (strcmp(p->comm, D.title)) {
				continue;
			}
			cpu += proc_cpu(p);
			rss += p->rss;
		}
	}

	kpr_hist_push(&D.h_cpu, cpu > 100.0 && D.h_cpu.pinned ? 100.0 : cpu);
	kpr_hist_push(&D.h_mem, (double)rss);
}

/* What the sampler must collect while this page is up. Without it a detail
 * view opened from the CPU page would show a subject with no cmdline and no
 * io, because those files are read only for the pages that display them. */
unsigned res_detail_wants(void)
{
	if (D.kind == DS_PROC || D.kind == DS_APP)
		return KPR_WANT_STATUS | KPR_WANT_IO | KPR_WANT_CMDLINE |
		       KPR_WANT_BOX;
	return 0;
}

/* ── facts ───────────────────────────────────────────────────────────── */

/* How many descriptors the subject holds. There is no counter for this in
 * libkproc because nothing else asks: it is one readdir, and only of the one
 * process somebody is looking at. */
static int fd_count(int pid)
{
	char dir[256];
	int n = 0;
	char **names;

	snprintf(dir, sizeof(dir), "%s/%d/fd", kpr_proc(), pid);
	names = kb_listdir(dir, &n);
	if (!names)
		return -1;
	kb_strv_free(names);
	return n;
}

/*
 * Elapsed since the process started.
 *
 * `starttime` is in clock ticks since BOOT, so the answer needs the boot
 * instant, which is /proc/stat's `btime`. A kernel or a fixture without it
 * gets a dash: a start time computed from an assumed boot is a timestamp
 * that is wrong by however long the machine has been up.
 */
static long long boot_time(void)
{
	char *st = kpr_slurp_proc("stat");
	long long bt = -1;

	if (!st)
		return -1;
	const char *b = strstr(st, "btime ");
	if (b)
		bt = strtoll(b + 6, NULL, 10);
	free(st);
	return bt;
}

static void fmt_elapsed(char *out, size_t cap, const KprProc *p)
{
	long long bt = boot_time();
	long hz = kpr_hz();

	if (bt <= 0 || hz <= 0) {
		snprintf(out, cap, "%s", res_none());
		return;
	}
	long long started = bt + (long long)(p->starttime / (unsigned long long)hz);
	long long now = bt + (long long)(R.sample.wall_ms / 1000);
	long long secs = now - started;

	if (secs < 0)
		secs = 0;
	if (secs < 3600)
		snprintf(out, cap, "%lldm %llds", secs / 60, secs % 60);
	else if (secs < 86400)
		snprintf(out, cap, "%lldh %lldm", secs / 3600,
			 (secs % 3600) / 60);
	else
		snprintf(out, cap, "%lldd %lldh", secs / 86400,
			 (secs % 86400) / 3600);
}

/*
 * RETURNS THE NEXT FREE ROW, because it does not always draw the same number
 * of them: a boxed process has an appbox line, a readable one has an exe and
 * a cmdline, and root's has neither. A caller that advanced by a constant
 * would leave a gap under a short subject and overdraw a long one — and the
 * gap is what pushed the charts off the bottom of the page.
 */
static int draw_proc_facts(int x, int y, int w, const KprProc *p)
{
	char line[320], tmp[64];
	int row = y;

	kch_group(x, row++, w, "identity");

	snprintf(line, sizeof(line), "pid         %d   ppid %d   threads %d",
		 p->pid, p->ppid, p->threads);
	ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);

	snprintf(line, sizeof(line), "user        %s   state %c   nice %d",
		 kpr_user_of(p->uid), p->state, p->nice);
	ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);

	int nfd = fd_count(p->pid);
	fmt_elapsed(tmp, sizeof(tmp), p);
	if (nfd >= 0)
		snprintf(line, sizeof(line), "elapsed     %s   open fds %d",
			 tmp, nfd);
	else
		snprintf(line, sizeof(line), "elapsed     %s   open fds %s",
			 tmp, res_none());
	ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);

	if (p->box[0]) {
		snprintf(line, sizeof(line), "appbox      %s", p->box);
		ktui_draw_text(x + 1, row++, w - 2, line, KT_ACCENT, KT_BG, 0);
	}
	if (p->exe) {
		snprintf(line, sizeof(line), "exe         %s", p->exe);
		ktui_draw_text(x + 1, row++, w - 2, line, KT_MID, KT_BG, 0);
	}
	if (p->cmdline) {
		snprintf(line, sizeof(line), "cmdline     %s", p->cmdline);
		ktui_draw_text(x + 1, row++, w - 2, line, KT_MID, KT_BG, 0);
	}

	kch_group(x, row++, w, "resources");
	snprintf(line, sizeof(line), "memory      %s rss   %s swap",
		 res_size(p->rss), res_size(p->swap));
	ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);
	/*
	 * `res_counter` is what renders KPR_UNREADABLE as a dash. An
	 * unprivileged reader gets EACCES on another user's io, and 0 there
	 * would be a claim that root's sshd has never touched the disk.
	 */
	snprintf(line, sizeof(line), "disk        %s read   %s written",
		 res_counter(p->rd_bytes), res_counter(p->wr_bytes));
	ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT, KT_BG, 0);
	return row;
}

void res_detail_draw(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;

	if (D.kind == DS_FACTS) {
		for (int i = D.top; i < D.nfacts && row < bottom - 2; i++)
			ktui_draw_text(x + 1, row++, w - 2, D.facts[i],
				       KT_TEXT, KT_BG, 0);
	} else {
		const KprProc *p = D.kind == DS_PROC
				   ? kpr_find_pid(&R.sample, D.pid) : NULL;

		if (D.kind == DS_PROC && !p) {
			/*
			 * The subject exited while it was open. Saying so
			 * beats an empty page, and beats closing the view out
			 * from under somebody who was reading it.
			 */
			ktui_draw_text(x + 1, row + 1, w - 2,
				       "this process has exited", KT_WARN,
				       KT_BG, 0);
			row += 3;
		} else if (p) {
			row = draw_proc_facts(x, row, w, p);
		} else {
			kch_group(x, row++, w, "identity");
			char line[160];
			snprintf(line, sizeof(line), "appbox      %s",
				 D.box[0] ? D.box : res_none());
			ktui_draw_text(x + 1, row++, w - 2, line, KT_ACCENT,
				       KT_BG, 0);
		}

		/*
		 * The charts get whatever is left above the button bar, split
		 * between them. Below four rows there is no chart worth
		 * drawing and the facts are the page.
		 */
		int avail = bottom - 2 - row;
		if (avail >= 4) {
			int ch = avail / 2;
			char rd[32];

			kch_group(x, row++, w, "since this page was opened");
			snprintf(rd, sizeof(rd), "%.0f%%",
				 D.h_cpu.n ? kpr_hist_at(&D.h_cpu,
							 D.h_cpu.n - 1) : 0.0);
			res_graph(900, krect(x + 1, row, w - 2, ch - 1),
				  &D.h_cpu, "cpu", rd);
			row += ch;
			res_graph(901, krect(x + 1, row, w - 2, ch - 1),
				  &D.h_mem, "memory",
				  res_size(D.h_mem.n
					   ? (unsigned long long)kpr_hist_at(
						     &D.h_mem, D.h_mem.n - 1)
					   : 0));
		}
	}

	/* ── the verbs ─────────────────────────────────────────────────── */
	struct kch_button b[B_N];
	const KprProc *p = D.kind == DS_PROC ? kpr_find_pid(&R.sample, D.pid)
					     : NULL;
	const char *why = p ? res_act_why_disabled(p) : NULL;
	int can = D.kind == DS_PROC && p && !why;

	b[B_END] = (struct kch_button){ "End", can };
	b[B_KILL] = (struct kch_button){ "Kill", can };
	b[B_NICE_DOWN] = (struct kch_button){ "Nice -", can };
	b[B_NICE_UP] = (struct kch_button){ "Nice +", can };
	b[B_CLOSE] = (struct kch_button){ "Close", 1 };

	int left = kch_buttons(w, bottom - 1, b, B_N, D.focus);

	/*
	 * A DISABLED BUTTON SAYS WHY, on the row it is on. "End" greyed out
	 * with no reason is a control that looks broken; the reason is
	 * usually "it is not yours", which is an instruction.
	 */
	if (why && left > 2)
		ktui_draw_text(x, bottom - 1, left - 1, why, KT_DIM, KT_BG, 0);
	else if (!why && left > 2)
		ktui_draw_text(x, bottom - 1, left - 1,
			       "Esc  back to the list", KT_DIM, KT_BG, 0);
}

/* ── the verbs ───────────────────────────────────────────────────────── */

static void do_signal(void)
{
	const KprProc *p = kpr_find_pid(&R.sample, g_act_pid);

	if (p)
		res_act_signal(p, g_act_sig);
}

static void do_renice(void)
{
	const KprProc *p = kpr_find_pid(&R.sample, g_act_pid);

	if (p)
		res_act_renice(p, g_act_nice);
}

static void ask_signal(int sig, const char *verb)
{
	const KprProc *p = kpr_find_pid(&R.sample, D.pid);
	char msg[224];

	if (!p || res_act_why_disabled(p))
		return;
	g_act_pid = D.pid;
	g_act_sig = sig;
	/* The subject is NAMED. "Are you sure?" is a question about nothing. */
	if (p->box[0])
		snprintf(msg, sizeof(msg),
			 "%s %s (pid %d) in appbox %s? Unsaved work in it is "
			 "lost.", verb, p->comm, p->pid, p->box);
	else
		snprintf(msg, sizeof(msg),
			 "%s %s (pid %d)? Unsaved work in it is lost.",
			 verb, p->comm, p->pid);
	res_confirm(sig == 9 ? "Kill process" : "End process", msg, verb,
		    do_signal);
}

static void bump_nice(int by)
{
	const KprProc *p = kpr_find_pid(&R.sample, D.pid);
	int want;

	if (!p || res_act_why_disabled(p))
		return;
	want = p->nice + by;
	if (want > 19)
		want = 19;
	if (want < -20)
		want = -20;
	g_act_pid = D.pid;
	g_act_nice = want;
	/*
	 * No confirm for a renice: it is reversible, it destroys nothing, and
	 * a dialog on every nudge of a priority is a dialog people click
	 * through without reading — which is what makes the kill confirm
	 * worth having.
	 */
	do_renice();
}

static void activate(int which)
{
	switch (which) {
	case B_END:
		ask_signal(15, "End");
		break;
	case B_KILL:
		ask_signal(9, "Kill");
		break;
	case B_NICE_DOWN:
		bump_nice(-1);
		break;
	case B_NICE_UP:
		bump_nice(1);
		break;
	case B_CLOSE:
		res_detail_close();
		break;
	}
}

int res_detail_key(int k)
{
	switch (k) {
	case KT_K_LEFT:
		if (D.focus > 0)
			D.focus--;
		return 1;
	case KT_K_RIGHT:
	case '\t':
		if (D.focus < B_N - 1)
			D.focus++;
		return 1;
	case KT_K_UP:
		if (D.top > 0)
			D.top--;
		return 1;
	case KT_K_DOWN:
		if (D.top + 1 < D.nfacts)
			D.top++;
		return 1;
	case '\n':
	case '\r':
		activate(D.focus);
		return 1;
	}
	return 0;
}

int res_detail_wheel(int up)
{
	if (up && D.top > 0)
		D.top--;
	else if (!up && D.top + 1 < D.nfacts)
		D.top++;
	return 1;
}

int res_detail_click(int mx, int my, int btn)
{
	int i = kch_button_at(mx, my);

	(void)btn;
	if (i >= 0 && i < B_N) {
		D.focus = i;
		activate(i);
	}
	return 1;
}
