/* kdos-con — the session server. See con.h.
 *
 * ONE BINARY, THREE NAMES, dispatched on argv[0] as ksvc and kdos-appbox
 * already are:
 *
 *   kdos-con        the session, which a view attaches to
 *   kdos-grid       start a session HERE and attach a view to it
 *   kdos-con-login  read con.conf and either autologin or greet
 */

#include <fcntl.h>
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
"  --attach [-t NAME] [--observe]\n"
"                     put a view on one; --observe watches without typing\n"
"  --detach [-t NAME] take every view off one, leaving it running\n"
"  --kill   [-t NAME] end one\n"
"  --conf KEY         print one con.conf value, for a script that must\n"
"                     agree with the session about what it says\n"
"  --capture [-t NAME|--socket PATH] [-w N]\n"
"                     print what is on a RUNNING session's screen as text;\n"
"                     -w N narrows it to one window by its ring number\n"
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
"  --press CHORD      press one of the session's own chords before the dump,\n"
"                     spelled the way keys.conf spells it; repeatable, and\n"
"                     only with --dump\n"
"  --layout NAME      open a saved arrangement before the dump; only with\n"
"                     --dump, because a dump composites a session of its own\n"
"  --clip-text        put stdin on this session's clipboard. Stdin rather\n"
"                     than an argument: what is copied is a password as\n"
"                     often as a URL, and argv is visible to every process\n"
"  --clip-take        print this session's clipboard on stdout, with no\n"
"                     trailing newline added\n"
"  --pick-colour      ask the session already running for the colour under\n"
"                     the pointer: it prompts, waits for a click and puts\n"
"                     the slot name and hex on its own clipboard. What\n"
"                     `kdos-shot colour` runs\n"
"  --layout-save NAME, --layout-load NAME\n"
"                     ask the session already running to write what is open\n"
"                     under NAME, or to open that arrangement. What\n"
"                     `kdos con layout save|load NAME` runs\n"
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

/*
 * ── WHO HAS THE KEYBOARD, PUBLISHED BY A DIFF ───────────────────────────
 *
 * Compared against the last turn of the loop rather than called from the
 * places focus changes — a raise, a minimise, a workspace switch, a window
 * closing, a surface being adopted, a lock going up. That is six call sites
 * to remember and it is the shape `mgmt.c` already uses for the window list:
 * one place to be wrong, and it cannot miss a path that did not exist when it
 * was written.
 *
 * A TERMINAL TELLS ITS CHILD (`CSI I` / `CSI O`, and only while the child
 * asked with DECSET 1004) and a surface is sent `KCON_OP_FOCUS`. An editor
 * that is not told it lost the focus does not reload a file changed underneath
 * it, and its next write is over somebody else's work.
 */
