/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — an arrangement of windows, with a name
 *
 * A LAYOUT IS THE SESSION RECORD WITH A NAME. Same rows, same columns, same
 * reader — `state.c` writes what a session had open when it ended, and this
 * writes the same thing when somebody asks for it, under a name they chose.
 * Two formats for one idea would be two things to keep in step.
 *
 * WHAT SEPARATES A LOAD FROM A RESTORE is three rules, and each is why this
 * file exists rather than a second argument to `con_state_restore()`:
 *
 *   IT CLOSES NOTHING AND OPENS ONLY WHAT IS MISSING. A restore runs into an
 *   empty session; a load runs into whatever a person is already doing. Asked
 *   for `work` twice, the second press must not open a second file manager.
 *
 *   IT OPENS ONLY WHAT IS INSTALLED. A shipped layout names seven programs and
 *   no image carries all of them; a row whose program is absent is skipped in
 *   silence, because a layout that refused to load at all would be a layout
 *   nobody could use.
 *
 *   AND ITS ROWS NAME ROLES. This is the whole of why a layout can hold `mc`
 *   at all: every terminal window's app id is the literal `terminal`, so the
 *   session record cannot say WHICH program a terminal was running — and a
 *   file that named the program would be a file naming an argv, which is the
 *   one thing the record refuses. A row names `files` and `con.conf` names the
 *   file manager, the same indirection the chords keep.
 *
 * WHERE THEY LIVE: `~/.config/kdos-con/layouts/<name>` first, then
 * `/usr/share/kdos/layouts/<name>`. A person's own copy of a shipped name
 * replaces it rather than merging with it — an arrangement is a whole
 * statement about a screen, and half of one is not an arrangement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kcon.h"
#include "kxdg.h"
#include "con.h"

#define LAYOUT_SHARED "/usr/share/kdos/layouts"

/*
 * A NAME BECOMES A FILE NAME, so it decides a path. A slash or a dot-dot in
 * one would read and write outside the directory the person meant, and the
 * name arrives over a socket.
 */
static int name_ok(const char *name)
{
	if (!name || !*name || strlen(name) > 63)
		return 0;
	for (const char *p = name; *p; p++)
		if (*p == '/' || *p == '\\' || *p == ':' ||
		    (unsigned char)*p <= ' ')
			return 0;
	return strcmp(name, ".") && strcmp(name, "..");
}

/* The user's copy, which is also the only one written to. */
static int layout_home(const char *name, char *out, size_t n)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (!name_ok(name))
		return 0;
	if (xdg && *xdg)
		return snprintf(out, n, "%s/kdos-con/layouts/%s", xdg, name)
		       < (int)n;
	if (home && *home)
		return snprintf(out, n, "%s/.config/kdos-con/layouts/%s", home,
				name) < (int)n;
	return 0;
}

/*
 * ── WHAT A ROW NAMES ────────────────────────────────────────────────────
 *
 * Four answers, in this order, and the order is the point: a row is resolved
 * by asking the tables that already exist, most specific first.
 */
int con_layout_resolve(const char *kind, const char *app, const char **cmd)
{
	*cmd = NULL;
	/* A `term` row is a terminal and says nothing about what ran in it —
	 * which is the record's own rule, kept here. */
	if (!strcmp(kind, "term")) {
		*cmd = kcon_conf_str("terminal", "sh");
		return CON_ROW_TERM;
	}
	if (strcmp(kind, "app") || !app[0] || !strcmp(app, "-"))
		return CON_ROW_NONE;

	/* A ROLE: `files`, `mail`, `writing`… The row names the role and
	 * con.conf names the program, so a layout survives somebody changing
	 * their editor. Every one of these runs in a terminal, which is what
	 * makes one branch enough. */
	for (int i = 0; i < CON_APP_N; i++) {
		const char *role = con_app_name(i);

		if (role && !strcmp(role, app)) {
			*cmd = con_app(i);
			return CON_ROW_ROLE;
		}
	}

	/* A SURFACE OF THIS DESKTOP'S OWN: `monitor`, `notes`, `calculator`.
	 * These have no desktop entry — they are programs this tree ships and
	 * con.conf names — so the entry path below would find nothing. */
	for (int i = 0; i < CON_CMD_N; i++) {
		const char *key = con_command_name(i);

		if (key && !strcmp(key, app)) {
			*cmd = con_command(i);
			return CON_ROW_SURFACE;
		}
	}

	/* ANYTHING ELSE IS AN APP ID, started through its desktop entry the
	 * way the launcher and the menu start one. */
	return CON_ROW_APP;
}

/*
 * THE ROLE A PROGRAM FILLS, or NULL. The inverse of the lookup above, and the
 * reason a saved arrangement reopens the file manager rather than a bare
 * shell: a terminal window's app id is the literal `terminal`, so without this
 * the writer has no name for what was running in it — and the one name it must
 * not write is the command line.
 */
const char *con_layout_role_of(const char *prog)
{
	if (!prog || !*prog)
		return NULL;
	for (int i = 0; i < CON_APP_N; i++) {
		const char *cmd = con_app(i);
		char base[64];
		size_t n;

		if (!cmd || !*cmd)
			continue;
		n = strcspn(cmd, " \t");
		if (n >= sizeof(base))
			n = sizeof(base) - 1;
		memcpy(base, cmd, n);
		base[n] = '\0';
		if (!strcmp(kb_basename(base), prog))
			return con_app_name(i);
	}
	return NULL;
}

