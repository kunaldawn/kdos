/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — view geometry and log classification
 *
 * The parts of the screens that are DECISIONS rather than drawing, so they can
 * be asserted without a terminal. `kdosbuild --selftest` runs what is here.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdosbuild.h"

/* The order regions are given up in as the terminal shrinks. Chosen so the
 * thing being watched survives longest: telemetry goes before the log, the
 * log before the tree, and the tree is never given up. The HUD is six rows —
 * boxed panels around graphs of sixteen braille dot levels — from 26 rows,
 * where the tree still has fourteen; one unboxed row from 16; none below. */
Layout layout_compute(int w, int h)
{
	Layout l;
	memset(&l, 0, sizeof(l));
	if (w < 40 || h < 10) {
		l.too_small = 1;
		return l;
	}

	l.hud_rows = h >= 26 ? HUD_BOXED_ROWS : (h >= 16 ? 1 : 0);
	l.has_header = h >= 12;
	l.has_detail = w >= 70;

	int top = 1;			/* inside the frame              */
	if (l.has_header) {
		l.header = krect(1, top, w - 2, 2);
		top += 3;		/* two rows plus the rule        */
	}

	l.footer = krect(1, h - 2, w - 2, 1);
	int bottom = l.footer.y - 1;
	if (l.hud_rows) {
		l.hud = krect(1, bottom - l.hud_rows + 1, w - 2, l.hud_rows);
		bottom = l.hud.y - 1;
	}

	int rows = bottom - top + 1;
	if (rows < 1)
		rows = 1;

	if (l.has_detail) {
		int tw = w * 40 / 100;
		if (tw < 26)
			tw = 26;
		if (tw > 48)
			tw = 48;
		l.tree = krect(1, top, tw, rows);
		l.divider = krect(1 + tw, top, 1, rows);
		l.detail = krect(2 + tw, top, w - tw - 3, rows);
	} else {
		l.tree = krect(1, top, w - 2, rows);
	}
	return l;
}

/* Each caption's widest line: CPU's "load 123.45", memory's "free 1023.9G",
 * output's "out 999,999/s". */
int hud_caption_w(int which)
{
	static const int W[HUD_NGRAPH] = { 11, 12, 13 };
	return which >= 0 && which < HUD_NGRAPH ? W[which] : 0;
}

#define HUD_GAP 2		/* between unboxed panels                   */
#define HUD_BOX_GAP 1		/* between boxes                            */
#define HUD_INFO_MAX 80

/* What a panel adds to its content: unboxed, the gap before every panel but
 * the first; boxed, the gap before every box but the first, its two border
 * columns and one blank column inside the right border. */
static int hud_frame(int i, int boxed)
{
	if (!boxed)
		return i ? HUD_GAP : 0;
	return (i ? HUD_BOX_GAP : 0) + 3;
}

/* Columns `k` graphs at their minimum take, with the info panel when `info`. */
static int hud_need(int k, int info, int boxed)
{
	int n = 0;
	for (int i = 0; i < k; i++)
		n += hud_frame(i, boxed) + HUD_GRAPH_MIN + 1 + hud_caption_w(i);
	if (info)
		n += hud_frame(k, boxed) + HUD_INFO_MIN;
	return n;
}

/* What is left once every panel has its minimum goes half to the info panel,
 * up to HUD_INFO_MAX, and the rest to the graphs in equal shares, a
 * graph never wider than its history; the info panel then takes whatever is
 * left, so the panels end exactly at the HUD's right edge. */
