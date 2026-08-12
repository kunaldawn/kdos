/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — headless reporting, in text and in JSON
 *
 * The build's progress reaches a person one of two ways, and BOTH GO THROUGH
 * THIS ONE TRAVERSAL. run_plain() walks the step tree once and calls these
 * hooks; text and JSON are two implementations of the same six events, not two
 * passes over the build. A second walk would be a second thing to keep true,
 * and the interesting failures — a step that ran but was never reported, a
 * snapshot attributed to the wrong phase — are exactly the ones a second walk
 * would hide.
 *
 * WHY JSON AT ALL: a build is the longest-running thing in this repo and the
 * hardest to assert about, because its output is prose. `--json` makes the
 * engine's decisions machine-checkable — which steps ran, in what order, which
 * were skipped, which phase got a snapshot and how big it was — so
 * testing/selftest.sh can check the ENGINE rather than grep for "BUILD
 * COMPLETE". That is the point of it: testability, not a service interface.
 *
 * NDJSON, one object per line, never one document. A build runs for hours and
 * can be killed at any moment; a single top-level object is only valid once its
 * closing brace arrives, so the one case a machine-readable log exists for —
 * reading the tail of a build that died — is the case it would not cover.
 *
 * An error stays on stderr and out of the stream. A consumer that gets a parse
 * failure plus a non-zero exit knows more than one handed a well-formed object
 * describing nothing.
 */

#include <stdio.h>
#include <string.h>

#include "kdosbuild.h"

/* ── text ──────────────────────────────────────────────────────────────── */

/* Set while a step line is open and waiting for its "ok"/"FAILED". A snapshot
 * starts INSIDE that window, so its progress has to break the line rather than
 * land in the middle of it. */
static int line_open;

static void break_line(void)
{
	if (line_open) {
		printf("\n");
		line_open = 0;
	}
}

static void t_group(const Manager *m, const BStep *s)
{
	(void)m;
	printf("==> %s\n", s->title);
	fflush(stdout);
}

static void t_step_open(const Manager *m, const BStep *s)
{
	(void)m;
	printf("    %-40s ", s->title);
	line_open = 1;
	fflush(stdout);
}

static void t_step_close(const Manager *m, const BStep *s)
{
	(void)m;
	/* The title is reprinted when a notice or a snapshot broke the line
	 * this result belongs on. */
	if (!line_open)
		printf("    %-40s ", s->title);
	line_open = 0;
	printf("%s (%s)\n", s->status == ST_DONE ? "ok" : "FAILED",
	       human_time(step_duration(s)));
	fflush(stdout);
}

static void t_notice(const Manager *m, const char *text)
{
	(void)m;
	break_line();
	printf("... %s\n", text);
	fflush(stdout);
}

static void t_restore(const Manager *m, const char *phase)
{
	(void)m;
	printf("==> restoring %s\n", phase);
	fflush(stdout);
}

/*
 * Snapshot and restore progress: one line when the path changes, then a
 * heartbeat, so a forty-minute tar is visibly alive in a log file.
 */
static void t_snap_tick(const Manager *m)
{
	static double last;
	static char seen[160];

	char now[160];
	snprintf(now, sizeof(now), "%s %s", m->snap.action, m->snap.path);
	if (strcmp(seen, now)) {
		kb_strlcpy(seen, now, sizeof(seen));
		break_line();
		printf("    %s %s\n", m->snap.action, m->snap.path);
		fflush(stdout);
		last = kb_now_s();
		return;
	}
	if (kb_now_s() - last < 15)
		return;
	last = kb_now_s();
	break_line();
	printf("      %s / %s   %lld files\n", human_bytes(m->snap.bytes),
	       m->snap.est_bytes ? human_bytes(m->snap.est_bytes) : "?",
	       m->snap.files);
	fflush(stdout);
}

static void t_finish(const Manager *m)
{
	if (m->error_step)
		printf("BUILD FAILED at %s\n", m->error_step->title);
	else
		printf("BUILD COMPLETE in %s\n",
		       human_time(kb_now_s() - m->start_time));
	fflush(stdout);
}

/* ── json ──────────────────────────────────────────────────────────────── */

static void emit(KbBuf *b)
{
	kb_buf_str(b, "}\n");
	fwrite(b->p, 1, b->n, stdout);
	fflush(stdout);
	kb_buf_free(b);
}

static const char *status_name(int st)
{
	switch (st) {
	case ST_PENDING: return "pending";
	case ST_RUNNING: return "running";
	case ST_DONE:    return "ok";
	case ST_FAIL:    return "failed";
	case ST_SKIPPED: return "skipped";
	}
	return "?";
}

