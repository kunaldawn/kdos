/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the faces this backend can wear, and switching between them
 *
 * MONOSPACE ONLY, AND THAT IS NOT A PREFERENCE. Everything libkwl draws is a
 * cell of a fixed width; a proportional face renders every column at a
 * different advance and the screen is a smear. So the list is
 * `FC_SPACING == FC_MONO` and nothing else — a family fontconfig does not call
 * monospaced is a family this library cannot use, and offering it in a picker
 * is offering a broken screen.
 *
 * ONE FAMILY IS ONE ROW. fontconfig lists a pattern per face, so a family with
 * a bold, an oblique and four weights arrives six times; the list is
 * deduplicated by family and sorted, because the choice a person makes here is
 * of a typeface and the style is fcft's to resolve.
 *
 * A NAME IS NOT A FAMILY. What fcft loads is fontconfig's name syntax —
 * `family-size:key=value` — so a family carrying `-`, `:` or `,` must be
 * escaped on the way in and unescaped on the way out, or a face called
 * `Iosevka Term SS08` is fine and one called `Some-Mono` silently asks for a
 * size.
 *
 * THE SIZE IS NOT THE PICKER'S. Switching face keeps whatever size the name in
 * force carries: a 32-pixel chrome that changed typeface and fell back to
 * fontconfig's default would resize every window on the desktop, and nothing on
 * screen would say the size had moved at all.
 *
 * AND KEEPING IT IS A FILE, not this process. Under the compositor each
 * surface is its own process with its own font, so a face set live here
 * reaches this window and no other; `chrome_font` and `panel_font` in
 * ~/.config/kdos/comp.conf are what the rest of them read, once, at startup.
 * ---------------------------------
 */

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fontconfig/fontconfig.h>

#include "kwl_priv.h"

/* A fontconfig family runs long; this is what one costs, escaped and with its
 * size behind it. */
#define KWL_FAM_MAX 192

/* ── the list ──────────────────────────────────────────────────────────── */

static char **fam_v;
static int fam_n;
/* The enumeration has run. A machine with no monospace face at all answers
 * zero for ever, and asking fontconfig again on every keystroke would walk
 * every font directory it knows to arrive at the same nothing. */
static int fam_done;

static int fam_cmp(const void *a, const void *b)
{
	return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static void fam_build(void)
{
	FcPattern *pat;
	FcObjectSet *os;
	FcFontSet *fs;
	int n = 0;

	if (fam_done)
		return;
	fam_done = 1;
	if (!FcInit())
		return;
	pat = FcPatternCreate();
	if (!pat)
		return;
	/* An element in the pattern handed to FcFontList is a FILTER, not a
	 * preference: this is what keeps a proportional face out. */
	FcPatternAddInteger(pat, FC_SPACING, FC_MONO);
	os = FcObjectSetBuild(FC_FAMILY, (char *)NULL);
	if (!os) {
		FcPatternDestroy(pat);
		return;
	}
	fs = FcFontList(NULL, pat, os);
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);
	if (!fs)
		return;
	if (fs->nfont > 0)
		fam_v = calloc((size_t)fs->nfont, sizeof(*fam_v));
	for (int i = 0; fam_v && i < fs->nfont; i++) {
		FcChar8 *s = NULL;

		if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &s) !=
		    FcResultMatch || !s || !*s)
			continue;
		fam_v[n] = strdup((const char *)s);
		if (!fam_v[n])
			break;
		n++;
	}
	FcFontSetDestroy(fs);
	if (n < 2) {
		fam_n = n;
		return;
	}
	qsort(fam_v, (size_t)n, sizeof(*fam_v), fam_cmp);
	/* Sorted, so a duplicate is the neighbour. */
	fam_n = 1;
	for (int i = 1; i < n; i++) {
		if (!strcasecmp(fam_v[i], fam_v[fam_n - 1])) {
			free(fam_v[i]);
			continue;
		}
		fam_v[fam_n++] = fam_v[i];
	}
}

