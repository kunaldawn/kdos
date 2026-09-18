/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — a session that survives the login that started it
 *
 * A console session ends with its login, and everything open in it goes with
 * it. What can come back is the LIST: which windows there were, how big, where,
 * on which workspace, and what each one was. What cannot come back is a
 * process — so nothing here replays a command line.
 *
 * NO ROW NAMES A COMMAND. A state file that named an argv would be a state
 * file that executes one: it is written by a program into a directory anything
 * running as this person can write, so what a row carries is a NAME something
 * else resolves — `con.conf` for a role or one of this desktop's own surfaces,
 * the pack store for an application. See `con_layout_resolve()`, which is the
 * one place that decides, and which a layout reads its rows through too.
 *
 * WHICH IS WHY A TERMINAL'S ROW NAMES A ROLE. Every terminal window's app id
 * is the literal `terminal`, so a row carrying that says a window WAS a
 * terminal and not which program was in it — and a session restored from one
 * would come back as a screen of bare shells. A window running the file
 * manager is written as `files`, and `con.conf` says what fills that.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kcon.h"
#include "kxdg.h"
#include "con.h"

/*
 * `$XDG_STATE_HOME/kdos/con/<name>.session`, through libkbase because the
 * state directory has two spellings and one place knows both.
 */
int con_state_path(const char *name, char *out, size_t n)
{
	char rel[192];

	if (!name || !*name)
		name = "con";
	/* A session name reaches this from `-t`, so it decides a FILE NAME: a
	 * slash or a dot-dot in it would write outside the state directory. */
	for (const char *p = name; *p; p++)
		if (*p == '/' || *p == '\\' || *p == ':' ||
		    (unsigned char)*p < 32)
			return 0;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return 0;
	if (snprintf(rel, sizeof(rel), "kdos/con/%s.session", name) >=
	    (int)sizeof(rel))
		return 0;
	return kb_state_path(rel, out, n);
}

/* The text of one saved terminal, numbered by its row. Beside the session
 * file, so one directory holds a session and everything in it. */
static int text_path(const char *name, int row, char *out, size_t n)
{
	char rel[192];

	if (!name || !*name)
		name = "con";
	if (snprintf(rel, sizeof(rel), "kdos/con/%s.%d.text", name, row) >=
	    (int)sizeof(rel))
		return 0;
	return kb_state_path(rel, out, n);
}

/* One field of a saved row, with everything a reader would trip over taken
 * out. Tabs separate the fields and a newline ends the row, so neither may
 * appear inside one — a title is a window's own text and can contain both. */
static void field(char *out, size_t n, const char *in)
{
	size_t k = 0;

	if (!n)
		return;
	for (const char *p = in ? in : ""; *p && k + 1 < n; p++)
		out[k++] = ((unsigned char)*p < 32 || *p == '\t') ? ' ' : *p;
	out[k] = '\0';
	if (!k && n > 1) {
		out[0] = '-';
		out[1] = '\0';
	}
}

/*
 * WHAT IS OPEN, AS TEXT. One row per window, oldest first — the list is kept
 * newest-first, so it is walked backwards here and the restore then opens them
 * in the order they were opened.
 *
 * A panel, a layer, the saver and the lock are NOT saved: every one of them is
 * a program the session starts for itself, and restoring one would put a
 * second copy on the screen beside the one that has just started.
 */
/* One terminal's lines, into its own file beside the session. */
static void save_text(const char *name, int row, Win *w)
{
	char tp[512];
	size_t len = 0;
	char *txt = kvt_term_text(w->term, &len);

	if (!txt)
		return;
	if (len && text_path(name, row, tp, sizeof(tp)))
		kb_write_file_atomic(tp, txt);
	free(txt);
}

