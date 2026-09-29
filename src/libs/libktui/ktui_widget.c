/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — immediate-mode controls
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "ktui.h"

/* Frame state. Private, and reached only through the calls below: a field an
 * application sets by hand — `ui.consumed = 1` — makes that caller part of
 * this struct's shape, and no such caller survives a change to it. */
typedef struct {
	int focus;		/* id of the focused control               */
	int nfocus;		/* positional ids claimed this frame       */
	int clicked;		/* control id clicked this frame, -1 none  */
	int dblclick;
	int wheel;		/* -1 up, +1 down, 0 none                  */
	int wheel_id;		/* wheel-consuming control under the pointer */
	KtuiEvent ev;		/* event being dispatched this frame       */
	int consumed;
	int mx, my;		/* pointer position for hover              */
	KRect focus_rect;	/* where the focused control landed        */
	int focus_seen;
	/* Press capture: the id whose rect saw the left press keeps every
	 * subsequent motion until the release, however far the pointer strays —
	 * a slider or a text selection that loses its widget the moment the hand
	 * drifts a row is not a drag at all. `drag` is that id on the frames a
	 * captured drag arrived on and -1 otherwise; ktui_drag() reads it. */
	int capture;
	int drag;
	int in_frame;		/* between ktui_frame_begin and _end       */
} KtuiUi;

/* EVERY "none" FIELD RESTS AT -1, not at 0, because 0 is the first id
 * ktui_id() hands out: a zeroed `clicked` makes the first control of a
 * surface that has not begun a frame yet read as clicked and fire itself. */
static KtuiUi ui = {
	.capture = -1,
	.drag = -1,
	.clicked = -1,
	.wheel_id = -1,
};

#define MAX_HITS 512
#define MAX_RING 512

typedef struct {
	KRect r;
	int id;
	int wheel;		/* claims the wheel over its rect          */
} Hit;

static Hit hits_a[MAX_HITS], hits_b[MAX_HITS];
static int na, nb;
static Hit *cur_hits = hits_a, *prev_hits = hits_b;
static int *cur_n = &na, *prev_n = &nb;

static double last_click_t;
static int last_click_id = -1;

/* The hit list of the last frame ENDED, for ktui_hit_at(). It is the array
 * that frame filled; the next ktui_frame_begin() makes it `prev_hits`, which
 * nothing writes, and the one after that refills it — so it is dropped there. */
static const Hit *done_hits;
static int done_n;

/*
 * THE TAB RING IS THE IDS CLAIMED THIS FRAME, IN THE ORDER THEY WERE CLAIMED.
 * Positional ids happen to be their own ring positions; a hashed id is not a
 * position at all, so Tab walks this list and never does arithmetic on an id.
 * A control claimed past MAX_RING is drawn and clickable but Tab never
 * reaches it.
 *
 * `focus_at` is where the focused id stood in the ring the last time it was
 * seen. A focused control that stops being drawn hands the focus to whatever
 * now stands in its place rather than to the top of the page.
 */
static int ring[MAX_RING];
static int nring;
static int focus_at;
static unsigned long frame_no;	/* frames begun; ages the state store */

/*
 * THE ID STACK. Inside a pushed scope ktui_id() hashes the scope's seed with a
 * counter local to the scope, and the positional counter outside it does not
 * move — so a group drawn only some of the time leaves every id after it
 * where it was. Deeper than ID_DEPTH, pushes are counted (a pop still pairs
 * with its push) but share the deepest seed.
 */
#define ID_DEPTH 16
typedef struct {
	uint32_t seed;
	int n;
} IdScope;
static IdScope idstk[ID_DEPTH + 1];	/* [0] is unused: depth 0 is positional */
static int iddepth;

#define FNV_BASIS 2166136261u
#define FNV_PRIME 16777619u

/* A tag byte starts every piece hashed in, and a string piece keeps its
 * terminator: without both, push("ab") + id_str("c") and push("a") +
 * id_str("bc") would be one id, and so would a string and a counter that
 * happen to share bytes. */
static uint32_t fnv(uint32_t h, char tag, const void *p, size_t n)
{
	const unsigned char *b = p;

	h = (h ^ (unsigned char)tag) * FNV_PRIME;
	for (size_t i = 0; i < n; i++)
		h = (h ^ b[i]) * FNV_PRIME;
	return h;
}

static uint32_t id_seed(void)
{
	int d = iddepth < ID_DEPTH ? iddepth : ID_DEPTH;

	return d ? idstk[d].seed : FNV_BASIS;
}

/* Above the chrome range and never negative, because -1 is "none". */
static int id_hashed(uint32_t h)
{
	return (int)(KTUI_ID_HASHED | (h & 0x3fffffffu));
}

static int id_is_chrome(int id)
{
	return id >= KTUI_ID_CHROME && id < KTUI_ID_HASHED;
}

static void ring_add(int id)
{
	if (nring < MAX_RING)
		ring[nring++] = id;
}

static int ring_find(int id)
{
	for (int i = 0; i < nring; i++)
		if (ring[i] == id)
			return i;
	return -1;
}

/* The ring position for a focus the ring does not hold: where it last
 * stood, clamped onto the ring. */
