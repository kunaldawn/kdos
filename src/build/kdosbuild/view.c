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
 * log before the tree, and the tree is never given up. */
Layout layout_compute(int w, int h)
{
	Layout l;
	memset(&l, 0, sizeof(l));
	if (w < 40 || h < 10) {
		l.too_small = 1;
		return l;
	}

	l.hud_rows = h >= 24 ? 2 : (h >= 16 ? 1 : 0);
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
	 * can be. This is the assertion, not the instance: the divider landing
	 * on the log text was one size out of this range. */
	int bad_size = 0;
	/* A region left at zero size passes both checks above silently — it
	 * neither overlaps anything nor leaves the screen. So a region that is
	 * supposed to be ACTIVE at this size (its governing flag says so) is
	 * also checked for positive width and height, or a future regression
	 * that forgot to size one would pass this whole test. */
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
	vok(layout_compute(100, 30).hud_rows == 2, "a tall terminal keeps both hud rows");
	vok(layout_compute(100, 18).hud_rows == 1, "a shorter one keeps one");
	vok(layout_compute(100, 14).hud_rows == 0, "a short one drops the hud");
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
 * A Manager, a Sampler and a Timings filled in by hand so a screen can be
 * drawn without a build behind it. The values are not "some plausible data":
 * each one is here because it is what breaks a layout. Sizes and counts sit
 * at the top of their format's width (a multi-terabyte total is 4 characters,
 * a nine-digit file count is 11 with its separators, an hour-scale ETA is 5),
 * because every column defect this TUI has shipped was a field that fitted
 * the value it was tried with and not the value it will see on a real build.
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
	snprintf(p->dir_path, sizeof(p->dir_path), "script/%s", dir_name);
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

