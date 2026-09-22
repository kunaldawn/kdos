/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kvt_grid — a terminal screen becomes KtuiCells
 *
 * The render boundary, and the only file in this library that knows KDOS
 * exists. Everything below it is upstream's state machine with 24-bit colour
 * and a per-cell age; everything above it is the eight-slot grid the rest of
 * the desktop draws on.
 *
 * THE CONVERSION HAPPENS HERE AND ONLY HERE, once per frame, over the runs the
 * screen says changed. That is the trade for keeping upstream's cell: the
 * terminal keeps colours a KtuiCell cannot hold, and one function pays for it.
 * ---------------------------------
 */

#include <string.h>

#include "kvt.h"
#include "ktui.h"

/*
 * ONE RULE FOR EVERY COLOUR: the nearest of the theme's eight slots. The rule
 * itself is libktui's, so a terminal's SGR and a picture's average tint reduce
 * to a slot the same way — two implementations would drift and `kdos theme
 * amber` would move one of them.
 *
 * AND IT IS ANSWERED ONCE PER COLOUR, not once per cell. The rule is an
 * eight-way squared-distance search and it is otherwise run twice for every
 * cell of every frame; a full-screen animation asks it tens of thousands of
 * times a second for a few dozen distinct colours. They go in a direct-mapped
 * cache, thrown away when the palette in force moves — which is what `kdos
 * theme` and the night light do, and the only thing that can change an answer.
 *
 * THE PALETTE IS COMPARED BY VALUE, NOT BY ADDRESS. While the night light is
 * on `ktui_theme` points at one reused struct that libktui refills in place,
 * so the pointer does not move across a theme change and every cached answer
 * would keep the previous scheme's slot. The comparison is eight RGB triples
 * once a frame, against eight squared-distance searches per cell.
 *
 * EVERY COLOUR ARRIVES AS RGB. The vte resolves a palette index through the
 * palette in force and a 24-bit request needs no resolving, so there is one
 * kind of input here and one cache for it.
 *
 * THE BUCKET COMES OFF THE HIGH BITS of the multiplicative hash. The low bits
 * of a product carry only the low bits of its inputs, so an index taken from
 * the low end depends on nothing but the low bits of blue and drops the whole
 * 216-colour xterm cube into six of the buckets — a miss on almost every
 * lookup for exactly the content the cache exists for. LIT_BITS and LIT_CACHE
 * are spelled as one pair so the shift cannot drift from the size.
 */
#define LIT_BITS 6
#define LIT_CACHE (1u << LIT_BITS)

static KRgb slot_pal[KT_NCOLOR];
static unsigned char slot_have;
static uint32_t lit_key[LIT_CACHE];
static uint8_t lit_slot[LIT_CACHE];
static unsigned char lit_set[LIT_CACHE];

static void slot_sync(void)
{
	if (slot_have && !memcmp(slot_pal, ktui_theme->slot, sizeof(slot_pal)))
		return;
	memcpy(slot_pal, ktui_theme->slot, sizeof(slot_pal));
	slot_have = 1;
	memset(lit_set, 0, sizeof(lit_set));
}

static uint8_t nearest_slot(uint32_t rgb)
{
	unsigned h = (unsigned)((rgb * 2654435761u) >> (32 - LIT_BITS));

	if (lit_set[h] && lit_key[h] == rgb)
		return lit_slot[h];

	uint8_t v = (uint8_t)ktui_theme_nearest(rgb);

	lit_key[h] = rgb;
	lit_slot[h] = v;
	lit_set[h] = 1;
	return v;
}



/*
 * THE DEFAULT COLOURS ARE SLOTS, NOT LITERALS — the rule the whole tree is
 * written under, and here it is load-bearing rather than tidy.
 *
 * A terminal's default foreground is a light grey and its default background
 * is black. Reducing that grey by RGB distance against eight phosphor greens
 * can land on the very slot the black reduces to, and then every character a
 * program writes is drawn in the colour of the screen behind it: the cells
 * hold the text, the window looks empty, and a dump still prints it because a
 * dump throws the colour away.
 *
 * "Default" means whatever THIS desktop calls text and background, so it is
 * answered with the theme's own slots and no arithmetic. A colour a program
 * actually asked for is still reduced — including one that reduces to its own
 * background, because a program that writes black on black meant to.
 */