static int ring_fallback(void)
{
	int i = focus_at;

	if (i >= nring)
		i = nring - 1;
	if (i < 0)
		i = 0;
	return i;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * The frame's announcements. Fixed, cleared below, and never allocated: see
 * ktui.h for why silence rather than a stale name is the failure mode.
 */
static KtuiA11y a11y[KTUI_A11Y_MAX];
static int na11y;

void ktui_announce(int role, const char *label, const char *value, int index,
		   int count)
{
	KtuiA11y *a;

	if (na11y >= KTUI_A11Y_MAX || role == KT_A11Y_NONE)
		return;
	a = &a11y[na11y++];
	a->role = role;
	a->index = index;
	a->count = count;
	kb_strlcpy(a->label, label ? label : "", sizeof(a->label));
	kb_strlcpy(a->value, value ? value : "", sizeof(a->value));
}

int ktui_announce_count(void)
{
	return na11y;
}

const KtuiA11y *ktui_announce_at(int i)
{
	return i >= 0 && i < na11y ? &a11y[i] : NULL;
}

void ktui_frame_begin(KtuiEvent *ev)
{
	/* SWAPPED BEFORE THE SCAN, because the click being dispatched belongs
	 * to the layout the caller drew LAST frame. Scanning first would match
	 * it against the frame before that, and every surface would swallow
	 * its first two clicks. */
	Hit *t = cur_hits;
	cur_hits = prev_hits;
	prev_hits = t;
	int *tn = cur_n;
	cur_n = prev_n;
	prev_n = tn;
	*cur_n = 0;
	if (done_hits == cur_hits)
		done_n = 0;

	na11y = 0;
	ui.ev = *ev;
	ui.consumed = 0;
	ui.clicked = -1;
	ui.dblclick = 0;
	ui.wheel = 0;
	ui.wheel_id = -1;
	ui.nfocus = 0;
	ui.focus_seen = 0;
	ui.drag = -1;
	ui.in_frame = 1;
	nring = 0;
	iddepth = 0;
	frame_no++;

	if (ev->type == KT_EVT_MOUSE) {
		ui.mx = ev->mx;
		ui.my = ev->my;
		int is_wheel = ev->btn == KT_MB_WHEEL_UP ||
			       ev->btn == KT_MB_WHEEL_DOWN;
		/* A wheel exists whether or not anything is under it: an
		 * unclaimed one falls through to the page, which is what
		 * ktui_wheel_take() collects. */
		if (is_wheel)
			ui.wheel = ev->btn == KT_MB_WHEEL_UP ? -1 : 1;
		if (!is_wheel && ev->press == KT_MP_DRAG && ui.capture >= 0) {
			ui.drag = ui.capture;
		} else for (int i = *prev_n - 1; i >= 0; i--) {
			if (!krect_hit(prev_hits[i].r, ev->mx, ev->my))
				continue;
			if (is_wheel) {
				/* Only a control that REGISTERED for the wheel
				 * claims it, and the scan keeps going past the
				 * ones that did not — a button drawn inside a
				 * list sits on top of the list's rect and would
				 * otherwise swallow the list's scrolling. */
				if (!prev_hits[i].wheel)
					continue;
				ui.wheel_id = prev_hits[i].id;
			} else if (ev->btn == KT_MB_LEFT && ev->press == KT_MP_PRESS) {
				ui.clicked = prev_hits[i].id;
				ui.capture = prev_hits[i].id;
				/* Chrome ids are not focus ids — clicking the
				 * sidebar must not throw the caret at whatever
				 * control happens to sit last on the page. */
				if (!id_is_chrome(prev_hits[i].id))
					ui.focus = prev_hits[i].id;
				double t = kb_now_s();
				if (last_click_id == ui.clicked && t - last_click_t < 0.4)
					ui.dblclick = 1;
				last_click_id = ui.clicked;
				last_click_t = t;
			}
			break;
		}
		if (!is_wheel && ev->press == KT_MP_RELEASE)
			ui.capture = -1;
	}
}

/*
 * THE FOCUS IS RESOLVED ONTO THE RING BEFORE TAB STEPS FROM IT. A focus the
 * ring does not hold — a control that stopped being drawn, or an id set
 * before its control existed — lands on whatever stands where it last stood,
 * so Tab never steps from a position nobody can see.
 */
static void focus_resolve(void)
{
	int i;

	if (!nring) {
		if (ui.focus < 0)
			ui.focus = 0;
		return;
	}
	i = ring_find(ui.focus);
	if (i < 0) {
		i = ring_fallback();
		ui.focus = ring[i];
	}
	focus_at = i;
}

void ktui_frame_end(void)
{
	focus_resolve();
	if (ui.ev.type == KT_EVT_KEY && !ui.consumed) {
		if (ui.ev.key == KT_K_TAB && !(ui.ev.mods & KT_MOD_SHIFT)) {
			ktui_focus_next(1);
			ui.consumed = 1;
		} else if (ui.ev.key == KT_K_BTAB ||
			   (ui.ev.key == KT_K_TAB && (ui.ev.mods & KT_MOD_SHIFT))) {
			ktui_focus_next(-1);
			ui.consumed = 1;
		}
	}
	ui.in_frame = 0;
	done_hits = cur_hits;
	done_n = *cur_n;
}

int ktui_hit_count(void)
{
	return done_n;
}

int ktui_hit_at(int i, KRect *r, int *id)
{
	if (i < 0 || i >= done_n)
		return 0;
	*r = done_hits[i].r;
	*id = done_hits[i].id;
	return 1;
}

/*
 * THREE KINDS OF ID, IN THREE RANGES. Positional ids count up from 0 at
 * depth 0 and stay below KTUI_ID_CHROME; chrome ids sit from KTUI_ID_CHROME;
 * hashed ids (ktui_id_str, and ktui_id inside a pushed scope) sit from
 * KTUI_ID_HASHED. A positional id is its control's place in the page, so a
 * control drawn only some of the time moves every positional id after it —
 * and focus, and the state keyed by them, move with the ids. Such a group is
 * wrapped in ktui_id_push()/ktui_id_pop().
 */
int ktui_id(void)
{
	int id;

	if (iddepth) {
		IdScope *s = &idstk[iddepth < ID_DEPTH ? iddepth : ID_DEPTH];

		id = id_hashed(fnv(s->seed, 'n', &s->n, sizeof(s->n)));
		s->n++;
	} else {
		id = ui.nfocus++;
		if (id >= KTUI_ID_CHROME)
			id = KTUI_ID_CHROME - 1;
	}
	ring_add(id);
	return id;
}

/* An id named by the caller, the same on every frame whatever else was drawn
 * before it. It claims a place in the Tab ring as ktui_id() does. */
int ktui_id_str(const char *s)
{
	int id = id_hashed(fnv(id_seed(), 's', s ? s : "", s ? strlen(s) + 1 : 1));

	ring_add(id);
	return id;
}

static void id_push(uint32_t seed)
{
	iddepth++;
	if (iddepth <= ID_DEPTH) {
		idstk[iddepth].seed = seed;
		idstk[iddepth].n = 0;
	}
}

void ktui_id_push(const char *s)
{
	id_push(fnv(id_seed(), 'S', s ? s : "", s ? strlen(s) + 1 : 1));
}

void ktui_id_push_int(int n)
{
	id_push(fnv(id_seed(), 'I', &n, sizeof(n)));
}

void ktui_id_pop(void)
{
	if (iddepth > 0)
		iddepth--;
}

/*
 * The id the next control will claim, for a group that has to point the focus
 * at one of its own members before drawing them. `ktui_id_base() + k` names
 * the k-th control of the group at depth 0 only: inside a pushed scope the
 * next id is a hash, and the one after it is not the next integer.
 *
 * Outside a frame the counter, the ring and the id stack are restarted first.
 * ktui_frame_begin() is what resets them, and a surface that runs its own
 * event loop and calls a draw/key pair never begins one, so its ids would
 * climb with every repaint until they all clamped to the top of the range
 * and every control in the group answered to the same id. Inside a frame the
 * counter belongs to the page and is only read — restarting it there would
 * hand two controls one id.
 */
int ktui_id_base(void)
{
	if (!ui.in_frame) {
		ui.nfocus = 0;
		nring = 0;
		iddepth = 0;
	}
	if (iddepth) {
		IdScope *s = &idstk[iddepth < ID_DEPTH ? iddepth : ID_DEPTH];

		return id_hashed(fnv(s->seed, 'n', &s->n, sizeof(s->n)));
	}
	return ui.nfocus;
}

/*
 * ────────────────────────────────────────────────────────────────────────
 * Per-id state
 *
 * A FIXED TABLE OF SMALL RECORDS, NEVER ALLOCATED: libktui is libc-only and a
 * frame caller holds nothing but its own values. Open addressing with linear
 * probing; a slot, once used, is never emptied, only taken over — a record
 * unseen for STATE_TTL frames is stale and the next new id to probe past it
 * claims it — so no probe chain is ever broken by an eviction.
 *
 * A record asked for at a different size is another life of its id and comes
 * back zeroed. NULL means the size is over KTUI_STATE_MAX or every slot is
 * live; a caller keeps working on a fallback of its own.
 * ────────────────────────────────────────────────────────────────────────
 */
#define STATE_SLOTS 256
#define STATE_TTL 600

typedef struct {
	int used;
	int id;
	size_t n;
	unsigned long seen;
	unsigned char d[KTUI_STATE_MAX];
} StateSlot;

static StateSlot store[STATE_SLOTS];

void *ktui_state(int id, size_t n)
{
	unsigned h = ((unsigned)id * 2654435761u) >> 24;	/* 8 bits */
	StateSlot *take = NULL;

	if (!n || n > KTUI_STATE_MAX)
		return NULL;
	for (int i = 0; i < STATE_SLOTS; i++) {
		StateSlot *s = &store[(h + (unsigned)i) % STATE_SLOTS];

		if (!s->used) {
			if (!take)
				take = s;
			break;
		}
		if (s->id == id) {
			if (s->n != n) {
				memset(s->d, 0, sizeof(s->d));
				s->n = n;
			}
			s->seen = frame_no;
			return s->d;
		}
		if (!take && frame_no - s->seen > STATE_TTL)
			take = s;
	}
	if (!take)
		return NULL;
	take->used = 1;
	take->id = id;
	take->n = n;
	take->seen = frame_no;
	memset(take->d, 0, sizeof(take->d));
	return take->d;
}

void ktui_hit(KRect r, int id)
{
	/* Remembering where the focused control ended up is what lets the
	 * page scroll itself to follow the Tab key. */
	if (id == ui.focus && !id_is_chrome(id)) {
		ui.focus_rect = r;
		ui.focus_seen = 1;
	}
	if (*cur_n >= MAX_HITS)
		return;
	cur_hits[*cur_n].r = r;
	cur_hits[*cur_n].id = id;
	cur_hits[*cur_n].wheel = 0;
	(*cur_n)++;
}

/* A control that SCROLLS registers with this instead, and everything else
 * lets the wheel through to the page under it. A rect that claimed the wheel
 * merely by being under the pointer would make a page with one list in it
 * unscrollable everywhere the list is. */
static void hit_wheel(KRect r, int id)
{
	int n = *cur_n;

	ktui_hit(r, id);
	if (*cur_n > n)
		cur_hits[n].wheel = 1;
}

int ktui_focused(int id)
{
	return ui.focus == id;
}

/* A positional id names a ring position too, so a focus set to one the page
 * does not reach resolves to the last control, as a count would clamp it.
 * Any other id not drawn this frame resolves to the first. */
void ktui_focus_set(int id)
{
	ui.focus = id;
	focus_at = id >= 0 && id < KTUI_ID_CHROME ? id : 0;
}

void ktui_focus_next(int dir)
{
	int i;

	if (!nring)
		return;
	i = ring_find(ui.focus);
	if (i < 0)
		i = ring_fallback();
	i = ((i + dir) % nring + nring) % nring;
	ui.focus = ring[i];
	focus_at = i;
}

int ktui_activated(int id, KRect r)
{
	ktui_hit(r, id);
	if (ui.clicked == id)
		return 1;
	if (ui.focus == id && !ui.consumed && ui.ev.type == KT_EVT_KEY &&
	    (ui.ev.key == KT_K_ENTER || ui.ev.key == ' ')) {
		ui.consumed = 1;
		return 1;
	}
	return 0;
}

/* Chrome — a sidebar, a tab bar, a close button. Registered in a reserved id
 * range so a click on one is still recognised, but it never joins the Tab
 * ring and never drags the page scroll after it. Chrome usually draws BEFORE
 * the page, so handing it real ids from ktui_id() would push every control on
 * every page N places down the ring and leave the caret parked on a
 * decoration. Ids are caller-local: each consumer numbers its own from 0. */
void ktui_hit_chrome(KRect r, int id)
{
	ktui_hit(r, KTUI_ID_CHROME + id);
}

int ktui_chrome_clicked(int id)
{
	return ui.clicked == KTUI_ID_CHROME + id;
}

/* ──────────────────────────────────────────────────────────────────────── */

const KtuiEvent *ktui_event(void)
{
	return &ui.ev;
}

int ktui_consumed(void)
{
	return ui.consumed;
}

void ktui_consume(void)
{
	ui.consumed = 1;
}

int ktui_focus_get(void)
{
	return ui.focus;
}

int ktui_clicked(void)
{
	return ui.clicked;
}

/* The id holding the press capture on this frame, -1 otherwise. A dragging
 * control must test THIS and not its own rect: the whole point of the capture
 * is that the pointer has left the rect. */
int ktui_drag(void)
{
	return ui.drag;
}

int ktui_mouse_x(void)
{
	return ui.mx;
}

int ktui_mouse_y(void)
{
	return ui.my;
}

/* The wheel that no scrolling control claimed, if the pointer is inside r.
 * `wheel_id >= 0` means a control registered for the wheel sits under the
 * pointer and has first refusal; the page gets what is left. */
int ktui_wheel_take(KRect r)
{
	if (!ui.wheel || ui.wheel_id >= 0 || !krect_hit(r, ui.mx, ui.my))
		return 0;
	int w = ui.wheel;
	ui.wheel = 0;
	return w;
}

int ktui_focus_rect(KRect *out)
{
	if (!ui.focus_seen)
		return 0;
	*out = ui.focus_rect;
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* ── the selection rule ────────────────────────────────────────────────────
 *
 * See ktui.h for the measurements. The short version: the fill is KT_DIM and
 * the label is KT_TEXT, which reads at 8.30:1 or better in every scheme; the
 * accent is spent on the one cell that says where the caret is, not on the
 * whole row.
 */
void ktui_sel_slots(int selected, int pane_focused, int page, int *fg, int *bg)
{
	/*
	 * THE CARET ON A PANE THAT LOST THE KEYBOARD KEEPS ITS MARKER AND
	 * LOSES ITS FILL. A surface with two panes has two carets, and filling
	 * both says both are live; dropping the cold one loses the place that
	 * pane will come back to.
	 */
	*fg = KT_TEXT;
	*bg = (selected && pane_focused) ? KT_DIM : page;
}

int ktui_sel_dim(int selected, int pane_focused)
{
	/* KT_MID off the fill and KT_TEXT on it. The derived muted colour
	 * measures 2.84-3.38:1 against KT_DIM, so a tag left muted on a
	 * selected row is a tag nobody can read. */
	return (selected && pane_focused) ? KT_TEXT : KT_MID;
}

void ktui_sel_row(KRect r, int selected, int pane_focused, int page_bg,
		  int *fg, int *bg)
{
	int f, b;

	ktui_sel_slots(selected, pane_focused, page_bg, &f, &b);
	ktui_draw_fill(r, b);
	/* The marker, and it is the only thing wearing the accent. On a cold
	 * pane it is KT_MID, which is what distinguishes "this is where the
	 * caret is" from "this is the pane you are typing into". */
	if (selected && r.w > 1)
		ktui_draw_text(r.x, r.y, 1, ktui_glyph[KT_G_ARROW_R],
			       pane_focused ? KT_ACCENT : KT_MID, b,
			       KT_A_NONE);
	*fg = f;
	*bg = b;
}

int ktui_button(KRect r, const char *label, int enabled, int primary)
{
	/* A disabled control claims no focus id, so Tab never parks on a Back
	 * button that cannot go anywhere. Enablement only flips on a page
	 * change, which resets focus anyway, so ids stay stable within a page. */
	int id = enabled ? ktui_id() : -1;
	int focus = enabled && ktui_focused(id);
	int hover = enabled && krect_hit(r, ui.mx, ui.my);

	int fg, bg;
	if (!enabled) {
		fg = KT_DIM;
		bg = KT_SURFACE;
	} else if (focus) {
		fg = KT_BG;
		bg = primary ? KT_ACCENT : KT_MID;
	} else if (primary) {
		fg = KT_ACCENT;
		bg = KT_SURFACE;
	} else {
		fg = hover ? KT_TEXT : KT_MID;
		bg = KT_SURFACE;
	}

	ktui_draw_fill(r, bg);
	int w = ktui_utf8_width(label);
	int x = r.x + (r.w - w) / 2;
	if (x < r.x)
		x = r.x;
	int cy = r.y + r.h / 2;
	ktui_draw_text(x, cy, r.w, label, fg, bg, 0);

	/* SAID WHERE FOCUS IS COMPUTED, not where it is drawn: a second place
	 * that worked out which control has focus is a second place to get it
	 * wrong. */
	if (focus)
		ktui_announce(KT_A11Y_BUTTON, label, NULL, 0, 0);

	/*
	 * THE FOCUSED BUTTON CARRIES MARKERS AND NOT ONLY A COLOUR. On a
	 * washed-out laptop panel a colour alone is not a focus indicator, and
	 * the markers are what a person looking for "which one does Enter
	 * press" finds. Solid triangles rather than the text arrows: this is
	 * control furniture, and `◀`/`▶` in running text mean direction.
	 */
	if (focus && r.w > 4) {
		ktui_draw_text(r.x, cy, 1, ktui_glyph[KT_G_ARROW_R], fg, bg, 0);
		ktui_draw_text(r.x + r.w - 1, cy, 1, ktui_glyph[KT_G_ARROW_L],
			       fg, bg, 0);
	}

	/*
	 * THE SHADOW IS WHAT MAKES A PLATE A BUTTON. Without it a filled
	 * rectangle with a word in it is indistinguishable from a selected list
	 * row — which is exactly the confusion a form full of both produces.
	 *
	 * It is drawn one column right and one row below, in KT_DIM on the
	 * page. `░` AND NOT A HALF BLOCK: `▀`/`▄` would hug the plate's edge
	 * and are not in the console font, so on `tty1` the shadow would be a
	 * blank strip — which reads as the button having a hole beside it. A
	 * DISABLED button casts none; it is not raised, because it cannot be
	 * pressed.
	 */
	if (enabled && r.w > 2) {
		int sy = r.y + r.h;

		ktui_draw_text(r.x + r.w, cy, 1, ktui_glyph[KT_G_SHADE],
			       KT_DIM, KT_BG, 0);
		for (int i = 1; i <= r.w; i++)
			ktui_draw_text(r.x + i, sy, 1, ktui_glyph[KT_G_SHADE],
				       KT_DIM, KT_BG, 0);
	}

	return enabled ? ktui_activated(id, r) : 0;
}

int ktui_check(int x, int y, int w, const char *label, int *val)
{
	int id = ktui_id();
	KRect r = krect(x, y, w, 1);
	int focus = ktui_focused(id);
	int fg, bg;

	/*
	 * THE FOCUSED BOX IS A QUIET PLATE AND THE MARK KEEPS THE ACCENT. A
	 * whole row filled with the accent put the background colour on the
	 * label and made the one cell that carries the STATE — the mark inside
	 * the brackets — the same colour as everything around it. The plate
	 * says where focus is; the mark says on or off, and they are different
	 * questions.
	 */
	ktui_sel_slots(1, focus, KT_BG, &fg, &bg);
	ktui_draw_fill(r, bg);
	ktui_draw_text(x, y, 1, "[", KT_MID, bg, 0);
	ktui_draw_text(x + 1, y, 1, *val ? ktui_glyph[KT_G_SQUARE] : " ",
		  KT_ACCENT, bg, 0);
	ktui_draw_text(x + 2, y, 1, "]", KT_MID, bg, 0);
	ktui_draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	/* The VALUE as well as the name: "on" and "off" is what the box says,
	 * and a reader given only the label has to guess which. */
	if (focus)
		ktui_announce(KT_A11Y_CHECK, label, *val ? "on" : "off", 0, 0);

	if (ktui_activated(id, r)) {
		*val = !*val;
		return 1;
	}
	return 0;
}

int ktui_radio(int x, int y, int w, const char *label, int *val, int on)
{
	int id = ktui_id();
	KRect r = krect(x, y, w, 1);
	int focus = ktui_focused(id);
	int sel = (*val == on);
	int fg, bg;

	/* Same rule as the checkbox: the plate is focus, the bullet is state. */
	ktui_sel_slots(1, focus, KT_BG, &fg, &bg);
	if (!focus && !sel)
		fg = KT_MID;		/* an unchosen option is secondary */
	ktui_draw_fill(r, bg);
	ktui_draw_text(x, y, 1, "(", KT_MID, bg, 0);
	ktui_draw_text(x + 1, y, 1, sel ? ktui_glyph[KT_G_BULLET] : " ",
		  KT_ACCENT, bg, 0);
	ktui_draw_text(x + 2, y, 1, ")", KT_MID, bg, 0);
	ktui_draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	if (focus)
		ktui_announce(KT_A11Y_RADIO, label,
			      sel ? "selected" : "not selected", 0, 0);

	if (ktui_activated(id, r)) {
		*val = on;
		return 1;
	}
	return 0;
}

/* Pasted text waiting for the focused field. One queue for the process: a
 * surface has at most one focused field, and the first ktui_field_key() after
 * the push takes the lot. */
static char paste_buf[4096];
static size_t paste_len;
static double paste_at;
#define PASTE_TTL 2.0		/* seconds an unclaimed paste waits */

void ktui_paste_push(const char *utf8, size_t len)
{
	/* A new paste SUPERSEDES an unclaimed one. The backend delivers a
	 * whole selection in one push, so appending never joins anything —
	 * it only accumulates pastes nobody took, until the queue is full and
	 * the one being made is the one lost. */
	paste_len = 0;
	paste_at = kb_now_s();

	/* Stripped at the door: control bytes never match a UTF-8 continuation
	 * byte, so this filter cannot split a sequence. */
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)utf8[i];
		if (c == '\n')
			c = ' ';
		else if (c < 0x20 || c == 0x7f)
			continue;
		if (paste_len + 1 >= sizeof(paste_buf))
			break;
		paste_buf[paste_len++] = (char)c;
	}
	/* A full queue may have cut a sequence in half; drop the fragment
	 * rather than hand the insert path a lead byte with no body. */
	size_t k = paste_len;
	while (k > 0 && ((unsigned char)paste_buf[k - 1] & 0xc0) == 0x80)
		k--;
	if (k > 0 && (unsigned char)paste_buf[k - 1] >= 0xc0) {
		unsigned char lead = (unsigned char)paste_buf[k - 1];
		size_t need = (lead & 0xe0) == 0xc0 ? 2
			      : (lead & 0xf0) == 0xe0 ? 3 : 4;
		if (paste_len - (k - 1) < need)
			paste_len = k - 1;
	}
	paste_buf[paste_len] = 0;
}