/* The phase a step belongs to, by NAME. Every event carries it, because a step
 * name is not unique across phases — `packages.txt` phases expand to a port
 * name and several ports appear in more than one. */
static void json_phase_of(KbBuf *b, const char *key, const BStep *s)
{
	BStep *g = mgr_phase_of((BStep *)s);
	kb_buf_printf(b, ", \"%s\": ", key);
	kb_json_str(b, g && g->meta ? g->meta->dir_name : "");
}

/* A commit nobody recorded is `null`, not "". kb_json_str renders a NULL
 * pointer as an empty string, which here would read as a commit whose id is
 * the empty string — a value, not an absence. */
static void json_or_null(KbBuf *b, const char *s)
{
	if (s && *s)
		kb_json_str(b, s);
	else
		kb_buf_str(b, "null");
}

/*
 * There is no total step count here and deliberately no estimate of one: a
 * `packages.txt` phase expands to its ports only when the phase is entered, so
 * before the build starts the tree is phases and nothing else. A number that
 * grows as the build runs is worse than no number — it reads as progress
 * against a denominator that is not real. The per-phase event carries the
 * count once it exists.
 */
static void j_begin(const Manager *m)
{
	KbBuf b = {0};

	kb_buf_str(&b, "{\"event\": \"build\", \"build_dir\": ");
	kb_json_str(&b, m->build_dir);
	kb_buf_str(&b, ", \"script_dir\": ");
	kb_json_str(&b, m->script_dir);
	kb_buf_printf(&b, ", \"phases\": %d, \"snapshots\": %s", m->nroot,
		      m->snapshot_enabled ? "true" : "false");
	emit(&b);
}

static void j_group(const Manager *m, const BStep *s)
{
	(void)m;
	KbBuf b = {0};
	kb_buf_str(&b, "{\"event\": \"phase\", \"phase\": ");
	kb_json_str(&b, s->meta ? s->meta->dir_name : s->title);
	kb_buf_str(&b, ", \"title\": ");
	kb_json_str(&b, s->title);
	kb_buf_printf(&b, ", \"steps\": %d", s->nchild);
	emit(&b);
}

static void j_step_open(const Manager *m, const BStep *s)
{
	(void)m;
	KbBuf b = {0};
	kb_buf_str(&b, "{\"event\": \"step\", \"status\": \"running\", "
		       "\"step\": ");
	kb_json_str(&b, s->title);
	json_phase_of(&b, "phase", s);
	emit(&b);
}

static void j_step_close(const Manager *m, const BStep *s)
{
	KbBuf b = {0};
	char log[1024];

	log_path_for(m, s, log, sizeof(log));
	kb_buf_str(&b, "{\"event\": \"step\", \"status\": ");
	kb_json_str(&b, status_name(s->status));
	kb_buf_str(&b, ", \"step\": ");
	kb_json_str(&b, s->title);
	json_phase_of(&b, "phase", s);
	kb_buf_printf(&b, ", \"seconds\": %.2f, \"rc\": %d, \"lines\": %d",
		      step_duration(s), s->return_code, s->nlog);
	kb_buf_str(&b, ", \"log\": ");
	kb_json_str(&b, log);
	emit(&b);
}

static void j_notice(const Manager *m, const char *text)
{
	(void)m;
	KbBuf b = {0};
	kb_buf_str(&b, "{\"event\": \"notice\", \"text\": ");
	kb_json_str(&b, text);
	emit(&b);
}

static void j_restore(const Manager *m, const char *phase)
{
	(void)m;
	KbBuf b = {0};
	kb_buf_str(&b, "{\"event\": \"restore\", \"phase\": ");
	kb_json_str(&b, phase);
	emit(&b);
}

/*
 * One event per (action, path), not per tick. The text heartbeat exists so a
 * human can see a long tar is alive; a consumer reading NDJSON has the process
 * for that, and a line every 15 seconds for forty minutes is noise it would
 * have to filter.
 */
static void j_snap_tick(const Manager *m)
{
	static char seen[160];
	char now[160];

	snprintf(now, sizeof(now), "%s %s", m->snap.action, m->snap.path);
	if (!strcmp(seen, now))
		return;
	kb_strlcpy(seen, now, sizeof(seen));

	KbBuf b = {0};
	kb_buf_str(&b, "{\"event\": \"snapshot\", \"action\": ");
	kb_json_str(&b, m->snap.action);
	kb_buf_str(&b, ", \"phase\": ");
	kb_json_str(&b, m->snap.phase);
	kb_buf_str(&b, ", \"path\": ");
	kb_json_str(&b, m->snap.path);
	emit(&b);
}

