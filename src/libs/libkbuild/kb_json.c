/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbuild — the JSON the build tree already carries
 *
 * Snapshot manifests and the restore marker are written by python's json
 * module, so they have to be READ the way python reads them: a document that
 * does not parse is not a partial document, it is no document. Every caller
 * here treats a parse failure as "the snapshot is absent", which is the safe
 * direction — the dangerous one is a half-read manifest that looks complete.
 *
 * This is a reader only. Nothing in KDOS generates JSON that is not either the
 * plan file (kb_plan.c writes those bytes directly, to match json.dump) or a
 * manifest written by build.py.
 * ---------------------------------
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "kbuild.h"

typedef struct {
	const char *p;
	int depth;
	int bad;
} Scan;

#define KJ_MAX_DEPTH 32

static KjNode *parse_value(Scan *s);

static void skip(Scan *s)
{
	while (*s->p && isspace((unsigned char)*s->p))
		s->p++;
}

static KjNode *node(KjType t)
{
	KjNode *n = kb_calloc(1, sizeof(*n));
	n->type = t;
	return n;
}

/* \uXXXX is emitted by json.dump for anything non-ASCII, so it has to become
 * real UTF-8 or a phase title with an em dash in it comes back mangled. */
static void put_utf8(KbBuf *b, unsigned cp)
{
	char u[4];
	if (cp < 0x80) {
		u[0] = (char)cp;
		kb_buf_add(b, u, 1);
	} else if (cp < 0x800) {
		u[0] = (char)(0xC0 | (cp >> 6));
		u[1] = (char)(0x80 | (cp & 0x3F));
		kb_buf_add(b, u, 2);
	} else if (cp < 0x10000) {
		u[0] = (char)(0xE0 | (cp >> 12));
		u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		u[2] = (char)(0x80 | (cp & 0x3F));
		kb_buf_add(b, u, 3);
	} else {
		u[0] = (char)(0xF0 | (cp >> 18));
		u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
		u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
		u[3] = (char)(0x80 | (cp & 0x3F));
		kb_buf_add(b, u, 4);
	}
}

static unsigned hex4(const char *p, int *ok)
{
	unsigned v = 0;
	for (int i = 0; i < 4; i++) {
		char c = p[i];
		v <<= 4;
		if (c >= '0' && c <= '9')
			v |= (unsigned)(c - '0');
		else if (c >= 'a' && c <= 'f')
			v |= (unsigned)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v |= (unsigned)(c - 'A' + 10);
		else {
			*ok = 0;
			return 0;
		}
	}
	return v;
}

static char *parse_string(Scan *s)
{
	if (*s->p != '"') {
		s->bad = 1;
		return NULL;
	}
	s->p++;

	KbBuf b = {0};
	while (*s->p && *s->p != '"') {
		if (*s->p != '\\') {
			kb_buf_add(&b, s->p, 1);
			s->p++;
			continue;
		}
		s->p++;
		char c = *s->p++;
		switch (c) {
		case 'n': kb_buf_add(&b, "\n", 1); break;
		case 't': kb_buf_add(&b, "\t", 1); break;
		case 'r': kb_buf_add(&b, "\r", 1); break;
		case 'b': kb_buf_add(&b, "\b", 1); break;
		case 'f': kb_buf_add(&b, "\f", 1); break;
		case '"': case '\\': case '/': kb_buf_add(&b, &c, 1); break;
		case 'u': {
			int ok = 1;
			unsigned cp = hex4(s->p, &ok);
			if (!ok) {
				s->bad = 1;
				kb_buf_free(&b);
				return NULL;
			}
			s->p += 4;
			/* A surrogate pair is two escapes; join them. */
			if (cp >= 0xD800 && cp <= 0xDBFF && s->p[0] == '\\' &&
			    s->p[1] == 'u') {
				unsigned lo = hex4(s->p + 2, &ok);
				if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
					cp = 0x10000 + ((cp - 0xD800) << 10) +
					     (lo - 0xDC00);
					s->p += 6;
				}
			}
			put_utf8(&b, cp);
			break;
		}
		default:
			s->bad = 1;
			kb_buf_free(&b);
			return NULL;
		}
	}
	if (*s->p != '"') {
		s->bad = 1;
		kb_buf_free(&b);
		return NULL;
	}
	s->p++;

	char *out = b.p ? kb_strdup(b.p) : kb_strdup("");
	kb_buf_free(&b);
	return out;
}

