/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-ime — the candidate window, in cells
 *
 *   ╔══════════════════════════════════════╗
 *   ║ nihao                                ║
 *   ║ 1.你好  2.尼豪  3.泥壕  4.拟好  ▸    ║
 *   ╚══════════════════════════════════════╝
 *
 * THE ONE THING ON THIS DESKTOP THAT WAS NOT CELLS. An input engine draws its
 * own candidate list with its own toolkit, which on a character grid is a
 * rounded antialiased panel sitting on top of a text-mode desktop. This draws
 * it the way everything else here is drawn: `libkchrome` furniture,
 * `libkcolor` slots, through `libkdisp` — so it is one surface on the Wayland
 * desktop and one surface on the console, and `kdos theme amber` moves it.
 *
 * IT SPEAKS kimpanel, which is the generic D-Bus user interface of the
 * input-method framework and the mechanism KDE's plasmoid and the GNOME
 * extension already use. So this is not an input method and knows nothing about
 * any language: the engine decides what the candidates are and this draws them.
 *
 * THERE CAN BE ONLY ONE kimpanel ON A BUS, and the protocol has no way to hand
 * the name back. So a second one refuses to start and says what owns the name,
 * rather than taking it and leaving whatever was drawing the candidates in a
 * state it cannot recover from.
 * ---------------------------------
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basu/sd-bus.h>

#include "kchrome.h"
#include "kdisp.h"
#include "shell.h"

/* The panel's own name and object, which the engine looks for. */
#define IME_BUS  "org.kde.impanel"
#define IME_PATH "/org/kde/impanel"
#define IME_IFACE "org.kde.impanel"

/*
 * WHERE THE ENGINE EMITS, and the path is NOT matched. fcitx5 5.1 exports its
 * object at `/kimpanel` and older panels documented `/kimpanel/inputmethod`;
 * the interface is the same in both, and matching on it alone is what makes
 * this work against either without a version test.
 */
#define IM_IFACE "org.kde.kimpanel.inputmethod"

/*
 * AND THE ENGINE CALLS METHODS TOO. The candidate list does not arrive as a
 * signal: fcitx5 sends it as `org.kde.impanel2.SetLookupTable` on the panel's
 * own object, so a panel that only listened would show a preedit with nothing
 * under it. The version-1 signals carry everything else.
 */
#define IME_IFACE2 "org.kde.impanel2"

#define IME_MAX_CAND 16
#define IME_COLS_MAX 96
#define IME_TEXT 256

static sd_bus *bus;

/*
 * What the engine last said. Every field is independently shown or hidden —
 * that is the protocol's shape, not a simplification: an engine may put an
 * auxiliary string up with no candidates, or candidates with no preedit.
 */
static struct {
	char preedit[IME_TEXT];
	char aux[IME_TEXT];
	char label[IME_MAX_CAND][16];
	char cand[IME_MAX_CAND][64];
	int ncand;
	int cursor;			/* the highlighted candidate, or -1 */
	int has_prev, has_next;
	int show_preedit, show_aux, show_table;
	int enabled;
	int spot_x, spot_y;		/* pixels, from the engine */
} S;

/* ── drawing ───────────────────────────────────────────────────────────── */

static int ime_rows(void)
{
	int rows = 0;

	if (!S.enabled)
		return 0;
	if (S.show_preedit && S.preedit[0])
		rows++;
	if (S.show_aux && S.aux[0])
		rows++;
	if (S.show_table && S.ncand > 0)
		rows++;
	return rows ? rows + 2 : 0;	/* the frame is a row top and bottom */
}

/* As wide as the longest line, within the grid. A candidate window that was
 * always the same width would be a box with a word in the corner. */
static int ime_cols(void)
{
	int w = 0;

	if (S.show_preedit && S.preedit[0])
		w = ktui_utf8_width(S.preedit);
	if (S.show_aux && S.aux[0]) {
		int a = ktui_utf8_width(S.aux);

		if (a > w)
			w = a;
	}
	if (S.show_table && S.ncand > 0) {
		int c = 0;

		for (int i = 0; i < S.ncand; i++)
			c += ktui_utf8_width(S.label[i]) +
			     ktui_utf8_width(S.cand[i]) + 2;
		if (S.has_prev || S.has_next)
			c += 2;
		if (c > w)
			w = c;
	}
	w += 4;				/* the frame and a space each side */
	if (w < 12)
		w = 12;
	if (w > IME_COLS_MAX)
		w = IME_COLS_MAX;
	return w;
}

