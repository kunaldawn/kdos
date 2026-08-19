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
 * One tick: what gets read, and what does not.
 *
 * A MONITOR THAT IS THE LOAD IS A BUG. The visible page decides the flags, so
 * a device page walks no per-process files at all and the GPU column's fd/
 * readdir happens only while a GPU column is on screen.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

unsigned res_wanted_flags(void)
{
	/*
	 * The detail page's needs are ORed in rather than switched on: it is
	 * an overlay, so the subject can have been opened from a page that
	 * asks for nothing per-process, and a subject with no cmdline is a
	 * detail view that cannot say what it is looking at.
	 */
	unsigned extra = res_detail_wants();

	switch (res_page_current()) {
	case RP_PROCESSES:
		return KPR_WANT_STATUS | KPR_WANT_IO | KPR_WANT_CMDLINE |
		       KPR_WANT_BOX | extra;
	case RP_APPLICATIONS:
		return KPR_WANT_STATUS | KPR_WANT_IO | KPR_WANT_CMDLINE |
		       KPR_WANT_BOX | extra;
	case RP_GPU:
		return KPR_WANT_STATUS | KPR_WANT_GPU | KPR_WANT_BOX | extra;
	default:
		/*
		 * Zero: `stat` only. A CPU, memory, drive, network or battery
		 * page needs no per-process file, and reading them anyway is
		 * how a monitor with nine pages costs nine pages of io on
		 * every one of them.
		 */
		return extra;
	}
}

static void hist_alloc(void)
{
	if (R.h_core || R.cpu.ncpu <= 0)
		return;
	R.h_core = kb_calloc((size_t)R.cpu.ncpu, sizeof(*R.h_core));
	for (int i = 0; i < R.cpu.ncpu; i++)
		kpr_hist_init(&R.h_core[i], 1);
}

void res_sample(void)
{
	static int inited;
	if (!inited) {
		kpr_hist_init(&R.h_cpu, 1);
		kpr_hist_init(&R.h_mem, 1);
		kpr_hist_init(&R.h_swap, 1);
		inited = 1;
	}

	/* CPU: keep the previous times, because busy is over the delta. */
	if (R.cpu.ncpu) {
		R.cpu_prev = R.cpu;
		R.cpu_prev.per = NULL;	/* the arrays belong to R.cpu   */
		R.cpu_prev.khz = NULL;
		R.cpu_have_prev = 1;
	}
	KprCpu fresh;
	if (kpr_cpu_read(&fresh) == 0) {
		KprCpuTimes prev_total = R.cpu.total;
		int had = R.cpu.ncpu != 0;
		KprCpuTimes *prev_per = R.cpu.per;
		int prev_n = R.cpu.ncpu;

		if (had) {
			double busy = kpr_cpu_busy(&prev_total, &fresh.total);
			kpr_hist_push(&R.h_cpu, busy * 100.0);
		}
		if (had && prev_per && R.h_core)
			for (int i = 0; i < fresh.ncpu && i < prev_n; i++)
				kpr_hist_push(&R.h_core[i],
					kpr_cpu_busy(&prev_per[i],
						     &fresh.per[i]) * 100.0);

		kpr_cpu_free(&R.cpu);
		R.cpu = fresh;
		hist_alloc();
	}

	if (kpr_mem_read(&R.mem) == 0 && R.mem.total) {
		double used = (double)(R.mem.total - R.mem.available);
		kpr_hist_push(&R.h_mem, used / (double)R.mem.total * 100.0);
		if (R.mem.swap_total) {
			double sw = (double)(R.mem.swap_total - R.mem.swap_free);
			kpr_hist_push(&R.h_swap,
				      sw / (double)R.mem.swap_total * 100.0);
		} else {
			kpr_hist_push(&R.h_swap, 0.0);
		}
	}

	kpr_psi_read("cpu", &R.psi_cpu);
	kpr_psi_read("memory", &R.psi_mem);
	kpr_psi_read("io", &R.psi_io);

	unsigned want = res_wanted_flags();
	if (want) {
		/*
		 * A PAGE THAT DOES NOT WALK PROCESSES LEAVES THE LAST WALK
		 * BEHIND, and it is not comparable when the user comes back:
		 * the previous sample was taken minutes ago on another page,
		 * so a rate computed against it is spread over that whole
		 * gap. It is also not comparable when the FLAGS changed, since
		 * the fields the new page reads may never have been filled.
		 *
		 * Either way the first tick back reports no rate and the
		 * second reports a correct one, which is the same rule
		 * kpr_proc_cpu already applies to a process it has not seen
		 * before.
		 */
		if (R.sample_flags != want)
			R.have_prev = 0;

		kpr_sample_free(&R.prev);
		R.prev = R.sample;
		memset(&R.sample, 0, sizeof(R.sample));
		if (R.prev.n > 0 && R.sample_flags == want)
			R.have_prev = 1;
		if (kpr_sample_take(&R.sample, want) == 0) {
			/*
			 * A FIXTURE'S CLOCK IS ITS OWN. Its two snapshots are
			 * one sample interval apart by construction, but a
			 * dump takes them microseconds apart by the monotonic
			 * clock — so a rate divided by the real elapsed time
			 * comes out thousands of times too large. Stamping the
			 * declared interval is what makes a recorded machine
			 * produce the numbers it recorded.
			 */
			if (R.fixture)
				R.sample.wall_ms = R.tick *
					(unsigned long long)RC.interval_ms;
			R.have_prev = R.have_prev && R.prev.n > 0;
		}
		R.sample_flags = want;
	}

	/* The subject's own rings advance on the program's clock, not on how
	 * often its page happens to be drawn — and the same is true of the
	 * device rates, which is why neither lives in a prepare(). */
	res_detail_sample();
	res_dev_sample();

	R.tick++;
}

