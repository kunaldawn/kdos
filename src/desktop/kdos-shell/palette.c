/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-palette — one search over everything the desktop can reach
 *
 *   ╔═ ═══════════════════════════════════════════════════╗
 *   ║ prin_                                               ║
 *   ╟─────────────────────────────────────────────────────╢
 *   ║ APPLICATIONS                                        ║
 *   ║ ▸ Printers                    kdos-print            ║
 *   ║ SETTINGS                                            ║
 *   ║   Hardware                    kdos-settings --page  ║
 *   ╚═════════════════════════════════════════════════════╝
 *
 * SIX SOURCES AND ONE MATCHER. A window, an application, a route, a settings
 * page, a file and a chord are six different things to want and one thing to
 * type, so they are searched together and ranked by the same `kb_fuzzy()` the
 * launcher and the Start menu use. Three private matchers is what this
 * replaced, and it meant `sm` found the system monitor in one surface and
 * nothing in another.
 *
 * THE ORDER OF THE HEADINGS IS FIXED AND IS NOT THE RANKING. Windows first,
 * because the cheapest thing to want is the window you already had; then
 * applications, routes, settings, files, chords. Within a heading the score
 * orders the rows. A heading with no hits is not drawn — a column of empty
 * category names is a list that looks broken.
 *
 * FILES ONLY AFTER THREE CHARACTERS. Every other source is a table already in
 * memory; the file source forks `fd`, and doing that per keystroke is a
 * directory walk per keystroke. Three characters is where a query stops
 * matching most of a disk.
 *
 * A CHORD ROW SHOWS THE CHORD AND DOES NOT PRESS IT. Enter runs a chord's
 * program when the chord runs one, and otherwise says which keys to press: a
 * client that could fire the session's own actions would be synthesised input
 * over the surface socket, which is exactly the hole this desktop's protocol
 * is shaped to not have. The row is still worth having — "what was the chord
 * for tiling" is a question people ask far more often than they rebind one.
 *
 * `--apps` IS THE LAUNCHER. `Super+d` wants applications and nothing else, and
 * that is one flag rather than a second program with a second matcher and a
 * second idea of ranking.
 * ---------------------------------
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kxdg.h"
#include "chords.h"
#include "filesearch.h"
#include "pages.h"
#include "routes.h"
#include "kwl.h"
#include "shell.h"

#define PA_COLS		78
#define PA_ROWS		24
#define PA_MAX_ROWS	400
/* Per source, so one source cannot crowd out the rest: a query matching four
 * hundred files would otherwise push every window and every chord off the
 * bottom of a list somebody is looking at for a window. */
#define PA_PER_KIND	12
/* Below this the file source does not run at all. */
#define PA_FILE_MIN	3

enum { P_WINDOW = 0, P_APP, P_ROUTE, P_SETTING, P_FILE, P_CHORD, P_N };

static const char *const KIND[P_N] = {
	"WINDOWS", "APPLICATIONS", "ROUTES", "SETTINGS", "FILES", "CHORDS"
};

struct prow {
	int kind;
	int score;
	int heading;
	char label[128];
	char detail[160];
	/* What Enter does, one of these by kind. */
	unsigned win;			/* P_WINDOW: the display's window id */
	char id[SH_APP_ID];		/* P_APP: the desktop-entry id       */
	int route;			/* P_ROUTE: the shared route index   */
	char page[32];			/* P_SETTING: what --page takes      */
	char path[PATH_MAX];		/* P_FILE                            */
	char chord[48];			/* P_CHORD: what to press            */
	char code[3];			/* M.7's two letters, when it has one */
};

static struct prow rows[PA_MAX_ROWS];
static int nrows, sel, top;
static char query[128];
static int qlen;
static char note[192];
/* `--apps`: the launcher is this program with one source. */
static int apps_only;
static KtuiKeys keys;

/* ── the rows ──────────────────────────────────────────────────────────── */

