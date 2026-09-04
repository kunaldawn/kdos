/* kdos-con — the session server. See con.h.
 *
 * ONE BINARY, THREE NAMES, dispatched on argv[0] as ksvc and kdos-appbox
 * already are:
 *
 *   kdos-con        the session, which a view attaches to
 *   kdos-grid       start a session HERE and attach a view to it
 *   kdos-con-login  read con.conf and either autologin or greet
 */

#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "con.h"
#include "kbase.h"
#include "kcolor.h"
#include "kxdg.h"

static int quit;

void con_quit(void)
{
	quit = 1;
	/* A guest holds a terminal this session allocated. Leaving one behind
	 * is leaving a VT nobody can get back without a reboot. */
	vt_close_all();
	/* An embedded guest's compositor is reached only through a descriptor
	 * this process holds, so one left running is one nothing can talk to. */
	embed_close_all();
}

static void usage(FILE *f)
{
	fprintf(f,
"kdos-con — the console session\n"
"\n"
"  --serve            hold the session and wait for a view to attach\n"
"  --new  [-t NAME]   start a session; --ls lists them\n"
"  --attach [-t NAME] put a view on one\n"
"  --detach [-t NAME] take every view off one, leaving it running\n"
"  --kill   [-t NAME] end one\n"
"  --greet            the login surface, as kdos-con-login\n"
"  --keys             the chord table after keys.conf, one action and chord\n"
"                     per line — what the key card reads on this desktop\n"
"  --dump COLSxROWS   composite one frame and write it as cells\n"
"  --run [--bare] -- CMD...\n"
"                     run a graphical program. It becomes a window; the\n"
"                     terminal it was given is printed, or 0 for a window.\n"
"                     --bare when the program IS a compositor, so it is not\n"
"                     put inside one — those always take a terminal\n"
"  --term CMD         open a terminal window running CMD; repeatable. The\n"
"                     command is split the way a desktop entry is, so\n"
"                     quoting works and no shell is involved\n"
"  --socket PATH      where surfaces attach\n"
"  --help\n");
}

/*
 * Everything a session needs before anything is drawn. The grid size is the
 * view's to decide; until one attaches, --dump supplies it.
 */
static void session_init(int cols, int rows)
{
	memset(&S, 0, sizeof(S));
	S.cols = cols;
	S.rows = rows;
	/* con.conf, clamped: nine is the last digit Super can reach, so a
	 * tenth workspace would exist with no way to get to it. */
	S.nworkspace = kcon_conf_int("sessions", 4);
	if (S.nworkspace < 1)
		S.nworkspace = 1;
	if (S.nworkspace > 9)
		S.nworkspace = 9;
	S.gap = 0;
	S.next_id = 0;
}

/*
 * EVERY WINDOW, ASKED AND THEN TAKEN OUT.
 *
 * win_close() leaves an entry standing until the program behind it is actually
 * gone, which is right while the session is running and is a loop with no end
 * once it is not: nothing is left to reap. So each window is asked once, and
 * taken out whether or not it went.
 */
static void teardown(void)
{
	while (S.wins) {
		Win *w = S.wins;

		win_close(w);
		if (S.wins == w)
			win_drop(w);
	}
}

static void composite(void)
{
	KRect all = krect(0, 0, S.cols, S.rows);

	/* The desktop itself. A backdrop, not a wallpaper: a picture is the
	 * shell's business and this is what a session with no shell shows. */
	ktui_draw_fill(all, KT_BG);

	win_draw_all();

	/* NOT WHILE LOCKED, AND NOT UNDER A SAVER. win_draw_all() draws one of
	 * those and nothing else, and a taskbar painted after it would list the
	 * windows — their titles included — across a screen that is supposed to
	 * show none. */
	if (!S.locked && !S.saver)
		panel_draw();
}

/*
 * Run every terminal until its child has finished and its output has been
 * consumed, then composite once. This is what makes a dump reproducible: a
 * frame taken while a program is still writing is a different frame every
 * time it is taken.
 */
static void settle(void)
{
	for (int spin = 0; spin < 4000; spin++) {
		int live = 0;
		struct pollfd p[32];
		int n = 0;

		term_pump_all();

		for (Win *w = S.wins; w && n < 32; w = w->next) {
			if (w->kind != WIN_TERM || !w->term)
				continue;
			if (kvt_term_alive(w->term))
				live = 1;
			p[n].fd = kvt_term_fd(w->term);
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		if (!n)
			return;

		int r = poll(p, (nfds_t)n, live ? 5 : 1);

		if (r <= 0 && !live) {
			/* Nothing running and nothing waiting: one more pump so
			 * the last write is in the screen, then done. */
			term_pump_all();
			return;
		}
	}
}

/* ── the live session ────────────────────────────────────────────────────
 *
 * kdos-con draws through libktui like everything else; its backend is where
 * the composited grid LEAVES for whatever is displaying it. There is no
 * screen here and never will be.
 * ──────────────────────────────────────────────────────────────────────── */

static KtuiEvent evq[128];
static int evhead, evtail;

/* Every view's input arrives through ev_push, which is why the idle timer is
 * reset there and in exactly one other place: nowhere. */
static void idle_poke(void);

static void ev_push(const KtuiEvent *e)
{
	int next = (evtail + 1) % (int)(sizeof(evq) / sizeof(evq[0]));

	idle_poke();

	if (next == evhead)
		return;		/* full: the session is behind, drop the newest */
	evq[evtail] = *e;
	evtail = next;
}

static int ev_pop(KtuiEvent *e)
{
	if (evhead == evtail)
		return 0;
	*e = evq[evhead];
	evhead = (evhead + 1) % (int)(sizeof(evq) / sizeof(evq[0]));
	return 1;
}

static void on_view_key(KconSurface *v, int key, int mods, void *user)
{
	KtuiEvent e;

	(void)v;
	(void)user;
	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_KEY;
	e.key = key;
	e.mods = mods;
	ev_push(&e);
}

static void on_view_ptr(KconSurface *v, int x, int y, int subx, int suby,
			int btn, int press, void *user)
{
	KtuiEvent e;

	(void)v;
	(void)user;
	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_MOUSE;
	e.mx = x;
	e.my = y;
	/* Carried, not acted on: everything drawn in cells is pointed at a
	 * cell at a time, and the one thing that is not is an embedded pixel
	 * guest. */
	e.subx = subx - 128;
	e.suby = suby - 128;
	e.btn = btn;
	e.press = press;
	ev_push(&e);
}

/*
 * THE SESSION GRID IS THE PRIMARY VIEW'S — the first one to attach. A second
 * view of a different size letterboxes rather than resizing every window out
 * from under whoever is using them.
 */
static void con_size(int *w, int *h)
{
	KconSurface *v = S.server ? kcon_server_view_at(S.server, 0) : NULL;

	if (v && kcon_view_cols(v) > 0) {
		*w = kcon_view_cols(v);
		*h = kcon_view_rows(v);
		return;
	}

	*w = S.cols > 0 ? S.cols : 80;
	*h = S.rows > 0 ? S.rows : 24;
}

/*
 * WHERE THE CARET IS, told to every view.
 *
 * A view holds no window state, so it cannot know where the text cursor of the
 * focused window is — and a display that does not know cannot put a real
 * cursor there. On a `--tty` view that costs a person the one thing their own
 * terminal could have shown them: a blinking cursor at the place they are
 * typing, drawn by the terminal they are sitting at rather than by a cell this
 * desktop painted.
 *
 * ONLY THE SESSION'S OWN TERMINALS REPORT ONE so far. A `libkcon` surface
 * knows its caret and has no message to say so, which is the other half of
 * this and is not pretended at here: a surface with the focus reports nothing
 * and the caret stays where the last terminal put it rather than moving to a
 * position nobody sent.
 */
static void publish_caret(void)
{
	static int last_x = -1, last_y = -1;
	Win *w = win_focused();
	int x = -1, y = -1;

	if (w && w->kind == WIN_TERM && w->term && !S.locked && !S.saver) {
		struct kvt_screen *sc = kvt_term_screen(w->term);

		if (sc) {
			x = w->geom.x + (int)kvt_screen_get_cursor_x(sc);
			y = w->geom.y + (int)kvt_screen_get_cursor_y(sc);
		}
	}

	if (x == last_x && y == last_y)
		return;
	last_x = x;
	last_y = y;

	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_cursor(kcon_server_view_at(S.server, i), x, y);
}

static void con_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full)
{
	/*
	 * libktui's `prev` is ignored on purpose: it is ONE previous frame and
	 * there may be several views, each of which has seen a different
	 * amount. Every view diffs against its own.
	 */
	(void)prev;
	(void)force_full;

	if (!S.server)
		return;

	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_send(kcon_server_view_at(S.server, i), cur, w, h);
}

