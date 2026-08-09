/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the screens
 *
 * On libktui, which is why buildlib/tui.py's 1220 lines of python curses can
 * go. Everything here draws in the SAME eight colour slots the installer and
 * kdos-appbox use, so a tty and a truecolor terminal render one picture.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdosbuild.h"

static const char *LOGO[] = {
	"██╗  ██╗██████╗  ██████╗ ███████╗",
	"██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝",
	"█████╔╝ ██║  ██║██║   ██║███████╗",
	"██╔═██╗ ██║  ██║██║   ██║╚════██║",
	"██║  ██╗██████╔╝╚██████╔╝███████║",
	"╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝",
};
#define LOGO_H 6
#define LOGO_W 33

static const char *SPINNER[] = { "⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷" };
static const char *SPINNER_ASCII[] = { "|", "/", "-", "\\", "|", "/", "-", "\\" };

static const char *spin(void)
{
	int i = (int)(kb_now_s() * 10) & 7;
	return (ktui_caps & KT_CAP_UTF8) ? SPINNER[i] : SPINNER_ASCII[i];
}

static void centre(int y, const char *s, int fg, int attr)
{
	int w = ktui_utf8_width(s);
	int x = (ktui_w - w) / 2;
	ktui_draw_text(x < 2 ? 2 : x, y, ktui_w - 2, s, fg, KT_BG, attr);
}

static void keyhint(int y, const char *const *keys, const char *const *labels,
		    int n)
{
	int x = 2;
	for (int i = 0; i < n; i++) {
		char k[16];
		snprintf(k, sizeof(k), "[%s]", keys[i]);
		x += ktui_draw_text(x, y, ktui_w - x, k, KT_WARN, KT_BG,
				    KT_A_BOLD);
		char l[32];
		snprintf(l, sizeof(l), ":%s ", labels[i]);
		x += ktui_draw_text(x, y, ktui_w - x, l, KT_TEXT, KT_BG, 0);
	}
}

/* A snapshot taken from a different or dirty commit. Not an error — the tree
 * it restores simply is not the tree the working copy describes. */
static int snap_stale(const KbuildSnapshot *sn, const char *commit)
{
	if (sn->git_dirty)
		return 1;
	if (!commit || !*commit || !sn->git_commit[0])
		return 0;
	return strcmp(sn->git_commit, commit) != 0;
}

