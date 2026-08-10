/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the command line
 *
 * Every flag build.py accepted, with the same names and the same meanings:
 * `make build BUILD_ARGS=...` has to keep working while both drivers exist.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdosbuild.h"

typedef struct {
	const char *build_dir;
	const char *script_dir;
	int fresh;
	const char *restore;
	const char *continue_from;
	int no_snapshot;
	int want_plan;
	const char *phases;
	const char *steps;
	const char *rebuild;
	int snapshot;
	int list;
	int plain;
	const char *del;
} Args;

static void usage(void)
{
	printf(
"kdosbuild — KDOS build orchestrator\n"
"\n"
"  --build-dir DIR       build output directory (default: build)\n"
"  --script-dir DIR      phase directory (default: script)\n"
"  --fresh               skip the snapshot picker and run every phase\n"
"  --restore PHASE       restore a snapshot and continue after it\n"
"                        (phase name, directory name, 1-based index, 'latest')\n"
"  --continue-from PHASE resume at PHASE on the existing tree, no restore\n"
"  --no-snapshot         do not write snapshots during this build\n"
"  --plan                open the build-plan picker and run it on this tree\n"
"  --phases LIST         only run these phases\n"
"  --steps LIST          only run these scripts, PHASE:script.sh\n"
"  --rebuild LIST        force-rebuild these ports even though installed\n"
"  --snapshot            write snapshots even for a partial plan\n"
"  --plain               no TUI: plain lines (implied by a non-tty stdout)\n"
"  --list                list snapshots and exit\n"
"  --delete PHASE        delete one snapshot and exit\n"
"\n"
"  --selftest            run the view-geometry and log-classifier assertions\n"
"  --preview SCREEN WxH TIER\n"
"                        draw one screen offscreen and dump it as plain text.\n"
"                        SCREEN: build activity failure pinned startup plan\n"
"                                packages\n"
"                        TIER:   rich (eighth blocks) | vt (the 512-glyph\n"
"                                console font) | ascii\n");
}

static const KbuildPhase *resolve_phase(Manager *m, const char *token)
{
	if (!token || !*token)
		return NULL;

	if (!strcmp(token, "latest")) {
		KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES,
						  sizeof(*snaps));
		int n = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);
		const KbuildPhase *best = NULL;
		for (int i = 0; i < m->nphase; i++)
			if (kbuild_snap_find(snaps, n, m->phase[i].dir_name))
				best = &m->phase[i];
		free(snaps);
		return best;
	}

	const KbuildPhase *p = kbuild_find(m->phase, m->nphase, token);
	if (p)
		return p;

	char *end = NULL;
	long idx = strtol(token, &end, 10);
	if (end && !*end && idx >= 1 && idx <= m->nphase)
		return &m->phase[idx - 1];
	return NULL;
}

static int cmd_list(Manager *m)
{
	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int n = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);
	if (!n) {
		printf("no snapshots in %s\n", m->snap_root);
		free(snaps);
		return 0;
	}

	char commit[64];
	int dirty = 0;
	snap_git_info(".", commit, sizeof(commit), &dirty);

	printf("%-3s %-16s %-17s %9s %10s %6s\n", "#", "PHASE", "WHEN", "SIZE",
	       "COMMIT", "STEPS");
	for (int i = 0; i < m->nphase; i++) {
		const KbuildSnapshot *sn = kbuild_snap_find(snaps, n,
							   m->phase[i].dir_name);
		if (!sn)
			continue;
		long long total = 0;
		for (int k = 0; k < sn->nentries; k++)
			total += sn->entry[k].bytes_compressed;
		int stale = sn->git_dirty ||
			    (commit[0] && sn->git_commit[0] &&
			     strcmp(sn->git_commit, commit));
		char cm[72];
		snprintf(cm, sizeof(cm), "%s%s",
			 sn->git_commit[0] ? sn->git_commit : "-",
			 stale ? "*" : "");
		char when[32], size[32];
		kb_strlcpy(when, format_when(sn->created), sizeof(when));
		kb_strlcpy(size, human_bytes(total), sizeof(size));
		printf("%-3d %-16s %-17s %9s %10s %6d\n", i + 1,
		       m->phase[i].dir_name, when, size, cm, sn->steps);
		for (int k = 0; k < sn->nentries; k++) {
			char comp[32], raw[32];
			kb_strlcpy(comp, human_bytes(sn->entry[k].bytes_compressed),
				   sizeof(comp));
			kb_strlcpy(raw, human_bytes(sn->entry[k].bytes_raw),
				   sizeof(raw));
			printf("      %-12s %9s <- %9s  %s files\n",
			       sn->entry[k].path, comp, raw,
			       human_count(sn->entry[k].files));
		}
	}
	free(snaps);
	return 0;
}

