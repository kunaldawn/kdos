/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-tools — the clipboard, on whichever desktop this is
 *
 * TWO DESKTOPS, TWO ANSWERS, AND NEITHER IS AVAILABLE ON THE OTHER. The
 * compositor's clipboard is `wl-copy`/`wl-paste`, which are Wayland clients;
 * the console has no Wayland at all and its clipboard is the session's, reached
 * through `kdos-con`. The console is asked FIRST, because a console session
 * running inside a graphical one has both and the near one is right.
 *
 * NOT OVER libkcon. This binary is on every image and linking the session
 * protocol would drag the cell model in behind it, so the console half goes
 * through `kdos-con` as a program — the same reason `kdos con <verb>` execs it.
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
	const char *con = getenv("KDOS_CON");

	if (!text)
		return 0;

	if (con && *con) {
		KbArgv c = { 0 };

		kb_argv_add(&c, "kdos-con");
		kb_argv_add(&c, "--clip-text");
		kb_argv_end(&c);
		if (kb_run_feed(&c, text, strlen(text)) == 0)
			return 1;
	}

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
	const char *con = getenv("KDOS_CON");

	if (!buf || n < 2)
		return 0;
	buf[0] = '\0';

	if (con && *con) {
		KbArgv c = { 0 };

		kb_argv_add(&c, "kdos-con");
		kb_argv_add(&c, "--clip-take");
		kb_argv_end(&c);
		if (kb_run_capture(&c, buf, n) == 0 && buf[0])
			return 1;
		buf[0] = '\0';
	}

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
