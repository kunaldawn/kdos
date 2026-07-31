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
 * Only the double-line box characters and the full block are drawn, because
 * the console font (ter-kdos32n) has those and no half blocks or shades.
 */

#include "kdos-appbox.h"

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANE_BOXES   0
#define PANE_PROFILE 1

#define MAX_BOXES 64

typedef struct {
	char name[64];
	char image[128];
	char state[32];
} BoxRow;

static BoxRow boxes[MAX_BOXES];
static int nboxes, sel_box, sel_key, pane;
static Profile prof;
static char msg[256];

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
	Argv a = {0};
	char *buf = xmalloc(1 << 16);
	char *line, *save;

	nboxes = 0;
	argv_add(&a, "podman");
	argv_add(&a, "ps");
	argv_add(&a, "--all");
	argv_add(&a, "--format");
	argv_add(&a, "{{.Names}}\t{{.Image}}\t{{.State}}");
	argv_end(&a);
	if (run_capture(&a, buf, 1 << 16) == 0) {
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
	if (sel_box >= nboxes)
		sel_box = nboxes ? nboxes - 1 : 0;
	if (nboxes)
		profile_load(&prof, boxes[sel_box].name);
	else
		profile_defaults(&prof, DEFAULT_BOX);
}

static void frame(int y, int x, int h, int w, const char *title, int active)
{
	int i;
	attron(COLOR_PAIR(active ? 2 : 1));
	mvaddch(y, x, ACS_ULCORNER);
	mvaddch(y + h - 1, x, ACS_LLCORNER);
	mvaddch(y, x + w - 1, ACS_URCORNER);
	mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
	for (i = 1; i < w - 1; i++) {
		mvaddch(y, x + i, ACS_HLINE);
		mvaddch(y + h - 1, x + i, ACS_HLINE);
	}
	for (i = 1; i < h - 1; i++) {
		mvaddch(y + i, x, ACS_VLINE);
		mvaddch(y + i, x + w - 1, ACS_VLINE);
	}
	mvprintw(y, x + 2, " %s ", title);
	attroff(COLOR_PAIR(active ? 2 : 1));
}

static void draw(void)
{
	int h, w, i, lw, rw;

	getmaxyx(stdscr, h, w);
	erase();
	lw = w / 2;
	rw = w - lw;

	attron(COLOR_PAIR(2) | A_BOLD);
	mvprintw(0, 2, "KDOS appbox manager");
	attroff(COLOR_PAIR(2) | A_BOLD);

	frame(1, 0, h - 4, lw, "Boxes", pane == PANE_BOXES);
	for (i = 0; i < nboxes && i < h - 6; i++) {
		if (i == sel_box && pane == PANE_BOXES)
			attron(A_REVERSE);
		mvprintw(2 + i, 2, "%-16.16s %-10.10s", boxes[i].name,
			 boxes[i].state);
		if (i == sel_box && pane == PANE_BOXES)
			attroff(A_REVERSE);
	}
	if (!nboxes)
		mvprintw(2, 2, "(no boxes — press n)");

	frame(1, lw, h - 4, rw, "Sandbox profile", pane == PANE_PROFILE);
	if (nboxes) {
		mvprintw(2, lw + 2, "image %.*s", rw - 10, prof.image);
		for (i = 0; i < NKEYS; i++) {
			int on = *key_field(&prof, i);
			if (i == sel_key && pane == PANE_PROFILE)
				attron(A_REVERSE);
			mvprintw(4 + i, lw + 2, "%-10s %-8s", KEYS[i].label,
				 on ? "private" : "shared");
			if (i == sel_key && pane == PANE_PROFILE)
				attroff(A_REVERSE);
		}
		mvprintw(4 + NKEYS + 1, lw + 2,
			 "private = its own namespace,");
		mvprintw(4 + NKEYS + 2, lw + 2,
			 "not the host's.");
	}

	attron(COLOR_PAIR(1));
	mvprintw(h - 2, 2,
		 "TAB pane  SPACE toggle  s save  r recreate  n new  d delete  q quit");
	attroff(COLOR_PAIR(1));
	if (msg[0]) {
		attron(COLOR_PAIR(3));
		mvprintw(h - 3, 2, "%.*s", w - 4, msg);
		attroff(COLOR_PAIR(3));
	}
	refresh();
}

