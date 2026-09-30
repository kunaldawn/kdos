/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the clipboard
 *
 * `wl-copy`/`wl-paste` AND NOT A PROTOCOL OF OUR OWN. They are Wayland clients
 * and the desktop is a compositor, so the clipboard a boxed application sees
 * is the clipboard this reaches; a second road to it would be a second answer
 * to what is on it.
 *
 * NOTHING GOES IN argv IN EITHER DIRECTION. A clipboard is a password as often
 * as it is a URL and `/proc/<pid>/cmdline` is world-readable for the life of
 * the process, so the text travels on a pipe both ways.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kdos-tools.h"

/* See kdos-tools.h. */
int kdt_clip_put(const char *text)
{
	if (!text)
		return 0;

	if (!kb_have_prog("wl-copy"))
		return 0;

	KbArgv a = { 0 };

	kb_argv_add(&a, "wl-copy");
	kb_argv_add(&a, "--");
	kb_argv_add(&a, text);
	kb_argv_end(&a);
	return kb_run(&a) == 0;
}

/*
 * See kdos-tools.h. The buffer is bounded because every caller here wants a
 * clipboard small enough to be one: a person sharing a file shares the file.
 */
int kdt_clip_take(char *buf, size_t n)
{
	if (!buf || n < 2)
		return 0;
	buf[0] = '\0';

	if (!kb_have_prog("wl-paste"))
		return 0;

	KbArgv a = { 0 };

	kb_argv_add(&a, "wl-paste");
	/* wl-paste appends a newline to text unless told not to; one added
	 * here is one every caller would have to know to take off again. */
	kb_argv_add(&a, "--no-newline");
	kb_argv_end(&a);
	return kb_run_capture(&a, buf, n) == 0 && buf[0];
}