static long long snap_size(const KbuildSnapshot *sn)
{
	long long total = 0;
	for (int i = 0; i < sn->nentries; i++)
		total += sn->entry[i].bytes_compressed;
	return total;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The startup picker                                                       */

int screen_startup(Manager *m, int *index, const char *commit)
{
	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int nsnap = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);

	/* Row 0 is "start fresh"; the rest are phases that have a snapshot. */
	int row_phase[KBUILD_MAX_PHASES + 1];
	int nrow = 1;
	row_phase[0] = -1;
	for (int i = 0; i < m->nphase; i++)
		if (kbuild_snap_find(snaps, nsnap, m->phase[i].dir_name))
			row_phase[nrow++] = i;

	int sel = nrow > 1 ? nrow - 1 : 0;
	char status[256] = "";
	int result = PICK_QUIT;

	for (;;) {
		ktui_draw_clear();
		int y = 1;

		if (ktui_h > LOGO_H + 12) {
			for (int i = 0; i < LOGO_H; i++) {
				int x = (ktui_w - LOGO_W) / 2;
				ktui_draw_text(x < 2 ? 2 : x, y + i, ktui_w - 2,
					       LOGO[i], KT_ACCENT, KT_BG,
					       KT_A_BOLD);
			}
			y += LOGO_H + 1;
		}

		centre(y, "BUILD SNAPSHOTS", KT_WARN, KT_A_BOLD);
		y += 2;

		char header[160];
		snprintf(header, sizeof(header), "   %-16s %-17s %8s %10s %7s %9s",
			 "PHASE", "WHEN", "SIZE", "COMMIT", "STEPS", "BUILD");
		ktui_draw_text(2, y++, ktui_w - 4, header, KT_ACCENT, KT_BG,
			       KT_A_BOLD);
		ktui_draw_hline(2, y++, ktui_w - 4, KT_G_HL, KT_DIM, KT_BG);

		int any_stale = 0, any_partial = 0;
		for (int r = 0; r < nrow && y < ktui_h - 8; r++, y++) {
			int selrow = r == sel;
			int fg = selrow ? KT_BG : KT_TEXT;
			int bg = selrow ? KT_ACCENT : KT_BG;
			char text[256];

			if (row_phase[r] < 0) {
				snprintf(text, sizeof(text), "   %-16s %s",
					 "start fresh",
					 "run every phase from scratch   "
					 "([P]: pick phases/ports instead)");
			} else {
				const KbuildPhase *p = &m->phase[row_phase[r]];
				const KbuildSnapshot *sn =
					kbuild_snap_find(snaps, nsnap, p->dir_name);
				int stale = snap_stale(sn, commit);
				any_stale |= stale;
				any_partial |= !sn->complete;

				char cm[72];
				snprintf(cm, sizeof(cm), "%s%s",
					 sn->git_commit[0] ? sn->git_commit : "-",
					 stale ? "*" : "");
				char steps[32];
				if (sn->complete)
					snprintf(steps, sizeof(steps), "%d",
						 sn->steps);
				else
					snprintf(steps, sizeof(steps), "%d/%d!",
						 sn->steps, sn->total_steps);

				char when[32], size[32], dur[32];
				kb_strlcpy(when, format_when(sn->created),
					   sizeof(when));
				kb_strlcpy(size, human_bytes(snap_size(sn)),
					   sizeof(size));
				kb_strlcpy(dur, human_time(sn->duration_s),
					   sizeof(dur));
				snprintf(text, sizeof(text),
					 "%2d %-16s %-17s %8s %10s %7s %9s", r,
					 p->dir_name, when, size, cm, steps, dur);
			}
			ktui_draw_fill(krect(2, y, ktui_w - 4, 1), bg);
			ktui_draw_text(2, y, ktui_w - 4, text, fg, bg,
				       selrow ? KT_A_BOLD : 0);
		}

		if (nrow == 1) {
			ktui_draw_text(4, ++y, ktui_w - 6,
				       "no snapshots yet — the first build will "
				       "create them", KT_DIM, KT_BG, 0);
			y++;
		}
		y++;

		if (any_stale)
			ktui_draw_text(2, y++, ktui_w - 4,
				       "* snapshot taken from a different or "
				       "dirty commit", KT_WARN, KT_BG, 0);
		if (any_partial)
			ktui_draw_text(2, y++, ktui_w - 4,
				       "! partial snapshot — restoring it "
				       "re-runs that phase", KT_WARN, KT_BG, 0);

		char stale_target[128];
		if (kbuild_snap_interrupted(m->build_dir, stale_target,
					    sizeof(stale_target))) {
			char msg[384];
			snprintf(msg, sizeof(msg),
				 "INTERRUPTED RESTORE of %s — build/ is "
				 "inconsistent; restore again or 'make cleanbuild'",
				 stale_target[0] ? stale_target : "?");
			ktui_draw_text(2, y++, ktui_w - 4, msg, KT_ERR, KT_BG,
				       KT_A_BOLD);
		}

		if (row_phase[sel] >= 0) {
			const KbuildPhase *p = &m->phase[row_phase[sel]];
			const KbuildSnapshot *sn =
				kbuild_snap_find(snaps, nsnap, p->dir_name);
			KbBuf b = {0};
			kb_buf_str(&b, "restores: ");
			for (int i = 0; i < sn->nentries; i++)
				kb_buf_printf(&b, "%s%s %s", i ? "  " : "",
					      sn->entry[i].path,
					      human_bytes(sn->entry[i].bytes_compressed));
			ktui_draw_text(2, y++, ktui_w - 4, b.p ? b.p : "",
				       KT_MID, KT_BG, 0);
			kb_buf_free(&b);

			char cont[128];
			snprintf(cont, sizeof(cont), "continues from: %s",
				 p->index + 1 < m->nphase
					 ? m->phase[p->index + 1].dir_name
					 : "nothing left");
			ktui_draw_text(2, y++, ktui_w - 4, cont, KT_MID, KT_BG, 0);
		}

		if (status[0])
			ktui_draw_text(2, y + 1, ktui_w - 4, status, KT_WARN,
				       KT_BG, 0);

		static const char *keys[] = { "↑↓", "ENTER", "P", "D", "Q" };
		static const char *labels[] = { "select", "start", "plan",
						"delete", "quit" };
		keyhint(ktui_h - 2, keys, labels, 5);

		char foot[640];
		snprintf(foot, sizeof(foot), "codec: %s   snapshots: %s",
			 snap_codec(), m->snap_root);
		ktui_draw_text(2, ktui_h - 1, ktui_w - 4, foot, KT_DIM, KT_BG, 0);
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_input_next(&ev, 200))
			continue;
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (ev.key == 'q' || ev.key == 'Q' || ev.key == KT_K_ESC) {
			result = PICK_QUIT;
			break;
		}
		if (ev.key == KT_K_UP || ev.key == 'k')
			sel = sel > 0 ? sel - 1 : 0;
		else if (ev.key == KT_K_DOWN || ev.key == 'j')
			sel = sel < nrow - 1 ? sel + 1 : sel;
		else if (ev.key == 'p' || ev.key == 'P') {
			result = PICK_PLAN;
			break;
		} else if (ev.key == 'd' || ev.key == 'D') {
			if (row_phase[sel] < 0) {
				kb_strlcpy(status, "nothing to delete",
					   sizeof(status));
			} else {
				const KbuildPhase *p = &m->phase[row_phase[sel]];
				snap_delete(m, p->dir_name);
				snprintf(status, sizeof(status), "deleted %s",
					 p->dir_name);
				nsnap = kbuild_snap_list(m->snap_root, snaps,
							 KBUILD_MAX_PHASES);
				nrow = 1;
				for (int i = 0; i < m->nphase; i++)
					if (kbuild_snap_find(snaps, nsnap,
							     m->phase[i].dir_name))
						row_phase[nrow++] = i;
				if (sel >= nrow)
					sel = nrow - 1;
			}
		} else if (ev.key == KT_K_ENTER || ev.key == '\n') {
			if (row_phase[sel] < 0) {
				result = PICK_FRESH;
			} else {
				result = PICK_RESTORE;
				*index = row_phase[sel];
			}
			break;
		}
	}

	free(snaps);
	return result;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The package picker, reached with '/' from the plan screen                */

typedef struct {
	KbuildPkgRef *pkg;
	int npkg;
	char (*chosen)[64];
	int *nchosen;
} PickState;

static int pick_has(PickState *st, const char *name)
{
	for (int i = 0; i < *st->nchosen; i++)
		if (!strcmp(st->chosen[i], name))
			return 1;
	return 0;
}

static void pick_toggle(PickState *st, const char *name)
{
	for (int i = 0; i < *st->nchosen; i++)
		if (!strcmp(st->chosen[i], name)) {
			memmove(st->chosen[i], st->chosen[i + 1],
				(size_t)(*st->nchosen - i - 1) * 64);
			(*st->nchosen)--;
			return;
		}
	if (*st->nchosen < KBUILD_MAX_REBUILD)
		kb_strlcpy(st->chosen[(*st->nchosen)++], name, 64);
}

