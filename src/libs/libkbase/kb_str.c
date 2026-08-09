/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — strings
 * ---------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "kbase.h"

void kb_strlcpy(char *d, const char *s, size_t n)
{
	if (!n)
		return;
	size_t i = 0;
	for (; i + 1 < n && s[i]; i++)
		d[i] = s[i];
	d[i] = 0;
}

int kb_str_ieq(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

const char *kb_basename(const char *p)
{
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}

const char *kb_human_size(unsigned long long bytes)
{
	static char buf[8][32];
	static int slot;
	char *b = buf[slot = (slot + 1) & 7];
	const char *u[] = { "B", "K", "M", "G", "T", "P" };
	double v = (double)bytes;
	int i = 0;
	while (v >= 1024.0 && i < 5) {
		v /= 1024.0;
		i++;
	}
	if (i == 0)
		snprintf(b, 32, "%llu%s", bytes, u[0]);
	else if (v < 10.0)
		snprintf(b, 32, "%.1f%s", v, u[i]);
	else
		snprintf(b, 32, "%.0f%s", v, u[i]);
	return b;
}