static int con_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)timeout_ms;
	if (ev_pop(ev))
		return 1;
	ev->type = KT_EVT_TICK;
	return 0;
}

static int con_caps(void)
{
	return KT_CAP_TRUECOLOR | KT_CAP_UTF8 | KT_CAP_MOUSE;
}

static const KtuiBackend con_backend = {
	.name = "session",
	.flush = con_flush,
	.poll_event = con_poll,
	.size = con_size,
	.caps = con_caps,
};

/*
 * The programs a chord starts. Each is a con.conf key with a default, so an
 * image that ships a different launcher changes one line and every chord that
 * reaches it follows.
 */
const char *con_command(int which)
{
	static const struct { const char *key, *def; } cmd[CON_CMD_N] = {
		[CON_CMD_MENU]     = { "menu",     "kdos-start" },
		[CON_CMD_LAUNCHER] = { "launcher", "kdos-launcher" },
		[CON_CMD_LOCK]     = { "lock",     "kdos-lock" },
		[CON_CMD_SAVER]    = { "saver",    "kdos-saver" },
	};

	if (which < 0 || which >= CON_CMD_N)
		return NULL;
	return kcon_conf_str(cmd[which].key, cmd[which].def);
}

/*
 * Start a program that will attach as a surface of its own. Double-forked, so
 * the session never has to reap it: a desktop that waited on its children
 * would stop drawing while a launcher was open, and one that did not wait
 * would fill its process table with zombies over a day's use.
 *
 * NO SHELL. The command is split into an argument vector, which is the only
 * way anything is executed in this tree.
 */
void con_spawn(const char *cmd)
{
	char store[512];
	const char *av[16];
	int n;

	if (!cmd || !*cmd)
		return;
	n = kxdg_exec_split(cmd, NULL, 0, store, sizeof(store), av, 16);
	if (n <= 0)
		return;
	av[n] = NULL;

	pid_t p = fork();

	if (p == 0) {
		if (fork() == 0) {
			execvp(av[0], (char *const *)av);
			_exit(127);
		}
		_exit(0);
	}
	if (p > 0)
		waitpid(p, NULL, 0);
}

/*
 * The chords the desktop keeps for itself, nearly all on KT_MOD_SUPER so none
 * of them can collide with what a program inside a window wants. WHICH chord
 * runs which action is keys.c's; this is only what the actions do.
 *
 * A backend that cannot report Super leaves those chords unreachable rather
 * than stealing a key.
 */
/*
 * THE LEADER HAS BEEN PRESSED AND THE NEXT KEY IS A CHORD.
 *
 * One key deep and no timeout: a prefix that expired would fire a chord or a
 * literal depending on how fast somebody typed, and a person who pressed it by
 * mistake presses any key that names nothing to be rid of it.
 */
static int leader_armed;

static int session_key(const KtuiEvent *ev)
{
	Win *w = win_focused();
	int arg;

	switch (keys_action(ev->key, ev->mods, &arg)) {
	case CON_ACT_LEADER:
		leader_armed = 1;
		return 1;
	case CON_ACT_TERM: {
		/* No shell and no system(): the command is split into an
		 * argument vector, which is the only way a program is started
		 * anywhere in this tree. */
		char store[512];
		const char *av[16];
		int n = kxdg_exec_split(kcon_conf_str("terminal", "sh"), NULL,
					0, store, sizeof(store), av, 16);

		if (n > 0) {
			av[n] = NULL;
			term_open(av);
		}
		return 1;
	}
	case CON_ACT_CLOSE:
		win_close(w);
		return 1;
	case CON_ACT_QUIT:
		con_quit();
		return 1;
	case CON_ACT_MAX:
		win_maximise(w);
		return 1;
	case CON_ACT_FULL:
		win_fullscreen(w);
		return 1;
	case CON_ACT_RESTORE:
		win_restore(win_last_minimised());
		return 1;
	case CON_ACT_MIN:
		win_minimise(w);
		return 1;
	case CON_ACT_EXEC:
		con_spawn(con_command(arg));
		return 1;
	case CON_ACT_NEXT:
		win_cycle(1);
		return 1;
	case CON_ACT_PREV:
		win_cycle(-1);
		return 1;
	case CON_ACT_SNAP:
		win_snap(w, (unsigned)arg, 1);
		return 1;
	case CON_ACT_WS:
		win_workspace(arg);
		return 1;
	case CON_ACT_FOCUS_DIR: {
		Win *t = win_dir((unsigned)arg);

		/* Raising is what focusing is here: the front of the list is
		 * the top of the stack, so a focus that did not raise would
		 * put the keyboard on a window still behind another one. */
		if (t)
			win_raise(t->id);
		return 1;
	}
	case CON_ACT_SWAP_DIR:
		win_swap(w, win_dir((unsigned)arg));
		return 1;
	case CON_ACT_WS_STEP:
		win_workspace_step(arg);
		return 1;
	case CON_ACT_SEND:
		win_send(w, arg);
		return 1;
	default:
		break;
	}

	return 0;
}

static void route_key(const KtuiEvent *ev)
{
	/*
	 * WHILE LOCKED, NOTHING ELSE HEARS A KEY — not a window, and not the
	 * session's own chords. A lock screen that still honoured Super+Return
	 * would open a terminal on a locked machine.
	 */
	if (S.locked) {
		if (S.lock && S.lock->surf)
			kcon_surface_key(S.lock->surf, ev->key, ev->mods);
		return;
	}

	if (leader_armed) {
		int arg;

		leader_armed = 0;
		/*
		 * THE LEADER TWICE IS THE LITERAL. Falling through to the
		 * window is what gives the key back — Ctrl+A is the start of
		 * the line in every shell on this image, and a desktop that
		 * took it outright would be one people turn off.
		 */
		if (keys_action(ev->key, ev->mods, &arg) != CON_ACT_LEADER) {
			KtuiEvent e = *ev;

			/*
			 * Looked up as though Super were held, which is the
			 * whole of the mechanism: one chord table, reached two
			 * ways. A key that names no chord is swallowed rather
			 * than typed — a prefix that leaked its second key
			 * would put a stray character in a document every time
			 * somebody mistyped a chord.
			 */
			e.mods |= KT_MOD_SUPER;
			session_key(&e);
			return;
		}
	} else if (session_key(ev)) {
		return;
	}

	Win *w = win_focused();

	if (!w)
		return;
	if (w->kind == WIN_TERM)
		term_key(w, ev);
	else if (w->kind == WIN_EMBED)
		embed_key(w, ev);
	else if (w->surf)
		kcon_surface_key(w->surf, ev->key, ev->mods);
}

/*
 * A DRAG IN PROGRESS.
 *
 * `route_ptr` held no state at all, so the pointer could raise a window and
 * click inside it and nothing else — no move, no resize. The grab records
 * where the pointer was and what the window's rectangle was when the button
 * went down, and every frame is computed from those rather than accumulated
 * from each motion: accumulating drifts, and a window that ends up somewhere
 * other than under the hand is one nobody trusts the mouse with again.
 */