static void focus_publish(void)
{
	static int last = -1;

	if (S.focus == last)
		return;

	Win *lost = win_find(last);
	Win *got = win_find(S.focus);

	last = S.focus;
	if (lost) {
		if (lost->kind == WIN_TERM && lost->term)
			kvt_term_focus(lost->term, 0);
		else if (lost->kind == WIN_SURFACE && lost->surf)
			kcon_surface_focus(lost->surf, 0);
	}
	if (got) {
		if (got->kind == WIN_TERM && got->term)
			kvt_term_focus(got->term, 1);
		else if (got->kind == WIN_SURFACE && got->surf)
			kcon_surface_focus(got->surf, 1);
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
 * A FINGER, WITH THE VERDICT THE VIEW'S RECOGNISER ALREADY REACHED.
 *
 * PUT ON THE SAME QUEUE THE POINTER USES, so it reaches whatever is under it
 * by the same hit test — the session has one route from a coordinate to a
 * window and a second one would be a second answer to where a click lands.
 *
 * THE MOUSE EVENT ARRIVES SEPARATELY and is what every widget already handles:
 * the recogniser synthesises one beside the touch at the view, so a tap is a
 * click on a surface that has never heard of touch. What this adds is the
 * gesture, for the two things a click cannot say — a long press, which is what
 * `Shift+F10` is with a finger, and the direction of an edge swipe.
 */
static void on_view_touch(KconSurface *v, int x, int y, int slot, int phase,
			  unsigned ms, int gesture, void *user)
{
	KtuiEvent e;

	(void)v;
	(void)user;
	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_TOUCH;
	e.mx = x;
	e.my = y;
	e.slot = slot;
	e.phase = phase;
	e.ms = ms;
	e.gesture = gesture;
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
 * A TERMINAL AND A SURFACE BOTH REPORT ONE, and both report it in their own
 * cells: a window knows where its caret is and nothing else knows where the
 * window is, so the offset is added here and only here. A surface that has no
 * text field says so with a negative x and the cursor goes away, which is the
 * same answer as no focused window at all.
 */
static void publish_caret(void)
{
	static int last_x = -1, last_y = -1;
	Win *w = win_focused();
	int x = -1, y = -1;

	if (w && !S.locked && !S.saver) {
		int cx, cy;

		if (w->kind == WIN_TERM && w->term) {
			struct kvt_screen *sc = kvt_term_screen(w->term);

			if (sc) {
				x = w->geom.x + (int)kvt_screen_get_cursor_x(sc);
				y = w->geom.y + (int)kvt_screen_get_cursor_y(sc);
			}
		} else if (w->surf && kcon_surface_caret(w->surf, &cx, &cy)) {
			x = w->geom.x + cx;
			y = w->geom.y + cy;
		}
	}

	if (x == last_x && y == last_y)
		return;
	last_x = x;
	last_y = y;

	for (int i = 0; i < kcon_server_view_count(S.server); i++)
		kcon_view_cursor(kcon_server_view_at(S.server, i), x, y);
}

/*
 * WHAT THE FRAME JUST SAID, out to the readers.
 *
 * The toolkit's queue is this frame's and is cleared at the start of the next
 * one, so it is drained HERE — after the frame is composed and before anything
 * clears it. A record left for later is last frame's control, which is the
 * wrong one by default.
 *
 * The focused window goes first and only when it CHANGES: a reader that was
 * told the window every frame would say its title over the control a person
 * just moved to.
 */
static void publish_announce(void)
{
	static int last_win;
	static char last_title[128];
	static int last_readers;
	Win *w = win_focused();
	int readers = S.server ? kcon_server_a11y_count(S.server) : 0;

	if (!S.server || !readers)
		return;

	/*
	 * A READER THAT HAS JUST ATTACHED HAS BEEN TOLD NOTHING. The window is
	 * said again when one arrives, for the same reason a view that
	 * attaches is sent the whole frame: the desktop's state is not a
	 * stream somebody joins half way through. The widget records cannot be
	 * replayed — the queue is this frame's — and the next frame a person
	 * touches produces them again.
	 */
	if (readers > last_readers)
		last_win = 0;
	last_readers = readers;

	if (w && (w->id != last_win || strcmp(w->title, last_title))) {
		last_win = w->id;
		kb_strlcpy(last_title, w->title, sizeof(last_title));
		kcon_a11y_announce(S.server, KT_A11Y_WINDOW, w->title, NULL,
				   0, 0, w->geom.x, w->geom.y, w->geom.w,
				   w->geom.h);
	} else if (!w && last_win) {
		last_win = 0;
		last_title[0] = '\0';
	}

	for (int i = 0; i < ktui_announce_count(); i++) {
		const KtuiA11y *a = ktui_announce_at(i);

		kcon_a11y_announce(S.server, a->role, a->label, a->value,
				   a->index, a->count, 0, 0, 0, 0);
	}
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
		[CON_CMD_KEYS]     = { "keys",     "kdos-keys" },
		[CON_CMD_AUDIO]    = { "audio",    "kdos-audio" },
		[CON_CMD_NET]      = { "net",      "kdos-net" },
		[CON_CMD_BT]       = { "bluetooth", "kdos-bt" },
		[CON_CMD_DEVICES]  = { "devices",  "kdos-devices" },
		[CON_CMD_SETTINGS] = { "settings", "kdos-settings" },
		[CON_CMD_CAL]      = { "calendar", "kdos-cal" },
		[CON_CMD_DOC]      = { "docs",     "kdos-doc" },
		[CON_CMD_DISPLAY]  = { "displays", "kdos-display" },
		[CON_CMD_ENERGY]   = { "power",    "kdos-energy" },
		[CON_CMD_RES]      = { "monitor",  "kdos-res" },
		[CON_CMD_CALC]     = { "calculator", "kdos-calc" },
		[CON_CMD_NOTE]     = { "notes",    "kdos-note" },
		[CON_CMD_CLIP]     = { "clipboard", "kdos-clip" },
		[CON_CMD_CHARS]    = { "characters", "kdos-chars" },
		[CON_CMD_CONTACTS] = { "contacts", "kdos-contacts" },
		[CON_CMD_FIND]     = { "find",     "kdos-find" },
		[CON_CMD_CAPTURE]  = { "capture",  "kdos-shot" },
		[CON_CMD_THEME]    = { "theme",    "kdos-style" },
		[CON_CMD_BACKGROUND] = { "background",
					 "kdos background next" },
		[CON_CMD_PALETTE]  = { "palette",  "kdos-palette" },
		[CON_CMD_CAPTMENU] = { "capture_menu",
				       "kdos-palette --route capture" },
		[CON_CMD_SETUPMENU] = { "setup_menu",
					"kdos-palette --route setup" },
		[CON_CMD_CAPTSCREEN] = { "capture_screen",
					 "kdos-shot screen" },
		[CON_CMD_RECORD]   = { "record",   "kdos-record" },
		[CON_CMD_TIME]     = { "time",     "kdos notify --time" },
		[CON_CMD_BATTERY]  = { "battery",  "kdos notify --battery" },
		[CON_CMD_REMIND]   = { "remind",   "kdos remind --ask" },
		[CON_CMD_REMINDLS] = { "remind_ls", "kdos remind ls" },
		[CON_CMD_REMINDCLR] = { "remind_clear", "kdos remind clear" },
		[CON_CMD_DISMISS]  = { "dismiss", "kdos notify --dismiss" },
		[CON_CMD_DISMISSALL] = { "dismiss_all",
					 "kdos notify --dismiss-all" },
		[CON_CMD_DND]      = { "dnd",     "kdos notify --dnd" },
		[CON_CMD_UNDISMISS] = { "undismiss", "kdos notify --raise" },
		[CON_CMD_AWAKE]    = { "stay_awake",
				       "kdos toggle stay-awake" },
		[CON_CMD_NIGHT]    = { "night_light",
				       "kdos toggle night-light" },
		[CON_CMD_VOLUP]    = { "volume_up",   "kdos-osd volume +5" },
		[CON_CMD_VOLDOWN]  = { "volume_down", "kdos-osd volume -5" },
		[CON_CMD_MUTE]     = { "volume_mute", "kdos-osd volume mute" },
		[CON_CMD_PLAY]     = { "media_play",  "kdos-mpctl toggle" },
		[CON_CMD_STOP]     = { "media_stop",  "kdos-mpctl stop" },
		[CON_CMD_NEXT]     = { "media_next",  "kdos-mpctl next" },
		[CON_CMD_PREV]     = { "media_prev",  "kdos-mpctl prev" },
	};

	if (which < 0 || which >= CON_CMD_N)
		return NULL;
	return kcon_conf_str(cmd[which].key, cmd[which].def);
}

/*
 * THE KEY ITSELF, not what it resolves to. A layout row names a role rather
 * than a program, so something has to be able to ask this table "is `monitor`
 * one of yours" — and the answer must come from the table that binds the key,
 * or a second list of names goes stale the day a key is renamed.
 */
const char *con_command_name(int which)
{
	static const char *const key[CON_CMD_N] = {
		[CON_CMD_MENU] = "menu", [CON_CMD_LAUNCHER] = "launcher",
		[CON_CMD_LOCK] = "lock", [CON_CMD_SAVER] = "saver",
		[CON_CMD_KEYS] = "keys", [CON_CMD_AUDIO] = "audio",
		[CON_CMD_NET] = "net", [CON_CMD_BT] = "bluetooth",
		[CON_CMD_DEVICES] = "devices", [CON_CMD_SETTINGS] = "settings",
		[CON_CMD_CAL] = "calendar", [CON_CMD_DOC] = "docs",
		[CON_CMD_DISPLAY] = "displays", [CON_CMD_ENERGY] = "power",
		[CON_CMD_RES] = "monitor", [CON_CMD_CALC] = "calculator",
		[CON_CMD_NOTE] = "notes", [CON_CMD_CLIP] = "clipboard",
		[CON_CMD_CHARS] = "characters", [CON_CMD_FIND] = "find",
		[CON_CMD_CONTACTS] = "contacts",
		[CON_CMD_CAPTURE] = "capture", [CON_CMD_THEME] = "theme",
		[CON_CMD_BACKGROUND] = "background",
		[CON_CMD_PALETTE] = "palette",
		[CON_CMD_CAPTMENU] = "capture_menu",
		[CON_CMD_SETUPMENU] = "setup_menu",
		[CON_CMD_CAPTSCREEN] = "capture_screen",
		[CON_CMD_RECORD] = "record",
		[CON_CMD_TIME] = "time", [CON_CMD_BATTERY] = "battery",
		[CON_CMD_REMIND] = "remind",
		[CON_CMD_REMINDLS] = "remind_ls",
		[CON_CMD_REMINDCLR] = "remind_clear",
		[CON_CMD_DISMISS] = "dismiss",
		[CON_CMD_DISMISSALL] = "dismiss_all",
		[CON_CMD_DND] = "dnd", [CON_CMD_UNDISMISS] = "undismiss",
		[CON_CMD_AWAKE] = "stay_awake",
		[CON_CMD_NIGHT] = "night_light",
		[CON_CMD_VOLUP] = "volume_up",
		[CON_CMD_VOLDOWN] = "volume_down",
		[CON_CMD_MUTE] = "volume_mute", [CON_CMD_PLAY] = "media_play",
		[CON_CMD_STOP] = "media_stop", [CON_CMD_NEXT] = "media_next",
		[CON_CMD_PREV] = "media_prev",
	};

	if (which < 0 || which >= CON_CMD_N)
		return NULL;
	return key[which];
}

/*
 * The programs a run-or-raise chord reaches, each a con.conf key with a
 * default — the same split con_command() keeps, and for the same reason:
 * which key opens the mail is a keyboard question and which program IS the
 * mail is not.
 *
 * THE DEFAULTS ARE WHAT THIS IMAGE CARRIES. A key pointed at a program that
 * is not installed is not an error here — the chord starts nothing and the key
 * card drops its row, which is the same rule a route with no command keeps.
 */
const char *con_app(int which)
{
	static const struct { const char *key, *def; } app[CON_APP_N] = {
		[CON_APP_FILES]   = { "files",   "mc" },
		[CON_APP_MAIL]    = { "mail",    "aerc" },
		[CON_APP_BROWSER] = { "browser", "lynx" },
		[CON_APP_MUSIC]   = { "music",   "rmpc" },
		[CON_APP_AGENDA]  = { "agenda",  "ikhal" },
		[CON_APP_CHAT]    = { "chat",    "iamb" },
		[CON_APP_WRITE]   = { "writing", "micro" },
	};

	if (which < 0 || which >= CON_APP_N)
		return NULL;
	return kcon_conf_str(app[which].key, app[which].def);
}

/* The role's own name, for the same reason con_command_name() exists. */
const char *con_app_name(int which)
{
	static const char *const key[CON_APP_N] = {
		[CON_APP_FILES] = "files", [CON_APP_MAIL] = "mail",
		[CON_APP_BROWSER] = "browser", [CON_APP_MUSIC] = "music",
		[CON_APP_AGENDA] = "agenda", [CON_APP_CHAT] = "chat",
		[CON_APP_WRITE] = "writing",
	};

	if (which < 0 || which >= CON_APP_N)
		return NULL;
	return key[which];
}

/*
 * Start a program that will attach as a surface of its own. Double-forked, so
 * the session never has to reap it: a desktop that waited on its children
 * would stop drawing while a launcher was open, and one that did not wait
 * would fill its process table with zombies over a day's use.
 *
 * NO SHELL. The command is split into an argument vector, which is the only
 * way anything is executed in this tree.
 *
 * AND IT GETS NO CONSOLE. This session's own stdout is the tty the composited
 * grid is drawn on, so a child that inherited it would write over the desktop
 * — and a program deciding whether it has somewhere to print would be told
 * yes, on a terminal nobody can read. `/dev/null` in and out; stderr is left
 * alone, because the session's is already the log and that is where a
 * diagnostic belongs.
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
			int null = open("/dev/null", O_RDWR);

			if (null >= 0) {
				dup2(null, STDIN_FILENO);
				dup2(null, STDOUT_FILENO);
				if (null > STDERR_FILENO)
					close(null);
			}
			execvp(av[0], (char *const *)av);
			_exit(127);
		}
		_exit(0);
	}
	if (p > 0)
		waitpid(p, NULL, 0);
}

/*
 * THE SAME, FOR A SURFACE THAT BELONGS TO A PLACE ON THE PANEL.
 *
 * `kdos-start` and `kdos-cal` both take `--at-bottom X Y`, which becomes the
 * corner and the margins in their attach — so a menu opened by a click on the
 * bar lands above the thing that was clicked, and one opened by a chord names
 * no position and is centred. The unit is a cell here and a pixel on Wayland,
 * and the caller's number is the same on both because `kdisp_cell_w()` answers
 * 1 on this transport.
 */
void con_spawn_at(const char *cmd, int x)
{
	char buf[256];
	char at[32];

	if (!cmd || !*cmd)
		return;
	snprintf(at, sizeof(at), " --at-bottom %d %d", x < 0 ? 0 : x,
		 panel_rows());
	if (snprintf(buf, sizeof(buf), "%s%s", cmd, at) >= (int)sizeof(buf)) {
		con_spawn(cmd);
		return;
	}
	con_spawn(buf);
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

/*
 * ── REARRANGE: THE KEYBOARD MOVES A WINDOW ──────────────────────────────
 *
 * Turbo Vision's Ctrl+F5 and DESQview's Rearrange. Arrows move by a cell,
 * Shift+arrows resize from the bottom-right corner, Ctrl+arrows move by eight,
 * Return keeps and Escape puts back the rectangle from before the mode.
 *
 * POINTER RESISTANCE IS NOT APPLIED. A drag has resist and attract zones
 * because a hand is imprecise; a keyboard is exact, and a key that moved a
 * window by one cell except near an edge would be a key that lies.
 *
 * Every intermediate rectangle still goes through `kwm_fit`, so the work area
 * binds here exactly as it binds a snap and a drag.
 */
static struct {
	int id;			/* the window being rearranged, or 0 */
	KwmRect from;		/* what Escape puts back */
} rear;

static void rearrange_end(int keep)
{
	Win *w = win_find(rear.id);

	if (w && !keep) {
		w->geom = rear.from;
		win_resized(w);
	}
	rear.id = 0;
	ktui_draw_invalidate();
}

/* True when the key was the mode's. The mode owns every key while it is on:
 * a window being moved must not also be typed into. */
static int rearrange_key(const KtuiEvent *ev)
{
	Win *w = win_find(rear.id);
	int step = (ev->mods & KT_MOD_CTRL) ? 8 : 1;
	int size = (ev->mods & KT_MOD_SHIFT) != 0;
	int dx = 0, dy = 0;

	if (!w) {
		rear.id = 0;
		return 0;
	}
	switch (ev->key) {
	case KT_K_ESC:
		rearrange_end(0);
		return 1;
	case KT_K_ENTER:
		rearrange_end(1);
		return 1;
	case KT_K_LEFT:  dx = -step; break;
	case KT_K_RIGHT: dx =  step; break;
	case KT_K_UP:    dy = -step; break;
	case KT_K_DOWN:  dy =  step; break;
	default:
		/* Anything else ends the mode and is not passed on: a stray
		 * key while a window is being moved is a mistake, and putting
		 * it into the window underneath is the worse of the two
		 * answers. */
		rearrange_end(1);
		return 1;
	}

	KwmRect g = w->geom;

	if (size) {
		g.w += dx;
		g.h += dy;
		if (g.w < 8)
			g.w = 8;
		if (g.h < 3)
			g.h = 3;
	} else {
		g.x += dx;
		g.y += dy;
	}
	w->tiled = 0;
	w->geom = kwm_fit(g, win_workarea(), w->min_w, w->min_h);
	win_resized(w);
	ktui_draw_invalidate();
	return 1;
}

/*
 * A LINE ON THE BAR FOR A MOMENT.
 *
 * The console has one row that is always on screen and no notification of its
 * own — kdos-notifyd draws into a window, and a chord that could not do what
 * was asked has to answer before any window exists. It expires by itself: a
 * message that stayed would be a bar that had stopped being a taskbar.
 */
#define NOTICE_MS 4000

static char notice[160];
static unsigned long long notice_until;

void con_notice(const char *text)
{
	snprintf(notice, sizeof(notice), "%s", text ? text : "");
	notice_until = notice[0] ? con_now_ms() + NOTICE_MS : 0;
	ktui_draw_invalidate();
}

const char *con_notice_text(void)
{
	return notice_until && notice_until > con_now_ms() ? notice : NULL;
}

int con_rearranging(void)
{
	return rear.id != 0;
}

/*
 * ── MARK AND TRANSFER ───────────────────────────────────────────────────
 *
 * DESQview's, and it is cheaper here than it was there or anywhere since: the
 * session composes every window into one grid of cells, so the text on the
 * screen IS text, and marking a rectangle of it is a read of a buffer that
 * already exists. No protocol between two programs, no selection ownership,
 * and no cooperation from the program being marked — which is the point. It
 * works over a terminal, a surface, and an embedded graphical application
 * rendered as characters, because all three are cells by the time they are
 * here.
 *
 * The rectangle is the SCREEN'S, not a window's. A person marking a column of
 * numbers out of two windows side by side means the column, and a mark that
 * stopped at a frame would be a mark that knew better than they did.
 */
static struct {
	int on;
	int capture;		/* also file the rectangle as a picture */
	int dragging;		/* the button is down and drawing the box */
	int ax, ay;		/* the anchor: where the mark started */
	int cx, cy;		/* the caret */
} mark;

int con_marking(void)
{
	return mark.on;
}

/* The drag machinery is defined with the surface hooks, below the pointer
 * routing and the key routing that consult it. */
static int drag_ptr(const KtuiEvent *ev);
static void drag_end(void);
/* THE ICON LAYER LAST. `win_at()` skips the background on purpose — the layer
 * covers the whole grid — so a finger and a drop both ask this instead. */
static Win *drag_target(int x, int y);

static void mark_begin(int capture)
{
	Win *w = win_focused();

	/* Over the focused window's first content cell, which is where a
	 * person is looking; over the middle of the grid when there is no
	 * window, because the desktop is what they can see. */
	if (w) {
		mark.cx = w->geom.x;
		mark.cy = w->geom.y;
	} else {
		mark.cx = S.cols / 2;
		mark.cy = S.rows / 2;
	}
	mark.ax = mark.cx;
	mark.ay = mark.cy;
	mark.on = 1;
	mark.capture = capture;
	mark.dragging = 0;
	ktui_draw_invalidate();
}

/* The rectangle, corners sorted. The anchor and the caret are either way
 * round, and every reader of the mark wants x0 <= x1 and y0 <= y1. */
static void mark_rect(int *x0, int *y0, int *x1, int *y1)
{
	*x0 = mark.ax < mark.cx ? mark.ax : mark.cx;
	*x1 = mark.ax < mark.cx ? mark.cx : mark.ax;
	*y0 = mark.ay < mark.cy ? mark.ay : mark.cy;
	*y1 = mark.ay < mark.cy ? mark.cy : mark.ay;
}

/*
 * The marked text, trailing spaces trimmed per row and rows joined with a
 * newline — the same shape `kvt_selection.c` gives a dragged selection, so a
 * paste of either arrives looking the same.
 */
static void mark_copy(void)
{
	int w, h;
	/*
	 * THE COMPOSED FRAME, not `ktui_cells()`. This session's own backend
	 * ignores libktui's `prev` — each view diffs against its own previous
	 * frame — so the buffer that call hands out is never written and a copy
	 * taken from it is a rectangle of zeroes indistinguishable from a
	 * rectangle of spaces.
	 */
	const KtuiCell *cells = ktui_draw_cells(&w, &h);
	int x0, x1, y0, y1;
	char *buf;
	size_t cap, len = 0;

	mark_rect(&x0, &y0, &x1, &y1);

	if (!cells || w <= 0 || h <= 0)
		return;
	if (x1 >= w)
		x1 = w - 1;
	if (y1 >= h)
		y1 = h - 1;
	/* Four bytes a cell is the widest a codepoint encodes, plus a newline
	 * a row and a terminator. */
	cap = (size_t)(x1 - x0 + 1) * (size_t)(y1 - y0 + 1) * 4 +
	      (size_t)(y1 - y0 + 1) + 1;
	buf = malloc(cap);
	if (!buf)
		return;

	for (int y = y0; y <= y1; y++) {
		size_t rowstart = len;
		size_t lastink = len;

		for (int x = x0; x <= x1; x++) {
			const KtuiCell *c = &cells[y * w + x];
			uint32_t ch = c->ch ? c->ch : ' ';

			/* A sprite's cell carries a slot number rather than a
			 * character; it is a picture and it marks as a
			 * space, which is what a person copying text around
			 * one means. */
			if (KTUI_IS_SPRITE(ch))
				ch = ' ';
			len += (size_t)ktui_utf8_encode(ch, buf + len);
			if (ch != ' ')
				lastink = len;
		}
		len = lastink > rowstart ? lastink : rowstart;
		if (y < y1)
			buf[len++] = '\n';
	}
	buf[len] = '\0';
	clip_put(buf, len, 0);
	free(buf);
}

void con_mark_draw(void)
{
	int x0, x1, y0, y1;

	if (!mark.on)
		return;
	mark_rect(&x0, &y0, &x1, &y1);

	/*
	 * REVERSED, NOT REPAINTED. The cells under the mark belong to whatever
	 * program drew them and are what is being copied; drawing the mark in
	 * a colour of its own would hide the text a person is trying to see
	 * the extent of.
	 */
	ktui_draw_reverse(krect(x0, y0, x1 - x0 + 1, y1 - y0 + 1));
}

/*
 * THE END OF A MARK: the text always, the picture when this was a capture.
 *
 * THE MARK IS TAKEN DOWN BEFORE THE PICTURE IS ASKED FOR. `CON_CMD_CAPTURE`
 * photographs the session by attaching a second view to it, so a rectangle
 * still drawn in reverse video would be reverse video in the file.
 *
 * The geometry is CELLS, which is what the console has; the program it names
 * hands the same four numbers to a view, and a view is the only thing in the
 * tree that knows what a cell is in pixels.
 */
static void mark_finish(void)
{
	int x0, y0, x1, y1;
	int capture = mark.capture;
	char cmd[256];
	const char *prog;

	mark_rect(&x0, &y0, &x1, &y1);
	mark_copy();
	mark.on = 0;
	mark.capture = 0;
	mark.dragging = 0;
	ktui_draw_invalidate();
	if (!capture)
		return;
	prog = con_command(CON_CMD_CAPTURE);
	if (!prog || !*prog)
		return;
	if (snprintf(cmd, sizeof(cmd), "%s region --geom %d,%d,%d,%d", prog,
		     x0, y0, x1 - x0 + 1, y1 - y0 + 1) < (int)sizeof(cmd))
		con_spawn(cmd);
}

/*
 * ── THE COLOUR PICKER ────────────────────────────────────────────────────
 *
 * A LOOKUP, NOT A PROBE. Every cell on this desktop carries the slot it was
 * drawn in, so the colour under the pointer is already known — reading it back
 * is a read of the composed frame, the same buffer a mark is copied out of.
 * There is nothing to sample and no screen to grab.
 *
 * THE INK, AND THE GROUND WHERE THERE IS NO INK. A cell holding a character is
 * that character's colour; an empty cell is its background. A picker that
 * always answered the foreground would answer with the colour of a glyph
 * nobody can see.
 *
 * A TERMINAL'S OWN COLOUR IS ANSWERED AS ITSELF. A cell a program painted in
 * truecolor carries the literal beside the slot it reduced to; the literal is
 * what that program chose, so it is what a person asking is told, with no slot
 * name in front of it.
 */
static int picking;

int con_picking(void)
{
	return picking;
}

static void pick_begin(void)
{
	picking = 1;
	ktui_draw_invalidate();
}

/* The colour of one cell, as `accent #39ff14` or a bare `#rrggbb`. */
static void pick_at(int x, int y)
{
	int w, h;
	const KtuiCell *cells = ktui_draw_cells(&w, &h);
	char out[64];

	picking = 0;
	ktui_draw_invalidate();
	if (!cells || x < 0 || y < 0 || x >= w || y >= h)
		return;

	const KtuiCell *c = &cells[y * w + x];
	uint32_t ch = c->ch ? c->ch : ' ';
	int ink = !KTUI_IS_SPRITE(ch) && ch != ' ';
	int slot = ink ? c->fg : c->bg;
	int lit = c->attr & (ink ? KT_A_FGRGB : KT_A_BGRGB);
	uint32_t rgb;

	if (lit) {
		rgb = ink ? c->fgc : c->bgc;
		snprintf(out, sizeof(out), "#%06x", rgb & 0xffffffu);
	} else {
		const char *nm = ktui_slot_name(slot);
		KRgb v;

		if (!nm)
			return;
		v = ktui_theme->slot[slot];
		snprintf(out, sizeof(out), "%s #%02x%02x%02x", nm, v.r, v.g,
			 v.b);
	}
	clip_put(out, strlen(out), 0);
	con_notice(out);
}

/* True when the key was the picker's: it owns the keyboard while it is on, for
 * the reason the mark does. */
static int pick_key(const KtuiEvent *ev)
{
	if (ev->key == KT_K_ESC) {
		picking = 0;
		ktui_draw_invalidate();
	}
	return 1;
}

/*
 * THE POINTER DRAWS THE SAME RECTANGLE the arrows do. The mark owns the
 * pointer while it is on, for the reason it owns the keyboard: a press that
 * fell through would raise a window over the region being marked, and a
 * capture would photograph that instead.
 */
static void mark_ptr(const KtuiEvent *ev)
{
	int x = ev->mx, y = ev->my;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= S.cols)
		x = S.cols - 1;
	if (y >= S.rows)
		y = S.rows - 1;

	if (ev->press == KT_MP_PRESS) {
		mark.ax = mark.cx = x;
		mark.ay = mark.cy = y;
		mark.dragging = 1;
	} else if (!mark.dragging) {
		return;
	} else if (ev->press == KT_MP_DRAG) {
		mark.cx = x;
		mark.cy = y;
	} else if (ev->press == KT_MP_RELEASE) {
		mark.cx = x;
		mark.cy = y;
		mark_finish();
		return;
	} else {
		return;
	}
	ktui_draw_invalidate();
}

/* True when the key was the mark's. It owns the keyboard while it is on. */
static int mark_key(const KtuiEvent *ev)
{
	int step = (ev->mods & KT_MOD_CTRL) ? 8 : 1;
	int extend = (ev->mods & KT_MOD_SHIFT) != 0;
	int dx = 0, dy = 0;

	switch (ev->key) {
	case KT_K_ESC:
		mark.on = 0;
		mark.capture = 0;
		mark.dragging = 0;
		ktui_draw_invalidate();
		return 1;
	case KT_K_ENTER:
		mark_finish();
		return 1;
	case KT_K_LEFT:  dx = -step; break;
	case KT_K_RIGHT: dx =  step; break;
	case KT_K_UP:    dy = -step; break;
	case KT_K_DOWN:  dy =  step; break;
	case KT_K_HOME:  mark.cx = 0; goto moved;
	case KT_K_END:   mark.cx = S.cols - 1; goto moved;
	default:
		return 1;	/* swallowed: the mode owns the keyboard */
	}

	mark.cx += dx;
	mark.cy += dy;
moved:
	if (mark.cx < 0)
		mark.cx = 0;
	if (mark.cy < 0)
		mark.cy = 0;
	if (mark.cx >= S.cols)
		mark.cx = S.cols - 1;
	if (mark.cy >= S.rows)
		mark.cy = S.rows - 1;
	/* WITHOUT SHIFT THE ANCHOR FOLLOWS, so the arrows place the corner and
	 * Shift+arrows drag the other one out of it — which is how every text
	 * selection since the eighties has worked. */
	if (!extend) {
		mark.ax = mark.cx;
		mark.ay = mark.cy;
	}
	ktui_draw_invalidate();
	return 1;
}

/*
 * THE SCREEN'S FONT, ONE STEP. Every view that rasterises its own glyphs, not
 * the primary alone: two screens showing one session must not end up at two
 * cell sizes because the chord reached whichever attached first.
 *
 * A view inside somebody else's terminal said so in its hello and is sent
 * nothing — that terminal owns the font, no message from here can change it,
 * and the honest answer is the one on the bar.
 */
/*
 * THE FACES A DISPLAY OFFERED, and the picker that asked for them.
 *
 * THE SESSION GATHERS NOTHING. A view is the only end with a font stack and it
 * may be on another machine, so the list is asked of the displays and relayed;
 * what goes back is an index into what a display itself produced, which is the
 * same rule the font STEP keeps said about a list.
 *
 * The first view to answer wins. Two screens showing one session are already
 * held to one font by font_step(), so two lists would be one question with two
 * answers and no way to ask which screen a person meant.
 */
static void fonts_ask_views(void)
{
	for (int i = 0; i < kcon_server_view_count(S.server); i++) {
		KconSurface *v = kcon_server_view_at(S.server, i);

		if (kcon_view_caps(v) & KCON_VIEW_FONT)
			kcon_view_fonts_ask(v);
	}
}

static void on_view_fonts(KconSurface *v, const char *const *names, int n,
			  int cur, void *user)
{
	const char *ptr[KCON_MAX_FONTS];

	(void)v;
	(void)user;
	if (n < 0)
		n = 0;
	if (n > KCON_MAX_FONTS)
		n = KCON_MAX_FONTS;
	for (int i = 0; i < n; i++)
		snprintf(S.fonts[i], KCON_FONT_NAME, "%s",
			 names && names[i] ? names[i] : "");
	S.nfonts = n;
	S.font_cur = cur >= 0 && cur < n ? cur : -1;

	if (!S.fonts_for)
		return;
	for (int i = 0; i < S.nfonts; i++)
		ptr[i] = S.fonts[i];
	kcon_surface_fonts(S.fonts_for, ptr, S.nfonts, S.font_cur);
	S.fonts_for = NULL;
}

/*
 * A PICKER ASKED. It is answered from what is held AND the displays are asked
 * again, so a second opening is instant and a font installed since the last
 * one is still offered. A session with no display that rasterises its own
 * glyphs answers an empty list, which is the honest answer and not an error.
 */
static void on_fonts_ask(KconSurface *f, void *user)
{
	const char *ptr[KCON_MAX_FONTS];

	(void)user;
	for (int i = 0; i < S.nfonts; i++)
		ptr[i] = S.fonts[i];
	kcon_surface_fonts(f, ptr, S.nfonts, S.font_cur);
	S.fonts_for = S.nfonts ? NULL : f;
	fonts_ask_views();
}

/*
 * WEAR THE NTH OF THEM. The index is checked against the list this session
 * relayed, and the display checks it again against the list IT sent — a
 * display that rebuilt a shorter list between the two is a display the session
 * cannot know about.
 */
static void on_font_set(KconSurface *f, int index, int keep, void *user)
{
	(void)f;
	(void)user;
	if (index >= 0 && index >= S.nfonts)
		return;
	for (int i = 0; i < kcon_server_view_count(S.server); i++) {
		KconSurface *v = kcon_server_view_at(S.server, i);

		if (kcon_view_caps(v) & KCON_VIEW_FONT)
			kcon_view_set_font(v, index, keep);
	}
	if (index >= 0)
		S.font_cur = index;
}

static int font_step(int step)
{
	int sent = 0;

	for (int i = 0; i < kcon_server_view_count(S.server); i++) {
		KconSurface *v = kcon_server_view_at(S.server, i);

		if (!(kcon_view_caps(v) & KCON_VIEW_FONT))
			continue;
		kcon_view_font(v, step);
		sent = 1;
	}
	if (!sent)
		con_notice("the terminal this view runs in owns the font — "
			   "change it there");
	return 1;
}

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
	case CON_ACT_FOCUS_OR_LAUNCH: {
		const char *prog = con_app(arg);
		char store[512];
		const char *av[16];
		int n;

		if (!prog || !*prog)
			return 1;
		/*
		 * FROM THE FOCUSED WINDOW WHEN IT IS ALREADY THIS PROGRAM,
		 * which is what turns a second press into a cycle: three
		 * terminals under one chord are reachable, and a first press
		 * from anywhere else still lands on the front one.
		 */
		Win *t = win_find_prog(prog,
				       w && !strcmp(w->prog, prog) ? w->id : 0);

		if (t) {
			/*
			 * THE WORKSPACE FIRST AND THE RAISE AFTER, never the
			 * other way round: win_workspace() clears the focus and
			 * cycles to whatever the ring lands on, so a raise
			 * before it is undone. And a minimised window is
			 * un-minimised WHERE IT IS — win_restore() moves the
			 * window to the current workspace, which is the
			 * opposite of going to it.
			 *
			 * A STICKY WINDOW IS ALREADY HERE, so its `workspace`
			 * is not asked: the scratchpad can be running the very
			 * program this chord names, and switching to the
			 * workspace it last recorded would move the screen for
			 * a window that never left it. Hidden is cleared for
			 * the same reason minimised is — a raise onto a window
			 * drawn nowhere is a focus a person cannot see.
			 */
			if (!t->sticky && t->workspace != S.workspace)
				win_workspace(t->workspace);
			t->minimised = 0;
			t->hidden = 0;
			win_raise(t->id);
			S.focus = t->id;
			ktui_draw_invalidate();
			return 1;
		}

		/* NO SHELL. The con.conf value is an argument vector, split
		 * the way every other command in this file is. */
		n = kxdg_exec_split(prog, NULL, 0, store, sizeof(store), av,
				    16);
		if (n > 0) {
			av[n] = NULL;
			term_open(av);
		}
		return 1;
	}
	case CON_ACT_SCRATCH: {
		Win *t = win_scratch();
		char store[512];
		const char *av[16];
		int n;

		if (t) {
			/*
			 * BOTH WAYS OF BEING AWAY ANSWER THE SHOW. A person can
			 * minimise the scratchpad from its own frame like any
			 * other window, and a chord that only understood
			 * `hidden` would hide an already invisible window and
			 * need a second press to undo it.
			 */
			if (t->hidden || t->minimised)
				win_scratch_show(t);
			else
				win_scratch_hide(t);
			return 1;
		}

		/*
		 * NONE YET, SO THE FIRST PRESS OPENS ONE — the session's own
		 * terminal, the same `con.conf` key `Super+Return` reads. No
		 * shell and no system(): the value is an argument vector.
		 */
		n = kxdg_exec_split(kcon_conf_str("terminal", "sh"), NULL, 0,
				    store, sizeof(store), av, 16);
		if (n > 0) {
			av[n] = NULL;
			t = term_open(av);
		}
		if (t) {
			win_scratch_mark(t);
			win_scratch_show(t);
		}
		return 1;
	}
	case CON_ACT_BAR:
		con_bar_toggle();
		return 1;
	case CON_ACT_SCRATCH_MARK:
		win_scratch_mark(w);
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
	case CON_ACT_TILE:
		win_tile_all();
		ktui_draw_invalidate();
		return 1;
	case CON_ACT_CASCADE:
		win_cascade();
		ktui_draw_invalidate();
		return 1;
	case CON_ACT_REARRANGE:
		if (w && !w->full) {
			rear.id = w->id;
			rear.from = w->geom;
			ktui_draw_invalidate();
		}
		return 1;
	case CON_ACT_SHOW_DESKTOP:
		win_show_desktop();
		ktui_draw_invalidate();
		return 1;
	case CON_ACT_WIN_N: {
		Win *t = win_nth(arg);

		if (t)
			win_raise(t->id);
		return 1;
	}
	case CON_ACT_WINLIST:
		win_list_toggle();
		return 1;
	case CON_ACT_MARK:
		mark_begin(0);
		return 1;
	case CON_ACT_CAPTURE:
		mark_begin(1);
		return 1;
	case CON_ACT_LEARN:
		scr_learn_toggle();
		return 1;
	case CON_ACT_PLAY:
		scr_play_arm();
		return 1;
	case CON_ACT_FONT_UP:
		return font_step(1);
	case CON_ACT_FONT_DOWN:
		return font_step(-1);
	case CON_ACT_FONT_RESET:
		return font_step(0);
	case CON_ACT_PASTE: {
		/*
		 * THE SESSION'S CLIPBOARD INTO THE FOCUSED WINDOW, which is
		 * DESQview's Transfer. A view can already hand the session a
		 * paste over KCON_OP_PASTE, but a `--kms` view has no host
		 * terminal to take one from, so on tty1 this chord is the
		 * whole of it.
		 */
		size_t n = 0;
		const char *t = clip_get(0, &n);

		if (!t || !n || !w)
			return 1;
		con_paste_win(w, t);
		return 1;
	}
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

	/*
	 * A SCRIPT'S LETTER PROMPT OWNS THE KEYBOARD WHILE IT IS UP, ahead of
	 * everything: the answer is one letter, and a letter that fell through
	 * to the chord table would bind the script and fire an action with the
	 * same press.
	 */
	if ((scr_prompt_active() || scr_play_armed()) && scr_prompt_key(ev))
		return;

	/*
	 * AND ESCAPE STOPS A REPLAY. A script types into a window at its own
	 * rate, so the only way out of one that is typing the wrong thing is a
	 * key the session takes before the window does.
	 */
	if (scr_playing() && ev->key == KT_K_ESC && !ev->mods) {
		scr_stop();
		return;
	}

	/*
	 * REARRANGE OWNS THE KEYBOARD WHILE IT IS ON, ahead of the chord table
	 * and ahead of the window: the arrows are the mode's, and a chord that
	 * fired underneath it would move the window and switch the workspace in
	 * one keystroke.
	 */
	if (con_rearranging() && rearrange_key(ev))
		return;

	/*
	 * AND SO DOES THE MARK. Its arrows place a corner, and a chord firing
	 * underneath would snap a window while somebody was selecting out of
	 * it.
	 */
	/* ESC GIVES IT BACK. A drag with no way out is a pointer that has
	 * stopped answering — and the one that started it is another program,
	 * which cannot be asked to stop. */
	if (con_dragging() && ev->key == KT_K_ESC) {
		drag_end();
		ktui_draw_invalidate();
		return;
	}
	if (picking && pick_key(ev))
		return;
	if (con_marking() && mark_key(ev))
		return;

	/*
	 * THE WINDOW LIST OWNS THE KEYBOARD WHILE IT IS UP, for the same
	 * reason: its arrows move the selection, and a chord firing underneath
	 * would snap a window while somebody was choosing one.
	 */
	if (win_list_active() && win_list_key(ev->key))
		return;

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

	/*
	 * RECORDED WHERE IT IS ROUTED, not where it arrives: a chord that the
	 * session consumed above never reached a window, so a recording that
	 * held one would replay an action nobody typed into anything.
	 */
	scr_note(ev);
	con_key_to_window(ev);
}

void con_key_to_window(const KtuiEvent *ev)
{
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
	w->geom = kwm_fit(g, win_workarea(), w->min_w, w->min_h);
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
		/* A LOCK ENDS A DRAG. The release that would have finished it
		 * goes to the lock surface, so a drag left on would be one
		 * waiting for an event that is never coming — and holding
		 * somebody's payload while the screen is locked. */
		if (con_dragging())
			drag_end();
		if (S.lock && S.lock->surf)
			kcon_surface_ptr(S.lock->surf, ev->mx, ev->my,
					 ev->btn, ev->press);
		return;
	}

	/* A DRAG OWNS THE POINTER FIRST OF ALL. It is the only mode a program
	 * outside this process started, so it is the one this process must not
	 * quietly take the pointer away from. */
	if (drag_ptr(ev))
		return;

	/* And so does the picker, for the reason the mark does: a press that
	 * fell through would raise a window over the cell being read. */
	if (picking) {
		if (ev->press == KT_MP_PRESS)
			pick_at(ev->mx, ev->my);
		return;
	}

	/* The mark owns the pointer while it is on — see mark_ptr(). */
	if (mark.on) {
		mark_ptr(ev);
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
				con_spawn_at(con_command(CON_CMD_MENU),
					     panel_span_x0(PANEL_HIT_START));
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
				con_spawn_at("kdos-cal",
					     panel_span_x0(PANEL_HIT_CLOCK));
			return;
		case PANEL_HIT_WS:
			if (ev->press == KT_MP_PRESS)
				win_workspace(arg);
			return;
		case PANEL_HIT_FKEY:
			/*
			 * THE CLICK IS THE CHORD, synthesised and fed to the
			 * same handler. Dispatching the action directly here
			 * would be a second copy of the mapping, and the row
			 * exists to teach the chord — so the day the two
			 * disagreed, the row would be teaching the wrong one.
			 */
			if (ev->press == KT_MP_PRESS) {
				KtuiEvent k = { 0 };

				k.type = KT_EVT_KEY;
				k.key = KT_K_F1 + (arg - 1);
				k.mods = KT_MOD_SUPER;
				session_key(&k);
			}
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
		/*
		 * THE TERMINAL'S OWN GRID, like every other consumer here. A
		 * window's cell (0,0) is at its origin, and the hit test that
		 * found this window used the FRAME rect — so a press on the
		 * border is outside the content and is dropped rather than
		 * clamped onto the first row.
		 */
		KtuiEvent in = *ev;

		in.mx -= w->geom.x;
		in.my -= w->geom.y;
		if (in.mx < 0 || in.my < 0 || in.mx >= w->geom.w ||
		    in.my >= w->geom.h)
			return;

		/* Middle-click pastes the primary before the terminal sees the
		 * button, the same order libkwl keeps: the click is still
		 * offered below, so a program tracking the mouse still gets
		 * it. */
		if (ev->btn == KT_MB_MIDDLE && ev->press == KT_MP_PRESS)
			term_paste(w, 1);
		term_mouse(w, &in);
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

/*
 * A FINGER, TO WHATEVER IT IS ON.
 *
 * THE SAME HIT TEST THE POINTER USES, and the icon layer last, for the reason
 * a drop keeps: `win_at()` skips the background on purpose — the icon layer
 * covers the whole grid — so a finger on the desktop would otherwise find
 * nothing and a long press there could never open the icon's menu.
 *
 * THE MOUSE THE RECOGNISER SYNTHESISED HAS ALREADY BEEN ROUTED as an ordinary
 * pointer event, which is what selects the row under the finger. This carries
 * only what a click cannot say — the gesture — and it is why a surface that
 * has never heard of touch still works under one.
 */
static void route_touch(const KtuiEvent *ev)
{
	Win *w;

	if (S.locked || S.saver)
		return;
	w = drag_target(ev->mx, ev->my);
	if (!w || !w->surf)
		return;
	kcon_surface_touch(w->surf, ev->mx - w->geom.x, ev->my - w->geom.y,
			   ev->slot, ev->phase, ev->ms, ev->gesture);
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
				/* THE MINIMUM IS RE-READ ON EVERY ATTACH: a
				 * surface that grew reports the new floor in
				 * the same message, and a session holding the
				 * first one would clamp it back. */
				w->min_w = kcon_surface_min_cols(f);
				w->min_h = kcon_surface_min_rows(f);
				if (!w->panel && !w->full &&
				    (kcon_surface_cols(f) != w->geom.w ||
				     kcon_surface_rows(f) != w->geom.h)) {
					/*
					 * AN ANCHORED OVERLAY IS RE-ANCHORED,
					 * not re-placed. `win_place` is the
					 * minimal-overlap search, and running
					 * a corner surface through it throws
					 * the anchor away: a stack of toasts
					 * that grew by one row walked out of
					 * the corner it asked for and into
					 * the middle of the screen.
					 */
					if (w->overlay &&
					    kcon_surface_corner(f))
						win_place_corner(
							w,
							kcon_surface_cols(f),
							kcon_surface_rows(f),
							kcon_surface_corner(f),
							kcon_surface_margin_x(f),
							kcon_surface_margin_y(f));
					else
						win_place(w,
							  kcon_surface_cols(f),
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
		w->min_w = kcon_surface_min_cols(f);
		w->min_h = kcon_surface_min_rows(f);
		snprintf(w->title, sizeof(w->title), "%s",
			 kcon_surface_title(f));
		snprintf(w->app_id, sizeof(w->app_id), "%s",
			 kcon_surface_app_id(f));
		/* A CELL CLIENT'S APP ID IS ITS PROGRAM. It is the one window
		 * kind whose identity comes from the client itself, and
		 * `kdos-term --app-id` is what makes a terminal entry declare
		 * the program it is running rather than the emulator. */
		snprintf(w->prog, sizeof(w->prog), "%s",
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
		 *
		 * AND SO IS THE POSITION. An overlay names a corner and its
		 * margins from that corner's two edges — the same field libkwl
		 * reads — so the Start menu opens above the Start button and a
		 * toast opens in the top right on both desktops. Placing a
		 * layer with the minimal-overlap search put the menu wherever
		 * there happened to be room, which for a menu is nowhere.
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
				win_place(w, cw, ch);
			} else {
				w->overlay = 1;
				win_place_corner(w, cw, ch,
						 kcon_surface_corner(f),
						 kcon_surface_margin_x(f),
						 kcon_surface_margin_y(f));
			}
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

		/*
		 * A RESTORED APPLICATION GOES BACK WHERE IT WAS. The row was
		 * read at startup and is taken the first time that app_id
		 * attaches; everything else is placed the ordinary way.
		 */
		int rw, rh, rx, ry, rws;
		char rfl[8];

		if (con_state_take(w->app_id, &rws, &rx, &ry, &rw, &rh, rfl,
				   sizeof(rfl))) {
			w->workspace = rws;
			win_place_at(w, rx, ry, rw, rh);
			con_state_apply_flags(w, rfl);
		} else {
			/* Before the placement, because it is what decides
			 * which placement this is. */
			w->floating = kcon_surface_floating(f);
			win_place(w, kcon_surface_cols(f),
				  kcon_surface_rows(f));
		}
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
 *
 * SET AT STARTUP, so the first turn of the loop applies the accent and the
 * night-light toggle. There is no second place that reads them: a session that
 * only ever retinted on the signal came up in the table's first scheme and
 * stayed there until somebody ran `kdos theme` again.
 */
static volatile sig_atomic_t g_retint = 1;

static void on_hup(int sig)
{
	(void)sig;
	g_retint = 1;
}

/*
 * A SIGNAL IS HOW A SESSION USUALLY ENDS, so it has to be a clean end.
 *
 * The quit verb is the tidy path and almost nobody takes it: a login ending
 * sends TERM, and with the default disposition the process dies where it
 * stands — every terminal's child is orphaned, a guest keeps the VT it was
 * given, and the state file is whatever it was last time. So the signal sets
 * the same flag the verb does and the loop leaves through the same door.
 *
 * A FLAG AND NOT THE WORK: closing guests and writing files inside a handler
 * is allocation and file IO inside a signal, and the loop is one tick away
 * from noticing.
 */
static volatile sig_atomic_t g_stop;

static void on_term(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void retint(void)
{
	char name[64];

	if (kcol_theme_name(name, sizeof(name)) && *name)
		ktui_theme_set(name);
	/* AFTER the scheme, because it transforms whatever the scheme just
	 * became — and unconditionally, because turning the toggle off is a
	 * retint too. */
	ktui_theme_night(kb_toggle_on("night-light"));

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
/*
 * ── PASTE INTO A WINDOW, GUARDED ────────────────────────────────────────
 *
 * With bracketed paste on, the child sees the text as text and decides for
 * itself. With it off, a newline EXECUTES — at a plain shell, at an `ssh`
 * password prompt, inside `read`. That is the one place a terminal can be made
 * to act as the user, and a clipboard that can arrive from a forwarded view is
 * what makes it something other than a thought experiment.
 *
 * THE SESSION CANNOT RAISE A MODAL over a window it does not own the toolkit
 * of, so the answer here is not a dialog: the paste is refused and the taskbar
 * says so, and the person presses the chord again within five seconds to mean
 * it. A confirmation nobody can see would be a refusal with no way past it.
 */
static struct {
	int win;
	unsigned long long until;
} paste_arm;

void con_paste_win(Win *w, const char *text)
{
	if (!w || !text || !*text)
		return;
	if (w->kind == WIN_SURFACE && w->surf) {
		kcon_surface_clip_data(w->surf, text);
		return;
	}
	if (w->kind != WIN_TERM || !w->term)
		return;
	if (kcon_conf_bool("paste_guard", 1) &&
	    kvt_term_paste_needs_confirm(w->term, text)) {
		unsigned long long now = con_now_ms();

		if (paste_arm.win != w->id || now > paste_arm.until) {
			paste_arm.win = w->id;
			paste_arm.until = now + 5000;
			ktui_draw_invalidate();
			return;
		}
	}
	paste_arm.win = 0;
	kvt_term_paste(w->term, text);
}

/* True while a paste is waiting to be meant twice, so the taskbar can say so. */
int con_paste_armed(void)
{
	return paste_arm.win && con_now_ms() <= paste_arm.until;
}

/*
 * ── WHAT IS ON THE SCREEN, AS TEXT ──────────────────────────────────────
 *
 * `kdos con capture`. The frame is composed FIRST rather than read as it
 * stands: the loop draws when something happened, so the last composed frame
 * can be older than the terminal's own output.
 *
 * WHAT IS ALREADY WRITTEN, NOT WHAT IS STILL COMING. `--dump` settles — it
 * runs every terminal until its child has EXITED — because there the children
 * are one-shot commands. A live session's shell never exits, so settling here
 * would hold the whole session for the length of the settle's own spin and
 * answer nothing until it gave up. One pump takes what the children have
 * already written, which is what "what is on the screen" means.
 *
 * A WINDOW'S RECTANGLE WHEN ASKED, by its ring number — the one on the title
 * bar and behind `Super+Alt+N`, because a person capturing "window 2" means
 * the one labelled 2. A number naming no window is nothing, not the screen:
 * silently widening a request to everything is how a script ends up publishing
 * what it did not mean to.
 */
static char *on_capture(KconSurface *f, int window, void *user)
{
	(void)f;
	(void)user;

	KRect r;
	int w = 0, h = 0;

	term_pump_all();
	composite();

	const KtuiCell *cells = ktui_draw_cells(&w, &h);

	if (!cells || w <= 0 || h <= 0)
		return NULL;

	if (window <= 0) {
		r = krect(0, 0, w, h);
	} else {
		Win *win = win_nth(window);

		if (!win)
			return NULL;
		/* The window manager's rectangle and the toolkit's are the
		 * same four numbers in two types; the conversion is here and
		 * not a cast, because a cast would compile after somebody
		 * reorders one of them. */
		r = krect(win->geom.x, win->geom.y, win->geom.w, win->geom.h);
	}
	if (r.x < 0)
		r.x = 0;
	if (r.y < 0)
		r.y = 0;
	if (r.x + r.w > w)
		r.w = w - r.x;
	if (r.y + r.h > h)
		r.h = h - r.y;
	if (r.w <= 0 || r.h <= 0)
		return NULL;

	/* Four bytes a cell is the widest UTF-8 this grid can hold, plus a
	 * newline a row and the terminator. */
	size_t cap = (size_t)r.w * (size_t)r.h * 4 + (size_t)r.h + 1;
	char *out = malloc(cap);
	size_t n = 0;

	if (!out)
		return NULL;
	for (int y = r.y; y < r.y + r.h; y++) {
		size_t eol = n;

		for (int x = r.x; x < r.x + r.w; x++) {
			const KtuiCell *c = &cells[y * w + x];
			uint32_t ch = c->ch;

			/* A continuation cell is the right half of a glyph
			 * already written, and a sprite is a picture: neither
			 * is a character, and printing either would put a
			 * control byte or a duplicate into text somebody is
			 * about to pipe somewhere. */
			if (ch == KTUI_WIDE_CONT)
				continue;
			if (!ch || KTUI_IS_SPRITE(ch))
				ch = ' ';
			n += (size_t)ktui_utf8_encode(ch, out + n);
			if (ch != ' ')
				eol = n;
		}
		/* Trailing blanks are dropped: a screen is mostly empty on the
		 * right, and a capture full of padding is a capture nothing
		 * can diff. */
		n = eol;
		out[n++] = '\n';
	}
	out[n] = '\0';
	return out;
}

static void on_paste(KconSurface *v, const char *text, void *user)
{
	Win *w = win_focused();

	(void)v;
	(void)user;
	clip_put(text, strlen(text), 0);
	con_paste_win(w, text);
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

/*
 * A SHELL ASKED FOR A WINDOW STATE. The session's own verbs are TOGGLES, so
 * each is called only when the window is not already in the state asked for:
 * a panel redraws its rows from the list it was sent, and two clicks racing
 * one publication would otherwise leave the window in the state nobody chose.
 */
static void on_win_state(KconSurface *f, unsigned id, unsigned flag, int on,
			 void *user)
{
	Win *w = win_find((int)id);

	(void)f;
	(void)user;
	if (!w)
		return;
	switch (flag) {
	case KCON_TL_MINIMISED:
		if (on && !w->minimised)
			win_minimise(w);
		else if (!on && w->minimised)
			win_restore(w);
		break;
	case KCON_TL_MAXIMISED:
		/* A minimised window cannot show itself maximised, so it comes
		 * back first — otherwise Maximize on a minimised row does
		 * nothing a person can see. */
		if (on && w->minimised)
			win_restore(w);
		if (on != (w->tiled == KWM_EDGES_CARDINAL))
			win_maximise(w);
		break;
	case KCON_TL_FULLSCREEN:
		if (on && w->minimised)
			win_restore(w);
		if (on != (w->full != 0))
			win_fullscreen(w);
		break;
	default:
		break;
	}
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

/*
 * IS THE MACHINE ALLOWED TO GO IDLE? The `stay-awake` toggle says no while it
 * is on. `kb_toggle_on` is the one reader of these in the tree — a second copy
 * of the path is a second place for the toggle a person set to be looked for
 * where nothing wrote it.
 */
static int stay_awake(void)
{
	return kb_toggle_on("stay-awake");
}

static void idle_tick(void)
{
	unsigned long long idle = mono_ms() - I.last_ms;

	/*
	 * BEFORE ALL THREE STEPS, not the first only: somebody who suppressed
	 * the saver did not ask to be locked either, and a machine that locked
	 * during the presentation they turned the saver off for is the failure
	 * this toggle exists to prevent.
	 */
	if (stay_awake())
		return;

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
		/* Hidden counts as away for the same reason minimised does:
		 * this list is what a box is kept warm for, and a scratchpad
		 * nobody can see is not a window on the screen. */
		if (w->panel || w->minimised || w->hidden || !w->app_id[0])
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
/*
 * A LAYOUT, ASKED FOR OVER THE SOCKET. The session is the half that holds the
 * windows, so it is the half that can write what is open or put it back; the
 * client is only a name and a direction.
 */
static int on_layout(KconSurface *f, const char *name, int save, void *user)
{
	(void)f;
	(void)user;
	return save ? con_layout_save(name) : con_layout_load(name);
}

/*
 * ── A DRAG ACROSS THE SESSION ────────────────────────────────────────────
 *
 * A surface picked something up — `kdos-desk` handing over an icon's URI — and
 * from here it is the session's, because only the session knows what is under
 * the pointer. The payload is held HERE and not passed on until the release:
 * a drag crossing six windows would otherwise hand its bytes to all six, and
 * five of those are windows somebody was only passing over.
 *
 * THE SOURCE IS NOT A TARGET OF ITS OWN DRAG in any special way — it is asked
 * like every other window. A file manager that highlights its own drop zones
 * while dragging out of them is a file manager behaving correctly.
 */
static struct {
	int on;
	char mime[64];
	char *data;
	size_t len;
	unsigned src;		/* the window that picked it up          */
	unsigned over;		/* the window id under the pointer, or 0 */
} drag;

/* True while something is being carried across the session. The bar says so,
 * and Esc gives it back — a drag with no way out is a pointer that has stopped
 * answering. */
int con_dragging(void)
{
	return drag.on;
}

static void drag_end(void)
{
	free(drag.data);
	drag.data = NULL;
	drag.len = 0;
	drag.on = 0;
	drag.src = 0;
	drag.over = 0;
	drag.mime[0] = '\0';
}

static void on_drag_start(KconSurface *f, const char *mime, const char *data,
			  size_t len, void *user)
{
	(void)f;
	(void)user;
	drag_end();
	/* TWO TYPES AND NO OTHERS. A drag is a filename or a line of text on
	 * this desktop; anything else is a payload nothing here can act on,
	 * and accepting it would mean a highlight over targets that would
	 * refuse the drop. */
	if (!mime || (strcmp(mime, "text/plain") &&
		      strcmp(mime, "text/uri-list")))
		return;
	if (!data || !len || len > (1u << 20))
		return;
	drag.data = malloc(len + 1);
	if (!drag.data)
		return;
	memcpy(drag.data, data, len);
	drag.data[len] = '\0';
	drag.len = len;
	snprintf(drag.mime, sizeof(drag.mime), "%s", mime);
	for (Win *w = S.wins; w; w = w->next)
		if (w->surf == f)
			drag.src = w->id;
	drag.on = 1;
	ktui_draw_invalidate();
}

/*
 * True when the pointer event was the drag's. It owns the pointer while it is
 * on, for the reason the mark does: a press that fell through would raise a
 * window under the thing being carried.
 */
/*
 * WHAT A DRAG IS OVER, WHICH IS NOT WHAT A CLICK IS OVER.
 *
 * `win_at()` skips the background deliberately: the icon layer covers the whole
 * grid, so hit-testing it before the windows would take every click on the
 * desktop. A DROP is the one case where that layer is a target — it is where
 * the trash is — so it is asked last, after every window has declined.
 *
 * Without this the console's whole drag path is dead where it matters: a
 * release over the desktop finds nothing, is treated as a cancel, and
 * `drop_to_trash()` is never reached.
 */
static Win *drag_target(int x, int y)
{
	Win *t = win_at(x, y);

	if (t)
		return t;
	for (Win *w = S.wins; w; w = w->next)
		if (w->background && !w->hidden && w->surf)
			return w;
	return NULL;
}

/* The drag's own coordinates, clamped into the window they are for. `win_at()`
 * hit-tests the frame, which is a cell wider than the content on every side, so
 * a release on a border would otherwise hand a surface a negative position. */
static void drag_local(const Win *w, int x, int y, int *lx, int *ly)
{
	*lx = x - w->geom.x;
	*ly = y - w->geom.y;
	if (*lx < 0)
		*lx = 0;
	if (*ly < 0)
		*ly = 0;
	if (*lx >= w->geom.w)
		*lx = w->geom.w - 1;
	if (*ly >= w->geom.h)
		*ly = w->geom.h - 1;
}

static int drag_ptr(const KtuiEvent *ev)
{
	Win *t;
	unsigned id;
	int lx, ly;

	if (!drag.on)
		return 0;

	/*
	 * THE SOURCE WENT AWAY. A client that died mid-drag leaves a payload
	 * nobody is carrying, and a drag left on would swallow every pointer
	 * event until some later release dropped a dead program's bytes onto
	 * whatever happened to be under the pointer.
	 */
	if (drag.src && !win_find((int)drag.src)) {
		drag_end();
		return 0;
	}

	t = drag_target(ev->mx, ev->my);
	id = t ? t->id : 0;

	if (id != drag.over) {
		Win *prev = drag.over ? win_find((int)drag.over) : NULL;

		if (prev && prev->surf)
			kcon_surface_drag_leave(prev->surf);
		drag.over = id;
		if (t && t->surf) {
			drag_local(t, ev->mx, ev->my, &lx, &ly);
			kcon_surface_drag_enter(t->surf, lx, ly, drag.mime);
		}
	} else if (t && t->surf && ev->press == KT_MP_DRAG) {
		drag_local(t, ev->mx, ev->my, &lx, &ly);
		kcon_surface_drag_motion(t->surf, lx, ly);
	}

	if (ev->press == KT_MP_RELEASE) {
		/* THE PAYLOAD, ONCE, TO WHATEVER IT WAS LET GO OVER. A release
		 * over nothing is a drag cancelled, which is what every desktop
		 * has meant by it. */
		if (t && t->surf) {
			drag_local(t, ev->mx, ev->my, &lx, &ly);
			kcon_surface_drop(t->surf, lx, ly, drag.data);
		}
		drag_end();
		ktui_draw_invalidate();
	}
	return 1;
}

/*
 * THE COLOUR PICK, ASKED FOR OVER THE SOCKET. The session finishes it: it has
 * the composed frame, the pointer and the clipboard, and the caller has none
 * of the three.
 */
static void on_pick(KconSurface *f, void *user)
{
	(void)f;
	(void)user;
	pick_begin();
}

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

/*
 * The session's own name, for the state file. A session started with an
 * explicit socket and no name is `con`, the same default every verb resolves
 * to — a state file named after the socket path would move whenever the
 * runtime directory did.
 */
static const char *S_name = "con";

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

	/*
	 * AND THE READER'S SOCKET. A third listener whose clients are views
	 * that may not drive: this desktop holds the literal text of every
	 * cell, so reading the screen is a loop over a buffer that already
	 * exists rather than a tree of objects somebody hopes matches what
	 * was drawn. A session that cannot open it still runs — a reader is
	 * something a person adds, not something the desktop needs.
	 */
	{
		/*
		 * FROM THE VIEW SOCKET WHERE THERE IS ONE, and from the
		 * surface socket where there is not: a session started with
		 * `--socket` alone still has a reader's socket, because being
		 * readable must not depend on somebody having asked for a
		 * display as well.
		 */
		char a11y[192];
		const char *from = view && *view ? view : sock;
		size_t len = strlen(from);
		char base[192];

		kb_strlcpy(base, from, sizeof(base));
		if (len > 5 && !strcmp(base + len - 5, ".sock"))
			kb_strlcpy(base + len - 5, ".view", sizeof(base) - len + 5);
		if (con_a11y_path(base, a11y, sizeof(a11y)) == 0)
			kcon_server_listen(S.server, a11y, KCON_LISTEN_A11Y);
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
	h.win_state = on_win_state;
	h.capture = on_capture;
	h.layout = on_layout;
	h.pick = on_pick;
	h.drag_start = on_drag_start;
	h.view_touch = on_view_touch;
	h.view_fonts = on_view_fonts;
	h.fonts_ask = on_fonts_ask;
	h.font_set = on_font_set;
	kcon_server_hooks(S.server, &h, NULL);
	/* HOW MANY DISPLAYS AT ONCE. The number is this desktop's and the
	 * refusal is the server's, because it is the end that sees a view
	 * arrive; 0 is no limit, which is what a session that never set the
	 * key has always had. */
	kcon_server_view_max(S.server, kcon_conf_int("views", 0));

	ktui_backend_set(&con_backend);
	ktui_draw_init();

	/*
	 * WHAT WAS OPEN LAST TIME, when `restore` says so. After the backend
	 * and the draw layer, because a restored window is placed against a
	 * screen size — and before the loop, so the first frame a view sees is
	 * the desktop as it was rather than an empty one that fills in.
	 */
	if (kcon_conf_bool("restore", 0))
		con_state_restore(S_name);
	signal(SIGHUP, on_hup);
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	idle_init();

	while (!quit) {
		/* The signal's half of con_quit(), out of the handler: a
		 * session asked to stop closes its guests and its embedded
		 * compositors like any other quit. */
		if (g_stop) {
			con_quit();
			break;
		}
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
		 * instead of waiting for the next tick. A descriptor that is
		 * not one — a client with no socket yet — is SKIPPED rather
		 * than pushed as -1: poll ignores those, so they would spend
		 * the budget below and the loop would wake on nothing.
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

		/* And a notice ends on a frame for the same reason: nothing
		 * else changes when its deadline passes. */
		if (notice_until && notice_until <= mono_ms()) {
			notice_until = 0;
			ktui_draw_invalidate();
		}

		if (g_retint) {
			g_retint = 0;
			retint();
		}
		idle_tick();
		scr_pump();

		kcon_server_pump(S.server);
		term_pump_all();
		adopt_surfaces();
		vt_reap();
		embed_reap();
		win_gc();
		publish_windows();
		mgmt_publish(0);
		publish_caret();
		publish_announce();

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
			 * A GRID THAT MOVED MAY BE A CELL THAT MOVED. A font
			 * step is announced as a resize — that is what it is,
			 * once the mode is divided by the new cell — and every
			 * picture on the far side was cut for the old one, so
			 * the view dropped them and is waiting to be sent them
			 * again. Blocks are re-cut here for the same reason:
			 * an embedded guest is sized in pixels from the cell.
			 */
			embed_view_attached();
			kcon_server_resend_sprites(S.server);

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
					w->geom = kwm_fit(w->geom, area, w->min_w, w->min_h);
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
			else if (ev.type == KT_EVT_TOUCH)
				route_touch(&ev);
		}

		/* THE PICTURES BEFORE THE CELLS THAT NAME THEM. A commit
		 * referring to a sprite a view has not been sent draws the
		 * fallback mark for a frame. */
		embed_pump();

		/* A view that has just attached has seen nothing, and its own
		 * previous frame is what decides how much it is sent. */
		focus_publish();
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

	/*
	 * SAVED ON THE WAY OUT, and only on this path: reaching here means the
	 * loop ended because the session was asked to quit. A session that was
	 * killed keeps the state file it had, which is the one from the last
	 * time it left cleanly — better than the empty list a crash would
	 * otherwise write over it.
	 */
	if (kcon_conf_bool("restore", 0))
		con_state_save(S_name);

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
		/* `--kms` AND NOT `--tty`: the view PROBES for a DRM device
		 * and a seat, takes the screen when it can, and says on
		 * stderr which of the two it chose. A grid that always took
		 * the terminal was a grid that could not use the screen it
		 * was started from, and nothing said so. */
		execlp("kdos-view", "kdos-view", "--kms", "--socket", view,
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
	const char *lay_name = NULL;
	int lay_save = 0;
	int do_cliptext = 0, do_pick = 0, do_cliptake = 0;
	int do_attach = 0, do_kill = 0, do_detach = 0, do_run = 0;
	int do_capture = 0, cap_win = 0, do_observe = 0;
	const char *conf_key = NULL;
	const char *tname = NULL;
	const char *login_tty = NULL;
	const char *terms[8];
	int nterms = 0;
	/*
	 * CHORDS TO PRESS BEFORE THE FRAME IS COMPOSITED, so a golden can be
	 * taken of what a key DOES rather than only of what a window looks
	 * like. The alternative is a flag per behaviour — one that opens a
	 * scratchpad, one that tiles — and every one of those is a second path
	 * into the code the chord already reaches.
	 */
	const char *presses[8];
	int npress = 0;
	/* A named arrangement, applied to the frame a dump composites. What it
	 * proves is the file and the placement, not the socket verb — a dump
	 * is a second session of its own, which is the whole difference
	 * between it and `--capture`. */
	const char *layout = NULL;
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
		 * A CLIENT OF THE SESSION THAT IS ALREADY RUNNING, not a
		 * session of its own — which is the whole difference between
		 * these two and `--layout` below. `kdos con layout save|load`
		 * is spelled this way for the reason every other `kdos con`
		 * verb is: the session binary is the one thing that already
		 * speaks the protocol.
		 */
		/*
		 * TEXT ONTO THE SESSION CLIPBOARD, READ FROM STDIN.
		 *
		 * NOT AN ARGUMENT. What a program copies is a password as often
		 * as it is a URL — a decoded QR most of all — and an argument
		 * vector is visible to every process on the machine for as long
		 * as this one runs. The pipe is the whole reason this is a flag
		 * rather than `kdos-con --clip-text <text>`.
		 */
		if (!strcmp(argv[i], "--clip-text")) {
			do_cliptext = 1;
			continue;
		}
		/*
		 * AND THE OTHER HALF. `wl-paste` is a Wayland client and this
		 * desktop has no Wayland, so a program that wants what was
		 * copied has nothing else to ask. Stdout rather than a toast
		 * or a file for the same reason `--clip-text` reads stdin: a
		 * clipboard is a password as often as it is a URL, and a pipe
		 * is the only one of the three nothing else can read.
		 */
		if (!strcmp(argv[i], "--clip-take")) {
			do_cliptake = 1;
			continue;
		}
		/*
		 * THE COLOUR UNDER THE POINTER, ONTO THE CLIPBOARD. It asks
		 * and returns: the answer arrives when a person clicks, and
		 * the session is what has the frame, the pointer and the
		 * clipboard. `kdos-shot colour` is what runs this.
		 */
		if (!strcmp(argv[i], "--pick-colour")) {
			do_pick = 1;
			continue;
		}
		if ((!strcmp(argv[i], "--layout-save") ||
		     !strcmp(argv[i], "--layout-load")) && i + 1 < argc) {
			lay_save = argv[i][9] == 's';
			lay_name = argv[++i];
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
		if (!strcmp(argv[i], "--capture")) {
			do_capture = 1;
			continue;
		}
		if (!strcmp(argv[i], "--observe")) {
			do_observe = 1;
			continue;
		}
		if (!strcmp(argv[i], "--conf") && i + 1 < argc) {
			conf_key = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "-w") && i + 1 < argc) {
			cap_win = atoi(argv[++i]);
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
		if (!strcmp(argv[i], "--layout") && i + 1 < argc) {
			layout = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--press") && i + 1 < argc) {
			if (npress < (int)(sizeof(presses) / sizeof(presses[0])))
				presses[npress++] = argv[++i];
			else
				i++;
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

	if (do_cliptext) {
		const char *sock = getenv("KDOS_CON");
		char buf[65536];
		size_t n = 0;
		ssize_t r;

		if (!sock || !*sock) {
			fprintf(stderr, "%s: no console session here — "
				"$KDOS_CON is unset\n", name);
			return 1;
		}
		while (n < sizeof(buf) - 1 &&
		       (r = read(STDIN_FILENO, buf + n, sizeof(buf) - 1 - n)) > 0)
			n += (size_t)r;
		buf[n] = '\0';
		/* A trailing newline would paste an Enter into whatever has the
		 * focus, which at a shell prompt runs the line. */
		while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
			buf[--n] = '\0';
		if (!n)
			return 1;
		return kcon_clip_offer(sock, buf, n) == 0 ? 0 : 1;
	}

	if (do_cliptake) {
		const char *sock = getenv("KDOS_CON");
		char *text = NULL;

		if (!sock || !*sock) {
			fprintf(stderr, "%s: no console session here — "
				"$KDOS_CON is unset\n", name);
			return 1;
		}
		if (kcon_clip_take(sock, &text) != 0)
			return 1;
		/* NO TRAILING NEWLINE. What comes back is the clipboard's
		 * bytes, and one added here is one every caller would have to
		 * know to take off again. */
		fputs(text, stdout);
		free(text);
		return 0;
	}

	if (do_pick) {
		const char *sock = getenv("KDOS_CON");

		if (!sock || !*sock) {
			fprintf(stderr, "%s: no console session here — "
				"$KDOS_CON is unset\n", name);
			return 1;
		}
		return kcon_pick_colour(sock) == 0 ? 0 : 1;
	}

	if (lay_name) {
		const char *sock = getenv("KDOS_CON");

		if (!sock || !*sock) {
			fprintf(stderr, "%s: no console session here — "
				"$KDOS_CON is unset\n", name);
			return 1;
		}

		int done = kcon_layout(sock, lay_name, lay_save);

		if (done < 0) {
			fprintf(stderr, "%s: no layout called '%s'\n", name,
				lay_name);
			return 1;
		}
		printf("%s %d window%s\n", lay_save ? "saved" : "opened",
		       done, done == 1 ? "" : "s");
		return 0;
	}

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

	/*
	 * ONE READER FOR `con.conf`, AND THE SHELL ASKS IT.
	 *
	 * `kdos-con-start` needs two of these keys to decide how to bring the
	 * desktop up, and a second parser written in shell would answer
	 * differently the first time a value was quoted or a comment moved.
	 * Printed raw with a newline: a script takes it with a command
	 * substitution and gets an empty line when the key is unset.
	 */
	if (conf_key) {
		printf("%s\n", kcon_conf_str(conf_key, ""));
		return 0;
	}

	/*
	 * THE SURFACE SOCKET, and this program is the client. A capture is a
	 * management verb: it asks the session that is already running rather
	 * than compositing a second one, which is what `--dump` does and why
	 * the two are different flags.
	 *
	 * BEFORE THE NAME IS RESOLVED, because `--socket` says exactly where
	 * to connect and resolving a name needs a runtime directory. A script
	 * capturing over a socket it was handed has no session of its own and
	 * must not need one — which is precisely how this failed in the build
	 * container, where there is no XDG_RUNTIME_DIR at all.
	 */
	if (do_capture) {
		char ssock[192], sview[192];
		const char *at = sock;
		char *txt = NULL;

		if (!at) {
			if (con_session_paths(tname ? tname : "con", ssock,
					      sizeof(ssock), sview,
					      sizeof(sview)) != 0)
				return 2;
			at = ssock;
		}
		if (kcon_capture(at, cap_win, &txt) != 0) {
			fprintf(stderr, "kdos-con: no session '%s'\n",
				tname ? tname : "con");
			return 1;
		}
		if (!txt || !*txt) {
			fprintf(stderr, "kdos-con: nothing to capture\n");
			free(txt);
			return 1;
		}
		fputs(txt, stdout);
		free(txt);
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
			/* `--kms` FOR THE SAME REASON kdos-grid uses it: the
			 * view probes and says which mode it took. An attach
			 * from a terminal on a machine with a free screen was
			 * pinned to that terminal by this argument alone. */
			if (do_observe)
				execlp("kdos-view", "kdos-view", "--kms",
				       "--observe", "--socket", sview,
				       (char *)NULL);
			execlp("kdos-view", "kdos-view", "--kms", "--socket",
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
		S_name = tname ? tname : "con";
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
		S_name = tname ? tname : "con";
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

	if (layout && con_layout_load(layout) < 0) {
		fprintf(stderr, "%s: no layout called '%s'\n", name, layout);
		return 1;
	}
	/*
	 * THROUGH THE CHORD TABLE, which is what makes this evidence: a press
	 * reaches the same handler a keyboard does, so a golden taken after one
	 * is a golden of the action rather than of a second implementation of
	 * it. A chord this session does not bind is a silent no-op for the same
	 * reason a typo in `keys.conf` is one.
	 */
	for (int i = 0; i < npress; i++) {
		KtuiEvent ev = { 0 };
		int key, mods;

		if (!keys_chord_parse(presses[i], &key, &mods)) {
			fprintf(stderr, "%s: cannot read the chord '%s'\n",
				name, presses[i]);
			return 2;
		}
		ev.type = KT_EVT_KEY;
		ev.key = key;
		ev.mods = mods;
		session_key(&ev);
	}

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