static void ime_draw(void)
{
	KRect all = krect(0, 0, ktui_w, ktui_h);
	int y = 1;

	ktui_draw_fill(all, KT_SURFACE);
	ktui_draw_box(all, NULL, KT_ACCENT, KT_SURFACE, 0);

	/* NEVER ON THE BORDER. A display that could not give the surface the
	 * rows it asked for is not a failure — a console session places
	 * windows itself — but a row written where the frame is reads as a
	 * broken box rather than as a smaller one. */
	if (ktui_h < 3 || ktui_w < 4)
		return;

	if (S.show_preedit && S.preedit[0] && y < ktui_h - 1) {
		/*
		 * THE PREEDIT IS WHAT HAS BEEN TYPED and not yet committed, so
		 * it is the text slot with the caret's own underline — the
		 * same thing a terminal does with it.
		 */
		ktui_draw_text(2, y, ktui_w - 4, S.preedit, KT_TEXT,
			       KT_SURFACE, KT_A_UNDERLINE);
		y++;
	}
	if (S.show_aux && S.aux[0] && y < ktui_h - 1) {
		ktui_draw_text(2, y, ktui_w - 4, S.aux, KT_MID, KT_SURFACE, 0);
		y++;
	}
	if (S.show_table && S.ncand > 0 && y < ktui_h - 1) {
		int x = 2;

		int right = ktui_w - 2 - ((S.has_prev || S.has_next) ? 2 : 0);

		for (int i = 0; i < S.ncand && x < right; i++) {
			int sel = i == S.cursor;

			x += ktui_draw_text(x, y, right - x, S.label[i],
					    KT_DIM, KT_SURFACE, 0);
			x += ktui_draw_text(x, y, right - x, S.cand[i],
					    sel ? KT_BG : KT_TEXT,
					    sel ? KT_ACCENT : KT_SURFACE,
					    sel ? KT_A_BOLD : 0);
			x += ktui_draw_text(x, y, right - x, " ", KT_TEXT,
					    KT_SURFACE, 0);
		}
		/* A page marker rather than a scrollbar: the list is one row
		 * and there is nothing for a bar to be proportional to. */
		if (S.has_next && ktui_w > 4)
			ktui_draw_text(ktui_w - 3, y, 2, ktui_glyph[KT_G_RIGHT],
				       KT_DIM, KT_SURFACE, 0);
		else if (S.has_prev && ktui_w > 4)
			ktui_draw_text(ktui_w - 3, y, 2, ktui_glyph[KT_G_LEFT],
				       KT_DIM, KT_SURFACE, 0);
	}
}

/* ── what the engine says ──────────────────────────────────────────────── */

static void take_str(char *dst, size_t cap, const char *src)
{
	snprintf(dst, cap, "%s", src ? src : "");
}

static int on_preedit(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *text = NULL, *attr = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ss", &text, &attr) < 0)
		return 0;
	take_str(S.preedit, sizeof(S.preedit), text);
	return 0;
}

static int on_aux(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *text = NULL, *attr = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ss", &text, &attr) < 0)
		return 0;
	take_str(S.aux, sizeof(S.aux), text);
	return 0;
}

/* One string array into `out`, up to `max` entries. Returns how many. */
static int read_sa(sd_bus_message *m, char out[][64], size_t cap, int max)
{
	int n = 0;

	if (sd_bus_message_enter_container(m, 'a', "s") < 0)
		return 0;
	for (;;) {
		const char *s = NULL;
		int r = sd_bus_message_read(m, "s", &s);

		if (r <= 0)
			break;
		if (n < max)
			snprintf(out[n], cap, "%s", s ? s : "");
		n++;
	}
	sd_bus_message_exit_container(m);
	return n < max ? n : max;
}

static int read_sa16(sd_bus_message *m, char out[][16], size_t cap, int max)
{
	int n = 0;

	if (sd_bus_message_enter_container(m, 'a', "s") < 0)
		return 0;
	for (;;) {
		const char *s = NULL;
		int r = sd_bus_message_read(m, "s", &s);

		if (r <= 0)
			break;
		if (n < max)
			snprintf(out[n], cap, "%s", s ? s : "");
		n++;
	}
	sd_bus_message_exit_container(m);
	return n < max ? n : max;
}

/*
 * The candidate list, and it is a METHOD the engine calls rather than a signal
 * it emits:
 *
 *   SetLookupTable(as labels, as texts, as attrs, b hasPrev, b hasNext,
 *                  i cursor, i layout)
 *
 * The engine sends as many labels as candidates; a table longer than this can
 * draw is truncated rather than refused, because a truncated candidate list is
 * still usable and a missing one is not.
 */