/* Why `target` cannot be restored, or NULL if it can.
 *
 * Restoring must NEVER silently fall back to an older phase: the plan is
 * layered, so it is non-empty whenever any earlier phase has a snapshot, and
 * marking the requested phase skipped on that basis would build the remaining
 * phases against a rootfs that never had the target's packages. */
static const char *check_restorable(Manager *m, const KbuildPhase *target,
				    char *err, size_t errcap)
{
	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int n = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);

	if (!kbuild_snap_find(snaps, n, target->dir_name)) {
		KbBuf b = {0};
		for (int i = 0; i < n; i++)
			kb_buf_printf(&b, "%s%s", b.n ? ", " : "",
				      snaps[i].dir_name);
		snprintf(err, errcap, "no snapshot for %s (available: %s)",
			 target->dir_name, b.n ? b.p : "none");
		kb_buf_free(&b);
		free(snaps);
		return err;
	}
	free(snaps);

	/* Every path any earlier phase declares must come from somewhere, or
	 * the restored tree is missing a component (cross/ when 00 and 01 were
	 * deleted, say). */
	KbuildRestoreItem *plan = kb_calloc(KBUILD_MAX_PATHS * 4, sizeof(*plan));
	int np = kbuild_snap_plan_restore(m->snap_root, m->phase, m->nphase,
					  target->index, plan,
					  KBUILD_MAX_PATHS * 4);
	/* A SET, sorted — several phases declare `cross` and `mark`, and
	 * listing one of them twice reads as two different problems. */
	char miss[KBUILD_MAX_PATHS * KBUILD_MAX_PHASES][128];
	int nmiss = 0;
	for (int i = 0; i < m->nphase && m->phase[i].index <= target->index; i++)
		for (int k = 0; k < m->phase[i].nsnap; k++) {
			const char *path = m->phase[i].snap_path[k];
			int seen = 0;
			for (int j = 0; j < np && !seen; j++)
				seen = !strcmp(plan[j].path, path);
			for (int j = 0; j < nmiss && !seen; j++)
				seen = !strcmp(miss[j], path);
			if (!seen)
				kb_strlcpy(miss[nmiss++], path, 128);
		}
	free(plan);

	for (int i = 1; i < nmiss; i++) {
		char key[128];
		kb_strlcpy(key, miss[i], sizeof(key));
		int k = i - 1;
		while (k >= 0 && strcmp(miss[k], key) > 0) {
			memcpy(miss[k + 1], miss[k], 128);
			k--;
		}
		memcpy(miss[k + 1], key, 128);
	}

	KbBuf missing = {0};
	for (int i = 0; i < nmiss; i++)
		kb_buf_printf(&missing, "%s%s", i ? " " : "", miss[i]);

	if (missing.n) {
		snprintf(err, errcap,
			 "snapshots for %s are incomplete: no source for %s",
			 target->dir_name, missing.p);
		kb_buf_free(&missing);
		return err;
	}
	kb_buf_free(&missing);
	return NULL;
}

static void plain_tick(Manager *m);

/* The newest phase that actually contributed data. The skip ceiling comes
 * from this rather than from the requested target, so the header can never
 * claim a phase that supplied nothing. */
static const KbuildPhase *effective_source(Manager *m,
					   const KbuildRestoreItem *plan, int n,
					   const KbuildPhase *target)
{
	const KbuildPhase *best = NULL;
	for (int i = 0; i < m->nphase; i++)
		for (int k = 0; k < n; k++)
			if (!strcmp(plan[k].source, m->phase[i].dir_name)) {
				best = &m->phase[i];
				break;
			}
	return best ? best : target;
}

