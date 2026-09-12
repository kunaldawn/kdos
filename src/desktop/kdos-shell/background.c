/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The console desktop's background: a picture made of characters
 *
 * A BBS drew art on the screen it welcomed you to, and the console desktop has
 * the same screen and the same character cells. This turns a text file with
 * SGR colour in it into a grid of cells `kdos-desk` blits behind its icons.
 *
 * THE PARSER IS libkvt's, NOT A SECOND ONE. The file is exactly what a program
 * writes to a terminal, so the thing that reads it is the thing that reads a
 * terminal: a screen, a state machine, the bytes, and the same render boundary
 * every terminal window on this desktop goes through. A private SGR reader
 * here would be a second answer to what `ESC [ 1 ; 36 m` means, and it would
 * drift.
 *
 * WHICH IS ALSO WHAT MAKES THE ART FOLLOW THE THEME. `kvt_grid_render()`
 * reduces every colour — the ANSI sixteen, the 256 and truecolour alike — to
 * the eight slots by nearest distance, so a piece written in cyan is drawn in
 * whatever this accent's cyan-most slot is and `kdos theme` moves it with
 * everything else. Art that carried literal colours would be the one
 * rectangle of the desktop a retint could not reach.
 *
 * NO CHILD AND NO DESCRIPTOR. `kvt_term` is a screen plus a pty plus a
 * process; this needs the first of those three, so it builds the screen and
 * the state machine directly and feeds them a buffer.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kvt.h"
#include "background.h"

/*
 * The cap is a screen, not a document. A background is drawn behind icons on a
 * display that exists, and a file claiming ten thousand columns is a file that
 * would allocate a gigabyte of cells for a picture nobody can see.
 */
#define BG_MAX_W 400
#define BG_MAX_H 200
/* The file itself. A quarter of a megabyte is far past any character art and
 * well short of anything that would matter to read. */
#define BG_MAX_BYTES (256 * 1024)

/* The state machine wants somewhere to send a reply — a device-attributes
 * question, a cursor report. A file is not a program and there is nothing to
 * answer, so the replies are dropped here rather than left to a null pointer. */
static void bg_reply(struct kvt_vte *vte, const char *u8, size_t len,
		     void *data)
{
	(void)vte;
	(void)u8;
	(void)len;
	(void)data;
}

/*
 * The art's own size: the longest line in cells, and how many lines there are.
 *
 * MEASURED IN CELLS AND NOT IN BYTES, because the whole point of the format is
 * that it is UTF-8 box drawing: a line of forty box characters is 120 bytes
 * and forty columns. SGR is skipped rather than counted — an escape occupies
 * no cell — and nothing else in the file may move the cursor, which is the
 * rule the shipped pieces keep and the reason this measure can be this simple.
 */
static void bg_measure(const char *s, size_t len, int *w, int *h)
{
	int col = 0, wide = 0, rows = 0, any = 0;

	*w = *h = 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];

		if (c == 0x1b) {
			/* CSI ... final, or a two-byte escape. Everything the
			 * shipped pieces use is CSI m; anything else is
			 * skipped by the same rule rather than counted. */
			i++;
			if (i < len && s[i] == '[') {
				i++;
				while (i < len && (unsigned char)s[i] < 0x40)
					i++;
			}
			continue;
		}
		if (c == '\n') {
			rows++;
			if (col > wide)
				wide = col;
			col = 0;
			continue;
		}
		if (c == '\r')
			continue;
		/* A UTF-8 continuation byte is not a column. */
		if ((c & 0xc0) == 0x80)
			continue;
		col++;
		any = 1;
	}
	if (col > wide)
		wide = col;
	if (col > 0 || (any && rows == 0))
		rows++;

	*w = wide > BG_MAX_W ? BG_MAX_W : wide;
	*h = rows > BG_MAX_H ? BG_MAX_H : rows;
}

KtuiCell *sh_bg_load(const char *path, int *out_w, int *out_h)
{
	struct kvt_screen *scr = NULL;
	struct kvt_vte *vte = NULL;
	KtuiCell *cells = NULL;
	char *text;
	size_t len = 0;
	int w = 0, h = 0;

	*out_w = *out_h = 0;
	text = kb_read_whole(path, &len);
	if (!text)
		return NULL;
	if (len > BG_MAX_BYTES)
		len = BG_MAX_BYTES;

	bg_measure(text, len, &w, &h);
	if (w < 1 || h < 1)
		goto out;

	if (kvt_screen_new(&scr, NULL, NULL) < 0)
		goto out;
	if (kvt_screen_resize(scr, (unsigned)w, (unsigned)h) < 0)
		goto out;
	if (kvt_vte_new(&vte, scr, bg_reply, NULL, NULL, NULL) < 0)
		goto out;

	/*
	 * TWO MODES BEFORE THE ART, and both are what makes a FILE behave like
	 * a picture instead of like output.
	 *
	 * `?7l` turns auto-wrap off. A line exactly as wide as the screen —
	 * which the widest line always is, because the screen is sized from it
	 * — would otherwise set the deferred wrap and the newline after it
	 * would advance twice, so every second row of the piece came out blank
	 * and the first scrolled off the top.
	 *
	 * `20h` is newline mode: LF also returns the carriage. A text file has
	 * no CR in it, and without this the second line starts in the column
	 * the first ended in, which is the last one — every row but the first
	 * kept only its final character.
	 */
	kvt_vte_input(vte, "\033[?7l\033[20h", 10);
	/* AND NO TRAILING NEWLINE. The screen is exactly as tall as the art,
	 * so a newline after the last row moves off the bottom and scrolls the
	 * whole piece up by one. */
	while (len && (text[len - 1] == '\n' || text[len - 1] == '\r'))
		len--;
	kvt_vte_input(vte, text, len);

	cells = calloc((size_t)w * (size_t)h, sizeof(*cells));
	if (!cells)
		goto out;
	kvt_grid_render(scr, cells, w, h);
	*out_w = w;
	*out_h = h;

out:
	if (vte)
		kvt_vte_unref(vte);
	if (scr)
		kvt_screen_unref(scr);
	free(text);
	return cells;
}

/*
 * WHICH FILE. A person's own art wins outright, and the shipped pieces are
 * reached by NAME rather than by path — the name is what the chord cycles and
 * what `kdos background` writes, and a path in that state file would be a way
 * to point the desktop at any file on the machine.
 *
 * `none` is an honest off and is spelled, because a missing state file means
 * "nobody has chosen" and an empty one would then be indistinguishable from a
 * choice to have nothing.
 */
int sh_bg_path(char *out, size_t n)
{
	char name[64] = "";
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char own[512], sf[512];

	if (cfg && *cfg)
		snprintf(own, sizeof(own), "%s/kdos/background.txt", cfg);
	else
		snprintf(own, sizeof(own), "%s/.config/kdos/background.txt",
			 kb_home_dir());
	if (access(own, R_OK) == 0) {
		snprintf(out, n, "%s", own);
		return 1;
	}

	if (!kb_state_path("kdos/background", sf, sizeof(sf)))
		return 0;
	if (kb_read_line_file(sf, name, sizeof(name)) <= 0 || !name[0])
		return 0;	/* nobody has chosen: the theme's ground */
	if (!strcmp(name, "none"))
		return 0;
	/* ONE NAME, AND ONLY A NAME. It becomes a path, so a separator in it
	 * is a path somebody chose. */
	for (const char *p = name; *p; p++)
		if (*p == '/' || *p == '.')
			return 0;
	snprintf(out, n, "%s/%s.txt", KB_BACKGROUND_DIR, name);
	return access(out, R_OK) == 0;
}