/* ── the shared renderings ───────────────────────────────────────────── */

const char *res_size(unsigned long long bytes)
{
	static char buf[32];
	if (bytes == KPR_UNREADABLE)
		return res_none();
	if (RC.units_1024)
		return kb_human_size(bytes);

	static const char *u[] = { "B", "kB", "MB", "GB", "TB", "PB" };
	double v = (double)bytes;
	int i = 0;
	while (v >= 1000.0 && i < 5) {
		v /= 1000.0;
		i++;
	}
	snprintf(buf, sizeof(buf), i ? "%.1f%s" : "%.0f%s", v, u[i]);
	return buf;
}

const char *res_temp(double c)
{
	static char buf[24];
	if (c < 0.0)
		return res_none();	/* no sensor is not 0 degrees */
	/*
	 * The degree sign comes from the glyph table, not from a literal: the
	 * ASCII tier has no such codepoint and a literal renders as '?' on
	 * tty1 and in every golden.
	 */
	if (RC.fahrenheit)
		snprintf(buf, sizeof(buf), "%.0f%sF",
			 c * 9.0 / 5.0 + 32.0, ktui_glyph[KT_G_DEG]);
	else
		snprintf(buf, sizeof(buf), "%.0f%sC", c, ktui_glyph[KT_G_DEG]);
	return buf;
}

/*
 * The one rendering of a counter the caller may not read. An em dash, never a
 * zero: a column of confident zeroes is worse than an empty column, because
 * somebody will act on it.
 */
const char *res_counter(unsigned long long v)
{
	if (v == KPR_UNREADABLE)
		return res_none();
	return res_size(v);
}

/*
 * "No reading", and it is a dash rather than an em dash for the same reason
 * the degree sign is not a literal: the ASCII tier the goldens and tty1 are
 * rendered at has no em dash, and an unreadable counter that prints as '?'
 * reads as a bug rather than as an absence.
 */
const char *res_none(void)
{
	return "-";
}