static int do_restore(Manager *m, const KbuildPhase *target, int plain,
		      char *err, size_t errcap)
{
	KbuildRestoreItem *plan = kb_calloc(KBUILD_MAX_PATHS * 4, sizeof(*plan));
	int n = kbuild_snap_plan_restore(m->snap_root, m->phase, m->nphase,
					 target->index, plan,
					 KBUILD_MAX_PATHS * 4);
	if (!n) {
		snprintf(err, errcap, "no snapshot data for %s",
			 target->dir_name);
		free(plan);
		return -1;
	}

	KbuildSnapshot sn;
	int partial = 0;
	if (kbuild_snap_load(m->snap_root, target->dir_name, &sn) == 0)
		partial = !sn.complete;

	if (plain) {
		/* screen_progress() installs a tick that DRAWS — calling it
		 * with no terminal taken over is a segfault, not a no-op. */
		snap_set_tick(m, plain_tick);
		printf("==> restoring %s\n", target->dir_name);
		fflush(stdout);
	} else {
		char title[128];
		snprintf(title, sizeof(title), " RESTORING %s ",
			 target->dir_name);
		screen_progress(m, title);
	}

	if (snap_restore(m, plan, n, target->dir_name, err, errcap) < 0) {
		free(plan);
		return -1;
	}

	const KbuildPhase *eff = effective_source(m, plan, n, target);
	mgr_mark_restored(m, eff->index, partial);

	KbBuf paths = {0};
	for (int i = 0; i < n; i++)
		kb_buf_printf(&paths, "%s%s", i ? ", " : "", plan[i].path);
	mgr_notice(m, "restored %s%s (%s)", eff->dir_name,
		   partial ? " [partial - phase re-runs]" : "",
		   paths.p ? paths.p : "");
	kb_buf_free(&paths);
	free(plan);
	return 0;
}

/* Headless snapshot/restore progress: one line when the path changes, then a
 * heartbeat, so a forty-minute tar is visibly alive in a log file. */
/* Set while a step line is open and waiting for its "ok"/"FAILED". A
 * snapshot starts INSIDE that window, so its progress has to break the line
 * rather than land in the middle of it. */
static int plain_line_open;

static void plain_break_line(void)
{
	if (plain_line_open) {
		printf("\n");
		plain_line_open = 0;
	}
}