static struct {
	int id;			/* the window, or 0 for no grab            */
	int resizing;		/* 0 moves the window, 1 resizes it        */
	int ox, oy;		/* the pointer when the button went down   */
	KwmRect og;		/* its rectangle then                      */
	unsigned edges;		/* which edges a resize moves              */
} grab;

/*
 * WHICH EDGES A RESIZE TAKES, from where in the window the press landed. The
 * nearest edge in each axis, so a press near a corner takes both and a press
 * in the middle of one side takes only that side.
 */
static unsigned grab_edges(const Win *w, int x, int y)
{
	unsigned e = 0;
	int third_w = w->geom.w / 3, third_h = w->geom.h / 3;

	if (third_w < 1)
		third_w = 1;
	if (third_h < 1)
		third_h = 1;

	if (x < w->geom.x + third_w)
		e |= KWM_EDGE_LEFT;
	else if (x >= w->geom.x + w->geom.w - third_w)
		e |= KWM_EDGE_RIGHT;
	if (y < w->geom.y + third_h)
		e |= KWM_EDGE_TOP;
	else if (y >= w->geom.y + w->geom.h - third_h)
		e |= KWM_EDGE_BOTTOM;

	/* A press in the exact middle still resizes: the bottom-right corner
	 * is what a hand expects when nothing else is nearer. */
	return e ? e : (KWM_EDGE_RIGHT | KWM_EDGE_BOTTOM);
}

static void grab_apply(const KtuiEvent *ev)
{
	Win *w = win_find(grab.id);
	int dx = ev->mx - grab.ox, dy = ev->my - grab.oy;
	KwmRect g = grab.og;

	if (!w)
		return;

	if (!grab.resizing) {
		g.x += dx;
		g.y += dy;
	} else {
		if (grab.edges & KWM_EDGE_LEFT) {
			g.x += dx;
			g.w -= dx;
		} else if (grab.edges & KWM_EDGE_RIGHT) {
			g.w += dx;
		}
		if (grab.edges & KWM_EDGE_TOP) {
			g.y += dy;
			g.h -= dy;
		} else if (grab.edges & KWM_EDGE_BOTTOM) {
			g.h += dy;
		}
		/* A window narrower than its own frame has no content and no
		 * title, and cannot be grabbed again to undo it. */
		if (g.w < 12)
			g.w = 12;
		if (g.h < 4)
			g.h = 4;
	}

	/*
	 * THROUGH kwm_fit, exactly as snapping is. A dragged window and a
	 * snapped one obeying different work-area rules is two answers to one
	 * question, and the panel's exclusive zone is in that answer.
	 */
	w->geom = kwm_fit(g, win_workarea());
	/* A dragged window is no longer where a tile put it. */
	w->tiled = 0;
	win_resized(w);
	ktui_draw_invalidate();
}

static void route_ptr(const KtuiEvent *ev)
{
	/* While locked the pointer reaches the lock surface and nothing else,
	 * for the same reason the keyboard does. */
	if (S.locked) {
		if (S.lock && S.lock->surf)
			kcon_surface_ptr(S.lock->surf, ev->mx, ev->my,
					 ev->btn, ev->press);
		return;
	}

	/*
	 * THE PANEL ROW IS ASKED FIRST, because it is not a window and
	 * `win_at()` cannot see it. Without this the bar is painted and
	 * nothing more: Start, every window row and the clock are drawn, look
	 * pressable, and answer nothing.
	 *
	 * Only when the session draws its own bar. A docked shell panel is a
	 * real window with its own hit testing, and answering for it here
	 * would take every click before its client saw one.
	 */
	if (!panel_have_shell()) {
		int arg;

		switch (panel_hit(ev->mx, ev->my, &arg)) {
		case PANEL_HIT_START:
			if (ev->press == KT_MP_PRESS)
				con_spawn(con_command(CON_CMD_MENU));
			return;
		case PANEL_HIT_WIN:
			if (ev->press == KT_MP_PRESS) {
				Win *t = win_find(arg);

				/* One row, two meanings, and the window's own
				 * state picks: a row is how a minimised window
				 * comes back, and how a visible one is
				 * raised. */
				if (t && t->minimised)
					win_restore(t);
				else if (t)
					win_raise(t->id);
			}
			return;
		case PANEL_HIT_CLOCK:
			if (ev->press == KT_MP_PRESS)
				con_spawn("kdos-cal");
			return;
		case PANEL_HIT_WS:
			if (ev->press == KT_MP_PRESS)
				win_workspace(arg);
			return;
		default:
			break;
		}
	}

	/* A grab owns the pointer until the button comes up, wherever it goes:
	 * a drag that stopped at the window's edge could not make it smaller. */
	if (grab.id) {
		if (ev->press == KT_MP_DRAG)
			grab_apply(ev);
		else if (ev->press == KT_MP_RELEASE)
			grab.id = 0;
		return;
	}

	/*
	 * A FRAME BUTTON IS ASKED BEFORE THE WINDOW UNDER IT. The buttons sit
	 * on the frame, which is inside the window's own rectangle, so
	 * `win_at()` answers for both and a click would raise the window and
	 * do nothing else.
	 */
	if (ev->press == KT_MP_PRESS) {
		int id;

		switch (win_button_at(ev->mx, ev->my, &id)) {
		case WIN_BTN_MIN:
			win_minimise(win_find(id));
			return;
		case WIN_BTN_MAX:
			win_maximise(win_find(id));
			return;
		case WIN_BTN_CLOSE:
			win_close(win_find(id));
			return;
		default:
			break;
		}
	}

	Win *w = win_at(ev->mx, ev->my);

	/* A press raises and focuses; motion is delivered where it landed
	 * without changing which window has the keyboard. */
	if (w && ev->press == KT_MP_PRESS)
		win_raise(w->id);

	/*
	 * WHERE A DRAG STARTS. The title row moves the window and the right
	 * button resizes it, and Super with either does the same from anywhere
	 * inside — which is what makes a window that is all content, a
	 * terminal or an embedded application, still movable without hunting
	 * for its one draggable row.
	 *
	 * A panel is neither: it is docked, its rectangle is its exclusive
	 * zone, and dragging it would move the work area out from under every
	 * other window.
	 */
	if (w && !w->panel && !w->full && ev->press == KT_MP_PRESS) {
		int on_title = ev->my == w->geom.y;
		int super = (ev->mods & KT_MOD_SUPER) != 0;
		int right = ev->btn == KT_MB_RIGHT;

		if (on_title || super || right) {
			grab.id = w->id;
			grab.resizing = right || (super && ev->btn == KT_MB_MIDDLE);
			grab.ox = ev->mx;
			grab.oy = ev->my;
			grab.og = w->geom;
			grab.edges = grab_edges(w, ev->mx, ev->my);
			return;
		}
	}

	if (!w)
		return;
	if (w->kind == WIN_TERM) {
		/* Middle-click pastes the primary before the terminal sees the
		 * button, the same order libkwl keeps: the click is still
		 * offered below, so a program tracking the mouse still gets
		 * it. */
		if (ev->btn == KT_MB_MIDDLE && ev->press == KT_MP_PRESS)
			term_paste(w, 1);
		term_mouse(w, ev);
		return;
	}
	if (w->kind == WIN_EMBED) {
		embed_ptr(w, ev);
		return;
	}
	if (!w->surf)
		return;

	kcon_surface_ptr(w->surf, ev->mx - w->geom.x, ev->my - w->geom.y,
			 ev->btn, ev->press);
}