HudLayout hud_compute(KRect r)
{
	HudLayout l;
	memset(&l, 0, sizeof(l));
	int boxed = r.h >= HUD_BOXED_ROWS;
	int k = 0;
	while (k < HUD_NGRAPH && hud_need(k + 1, 0, boxed) <= r.w)
		k++;
	int info = k == HUD_NGRAPH && hud_need(k, 1, boxed) <= r.w;
	if (!k)
		return l;
	l.boxed = boxed;

	int extra = r.w - hud_need(k, info, boxed);
	int info_w = 0;
	if (info) {
		info_w = extra / 2;
		if (info_w > HUD_INFO_MAX - HUD_INFO_MIN)
			info_w = HUD_INFO_MAX - HUD_INFO_MIN;
		extra -= info_w;
		info_w += HUD_INFO_MIN;
	}
	int gw[HUD_NGRAPH];
	for (int i = 0; i < k; i++) {
		gw[i] = HUD_GRAPH_MIN + extra / k + (i < extra % k ? 1 : 0);
		if (gw[i] > KB_HIST / 2)
			gw[i] = KB_HIST / 2;
	}

	/* Inside a box the content starts one column and one row in, and is
	 * the box's height less its two border rows. */
	int in = boxed ? 1 : 0, cy = r.y + in, ch = r.h - 2 * in;
	int x = r.x;
	for (int i = 0; i < k; i++) {
		x += boxed ? (i ? HUD_BOX_GAP : 0) : (i ? HUD_GAP : 0);
		int bx = x;
		x += in;
		l.graph[i] = krect(x, cy, gw[i], ch);
		x += gw[i] + 1;
		l.caption[i] = krect(x, cy, hud_caption_w(i), ch);
		x += hud_caption_w(i);
		if (boxed) {
			x += 2;		/* the blank column and the border */
			l.box[i] = krect(bx, r.y, x - bx, r.h);
		}
	}
	if (info) {
		x += boxed ? HUD_BOX_GAP : HUD_GAP;
		int end = r.x + r.w;
		l.info = krect(x + in, cy, end - x - 3 * in, ch);
		if (boxed)
			l.box[HUD_INFO] = krect(x, r.y, end - x, r.h);
	}
	return l;
}

/* The classifier is deliberately word-anchored. A configure run prints
 * "checking for error_at_line" hundreds of times and colouring all of it red
 * makes the pane useless exactly when it matters. */
static int has_word(const char *hay, const char *word)
{
	size_t n = strlen(word);
	for (const char *p = hay; (p = strstr(p, word)) != NULL; p += n) {
		char before = p == hay ? ' ' : p[-1];
		char after = p[n];
		if (!isalnum((unsigned char)before) && before != '_' &&
		    !isalnum((unsigned char)after) && after != '_')
			return 1;
	}
	return 0;
}

int log_severity(const char *line)
{
	if (!line || !*line)
		return LOG_PLAIN;
	if (strstr(line, "error:") || strstr(line, "ERROR:") ||
	    strstr(line, "undefined reference") || strstr(line, "FAILED") ||
	    strstr(line, "No such file") || has_word(line, "Error"))
		return LOG_ERR;
	if (strstr(line, "warning:") || has_word(line, "WARNING") ||
	    has_word(line, "Warning"))
		return LOG_WARN;
	return LOG_PLAIN;
}

int log_find(const char *hay, const char *needle)
{
	if (!hay || !needle || !*needle)
		return -1;
	size_t n = strlen(needle);
	for (const char *p = hay; *p; p++) {
		size_t i = 0;
		while (i < n && p[i] &&
		       tolower((unsigned char)p[i]) ==
			       tolower((unsigned char)needle[i]))
			i++;
		if (i == n)
			return (int)(p - hay);
	}
	return -1;
}

static int vfail;

static void vok(int cond, const char *what)
{
	if (!cond) {
		vfail++;
		printf("  FAIL  %s\n", what);
	}
}

static int overlaps(KRect a, KRect b)
{
	if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0)
		return 0;
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
	       b.y < a.y + a.h;
}

