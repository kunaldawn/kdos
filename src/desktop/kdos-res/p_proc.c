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
 * The Processes page: flat, sortable, searchable.
 *
 * THE SELECTION FOLLOWS THE PID, NOT THE ROW. A table sorted by CPU reorders
 * under the pointer every tick, so an index selection means the verb lands on
 * whatever moved into that row — which is somebody else's process.
 *
 * A ROW THE CALLER CANNOT FULLY READ SHOWS A DASH, never 0. On a single-user
 * desktop that is root's daemons and other users' sessions, and a column of
 * zeroes there is a claim that they have done no disk io.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "res.h"

/* Column layout, computed from the width so a narrow window drops the least
 * useful column rather than overlapping two. */
struct cols { int pid, user, cpu, mem, disk, box, name; };

enum { SORT_CPU = 0, SORT_MEM, SORT_PID, SORT_NAME, SORT_DISK, SORT_N };
static const char *SORT_NAMES[SORT_N] = { "cpu", "memory", "pid", "name", "disk" };

static int g_sort = SORT_CPU;
static int g_rev;		/* the sort is descending by default     */
static int g_sel_pid = -1;	/* the SELECTION, by pid                 */
static int g_top;
static char g_search[64];
static int g_searching;

/* The rows this page decided to show, rebuilt each draw. */
static const KprProc **g_rows;
static double *g_cpu;
static int g_nrows, g_cap;
static int g_body = 1;		/* body rows the last draw used  */
static int g_hover = -1;	/* the row under the pointer     */
static int g_dragbar;		/* a press landed on the scrollbar */
/* Recorded from what the last draw ACTUALLY put on the screen — the rule the
 * panel's hit map keeps. A geometry derived a second time is a control you
 * cannot hit the frame after a resize. */
/* WHOSE MOVE THE LAST ONE WAS — see the same flag in p_app.c. A draw that
 * tells kch_list_clamp the SELECTION moved undoes a viewport scroll on the
 * very next frame, which is why the wheel here appeared to do nothing. */
static int g_follow = 1;
/* `g_row0` is the first LIST row, body-relative — body row 0 is the header.
 * `g_ox`/`g_oy` are the origin the frame handed this page: it draws in its own
 * coordinates and libkchrome records the bar in the screen's, so the one test
 * that crosses that line adds them back. */
static int g_row0 = 1;
static int g_ox, g_oy;
static struct cols g_hdr;
static int g_width;

static double cpu_of(const KprProc *p)
{
	const KprProc *prev = R.have_prev ? kpr_find_pid(&R.prev, p->pid) : NULL;
	double pc = kpr_proc_cpu(prev, p, R.sample.wall_ms - R.prev.wall_ms);
	/*
	 * Percent of ONE core by default — top's convention, where a busy
	 * 8-thread build reads 800%. Which one is in force is written in the
	 * column header, because the two differ by a factor of ncpu and both
	 * are common.
	 */
	if (RC.cpu_of_machine && R.cpu.ncpu > 0)
		pc /= (double)R.cpu.ncpu;
	return pc;
}

static int matches(const KprProc *p)
{
	if (!g_search[0])
		return 1;
	if (strcasestr(p->comm, g_search))
		return 1;
	if (p->cmdline && strcasestr(p->cmdline, g_search))
		return 1;
	if (p->box[0] && strcasestr(p->box, g_search))
		return 1;
	const char *u = kpr_user_of(p->uid);
	return u && strcasestr(u, g_search) != NULL;
}

static int cmp(const void *a, const void *b)
{
	int ia = *(const int *)a, ib = *(const int *)b;
	const KprProc *pa = g_rows[ia], *pb = g_rows[ib];
	double d = 0;

	switch (g_sort) {
	case SORT_CPU:  d = g_cpu[ib] - g_cpu[ia]; break;
	case SORT_MEM:  d = (double)pb->rss - (double)pa->rss; break;
	case SORT_PID:  d = (double)pa->pid - (double)pb->pid; break;
	case SORT_NAME: d = strcasecmp(pa->comm, pb->comm); break;
	case SORT_DISK: {
		/*
		 * Unreadable sorts LAST whichever way the column is pointed:
		 * it is an absence, and letting the sentinel's numeric value
		 * carry it to the top would fill the first screen with rows
		 * that have nothing to show.
		 */
		int ua = pa->rd_bytes == KPR_UNREADABLE;
		int ub = pb->rd_bytes == KPR_UNREADABLE;
		if (ua != ub)
			return ua ? 1 : -1;
		unsigned long long ta = ua ? 0 : pa->rd_bytes + pa->wr_bytes;
		unsigned long long tb = ub ? 0 : pb->rd_bytes + pb->wr_bytes;
		d = (double)tb - (double)ta;
		break;
	}
	}
	if (d < 0)
		return g_rev ? 1 : -1;
	if (d > 0)
		return g_rev ? -1 : 1;
	return pa->pid - pb->pid;	/* stable: pid breaks every tie */
}

