/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view — what a font name is, and where the stepped one is kept
 *
 * A FILE OF ITS OWN because it is the only part of the font chords that can be
 * tested without a screen: everything else is a DRM device and a glyph cache,
 * and this is string arithmetic over a fontconfig name.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "view.h"

int view_font_state_path(char *out, size_t n)
{
	/* THROUGH libkbase, because the state directory has two spellings and
	 * a program that keeps its own copy of the second one writes where
	 * nothing reads after `$XDG_STATE_HOME` appears. */
	return kb_state_path("kdos/con-font", out, n);
}

int view_font_stepped(const char *base, int step, char *out, size_t n)
{
	const char *key = ":pixelsize=";
	const char *at = base ? strstr(base, key) : NULL;
	int lo = 8, hi = 72, size;

	if (!out || !n)
		return 0;
	if (!at) {
		key = ":size=";
		at = base ? strstr(base, key) : NULL;
		lo = 5;
		hi = 48;
	}
	if (at) {
		size = atoi(at + strlen(key));
		if (size <= 0)
			return 0;
	} else {
		/* kcell_font_load()'s own default, spelled out so a step from
		 * "no font at all" lands somewhere a person recognises. */
		base = "monospace";
		key = ":size=";
		size = 11;
	}

	size += step;
	if (size < lo)
		size = lo;
	if (size > hi)
		size = hi;

	if (at) {
		/* Whatever follows the size is kept: a name carrying a style
		 * or a fallback after it is a name the person wrote. */
		const char *rest = strchr(at + 1, ':');
		int head = (int)(at - base);

		return snprintf(out, n, "%.*s%s%d%s", head, base, key, size,
				rest ? rest : "") < (int)n;
	}
	return snprintf(out, n, "%s%s%d", base, key, size) < (int)n;
}
