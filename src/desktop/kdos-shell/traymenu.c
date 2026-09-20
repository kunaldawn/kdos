/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-traymenu — a tray item's own menu, drawn as cells
 *
 *   ┌ Syncthing ───────────────────┐   ┌ Syncthing / Folders ──────────┐
 *   │ ▸ Open Web GUI               │   │ ▸ ✓ Documents                 │
 *   │   Folders                  ▶ │   │     Pictures                  │
 *   │   ───────────────────────────│   │     Music                     │
 *   │   Pause                      │   │                               │
 *   │   Quit                       │   │                               │
 *   └──────────────────────────────┘   └───────────────────────────────┘
 *
 * `com.canonical.dbusmenu` IS THE SECOND HALF OF A TRAY ICON. An item that
 * sets `ItemIsMenu` has no useful `Activate` — the spec's answer to a click on
 * one is "show the menu" — and the menu is not a window the application draws.
 * It is a LAYOUT TREE published on the bus, and the host is expected to render
 * it. Without this, such an item did nothing at all when clicked.
 *
 * ITS OWN PROCESS, like kdos-menu and for the same two reasons: the panel's
 * event loop owns one surface and one cell buffer, and an application that
 * takes four seconds to answer `GetLayout` must not take the panel with it.
 * The panel spawns this with the item's service name and its `Menu` object
 * path, which are the only two things needed to reach the tree.
 *
 * ONE COLUMN AT A TIME, which is kdos-menu's shape and is not a coincidence:
 * a cascade needs a surface per level and a pointer-tracking policy to decide
 * when a level goes away. Enter or Right descends into a submenu, Escape or
 * Left comes back, and the breadcrumb in the title says where you are.
 *
 * THE TREE IS READ ONCE AND NOT WATCHED. `LayoutUpdated` and `ItemsPropertiesUpdated`
 * exist and this listens to neither: the menu is open for as long as somebody
 * is looking at it, and a menu whose rows move under the hand is a menu that
 * activates the wrong row. `AboutToShow` is sent first, which is where an
 * application fills a submenu in, and its answer is the only update taken.
 *
 * AN ICON IS A SPRITE AND A SHORTCUT IS TEXT. `icon-name` goes through the
 * same theme lookup every other surface in this binary makes, `icon-data` is
 * a PNG decoded straight from the bus, and either one occupies two cells of a
 * column the level spends only if something in it resolved. `shortcut` is the
 * application's own chord and nothing here can press it — it is drawn because
 * a menu that names the key is how somebody stops opening the menu. A toggle
 * is drawn for the same kind of reason: a row that says "Pause" with no mark
 * is a row whose state is a guess.
 *
 * THE ICON COLUMN IS SPENT BY THE LEVEL, NOT BY THE ROW. Labels that start in
 * different columns depending on whether the row above resolved a picture
 * read as ragged, which is the rule the toggle mark already keeps.
 * ---------------------------------
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection tray.c and notifyd.c make. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kchrome.h"
#include "kicon.h"
#include "kwl.h"
#include "shell.h"

#define TM_IFACE "com.canonical.dbusmenu"

/* A call that outlives this is a wedged application, not a slow bus. The
 * ceiling is tray.c's, for its reason: nothing here may hang on somebody
 * else's process. */
#define TM_TIMEOUT_US 300000

/* How many rows one menu may carry, at every level together. A tray menu with
 * more than this is not a menu anybody reads, and the cap is what makes the
 * recursion below bounded without a depth counter of its own. */
#define TM_MAX 256
/* And how deep it may nest. A malicious or merely broken tree is the only way
 * to reach this; an application's real menu is two levels. */
#define TM_DEPTH 8

/* A chord as it is drawn. `Ctrl+Shift+Alt+Super+Backspace` is 28 characters
 * and nothing real is longer; a chord that overflows is cut rather than
 * allowed to push the label out of its column. */
#define TM_CHORD 32

/* THE ICON IS TWO CELLS WIDE AND ONE TALL, which is what "one icon" means
 * everywhere in this binary — a cell is taller than it is wide, so a square
 * picture needs two of them. */
#define TM_ICON_W 2

enum { TM_TOGGLE_NONE = 0, TM_TOGGLE_CHECK, TM_TOGGLE_RADIO };

