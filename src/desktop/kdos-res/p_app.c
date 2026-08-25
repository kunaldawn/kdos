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
static int g_body = 1;		/* body rows the last draw used  */
static int g_hover = -1;	/* the row under the pointer     */
static int g_dragbar;		/* a press landed on the scrollbar */
/*
 * WHOSE MOVE THE LAST ONE WAS, and it is the load-bearing half of the wheel
 * rule. `kch_list_clamp` pulls the SELECTION into view when it is told the
 * selection is what moved — and a draw that tells it that unconditionally
 * undoes a viewport scroll on the very next frame. This table passed a hard 1,
 * so the wheel appeared to do nothing at all and the scrollbar drag that came
 * later did nothing either: both moved `g_top`, and the next draw put it back.
 * Set by everything that moves the CURSOR, cleared by everything that moves
 * the PAGE.
 */
static int g_follow = 1;
/*
 * WHERE THE LAST DRAW PUT THINGS. `g_row0` is the first LIST row, body-
 * relative — body row 0 is the column header. `g_ox`/`g_oy` are the origin the
 * frame handed the page, and they are here for one reason: the frame gives a
 * page its OWN coordinates, and libkchrome records the scrollbar in the
 * SCREEN's, so the one test that crosses that line has to add them back.
 */
static int g_row0 = 1;
static int g_ox, g_oy;
static int g_width;		/* the width the last draw had     */

/*
 * THE COLUMN HEADER IS THE SORT CONTROL, on the page whose whole question is
 * "which of these is the busiest". It was CPU and nothing else, so the answer
 * to "what is eating the disk" was to read forty rows. The order is the
 * columns' own, so the header a pointer lands on IS the key it sets — and a
 * second click on the same one reverses it, which is what every table on every
 * desktop has done since the eighties.
 */
enum { AS_NAME = 0, AS_CPU, AS_MEM, AS_DISK, AS_PROCS, AS_N };
static const char *AS_NAMES[AS_N] = { "name", "cpu", "memory", "disk",
				      "procs" };
static int g_sort = AS_CPU;
static int g_rev;

/* The column a header cell belongs to: the x each label was drawn at, and the
 * one after it. Shared by the draw and the hit test — a table whose header
 * positions are written twice is a table you click on the wrong column of. */
static const int AS_X[AS_N] = { 0, 28, 35, 45, 55 };

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
	int d = 0;

	switch (g_sort) {
	case AS_NAME:
		d = strcmp(a->name, b->name);
		break;
	case AS_MEM:
		d = a->rss < b->rss ? 1 : a->rss > b->rss ? -1 : 0;
		break;
	case AS_DISK: {
		unsigned long long ia = a->rd + a->wr, ib = b->rd + b->wr;
		d = ia < ib ? 1 : ia > ib ? -1 : 0;
		break;
	}
	case AS_PROCS:
		d = a->nproc < b->nproc ? 1 : a->nproc > b->nproc ? -1 : 0;
		break;
	case AS_CPU:
	default:
		d = a->cpu < b->cpu ? 1 : a->cpu > b->cpu ? -1 : 0;
		break;
	}
	if (!d)
		d = strcmp(a->name, b->name);
	return g_rev ? -d : d;
}