static void screen_packages(PickState *st)
{
	char filter[64] = "";
	int sel = 0, off = 0;
	int *match = kb_calloc((size_t)st->npkg + 1, sizeof(*match));

	for (;;) {
		int nmatch = 0;
		for (int i = 0; i < st->npkg; i++)
			if (!filter[0] || strstr(st->pkg[i].name, filter))
				match[nmatch++] = i;
		if (sel >= nmatch)
			sel = nmatch ? nmatch - 1 : 0;

		ktui_draw_clear();
		centre(1, "FORCE REBUILD — type to filter, SPACE to toggle",
		       KT_WARN, KT_A_BOLD);

		char line[160];
		snprintf(line, sizeof(line), "filter: %s_", filter);
		ktui_draw_text(2, 3, ktui_w - 4, line, KT_TEXT, KT_BG, KT_A_BOLD);
		snprintf(line, sizeof(line), "%d selected, %d of %d shown",
			 *st->nchosen, nmatch, st->npkg);
		ktui_draw_text(2, 4, ktui_w - 4, line, KT_DIM, KT_BG, 0);

		int top = 6, rows = ktui_h - top - 3;
		if (sel < off)
			off = sel;
		if (sel >= off + rows)
			off = sel - rows + 1;

		for (int i = 0; i < rows && off + i < nmatch; i++) {
			const KbuildPkgRef *p = &st->pkg[match[off + i]];
			int on = pick_has(st, p->name);
			int selrow = off + i == sel;
			int fg = selrow ? KT_BG : (on ? KT_WARN : KT_TEXT);
			int bg = selrow ? KT_ACCENT : KT_BG;
			snprintf(line, sizeof(line), " %s %-28s %s",
				 on ? "[x]" : "[ ]", p->name,
				 p->phase[0] ? p->phase : "-");
			ktui_draw_fill(krect(2, top + i, ktui_w - 4, 1), bg);
			ktui_draw_text(2, top + i, ktui_w - 4, line, fg, bg,
				       selrow ? KT_A_BOLD : 0);
		}

		static const char *keys[] = { "↑↓", "SPACE", "BKSP", "ENTER" };
		static const char *labels[] = { "select", "toggle", "clear",
						"done" };
		keyhint(ktui_h - 2, keys, labels, 4);
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_input_next(&ev, 200))
			continue;
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (ev.key == KT_K_ESC || ev.key == KT_K_ENTER || ev.key == '\n')
			break;
		if (ev.key == KT_K_UP)
			sel = sel > 0 ? sel - 1 : 0;
		else if (ev.key == KT_K_DOWN)
			sel = sel + 1 < nmatch ? sel + 1 : sel;
		else if (ev.key == ' ') {
			if (nmatch)
				pick_toggle(st, st->pkg[match[sel]].name);
		} else if (ev.key == KT_K_BACKSPACE) {
			size_t n = strlen(filter);
			if (n)
				filter[n - 1] = 0;
		} else if (ev.key >= 32 && ev.key < 127) {
			size_t n = strlen(filter);
			if (n < sizeof(filter) - 1) {
				filter[n] = (char)ev.key;
				filter[n + 1] = 0;
			}
		}
	}
	free(match);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The plan picker                                                          */