/*
 * Take the pending paste, for a consumer that is not a text field. A terminal
 * is the case: its "caret" is a child process on a pty, so it cannot go
 * through a KtuiField and has to be handed the bytes.
 *
 * The same TTL as the field path, and the same filter: newlines arrived as
 * spaces, so a paste cannot press Enter in a shell any more than it can in a
 * field. Returns the length and clears the queue — a paste is taken once.
 */
size_t ktui_paste_take(const char **out)
{
	if (paste_len && kb_now_s() - paste_at > PASTE_TTL)
		paste_len = 0;
	if (!paste_len)
		return 0;

	size_t n = paste_len;

	if (out)
		*out = paste_buf;
	paste_len = 0;
	return n;
}

/* The caret is a BYTE index into a UTF-8 buffer; it only ever rests on a
 * sequence boundary, and these two walk between boundaries. */
static int in_prev_bound(const char *buf, int at)
{
	if (at > 0)
		at--;
	while (at > 0 && ((unsigned char)buf[at] & 0xc0) == 0x80)
		at--;
	return at;
}

static int in_next_bound(const char *buf, int at)
{
	uint32_t cp;
	return (int)(ktui_utf8_next(buf + at, &cp) - buf);
}

/* Display column of byte offset `at` — a secret field shows one bullet per
 * CODEPOINT, so its column is the sequence count, not the glyph width. */