int view_selftest(void)
{
	vfail = 0;

	/* No region may ever be drawn over another, at any size the terminal
	 * can be. This is the assertion, not the instance: a divider landing
	 * on the log text is one size out of this range. */
	int bad_size = 0;
	/* A region left at zero size passes both checks above silently — it
	 * neither overlaps anything nor leaves the screen. So a region that is
	 * supposed to be ACTIVE at this size (its governing flag says so) is
	 * also checked for positive width and height, or a layout change that
	 * forgets to size one passes this whole test. */
	int bad_zero = 0;
	for (int w = 40; w <= 300; w++)
		for (int h = 10; h <= 100; h++) {
			Layout l = layout_compute(w, h);
			if (l.too_small)
				continue;
			KRect r[] = { l.header, l.tree, l.divider, l.detail,
				      l.hud, l.footer };
			for (int i = 0; i < 6; i++) {
				if (r[i].x < 0 || r[i].y < 0 ||
				    r[i].x + r[i].w > w || r[i].y + r[i].h > h)
					bad_size = 1;
				for (int k = i + 1; k < 6; k++)
					if (overlaps(r[i], r[k]))
						bad_size = 1;
			}
			int active[] = { l.has_header, 1, l.has_detail,
					 l.has_detail, l.hud_rows, 1 };
			for (int i = 0; i < 6; i++)
				if (active[i] && (r[i].w <= 0 || r[i].h <= 0))
					bad_zero = 1;
		}
	vok(!bad_size, "no two regions overlap and none leaves the screen");
	vok(!bad_zero, "every active region has positive width and height");

	/* The drop order. Each of these is the first size at which the region
	 * above it goes. */
	vok(layout_compute(100, 9).too_small, "below 10 rows is too small");
	vok(layout_compute(39, 30).too_small, "below 40 columns is too small");
	vok(layout_compute(100, 30).hud_rows == HUD_BOXED_ROWS,
	    "a tall terminal has the boxed hud");
	vok(layout_compute(100, 26).hud_rows == HUD_BOXED_ROWS, "26 rows still has it");
	vok(layout_compute(100, 26).tree.h >= 12, "and leaves the tree a dozen rows");
	vok(layout_compute(100, 25).hud_rows == 1, "below 26 the hud is one row");
	vok(layout_compute(100, 18).hud_rows == 1, "a shorter one keeps one");
	vok(layout_compute(100, 14).hud_rows == 0, "a short one drops the hud");

	/* The HUD's panels, over every width and both heights: boxes inside the
	 * HUD and apart, each content inside its own box's border (or, with no
	 * boxes, on the HUD's rows), contents apart, each at least its minimum,
	 * every caption at its own width, and given up in order — info first,
	 * CPU last. */
	int bad_hud = 0, bad_order = 0, bad_box = 0;
	for (int w = 40; w <= 600; w++)
		for (int h = 1; h <= HUD_BOXED_ROWS; h += HUD_BOXED_ROWS - 1) {
			KRect r = krect(1, 5, w - 2, h);
			HudLayout hl = hud_compute(r);
			KRect all[2 * HUD_NGRAPH + 1];
			int owner[2 * HUD_NGRAPH + 1];
			int n = 0;
			for (int i = 0; i < HUD_NGRAPH; i++) {
				if (!hl.graph[i].w) {
					if (hl.caption[i].w || hl.box[i].w)
						bad_hud = 1;
					continue;
				}
				if (hl.graph[i].w < HUD_GRAPH_MIN ||
				    hl.graph[i].w > KB_HIST / 2 ||
				    hl.caption[i].w != hud_caption_w(i))
					bad_hud = 1;
				owner[n] = i;
				all[n++] = hl.graph[i];
				owner[n] = i;
				all[n++] = hl.caption[i];
			}
			if (hl.info.w) {
				if (hl.info.w < HUD_INFO_MIN)
					bad_hud = 1;
				owner[n] = HUD_INFO;
				all[n++] = hl.info;
			}
			if (hl.boxed != (h >= HUD_BOXED_ROWS) && n)
				bad_box = 1;
			for (int i = 0; i < n; i++) {
				KRect in = hl.boxed ? hl.box[owner[i]] : r;
				int pad = hl.boxed ? 1 : 0;
				if (all[i].x < in.x + pad ||
				    all[i].x + all[i].w > in.x + in.w - pad ||
				    all[i].y != in.y + pad ||
				    all[i].h != in.h - 2 * pad)
					bad_hud = 1;
				for (int k = i + 1; k < n; k++)
					if (overlaps(all[i], all[k]))
						bad_hud = 1;
			}
			for (int i = 0; hl.boxed && i <= HUD_INFO; i++) {
				KRect b = hl.box[i];
				if (!b.w)
					continue;
				if (b.x < r.x || b.x + b.w > r.x + r.w ||
				    b.y != r.y || b.h != r.h)
					bad_box = 1;
				for (int k = i + 1; k <= HUD_INFO; k++)
					if (overlaps(b, hl.box[k]))
						bad_box = 1;
			}
			if ((hl.graph[HUD_MEM].w && !hl.graph[HUD_CPU].w) ||
			    (hl.graph[HUD_OUT].w && !hl.graph[HUD_MEM].w) ||
			    (hl.info.w && !hl.graph[HUD_OUT].w))
				bad_order = 1;
		}
	vok(!bad_hud, "every hud panel lies inside its box or the hud, apart, at its size");
	vok(!bad_box, "a six-row hud boxes every panel, inside the hud and apart");
	vok(!bad_order, "hud panels are given up info first and cpu last");
	HudLayout h80 = hud_compute(krect(1, 0, 78, HUD_BOXED_ROWS));
	vok(h80.graph[HUD_OUT].w && !h80.info.w,
	    "80 columns keep the three graphs and drop the info panel");
	HudLayout h132 = hud_compute(krect(1, 0, 130, HUD_BOXED_ROWS));
	vok(h132.info.w && h132.box[HUD_INFO].x + h132.box[HUD_INFO].w == 131,
	    "132 columns keep every panel, the last box ending at the hud's edge");
	vok(!hud_compute(krect(1, 0, 19, HUD_BOXED_ROWS)).graph[HUD_CPU].w,
	    "a hud too narrow for one graph draws none");
	vok(layout_compute(100, 30).has_header, "a tall terminal keeps the header");
	vok(!layout_compute(100, 11).has_header, "a very short one drops it");
	vok(layout_compute(100, 30).has_detail, "a wide terminal keeps the detail pane");
	vok(!layout_compute(50, 30).has_detail, "a narrow one gives the tree the width");
	vok(layout_compute(50, 30).tree.w >= 40,
	    "and the tree then takes the whole width");

	/* Severity, including the trap: a word CONTAINING "error" is not one. */
	vok(log_severity("ld: undefined reference to `x'") == LOG_ERR, "undefined reference");
	vok(log_severity("foo.c:12: error: bad") == LOG_ERR, "error:");
	vok(log_severity("ERROR: File conflict detected:") == LOG_ERR, "ERROR:");
	vok(log_severity("make: *** [all] Error 1") == LOG_ERR, "make's Error");
	vok(log_severity("FAILED: zig2") == LOG_ERR, "FAILED");
	vok(log_severity("foo.c:12: warning: unused") == LOG_WARN, "warning:");
	vok(log_severity("WARNINGTON: linking") == LOG_PLAIN,
	    "WARNINGTON is not WARNING");
	vok(log_severity("checking for error_at_line... yes") == LOG_PLAIN,
	    "error_at_line is not an error");
	vok(log_severity("[15/18] Building C object") == LOG_PLAIN, "an ordinary line");
	vok(log_severity("") == LOG_PLAIN, "an empty line");

	vok(log_find("Building C object", "c object") == 9, "find is case-insensitive");
	vok(log_find("Building", "zzz") == -1, "a miss is -1");
	vok(log_find("Building", "") == -1, "an empty needle never matches");

	printf(vfail ? "view: %d failed\n" : "view: ok\n", vfail);
	return vfail ? 1 : 0;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The preview fixture
 *
 * A Manager and a Sampler filled in by hand so a screen can be drawn without
 * a build behind it. The values are not "some plausible data": each one is
 * here because it is what breaks a layout. Sizes and counts sit at the top of
 * their format's width (a multi-terabyte total is 4 characters, a nine-digit
 * file count is 11 with its separators, an hour-scale time is 5), because a
 * column defect is a field that fitted the value it was tried with and not
 * the value it will see on a real build.
 */

