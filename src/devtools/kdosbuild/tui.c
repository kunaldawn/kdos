/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the screens
 *
 * On libktui. Everything here draws in the SAME eight colour slots the
 * installer and kdos-appbox use, so a tty and a truecolor terminal render one
 * picture.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

/* Same three-tier split ktui_ramp_init() uses, and for the same reason: the
 * braille block is not in ter-kdos32n's 512 glyphs (verified against
 * uni/xos4-2.uni), so the running-step marker went blank on tty1 -- the one
 * glyph that most needs to be visible there. vt gets the ascii spinner too,
 * not a third table, because there is nothing between "dots" and "braille"
 * worth drawing. */
static const char *spin(void)
{
	int i = (int)(kb_now_s() * 10) & 7;
	if ((ktui_caps & KT_CAP_UTF8) && !(ktui_caps & KT_CAP_LINUXVT))
		return SPINNER[i];
	return SPINNER_ASCII[i];
}

static void centre(int y, const char *s, int fg, int attr)
{
	int w = ktui_utf8_width(s);
	int x = (ktui_w - w) / 2;
	ktui_draw_text(x < 2 ? 2 : x, y, ktui_w - 2, s, fg, KT_BG, attr);
}

/* ktui_glyph[] resolves KT_G_UP/DOWN/LEFT/RIGHT against what the font
 * actually carries (uni/xos4-2.uni has neither U+2190 nor U+2192, so LEFT and
 * RIGHT are already ◀ ▶ there); building the hint strings from it rather than
 * from a literal keeps the ascii tier's "^"/"v"/"<"/">" too, which a literal
 * codepoint only reaches by way of ktui_draw_text's cp>0x7f fallback to '?'.
 * Buffers rather than string literals because ktui_glyph is a runtime table —
 * a `static const char *` array initialised from it is not a constant
 * expression. */
static const char *arrow_ud(void)
{
	static char buf[16];
	snprintf(buf, sizeof(buf), "%s%s", ktui_glyph[KT_G_UP],
		 ktui_glyph[KT_G_DOWN]);
	return buf;
}

static const char *arrow_lr(void)
{
	static char buf[16];
	snprintf(buf, sizeof(buf), "%s%s", ktui_glyph[KT_G_LEFT],
		 ktui_glyph[KT_G_RIGHT]);
	return buf;
}

/* Bounded by `r`, never by the whole terminal: a fragment that would not fit
 * ENTIRELY inside r's width is not drawn at all, rather than being cut
 * mid-glyph. On the build screen, an `r` spanning the whole row lets the
 * footer's hints run straight over the frame's own right border at any width
 * <= 83. */
