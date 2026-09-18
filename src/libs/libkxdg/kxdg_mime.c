/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkxdg — what type is this file, and what is that type's icon
 *
 * THE LONGEST MATCHING SUFFIX WINS. Get it backwards and every `.tar.gz`
 * resolves to the decompressor rather than to the archiver — the rule
 * `kdos-appbox open` was written around, and the reason this is a library
 * function now rather than a third private copy of it. `openwith.c` had one,
 * `open.c` in kdos-appbox has one it cannot share (a different binary), and
 * libkicon needed a third to put a picture next to a file name. Two copies in
 * one binary is one too many.
 *
 * `/usr/share/mime/globs` is COMPILED ON THE TARGET by update-mime-database, in
 * shared-mime-info's postinstall — the port builds --disable-update-mimedb and
 * ships only the source XML, so on a machine where that hook has not run this
 * file resolves nothing at all and every caller falls back. That is a real
 * state and not a hypothetical: it is what a booted ISO looked like before the
 * hook existed.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kxdg.h"

static const char *base_of(const char *path)
{
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

/*
 * THE TABLE IS PARSED ONCE PER VERSION OF THE FILE, not once per call and not
 * once per process.
 *
 * It is a thousand lines and it is asked per FILE: a directory listing with
 * icons asks once per row per redraw, so a read and a full parse per call is
 * a megabyte of reading and a thousand string comparisons to decide the icon
 * for one name. What is checked per call instead is the file's size and
 * mtime, because update-mime-database rewrites it from a package postinstall
 * hook while the desktop is up — latch the parse on a bare flag and a type
 * installed under a running surface never gets an icon or a handler until the
 * next login. A file that cannot be stat()ed leaves whatever is loaded in
 * place and latches nothing, so the table appears as soon as the hook has
 * created it.
 *
 * Both tables point INTO the retained buffer, which is why the newlines and
 * the colons are overwritten in place and the buffer is freed only when it is
 * replaced wholesale. Nothing escapes a lookup — the type is copied into the
 * caller's buffer — so replacing them cannot strand a pointer.
 */
struct glob_entry {
	const char *pattern;	/* the suffix, or the whole literal name */
	size_t len;
	const char *type;
};

static char *glob_data;
static struct glob_entry *glob_suffix, *glob_exact;
static int glob_nsuffix, glob_nexact;
static struct timespec glob_mtim;
static off_t glob_size;
static int glob_have;

static void glob_load(void)
{
	struct stat st;
	size_t len = 0;
	size_t lines = 1;

	if (stat(KXDG_MIME_GLOBS, &st) != 0)
		return;
	if (glob_have && st.st_size == glob_size &&
	    st.st_mtim.tv_sec == glob_mtim.tv_sec &&
	    st.st_mtim.tv_nsec == glob_mtim.tv_nsec)
		return;

	free(glob_data);
	free(glob_suffix);
	free(glob_exact);
	glob_data = NULL;
	glob_suffix = NULL;
	glob_exact = NULL;
	glob_nsuffix = 0;
	glob_nexact = 0;
	glob_have = 0;

	glob_data = kb_read_all(KXDG_MIME_GLOBS, &len);
	if (!glob_data)
		return;

	/*
	 * ONE DESTRUCTIVE PASS, so the arrays are sized from the LINE COUNT
	 * rather than from a first parse: the parse terminates each line in
	 * place, and a second pass over the same buffer would find no line
	 * endings left to split on.
	 */
	for (size_t i = 0; i < len; i++)
		if (glob_data[i] == '\n')
			lines++;

	glob_suffix = kb_calloc(lines + 1, sizeof(*glob_suffix));
	glob_exact = kb_calloc(lines + 1, sizeof(*glob_exact));

	for (char *p = glob_data; *p;) {
		char *nl = strchr(p, '\n');

		if (nl)
			*nl = '\0';
		if (*p == '#' || !*p)
			goto next;

		char *colon = strchr(p, ':');

		if (!colon)
			goto next;
		*colon = '\0';

		const char *type = p, *glob = colon + 1;

		if (glob[0] == '*' && glob[1] == '.') {
			glob_suffix[glob_nsuffix].pattern = glob + 1;
			glob_suffix[glob_nsuffix].len = strlen(glob + 1);
			glob_suffix[glob_nsuffix].type = type;
			glob_nsuffix++;
		} else if (!strchr(glob, '*') && !strchr(glob, '?')) {
			glob_exact[glob_nexact].pattern = glob;
			glob_exact[glob_nexact].len = strlen(glob);
			glob_exact[glob_nexact].type = type;
			glob_nexact++;
		}
next:
		if (!nl)
			break;
		p = nl + 1;
	}
	glob_mtim = st.st_mtim;
	glob_size = st.st_size;
	glob_have = 1;
}

int kxdg_mime_from_globs(const char *base, char *out, size_t n)
{
	size_t best = 0, bl;
	int found = 0;

	glob_load();
	if (!glob_data || !glob_suffix || !glob_exact)
		return 0;

	bl = strlen(base);
	for (int i = 0; i < glob_nsuffix; i++) {
		const struct glob_entry *g = &glob_suffix[i];

		if (bl > g->len && g->len > best &&
		    !strcasecmp(base + bl - g->len, g->pattern)) {
			best = g->len;
			snprintf(out, n, "%s", g->type);
			found = 1;
		}
	}
	if (best)
		return found;

	/* An exact name — `Makefile`, `.bashrc`. Only when no suffix matched:
	 * a suffix is the more specific claim. */
	for (int i = 0; i < glob_nexact; i++)
		if (glob_exact[i].len == bl &&
		    !strcasecmp(glob_exact[i].pattern, base)) {
			snprintf(out, n, "%s", glob_exact[i].type);
			return 1;
		}
	return found;
}

void kxdg_mime_for_path(const char *path, char *out, size_t n)
{
	struct stat st;

	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		snprintf(out, n, "inode/directory");
		return;
	}
	if (kxdg_mime_from_globs(base_of(path), out, n))
		return;
	snprintf(out, n, "application/octet-stream");
}

