/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The root pair, and the only global state in this library.
 *
 * Every path anywhere in libkproc is built from these two. A reader that
 * open()s "/proc/stat" directly is a reader that cannot be tested, and the
 * fixture is the only way the selection rules and the arithmetic here get
 * exercised at all.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

static char g_proc[512] = "/proc";
static char g_sys[512] = "/sys";

void kpr_root_set(const char *proc, const char *sys)
{
	if (proc && *proc)
		kb_strlcpy(g_proc, proc, sizeof(g_proc));
	if (sys && *sys)
		kb_strlcpy(g_sys, sys, sizeof(g_sys));
}

const char *kpr_proc(void) { return g_proc; }
const char *kpr_sys(void)  { return g_sys; }

static char *slurp_under(const char *root, const char *fmt, va_list ap)
{
	char rel[512];
	vsnprintf(rel, sizeof(rel), fmt, ap);

	char path[1100];
	snprintf(path, sizeof(path), "%s/%s", root, rel);
	/*
	 * kb_read_all reads to real EOF. Every file under /proc reports
	 * st_size 0, so a reader that trusts stat gets an empty string and
	 * concludes the machine has nothing running.
	 */
	return kb_read_all(path, NULL);
}

char *kpr_slurp_proc(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *r = slurp_under(g_proc, fmt, ap);
	va_end(ap);
	return r;
}

char *kpr_slurp_sys(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *r = slurp_under(g_sys, fmt, ap);
	va_end(ap);
	return r;
}

unsigned long long kpr_uptime_s(void)
{
	char *d = kpr_slurp_proc("uptime");
	unsigned long long v = 0;

	if (!d)
		return 0;
	/* "2678.97 28518.85" — the first field, whole seconds. The fraction is
	 * dropped rather than rounded: this is compared against a tick count
	 * that has already been truncated by the same division. */
	v = strtoull(d, NULL, 10);
	free(d);
	return v;
}

long long kpr_num_sys(long long def, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *d = slurp_under(g_sys, fmt, ap);
	va_end(ap);
	if (!d)
		return def;
	char *end = NULL;
	long long v = strtoll(d, &end, 10);
	int ok = end && end != d;
	free(d);
	return ok ? v : def;
}