struct tmitem {
	int id;			/* the dbusmenu id, which Event names     */
	int parent;		/* index into tm[], -1 at the root        */
	char label[96];
	char icon[96];		/* `icon-name`, a theme name              */
	char chord[TM_CHORD];	/* `shortcut`, already in this desk's form */
	unsigned char *png;	/* `icon-data`, owned; NULL for most rows */
	size_t png_len;
	int enabled;
	int separator;
	int submenu;		/* `children-display` said `submenu`      */
	int toggle;		/* TM_TOGGLE_*                            */
	int state;		/* -1 the item did not say, else 0 or 1   */
};

static struct tmitem tm[TM_MAX];
static int ntm;
/* The row the surface is showing the children of: an index into tm[], or -1
 * for the root. */
static int open_at = -1;
static int sel, top;
static char title[96];
static char root_title[64];

/* Where a failure is put. A menu that cannot be read draws the reason rather
 * than an empty box: "nothing happened" is the state this whole file exists to
 * get rid of. */
static char why[160];

/* ── reading the tree ──────────────────────────────────────────────────── */

/*
 * `label` CARRIES MNEMONIC UNDERSCORES and this strips them. The convention is
 * GTK's — `_Quit` means Q is the accelerator — and a row drawn with the
 * underscore in it reads as a typo. A doubled `__` is a literal one.
 */
static void take_label(char *out, size_t n, const char *in)
{
	size_t k = 0;

	if (!n)
		return;
	for (const char *p = in ? in : ""; *p && k + 1 < n; p++) {
		if (*p == '_') {
			if (p[1] == '_')
				out[k++] = *++p;
			continue;
		}
		/* A newline or a tab in a label would break the one-row-per-
		 * item shape this draws in. */
		out[k++] = (unsigned char)*p < 32 ? ' ' : *p;
	}
	out[k] = '\0';
}

/*
 * `shortcut` IS aas AND ONLY THE FIRST SEQUENCE IS DRAWN. The property is a
 * LIST of chords — an application may offer two ways to reach one row — and a
 * menu column wide enough for both is a column that has taken the label's
 * space to say the same thing twice.
 *
 * THE MODIFIER NAMES ARE THE SPEC'S AND THE DRAWN ONES ARE THIS DESKTOP'S.
 * dbusmenu spells them `Control`, `Alt`, `Shift` and `Super`; every chord
 * KDOS prints says `Ctrl`, and a menu that said `Control` beside a keyboard
 * card that says `Ctrl` reads as two different keys.
 *
 * A token this does not know is copied through. The last token of a sequence
 * is the key itself and the spec puts no vocabulary on it at all.
 */
static void take_chord(char *out, size_t n, sd_bus_message *m)
{
	size_t k = 0;
	int first = 1;

	if (!n)
		return;
	out[0] = '\0';
	if (sd_bus_message_enter_container(m, 'a', "as") <= 0)
		return;
	/* The first sequence fills the field; the rest are entered and left
	 * so the message stays in step for the property after this one. */
	while (sd_bus_message_enter_container(m, 'a', "s") > 0) {
		const char *tok = NULL;

		while (sd_bus_message_read_basic(m, 's', &tok) > 0) {
			if (!first || !tok)
				continue;
			if (!strcmp(tok, "Control"))
				tok = "Ctrl";
			if (k && k + 1 < n)
				out[k++] = '+';
			while (*tok && k + 1 < n)
				out[k++] = *tok++;
		}
		sd_bus_message_exit_container(m);
		first = 0;
	}
	sd_bus_message_exit_container(m);
	out[k] = '\0';
}

/* Forward: the tree is read recursively and each level reads the next. */
static int read_item(sd_bus_message *m, int parent, int depth);

/*
 * The `av` of children, each variant holding one `(ia{sv}av)`. A child the
 * reader cannot make sense of ends the level rather than being skipped: a
 * partially-read container leaves sd-bus's cursor somewhere this code cannot
 * name, and every row after it would be nonsense.
 */
