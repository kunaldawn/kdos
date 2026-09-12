/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — allocation
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"

static kb_oom_fn oom_hook;
static const char *prog = "kdos";

void kb_set_oom_handler(kb_oom_fn fn)
{
	oom_hook = fn;
}

void kb_set_progname(const char *name)
{
	if (name)
		prog = name;
}

const char *kb_progname(void)
{
	return prog;
}

/*
 * The format is GUARDED in both of these. vfprintf with a null format is
 * undefined, and the sanitiser build's interprocedural pass cannot prove one
 * non-null across a whole program compiled in a single line — so without the
 * guard `-Wformat-overflow` refuses to build the self-test at all.
 */
void kb_die(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", prog);
	va_start(ap, fmt);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void kb_warn(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", prog);
	va_start(ap, fmt);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void *kb_calloc(size_t n, size_t sz)
{
	void *p = calloc(n, sz);
	if (!p) {
		if (oom_hook)
			oom_hook();
		fprintf(stderr, "%s: out of memory\n", prog);
		_exit(1);
	}
	return p;
}

char *kb_strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = kb_calloc(1, n);
	memcpy(p, s, n);
	return p;
}
