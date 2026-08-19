/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shell — the window chrome the device apps share
 *
 *   ╔══════════════════════════════════════════════════════════════════╗
 *   ║▓▓ Network                                                      ▓▓║
 *   ║▓▓ wlan0 · connected to MYSSID                                  ▓▓║
 *   ╟──────────────────────────────────────────────────────────────────╢
 *   ║  Wireless                                                        ║
 *   ║ ▸ MYSSID                    ▂▄▆█  WPA2       connected           ║
 *   ║   neighbour-5G              ▂▄▆_  WPA2       saved               ║
 *   ╟──────────────────────────────────────────────────────────────────╢
 *   ║ joined MYSSID          [ Connect ] [ Forget ] [ Rescan ] [ Close ]║
 *   ╚══════════════════════════════════════════════════════════════════╝
 *
 * WHY THIS EXISTS. kdos-net, kdos-bt, kdos-audio and kdos-devices were four
 * lists with a box round them and a row of key hints at the bottom, and the
 * whole of their interface was "here are the words, now guess which letter
 * does the thing". That is a fine shape for a program somebody types the name
 * of and a poor one for the surface a person reaches by clicking `NET wlan0`
 * on a taskbar — which is how all four are actually opened.
 *
 * What a control panel of the Windows-XP/System-7 lineage has and these did
 * not is exactly three things, and none of them is decoration:
 *
 *   - A HEADER that says what the window is AND what its subject is doing
 *     right now, so the answer to "am I connected" is in the title rather
 *     than somewhere in a list.
 *   - GROUP HEADINGS, so a list of eight things reads as two groups of four.
 *   - REAL BUTTONS, labelled with verbs, clickable, and carrying the same key
 *     hint they always had. A key hint alone tells you what a key does; a
 *     button tells you what the program can DO, which is the question somebody
 *     opening it for the first time is asking.
 *
 * It is a shared file rather than four copies because four copies is four
 * places for the footer to end up on a different row — and the hit map here is
 * recorded from what was DRAWN, which is the rule the panel's applets already
 * keep and for the same reason: a hit map that outlives what it describes is
 * how a narrow window ends up doing the wrong thing to somebody's wifi.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kicon.h"
#include "kchrome.h"

/* ── the header band ───────────────────────────────────────────────────── */