static uint8_t attr_fg(const struct kvt_screen_attr *a)
{
	if (a->fccode == KVT_COLOR_FOREGROUND)
		return KT_TEXT;

	/*
	 * THE COLOUR THE TERMINAL RESOLVED, whether it came from an index or
	 * from a 24-bit request. The vte has already turned a palette index
	 * into rgb through the palette in force — which on this desktop is the
	 * eighteen colours `kdos theme` generated — so reducing the index
	 * through a built-in VGA table instead threw that away: the file was
	 * read, installed and then ignored for every one of the sixteen
	 * colours it exists to set. It is also what applies bold's brightening,
	 * which is resolved into the same fields.
	 */
	return nearest_slot(((uint32_t)a->fr << 16) |
			    ((uint32_t)a->fg << 8) | (uint32_t)a->fb);
}

static uint8_t attr_bg(const struct kvt_screen_attr *a)
{
	if (a->bccode == KVT_COLOR_BACKGROUND)
		return KT_BG;

	return nearest_slot(((uint32_t)a->br << 16) |
			    ((uint32_t)a->bg << 8) | (uint32_t)a->bb);
}

struct grid {
	KtuiCell *cells;
	int w, h;
	/*
	 * WHERE THE LAST WIDE GLYPH PUT ITS CONTINUATION, so the blank the
	 * screen sends for that same cell is recognised without reading the
	 * buffer back. Reading it meant the whole grid had to be filled with
	 * blanks first — a second full pass over every cell of every frame,
	 * immediately overwritten — because a marker left by the PREVIOUS
	 * frame would otherwise swallow a real character.
	 */
	int cont_x, cont_y;
};

static int draw_cb(struct kvt_screen *con, uint64_t id, const uint32_t *ch,
		   size_t len, unsigned int width, unsigned int posx,
		   unsigned int posy, const struct kvt_screen_attr *attr,
		   kvt_age_t age, void *data)
{
	struct grid *g = data;
	KtuiCell c;

	(void)con;
	(void)id;
	(void)age;

	if ((int)posx >= g->w || (int)posy >= g->h)
		return 0;

	/*
	 * THE SCREEN WALKS EVERY CELL, the one a wide glyph already owns
	 * included — it arrives as a blank, immediately after the glyph, and
	 * is dropped because the glyph before it already covered that cell.
	 * The position is remembered from writing it rather than read back out
	 * of the grid, so this frame's marker is told from one the last frame
	 * left in the same place.
	 */
	if ((int)posx == g->cont_x && (int)posy == g->cont_y)
		return 0;

	/*
	 * A combining sequence collapses to its BASE codepoint: a KtuiCell
	 * holds one. The mark is already gone by the time it reaches here —
	 * kvt_screen_write drops a zero-width symbol before it ever reaches a
	 * cell — so this reads the first codepoint of whatever the screen
	 * stored and nothing downstream has to know about composed symbols.
	 */
	c.ch = len ? ch[0] : ' ';
	if (!c.ch)
		c.ch = ' ';

	c.fg = attr_fg(attr);
	c.bg = attr_bg(attr);
	c.attr = KT_A_NONE;
	c.fgc = c.bgc = c.ulc = 0;

	/*
	 * THE LITERAL BESIDE THE SLOT, AND ONLY FOR A COLOUR OUTSIDE THE
	 * SIXTEEN.
	 *
	 * The sixteen named colours are what a palette NAMES: they are the
	 * theme's on a real VT, where the kernel is drawing them out of the
	 * colour map this desktop installed, and a terminal whose red stopped
	 * following `kdos theme` would be the one window on the screen wearing
	 * somebody else's scheme. Everything else — the 216-colour cube, the
	 * greys, a 24-bit SGR — is a colour the program chose exactly, and
	 * reducing it to eight slots is what loses the picture. `fccode < 0`
	 * is precisely that boundary: the vte resolves an index of 16 or more
	 * to RGB and clears the code.
	 *
	 * The slot stays either way, so a consumer that reads slots alone — a
	 * dump, a golden, a sixteen-colour terminal — draws exactly what it
	 * drew before.
	 */
	if (attr->fccode < 0) {
		c.fgc = ((uint32_t)attr->fr << 16) |
			((uint32_t)attr->fg << 8) | (uint32_t)attr->fb;
		c.attr |= KT_A_FGRGB;
	}
	if (attr->bccode < 0) {
		c.bgc = ((uint32_t)attr->br << 16) |
			((uint32_t)attr->bg << 8) | (uint32_t)attr->bb;
		c.attr |= KT_A_BGRGB;
	}
	if (attr->ul_rgb) {
		c.ulc = ((uint32_t)attr->ulr << 16) |
			((uint32_t)attr->ulg << 8) | (uint32_t)attr->ulb;
		c.attr |= KT_A_ULCOLOR;
	}
	if (attr->ul_style)
		c.attr |= KT_UL_SET(attr->ul_style);

	if (attr->bold)
		c.attr |= KT_A_BOLD;
	if (attr->underline)
		c.attr |= KT_A_UNDERLINE;
	if (attr->inverse)
		c.attr |= KT_A_REVERSE;
	/* The three styles a KtuiCell has room for. `blink` and `dim` have no
	 * bit and are dropped here rather than approximated: a blink drawn as
	 * bold and a dim drawn as normal are both a lie about the text, and
	 * the cell is the one place that can say so. */
	if (attr->italic)
		c.attr |= KT_A_ITALIC;
	if (attr->strike)
		c.attr |= KT_A_STRIKE;
	if (attr->overline)
		c.attr |= KT_A_OVERLINE;

	g->cells[posy * g->w + posx] = c;

	/*
	 * A double-width glyph owns the cell after it. The marker carries the
	 * same colours, because a backend that skips it still paints its
	 * background.
	 */
	if (width > 1 && (int)posx + 1 < g->w) {
		c.ch = KTUI_WIDE_CONT;
		g->cells[posy * g->w + posx + 1] = c;
		g->cont_x = (int)posx + 1;
		g->cont_y = (int)posy;
	}

	return 0;
}

