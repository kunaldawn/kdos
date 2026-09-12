/* libkwm — cycling a ring of windows, and finding an occupied workspace.
 * See kwm.h. Ported from cycle.c's get_next_selected_view() and
 * workspaces.c's get_adjacent_occupied().
 */

#include "kwm.h"

int
kwm_ring_next(int n, int cur, int dir)
{
	if (n <= 0 || cur < 0 || cur >= n)
		return -1;

	/*
	 * The compositor walks a list whose head is a sentinel and steps over
	 * it when it lands there, which is what makes the ring close. Over an
	 * array that is the same thing as wrapping.
	 */
	if (dir > 0)
		return (cur + 1) % n;

	return (cur + n - 1) % n;
}

int
kwm_ws_adjacent(const unsigned char *occupied, int n, int cur,
		int reverse, int wrap)
{
	if (!occupied || n <= 0 || cur < 0 || cur >= n)
		return -1;

	int i = reverse ? cur - 1 : cur + 1;
	int wrapped = 0;

	for (;;) {
		if (i < 0 || i >= n) {
			/*
			 * The list head. Wrapping is allowed ONCE — a second
			 * pass over a set of workspaces that are all empty
			 * would never terminate.
			 */
			if (!wrap || wrapped)
				return -1;
			i = reverse ? n - 1 : 0;
			wrapped = 1;
			continue;
		}

		/* Full circle: every other workspace was empty. */
		if (i == cur)
			return -1;

		if (occupied[i])
			return i;

		i = reverse ? i - 1 : i + 1;
	}
}
