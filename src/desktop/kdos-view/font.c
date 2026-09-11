/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view — what a font name is, where the stepped one is kept, and
 *   which faces this display can render
 *
 * A FILE OF ITS OWN because it is the part of the font work that can be tested
 * without a screen: everything else is a DRM device and a glyph cache, and
 * this is string arithmetic over a fontconfig name plus one pipe.
 *
 * THE LIST IS THE DISPLAY'S AND IS GATHERED HERE. A view may be at the far end
 * of an ssh link with its own machine's fonts, so a list gathered by the
 * session would be the wrong machine's offered to this screen. What travels
 * back the other way is an index into what this file produced.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
/* view.h names KtuiBackend in its ttypix block, so the toolkit's header comes
 * first here as it does in every other file of this program. */
#include "ktui.h"
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

/*
 * A FAMILY AS FONTCONFIG'S NAME SYNTAX, not as text.
 *
 * Three characters mean something in a name: `-` opens a size, `:` opens a
 * property and `,` opens an alternate family. Several shipped families carry
 * one — `DejaVu Sans,DejaVu Sans Condensed` is one string naming two faces —
 * so a name pasted in verbatim resolves to a DIFFERENT face and the screen
 * silently wears something else. A backslash is escaped for the same reason it
 * is the escape.
 */
int view_font_escape(const char *family, char *out, size_t n)
{
	size_t o = 0;

	if (!out || !n)
		return 0;
	out[0] = '\0';
	if (!family)
		return 0;
	for (const char *p = family; *p; p++) {
		if (*p == '-' || *p == ':' || *p == ',' || *p == '\\') {
			if (o + 2 >= n)
				return 0;
			out[o++] = '\\';
		} else if (o + 1 >= n) {
			return 0;
		}
		out[o++] = *p;
	}
	out[o] = '\0';
	return o > 0;
}

/* The size `base` is at, as the key that carries it, so a face swapped here
 * keeps the size the chords put the screen at. A name with neither key is the
 * loader's own default said out loud. */
static void size_suffix(const char *base, char *out, size_t n)
{
	static const char *const keys[] = { ":pixelsize=", ":size=" };

	for (unsigned k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
		const char *at = base ? strstr(base, keys[k]) : NULL;

		if (!at)
			continue;

		int size = atoi(at + strlen(keys[k]));

		if (size > 0) {
			snprintf(out, n, "%s%d", keys[k], size);
			return;
		}
	}
	snprintf(out, n, ":size=11");
}

/*
 * `fc-list` AND NOT THE LIBRARY. Enumerating through fontconfig's C API would
 * put `-lfontconfig` on this program and on everything that links its files
 * for a test; the answer is one line per face on a pipe, and the tool that
 * prints it is already on the image because fontconfig is.
 */
int view_font_list(const char *cur, char names[][VIEW_FONT_NAME], int max)
{
	static char raw[64 * 1024];
	const char *fixture = getenv("KDOS_FONT_LIST");
	char suffix[64];
	int n = 0;

	if (!names || max <= 0)
		return 0;
	raw[0] = '\0';
	if (fixture && *fixture) {
		size_t len = 0;
		char *buf = kb_read_whole(fixture, &len);

		if (!buf)
			return 0;
		snprintf(raw, sizeof(raw), "%s", buf);
		free(buf);
	} else {
		KbArgv a = { 0 };

		kb_argv_add(&a, "fc-list");
		/* `spacing=100` IS FC_MONO. A proportional face divided into a
		 * cell grid gives cells twice as wide as they are tall, and
		 * nothing in the pipeline notices. */
		kb_argv_add(&a, ":spacing=100");
		kb_argv_add(&a, "family");
		kb_argv_end(&a);
		if (kb_run_feed_capture(&a, "", 0, raw, sizeof(raw)) != 0)
			return 0;
	}

	size_suffix(cur, suffix, sizeof(suffix));

	for (char *line = strtok(raw, "\r\n"); line && n < max;
	     line = strtok(NULL, "\r\n")) {
		char fam[VIEW_FONT_NAME], esc[VIEW_FONT_NAME];
		char *comma;

		/* THE FIRST ALTERNATE IS THE FAMILY. fc-list prints every name
		 * a face answers to on one line; the rest are aliases of the
		 * same file, and listing them would be the same face offered
		 * several times under different words. */
		snprintf(fam, sizeof(fam), "%s", line);
		comma = strchr(fam, ',');
		if (comma)
			*comma = '\0';
		while (*fam && fam[strlen(fam) - 1] == ' ')
			fam[strlen(fam) - 1] = '\0';
		if (!*fam)
			continue;
		if (!view_font_escape(fam, esc, sizeof(esc)))
			continue;

		char want[VIEW_FONT_NAME];

		if (snprintf(want, sizeof(want), "%s%s", esc, suffix) >=
		    (int)sizeof(want))
			continue;

		int seen = 0;

		for (int i = 0; i < n; i++)
			if (!strcmp(names[i], want))
				seen = 1;
		if (seen)
			continue;
		snprintf(names[n], VIEW_FONT_NAME, "%s", want);
		n++;
	}
	return n;
}