/* A libkcon surface that attached but has no window yet gets one. */
static void adopt_surfaces(void)
{
	for (int i = 0; i < kcon_server_count(S.server); i++) {
		KconSurface *f = kcon_server_at(S.server, i);
		int known = 0;

		/*
		 * A VIEW IS NOT A WINDOW. It shares the client list with
		 * surfaces, and its cells are its PREVIOUS FRAME — so adopting
		 * one draws the session's last frame inside itself, one frame
		 * further in every time.
		 */
		if (kcon_surface_kind(f) == KCON_KIND_VIEW)
			continue;

		for (Win *w = S.wins; w; w = w->next)
			if (w->surf == f) {
				known = 1;
				/*
				 * A SECOND ATTACH IS A RESIZE REQUEST. An
				 * overlay that has grown — a candidate list
				 * that gained a row — asks by attaching again,
				 * and a session that ignored it would clip
				 * every frame after the first.
				 */
				if (!w->panel && !w->full &&
				    (kcon_surface_cols(f) != w->geom.w ||
				     kcon_surface_rows(f) != w->geom.h)) {
					win_place(w, kcon_surface_cols(f),
						  kcon_surface_rows(f));
					/* And say what it got: the client is
					 * waiting for the answer and draws at
					 * its old size until it arrives. */
					win_resized(w);
				}
			}
		unsigned role = kcon_surface_role(f);

		/*
		 * A CLIENT WITH NO SIZE HAS NOT ATTACHED YET — it has said
		 * hello and nothing more, and adopting it would put an empty
		 * window on the desktop for the rest of the round trip.
		 *
		 * A SAVER AND A DOCKED PANEL ARE THE EXCEPTIONS, because they
		 * attach asking for nothing — the panel naming only its
		 * thickness: the session owns the answer and sends it in the
		 * configure below. The server refuses a zero size from every
		 * other role, so nothing else can reach here that way.
		 */
		if (known || (!kcon_surface_cols(f) &&
			      role != KDISP_ROLE_SAVER &&
			      role != KDISP_ROLE_PANEL))
			continue;

		Win *w = calloc(1, sizeof(*w));

		if (!w)
			continue;
		w->kind = WIN_SURFACE;
		w->id = ++S.next_id;
		w->workspace = S.workspace;
		w->surf = f;
		snprintf(w->title, sizeof(w->title), "%s",
			 kcon_surface_title(f));
		snprintf(w->app_id, sizeof(w->app_id), "%s",
			 kcon_surface_app_id(f));
		w->next = S.wins;
		S.wins = w;

		if (role == KDISP_ROLE_PANEL) {
			w->panel = 1;
			w->panel_edge = kcon_surface_edge(f);
			w->exclusive = kcon_surface_exclusive(f);
			w->geom.w = kcon_surface_cols(f);
			w->geom.h = kcon_surface_rows(f);
			/*
			 * A PANEL THAT NAMED A THICKNESS IS SIZED HERE, along
			 * its edge by the screen and across it by what it
			 * asked for. Clamped to the screen because a bar
			 * longer than the display leaves no work area at all
			 * and the desktop underneath becomes unreachable.
			 */
			if (!w->geom.w || !w->geom.h) {
				int c = kcon_surface_want_cells(f);
				int horiz =
					w->panel_edge == KDISP_EDGE_TOP ||
					w->panel_edge == KDISP_EDGE_BOTTOM;

				if (c < 1)
					c = 1;
				w->geom.w = horiz ? S.cols :
					(c < S.cols ? c : S.cols);
				w->geom.h = horiz ?
					(c < S.rows ? c : S.rows) : S.rows;
			}
			win_dock(w);
			/* A panel never takes the keyboard by attaching. */
			kcon_surface_configure(f, w->geom.w, w->geom.h);
			continue;
		}

		if (role == KDISP_ROLE_LOCK) {
			/*
			 * ONE LOCK AT A TIME. A second client is refused and
			 * told so rather than replacing the first: it would
			 * otherwise take a screen it does not own, and the
			 * person typing would be answering a different program
			 * from the one that locked the session.
			 */
			if (S.locked && S.lock && S.lock->surf != f) {
				/* The refusal before the close, and on the
				 * same connection: they are read in order, so
				 * the client knows why it was closed rather
				 * than exiting 0 on a session it never got. */
				kcon_surface_lock_state(f,
							KCON_LOCK_FINISHED);
				win_close(w);
				continue;
			}

			/*
			 * THE WHOLE GRID, no frame, above everything. It is
			 * configured to the screen rather than placed: a lock
			 * surface that was given a window's worth of cells
			 * would leave the desktop visible around it.
			 */
			w->full = 1;
			w->geom.x = 0;
			w->geom.y = 0;
			w->geom.w = S.cols;
			w->geom.h = S.rows;
			S.locked = 1;
			S.lock = w;
			S.focus = w->id;
			kcon_surface_configure(f, S.cols, S.rows);

			/*
			 * THE GRANT, AND IT IS THE SESSION'S TO GIVE. The
			 * client refuses every keystroke until this arrives —
			 * attaching is a request, and a lock program that
			 * accepted input on its own say-so would take a
			 * password while the desktop was still on screen.
			 */
			kcon_surface_lock_state(f, KCON_LOCK_ENGAGED);
			continue;
		}

		/*
		 * A LAYER, NOT A TOPLEVEL. Twenty files in `kdos-shell` ask for
		 * OVERLAY — the Start menu, the launcher, the run box, toasts,
		 * the OSD, tooltips, the candidate window, the calendar — and
		 * `desk.c` asks for BACKGROUND. Both roles are declared with
		 * these semantics and every one of those surfaces means them.
		 *
		 * The size is the client's: a menu knows how big it is, and a
		 * layer that was placed like a window would be given a
		 * two-thirds rectangle it never asked for. A background that
		 * asks for nothing is given the grid, the way a saver is.
		 */
		if (role == KDISP_ROLE_OVERLAY ||
		    role == KDISP_ROLE_BACKGROUND) {
			int cw = kcon_surface_cols(f);
			int ch = kcon_surface_rows(f);

			if (role == KDISP_ROLE_BACKGROUND) {
				w->background = 1;
				if (!cw || !ch) {
					cw = S.cols;
					ch = S.rows;
				}
			} else {
				w->overlay = 1;
			}
			win_place(w, cw, ch);
			kcon_surface_configure(f, w->geom.w, w->geom.h);
			/*
			 * AN OVERLAY TAKES THE KEYBOARD; A BACKGROUND DOES
			 * NOT. The Start menu, the launcher and the run box
			 * are overlays and are answered by typing, so an
			 * overlay that did not focus would be a menu nobody
			 * could drive. The icon layer covers the whole grid
			 * and is behind everything — focusing it would take
			 * the keyboard away from the window a person is
			 * working in, every time the desktop redraws.
			 */
			if (role == KDISP_ROLE_OVERLAY)
				S.focus = w->id;
			continue;
		}

		if (role == KDISP_ROLE_SAVER) {
			/*
			 * THE WHOLE GRID, LIKE A LOCK, AND FOCUSED LIKE
			 * NOTHING. It is configured to the screen for the same
			 * reason the lock is — a saver given a window's worth
			 * of cells leaves the desktop showing around it — but
			 * it never becomes S.focus and it is not in the
			 * taskbar, the cycle order or the hit test. Every
			 * keystroke and every click goes to what is underneath,
			 * which is what lets the idle policy see the activity
			 * that takes this away.
			 *
			 * A SECOND ONE REPLACES THE FIRST: two savers is two
			 * animations on a screen nobody is looking at.
			 */
			if (S.saver)
				win_close(S.saver);
			w->full = 1;
			w->geom.x = 0;
			w->geom.y = 0;
			w->geom.w = S.cols;
			w->geom.h = S.rows;
			S.saver = w;
			kcon_surface_configure(f, S.cols, S.rows);
			continue;
		}

		win_place(w, kcon_surface_cols(f), kcon_surface_rows(f));
		S.focus = w->id;
	}
}

/*
 * SIGHUP is the live retint, on the same signal `kdos theme` already sends to
 * every long-lived surface. A FLAG rather than the work itself: reparsing a
 * file inside a handler is allocation inside a signal, and the loop is never
 * more than one tick away from noticing.
 *
 * The default disposition for SIGHUP is DEATH, so a program on
 * reload_session()'s list that does not handle it is one that gets killed by
 * `kdos theme amber` and comes back looking retinted by accident.
 */