/*
 * THE ROWS, RENDERED. Split out from the save below because a LAYOUT is the
 * same rows under a different name: one renderer means the two files cannot
 * drift into two formats, and it is the reason `kdos con layout save` does not
 * have to reach for the session file at all.
 *
 * `name` and `text` are for the per-terminal output files, which only a
 * session save writes — a layout is an arrangement and not a transcript.
 */
int con_state_rows(char *buf, size_t cap, const char *name, int text)
{
	char t[160], a[80];
	size_t n = 0;
	int rows = 0;

	n += (size_t)snprintf(buf + n, cap - n,
			      "# kdos-con session state\n"
			      "# kind\tworkspace\tx\ty\tw\th\tapp\tflags\ttitle\n");

	/* THE LIST IS THE STACK, newest first, so it is collected and walked
	 * back: a restore that opened them in stacking order would leave the
	 * window that was at the bottom on top. */
	Win *ord[CON_STATE_MAX];
	int nord = 0;

	for (Win *w = S.wins; w && nord < CON_STATE_MAX; w = w->next) {
		if (w->panel || w->overlay || w->background || w->kind == WIN_VT)
			continue;
		if (w == S.saver || w == S.lock)
			continue;
		if (w->kind != WIN_TERM && w->kind != WIN_SURFACE)
			continue;
		/* An application with no app_id cannot be found again: there
		 * is no entry to start it from, and guessing at its title
		 * would start whatever happens to match. */
		if (w->kind == WIN_SURFACE && !w->app_id[0])
			continue;
		ord[nord++] = w;
	}

	while (nord--) {
		Win *w = ord[nord];

		/*
		 * A TERMINAL'S ROW NAMES THE ROLE IT WAS FILLING, when it was
		 * filling one. Its app id says only that it is a terminal, so
		 * a row carrying that reopens a bare shell and loses the file
		 * manager somebody had arranged — and the one thing the row
		 * must not carry instead is the command line. `files` is a
		 * name con.conf resolves, which is the same indirection the
		 * chord that opened it used.
		 */
		const char *role = w->kind == WIN_TERM
			? con_layout_role_of(w->prog) : NULL;

		field(a, sizeof(a), role ? role : w->app_id);
		field(t, sizeof(t), w->title);
		if (n + 256 >= cap)
			break;
		/*
		 * THE STATE A RECTANGLE CANNOT SAY. A fullscreen window's
		 * rectangle is the whole grid and a scratchpad's is its
		 * drop-down shape, so a row carrying only x y w h brings both
		 * back as ordinary windows the size they happened to be.
		 * A dash is the empty set, so the column is never empty and
		 * the one after it is never mistaken for it.
		 */
		char fl[8];
		int nf = 0;

		if (w->full)
			fl[nf++] = 'f';
		if (w->sticky)
			fl[nf++] = 's';
		if (!nf)
			fl[nf++] = '-';
		fl[nf] = '\0';

		n += (size_t)snprintf(buf + n, cap - n,
				      "%s\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
				      w->kind == WIN_TERM && !role
					      ? "term" : "app",
				      w->workspace, w->geom.x, w->geom.y,
				      w->geom.w, w->geom.h, a, fl, t);

		/*
		 * AND WHAT IT PRINTED, when the key says so. A separate file
		 * per terminal and a separate key, because this is the half
		 * that can mislead: old output above a fresh shell reads as
		 * live, and somebody scrolling up finds a build that never
		 * ran in this session. Off by default for that reason.
		 */
		if (w->kind == WIN_TERM && w->term && text)
			save_text(name, rows, w);
		rows++;
	}
	buf[n] = '\0';
	return rows;
}