static int in_col_at(const char *buf, int at, int secret)
{
	const char *p = buf;
	uint32_t cp;
	int col = 0;
	while (p < buf + at && *p) {
		p = ktui_utf8_next(p, &cp);
		col += secret ? 1 : ktui_wcwidth(cp);
	}
	return col;
}

/* First byte offset at or past display column `col`. */
static int in_byte_at(const char *buf, int col, int secret)
{
	const char *p = buf;
	uint32_t cp;
	int c = 0;
	while (*p && c < col) {
		p = ktui_utf8_next(p, &cp);
		c += secret ? 1 : ktui_wcwidth(cp);
	}
	return (int)(p - buf);
}

static int in_insert(char *buf, size_t cap, int *cur, int len,
		     const char *src, size_t n)
{
	/* Whole sequences only: a paste cut mid-codepoint would leave a
	 * trailing byte every later edit trips over. */
	size_t room = cap - 1 - (size_t)len;
	size_t take = 0;
	while (take < n) {
		uint32_t cp;
		size_t sl = (size_t)(ktui_utf8_next(src + take, &cp) - (src + take));
		if (take + sl > n || take + sl > room)
			break;
		take += sl;
	}
	if (!take)
		return 0;
	memmove(buf + *cur + take, buf + *cur, (size_t)(len - *cur + 1));
	memcpy(buf + *cur, src, take);
	*cur += (int)take;
	return 1;
}