static volatile sig_atomic_t g_retint;

static void on_hup(int sig)
{
	(void)sig;
	g_retint = 1;
}

static void retint(void)
{
	char name[64];

	if (kcol_theme_name(name, sizeof(name)) && *name)
		ktui_theme_set(name);

	/* The palette the terminal itself was given, then a full repaint: the
	 * diff against the previous frame would otherwise leave every cell
	 * that did not change its character in the old accent. */
	ktui_term_repalette();
	ktui_draw_invalidate();
}

/*
 * The lock surface asked to be dismissed — the password was accepted. This is
 * the ONLY path that clears the lock; a lock client that crashes reaches
 * win_gc() instead, which leaves `locked` standing.
 */
/*
 * `kdos con kill` REACHED THIS SESSION. Set the flag the loop reads and let
 * teardown run: the listeners are stopped and the clients drained on the way
 * out, which is the same path a logout takes. Ending here — closing
 * descriptors from inside a handler the pump is walking — would free surfaces
 * the caller is still iterating.
 */
/*
 * TEXT A DISPLAY WAS HANDED, into the focused window.
 *
 * Into the WINDOW rather than onto the clipboard: a person who pressed paste
 * in their own terminal means it to arrive, and leaving it in a selection they
 * would then have to paste again is asking twice for one gesture. It also goes
 * to the session's clipboard, so the next window can have it without the
 * far-end terminal being asked a second time.
 *
 * A terminal window takes it bracketed when its child asked for that, which is
 * `kvt_term_paste`'s whole job; a surface is handed it as clipboard data,
 * which is the only text channel a surface has.
 */
static void on_paste(KconSurface *v, const char *text, void *user)
{
	Win *w = win_focused();

	(void)v;
	(void)user;
	clip_put(text, strlen(text), 0);
	if (!w)
		return;
	if (w->kind == WIN_TERM && w->term)
		kvt_term_paste(w->term, text);
	else if (w->kind == WIN_SURFACE && w->surf)
		kcon_surface_clip_data(w->surf, text);
}

static void on_quit(KconSurface *f, void *user)
{
	(void)f;
	(void)user;
	con_quit();
}

/*
 * A SHELL ASKED FOR A WINDOW. Raising and closing stay the session's: it owns
 * the stack and the lifetime, and a panel that could do either itself would be
 * a second implementation of both.
 */
static void on_activate(KconSurface *f, unsigned id, void *user)
{
	Win *w = win_find((int)id);

	(void)f;
	(void)user;
	if (!w)
		return;
	/* A row for a minimised window is how it comes back, on the shell's
	 * bar exactly as on the session's own. */
	if (w->minimised)
		win_restore(w);
	else
		win_raise(w->id);
}

static void on_close_request(KconSurface *f, unsigned id, void *user)
{
	(void)f;
	(void)user;
	win_close(win_find((int)id));
}

static void on_unlock(KconSurface *f, void *user)
{
	(void)user;
	if (S.lock && S.lock->surf == f) {
		S.locked = 0;
		S.lock = NULL;
		S.focus = 0;
		/* The grant is withdrawn where it was given. A client that
		 * unlocked and stayed up would otherwise still believe it
		 * holds the session. */
		kcon_surface_lock_state(f, 0);
	}
}

/*
 * THE IDLE POLICY. Three steps — the saver, the lock, then the screen off —
 * and they must fire in that order: a screen that powered down before the lock
 * surface was up comes back on showing whatever was under it.
 *
 * `idle_dim` IS NOT ONE OF THEM. A dim is a brightness, and this desktop's
 * colours are eight palette slots with no brightness between them — a "dimmed"
 * grid would have to be repainted in different slots, which is a different
 * picture rather than a darker one, and it would fight every surface that
 * paints its own. The saver is not a dim standing in for one: it is a picture,
 * which a grid can draw exactly.
 *
 * IN A VIRTUAL MACHINE EVERY STEP DEFAULTS TO 0. A blanked screen over VNC is
 * indistinguishable from a crashed session, and that has already cost a
 * debugging afternoon on the graphical side. Writing any of the keys in
 * con.conf overrides that — including writing 0, which is how you say you
 * mean it.
 */
/*
 * clock_gettime rather than libkproc's helper: this is the only clock the
 * console session reads, and a library linked for one function is a library
 * every consumer of kdos-con then carries.
 */
static unsigned long long mono_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000 +
	       (unsigned long long)ts.tv_nsec / 1000000;
}

unsigned long long con_now_ms(void)
{
	return mono_ms();
}

/*
 * 120ms: long enough to be seen and short enough that a program ringing in a
 * loop is a flicker rather than a window that stays lit.
 */
#define BELL_MS 120

void con_bell(Win *w)
{
	if (w)
		w->bell_until = mono_ms() + BELL_MS;
	kcon_view_bell(S.server);
	ktui_draw_invalidate();
}

static struct {
	int saver_after, lock_after, off_after;	/* seconds; 0 is never */
	unsigned long long last_ms;
	int blanked;
	int saver_started;
	int locked_by_idle;
} I;

static void idle_init(void)
{
	int vm = kb_in_vm();

	I.saver_after = kcon_conf_int("idle_saver", vm ? 0 : 300);
	I.lock_after = kcon_conf_int("idle_lock", vm ? 0 : 600);
	I.off_after = kcon_conf_int("idle_off", vm ? 0 : 900);
	I.last_ms = mono_ms();
}

/*
 * Any input at all. Ends a blank and takes the saver away; does NOT end a lock
 * — only a password does that, which is the whole difference between them.
 *
 * The saver is ASKED TO CLOSE rather than killed. The session double-forks
 * everything it starts, so it does not know the process; and asking is what
 * lets a saver put its own affairs in order. A saver that ignores the request
 * stays on screen, which is a bug in that program and visible as one.
 */
static void idle_poke(void)
{
	I.last_ms = mono_ms();
	if (S.saver)
		win_close(S.saver);		/* which clears S.saver */
	I.saver_started = 0;
	if (!I.blanked)
		return;
	I.blanked = 0;
	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_blank(kcon_server_view_at(S.server, i), 0);
}

static void idle_tick(void)
{
	unsigned long long idle = mono_ms() - I.last_ms;

	/*
	 * STARTED ONCE, not once a tick. A saver that has been asked to close
	 * and has not finished exiting is still the saver for this idle
	 * period; only activity, which clears the flag, starts another.
	 */
	if (I.saver_after > 0 && !I.saver_started && !S.locked &&
	    idle >= (unsigned long long)I.saver_after * 1000) {
		I.saver_started = 1;
		con_spawn(con_command(CON_CMD_SAVER));
	}

	/* The lock first and the blank after it, in that order and never the
	 * other way: a screen that powered down before the lock surface was up
	 * would come back on to whatever was on it. */
	if (I.lock_after > 0 && !S.locked && !I.locked_by_idle &&
	    idle >= (unsigned long long)I.lock_after * 1000) {
		I.locked_by_idle = 1;
		/*
		 * The saver goes when the lock arrives. It is drawn under the
		 * lock and would be invisible, and an animation nobody can see
		 * is a machine burning a battery to draw for nobody.
		 */
		if (S.saver)
			win_close(S.saver);
		con_spawn(con_command(CON_CMD_LOCK));
	}
	if (!S.locked)
		I.locked_by_idle = 0;

	if (I.off_after > 0 && !I.blanked &&
	    idle >= (unsigned long long)I.off_after * 1000) {
		I.blanked = 1;
		for (int i = 0; i < kcon_server_view_count(S.server); i++)
			kcon_view_blank(kcon_server_view_at(S.server, i), 1);
	}
}

