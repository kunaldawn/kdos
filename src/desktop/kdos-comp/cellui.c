/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   cellui.c — the compositor's own popups
 *
 * Two of them, and they are here rather than in kdos-shell for the same reason
 * the frames are: STACKING ORDER and SURVIVAL.
 *
 *   - the WINDOW MENU belongs to a titlebar, which is compositor-drawn; a
 *     client asked to draw it would have to be told where every frame is
 *   - the ALT-TAB SWITCHER must work when the shell is dead. It is how you
 *     reach another window to fix things from, and a session whose window
 *     switcher dies with its panel is a session you have to kill
 *
 * Both are one cell grid in one wlr_scene_buffer, painted with libkcell —
 * exactly the machinery deco.c already uses, so a menu is the same typeface,
 * the same palette and the same pixel grid as the titlebar it came from.
 *
 *      ┌ mc ──────────────┐
 *      │  Move      Alt+F7│
 *      │  Resize    Alt+F8│
 *      │  Minimise  Alt+F9│
 *      │ ▸Maximise Alt+F10│
 *      │  Shade     Alt+F5│
 *      │  ─────────────── │
 *      │  Close     Alt+F4│
 *      └──────────────────┘
 *
 * The ownership rule is cellbuf.c's and is not repeated per popup:
 * create -> paint -> set_buffer -> drop.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kdos-comp.h"

/* ── the primitive ─────────────────────────────────────────────────────── */

static int popup_scale(struct kc_server *s, double x, double y)
{
	int sc = s->deco_scale;
	if (sc <= 0) {
		struct wlr_output *o =
			wlr_output_layout_output_at(s->output_layout, x, y);
		sc = o ? (int)(o->scale + 0.5f) : 1;
	}
	if (sc < 1)
		sc = 1;
	if (sc > KCELL_MAX_SCALE)
		sc = KCELL_MAX_SCALE;
	return sc;
}

static void popup_free(struct kc_popup *p)
{
	if (!p)
		return;
	if (p->node)
		wlr_scene_node_destroy(&p->node->node);
	for (int i = 0; i < 2; i++)
		if (p->shadow[i])
			wlr_scene_node_destroy(&p->shadow[i]->node);
	free(p->cells);
	free(p);
}

static struct kc_popup *popup_new(struct kc_server *s, int cols, int rows,
				  int scale)
{
	struct kc_popup *p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;
	p->s = s;
	p->cols = cols;
	p->rows = rows;
	p->scale = scale;
	p->cells = calloc((size_t)cols * rows, sizeof(*p->cells));
	if (!p->cells) {
		free(p);
		return NULL;
	}

	/*
	 * layer_above, so a popup is over the windows AND over the panels — a
	 * window menu that appeared under the taskbar would be unreachable for
	 * exactly the windows whose entry you just right-clicked.
	 */
	float black[4] = { 0.f, 0.f, 0.f, 0.5f };
	for (int i = 0; i < 2; i++)
		p->shadow[i] = wlr_scene_rect_create(s->layer_above, 1, 1, black);
	p->node = wlr_scene_buffer_create(s->layer_above, NULL);
	if (!p->node) {
		popup_free(p);
		return NULL;
	}
	return p;
}

static void popup_place(struct kc_popup *p, int x, int y)
{
	const int cw = kcell_w() * p->scale, ch = kcell_h() * p->scale;
	const int w = p->cols * cw, h = p->rows * ch;

	/*
	 * Clamp into the output it was opened on. A menu raised near the right
	 * edge would otherwise run off the screen, and the item you were aiming
	 * at is always the one that goes.
	 */
	struct wlr_box u;
	if (kc_usable_at(p->s, x, y, &u)) {
		if (x + w > u.x + u.width)
			x = u.x + u.width - w;
		if (y + h > u.y + u.height)
			y = u.y + u.height - h;
		if (x < u.x)
			x = u.x;
		if (y < u.y)
			y = u.y;
	}
	p->x = x;
	p->y = y;

	wlr_scene_node_set_position(&p->node->node, x, y);
	if (p->shadow[0]) {
		wlr_scene_node_set_position(&p->shadow[0]->node, x + w, y + ch);
		wlr_scene_rect_set_size(p->shadow[0], 2 * cw, h);
	}
	if (p->shadow[1]) {
		wlr_scene_node_set_position(&p->shadow[1]->node, x + 2 * cw, y + h);
		wlr_scene_rect_set_size(p->shadow[1], w - 2 * cw, ch);
	}
}