const char *kxdg_mime_for_arg(const char *arg, char *out, size_t n)
{
	char scheme[32];
	struct stat st;
	size_t i, k;

	if (!arg || !*arg) {
		snprintf(out, n, "application/octet-stream");
		return arg;
	}
	/* A NAME THAT `stat()`s IS A PATH. Checked first, so a file whose name
	 * happens to carry a colon is opened rather than handed to a handler
	 * for a scheme nobody registered. It follows symlinks, so a dangling
	 * one is not a name that is there. */
	if (stat(arg, &st) == 0) {
		kxdg_mime_for_path(arg, out, n);
		return arg;
	}
	/* RFC 3986: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":" */
	if (!isalpha((unsigned char)arg[0]))
		goto path;
	for (i = 1; arg[i] && arg[i] != ':'; i++)
		if (!isalnum((unsigned char)arg[i]) && arg[i] != '+' &&
		    arg[i] != '-' && arg[i] != '.')
			goto path;
	if (arg[i] != ':')
		goto path;

	if (i == 4 && !strncasecmp(arg, "file", 4)) {
		const char *p = arg + 5;

		/* `file://host/path` and `file:///path` alike: the path starts
		 * at the slash that ends the authority. `file:/path` has no
		 * authority and starts immediately. */
		if (!strncmp(p, "//", 2)) {
			p += 2;
			while (*p && *p != '/')
				p++;
		}
		if (!*p)
			p = "/";
		kxdg_mime_for_path(p, out, n);
		return p;
	}

	/* A scheme is case-insensitive; the table it is looked up in is not. */
	k = i < sizeof(scheme) - 1 ? i : sizeof(scheme) - 1;
	for (size_t j = 0; j < k; j++)
		scheme[j] = (char)tolower((unsigned char)arg[j]);
	scheme[k] = '\0';
	snprintf(out, n, "x-scheme-handler/%s", scheme);
	return arg;
path:
	kxdg_mime_for_path(arg, out, n);
	return arg;
}

/*
 * The icon names a type may be drawn with, most specific first.
 *
 * freedesktop's rule is `type/subtype` with the slash replaced, then the
 * generic `type-x-generic`, and that is all there is to it — there is no
 * subclass graph here because /usr/share/mime/subclasses is another compiled
 * file and the two extra names below catch what it would have caught for the
 * types anyone has an icon for.
 */
int kxdg_mime_icon_names(const char *mime, char out[][64], int n)
{
	int k = 0;
	char buf[64];

	if (!mime || !*mime || n <= 0)
		return 0;

	snprintf(buf, sizeof(buf), "%s", mime);
	for (char *p = buf; *p; p++)
		if (*p == '/')
			*p = '-';
	snprintf(out[k++], 64, "%s", buf);

	/* `application/x-shellscript` also answers to `text-x-script`, and a
	 * directory is the folder icon under both of its names. */
	if (!strcmp(mime, "inode/directory") && k < n)
		snprintf(out[k++], 64, "folder");

	const char *slash = strchr(mime, '/');
	if (slash && k < n) {
		size_t tl = (size_t)(slash - mime);
		if (tl > 32)
			tl = 32;
		snprintf(out[k], 64, "%.*s-x-generic", (int)tl, mime);
		k++;
	}
	if (k < n)
		snprintf(out[k++], 64, "unknown");
	return k;
}