static int on_table(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int nl, nc;
	int prev = 0, next = 0, pos = -1, layout = 0;

	(void)u;
	(void)e;

	nl = read_sa16(m, S.label, sizeof(S.label[0]), IME_MAX_CAND);
	nc = read_sa(m, S.cand, sizeof(S.cand[0]), IME_MAX_CAND);

	/* The attribute array, which this draws nothing from: an engine's
	 * per-candidate colours are its palette and this desktop has one. */
	sd_bus_message_skip(m, "as");
	sd_bus_message_read(m, "bbii", &prev, &next, &pos, &layout);

	S.ncand = nl < nc ? nl : nc;
	S.has_prev = prev;
	S.has_next = next;
	S.cursor = pos < S.ncand ? pos : -1;
	return sd_bus_reply_method_return(m, NULL);
}

/* SetSpotRect(x, y, w, h), also a method. */
static int method_spot_rect(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int x = 0, y = 0, w = 0, h = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "iiii", &x, &y, &w, &h) >= 0) {
		S.spot_x = x;
		S.spot_y = y + h;
	}
	return sd_bus_reply_method_return(m, NULL);
}

static int on_table_cursor(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int c = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "i", &c) < 0)
		return 0;
	S.cursor = c;
	return 0;
}

static int on_show_preedit(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int v = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "b", &v) >= 0)
		S.show_preedit = v;
	return 0;
}

static int on_show_aux(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int v = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "b", &v) >= 0)
		S.show_aux = v;
	return 0;
}

static int on_show_table(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int v = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "b", &v) >= 0)
		S.show_table = v;
	return 0;
}

/*
 * Enable(false) is the engine saying there is no input context at all, and it
 * must clear what is on the screen: a candidate list left up after the text
 * field went away is a panel floating over an unrelated window.
 */
static int on_enable(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int v = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "b", &v) < 0)
		return 0;
	S.enabled = v;
	if (!v) {
		S.preedit[0] = '\0';
		S.aux[0] = '\0';
		S.ncand = 0;
	}
	return 0;
}

static int on_spot(sd_bus_message *m, void *u, sd_bus_error *e)
{
	int x = 0, y = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ii", &x, &y) >= 0) {
		S.spot_x = x;
		S.spot_y = y;
	}
	return 0;
}

/* ── what the panel says ───────────────────────────────────────────────── */

/*
 * The panel's own interface carries signals TO the engine and no methods at
 * all — clicking a candidate, paging the list, moving the caret. It is exported
 * so those can be sent and so the engine can find the object at all.
 */
static const sd_bus_vtable impanel_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_SIGNAL("PanelCreated", "", 0),
	SD_BUS_SIGNAL("SelectCandidate", "i", 0),
	SD_BUS_SIGNAL("LookupTablePageUp", "", 0),
	SD_BUS_SIGNAL("LookupTablePageDown", "", 0),
	SD_BUS_SIGNAL("MovePreeditCaret", "i", 0),
	SD_BUS_SIGNAL("TriggerProperty", "s", 0),
	SD_BUS_SIGNAL("Exit", "", 0),
	SD_BUS_SIGNAL("ReloadConfig", "", 0),
	SD_BUS_SIGNAL("Configure", "", 0),
	SD_BUS_VTABLE_END
};

/*
 * Version two of the same object: the methods the engine CALLS. Only the ones
 * this can actually answer are declared — the engine reads the introspection to
 * decide what to send, so a method declared and ignored is a worse answer than
 * one that is absent.
 */
static const sd_bus_vtable impanel2_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("SetLookupTable", "asasasbbii", "", on_table,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetSpotRect", "iiii", "", method_spot_rect,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("PanelCreated2", "", 0),
	SD_BUS_VTABLE_END
};

static void emit(const char *iface, const char *name)
{
	sd_bus_emit_signal(bus, IME_PATH, iface, name, NULL);
}

/* ── the program ───────────────────────────────────────────────────────── */

static const struct {
	const char *member;
	sd_bus_message_handler_t fn;
} IM_SIGNALS[] = {
	{ "UpdatePreeditText", on_preedit },
	{ "UpdateAux", on_aux },
	{ "UpdateLookupTableCursor", on_table_cursor },
	{ "ShowPreedit", on_show_preedit },
	{ "ShowAux", on_show_aux },
	{ "ShowLookupTable", on_show_table },
	{ "Enable", on_enable },
	{ "UpdateSpotLocation", on_spot },
};