void res_app_prepare(void)
{
	/* res.conf's `sort` names a COLUMN, and it names the same columns on
	 * both tables — applied once, here, because conf.c parses the file and
	 * this is the only place that knows what these column names mean. */
	static int applied;

	if (!applied) {
		applied = 1;
		for (int i = 0; i < AS_N; i++)
			if (!strcmp(RC.sort, AS_NAMES[i])) {
				g_sort = i;
				break;
			}
	}
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

	/*
	 * The header is a row of CONTROLS, and it says which one is in force:
	 * the sorted column is drawn in the accent with the direction marker
	 * beside it. A table that sorts and gives no sign of which key it used
	 * is a table nobody can check.
	 */
	static const char *HDR[AS_N] = { "APPLICATION", "CPU%", "MEMORY",
					 "DISK", "PROCS" };
	static const int HDR_W[AS_N] = { 26, 6, 8, 8, 6 };
	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	for (int i = 0; i < AS_N; i++) {
		char lab[24];

		if ((i == AS_DISK && w < 60) || (i == AS_PROCS && w < 72))
			continue;
		snprintf(lab, sizeof(lab), "%s%s", HDR[i],
			 i == g_sort ? (g_rev ? ktui_glyph[KT_G_UP]
					      : ktui_glyph[KT_G_DOWN]) : "");
		ktui_draw_text(x + AS_X[i], row, HDR_W[i], lab,
			       i == g_sort ? KT_ACCENT : KT_MID, KT_SURFACE, 0);
	}
	row++;

	int body = bottom - row - 2;
	if (body < 1)
		return;
	g_body = body;
	kch_list_clamp(&g_top, g_sel, g_napp, body, g_follow);

	for (int i = 0; i < body && g_top + i < g_napp; i++) {
		const struct app *a = &g_app[g_top + i];
		int is_sel = (g_top + i == g_sel);
		int is_hov = !is_sel && g_top + i == g_hover;
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : is_hov ? KT_MID : KT_BG;
		int ry = row + i;
		/* A FILL, never KT_A_REVERSE over the text: the attribute
		 * inverts only the cells a glyph covers, so a row would come
		 * out as one lit block per word. */
		if (is_sel || is_hov)
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
	/* Recorded from what was DRAWN — the rule the panel's hit map keeps.
	 * A bar whose column is derived a second time is a bar you cannot
	 * grab the frame after a resize. */
	g_row0 = row - y;
	g_ox = x;
	g_oy = y;
	g_width = w;
	kch_scrollbar(0, x + w - 1, row, body, g_napp, g_top, KT_BG);

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

/* The same rule as the process table, and it has to BE the same rule: two
 * lists on one desktop that scroll differently is a hand that has to learn
 * each surface separately. */
int res_app_wheel(int up)
{
	if (!kch_list_wheel(up, &g_top, g_napp, g_body)) {
		g_follow = 1;
		if (up && g_sel > 0)
			g_sel--;
		else if (!up && g_sel + 1 < g_napp)
			g_sel++;
	} else {
		g_follow = 0;
	}
	return 1;
}

/*
 * THE GROUP VERBS, and the confirm is the feature rather than the ceremony.
 *
 * "End Firefox" is forty processes on this distro, and the one thing a person
 * has to be told before it happens is HOW MANY and WHICH BOX — an application
 * here is a container's worth of work, and unsaved state in it goes with it.
 * So the modal names the application, the box and the count, and it is the
 * same modal a single process goes through.
 */
static int g_act_kind;			/* the signal a confirmed yes sends */

static void group_signal(void)
{
	const struct app *a = g_sel >= 0 && g_sel < g_napp ? &g_app[g_sel]
							  : NULL;

	if (!a)
		return;
	for (int i = 0; i < R.sample.n; i++) {
		const KprProc *p = &R.sample.p[i];

		if (a->box[0]) {
			if (strcmp(p->box, a->box))
				continue;
		} else if (strcmp(p->comm, a->name)) {
			continue;
		}
		/* Whatever this program may not end, it says so on the detail
		 * page rather than failing quietly forty times here. */
		if (!res_act_why_disabled(p))
			res_act_signal(p, g_act_kind);
	}
}

static void ask_group(int sig, const char *verb)
{
	const struct app *a = g_sel >= 0 && g_sel < g_napp ? &g_app[g_sel]
							  : NULL;
	char msg[224];
	int n = 0;

	if (!a)
		return;
	for (int i = 0; i < R.sample.n; i++) {
		const KprProc *p = &R.sample.p[i];

		if (a->box[0] ? !strcmp(p->box, a->box)
			      : !strcmp(p->comm, a->name))
			n++;
	}
	if (!n)
		return;

	g_act_kind = sig;
	if (a->box[0])
		snprintf(msg, sizeof(msg),
			 "%s %s — %d process%s in appbox %s. Unsaved work in "
			 "them is lost.", verb, a->name, n,
			 n == 1 ? "" : "es", a->box);
	else
		snprintf(msg, sizeof(msg),
			 "%s %s — %d process%s. Unsaved work in them is lost.",
			 verb, a->name, n, n == 1 ? "" : "es");
	res_confirm(sig == 9 ? "Kill application" : "End application", msg,
		    verb, group_signal);
}

/*
 * THE POINTER, and it is the same contract the whole desktop keeps: motion
 * lights a row, a press selects it, a press on the row that is ALREADY
 * selected opens it. The second press rather than a double click because
 * nothing in this toolkit measures a double click, and because opening a
 * detail page is not destructive — the verbs live there, behind a confirm.
 */
static int row_at(int my)
{
	int i = my - g_row0 + g_top;	/* body row 0 is the header */

	if (my < g_row0 || i < 0 || i >= g_napp ||
	    my - g_row0 >= g_body)
		return -1;
	return i;
}

/* Which header cell a column belongs to: the LAST label whose x it is at or
 * past, which is what makes the whole width of a column clickable rather than
 * the letters of its name. */
static int header_at(int mx, int w)
{
	int hit = -1;

	for (int i = 0; i < AS_N; i++) {
		if ((i == AS_DISK && w < 60) || (i == AS_PROCS && w < 72))
			continue;
		if (mx >= AS_X[i])
			hit = i;
	}
	return hit;
}

void res_app_motion(int mx, int my)
{
	(void)mx;
	if (g_dragbar) {
		/* A DRAG IS A PRESS THAT IS STILL DOWN. Wayland delivers plain
		 * motion and dragged motion identically — the event carries no
		 * button state at all — so the press is what has to be
		 * remembered. */
		int t = kch_scrollbar_drag(my + g_oy);

		if (t >= 0) {
			g_top = t;
			g_follow = 0;
		}
		return;
	}
	g_hover = row_at(my);
}

void res_app_release(void)
{
	g_dragbar = 0;
}

int res_app_click(int mx, int my, int btn)
{
	(void)btn;
	if (my == 0) {
		int c = header_at(mx, g_width);

		if (c >= 0) {
			if (c == g_sort)
				g_rev = !g_rev;
			else {
				g_sort = c;
				g_rev = 0;
			}
		}
		return 1;
	}
	int t = kch_scrollbar_press(0, mx + g_ox, my + g_oy);

	if (t >= 0) {
		g_top = t;
		g_dragbar = 1;
		g_follow = 0;
		return 1;
	}
	int i = row_at(my);

	if (i < 0)
		return 1;
	g_follow = 1;
	if (i == g_sel)
		res_detail_open_app(g_app[i].name, g_app[i].box, 0);
	else
		g_sel = i;
	return 1;
}

int res_app_key(int k)
{
	if (k == 's') {
		g_sort = (g_sort + 1) % AS_N;
		return 1;
	}
	if (k == 'r') {
		g_rev = !g_rev;
		return 1;
	}
	if (k == KT_K_UP && g_sel > 0) {
		g_sel--;
		g_follow = 1;
		return 1;
	}
	if (k == KT_K_DOWN && g_sel + 1 < g_napp) {
		g_sel++;
		g_follow = 1;
		return 1;
	}
	if ((k == '\n' || k == '\r') && g_sel >= 0 && g_sel < g_napp) {
		res_detail_open_app(g_app[g_sel].name, g_app[g_sel].box, 0);
		return 1;
	}
	if (k == 'e') {
		ask_group(15, "End all");
		return 1;
	}
	if (k == 'k') {
		ask_group(9, "Kill all");
		return 1;
	}
	return 0;
}