int con_state_save(const char *name)
{
	char path[512], buf[8192];
	int rows;

	if (!con_state_path(name, path, sizeof(path)))
		return -1;

	/* THE DIRECTORY FIRST, because the per-terminal text files are written
	 * while the rows are rendered and the session file only after — made
	 * at the end, the first save would write its rows and lose every one
	 * of those. */
	char *slash = strrchr(path, '/');

	if (slash) {
		*slash = '\0';
		kb_mkdir_p(path);
		*slash = '/';
	}

	rows = con_state_rows(buf, sizeof(buf), name,
			      kcon_conf_bool("restore_scrollback", 0));
	if (rows < 0)
		return -1;

	/* ATOMIC, because a session that was killed part-way through writing
	 * this would be restored from half a file — and the half that survived
	 * would be the windows nobody could see the loss of. */
	if (kb_write_file_atomic(path, buf) != 0)
		return -1;
	return rows;
}

/*
 * ── THE RESTORE ─────────────────────────────────────────────────────────
 *
 * An application is started through its DESKTOP ENTRY by `app_id` — the same
 * path a launcher takes — and its window is placed when it attaches, because
 * a client's window does not exist until it does. The pending list below is
 * what carries the saved rectangle across that gap.
 *
 * A row whose entry is gone starts nothing and says so in the log: an
 * application that was uninstalled between two logins is not an error worth
 * refusing the whole restore over.
 */
static struct {
	char app[64];
	char flags[8];
	int ws, x, y, w, h;
	int used;
} pend[CON_STATE_MAX];
static int npend;

/* The saved place for an app_id, taken the FIRST time it attaches. A second
 * window of the same application is placed the ordinary way: two rows for one
 * app_id would otherwise both claim the first window. */
int con_state_take(const char *app_id, int *ws, int *x, int *y, int *w, int *h,
		   char *flags, size_t nflags)
{
	if (!app_id || !*app_id)
		return 0;
	for (int i = 0; i < npend; i++) {
		if (pend[i].used || strcmp(pend[i].app, app_id))
			continue;
		pend[i].used = 1;
		if (flags && nflags)
			snprintf(flags, nflags, "%s", pend[i].flags);
		if (ws)
			*ws = pend[i].ws;
		if (x)
			*x = pend[i].x;
		if (y)
			*y = pend[i].y;
		if (w)
			*w = pend[i].w;
		if (h)
			*h = pend[i].h;
		return 1;
	}
	return 0;
}

/*
 * THE FLAGS COLUMN OF A ROW, or "-". Eighth field, after the app id: read by
 * hand rather than in the scan above so that a row written before the column
 * existed still parses as a row with no flags rather than as no row at all.
 */
void con_state_flags(const char *line, char *out, size_t n)
{
	const char *p = line;

	snprintf(out, n, "-");
	for (int i = 0; i < 7; i++) {
		p = strchr(p, '\t');
		if (!p)
			return;
		p++;
	}

	const char *e = strchr(p, '\t');
	size_t len = e ? (size_t)(e - p) : strlen(p);

	if (!len || len >= n)
		return;
	memcpy(out, p, len);
	out[len] = '\0';
}

/*
 * WHAT A RECTANGLE CANNOT SAY, put back. Applied AFTER the placement, because
 * both of these replace the rectangle rather than adjust it: a fullscreen
 * window is the whole grid and a scratchpad is its own drop-down shape.
 */
void con_state_apply_flags(Win *w, const char *flags)
{
	if (!w || !flags)
		return;
	for (const char *p = flags; *p; p++) {
		if (*p == 'f')
			win_fullscreen(w);
		else if (*p == 's')
			win_scratch_mark(w);
	}
}

/*
 * A TERMINAL, AT THE GEOMETRY IT HAD, RUNNING WHAT ITS ROW NAMES.
 *
 * `cmd` came out of the one row resolver, so it is either `con.conf`'s own
 * `terminal` key or the key for the role the row names — never a command line
 * out of the file. A state file that named an argv would be a state file that
 * executes one, and it is written by a program into a directory anything
 * running as this person can write.
 */