static BStep *pv_step(Manager *m, BStep *parent, const char *title, int status,
		      double dur)
{
	BStep *s = kb_calloc(1, sizeof(*s));
	kb_strlcpy(s->title, title, sizeof(s->title));
	kb_strlcpy(s->path, title, sizeof(s->path));
	s->status = status;
	s->parent = parent;
	s->logcap = 16;
	s->log = kb_calloc((size_t)s->logcap, sizeof(*s->log));

	/* Anchored on now rather than on a fixed epoch: a RUNNING step's
	 * duration is measured against the clock, so only a relative start
	 * gives the same number on every run. */
	double now = kb_now_s();
	if (status == ST_RUNNING) {
		s->start_time = now - dur;
	} else if (status == ST_DONE || status == ST_FAIL) {
		s->start_time = now - dur - 1;
		s->end_time = now - 1;
	}

	if (parent)
		parent->child[parent->nchild++] = s;
	m->order[m->norder++] = s;
	return s;
}

static void pv_log(BStep *s, const char *line)
{
	if (s->nlog == s->logcap) {
		s->logcap *= 2;
		char **nv = kb_calloc((size_t)s->logcap, sizeof(*nv));
		memcpy(nv, s->log, (size_t)s->nlog * sizeof(*nv));
		free(s->log);
		s->log = nv;
	}
	s->log[s->nlog++] = kb_strdup(line);
}

