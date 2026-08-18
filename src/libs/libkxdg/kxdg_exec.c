/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkxdg — splitting an Exec line
 *
 * See kxdg.h for why this is not a strtok. In short: the shipped appbox has
 * entries this gets right and a whitespace split gets wrong, and the failure
 * mode of every one of them is an application that appears not to start.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kxdg.h"

struct sink {
	char *store;
	size_t cap, used;
	const char **argv;
	int max, n;
	int arg_open;		/* a word is being built */
	int overflow;
};

static void sink_open(struct sink *s)
{
	if (s->arg_open || s->overflow)
		return;
	if (s->n >= s->max || s->used + 1 >= s->cap) {
		s->overflow = 1;
		return;
	}
	s->argv[s->n++] = s->store + s->used;
	s->arg_open = 1;
}

static void sink_putc(struct sink *s, char c)
{
	if (s->overflow)
		return;
	sink_open(s);
	if (s->overflow)
		return;
	if (s->used + 2 >= s->cap) {
		s->overflow = 1;
		return;
	}
	s->store[s->used++] = c;
}

static void sink_puts(struct sink *s, const char *p)
{
	for (; *p; p++)
		sink_putc(s, *p);
}

/* End the current word. An EMPTY word is kept when it was quoted — `""` is a
 * real, empty argument — and dropped when it was only whitespace. */
static void sink_close(struct sink *s)
{
	if (!s->arg_open || s->overflow)
		return;
	s->store[s->used++] = '\0';
	s->arg_open = 0;
}

/*
 * A field code, expanded. Returns how many bytes of `p` it consumed (0 when
 * `p` is not one).
 *
 * `%%` is a literal percent and is the only escape here. Everything else is a
 * placeholder: the four file codes expand to what was passed and the rest
 * expand to nothing, because there is no icon, no translated name and no
 * device for this program to substitute.
 */
static int field_code(struct sink *s, const char *p, const char *const *files,
		      int nfiles)
{
	if (p[0] != '%' || !p[1])
		return 0;
	/* Rewriting rather than running: every code survives untouched. */
	if (nfiles < 0) {
		sink_putc(s, p[0]);
		sink_putc(s, p[1]);
		return 2;
	}
	switch (p[1]) {
	case '%':
		sink_putc(s, '%');
		return 2;
	case 'f':
	case 'u':
		/* One file. A code that expands to nothing must not leave an
		 * empty argument behind — `mpv --` with a stray "" is `mpv`
		 * being told to open a file with no name. */
		if (nfiles > 0 && files[0] && files[0][0]) {
			sink_open(s);
			sink_puts(s, files[0]);
		}
		return 2;
	case 'F':
	case 'U':
		for (int i = 0; i < nfiles; i++) {
			if (!files[i] || !files[i][0])
				continue;
			if (i)
				sink_close(s);
			sink_open(s);
			sink_puts(s, files[i]);
		}
		return 2;
	case 'i':
	case 'c':
	case 'k':
	case 'd':
	case 'D':
	case 'n':
	case 'N':
	case 'v':
	case 'm':
		return 2;
	default:
		/* Not a code we know: leave the byte alone rather than eating
		 * an argument. A `%` in a path is legal. */
		return 0;
	}
}

int kxdg_exec_split(const char *exec, const char *const *files, int nfiles,
		    char *store, size_t cap, const char **argv, int max)
{
	struct sink s = { store, cap, 0, argv, max, 0, 0, 0 };
	const char *p = exec;

	if (!exec || !store || !argv || max < 1 || cap < 2)
		return 0;
	if (!files && nfiles > 0)
		nfiles = 0;

	while (*p && !s.overflow) {
		if (*p == ' ' || *p == '\t') {
			sink_close(&s);
			p++;
			continue;
		}
		if (*p == '"' || *p == '\'') {
			/*
			 * The spec quotes with `"` and escapes `"`, `` ` ``,
			 * `$` and `\` inside it with a backslash. `'` is not
			 * in the spec but appears in real entries and is
			 * treated as a dumb quote — the alternative is
			 * splitting a path that contains an apostrophe.
			 */
			char q = *p++;
			sink_open(&s);	/* "" is an argument */
			while (*p && *p != q) {
				if (q == '"' && *p == '\\' && p[1]) {
					p++;
					sink_putc(&s, *p++);
					continue;
				}
				sink_putc(&s, *p++);
			}
			if (*p == q)
				p++;
			continue;
		}
		if (*p == '\\' && p[1]) {
			p++;
			sink_putc(&s, *p++);
			continue;
		}
		int used = field_code(&s, p, files, nfiles);
		if (used) {
			p += used;
			continue;
		}
		sink_putc(&s, *p++);
	}
	sink_close(&s);
	if (s.overflow)
		return 0;
	return s.n;
}

int kxdg_exec_quote(const char *arg, char *out, size_t n)
{
	size_t used = 0;
	int need = 0;

	if (!arg || !out || n < 2)
		return -1;
	for (const char *p = arg; *p; p++)
		if (*p == ' ' || *p == '\t' || *p == '"' || *p == '\'' ||
		    *p == '\\' || *p == '`' || *p == '$' || !*arg)
			need = 1;
	if (!*arg)
		need = 1;
	if (!need) {
		size_t l = strlen(arg);
		if (l + 1 > n)
			return -1;
		memcpy(out, arg, l + 1);
		return 0;
	}
	if (used + 1 >= n)
		return -1;
	out[used++] = '"';
	for (const char *p = arg; *p; p++) {
		if (*p == '"' || *p == '\\' || *p == '`' || *p == '$') {
			if (used + 2 >= n)
				return -1;
			out[used++] = '\\';
		}
		if (used + 1 >= n)
			return -1;
		out[used++] = *p;
	}
	if (used + 2 > n)
		return -1;
	out[used++] = '"';
	out[used] = '\0';
	return 0;
}
