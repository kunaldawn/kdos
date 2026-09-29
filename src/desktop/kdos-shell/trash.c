/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-trash — what was deleted, and the way back
 *
 *   ╔═ Trash ══════════════════════════════════════════════╗
 *   ║ Name              Size  Deleted ↓   From             ║
 *   ║──────────────────────────────────────────────────────║
 *   ║ notes.txt         1.2K  2026-09-05  ~/Documents      ║
 *   ║ old-build       folder  2026-09-04  ~/src            ║
 *   ╟──────────────────────────────────────────────────────╢
 *   ║ Enter put back  d delete  c empty  Esc Close         ║
 *   ╚══════════════════════════════════════════════════════╝
 *
 * PUT BACK IS THE POINT. Trash without it is a slower delete: the desktop
 * already moves a file here and `kdos trash` already lists it, but the way
 * BACK was a command line and a name nobody had written down. Enter on a row
 * is that way back, and it is why this surface exists rather than a menu entry
 * that runs `kdos trash --list` in a terminal.
 *
 * IT CALLS `kb_trash_*` AND NOTHING ELSE. The trash specification is one
 * implementation in libkbase — the escaping, the `.trashinfo` record, the
 * unique-name walk, the refusal to overwrite what is already back at the
 * origin — and a surface that reimplemented any of it would be a second
 * answer to where a deleted file lives.
 *
 * A DESTRUCTIVE ROW ASKS FIRST, and the question is a declared Esc rung rather
 * than a flag: Escape while it is up answers "no" and leaves the list exactly
 * as it was, which is what Escape means everywhere else on this desktop.
 *
 * THE LIST IS A ktui_table, so its columns sort from their titles (or `s`),
 * the name column's edge drags, and a long trash gets the table's scrollbar.
 * The table sorts an index array and never the records: `items` stays in
 * the canonical order reload() gives it, which is what breaks every tie.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define TR_COLS 68
#define TR_ROWS 20

static KbTrashItem *items;
static int *order;		/* row i draws items[order[i]]             */
static int nitems;
static char note[160];

/*
 * Newest first to begin with, because that is the canonical order and the
 * header should say so. The selection rule on the surface's own slot, not
 * the accent fill: the secondary columns lift to KT_TEXT on the selected row
 * (ktui_sel_dim) where the accent fill would leave them muted.
 */
enum { COL_NAME, COL_SIZE, COL_WHEN, COL_FROM, NCOL };
static KtuiTable tbl = {
	.sort = COL_WHEN + 1,
	.desc = 1,
	.page = KT_SURFACE,
	.selrule = 1,
	.inset = 1,
};

/*
 * The question, and what answering yes does. One rung, because only one can be
 * up: a confirm that could stack would be two questions about the same list
 * with no way to tell which is being answered.
 */
enum { ASK_NONE = 0, ASK_DELETE, ASK_EMPTY };
static int asking;

static KtuiKeys keys;

static int ask_up(void *user)
{
	(void)user;
	return asking != ASK_NONE;
}

static void ask_cancel(void *user)
{
	(void)user;
	asking = ASK_NONE;
}

/*
 * Newest first, and by name within a second — `kdos trash --list` sorts the
 * same way and for the same reason. `kb_trash_list` returns READDIR ORDER, so
 * a surface that did not sort would draw a different list on every machine and
 * a golden of it would assert the filesystem.
 */
static int cmp_when(const void *a, const void *b)
{
	const KbTrashItem *x = a, *y = b;
	int c = strcmp(y->when, x->when);

	return c ? c : strcmp(x->name, y->name);
}

/*
 * One column, rising; the table turns it round and breaks ties by index,
 * which is the canonical order above. A folder has no size worth comparing
 * (see the size column), so folders sort together: ahead of every file
 * rising, after every file falling.
 */
static int cmp_col(int a, int b, int col, void *user)
{
	const KbTrashItem *x = &items[a], *y = &items[b];

	(void)user;
	switch (col) {
	case COL_NAME:
		return strcmp(x->name, y->name);
	case COL_SIZE:
		if (x->isdir != y->isdir)
			return x->isdir ? -1 : 1;
		if (x->isdir)
			return 0;
		return (x->bytes > y->bytes) - (x->bytes < y->bytes);
	case COL_WHEN:
		return strcmp(x->when, y->when);
	default:
		return strcmp(x->orig, y->orig);
	}
}

static void resort(void)
{
	ktui_table_sort(&tbl, order, nitems, cmp_col, NULL);
}

static void reload(void)
{
	free(items);
	free(order);
	items = NULL;
	order = NULL;
	nitems = kb_trash_list(&items);
	if (nitems < 0)
		nitems = 0;
	if (nitems > 1)
		qsort(items, (size_t)nitems, sizeof(items[0]), cmp_when);
	if (nitems)
		order = malloc((size_t)nitems * sizeof(*order));
	if (!order)
		nitems = 0;
	for (int i = 0; i < nitems; i++)
		order[i] = i;
	/* A reload renumbers the records, so the selection stays on its ROW
	 * rather than following a record index that now names another file —
	 * after a delete, the row under the one that went. */
	int row = tbl.sel;

	tbl.sel = -1;
	resort();
	tbl.sel = row;
	if (tbl.sel >= nitems)
		tbl.sel = nitems ? nitems - 1 : 0;
	if (tbl.sel < 0)
		tbl.sel = 0;
}

