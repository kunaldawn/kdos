/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   vtrender — a recorded terminal stream, as the grid it produces
 *
 * Reads one of the `.esc` recordings beside this file and prints the 80x24
 * screen libkvt ends up with. That printout is the golden.
 *
 * FED IN SMALL, UNEVEN CHUNKS on purpose. A terminal never receives a whole
 * frame at once — a pty hands over whatever is in the buffer — so an escape
 * sequence split across two reads is the ordinary case, not the edge one. A
 * parser that only works on a complete sequence passes a single-write test and
 * corrupts the screen on a real terminal.
 *
 * ONLY THE CHARACTERS, not the colours. The palette is asserted in the libkvt
 * block of the self-test, where one slot at a time can be named; a golden that
 * carried colour too would fail on a theme change and say "vim drifted".
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ktui.h"
#include "kvt.h"

#define W 80
#define H 24

static KtuiCell cells[W * H];

static void on_write(struct kvt_vte *vte, const char *u8, size_t len,
		     void *data)
{
	/* The terminal's replies to the host — a device-attributes answer, a
	 * cursor report. Nothing reads them: a recording is one direction, and
	 * a test that answered would be testing the answer. */
	(void)vte; (void)u8; (void)len; (void)data;
}

int main(int argc, char **argv)
{
	struct kvt_screen *scr;
	struct kvt_vte *vte;
	char *buf;
	long n;
	FILE *f;

	if (argc != 2) {
		fprintf(stderr, "usage: vtrender <stream.esc>\n");
		return 2;
	}
	f = fopen(argv[1], "rb");
	if (!f) {
		perror(argv[1]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || !(buf = malloc((size_t)n + 1))) {
		fclose(f);
		return 1;
	}
	n = (long)fread(buf, 1, (size_t)n, f);
	fclose(f);

	if (kvt_screen_new(&scr, NULL, NULL) != 0)
		return 1;
	if (kvt_screen_resize(scr, W, H) != 0)
		return 1;
	if (kvt_vte_new(&vte, scr, on_write, NULL, NULL, NULL) != 0)
		return 1;

	/* 7, 13, 1, 64, 3, … — coprime-ish sizes that walk across sequence
	 * boundaries rather than landing on them. */
	static const size_t chunk[] = { 7, 13, 1, 64, 3, 29, 2, 128 };
	size_t at = 0, i = 0;

	while (at < (size_t)n) {
		size_t c = chunk[i++ % (sizeof(chunk) / sizeof(chunk[0]))];

		if (c > (size_t)n - at)
			c = (size_t)n - at;
		kvt_vte_input(vte, buf + at, c);
		at += c;
	}
	kvt_grid_render(scr, cells, W, H);

	for (int y = 0; y < H; y++) {
		char row[W * 4 + 1];
		size_t o = 0;

		for (int x = 0; x < W; x++) {
			uint32_t ch = cells[y * W + x].ch;

			/*
			 * A CONTINUATION CELL IS A SPACE, not a repeat of the
			 * character before it: a wide glyph occupies two cells
			 * and printing it twice would put a column of text out
			 * of step with the screen it describes.
			 */
			if (ch == KTUI_WIDE_CONT)
				ch = ' ';
			/* Anything outside printable ASCII is a dot. The
			 * golden is read by a person, and a box-drawing glyph
			 * that renders as a question mark in one terminal and a
			 * box in another is a diff nobody can act on. */
			row[o++] = (ch >= 32 && ch < 127) ? (char)ch : '.';
		}
		while (o && row[o - 1] == ' ')
			o--;
		row[o] = '\0';
		printf("%s\n", row);
	}

	/*
	 * THEN THE ATTRIBUTES, one letter per cell.
	 *
	 * A style has no character to show, so a golden holding only the text
	 * cannot tell an italic comment from an upright one — and the whole
	 * point of carrying italic, strike and overline is that a person sees
	 * them. Colour stays out: it moves with the theme, and a palette
	 * change reading as "vim drifted" is a test that changed its own
	 * question. An attribute is the program's own and moves with nothing.
	 *
	 * The first style set wins the cell, in this order, so a diff names
	 * one thing rather than a combination.
	 */
	printf("--\n");
	for (int y = 0; y < H; y++) {
		char row[W + 1];
		size_t o = 0;

		for (int x = 0; x < W; x++) {
			/* The WHOLE attribute: the underline's shape lives
			 * above the wire's byte, and a uint8_t here would
			 * print every shape as a plain line. */
			unsigned a = cells[y * W + x].attr;
			char c = '.';

			if (a & KT_A_ITALIC)
				c = 'i';
			else if (a & KT_A_STRIKE)
				c = 's';
			else if (a & KT_A_OVERLINE)
				c = 'o';
			else if (a & KT_A_UNDERLINE)
				/* The shape as its own digit, because a curly
				 * and a straight line are the same attribute
				 * and different pictures. */
				c = KT_UL_STYLE(a) ? (char)('0' + KT_UL_STYLE(a))
						   : 'u';
			else if (a & KT_A_BOLD)
				c = 'b';
			else if (a & KT_A_REVERSE)
				c = 'r';
			row[o++] = c;
		}
		while (o && row[o - 1] == '.')
			o--;
		row[o] = '\0';
		printf("%s\n", row);
	}

	/*
	 * AND WHICH CELLS CARRY A COLOUR OF THEIR OWN — never WHICH colour.
	 *
	 * The boundary is the thing worth pinning: the sixteen named colours
	 * reduce to the theme's slots and follow `kdos theme`, and everything
	 * above them is a literal the program chose. A golden holding the
	 * values would drift on a palette change and say "vim drifted"; one
	 * holding the DECISION drifts only when the rule does.
	 *
	 * `f` foreground, `b` background, `B` both, `u` the underline's own.
	 */
	printf("--\n");
	for (int y = 0; y < H; y++) {
		char row[W + 1];
		size_t o = 0;

		for (int x = 0; x < W; x++) {
			unsigned a = cells[y * W + x].attr;
			char c = '.';

			if ((a & (KT_A_FGRGB | KT_A_BGRGB)) ==
			    (KT_A_FGRGB | KT_A_BGRGB))
				c = 'B';
			else if (a & KT_A_FGRGB)
				c = 'f';
			else if (a & KT_A_BGRGB)
				c = 'b';
			else if (a & KT_A_ULCOLOR)
				c = 'u';
			row[o++] = c;
		}
		while (o && row[o - 1] == '.')
			o--;
		row[o] = '\0';
		printf("%s\n", row);
	}

	/*
	 * AND WHICH CELLS ARE A HYPERLINK, as the id itself — `1` for the
	 * first address this stream named, `2` for the second, `+` past nine.
	 *
	 * The id rather than a flag, because the two things worth pinning are
	 * that one run is one link and that the SAME address twice is the same
	 * link: a table that stopped interning would show two digits where the
	 * golden has one, and a link that leaked past its close would show a
	 * digit under text that is not a link at all.
	 *
	 * A link lives on the screen's own cell and never on a KtuiCell, so
	 * this block is read from the screen rather than from the grid above.
	 */
	printf("--\n");
	for (int y = 0; y < H; y++) {
		char row[W + 1];
		size_t o = 0;

		for (int x = 0; x < W; x++) {
			unsigned int id = kvt_screen_link_at(scr,
							     (unsigned int)x,
							     (unsigned int)y);

			row[o++] = !id ? '.'
				 : id <= 9 ? (char)('0' + id)
					   : '+';
		}
		while (o && row[o - 1] == '.')
			o--;
		row[o] = '\0';
		printf("%s\n", row);
	}

	/*
	 * AND THE PROMPT MARKS, one character per ROW because that is what a
	 * mark is — a property of the line, not of a cell. `?` is a prompt
	 * whose command has not finished, `0` one that succeeded and `!` one
	 * that failed; `.` is a line no shell marked.
	 *
	 * The status is what makes this worth a block: the mark lands on the
	 * PROMPT the command was typed at rather than on the line the shell
	 * reported it from, and those are several lines apart in any stream
	 * with output in it.
	 */
	printf("--\n");
	for (int y = 0; y < H; y++) {
		int status = -1;
		int mark = kvt_screen_mark_at(scr, (unsigned int)y, &status);

		printf("%c", !mark ? '.'
			   : status < 0 ? '?'
			   : status ? '!' : '0');
	}
	printf("\n");

	kvt_vte_unref(vte);
	kvt_screen_unref(scr);
	free(buf);
	return 0;
}