static void restore_term(const char *name, int row, int ws, int x, int y,
			 int w, int h, const char *flags, const char *cmd)
{
	char store[512], tp[512];
	const char *av[16];
	int n = kxdg_exec_split(cmd, NULL, 0, store, sizeof(store), av, 16);
	Win *win;

	if (n <= 0)
		return;
	av[n] = NULL;
	win = term_open(av);
	if (!win)
		return;
	win->workspace = ws;
	win_place_at(win, x, y, w, h);
	con_state_apply_flags(win, flags);

	if (!kcon_conf_bool("restore_scrollback", 0))
		return;
	if (!text_path(name, row, tp, sizeof(tp)))
		return;

	char *txt = kb_read_whole(tp, NULL);

	if (!txt)
		return;

	/*
	 * SAID TO BE THE LAST SESSION'S, in the terminal itself. Old output
	 * above a fresh prompt is indistinguishable from live output, and
	 * somebody scrolling up finds a build that never ran here — the line
	 * below is what makes the difference visible where the confusion
	 * would happen. It goes in before the text so the text is under it.
	 */
	static const char mark[] =
		"── the previous session's output ──\r\n";

	kvt_term_show(win->term, mark, sizeof(mark) - 1);
	for (char *p = txt; *p; p++) {
		/* The file holds newlines; a terminal needs both halves of
		 * one, or every line starts where the last one ended. */
		if (*p == '\n')
			kvt_term_show(win->term, "\r\n", 2);
		else
			kvt_term_show(win->term, p, 1);
	}
	free(txt);
}

int con_state_restore(const char *name)
{
	char path[512], *text;
	int opened = 0;

	npend = 0;
	if (!con_state_path(name, path, sizeof(path)))
		return 0;
	text = kb_read_whole(path, NULL);
	if (!text)
		return 0;

	for (char *line = text, *nl; line && *line; line = nl) {
		char kind[16], app[64], fl[8] = "-";
		int ws, x, y, w, h;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		if (*line == '#' || !*line)
			continue;
		if (sscanf(line, "%15[^\t]\t%d\t%d\t%d\t%d\t%d\t%63[^\t]",
			   kind, &ws, &x, &y, &w, &h, app) != 7)
			continue;
		if (w < 1 || h < 1 || ws < 0)
			continue;
		/* THE FLAGS COLUMN IS OPTIONAL, read separately rather than
		 * added to the scan above: a file written before it existed
		 * still restores, as no flags at all. */
		con_state_flags(line, fl, sizeof(fl));

		/* THROUGH THE ONE ROW RESOLVER, so a restored session and a
		 * loaded layout cannot disagree about what a row means. */
		const char *cmd = NULL;
		int what = con_layout_resolve(kind, app, &cmd);

		if (what == CON_ROW_TERM || what == CON_ROW_ROLE) {
			restore_term(name, opened, ws, x, y, w, h, fl, cmd);
			opened++;
			continue;
		}
		if (what == CON_ROW_SURFACE) {
			con_spawn_at(cmd, -1);
			opened++;
			continue;
		}
		if (what != CON_ROW_APP)
			continue;

		/* THE ENTRY, NOT A COMMAND. `kdos-appbox run` is what "start
		 * this application" means here, and it resolves the id the
		 * same way the launcher and the menu do. */
		if (npend < CON_STATE_MAX) {
			snprintf(pend[npend].app, sizeof(pend[npend].app),
				 "%s", app);
			snprintf(pend[npend].flags, sizeof(pend[npend].flags),
				 "%s", fl);
			pend[npend].ws = ws;
			pend[npend].x = x;
			pend[npend].y = y;
			pend[npend].w = w;
			pend[npend].h = h;
			pend[npend].used = 0;
			npend++;
		}

		KbArgv a = { 0 };

		kb_argv_add(&a, "kdos-appbox");
		kb_argv_add(&a, "run");
		kb_argv_add(&a, app);
		kb_argv_end(&a);
		kb_run_detach(&a);
		opened++;
	}

	free(text);
	return opened;
}
