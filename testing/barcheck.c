/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   barcheck — the scrollbar you can see is the scrollbar you can grab
 *
 * A BAR YOU CAN SEE IN ONE PLACE AND GRAB IN ANOTHER IS WORSE THAN NO BAR, so
 * the draw and the hit test are asserted against each other at every position
 * rather than each against a number somebody wrote down. The frame is rendered
 * offscreen and the thumb is FOUND IN THE CELLS — the test reads what was
 * drawn, which is the only way the two can be compared at all.
 *
 * THE INVARIANT IS ROUND-TRIP STABILITY, NOT EQUALITY. A twelve-row bar over a
 * hundred-row list draws one thumb cell per ten rows, so several tops share a
 * cell and a press can only answer one of them. What must hold is that the
 * answer draws the thumb where the hand is: both directions floor, so the
 * inverse has to round up or the thumb hops a row the instant it is grabbed.
 *
 * A BINARY OF ITS OWN, for the reason boxcheck has one: `selftest.c` links no
 * libkchrome, and pulling the chrome in to reach one function would bring an
 * icon atlas and a display layer with it. Two stubs are the whole cost.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include "kchrome.h"

/* Named by the scrollbar's own file and not reached by anything below. */
int kch_body_slot(void) { return KT_BG; }
int kicon_slot(const char *name, int cw, int ch)
{
	(void)name; (void)cw; (void)ch;
	return -1;
}


static int fails;
static void t(const char *what, int ok) {
	if (!ok) { printf("  FAIL %s\n", what); fails++; }
}

int main(void)
{
	ktui_offscreen_init(40, 20);
	ktui_draw_init();

	/* A 12-row bar over a 100-row list. */
	const int X = 39, Y = 2, ROWS = 12, N = 100;

	for (int top = 0; top <= N - ROWS; top += 7) {
		int w, h, first = -1, last = -1;

		/* DRAW FIRST, ALWAYS: kch_scrollbar() is what records the bar
		 * a press is measured against, including the `top` its caps
		 * step from. Asking before drawing asks about the last frame. */
		ktui_draw_clear();
		ktui_draw_fill(krect(0, 0, 40, 20), KT_BG);
		kch_scrollbar(0, X, Y, ROWS, N, top, KT_BG);
		const KtuiCell *c = ktui_draw_cells(&w, &h);
		for (int i = 1; i < ROWS - 1; i++)
			if (c[(Y + i) * w + X].fg == KT_MID) {
				if (first < 0) first = i;
				last = i;
			}
		t("a thumb is drawn between the caps", first > 0);
		t("the thumb stays inside the track", last <= ROWS - 2);

		/* The caps step exactly one row, and arm no drag. */
		int up = kch_scrollbar_press(0, X, Y);
		kch_scrollbar_release();
		t("the up cap steps one row", up == (top > 0 ? top - 1 : 0));
		t("the up cap arms no drag", kch_scrollbar_grabbed() == -1);

		int dn = kch_scrollbar_press(0, X, Y + ROWS - 1);
		kch_scrollbar_release();
		t("the down cap steps one row",
		  dn == (top < N - ROWS ? top + 1 : N - ROWS));

		/*
		 * GRABBING THE THUMB WHERE IT IS DRAWN MUST LEAVE IT THERE.
		 * Not "must answer the same top": a twelve-row bar over a
		 * hundred-row list draws one thumb cell per ten rows, so
		 * several tops share a position and the press can only answer
		 * one of them. What must hold is that the answer draws the
		 * thumb in the SAME place — otherwise it jumps out from under
		 * the hand that grabbed it.
		 */
		int got = kch_scrollbar_press(0, X, Y + (first + last) / 2);
		kch_scrollbar_release();
		t("a press returns a top inside the list",
		  got >= 0 && got <= N - ROWS);

		ktui_draw_clear();
		ktui_draw_fill(krect(0, 0, 40, 20), KT_BG);
		kch_scrollbar(0, X, Y, ROWS, N, got, KT_BG);
		const KtuiCell *c2 = ktui_draw_cells(&w, &h);
		int f2 = -1;
		for (int i = 1; i < ROWS - 1; i++)
			if (c2[(Y + i) * w + X].fg == KT_MID) { f2 = i; break; }
		t("a press on the thumb leaves the thumb where it was",
		  f2 == first);
	}

	/* A bar with no room for caps plus a track is not live. */
	ktui_draw_clear();
	kch_scrollbar(1, X, Y, 2, N, 0, KT_BG);
	t("a 2-row bar is not grabbable", kch_scrollbar_press(1, X, Y) == -1);

	printf("%s\n", fails ? "  FAILED" : "  draw and hit agree at every position");
	return fails != 0;
}