static void build_rows(void)
{
	g_nrows = 0;
	if (R.sample.n > g_cap) {
		free(g_rows);
		free(g_cpu);
		g_cap = R.sample.n + 32;
		g_rows = kb_calloc((size_t)g_cap, sizeof(*g_rows));
		g_cpu = kb_calloc((size_t)g_cap, sizeof(*g_cpu));
	}
	if (!g_rows)
		return;

	for (int i = 0; i < R.sample.n; i++) {
		const KprProc *p = &R.sample.p[i];
		/*
		 * Kernel threads are hidden by default and COUNTED: a table
		 * that silently omits half of /proc is one that gets argued
		 * with, so the footer says how many are missing.
		 */
		if (!RC.kernel_threads && (p->ppid == 2 || p->pid == 2))
			continue;
		if (!matches(p))
			continue;
		g_rows[g_nrows] = p;
		g_cpu[g_nrows] = cpu_of(p);
		g_nrows++;
	}

	int *idx = kb_calloc((size_t)(g_nrows ? g_nrows : 1), sizeof(int));
	for (int i = 0; i < g_nrows; i++)
		idx[i] = i;
	qsort(idx, (size_t)g_nrows, sizeof(int), cmp);

	const KprProc **rs = kb_calloc((size_t)(g_nrows ? g_nrows : 1), sizeof(*rs));
	double *cs = kb_calloc((size_t)(g_nrows ? g_nrows : 1), sizeof(*cs));
	for (int i = 0; i < g_nrows; i++) {
		rs[i] = g_rows[idx[i]];
		cs[i] = g_cpu[idx[i]];
	}
	memcpy(g_rows, rs, (size_t)g_nrows * sizeof(*rs));
	memcpy(g_cpu, cs, (size_t)g_nrows * sizeof(*cs));
	free(rs);
	free(cs);
	free(idx);
}

static int sel_index(void)
{
	for (int i = 0; i < g_nrows; i++)
		if (g_rows[i]->pid == g_sel_pid)
			return i;
	return g_nrows ? 0 : -1;
}

const char *res_proc_headline(void)
{
	static char s[96];
	snprintf(s, sizeof(s), "%d shown%s%s", g_nrows,
		 g_search[0] ? ", filtered by " : "",
		 g_search[0] ? g_search : "");
	return s;
}

static struct cols layout(int w)
{
	struct cols c = { 0 };
	c.pid = 0;
	c.user = 8;
	c.cpu = w >= 60 ? 18 : 16;
	c.mem = c.cpu + 7;
	c.disk = w >= 92 ? c.mem + 9 : -1;
	c.box = w >= 78 ? (c.disk > 0 ? c.disk + 11 : c.mem + 9) : -1;
	int after = c.box > 0 ? c.box + 13
		    : c.disk > 0 ? c.disk + 11 : c.mem + 9;
	c.name = after;
	return c;
}

void res_procs_prepare(void)
{
	/*
	 * res.conf's `sort` is applied once, here rather than at load: conf.c
	 * parses the file and this is the only place that knows what the
	 * column names mean.
	 */
	static int applied;
	if (!applied) {
		applied = 1;
		for (int i = 0; i < SORT_N; i++)
			if (!strcmp(RC.sort, SORT_NAMES[i])) {
				g_sort = i;
				break;
			}
	}
	build_rows();
}

/* One header cell: the label, plus the direction marker when this is the key
 * the table is sorted on. */
static void hdr_cell(int x, int y, int w, const char *label, int key)
{
	char buf[24];

	snprintf(buf, sizeof(buf), "%s%s", label,
		 key == g_sort ? (g_rev ? ktui_glyph[KT_G_UP]
					: ktui_glyph[KT_G_DOWN]) : "");
	ktui_draw_text(x, y, w, buf, key == g_sort ? KT_ACCENT : KT_MID,
		       KT_SURFACE, 0);
}