static void popup_flush(struct kc_popup *p)
{
	const int cw = kcell_w() * p->scale, ch = kcell_h() * p->scale;
	const int w = p->cols * cw, h = p->rows * ch;

	struct kc_cellbuf *cb = kc_cellbuf_create(w, h, true, true);
	if (!cb)
		return;
	kcell_paint(kc_cellbuf_image(cb), p->cells, NULL, p->cols, p->rows, 1,
		    p->scale, w, h);
	wlr_scene_buffer_set_buffer(p->node, kc_cellbuf_base(cb));
	wlr_scene_buffer_set_dest_size(p->node, w, h);
	kc_cellbuf_drop(cb);
	wlr_scene_node_raise_to_top(&p->node->node);
}

static void cell(struct kc_popup *p, int x, int y, uint32_t ch, uint8_t fg,
		 uint8_t bg)
{
	if (x < 0 || y < 0 || x >= p->cols || y >= p->rows)
		return;
	KtuiCell *c = &p->cells[(size_t)y * p->cols + x];
	c->ch = ch;
	c->fg = fg;
	c->bg = bg;
	c->attr = KT_A_NONE;
}

/* ASCII only — every string this file draws is one of its own, so there is no
 * UTF-8 to decode and no width to measure. */
static void text(struct kc_popup *p, int x, int y, const char *s, uint8_t fg,
		 uint8_t bg)
{
	for (int i = 0; s[i]; i++)
		cell(p, x + i, y, (unsigned char)s[i], fg, bg);
}

static void fill(struct kc_popup *p, uint8_t bg)
{
	for (int i = 0; i < p->cols * p->rows; i++) {
		p->cells[i].ch = ' ';
		p->cells[i].fg = KT_TEXT;
		p->cells[i].bg = bg;
	}
}

/* A single-line box with an optional title in the top rule. Single rather than
 * double: a popup is transient and must not compete with the focused window's
 * frame, which is the one thing on screen wearing the double rule. */
static void box(struct kc_popup *p, const char *title, uint8_t line)
{
	const uint8_t bg = KT_SURFACE;
	for (int x = 1; x < p->cols - 1; x++) {
		cell(p, x, 0, 0x2500, line, bg);
		cell(p, x, p->rows - 1, 0x2500, line, bg);
	}
	for (int y = 1; y < p->rows - 1; y++) {
		cell(p, 0, y, 0x2502, line, bg);
		cell(p, p->cols - 1, y, 0x2502, line, bg);
	}
	cell(p, 0, 0, 0x250c, line, bg);
	cell(p, p->cols - 1, 0, 0x2510, line, bg);
	cell(p, 0, p->rows - 1, 0x2514, line, bg);
	cell(p, p->cols - 1, p->rows - 1, 0x2518, line, bg);

	if (title && *title) {
		int max = p->cols - 4;
		if (max > 0) {
			cell(p, 1, 0, ' ', line, bg);
			int i = 0;
			for (; title[i] && i < max; i++)
				cell(p, 2 + i, 0, (unsigned char)title[i],
				     KT_TEXT, bg);
			cell(p, 2 + i, 0, ' ', line, bg);
		}
	}
}

/* ── the window menu ───────────────────────────────────────────────────── */

struct menu_item {
	const char *label;
	const char *key;
	enum kc_menu_action act;
	bool separator;
};

/* The order is GNOME 2's, which is also Turbo Vision's: the two things you
 * reach for most at the top, the destructive one alone at the bottom behind a
 * rule. */
static const struct menu_item MENU[] = {
	{ "Move",     "Alt+F7",  KC_MENU_MOVE,     false },
	{ "Resize",   "Alt+F8",  KC_MENU_RESIZE,   false },
	{ "Minimise", "Alt+F9",  KC_MENU_MINIMIZE, false },
	{ "Maximise", "Alt+F10", KC_MENU_MAXIMIZE, false },
	{ "Shade",    "Alt+F5",  KC_MENU_SHADE,    false },
	{ NULL,       NULL,      KC_MENU_NONE,     true  },
	{ "Close",    "Alt+F4",  KC_MENU_CLOSE,    false },
};
#define MENU_N ((int)(sizeof(MENU) / sizeof(MENU[0])))