static BStep *pv_group(Manager *m, KbuildPhase *p, int index,
		       const char *dir_name, const char *name, int chroot,
		       const char *title, const char *desc, const char *snap)
{
	memset(p, 0, sizeof(*p));
	p->index = index;
	kb_strlcpy(p->dir_name, dir_name, sizeof(p->dir_name));
	kb_strlcpy(p->name, name, sizeof(p->name));
	snprintf(p->dir_path, sizeof(p->dir_path), "script/phases/%s", dir_name);
	p->chroot = chroot;
	kb_strlcpy(p->title, title, sizeof(p->title));
	kb_strlcpy(p->desc, desc, sizeof(p->desc));
	if (snap) {
		kb_strlcpy(p->snap_path[0], snap, sizeof(p->snap_path[0]));
		p->nsnap = 1;
	}

	char label[128];
	kbuild_label(p, label, sizeof(label));
	BStep *g = kb_calloc(1, sizeof(*g));
	g->is_group = 1;
	g->expanded = 1;
	g->status = ST_PENDING;
	g->meta = p;
	g->step_type = chroot ? SX_CHROOT : SX_HOST;
	kb_strlcpy(g->title, label, sizeof(g->title));
	kb_strlcpy(g->path, label, sizeof(g->path));
	g->logcap = 16;
	g->log = kb_calloc((size_t)g->logcap, sizeof(*g->log));

	m->root[m->nroot++] = g;
	m->order[m->norder++] = g;
	m->nphase = index + 1;
	return g;
}