/* The record under the selection, or NULL on an empty trash. */
static const KbTrashItem *picked(void)
{
	return tbl.sel >= 0 && tbl.sel < nitems ? &items[order[tbl.sel]] : NULL;
}

/*
 * `~` for the home directory, because the origin column is the one a person
 * reads to tell two files of the same name apart and a full path pushes the
 * part that differs off the row.
 */
static const char *pretty_orig(const char *path, char *buf, size_t n)
{
	const char *home = kb_home_dir();
	size_t hl = home ? strlen(home) : 0;

	if (!path || !*path)
		return "(no record)";
	if (hl && !strncmp(path, home, hl) && (path[hl] == '/' || !path[hl])) {
		snprintf(buf, n, "~%s", path + hl);
		return buf;
	}
	return path;
}

/*
 * The columns at this width. The name takes a third, as it always has, and
 * the origin the remainder: the origin is the column a person reads to tell
 * two files of the same name apart. Rebuilt every draw and every pointer
 * event from the same width, so a press is measured against what is shown.
 */
static KtuiCol cols[NCOL];

static void make_cols(int w)
{
	cols[COL_NAME] = (KtuiCol){ "Name", (w - 4) / 3,
				    KT_COL_SORT | KT_COL_RESIZE };
	cols[COL_SIZE] = (KtuiCol){ "Size", 6, KT_COL_SORT | KT_COL_RIGHT };
	cols[COL_WHEN] = (KtuiCol){ "Deleted", 10, KT_COL_SORT };
	cols[COL_FROM] = (KtuiCol){ "From", 0, KT_COL_SORT };
}

/* The table's rect: inside the frame, above the rule over the hint row. */
static KRect list_rect(void)
{
	return krect(1, 1, ktui_w - 2, ktui_h - 4 > 3 ? ktui_h - 4 : 3);
}

static int list_rows(void)
{
	return list_rect().h - 2;	/* under the titles and their rule */
}

static void tr_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const KbTrashItem *it = &items[order[idx]];
	int dim = ktui_sel_dim(idx == tbl.sel, 1);
	char buf[512];

	(void)user;
	switch (col) {
	case COL_NAME:
		ktui_table_text(&cols[col], x, y, w, it->name, fg, bg);
		break;
	case COL_SIZE:
		/* A directory's `bytes` is its inode, not a recursive total,
		 * so it is named rather than measured — a folder reported as
		 * 4K is a number that is wrong rather than missing. */
		ktui_table_text(&cols[col], x, y, w,
				it->isdir ? "folder" : kb_human_size(it->bytes),
				dim, bg);
		break;
	case COL_WHEN:
		/* TEN, which is the date and not the `T`: the record's
		 * timestamp is ISO, and the time of day is noise in a list
		 * sorted by it. */
		ktui_table_text(&cols[col], x, y, w < 10 ? w : 10, it->when,
				dim, bg);
		break;
	default:
		/* One cell short of the frame, so a long path stops before
		 * the border rather than against it. */
		ktui_table_text(&cols[col], x, y, w - 1,
				pretty_orig(it->orig, buf, sizeof(buf)), dim,
				bg);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	if (w < 24 || h < 8)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "Trash", KT_ACCENT, KT_SURFACE, 1);

	if (!nitems) {
		/* The empty state in the middle, where somebody is already
		 * looking, rather than a status line saying nothing is here. */
		const char *msg = "Nothing has been deleted";

		ktui_draw_text((w - (int)strlen(msg)) / 2, h / 2 - 1,
			       w - 2, msg, KT_MID, KT_SURFACE, KT_A_NONE);
	} else {
		make_cols(w);
		ktui_table_draw(list_rect(), &tbl, nitems, cols, NCOL, tr_cell,
				NULL, NULL, -1);
	}

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	if (asking) {
		/* The question owns the row while it is up; the pool is still
		 * drained, because the row is what clears it. */
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_SURFACE);
		if (asking == ASK_EMPTY)
			ktui_draw_textf(2, h - 2, w - 4, KT_WARN, KT_SURFACE,
					KT_A_NONE,
					"Delete all %d for good?  y/n", nitems);
		else if (picked())
			ktui_draw_textf(2, h - 2, w - 4, KT_WARN, KT_SURFACE,
					KT_A_NONE,
					"Delete %.40s for good?  y/n",
					picked()->name);
	} else if (note[0]) {
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_SURFACE);
		ktui_draw_text(2, h - 2, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	} else {
		ktui_hint_if(nitems > 0, "Enter", "put back");
		ktui_hint_if(nitems > 0, "d", "delete");
		ktui_hint_if(nitems > 0, "c", "empty");
		ktui_hint("Esc", ktui_esc_verb(&keys));
		/* After Esc, because the row stops at the first hint that does
		 * not fit: on a narrow window the sort key, which the titles
		 * also answer, is the one to lose. */
		ktui_hint_if(nitems > 1, "s", "sort");
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	}
}