int kch_header(int w, const char *icon_name, const char *title,
		     const char *subtitle, int icons_on)
{
	int icon = icons_on && icon_name ? kicon_slot(icon_name, 2, 1) : -1;
	int tx = 2;

	if (w < 8)
		return 1;
	/* FILL, then draw with the slots swapped — never KT_A_REVERSE over the
	 * text, which inverts only the cells a glyph covers and turns a
	 * two-word title into two lit blocks with a hole between them. */
	ktui_draw_fill(krect(1, 1, w - 2, 2), KT_ACCENT);
	if (icon >= 0) {
		/* 2x1: on a 16x32 cell that is a 32x32 square, on the title's
		 * own row. Two ROWS would centre it across the boundary
		 * between them and line up with nothing. */
		ktui_draw_sprite(krect(2, 1, 2, 1), icon, KT_SURFACE,
				 KT_ACCENT);
		tx = 5;
	}
	ktui_draw_text(tx, 1, w - tx - 2, title, KT_SURFACE, KT_ACCENT,
		       KT_A_NONE);
	if (subtitle && *subtitle)
		ktui_draw_text(tx, 2, w - tx - 2, subtitle, KT_SURFACE,
			       KT_ACCENT, KT_A_NONE);
	ktui_draw_hline(1, 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	return 4;			/* the first body row */
}

/* ── a group heading inside the body ───────────────────────────────────── */

void kch_group(int x, int y, int w, const char *label)
{
	int lw = ktui_utf8_width(label);

	if (w < lw + 2)
		return;
	ktui_draw_text(x, y, lw, label, KT_ACCENT, KT_BG, KT_A_NONE);
	/* The rule runs to the right margin, so the heading reads as a band
	 * rather than as another list row that happens to be a colour. */
	if (w > lw + 2)
		ktui_draw_hline(x + lw + 1, y, w - lw - 1, KT_G_HL, KT_DIM,
				KT_BG);
}

/* ── the button bar ────────────────────────────────────────────────────── */

static struct {
	int x, end, row;
} btn[SH_MAX_BTN];
static int nbtn;
static int hover_mx = -1, hover_my = -1;

void kch_hover(int mx, int my)
{
	hover_mx = mx;
	hover_my = my;
}

int kch_buttons(int w, int row, const struct kch_button *b, int n,
		      int focus)
{
	int total = 0;

	nbtn = 0;
	for (int i = 0; i < SH_MAX_BTN; i++)
		btn[i].x = btn[i].end = btn[i].row = 0;
	if (n > SH_MAX_BTN)
		n = SH_MAX_BTN;

	/*
	 * WHOLE BUTTONS, FEWER OF THEM — never half a row of them.
	 *
	 * The rule used to be all or nothing, which was right while these were
	 * full-screen windows and wrong the moment the same list became a
	 * fifty-column panel popup: the five buttons kdos-net wants come to
	 * sixty-one columns, so a popup showed NONE of them and fell back to a
	 * row of key hints — the interface the buttons were added to replace.
	 * They are ordered most-useful-first by every caller, so dropping from
	 * the RIGHT gives up Close (the popup dismisses on Esc and on a click
	 * away) before it gives up Connect. What is left of the row is still
	 * the status line, because the bar clears only its own span.
	 */
	while (n > 0) {
		total = 0;
		for (int i = 0; i < n; i++)
			total += ktui_utf8_width(b[i].label) + 5;
		if (total <= w - 4)
			break;
		n--;
	}
	if (n <= 0)
		return w - 2;

	int x = w - 2 - total, left = x;
	/*
	 * CLEAR THE WHOLE STRIP FIRST. The single column BETWEEN two buttons
	 * is not covered by either of them, so whatever was on the row showed
	 * through the gap: `[ Rescan ]s[ Wi-Fi Off ]`, photographed. Anything
	 * that overdraws part of a row has to own all of it.
	 *
	 * The RETURN VALUE is the other half of that rule: the status line
	 * shares this row, and clearing a span it was already drawn across cuts
	 * it off mid-word — `Enter [ Connect ]`, photographed on the fifty-two
	 * column popup, where the bar now fits and used to be dropped whole.
	 * The caller draws its status clipped to this column instead.
	 */
	ktui_draw_fill(krect(x, row, w - 2 - x, 1), KT_BG);
	for (int i = 0; i < n; i++) {
		int lw = ktui_utf8_width(b[i].label);
		int on = i == focus;
		int enabled = b[i].enabled;
		/*
		 * HOVER IS ITS OWN STATE, dimmer than the focus. Three levels
		 * rather than two, and they say three different things: this
		 * is the button Enter will press (accent), this is the one the
		 * pointer is on (fill), this is a button (nothing). Two would
		 * have to make the pointer take the keyboard's focus, and a
		 * mouse crossing a row must never change what a key does.
		 */
		int hot = enabled && hover_my == row && hover_mx >= x &&
			  hover_mx < x + lw + 4;
		int bg = on && enabled ? KT_ACCENT : hot ? KT_DIM : KT_BG;

		ktui_draw_text(x, row, 1, "[", hot ? KT_TEXT : KT_DIM, KT_BG,
			       KT_A_NONE);
		ktui_draw_fill(krect(x + 1, row, lw + 2, 1), bg);
		ktui_draw_text(x + 2, row, lw, b[i].label,
			       !enabled	  ? KT_DIM
			       : on	  ? KT_SURFACE
					  : KT_TEXT,
			       bg, KT_A_NONE);
		ktui_draw_text(x + lw + 3, row, 1, "]", hot ? KT_TEXT : KT_DIM,
			       KT_BG, KT_A_NONE);
		/* The span is what was DRAWN, and a disabled button still
		 * records one: a click on it has to be swallowed rather than
		 * falling through to whatever is behind it. */
		btn[nbtn].x = x;
		btn[nbtn].end = x + lw + 4;
		btn[nbtn].row = row;
		nbtn++;
		x += lw + 5;
	}
	return left;
}

int kch_button_at(int mx, int my)
{
	for (int i = 0; i < nbtn; i++)
		if (my == btn[i].row && mx >= btn[i].x && mx < btn[i].end)
			return i;
	return -1;
}

/* ── scrolling lists ───────────────────────────────────────────────────────
 *
 * See shell.h for the rule and for the libkwl half that makes it hold.
 */

int kch_list_wheel(int up, int *top, int n, int body)
{
	if (!top || body <= 0 || n <= body)
		return 0;		/* it fits: the wheel is a cursor step */

	int max = n - body;
	int t = *top + (up ? -SH_WHEEL_ROWS : SH_WHEEL_ROWS);

	if (t < 0)
		t = 0;
	if (t > max)
		t = max;
	*top = t;
	return 1;
}

void kch_list_clamp(int *top, int sel, int n, int body, int follow)
{
	if (!top || body <= 0)
		return;

	int max = n > body ? n - body : 0;

	if (follow) {
		if (sel < *top)
			*top = sel;
		if (sel >= *top + body)
			*top = sel - body + 1;
	}
	if (*top > max)
		*top = max;
	if (*top < 0)
		*top = 0;
}

void kch_list_scrollbar(int x, int y, int rows, int n, int top, int bg)
{
	if (rows <= 1 || n <= rows || x < 0 || x >= ktui_w)
		return;

	/* At least one cell of thumb, and never the whole track: a bar that
	 * fills its own length says the same thing as no bar at all. */
	int th = rows * rows / n;
	if (th < 1)
		th = 1;
	if (th > rows - 1)
		th = rows - 1;

	int span = rows - th;
	int max = n - rows;
	int at = max > 0 ? top * span / max : 0;

	if (at < 0)
		at = 0;
	if (at > span)
		at = span;
	for (int i = 0; i < rows; i++)
		ktui_draw_text(x, y + i, 1,
			       ktui_glyph[i >= at && i < at + th ? KT_G_FULL
								 : KT_G_SHADE],
			       i >= at && i < at + th ? KT_MID : KT_DIM, bg,
			       KT_A_NONE);
}