/*
 * THE CARET IS THE CALLER'S TO SET, so every entry point puts it back on the
 * text before using it. A caller that loads a new value and leaves the caret
 * where the old one ended, or one that says "the end" with any large number,
 * would otherwise hand memmove an offset past the terminator — and a caret
 * that lands inside a sequence would split it on the next insert.
 */
static int field_caret(const char *buf, int caret)
{
	int len = (int)strlen(buf);

	if (caret > len)
		caret = len;
	if (caret < 0)
		caret = 0;
	while (caret > 0 && ((unsigned char)buf[caret] & 0xc0) == 0x80)
		caret--;
	return caret;
}

/*
 * The first column shown. It follows the caret and nothing else, so the draw
 * and the press compute the same window from the same field and a click lands
 * on the character it was aimed at without either keeping a scroll position.
 * In display COLUMNS, converted to a byte offset for the draw — byte
 * arithmetic here is exactly the CJK-corrupts-the-row bug.
 */
static int field_scroll(KRect r, const char *buf, int caret, int secret,
			int *sbyte)
{
	int room = r.w - 2;
	int ccol = in_col_at(buf, caret, secret);
	int scol = ccol > room - 1 ? ccol - room + 1 : 0;
	int sb = in_byte_at(buf, scol, secret);

	if (sb)
		scol = in_col_at(buf, sb, secret);	/* wide-glyph straddle */
	*sbyte = sb;
	return scol;
}

