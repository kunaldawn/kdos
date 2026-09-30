/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the command line
 *
 * The flags `make build BUILD_ARGS=...` passes through, and the whole of what
 * this program accepts: there is no second driver to agree with.
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
	int full_snapshots;
	int want_plan;
	const char *phases;
	const char *steps;
	const char *rebuild;
	int snapshot;
	int list;
	int plain;
	int json;
	const char *del;
	int port_jobs;
} Args;

/* Text or NDJSON, chosen once in main() and read by everything headless. */
static const Reporter *rep;

static void usage(void)
{
	printf(
"kdosbuild — KDOS build orchestrator\n"
"\n"
"  --build-dir DIR       build output directory (default: build)\n"
"  --script-dir DIR      script directory; phases are DIR/phases/NN_name\n"
"                        (default: script)\n"
"  --fresh               skip the startup picker and run every phase\n"
"  --restore PHASE       restore a snapshot and continue after it\n"
"                        (phase name, directory name, 1-based index, 'latest')\n"
"  --continue-from PHASE resume at PHASE on the existing tree, no restore\n"
"  --no-snapshot         do not write snapshots during this build; with a TUI\n"
"                        this is what the picker opens on, and S toggles it\n"
"  --full-snapshots      archive every snapshot path whole, never as a layer\n"
"                        on the snapshot before it\n"
"  --plan                open the build-plan picker and run it on this tree\n"
"  --phases LIST         only run these phases\n"
"  --steps LIST          only run these scripts, PHASE:script.sh\n"
"  --rebuild LIST        force-rebuild these ports even though installed\n"
"  --snapshot            write snapshots even for a partial plan\n"
"  --port-jobs N         build up to N ports of a package phase at once, by\n"
"                        dependency level (1-8; default 1, one at a time)\n"
"  --plain               no TUI: plain lines (implied by a non-tty stdout)\n"
"  --json                no TUI: one JSON object per event (NDJSON), and\n"
"                        --list prints the snapshot inventory as one object\n"
"  --list                list snapshots and exit\n"
"  --delete PHASE        delete one snapshot and exit; one that others layer\n"
"                        on is held until the last of them goes\n"
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
			if (kbuild_snapshottable(&m->phase[i]) &&
			    kbuild_snap_find(snaps, n, m->phase[i].dir_name))
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

/* The bytes a restore of `sn` reads for `path`: every archive in its chain. */
static long long chain_bytes(const KbuildSnapshot *all, int n,
			     const KbuildSnapshot *sn, const char *path,
			     int *count)
{
	const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
	int len = kbuild_snap_chain(all, n, sn, path, chain, KBUILD_MAX_CHAIN);
	long long total = 0;
	for (int i = 0; i < len; i++)
		total += kbuild_snap_entry(chain[i], path)->bytes_compressed;
	if (count)
		*count = len < 0 ? 0 : len;
	return total;
}

static long long own_bytes(const KbuildSnapshot *sn)
{
	long long total = 0;
	for (int k = 0; k < sn->nentries; k++)
		total += sn->entry[k].bytes_compressed;
	return total;
}

static int cmd_list(Manager *m, int json)
{
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int n = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);

	char commit[64];
	int dirty = 0;
	snap_git_info(".", commit, sizeof(commit), &dirty);

	/* An empty inventory is an empty ARRAY, not a message: "there are no
	 * snapshots" is an answer, and a consumer that has to tell it apart
	 * from a failure by reading prose has been handed the wrong thing. */
	if (json) {
		report_snapshots_json(m->phase, m->nphase, all, n, commit);
		free(all);
		return 0;
	}

	if (!n) {
		printf("no snapshots in %s\n", m->snap_root);
		free(all);
		return 0;
	}

	long long on_disk = 0, held_bytes = 0;
	for (int i = 0; i < n; i++) {
		on_disk += own_bytes(&all[i]);
		if (all[i].held)
			held_bytes += own_bytes(&all[i]);
	}

	printf("%-3s %-16s %-17s %9s %10s %6s  %s\n", "#", "PHASE", "WHEN",
	       "SIZE", "COMMIT", "STEPS", "KIND");
	KbBuf unusable = {0};
	for (int i = 0; i < m->nphase; i++) {
		const KbuildSnapshot *sn = kbuild_snap_find(all, n,
							   m->phase[i].dir_name);
		if (!sn)
			continue;
		if (!kbuild_snap_usable(all, n, sn)) {
			for (int k = 0; k < sn->nentries; k++) {
				const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
				if (kbuild_snap_chain(all, n, sn,
						      sn->entry[k].path, chain,
						      KBUILD_MAX_CHAIN) >= 0)
					continue;
				kb_buf_printf(&unusable, "    %-16s %s: base %s "
					      "missing\n", sn->dir_name,
					      sn->entry[k].path,
					      sn->entry[k].base_id);
				break;
			}
			continue;
		}
		int stale = sn->git_dirty ||
			    (commit[0] && sn->git_commit[0] &&
			     strcmp(sn->git_commit, commit));
		char cm[72];
		snprintf(cm, sizeof(cm), "%s%s",
			 sn->git_commit[0] ? sn->git_commit : "-",
			 stale ? "*" : "");
		char when[32], size[32], kind[96] = "full";
		kb_strlcpy(when, format_when(sn->created), sizeof(when));
		kb_strlcpy(size, human_bytes(own_bytes(sn)), sizeof(size));
		for (int k = 0; k < sn->nentries; k++) {
			if (!sn->entry[k].layer)
				continue;
			const KbuildSnapshot *b = kbuild_snap_by_id(
				all, n, sn->entry[k].base_id);
			snprintf(kind, sizeof(kind), "on %s%s",
				 b ? b->phase_dir : "?",
				 b && b->held ? " (held)" : "");
			break;
		}
		printf("%-3d %-16s %-17s %9s %10s %6d  %s%s\n", i + 1,
		       m->phase[i].dir_name, when, size, cm, sn->steps, kind,
		       kbuild_snapshottable(&m->phase[i]) ? "" :
		       "  leftover (phase declares no paths)");
		for (int k = 0; k < sn->nentries; k++) {
			const KbuildSnapEntry *e = &sn->entry[k];
			char comp[32], raw[32], reads[32];
			kb_strlcpy(comp, human_bytes(e->bytes_compressed),
				   sizeof(comp));
			kb_strlcpy(raw, human_bytes(e->bytes_raw), sizeof(raw));
			int links = 0;
			kb_strlcpy(reads, human_bytes(chain_bytes(all, n, sn,
								  e->path,
								  &links)),
				   sizeof(reads));
			printf("      %-12s %9s <- %9s  %s files", e->path, comp,
			       raw, human_count(e->files));
			if (e->layer)
				printf(", %lld removed; restore reads %s from "
				       "%d archives", e->removed_count, reads,
				       links);
			printf("\n");
		}
	}

	if (unusable.n)
		printf("\nunusable (a base is missing; --delete removes it):\n%s",
		       unusable.p);
	kb_buf_free(&unusable);

	int any_held = 0;
	for (int i = 0; i < n; i++) {
		if (!all[i].held)
			continue;
		if (!any_held++)
			printf("\nheld (bases of the snapshots named; freed "
			       "with the last of them):\n");
		int dep[KBUILD_MAX_SNAPS];
		int nd = kbuild_snap_dependants(all, n, all[i].id, dep,
						KBUILD_MAX_SNAPS);
		KbBuf needs = {0};
		for (int k = 0; k < nd; k++)
			if (!all[dep[k]].held)
				kb_buf_printf(&needs, "%s%s", needs.n ? " " : "",
					      all[dep[k]].dir_name);
		printf("    %-44s %9s  needed by %s\n", all[i].id,
		       human_bytes(own_bytes(&all[i])),
		       needs.n ? needs.p : "-");
		kb_buf_free(&needs);
	}

	char od[32];
	kb_strlcpy(od, human_bytes(on_disk), sizeof(od));
	printf("\non disk: %s (held %s)\n", od, human_bytes(held_bytes));
	free(all);
	return 0;
}

/* Why `target` cannot be restored, or NULL if it can.
 *
 * Restoring must NEVER silently fall back to an older phase: the plan is
 * layered, so it is non-empty whenever any earlier phase has a snapshot, and
 * marking the requested phase skipped on that basis would build the remaining
 * phases against a rootfs that never had the target's packages. */
/* The phases that declare snapshot paths, in order and with their .index
 * kept. A phase that declares none is re-run, never restored: a snapshot
 * directory left under its name by an earlier layout is a leftover, and
 * layering it into a restore would put an older tree back over the newer
 * layers below it. Freed by the caller. */
static KbuildPhase *snap_phases(const Manager *m, int *count)
{
	KbuildPhase *out = kb_calloc((size_t)(m->nphase ? m->nphase : 1),
				     sizeof(*out));
	int n = 0;
	for (int i = 0; i < m->nphase; i++)
		if (kbuild_snapshottable(&m->phase[i]))
			out[n++] = m->phase[i];
	*count = n;
	return out;
}

static const char *check_restorable(Manager *m, const KbuildPhase *target,
				    char *err, size_t errcap)
{
	if (!kbuild_snapshottable(target)) {
		snprintf(err, errcap,
			 "%s declares no snapshot paths; its snapshot is a "
			 "leftover", target->dir_name);
		return err;
	}

	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int n = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);

	if (!kbuild_snap_find(snaps, n, target->dir_name)) {
		/* On disk but with a base gone: absent, and said so, since the
		 * directory is right there in a listing. */
		KbuildSnapshot *one = kb_calloc(1, sizeof(*one));
		int there = kbuild_snap_load(m->snap_root, target->dir_name,
					     one) == 0;
		free(one);
		if (there) {
			snprintf(err, errcap, "the snapshot of %s is unusable: "
				 "a snapshot its layers are built on is "
				 "missing (--list names it; --delete removes "
				 "it)", target->dir_name);
			free(snaps);
			return err;
		}
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
	int nsp;
	KbuildPhase *sp = snap_phases(m, &nsp);
	KbuildRestoreItem *plan = kb_calloc(KBUILD_MAX_RESTORE, sizeof(*plan));
	int np = kbuild_snap_plan_restore(m->snap_root, sp, nsp,
					  target->index, plan,
					  KBUILD_MAX_RESTORE);
	free(sp);
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

/* snap_set_tick wants a plain function; the reporter is chosen at run time. */
static void snap_tick_hook(Manager *m)
{
	rep->snap_tick(m);
}

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
	int nsp;
	KbuildPhase *sp = snap_phases(m, &nsp);
	KbuildRestoreItem *plan = kb_calloc(KBUILD_MAX_RESTORE, sizeof(*plan));
	int n = kbuild_snap_plan_restore(m->snap_root, sp, nsp,
					 target->index, plan,
					 KBUILD_MAX_RESTORE);
	free(sp);
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
		snap_set_tick(m, snap_tick_hook);
		rep->restore(m, target->dir_name);
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
		if (!plan[i].seq)
			kb_buf_printf(&paths, "%s%s", paths.n ? ", " : "",
				      plan[i].path);
	mgr_notice(m, "restored %s%s (%s)", eff->dir_name,
		   partial ? " [partial - phase re-runs]" : "",
		   paths.p ? paths.p : "");
	kb_buf_free(&paths);
	free(plan);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Headless
 *
 * A driver that always took the terminal would draw a full-screen UI into a
 * log file whenever its output was redirected, and could not start at all
 * with no terminal. kpkg already answers TERM=dumb or a non-tty stdout with
 * plain lines; this is the same rule, and it is also what makes the engine
 * testable without a pty.
 *
 * ONE traversal, two renderings. `rep` is text or NDJSON (report.c); this loop
 * does not know which, so the two cannot disagree about what happened.
 */
static void run_plain(Manager *m, Sampler *sam, Timings *tm)
{
	snap_set_tick(m, snap_tick_hook);
	if (rep->begin)
		rep->begin(m);
	mgr_start(m);

	const BStep *announced = NULL;
	int shown_notices = 0;
	int low = 0;

	/* A step's result and any notice it produced are printed on the SAME
	 * pump turn the child exited on, because finish_phase() — which is
	 * where a snapshot happens — runs in that turn too. Deferring either
	 * to the next turn interleaves a snapshot line into the middle of the
	 * step line it belongs after.
	 *
	 * Steps are opened when they start and closed when they end, each once,
	 * found by walking the order: a level runs several at once, so opens
	 * and closes interleave, and a consumer matches them by phase and step.
	 * Everything before `low` has been reported or will never run, and the
	 * walk ends past the cursor and the level open at it — nothing later
	 * has started. */
	while (m->is_running) {
		int wait_ms = mgr_pump(m);
		sam_pump(sam, m);

		BStep *g = m->current_step;
		if (g && g->is_group && g != announced) {
			announced = g;
			rep->group(m, g);
		}
		int end = m->cursor > m->lvl_commit ? m->cursor : m->lvl_commit;
		for (int i = low; i < m->norder && i <= end; i++) {
			BStep *s = m->order[i];
			if (s->is_group || s->reported)
				continue;
			if (!s->opened && s->start_time > 0) {
				s->opened = 1;
				rep->step_open(m, s);
			}
			if (s->opened && s->end_time > 0 &&
			    s->status != ST_RUNNING) {
				rep->step_close(m, s);
				s->reported = 1;
			}
		}
		while (low < m->cursor && low < m->norder &&
		       (m->order[low]->is_group || m->order[low]->reported ||
			m->order[low]->start_time <= 0))
			low++;
		for (; shown_notices < m->nnotice; shown_notices++)
			rep->notice(m, m->notice[shown_notices].text);

		usleep((useconds_t)(wait_ms > 0 ? wait_ms : 5) * 1000);
	}

	for (; shown_notices < m->nnotice; shown_notices++)
		rep->notice(m, m->notice[shown_notices].text);

	rep->finish(m);

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
		/* A CLICK IS A KEY HERE. Mouse reporting is on for this
		 * program's own picker, so a press on the failure screen was
		 * swallowed and the build appeared to have hung on its last
		 * message — the one screen where that reading is worst. */
		if (!ktui_input_next(&ev, 1000))
			continue;
		if (ev.type == KT_EVT_KEY)
			break;
		if (ev.type == KT_EVT_MOUSE && ev.press == KT_MP_PRESS &&
		    (ev.btn == KT_MB_LEFT || ev.btn == KT_MB_RIGHT))
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
		else if (!strcmp(argv[i], "--full-snapshots"))
			a.full_snapshots = 1;
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
		else if (!strcmp(argv[i], "--json"))
			a.json = 1;
		else if (!strcmp(argv[i], "--list"))
			a.list = 1;
		else if (!strcmp(argv[i], "--delete") && v)
			a.del = argv[++i];
		else if (!strcmp(argv[i], "--port-jobs") && v) {
			char *end = NULL;
			long n = strtol(v, &end, 10);
			if (!*v || *end || n < 1 || n > KB_MAX_WORKERS) {
				fprintf(stderr, "--port-jobs takes 1 to %d, "
					"got '%s'\n", KB_MAX_WORKERS, v);
				return 2;
			}
			a.port_jobs = (int)n;
			i++;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage();
			return 0;
		} else {
			fprintf(stderr, "unknown argument: %s\n", argv[i]);
			return 2;
		}
	}

	rep = reporter_for(a.json);

	Manager m;
	mgr_init(&m, a.script_dir, a.build_dir);
	if (a.port_jobs)
		m.port_jobs = a.port_jobs;
	m.full_snapshots = a.full_snapshots;
	kb_mkdir_p(a.build_dir);

	/* The package store's salt: what the bootstrap phases built the chroot
	 * from, which no chroot-side key can see. Exported before any phase
	 * runs, so every chroot entry inherits it through exec.sh. */
	const char *ps = getenv("KDOS_PKG_STORE");
	if (ps && (!strcmp(ps, "1") || !strcmp(ps, "check"))) {
		char salt[65];
		if (kp_store_salt(m.repo_root, salt) == 0)
			setenv("KPKG_STORE_SALT", salt, 1);
		else
			fprintf(stderr, "kdosbuild: no package-store salt "
				"under %s; the store stays off\n",
				m.repo_root);
	}

	/* A phase that cannot run is named on every invocation, and refuses a
	 * build before its first step: found half-way through, it is hours of
	 * build later and a phase that silently ran the wrong list. */
	int broken = 0;
	for (int i = 0; i < m.nphase; i++)
		if (m.phase[i].error[0]) {
			fprintf(stderr, "kdosbuild: %s\n", m.phase[i].error);
			broken = 1;
		}

	if (a.list)
		return cmd_list(&m, a.json);

	if (a.del) {
		const KbuildPhase *t = resolve_phase(&m, a.del);
		if (!t) {
			fprintf(stderr, "unknown phase: %s\n", a.del);
			return 2;
		}
		char deps[256];
		int drc = snap_delete(&m, t->dir_name, deps, sizeof(deps));
		if (drc == 2)
			printf("deleted %s; kept as the base of %s\n",
			       t->dir_name, deps);
		else
			printf(drc ? "deleted %s\n" : "no snapshot for %s\n",
			       t->dir_name);
		return 0;
	}

	if (broken)
		return 2;
	/* No phases is a script directory this driver cannot read, not a build
	 * with nothing to do: it would report BUILD COMPLETE having run
	 * nothing. */
	if (!m.nphase) {
		fprintf(stderr, "kdosbuild: no phase under %s/" KBUILD_PHASES_DIR
			"/\n", a.script_dir);
		return 2;
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

	/* --plan is a PICKER, and a run that is emitting NDJSON has nobody at
	 * the terminal to use it. Refused rather than silently ignored. */
	if (a.want_plan && (!isatty(STDIN_FILENO) || a.json)) {
		fprintf(stderr, "--plan needs a terminal; use "
			"--phases/--steps/--rebuild instead\n");
		return 2;
	}

	const char *term = getenv("TERM");
	int plain = a.plain || a.json || !isatty(STDOUT_FILENO) ||
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
	/* THE BUILD MUST NOT BE STOPPABLE BY ITS OWN OUTPUT. A snapshot drains
	 * tar's member names from a 64 K pipe in the same loop that redraws
	 * this screen, so a terminal that stops reading — a pager, a paused
	 * pty, a stalled ssh — would block the redraw, starve the drain, and
	 * wedge tar against its full pipe with hours of work outstanding. A
	 * frame is worth two seconds and no more; the archive is worth the
	 * build. */
	ktui_term_set_write_timeout(2000);
	ktui_draw_init();
	ktui_input_init(1);

	int rc = 0;
	char commit[64];
	int dirty = 0;
	snap_git_info(".", commit, sizeof(commit), &dirty);

	/* The command line sets what the picker OPENS on; what the operator
	 * leaves it at is what the build uses. */
	m.snapshot_enabled = !a.no_snapshot;

	/* Interactive entry points. A plan given on the command line wins.
	 *
	 * THE PICKER OPENS WHETHER OR NOT A SNAPSHOT EXISTS. Both questions it
	 * answers — restore from which phase, and write snapshots at all —
	 * have to be asked on a tree with none, because that is precisely the
	 * from-scratch run where the second one costs gigabytes and a part of
	 * the run's time. Its row 0 is "start fresh", so a tree with no snapshot simply
	 * opens on the only restore choice there is. */
	if (!m.have_plan && a.want_plan) {
		if (!screen_plan(&m, &m.plan))
			goto quit;
		m.have_plan = 1;
	} else if (!m.have_plan && !preselected && !continue_at && !a.fresh &&
		   isatty(STDIN_FILENO)) {
		int index = 0;
		int choice = screen_startup(&m, &index, commit,
					    &m.snapshot_enabled);
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