void preview_fixture(Manager *m, Sampler *s, Timings *t)
{
	memset(m, 0, sizeof(*m));
	memset(s, 0, sizeof(*s));
	memset(t, 0, sizeof(*t));
	kb_strlcpy(m->script_dir, "script", sizeof(m->script_dir));
	kb_strlcpy(m->build_dir, "build", sizeof(m->build_dir));
	kb_strlcpy(m->repo_root, "/home/kdos/kdos", sizeof(m->repo_root));
	kb_strlcpy(m->snap_root, "build/snapshots", sizeof(m->snap_root));
	m->is_running = 1;
	m->snapshot_enabled = 1;
	m->timings = t;

	BStep *g0 = pv_group(m, &m->phase[0], 0, "00_toolchain", "toolchain", 0,
			     "Cross Toolchain",
			     "cross binutils + gcc targeting x86_64-kdos-linux-musl",
			     "cross");
	g0->status = ST_DONE;
	g0->expanded = 0;
	kb_strlcpy(g0->snap_state, "ok", sizeof(g0->snap_state));
	kb_strlcpy(g0->note, "1.9G", sizeof(g0->note));
	pv_step(m, g0, "cross binutils", ST_DONE, 96);
	pv_step(m, g0, "cross gcc (stage 1)", ST_DONE, 411);
	pv_step(m, g0, "musl headers", ST_SKIPPED, 0);

	BStep *g1 = pv_group(m, &m->phase[1], 1, "04_phase4", "phase4", 1,
			     "Userland & GUI Sliver",
			     "the COSMIC desktop, the wayland utilities and the "
			     "alien-app plumbing", "fs");
	g1->status = ST_RUNNING;
	pv_step(m, g1, "mesa", ST_DONE, 872);
	pv_step(m, g1, "cosmic-comp", ST_DONE, 1934);
	pv_step(m, g1, "pop-launcher", ST_SKIPPED, 0);
	/* Longer than any tree pane this layout will ever hand it: the pane is
	 * capped at 48 columns and the title starts 6 in. */
	BStep *run = pv_step(m, g1,
			     "xdg-desktop-portal-cosmic-with-a-very-long-port-name",
			     ST_RUNNING, 47);
	BStep *bad = pv_step(m, g1, "cosmic-settings", ST_FAIL, 213);
	bad->return_code = 2;	/* matches the "make: *** ... Error 2" log line */

	BStep *g2 = pv_group(m, &m->phase[2], 2, "06_packaging", "packaging", 0,
			     "Packaging",
			     "trim rootfs, theme, user, appbox, initramfs, ISO",
			     "fs");
	pv_step(m, g2, "00_theme.sh", ST_PENDING, 0);
	pv_step(m, g2, "01_appbox.sh", ST_PENDING, 0);
	pv_step(m, g2, "02_initramfs.sh", ST_PENDING, 0);
	pv_step(m, g2, "03_iso.sh", ST_PENDING, 0);
	/* Padded well past the tallest tree pane this file lays out (22 rows
	 * at 100x30) so a selection near the end of the list forces REAL
	 * scrolling. Feature 1's sticky header has nothing to pin without
	 * it -- a feature the preview cannot show is a feature nobody can
	 * check. */
	static const char *PAD[] = {
		"04_verify_manifest.sh", "05_strip_symbols.sh",
		"06_gen_locales.sh",     "07_seed_fonts.sh",
		"08_vendor_licenses.sh", "09_gen_checksums.sh",
		"10_sign_kernel.sh",     "11_build_squashfs.sh",
		"12_layout_iso9660.sh",  "13_write_hybrid_mbr.sh",
		"14_smoke_boot.sh",      "15_archive_logs.sh",
	};
	for (size_t i = 0; i < sizeof(PAD) / sizeof(PAD[0]); i++)
		pv_step(m, g2, PAD[i], ST_PENDING, 0);

	m->current_phase = g1;
	m->current_step = run;
	m->error_step = bad;

	pv_log(run, "[142/318] Compiling cosmic-comp v1.0.0-alpha.7");
	pv_log(run, "  Compiling smithay v0.4.0");
	pv_log(run, "checking for error_at_line... yes");
	pv_log(run, "warning: unused variable: `output_state`");
	pv_log(bad, "  Compiling cosmic-settings-page v0.1.0");
	pv_log(bad, "warning: field `subscription` is never read");
	pv_log(bad, "  --> src/pages/display/mod.rs:118:5");
	pv_log(bad, "error: linking with `cc` failed: exit status: 1");
	pv_log(bad, "  = note: /usr/lib/libEGL.so: undefined reference to "
		    "`__unw_getcontext'");
	pv_log(bad, "error: could not compile `cosmic-settings` (bin "
		    "\"cosmic-settings\") due to 1 previous error");
	pv_log(bad, "make: *** [Makefile:41: all] Error 2");

	/* An hour-scale ETA. Every step still to run has a recorded estimate,
	 * so the header's `eta` field is exercised at its widest. */
	for (int i = 0; i < m->norder; i++) {
		BStep *st = m->order[i];
		if (st->is_group || st->status == ST_DONE ||
		    st->status == ST_SKIPPED)
			continue;
		char key[192];
		step_timing_key(st, key, sizeof(key));
		tm_record_step(t, key, 1450);
	}

	/* Short on purpose. The notice has first claim on the HUD's second row
	 * and the heat strip is sized from what is left, so a long one drops
	 * the strip entirely — which is correct behaviour, and would leave the
	 * widget this fixture exists to look at undrawn. */
	mgr_notice(m, "snapshot of 04_phase4 queued");

	s->load1 = 23.47;
	s->mem_total = 67 * 1024LL * 1024 * 1024;
	s->mem_used = 61 * 1024LL * 1024 * 1024;
	s->disk_free = 2199023255552LL;			/* 2.0T          */
	s->lines_per_sec = 12345;
	s->fs_bytes = 4076863488000LL;			/* 3.7T          */
	s->fs_files = 918273645;			/* 918,273,645   */
	s->fs_partial = 1;
	s->fs_sampled_at = kb_now_s();
	s->walker = -1;
	/* A sawtooth rather than noise: a sparkline that is visibly wrong
	 * (inverted, off by a cell, clipped) is only visible against a shape
	 * whose direction is known. */
	s->nhistory = 60;
	for (int i = 0; i < s->nhistory; i++)
		s->history[i] = (double)((i * 7) % 23);
	s->nload = 40;
	for (int i = 0; i < s->nload; i++)
		s->load_hist[i] = (double)((i * 5) % 19);
	/* The sampler pushes load1 into the ring on the same tick it reads it,
	 * so the newest sample IS the current load and the window peak can
	 * never come out below it. The fixture has to obey that or the preview
	 * renders a state the real screen cannot reach — which is the one thing
	 * a verification fixture must not do. */
	s->load_hist[s->nload - 1] = s->load1;

	/* The snapshot overlay's numbers. Only drawn when a screen asks for it,
	 * but filled here so the two agree. */
	m->snap.est_bytes = 4076863488000LL;
	m->snap.bytes = 2938404864000LL;
	m->snap.files = 918273645;
	m->snap.rate = 187 * 1024LL * 1024;
	m->snap.started = kb_now_s() - 4231;
	kb_strlcpy(m->snap.action, "snapshot", sizeof(m->snap.action));
	kb_strlcpy(m->snap.phase, "04_phase4", sizeof(m->snap.phase));
	kb_strlcpy(m->snap.path, "fs", sizeof(m->snap.path));
	kb_strlcpy(m->snap.current,
		   "fs/usr/lib/cosmic/libcosmic-workspaces-applet.so",
		   sizeof(m->snap.current));
}