int ktui_field_col(const KtuiField *f)
{
	return in_col_at(f->buf, field_caret(f->buf, f->caret), f->secret);
}

/* A word is a run of anything but spaces. A SECRET FIELD IS ONE WORD: where
 * its spaces are is part of the secret, and a Ctrl+Left that stopped at each
 * would read them back to anybody watching the caret. */
static int field_word_left(const KtuiField *f, int at)
{
	if (f->secret)
		return 0;
	while (at > 0 && f->buf[at - 1] == ' ')
		at--;
	while (at > 0 && f->buf[at - 1] != ' ')
		at = in_prev_bound(f->buf, at);
	return at;
}

static int field_word_right(const KtuiField *f, int at, int len)
{
	if (f->secret)
		return len;
	while (at < len && f->buf[at] == ' ')
		at++;
	while (at < len && f->buf[at] != ' ')
		at = in_next_bound(f->buf, at);
	return at;
}

static int field_cut(KtuiField *f, int from, int to, int len)
{
	if (from >= to)
		return 0;
	memmove(f->buf + from, f->buf + to, (size_t)(len - to + 1));
	f->caret = from;
	return KTUI_FIELD_CHANGED;
}

static int ctrl_letter(const KtuiEvent *ev, int c)
{
	return (ev->mods & KT_MOD_CTRL) && (ev->key == c || ev->key == c - 32);
}

int ktui_field_key(KtuiField *f, const KtuiEvent *ev)
{
	int rc = 0;
	int len = (int)strlen(f->buf);

	f->caret = field_caret(f->buf, f->caret);

	/*
	 * THE PASTE FIRST, AND WHATEVER THE EVENT IS. A paste reaches this
	 * process as a queue and not as a key, so the field that has the focus
	 * takes it on its next call — which is why a caller passes every
	 * event, and NULL on a wake that carried none.
	 *
	 * Expired rather than held: in a process with no focused field when
	 * the paste arrived — the panel, the desktop, a chooser whose list has
	 * the focus — the text would otherwise sit in the queue and be
	 * injected into whatever field is drawn next, seconds later and
	 * somewhere the user was not aiming.
	 */
	if (paste_len && kb_now_s() - paste_at > PASTE_TTL)
		paste_len = 0;
	if (paste_len) {
		if (in_insert(f->buf, f->cap, &f->caret, len, paste_buf,
			      paste_len))
			rc |= KTUI_FIELD_CHANGED;
		paste_len = 0;
		len = (int)strlen(f->buf);
	}

	/* Alt is never the field's, arrows included: Alt+Left and Alt+Right
	 * are page navigation on the surfaces that have pages, and a field
	 * that took them would trap the focus on its page. */
	if (!ev || ev->type != KT_EVT_KEY || (ev->mods & KT_MOD_ALT))
		return rc;

	int k = ev->key;
	int ctrl = (ev->mods & KT_MOD_CTRL) != 0;

	if (k == KT_K_LEFT) {
		f->caret = ctrl ? field_word_left(f, f->caret)
				: in_prev_bound(f->buf, f->caret);
	} else if (k == KT_K_RIGHT) {
		if (f->caret < len)
			f->caret = ctrl ? field_word_right(f, f->caret, len)
					: in_next_bound(f->buf, f->caret);
	} else if (k == KT_K_HOME) {
		f->caret = 0;
	} else if (k == KT_K_END) {
		f->caret = len;
	} else if (k == KT_K_BACKSPACE && !ctrl) {
		if (f->caret > 0)
			rc |= field_cut(f, in_prev_bound(f->buf, f->caret),
					f->caret, len);
	} else if (k == KT_K_DEL) {
		if (f->caret < len)
			rc |= field_cut(f, f->caret,
					in_next_bound(f->buf, f->caret), len);
	} else if (ctrl_letter(ev, 'u')) {
		rc |= field_cut(f, 0, len, len);
	} else if (ctrl_letter(ev, 'w') || (k == KT_K_BACKSPACE && ctrl)) {
		rc |= field_cut(f, field_word_left(f, f->caret), f->caret,
				len);
	} else if (k >= 0x20 && k != 0x7f && k < KT_K_SPECIAL && !ctrl) {
		/*
		 * A CHORD IS NEVER TEXT. Every backend delivers Ctrl+S as the
		 * letter with KT_MOD_CTRL, so a field that inserted every
		 * printable key would type an `s` for a chord the surface
		 * meant to act on. It is passed back instead.
		 */
		char enc[4];
		int el = ktui_utf8_encode((uint32_t)k, enc);

		if ((size_t)(len + el) < f->cap) {
			memmove(f->buf + f->caret + el, f->buf + f->caret,
				(size_t)(len - f->caret + 1));
			memcpy(f->buf + f->caret, enc, (size_t)el);
			f->caret += el;
			rc |= KTUI_FIELD_CHANGED;
		}
	} else {
		return rc;
	}
	return rc | KTUI_FIELD_USED;
}

void ktui_field_draw(KRect r, const KtuiField *f, int focus, int bg)
{
	const char *buf = f->buf;
	int len = (int)strlen(buf);
	int cur = field_caret(buf, f->caret);
	int secret = f->secret;
	/* On the selection fill KT_DIM is the fill itself, so the marker and
	 * the placeholder step up one slot rather than vanish into it. */
	int quiet = bg == KT_DIM ? KT_MID : KT_DIM;
	int fg = focus ? KT_TEXT : KT_MID;
	int room = r.w - 2;
	int sbyte;
	int scol;

	if (r.w < 1 || r.h < 1)
		return;
	r.h = 1;
	scol = field_scroll(r, buf, cur, secret, &sbyte);

	ktui_draw_fill(r, bg);
	ktui_draw_text(r.x, r.y, 1, focus ? ktui_glyph[KT_G_RIGHT] : " ",
		       focus ? KT_ACCENT : quiet, bg, 0);

	if (!len && f->placeholder && !focus) {
		ktui_draw_text(r.x + 2, r.y, room, f->placeholder, quiet, bg,
			       0);
	} else if (secret) {
		int glyphs = in_col_at(buf, len, 1);
		for (int i = 0; i < glyphs - scol && i < room; i++)
			ktui_draw_text(r.x + 2 + i, r.y, 1,
				       ktui_glyph[KT_G_BULLET], fg, bg, 0);
	} else {
		ktui_draw_text(r.x + 2, r.y, room, buf + sbyte, fg, bg, 0);
	}

	if (focus) {
		int cx = r.x + 2 + (in_col_at(buf, cur, secret) - scol);
		if (cx < r.x + r.w) {
			uint32_t under = ' ';
			if (cur < len) {
				if (secret) {
					under = 0x2022;
				} else {
					uint32_t cp;
					ktui_utf8_next(buf + cur, &cp);
					under = cp;
				}
			}
			ktui_draw_cell(cx, r.y, under, KT_BG, KT_ACCENT, 0);
			/* A wide glyph owns two cells; without the
			 * continuation the caret highlights its left half and
			 * the right half keeps the text's colours. */
			if (ktui_wcwidth(under) == 2 && cx + 1 < r.x + r.w)
				ktui_draw_cell(cx + 1, r.y, KTUI_WIDE_CONT,
					       KT_BG, KT_ACCENT, 0);
		}
	}
}

