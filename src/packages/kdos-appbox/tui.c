/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * kdos-appbox tui — the same operations as the CLI, in a full-screen pane.
 *
 * It is a front-end and nothing more: every action calls the identical
 * box_ and app_ function the CLI calls, so there is one implementation of each
 * operation and the TUI cannot drift from `kdos-appbox security`.
 *
 * Drawn with libktui, which is where the installer's toolkit went. This used
 * to be ncurses — a second terminal library, a second glyph story and a real
 * -lncursesw on a host that otherwise needs none. The keys are unchanged.
 */

#include "kdos-appbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ktui.h"

#define PANE_BOXES   0
#define PANE_PROFILE 1

#define MAX_BOXES 64

typedef struct {
	char name[64];
	char image[128];
	char state[32];
} BoxRow;

static BoxRow boxes[MAX_BOXES];
static int nboxes, sel_key, pane;
static KtuiList blist;
static Profile prof;
static char msg[256];

/* An overlay text prompt. Immediate mode, so it is a flag and a buffer rather
 * than the nested event loop ncurses' mvgetnstr forced. */
enum { PR_NONE = 0, PR_NEWBOX, PR_DELETE };
static int prompt_kind;
static char prompt_buf[64];

/* The profile keys, in the order the pane lists them. */
static const struct {
	const char *label;
	size_t offset;
} KEYS[] = {
	{ "network",   offsetof(Profile, netns)    },
	{ "ipc",       offsetof(Profile, ipc)      },
	{ "devices",   offsetof(Profile, devsys)   },
	{ "processes", offsetof(Profile, process)  },
	{ "home",      offsetof(Profile, privhome) },
	{ "init",      offsetof(Profile, init)     },
};
#define NKEYS ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

static int *key_field(Profile *p, int i)
{
	return (int *)((char *)p + KEYS[i].offset);
}

static void load_boxes(void)
{
	KbArgv a = {0};
	char *buf = kb_calloc(1, 1 << 16);
	char *line, *save;

	nboxes = 0;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}\t{{.Image}}\t{{.State}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, 1 << 16) == 0) {
		for (line = strtok_r(buf, "\n", &save);
		     line && nboxes < MAX_BOXES;
		     line = strtok_r(NULL, "\n", &save)) {
			char *img = strchr(line, '\t'), *st;
			if (!img)
				continue;
			*img++ = '\0';
			st = strchr(img, '\t');
			if (!st)
				continue;
			*st++ = '\0';
			snprintf(boxes[nboxes].name, sizeof(boxes[0].name), "%s", line);
			snprintf(boxes[nboxes].image, sizeof(boxes[0].image), "%s", img);
			snprintf(boxes[nboxes].state, sizeof(boxes[0].state), "%s", st);
			nboxes++;
		}
	}
	free(buf);
	if (blist.sel >= nboxes)
		blist.sel = nboxes ? nboxes - 1 : 0;
	if (nboxes)
		profile_load(&prof, boxes[blist.sel].name);
	else
		profile_defaults(&prof, DEFAULT_BOX);
}

/* The accent the user picked. Not decoration: one palette for the distro is
 * the whole reason libkcolor exists, and this window sits next to a desktop
 * that has already been retinted. */