static KjNode *parse_object(Scan *s)
{
	s->p++;			/* { */
	KjNode *obj = node(KJ_OBJ), *tail = NULL;
	skip(s);
	if (*s->p == '}') {
		s->p++;
		return obj;
	}
	for (;;) {
		skip(s);
		char *key = parse_string(s);
		if (s->bad)
			break;
		skip(s);
		if (*s->p != ':') {
			s->bad = 1;
			free(key);
			break;
		}
		s->p++;
		KjNode *val = parse_value(s);
		if (!val || s->bad) {
			free(key);
			kj_free(val);
			s->bad = 1;
			break;
		}
		val->key = key;
		if (tail)
			tail->next = val;
		else
			obj->child = val;
		tail = val;

		skip(s);
		if (*s->p == ',') {
			s->p++;
			continue;
		}
		if (*s->p == '}') {
			s->p++;
			return obj;
		}
		s->bad = 1;
		break;
	}
	kj_free(obj);
	return NULL;
}

static KjNode *parse_array(Scan *s)
{
	s->p++;			/* [ */
	KjNode *arr = node(KJ_ARR), *tail = NULL;
	skip(s);
	if (*s->p == ']') {
		s->p++;
		return arr;
	}
	for (;;) {
		KjNode *val = parse_value(s);
		if (!val || s->bad) {
			kj_free(val);
			s->bad = 1;
			break;
		}
		if (tail)
			tail->next = val;
		else
			arr->child = val;
		tail = val;

		skip(s);
		if (*s->p == ',') {
			s->p++;
			continue;
		}
		if (*s->p == ']') {
			s->p++;
			return arr;
		}
		s->bad = 1;
		break;
	}
	kj_free(arr);
	return NULL;
}

static KjNode *parse_value(Scan *s)
{
	skip(s);
	if (s->depth++ > KJ_MAX_DEPTH) {
		s->bad = 1;
		return NULL;
	}

	KjNode *out = NULL;
	if (*s->p == '{') {
		out = parse_object(s);
	} else if (*s->p == '[') {
		out = parse_array(s);
	} else if (*s->p == '"') {
		char *str = parse_string(s);
		if (str) {
			out = node(KJ_STR);
			out->str = str;
		}
	} else if (!strncmp(s->p, "true", 4)) {
		s->p += 4;
		out = node(KJ_BOOL);
		out->bval = 1;
	} else if (!strncmp(s->p, "false", 5)) {
		s->p += 5;
		out = node(KJ_BOOL);
	} else if (!strncmp(s->p, "null", 4)) {
		s->p += 4;
		out = node(KJ_NULL);
	} else {
		char *end = NULL;
		double v = strtod(s->p, &end);
		if (end && end != s->p) {
			s->p = end;
			out = node(KJ_NUM);
			out->num = v;
		} else {
			s->bad = 1;
		}
	}
	s->depth--;
	return out;
}

KjNode *kj_parse(const char *text)
{
	if (!text)
		return NULL;
	Scan s = { text, 0, 0 };
	KjNode *root = parse_value(&s);
	if (s.bad) {
		kj_free(root);
		return NULL;
	}
	skip(&s);
	if (*s.p) {		/* trailing junk is a parse error, as in python */
		kj_free(root);
		return NULL;
	}
	return root;
}

void kj_free(KjNode *n)
{
	while (n) {
		KjNode *next = n->next;
		kj_free(n->child);
		free(n->key);
		free(n->str);
		free(n);
		n = next;
	}
}

const KjNode *kj_get(const KjNode *obj, const char *key)
{
	if (!obj || obj->type != KJ_OBJ)
		return NULL;
	for (const KjNode *c = obj->child; c; c = c->next)
		if (c->key && !strcmp(c->key, key))
			return c;
	return NULL;
}

const char *kj_str(const KjNode *obj, const char *key, const char *def)
{
	const KjNode *n = kj_get(obj, key);
	return (n && n->type == KJ_STR) ? n->str : def;
}

double kj_num(const KjNode *obj, const char *key, double def)
{
	const KjNode *n = kj_get(obj, key);
	return (n && n->type == KJ_NUM) ? n->num : def;
}

int kj_bool(const KjNode *obj, const char *key, int def)
{
	const KjNode *n = kj_get(obj, key);
	if (!n)
		return def;
	if (n->type == KJ_BOOL)
		return n->bval;
	if (n->type == KJ_NUM)		/* python's bool(0) / bool(1) */
		return n->num != 0;
	if (n->type == KJ_STR)
		return n->str[0] != 0;
	return def;
}

int kj_len(const KjNode *n)
{
	int k = 0;
	for (const KjNode *c = n ? n->child : NULL; c; c = c->next)
		k++;
	return k;
}