static void menu_paint(struct kc_server *s)
{
	struct kc_popup *p = s->menu;
	struct kc_toplevel *t = s->menu_for;
	if (!p || !t)
		return;

	fill(p, KT_SURFACE);
	box(p, t->xdg_toplevel->title, KT_DIM);

	for (int i = 0; i < MENU_N; i++) {
		int y = 1 + i;
		if (MENU[i].separator) {
			for (int x = 1; x < p->cols - 1; x++)
				cell(p, x, y, 0x2500, KT_DIM, KT_SURFACE);
			continue;
		}
		bool sel = i == s->menu_sel;
		uint8_t fg = sel ? KT_BG : KT_TEXT;
		uint8_t bg = sel ? KT_ACCENT : KT_SURFACE;

		for (int x = 1; x < p->cols - 1; x++)
			cell(p, x, y, ' ', fg, bg);

		/* The state is in the LABEL, not in a checkmark: "Restore" and
		 * "Unshade" say what the item will do, which a tick beside
		 * "Maximise" does not. */
		const char *label = MENU[i].label;
		if (MENU[i].act == KC_MENU_MAXIMIZE && t->maximized)
			label = "Restore";
		if (MENU[i].act == KC_MENU_SHADE && t->shaded)
			label = "Unshade";

		text(p, 2, y, label, fg, bg);
		int klen = (int)strlen(MENU[i].key);
		text(p, p->cols - 2 - klen, y, MENU[i].key,
		     sel ? KT_BG : KT_DIM, bg);
	}
	popup_flush(p);
}

void kc_menu_open(struct kc_toplevel *t, double x, double y)
{
	struct kc_server *s = t->server;
	kc_menu_close(s);

	int scale = popup_scale(s, x, y);
	/* Widest label plus widest key plus the padding and the two borders. */
	int cols = 20;
	for (int i = 0; i < MENU_N; i++)
		if (!MENU[i].separator) {
			int n = (int)strlen(MENU[i].label) +
				(int)strlen(MENU[i].key) + 6;
			if (n > cols)
				cols = n;
		}

	s->menu = popup_new(s, cols, MENU_N + 2, scale);
	if (!s->menu)
		return;
	s->menu_for = t;
	s->menu_sel = 0;
	popup_place(s->menu, (int)x, (int)y);
	menu_paint(s);
}

void kc_menu_close(struct kc_server *s)
{
	popup_free(s->menu);
	s->menu = NULL;
	s->menu_for = NULL;
}

bool kc_menu_open_p(const struct kc_server *s)
{
	return s->menu != NULL;
}

static void menu_activate(struct kc_server *s)
{
	struct kc_toplevel *t = s->menu_for;
	int sel = s->menu_sel;
	if (!t || sel < 0 || sel >= MENU_N || MENU[sel].separator) {
		kc_menu_close(s);
		return;
	}
	enum kc_menu_action act = MENU[sel].act;
	/*
	 * Closed BEFORE the action runs. Two of these destroy or reparent the
	 * window the menu belongs to, and a menu still holding kc_menu_for is a
	 * pointer to something that may be gone by the time it is read.
	 */
	kc_menu_close(s);

	switch (act) {
	case KC_MENU_MOVE:     kc_window_kbgrab(t, false); break;
	case KC_MENU_RESIZE:   kc_window_kbgrab(t, true);  break;
	case KC_MENU_MINIMIZE: kc_window_minimize(t, true); break;
	case KC_MENU_MAXIMIZE: kc_window_maximize(t, !t->maximized); break;
	case KC_MENU_SHADE:    kc_window_shade(t, !t->shaded); break;
	case KC_MENU_CLOSE:    kc_window_close(t); break;
	case KC_MENU_NONE:     break;
	}
}

/* Skip separators in whichever direction we are going, so a separator can never
 * be the selected row. */
static void menu_step(struct kc_server *s, int dir)
{
	int n = MENU_N;
	for (int i = 0; i < n; i++) {
		s->menu_sel = (s->menu_sel + dir + n) % n;
		if (!MENU[s->menu_sel].separator)
			return;
	}
}

bool kc_menu_key(struct kc_server *s, xkb_keysym_t sym)
{
	if (!s->menu)
		return false;
	switch (sym) {
	case XKB_KEY_Escape:
		kc_menu_close(s);
		return true;
	case XKB_KEY_Up:
		menu_step(s, -1);
		menu_paint(s);
		return true;
	case XKB_KEY_Down:
		menu_step(s, 1);
		menu_paint(s);
		return true;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		menu_activate(s);
		return true;
	default:
		/* Anything else dismisses it. A menu that swallowed every key
		 * would be a session that looks frozen to anyone who opened one
		 * by accident. */
		kc_menu_close(s);
		return true;
	}
}

/* Which row the pointer is on, or -1. */
static int menu_row_at(struct kc_server *s, double x, double y)
{
	struct kc_popup *p = s->menu;
	const int cw = kcell_w() * p->scale, ch = kcell_h() * p->scale;
	if (x < p->x || y < p->y || x >= p->x + p->cols * cw ||
	    y >= p->y + p->rows * ch)
		return -1;
	int row = (int)((y - p->y) / ch) - 1;
	if (row < 0 || row >= MENU_N || MENU[row].separator)
		return -1;
	return row;
}

