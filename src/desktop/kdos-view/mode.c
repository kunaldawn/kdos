/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-view — which mode each screen was left wearing
 *
 * A FILE OF ITS OWN for the reason font.c is one: this is the part of the mode
 * work that can be tested with no screen at all. Everything else is a DRM
 * device; this is one line per connector.
 *
 * THE VIEW WRITES IT AND NOT THE PICKER. A view may be at the far end of an
 * ssh link driving its own machine's screens, so the end that knows what a
 * connector is called and what it is wearing is the only end that can record
 * it — the same argument that puts the font list here.
 *
 * A MODE IS A GEOMETRY AND NEVER AN INDEX. The index a picker sends is a row
 * in the list that screen published this boot; the next boot, a firmware
 * update or a different cable can publish them in another order, and an index
 * replayed onto that list is a resolution nobody chose. `WxH@mHz` names the
 * same mode or names nothing, and naming nothing leaves the screen on the one
 * the monitor prefers.
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

int view_mode_state_path(char *out, size_t n)
{
	/* THROUGH libkbase, because the state directory has two spellings and
	 * a program that keeps its own copy of the second one writes where
	 * nothing reads after `$XDG_STATE_HOME` appears. */
	return kb_state_path("kdos/con-modes", out, n);
}

/*
 * ONE LINE PER CONNECTOR: `<name> <W>x<H>@<mHz>`. A line for a name this
 * machine no longer has is kept rather than dropped — a monitor unplugged for
 * a week is the same monitor when it comes back, and a file that forgot it
 * would make every reconnection a fresh choice.
 */
static int line_name(const char *line, char *out, size_t n)
{
	size_t i = 0;

	while (line[i] && line[i] != ' ' && line[i] != '\t' && i + 1 < n) {
		out[i] = line[i];
		i++;
	}
	out[i] = '\0';
	return i > 0;
}

int view_mode_recall(const char *conn, int *w, int *h, int *refresh)
{
	char path[512], name[64];
	char *text;
	size_t len = 0;
	int found = 0;

	if (!conn || !*conn || !view_mode_state_path(path, sizeof(path)))
		return 0;
	text = kb_read_whole(path, &len);
	if (!text)
		return 0;

	for (char *p = text; *p && !found;) {
		char *nl = strchr(p, '\n');
		int mw = 0, mh = 0, mr = 0;

		if (nl)
			*nl = '\0';
		if (line_name(p, name, sizeof(name)) && !strcmp(name, conn)) {
			const char *sp = strchr(p, ' ');

			if (sp && sscanf(sp + 1, "%dx%d@%d", &mw, &mh, &mr) == 3
			    && mw > 0 && mh > 0) {
				if (w)
					*w = mw;
				if (h)
					*h = mh;
				if (refresh)
					*refresh = mr;
				found = 1;
			}
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(text);
	return found;
}

/*
 * WRITTEN WHOLE, never appended. The file is a handful of lines and a reader
 * that had to cope with the same connector named twice would be a reader that
 * silently prefers one of two answers.
 */
int view_mode_remember(const char *conn, int w, int h, int refresh)
{
	char path[512], name[64], line[128], *slash;
	char *text = NULL;
	size_t len = 0;
	KbBuf b = { 0 };
	int ok;

	if (!conn || !*conn || w <= 0 || h <= 0)
		return 0;
	if (!view_mode_state_path(path, sizeof(path)))
		return 0;

	snprintf(line, sizeof(line), "%s %dx%d@%d\n", conn, w, h, refresh);

	text = kb_read_whole(path, &len);
	if (text) {
		for (char *p = text; *p;) {
			char *nl = strchr(p, '\n');

			if (nl)
				*nl = '\0';
			/* Every OTHER screen's row, verbatim. */
			if (*p && line_name(p, name, sizeof(name)) &&
			    strcmp(name, conn)) {
				kb_buf_str(&b, p);
				kb_buf_str(&b, "\n");
			}
			if (!nl)
				break;
			p = nl + 1;
		}
		free(text);
	}
	kb_buf_str(&b, line);

	slash = strrchr(path, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(path);
		*slash = '/';
	}
	ok = kb_write_file_atomic(path, b.p ? b.p : line) == 0;
	kb_buf_free(&b);
	return ok;
}