/*
 * WHAT HAS A WINDOW, as a file, next to the session's sockets.
 *
 * `kdos-box gc` has to know whether a box still has something on the screen
 * before it stops it, and on Wayland it asks the compositor's command socket.
 * There is no compositor here, and teaching kdos-tools this protocol would
 * pull libkcon and the whole cell model into a binary that is on every image.
 * A file it can read costs neither.
 *
 * Rewritten only when the set changes: a desktop that rewrote a file every
 * frame would be a desktop doing IO for as long as it is switched on.
 */
static void publish_windows(void)
{
	char buf[4096], path[192];
	size_t n = 0;
	static char last[4096];

	for (Win *w = S.wins; w; w = w->next) {
		if (w->panel || w->minimised || !w->app_id[0])
			continue;
		/* The saver is not something a box has on screen: it covers
		 * every window without being one, and `kdos-box gc` reading it
		 * as one would keep a box warm for as long as the machine sat
		 * idle. */
		if (w == S.saver)
			continue;
		if (n + strlen(w->app_id) + 2 >= sizeof(buf))
			break;
		n += (size_t)snprintf(buf + n, sizeof(buf) - n, "%s\n",
				      w->app_id);
	}
	buf[n] = '\0';

	if (!strcmp(buf, last))
		return;
	snprintf(last, sizeof(last), "%s", buf);

	/* Beside the socket, so it is inside the same 0700 directory and needs
	 * no mode of its own. */
	size_t sl = strlen(S.sock);

	if (sl < 6 || strcmp(S.sock + sl - 5, ".sock"))
		return;
	snprintf(path, sizeof(path), "%.*s.windows", (int)(sl - 5), S.sock);
	kb_write_file(path, buf);
}

/*
 * A surface sent a picture. THE SESSION LOOKS AT NONE OF IT: it holds no pixel
 * code, and forwarding the blob is the whole of what it can do — which is also
 * the whole of what it should do, because whether those pixels can be shown at
 * all is the display's question and not the session's.
 *
 * Every attached view, not the primary only: a second display is showing the
 * same desktop and would otherwise show a hole where the picture is.
 */
static void on_sprite(KconSurface *f, int slot, int w, int h,
		      uint32_t fallback, const uint32_t *argb, int pw, int ph,
		      void *user)
{
	(void)f;
	(void)user;
	if (!S.server)
		return;
	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_sprite(kcon_server_view_at(S.server, i), slot, w, h,
				 fallback, argb, pw, ph);
}

/*
 * A SHELL ASKED FOR A GRAPHICAL APPLICATION. It becomes an ordinary window:
 * kdos-cage composites it in a process of its own and the session cuts the
 * frames into sprites. The answer is 0 for that, the terminal number for a
 * guest that was pinned to one, and -1 when it could not be started at all —
 * the requester is the only thing in the chain with a person in front of it,
 * which is why anything comes back.
 */
static int on_run(KconSurface *f, const char *const argv[], const char *title,
		  unsigned flags, void *user)
{
	const char *why = "";

	(void)f;
	(void)user;

	/*
	 * A COMPOSITOR IS NEVER EMBEDDED. KCON_RUN_BARE says the guest IS one,
	 * and a compositor inside a kiosk compositor is a screen inside a
	 * screen — so it takes a terminal directly, with nothing holding it.
	 */
	if (flags & KCON_RUN_BARE) {
		Win *w = vt_open(argv, title, 0);

		return w ? w->vt : -1;
	}

	if (!(flags & KCON_RUN_VT) &&
	    con_display_mode(argv, &why) == CON_DISPLAY_EMBED) {
		Win *w = embed_open(argv, title);

		if (w)
			return 0;
		/*
		 * A terminal is what is left when the embedded compositor
		 * cannot be started. Better a full-screen application on
		 * another VT than a launcher that did nothing.
		 */
		fprintf(stderr, "kdos-con: cannot embed '%s' — falling back to "
				"a terminal of its own\n", argv[0]);
	}

	/*
	 * SAY WHY IT TOOK A TERMINAL. `con_display_mode` computes the reason
	 * and it was thrown away, so a person who asked for an application and
	 * got a full-screen guest on another terminal had no way to learn what
	 * decided that — a box profile, a configuration key, or the program
	 * being a compositor itself.
	 */
	if (why && *why)
		fprintf(stderr, "kdos-con: '%s' takes a terminal of its own: "
				"%s\n", argv[0] ? argv[0] : "?", why);

	Win *w = vt_open(argv, title, 1);

	return w ? w->vt : -1;
}

