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
 *   ║ notes.txt              1.2K  2026-09-05  ~/Documents ║
 *   ║ old-build              4.0K  2026-09-04  ~/src       ║
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
static int nitems, sel, top;
static char note[160];

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

static void reload(void)
{
	free(items);
	items = NULL;
	nitems = kb_trash_list(&items);
	if (nitems < 0)
		nitems = 0;
	if (nitems > 1)
		qsort(items, (size_t)nitems, sizeof(items[0]), cmp_when);
	if (sel >= nitems)
		sel = nitems ? nitems - 1 : 0;
	if (sel < 0)
		sel = 0;
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

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int rows = h - 4;

	if (w < 24 || h < 8)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "Trash", KT_ACCENT, KT_SURFACE, 1);

	if (sel < top)
		top = sel;
	if (sel >= top + rows)
		top = sel - rows + 1;

	if (!nitems) {
		/* The empty state in the middle, where somebody is already
		 * looking, rather than a status line saying nothing is here. */
		const char *msg = "Nothing has been deleted";

		ktui_draw_text((w - (int)strlen(msg)) / 2, h / 2 - 1,
			       w - 2, msg, KT_MID, KT_SURFACE, KT_A_NONE);
	}

	for (int i = 0; i < rows && top + i < nitems; i++) {
		const KbTrashItem *it = &items[top + i];
		int y = 1 + i, on = top + i == sel;
		int bg = on ? KT_ACCENT : KT_SURFACE;
		int fg = on ? KT_SURFACE : KT_TEXT;
		char buf[512];
		const char *orig = pretty_orig(it->orig, buf, sizeof(buf));

		ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		ktui_draw_text(2, y, (w - 4) / 3, it->name, fg, bg, KT_A_NONE);
		/* A directory's `bytes` is its inode, not a recursive total, so
		 * it is named rather than measured — a folder reported as 4K is
		 * a number that is wrong rather than missing. */
		ktui_draw_text(2 + (w - 4) / 3, y, 10,
			       it->isdir ? "folder" : kb_human_size(it->bytes),
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
		/* TEN, which is the date and not the `T`: the record's
		 * timestamp is ISO and a column that stopped one character
		 * later would end every row on a separator. */
		ktui_draw_text(12 + (w - 4) / 3, y, 10, it->when,
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
		ktui_draw_text(24 + (w - 4) / 3, y,
			       w - 26 - (w - 4) / 3, orig,
			       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
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
		else if (sel < nitems)
			ktui_draw_textf(2, h - 2, w - 4, KT_WARN, KT_SURFACE,
					KT_A_NONE,
					"Delete %.40s for good?  y/n",
					items[sel].name);
	} else if (note[0]) {
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_SURFACE);
		ktui_draw_text(2, h - 2, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	} else {
		ktui_hint_if(nitems > 0, "Enter", "put back");
		ktui_hint_if(nitems > 0, "d", "delete");
		ktui_hint_if(nitems > 0, "c", "empty");
		ktui_hint("Esc", ktui_esc_verb(&keys));
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	}
	ktui_draw_flush();
}

/*
 * The three failures `kb_trash_restore` distinguishes, in the words the
 * command line already uses: a record that cannot be parsed and a file that is
 * already back are different problems and a person can act on the difference.
 */
static void put_back(void)
{
	char to[KB_TRASH_PATH];

	if (sel >= nitems)
		return;
	if (kb_trash_restore(items[sel].name, to, sizeof(to)) == 0) {
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
			 items[sel].name);
		break;
	case EEXIST:
		snprintf(note, sizeof(note),
			 "something already exists where %.40s came from",
			 items[sel].name);
		break;
	default:
		snprintf(note, sizeof(note), "cannot put back: %s",
			 strerror(errno));
		break;
	}
}

int trash_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else {
			fprintf(stderr, "usage: kdos-trash [--font NAME] "
					"[--dump]\n");
			return 2;
		}
	}

	ktui_keys_layer(&keys, "Cancel", ask_up, ask_cancel, NULL);
	reload();

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = TR_COLS,
		.rows = TR_ROWS,
		.app_id = "kdos-trash",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(TR_COLS, TR_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-trash: no display server\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		draw();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				goto done;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		/* The question owns the keyboard while it is up: a list that
		 * scrolled under an unanswered confirm would answer it about a
		 * different row. */
		if (asking) {
			if (ev.key == 'y' || ev.key == 'Y') {
				if (asking == ASK_EMPTY) {
					int n = kb_trash_empty();

					snprintf(note, sizeof(note),
						 n < 0 ? "could not empty the trash"
						       : "emptied %d", n);
				} else if (sel < nitems) {
					if (kb_trash_remove(items[sel].name) != 0)
						snprintf(note, sizeof(note),
							 "cannot delete: %s",
							 strerror(errno));
					else
						note[0] = '\0';
				}
				asking = ASK_NONE;
				reload();
			} else if (ev.key == 'n' || ev.key == 'N') {
				asking = ASK_NONE;
			}
			continue;
		}

		note[0] = '\0';
		switch (ev.key) {
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nitems)
				sel++;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
			sel = nitems ? nitems - 1 : 0;
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
	}
done:
	free(items);
	kdisp_shutdown();
	return 0;
}
