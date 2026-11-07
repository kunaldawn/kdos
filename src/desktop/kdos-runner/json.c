/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   json.c — enough JSON to read what kiwix-serve and Kolibri answer
 *
 * No tree is built. A value is a pointer to its first character inside the
 * NUL-terminated document, and every walk stops at the NUL: a truncated or
 * hostile answer ends a lookup with NULL, never a read past the buffer.
 * Nesting is bounded, so an answer of ten thousand `[` cannot overflow the
 * stack.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "runner.h"

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
				p = json_ws(p);
				if (!p || *p != ':')
					return NULL;
				p++;
			}
			p = json_ws(skip_at(p, depth + 1));
			if (!p)
				return NULL;
			if (*p == ',') {
				p++;
				continue;
			}
			return *p == close ? p + 1 : NULL;
		}
	}
	/* number, true, false, null: up to the next delimiter */
	while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' &&
	       *p != '\n' && *p != '\t' && *p != '\r')
		p++;
	return p;
}

const char *json_skip(const char *p)
{
	return skip_at(p, 0);
}

const char *json_get(const char *obj, const char *key)
{
	size_t kl = strlen(key);

	obj = json_ws(obj);
	if (!obj || *obj != '{')
		return NULL;
	obj = json_ws(obj + 1);
	while (obj && *obj == '"') {
		const char *k = obj + 1, *end = skip_string(obj);
		const char *v;

		if (!end)
			return NULL;
		v = json_ws(end);
		if (*v != ':')
			return NULL;
		v = json_ws(v + 1);
		if ((size_t)(end - 1 - k) == kl && !strncmp(k, key, kl))
			return v;
		obj = json_ws(json_skip(v));
		if (!obj || *obj != ',')
			return NULL;
		obj = json_ws(obj + 1);
	}
	return NULL;
}

const char *json_first(const char *p)
{
	p = json_ws(p);
	if (!p || *p != '[')
		return NULL;
	p = json_ws(p + 1);
	return *p && *p != ']' ? p : NULL;
}

const char *json_next(const char *p)
{
	p = json_ws(json_skip(p));
	if (!p || *p != ',')
		return NULL;
	p = json_ws(p + 1);
	return *p && *p != ']' ? p : NULL;
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

static size_t put_utf8(char *o, unsigned cp)
{
	if (cp < 0x80) {
		o[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		o[0] = (char)(0xc0 | (cp >> 6));
		o[1] = (char)(0x80 | (cp & 0x3f));
		return 2;
	}
	if (cp < 0x10000) {
		o[0] = (char)(0xe0 | (cp >> 12));
		o[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		o[2] = (char)(0x80 | (cp & 0x3f));
		return 3;
	}
	o[0] = (char)(0xf0 | (cp >> 18));
	o[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
	o[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
	o[3] = (char)(0x80 | (cp & 0x3f));
	return 4;
}

static int hex4(const char *p, unsigned *out)
{
	unsigned v = 0;

	for (int i = 0; i < 4; i++) {
		int h = hexval(p[i]);

		if (h < 0)
			return 0;
		v = v * 16 + (unsigned)h;
	}
	*out = v;
	return 1;
}

int json_str(const char *p, char *out, size_t cap)
{
	size_t o = 0;

	p = json_ws(p);
	if (!cap)
		return 0;
	out[0] = '\0';
	if (!p || *p != '"')
		return 0;
	for (p++; *p && *p != '"'; p++) {
		char tmp[4];
		size_t n = 1;

		if (*p != '\\') {
			tmp[0] = *p;
		} else {
			p++;
			switch (*p) {
			case 'n': tmp[0] = '\n'; break;
			case 't': tmp[0] = '\t'; break;
			case 'r': tmp[0] = '\r'; break;
			case 'b': tmp[0] = '\b'; break;
			case 'f': tmp[0] = '\f'; break;
			case 'u': {
				unsigned cp, lo;

				if (!hex4(p + 1, &cp))
					return 0;
				p += 4;
				if (cp >= 0xd800 && cp < 0xdc00 && p[1] == '\\' &&
				    p[2] == 'u' && hex4(p + 3, &lo) &&
				    lo >= 0xdc00 && lo < 0xe000) {
					cp = 0x10000 + ((cp - 0xd800) << 10) +
					     (lo - 0xdc00);
					p += 6;
				} else if (cp >= 0xd800 && cp < 0xe000) {
					cp = 0xfffd;
				}
				/* A NUL inside a string would cut it short
				 * where the caller cannot see it. */
				if (!cp)
					cp = 0xfffd;
				n = put_utf8(tmp, cp);
				break;
			}
			case '\0':
				return 0;
			default: tmp[0] = *p; break;
			}
		}
		if (o + n < cap) {
			memcpy(out + o, tmp, n);
			o += n;
		}
	}
	out[o] = '\0';
	return *p == '"';
}

long long json_int(const char *p, long long def)
{
	char *end;
	long long v;

	p = json_ws(p);
	if (!p || !*p)
		return def;
	v = strtoll(p, &end, 10);
	return end == p ? def : v;
}

/* ── HTML text ─────────────────────────────────────────────────────────── */

static const struct {
	const char *name;
	unsigned cp;
} entities[] = {
	{ "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
	{ "apos", '\'' }, { "nbsp", 0xa0 },
};

void rn_html_text(char *s)
{
	char *o = s;

	for (char *p = s; *p;) {
		if (*p == '<') {
			char *e = strchr(p, '>');

			if (e) {
				p = e + 1;
				continue;
			}
		}
		if (*p == '&') {
			char *e = strchr(p, ';');
			unsigned cp = 0;

			if (e && e - p < 12) {
				if (p[1] == '#') {
					char *end;

					cp = (unsigned)strtoul(p + 2 + (p[2] == 'x' || p[2] == 'X'),
							       &end, p[2] == 'x' || p[2] == 'X' ? 16 : 10);
					if (end != e)
						cp = 0;
				} else {
					for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); i++)
						if ((size_t)(e - p - 1) == strlen(entities[i].name) &&
						    !strncmp(p + 1, entities[i].name, (size_t)(e - p - 1)))
							cp = entities[i].cp;
				}
				if (cp && cp < 0x110000 && !(cp >= 0xd800 && cp < 0xe000)) {
					char tmp[4];
					size_t n = put_utf8(tmp, cp);

					/* An entity's UTF-8 is never longer
					 * than the entity, so this stays in
					 * place. */
					if ((size_t)(e - p + 1) >= n) {
						memcpy(o, tmp, n);
						o += n;
						p = e + 1;
						continue;
					}
				}
			}
		}
		*o++ = *p++;
	}
	*o = '\0';
}