/*
 * IS IT ALREADY OPEN? A load adds to what is there, so this is the test that
 * makes a second press do nothing.
 *
 * A ROLE AND A SURFACE ARE MATCHED BY `prog` and an app id by `app_id`,
 * because those are the two fields that mean "which program" for the two
 * kinds of window. A `term` row matches nothing: a person asking for a layout
 * with two terminals in it means two terminals, and there is no name that
 * separates one plain shell from another.
 */
static int already_open(int what, const char *cmd, const char *app)
{
	char base[64];

	if (what == CON_ROW_TERM)
		return 0;
	if (what == CON_ROW_APP) {
		for (Win *w = S.wins; w; w = w->next)
			if (!w->panel && !w->overlay && !w->background &&
			    !strcmp(w->app_id, app))
				return 1;
		return 0;
	}
	/* The program a role or a surface resolves to, matched the way
	 * run-or-raise matches it: the first word, without its path. */
	if (!cmd || !*cmd)
		return 0;

	size_t n = strcspn(cmd, " \t");

	if (n >= sizeof(base))
		n = sizeof(base) - 1;
	memcpy(base, cmd, n);
	base[n] = '\0';
	return win_find_prog(kb_basename(base), 0) != NULL;
}

/* Whether the image carries it at all. The first word of the value, because
 * that is the program and the rest is its arguments. */
static int installed(const char *cmd)
{
	char base[64];
	size_t n;

	if (!cmd || !*cmd)
		return 0;
	n = strcspn(cmd, " \t");
	if (n >= sizeof(base))
		n = sizeof(base) - 1;
	memcpy(base, cmd, n);
	base[n] = '\0';
	return kb_have_prog(base);
}

/*
 * A ROLE, IN A TERMINAL, AT THE RECTANGLE THE ROW NAMES. The same thing the
 * chord does — kxdg_exec_split then term_open — because a role is a program
 * that runs in a terminal and there is only one way to open one of those.
 */
static Win *open_in_term(const char *cmd)
{
	char store[512];
	const char *av[16];
	int n = kxdg_exec_split(cmd, NULL, 0, store, sizeof(store), av, 16);

	if (n <= 0)
		return NULL;
	av[n] = NULL;
	return term_open(av);
}

int con_layout_save(const char *name)
{
	char path[512], buf[8192];
	int rows;

	if (!name_ok(name) || !layout_home(name, path, sizeof(path)))
		return -1;

	/*
	 * THE ROWS ARE RENDERED, NOT COPIED OUT OF THE SESSION FILE. That file
	 * is written once, on a clean exit, and only when `restore` is on — so
	 * reaching for it here would make `layout save` depend on a key that
	 * has nothing to do with it, and would write a session file as a side
	 * effect of somebody naming an arrangement.
	 *
	 * NO PER-TERMINAL TEXT. A layout is an arrangement and not a
	 * transcript: what was on the screen belongs to the session that had
	 * it, and copying it into a named layout would put one afternoon's
	 * output into every future use of the name.
	 */
	rows = con_state_rows(buf, sizeof(buf), name, 0);
	if (rows < 0)
		return -1;

	char *slash = strrchr(path, '/');

	if (slash) {
		*slash = '\0';
		kb_mkdir_p(path);
		*slash = '/';
	}
	return kb_write_file_atomic(path, buf) == 0 ? rows : -1;
}

int con_layout_load(const char *name)
{
	char path[512], *text;
	int opened = 0;

	if (!name_ok(name))
		return -1;
	text = NULL;
	if (layout_home(name, path, sizeof(path)))
		text = kb_read_whole(path, NULL);
	if (!text) {
		snprintf(path, sizeof(path), "%s/%s", LAYOUT_SHARED, name);
		text = kb_read_whole(path, NULL);
	}
	if (!text)
		return -1;

	for (char *line = text, *nl; line && *line; line = nl) {
		char kind[16], app[64], fl[8];
		int ws, x, y, w, h;
		const char *cmd;
		Win *win = NULL;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		if (*line == '#' || !*line)
			continue;
		if (sscanf(line, "%15[^\t]\t%d\t%d\t%d\t%d\t%d\t%63[^\t]",
			   kind, &ws, &x, &y, &w, &h, app) != 7)
			continue;
		if (w < 1 || h < 1 || ws < 0 || ws >= S.nworkspace)
			continue;
		con_state_flags(line, fl, sizeof(fl));

		int what = con_layout_resolve(kind, app, &cmd);

		if (what == CON_ROW_NONE)
			continue;
		if (what != CON_ROW_APP && !installed(cmd))
			continue;	/* not on this image; not an error */
		if (already_open(what, cmd, app))
			continue;

		switch (what) {
		case CON_ROW_TERM:
		case CON_ROW_ROLE:
			win = open_in_term(cmd);
			break;
		case CON_ROW_SURFACE:
			/*
			 * SPAWNED, NOT PLACED HERE. A surface of this
			 * desktop's own is a client: its window does not exist
			 * until it attaches, so the rectangle is left for the
			 * adopt path the same way a restored application's is.
			 */
			con_spawn_at(cmd, -1);
			opened++;
			continue;
		default:
			/* THE ENTRY, NOT A COMMAND — `kdos-appbox run` is what
			 * "start this application" means here, and it resolves
			 * the id the way the launcher does. */
			{
				KbArgv a = { 0 };

				kb_argv_add(&a, "kdos-appbox");
				kb_argv_add(&a, "run");
				kb_argv_add(&a, app);
				kb_argv_end(&a);
				kb_run_detach(&a);
			}
			opened++;
			continue;
		}

		if (!win)
			continue;
		win->workspace = ws;
		win_place_at(win, x, y, w, h);
		con_state_apply_flags(win, fl);
		opened++;
	}

	free(text);
	ktui_draw_invalidate();
	return opened;
}
