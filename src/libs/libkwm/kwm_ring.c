/* libkwm — finding an occupied workspace. See kwm.h. Ported from
 * workspaces.c's get_adjacent_occupied().
 */

#include "kwm.h"

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