int screen_plan(Manager *m, KbuildPlan *out)
{
	int phase_on[KBUILD_MAX_PHASES];
	char **steps[KBUILD_MAX_PHASES];
	int nsteps[KBUILD_MAX_PHASES];
	int step_on[KBUILD_MAX_PHASES][KBUILD_MAX_STEPS];
	int expanded[KBUILD_MAX_PHASES];

	for (int i = 0; i < m->nphase; i++) {
		phase_on[i] = 1;
		expanded[i] = 0;
		steps[i] = kbuild_steps(&m->phase[i], &nsteps[i]);
		for (int k = 0; k < nsteps[i]; k++)
			step_on[i][k] = 1;
	}

	KbuildPkgRef *pkg = kb_calloc(2048, sizeof(*pkg));
	int npkg = kbuild_package_index(m->phase, m->nphase, m->repo_root, pkg,
					2048);
	char (*rebuild)[64] = kb_calloc(KBUILD_MAX_REBUILD, sizeof(*rebuild));
	int nrebuild = 0;

	/* Seed from a plan already on disk, so re-opening the picker shows
	 * what the last run selected rather than everything. */
	KbuildPlan preset;
	if (kbuild_plan_load(&preset, m->build_dir) == 0) {
		if (preset.has_phases)
			for (int i = 0; i < m->nphase; i++) {
				phase_on[i] = 0;
				for (int k = 0; k < preset.nphase; k++)
					if (!strcmp(preset.phase[k],
						    m->phase[i].dir_name))
						phase_on[i] = 1;
			}
		for (int s = 0; s < preset.nsteps; s++)
			for (int i = 0; i < m->nphase; i++) {
				if (strcmp(preset.steps[s].dir,
					   m->phase[i].dir_name))
					continue;
				expanded[i] = 1;
				for (int k = 0; k < nsteps[i]; k++) {
					step_on[i][k] = 0;
					for (int j = 0; j < preset.steps[s].n; j++)
						if (!strcmp(preset.steps[s].step[j],
							    steps[i][k]))
							step_on[i][k] = 1;
				}
			}
		for (int i = 0; i < preset.nrebuild; i++)
			for (int k = 0; k < npkg; k++)
				if (!strcmp(pkg[k].name, preset.rebuild[i]) &&
				    nrebuild < KBUILD_MAX_REBUILD)
					kb_strlcpy(rebuild[nrebuild++],
						   preset.rebuild[i], 64);
	}

	/* Flattened rows: a phase, then its steps when expanded. */
	struct { int phase, step; } row[KBUILD_MAX_PHASES * (KBUILD_MAX_STEPS + 1)];
	int nrow = 0, sel = 0, off = 0, accepted = 0;
	char status[160] = "";

	for (;;) {
		nrow = 0;
		for (int i = 0; i < m->nphase; i++) {
			row[nrow].phase = i;
			row[nrow].step = -1;
			nrow++;
			if (!expanded[i])
				continue;
			for (int k = 0; k < nsteps[i]; k++) {
				row[nrow].phase = i;
				row[nrow].step = k;
				nrow++;
			}
		}
		if (sel >= nrow)
			sel = nrow - 1;
		if (sel < 0)
			sel = 0;

		ktui_draw_clear();
		centre(1, "BUILD PLAN — existing tree, nothing restored",
		       KT_WARN, KT_A_BOLD);

		char header[160];
		snprintf(header, sizeof(header), "    %-18s %-38s %s", "PHASE",
			 "WHAT RUNS", "REBUILD");
		ktui_draw_text(2, 3, ktui_w - 4, header, KT_ACCENT, KT_BG,
			       KT_A_BOLD);
		ktui_draw_hline(2, 4, ktui_w - 4, KT_G_HL, KT_DIM, KT_BG);

		int top = 5, rows = ktui_h - top - 4;
		if (sel < off)
			off = sel;
		if (sel >= off + rows)
			off = sel - rows + 1;

		for (int i = 0; i < rows && off + i < nrow; i++) {
			int pi = row[off + i].phase, si = row[off + i].step;
			int selrow = off + i == sel;
			int fg = selrow ? KT_BG : KT_TEXT;
			int bg = selrow ? KT_ACCENT : KT_BG;
			char line[256];

			if (si < 0) {
				int on = 0, all = 1;
				for (int k = 0; k < nsteps[pi]; k++) {
					on |= step_on[pi][k];
					all &= step_on[pi][k];
				}
				const char *mark = !phase_on[pi] ? "[ ]"
						 : (nsteps[pi] && !all) ? "[~]"
						 : "[x]";
				int nreb = 0;
				for (int k = 0; k < nrebuild; k++)
					for (int j = 0; j < npkg; j++)
						if (!strcmp(pkg[j].name, rebuild[k]) &&
						    !strcmp(pkg[j].phase,
							    m->phase[pi].dir_name))
							nreb++;
				char what[64];
				if (nsteps[pi])
					snprintf(what, sizeof(what), "%d step%s",
						 nsteps[pi],
						 nsteps[pi] == 1 ? "" : "s");
				else
					snprintf(what, sizeof(what), "packages");
				snprintf(line, sizeof(line),
					 " %s %-18s %-38s %s", mark,
					 m->phase[pi].dir_name, what,
					 nreb ? "*" : "");
				(void)on;
			} else {
				snprintf(line, sizeof(line), "     %s %s",
					 step_on[pi][si] ? "[x]" : "[ ]",
					 steps[pi][si]);
			}
			ktui_draw_fill(krect(2, top + i, ktui_w - 4, 1), bg);
			ktui_draw_text(2, top + i, ktui_w - 4, line, fg, bg,
				       selrow ? KT_A_BOLD : 0);
		}

		char sum[200];
		snprintf(sum, sizeof(sum), "%d port(s) marked for rebuild%s",
			 nrebuild, nrebuild ? "  ('/' to edit)" : "");
		ktui_draw_text(2, ktui_h - 4, ktui_w - 4, sum, KT_MID, KT_BG, 0);
		if (status[0])
			ktui_draw_text(2, ktui_h - 3, ktui_w - 4, status,
				       KT_WARN, KT_BG, 0);

		static const char *keys[] = { "↑↓", "SPACE", "←→", "A/N", "/",
					      "ENTER", "Q" };
		static const char *labels[] = { "move", "toggle", "steps",
						"all/none", "ports", "run",
						"back" };
		keyhint(ktui_h - 2, keys, labels, 7);
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_input_next(&ev, 200))
			continue;
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		int pi = nrow ? row[sel].phase : 0;
		int si = nrow ? row[sel].step : -1;

		if (ev.key == 'q' || ev.key == 'Q' || ev.key == KT_K_ESC)
			break;
		else if (ev.key == KT_K_UP || ev.key == 'k')
			sel = sel > 0 ? sel - 1 : 0;
		else if (ev.key == KT_K_DOWN || ev.key == 'j')
			sel = sel + 1 < nrow ? sel + 1 : sel;
		else if (ev.key == ' ') {
			if (si < 0) {
				int on = !phase_on[pi];
				phase_on[pi] = on;
				for (int k = 0; k < nsteps[pi]; k++)
					step_on[pi][k] = on;
			} else {
				step_on[pi][si] = !step_on[pi][si];
				int any = 0;
				for (int k = 0; k < nsteps[pi]; k++)
					any |= step_on[pi][k];
				phase_on[pi] = any;
			}
		} else if (ev.key == KT_K_RIGHT || ev.key == 'l')
			expanded[pi] = nsteps[pi] > 0;
		else if (ev.key == KT_K_LEFT || ev.key == 'h')
			expanded[pi] = 0;
		else if (ev.key == 'a' || ev.key == 'A' || ev.key == 'n' ||
			 ev.key == 'N') {
			int v = (ev.key == 'a' || ev.key == 'A');
			for (int i = 0; i < m->nphase; i++) {
				phase_on[i] = v;
				for (int k = 0; k < nsteps[i]; k++)
					step_on[i][k] = v;
			}
		} else if (ev.key == '/') {
			PickState st = { pkg, npkg, rebuild, &nrebuild };
			screen_packages(&st);
		} else if (ev.key == KT_K_ENTER || ev.key == '\n') {
			int any = 0;
			for (int i = 0; i < m->nphase; i++)
				any |= phase_on[i];
			if (!any) {
				kb_strlcpy(status, "nothing selected — pick at "
					   "least one phase", sizeof(status));
				continue;
			}
			accepted = 1;
			break;
		}
	}

	if (accepted) {
		memset(out, 0, sizeof(*out));
		int nsel = 0;
		for (int i = 0; i < m->nphase; i++)
			nsel += phase_on[i] != 0;
		/* Everything selected means "no phase filter at all", which is
		 * what keeps snapshots enabled for a full run. */
		if (nsel != m->nphase) {
			out->has_phases = 1;
			for (int i = 0; i < m->nphase; i++)
				if (phase_on[i])
					kb_strlcpy(out->phase[out->nphase++],
						   m->phase[i].dir_name, 64);
		}
		for (int i = 0; i < m->nphase; i++) {
			int all = 1;
			for (int k = 0; k < nsteps[i]; k++)
				all &= step_on[i][k];
			if (!nsteps[i] || all)
				continue;
			KbuildPlanSteps *slot = &out->steps[out->nsteps++];
			kb_strlcpy(slot->dir, m->phase[i].dir_name,
				   sizeof(slot->dir));
			for (int k = 0; k < nsteps[i]; k++)
				if (step_on[i][k] && slot->n < KBUILD_MAX_STEPS)
					kb_strlcpy(slot->step[slot->n++],
						   steps[i][k], 64);
		}
		for (int i = 0; i < nrebuild; i++)
			kb_strlcpy(out->rebuild[out->nrebuild++], rebuild[i], 64);
	}

	for (int i = 0; i < m->nphase; i++)
		kb_strv_free(steps[i]);
	free(pkg);
	free(rebuild);
	return accepted;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The snapshot / restore HUD                                               */