static void keyhint(KRect r, const char *const *keys, const char *const *labels,
		    int n)
{
	int x = r.x;
	int right = r.x + r.w;
	for (int i = 0; i < n; i++) {
		char k[16];
		snprintf(k, sizeof(k), "[%s]", keys[i]);
		char l[32];
		snprintf(l, sizeof(l), ":%s ", labels[i]);
		int w = ktui_utf8_width(k) + ktui_utf8_width(l);
		if (x + w > right)
			break;
		x += ktui_draw_text(x, r.y, right - x, k, KT_WARN, KT_BG,
				    KT_A_BOLD);
		x += ktui_draw_text(x, r.y, right - x, l, KT_TEXT, KT_BG, 0);
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

/* Everything the startup picker's drawing half reads. `first_row` is an OUTPUT:
 * the rows are laid out here, so the loop learns where they landed from the
 * draw rather than recomputing the same arithmetic to map a click. */
typedef struct {
	Manager *m;
	const KbuildSnapshot *snaps;
	int nsnap;
	/* Every snapshot, held ones included: the chains a restore reads. */
	const KbuildSnapshot *all;
	int nall;
	const int *row_phase;
	int nrow;
	int sel;
	const char *commit;
	const char *status;
	int first_row;
	int snap_on;
} StartupView;

static void draw_startup_frame(StartupView *v)
{
	Manager *m = v->m;
	const KbuildSnapshot *snaps = v->snaps;
	int nsnap = v->nsnap, nrow = v->nrow, sel = v->sel;
	const int *row_phase = v->row_phase;

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

	/* Below this width the SIZE/COMMIT/STEPS/BUILD columns would
	 * collide with the fixed-width PHASE/WHEN text on the left,
	 * so a narrow terminal drops them instead of overlapping it. */
	int wide = ktui_w >= 90;
	/* rx is 3 columns short of the row's own width (ktui_w - 4)
	 * so BUILD's right edge lands on ktui_w-3, this row's true
	 * right margin. Anchoring on ktui_w-4 directly runs BUILD one
	 * column past the framebuffer and silently drops its last
	 * character -- verified against ktui_draw_cell's bounds
	 * check, not assumed. */
	int rx = ktui_w - 7;

	char header[160];
	snprintf(header, sizeof(header), "   %-16s %-17s", "PHASE", "WHEN");
	ktui_draw_text(2, y, ktui_w - 4, header, KT_ACCENT, KT_BG, KT_A_BOLD);
	if (wide) {
		ktui_draw_text_right(rx - 36, y, 9, "SIZE", KT_ACCENT,
				     KT_BG, KT_A_BOLD);
		ktui_draw_text_right(rx - 26, y, 11, "COMMIT",
				     KT_ACCENT, KT_BG, KT_A_BOLD);
		ktui_draw_text_right(rx - 14, y, 8, "STEPS", KT_ACCENT,
				     KT_BG, KT_A_BOLD);
		ktui_draw_text_right(rx - 5, y, 10, "BUILD", KT_ACCENT,
				     KT_BG, KT_A_BOLD);
	}
	y++;
	ktui_draw_hline(2, y++, ktui_w - 4, KT_G_HL, KT_DIM, KT_BG);

	int any_stale = 0, any_partial = 0;
	int nshown = 0;
	v->first_row = y;
	for (int r = 0; r < nrow && y < ktui_h - 8; r++, y++, nshown++) {
		int selrow = r == sel;
		int fg, bg;

		ktui_sel_slots(selrow, 1, KT_BG, &fg, &bg);
		char text[256];
		char size[32] = "", cm[72] = "", steps[32] = "", dur[32] = "";

		if (row_phase[r] < 0) {
			snprintf(text, sizeof(text), "   %-16s %s",
				 "start fresh",
				 "run every phase from scratch   "
				 "([P]: pick phases/ports instead)");
		} else {
			const KbuildPhase *p = &m->phase[row_phase[r]];
			const KbuildSnapshot *sn =
				kbuild_snap_find(snaps, nsnap, p->dir_name);
			int stale = snap_stale(sn, v->commit);
			any_stale |= stale;
			any_partial |= !sn->complete;

			snprintf(cm, sizeof(cm), "%s%s",
				 sn->git_commit[0] ? sn->git_commit : "-",
				 stale ? "*" : "");
			if (sn->complete)
				snprintf(steps, sizeof(steps), "%d", sn->steps);
			else
				snprintf(steps, sizeof(steps), "%d/%d!",
					 sn->steps, sn->total_steps);

			char when[32];
			kb_strlcpy(when, format_when(sn->created),
				   sizeof(when));
			kb_strlcpy(size, human_bytes(snap_size(sn)),
				   sizeof(size));
			kb_strlcpy(dur, human_time(sn->duration_s),
				   sizeof(dur));
			snprintf(text, sizeof(text), "%2d %-16s %-17s",
				 r, p->dir_name, when);
		}
		ktui_draw_fill(krect(2, y, ktui_w - 4, 1), bg);
		ktui_draw_text(2, y, ktui_w - 4, text, fg, bg,
			       selrow ? KT_A_BOLD : 0);
		/* Columns end at fixed offsets from the RIGHT edge, so
		 * a 6 GB size or a 12/40! step count cannot push the
		 * next field out of its header's column. */
		if (wide && row_phase[r] >= 0) {
			ktui_draw_text_right(rx - 36, y, 9, size, fg, bg, 0);
			ktui_draw_text_right(rx - 26, y, 11, cm, fg, bg, 0);
			ktui_draw_text_right(rx - 14, y, 8, steps, fg, bg, 0);
			ktui_draw_text_right(rx - 5, y, 10, dur, fg, bg, 0);
		}
	}
	/* `nshown` — the rows the loop above actually drew, short of `nrow`
	 * whenever the list doesn't fit — sizes both the rect and `shown`, so
	 * the thumb reflects the truncation it sits beside instead of always
	 * rendering full-height and denying it. */
	ktui_scrollbar(krect(ktui_w - 2, v->first_row, 1, nshown), nrow, nshown, 0);

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
		/* Its own archives, then what restoring it reads: a layer is
		 * small, and its chain down to a full archive is not. */
		KbBuf b = {0};
		long long reads = 0;
		int narch = 0;
		kb_buf_str(&b, "restores: ");
		for (int i = 0; i < sn->nentries; i++) {
			const KbuildSnapEntry *e = &sn->entry[i];
			kb_buf_printf(&b, "%s%s %s %s", i ? "  " : "", e->path,
				      human_bytes(e->bytes_compressed),
				      e->layer ? "layer" : "full");
			const KbuildSnapshot *chain[KBUILD_MAX_CHAIN];
			int len = kbuild_snap_chain(v->all, v->nall, sn,
						    e->path, chain,
						    KBUILD_MAX_CHAIN);
			for (int k = 0; k < len; k++)
				reads += kbuild_snap_entry(chain[k], e->path)
						 ->bytes_compressed;
			narch += len > 0 ? len : 0;
		}
		kb_buf_printf(&b, " · restore reads %s from %d archive%s",
			      human_bytes(reads), narch, narch == 1 ? "" : "s");
		ktui_draw_text(2, y++, ktui_w - 4, b.p ? b.p : "",
			       KT_MID, KT_BG, 0);
		kb_buf_free(&b);

		long long biggest = 0;
		for (int i = 0; i < nsnap; i++)
			if (snap_size(&snaps[i]) > biggest)
				biggest = snap_size(&snaps[i]);
		if (biggest)
			ktui_gauge(2, y, 12, (double)snap_size(sn) / biggest,
				   KT_MID, KT_BG);
		y++;

		char cont[128];
		snprintf(cont, sizeof(cont), "continues from: %s",
			 p->index + 1 < m->nphase
				 ? m->phase[p->index + 1].dir_name
				 : "nothing left");
		ktui_draw_text(2, y++, ktui_w - 4, cont, KT_MID, KT_BG, 0);
	}

	if (v->status && v->status[0])
		ktui_draw_text(2, y + 1, ktui_w - 4, v->status, KT_WARN,
			       KT_BG, 0);

	/* Writing is off unless asked for, and the picker says what that
	 * leaves: a failure is retried in place ([R] on the build screen), but
	 * a tree that has to be thrown away goes back to the last snapshot
	 * there is, or to the cross toolchain. */
	if (!v->snap_on && y + 2 < ktui_h - 2)
		ktui_draw_text(2, y + 2, ktui_w - 4,
			       "snapshot writing is off — this run leaves no "
			       "restore points (S turns it on)",
			       KT_MID, KT_BG, 0);

	const char *keys[] = { arrow_ud(), "ENTER", "S", "P", "D", "Q" };
	static const char *labels[] = { "select", "start", "snapshots",
					"plan", "delete", "quit" };
	keyhint(krect(2, ktui_h - 2, ktui_w - 4, 1), keys, labels, 6);

	char foot[640];
	snprintf(foot, sizeof(foot), "codec: %s   writing: %s   snapshots: %s",
		 snap_codec(), v->snap_on ? "on" : "off", m->snap_root);
	ktui_draw_text(2, ktui_h - 1, ktui_w - 4, foot, KT_DIM, KT_BG, 0);
}

int screen_startup(Manager *m, int *index, const char *commit,
		   int *snapshot_enabled)
{
	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int nsnap = kbuild_snap_list(m->snap_root, snaps, KBUILD_MAX_PHASES);
	KbuildSnapshot *all = kb_calloc(KBUILD_MAX_SNAPS, sizeof(*all));
	int nall = kbuild_snap_list_all(m->snap_root, all, KBUILD_MAX_SNAPS);

	/* Row 0 is "start fresh"; the rest are phases that have a snapshot and
	 * still declare snapshot paths. A snapshot under a phase that declares
	 * none is a leftover it can never be restored from, and `--delete`
	 * removes it. */
	int row_phase[KBUILD_MAX_PHASES + 1];
	int nrow = 1;
	row_phase[0] = -1;
	for (int i = 0; i < m->nphase; i++)
		if (kbuild_snapshottable(&m->phase[i]) &&
		    kbuild_snap_find(snaps, nsnap, m->phase[i].dir_name))
			row_phase[nrow++] = i;

	int sel = nrow > 1 ? nrow - 1 : 0;
	char status[256] = "";
	int result = PICK_QUIT;

	for (;;) {
		StartupView vw = { m, snaps, nsnap, all, nall, row_phase,
				   nrow, sel, commit, status, 0,
				   *snapshot_enabled };
		draw_startup_frame(&vw);
		int first_row = vw.first_row;
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_input_next(&ev, 200))
			continue;
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			continue;
		}
		if (ev.type == KT_EVT_MOUSE && ev.btn == KT_MB_LEFT &&
		    ev.press == KT_MP_PRESS) {
			int idx = ev.my - first_row;
			if (idx >= 0 && idx < nrow) {
				/* A second click on the row already selected
				 * starts it. libktui reports presses and
				 * releases and has no double-click notion, so
				 * this is the gesture that can be expressed
				 * without inventing a timing rule. */
				if (idx == sel) {
					result = row_phase[sel] < 0
							 ? PICK_FRESH
							 : PICK_RESTORE;
					if (row_phase[sel] >= 0)
						*index = row_phase[sel];
					break;
				}
				sel = idx;
			}
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
		else if (ev.key == 's' || ev.key == 'S') {
			*snapshot_enabled = !*snapshot_enabled;
			kb_strlcpy(status,
				   *snapshot_enabled
					   ? "snapshots will be written"
					   : "snapshots will NOT be written",
				   sizeof(status));
		} else if (ev.key == 'p' || ev.key == 'P') {
			result = PICK_PLAN;
			break;
		} else if (ev.key == 'd' || ev.key == 'D') {
			if (row_phase[sel] < 0) {
				kb_strlcpy(status, "nothing to delete",
					   sizeof(status));
			} else {
				const KbuildPhase *p = &m->phase[row_phase[sel]];
				char deps[160];
				if (snap_delete(m, p->dir_name, deps,
						sizeof(deps)) == 2)
					snprintf(status, sizeof(status),
						 "deleted %s; kept as the base "
						 "of %s until they go",
						 p->dir_name, deps);
				else
					snprintf(status, sizeof(status),
						 "deleted %s", p->dir_name);
				nsnap = kbuild_snap_list(m->snap_root, snaps,
							 KBUILD_MAX_PHASES);
				nall = kbuild_snap_list_all(m->snap_root, all,
							    KBUILD_MAX_SNAPS);
				nrow = 1;
				for (int i = 0; i < m->nphase; i++)
					if (kbuild_snapshottable(&m->phase[i]) &&
					    kbuild_snap_find(snaps, nsnap,
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
	free(all);
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

/* `off` is in/out: the scroll clamp needs the row count, which is geometry, so
 * it belongs on the drawing side rather than being recomputed by the loop. */
typedef struct {
	PickState *st;
	const int *match;
	int nmatch;
	int sel;
	int off;
	const char *filter;
} PackView;

static void draw_packages_frame(PackView *v)
{
	PickState *st = v->st;
	const int *match = v->match;
	int nmatch = v->nmatch, sel = v->sel;
	const char *filter = v->filter;

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
	if (sel < v->off)
		v->off = sel;
	if (sel >= v->off + rows)
		v->off = sel - rows + 1;
	int off = v->off;

	for (int i = 0; i < rows && off + i < nmatch; i++) {
		const KbuildPkgRef *p = &st->pkg[match[off + i]];
		int on = pick_has(st, p->name);
		int selrow = off + i == sel;
		int fg, bg;

		ktui_sel_slots(selrow, 1, KT_BG, &fg, &bg);
		/* The enabled mark keeps its own colour off the fill and takes
		 * the label's on it: KT_WARN on KT_DIM is below any reading
		 * floor, and the mark is the column this list is FOR. */
		if (on && !selrow)
			fg = KT_WARN;
		snprintf(line, sizeof(line), " %s %-28s %s",
			 on ? "[x]" : "[ ]", p->name,
			 p->phase[0] ? p->phase : "-");
		ktui_draw_fill(krect(2, top + i, ktui_w - 4, 1), bg);
		ktui_draw_text(2, top + i, ktui_w - 4, line, fg, bg,
			       selrow ? KT_A_BOLD : 0);

		if (filter[0]) {
			const char *at = strstr(p->name, filter);
			if (at) {
				/* " [x] " ahead of the name above is 5
				 * columns (space + "[x]" + space) --
				 * matches the row's own snprintf, not
				 * assumed. Clamped to the row's own
				 * right edge so a match near the end
				 * of a long name cannot paint over the
				 * scrollbar. */
				int mcol = 5 + (int)(at - p->name);
				int maxw = (ktui_w - 4) - mcol;
				int fw = (int)strlen(filter);
				if (fw > maxw)
					fw = maxw;
				if (fw > 0)
					ktui_draw_text(
						2 + mcol, top + i, fw, at,
						selrow ? KT_ACCENT : KT_BG,
						selrow ? KT_BG : KT_WARN,
						KT_A_REVERSE);
			}
		}
	}
	ktui_scrollbar(krect(ktui_w - 2, top, 1, rows), nmatch, rows, off);

	/* BKSP deletes ONE character — it is a text filter, not a mode. The
	 * label reads "del" and not "clear", which would promise something
	 * the handler does not do. */
	const char *keys[] = { arrow_ud(), "SPACE", "BKSP", "ENTER" };
	static const char *labels[] = { "select", "toggle", "del", "done" };
	keyhint(krect(2, ktui_h - 2, ktui_w - 4, 1), keys, labels, 4);
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

		PackView vw = { st, match, nmatch, sel, off, filter };
		draw_packages_frame(&vw);
		off = vw.off;
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
		} else if (ev.key >= 32 && ev.key < 127 &&
		    !(ev.mods & (KT_MOD_CTRL | KT_MOD_ALT))) {
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

/* Flattened rows: a phase, then its steps when expanded. */
typedef struct {
	int phase, step;
} PlanRow;

/* The plan picker's drawing half. Every member is a pointer into the loop's
 * own locals — the picker keeps a lot of parallel state and copying it per
 * frame would be the expensive half of the split. `off` is in/out for the
 * same reason as the package picker's. */
typedef struct {
	Manager *m;
	int *phase_on;
	char ***steps;
	int *nsteps;
	int (*step_on)[KBUILD_MAX_STEPS];
	KbuildPkgRef *pkg;
	int npkg;
	char (*rebuild)[64];
	int nrebuild;
	PlanRow *row;
	int nrow;
	int sel;
	int off;
	const char *status;
} PlanView;

static void draw_plan_frame(PlanView *v)
{
	Manager *m = v->m;
	int *nsteps = v->nsteps;
	int nrow = v->nrow, sel = v->sel;

	ktui_draw_clear();
	centre(1, "BUILD PLAN — existing tree, nothing restored",
	       KT_WARN, KT_A_BOLD);

	/* Below this width the PROGRESS bar and REBUILD marker would
	 * collide with the fixed-width WHAT RUNS text on the left, so
	 * a narrow terminal drops them instead of overlapping it. */
	int wide = ktui_w >= 80;

	char header[160];
	snprintf(header, sizeof(header), "     %-18s %-20s", "PHASE",
		 "WHAT RUNS");
	ktui_draw_text(2, 3, ktui_w - 4, header, KT_ACCENT, KT_BG, KT_A_BOLD);
	if (wide) {
		ktui_draw_text_right(ktui_w - 22, 3, 14, "PROGRESS",
				     KT_ACCENT, KT_BG, KT_A_BOLD);
		ktui_draw_text_right(ktui_w - 8, 3, 6, "RBLD",
				     KT_ACCENT, KT_BG, KT_A_BOLD);
	}
	ktui_draw_hline(2, 4, ktui_w - 4, KT_G_HL, KT_DIM, KT_BG);

	int top = 5, rows = ktui_h - top - 4;
	if (sel < v->off)
		v->off = sel;
	if (sel >= v->off + rows)
		v->off = sel - rows + 1;
	int off = v->off;

	for (int i = 0; i < rows && off + i < nrow; i++) {
		int pi = v->row[off + i].phase, si = v->row[off + i].step;
		int selrow = off + i == sel;
		int fg, bg;

		ktui_sel_slots(selrow, 1, KT_BG, &fg, &bg);
		char line[256];
		int on = 0, nreb = 0;

		if (si < 0) {
			for (int k = 0; k < nsteps[pi]; k++)
				on += v->step_on[pi][k] != 0;
			const char *mark = !v->phase_on[pi] ? "[ ]"
					 : (nsteps[pi] && on < nsteps[pi])
						   ? "[~]" : "[x]";
			for (int k = 0; k < v->nrebuild; k++)
				for (int j = 0; j < v->npkg; j++)
					if (!strcmp(v->pkg[j].name, v->rebuild[k]) &&
					    !strcmp(v->pkg[j].phase,
						    m->phase[pi].dir_name))
						nreb++;
			char what[64];
			if (nsteps[pi])
				snprintf(what, sizeof(what), "%d of %d steps",
					 on, nsteps[pi]);
			else
				snprintf(what, sizeof(what), "packages");
			snprintf(line, sizeof(line), " %s %-18s %-20s",
				 mark, m->phase[pi].dir_name, what);
		} else {
			snprintf(line, sizeof(line), "     %s %s",
				 v->step_on[pi][si] ? "[x]" : "[ ]",
				 v->steps[pi][si]);
		}
		ktui_draw_fill(krect(2, top + i, ktui_w - 4, 1), bg);
		ktui_draw_text(2, top + i, ktui_w - 4, line, fg, bg,
			       selrow ? KT_A_BOLD : 0);
		/* `bg` is the ROW's background: these bars are
		 * drawn inside a selectable row, and a
		 * hardcoded KT_BG punches a hole through the
		 * selection highlight. */
		if (wide && si < 0) {
			if (nsteps[pi])
				ktui_progress_ex(
					krect(ktui_w - 22, top + i, 14, 1),
					(double)on / nsteps[pi], NULL,
					KT_BAR_SEGMENTED, bg);
			if (nreb)
				ktui_draw_text_right(ktui_w - 8, top + i,
						     6, "*", KT_WARN, bg, 0);
		}
	}
	ktui_scrollbar(krect(ktui_w - 2, top, 1, rows), nrow, rows, off);

	char sum[200];
	snprintf(sum, sizeof(sum), "%d port(s) marked for rebuild%s",
		 v->nrebuild, v->nrebuild ? "  ('/' to edit)" : "");
	ktui_draw_text(2, ktui_h - 4, ktui_w - 4, sum, KT_MID, KT_BG, 0);
	if (v->status && v->status[0])
		ktui_draw_text(2, ktui_h - 3, ktui_w - 4, v->status,
			       KT_WARN, KT_BG, 0);

	const char *keys[] = { arrow_ud(), "SPACE", arrow_lr(), "A/N", "/",
			       "ENTER", "Q" };
	static const char *labels[] = { "move", "toggle", "steps",
					"all/none", "ports", "run", "back" };
	keyhint(krect(2, ktui_h - 2, ktui_w - 4, 1), keys, labels, 7);
}

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

	/* A tree the walker refuses leaves the port list empty and says why on
	 * the status line, rather than offering an empty list as the answer. */
	KbuildPkgRef *pkg = kb_calloc(KB_MAX_PKGS, sizeof(*pkg));
	char status[160] = "";
	int npkg = kbuild_package_index(m->phase, m->nphase, m->repo_root, pkg,
					KB_MAX_PKGS, status, sizeof(status));
	if (npkg < 0)
		npkg = 0;
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

	PlanRow row[KBUILD_MAX_PHASES * (KBUILD_MAX_STEPS + 1)];
	int nrow = 0, sel = 0, off = 0, accepted = 0;

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

		PlanView vw = { m, phase_on, steps, nsteps, step_on, pkg, npkg,
				rebuild, nrebuild, row, nrow, sel, off, status };
		draw_plan_frame(&vw);
		off = vw.off;
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

/* The archive's throughput over the last two minutes. It is sampled here
 * rather than in the sampler because the snapshot engine drives its own tick
 * loop and the sampler is not pumped during a restore. */
static double snap_rate_hist[120];
static int snap_nrate;
static double snap_last_sample;

static void snap_sample(const SnapActivity *a)
{
	double now = kb_now_s();
	if (now - snap_last_sample < 1.0)
		return;
	snap_last_sample = now;
	int cap = (int)(sizeof(snap_rate_hist) / sizeof(snap_rate_hist[0]));
	if (snap_nrate == cap) {
		memmove(snap_rate_hist, snap_rate_hist + 1,
			sizeof(snap_rate_hist[0]) * (size_t)(cap - 1));
		snap_nrate--;
	}
	snap_rate_hist[snap_nrate++] = a->rate;
}

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

	snap_sample(a);

	char line[320];
	snprintf(line, sizeof(line), "%s %s %s%s%s%s", a->action, a->phase,
		 a->path, a->layer[0] ? " (" : "", a->layer,
		 a->layer[0] ? ")" : "");
	ktui_draw_text(r.x + 2, r.y + 1, w - 4, line, KT_TEXT, KT_SURFACE,
		       KT_A_BOLD);

	double frac = a->est_bytes > 0 ? (double)a->bytes / (double)a->est_bytes
				       : 0;
	if (frac > 1)
		frac = 1;
	/* Always pulsing: this panel is only on screen while a tar is running,
	 * and a forty-minute archive that has not moved a cell is the exact
	 * case where the user starts wondering whether it has hung. */
	ktui_progress_ex(krect(r.x + 2, r.y + 3, w - 4, 1), frac, NULL,
			 KT_BAR_TIP | KT_BAR_PULSE, KT_SURFACE);

	/* Row-margin convention for this whole panel: every line reserves two
	 * columns before the left border and two before the right one, so the
	 * last drawable interior column is r.x + w - 3.
	 *
	 * Row 4 carries two fields rather than one, and they do NOT get to
	 * share the full interior width the way a single line would: a
	 * multi-terabyte total, a nine-digit file count and an hour-scale time
	 * are all real at ordinary terminal widths, and if both fields draw
	 * across the same w-4 span the later call silently overwrites the
	 * earlier one instead of merely running long. So the interior is cut
	 * into two disjoint spans with a one-column gap between them: left
	 * gets the first lw columns, right gets the remaining rw and is
	 * right-aligned within them. Whatever either field's content does,
	 * it is truncated to its OWN span and can never reach the other's. */
	int lw = (w - 5) / 2;
	int rw = (w - 4) - lw - 1;
	int rx = r.x + 2 + lw + 1;

	snprintf(line, sizeof(line), "%s%s%s   %s files",
		 human_bytes(a->bytes), a->est_bytes ? " / " : "",
		 a->est_bytes ? human_bytes(a->est_bytes) : "",
		 human_count(a->files));
	ktui_draw_text(r.x + 2, r.y + 4, lw, line, KT_MID, KT_SURFACE, 0);

	snprintf(line, sizeof(line), "%s/s   %s elapsed",
		 human_bytes((long long)a->rate),
		 human_time(kb_now_s() - a->started));
	ktui_draw_text_right(rx, r.y + 4, rw, line, KT_MID, KT_SURFACE, 0);

	ktui_sparkline(krect(r.x + 2, r.y + 5, w - 4 > 24 ? 24 : w - 4, 1),
		       snap_rate_hist, snap_nrate, 0, KT_SURFACE);
	/* Same right-margin convention as above: the sparkline is 24 wide
	 * plus a 2-column gap (26 total), so what is left for the path is
	 * (w - 4) - 26 = w - 30, ending at r.x + w - 3 like every other line
	 * — NOT w - 28, which ran one column into the box's own border. */
	ktui_draw_text(r.x + 2 + 26, r.y + 5, w - 30, a->current, KT_DIM,
		       KT_SURFACE, 0);
	ktui_draw_text(r.x + 2, r.y + 6, w - 4, "[Q] cancel", KT_WARN,
		       KT_SURFACE, KT_A_BOLD);
}

/* When the activity screen was last drawn; 0 forces the next tick to draw. */
static double activity_drawn;

/* The redraw hook handed to the snapshot engine. Also the place a cancel is
 * noticed, because a tar can run for forty minutes. The engine calls it after
 * every read of the archiver's output, which on a tree of small files is
 * hundreds of times a second, so the frame is redrawn at most every 100 ms
 * — a full clear-draw-flush per read costs more than the read — while input
 * is read on every call, so Q is noticed at once. */
static void activity_tick(Manager *m)
{
	double now = kb_now_s();
	if (now - activity_drawn >= 0.1) {
		activity_drawn = now;
		ktui_draw_clear();
		draw_activity(m);
		ktui_draw_flush();
	}

	KtuiEvent ev;
	while (ktui_input_next(&ev, 0)) {
		if (ev.type == KT_EVT_RESIZE) {
			ktui_draw_resize();
			activity_drawn = 0;
		} else if (ev.type == KT_EVT_KEY &&
			 (ev.key == 'q' || ev.key == 'Q'))
			m->stop_requested = 1;
	}
}

void screen_progress(Manager *m, const char *title)
{
	kb_strlcpy(progress_title, title, sizeof(progress_title));
	activity_drawn = 0;
	snap_set_tick(m, activity_tick);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The build screen                                                         */

typedef struct {
	Manager *m;
	Sampler *sam;
	Layout lay;	/* OUTPUT of draw_build_frame: the loop's mouse handler
			 * reads it back to hit-test the tree and detail panes,
			 * the same first_row/off contract as the other screens'
			 * view structs. */

	BStep *visible[KB_MAX_STEPS];
	int nvisible;
	BStep *selected;
	int scroll;
	int log_scroll;
	int auto_follow;
	int quit_requested;

	char search[64];
	int searching;		/* the prompt is open and eating keys */
	int hit[512];		/* log line indices matching `search` */
	int nhit, hit_at;

	/* The step the failure panel was last dismissed FOR, not a bare open/
	 * shut flag: Esc/Enter must hide the panel without forgetting what
	 * failed (m->error_step stays set, the step stays selected), and the
	 * panel is owed a fresh appearance if a later run fails on a
	 * DIFFERENT step. "up" is simply m->error_step && != err_dismissed. */
	BStep *err_dismissed;

	/* The success panel's counterpart to err_dismissed. A bare flag is
	 * right here where a step pointer was right there: a build finishes
	 * exactly once per run, so there is no "a DIFFERENT one succeeded"
	 * case to distinguish. Cleared by a retry, which makes another finish
	 * possible. */
	int done_dismissed;

	/* The bell and the desktop notification fire once per event: a failure
	 * once until a retry, the end of the run once. */
	int notified_fail;
	int notified_end;

	/* The terminal title as last set, and when. */
	char title[256];
	double title_at;
} BuildView;

/* Marks rather than filters: a build log is read for context around the hit,
 * and a filtered view throws exactly that away. */
static void search_refresh(BuildView *v)
{
	v->nhit = 0;
	BStep *n = v->selected;
	if (!n || !v->search[0])
		return;
	for (int i = 0; i < n->nlog && v->nhit < 512; i++)
		if (log_find(n->log[i], v->search) >= 0)
			v->hit[v->nhit++] = i;
	if (v->hit_at >= v->nhit)
		v->hit_at = v->nhit ? v->nhit - 1 : 0;
}

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
		/* SKIPPED counts as finished: a step that will not run again
		 * this pass — restored from a snapshot, or a package already
		 * installed — or a phase full of skips, the ordinary case on a
		 * resumed build, could never reach 100%. */
		if (g->child[i]->status == ST_DONE ||
		    g->child[i]->status == ST_SKIPPED ||
		    g->child[i]->status == ST_FAIL)
			(*done)++;
	return g;
}

static void draw_header(BuildView *v)
{
	Manager *m = v->m;
	KRect hr = v->lay.header;
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
	ktui_draw_fill(krect(hr.x, hr.y, hr.w, 1), KT_BG);
	ktui_draw_text(hr.x, hr.y, hr.w, left, KT_ACCENT, KT_BG, KT_A_BOLD);

	char right[128] = "";
	if (m->restored_from)
		snprintf(right, sizeof(right), "restored <- %s ",
			 m->restored_from->dir_name);
	else if (m->continued_from)
		snprintf(right, sizeof(right), "continued past %s ",
			 m->continued_from->dir_name);
	if (right[0] && (int)strlen(right) + (int)strlen(left) + 4 < hr.w)
		ktui_draw_text(hr.x + hr.w - (int)strlen(right), hr.y, hr.w,
			       right, KT_WARN, KT_BG, KT_A_BOLD);

	double pct = total ? (double)done / total : 0;
	/* Elapsed times only: the step running (under --port-jobs, how many
	 * are), the phase and the whole run. Each is given up, step first,
	 * before the bar goes below ten columns. */
	char step[32] = "", phase[32] = "", run[32];
	BStep *cur = m->current_step;
	if (m->nrunning > 1)
		snprintf(step, sizeof(step), "   %d running", m->nrunning);
	else if (cur && !cur->is_group && cur->status == ST_RUNNING)
		snprintf(step, sizeof(step), "   step %s",
			 human_time(step_duration(cur)));
	if (g)
		snprintf(phase, sizeof(phase), "   phase %s",
			 human_time(step_duration(g)));
	snprintf(run, sizeof(run), "   total %s", human_time(mgr_elapsed(m)));
	char suffix[160];
	int bar_w = 0;
	for (int drop = 0; drop < 4; drop++) {
		snprintf(suffix, sizeof(suffix), "%3d%%   %d/%d%s%s%s",
			 (int)(pct * 100), done, total, drop < 1 ? step : "",
			 drop < 2 ? phase : "", drop < 3 ? run : "");
		bar_w = hr.w - ktui_utf8_width(suffix) - 2;
		if (bar_w >= 10)
			break;
	}
	if (bar_w < 10)
		bar_w = 10;
	/* Pulsing only while the build is actually running: a finished or
	 * stopped build showing a live sweep would claim work that is not
	 * happening. A phase bar can sit on the same percentage for half an
	 * hour during zig or llvm, which is exactly when it needs to look
	 * alive rather than hung. */
	ktui_progress_ex(krect(hr.x, hr.y + 1, bar_w, 1), pct, NULL,
			 KT_BAR_TIP | (m->is_running ? KT_BAR_PULSE : 0),
			 KT_BG);
	/* Right-aligned: the times' digits change width as they count and a
	 * left-aligned suffix jitters the whole line every second. */
	ktui_draw_text_right(hr.x + bar_w, hr.y + 1, hr.w - bar_w, suffix,
			     KT_TEXT, KT_BG, KT_A_BOLD);
	/* hr.h, not a literal 2: the rule belongs on the row past the header
	 * rect's own height, whatever that height is, rather than assuming the
	 * two rows the header happens to be today. */
	ktui_draw_hline(hr.x, hr.y + hr.h, hr.w, KT_G_HL, KT_DIM, KT_BG);
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
		*fg = n->installed ? KT_MID : KT_ACCENT;
		break;
	default:
		kb_strlcpy(out, " !! ", cap);
		*fg = KT_ERR;
		break;
	}
}

/* Whether the topmost visible row's own group has scrolled off the tree
 * pane, and where that leaves the scrollable content. Shared by the draw,
 * the click mapper and view_update's scroll margin so exactly ONE formula
 * decides "a header is pinned here" — trap A: writing "+1 row when pinned"
 * a second time in the click handler drifted from the draw side, and every
 * click selected the row above the one it visibly landed on.
 *
 * Structurally, if visible[scroll] is a STEP rather than a group, its
 * parent group must be at some earlier index — flatten() always emits a
 * group immediately before the children it recurses into — so "the top row
 * is a step" already means "its group scrolled off"; there is no separate
 * scroll==0 case to special-case, and no case where the top row's own group
 * is itself the top row (that group IS visible, nothing to pin). */
static BStep *tree_pin(BStep *const *visible, int nvisible, int scroll)
{
	if (scroll <= 0 || scroll >= nvisible)
		return NULL;
	BStep *top = visible[scroll];
	return top->is_group ? NULL : top->parent;
}

typedef struct {
	BStep *header;	/* NULL when nothing is pinned this frame          */
	KRect content;	/* the scrollable region -- excludes the header row */
} TreeGeom;

static TreeGeom tree_geom(const BuildView *v)
{
	KRect tr = v->lay.tree;
	TreeGeom g;
	g.header = tree_pin(v->visible, v->nvisible, v->scroll);
	g.content = g.header ? krect(tr.x, tr.y + 1, tr.w, tr.h - 1) : tr;
	return g;
}

/* One row of the tree: icon, title, and either a duration or (a group with
 * children) a completion gauge. `bg`/`sel` are parameters rather than
 * derived from `n` so the pinned header can reuse this exact drawing without
 * being rendered as the current selection — KT_SURFACE with sel=0 is what
 * makes it visibly a header and not a selectable row, even on the rare frame
 * where its group also happens to be v->selected. */
static void draw_tree_row(BStep *n, int rx, int y, int width, int bg, int sel)
{
	char icon[16];
	int icon_fg;
	node_icon(n, icon, sizeof(icon), &icon_fg);

	double dur = step_duration(n);
	char t[32] = "";
	if (n->status == ST_SKIPPED)
		kb_strlcpy(t, "(skip)", sizeof(t));
	else if (n->installed)
		kb_strlcpy(t, "(installed)", sizeof(t));
	else if (dur > 0 || n->status == ST_RUNNING) {
		int s = (int)dur;
		if (s < 60)
			snprintf(t, sizeof(t), "(%ds)", s);
		else
			snprintf(t, sizeof(t), "(%dm%02ds)", s / 60, s % 60);
	}

	ktui_draw_fill(krect(rx, y, width, 1), bg);
	int x = rx;
	if (!n->is_group)
		x += 2;
	ktui_draw_text(x, y, width - x, icon, sel ? KT_BG : icon_fg, bg,
		       KT_A_BOLD);
	x += (int)strlen(icon) > 4 ? ktui_utf8_width(icon) : 4;
	x += 1;

	/* `width` is already the rightmost usable column (the last one
	 * is the scrollbar's), so nothing here subtracts from it again:
	 * doing so leaves the title, the duration and the gauge all a
	 * column or two short of that edge and the scrollbar, not
	 * overlapping it but not reaching it either. Every one of them
	 * ends exactly at `width`.
	 *
	 * A group row with children draws the 8-column gauge below,
	 * never `t` (a group can carry a duration string too, e.g.
	 * while it is ST_RUNNING) — so the reserve for such a row is
	 * the gauge's 8 and not strlen(t), which is usually less and
	 * lets a long phase label run under a gauge it was never
	 * measured against. */
	int reserved = n->is_group && n->nchild ? 8 : (int)strlen(t);
	int room = width - x - reserved;
	if (room < 1)
		room = 1;
	ktui_draw_text(x, y, room, n->title,
		       sel ? KT_BG : n->installed ? KT_MID : KT_TEXT, bg,
		       sel ? KT_A_BOLD : 0);

	if (n->is_group && n->nchild) {
		int cdone = 0;
		for (int k = 0; k < n->nchild; k++)
			if (n->child[k]->status == ST_DONE ||
			    n->child[k]->status == ST_FAIL ||
			    n->child[k]->status == ST_SKIPPED)
				cdone++;
		int bw = 8;
		if (width - bw + 1 > x)
			ktui_gauge(width - bw + 1, y, bw,
				   (double)cdone / n->nchild,
				   sel ? KT_BG : KT_ACCENT, bg);
	} else if (t[0]) {
		ktui_draw_text_right(x, y, width - x + 1, t,
				     sel ? KT_BG
				     : (n->status == ST_SKIPPED ? KT_DIM
								: KT_MID),
				     bg, 0);
	}
}

static void draw_tree(BuildView *v)
{
	KRect tr = v->lay.tree;
	int width = tr.w - 1;	/* last column is the scrollbar */
	TreeGeom g = tree_geom(v);

	if (g.header)
		draw_tree_row(g.header, tr.x, tr.y, width, KT_SURFACE, 0);

	for (int i = 0; i < g.content.h; i++) {
		int idx = v->scroll + i;
		if (idx >= v->nvisible)
			break;
		BStep *n = v->visible[idx];
		int y = g.content.y + i;
		int sel = n == v->selected;
		{
			int rfg, rbg;

			ktui_sel_slots(sel, 1, KT_BG, &rfg, &rbg);
			draw_tree_row(n, tr.x, y, width, rbg, sel);
		}
	}

	ktui_scrollbar(krect(tr.x + tr.w - 1, g.content.y, 1, g.content.h),
		       v->nvisible, g.content.h, v->scroll);
}

/* Every phase's time: a finished one its duration, the one running its time
 * so far, one that has not started "-". The selected phase's row is bold. */
static void draw_phase_times(Manager *m, BStep *sel, int x, int w, int y,
			     int h)
{
	if (h < 2)
		return;
	char buf[256];
	snprintf(buf, sizeof(buf), "%-18s %9s", "PHASE", "TIME");
	ktui_draw_text(x, y, w, buf, KT_ACCENT, KT_BG, KT_A_BOLD);
	for (int i = 0; i < m->nroot && i + 1 < h; i++) {
		BStep *g = m->root[i];
		double d = step_duration(g);
		int running = m->is_running && g == m->current_phase;
		const char *what = "";
		int fg = KT_MID;
		char t[32];
		if (g->status == ST_SKIPPED) {
			kb_strlcpy(t, "skipped", sizeof(t));
		} else if (d <= 0) {
			kb_strlcpy(t, "-", sizeof(t));
		} else {
			kb_strlcpy(t, human_time(d), sizeof(t));
			if (running) {
				what = "  running";
				fg = KT_WARN;
			} else if (g->status == ST_FAIL) {
				what = "  failed";
				fg = KT_ERR;
			}
		}
		snprintf(buf, sizeof(buf), "%-18s %9s%s",
			 g->meta ? g->meta->dir_name : g->title, t, what);
		ktui_draw_text(x, y + 1 + i, w, buf, g == sel ? KT_TEXT : fg,
			       KT_BG, g == sel ? KT_A_BOLD : 0);
	}
}

static void draw_group_detail(Manager *m, BStep *n, int x, int w, int y,
			      int h)
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
	line++;
	draw_phase_times(m, n, x, w, y + line, h - line);
}

/* Drawn on the pane's own last row, which the caller has already excluded
 * from the log window — see the `log_bottom` split in draw_detail. */
static void draw_search_bar(BuildView *v, int x, int w, int bottom)
{
	if (!v->searching && !v->search[0])
		return;
	char pr[128];
	snprintf(pr, sizeof(pr), "/%s%s", v->search, v->searching ? "_" : "");
	ktui_draw_text(x, bottom, w, pr, KT_WARN, KT_BG, KT_A_BOLD);
	char cnt[48];
	snprintf(cnt, sizeof(cnt), "%d hit%s%s", v->nhit,
		 v->nhit == 1 ? "" : "s", v->nhit ? "  [n]/[N]" : "");
	ktui_draw_text_right(x, bottom, w, cnt, KT_DIM, KT_BG, 0);
}

static void draw_detail(BuildView *v)
{
	BStep *n = v->selected;
	if (!n)
		return;

	KRect dr = v->lay.detail;
	int x = dr.x, w = dr.w, top = dr.y, bottom = dr.y + dr.h - 1;

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

	/* The search prompt claims the pane's own last row while it is open,
	 * so the log window is one line shorter then — otherwise the bottom
	 * log line and the prompt draw on top of each other. */
	int show_search = v->searching || v->search[0];
	int log_y = top + 3;
	int log_bottom = show_search ? bottom - 1 : bottom;
	int log_h = log_bottom - log_y + 1;
	if (log_h <= 0) {
		draw_search_bar(v, x, w, bottom);
		return;
	}

	if (n->is_group) {
		/* A group only has logs when its EXPANSION failed — show those
		 * instead of the metadata, or the failure is invisible. */
		if (n->nlog && n->status == ST_FAIL) {
			int start = n->nlog > log_h ? n->nlog - log_h : 0;
			for (int i = 0; start + i < n->nlog && i < log_h; i++)
				ktui_draw_text(x, log_y + i, w, n->log[start + i],
					       KT_ERR, KT_BG, KT_A_BOLD);
			draw_search_bar(v, x, w, bottom);
			return;
		}
		draw_group_detail(v->m, n, x, w, log_y, log_h);
		draw_search_bar(v, x, w, bottom);
		return;
	}

	if (!n->nlog) {
		const char *msg = n->status == ST_PENDING ? "Pending..."
				: n->status == ST_RUNNING ? "Starting..."
				: n->status == ST_SKIPPED
					? "Skipped (restored from snapshot)"
				: n->installed
					? "Installed and current: not run."
					: "No logs.";
		ktui_draw_text(x, log_y, w, msg, KT_DIM, KT_BG, 0);
		draw_search_bar(v, x, w, bottom);
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
	for (int i = 0; start + i < end; i++) {
		int idx = start + i;
		const char *line = n->log[idx];
		int sev = log_severity(line);
		int lfg = sev == LOG_ERR ? KT_ERR
			: sev == LOG_WARN ? KT_WARN : KT_MID;
		ktui_draw_text(x, log_y + i, w, line, lfg, KT_BG, 0);

		/* The hit itself is drawn over the line in reverse, so the
		 * surrounding text keeps its severity colour. */
		int at = v->search[0] ? log_find(line, v->search) : -1;
		if (at >= 0) {
			int nlen = (int)strlen(v->search);
			if (at < w)
				ktui_draw_text(x + at, log_y + i,
					       w - at < nlen ? w - at : nlen,
					       line + at, KT_BG, KT_WARN,
					       KT_A_REVERSE);
		}
	}

	draw_search_bar(v, x, w, bottom);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The HUD's graphs
 *
 * FIXED SCALES, so a height means the same quantity on every frame: CPU and
 * memory from 0 to all of it, output on a logarithmic scale from 1 to 100,000
 * lines a second. A graph scaled to its own window draws an idle machine and
 * a saturated one as the same shape. Time is fixed too: one sample a second,
 * one braille dot column a sample, the newest at the right edge, so a cell is
 * always two seconds and the graph slides rather than rescales.
 *
 * Drawn here rather than in libktui: a change to a library changes the recipe
 * hash of every port of ours, and each would be rebuilt for it.
 *
 * No gridlines: a dotted rule across the graph reads as data at a glance.
 * The box around each graph is the scale's frame — its top border is 100%
 * (100,000 lines a second for output) and its bottom border zero.
 *
 * The glyph tiers are libktui's. A rich terminal gets braille, 2x4 dots a
 * cell, so four rows hold sixteen levels; the Linux VT's console font has no
 * braille, and gets a full block, a medium shade or a light shade per cell
 * for a quarter row filled, half or more, or less; the ascii tier the same in
 * ktui_glyph's ascii stand-ins. Both sample columns of a cell are folded to
 * their larger there, so the time axis matches the braille one.
 */

#define HUD_LEVELS 16		/* dot rows in a four-row graph             */

/* lines a second at which the log scale reaches each level: level L is
 * filled from 10^((L - 0.5) * 5/16), so a decade is 3.2 levels. */
static const double LOG_STEP[HUD_LEVELS] = {
	1.43301, 2.94273, 6.04296, 12.4094, 25.483, 52.3299, 107.461,
	220.673, 453.158, 930.572, 1910.95, 3924.19, 8058.42, 16548.2,
	33982.1, 69783.1,
};

/* Exact at both ends, and any value above zero shows: a level rounded down to
 * nothing reads as an idle machine. */
static int hud_level_lin(double f)
{
	if (f <= 0)
		return 0;
	if (f >= 1)
		return HUD_LEVELS;
	int l = (int)(f * HUD_LEVELS);
	if ((double)l < f * HUD_LEVELS)
		l++;
	return l;
}

static int hud_level_log(double v)
{
	if (v < 1)
		return 0;
	int l = 1;
	for (int i = 0; i < HUD_LEVELS; i++)
		if (v >= LOG_STEP[i])
			l = i + 1;
	return l;
}

typedef struct {
	const double *hist;
	int n;
	int logscale;
	double warn, err;	/* fractions; 0 is no such threshold        */
} HudGraph;

/* The largest sample the graph shows, -1 when it shows none. */
static double hud_peak(const HudGraph *g, int cells)
{
	double peak = -1;
	for (int i = g->n - 2 * cells; i < g->n; i++)
		if (i >= 0 && g->hist[i] > peak)
			peak = g->hist[i];
	return peak;
}

static int hud_fill_slot(const HudGraph *g, double v)
{
	if (g->err > 0 && v > g->err)
		return KT_ERR;
	if (g->warn > 0 && v > g->warn)
		return KT_WARN;
	return KT_ACCENT;
}

static void hud_glyph(int x, int y, const char *s, int fg)
{
	ktui_draw_text(x, y, 1, s, fg, KT_BG, 0);
}

static void hud_graph(KRect r, const HudGraph *g)
{
	static const unsigned char DOT[2][4] = {
		{ 0x01, 0x02, 0x04, 0x40 },
		{ 0x08, 0x10, 0x20, 0x80 },
	};
	int braille = (ktui_caps & KT_CAP_UTF8) && !(ktui_caps & KT_CAP_LINUXVT);
	int levels = 4 * r.h;

	for (int c = 0; c < r.w; c++) {
		int have[2], lvl[2];
		double top = 0;
		for (int dx = 0; dx < 2; dx++) {
			int i = g->n - 2 * r.w + 2 * c + dx;
			have[dx] = i >= 0 && i < g->n;
			double v = have[dx] ? g->hist[i] : 0;
			int l = !have[dx] ? 0
				: g->logscale ? hud_level_log(v)
				: hud_level_lin(v);
			/* A shorter graph keeps the scale and loses
			 * resolution: level 16 of 16 is its top dot too. */
			lvl[dx] = (l * levels + HUD_LEVELS - 1) / HUD_LEVELS;
			if (have[dx] && v > top)
				top = v;
		}
		int fg = hud_fill_slot(g, top);
		for (int row = 0; row < r.h; row++) {
			int x = r.x + c, y = r.y + row;
			int fill = 0;
			for (int k = 0; k < 4; k++) {
				int dy = row * 4 + k;	/* dot rows from the top */
				for (int dx = 0; dx < 2; dx++)
					if (have[dx] && lvl[dx] >= levels - dy)
						fill |= DOT[dx][k];
			}
			if (braille) {
				if (fill)
					ktui_draw_cell(x, y, 0x2800u + (uint32_t)fill,
						       fg, KT_BG, 0);
				else
					ktui_draw_cell(x, y, ' ', KT_DIM, KT_BG, 0);
				continue;
			}
			int hi = lvl[0] > lvl[1] ? lvl[0] : lvl[1];
			int part = hi - (r.h - 1 - row) * 4;
			if (part >= 4)
				hud_glyph(x, y, ktui_glyph[KT_G_FULL], fg);
			else if (part >= 2)
				hud_glyph(x, y, ktui_glyph[KT_G_SHADE_MED], fg);
			else if (part == 1)
				hud_glyph(x, y, ktui_glyph[KT_G_SHADE], fg);
			else
				ktui_draw_cell(x, y, ' ', KT_DIM, KT_BG, 0);
		}
	}
}

/* One caption line: the label in KT_MID at the left of the caption's column,
 * the value right-aligned against its right edge. hud_caption_w is the
 * widest label, space and value any line can have. */
static void hud_cap(KRect c, int row, const char *label, const char *value,
		    int fg)
{
	if (row >= c.h)
		return;
	ktui_draw_text(c.x, c.y + row, c.w, label, KT_MID, KT_BG, 0);
	int lw = ktui_utf8_width(label) + 1;
	if (lw < c.w)
		ktui_draw_text_right(c.x + lw, c.y + row, c.w - lw, value, fg,
				     KT_BG, 0);
}

/* At most seven characters: grouped to 999,999, then 1.0M up. */
static const char *hud_count(double v)
{
	static char out[4][16];
	static int slot;
	char *b = out[slot++ & 3];
	if (v < 999999.5)
		kb_strlcpy(b, human_count((long long)(v + 0.5)), 16);
	else if (v < 999.95e6)
		snprintf(b, 16, "%.1fM", v / 1e6);
	else
		snprintf(b, 16, "%.1fG", v / 1e9);
	return b;
}

/* ── the duration bands of the heat strip ─────────────────────────────── */

#define HEAT_BANDS 5
static const double HEAT_EDGE[HEAT_BANDS - 1] = { 10, 60, 600, 3600 };
static const char *const HEAT_NAME[HEAT_BANDS] = { "<10s", "<1m", "<10m",
						   "<1h", "1h+" };
static const int HEAT_SLOT[HEAT_BANDS] = { KT_MID, KT_MID, KT_ACCENT, KT_WARN,
					   KT_ERR };

static int heat_band(double secs)
{
	int b = 0;
	while (b < HEAT_BANDS - 1 && secs >= HEAT_EDGE[b])
		b++;
	return b;
}

/* The bands step up the vertical ramp — taller is slower — and the colour
 * tells apart what the VT's three glyphs cannot. */
static void heat_cell(int x, int y, int band)
{
	ktui_draw_text(x, y, 1, ktui_ramp_v((band + 1) / (double)HEAT_BANDS),
		       HEAT_SLOT[band], KT_BG, 0);
}

/* "▂<10s ▄<1m ..." */
static int heat_legend_w(void)
{
	int w = 0;
	for (int b = 0; b < HEAT_BANDS; b++)
		w += (b ? 1 : 0) + 1 + (int)strlen(HEAT_NAME[b]);
	return w;
}

static void heat_legend(int x, int y)
{
	for (int b = 0; b < HEAT_BANDS; b++) {
		if (b)
			x++;
		heat_cell(x++, y, b);
		x += ktui_draw_text(x, y, 8, HEAT_NAME[b], KT_MID, KT_BG, 0);
	}
}

/* One cell per finished step in run order, or, with more steps than cells,
 * the slowest of each cell's share: a short slow port must not average away
 * into its fast neighbours. */
static void heat_strip(KRect r, const double *v, int n)
{
	int cells = n < r.w ? n : r.w;
	for (int i = 0; i < cells; i++) {
		int lo = (int)((long long)i * n / cells);
		int hi = (int)((long long)(i + 1) * n / cells);
		if (hi <= lo)
			hi = lo + 1;
		double slow = 0;
		for (int k = lo; k < hi && k < n; k++)
			if (v[k] > slow)
				slow = v[k];
		heat_cell(r.x + i, r.y, heat_band(slow));
	}
}

/* `text` in at most two lines of `w`, broken at the last space that fits;
 * the second line is cut at the edge. */
static void wrap2(int x, int y, int w, int rows, const char *text, int fg)
{
	if (rows <= 0 || w <= 0)
		return;
	const char *p = text, *brk = NULL;
	int used = 0;
	while (*p) {
		uint32_t cp;
		const char *next = ktui_utf8_next(p, &cp);
		if (used + 1 > w)
			break;
		if (cp == ' ')
			brk = p;
		used++;
		p = next;
	}
	if (!*p || rows < 2) {
		ktui_draw_text(x, y, w, text, fg, KT_BG, 0);
		return;
	}
	const char *cut = brk ? brk : p;
	char first[512];
	size_t n = (size_t)(cut - text);
	if (n >= sizeof(first))
		n = sizeof(first) - 1;
	memcpy(first, text, n);
	first[n] = 0;
	ktui_draw_text(x, y, w, first, fg, KT_BG, 0);
	while (*cut == ' ')
		cut++;
	ktui_draw_text(x, y + 1, w, cut, fg, KT_BG, 0);
}

/* One sample per COMPLETED step, in run order. Rebuilt each frame rather than
 * cached: the array is at most KB_MAX_STEPS doubles and the alternative is a
 * second copy of the step list to keep in sync. m->order is itself capped at
 * KB_MAX_STEPS (manager.c clamps every insert), and cap enforces the same
 * bound on out[] independently of that invariant. */
static int heat_samples(Manager *m, double *out, int cap)
{
	int n = 0;
	for (int i = 0; i < m->norder && n < cap; i++) {
		BStep *s = m->order[i];
		if (s->is_group || s->status == ST_PENDING ||
		    s->status == ST_RUNNING)
			continue;
		out[n++] = step_duration(s);
	}
	return n;
}

/* rootfs, the heat strip and the last notice. Each field is drawn only if it
 * fits whole in the panel; a notice is prose, and wraps instead. */
static void hud_info(KRect r, Manager *m, Sampler *s)
{
	int y = r.y;
	char notice[300] = "";
	if (m->nnotice)
		snprintf(notice, sizeof(notice), "%s %s", ktui_glyph[KT_G_BULLET],
			 m->notice[m->nnotice - 1].text);

	/* One row has room for one thing, and the newest notice is the one
	 * that says what just happened. */
	if (r.h < 4 && notice[0]) {
		ktui_draw_text(r.x, y, r.w, notice, KT_WARN, KT_BG, 0);
		return;
	}

	int x = r.x, right = r.x + r.w;
	x += ktui_draw_text(x, y, right - x, "rootfs", KT_MID, KT_BG, 0);
	if (s->fs_sampled_at > 0) {
		char part[3][64];
		snprintf(part[0], sizeof(part[0]), " %s", human_bytes(s->fs_bytes));
		snprintf(part[1], sizeof(part[1]), "  %s files",
			 human_count(s->fs_files));
		snprintf(part[2], sizeof(part[2]), "%s",
			 s->fs_partial ? " (partial)" : "");
		for (int i = 0; i < 3 && part[i][0]; i++) {
			int pw = ktui_utf8_width(part[i]);
			if (x + pw > right)
				break;
			x += ktui_draw_text(x, y, pw, part[i],
					    i ? KT_MID : KT_TEXT, KT_BG, 0);
		}
	} else if (x + 4 <= right) {
		ktui_draw_text(x, y, 4, " ...", KT_MID, KT_BG, 0);
	}
	if (r.h < 2)
		return;

	static double heat[KB_MAX_STEPS];
	int nheat = heat_samples(m, heat, KB_MAX_STEPS);
	y++;
	x = r.x + ktui_draw_text(r.x, y, 5, "heat ", KT_MID, KT_BG, 0);
	int lw = heat_legend_w();
	int legend = right - x - lw - 1 >= HUD_GRAPH_MIN;
	int hw = right - x - (legend ? lw + 1 : 0);
	if (nheat)
		heat_strip(krect(x, y, hw, 1), heat, nheat);
	else
		ktui_draw_text(x, y, hw, "no step finished yet", KT_MID, KT_BG, 0);
	if (legend)
		heat_legend(right - lw, y);

	if (notice[0])
		wrap2(r.x, y + 1, r.w, r.h - 2, notice, KT_WARN);
}

static void draw_hud(BuildView *v)
{
	Sampler *s = v->sam;
	Manager *m = v->m;
	KRect r = v->lay.hud;
	if (!s || r.h <= 0)
		return;
	HudLayout hl = hud_compute(r);
	char val[32];

	static const char *const TITLE[HUD_NGRAPH + 1] = {
		"CPU", "Memory", "Output", "Build",
	};
	for (int i = 0; hl.boxed && i <= HUD_INFO; i++)
		if (hl.box[i].w)
			ktui_draw_box(hl.box[i], TITLE[i], KT_DIM, KT_BG, 0);

	if (hl.graph[HUD_CPU].w) {
		KRect gr = hl.graph[HUD_CPU], c = hl.caption[HUD_CPU];
		HudGraph g = { s->cpu_hist, s->nhist, 0, 0.85, 0 };
		hud_graph(gr, &g);
		snprintf(val, sizeof(val), "%d%%", (int)(s->cpu_busy * 100 + 0.5));
		hud_cap(c, 0, "cpu", val, hud_fill_slot(&g, s->cpu_busy));
		double pk = hud_peak(&g, gr.w);
		if (pk < 0)
			kb_strlcpy(val, "-", sizeof(val));
		else
			snprintf(val, sizeof(val), "%d%%", (int)(pk * 100 + 0.5));
		hud_cap(c, 1, "peak", val, KT_TEXT);
		snprintf(val, sizeof(val), "%d", s->ncpu);
		hud_cap(c, 2, "cpus", val, KT_TEXT);
		snprintf(val, sizeof(val), s->load1 < 1000 ? "%.2f" : "%.0f",
			 s->load1);
		hud_cap(c, 3, "load", val, KT_TEXT);
	}

	if (hl.graph[HUD_MEM].w) {
		KRect c = hl.caption[HUD_MEM];
		HudGraph g = { s->mem_hist, s->nhist, 0, 0.85, 0.95 };
		hud_graph(hl.graph[HUD_MEM], &g);
		double f = s->mem_total ? (double)s->mem_used / s->mem_total : 0;
		hud_cap(c, 0, "mem", human_bytes(s->mem_used),
			hud_fill_slot(&g, f));
		hud_cap(c, 1, "of", human_bytes(s->mem_total), KT_TEXT);
		/* Swap in use is a warning only while it is still growing: a
		 * build that swapped once an hour ago and stopped is not one
		 * that is swapping. */
		int rising = s->swap_used > 0 && s->swap_rose_at > 0 &&
			     kb_now_s() - s->swap_rose_at < 10;
		hud_cap(c, 2, "swap", human_bytes(s->swap_used),
			rising ? KT_WARN : KT_TEXT);
		hud_cap(c, 3, "free", human_bytes(s->disk_free), KT_TEXT);
	}

	if (hl.graph[HUD_OUT].w) {
		KRect gr = hl.graph[HUD_OUT], c = hl.caption[HUD_OUT];
		HudGraph g = { s->rate_hist, s->nhist, 1, 0, 0 };
		hud_graph(gr, &g);
		snprintf(val, sizeof(val), "%s/s", hud_count(s->lines_per_sec));
		hud_cap(c, 0, "out", val, KT_TEXT);
		double pk = hud_peak(&g, gr.w);
		hud_cap(c, 1, "peak", pk < 0 ? "-" : hud_count(pk), KT_TEXT);
		hud_cap(c, 2, "scale", "log", KT_TEXT);
		hud_cap(c, 3, "lines", hud_count((double)m->total_lines), KT_TEXT);
	}

	if (hl.info.w)
		hud_info(hl.info, m, s);
}

/* [R] is offered once the failure has stopped the run: nothing is running,
 * no level is still draining, no snapshot is being written. */
static int can_retry(const Manager *m)
{
	return m->error_step && !m->is_running && !m->nrunning &&
	       !m->snap.active;
}

static void draw_footer(BuildView *v)
{
	Manager *m = v->m;
	KRect fr = v->lay.footer;
	/* Suffix-style hints: the bold key letter plus the label's remaining
	 * letters spell the whole word (e.g. "[E]:rror" reads as "Error"),
	 * which is what fits '/', E and O onto a row that also carries T and
	 * F/S/Q — full-word labels do not leave room. PGUP/DN drops off the
	 * row for the same reason; it stays discoverable by trying the obvious
	 * key while a log is on screen. Q leads the list rather than trailing
	 * it: keyhint() drops whatever doesn't fit from the END, and 80
	 * columns is the most common width, so the most important key placed
	 * last is the one lost exactly there. R follows it while a failure can
	 * be retried, for the same reason. */
	const char *keys[10] = { "Q", arrow_ud(), "SPC", "/", "E", "O", "F", "S",
				 "T" };
	const char *labels[10] = { "uit", "sel", "fold", "find", "rror", "pen",
				   "ollow", "nap", "heme" };
	int nkeys = 9;
	if (can_retry(m)) {
		memmove(keys + 2, keys + 1, sizeof(keys[0]) * 8);
		memmove(labels + 2, labels + 1, sizeof(labels[0]) * 8);
		keys[1] = "R";
		labels[1] = "etry";
		nkeys = 10;
	}
	ktui_draw_fill(krect(fr.x, fr.y, fr.w, 1), KT_BG);

	char right[128];
	snprintf(right, sizeof(right), "%s%s ",
		 v->auto_follow ? "following" : "manual",
		 m->is_running ? "" : "  (finished)");
	int rw = (int)strlen(right);
	/* Reserve the indicator's ACTUAL width, not a fixed guess: the nine-hint
	 * block below is 81 columns wide, so a flat 40-column reserve let the
	 * indicator draw straight on top of the hints at any width <= 91. */
	int reserve = rw + 2 <= fr.w ? rw + 2 : 0;
	keyhint(krect(fr.x, fr.y, fr.w - reserve, 1), keys, labels, nkeys);

	if (reserve)
		ktui_draw_text(fr.x + fr.w - 1 - rw, fr.y, rw, right,
			       v->auto_follow ? KT_ACCENT : KT_WARN, KT_BG,
			       KT_A_BOLD);
}

/* Rows of upcoming list kept visible below the selection — Feature 3. */
#define TREE_CONTEXT 3

static void view_update(BuildView *v)
{
	Manager *m = v->m;
	v->nvisible = 0;
	flatten(v, m->root, m->nroot);

	if (v->auto_follow && m->current_step)
		v->selected = m->current_step;
	if (!v->selected && v->nvisible)
		v->selected = v->visible[0];

	/* Runs every pump, not just on a selection edge: it also has to pick
	 * up new hits as a followed step's log keeps growing, and it is a
	 * no-op the moment `search` is empty. */
	search_refresh(v);

	int sel_idx = 0;
	for (int i = 0; i < v->nvisible; i++)
		if (v->visible[i] == v->selected) {
			sel_idx = i;
			break;
		}

	/* v->lay is set by draw_build_frame, which runs AFTER this pump in
	 * screen_build's loop — so lay.tree is the PREVIOUS frame's layout,
	 * one frame stale. That is acceptable: the terminal cannot resize
	 * mid-pump, so last frame's height is still this frame's height in
	 * every case but the very first, and on the first frame BuildView is
	 * still zeroed (nothing has drawn yet), so tree.h reads 0 and would
	 * clamp scroll to a single row for exactly one frame. draw_build_frame
	 * computes the REAL layout before draw_tree runs, so that one frame
	 * is never actually rendered wrong — only this scroll math is briefly
	 * conservative, and it corrects itself on the very next pump. */
	int rows = v->lay.tree.h;
	/* A pinned header (tree_pin) consumes the tree pane's own first row,
	 * so only tr.h - 1 rows are ever scrollable — trap C: the bottom
	 * margin below has to know that, or it claims a row the header is
	 * about to take back and the last line of "context" ends up hidden
	 * under the very header that made room necessary. Whether a header
	 * WILL be pinned depends on the scroll value this function is about
	 * to recompute, so this reads last frame's scroll — the same
	 * one-frame-stale trade `lay` above already makes. */
	if (tree_pin(v->visible, v->nvisible, v->scroll))
		rows -= 1;
	if (rows < 1)
		rows = 1;

	if (sel_idx < v->scroll)
		v->scroll = sel_idx;

	/* Keep TREE_CONTEXT rows of the list visible BELOW the selection, so
	 * the steps about to run are already on screen instead of appearing
	 * only once they become current. Bounded by max_scroll just below, so
	 * a selection near the very end of the list is never scrolled past
	 * its own end just to manufacture blank "context" that does not
	 * exist — Feature 3's own trap. */
	int want_bottom = sel_idx + TREE_CONTEXT;
	if (want_bottom >= v->scroll + rows)
		v->scroll = want_bottom - rows + 1;

	if (v->scroll < 0)
		v->scroll = 0;
	int max_scroll = v->nvisible - rows;
	if (max_scroll < 0)
		max_scroll = 0;
	if (v->scroll > max_scroll)
		v->scroll = max_scroll;
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

/* The drawing half of screen_build, split out of its loop so `--preview` can
 * call it with a synthetic Manager and no terminal underneath. The loop calls
 * exactly this and nothing else — a second drawing path would be a second
 * thing to keep in agreement with the one people actually look at. */
/* Drop out of the TUI and page the step's log file — shared by the normal
 * [O] keybinding (on whatever is selected) and the failure panel's [O] (on
 * m->error_step specifically, which is not always the same node once the
 * user has clicked around after a failure). */
static void open_step_log(Manager *m, BStep *n)
{
	if (!n || n->is_group)
		return;
	char p[600];
	log_path_for(m, n, p, sizeof(p));
	if (!kb_path_exists(p)) {
		mgr_notice(m, "no log file for %s yet", n->title);
		return;
	}
	/* The pager name arrives from PAGER, never through a shell —
	 * ktui_run_console execs argv directly. */
	const char *pager = getenv("PAGER");
	char *const argv2[] = { (char *)(pager && *pager ? pager : "less"), p,
				NULL };
	ktui_run_console(argv2);
	ktui_draw_invalidate();
}

/* The last few LOG_ERR lines, oldest first. Walked from the tail rather than
 * the head: on a long step the lines that actually explain the failure are
 * the ones closest to the end, and a head-first scan of a 2000-line cap can
 * fill its whole quota on an unrelated "error_at_line" style false-positive
 * long before it reaches them. */
#define FAIL_LOG_LINES 4

static int collect_err_lines(const BStep *n, const char *out[], int max)
{
	int c = 0;
	for (int i = n->nlog - 1; i >= 0 && c < max; i--)
		if (log_severity(n->log[i]) == LOG_ERR)
			out[c++] = n->log[i];
	for (int i = 0; i < c / 2; i++) {
		const char *t = out[i];
		out[i] = out[c - 1 - i];
		out[c - 1 - i] = t;
	}
	return c;
}

/* The failure overlay — same hand-drawn-panel shape as draw_activity (see
 * its header comment): a filled KRect, a box, text confined to r.x+2 ..
 * r.x+w-3. Every width below is measured against `iw`, never against ktui_w,
 * so the panel cannot be made to draw outside its own rect by a wide or
 * narrow terminal — this file has shipped that exact defect four times
 * (trap D), and the fix every time was "stop trusting the terminal width
 * and trust the rect instead". Below the size that fits the fixed layout
 * below, the panel is not drawn AT ALL rather than clipped: a panel that
 * silently drops its last two lines of context is worse than no panel. */
static void draw_failure_panel(Manager *m, BStep *n)
{
	if (!n)
		return;
	int w = ktui_w - 8;
	int h = 9 + FAIL_LOG_LINES;	/* 2 border + 6 fixed lines + N err + 1 keys */
	if (w < 60 || h > ktui_h - 2)
		return;
	KRect r = krect(4, (ktui_h - h) / 2, w, h);

	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, " STEP FAILED ", KT_ERR, KT_SURFACE, 1);

	int x = r.x + 2, iw = w - 4;
	int y = r.y + 1;
	char buf[700];	/* room for the "log         " prefix ahead of a full
			 * 600-byte path -- 600 alone made GCC's own truncation
			 * checker flag the snprintf below, correctly: the two
			 * summed can exceed a 600-byte destination. */

	snprintf(buf, sizeof(buf), "step        %s", n->title);
	ktui_draw_text(x, y++, iw, buf, KT_TEXT, KT_SURFACE, KT_A_BOLD);

	BStep *ph = mgr_phase_of(n);
	snprintf(buf, sizeof(buf), "phase       %s", ph ? ph->title : "-");
	ktui_draw_text(x, y++, iw, buf, KT_MID, KT_SURFACE, 0);

	/* Exit code and duration share a row, split into two disjoint spans
	 * the same way draw_activity's byte/rate row is: two independent
	 * snprintf calls into the SAME width would let the second silently
	 * overwrite the first instead of merely running long. */
	int lw = iw / 2, rw = iw - lw - 1;
	snprintf(buf, sizeof(buf), "exit code   %d", n->return_code);
	ktui_draw_text(x, y, lw, buf, KT_ERR, KT_SURFACE, KT_A_BOLD);
	snprintf(buf, sizeof(buf), "duration %s", human_time(step_duration(n)));
	ktui_draw_text_right(x + lw + 1, y, rw, buf, KT_MID, KT_SURFACE, 0);
	y++;

	char path[600];
	log_path_for(m, n, path, sizeof(path));
	snprintf(buf, sizeof(buf), "log         %s", path);
	ktui_draw_text(x, y++, iw, buf, KT_MID, KT_SURFACE, 0);

	ktui_draw_hline(r.x + 1, y++, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	const char *err[FAIL_LOG_LINES];
	int nerr = collect_err_lines(n, err, FAIL_LOG_LINES);
	if (nerr) {
		ktui_draw_text(x, y++, iw, "last errors:", KT_DIM, KT_SURFACE, 0);
		for (int i = 0; i < nerr; i++)
			ktui_draw_text(x, y++, iw, err[i], KT_ERR, KT_SURFACE, 0);
	} else {
		ktui_draw_text(x, y++, iw, "(no lines classified as an error)",
			       KT_DIM, KT_SURFACE, 0);
	}

	/* Pinned to the panel's own last interior row regardless of how many
	 * error lines actually printed above, so the keys are always in the
	 * same place rather than chasing a variable-height log excerpt. A key
	 * that does not fit whole is left off, from the end. */
	static const char *const KEYS[] = { "[R] retry phase", "[O] open log",
					    "[C] copy path",
					    "[Esc/Enter] dismiss" };
	int kx = x, first = 1;
	for (int i = can_retry(m) ? 0 : 1; i < 4; i++) {
		int kw = (first ? 0 : 3) + (int)strlen(KEYS[i]);
		if (kx + kw > x + iw)
			break;
		if (!first)
			kx += 3;
		kx += ktui_draw_text(kx, r.y + h - 2, x + iw - kx, KEYS[i],
				     KT_WARN, KT_SURFACE, KT_A_BOLD);
		first = 0;
	}
}

/* Every phase ran and none failed. Deliberately NOT just `!is_running`: a run
 * the user stopped with Q, or one that never started a step, has not completed
 * and must not be congratulated. m->stop_requested covers both the graceful
 * stop and the force quit. */
static int build_completed_ok(const Manager *m)
{
	if (m->is_running || m->error_step || m->stop_requested)
		return 0;
	if (m->start_time <= 0)
		return 0;
	/* A plan can legitimately select a subset, so "every phase" means
	 * every phase this run intended to run — a skipped-by-plan phase is
	 * not an unfinished one. */
	for (int i = 0; i < m->nroot; i++)
		if (m->root[i]->status != ST_DONE &&
		    m->root[i]->status != ST_SKIPPED)
			return 0;
	return 1;
}

static void draw_success_panel(Manager *m)
{
	int w = ktui_w - 8;
	/* Sized to its content rather than to a round number: 2 border, four
	 * fact lines, a rule, the last notice when there is one, and the keys.
	 * A fixed height left two dead rows above the keys. */
	int h = 8 + (m->nnotice ? 1 : 0);
	if (w < 60 || h > ktui_h - 2)
		return;
	KRect r = krect(4, (ktui_h - h) / 2, w, h);

	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, " BUILD COMPLETE ", KT_ACCENT, KT_SURFACE, 1);

	int x = r.x + 2, iw = w - 4;
	int y = r.y + 1;
	char buf[700];

	int phases = 0, done = 0, installed = 0, skipped = 0;
	for (int i = 0; i < m->nroot; i++)
		if (m->root[i]->status == ST_DONE)
			phases++;
	for (int i = 0; i < m->norder; i++) {
		if (m->order[i]->is_group)
			continue;
		if (m->order[i]->installed)
			installed++;
		else if (m->order[i]->status == ST_DONE)
			done++;
		else if (m->order[i]->status == ST_SKIPPED)
			skipped++;
	}

	snprintf(buf, sizeof(buf), "phases      %d of %d", phases, m->nroot);
	ktui_draw_text(x, y++, iw, buf, KT_TEXT, KT_SURFACE, KT_A_BOLD);

	snprintf(buf, sizeof(buf), "steps       %d run, %d installed, %d skipped",
		 done, installed, skipped);
	ktui_draw_text(x, y++, iw, buf, KT_MID, KT_SURFACE, 0);

	/* Two disjoint spans, the same rule draw_activity and the failure
	 * panel follow: two snprintfs into one width let the second overwrite
	 * the first rather than merely run long. */
	int lw = iw / 2, rw = iw - lw - 1;
	snprintf(buf, sizeof(buf), "elapsed     %s",
		 human_time(mgr_elapsed(m)));
	ktui_draw_text(x, y, lw, buf, KT_ACCENT, KT_SURFACE, KT_A_BOLD);
	if (m->restored_from)
		snprintf(buf, sizeof(buf), "restored <- %s",
			 m->restored_from->dir_name);
	else if (m->continued_from)
		snprintf(buf, sizeof(buf), "continued past %s",
			 m->continued_from->dir_name);
	else
		snprintf(buf, sizeof(buf), "from scratch");
	ktui_draw_text_right(x + lw + 1, y, rw, buf, KT_MID, KT_SURFACE, 0);
	y++;

	/* The artefact is the point of the whole run, so it gets its own line
	 * with a size — an ISO that exists but is 0 bytes has happened, and a
	 * bare path would not have shown it. */
	char iso[600];
	snprintf(iso, sizeof(iso), "%s/iso-build/kdos.iso", m->build_dir);
	struct stat ist;
	long long isz = stat(iso, &ist) == 0 ? (long long)ist.st_size : -1;
	if (isz > 0)
		snprintf(buf, sizeof(buf), "iso         %s  (%s)", iso,
			 human_bytes(isz));
	else
		snprintf(buf, sizeof(buf), "iso         not produced by this run");
	ktui_draw_text(x, y++, iw, buf, isz > 0 ? KT_TEXT : KT_DIM, KT_SURFACE,
		       0);

	ktui_draw_hline(r.x + 1, y++, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	if (m->nnotice)
		ktui_draw_text(x, y++, iw, m->notice[m->nnotice - 1].text,
			       KT_WARN, KT_SURFACE, 0);

	ktui_draw_text(x, r.y + h - 2, iw,
		       "[Esc/Enter] dismiss   [Q] quit", KT_WARN, KT_SURFACE,
		       KT_A_BOLD);
}

static void draw_build_frame(BuildView *v)
{
	ktui_draw_clear();
	ktui_draw_box(krect(0, 0, ktui_w, ktui_h), " KDOS BUILD SYSTEM ",
		      KT_ACCENT, KT_BG, 0);

	v->lay = layout_compute(ktui_w, ktui_h);
	if (v->lay.too_small) {
		ktui_toosmall(" KDOS BUILD SYSTEM ", 40, 10);
		return;
	}

	if (v->lay.has_header)
		draw_header(v);
	if (v->lay.has_detail) {
		ktui_draw_vline(v->lay.divider.x, v->lay.divider.y,
				v->lay.divider.h, KT_G_VL, KT_DIM, KT_BG);
		draw_detail(v);
	}
	draw_tree(v);
	if (v->lay.hud_rows)
		draw_hud(v);
	draw_footer(v);

	if (v->m->snap.active)
		draw_activity(v->m);
	else if (v->m->error_step && v->m->error_step != v->err_dismissed)
		draw_failure_panel(v->m, v->m->error_step);
	else if (build_completed_ok(v->m) && !v->done_dismissed)
		draw_success_panel(v->m);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* The terminal outside the frame: bell, notification and title
 *
 * Written through libktui's own output, as its OSC 52 copy is, so they go out
 * between frames rather than inside one. The Linux VT has neither a title nor
 * a notification, so it gets the bell alone.
 */

/* `s` with every byte a terminal could read as the end of the sequence or as
 * a control dropped, and in a field of OSC 777 the `;` that separates them. */
static void osc_clean(char *out, size_t cap, const char *s, int field)
{
	size_t o = 0;
	for (; s && *s && o + 1 < cap; s++) {
		unsigned char c = (unsigned char)*s;
		if (c < 0x20 || c == 0x7f || (field && c == ';'))
			continue;
		out[o++] = (char)c;
	}
	out[o] = 0;
}

static void term_osc(const char *body)
{
	ktui_term_write("\033]", 2);
	ktui_term_write(body, strlen(body));
	ktui_term_write("\007", 1);
}

/* The bell, then a desktop notification in both the forms terminals take:
 * OSC 777 (title and body) and OSC 9 (body alone). */
static void notify(const char *title, const char *body)
{
	ktui_term_write("\007", 1);
	if (!(ktui_caps & KT_CAP_LINUXVT)) {
		char t[128], b[256], seq[420];
		osc_clean(t, sizeof(t), title, 1);
		osc_clean(b, sizeof(b), body, 1);
		snprintf(seq, sizeof(seq), "777;notify;%s;%s", t, b);
		term_osc(seq);
		snprintf(seq, sizeof(seq), "9;%s", b);
		term_osc(seq);
	}
	ktui_term_flush();
}

static void set_title(const char *text)
{
	if (ktui_caps & KT_CAP_LINUXVT)
		return;
	char t[256], seq[280];
	osc_clean(t, sizeof(t), text, 0);
	snprintf(seq, sizeof(seq), "2;%s", t);
	term_osc(seq);
	ktui_term_flush();
}

/* CSI 22;0t saves the title on xterm's stack and CSI 23;0t puts it back; a
 * terminal without the stack ignores both, and keeps "kdosbuild". */
void term_title_begin(void)
{
	if (ktui_caps & KT_CAP_LINUXVT)
		return;
	ktui_term_write("\033[22;0t", 7);
	ktui_term_flush();
}

void term_title_end(void)
{
	if (ktui_caps & KT_CAP_LINUXVT)
		return;
	set_title("kdosbuild");
	ktui_term_write("\033[23;0t", 7);
	ktui_term_flush();
}

/* At most once a second, and only when the text changed. */
static void title_update(BuildView *v)
{
	Manager *m = v->m;
	double now = kb_now_s();
	if (now - v->title_at < 1.0)
		return;
	char t[256];
	if (m->error_step) {
		BStep *ph = mgr_phase_of(m->error_step);
		snprintf(t, sizeof(t), "kdosbuild FAILED %s %s",
			 m->error_step->title,
			 ph && ph->meta ? ph->meta->dir_name : "");
	} else if (!m->is_running) {
		snprintf(t, sizeof(t), "kdosbuild %s %s",
			 build_completed_ok(m) ? "done" : "stopped",
			 human_time(mgr_elapsed(m)));
	} else {
		int done, total;
		BStep *g = phase_progress(m, &done, &total);
		snprintf(t, sizeof(t), "kdosbuild %s %d/%d %s",
			 g && g->meta ? g->meta->dir_name : "-", done, total,
			 human_time(mgr_elapsed(m)));
	}
	v->title_at = now;
	if (!strcmp(t, v->title))
		return;
	kb_strlcpy(v->title, t, sizeof(v->title));
	set_title(t);
}

/* A failure is announced once, until a retry; the end of the run once. */
static void notify_update(BuildView *v)
{
	Manager *m = v->m;
	char body[256];
	if (m->error_step && !v->notified_fail) {
		v->notified_fail = 1;
		BStep *ph = mgr_phase_of(m->error_step);
		snprintf(body, sizeof(body), "%s failed in %s (exit %d)",
			 m->error_step->title,
			 ph && ph->meta ? ph->meta->dir_name : "?",
			 m->error_step->return_code);
		notify("kdosbuild: step failed", body);
	}
	if (!m->is_running && !m->error_step && m->start_time > 0 &&
	    !v->notified_end) {
		v->notified_end = 1;
		int ok = build_completed_ok(m);
		snprintf(body, sizeof(body), "%s after %s",
			 ok ? "every phase finished" : "stopped",
			 human_time(mgr_elapsed(m)));
		notify(ok ? "kdosbuild: build complete" : "kdosbuild: build stopped",
		       body);
	}
}

/* [R]: retry the failed step's phase. Every pointer the view holds into that
 * phase's rows is dropped before they are freed; the view then follows the
 * run again from kpkg's step. */
static void retry(BuildView *v)
{
	Manager *m = v->m;
	char why[128];
	if (!can_retry(m)) {
		mgr_notice(m, "nothing to retry while a step is running");
		return;
	}
	v->selected = NULL;
	v->err_dismissed = NULL;
	v->nvisible = 0;
	v->nhit = v->hit_at = 0;
	v->log_scroll = 0;
	v->scroll = 0;
	if (mgr_retry(m, why, sizeof(why)) < 0) {
		mgr_notice(m, "retry: %s", why);
		return;
	}
	v->auto_follow = 1;
	v->done_dismissed = 0;
	v->notified_fail = 0;
	v->notified_end = 0;
	v->selected = m->current_phase;
}

void screen_build(Manager *m, Sampler *sam)
{
	BuildView v = {0};
	v.m = m;
	v.sam = sam;
	v.auto_follow = 1;

	mgr_start(m);
	screen_progress(m, " SNAPSHOT ");

	for (;;) {
		int wait_ms = mgr_pump(m);
		sam_pump(sam, m);
		view_update(&v);
		notify_update(&v);
		title_update(&v);

		draw_build_frame(&v);
		ktui_draw_flush();
		if (v.lay.too_small) {
			KtuiEvent small;
			if (ktui_input_next(&small, 200)) {
				if (small.type == KT_EVT_RESIZE)
					ktui_draw_resize();
				/* A finished build has no in-flight step to
				 * protect, so the quit key can act right here
				 * instead of only through the switch below --
				 * without this, a build that finished while
				 * the terminal was under 40x10 could only be
				 * left by resizing it back up. */
				else if (!m->is_running &&
					 small.type == KT_EVT_KEY &&
					 (small.key == 'q' || small.key == 'Q'))
					break;
			}
			continue;
		}

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
			int in_log = v.lay.has_detail &&
				     krect_hit(v.lay.detail, ev.mx, ev.my);
			/* The wheel is routed by PANE. Always scrolling the
			 * tree, even with the pointer over the log pane,
			 * yanks the selection out from under a log somebody
			 * is reading. */
			if (ev.btn == KT_MB_WHEEL_UP) {
				if (in_log)
					v.log_scroll += 3;
				else
					view_move(&v, -3);
			} else if (ev.btn == KT_MB_WHEEL_DOWN) {
				if (in_log) {
					v.log_scroll -= 3;
					if (v.log_scroll < 0)
						v.log_scroll = 0;
				} else {
					view_move(&v, 3);
				}
			} else if (ev.btn == KT_MB_LEFT && ev.press == KT_MP_PRESS &&
				   krect_hit(v.lay.tree, ev.mx, ev.my) &&
				   /* draw_tree reserves the tree's last column
				    * for the scrollbar (`width = tr.w - 1`
				    * there); without this a click on the
				    * scrollbar silently acted on whatever row
				    * happened to be under it. */
				   ev.mx < v.lay.tree.x + v.lay.tree.w - 1) {
				/* tree_geom is the SAME function draw_tree
				 * calls to decide whether a header is pinned
				 * and where the scrollable content starts —
				 * trap A: computing that offset a second time
				 * here drifted from the draw side, and every
				 * click selected the row above the one it
				 * visibly landed on whenever a header was up. */
				TreeGeom g = tree_geom(&v);
				if (g.header && ev.my == v.lay.tree.y) {
					/* Clicking the pinned row selects the
					 * group it stands in for — the useful
					 * behaviour, not a dead click. */
					v.auto_follow = 0;
					v.log_scroll = 0;
					v.selected = g.header;
				} else {
					int idx = v.scroll + (ev.my - g.content.y);
					if (idx >= 0 && idx < v.nvisible) {
						BStep *hit = v.visible[idx];
						/* node_icon's group marker is
						 * " [+]"/" [-]" — 4 columns starting
						 * at the row's own left edge (a group
						 * draws its icon at tr.x with no +2
						 * indent, unlike a step) — so that is
						 * the fold hit region, read off the
						 * draw rather than guessed. A second
						 * click on an already-selected group's
						 * marker folds it; anywhere else (or a
						 * fresh selection) just selects. */
						if (hit->is_group && hit == v.selected &&
						    ev.mx <= v.lay.tree.x + 3)
							hit->expanded = !hit->expanded;
						v.auto_follow = 0;
						v.log_scroll = 0;
						v.selected = hit;
					}
				}
			}
			/* Handled the same whether or not the search prompt is
			 * open: draw_search_bar keeps it on screen regardless,
			 * so there is no hidden-modal state to worry about, and
			 * this block already ran ahead of the v.searching gate
			 * below before it existed. */
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* The failure panel owns every key except Q while it is up —
		 * "must not trap the user" (see the panel's own comment)
		 * means Q has to keep falling through to the normal quit
		 * logic below rather than being swallowed here too. */
		if (m->error_step && m->error_step != v.err_dismissed &&
		    ev.key != 'q' && ev.key != 'Q') {
			switch (ev.key) {
			case 'r':
			case 'R':
				retry(&v);
				break;
			case 'o':
			case 'O':
				open_step_log(m, m->error_step);
				break;
			case 'c':
			case 'C': {
				char path[600];
				log_path_for(m, m->error_step, path, sizeof(path));
				int copied = ktui_clip_copy(path);
				/* Tell the truth: OSC 52 is a no-op on the
				 * Linux VT (KT_CAP_LINUXVT), so a flat "copied"
				 * message would lie there every time. */
				mgr_notice(m, copied
						   ? "log path copied to the "
						     "clipboard"
						   : "clipboard copy not "
						     "supported here — the "
						     "path is shown above");
				break;
			}
			case KT_K_ESC:
			case KT_K_ENTER:
			case '\n':
				/* Hides the panel WITHOUT clearing
				 * m->error_step: the failed step stays
				 * selected underneath, exactly as it was
				 * before the panel existed. */
				v.err_dismissed = m->error_step;
				break;
			default:
				break;
			}
			continue;
		}

		/* Same contract as the failure panel: it owns Esc/Enter while
		 * up, and Q keeps falling through to the quit logic so the
		 * panel can never trap the user. Everything else is left alone
		 * — the tree and the log are still worth browsing after a
		 * successful build, and dismissing is one keystroke away. */
		if (build_completed_ok(m) && !v.done_dismissed &&
		    (ev.key == KT_K_ESC || ev.key == KT_K_ENTER ||
		     ev.key == '\n')) {
			v.done_dismissed = 1;
			continue;
		}

		/* While the prompt is open it owns every printable key, the
		 * same convention the package picker's filter uses. */
		if (v.searching) {
			if (ev.key == KT_K_ENTER || ev.key == '\n') {
				v.searching = 0;
			} else if (ev.key == KT_K_ESC) {
				v.searching = 0;
				v.search[0] = 0;
			} else if (ev.key == KT_K_BACKSPACE) {
				size_t n = strlen(v.search);
				if (n)
					v.search[n - 1] = 0;
			} else if (ev.key >= 32 && ev.key < 127 &&
			    !(ev.mods & (KT_MOD_CTRL | KT_MOD_ALT))) {
				size_t n = strlen(v.search);
				if (n < sizeof(v.search) - 1) {
					v.search[n] = (char)ev.key;
					v.search[n + 1] = 0;
				}
			}
			search_refresh(&v);
			continue;
		}

		switch (ev.key) {
		case 'q':
		case 'Q':
			if (!m->is_running) {
				v.quit_requested = 1;
			} else if (m->stop_requested) {
				/* Asked twice: kill every running step's
				 * process group outright and leave. A build
				 * that cannot be quit is worse than a build
				 * that is killed untidily. */
				m->force_quit = 1;
				v.quit_requested = 1;
				mgr_notice(m, "force quit — killing the "
					   "running steps");
			} else {
				/* Once: SIGTERM to every running step's
				 * process group, SIGKILL five seconds on. */
				m->stop_requested = 1;
				mgr_notice(m, "stop requested — terminating the "
					   "running steps (Q again to force)");
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
				mgr_notice(m, "snapshot of %s queued for when "
					   "no step is running", g->title);
			}
			break;
		}
		case 't':
		case 'T': {
			/* Local rather than file-scope: nothing outside this
			 * one keybinding needs to see or reset which theme is
			 * next, and a static here still persists across frames
			 * the same way a global would. */
			static int which;
			which = (which + 1) % ktui_ntheme;
			ktui_theme_set(ktui_themes[which].name);
			/* The palette is installed into the terminal, not just
			 * read out of a struct — repalette without invalidate
			 * leaves every cell that isn't touched this frame still
			 * carrying the old colours until something else forces
			 * it to redraw. */
			ktui_term_repalette();
			ktui_draw_invalidate();
			mgr_notice(m, "theme: %s", ktui_themes[which].label);
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
		case '/':
			v.searching = 1;
			v.auto_follow = 0;
			break;
		case 'n':
		case 'N': {
			if (!v.nhit || !v.selected)
				break;
			v.hit_at += ev.key == 'n' ? 1 : -1;
			if (v.hit_at >= v.nhit)
				v.hit_at = 0;
			if (v.hit_at < 0)
				v.hit_at = v.nhit - 1;
			v.auto_follow = 0;
			v.log_scroll = v.selected->nlog - 1 - v.hit[v.hit_at];
			if (v.log_scroll < 0)
				v.log_scroll = 0;
			break;
		}
		case 'e':
		case 'E': {
			BStep *n = v.selected;
			if (!n)
				break;
			for (int i = 0; i < n->nlog; i++)
				if (log_severity(n->log[i]) == LOG_ERR) {
					v.auto_follow = 0;
					v.log_scroll = n->nlog - 1 - i;
					break;
				}
			break;
		}
		case 'o':
		case 'O':
			open_step_log(m, v.selected);
			break;
		case 'r':
		case 'R':
			if (m->error_step)
				retry(&v);
			break;
		default:
			break;
		}
	}

	sam_stop(sam);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* --preview
 *
 * Lives here rather than beside preview_main because the view structs above
 * are private to this file, and they should stay that way: a screen's state is
 * this file's business, and the preview is a second CALLER of the drawing
 * half, not a second implementation of it.
 */

static void preview_build(Manager *m, Sampler *sam, int activity,
			  int show_failure)
{
	BuildView v = {0};
	v.m = m;
	v.sam = sam;
	/* Following would park the selection on the RUNNING step; the failed
	 * one is what a person is looking at when they need this screen to be
	 * right, and it is the one carrying a log worth classifying. */
	v.auto_follow = 0;
	v.selected = m->error_step;
	kb_strlcpy(v.search, "error", sizeof(v.search));
	m->snap.active = activity;
	/* The fixture's error_step is always set, so the ordinary "build" and
	 * "activity" renders mark it already dismissed -- exactly the state
	 * the real screen is in after Esc -- and only "failure" (dispatched
	 * from preview_screen) leaves it unset, which is what actually
	 * exercises draw_failure_panel. One draw path, two renders of it. */
	v.err_dismissed = show_failure ? NULL : m->error_step;
	/* The panel as a failure leaves it: the run stopped, nothing running,
	 * so [R] is offered. */
	if (show_failure) {
		m->is_running = 0;
		m->nrunning = 0;
		m->end_time = kb_now_s();
	}
	/* view_update's OWN scroll math reads v.lay, which only draw_build_frame
	 * sets -- so a single pass here would run against the still-zeroed
	 * layout BuildView starts with (see view_update's comment on that).
	 * The real screen self-corrects on its very next pump; an offscreen
	 * render has no "next frame", so it is given one explicitly: once to
	 * populate v.lay for real, once more so the scroll settles exactly
	 * where the live screen would settle after its first pump. */
	view_update(&v);
	draw_build_frame(&v);
	view_update(&v);
	draw_build_frame(&v);
}

/* Selects the LAST step of the image phase -- extended in the fixture
 * past the height of any tree pane this file lays out (Feature 1 has
 * nothing to pin without a scroll that outruns the tallest pane) -- and lets
 * view_update's own scroll-margin math push the phase's own row off screen,
 * exactly the way a real build reaching that step would. The scroll comes
 * out of the real code path, not a value poked in here. */
static void preview_pinned(Manager *m, Sampler *sam)
{
	BuildView v = {0};
	v.m = m;
	v.sam = sam;
	v.auto_follow = 0;
	BStep *last_phase = m->root[m->nroot - 1];
	v.selected = last_phase->child[last_phase->nchild - 1];
	v.err_dismissed = m->error_step;	/* keep the failure panel off this render */
	view_update(&v);
	draw_build_frame(&v);
	view_update(&v);
	draw_build_frame(&v);
}

/* preview_fixture only stands up the 3 phases the BUILD screen needs a tree
 * for. The startup picker needs all 13 real phases instead -- that is the
 * whole point of this fixture, matching preview_snapshots' 13-entry table row
 * for row -- so it overwrites m->phase itself. Safe because preview_screen()
 * renders exactly one screen per process: nothing downstream of this call
 * still depends on the 3-phase build-screen shape. */
static void preview_startup(Manager *m)
{
	static const struct {
		const char *dir, *title;
	} PH[] = {
		{ "00_cross", "Cross Toolchain" },
		{ "10_bootstrap", "Base Userland" },
		{ "20_selfhost", "Self-Hosting Bootstrap" },
		{ "30_foundation", "Build Foundation" },
		{ "31_compilers", "Compilers" },
		{ "40_lang", "Languages" },
		{ "41_system", "System" },
		{ "42_graphics", "Graphics Stack" },
		{ "43_toolkits", "Toolkits" },
		{ "44_apps", "Applications" },
		{ "50_desktop", "Desktop" },
		{ "60_kernel", "Kernel" },
		{ "70_image", "Image" },
	};
	int nph = (int)(sizeof(PH) / sizeof(PH[0]));
	for (int i = 0; i < nph; i++) {
		m->phase[i].index = i;
		kb_strlcpy(m->phase[i].dir_name, PH[i].dir,
			   sizeof(m->phase[i].dir_name));
		kb_strlcpy(m->phase[i].title, PH[i].title,
			   sizeof(m->phase[i].title));
	}
	m->nphase = nph;

	KbuildSnapshot *snaps = kb_calloc(KBUILD_MAX_PHASES, sizeof(*snaps));
	int nsnap = preview_snapshots(m, snaps, KBUILD_MAX_PHASES);

	int row_phase[KBUILD_MAX_PHASES + 1];
	int nrow = 1;
	row_phase[0] = -1;
	for (int i = 0; i < m->nphase; i++)
		if (kbuild_snap_find(snaps, nsnap, m->phase[i].dir_name))
			row_phase[nrow++] = i;

	StartupView vw = { m, snaps, nsnap, snaps, nsnap, row_phase, nrow,
			   nrow > 1 ? nrow - 1 : 0, "9f3a1c2", "", 0, 1 };
	draw_startup_frame(&vw);
	free(snaps);
}

/* The plan picker's own data comes off disk (kbuild_steps, the package index);
 * here it is spelled out, so the preview never depends on the tree it is run
 * from and renders the same at every size. */
static void preview_plan(Manager *m)
{
	static char *steps0[] = { "000_file_system.sh", "020_musl_libc.sh",
				  "062_coreutils.sh" };
	static char *steps2[] = { "030_launchers.sh", "050_theme.sh",
				  "090_initramfs.sh", "110_iso.sh" };
	char **steps[KBUILD_MAX_PHASES] = { steps0, NULL, steps2 };
	int nsteps[KBUILD_MAX_PHASES] = { 3, 0, 4 };
	int phase_on[KBUILD_MAX_PHASES] = { 1, 1, 1 };
	int expanded[KBUILD_MAX_PHASES] = { 0, 0, 1 };
	int step_on[KBUILD_MAX_PHASES][KBUILD_MAX_STEPS];
	memset(step_on, 0, sizeof(step_on));
	step_on[0][0] = step_on[0][2] = 1;	/* a partial phase: "[~]" */
	step_on[2][0] = step_on[2][1] = step_on[2][3] = 1;

	static KbuildPkgRef pkg[] = {
		{ "gst-plugins-base", "42_graphics" },
		{ "gst-plugins-good", "42_graphics" },
		{ "mesa", "42_graphics" },
	};
	static char rebuild[2][64] = { "gst-plugins-base", "mesa" };

	PlanRow row[KBUILD_MAX_PHASES * (KBUILD_MAX_STEPS + 1)];
	int nrow = 0;
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

	PlanView vw = { m, phase_on, steps, nsteps, step_on, pkg, 3, rebuild, 2,
			row, nrow, 1, 0, "nothing selected — pick at least one "
			"phase" };
	draw_plan_frame(&vw);
}

static void preview_packages(void)
{
	static KbuildPkgRef pkg[] = {
		{ "gst-plugins-base", "42_graphics" },
		{ "gst-plugins-good", "42_graphics" },
		{ "gst-plugins-bad", "42_graphics" },
		{ "gst-plugins-ugly", "42_graphics" },
		{ "mesa", "42_graphics" },
		{ "libunwind", "" },
	};
	static char chosen[KBUILD_MAX_REBUILD][64] = { "gst-plugins-base",
						       "gst-plugins-bad" };
	int nchosen = 2;
	PickState st = { pkg, 6, chosen, &nchosen };

	int match[6];
	int nmatch = 0;
	for (int i = 0; i < 6; i++)
		if (strstr(pkg[i].name, "gst"))
			match[nmatch++] = i;

	PackView vw = { &st, match, nmatch, 1, 0, "gst" };
	draw_packages_frame(&vw);
}

/* The fixture is built around a FAILED run, because that is the state most of
 * the screens want to show. A completed run differs from it in a handful of
 * fields, so this flips those rather than carrying a second fixture that
 * would drift out of step with the first. */
static void preview_complete(Manager *m, Sampler *sam)
{
	m->error_step = NULL;
	m->stop_requested = 0;
	m->is_running = 0;
	/* A two-hour run that has just stopped: mgr_pump() stamps end_time on
	 * the turn a run stops, and the elapsed time reads up to it. */
	m->start_time = kb_now_s() - 7384;
	m->end_time = kb_now_s();
	for (int i = 0; i < m->nroot; i++)
		m->root[i]->status = ST_DONE;
	for (int i = 0; i < m->norder; i++)
		if (!m->order[i]->is_group && m->order[i]->status != ST_SKIPPED)
			m->order[i]->status = ST_DONE;

	BuildView v = {0};
	v.m = m;
	v.sam = sam;
	v.auto_follow = 0;
	v.selected = m->norder ? m->order[m->norder - 1] : NULL;
	/* Twice, for the same reason preview_build renders twice: view_update's
	 * scroll math reads v.lay, which only draw_build_frame sets. */
	view_update(&v);
	draw_build_frame(&v);
	view_update(&v);
	draw_build_frame(&v);
}

int preview_screen(const char *screen)
{
	static Manager m;
	static Sampler sam;
	preview_fixture(&m, &sam);

	if (!strcmp(screen, "build"))
		preview_build(&m, &sam, 0, 0);
	else if (!strcmp(screen, "activity"))
		preview_build(&m, &sam, 1, 0);
	else if (!strcmp(screen, "failure"))
		preview_build(&m, &sam, 0, 1);
	else if (!strcmp(screen, "pinned"))
		preview_pinned(&m, &sam);
	else if (!strcmp(screen, "complete"))
		preview_complete(&m, &sam);
	else if (!strcmp(screen, "startup"))
		preview_startup(&m);
	else if (!strcmp(screen, "plan"))
		preview_plan(&m);
	else if (!strcmp(screen, "packages"))
		preview_packages();
	else
		return 1;
	return 0;
}