void preview_fixture(Manager *m, Sampler *s)
{
	memset(m, 0, sizeof(*m));
	memset(s, 0, sizeof(*s));
	kb_strlcpy(m->script_dir, "script", sizeof(m->script_dir));
	kb_strlcpy(m->build_dir, "build", sizeof(m->build_dir));
	kb_strlcpy(m->repo_root, "/home/kdos/kdos", sizeof(m->repo_root));
	kb_strlcpy(m->snap_root, "build/snapshots", sizeof(m->snap_root));
	m->is_running = 1;
	/* Nineteen hours in: the header's total at its widest. */
	m->start_time = kb_now_s() - 71000;

	BStep *g0 = pv_group(m, &m->phase[0], 0, "00_cross", "cross", 0,
			     "Cross Toolchain",
			     "cross binutils + gcc targeting x86_64-kdos-linux-musl",
			     "cross");
	g0->status = ST_DONE;
	g0->expanded = 0;
	kb_strlcpy(g0->snap_state, "ok", sizeof(g0->snap_state));
	kb_strlcpy(g0->note, "1.9G", sizeof(g0->note));
	pv_step(m, g0, "cross binutils", ST_DONE, 96);
	/* Over an hour: the heat strip's top band. */
	pv_step(m, g0, "cross gcc (stage 1)", ST_DONE, 4111);
	pv_step(m, g0, "musl headers", ST_SKIPPED, 0);

	BStep *g1 = pv_group(m, &m->phase[1], 1, "42_graphics", "graphics", 1,
			     "Graphics Stack",
			     "wayland, X11 libraries, mesa, media frameworks",
			     "fs");
	g1->status = ST_RUNNING;
	pv_step(m, g1, "mesa", ST_DONE, 872);
	pv_step(m, g1, "gstreamer", ST_DONE, 1934);
	pv_step(m, g1, "wl-clipboard", ST_SKIPPED, 0);
	/* Longer than any tree pane this layout will ever hand it: the pane is
	 * capped at 48 columns and the title starts 6 in. */
	BStep *run = pv_step(m, g1,
			     "xdg-desktop-portal-wlr-with-a-very-long-port-name",
			     ST_RUNNING, 47);
	BStep *bad = pv_step(m, g1, "gst-plugins-bad", ST_FAIL, 213);
	bad->return_code = 2;	/* matches the "make: *** ... Error 2" log line */

	BStep *g2 = pv_group(m, &m->phase[2], 2, "70_image", "image", 0,
			     "Image",
			     "trim rootfs, build initramfs, assemble the ISO",
			     "fs");
	pv_step(m, g2, "030_launchers.sh", ST_PENDING, 0);
	pv_step(m, g2, "050_theme.sh", ST_PENDING, 0);
	pv_step(m, g2, "090_initramfs.sh", ST_PENDING, 0);
	pv_step(m, g2, "110_iso.sh", ST_PENDING, 0);
	/* Padded well past the tallest tree pane this file lays out (22 rows
	 * at 100x30) so a selection near the end of the list forces REAL
	 * scrolling. Feature 1's sticky header has nothing to pin without
	 * it -- a feature the preview cannot show is a feature nobody can
	 * check. */
	static const char *PAD[] = {
		"120_verify_manifest.sh", "130_strip_symbols.sh",
		"140_gen_locales.sh",     "150_seed_fonts.sh",
		"160_vendor_licenses.sh", "170_gen_checksums.sh",
		"180_sign_kernel.sh",     "190_build_squashfs.sh",
		"200_layout_iso9660.sh",  "210_write_hybrid_mbr.sh",
		"220_smoke_boot.sh",      "230_archive_logs.sh",
	};
	for (size_t i = 0; i < sizeof(PAD) / sizeof(PAD[0]); i++)
		pv_step(m, g2, PAD[i], ST_PENDING, 0);

	m->current_phase = g1;
	m->current_step = run;
	m->error_step = bad;
	/* --port-jobs at its ceiling with every slot busy: the header's
	 * `running` count at its widest. Only the count is set — the fixture
	 * never pumps, so no slot needs a process. */
	m->port_jobs = KB_MAX_WORKERS;
	m->nrunning = KB_MAX_WORKERS;
	m->lvl_from = m->lvl_commit = -1;

	pv_log(run, "[142/318] Compiling wlroots v0.20.2");
	pv_log(run, "  Compiling smithay v0.4.0");
	pv_log(run, "checking for error_at_line... yes");
	pv_log(run, "warning: unused variable: `output_state`");
	pv_log(bad, "  CC       gstreamer/plugins/element.lo");
	pv_log(bad, "warning: field `subscription` is never read");
	pv_log(bad, "  --> src/pages/display/mod.rs:118:5");
	pv_log(bad, "error: linking with `cc` failed: exit status: 1");
	pv_log(bad, "  = note: /usr/lib/libEGL.so: undefined reference to "
		    "`__unw_getcontext'");
	pv_log(bad, "error: could not compile `gst-plugins-bad` (bin "
		    "\"gst-plugins-bad\") due to 1 previous error");
	pv_log(bad, "make: *** [Makefile:41: all] Error 2");

	/* A real snapshot summary, longer than the HUD's info panel at any
	 * width short of the widest: it has to wrap onto its second row and be
	 * cut there. Set directly rather than through mgr_notice(), which
	 * appends to <build>/logs/snapshots.log — a preview run in a checkout
	 * would write into that checkout's build log. */
	m->notice[0].when = kb_now_s();
	kb_strlcpy(m->notice[0].text,
		   "PARTIAL snapshot 41_system at step 717/965 -> 3.7T (layer on "
		   "40_lang; restoring it re-runs the phase)",
		   sizeof(m->notice[0].text));
	m->nnotice = 1;

	s->load1 = 123.47;				/* "load 123.47" */
	s->ncpu = 1024;					/* "1024 cpus"   */
	s->mem_total = 67 * 1024LL * 1024 * 1024;
	s->mem_used = 65 * 1024LL * 1024 * 1024;	/* past 95%      */
	s->swap_used = 1288490188LL;			/* 1.2G, rising  */
	s->swap_rose_at = kb_now_s();
	s->disk_free = 2199023255552LL;			/* 2.0T          */
	s->fs_bytes = 4076863488000LL;			/* 3.7T          */
	s->fs_files = 918273645;			/* 918,273,645   */
	s->fs_partial = 1;
	s->fs_sampled_at = kb_now_s();
	s->walker = -1;
	/* Shapes whose direction is known, because a graph that is wrong
	 * (inverted, off by a dot, clipped, rescaled) only shows against one:
	 * CPU a sawtooth through the 85% warning to 100%, memory a sawtooth
	 * from 40% to past 95%, output a sawtooth climbing a decade every five
	 * seconds from 1 line a second to 100,000. 600 samples overfill every
	 * graph short of 300 cells, so a wide one shows where history starts. */
	static const double FIFTH[] = { 1, 1.58489, 2.51189, 3.98107, 6.30957 };
	for (int i = 0; i < 600; i++) {
		double rate = FIFTH[(i % 26) % 5];
		for (int d = 0; d < (i % 26) / 5; d++)
			rate *= 10;
		sam_push(s, (double)((i * 7) % 23) / 22.0,
			 0.40 + 0.57 * (i % 64) / 63.0, rate);
	}
	/* The sampler pushes each reading on the tick it takes it, so the
	 * newest sample IS the current value and the window's peak can never
	 * come out below it. The fixture obeys that, or the preview renders a
	 * state the real screen cannot reach. */
	s->cpu_hist[s->nhist - 1] = s->cpu_busy = 0.92;
	s->mem_hist[s->nhist - 1] = (double)s->mem_used / (double)s->mem_total;
	s->rate_hist[s->nhist - 1] = s->lines_per_sec = 123456;	/* widest */
	m->total_lines = 123456789;			/* "123.5M"      */

	/* The snapshot overlay's numbers. Only drawn when a screen asks for it,
	 * but filled here so the two agree. */
	m->snap.est_bytes = 4076863488000LL;
	m->snap.bytes = 2938404864000LL;
	m->snap.files = 918273645;
	m->snap.rate = 187 * 1024LL * 1024;
	m->snap.started = kb_now_s() - 4231;
	kb_strlcpy(m->snap.action, "snapshot", sizeof(m->snap.action));
	kb_strlcpy(m->snap.phase, "42_graphics", sizeof(m->snap.phase));
	kb_strlcpy(m->snap.path, "fs", sizeof(m->snap.path));
	kb_strlcpy(m->snap.layer, "layer on 41_system", sizeof(m->snap.layer));
	kb_strlcpy(m->snap.current,
		   "fs/usr/lib/gstreamer-1.0/libgstvideoconvert.so",
		   sizeof(m->snap.current));
}