static struct prow *row_add(int kind, int score)
{
	if (nrows >= PA_MAX_ROWS)
		return NULL;
	memset(&rows[nrows], 0, sizeof(rows[0]));
	rows[nrows].kind = kind;
	rows[nrows].score = score;
	rows[nrows].route = -1;
	return &rows[nrows++];
}

static int cmp_row(const void *pa, const void *pb)
{
	const struct prow *a = pa, *b = pb;

	/* THE HEADING ORDER IS NOT THE RANKING. The kind decides the group and
	 * the score decides the order inside it, so a very good file match
	 * never climbs above the window you were just looking at. */
	if (a->kind != b->kind)
		return a->kind - b->kind;
	if (a->score != b->score)
		return a->score < b->score ? 1 : -1;
	return strcasecmp(a->label, b->label);
}

/* Cap each kind AFTER the sort, so what survives is that kind's best. */
static void cap_kinds(void)
{
	int seen[P_N] = { 0 }, out = 0;

	for (int i = 0; i < nrows; i++) {
		int k = rows[i].kind;

		if (seen[k] >= PA_PER_KIND)
			continue;
		seen[k]++;
		if (out != i)
			rows[out] = rows[i];
		out++;
	}
	nrows = out;
}

/*
 * The headings, inserted once the rows are in their groups. A heading with no
 * rows under it is never inserted rather than being inserted and hidden: a
 * hidden row is still a row the selection can land on.
 */
static void headings_add(void)
{
	for (int i = 0; i < nrows; i++) {
		if (i && rows[i].kind == rows[i - 1].kind)
			continue;
		if (nrows >= PA_MAX_ROWS)
			return;
		memmove(&rows[i + 1], &rows[i],
			sizeof(rows[0]) * (size_t)(nrows - i));
		nrows++;
		memset(&rows[i], 0, sizeof(rows[i]));
		rows[i].kind = rows[i + 1].kind;
		rows[i].heading = 1;
		snprintf(rows[i].label, sizeof(rows[i].label), "%s",
			 KIND[rows[i].kind]);
		i++;		/* step over the row the heading now precedes */
	}
}

/* ── the six sources ───────────────────────────────────────────────────── */

/*
 * WINDOWS. Through libkdisp, in this process, rather than by asking the
 * session over its socket: the list is already here — the panel's window row
 * and the Alt-Tab ring both read it — and a second path to it would be a
 * second answer to what is open.
 */
static void src_windows(void)
{
	if (!kdisp_win_supported())
		return;
	for (int i = 0; i < kdisp_win_count(); i++) {
		KDispWin w;
		int s;

		if (!kdisp_win_at(i, &w))
			continue;
		s = kb_fuzzy(w.title, query);
		if (!s)
			s = kb_fuzzy(w.app_id, query);
		if (!s)
			continue;

		struct prow *r = row_add(P_WINDOW, s);

		if (!r)
			return;
		/* THE ID AND NOT THE INDEX: kdisp_win_activate takes an id,
		 * and the list can be rebuilt between the frame this row was
		 * made in and the keystroke that fires it — an index would
		 * then raise whatever had moved into that slot. */
		r->win = w.id;
		snprintf(r->label, sizeof(r->label), "%s",
			 w.title[0] ? w.title : w.app_id);
		snprintf(r->detail, sizeof(r->detail), "%s", w.app_id);
	}
}

/* APPLICATIONS, through the shared index and the shared matcher. */
static void src_apps(void)
{
	const struct sh_app *hits[PA_MAX_ROWS];
	int n = sh_apps_match(query, hits, PA_PER_KIND * 2);

	for (int i = 0; i < n; i++) {
		struct prow *r;
		/* The index already ranked these, and it ranked them by the
		 * same function — so the position IS the score, inverted so
		 * that first is highest. */
		int s = n - i;

		r = row_add(P_APP, s);
		if (!r)
			return;
		snprintf(r->id, sizeof(r->id), "%s", hits[i]->id);
		snprintf(r->label, sizeof(r->label), "%s", hits[i]->name);
		snprintf(r->detail, sizeof(r->detail), "%s", hits[i]->exec);
		{
			const char *c = sh_fav_code(hits[i]->id);

			if (c && c[0])
				snprintf(r->code, sizeof(r->code), "%s", c);
		}
	}
}