void res_draw_procs(int x, int y, int w, int h)
{
	const int bottom = y + h;

	struct cols c = layout(w);
	int row = y;

	/*
	 * The header row is the sort control, and it SAYS which key is in
	 * force: the sorted column in the accent with the direction beside it.
	 * It always was the control — `s` cycled it — and it never looked like
	 * one, so the only way to sort by memory was to read the manual.
	 */
	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	hdr_cell(x + c.pid, row, 7, "PID", SORT_PID);
	ktui_draw_text(x + c.user, row, 9, "USER", KT_MID, KT_SURFACE, 0);
	hdr_cell(x + c.cpu, row, 6, RC.cpu_of_machine ? "CPU%m" : "CPU%",
		 SORT_CPU);
	hdr_cell(x + c.mem, row, 8, "MEM", SORT_MEM);
	if (c.disk > 0)
		hdr_cell(x + c.disk, row, 10, "DISK", SORT_DISK);
	if (c.box > 0)
		ktui_draw_text(x + c.box, row, 12, "BOX", KT_MID, KT_SURFACE, 0);
	hdr_cell(x + c.name, row, w - c.name, "NAME", SORT_NAME);
	row++;
	g_hdr = c;
	g_width = w;

	int body = bottom - row - 1;
	if (body < 1)
		return;
	/* The wheel is answered against the rows this frame actually drew, the
	 * same rule the panel's hit map keeps: a height derived twice is a
	 * height that disagrees with itself the frame the window is resized. */
	g_body = body;

	int sel = sel_index();
	if (sel >= 0)
		g_sel_pid = g_rows[sel]->pid;
	kch_list_clamp(&g_top, sel, g_nrows, body, g_follow);

	for (int i = 0; i < body && g_top + i < g_nrows; i++) {
		const KprProc *p = g_rows[g_top + i];
		int is_sel = (g_top + i == sel);
		int is_hov = !is_sel && g_top + i == g_hover;
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : is_hov ? KT_MID : KT_BG;
		int ry = row + i;

		if (is_sel || is_hov)
			ktui_draw_fill(krect(x, ry, w, 1), bg);

		char buf[64];
		snprintf(buf, sizeof(buf), "%d", p->pid);
		ktui_draw_text(x + c.pid, ry, 7, buf, fg, bg, 0);
		ktui_draw_text(x + c.user, ry, 9, kpr_user_of(p->uid), fg, bg, 0);
		snprintf(buf, sizeof(buf), "%.1f", g_cpu[g_top + i]);
		ktui_draw_text(x + c.cpu, ry, 6, buf, fg, bg, 0);
		ktui_draw_text(x + c.mem, ry, 8, res_size(p->rss), fg, bg, 0);
		if (c.disk > 0) {
			/* An unreadable pair is one dash, not "0 / 0". */
			if (p->rd_bytes == KPR_UNREADABLE)
				snprintf(buf, sizeof(buf), "%s", res_none());
			else
				snprintf(buf, sizeof(buf), "%s",
					 res_size(p->rd_bytes + p->wr_bytes));
			ktui_draw_text(x + c.disk, ry, 10, buf, fg, bg, 0);
		}
		if (c.box > 0)
			ktui_draw_text(x + c.box, ry, 12,
				       p->box[0] ? p->box : "", fg, bg, 0);
		ktui_draw_text(x + c.name, ry, w - c.name, p->comm, fg, bg, 0);
	}

	g_row0 = row - y;
	g_ox = x;
	g_oy = y;
	kch_scrollbar(0, x + w - 1, row, body, g_nrows, g_top, KT_BG);

	/* The footer says what is NOT on screen. */
	char foot[160];
	if (g_searching)
		snprintf(foot, sizeof(foot), "search: %s_", g_search);
	else
		snprintf(foot, sizeof(foot),
			 "%d hidden kernel thread%s  %s  sort: %s%s",
			 R.sample.nkthread, R.sample.nkthread == 1 ? "" : "s",
			 ktui_glyph[KT_G_DOT], SORT_NAMES[g_sort],
			 g_rev ? " (reversed)" : "");
	ktui_draw_text(x, bottom - 1, w, foot, KT_DIM, KT_BG, 0);
}