int ktui_field_hit(KRect r, KtuiField *f, int mx, int my)
{
	int sbyte, scol, col;

	if (r.w < 1 || r.h < 1 || !krect_hit(krect(r.x, r.y, r.w, 1), mx, my))
		return 0;
	/* The window the press was aimed at is the one the LAST draw showed,
	 * and that window is a function of the caret the press is about to
	 * move — so it is computed before the caret changes. */
	scol = field_scroll(r, f->buf, field_caret(f->buf, f->caret),
			    f->secret, &sbyte);
	col = mx - (r.x + 2) + scol;
	if (col < 0)
		col = 0;
	f->caret = in_byte_at(f->buf, col, f->secret);
	return 1;
}

/*
 * THE FRAME CONTROL IS THE TRIO WITH THE FOCUS RING AROUND IT. The caret is
 * kept in the per-id state store, because a frame caller holds only the
 * buffer; a surface with its own loop holds a KtuiField and keeps the caret
 * itself. A field whose id moves takes another field's caret, clamped onto
 * its own text — which is why a conditional group is drawn inside
 * ktui_id_push().
 */
int ktui_input(KRect r, char *buf, size_t cap, int secret, const char *placeholder)
{
	int id = ktui_id();
	int focus = ktui_focused(id);
	static int spare;	/* the caret when the store has no room */
	int *caret = ktui_state(id, sizeof(int));
	KtuiField f;
	int changed = 0;

	if (!caret)
		caret = &spare;
	f = (KtuiField){ buf, cap, *caret, secret, placeholder };

	if (focus) {
		int rc = ktui_field_key(&f, ui.consumed ? NULL : &ui.ev);

		if (rc & KTUI_FIELD_USED)
			ui.consumed = 1;
		changed = (rc & KTUI_FIELD_CHANGED) != 0;
	}

	ktui_field_draw(r, &f, focus, KT_SURFACE);
	ktui_hit(r, id);
	if (ui.clicked == id)
		ktui_field_hit(r, &f, ui.mx, ui.my);
	*caret = field_caret(buf, f.caret);

	/*
	 * A SECRET FIELD ANNOUNCES THAT IT IS ONE AND NEVER ITS CONTENTS. The
	 * whole point of the field is that what is typed into it is not on the
	 * screen; a reader that said it aloud would put it in the room.
	 */
	if (focus)
		ktui_announce(KT_A11Y_INPUT,
			      placeholder && *placeholder ? placeholder
							  : "text",
			      secret ? "hidden" : buf, 0, 0);
	return changed;
}

int ktui_bar_fill(int w, double frac, double *tip)
{
	if (tip)
		*tip = 0;
	if (w <= 0 || frac <= 0)
		return 0;
	if (frac >= 1)
		return w;
	double cells = frac * w;
	int solid = (int)cells;
	double rest = cells - solid;
	/* A boundary must not sprout a tip: 0.5 of 40 is exactly 20 cells, and
	 * a sliver there reads as 51%. The epsilon absorbs the binary
	 * representation of a value the caller computed as done/total. */
	if (rest < 1e-9)
		rest = 0;
	if (solid >= w) {
		solid = w;
		rest = 0;
	}
	if (tip)
		*tip = rest;
	return solid;
}

void ktui_progress_ex(KRect r, double frac, const char *label, int style,
		      int bg)
{
	int pulse = style & KT_BAR_PULSE;
	style &= KT_BAR_STYLE_MASK;

	if (r.w < 4)
		return;
	if (frac < 0) {
		/* Indeterminate: a scanning cell rather than a filled bar, so
		 * nobody reads a stalled percentage into it. */
		ktui_draw_hline(r.x, r.y, r.w, KT_G_SHADE, KT_DIM, bg);
		int p = (int)(kb_now_s() * 12) % (r.w * 2);
		if (p >= r.w)
			p = r.w * 2 - p - 1;
		int n = p + 3 > r.w ? r.w - p : 3;
		ktui_draw_hline(r.x + p, r.y, n, KT_G_FULL, KT_ACCENT, bg);
	} else if (style == KT_BAR_SEGMENTED) {
		if (frac > 1)
			frac = 1;
		int fill = (int)(frac * r.w + 0.5);
		ktui_draw_hline(r.x, r.y, fill, KT_G_FULL, KT_ACCENT, bg);
		ktui_draw_hline(r.x + fill, r.y, r.w - fill, KT_G_SHADE, KT_DIM,
				bg);
		for (int i = 1; i < r.w; i += 2)
			ktui_draw_cell(r.x + i, r.y, ' ',
				       i < fill ? KT_ACCENT : KT_DIM, bg, 0);
	} else {
		double tip = 0;
		int fill = style == KT_BAR_TIP
				   ? ktui_bar_fill(r.w, frac, &tip)
				   : (frac > 1 ? r.w : (int)(frac * r.w + 0.5));
		/* DRAWN AS RUNS, not cell by cell: ktui_draw_hline goes
		 * straight to the pre-decoded codepoint, while a per-cell
		 * ktui_draw_text re-decodes the same three bytes and re-walks
		 * the width tables for every column of every frame. */
		ktui_draw_hline(r.x, r.y, fill, KT_G_FULL, KT_ACCENT, bg);
		int rest = r.x + fill;
		if (tip > 0 && fill < r.w) {
			ktui_draw_text(rest, r.y, 1, ktui_ramp_h(tip),
				       KT_ACCENT, bg, 0);
			rest++;
		}
		ktui_draw_hline(rest, r.y, r.x + r.w - rest, KT_G_SHADE,
				KT_DIM, bg);

		/* One cell of the filled run is lit a shade brighter and walks
		 * left to right. It says "still moving" on a bar that has not
		 * advanced a whole cell in minutes — a zig sweep would read as
		 * progress going backwards, so it wraps instead. */
		if (pulse && fill > 0) {
			int p = (int)(kb_now_s() * 9) % (fill + 4);
			if (p < fill)
				ktui_draw_hline(r.x + p, r.y, 1, KT_G_FULL,
						KT_WARN, bg);
		}
	}

	if (label && *label) {
		int w = ktui_utf8_width(label);
		int x = r.x + (r.w - w) / 2;
		/* Reverse video: fg/bg swap at render time, so passing the
		 * panel's bg as fg here is what makes the chip's apparent
		 * background match the panel instead of a hardcoded slot. */
		ktui_draw_text(x, r.y, w, label, bg, KT_ACCENT, KT_A_REVERSE);
	}
}