/* ── fontconfig's name syntax ──────────────────────────────────────────── */

/*
 * The family `name` opens with, unescaped. The rest of a name is the size and
 * the options, and they start at the first `-` or `:` that is not itself
 * escaped.
 */
static void name_family(const char *name, char *out, size_t n)
{
	size_t o = 0;

	for (const char *p = name ? name : ""; *p; p++) {
		if (*p == '\\' && p[1])
			p++;
		else if (*p == ':' || *p == '-')
			break;
		if (o + 1 < n)
			out[o++] = *p;
	}
	while (o && (out[o - 1] == ' ' || out[o - 1] == '\t'))
		o--;
	if (n)
		out[o] = '\0';
}

/* Where the size and the options begin, so that a face change keeps them. */
static size_t name_tail(const char *name)
{
	size_t i = 0;

	for (; name && name[i]; i++) {
		if (name[i] == '\\' && name[i + 1]) {
			i++;
			continue;
		}
		if (name[i] == ':' || name[i] == '-')
			break;
	}
	return i;
}

/* 0 when it does not fit, and the caller then changes nothing: a name cut
 * short is a different family, not a shorter one. */
static int fam_escape(const char *fam, char *out, size_t n)
{
	size_t o = 0;

	for (const char *p = fam ? fam : ""; *p; p++) {
		if (*p == '\\' || *p == '-' || *p == ':' || *p == ',') {
			if (o + 1 >= n)
				return 0;
			out[o++] = '\\';
		}
		if (o + 1 >= n)
			return 0;
		out[o++] = *p;
	}
	if (o >= n || !o)
		return 0;
	out[o] = '\0';
	return 1;
}

/* `base` with its family replaced by `fam` and everything behind it kept. */
static int name_with_family(const char *base, const char *fam, char *out,
			    size_t n)
{
	char esc[KWL_FAM_MAX];

	if (!fam_escape(fam, esc, sizeof(esc)))
		return 0;
	return snprintf(out, n, "%s%s", esc,
			base ? base + name_tail(base) : "") < (int)n;
}

/* ── the vtable ────────────────────────────────────────────────────────── */

int kwl_font_count(void)
{
	fam_build();
	return fam_n;
}

int kwl_font_at(int i, char *out, int cap)
{
	fam_build();
	if (i < 0 || i >= fam_n || !out || cap <= 0)
		return 0;
	snprintf(out, (size_t)cap, "%s", fam_v[i]);
	return 1;
}

int kwl_font_current(void)
{
	char fam[KWL_FAM_MAX];

	fam_build();
	name_family(kwl_font_name(), fam, sizeof(fam));
	if (!*fam)
		return -1;
	for (int i = 0; i < fam_n; i++)
		if (!strcasecmp(fam_v[i], fam))
			return i;
	/*
	 * A NAME THAT IS NOT IN THE LIST IS NOT AN ERROR. `chrome_font` may
	 * ask for a face this machine does not have, or for an alias like
	 * `monospace` that fontconfig resolves and never lists, and the screen
	 * is then wearing something no row names. -1 says exactly that.
	 */
	return -1;
}

/* ── keeping it ────────────────────────────────────────────────────────── */

/*
 * The keys the rest of the desktop reads its face out of, and what each one
 * falls back to when comp.conf does not carry it. The two differ on purpose —
 * the bar is glanced at and the menus are read — so a face change grafts the
 * family onto each key's own size rather than writing one name into both.
 */
static const char *const CONF_KEY[] = { "chrome_font", "panel_font" };
static const char *const CONF_DEF[] = { "Terminus:pixelsize=32",
					"Terminus:pixelsize=20" };
#define CONF_N ((int)(sizeof(CONF_KEY) / sizeof(CONF_KEY[0])))

static void conf_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg) {
		snprintf(out, n, "%.400s/kdos/comp.conf", cfg);
		return;
	}
	if (!home || !*home) {
		struct passwd *pw = getpwuid(getuid());

		home = pw && pw->pw_dir ? pw->pw_dir : NULL;
	}
	if (home && *home)
		snprintf(out, n, "%.400s/.config/kdos/comp.conf", home);
	else if (n)
		*out = '\0';
}