/*
 * ROUTES. A route answers to its name and to the command it runs, which is
 * what makes `network` reach `setup.network` without a synonym table.
 */
static void src_routes(void)
{
	int n = sh_routes_load();

	for (int i = 0; i < n; i++) {
		const struct sh_route *rt = sh_route_at(i);
		int s, t;

		if (!rt)
			continue;
		s = kb_fuzzy(rt->name, query);
		t = kb_fuzzy(rt->cmd, query);
		if (t > 8 && t - 8 > s)
			s = t - 8;
		else if (t && !s)
			s = 1;
		if (!s)
			continue;

		struct prow *r = row_add(P_ROUTE, s);

		if (!r)
			return;
		r->route = i;
		snprintf(r->label, sizeof(r->label), "%s", rt->name);
		snprintf(r->detail, sizeof(r->detail), "%s", rt->cmd);
	}
}

/* SETTINGS PAGES, asked of the control centre rather than repeated here. */
static void src_settings(void)
{
	const char *const *labels, *const *names;
	int n = sh_settings_pages(&labels, &names);

	for (int i = 0; i < n; i++) {
		int s = kb_fuzzy(labels[i], query);
		struct prow *r;

		if (!s)
			continue;
		r = row_add(P_SETTING, s);
		if (!r)
			return;
		snprintf(r->page, sizeof(r->page), "%s", names[i]);
		snprintf(r->label, sizeof(r->label), "%s", labels[i]);
		snprintf(r->detail, sizeof(r->detail),
			 "kdos-settings --page %s", names[i]);
	}
}

/*
 * CHORDS. Read from whichever desktop this is — the shared reader keeps that
 * rule — so the palette never names the other one's keys.
 */
static void src_chords(void)
{
	int n = sh_chords_load();

	for (int i = 0; i < n; i++) {
		const struct sh_chord *c = sh_chord_at(i);
		int s, t;

		if (!c)
			continue;
		s = kb_fuzzy(c->action, query);
		t = kb_fuzzy(c->detail, query);
		if (t > 8 && t - 8 > s)
			s = t - 8;
		else if (t && !s)
			s = 1;
		if (!s)
			continue;

		struct prow *r = row_add(P_CHORD, s);

		if (!r)
			return;
		snprintf(r->chord, sizeof(r->chord), "%s", c->chord);
		/*
		 * A ROW IS NAMED BY WHAT IT DOES, and on the two desktops that
		 * is two different fields. The console's action IS the verb —
		 * `tile`, `launcher` — while the compositor's is labwc's, so
		 * nearly every row there is the word `Execute` and the thing a
		 * person recognises is the command it runs. A list of forty
		 * rows all called Execute is a list nobody can search.
		 */
		if (!strcmp(c->action, "Execute") && c->detail[0])
			snprintf(r->label, sizeof(r->label), "%s", c->detail);
		else
			snprintf(r->label, sizeof(r->label), "%s", c->action);
		snprintf(r->detail, sizeof(r->detail), "%s", c->chord);
	}
}

/*
 * FILES, and the only source that costs a fork. The child is asynchronous —
 * its lines arrive over several frames — so this starts it and the poll below
 * turns each line into a row. Under three characters it does not run at all.
 */
static int files_dirty;

static void file_line(const char *line, int tag, void *user)
{
	struct prow *r;

	(void)tag;
	(void)user;
	if (!line || !*line)
		return;
	files_dirty = 1;
	r = row_add(P_FILE, kb_fuzzy(kb_basename(line), query));
	if (!r)
		return;
	snprintf(r->path, sizeof(r->path), "%s", line);
	snprintf(r->label, sizeof(r->label), "%s", kb_basename(line));
	snprintf(r->detail, sizeof(r->detail), "%s", line);
}