static char progress_title[128];

static void draw_activity(Manager *m)
{
	const SnapActivity *a = &m->snap;
	int w = ktui_w - 8;
	if (w < 20)
		return;
	int h = 8;
	KRect r = krect(4, (ktui_h - h) / 2, w, h);

	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, progress_title[0] ? progress_title : " SNAPSHOT ",
		      KT_ACCENT, KT_SURFACE, 1);

	char line[256];
	snprintf(line, sizeof(line), "%s %s %s", a->action, a->phase, a->path);
	ktui_draw_text(r.x + 2, r.y + 1, w - 4, line, KT_TEXT, KT_SURFACE,
		       KT_A_BOLD);

	double frac = a->est_bytes > 0
			      ? (double)a->bytes / (double)a->est_bytes : 0;
	if (frac > 1)
		frac = 1;
	ktui_progress(krect(r.x + 2, r.y + 3, w - 4, 1), frac, NULL);

	snprintf(line, sizeof(line), "%s%s%s   %lld files   %s/s   %s",
		 human_bytes(a->bytes), a->est_bytes ? " / " : "",
		 a->est_bytes ? human_bytes(a->est_bytes) : "", a->files,
		 human_bytes((long long)a->rate),
		 human_time(kb_now_s() - a->started));
	ktui_draw_text(r.x + 2, r.y + 4, w - 4, line, KT_MID, KT_SURFACE, 0);
	ktui_draw_text(r.x + 2, r.y + 5, w - 4, a->current, KT_DIM, KT_SURFACE,
		       0);
	ktui_draw_text(r.x + 2, r.y + 6, w - 4, "[Q] cancel", KT_WARN,
		       KT_SURFACE, KT_A_BOLD);
}

/* The redraw hook handed to the snapshot engine. Also the place a cancel is
 * noticed, because a tar can run for forty minutes. */
static void activity_tick(Manager *m)
{
	ktui_draw_clear();
	draw_activity(m);
	ktui_draw_flush();

	KtuiEvent ev;
	while (ktui_input_next(&ev, 0)) {
		if (ev.type == KT_EVT_RESIZE)
			ktui_draw_resize();
		else if (ev.type == KT_EVT_KEY &&
			 (ev.key == 'q' || ev.key == 'Q'))
			m->stop_requested = 1;
	}
}