/* Every component, because the first login to change a face may be one where
 * ~/.config does not exist yet and a single mkdir would fail on its parent. */
static void dir_make(const char *path)
{
	char buf[512];
	size_t n = 0;

	for (const char *p = path; *p; p++) {
		if (n + 1 >= sizeof(buf))
			return;
		buf[n++] = *p;
		if (*p == '/' && n > 1) {
			buf[n - 1] = '\0';
			mkdir(buf, 0755);
			buf[n - 1] = '/';
		}
	}
	buf[n] = '\0';
	mkdir(buf, 0755);
}

static char *file_read(const char *path, size_t *len)
{
	FILE *f = fopen(path, "r");
	char *p = NULL;
	size_t n = 0, cap = 0;

	*len = 0;
	if (!f)
		return NULL;
	for (;;) {
		if (n + 4096 + 1 > cap) {
			size_t want = cap ? cap * 2 : 8192;
			char *q = realloc(p, want);

			if (!q) {
				free(p);
				fclose(f);
				return NULL;
			}
			p = q;
			cap = want;
		}
		size_t got = fread(p + n, 1, 4096, f);

		n += got;
		if (got < 4096)
			break;
	}
	fclose(f);
	p[n] = '\0';
	*len = n;
	return p;
}

/* A growing byte buffer, so that the rewrite below is one pass over the old
 * file. */
typedef struct {
	char *p;
	size_t n, cap;
} ConfBuf;

static int buf_add(ConfBuf *b, const char *s, size_t n)
{
	if (b->n + n + 1 > b->cap) {
		size_t want = b->cap ? b->cap * 2 : 4096;
		char *q;

		while (want < b->n + n + 1)
			want *= 2;
		q = realloc(b->p, want);
		if (!q)
			return 0;
		b->p = q;
		b->cap = want;
	}
	memcpy(b->p + b->n, s, n);
	b->n += n;
	b->p[b->n] = '\0';
	return 1;
}

/*
 * Is this line the setting `key`? A COMMENTED LINE IS A COMMENT: comp.conf
 * documents every key as `#chrome_font = …`, and rewriting those would replace
 * the file's own account of its defaults with the one value this picker set.
 */
static int line_is_key(const char *line, const char *key)
{
	size_t kl = strlen(key);

	while (*line == ' ' || *line == '\t')
		line++;
	if (strncmp(line, key, kl))
		return 0;
	line += kl;
	while (*line == ' ' || *line == '\t')
		line++;
	return *line == '=';
}

/* The value a `key = value` line carries, trimmed. */
static void line_value(const char *line, size_t len, char *out, size_t n)
{
	const char *eq = memchr(line, '=', len);
	const char *end;
	size_t l;

	if (n)
		*out = '\0';
	if (!eq)
		return;
	eq++;
	end = line + len;
	while (eq < end && (*eq == ' ' || *eq == '\t'))
		eq++;
	while (end > eq && (end[-1] == '\n' || end[-1] == '\r' ||
			    end[-1] == ' ' || end[-1] == '\t'))
		end--;
	l = (size_t)(end - eq);
	if (l >= n)
		l = n ? n - 1 : 0;
	memcpy(out, eq, l);
	out[l] = '\0';
}

/*
 * THE FAMILY INTO comp.conf, EVERYTHING ELSE IN IT UNTOUCHED.
 *
 * The file is a person's: its comments, its blank lines and every key this
 * library knows nothing about are copied byte for byte, and only the two font
 * lines are rewritten. It is replaced by a rename over a synced temporary,
 * because a half-written comp.conf is a session that comes up wrong at the next
 * login with no way to say why.
 *
 * A FILE THAT EXISTS AND WILL NOT READ IS NOT AN EMPTY ONE. Writing a fresh
 * two-line file over a comp.conf that is merely unreadable to this process
 * would throw away everything in it.
 */