/* Read a line in the status area. Returns 0 when the user cancels. */
static int prompt(const char *label, char *out, size_t n)
{
	int h, w, rc;
	getmaxyx(stdscr, h, w);
	(void)w;
	attron(COLOR_PAIR(2));
	mvprintw(h - 3, 2, "%s", label);
	attroff(COLOR_PAIR(2));
	clrtoeol();
	echo();
	curs_set(1);
	rc = mvgetnstr(h - 3, (int)strlen(label) + 3, out, (int)n - 1);
	curs_set(0);
	noecho();
	return rc == OK && out[0];
}

/* Leave curses for an operation that prints, then come back. */
static void shell_out(int (*fn)(const char *), const char *arg)
{
	def_prog_mode();
	endwin();
	fn(arg);
	printf("\n[enter] ");
	fflush(stdout);
	getchar();
	reset_prog_mode();
	refresh();
}

static int do_recreate(const char *box)
{
	Profile p;
	profile_load(&p, box);
	if (box_exists(box))
		box_remove(box, 1);
	return box_create(&p);
}

int tui_main(void)
{
	int ch, running = 1;

	if (!initscr())
		die("cannot start the TUI on this terminal");
	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_GREEN, -1);
	init_pair(3, COLOR_YELLOW, -1);
	cbreak();
	noecho();
	curs_set(0);
	keypad(stdscr, TRUE);

	load_boxes();
	while (running) {
		draw();
		ch = getch();
		msg[0] = '\0';
		switch (ch) {
		case '\t':
			pane = !pane;
			break;
		case KEY_UP:
		case 'k':
			if (pane == PANE_BOXES) {
				if (sel_box > 0)
					sel_box--;
				if (nboxes)
					profile_load(&prof, boxes[sel_box].name);
			} else if (sel_key > 0) {
				sel_key--;
			}
			break;
		case KEY_DOWN:
		case 'j':
			if (pane == PANE_BOXES) {
				if (sel_box + 1 < nboxes)
					sel_box++;
				if (nboxes)
					profile_load(&prof, boxes[sel_box].name);
			} else if (sel_key + 1 < NKEYS) {
				sel_key++;
			}
			break;
		case ' ':
			if (pane == PANE_PROFILE && nboxes) {
				int *f = key_field(&prof, sel_key);
				*f = !*f;
				snprintf(msg, sizeof(msg),
					 "unsaved — press s to write the profile");
			}
			break;
		case 's':
			if (nboxes && profile_save(&prof) == 0)
				snprintf(msg, sizeof(msg),
					 "saved; press r to recreate %s so it "
					 "takes effect", prof.name);
			break;
		case 'r':
			if (nboxes) {
				shell_out(do_recreate, boxes[sel_box].name);
				load_boxes();
			}
			break;
		case 'n': {
			char name[64] = {0};
			if (prompt("new box name:", name, sizeof(name))) {
				Profile p;
				profile_defaults(&p, name);
				profile_save(&p);
				def_prog_mode();
				endwin();
				box_create(&p);
				reset_prog_mode();
				refresh();
				load_boxes();
			}
			break;
		}
		case 'd':
			if (nboxes) {
				char yn[8] = {0};
				snprintf(msg, sizeof(msg), "delete %s?",
					 boxes[sel_box].name);
				draw();
				if (prompt("type yes to delete:", yn, sizeof(yn)) &&
				    !strcmp(yn, "yes")) {
					box_remove(boxes[sel_box].name, 1);
					load_boxes();
				}
			}
			break;
		case 'q':
		case 27:
			running = 0;
			break;
		default:
			break;
		}
	}
	endwin();
	return 0;
}