/*
 * The three failures `kb_trash_restore` distinguishes, in the words the
 * command line already uses: a record that cannot be parsed and a file that is
 * already back are different problems and a person can act on the difference.
 */
static void put_back(void)
{
	char to[KB_TRASH_PATH];
	const KbTrashItem *it = picked();

	if (!it)
		return;
	if (kb_trash_restore(it->name, to, sizeof(to)) == 0) {
		char pretty[512];

		snprintf(note, sizeof(note), "put back to %.120s",
			 pretty_orig(to, pretty, sizeof(pretty)));
		reload();
		return;
	}
	switch (errno) {
	case ENOENT:
		snprintf(note, sizeof(note),
			 "%.40s has no record to restore it by",
			 it->name);
		break;
	case EEXIST:
		snprintf(note, sizeof(note),
			 "something already exists where %.40s came from",
			 it->name);
		break;
	default:
		snprintf(note, sizeof(note), "cannot put back: %s",
			 strerror(errno));
		break;
	}
}

/*
 * THE POINTER WALKS THE LIST AND PUTS A ROW BACK, by the rule every table
 * here keeps: a press moves the caret, a press on the row it is already on
 * picks, the wheel walks and the right button is Back. A press on a title
 * sorts, and the name column's edge drags — motion and release come here too.
 *
 * NOT WHILE A QUESTION IS UP. The confirm owns the keyboard for a reason — a
 * list that scrolled or re-sorted under it would answer it about a different
 * row — and a pointer that could move it is the same defect through the other
 * hand.
 */
static int on_mouse(KtuiEvent *ev)
{
	if (asking || !nitems)
		return SH_EV_PASS;
	make_cols(ktui_w);
	switch (ktui_table_event(list_rect(), &tbl, nitems, list_rows(), NCOL,
				 cols, ev, NULL, NULL)) {
	case KTUI_TABLE_PICKED:
		put_back();
		break;
	case KTUI_TABLE_CLOSE:
		return SH_EV_CLOSE;
	case KTUI_TABLE_SORT:
		resort();
		break;
	default:
		break;
	}
	return SH_EV_TAKEN;
}

/* The question owns the keyboard while it is up: a list that scrolled under an
 * unanswered confirm would answer it about a different row. */
static void answer(int k)
{
	if (k == 'y' || k == 'Y') {
		if (asking == ASK_EMPTY) {
			int n = kb_trash_empty();

			snprintf(note, sizeof(note),
				 n < 0 ? "could not empty the trash"
				       : "emptied %d", n);
		} else if (picked()) {
			if (kb_trash_remove(picked()->name) != 0)
				snprintf(note, sizeof(note),
					 "cannot delete: %s", strerror(errno));
			else
				note[0] = '\0';
		}
		asking = ASK_NONE;
		reload();
	} else if (k == 'n' || k == 'N') {
		asking = ASK_NONE;
	}
}

static int on_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_MOUSE)
		return on_mouse(ev);
	if (ev->type != KT_EVT_KEY)
		return SH_EV_PASS;
	if (asking) {
		answer(ev->key);
		return SH_EV_TAKEN;
	}

	note[0] = '\0';
	if (ktui_table_key(&tbl, nitems, list_rows(), ev->key, NULL, NULL))
		return SH_EV_TAKEN;
	switch (ev->key) {
	case 's':
		/* A chord arrives as its letter with KT_MOD_CTRL. */
		if (ev->mods & KT_MOD_CTRL)
			break;
		make_cols(ktui_w);
		if (ktui_table_sort_next(&tbl, cols, NCOL))
			resort();
		break;
	case KT_K_ENTER:
		put_back();
		break;
	case 'd':
	case KT_K_DEL:
		if (nitems)
			asking = ASK_DELETE;
		break;
	case 'c':
		if (nitems)
			asking = ASK_EMPTY;
		break;
	case 'r':
		reload();
		break;
	default:
		break;
	}
	return SH_EV_TAKEN;
}

static void tr_start(int dump)
{
	(void)dump;
	ktui_keys_layer(&keys, "Cancel", ask_up, ask_cancel, NULL);
	reload();
}

int trash_main(int argc, char **argv)
{
	static const ShSurface s = {
		.cfg = {
			.role = KDISP_ROLE_OVERLAY,
			.cols = TR_COLS,
			.rows = TR_ROWS,
			.app_id = "kdos-trash",
			.keyboard = 1,
		},
		.keys = &keys,
		.popup = 1,
		.popup_bg = KT_SURFACE,
		.start = tr_start,
		.draw = draw,
		.event = on_event,
	};
	int r = sh_run(&s, argc, argv);

	free(items);
	free(order);
	return r;
}
