/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-note — the scratch pad, Sidekick's second accessory
 *
 * ONE BUFFER PER USER, at ~/.local/share/kdos/scratch.txt. Not a file chooser,
 * not tabs, not a document model: a place to put the line you will need in a
 * minute, summoned over whatever you are working in and dismissed leaving it
 * untouched.
 *
 * IT IS NOT AN EDITOR AND MUST NOT GROW INTO ONE. `micro` is the editor and is
 * two keys away — Ctrl+O opens this same file in it — and every feature this
 * surface grows past "type a line and find it later" is a feature that already
 * exists there and is better done there.
 *
 * SAVED ON EVERY CLOSE AND EVERY THIRTY SECONDS, temp-and-rename like every
 * other state file on this desktop. A scratch pad that lost what was in it
 * when the session ended would be a scratch pad nobody trusted with anything
 * worth scratching.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define NOTE_COLS 60
#define NOTE_ROWS 20
#define NOTE_LINES 200
#define NOTE_COLS_MAX 240

static char buf[NOTE_LINES][NOTE_COLS_MAX];
static int nlines = 1;
static int cy, cx;		/* the caret, in lines and columns */
static int top;			/* the first line drawn */
static int changed;

/* No layers: Esc closes the pad, and the pool answers that with CLOSE. */
static KtuiKeys keys;

static int note_path(char *out, size_t n)
{
	const char *data = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");

	if (data && *data)
		return snprintf(out, n, "%s/kdos/scratch.txt", data) < (int)n;
	if (home && *home)
		return snprintf(out, n, "%s/.local/share/kdos/scratch.txt",
				home) < (int)n;
	return 0;
}

static void note_load(void)
{
	char path[512];
	FILE *f;

	nlines = 1;
	buf[0][0] = '\0';
	if (!note_path(path, sizeof(path)))
		return;
	f = fopen(path, "r");
	if (!f)
		return;		/* no pad yet is not an error */
	while (nlines <= NOTE_LINES &&
	       fgets(buf[nlines - 1], NOTE_COLS_MAX, f)) {
		buf[nlines - 1][strcspn(buf[nlines - 1], "\r\n")] = '\0';
		nlines++;
	}
	if (nlines > 1)
		nlines--;
	fclose(f);
}

static void note_save(void)
{
	char path[512], tmp[544], dir[512];
	FILE *f;

	if (!changed || !note_path(path, sizeof(path)))
		return;

	/* The directory may not exist on a fresh account. */
	snprintf(dir, sizeof(dir), "%s", path);

	char *slash = strrchr(dir, '/');

	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return;
	for (int i = 0; i < nlines; i++)
		fprintf(f, "%s\n", buf[i]);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0)
		unlink(tmp);
	else
		changed = 0;
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int rows = h - 3;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "Notes", KT_ACCENT, KT_SURFACE, 1);

	if (cy < top)
		top = cy;
	if (cy >= top + rows)
		top = cy - rows + 1;

	for (int i = 0; i < rows && top + i < nlines; i++)
		ktui_draw_text(2, 1 + i, w - 4, buf[top + i], KT_TEXT,
			       KT_SURFACE, KT_A_NONE);

	char stat[32];
	int sw;

	/*
	 * THE COUNTER IS STATE AND THE KEYS ARE HINTS, and only the second
	 * half is the row's. The row FILLS its rect, so it starts to the right
	 * of the counter rather than over it.
	 */
	snprintf(stat, sizeof(stat), "line %d/%d%s", cy + 1, nlines,
		 changed ? " *" : "");
	ktui_draw_text(2, h - 2, w - 4, stat, KT_MID, KT_SURFACE, KT_A_NONE);
	sw = (int)strlen(stat) + 3;
	ktui_hint("Ctrl+O", "edit");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2 + sw, h - 2, w - 4 - sw, 1), KT_SURFACE);
	ktui_term_caret(2 + cx, 1 + (cy - top));
}

int note_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else {
			fprintf(stderr, "usage: kdos-note [--font NAME] "
					"[--dump]\n");
			return 2;
		}
	}

	note_load();

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = NOTE_COLS,
		.rows = NOTE_ROWS,
		.app_id = "kdos-note",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(NOTE_COLS, NOTE_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-note: no display server\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);

	time_t last_save = time(NULL);

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			/* Every thirty seconds, so a session that ends badly
			 * loses at most half a minute of scratch. */
			if (time(NULL) - last_save >= 30) {
				last_save = time(NULL);
				note_save();
			}
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* FIRST, above this surface's own switch. */
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			goto done;

		int len = (int)strlen(buf[cy]);

		if (cx > len)
			cx = len;

		switch (ev.key) {
		case 0x0f:		/* Ctrl+O */
			/*
			 * THE EDITOR, ON THE SAME FILE. This surface is a
			 * scratch pad and stops where an editor starts; the
			 * pad is saved first so the editor opens what is on
			 * the screen rather than what was there last time.
			 */
			note_save();
			{
				char path[512];
				char id[64];
				const char *av[10];
				int n = 0;

				if (note_path(path, sizeof(path))) {
					/* The terminal follows the desktop —
					 * sh_term_argv() is the one place that
					 * decides which one and what identity
					 * it wears. */
					n = sh_term_argv(av, 0, 8, "micro", id,
							 sizeof(id));
					av[n++] = "micro";
					av[n++] = path;
					av[n] = NULL;
					sh_spawn(av);
				}
			}
			goto done;
		case KT_K_ENTER:
			if (nlines < NOTE_LINES) {
				memmove(&buf[cy + 2], &buf[cy + 1],
					sizeof(buf[0]) *
					(size_t)(nlines - cy - 1));
				snprintf(buf[cy + 1], NOTE_COLS_MAX, "%s",
					 buf[cy] + cx);
				buf[cy][cx] = '\0';
				nlines++;
				cy++;
				cx = 0;
				changed = 1;
			}
			break;
		case KT_K_BACKSPACE:
			if (cx > 0) {
				memmove(buf[cy] + cx - 1, buf[cy] + cx,
					(size_t)(len - cx) + 1);
				cx--;
				changed = 1;
			} else if (cy > 0) {
				int plen = (int)strlen(buf[cy - 1]);

				if (plen + len < NOTE_COLS_MAX) {
					strcat(buf[cy - 1], buf[cy]);
					memmove(&buf[cy], &buf[cy + 1],
						sizeof(buf[0]) *
						(size_t)(nlines - cy - 1));
					nlines--;
					cy--;
					cx = plen;
					changed = 1;
				}
			}
			break;
		case KT_K_UP:
			if (cy > 0)
				cy--;
			break;
		case KT_K_DOWN:
			if (cy + 1 < nlines)
				cy++;
			break;
		case KT_K_LEFT:
			if (cx > 0)
				cx--;
			else if (cy > 0)
				cx = (int)strlen(buf[--cy]);
			break;
		case KT_K_RIGHT:
			if (cx < len)
				cx++;
			else if (cy + 1 < nlines) {
				cy++;
				cx = 0;
			}
			break;
		case KT_K_HOME:
			cx = 0;
			break;
		case KT_K_END:
			cx = len;
			break;
		default:
			if (ev.key >= 0x20 && ev.key < 0x7f &&
			    len + 1 < NOTE_COLS_MAX) {
				memmove(buf[cy] + cx + 1, buf[cy] + cx,
					(size_t)(len - cx) + 1);
				buf[cy][cx++] = (char)ev.key;
				changed = 1;
			}
			break;
		}
	}
done:
	note_save();
	kdisp_shutdown();
	return 0;
}