static void src_files_start(void)
{
	const char *home = kb_home_dir();

	sh_fsearch_stop();
	files_dirty = 0;
	if (qlen < PA_FILE_MIN || apps_only)
		return;
	sh_fsearch_names(home ? home : ".", query, P_FILE, file_line, NULL);
}

/* ── rebuilding the list ───────────────────────────────────────────────── */

static void refilter(void)
{
	nrows = 0;
	sel = 0;
	top = 0;

	if (apps_only) {
		src_apps();
	} else {
		/* Cheap sources first, but the ORDER HERE IS NOT THE DISPLAY
		 * ORDER — cmp_row puts the kinds in their fixed order. This
		 * order is only which table is walked first. */
		src_windows();
		src_apps();
		src_routes();
		src_settings();
		src_chords();
	}
	qsort(rows, (size_t)nrows, sizeof(rows[0]), cmp_row);
	cap_kinds();
	headings_add();

	/* The file child runs alongside; its rows arrive over the next few
	 * frames and are merged by file_merge(). */
	src_files_start();

	/* A heading may not be selected: it opens nothing. */
	while (sel < nrows && rows[sel].heading)
		sel++;
}

/*
 * A child's lines arrive after the list was already sorted, so the file rows
 * are appended and the whole list is put back in order. Re-sorting a few
 * hundred rows a few times a second is cheaper than the fork that produced
 * them, and it keeps one comparison rather than a second insertion path.
 */
static void file_merge(void)
{
	int before = nrows;

	/* The headings are rebuilt, so drop them first: a heading left in
	 * place would sort as a row of its kind and land in the middle. */
	int out = 0;

	for (int i = 0; i < nrows; i++)
		if (!rows[i].heading) {
			if (out != i)
				rows[out] = rows[i];
			out++;
		}
	nrows = out;
	qsort(rows, (size_t)nrows, sizeof(rows[0]), cmp_row);
	cap_kinds();
	headings_add();
	if (nrows != before && sel >= nrows)
		sel = nrows ? nrows - 1 : 0;
	while (sel < nrows && rows[sel].heading)
		sel++;
}

/* ── firing one ────────────────────────────────────────────────────────── */

static int quit;

static void run_argv(const char *const argv[])
{
	sh_spawn(argv);
}