int res_procs_key(int k)
{
	if (g_searching) {
		if (k == KT_K_ESC) {
			g_searching = 0;
			g_search[0] = 0;
			return 1;
		}
		if (k == KT_K_ENTER) {
			g_searching = 0;
			return 1;
		}
		if (k == KT_K_BACKSPACE) {
			size_t n = strlen(g_search);
			if (n)
				g_search[n - 1] = 0;
			return 1;
		}
		if (k >= 32 && k < 127) {
			size_t n = strlen(g_search);
			if (n + 1 < sizeof(g_search)) {
				g_search[n] = (char)k;
				g_search[n + 1] = 0;
			}
			return 1;
		}
		return 1;
	}

	int sel = sel_index();
	switch (k) {
	case '/':
		g_searching = 1;
		return 1;
	case '\n':
	case '\r':
		/*
		 * The verbs are the DETAIL page's, not this one's. A key that
		 * ended a process from a table would be a key pressed while
		 * the cursor happens to be on a row, with nothing on the
		 * screen saying which row that is.
		 */
		if (sel >= 0)
			res_detail_open_proc(g_rows[sel]->pid);
		return 1;
	case KT_K_UP:
		g_follow = 1;
		if (sel > 0)
			g_sel_pid = g_rows[sel - 1]->pid;
		return 1;
	case KT_K_DOWN:
		g_follow = 1;
		if (sel >= 0 && sel + 1 < g_nrows)
			g_sel_pid = g_rows[sel + 1]->pid;
		return 1;
	case 's':
		g_sort = (g_sort + 1) % SORT_N;
		return 1;
	case 'r':
		g_rev = !g_rev;
		return 1;
	}
	return 0;
}

/*
 * SCROLL THE PAGE WHILE THE LIST OVERFLOWS, STEP THE CURSOR WHILE IT FITS —
 * kch_list_wheel is that rule and is the only place it is written. A process
 * table is the longest list on this machine and was the one surface here the
 * wheel did nothing on.
 */
int res_procs_wheel(int up)
{
	if (!kch_list_wheel(up, &g_top, g_nrows, g_body)) {
		int sel = sel_index();

		g_follow = 1;
		if (up && sel > 0)
			g_sel_pid = g_rows[sel - 1]->pid;
		else if (!up && sel >= 0 && sel + 1 < g_nrows)
			g_sel_pid = g_rows[sel + 1]->pid;
	} else {
		g_follow = 0;
	}
	return 1;
}

/*
 * THE POINTER. Motion lights a row, a press selects it, a press on the row
 * that is ALREADY selected opens its detail page — the same contract the
 * Applications table keeps, because a hand that has learnt one of these two
 * tables has learnt both.
 */
static int row_at(int my)
{
	int i = my - g_row0 + g_top;

	if (my < g_row0 || my - g_row0 >= g_body || i < 0 ||
	    i >= g_nrows)
		return -1;
	return i;
}

/* Which sort key a header column belongs to, by the x it was drawn at. */
static int header_at(int mx)
{
	const struct cols *c = &g_hdr;

	if (c->name > 0 && mx >= c->name)
		return SORT_NAME;
	if (c->box > 0 && mx >= c->box)
		return -1;		/* BOX is not a sort key */
	if (c->disk > 0 && mx >= c->disk)
		return SORT_DISK;
	if (mx >= c->mem)
		return SORT_MEM;
	if (mx >= c->cpu)
		return SORT_CPU;
	if (mx >= c->user)
		return -1;		/* USER is not a sort key */
	return SORT_PID;
}

void res_procs_motion(int mx, int my)
{
	if (g_dragbar) {
		/* A DRAG IS A PRESS THAT IS STILL DOWN. Wayland delivers plain
		 * motion and dragged motion identically, so the press is what
		 * has to be remembered. */
		int t = kch_scrollbar_drag(my + g_oy);

		if (t >= 0) {
			g_top = t;
			g_follow = 0;
		}
		return;
	}
	(void)mx;
	g_hover = row_at(my);
}

void res_procs_release(void)
{
	g_dragbar = 0;
}

int res_procs_click(int mx, int my, int btn)
{
	(void)btn;
	if (my == 0) {
		int k = header_at(mx);

		if (k >= 0) {
			if (k == g_sort)
				g_rev = !g_rev;
			else {
				g_sort = k;
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
	if (g_rows[i]->pid == g_sel_pid)
		res_detail_open_proc(g_rows[i]->pid);
	else
		g_sel_pid = g_rows[i]->pid;
	return 1;
}