static int read_children(sd_bus_message *m, int parent, int depth)
{
	int r;

	r = sd_bus_message_enter_container(m, 'a', "v");
	if (r < 0)
		return r;
	while (sd_bus_message_enter_container(m, 'v', "(ia{sv}av)") > 0) {
		r = read_item(m, parent, depth + 1);
		sd_bus_message_exit_container(m);
		if (r < 0)
			break;
	}
	sd_bus_message_exit_container(m);
	return 0;
}

static int read_item(sd_bus_message *m, int parent, int depth)
{
	struct tmitem *it;
	int id = 0, slot, r;

	r = sd_bus_message_enter_container(m, 'r', "ia{sv}av");
	if (r < 0)
		return r;
	if (sd_bus_message_read_basic(m, 'i', &id) < 0) {
		sd_bus_message_exit_container(m);
		return -1;
	}

	/*
	 * THE ROOT IS NOT A ROW. dbusmenu's tree has a single item 0 at the
	 * top whose only job is to hold the real ones, and drawing it would
	 * put a blank line above every menu.
	 */
	slot = id == 0 && parent < 0 ? -1 : (ntm < TM_MAX ? ntm++ : -1);
	it = slot >= 0 ? &tm[slot] : NULL;
	if (it) {
		memset(it, 0, sizeof(*it));
		it->id = id;
		it->parent = parent;
		it->enabled = 1;
		it->state = -1;
	}

	if (sd_bus_message_enter_container(m, 'a', "{sv}") >= 0) {
		while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
			const char *key = NULL, *contents = NULL;
			char type = 0;

			if (sd_bus_message_read_basic(m, 's', &key) < 0)
				break;
			if (sd_bus_message_peek_type(m, &type, &contents) < 0)
				break;
			if (!it || !contents) {
				sd_bus_message_skip(m, "v");
				sd_bus_message_exit_container(m);
				continue;
			}
			if (!strcmp(contents, "s") &&
			    sd_bus_message_enter_container(m, 'v', "s") > 0) {
				const char *v = NULL;

				sd_bus_message_read_basic(m, 's', &v);
				if (!strcmp(key, "label"))
					take_label(it->label,
						   sizeof(it->label), v);
				else if (!strcmp(key, "icon-name") && v)
					snprintf(it->icon, sizeof(it->icon),
						 "%s", v);
				else if (!strcmp(key, "type") && v)
					it->separator =
						!strcmp(v, "separator");
				else if (!strcmp(key, "children-display") && v)
					it->submenu = !strcmp(v, "submenu");
				else if (!strcmp(key, "toggle-type") && v)
					it->toggle =
						!strcmp(v, "checkmark")
							? TM_TOGGLE_CHECK
						: !strcmp(v, "radio")
							? TM_TOGGLE_RADIO
							: TM_TOGGLE_NONE;
				sd_bus_message_exit_container(m);
			} else if (!strcmp(contents, "b") &&
				   sd_bus_message_enter_container(m, 'v', "b") >
					   0) {
				int b = 0;

				sd_bus_message_read_basic(m, 'b', &b);
				/* `visible` false is not a row that is drawn
				 * greyed — it is a row that is not there. The
				 * label is cleared and the skip below drops
				 * it. */
				if (!strcmp(key, "enabled"))
					it->enabled = b;
				else if (!strcmp(key, "visible") && !b)
					it->id = -1;
				sd_bus_message_exit_container(m);
			} else if (!strcmp(contents, "i") &&
				   sd_bus_message_enter_container(m, 'v', "i") >
					   0) {
				int v = 0;

				sd_bus_message_read_basic(m, 'i', &v);
				if (!strcmp(key, "toggle-state"))
					it->state = v;
				sd_bus_message_exit_container(m);
			} else if (!strcmp(contents, "ay") &&
				   !strcmp(key, "icon-data") &&
				   sd_bus_message_enter_container(m, 'v',
								  "ay") > 0) {
				const void *d = NULL;
				size_t dn = 0;

				/*
				 * COPIED, BECAUSE THE MESSAGE IS FREED FIRST.
				 * sd_bus hands out a pointer into the reply
				 * and the reply is unref'd the moment the
				 * tree is read; the decode happens later,
				 * once there is a display to size a sprite
				 * against.
				 *
				 * A blob past the cap is DROPPED rather than
				 * truncated: half a PNG decodes to nothing
				 * and would be retried every frame.
				 */
				if (sd_bus_message_read_array(m, 'y', &d,
							      &dn) > 0 &&
				    d && dn && dn <= KICON_PNG_MAX) {
					it->png = malloc(dn);
					if (it->png) {
						memcpy(it->png, d, dn);
						it->png_len = dn;
					}
				}
				sd_bus_message_exit_container(m);
			} else if (!strcmp(contents, "aas") &&
				   !strcmp(key, "shortcut") &&
				   sd_bus_message_enter_container(m, 'v',
								  "aas") > 0) {
				take_chord(it->chord, sizeof(it->chord), m);
				sd_bus_message_exit_container(m);
			} else {
				sd_bus_message_skip(m, "v");
			}
			sd_bus_message_exit_container(m);
		}
		sd_bus_message_exit_container(m);
	}

	/*
	 * A ROW WITH NO LABEL AND NO SEPARATOR MARK IS STILL A ROW, so it gets
	 * one that says what it is. An application that publishes a menu item
	 * with no label has published something a person has to be able to
	 * see in order to report.
	 */
	if (it && !it->label[0] && !it->separator)
		snprintf(it->label, sizeof(it->label), "(item %d)", it->id);

	/* THE DEPTH CAP IS ON THE DESCENT AND NOT ON THE ROW. A tree deeper
	 * than this keeps the rows it has already read; only the levels below
	 * the cap are dropped, which is what makes a broken tree a short menu
	 * rather than no menu. */
	if (depth < TM_DEPTH)
		read_children(m, slot, depth);
	else
		sd_bus_message_skip(m, "av");

	sd_bus_message_exit_container(m);
	return 0;
}