int preview_snapshots(Manager *m, KbuildSnapshot *out, int max)
{
	/* All seven real phase dirs, each with a snapshot, so the startup
	 * picker's row count matches the live system's exactly: 7 phases + the
	 * "start fresh" row is 8, which is what makes the picker's own list
	 * truncate on a 24-row terminal and is the only way `--preview startup`
	 * can exercise that path rather than just describe it. */
	static const struct {
		const char *dir, *commit;
		int dirty, complete, steps, total;
		double created, duration;
		long long comp, raw, files;
	} SN[] = {
		{ "00_toolchain", "38c01f2", 0, 1, 3, 3, 1786518000, 507,
		  1994294886LL, 5798205849LL, 41827 },
		{ "01_phase1", "9f3a1c2", 0, 1, 18, 18, 1786518600, 1360,
		  812345678LL, 2103456789LL, 64210 },
		{ "02_phase2", "9f3a1c2", 0, 1, 11, 11, 1786520500, 852,
		  734000000LL, 1900000000LL, 31500 },
		{ "03_phase3", "9f3a1c2", 0, 0, 134, 310, 1786530000, 9840,
		  2650000000LL, 6800000000LL, 210443 },
		/* Terabyte-scale and a five-digit step count in a nine-column
		 * field: this row is the one the header alignment has to hold. */
		{ "04_phase4", "aec48df", 1, 0, 217, 391, 1786604400, 27143,
		  4076863488000LL, 9895604649984LL, 918273645 },
		{ "05_phase5", "c112af0", 0, 1, 4, 4, 1786610000, 612,
		  512300000LL, 1330000000LL, 9840 },
		{ "06_packaging", "c112af0", 0, 1, 9, 9, 1786611000, 298,
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
		int is_cross = !strcmp(SN[i].dir, "00_toolchain");
		kb_strlcpy(sn->entry[0].path, is_cross ? "cross" : "fs",
			   sizeof(sn->entry[0].path));
		kb_strlcpy(sn->entry[0].archive,
			   is_cross ? "cross.tar.zst" : "fs.tar.zst",
			   sizeof(sn->entry[0].archive));
		sn->entry[0].bytes_compressed = SN[i].comp;
		sn->entry[0].bytes_raw = SN[i].raw;
		sn->entry[0].files = SN[i].files;
		sn->nentries = 1;
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
