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
 * xterm's 256, computed rather than tabulated: 0-15 are the standard ANSI
 * values, 16-231 a 6x6x6 cube on an uneven ramp, and 232-255 a grey run. The
 * ramp is not linear and guessing it wrong shifts every mid-tone.
 */
static uint32_t xterm_rgb(int idx)
{
	static const uint32_t base[16] = {
		0x000000, 0xaa0000, 0x00aa00, 0xaa5500,
		0x0000aa, 0xaa00aa, 0x00aaaa, 0xaaaaaa,
		0x555555, 0xff5555, 0x55ff55, 0xffff55,
		0x5555ff, 0xff55ff, 0x55ffff, 0xffffff,
	};
	static const int step[6] = { 0, 95, 135, 175, 215, 255 };

	if (idx < 0)
		return 0;
	if (idx < 16)
		return base[idx];

	if (idx < 232) {
		int i = idx - 16;
		int r = step[(i / 36) % 6];
		int g = step[(i / 6) % 6];
		int b = step[i % 6];

		return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	}

	if (idx < 256) {
		uint32_t l = (uint32_t)(8 + (idx - 232) * 10);

		return (l << 16) | (l << 8) | l;
	}

	return 0xffffff;
}

/*
 * ONE RULE FOR EVERY COLOUR: the nearest of the theme's eight slots. The rule
 * itself is libktui's, so a terminal's SGR and a picture's average tint reduce
 * to a slot the same way — two implementations would drift and `kdos theme
 * amber` would move one of them.
 */
static uint8_t nearest_slot(uint32_t rgb)
{
	return (uint8_t)ktui_theme_nearest(rgb);
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
	if (a->fccode >= 0)
		return nearest_slot(xterm_rgb(a->fccode));

	return nearest_slot(((uint32_t)a->fr << 16) |
			    ((uint32_t)a->fg << 8) | (uint32_t)a->fb);
}

static uint8_t attr_bg(const struct kvt_screen_attr *a)
{
	if (a->bccode == KVT_COLOR_BACKGROUND)
		return KT_BG;
	if (a->bccode >= 0)
		return nearest_slot(xterm_rgb(a->bccode));

	return nearest_slot(((uint32_t)a->br << 16) |
			    ((uint32_t)a->bg << 8) | (uint32_t)a->bb);
}

struct grid {
	KtuiCell *cells;
	int w, h;
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
	 * included — it arrives as a blank, immediately after the glyph. The
	 * marker is placed when the glyph is written, so a blank landing on top
	 * of one is that same cell coming round again and is dropped. The fill
	 * below uses a space, so only a real marker matches.
	 */
	if (g->cells[posy * g->w + posx].ch == KTUI_WIDE_CONT)
		return 0;

	/*
	 * A combining sequence collapses to its BASE codepoint: a KtuiCell
	 * holds one, and the marks are lost here rather than in the screen —
	 * which is why the symbol table upstream keeps is worth keeping, for
	 * the day the cell can carry them.
	 */
	c.ch = len ? ch[0] : ' ';
	if (!c.ch)
		c.ch = ' ';

	c.fg = attr_fg(attr);
	c.bg = attr_bg(attr);
	c.attr = KT_A_NONE;

	if (attr->bold)
		c.attr |= KT_A_BOLD;
	if (attr->underline)
		c.attr |= KT_A_UNDERLINE;
	if (attr->inverse)
		c.attr |= KT_A_REVERSE;

	g->cells[posy * g->w + posx] = c;

	/*
	 * A double-width glyph owns the cell after it. The marker carries the
	 * same colours, because a backend that skips it still paints its
	 * background.
	 */
	if (width > 1 && (int)posx + 1 < g->w) {
		c.ch = KTUI_WIDE_CONT;
		g->cells[posy * g->w + posx + 1] = c;
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
	struct grid g = { cells, w, h };
	KtuiCell blank;

	if (!con || !cells || w <= 0 || h <= 0)
		return 0;

	blank.ch = ' ';
	blank.fg = KT_TEXT;
	blank.bg = KT_BG;
	blank.attr = KT_A_NONE;

	for (int i = 0; i < w * h; i++)
		cells[i] = blank;

	return kvt_screen_draw(con, draw_cb, &g);
}