static void conf_keep(const char *fam)
{
	char path[512], tmp[540], dir[512];
	char *old;
	size_t oldn = 0;
	ConfBuf out = { 0 };
	int done[CONF_N] = { 0 };
	char *slash;
	FILE *f;
	int ok;

	conf_path(path, sizeof(path));
	if (!*path)
		return;
	old = file_read(path, &oldn);
	if (!old && access(path, F_OK) == 0)
		return;

	snprintf(dir, sizeof(dir), "%s", path);
	slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		dir_make(dir);
	}

	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		size_t len;
		int hit = -1;

		next = nl ? nl + 1 : line + strlen(line);
		len = (size_t)(next - line);
		/* EVERY occurrence, not the first: comp.conf is read top to
		 * bottom and the last setting of a key wins, so a file
		 * carrying the key twice would keep answering with the copy
		 * this pass left alone. */
		for (int i = 0; i < CONF_N && hit < 0; i++)
			if (line_is_key(line, CONF_KEY[i]))
				hit = i;
		if (hit < 0) {
			if (!buf_add(&out, line, len))
				goto fail;
			continue;
		}

		char val[KWL_FAM_MAX], want[KWL_FAM_MAX], row[KWL_FAM_MAX + 32];

		line_value(line, len, val, sizeof(val));
		if (!name_with_family(*val ? val : CONF_DEF[hit], fam, want,
				      sizeof(want)))
			goto fail;
		snprintf(row, sizeof(row), "%s = %s\n", CONF_KEY[hit], want);
		if (!buf_add(&out, row, strlen(row)))
			goto fail;
		done[hit] = 1;
	}

	for (int i = 0; i < CONF_N; i++) {
		char want[KWL_FAM_MAX], row[KWL_FAM_MAX + 32];

		if (done[i])
			continue;
		if (!name_with_family(CONF_DEF[i], fam, want, sizeof(want)))
			goto fail;
		if (out.n && out.p[out.n - 1] != '\n' && !buf_add(&out, "\n", 1))
			goto fail;
		snprintf(row, sizeof(row), "%s = %s\n", CONF_KEY[i], want);
		if (!buf_add(&out, row, strlen(row)))
			goto fail;
	}

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f)
		goto fail;
	ok = out.n == 0 || fwrite(out.p, 1, out.n, f) == out.n;
	if (ok)
		ok = fflush(f) == 0 && fsync(fileno(f)) == 0;
	/* fclose CLOSES whether it succeeds or not, so it is called exactly
	 * once on every path: a second one on the same stream is undefined. */
	if (fclose(f) != 0)
		ok = 0;
	if (ok && rename(tmp, path) == 0) {
		/* The directory entry is what the rename created, and it has
		 * to reach the disk too. */
		if (slash) {
			int d = open(dir, O_RDONLY);

			if (d >= 0) {
				fsync(d);
				close(d);
			}
		}
	} else {
		remove(tmp);
	}
fail:
	free(out.p);
	free(old);
}

void kwl_font_set(int index, int keep)
{
	char want[KWL_FAM_MAX];

	fam_build();
	/*
	 * A NEGATIVE INDEX IS THE FACE THIS SURFACE OPENED WITH, which is what
	 * a picker leaving without a choice asks for. It is never kept: the
	 * file already says what the next login wears.
	 */
	if (index < 0) {
		kwl_font_step(0);
		return;
	}
	if (index >= fam_n)
		return;
	if (!name_with_family(kwl_font_name(), fam_v[index], want,
			      sizeof(want)))
		return;
	/*
	 * NOTHING IS WRITTEN FOR A FACE THAT WILL NOT LOAD. fontconfig lists
	 * what it has scanned; a file that has since gone, or one fcft cannot
	 * open, fails here with the old face still on the screen, and
	 * persisting it would hand the same failure to the next login of every
	 * surface on the desktop.
	 */
	if (kwl_font_use(want) != 0)
		return;
	if (keep)
		conf_keep(fam_v[index]);
}