int ime_main(int argc, char **argv)
{
	const char *font = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else {
			fprintf(stderr, "usage: kdos-ime [--font NAME]\n");
			return 2;
		}
	}

	S.cursor = -1;
	S.enabled = 1;

	int r = sd_bus_open_user(&bus);

	if (r < 0) {
		fprintf(stderr, "kdos-ime: no session bus: %s\n", strerror(-r));
		return 1;
	}

	r = sd_bus_add_object_vtable(bus, NULL, IME_PATH, IME_IFACE,
				     impanel_vtable, NULL);
	if (r >= 0)
		r = sd_bus_add_object_vtable(bus, NULL, IME_PATH, IME_IFACE2,
					     impanel2_vtable, NULL);
	if (r < 0) {
		fprintf(stderr, "kdos-ime: cannot export %s: %s\n", IME_PATH,
			strerror(-r));
		return 1;
	}

	/*
	 * NO REPLACE_EXISTING, and no retry. The protocol has no way to hand
	 * the name back, so taking it from whatever is already drawing the
	 * candidates would leave that program owning nothing and still
	 * believing it is the panel.
	 */
	r = sd_bus_request_name(bus, IME_BUS, 0);
	if (r < 0) {
		sd_bus_creds *creds = NULL;
		const char *comm = NULL;

		/* NAME THE PROGRAM. "The name is taken" sends a person looking
		 * at the wrong thing; "fcitx5 has it" ends the search. */
		if (sd_bus_get_name_creds(bus, IME_BUS, SD_BUS_CREDS_COMM,
					  &creds) >= 0)
			sd_bus_creds_get_comm(creds, &comm);

		fprintf(stderr, "kdos-ime: %s is already owned%s%s — something "
				"else is drawing the candidate window\n",
			IME_BUS, comm ? " by " : "", comm ? comm : "");
		sd_bus_creds_unref(creds);
		return 1;
	}

	for (size_t i = 0; i < sizeof(IM_SIGNALS) / sizeof(IM_SIGNALS[0]); i++)
		sd_bus_match_signal(bus, NULL, NULL, NULL, IM_IFACE,
				    IM_SIGNALS[i].member, IM_SIGNALS[i].fn,
				    NULL);

	/*
	 * TELL THE ENGINE THE PANEL EXISTS. fcitx5 starts drawing its own
	 * candidate window until it sees this, and one that started before this
	 * program did would go on drawing it for the rest of the session.
	 */
	emit(IME_IFACE, "PanelCreated");
	emit(IME_IFACE2, "PanelCreated2");

	/*
	 * AN OVERLAY, BOTTOM CENTRE. The engine reports where the caret is in
	 * output pixels and layer-shell has no way to place a surface there —
	 * the compositor owns placement — so the honest position is the one a
	 * person is already looking near, rather than a computed one that is
	 * wrong on every output but the first.
	 */
	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.corner = KDISP_CORNER_BOTTOM_CENTER,
		.cols = 24,
		.rows = 3,
		.app_id = "kdos-ime",
		.font = font,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-ime: no display to draw on\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);
	sh_theme_watch();

	signal(SIGPIPE, SIG_IGN);

	int shown_rows = -1, shown_cols = -1;

	while (!kdisp_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}

		/* Drain the bus BEFORE deciding what to draw: a candidate list
		 * read on the way into poll would be shown a keystroke late,
		 * which on a phrase of ten characters is ten. */
		while (sd_bus_process(bus, NULL) > 0)
			;

		int rows = ime_rows();
		int cols = rows ? ime_cols() : 0;

		if (rows != shown_rows || cols != shown_cols) {
			/*
			 * WITH NOTHING TO SHOW THE SURFACE IS DESTROYED, not
			 * shrunk. A candidate window is up for a fraction of
			 * the time anything is typed, and a one-cell surface
			 * parked over somebody's editor is a mark they cannot
			 * explain.
			 */
			if (rows > 0)
				kdisp_overlay_show(cols, rows);
			else
				kdisp_overlay_hide();
			shown_rows = rows;
			shown_cols = cols;
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
			}
			ktui_draw_invalidate();
		}

		if (rows > 0) {
			ime_draw();
			ktui_draw_flush();
		}

		struct pollfd p[2];
		int n = 0;

		p[n].fd = sd_bus_get_fd(bus);
		p[n].events = (short)sd_bus_get_events(bus);
		p[n].revents = 0;
		n++;
		/*
		 * THE DISPLAY IS WATCHED AND PUMPED WHETHER OR NOT ANYTHING IS
		 * SHOWING. A candidate window is hidden most of the time, and a
		 * client that stopped reading while hidden is a client whose
		 * queue fills and whose session drops it — after which the next
		 * thing typed has nowhere to appear.
		 */
		if (kdisp_fd() >= 0) {
			p[n].fd = kdisp_fd();
			p[n].events = POLLIN;
			p[n].revents = 0;
			n++;
		}

		poll(p, (nfds_t)n, 200);
		kdisp_pump();
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
		}
	}

	kdisp_shutdown();
	sd_bus_unref(bus);
	return 0;
}