/*
 * Render the screen into `cells`, which is `w` by `h`. Returns the age the
 * screen reported, which a caller may keep to know whether anything moved.
 *
 * The grid is filled completely: a screen smaller than the buffer leaves the
 * remainder as blanks in the background slot, never as whatever was there
 * before.
 */
kvt_age_t kvt_grid_render(struct kvt_screen *con, KtuiCell *cells, int w, int h)
{
	struct grid g = { cells, w, h, -1, -1 };
	/*
	 * Every field, the literals included: the cell is compared whole by
	 * every consumer downstream — the painter's row diff, the tty
	 * backend's style test — so a field left uninitialised is a cell that
	 * differs from itself and a frame that is repainted for ever.
	 */
	KtuiCell blank = { .ch = ' ', .fg = KT_TEXT, .bg = KT_BG,
			   .attr = KT_A_NONE, .fgc = 0, .bgc = 0, .ulc = 0 };
	if (!con || !cells || w <= 0 || h <= 0)
		return 0;

	/* The palette in force decides every reduction below, so it is asked
	 * once a frame rather than once a cell. */
	slot_sync();

	unsigned sx = kvt_screen_get_width(con);
	unsigned sy = kvt_screen_get_height(con);

	/*
	 * ONLY WHAT THE SCREEN WILL NOT WRITE. It writes every cell of its own
	 * size_x by size_y, so filling those first was a full pass over the
	 * grid that the draw immediately overwrote — the single most expensive
	 * thing on this path after the conversion itself. What is left is the
	 * margin a buffer larger than the screen has, and it is blanked rather
	 * than left holding whatever was there before.
	 */
	for (int y = 0; y < h; y++) {
		int x0 = y < (int)sy ? (int)sx : 0;

		for (int x = x0; x < w; x++)
			cells[y * w + x] = blank;
	}

	return kvt_screen_draw(con, draw_cb, &g);
}