static void j_finish(const Manager *m)
{
	KbBuf b = {0};
	int ok = 0, failed = 0, skipped = 0;

	for (int i = 0; i < m->norder; i++) {
		const BStep *s = m->order[i];
		if (s->is_group)
			continue;
		if (s->status == ST_DONE)
			ok++;
		else if (s->status == ST_FAIL)
			failed++;
		else if (s->status == ST_SKIPPED)
			skipped++;
	}

	kb_buf_printf(&b, "{\"event\": \"result\", \"status\": \"%s\"",
		      m->error_step ? "failed" : "complete");
	kb_buf_printf(&b, ", \"seconds\": %.2f, \"ok\": %d, \"failed\": %d, "
		      "\"skipped\": %d", kb_now_s() - m->start_time, ok, failed,
		      skipped);
	if (m->error_step) {
		kb_buf_str(&b, ", \"failed_step\": ");
		kb_json_str(&b, m->error_step->title);
		json_phase_of(&b, "failed_phase", m->error_step);
	}
	emit(&b);
}

/* ── the two tables ────────────────────────────────────────────────────── */

static const Reporter reporter_text = {
	.begin = NULL,
	.group = t_group,
	.step_open = t_step_open,
	.step_close = t_step_close,
	.notice = t_notice,
	.restore = t_restore,
	.snap_tick = t_snap_tick,
	.finish = t_finish,
};

static const Reporter reporter_json = {
	.begin = j_begin,
	.group = j_group,
	.step_open = j_step_open,
	.step_close = j_step_close,
	.notice = j_notice,
	.restore = j_restore,
	.snap_tick = j_snap_tick,
	.finish = j_finish,
};

const Reporter *reporter_for(int json)
{
	return json ? &reporter_json : &reporter_text;
}

/* ── the snapshot inventory ────────────────────────────────────────────── */

/*
 * `--list --json`. The text form is a table with human sizes and a `*` for a
 * stale commit; both read the same inventory, and the flag decides only how it
 * is printed. `stale` is computed here rather than left to the consumer because
 * it is a comparison against the WORKING TREE's commit, which the consumer does
 * not have.
 */
void report_snapshots_json(const KbuildPhase *ph, int nph,
			   const KbuildSnapshot *snaps, int n,
			   const char *commit)
{
	KbBuf b = {0};

	kb_buf_str(&b, "{\n  \"commit\": ");
	json_or_null(&b, commit);
	kb_buf_str(&b, ",\n  \"snapshots\": [");

	int written = 0;
	for (int i = 0; i < nph; i++) {
		const KbuildSnapshot *sn = kbuild_snap_find(snaps, n,
							   ph[i].dir_name);
		if (!sn)
			continue;
		int stale = sn->git_dirty ||
			    (commit && commit[0] && sn->git_commit[0] &&
			     strcmp(sn->git_commit, commit));

		kb_buf_printf(&b, "%s\n    {\"index\": %d, \"phase\": ",
			      written++ ? "," : "", i + 1);
		kb_json_str(&b, ph[i].dir_name);
		kb_buf_str(&b, ", \"title\": ");
		kb_json_str(&b, sn->title);
		kb_buf_str(&b, ", \"created\": ");
		kb_json_str(&b, sn->created_iso);
		kb_buf_str(&b, ", \"commit\": ");
		json_or_null(&b, sn->git_commit);
		kb_buf_printf(&b, ", \"dirty\": %s, \"stale\": %s, "
			      "\"complete\": %s",
			      sn->git_dirty ? "true" : "false",
			      stale ? "true" : "false",
			      sn->complete ? "true" : "false");
		kb_buf_printf(&b, ", \"steps\": %d, \"total_steps\": %d, "
			      "\"duration\": %.2f, \"snapshot_seconds\": %.2f",
			      sn->steps, sn->total_steps, sn->duration_s,
			      sn->snapshot_s);
		kb_buf_str(&b, ", \"codec\": ");
		kb_json_str(&b, sn->codec);
		kb_buf_str(&b, ", \"entries\": [");
		for (int k = 0; k < sn->nentries; k++) {
			kb_buf_str(&b, k ? ", {\"path\": " : "{\"path\": ");
			kb_json_str(&b, sn->entry[k].path);
			kb_buf_str(&b, ", \"archive\": ");
			kb_json_str(&b, sn->entry[k].archive);
			kb_buf_printf(&b, ", \"bytes\": %lld, \"bytes_raw\": "
				      "%lld, \"files\": %lld}",
				      sn->entry[k].bytes_compressed,
				      sn->entry[k].bytes_raw,
				      sn->entry[k].files);
		}
		kb_buf_str(&b, "]}");
	}
	kb_buf_printf(&b, "%s],\n  \"count\": %d\n}\n", written ? "\n  " : "",
		      written);
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
}
