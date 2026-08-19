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
 * The Applications page — the rollup that is the reason this program exists.
 *
 * One row per APPLICATION, not per process. On this distro every fat
 * application is its own podman container whose supervisor knows its name, so
 * the identity half — the part no other Linux monitor can do without systemd
 * cgroup delegation — is already answered by libkproc's conmon walk.
 *
 * GROUPING, first match wins:
 *   1. the conmon box
 *   2. the executable's basename against the desktop-entry index
 *   3. nothing. An unmatched process is NOT invented into an application: it
 *      stays in Processes and the footer says how much CPU is outside the
 *      rollup, the way kdos-energy names its short-lived residue.
 *
 * IDENTITY DOES NOT COME FROM WAYLAND. Binding wlr-foreign-toplevel would name
 * windows beautifully and would make the TTY and the window two different
 * programs. Refused, not overlooked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "res.h"

#define APP_MAX 128

struct app {
	char name[64];
	char box[64];
	int boxed;
	int nproc;
	double cpu;
	char lead[24];		/* the busiest member, which names the row */
	double lead_cpu;
	unsigned long long rss, gpu_ns;
	unsigned long long rd, wr;
	int io_partial;		/* at least one member's io was unreadable */
};

static struct app g_app[APP_MAX];
static int g_napp;
static double g_unmatched_cpu;
static int g_sel;
static int g_top;

static struct app *find_app(const char *name)
{
	for (int i = 0; i < g_napp; i++)
		if (!strcmp(g_app[i].name, name))
			return &g_app[i];
	if (g_napp >= APP_MAX)
		return NULL;
	struct app *a = &g_app[g_napp++];
	memset(a, 0, sizeof(*a));
	kb_strlcpy(a->name, name, sizeof(a->name));
	return a;
}

static int cmp_app(const void *x, const void *y)
{
	const struct app *a = x, *b = y;
	if (a->cpu < b->cpu)
		return 1;
	if (a->cpu > b->cpu)
		return -1;
	return strcmp(a->name, b->name);
}

void res_app_prepare(void)
{
	g_napp = 0;
	g_unmatched_cpu = 0.0;

	for (int i = 0; i < R.sample.n; i++) {
		const KprProc *p = &R.sample.p[i];
		if (p->ppid == 2 || p->pid == 2)
			continue;		/* kernel threads are not apps */

		const KprProc *prev = R.have_prev
				      ? kpr_find_pid(&R.prev, p->pid) : NULL;
		double cpu = kpr_proc_cpu(prev, p,
					  R.sample.wall_ms - R.prev.wall_ms);

		struct app *a = NULL;
		if (p->box[0]) {
			a = find_app(p->box);
			if (a) {
				a->boxed = 1;
				kb_strlcpy(a->box, p->box, sizeof(a->box));
				/*
				 * The row is named for the APPLICATION and
				 * qualified by its box — kdos-energy's
				 * `firefox-esr (appbox kdos-apps)` shape. The
				 * busiest member wins the name, so a browser's
				 * forty helpers do not rename it: conmon is
				 * outside the box and never a candidate.
				 */
				if (cpu > a->lead_cpu || !a->lead[0]) {
					a->lead_cpu = cpu;
					kb_strlcpy(a->lead, p->comm,
						   sizeof(a->lead));
				}
			}
		} else if (p->pid > 1 && p->comm[0]) {
			/*
			 * A host process is grouped by its own comm. That is
			 * deliberately weaker than the box case: without a
			 * desktop entry to consult there is no better name,
			 * and inventing one would be a row nobody can act on.
			 */
			a = find_app(p->comm);
		}

		if (!a) {
			g_unmatched_cpu += cpu;
			continue;
		}
		a->nproc++;
		a->cpu += cpu;
		a->rss += p->rss;
		a->gpu_ns += p->gpu_ns;
		if (p->rd_bytes == KPR_UNREADABLE)
			a->io_partial = 1;
		else {
			a->rd += p->rd_bytes;
			a->wr += p->wr_bytes;
		}
	}
	qsort(g_app, (size_t)g_napp, sizeof(g_app[0]), cmp_app);
	if (g_sel >= g_napp)
		g_sel = g_napp ? g_napp - 1 : 0;
}

