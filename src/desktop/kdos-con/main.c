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
		Win *pw[32];
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
			pw[n] = w;
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

		(void)pw;
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
static int session_key(const KtuiEvent *ev)
{
	Win *w = win_focused();
	int arg;

	switch (keys_action(ev->key, ev->mods, &arg)) {
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
		if (arg < S.nworkspace)
			S.workspace = arg;
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

	if (session_key(ev))
		return;

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

	Win *w = win_at(ev->mx, ev->my);

	/* A press raises and focuses; motion is delivered where it landed
	 * without changing which window has the keyboard. */
	if (w && ev->press == KT_MP_PRESS)
		win_raise(w->id);

	if (!w || w->kind == WIN_TERM)
		return;
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
		 * A SAVER IS THE ONE EXCEPTION, because it attaches asking for
		 * nothing: the session owns the answer and sends it in the
		 * configure below. The server refuses a zero size from every
		 * other role, so nothing else can reach here that way.
		 */
		if (known ||
		    (!kcon_surface_cols(f) && role != KDISP_ROLE_SAVER))
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
			win_dock(w);
			/* A panel never takes the keyboard by attaching. */
			kcon_surface_configure(f, w->geom.w, w->geom.h);
			continue;
		}

		if (role == KDISP_ROLE_LOCK) {
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
static void on_unlock(KconSurface *f, void *user)
{
	(void)user;
	if (S.lock && S.lock->surf == f) {
		S.locked = 0;
		S.lock = NULL;
		S.focus = 0;
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
	kcon_server_hooks(S.server, &h, NULL);

	ktui_backend_set(&con_backend);
	ktui_draw_init();
	signal(SIGHUP, on_hup);
	idle_init();

	while (!quit) {
		struct pollfd p[34];
		int n = 0;

		for (int i = 0; i < kcon_server_nfds(S.server) && n < 34;
		     i++) {
			p[n].fd = kcon_server_fd_at(S.server, i);
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		for (int i = 0; i < kcon_server_count(S.server) && n < 33; i++) {
			KconSurface *f = kcon_server_at(S.server, i);

			(void)f;
			/* Every client's descriptor, so a commit wakes the
			 * session instead of waiting for the next tick. */
			p[n].fd = -1;
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		for (Win *w = S.wins; w && n < 34; w = w->next)
			if (w->kind == WIN_TERM && w->term) {
				p[n].fd = kvt_term_fd(w->term);
				p[n].events = POLLIN;
				p[n].revents = 0;
				n++;
			}

		int efd[16];
		int en = n < 34 ? embed_fds(efd, 34 - n) : 0;

		for (int i = 0; i < en && i < 16; i++) {
			p[n].fd = efd[i];
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		poll(p, (nfds_t)n, 20);

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

		/*
		 * A VIEW THAT JUST ATTACHED HAS BEEN SENT NO PICTURES. Its
		 * cells will name sprites it has never heard of, so every
		 * embedded window resends its blocks.
		 */
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
					KwmBorder m = { CON_FRAME, CON_FRAME,
							CON_FRAME, CON_FRAME };

					w->geom = kwm_tile_geom(area, S.gap, m,
								w->tiled);
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

	teardown();
	kcon_server_free(S.server);
	unlink(sock);
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
