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

/*
 * ── base64 ───────────────────────────────────────────────────────────────
 *
 * Here rather than in a state machine or a terminal: OSC 52 carries a base64
 * selection and the clipboard is not the only thing that will ever want this.
 *
 * DECODE ONLY. The encode side has one caller — `ktui_clip_copy` writes the
 * sequence as it goes, without a buffer — and a second implementation of the
 * same table would be a second thing to keep in step.
 */

static int b64_val(unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

/*
 * Decodes `in` into `out`, writing at most `outsz` bytes and NUL-terminating.
 * Returns the number of bytes written, or -1 when the input is not base64 or
 * would not fit.
 *
 * REFUSED WHOLE, NEVER PARTIAL. A half-decoded selection is a paste of
 * garbage; a refusal is a paste that did not happen, which is visible.
 * Whitespace is skipped, because a long payload may arrive wrapped.
 */
int kb_b64_decode(const char *in, size_t inlen, char *out, size_t outsz,
		  size_t *outlen)
{
	unsigned int acc = 0;
	int bits = 0;
	size_t n = 0;

	if (!in || !out || !outsz)
		return -1;

	for (size_t i = 0; i < inlen; i++) {
		unsigned char c = (unsigned char)in[i];
		int v;

		if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;
		if (c == '=')
			break;		/* padding: nothing follows it */
		v = b64_val(c);
		if (v < 0)
			return -1;
		acc = (acc << 6) | (unsigned int)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (n + 1 >= outsz)
				return -1;
			out[n++] = (char)((acc >> bits) & 0xff);
		}
	}

	out[n] = '\0';
	if (outlen)
		*outlen = n;
	return (int)n;
}

/*
 * A path as a `file://` URI, escaped the way every other program on the
 * machine escapes it.
 *
 * THE ESCAPE SET IS NOT A CHOICE. The thumbnail cache is named by the MD5 of
 * this string, and the cache is SHARED — a file manager, an image viewer and
 * this desktop all write into it. Escape one character differently and every
 * thumbnail misses: the entry is there, under a name nothing else computes.
 * The set is glib's `G_URI_RESERVED_CHARS_ALLOWED_IN_PATH` plus the unreserved
 * characters, which is what `g_filename_to_uri()` produces.
 *
 * Uppercase hex, for the same reason: `%2F` and `%2f` are the same URI and
 * different strings, and the hash is over the string.
 */
void kb_uri_file(const char *path, char *out, size_t n)
{
	static const char hex[] = "0123456789ABCDEF";
	static const char keep[] = "-_.~!$&'()*+,;=:@/";
	size_t o = 0;

	if (!out || n < 8)
		return;
	o += (size_t)snprintf(out, n, "file://");
	for (const unsigned char *p = (const unsigned char *)path;
	     *p && o + 4 < n; p++) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
		    (*p >= '0' && *p <= '9') || strchr(keep, *p)) {
			out[o++] = (char)*p;
			continue;
		}
		out[o++] = '%';
		out[o++] = hex[*p >> 4];
		out[o++] = hex[*p & 0x0f];
	}
	out[o] = '\0';
}