void screen_progress(Manager *m, const char *title)
{
	kb_strlcpy(progress_title, title, sizeof(progress_title));
	snap_set_tick(m, activity_tick);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The build screen                                                         */

typedef struct {
	Manager *m;
	Sampler *sam;
	Timings *tm;

	BStep *visible[KB_MAX_STEPS];
	int nvisible;
	BStep *selected;
	int scroll;
	int log_scroll;
	int auto_follow;
	int quit_requested;
} BuildView;

static void flatten(BuildView *v, BStep *const *nodes, int n)
{
	for (int i = 0; i < n; i++) {
		if (v->nvisible == KB_MAX_STEPS)
			return;
		v->visible[v->nvisible++] = nodes[i];
		if (nodes[i]->is_group && nodes[i]->expanded)
			flatten(v, nodes[i]->child, nodes[i]->nchild);
	}
}

static int tree_width(BuildView *v)
{
	int max_text = 20;
	for (int i = 0; i < v->nvisible; i++) {
		BStep *n = v->visible[i];
		int level = n->is_group ? 0 : 1;
		int w = level * 2 + 2 + 1 + ktui_utf8_width(n->title) + 1 + 9;
		if (w > max_text)
			max_text = w;
	}
	int tw = max_text + 4;
	int cap = ktui_w * 45 / 100;
	if (tw > cap)
		tw = cap;
	return tw < 25 ? 25 : tw;
}

static BStep *phase_progress(Manager *m, int *done, int *total)
{
	BStep *g = m->current_phase;
	if (!g)
		for (int i = 0; i < m->nroot; i++)
			if (m->root[i]->status == ST_RUNNING ||
			    m->root[i]->status == ST_PENDING) {
				g = m->root[i];
				break;
			}
	*done = *total = 0;
	if (!g)
		return NULL;
	*total = g->nchild;
	for (int i = 0; i < g->nchild; i++)
		if (g->child[i]->status == ST_DONE ||
		    g->child[i]->status == ST_FAIL)
			(*done)++;
	return g;
}

static void draw_header(BuildView *v)
{
	Manager *m = v->m;
	int done, total;
	BStep *g = phase_progress(m, &done, &total);

	char left[512];
	snprintf(left, sizeof(left), " PHASE %d/%d  %s",
		 g && g->meta ? g->meta->index + 1 : 0, m->nroot,
		 g ? g->title : "-");
	if (g && g->meta && g->meta->title[0] &&
	    strcmp(g->meta->title, g->title)) {
		size_t n = strlen(left);
		snprintf(left + n, sizeof(left) - n, "  %s %s",
			 ktui_glyph[KT_G_VL], g->meta->title);
	}
	if (g && g->meta && g->meta->desc[0] &&
	    (int)strlen(left) + (int)strlen(g->meta->desc) + 24 < ktui_w) {
		size_t n = strlen(left);
		snprintf(left + n, sizeof(left) - n, "  %s %s",
			 ktui_glyph[KT_G_VL], g->meta->desc);
	}
	ktui_draw_fill(krect(1, 1, ktui_w - 2, 1), KT_BG);
	ktui_draw_text(1, 1, ktui_w - 2, left, KT_ACCENT, KT_BG, KT_A_BOLD);

	char right[128] = "";
	if (m->restored_from)
		snprintf(right, sizeof(right), "restored <- %s ",
			 m->restored_from->dir_name);
	else if (m->continued_from)
		snprintf(right, sizeof(right), "continued past %s ",
			 m->continued_from->dir_name);
	if (right[0] && (int)strlen(right) + (int)strlen(left) + 4 < ktui_w)
		ktui_draw_text(ktui_w - 1 - (int)strlen(right), 1, ktui_w,
			       right, KT_WARN, KT_BG, KT_A_BOLD);

	double pct = total ? (double)done / total : 0;
	double eta = v->tm ? eta_seconds(m, v->tm) : -1;
	char suffix[64];
	snprintf(suffix, sizeof(suffix), "  %3d%%  %d/%d  eta %s",
		 (int)(pct * 100), done, total, human_time(eta));
	int bar_w = ktui_w - 4 - (int)strlen(suffix);
	if (bar_w < 10)
		bar_w = 10;
	ktui_progress(krect(2, 2, bar_w, 1), pct, NULL);
	ktui_draw_text(2 + bar_w, 2, ktui_w - 2 - bar_w, suffix, KT_TEXT, KT_BG,
		       KT_A_BOLD);
	ktui_draw_hline(1, 3, ktui_w - 2, KT_G_HL, KT_DIM, KT_BG);
}

static void node_icon(BStep *n, char *out, size_t cap, int *fg)
{
	if (n->is_group) {
		snprintf(out, cap, " [%s]", n->expanded ? "-" : "+");
		*fg = n->status == ST_SKIPPED ? KT_DIM : KT_ACCENT;
		return;
	}
	switch (n->status) {
	case ST_PENDING:
		kb_strlcpy(out, "    ", cap);
		*fg = KT_TEXT;
		break;
	case ST_SKIPPED:
		kb_strlcpy(out, (ktui_caps & KT_CAP_UTF8) ? " ·· " : " .. ", cap);
		*fg = KT_DIM;
		break;
	case ST_RUNNING:
		snprintf(out, cap, " %s  ", spin());
		*fg = KT_WARN;
		break;
	case ST_DONE:
		kb_strlcpy(out, " OK ", cap);
		*fg = KT_ACCENT;
		break;
	default:
		kb_strlcpy(out, " !! ", cap);
		*fg = KT_ERR;
		break;
	}
}

static void draw_tree(BuildView *v, int width, int top, int bottom)
{
	int rows = bottom - top + 1;
	for (int i = 0; i < rows; i++) {
		int idx = v->scroll + i;
		if (idx >= v->nvisible)
			break;
		BStep *n = v->visible[idx];
		int y = top + i;
		int sel = n == v->selected;
		int bg = sel ? KT_ACCENT : KT_BG;

		char icon[16];
		int icon_fg;
		node_icon(n, icon, sizeof(icon), &icon_fg);

		double dur = step_duration(n);
		char t[32] = "";
		if (n->status == ST_SKIPPED)
			kb_strlcpy(t, "(skip)", sizeof(t));
		else if (dur > 0 || n->status == ST_RUNNING) {
			int s = (int)dur;
			if (s < 60)
				snprintf(t, sizeof(t), "(%ds)", s);
			else
				snprintf(t, sizeof(t), "(%dm%02ds)", s / 60,
					 s % 60);
		}

		ktui_draw_fill(krect(1, y, width - 1, 1), bg);
		int x = 1;
		if (!n->is_group)
			x += 2;
		ktui_draw_text(x, y, width - x, icon, sel ? KT_BG : icon_fg, bg,
			       KT_A_BOLD);
		x += (int)strlen(icon) > 4 ? ktui_utf8_width(icon) : 4;
		x += 1;

		int room = width - 1 - x - 1 - (int)strlen(t);
		if (room < 1)
			room = 1;
		ktui_draw_text(x, y, room, n->title, sel ? KT_BG : KT_TEXT, bg,
			       sel ? KT_A_BOLD : 0);
		if (t[0])
			ktui_draw_text(width - 1 - (int)strlen(t), y,
				       (int)strlen(t), t,
				       sel ? KT_BG :
				       (n->status == ST_SKIPPED ? KT_DIM : KT_MID),
				       bg, 0);
	}
}

static void draw_group_detail(BStep *n, int x, int w, int y, int h)
{
	const KbuildPhase *meta = n->meta;
	if (!meta)
		return;
	int line = 0;
	char buf[512];

	snprintf(buf, sizeof(buf), "title       %s", meta->title);
	if (line < h)
		ktui_draw_text(x, y + line++, w, buf, KT_TEXT, KT_BG, 0);
	if (meta->desc[0]) {
		snprintf(buf, sizeof(buf), "desc        %s", meta->desc);
		if (line < h)
			ktui_draw_text(x, y + line++, w, buf, KT_MID, KT_BG, 0);
	}
	snprintf(buf, sizeof(buf), "runs in     %s",
		 meta->chroot ? "chroot" : "host");
	if (line < h)
		ktui_draw_text(x, y + line++, w, buf, KT_MID, KT_BG, 0);
	snprintf(buf, sizeof(buf), "steps       %d", n->nchild);
	if (line < h)
		ktui_draw_text(x, y + line++, w, buf, KT_MID, KT_BG, 0);

	KbBuf b = {0};
	for (int i = 0; i < meta->nsnap; i++)
		kb_buf_printf(&b, "%s%s", i ? " " : "", meta->snap_path[i]);
	snprintf(buf, sizeof(buf), "snapshots   %s",
		 b.n ? b.p : "none declared");
	kb_buf_free(&b);
	if (line < h)
		ktui_draw_text(x, y + line++, w, buf, KT_MID, KT_BG, 0);

	if (meta->nrejected) {
		KbBuf r = {0};
		for (int i = 0; i < meta->nrejected; i++)
			kb_buf_printf(&r, "%s%s", i ? " " : "",
				      meta->rejected[i]);
		snprintf(buf, sizeof(buf), "REJECTED    %s", r.p);
		kb_buf_free(&r);
		if (line < h)
			ktui_draw_text(x, y + line++, w, buf, KT_ERR, KT_BG,
				       KT_A_BOLD);
	}
	if (n->snap_state[0]) {
		snprintf(buf, sizeof(buf), "snapshot    %s  %s", n->snap_state,
			 n->note);
		if (line < h)
			ktui_draw_text(x, y + line++, w, buf, KT_WARN, KT_BG, 0);
	}
}

static void draw_detail(BuildView *v, int x, int w, int top, int bottom)
{
	BStep *n = v->selected;
	if (!n)
		return;

	int fg = KT_TEXT;
	if (n->status == ST_RUNNING)
		fg = KT_WARN;
	else if (n->status == ST_DONE)
		fg = KT_ACCENT;
	else if (n->status == ST_FAIL)
		fg = KT_ERR;
	else if (n->status == ST_SKIPPED)
		fg = KT_DIM;

	char head[256];
	snprintf(head, sizeof(head), " STEP: %s ", n->title);
	ktui_draw_fill(krect(x, top, w, 1), KT_BG);
	ktui_draw_text(x, top, w, head, fg, KT_BG, KT_A_BOLD);

	static const char *NAMES[] = { "PENDING", "RUNNING", "DONE", "FAIL",
				       "SKIPPED" };
	char status[256];
	snprintf(status, sizeof(status), " STATUS: %s ", NAMES[n->status]);
	if (n->status == ST_RUNNING && n->start_time > 0) {
		size_t k = strlen(status);
		snprintf(status + k, sizeof(status) - k, "(%ds)",
			 (int)(kb_now_s() - n->start_time));
	} else if (step_duration(n) > 0) {
		size_t k = strlen(status);
		snprintf(status + k, sizeof(status) - k, "(%.1fs)",
			 step_duration(n));
	}
	if (n->note[0]) {
		size_t k = strlen(status);
		snprintf(status + k, sizeof(status) - k, "  %s %s",
			 ktui_glyph[KT_G_BULLET], n->note);
	}
	ktui_draw_text(x, top + 1, w, status, KT_TEXT, KT_BG, KT_A_BOLD);
	ktui_draw_hline(x, top + 2, w, KT_G_HL, KT_DIM, KT_BG);

	int log_y = top + 3;
	int log_h = bottom - log_y + 1;
	if (log_h <= 0)
		return;

	if (n->is_group) {
		/* A group only has logs when its EXPANSION failed — show those
		 * instead of the metadata, or the failure is invisible. */
		if (n->nlog && n->status == ST_FAIL) {
			int start = n->nlog > log_h ? n->nlog - log_h : 0;
			for (int i = 0; start + i < n->nlog && i < log_h; i++)
				ktui_draw_text(x, log_y + i, w, n->log[start + i],
					       KT_ERR, KT_BG, KT_A_BOLD);
			return;
		}
		draw_group_detail(n, x, w, log_y, log_h);
		return;
	}

	if (!n->nlog) {
		const char *msg = n->status == ST_PENDING ? "Pending..."
				: n->status == ST_RUNNING ? "Starting..."
				: n->status == ST_SKIPPED
					? "Skipped (restored from snapshot)"
					: "No logs.";
		ktui_draw_text(x, log_y, w, msg, KT_DIM, KT_BG, 0);
		return;
	}

	int end = n->nlog - v->log_scroll;
	if (end > n->nlog)
		end = n->nlog;
	if (end < 1)
		end = 1;
	int start = end - log_h;
	if (start < 0)
		start = 0;
	for (int i = 0; start + i < end; i++)
		ktui_draw_text(x, log_y + i, w, n->log[start + i], KT_MID,
			       KT_BG, 0);
}

static void draw_hud(BuildView *v, int y, int h)
{
	Sampler *s = v->sam;
	Manager *m = v->m;
	if (!s || h <= 0)
		return;

	char line[512];
	double mem_pct = s->mem_total ? (double)s->mem_used / s->mem_total * 100
				      : 0;
	snprintf(line, sizeof(line),
		 " load %.2f   mem %s/%s (%d%%)   disk free %s   %.0f lines/s   "
		 "elapsed %s", s->load1, human_bytes(s->mem_used),
		 human_bytes(s->mem_total), (int)mem_pct,
		 human_bytes(s->disk_free), s->lines_per_sec,
		 human_time(m->start_time ? kb_now_s() - m->start_time : 0));
	ktui_draw_text(1, y, ktui_w - 2, line, KT_MID, KT_BG, 0);

	if (h < 2)
		return;

	if (s->fs_sampled_at > 0)
		snprintf(line, sizeof(line),
			 " rootfs %s in %lld files%s", human_bytes(s->fs_bytes),
			 s->fs_files, s->fs_partial ? " (partial)" : "");
	else
		snprintf(line, sizeof(line), " rootfs ...");

	/* The last notice, right-aligned: a failed snapshot has to be visible
	 * without opening a log. */
	if (m->nnotice) {
		const char *note = m->notice[m->nnotice - 1].text;
		int nw = ktui_utf8_width(note);
		if (nw + (int)strlen(line) + 4 < ktui_w)
			ktui_draw_text(ktui_w - 2 - nw, y + 1, nw, note,
				       KT_WARN, KT_BG, 0);
	}
	ktui_draw_text(1, y + 1, ktui_w - 2, line, KT_DIM, KT_BG, 0);
}

static void draw_footer(BuildView *v, int y)
{
	Manager *m = v->m;
	const char *keys[] = { "↑↓", "SPACE", "F", "S", "PGUP/DN", "Q" };
	const char *labels[] = { "select", "fold", "follow", "snapshot",
				 "log", "quit" };
	ktui_draw_fill(krect(1, y, ktui_w - 2, 1), KT_BG);
	keyhint(y, keys, labels, 6);

	char right[128];
	snprintf(right, sizeof(right), "%s%s ",
		 v->auto_follow ? "following" : "manual",
		 m->is_running ? "" : "  (finished)");
	int rw = (int)strlen(right);
	if (rw + 40 < ktui_w)
		ktui_draw_text(ktui_w - 1 - rw, y, rw, right,
			       v->auto_follow ? KT_ACCENT : KT_WARN, KT_BG,
			       KT_A_BOLD);
}

static void view_update(BuildView *v)
{
	Manager *m = v->m;
	v->nvisible = 0;
	flatten(v, m->root, m->nroot);

	if (v->auto_follow && m->current_step)
		v->selected = m->current_step;
	if (!v->selected && v->nvisible)
		v->selected = v->visible[0];

	int sel_idx = 0;
	for (int i = 0; i < v->nvisible; i++)
		if (v->visible[i] == v->selected) {
			sel_idx = i;
			break;
		}

	int rows = ktui_h - 8;
	if (rows < 1)
		rows = 1;
	if (sel_idx < v->scroll)
		v->scroll = sel_idx;
	if (sel_idx >= v->scroll + rows)
		v->scroll = sel_idx - rows + 1;
	if (v->scroll < 0)
		v->scroll = 0;
}

static void view_move(BuildView *v, int delta)
{
	v->auto_follow = 0;
	v->log_scroll = 0;
	int at = 0;
	for (int i = 0; i < v->nvisible; i++)
		if (v->visible[i] == v->selected) {
			at = i;
			break;
		}
	at += delta;
	if (at < 0)
		at = 0;
	if (at >= v->nvisible)
		at = v->nvisible - 1;
	if (v->nvisible)
		v->selected = v->visible[at];
}

void screen_build(Manager *m, Sampler *sam, Timings *tm)
{
	BuildView v = {0};
	v.m = m;
	v.sam = sam;
	v.tm = tm;
	v.auto_follow = 1;

	mgr_start(m);
	screen_progress(m, " SNAPSHOT ");

	for (;;) {
		int wait_ms = mgr_pump(m);
		sam_pump(sam, m);
		view_update(&v);

		ktui_draw_clear();
		ktui_draw_box(krect(0, 0, ktui_w, ktui_h), " KDOS BUILD SYSTEM ",
			      KT_ACCENT, KT_BG, 0);

		int header_h = ktui_h >= 12 ? 3 : 0;
		int top = 1 + header_h;
		int footer_y = ktui_h - 1;
		int hud_h = ktui_h >= 20 ? 2 : (ktui_h >= 16 ? 1 : 0);
		int hud_top = footer_y - 1 - hud_h + 1;
		int content_bottom = hud_h ? hud_top - 2 : footer_y - 1;

		if (header_h)
			draw_header(&v);

		int tree_w = tree_width(&v);
		int detail_x = tree_w + 1;
		int detail_w = ktui_w - tree_w - 2;
		if (detail_w >= 5) {
			ktui_draw_vline(tree_w, top, content_bottom - top + 1,
					KT_G_VL, KT_DIM, KT_BG);
			draw_tree(&v, tree_w, top, content_bottom);
			draw_detail(&v, detail_x, detail_w, top, content_bottom);
		}
		if (hud_h)
			draw_hud(&v, hud_top, hud_h);
		draw_footer(&v, footer_y - 1);

		if (m->snap.active)
			draw_activity(m);
		ktui_draw_flush();

		/* A forced quit leaves even with a child still being killed;
		 * mgr_pump has already signalled its group. */
		if (v.quit_requested && m->force_quit)
			break;

		if (!m->is_running) {
			if (v.quit_requested)
				break;
			if (m->error_step && v.auto_follow) {
				/* Hold the failed step on screen. */
				v.auto_follow = 0;
				v.selected = m->error_step;
			}
			if (m->stop_requested && !m->error_step)
				break;
			if (wait_ms < 30)
				wait_ms = 30;
		}

		KtuiEvent ev;
		if (!ktui_input_next(&ev, wait_ms))
			continue;
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			continue;
		}
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.btn == KT_MB_WHEEL_UP)
				view_move(&v, -3);
			else if (ev.btn == KT_MB_WHEEL_DOWN)
				view_move(&v, 3);
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		switch (ev.key) {
		case 'q':
		case 'Q':
			if (!m->is_running) {
				v.quit_requested = 1;
			} else if (m->stop_requested) {
				/* Asked twice: kill the step's process group
				 * outright and leave. A build that cannot be
				 * quit is worse than a build that is killed
				 * untidily. */
				m->force_quit = 1;
				v.quit_requested = 1;
				mgr_notice(m, "force quit — killing the "
					   "current step");
			} else {
				m->stop_requested = 1;
				mgr_notice(m, "stop requested — finishing the "
					   "current step (Q again to force)");
			}
			break;
		case KT_K_UP:
		case 'k':
			view_move(&v, -1);
			break;
		case KT_K_DOWN:
		case 'j':
			view_move(&v, 1);
			break;
		case ' ':
			if (v.selected && v.selected->is_group)
				v.selected->expanded = !v.selected->expanded;
			break;
		case 'f':
		case 'F':
			v.auto_follow = !v.auto_follow;
			v.log_scroll = 0;
			break;
		case 's':
		case 'S': {
			/* A partial snapshot of the phase in flight. Restoring
			 * one re-runs the phase rather than skipping it. */
			BStep *g = mgr_phase_of(m->current_step);
			if (g) {
				m->snapshot_request = g;
				mgr_notice(m, "snapshot of %s queued after this "
					   "step", g->title);
			}
			break;
		}
		case KT_K_PGUP:
			v.log_scroll += 10;
			break;
		case KT_K_PGDN:
			v.log_scroll -= 10;
			if (v.log_scroll < 0)
				v.log_scroll = 0;
			break;
		case KT_K_HOME:
			v.auto_follow = 0;
			if (v.nvisible)
				v.selected = v.visible[0];
			break;
		case KT_K_END:
			v.auto_follow = 0;
			if (v.nvisible)
				v.selected = v.visible[v.nvisible - 1];
			break;
		default:
			break;
		}
	}

	sam_stop(sam);
	tm_save(tm);
}