static void plain_tick(Manager *m)
{
	static double last;
	static char seen[160];

	char now[160];
	snprintf(now, sizeof(now), "%s %s", m->snap.action, m->snap.path);
	if (strcmp(seen, now)) {
		kb_strlcpy(seen, now, sizeof(seen));
		plain_break_line();
		printf("    %s %s\n", m->snap.action, m->snap.path);
		fflush(stdout);
		last = kb_now_s();
		return;
	}
	if (kb_now_s() - last < 15)
		return;
	last = kb_now_s();
	plain_break_line();
	printf("      %s / %s   %lld files\n", human_bytes(m->snap.bytes),
	       m->snap.est_bytes ? human_bytes(m->snap.est_bytes) : "?",
	       m->snap.files);
	fflush(stdout);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Headless
 *
 * build.py had no such mode: it went straight through curses.wrapper, so a
 * build with its output redirected drew a full-screen UI into a log file and
 * a build with no terminal at all could not start. kpkg already answers
 * TERM=dumb or a non-tty stdout with plain lines; this is the same rule, and
 * it is also what makes the engine testable without a pty.
 */
static void run_plain(Manager *m, Sampler *sam, Timings *tm)
{
	snap_set_tick(m, plain_tick);
	mgr_start(m);

	BStep *announced = NULL;
	int shown_notices = 0;

	/* A step's result and any notice it produced are printed on the SAME
	 * pump turn the child exited on, because finish_phase() — which is
	 * where a snapshot happens — runs in that turn too. Deferring either
	 * to the next turn interleaves a snapshot line into the middle of the
	 * step line it belongs after. */
	while (m->is_running) {
		int wait_ms = mgr_pump(m);
		sam_pump(sam, m);

		BStep *s = m->current_step;
		if (s && !s->is_group && s->reported)
			s = NULL;
		if (s && s != announced) {
			announced = s;
			if (s->is_group)
				printf("==> %s\n", s->title);
			else {
				printf("    %-40s ", s->title);
				plain_line_open = 1;
			}
			fflush(stdout);
		}
		if (announced && !announced->is_group && announced->end_time > 0 &&
		    announced->status != ST_RUNNING) {
			if (!plain_line_open)
				printf("    %-40s ", announced->title);
			plain_line_open = 0;
			printf("%s (%s)\n",
			       announced->status == ST_DONE ? "ok" : "FAILED",
			       human_time(step_duration(announced)));
			announced->reported = 1;
			announced = NULL;
		}
		if (shown_notices < m->nnotice)
			plain_break_line();
		for (; shown_notices < m->nnotice; shown_notices++)
			printf("... %s\n", m->notice[shown_notices].text);
		fflush(stdout);

		usleep((useconds_t)(wait_ms > 0 ? wait_ms : 5) * 1000);
	}

	for (; shown_notices < m->nnotice; shown_notices++)
		printf("... %s\n", m->notice[shown_notices].text);

	if (m->error_step)
		printf("BUILD FAILED at %s\n", m->error_step->title);
	else
		printf("BUILD COMPLETE in %s\n",
		       human_time(kb_now_s() - m->start_time));

	sam_stop(sam);
	tm_save(tm);
}

/* A fatal message on the TUI screen, then a key. Reached only after the
 * terminal has been taken over. */
static void bail(const char *msg)
{
	ktui_draw_clear();
	int y = 1;
	char copy[1024];
	kb_strlcpy(copy, msg, sizeof(copy));
	for (char *line = copy, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		ktui_draw_text(2, y++, ktui_w - 4, line, KT_ERR, KT_BG,
			       KT_A_BOLD);
	}
	ktui_draw_text(2, y + 1, ktui_w - 4, "press any key to exit", KT_TEXT,
		       KT_BG, 0);
	ktui_draw_flush();

	KtuiEvent ev;
	for (;;) {
		if (ktui_input_next(&ev, 1000) && ev.type == KT_EVT_KEY)
			break;
	}
}

int main(int argc, char **argv)
{
	kb_set_progname("kdosbuild");

	if (argc > 1 && !strcmp(argv[1], "--selftest"))
		return view_selftest();
	/* Matched on argv[1] rather than on argc so a short `--preview build`
	 * says what it wants instead of falling through to the option parser
	 * and reporting `--preview` itself as an unknown argument. */
	if (argc > 1 && !strcmp(argv[1], "--preview")) {
		if (argc < 5) {
			fprintf(stderr, "usage: kdosbuild --preview SCREEN WxH "
				"TIER  (see --help)\n");
			return 2;
		}
		return preview_main(argv[2], argv[3], argv[4]);
	}

	Args a = {0};
	a.build_dir = getenv("KDOS_BUILD_DIR");
	if (!a.build_dir)
		a.build_dir = "build";
	a.script_dir = "script";

	for (int i = 1; i < argc; i++) {
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(argv[i], "--build-dir") && v)
			a.build_dir = argv[++i];
		else if (!strcmp(argv[i], "--script-dir") && v)
			a.script_dir = argv[++i];
		else if (!strcmp(argv[i], "--fresh"))
			a.fresh = 1;
		else if (!strcmp(argv[i], "--restore") && v)
			a.restore = argv[++i];
		else if (!strcmp(argv[i], "--continue-from") && v)
			a.continue_from = argv[++i];
		else if (!strcmp(argv[i], "--no-snapshot"))
			a.no_snapshot = 1;
		else if (!strcmp(argv[i], "--plan"))
			a.want_plan = 1;
		else if (!strcmp(argv[i], "--phases") && v)
			a.phases = argv[++i];
		else if (!strcmp(argv[i], "--steps") && v)
			a.steps = argv[++i];
		else if (!strcmp(argv[i], "--rebuild") && v)
			a.rebuild = argv[++i];
		else if (!strcmp(argv[i], "--snapshot"))
			a.snapshot = 1;
		else if (!strcmp(argv[i], "--plain"))
			a.plain = 1;
		else if (!strcmp(argv[i], "--list"))
			a.list = 1;
		else if (!strcmp(argv[i], "--delete") && v)
			a.del = argv[++i];
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage();
			return 0;
		} else {
			fprintf(stderr, "unknown argument: %s\n", argv[i]);
			return 2;
		}
	}

	Manager m;
	mgr_init(&m, a.script_dir, a.build_dir);
	kb_mkdir_p(a.build_dir);

	if (a.list)
		return cmd_list(&m);

	if (a.del) {
		const KbuildPhase *t = resolve_phase(&m, a.del);
		if (!t) {
			fprintf(stderr, "unknown phase: %s\n", a.del);
			return 2;
		}
		printf(snap_delete(&m, t->dir_name) ? "deleted %s\n"
						    : "no snapshot for %s\n",
		       t->dir_name);
		return 0;
	}

	/* Resolve and validate out here: an error message is far more useful
	 * on a normal terminal than flashed through a full-screen TUI. */
	if (a.restore && a.continue_from) {
		fprintf(stderr,
			"--restore and --continue-from are mutually exclusive\n");
		return 2;
	}

	const KbuildPhase *continue_at = NULL, *preselected = NULL;
	if (a.continue_from) {
		continue_at = resolve_phase(&m, a.continue_from);
		if (!continue_at) {
			fprintf(stderr, "unknown phase for --continue-from: %s\n",
				a.continue_from);
			return 2;
		}
	}
	char err[512];
	if (a.restore) {
		preselected = resolve_phase(&m, a.restore);
		if (!preselected) {
			if (!strcmp(a.restore, "latest"))
				fprintf(stderr, "no snapshot to restore\n");
			else
				fprintf(stderr,
					"unknown phase for --restore: %s\n",
					a.restore);
			return 2;
		}
		if (check_restorable(&m, preselected, err, sizeof(err))) {
			fprintf(stderr, "%s\n", err);
			return 2;
		}
	}

	if (a.phases || a.steps || a.rebuild) {
		if (kbuild_plan_from_cli(&m.plan, a.phases, a.steps, a.rebuild,
					 m.phase, m.nphase, err,
					 sizeof(err)) < 0) {
			fprintf(stderr, "%s\n", err);
			return 2;
		}
		m.have_plan = 1;
		if (kbuild_plan_narrows(&m.plan) &&
		    (a.restore || a.continue_from)) {
			fprintf(stderr, "--phases/--steps select what runs; "
				"combine with neither --restore nor "
				"--continue-from\n");
			return 2;
		}
	}

	if (a.want_plan && !isatty(STDIN_FILENO)) {
		fprintf(stderr, "--plan needs a terminal; use "
			"--phases/--steps/--rebuild instead\n");
		return 2;
	}

	const char *term = getenv("TERM");
	int plain = a.plain || !isatty(STDOUT_FILENO) ||
		    (term && !strcmp(term, "dumb"));

	char timings_path[700];
	snprintf(timings_path, sizeof(timings_path), "%s/timings.json",
		 m.snap_root);
	Timings tm;
	tm_load(&tm, timings_path);
	m.timings = &tm;

	if (plain) {
		int prc = 0;

		m.snapshot_enabled = !a.no_snapshot;
		if (m.have_plan && kbuild_plan_narrows(&m.plan) && !a.snapshot)
			m.snapshot_enabled = 0;
		if (m.have_plan && kbuild_plan_custom(&m.plan)) {
			mgr_apply_plan(&m);
			kbuild_plan_save(&m.plan, a.build_dir);
			char sum[1024];
			kbuild_plan_summary(&m.plan, sum, sizeof(sum));
			mgr_notice(&m, "plan: %s", sum[0] ? sum : "everything");
			if (!m.snapshot_enabled && !a.no_snapshot)
				mgr_notice(&m, "snapshots disabled for this "
					   "partial run");
		}

		if (continue_at) {
			mgr_mark_continued(&m, continue_at->index);
			mgr_notice(&m, "continuing at %s on the existing tree "
				   "(no restore)", continue_at->dir_name);
		} else if (preselected) {
			if (check_restorable(&m, preselected, err, sizeof(err)) ||
			    do_restore(&m, preselected, 1, err, sizeof(err)) < 0) {
				fprintf(stderr, "%s\n", err);
				prc = 1;
				goto plain_out;
			}
		} else {
			char stale[128];
			if (kbuild_snap_interrupted(a.build_dir, stale,
						    sizeof(stale))) {
				fprintf(stderr,
					"a restore of %s never finished - build/ "
					"is inconsistent.\nrestore a snapshot "
					"again, or run 'make cleanbuild'.\n",
					stale[0] ? stale : "?");
				prc = 1;
				goto plain_out;
			}
		}

		Sampler sam;
		sam_init(&sam);
		run_plain(&m, &sam, &tm);
		prc = m.error_step ? 1 : 0;
plain_out:
		tm_save(&tm);
		tm_free(&tm);
		mgr_free(&m);
		return prc;
	}

	if (ktui_term_init(1) < 0) {
		fprintf(stderr, "cannot take over the terminal\n");
		return 1;
	}
	ktui_draw_init();
	ktui_input_init(1);

	int rc = 0;
	char commit[64];
	int dirty = 0;
	snap_git_info(".", commit, sizeof(commit), &dirty);

	/* Interactive entry points. A plan given on the command line wins. */
	KbuildSnapshot *probe = kb_calloc(KBUILD_MAX_PHASES, sizeof(*probe));
	int have_snaps = kbuild_snap_list(m.snap_root, probe,
					  KBUILD_MAX_PHASES) > 0;
	free(probe);

	if (!m.have_plan && a.want_plan) {
		if (!screen_plan(&m, &m.plan))
			goto quit;
		m.have_plan = 1;
	} else if (!m.have_plan && !preselected && !continue_at && !a.fresh &&
		   have_snaps && isatty(STDIN_FILENO)) {
		int index = 0;
		int choice = screen_startup(&m, &index, commit);
		if (choice == PICK_QUIT)
			goto quit;
		if (choice == PICK_RESTORE)
			preselected = &m.phase[index];
		else if (choice == PICK_PLAN) {
			if (!screen_plan(&m, &m.plan))
				goto quit;
			m.have_plan = 1;
		}
	}

	/* A plan that narrows anything suppresses snapshots: one taken from a
	 * partially re-run tree would be filed under a phase it no longer is. */
	m.snapshot_enabled = !a.no_snapshot;
	if (m.have_plan && kbuild_plan_narrows(&m.plan) && !a.snapshot)
		m.snapshot_enabled = 0;

	if (m.have_plan && kbuild_plan_custom(&m.plan)) {
		mgr_apply_plan(&m);
		kbuild_plan_save(&m.plan, a.build_dir);
		char sum[1024];
		kbuild_plan_summary(&m.plan, sum, sizeof(sum));
		mgr_notice(&m, "plan: %s", sum[0] ? sum : "everything");
		if (!m.snapshot_enabled && !a.no_snapshot)
			mgr_notice(&m, "snapshots disabled for this partial run");
	}

	if (continue_at) {
		mgr_mark_continued(&m, continue_at->index);
		mgr_notice(&m, "continuing at %s on the existing tree (no "
			   "restore)", continue_at->dir_name);
	} else if (preselected) {
		if (check_restorable(&m, preselected, err, sizeof(err)) ||
		    do_restore(&m, preselected, 0, err, sizeof(err)) < 0) {
			bail(err);
			rc = 1;
			goto quit;
		}
	} else {
		char stale[128];
		if (kbuild_snap_interrupted(a.build_dir, stale, sizeof(stale))) {
			/* Building on a half-extracted tree would bake the
			 * damage into the next snapshot. */
			snprintf(err, sizeof(err),
				 "a restore of %s never finished - build/ is "
				 "inconsistent.\nrestore a snapshot again, or "
				 "run 'make cleanbuild'.",
				 stale[0] ? stale : "?");
			bail(err);
			rc = 1;
			goto quit;
		}
	}

	{
		Sampler sam;
		sam_init(&sam);
		screen_build(&m, &sam, &tm);
		rc = m.error_step ? 1 : 0;
	}

quit:
	ktui_input_shutdown();
	ktui_term_shutdown();
	tm_save(&tm);
	tm_free(&tm);
	mgr_free(&m);
	return rc;
}