const char *res_app_headline(void)
{
	static char s[96];
	snprintf(s, sizeof(s), "%d application%s", g_napp,
		 g_napp == 1 ? "" : "s");
	return s;
}

void res_draw_apps(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;

	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	ktui_draw_text(x, row, 26, "APPLICATION", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 28, row, 6, "CPU%", KT_MID, KT_SURFACE, 0);
	ktui_draw_text(x + 35, row, 8, "MEMORY", KT_MID, KT_SURFACE, 0);
	if (w >= 60)
		ktui_draw_text(x + 45, row, 8, "DISK", KT_MID, KT_SURFACE, 0);
	if (w >= 72)
		ktui_draw_text(x + 55, row, 6, "PROCS", KT_MID, KT_SURFACE, 0);
	row++;

	int body = bottom - row - 2;
	if (body < 1)
		return;
	kch_list_clamp(&g_top, g_sel, g_napp, body, 1);

	for (int i = 0; i < body && g_top + i < g_napp; i++) {
		const struct app *a = &g_app[g_top + i];
		int is_sel = (g_top + i == g_sel);
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : KT_BG;
		int ry = row + i;
		if (is_sel)
			ktui_draw_fill(krect(x, ry, w, 1), bg);

		char nm[80];
		/*
		 * `[box]` is the same mark the Start menu uses and it is here
		 * for the same reason: a boxed application costs a container
		 * start on its first launch, and somebody is entitled to know
		 * which those are before they act on the row.
		 */
		if (a->boxed && a->lead[0])
			snprintf(nm, sizeof(nm), "%s [box %s]", a->lead, a->box);
		else if (a->boxed)
			snprintf(nm, sizeof(nm), "%s [box]", a->name);
		else
			snprintf(nm, sizeof(nm), "%s", a->name);
		ktui_draw_text(x, ry, 27, nm, fg, bg, 0);

		char buf[32];
		snprintf(buf, sizeof(buf), "%.1f", a->cpu);
		ktui_draw_text(x + 28, ry, 6, buf, fg, bg, 0);
		ktui_draw_text(x + 35, ry, 9, res_size(a->rss), fg, bg, 0);
		if (w >= 60) {
			/* A partial sum is marked, not silently short. */
			snprintf(buf, sizeof(buf), "%s%s",
				 res_size(a->rd + a->wr),
				 a->io_partial ? "+" : "");
			ktui_draw_text(x + 45, ry, 9, buf, fg, bg, 0);
		}
		if (w >= 72) {
			snprintf(buf, sizeof(buf), "%d", a->nproc);
			ktui_draw_text(x + 55, ry, 6, buf, fg, bg, 0);
		}
	}
	kch_list_scrollbar(x + w - 1, row, body, g_napp, g_top, KT_BG);

	/*
	 * The residue. Naming it is the difference between a rollup and a
	 * fabrication: CPU spent by processes this page could not group has to
	 * go somewhere the reader can see.
	 */
	char foot[160];
	snprintf(foot, sizeof(foot),
		 "%.1f%% CPU outside the rollup  %s  memory is RSS and "
		 "over-counts shared pages", g_unmatched_cpu,
		 ktui_glyph[KT_G_DOT]);
	ktui_draw_text(x, bottom - 1, w, foot, KT_DIM, KT_BG, 0);
}

int res_app_key(int k)
{
	if (k == KT_K_UP && g_sel > 0) {
		g_sel--;
		return 1;
	}
	if (k == KT_K_DOWN && g_sel + 1 < g_napp) {
		g_sel++;
		return 1;
	}
	return 0;
}
