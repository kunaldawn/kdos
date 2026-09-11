/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — windows reopen where you left them
 *
 * A window's rectangle is written when it goes and used when a window running
 * the same program next appears, so an application does not open in the middle
 * of the screen at the size its author picked every single time.
 *
 * THE KEY IS `prog`, NOT `app_id`. Every WIN_TERM's app id is `terminal` and
 * every caged guest's is `kdos-cage` — an app id says what KIND of window this
 * is — so a table keyed on one would give every terminal on the desk a single
 * shared rectangle. `prog` is the program the window was opened for, written
 * once and never rewritten by the guest, and for a native surface it is the
 * client's own name. An empty one is remembered for nothing.
 *
 * THE RECORD IS PER PROGRAM AND PER WORKSPACE. The same editor on workspace 1
 * and on workspace 3 is two windows a person arranged separately, and one line
 * for both would make each opening move the other.
 *
 * THIS IS NOT THE COMPOSITOR'S FILE, AND IT CANNOT BE. `kdos-comp` keeps the
 * same idea in `$XDG_STATE_HOME/kdos/winpos`, but ITS RECTANGLES ARE PIXELS
 * AND THESE ARE CELLS: one file with both writers would restore every window
 * at a size taken from the other desktop's units — an eighty-column terminal
 * coming back eighty pixels wide. What the two share is the rule, not the row.
 *
 * AND IT IS NOT SESSION RESTORE. That file is a list of what was open, read
 * once at login and spent; this one is a rectangle per program with no
 * lifetime at all. A restored window is placed from the session record and
 * never reaches here, which is why the two cannot disagree.
 *
 * `remember = no` in con.conf turns it off, in both directions: a person who
 * does not want the file does not want it written either.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kbase.h"
#include "con.h"

/*
 * The most recent this many programs. An unbounded history of every program
 * ever run is a file nobody can read and a scan on every window that opens;
 * the cap drops what has been left alone longest, because a rectangle nobody
 * has arranged in months is not one they are waiting for.
 */
#define GEO_MAX 200

typedef struct {
	char prog[64];
	int workspace;
	int x, y, w, h;
	unsigned tiled;
} Geo;

/* Most recent first, so the cap drops from the end. */
static Geo rec[GEO_MAX];
static int nrec;

static int geo_enabled(void)
{
	return kcon_conf_bool("remember", 1);
}

static int geo_path(char *out, size_t n)
{
	return kb_state_path("kdos/con/geometry", out, n);
}

/*
 * A PROGRAM NAME WITH A TAB OR A NEWLINE IN IT CANNOT ROUND-TRIP through a
 * line of this file, and no real one has either. Refusing here is what keeps
 * a malformed name from writing a second row rather than a broken one.
 */
static int geo_usable(const char *prog)
{
	if (!prog || !*prog)
		return 0;
	for (const char *p = prog; *p; p++)
		if ((unsigned char)*p <= ' ')
			return 0;
	return 1;
}

/*
 * READ AGAIN EVERY TIME, never cached for the life of the session.
 *
 * ONE PERSON MAY HAVE TWO SESSIONS and they share this file — the question it
 * answers is where a program's window goes, which belongs to the person rather
 * than to a session name. A list held from startup and written back whole
 * would make each session erase whatever the other had learned since it
 * started. Re-reading costs a two-hundred-line file per window opened or
 * closed, which is not a rate anything here happens at.
 */
static void geo_load(void)
{
	char path[512];
	char *text;

	nrec = 0;
	if (!geo_path(path, sizeof(path)))
		return;
	text = kb_read_whole(path, NULL);
	if (!text)
		return;		/* absent is the normal case on a fresh account */

	for (char *line = text, *nl; line && *line && nrec < GEO_MAX;
	     line = nl) {
		Geo g = { { 0 }, 0, 0, 0, 0, 0, 0 };
		int ws, x, y, w, h;
		unsigned t;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		if (*line == '#' || !*line)
			continue;
		if (sscanf(line, "%63[^\t]\t%d\t%d %d %d %d\t%u", g.prog, &ws,
			   &x, &y, &w, &h, &t) != 7)
			continue;	/* a line that does not parse is absent */
		/* A rectangle with no area is how a window comes back one cell
		 * wide, so it is dropped rather than clamped up to something
		 * nobody chose. */
		if (w < 1 || h < 1 || ws < 0)
			continue;
		g.workspace = ws;
		g.x = x;
		g.y = y;
		g.w = w;
		g.h = h;
		g.tiled = t;
		rec[nrec++] = g;
	}
	free(text);
}

static Geo *geo_find(const char *prog, int workspace)
{
	for (int i = 0; i < nrec; i++)
		if (rec[i].workspace == workspace &&
		    !strcmp(rec[i].prog, prog))
			return &rec[i];
	return NULL;
}