static void fire(int i)
{
	const struct prow *r;

	if (i < 0 || i >= nrows || rows[i].heading)
		return;
	r = &rows[i];

	switch (r->kind) {
	case P_WINDOW:
		/* Raising is the display's, not ours: the same call the
		 * taskbar's window row and the Alt-Tab ring make. */
		kdisp_win_activate((unsigned)r->win);
		break;
	case P_APP: {
		const struct sh_app *a = sh_apps_find(r->id);

		if (a)
			sh_apps_launch(a);
		break;
	}
	case P_ROUTE: {
		const struct sh_route *rt = sh_route_at(r->route);

		/* THE ROUTE'S OWN VECTOR, never a string put back together
		 * and split again: a route is split once, on blanks, with no
		 * quoting and no shell, and this is that split. */
		if (rt && rt->nargv)
			run_argv(rt->argv);
		break;
	}
	case P_SETTING: {
		const char *argv[4];

		argv[0] = "kdos-settings";
		argv[1] = "--page";
		argv[2] = r->page;
		argv[3] = NULL;
		run_argv(argv);
		break;
	}
	case P_FILE: {
		const char *argv[4];

		/* THROUGH THE ONE OPENER, so a file opens with whatever its
		 * type is bound to rather than with whatever this surface
		 * guessed. */
		argv[0] = "kdos-appbox";
		argv[1] = "open";
		argv[2] = r->path;
		argv[3] = NULL;
		run_argv(argv);
		break;
	}
	case P_CHORD:
		/*
		 * SHOWN, NOT PRESSED. A client cannot fire the session's own
		 * actions and must not be able to: synthesised input over the
		 * surface socket is a way to drive somebody's desktop for
		 * anything that can reach it, which is the rule the protocol
		 * is shaped around. So a chord that runs a PROGRAM runs it,
		 * and a chord that is a session action says which keys to
		 * press instead.
		 */
		if (r->detail[0] && strchr(r->detail, ' ') == NULL &&
		    r->detail[0] != 'S') {
			const char *argv[2];

			argv[0] = r->detail;
			argv[1] = NULL;
			run_argv(argv);
			break;
		}
		snprintf(note, sizeof(note), "press %s", r->chord);
		return;	/* the palette stays open: nothing was started */
	}
	quit = 1;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int list_y = 3, list_h = h - list_y - 2;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), apps_only ? "launch" : "search",
		      KT_ACCENT, KT_SURFACE, 1);

	/* The input row, with the caret drawn as a block so a dump shows where
	 * it is: a golden of a field with no visible caret cannot tell an
	 * empty field from a missing one. */
	ktui_draw_textf(2, 1, w - 4, KT_TEXT, KT_SURFACE, KT_A_NONE, "%s%s",
			query, ktui_glyph[KT_G_FULL]);
	ktui_draw_hline(1, 2, w - 2, 0, KT_DIM, KT_SURFACE);

	if (!nrows) {
		ktui_draw_text(2, list_y, w - 4,
			       qlen ? "no match" : "type to search", KT_MID,
			       KT_SURFACE, KT_A_NONE);
	}

	if (sel < top)
		top = sel;
	if (sel >= top + list_h)
		top = sel - list_h + 1;
	if (top < 0)
		top = 0;

	for (int i = top; i < nrows && i - top < list_h; i++) {
		int y = list_y + (i - top);
		const struct prow *r = &rows[i];

		if (r->heading) {
			ktui_draw_text(2, y, w - 4, r->label, KT_MID,
				       KT_SURFACE, KT_A_BOLD);
			continue;
		}
		ktui_draw_text(2, y, 2, i == sel ? ktui_glyph[KT_G_RIGHT] : " ",
			       KT_ACCENT, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(4, y, 34, r->label, KT_TEXT, KT_SURFACE,
			       i == sel ? KT_A_BOLD : KT_A_NONE);
		/* The two-letter code, where a row has one: it is the fastest
		 * way to reach the row and is worth a column of its own. */
		if (r->code[0])
			ktui_draw_text(39, y, 3, r->code, KT_WARN, KT_SURFACE,
				       KT_A_BOLD);
		if (w > 46)
			ktui_draw_text(43, y, w - 45, r->detail, KT_MID,
				       KT_SURFACE, KT_A_NONE);
		if (i == sel)
			ktui_draw_reverse(krect(1, y, w - 2, 1));
	}

	if (note[0])
		ktui_draw_text(2, h - 2, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	else {
		ktui_hint("Enter", "open");
		ktui_hint("Esc", "close");
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
	}
}

/* ── the two-letter codes ──────────────────────────────────────────────── */

/*
 * M.7's codes, and the same rule the menu keeps: BOTH letters with nothing
 * else in the field. A code that fired while somebody was in the middle of
 * typing a search would open a program at the second keystroke of a word.
 */
static int code_fire(int ch)
{
	static char typed[3];

	if (qlen)
		return 0;
	if (!isalpha(ch))
		return typed[0] = 0;
	typed[typed[0] ? 1 : 0] = (char)toupper(ch);
	if (!typed[1])
		return 1;	/* the first letter: eaten, waiting for the second */

	char want[3] = { typed[0], typed[1], 0 };

	typed[0] = typed[1] = 0;
	for (int i = 0; i < nrows; i++)
		if (!rows[i].heading && rows[i].code[0] &&
		    !strcmp(rows[i].code, want)) {
			fire(i);
			return 1;
		}
	return 1;
}

/* ── main ──────────────────────────────────────────────────────────────── */

static int pa_usage(void)
{
	fprintf(stderr,
		"usage: kdos-palette [--apps] [--route NAME] [--font NAME]\n"
		"                    [--dump] [--dump-query TEXT]\n");
	return 2;
}

int palette_main(int argc, char **argv)
{
	const char *font = NULL, *preset = "";
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--apps"))
			apps_only = 1;
		else if (!strcmp(argv[i], "--route") && i + 1 < argc)
			preset = argv[++i];
		else if (!strcmp(argv[i], "--dump-query") && i + 1 < argc)
			preset = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else
			return pa_usage();
	}

	/*
	 * `--apps` IS THE LAUNCHER, and the name it was reached by is what
	 * says so when no flag was given: kdos-launcher and kdos-palette are
	 * one program, so the symlink decides which sources run rather than
	 * a second binary with a second matcher.
	 */
	if (!apps_only && argc > 0 && argv[0]) {
		const char *self = kb_basename(argv[0]);

		if (!strcmp(self, "kdos-launcher"))
			apps_only = 1;
	}

	qlen = snprintf(query, sizeof(query), "%s", preset);
	if (qlen < 0 || qlen >= (int)sizeof(query))
		qlen = (int)strlen(query);

	sh_apps_load();
	sh_theme_from_cache();

	if (dump) {
		ktui_offscreen_init(PA_COLS, PA_ROWS);
		ktui_draw_init();
		refilter();
		/*
		 * THE FILE SOURCE IS NOT IN A DUMP, and that is a statement
		 * rather than an omission: it forks `fd`, so a golden with it
		 * would be a golden of whatever happens to be in the home
		 * directory of whoever ran the suite.
		 */
		sh_fsearch_stop();
		draw();
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.corner = KDISP_CORNER_CENTER,
		.cols = PA_COLS,
		.rows = PA_ROWS,
		.app_id = "kdos-palette",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-palette: no display\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);
	refilter();

	while (!quit && !kdisp_should_close()) {
		sh_theme_poll();
		draw();
		ktui_draw_flush();

		KtuiEvent ev;
		int fd = sh_fsearch_fd();

		/*
		 * A SHORT WAIT WHILE A CHILD IS RUNNING AND A LONG ONE WHEN IT
		 * IS NOT. The file search arrives on a descriptor this loop
		 * does not select on — the backend owns the wait — so the
		 * cheap way to collect it is to wake often enough to notice
		 * and rarely enough to cost nothing at rest.
		 */
		if (!ktui_backend()->poll_event(&ev, fd >= 0 ? 40 : 1000)) {
			/* sh_fsearch_poll() is 1 while the child runs and 0
			 * once it has been reaped — never negative, so the
			 * merge is gated on rows having ARRIVED rather than on
			 * a return value that is always true. */
			if (fd >= 0) {
				sh_fsearch_poll();
				if (files_dirty) {
					files_dirty = 0;
					file_merge();
				}
			}
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (fd >= 0) {
			sh_fsearch_poll();
			if (files_dirty) {
				files_dirty = 0;
				file_merge();
			}
		}
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (ev.type != KT_EVT_KEY)
			continue;

		note[0] = '\0';
		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_ENTER) {
			fire(sel);
			continue;
		}
		if (ev.key == KT_K_UP || ev.key == KT_K_DOWN) {
			int step = ev.key == KT_K_DOWN ? 1 : -1;
			int at = sel;

			/* Step over the headings: one opens nothing, so a
			 * selection resting on it is a row Enter does not
			 * answer. */
			do {
				at += step;
			} while (at >= 0 && at < nrows && rows[at].heading);
			if (at >= 0 && at < nrows)
				sel = at;
			continue;
		}
		if (ev.key == KT_K_BACKSPACE) {
			if (qlen) {
				query[--qlen] = '\0';
				refilter();
			}
			continue;
		}
		if (ev.key >= 0x20 && ev.key < 0x7f) {
			if (!apps_only && code_fire(ev.key))
				continue;
			if (qlen < (int)sizeof(query) - 1) {
				query[qlen++] = (char)ev.key;
				query[qlen] = '\0';
				refilter();
			}
		}
	}

	sh_fsearch_stop();
	if (!dump)
		kdisp_shutdown();
	return 0;
}
