/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * What is left of this file after the process, path and locking helpers moved
 * to libkbase — where they had been living as a second, subtly different copy
 * of the installer's.
 *
 * The no-shell rule moved with them and still holds: app names, package names
 * and file arguments all reach this program from .desktop files and the
 * command line, and a shell in the middle turns any of them into an injection
 * point.
 */

#include "kdos-appbox.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Stage timings, so "why did that take so long" stays answerable afterwards. */
void tracef(const char *fmt, ...)
{
	char *path = kb_path_join(kb_runtime_dir(), "kdos-appbox.trace");
	FILE *f = fopen(path, "a");
	struct timespec ts;
	va_list ap;

	free(path);
	if (!f)
		return;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(f, "%ld.%06ld ", (long)ts.tv_sec, ts.tv_nsec / 1000);
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

/*
 * Desktop notification straight over the bus — the host has no libnotify.
 * Only reachable at all because the session bus lives at a fixed path in
 * $XDG_RUNTIME_DIR, which the appbox shares.
 */
void notify(const char *summary, const char *body)
{
	KbArgv a = {0};
	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--timeout");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.freedesktop.Notifications");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/org/freedesktop/Notifications");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.freedesktop.Notifications.Notify");
	kb_argv_add(&a, "KDOS");
	kb_argv_add(&a, "0");
	kb_argv_add(&a, "distributor-logo-kdos");
	kb_argv_add(&a, summary);
	kb_argv_add(&a, body);
	kb_argv_add(&a, "[]");
	kb_argv_add(&a, "{}");
	kb_argv_add(&a, "8000");
	kb_argv_end(&a);
	kb_run_detach(&a);
}