void ktui_progress(KRect r, double frac, const char *label)
{
	/* kinstall (the disc's installer) links this wrapper and only this
	 * wrapper, never ktui_progress_ex directly — it must keep drawing
	 * exactly what it always has, so the style/bg stay pinned here. */
	ktui_progress_ex(r, frac, label, KT_BAR_SOLID, KT_BG);
}

void ktui_scrollbar(KRect r, int total, int shown, int off)
{
	if (r.h < 2)
		return;
	if (total <= shown) {
		ktui_draw_vline(r.x, r.y, r.h, KT_G_VL, KT_DIM, KT_BG);
		return;
	}
	int th = r.h * shown / total;
	if (th < 1)
		th = 1;
	/* The offset is clamped here and nowhere else: a caller is free to
	 * hand over one past its own end — a filter that shrinks the list
	 * under a scrolled view does exactly that — and the runs below are
	 * unclipped against the rect, so an unclamped thumb paints down over
	 * whatever sits beneath the panel. th < r.h on this branch, so
	 * r.h - th is always a valid upper bound and the track below the
	 * thumb never takes a negative length. */
	if (off < 0)
		off = 0;
	if (off > total - shown)
		off = total - shown;
	int ty = (r.h - th) * off / (total - shown);
	if (ty < 0)
		ty = 0;
	if (ty > r.h - th)
		ty = r.h - th;
	/* Three runs, not r.h separate strings: the track and the thumb are
	 * each one glyph repeated, and ktui_draw_vline writes the codepoint
	 * the tier already decoded instead of decoding it per cell. */
	ktui_draw_vline(r.x, r.y, ty, KT_G_VL, KT_DIM, KT_BG);
	ktui_draw_vline(r.x, r.y + ty, th, KT_G_FULL, KT_MID, KT_BG);
	ktui_draw_vline(r.x, r.y + ty + th, r.h - ty - th, KT_G_VL, KT_DIM,
			KT_BG);
}

int ktui_list(KRect r, KtuiList *st, int count, KtuiListRow row, void *user, int id)
{
	int focus = ktui_focused(id);
	int chosen = 0;
	int vis = r.h;

	if (st->sel >= count)
		st->sel = count - 1;
	if (st->sel < 0)
		st->sel = 0;

	if (focus && !ui.consumed && ui.ev.type == KT_EVT_KEY) {
		int k = ui.ev.key;
		if (k == KT_K_UP) {
			st->sel--;
			ui.consumed = 1;
		} else if (k == KT_K_DOWN) {
			st->sel++;
			ui.consumed = 1;
		} else if (k == KT_K_PGUP) {
			st->sel -= vis;
			ui.consumed = 1;
		} else if (k == KT_K_PGDN) {
			st->sel += vis;
			ui.consumed = 1;
		} else if (k == KT_K_HOME) {
			st->sel = 0;
			ui.consumed = 1;
		} else if (k == KT_K_END) {
			st->sel = count - 1;
			ui.consumed = 1;
		} else if (k == KT_K_ENTER || k == ' ') {
			chosen = 1;
			ui.consumed = 1;
		}
	}

	if (ui.wheel && ui.wheel_id == id) {
		st->off += ui.wheel * 3;
		ui.wheel = 0;
	}

	if (ui.clicked == id) {
		if (count > vis && ui.mx == r.x + r.w - 1) {
			/* The scrollbar column is paint, not rows — selecting
			 * the row hidden under it is the bug. A click above
			 * the thumb pages up, below it pages down, moving the
			 * selection the way PgUp/PgDn do so the off/sel clamps
			 * below cannot drag the view straight back. */
			int th = r.h * vis / count;
			if (th < 1)
				th = 1;
			int ty = (r.h - th) * st->off / (count - vis);
			int cy = ui.my - r.y;
			if (cy < ty)
				st->sel -= vis;
			else if (cy >= ty + th)
				st->sel += vis;
		} else {
			int idx = st->off + (ui.my - r.y);
			if (idx >= 0 && idx < count) {
				st->sel = idx;
				if (ui.dblclick)
					chosen = 1;
			}
		}
	}

	if (st->sel < 0)
		st->sel = 0;
	if (st->sel >= count)
		st->sel = count ? count - 1 : 0;

	int maxoff = count - vis;
	if (maxoff < 0)
		maxoff = 0;
	if (st->off > maxoff)
		st->off = maxoff;
	if (st->off < 0)
		st->off = 0;
	if (st->sel < st->off)
		st->off = st->sel;
	if (st->sel >= st->off + vis)
		st->off = st->sel - vis + 1;

	int listw = count > vis ? r.w - 1 : r.w;
	for (int i = 0; i < vis; i++) {
		int idx = st->off + i;
		int y = r.y + i;
		if (idx >= count) {
			ktui_draw_fill(krect(r.x, y, listw, 1), KT_BG);
			continue;
		}
		int sel = idx == st->sel;
		int rfg, rbg;

		/* The row painter is the caller's and is handed `sel` and
		 * `focus`; the FILL is this control's, so every ktui_list on
		 * the desktop selects the same way whatever its rows contain. */
		ktui_sel_slots(sel, focus, KT_BG, &rfg, &rbg);
		ktui_draw_fill(krect(r.x, y, listw, 1), rbg);
		row(idx, r.x, y, listw, sel, focus, user);
	}
	if (count > vis)
		ktui_scrollbar(krect(r.x + r.w - 1, r.y, 1, r.h), count, vis, st->off);

	/* THE POSITION, because the rows are the caller's callback and this
	 * knows which one is selected rather than what is in it. A surface
	 * that has the text may say the name itself. */
	if (focus && count > 0)
		ktui_announce(KT_A11Y_LIST, NULL, NULL, st->sel + 1, count);

	hit_wheel(r, id);
	return chosen;
}