/*
 * `AboutToShow` FIRST, AND ITS ANSWER IS DISCARDED. The spec says a menu may
 * fill itself in when it is told it is about to be shown, and several
 * applications build their submenus there; the boolean it returns says whether
 * the layout changed, which does not matter here because the layout has not
 * been read yet. Fire and forget would be wrong — the reply is what says the
 * application has finished filling it in.
 */
static void about_to_show(sd_bus *bus, const char *service, const char *path)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;

	sd_bus_call_method(bus, service, path, TM_IFACE, "AboutToShow", &err,
			   &reply, "i", 0);
	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
}

/*
 * THE PROPERTY LIST IS NAMED RATHER THAN LEFT EMPTY. An empty list means "give
 * me everything", and everything is whatever an application decided to hang
 * off a row — naming the ten this file reads is what bounds the message.
 *
 * `icon-data` IS ASKED FOR AND IT IS THE EXPENSIVE ONE: a PNG per row, where
 * every other property is a word. It is asked for because a tray item that
 * publishes one usually publishes no `icon-name` at all, so declining it is
 * declining the icon. The per-row cap is KICON_PNG_MAX and a blob past it is
 * dropped on the way in.
 */
static int load(sd_bus *bus, const char *service, const char *path)
{
	static const char *const props[] = {
		"label", "enabled", "visible", "type", "children-display",
		"toggle-type", "toggle-state", "icon-name", "icon-data",
		"shortcut", NULL
	};
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL, *reply = NULL;
	uint32_t rev = 0;
	int r;

	r = sd_bus_message_new_method_call(bus, &m, service, path, TM_IFACE,
					   "GetLayout");
	if (r < 0)
		goto out;
	/* -1 is "every level": one call rather than one per submenu, because
	 * a submenu opened by a second round trip is a submenu that opens
	 * blank on a slow application. */
	r = sd_bus_message_append(m, "ii", 0, -1);
	if (r < 0)
		goto out;
	r = sd_bus_message_append_strv(m, (char **)(uintptr_t)props);
	if (r < 0)
		goto out;
	r = sd_bus_call(bus, m, TM_TIMEOUT_US, &err, &reply);
	if (r < 0) {
		snprintf(why, sizeof(why), "%s",
			 err.message ? err.message : "the menu did not answer");
		goto out;
	}
	if (sd_bus_message_read_basic(reply, 'u', &rev) < 0) {
		r = -1;
		goto out;
	}
	r = read_item(reply, -1, 0);
	if (r < 0)
		snprintf(why, sizeof(why), "this menu is not one this desktop "
					   "can read");
out:
	if (r < 0 && !why[0])
		snprintf(why, sizeof(why), "the menu did not answer");
	sd_bus_error_free(&err);
	sd_bus_message_unref(m);
	sd_bus_message_unref(reply);
	return r;
}