bool kc_menu_motion(struct kc_server *s, double x, double y)
{
	if (!s->menu)
		return false;
	int row = menu_row_at(s, x, y);
	if (row >= 0 && row != s->menu_sel) {
		s->menu_sel = row;
		menu_paint(s);
	}
	/* True even when the pointer is off the menu: while one is open the
	 * pointer belongs to the compositor, and letting motion through would
	 * light up hover states in a window the click cannot reach. */
	return true;
}

bool kc_menu_button(struct kc_server *s, double x, double y, bool pressed)
{
	if (!s->menu)
		return false;
	if (!pressed)
		return true;

	int row = menu_row_at(s, x, y);
	if (row < 0) {
		kc_menu_close(s);	/* a click outside dismisses */
		return true;
	}
	s->menu_sel = row;
	menu_activate(s);
	return true;
}

/* ── the alt-tab switcher ──────────────────────────────────────────────── */

#define SWITCH_MAX_ROWS 12
#define SWITCH_COLS 44

void kc_switcher_hide(struct kc_server *s)
{
	popup_free(s->switcher);
	s->switcher = NULL;
}

/*
 * Redrawn on every step rather than scrolled.
 *
 * The list is the MRU order, which is what alt-tab walks, and it is capped —
 * a user with forty windows open does not read a forty-row list, and a popup
 * taller than the screen is worse than a short one.
 */
void kc_switcher_show(struct kc_server *s)
{
	struct kc_toplevel *t;
	int n = 0;

	wl_list_for_each(t, &s->toplevels, link)
		if (t->ws == s->cur_ws && ++n >= SWITCH_MAX_ROWS)
			break;
	if (n == 0) {
		kc_switcher_hide(s);
		return;
	}

	if (!s->switcher) {
		int scale = popup_scale(s, s->cursor->x, s->cursor->y);
		s->switcher = popup_new(s, SWITCH_COLS, n + 2, scale);
		if (!s->switcher)
			return;
	} else if (s->switcher->rows != n + 2) {
		/* The count changed under us — a window closed mid-cycle. The
		 * grid is a fixed allocation, so it is rebuilt rather than
		 * resized. */
		int scale = s->switcher->scale;
		popup_free(s->switcher);
		s->switcher = popup_new(s, SWITCH_COLS, n + 2, scale);
		if (!s->switcher)
			return;
	}

	struct kc_popup *p = s->switcher;
	fill(p, KT_SURFACE);
	box(p, "Windows", KT_ACCENT);

	int row = 0;
	wl_list_for_each(t, &s->toplevels, link) {
		if (t->ws != s->cur_ws)
			continue;
		if (row >= n)
			break;
		bool sel = t == s->cycle_at;
		uint8_t fg = sel ? KT_BG : KT_TEXT;
		uint8_t bg = sel ? KT_ACCENT : KT_SURFACE;

		for (int x = 1; x < p->cols - 1; x++)
			cell(p, x, 1 + row, ' ', fg, bg);

		const char *title = t->xdg_toplevel->title;
		if (!title || !*title)
			title = t->xdg_toplevel->app_id;
		if (!title)
			title = "(untitled)";

		/* Minimised windows are in the list — reaching one is most of
		 * what alt-tab is for — and are marked, because otherwise
		 * selecting one looks like nothing happened. */
		text(p, 2, 1 + row, t->minimized ? "-" : (sel ? ">" : " "),
		     fg, bg);
		for (int i = 0; title[i] && i < p->cols - 6; i++) {
			unsigned char c = (unsigned char)title[i];
			cell(p, 4 + i, 1 + row, c < 0x20 ? ' ' : c, fg, bg);
		}
		row++;
	}

	/* Centred on the output the pointer is on, which is the one the user is
	 * looking at — a switcher on the other monitor is a switcher nobody
	 * sees. */
	struct wlr_box u;
	if (kc_usable_at(s, s->cursor->x, s->cursor->y, &u)) {
		const int cw = kcell_w() * p->scale, ch = kcell_h() * p->scale;
		popup_place(p, u.x + (u.width - p->cols * cw) / 2,
			    u.y + (u.height - p->rows * ch) / 2);
	}
	popup_flush(p);
}

void kc_cellui_free(struct kc_server *s)
{
	kc_menu_close(s);
	kc_switcher_hide(s);
}