static int serve(const char *sock, const char *view)
{
	S.server = kcon_server_new(sock);
	if (!S.server) {
		fprintf(stderr, "kdos-con: cannot listen on %s\n", sock);
		return 1;
	}

	/*
	 * The view socket, when this session is one that can be attached to.
	 * Adding it demotes the socket above to surfaces only, and from that
	 * point what a client may be is decided by which one it reached.
	 */
	if (view && *view &&
	    kcon_server_listen(S.server, view, KCON_LISTEN_VIEW) != 0) {
		fprintf(stderr, "kdos-con: cannot listen on %s\n", view);
		kcon_server_free(S.server);
		S.server = NULL;
		return 1;
	}

	snprintf(S.sock, sizeof(S.sock), "%s", sock);

	/* THE SURFACE SOCKET, not the view socket. A program started inside
	 * the session inherits this and opens a window with it; it is never
	 * the address a display connects to. */
	setenv("KDOS_CON", sock, 1);

	KconServerHooks h = { 0 };

	h.view_key = on_view_key;
	h.view_ptr = on_view_ptr;
	h.unlock = on_unlock;
	h.sprite = on_sprite;
	h.run = on_run;
	h.quit = on_quit;
	h.paste = on_paste;
	h.clip_offer = clip_offer;
	h.clip_request = clip_request;
	h.activate = on_activate;
	h.close_request = on_close_request;
	kcon_server_hooks(S.server, &h, NULL);

	ktui_backend_set(&con_backend);
	ktui_draw_init();
	signal(SIGHUP, on_hup);
	idle_init();

	while (!quit) {
		/*
		 * SIZED FROM WHAT THERE IS, not from a constant.
		 *
		 * A fixed array of thirty-four made the count a silent budget:
		 * every listener, every client, every terminal and every
		 * embedded guest competed for it, and the loops written last
		 * were the ones squeezed out. A session with enough surfaces
		 * open simply stopped polling its terminals, so a window's
		 * output arrived at the tick rate however fast the program was
		 * writing.
		 *
		 * The buffer is kept between turns because the counts barely
		 * change, and grown when they do.
		 */
		static struct pollfd *p;
		static int pcap;
		int want = kcon_server_nfds(S.server) +
			   kcon_server_count(S.server) + 16;
		int n = 0;

		for (Win *w = S.wins; w; w = w->next)
			if (w->kind == WIN_TERM && w->term)
				want++;

		if (want > pcap) {
			struct pollfd *bigger =
				realloc(p, sizeof(*p) * (size_t)want);

			/* Out of memory keeps the array it has: polling fewer
			 * descriptors is slow, and polling a freed one is a
			 * crash. */
			if (bigger) {
				p = bigger;
				pcap = want;
			}
		}
		if (!p)
			continue;

		for (int i = 0; i < kcon_server_nfds(S.server) && n < pcap;
		     i++) {
			p[n].fd = kcon_server_fd_at(S.server, i);
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		/*
		 * EVERY CLIENT'S DESCRIPTOR, so a commit wakes the session
		 * instead of waiting for the next tick. It used to push -1,
		 * which poll ignores — so the loop consumed the budget and
		 * woke on nothing.
		 */
		for (int i = 0; i < kcon_server_count(S.server) && n < pcap;
		     i++) {
			KconSurface *f = kcon_server_at(S.server, i);
			int fd = kcon_surface_fd(f);

			if (fd < 0)
				continue;
			p[n].fd = fd;
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		for (Win *w = S.wins; w && n < pcap; w = w->next)
			if (w->kind == WIN_TERM && w->term) {
				p[n].fd = kvt_term_fd(w->term);
				p[n].events = POLLIN;
				p[n].revents = 0;
				n++;
			}

		int efd[16];
		int en = n < pcap ? embed_fds(efd, pcap - n) : 0;

		for (int i = 0; i < en && i < 16; i++) {
			p[n].fd = efd[i];
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		poll(p, (nfds_t)n, 20);

		/*
		 * A FLASH ENDS ON A FRAME, so there has to be one. Nothing
		 * else changes when the deadline passes, and a bell whose end
		 * waited for the next unrelated repaint would be a window left
		 * lit for as long as the desktop was quiet.
		 */
		for (Win *bw = S.wins; bw; bw = bw->next)
			if (bw->bell_until) {
				if (bw->bell_until <= mono_ms())
					bw->bell_until = 0;
				ktui_draw_invalidate();
			}

		if (g_retint) {
			g_retint = 0;
			retint();
		}
		idle_tick();

		kcon_server_pump(S.server);
		term_pump_all();
		adopt_surfaces();
		vt_reap();
		embed_reap();
		win_gc();
		publish_windows();
		mgmt_publish(0);
		publish_caret();

		/*
		 * A VIEW THAT JUST ATTACHED HAS BEEN SENT NO PICTURES. Its
		 * cells will name sprites it has never heard of, so every
		 * embedded window resends its blocks.
		 */
		/*
		 * A SHELL THAT HAS JUST ATTACHED MISSED EVERY ADD. It cannot
		 * ask for the list — there is no request verb, deliberately,
		 * because a client that could ask could ask repeatedly — so
		 * the session notices the arrival and sends it again.
		 */
		static int last_shells;
		int shells = 0;

		for (int i = 0; i < kcon_server_count(S.server); i++)
			if (kcon_surface_kind(kcon_server_at(S.server, i)) ==
			    KCON_KIND_SHELL)
				shells++;
		if (shells > last_shells)
			mgmt_resend();
		last_shells = shells;

		static int last_views;
		int views = kcon_server_view_count(S.server);

		if (views > last_views) {
			embed_view_attached();
			/* And every surface's, for the same reason: a picture
			 * crossed once and this display was not there. */
			kcon_server_resend_sprites(S.server);
		}
		last_views = views;

		/*
		 * THE GRID IS THE PRIMARY VIEW'S, and the primary is whichever
		 * attached first. When it detaches the next one is promoted and
		 * the grid becomes ITS size — so every window is brought back
		 * inside, moved rather than shrunk where that is enough.
		 */
		int vw = 0, vh = 0;

		con_size(&vw, &vh);
		if (vw != S.cols || vh != S.rows) {
			S.cols = vw;
			S.rows = vh;

			/*
			 * PANELS ARE RE-DOCKED BEFORE THE WORK AREA IS TAKEN.
			 * A docked rectangle is measured from an edge of the
			 * grid, so a panel left at its old place is off the
			 * screen or short of it — and its exclusive zone is
			 * what every window below is about to be fitted
			 * against, so the stale one moves them all wrong.
			 * Each keeps the thickness it asked for and gets the
			 * new extent; the configure tells it so.
			 */
			for (Win *w = S.wins; w; w = w->next) {
				if (!w->panel)
					continue;
				win_dock(w);
				if (w->surf)
					kcon_surface_configure(w->surf,
							       w->geom.w,
							       w->geom.h);
			}

			KwmRect area = win_workarea();

			for (Win *w = S.wins; w; w = w->next) {
				/*
				 * A FULL WINDOW IS THE WHOLE GRID, not the
				 * work area: a lock or a saver fitted to the
				 * area a panel left over would leave the
				 * panel's rows showing the desktop behind it.
				 */
				if (w->full) {
					w->geom.x = 0;
					w->geom.y = 0;
					w->geom.w = S.cols;
					w->geom.h = S.rows;
				} else if (w->tiled) {
					w->geom = win_tile_rect(w->tiled);
				} else {
					w->geom = kwm_fit(w->geom, area);
				}

				win_resized(w);
			}
			ktui_draw_invalidate();
		}

		KtuiEvent ev;

		while (ktui_backend()->poll_event(&ev, 0)) {
			if (ev.type == KT_EVT_KEY)
				route_key(&ev);
			else if (ev.type == KT_EVT_MOUSE)
				route_ptr(&ev);
		}

		/* THE PICTURES BEFORE THE CELLS THAT NAME THEM. A commit
		 * referring to a sprite a view has not been sent draws the
		 * fallback mark for a frame. */
		embed_pump();

		/* A view that has just attached has seen nothing, and its own
		 * previous frame is what decides how much it is sent. */
		ktui_draw_resize();
		composite();
		ktui_draw_flush();
	}

	/*
	 * STOP LISTENING BEFORE DRAINING. A client that connects while the
	 * session is on its way out gets a window on a desktop that is about
	 * to stop drawing, and its socket file outlives the process that owned
	 * it — which is what `kdos con ls` reads to decide a session is alive.
	 */
	kcon_server_unlisten(S.server, sock);
	if (view && *view)
		kcon_server_unlisten(S.server, view);

	teardown();
	kcon_server_free(S.server);
	return 0;
}

/*
 * kdos-grid: a session HERE, with a view on it, in one command. The socket
 * goes in the runtime directory, whose 0700 is the first half of the gate the
 * peer's credentials are the second half of.
 */
static int grid(const char *const *terms, int nterms)
{
	char nm[64], sock[192], view[192];

	/* Named after the process, so two `kdos-grid` in two terminals are two
	 * sessions rather than one refusing to start. */
	snprintf(nm, sizeof(nm), "grid-%d", (int)getpid());
	if (con_session_paths(nm, sock, sizeof(sock), view, sizeof(view)) != 0)
		return 1;

	session_init(80, 24);
	for (int i = 0; i < nterms; i++) {
		char store[1024];
		const char *av[32];
		int n = kxdg_exec_split(terms[i], NULL, 0, store,
					sizeof(store), av, 32);

		if (n <= 0)
			continue;
		av[n] = NULL;
		term_open(av);
	}

	/*
	 * The view is a CHILD, so the session outlives a view that crashes and
	 * the person gets their desktop back by attaching another.
	 */
	pid_t v = fork();

	if (v == 0) {
		setenv("KDOS_CON", sock, 1);
		/* Wait for the socket rather than racing it: the parent has
		 * not called serve() yet. */
		for (int i = 0; i < 200; i++) {
			if (access(view, F_OK) == 0)
				break;
			usleep(5000);
		}
		execlp("kdos-view", "kdos-view", "--tty", "--socket", view,
		       (char *)NULL);
		_exit(127);
	}
	if (v < 0) {
		fprintf(stderr, "kdos-grid: cannot start a view\n");
		return 1;
	}

	int r = serve(sock, view);

	kill(v, SIGTERM);
	waitpid(v, NULL, 0);
	return r;
}

int main(int argc, char **argv)
{
	const char *name = argc > 0 && argv[0] ? basename(argv[0]) : "kdos-con";
	int cols = 0, rows = 0;
	const char *sock = NULL;
	int do_serve = 0, do_greet = 0, do_new = 0, do_ls = 0;
	int do_attach = 0, do_kill = 0, do_detach = 0, do_run = 0;
	const char *tname = NULL;
	const char *login_tty = NULL;
	const char *terms[8];
	int nterms = 0;
	int run_at = 0;
	unsigned run_flags = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage(stdout);
			return 0;
		}
		if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
			if (sscanf(argv[++i], "%dx%d", &cols, &rows) != 2 ||
			    cols <= 0 || rows <= 0) {
				fprintf(stderr,
					"kdos-con: --dump wants COLSxROWS\n");
				return 2;
			}
			continue;
		}
		if (!strcmp(argv[i], "-t") && i + 1 < argc) {
			tname = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--new")) {
			do_new = 1;
			continue;
		}
		if (!strcmp(argv[i], "--ls")) {
			do_ls = 1;
			continue;
		}
		/*
		 * ANSWERED HERE AND NOW. It reads no socket and starts no
		 * session — the chords are a file and a table, and the card
		 * asks for them on a desktop that may not be running.
		 */
		if (!strcmp(argv[i], "--keys")) {
			keys_print();
			return 0;
		}
		if (!strcmp(argv[i], "--attach")) {
			do_attach = 1;
			continue;
		}
		if (!strcmp(argv[i], "--kill")) {
			do_kill = 1;
			continue;
		}
		if (!strcmp(argv[i], "--detach")) {
			do_detach = 1;
			continue;
		}
		if (!strcmp(argv[i], "--bare")) {
			run_flags |= KCON_RUN_BARE;
			continue;
		}
		if (!strcmp(argv[i], "--run")) {
			/*
			 * EVERYTHING AFTER IT IS THE GUEST'S. The argument
			 * vector arrives already split — by the caller, from a
			 * desktop entry — and is passed on whole, because
			 * re-joining it into a string here would be inventing a
			 * quoting rule for something that had none.
			 */
			do_run = 1;
			run_at = i + 1;
			/* `--` between the option and the command is allowed
			 * and means nothing: it is what a person types to stop
			 * a shell eating the guest's own options, and refusing
			 * it would make the separator itself the command. */
			if (run_at < argc && !strcmp(argv[run_at], "--"))
				run_at++;
			i = argc;
			continue;
		}
		if (!strcmp(argv[i], "--greet")) {
			do_greet = 1;
			continue;
		}
		if (!strcmp(argv[i], "--serve")) {
			do_serve = 1;
			continue;
		}
		if (!strcmp(argv[i], "--socket") && i + 1 < argc) {
			sock = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--term") && i + 1 < argc) {
			if (nterms < (int)(sizeof(terms) / sizeof(terms[0])))
				terms[nterms++] = argv[++i];
			else
				i++;
			continue;
		}
		if (argv[i][0] != '-' && !login_tty) {
			login_tty = argv[i];
			continue;
		}
		fprintf(stderr, "kdos-con: unknown option '%s'\n", argv[i]);
		usage(stderr);
		return 2;
	}

	if (!strcmp(name, "kdos-grid"))
		return grid(terms, nterms);

	if (do_ls)
		return con_sessions_list();

	/* EVERY OTHER SESSION VERB NEEDS A NAME, and the default is the one
	 * `kdos-con-start` opens: a person with one session should never have
	 * to name it. */
	if (do_run) {
		char ssock[192], sview[192];
		const char *env = getenv("KDOS_CON");

		if (run_at >= argc || !argv[run_at]) {
			fprintf(stderr, "kdos-con: --run wants a command\n");
			return 2;
		}

		/*
		 * $KDOS_CON when there is one, because a program started inside
		 * a session is talking to THAT session and not to whichever one
		 * the default name happens to reach.
		 */
		if (!tname && env && *env)
			snprintf(ssock, sizeof(ssock), "%s", env);
		else if (con_session_paths(tname ? tname : "con", ssock,
					   sizeof(ssock), sview,
					   sizeof(sview)) != 0)
			return 2;

		int vt = kcon_run(ssock, (const char *const *)argv + run_at,
				  argv[run_at], run_flags);

		if (vt < 0) {
			fprintf(stderr, "kdos-con: the session could not start "
					"'%s' — no session, or no free "
					"terminal for a guest that needs one\n",
				argv[run_at]);
			return 1;
		}
		/* ZERO IS THE ORDINARY ANSWER: it became a window. A number is
		 * the terminal a pinned guest was given. */
		printf("%d\n", vt);
		return 0;
	}

	if (do_new || do_attach || do_kill || do_detach ||
	    (do_serve && !sock)) {
		char ssock[192], sview[192];

		if (con_session_paths(tname ? tname : "con", ssock,
				      sizeof(ssock), sview,
				      sizeof(sview)) != 0)
			return 2;

		if (do_kill)
			return con_session_kill(tname ? tname : "con");

		if (do_detach) {
			if (kcon_detach_all(ssock) != 0) {
				fprintf(stderr, "kdos-con: no session '%s'\n",
					tname ? tname : "con");
				return 1;
			}
			return 0;
		}

		if (do_attach) {
			/* THE VIEW SOCKET. A display is handed cells and
			 * reports events; it is never given the surface
			 * socket, which is the right to place a window. */
			execlp("kdos-view", "kdos-view", "--tty", "--socket",
			       sview, (char *)NULL);
			fprintf(stderr, "kdos-con: cannot start a view\n");
			return 127;
		}

		session_init(cols > 0 ? cols : 80, rows > 0 ? rows : 24);
		for (int i = 0; i < nterms; i++) {
			char store[1024];
			const char *av[32];
			int n = kxdg_exec_split(terms[i], NULL, 0, store,
						sizeof(store), av, 32);

			if (n <= 0)
				continue;
			av[n] = NULL;
			term_open(av);
		}
		return serve(ssock, sview);
	}

	/* kdos-con-login is reached from /etc/inittab through kdos-getty, so
	 * its tty is an argument rather than something to discover: the getty
	 * knows which one it opened and nothing else here does. */
	if (!strcmp(name, "kdos-con-login") || do_greet)
		return con_login(login_tty ? login_tty : "tty1");

	if (do_serve) {
		if (!sock) {
			fprintf(stderr, "%s: --serve needs --socket PATH\n",
				name);
			return 2;
		}
		session_init(cols > 0 ? cols : 80, rows > 0 ? rows : 24);
		for (int i = 0; i < nterms; i++) {
			char store[1024];
			const char *av[32];
			int n = kxdg_exec_split(terms[i], NULL, 0, store,
						sizeof(store), av, 32);

			if (n <= 0)
				continue;
			av[n] = NULL;
			term_open(av);
		}
		return serve(sock, NULL);
	}

	if (!cols) {
		/*
		 * WITHOUT A VIEW THERE IS NO SIZE, and nothing to draw on. The
		 * session is real without a display, but it cannot guess a grid
		 * — that is exactly the decision a view exists to make.
		 */
		fprintf(stderr,
			"%s: no display. Attach a view, or use --dump COLSxROWS.\n",
			name);
		return 1;
	}

	session_init(cols, rows);

	if (ktui_offscreen_init(cols, rows) != 0) {
		fprintf(stderr, "%s: cannot render offscreen\n", name);
		return 1;
	}
	ktui_draw_init();

	if (sock) {
		S.server = kcon_server_new(sock);
		if (!S.server) {
			fprintf(stderr, "%s: cannot listen on %s\n", name, sock);
			return 1;
		}
		snprintf(S.sock, sizeof(S.sock), "%s", sock);
		setenv("KDOS_CON", sock, 1);
	}

	for (int i = 0; i < nterms; i++) {
		/*
		 * Split the way a desktop entry is, by libkxdg — the one
		 * correct way to turn a command line into an argument vector.
		 * No shell: this opens names that came from a menu.
		 */
		char store[1024];
		const char *av[32];
		int n = kxdg_exec_split(terms[i], NULL, 0, store, sizeof(store),
					av, 32);

		if (n <= 0) {
			fprintf(stderr, "%s: cannot read '%s'\n", name,
				terms[i]);
			return 1;
		}
		av[n] = NULL;
		if (!term_open(av)) {
			fprintf(stderr, "%s: cannot open a terminal\n", name);
			return 1;
		}
	}

	setenv("KDOS_CON_DUMP", "1", 1);
	settle();
	if (S.server)
		kcon_server_pump(S.server);
	win_gc();

	composite();
	ktui_draw_dump();

	teardown();
	if (S.server)
		kcon_server_free(S.server);
	return 0;
}