static void geo_save(void)
{
	char path[512];
	char *buf;
	size_t cap = (size_t)nrec * 128 + 256, n = 0;

	if (!geo_path(path, sizeof(path)))
		return;
	/* THE DIRECTORY FIRST. The atomic replace opens a temp file beside the
	 * target, so on a fresh account — the one case this has to work on —
	 * there is nothing for it to open beside. */
	char *slash = strrchr(path, '/');

	if (slash) {
		*slash = '\0';
		kb_mkdir_p(path);
		*slash = '/';
	}

	buf = malloc(cap);
	if (!buf)
		return;
	n += (size_t)snprintf(buf + n, cap - n,
			      "# kdos-con — where a program's window was, in cells\n"
			      "# prog\tworkspace\tx y w h\ttiled\n");
	for (int i = 0; i < nrec && n < cap; i++)
		n += (size_t)snprintf(buf + n, cap - n, "%s\t%d\t%d %d %d %d\t%u\n",
				      rec[i].prog, rec[i].workspace, rec[i].x,
				      rec[i].y, rec[i].w, rec[i].h,
				      rec[i].tiled);
	/*
	 * WHOLE OR NOT AT ALL. Half a record that still parses is exactly how
	 * a window comes back one cell wide, and the directory this lands in
	 * is the one a crash is most likely to interrupt.
	 */
	if (kb_write_file_atomic(path, buf) == 0)
		/* 0600, the mode the compositor's `winpos` uses for the same
		 * content: a list of every program somebody runs and where
		 * they keep it is nobody else's business. */
		chmod(path, 0600);
	free(buf);
}

/*
 * WHAT IS WORTH REMEMBERING, and every exclusion here is a role rather than a
 * flag somebody set:
 *
 *   CHROME IS NOT A WINDOW. A panel is docked, a layer is a menu or a toast,
 *   the lock and the saver cover the grid; none is something a person places.
 *
 *   A GUEST ON ANOTHER TERMINAL OWNS NO CELLS on this one, so it has no
 *   rectangle to keep.
 *
 *   THE SCRATCHPAD HAS A SHAPE OF ITS OWN, applied on every show. A remembered
 *   rectangle would be overwritten by it on the way in and would overwrite the
 *   next ordinary window of the same program on the way out.
 *
 *   AND A TILED WINDOW IS REMEMBERED BY ITS RESTORE RECTANGLE, not by the half
 *   of the screen it is currently filling: what an untile returns to is what
 *   the person chose, and the tile itself comes back from `tiled`.
 */
static int geo_worth(const Win *w)
{
	if (!w || w->panel || w->overlay || w->background || w->sticky)
		return 0;
	/* A FLOAT ASKED TO OPEN IN THE MIDDLE AT A SIZE OF ITS OWN, and
	 * remembering where it happened to be moved to would answer a
	 * different question the next time it is opened — and answer it
	 * first, because a recalled rectangle wins over a placement. */
	if (w->floating)
		return 0;
	if (w == S.lock || w == S.saver || w->kind == WIN_VT)
		return 0;
	return geo_usable(w->prog);
}

void geo_record(const Win *w)
{
	Geo *g;
	KwmRect r;

	if (!geo_enabled() || !geo_worth(w))
		return;
	geo_load();

	r = w->tiled ? w->restore : w->geom;
	if (r.w < 1 || r.h < 1)
		return;

	/*
	 * TO THE FRONT, whether it is new or already here. The cap drops from
	 * the end, so the list has to be in the order things were last closed
	 * — and one shift covers both cases: the row that was at `at` is the
	 * one overwritten, which for an existing record is itself.
	 */
	g = geo_find(w->prog, w->workspace);
	int at = g ? (int)(g - rec) : -1;

	if (at < 0) {
		if (nrec < GEO_MAX)
			nrec++;
		at = nrec - 1;
	}
	for (int i = at; i > 0; i--)
		rec[i] = rec[i - 1];
	memset(&rec[0], 0, sizeof(rec[0]));
	kb_strlcpy(rec[0].prog, w->prog, sizeof(rec[0].prog));
	rec[0].workspace = w->workspace;
	rec[0].x = r.x;
	rec[0].y = r.y;
	rec[0].w = r.w;
	rec[0].h = r.h;
	rec[0].tiled = w->tiled;
	geo_save();
}

/*
 * A SECOND WINDOW OF THE SAME PROGRAM MUST NOT LAND ON THE FIRST. One record
 * per program means every instance would take the same corner and the second
 * would open exactly on top of the first, with the placement search — which
 * exists precisely to avoid that — overwritten before it could run. The origin
 * is the test: a second window of a different size in the same corner reads as
 * just as broken as an identical one.
 */
static int geo_origin_taken(const Win *w, int x, int y)
{
	for (Win *o = S.wins; o; o = o->next) {
		if (o == w || o->minimised || o->hidden ||
		    o->workspace != w->workspace || strcmp(o->prog, w->prog))
			continue;
		if (o->geom.x == x && o->geom.y == y)
			return 1;
	}
	return 0;
}

int geo_recall(Win *w)
{
	Geo *g;
	KwmRect area;
	KwmRect r;

	if (!geo_enabled() || !geo_worth(w))
		return 0;
	geo_load();
	g = geo_find(w->prog, w->workspace);
	if (!g)
		return 0;

	/*
	 * THROUGH kwm_fit, INTO THE WORK AREA. A rectangle remembered from a
	 * wide screen must still come back onto a narrow one, and the window's
	 * own minimum is applied in the one place that knows it.
	 */
	area = win_workarea();
	r.x = g->x;
	r.y = g->y;
	r.w = g->w;
	r.h = g->h;
	r = kwm_fit(r, area, w->min_w, w->min_h);
	if (r.w < 1 || r.h < 1)
		return 0;
	if (geo_origin_taken(w, r.x, r.y))
		return 0;	/* the placement search has the better answer */

	w->geom = r;
	w->tiled = g->tiled;
	/* An untile has to return somewhere, and for a window that arrived
	 * tiled the only rectangle anybody chose is this one. */
	w->restore = r;
	if (w->tiled)
		w->geom = win_tile_rect(w->tiled);
	return 1;
}