int preview_snapshots(Manager *m, KbuildSnapshot *out, int max)
{
	/* All thirteen real phase dirs, each with a snapshot, so the startup
	 * picker's row count matches the live system's exactly: 13 phases +
	 * the "start fresh" row is 14, which is what makes the picker's own
	 * list truncate on a 24-row terminal and is the only way `--preview
	 * startup` can exercise that path rather than just describe it. */
	static const struct {
		const char *dir, *commit;
		int dirty, complete, steps, total;
		double created, duration;
		long long comp, raw, files;
	} SN[] = {
		{ "00_cross", "38c01f2", 0, 1, 2, 2, 1786518000, 507,
		  1994294886LL, 5798205849LL, 41827 },
		{ "10_bootstrap", "9f3a1c2", 0, 1, 16, 16, 1786518600, 1360,
		  812345678LL, 2103456789LL, 64210 },
		{ "20_selfhost", "9f3a1c2", 0, 1, 11, 11, 1786520500, 852,
		  734000000LL, 1900000000LL, 31500 },
		{ "30_foundation", "9f3a1c2", 0, 1, 115, 115, 1786524100, 3600,
		  1650000000LL, 4300000000LL, 120443 },
		{ "31_compilers", "9f3a1c2", 0, 0, 14, 22, 1786551500, 27360,
		  2650000000LL, 6800000000LL, 210443 },
		{ "40_lang", "aec48df", 0, 1, 169, 169, 1786556000, 2410,
		  2930000000LL, 7400000000LL, 262100 },
		/* Terabyte-scale and a five-digit step count in a nine-column
		 * field: this row is the one the header alignment has to hold. */
		{ "41_system", "aec48df", 1, 0, 717, 965, 1786604400, 27143,
		  4076863488000LL, 9895604649984LL, 918273645 },
		{ "42_graphics", "aec48df", 0, 1, 186, 186, 1786606200, 5120,
		  4180000000LL, 10400000000LL, 402110 },
		{ "43_toolkits", "aec48df", 0, 1, 242, 242, 1786607900, 9840,
		  5100000000LL, 12800000000LL, 511800 },
		{ "44_apps", "aec48df", 0, 1, 280, 280, 1786609000, 8420,
		  6020000000LL, 15100000000LL, 598230 },
		{ "50_desktop", "c112af0", 0, 1, 31, 31, 1786609600, 540,
		  6090000000LL, 15300000000LL, 603410 },
		{ "60_kernel", "c112af0", 0, 1, 4, 4, 1786610000, 612,
		  512300000LL, 1330000000LL, 9840 },
		{ "70_image", "c112af0", 0, 1, 11, 11, 1786611000, 298,
		  1450000000LL, 3200000000LL, 88210 },
	};
	int n = (int)(sizeof(SN) / sizeof(SN[0]));
	if (n > max)
		n = max;

	for (int i = 0; i < n; i++) {
		KbuildSnapshot *sn = &out[i];
		memset(sn, 0, sizeof(*sn));
		kb_strlcpy(sn->dir_name, SN[i].dir, sizeof(sn->dir_name));
		kb_strlcpy(sn->phase_dir, SN[i].dir, sizeof(sn->phase_dir));
		kb_strlcpy(sn->git_commit, SN[i].commit, sizeof(sn->git_commit));
		kb_strlcpy(sn->codec, "zstd", sizeof(sn->codec));
		sn->git_dirty = SN[i].dirty;
		sn->complete = SN[i].complete;
		sn->steps = SN[i].steps;
		sn->total_steps = SN[i].total;
		sn->created = SN[i].created;
		sn->duration_s = SN[i].duration;
		for (int k = 0; k < m->nphase; k++)
			if (!strcmp(m->phase[k].dir_name, SN[i].dir))
				kb_strlcpy(sn->title, m->phase[k].title,
					   sizeof(sn->title));
		int is_cross = !strcmp(SN[i].dir, "00_cross");
		kb_strlcpy(sn->entry[0].path, is_cross ? "cross" : "fs",
			   sizeof(sn->entry[0].path));
		kb_strlcpy(sn->entry[0].archive,
			   is_cross ? "cross.tar.zst" : "fs.tar.zst",
			   sizeof(sn->entry[0].archive));
		sn->entry[0].bytes_compressed = SN[i].comp;
		sn->entry[0].bytes_raw = SN[i].raw;
		sn->entry[0].files = SN[i].files;
		sn->entry[0].tree_bytes = SN[i].raw;
		sn->entry[0].tree_files = SN[i].files;
		sn->nentries = 1;
		snprintf(sn->id, sizeof(sn->id), "%s-1786518000-0000000%d",
			 SN[i].dir, i % 10);
		/* 00_cross archives cross and 10_bootstrap is the first full
		 * fs; every later fs is a layer on the one before, so the
		 * picker's "restore reads" line walks a real chain. */
		if (i >= 2) {
			sn->entry[0].layer = 1;
			kb_strlcpy(sn->entry[0].base_id, out[i - 1].id,
				   sizeof(sn->entry[0].base_id));
		}
	}
	return n;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* --preview                                                                */

int preview_main(const char *screen, const char *size, const char *tier)
{
	int w = 100, h = 30;
	if (sscanf(size, "%dx%d", &w, &h) != 2 || w < 8 || h < 4) {
		fprintf(stderr, "bad size: %s (want WxH)\n", size);
		return 1;
	}

	/* The tier decides which glyph set and which ramp the screen is drawn
	 * with, exactly as ktui_term_init would have from the real terminal —
	 * `vt` is the one that matters, because the 512-glyph console font has
	 * no eighth blocks, no half blocks and no braille, and a glyph it does
	 * not carry comes out blank on tty1 rather than wrong. */
	ktui_caps = KT_CAP_UTF8;
	if (!strcmp(tier, "vt"))
		ktui_caps |= KT_CAP_LINUXVT;
	else if (!strcmp(tier, "ascii"))
		ktui_caps = 0;
	else if (strcmp(tier, "rich")) {
		fprintf(stderr, "unknown tier: %s (rich|vt|ascii)\n", tier);
		return 1;
	}
	/* ktui_draw_init() calls it too; named here because the tier is only
	 * decided one line above and the ramps are the whole point of it. */
	ktui_ramp_init();
	if (ktui_offscreen_init(w, h) != 0)
		return 1;

	if (preview_screen(screen) != 0) {
		fprintf(stderr, "unknown screen: %s\n", screen);
		return 1;
	}
	ktui_draw_dump();
	return 0;
}
