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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kproc.h"

static char g_proc[512] = "/proc";
static char g_sys[512] = "/sys";
static unsigned g_gen = 1;

void kpr_root_set(const char *proc, const char *sys)
{
	if (proc && *proc)
		kb_strlcpy(g_proc, proc, sizeof(g_proc));
	if (sys && *sys)
		kb_strlcpy(g_sys, sys, sizeof(g_sys));
	g_gen++;
}

const char *kpr_proc(void) { return g_proc; }
const char *kpr_sys(void)  { return g_sys; }
unsigned kpr_root_gen(void) { return g_gen; }

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

/*
 * The same read with no allocation, for the small files this library reads by
 * the thousand.
 *
 * A whole-file read through stat costs an fstat, a heap block and a second
 * read to confirm EOF for every /proc/<pid>/stat and every sysfs attribute —
 * three syscalls and an allocation to carry two bytes, paid inside a draw
 * loop. Nothing under /proc reports a usable st_size, so the loop reads until
 * the file ends or the buffer is full and there is no size to trust.
 *
 * Returns the byte count and NUL-terminates at that offset, or -1 when the
 * file cannot be opened. A return of cap - 1 means the buffer filled and the
 * content MAY be cut: a caller reading anything not bounded by its buffer —
 * /proc/cpuinfo, /proc/stat, a command line — must treat that as truncation
 * and fall back to kpr_slurp_*, because cutting a value silently is a wrong
 * reading rather than a missing one.
 */
static int read_under(const char *root, char *buf, size_t cap,
		      const char *fmt, va_list ap)
{
	if (cap == 0)
		return -1;

	char rel[512];
	vsnprintf(rel, sizeof(rel), fmt, ap);

	char path[1100];
	snprintf(path, sizeof(path), "%s/%s", root, rel);

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	size_t got = 0;
	while (got + 1 < cap) {
		ssize_t r = read(fd, buf + got, cap - 1 - got);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (r == 0)
			break;
		got += (size_t)r;
	}
	close(fd);
	buf[got] = '\0';
	return (int)got;
}

int kpr_read_into_proc(char *buf, size_t cap, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = read_under(g_proc, buf, cap, fmt, ap);
	va_end(ap);
	return r;
}

int kpr_read_into_sys(char *buf, size_t cap, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = read_under(g_sys, buf, cap, fmt, ap);
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
	/* A sysfs attribute holding a number is a handful of bytes; the stack
	 * buffer is what keeps a page that reads a hundred of them per frame
	 * off the allocator. */
	char buf[64];
	va_list ap;
	va_start(ap, fmt);
	int n = read_under(g_sys, buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0)
		return def;
	char *end = NULL;
	long long v = strtoll(buf, &end, 10);
	return end && end != buf ? v : def;
}
