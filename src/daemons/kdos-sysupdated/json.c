/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   json.c — enough JSON to read `kdos update check --json` and
 *            `kdos-bootctl status --json`
 *
 * No tree is built. A value is a pointer to its first character inside the
 * NUL-terminated document, and every walk stops at the NUL: a truncated or
 * hostile document ends a lookup with NULL, never a read past the buffer.
 * Nesting is bounded, so a document of ten thousand `[` cannot overflow the
 * stack.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "sysupdated.h"

#define JSON_DEPTH 32

const char *json_ws(const char *p)
{
	while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

static const char *skip_string(const char *p)
{
	if (*p != '"')
		return NULL;
	for (p++; *p; p++) {
		if (*p == '\\') {
			if (!p[1])
				return NULL;
			p++;
		} else if (*p == '"') {
			return p + 1;
		}
	}
	return NULL;
}

static const char *skip_at(const char *p, int depth)
{
	p = json_ws(p);
	if (!p || !*p || depth > JSON_DEPTH)
		return NULL;
	if (*p == '"')
		return skip_string(p);
	if (*p == '{' || *p == '[') {
		char close = *p == '{' ? '}' : ']';
		p = json_ws(p + 1);
		if (*p == close)
			return p + 1;
		for (;;) {
			if (close == '}') {
				p = skip_string(json_ws(p));
				if (!p)
					return NULL;
				p = json_ws(p);
				if (*p != ':')
					return NULL;
				p++;
			}
			p = skip_at(p, depth + 1);
			if (!p)
				return NULL;
			p = json_ws(p);
			if (*p == ',') {
				p++;
				continue;
			}
			return *p == close ? p + 1 : NULL;
		}
	}
	if (!strncmp(p, "true", 4))
		return p + 4;
	if (!strncmp(p, "false", 5))
		return p + 5;
	if (!strncmp(p, "null", 4))
		return p + 4;
	if (*p == '-' || (*p >= '0' && *p <= '9')) {
		p++;
		while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' ||
		       *p == 'E' || *p == '+' || *p == '-')
			p++;
		return p;
	}
	return NULL;
}

const char *json_skip(const char *p)
{
	return skip_at(p, 0);
}

/* The value of `key` in the object at `obj`, or NULL. Only the object's own
 * keys are compared: a key of the same name inside a nested object is
 * skipped over with the value that holds it. */
const char *json_get(const char *obj, const char *key)
{
	size_t kl = strlen(key);
	const char *p = json_ws(obj);

	if (!p || *p != '{')
		return NULL;
	p = json_ws(p + 1);
	if (*p == '}')
		return NULL;
	for (;;) {
		p = json_ws(p);
		if (*p != '"')
			return NULL;
		const char *k = p + 1;
		const char *ke = skip_string(p);
		if (!ke)
			return NULL;
		int match = (size_t)(ke - 1 - k) == kl && !strncmp(k, key, kl);
		p = json_ws(ke);
		if (*p != ':')
			return NULL;
		p = json_ws(p + 1);
		if (match)
			return p;
		p = json_skip(p);
		if (!p)
			return NULL;
		p = json_ws(p);
		if (*p != ',')
			return NULL;
		p++;
	}
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex4(const char *p, unsigned *out)
{
	unsigned v = 0;
	for (int i = 0; i < 4; i++) {
		int h = hexval(p[i]);
		if (h < 0)
			return -1;
		v = v << 4 | (unsigned)h;
	}
	*out = v;
	return 0;
}

static size_t put_utf8(char *o, size_t room, unsigned cp)
{
	char b[4];
	size_t n;

	if (cp < 0x80) {
		b[0] = (char)cp;
		n = 1;
	} else if (cp < 0x800) {
		b[0] = (char)(0xc0 | cp >> 6);
		b[1] = (char)(0x80 | (cp & 0x3f));
		n = 2;
	} else if (cp < 0x10000) {
		b[0] = (char)(0xe0 | cp >> 12);
		b[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		b[2] = (char)(0x80 | (cp & 0x3f));
		n = 3;
	} else {
		b[0] = (char)(0xf0 | cp >> 18);
		b[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
		b[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
		b[3] = (char)(0x80 | (cp & 0x3f));
		n = 4;
	}
	if (n > room)
		return 0;
	memcpy(o, b, n);
	return n;
}

/*
 * The string at `v`, unescaped into `out`. 0 for a string, 1 for null (out
 * is ""), -1 for anything else. A string longer than `out` is cut at a
 * character boundary, never inside an escape. A NUL escape (\u0000) ends the
 * string there, because every caller treats the result as C text.
 */
int json_string(const char *v, char *out, size_t cap)
{
	size_t o = 0;

	out[0] = 0;
	v = json_ws(v);
	if (!v)
		return -1;
	if (!strncmp(v, "null", 4))
		return 1;
	if (*v != '"')
		return -1;
	for (v++; *v && *v != '"'; v++) {
		if (o + 1 >= cap)
			break;
		if (*v != '\\') {
			out[o++] = *v;
			continue;
		}
		v++;
		switch (*v) {
		case 'n': out[o++] = '\n'; break;
		case 't': out[o++] = '\t'; break;
		case 'r': out[o++] = '\r'; break;
		case 'b': out[o++] = '\b'; break;
		case 'f': out[o++] = '\f'; break;
		case 'u': {
			unsigned cp, lo;
			if (hex4(v + 1, &cp) != 0)
				goto done;
			v += 4;
			if (cp >= 0xd800 && cp < 0xdc00 && v[1] == '\\' &&
			    v[2] == 'u' && hex4(v + 3, &lo) == 0 &&
			    lo >= 0xdc00 && lo < 0xe000) {
				cp = 0x10000 + ((cp - 0xd800) << 10) +
				     (lo - 0xdc00);
				v += 6;
			} else if (cp >= 0xd800 && cp < 0xe000) {
				cp = 0xfffd;
			}
			if (!cp)
				goto done;
			size_t w = put_utf8(out + o, cap - 1 - o, cp);
			if (!w)
				goto done;
			o += w;
			break;
		}
		case 0:
			goto done;
		default:
			out[o++] = *v;	/* \" \\ \/ */
			break;
		}
	}
done:
	out[o] = 0;
	return 0;
}

int json_is_null(const char *v)
{
	v = json_ws(v);
	return !v || !strncmp(v, "null", 4);
}

long long json_number(const char *v)
{
	v = json_ws(v);
	if (!v || !(*v == '-' || (*v >= '0' && *v <= '9')))
		return 0;
	return strtoll(v, NULL, 10);
}

int json_true(const char *v)
{
	v = json_ws(v);
	return v && !strncmp(v, "true", 4);
}

/* The first element of the array at `container`; NULL when it is empty or
 * not an array. */
const char *json_first(const char *container)
{
	const char *p = json_ws(container);
	if (!p || *p != '[')
		return NULL;
	p = json_ws(p + 1);
	return *p == ']' ? NULL : p;
}

/* The element after `value` in an array; NULL at the end. */
const char *json_next(const char *value)
{
	const char *p = json_skip(value);
	if (!p)
		return NULL;
	p = json_ws(p);
	if (*p != ',')
		return NULL;
	return json_ws(p + 1);
}
