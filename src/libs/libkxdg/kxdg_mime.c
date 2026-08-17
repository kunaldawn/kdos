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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kxdg.h"

static const char *base_of(const char *path)
{
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

int kxdg_mime_from_globs(const char *base, char *out, size_t n)
{
	size_t len = 0;
	char *data = kb_read_all(KXDG_MIME_GLOBS, &len);
	size_t best = 0;
	int found = 0;

	if (!data)
		return 0;

	for (char *p = data; *p;) {
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
			const char *suffix = glob + 1;
			size_t sl = strlen(suffix), bl = strlen(base);
			if (bl > sl && !strcasecmp(base + bl - sl, suffix) &&
			    sl > best) {
				best = sl;
				snprintf(out, n, "%s", type);
				found = 1;
			}
		} else if (!strchr(glob, '*') && !strchr(glob, '?') &&
			   !strcasecmp(glob, base) && best == 0) {
			/* An exact name — `Makefile`, `.bashrc`. Only when no
			 * suffix matched: a suffix is the more specific claim. */
			snprintf(out, n, "%s", type);
			found = 1;
		}
next:
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
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