/*
 * A CLICK IS FIRE AND FORGET, which is tray.c's rule and is this file's for
 * the same reason: the reply carries nothing, and an application that takes
 * four seconds to act must not hold a menu open while it does.
 *
 * `data` IS AN EMPTY STRING AND NOT AN ABSENT ARGUMENT. The signature is
 * `isvu` and the variant is required; the spec says its contents are ignored
 * for `clicked`, and every host sends something inert.
 */
static void send_clicked(sd_bus *bus, const char *service, const char *path,
			 int id)
{
	sd_bus_message *m = NULL;

	if (sd_bus_message_new_method_call(bus, &m, service, path, TM_IFACE,
					   "Event") < 0)
		return;
	if (sd_bus_message_append(m, "is", id, "clicked") >= 0 &&
	    sd_bus_message_append(m, "v", "s", "") >= 0 &&
	    sd_bus_message_append(m, "u", (uint32_t)time(NULL)) >= 0)
		sd_bus_send(bus, m, NULL);
	sd_bus_message_unref(m);
	sd_bus_flush(bus);
}

/* ── the view ──────────────────────────────────────────────────────────── */

/* The rows of the level `open_at` holds, as indices into tm[]. Rebuilt rather
 * than cached: it is a walk of at most TM_MAX entries and it has to be right
 * after a submenu is opened. */
static int rows[TM_MAX];
static int nrows;

/* Forward: build() puts the cursor on a row Enter means something on, and both
 * of those live past it. */
static bool pickable(int r);
static void step(int by);

static void build(void)
{
	nrows = 0;
	for (int i = 0; i < ntm; i++) {
		/* `visible: false` marked the row with -1 rather than
		 * compacting the array, because a child's `parent` is an
		 * index into it. */
		if (tm[i].id < 0 || tm[i].parent != open_at)
			continue;
		rows[nrows++] = i;
	}
	sel = 0;
	top = 0;
	if (open_at < 0)
		snprintf(title, sizeof(title), "%s", root_title);
	else
		snprintf(title, sizeof(title), "%s / %s", root_title,
			 tm[open_at].label);
	/* THE CURSOR LANDS SOMEWHERE ENTER MEANS SOMETHING. A level whose
	 * first row is a separator or a disabled item would open with the
	 * highlight on a row that answers nothing, and this runs on every
	 * level rather than only on the first — a submenu is as likely to
	 * begin with a heading as the root is. */
	if (nrows && !pickable(sel))
		step(1);
}

/* A row somebody can land on. A separator and a disabled row are both drawn
 * and neither is a destination. */
static bool pickable(int r)
{
	const struct tmitem *it = &tm[rows[r]];

	return !it->separator && it->enabled;
}

