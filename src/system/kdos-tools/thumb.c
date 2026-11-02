/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos thumb — a file's picture in the shared thumbnail cache
 *
 *   kdos thumb <file>...        the cached thumbnail of each, when it is
 *                               there and still the file's
 *   kdos thumb --path <file>    where its thumbnail is or would be
 *
 * THE CACHE IS SHARED AND ITS NAME IS A HASH. A thumbnail lives at
 * `$XDG_CACHE_HOME/thumbnails/<size>/<md5>.png`, where the hash is over the
 * file's escaped `file://` URI — so Dolphin, Gwenview, a GTK file chooser and
 * a script all find each other's work. That is the whole value, and it is why
 * the URI escaper and the MD5 are libkbase's rather than this file's: one
 * character escaped differently is a thumbnail nothing else can find.
 *
 * THIS READS THE CACHE AND WRITES NOTHING. KDE's thumbnailers (KIO's, with
 * ffmpegthumbs and kdegraphics-thumbnailers behind them) are what fill it, the
 * first time a file manager or a viewer shows the file; a second writer here
 * would be a second opinion about what a video's picture is.
 *
 * `Thumb::MTime` IS CHECKED, not trusted. A thumbnail carries the source's
 * modification time in a PNG text chunk, and one that no longer matches is a
 * picture of a file that has since changed: it is reported as missing, not
 * handed out. The chunk is read directly — a PNG's chunks are a length, a
 * type and the bytes — so no image library is needed to answer.
 *
 * Exit 0 when every file had a current thumbnail, 1 when any had none, 2
 * usage.
 * ---------------------------------
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kdos-tools.h"

/* The standard's sizes, the one a file list draws first. */
static const char *const SIZES[] = { "normal", "large", "x-large",
				     "xx-large" };

/*
 * A resolved path is PATH_MAX — realpath writes up to that whatever the
 * argument's length, and glibc's fortified form aborts on a buffer that cannot
 * hold it. The URI is three times that plus the scheme, which is what every
 * byte percent-escaping costs: a URI that TRUNCATED would hash to a name no
 * other reader computes, which is the one thing this cache cannot get wrong.
 */
#define THUMB_URI (PATH_MAX * 3 + 8)

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos thumb <file>...        its cached thumbnail\n"
		"       kdos thumb --path <file>    where its thumbnail would "
		"be\n"
		"\nThe freedesktop cache under $XDG_CACHE_HOME/thumbnails, "
		"filled by KDE's\nthumbnailers when a file is first shown.\n");
	return 2;
}

/* The cache path for a source file at one size: the MD5 of its escaped URI. */
static char *thumb_path(const char *abs, const char *size)
{
	char uri[THUMB_URI], md5[33], rel[64];
	KbBuf b = { 0 };
	char *cache;

	snprintf(rel, sizeof(rel), "thumbnails/%s", size);
	cache = kdt_cache_home(rel);
	kb_uri_file(abs, uri, sizeof(uri));
	kb_md5_str(uri, md5);
	kb_buf_printf(&b, "%s/%s.png", cache, md5);
	free(cache);
	return b.p;
}

static uint32_t be32(const unsigned char *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
	       (uint32_t)p[2] << 8 | p[3];
}

/* The value of one tEXt keyword, or 0. A thumbnail's text chunks come before
 * its image data, so the walk stops at the first IDAT. */
static int png_text(const char *path, const char *key, char *out, size_t n)
{
	size_t len = 0, off = 8, klen = strlen(key);
	char *d = kb_read_all(path, &len);
	const unsigned char *p = (const unsigned char *)d;
	int found = 0;

	if (!d || len < 8 || memcmp(p, "\x89PNG\r\n\x1a\n", 8)) {
		free(d);
		return 0;
	}
	while (!found && off + 12 <= len) {
		uint32_t cl = be32(p + off);
		const unsigned char *type = p + off + 4, *data = p + off + 8;

		if (cl > len - off - 12)
			break;
		if (!memcmp(type, "IDAT", 4))
			break;
		if (!memcmp(type, "tEXt", 4) && cl > klen &&
		    !memcmp(data, key, klen) && data[klen] == '\0') {
			size_t vl = cl - klen - 1;

			if (vl >= n)
				vl = n - 1;
			memcpy(out, data + klen + 1, vl);
			out[vl] = '\0';
			found = 1;
		}
		off += 12 + cl;
	}
	free(d);
	return found;
}

int kdt_thumb(int argc, char **argv)
{
	int bad = 0;

	if (!argc)
		return usage();
	if (!strcmp(argv[0], "--path")) {
		char abs[PATH_MAX];
		char *p;

		if (argc != 2 || !realpath(argv[1], abs))
			return usage();
		p = thumb_path(abs, SIZES[0]);
		printf("%s\n", p);
		free(p);
		return 0;
	}
	if (argv[0][0] == '-')
		return usage();

	for (int i = 0; i < argc; i++) {
		char abs[PATH_MAX], mtime[32], want[32];
		struct stat st;
		int hit = 0;

		if (!realpath(argv[i], abs) || stat(abs, &st) != 0) {
			fprintf(stderr, "kdos thumb: %s: no such file\n",
				argv[i]);
			bad = 1;
			continue;
		}
		snprintf(want, sizeof(want), "%lld", (long long)st.st_mtime);
		for (size_t s = 0; s < sizeof(SIZES) / sizeof(SIZES[0]) && !hit;
		     s++) {
			char *p = thumb_path(abs, SIZES[s]);

			if (png_text(p, "Thumb::MTime", mtime, sizeof(mtime)) &&
			    !strcmp(mtime, want)) {
				printf("%s\n", p);
				hit = 1;
			}
			free(p);
		}
		if (!hit) {
			fprintf(stderr, "kdos thumb: %s: no current thumbnail "
					"in the cache — Dolphin makes one when "
					"it shows the file\n", argv[i]);
			bad = 1;
		}
	}
	return bad;
}