static void adopt_theme(void)
{
	const char *cache = getenv("XDG_CACHE_HOME");
	char *path, name[32] = {0};

	if (cache && *cache)
		path = kb_path_join(cache, "kdos/theme");
	else
		path = kb_path_join(kb_home_dir(), ".cache/kdos/theme");
	if (kb_read_line_file(path, name, sizeof(name)) > 0)
		ktui_theme_set(name);
	free(path);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void row_box(int idx, int x, int y, int w, int sel, int focus, void *user)
{
	(void)user;
	int on = sel && focus;
	ktui_draw_textf(x + 1, y, w - 2, on ? KT_BG : KT_TEXT,
			on ? KT_ACCENT : KT_BG, 0, "%-16.16s %-10.10s",
			boxes[idx].name, boxes[idx].state);
}

static void draw_profile(KRect r)
{
	if (!nboxes)
		return;

	ktui_draw_textf(r.x + 2, r.y, r.w - 4, KT_MID, KT_BG, 0, "image %.*s",
			r.w - 10, prof.image);

	for (int i = 0; i < NKEYS; i++) {
		int on = *key_field(&prof, i);
		int cur = (i == sel_key && pane == PANE_PROFILE);
		int fg = cur ? KT_BG : on ? KT_ACCENT : KT_MID;
		int bg = cur ? KT_ACCENT : KT_BG;
		ktui_draw_fill(krect(r.x + 1, r.y + 2 + i, r.w - 2, 1), bg);
		ktui_draw_textf(r.x + 2, r.y + 2 + i, r.w - 4, fg, bg, 0,
				"%-10s %-8s", KEYS[i].label,
				on ? "private" : "shared");
	}

	ktui_note(r.x + 2, r.y + 3 + NKEYS, r.w - 4,
		  "private = its own namespace,");
	ktui_note(r.x + 2, r.y + 4 + NKEYS, r.w - 4, "not the host's.");
}

static void draw_prompt(void)
{
	static const char *label[] = { "", "new box name", "type yes to delete" };

	int w = 48;
	if (w > ktui_w - 6)
		w = ktui_w - 6;
	int h = 6;
	int x = (ktui_w - w) / 2, y = (ktui_h - h) / 2;

	KRect r = krect(x, y, w, h);
	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_shadow(r);
	ktui_draw_box(r, label[prompt_kind], KT_ACCENT, KT_SURFACE, 1);
	ktui_input(krect(x + 2, y + 2, w - 4, 1), prompt_buf, sizeof(prompt_buf),
		   0, "");
	ktui_draw_text(x + 2, y + 4, w - 4, "Enter accept   Esc cancel", KT_DIM,
		       KT_SURFACE, 0);
}

/* Leave the TUI for an operation that prints, then come back. */
static void console_out(int (*fn)(const char *), const char *arg)
{
	ktui_term_suspend();
	ktui_input_suspend();
	fn(arg);
	fputs("\n[enter] ", stdout);
	fflush(stdout);
	int c;
	while ((c = getchar()) != '\n' && c != EOF)
		;
	ktui_input_resume();
	ktui_term_resume();
}

static int do_recreate(const char *box)
{
	Profile p;
	profile_load(&p, box);
	if (box_exists(box))
		box_remove(box, 1);
	return box_create(&p);
}

static int do_create(const char *name)
{
	Profile p;
	profile_defaults(&p, name);
	profile_save(&p);
	return box_create(&p);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void prompt_done(void)
{
	if (prompt_kind == PR_NEWBOX && prompt_buf[0]) {
		console_out(do_create, prompt_buf);
		load_boxes();
	} else if (prompt_kind == PR_DELETE && !strcmp(prompt_buf, "yes")) {
		box_remove(boxes[blist.sel].name, 1);
		load_boxes();
	}
	prompt_kind = PR_NONE;
	prompt_buf[0] = '\0';
}

static int handle_key(const KtuiEvent *ev, int *running)
{
	switch (ev->key) {
	case KT_K_TAB:
		pane = !pane;
		return 1;
	case KT_K_UP:
	case 'k':
		if (pane == PANE_BOXES) {
			if (blist.sel > 0)
				blist.sel--;
			if (nboxes)
				profile_load(&prof, boxes[blist.sel].name);
		} else if (sel_key > 0) {
			sel_key--;
		}
		return 1;
	case KT_K_DOWN:
	case 'j':
		if (pane == PANE_BOXES) {
			if (blist.sel + 1 < nboxes)
				blist.sel++;
			if (nboxes)
				profile_load(&prof, boxes[blist.sel].name);
		} else if (sel_key + 1 < NKEYS) {
			sel_key++;
		}
		return 1;
	case ' ':
		if (pane == PANE_PROFILE && nboxes) {
			int *f = key_field(&prof, sel_key);
			*f = !*f;
			snprintf(msg, sizeof(msg),
				 "unsaved — press s to write the profile");
		}
		return 1;
	case 's':
		if (nboxes && profile_save(&prof) == 0)
			snprintf(msg, sizeof(msg),
				 "saved; press r to recreate %s so it takes effect",
				 prof.name);
		return 1;
	case 'r':
		if (nboxes) {
			console_out(do_recreate, boxes[blist.sel].name);
			load_boxes();
		}
		return 1;
	case 'n':
		prompt_kind = PR_NEWBOX;
		prompt_buf[0] = '\0';
		return 1;
	case 'd':
		if (nboxes) {
			prompt_kind = PR_DELETE;
			prompt_buf[0] = '\0';
			snprintf(msg, sizeof(msg), "delete %s?",
				 boxes[blist.sel].name);
		}
		return 1;
	case 'q':
	case KT_K_ESC:
		*running = 0;
		return 1;
	default:
		return 0;
	}
}

int tui_main(void)
{
	int running = 1;

	kb_set_oom_handler(ktui_term_shutdown);

	adopt_theme();
	if (ktui_term_init(1) < 0)
		kb_die("cannot start the TUI on this terminal");
	ktui_draw_init();
	ktui_input_init(1);

	load_boxes();

	while (running) {
		KtuiEvent ev;
		memset(&ev, 0, sizeof(ev));
		ktui_input_next(&ev, 250);

		if (ktui_w < 50 || ktui_h < 14) {
			ktui_toosmall("KDOS APPBOX", 50, 14);
			continue;
		}

		ktui_draw_clear();
		ktui_frame_begin(&ev);

		if (prompt_kind) {
			if (ev.type == KT_EVT_KEY && ev.key == KT_K_ESC) {
				prompt_kind = PR_NONE;
				ktui_consume();
			} else if (ev.type == KT_EVT_KEY && ev.key == KT_K_ENTER) {
				ktui_consume();
				prompt_done();
			}
		} else if (ev.type == KT_EVT_KEY) {
			msg[0] = '\0';
			if (handle_key(&ev, &running))
				ktui_consume();
		}

		int lw = ktui_w / 2;
		int body_h = ktui_h - 4;

		ktui_draw_text(2, 0, ktui_w - 4, "KDOS appbox manager", KT_ACCENT,
			       KT_BG, 0);

		KRect lr = krect(0, 1, lw, body_h);
		KRect rr = krect(lw, 1, ktui_w - lw, body_h);
		ktui_draw_box(lr, "Boxes", pane == PANE_BOXES ? KT_ACCENT : KT_DIM,
			      KT_BG, 0);
		ktui_draw_box(rr, "Sandbox profile",
			      pane == PANE_PROFILE ? KT_ACCENT : KT_DIM, KT_BG, 0);

		if (nboxes)
			ktui_list(krect(lr.x + 1, lr.y + 1, lr.w - 2, lr.h - 2),
				  &blist, nboxes, row_box, NULL,
				  pane == PANE_BOXES ? 0 : -1);
		else
			ktui_note(lr.x + 2, lr.y + 1, lr.w - 4, "(no boxes — press n)");

		draw_profile(krect(rr.x, rr.y + 1, rr.w, rr.h - 1));

		ktui_draw_fill(krect(0, ktui_h - 1, ktui_w, 1), KT_SURFACE);
		ktui_draw_text(2, ktui_h - 1, ktui_w - 4,
			       "TAB pane  SPACE toggle  s save  r recreate  "
			       "n new  d delete  q quit", KT_MID, KT_SURFACE, 0);
		if (msg[0])
			ktui_draw_text(2, ktui_h - 2, ktui_w - 4, msg, KT_WARN,
				       KT_BG, 0);

		if (prompt_kind)
			draw_prompt();

		ktui_frame_end();

		int px, py;
		if (ktui_input_mouse_visible(&px, &py))
			ktui_draw_cursor(px, py);

		ktui_draw_flush();
	}

	ktui_input_shutdown();
	ktui_term_shutdown();
	return 0;
}