static void step(int by)
{
	int n = nrows;

	for (int k = 0; k < n; k++) {
		sel = (sel + by + n) % n;
		if (pickable(sel))
			return;
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/* Off for a dump and for `--no-icons`, which is bt.c's flag and its reason: a
 * golden frame is the character grid, and a picture in one is a diff against
 * whatever theme the machine that regenerated it happened to carry. */
static int icons_on = 1;

/*
 * THE SPRITE FOR ONE ROW, or -1. `icon-data` wins over `icon-name`: a row that
 * carries its own picture carries it because the theme has nothing for it.
 *
 * Both lookups are memoised inside libkicon, so calling this once per visible
 * row per frame is a hash after the first frame.
 */
static int row_icon(const struct tmitem *it)
{
	if (!icons_on)
		return -1;
	if (it->png)
		return kicon_slot_png(it->png, it->png_len, TM_ICON_W, 1);
	if (it->icon[0])
		return kicon_slot(it->icon, TM_ICON_W, 1);
	return -1;
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int body = h - 2;

	kch_px_reset();
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 0);

	if (why[0]) {
		ktui_draw_text(2, 1, w - 4, why, KT_ERR, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_flush();
		return;
	}
	if (!nrows) {
		ktui_draw_text(2, 1, w - 4, "this menu is empty", KT_MID,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_flush();
		return;
	}

	/*
	 * WHAT THIS LEVEL SPENDS ON PICTURES AND ON CHORDS, decided before the
	 * first row is drawn and the same for all of them.
	 *
	 * A column resolved per row would put the labels of a menu in two
	 * different places depending on which rows the theme happened to have
	 * a picture for, and a chord field as wide as the widest chord ON
	 * SCREEN keeps the whole of every label a menu can afford to show.
	 * Both are measured over the VISIBLE rows, so scrolling a long menu
	 * can move them — which is right: the alternative is every level
	 * paying for the one row with a four-modifier chord in it.
	 */
	int icol = 0, chordw = 0;

	for (int r = 0; r < body; r++) {
		int idx = top + r;

		if (idx >= nrows)
			break;

		const struct tmitem *it = &tm[rows[idx]];
		int cl = (int)strlen(it->chord);

		if (row_icon(it) >= 0)
			icol = TM_ICON_W + 1;
		if (cl > chordw)
			chordw = cl;
	}
	/* The gap before the chord is part of its field: a chord butting up
	 * against a label reads as one word. */
	if (chordw)
		chordw += 2;

	for (int r = 0; r < body; r++) {
		int idx = top + r;

		if (idx >= nrows)
			break;

		const struct tmitem *it = &tm[rows[idx]];
		bool on = idx == sel;
		uint8_t fg = it->enabled ? KT_TEXT : KT_DIM;
		uint8_t bg = KT_SURFACE;

		if (it->separator) {
			for (int x = 1; x < w - 1; x++)
				ktui_draw_text(x, 1 + r, 1,
					       ktui_glyph[KT_G_HL], KT_MID,
					       KT_SURFACE, KT_A_NONE);
			continue;
		}
		/* THE PLATE AND THE CELL FORM OF IT, which is kdos-menu's
		 * rule: the pixel plate is the whole highlight where there is
		 * a pixel layer, and on a character grid — the console, and
		 * every dump — an accent fill with the slots swapped is what
		 * says which row Enter will take. */
		if (on) {
			kch_px_row(1, 1 + r, w - 2, KCH_T_ACTIVE);
			if (!kch_px_live()) {
				int sfg, sbg;

				ktui_sel_slots(1, 1, KT_SURFACE, &sfg, &sbg);
				fg = (uint8_t)sfg;
				bg = (uint8_t)sbg;
				ktui_draw_fill(krect(1, 1 + r, w - 2, 1), bg);
			}
		}

		/*
		 * THE MARK IS A COLUMN OF ITS OWN and it is spent whether or
		 * not this row has one: a menu whose labels start in different
		 * columns depending on whether the row above is a checkbox is
		 * a menu that reads as ragged.
		 *
		 * A FILLED GLYPH FOR ON AND AN EMPTY COLUMN FOR OFF, from the
		 * table rather than as a literal: the console font is 512
		 * glyphs and every mark this desktop draws is one that table
		 * has a fallback for. `toggle-state` may also be -1, which the
		 * spec spells "indeterminate" and which no shipped application
		 * has been seen to send — it draws as the state it is, unknown.
		 */
		const char *mark = " ";

		if (it->toggle != TM_TOGGLE_NONE)
			mark = it->state > 0
				       ? ktui_glyph[it->toggle == TM_TOGGLE_RADIO
							   ? KT_G_BULLET
							   : KT_G_SQUARE]
				       : it->state == 0 ? " "
						        : ktui_glyph[KT_G_DOT];
		ktui_draw_text(2, 1 + r, 1, mark, fg, bg, KT_A_NONE);

		int icon = icol ? row_icon(it) : -1;

		/* THE PICTURE TAKES THE SELECTED ROW'S BACKGROUND, so a row
		 * under the accent plate does not carry a rectangle of the
		 * surface colour with an icon in it. */
		if (icon >= 0)
			ktui_draw_sprite(krect(4, 1 + r, TM_ICON_W, 1), icon,
					 fg, bg);

		/* The label ends where the chord's field begins, and the
		 * chord's field ends one column short of the submenu arm —
		 * which is spent whether or not this row has an arrow, for
		 * the reason the mark column is. */
		int lx = 4 + icol;
		int lw = w - 4 - lx - chordw;

		ktui_draw_text(lx, 1 + r, lw, it->label, fg, bg, KT_A_NONE);
		/*
		 * THE CHORD IS DIMMED EVEN ON THE SELECTED ROW. It is not a
		 * thing to aim at — this desktop cannot press it — and at the
		 * label's own weight it reads as a second label.
		 */
		if (it->chord[0] && chordw)
			ktui_draw_text_right(w - 4 - chordw + 1, 1 + r,
					     chordw - 1, it->chord,
					     it->enabled ? KT_MID : KT_DIM, bg,
					     KT_A_NONE);
		if (it->submenu)
			ktui_draw_text(w - 3, 1 + r, 1, ktui_glyph[KT_G_RIGHT],
				       fg, bg, KT_A_NONE);
	}

	kch_scrollbar(0, w - 1, 1, body, nrows, top, KT_SURFACE);
	ktui_draw_flush();
}

/*
 * THE BLOBS, HANDED BACK. Every other field of tm[] is inline and dies with
 * the process; `icon-data` is the one that is not, and a menu that exits
 * through any of the several paths below must not depend on which one it
 * took. Idempotent, so a path that frees twice is a path that works.
 */
static void drop_icons(void)
{
	for (int i = 0; i < ntm; i++) {
		free(tm[i].png);
		tm[i].png = NULL;
		tm[i].png_len = 0;
	}
}

/* ── the surface ───────────────────────────────────────────────────────── */

int traymenu_main(int argc, char **argv)
{
	const char *font = NULL, *service = NULL, *path = NULL;
	int at_x = -1, at_y = 0, at_bottom = 0, dump = 0;
	/* A dump takes no keys, so the two things a person does with this
	 * surface have to be sayable on a command line for a golden to show
	 * them: `--open` descends into a submenu by its dbusmenu id, and
	 * `--pick` sends one row's Event and exits without drawing. */
	int open_id = 0, pick_id = 0;
	sd_bus *bus = NULL;

	snprintf(root_title, sizeof(root_title), "Menu");

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		/* The item's own name, for the title. The panel has it and
		 * this does not: an `Id` is read off the StatusNotifierItem
		 * interface and the menu object does not publish one. */
		else if (!strcmp(argv[i], "--name") && i + 1 < argc)
			snprintf(root_title, sizeof(root_title), "%s",
				 argv[++i]);
		/* Where the cell that opened it is, in pixels — kdos-menu's
		 * two flags and for its reason: the panel knows and this does
		 * not, and a menu that appeared in the middle of the screen
		 * would not read as belonging to anything. */
		else if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--open") && i + 1 < argc) {
			open_id = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--pick") && i + 1 < argc) {
			pick_id = atoi(argv[++i]);
		} else if (!service) {
			service = argv[i];
		} else if (!path) {
			path = argv[i];
		} else {
			fprintf(stderr, "usage: kdos-traymenu SERVICE PATH "
					"[--name NAME] [--at X Y]\n"
					"                       "
					"[--at-bottom X Y] [--open ID] "
					"[--pick ID]\n"
					"                       "
					"[--dump] [--no-icons] "
					"[--font NAME]\n");
			return 2;
		}
	}

	if (!dump && (!service || !path)) {
		fprintf(stderr, "kdos-traymenu: a bus name and an object "
				"path are both needed\n");
		return 2;
	}

	/*
	 * THE BUS IS OPENED BEFORE THE SURFACE. A menu whose tree cannot be
	 * read draws the reason, and the size of the window is the number of
	 * rows — which is not known until the tree is in.
	 */
	if (service && path) {
		if (sd_bus_open_user(&bus) < 0) {
			snprintf(why, sizeof(why), "there is no session bus");
		} else {
			about_to_show(bus, service, path);
			load(bus, service, path);
		}
	} else {
		snprintf(why, sizeof(why), "no menu was named");
	}
	/*
	 * `--pick` NEVER DRAWS. It is the one outward effect this surface has
	 * and the one thing a frame cannot show, so it is a path of its own
	 * that sends the Event and leaves — not a flag that also opens a
	 * window somebody would then have to close.
	 */
	if (pick_id) {
		drop_icons();
		if (!bus)
			return 1;
		send_clicked(bus, service, path, pick_id);
		sd_bus_unref(bus);
		return 0;
	}

	/* `--open` NAMES AN id AND NOT A ROW, because a row number is only true
	 * of the level it came from and the ids are what the tree published. */
	if (open_id)
		for (int i = 0; i < ntm; i++)
			if (tm[i].id == open_id && tm[i].submenu)
				open_at = i;
	build();

	int wide = 42, high = (why[0] ? 1 : nrows) + 2;

	if (high > 24)
		high = 24;
	if (high < 4)
		high = 4;

	sh_theme_from_cache();
	if (dump) {
		/* A golden frame is the character grid — bt.c's rule. */
		icons_on = 0;
		ktui_offscreen_init(wide, high);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		drop_icons();
		sd_bus_unref(bus);
		return 0;
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = wide,
		.rows = high,
		.corner = at_x < 0	? KDISP_CORNER_CENTER
			  : at_bottom	? KDISP_CORNER_BOTTOM_LEFT
					: KDISP_CORNER_TOP_LEFT,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-traymenu",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-traymenu: no display server\n");
		drop_icons();
		sd_bus_unref(bus);
		return 1;
	}
	/*
	 * THE NOMINAL CELL WHERE THERE IS NO REAL ONE, and the sprite backend
	 * before it — osd.c's pair, for its reasons: a console surface has no
	 * pixel size of its own, and the console backend clears its client
	 * state when it connects, so a callback registered before kdisp_init
	 * is erased.
	 */
	sh_pic_backend();
	if (icons_on)
		kicon_init(sh_pic_cell_w(), sh_pic_cell_h(), kdisp_scale());
	ktui_draw_init();
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		sh_theme_poll();

		int body = ktui_h - 2;

		kch_list_clamp(&top, sel, nrows, body, 1);
		draw();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + top;
			bool on_row = ev.my >= 1 && ev.my < ktui_h - 1 &&
				      row >= 0 && row < nrows;

			if (ev.press == KT_MP_DRAG) {
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0)
					top = bt;
				else if (on_row && pickable(row))
					sel = row;
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt = kch_scrollbar_press(0, ev.mx, ev.my);

				if (bt >= 0) {
					top = bt;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;

				if (!kch_list_wheel(up, &top, nrows, body))
					step(up ? -1 : 1);
				continue;
			}
			/* A RIGHT PRESS BACKS OUT, which is kdos-menu's rule:
			 * the ladder is the same ladder and the way back has
			 * to be the same gesture. */
			if (ev.btn == KT_MB_RIGHT) {
				if (open_at < 0)
					break;
				open_at = tm[open_at].parent;
				build();
				continue;
			}
			if (ev.btn != KT_MB_LEFT || !on_row || !pickable(row))
				continue;
			sel = row;
			if (tm[rows[sel]].submenu) {
				open_at = rows[sel];
				build();
				continue;
			}
			send_clicked(bus, service, path, tm[rows[sel]].id);
			break;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (ev.key == KT_K_ESC) {
			/* THE ESCAPE LADDER: out of an open submenu first, and
			 * only then out of the menu. */
			if (open_at < 0)
				break;
			open_at = tm[open_at].parent;
			build();
			continue;
		}
		switch (ev.key) {
		case KT_K_UP:
			step(-1);
			break;
		case KT_K_DOWN:
			step(1);
			break;
		case KT_K_HOME:
			sel = 0;
			if (nrows && !pickable(sel))
				step(1);
			break;
		case KT_K_END:
			sel = nrows ? nrows - 1 : 0;
			if (nrows && !pickable(sel))
				step(-1);
			break;
		case KT_K_LEFT:
			if (open_at >= 0) {
				open_at = tm[open_at].parent;
				build();
			}
			break;
		case KT_K_RIGHT:
		case KT_K_ENTER:
			if (!nrows || !pickable(sel))
				break;
			if (tm[rows[sel]].submenu) {
				open_at = rows[sel];
				build();
				break;
			}
			if (ev.key == KT_K_RIGHT)
				break;
			send_clicked(bus, service, path, tm[rows[sel]].id);
			goto done;
		default:
			break;
		}
	}
done:
	if (icons_on)
		kicon_finish();
	drop_icons();
	kdisp_shutdown();
	sd_bus_unref(bus);
	return 0;
}
