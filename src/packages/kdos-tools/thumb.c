/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos thumb — a small picture of a file, in the cache everything reads
 *
 * THE CACHE IS SHARED AND ITS NAME IS A HASH. A thumbnail lives at
 * `$XDG_CACHE_HOME/thumbnails/normal/<md5>.png`, where the hash is over the
 * file's escaped `file://` URI — so a file manager, an image viewer and this
 * desktop all find each other's work. That is the whole value, and it is why
 * the URI escaper and the MD5 are libkbase's rather than this file's: one
 * character escaped differently is a thumbnail nothing else can find.
 *
 * `Thumb::URI` AND `Thumb::MTime` ARE REQUIRED, not decoration. A reader
 * checks them before trusting the picture: without the mtime a thumbnail of
 * an edited file is served forever, and without the URI a hash collision is
 * undetectable. libpng's simplified API cannot write a text chunk, so the
 * cache file goes out through the full one.
 *
 * THIS PROGRAM DECODES NOTHING IT DOES NOT HAVE TO. A video is
 * `ffmpegthumbnailer`, a PDF is `pdftoppm`, and every other still — JPEG, GIF,
 * WebP, a camera raw — is `magick`, each of them a program on this image that
 * does one thing well. What is left here is the PNG the helper wrote, the
 * scale, and the cache's own rules.
 *
 * A HELPER MUST HAND BACK A FILE AT THE NAME IT WAS GIVEN, which is why
 * `exiv2` is not one of them: `-ep1` writes `<file>-preview1.<ext>` with an
 * extension it chooses, usually a JPEG, and a caller cannot name the result or
 * read it. `magick` covers the same files and writes where it is told.
 *
 * `--ppm` IS FOR A CALLER WITH NO IMAGE LIBRARY. `kdos-pick`'s preview pane
 * is compiled without libkcell so its offscreen build stays dependency-free,
 * and it already parses P6; handing it one is what lets the chooser show a
 * photograph without linking a decoder.
 * ---------------------------------
 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef KDOS_HAVE_LIBPNG
#include <png.h>
#endif

#include "kbase.h"
#include "kdos-tools.h"

/* The standard's two sizes. `normal` is what a file list wants and is the one
 * this writes; `large` costs four times the pixels for a pane nothing here
 * draws. */
#define THUMB_PX 128

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos thumb <file>...        put one in the shared cache\n"
		"       kdos thumb --path <file>    where its thumbnail would be\n"
		"       kdos thumb --ppm <file> <out.ppm>\n"
		"                                   a small P6, for a caller with\n"
		"                                   no image library\n"
		"\nThe freedesktop cache under $XDG_CACHE_HOME/thumbnails.\n");
	return 2;
}

/* The cache path for a source file: the MD5 of its escaped URI. */
static char *thumb_path(const char *abs)
{
	char uri[2048], md5[33];
	KbBuf b = { 0 };
	char *cache = kdt_cache_home("thumbnails/normal");

	kb_uri_file(abs, uri, sizeof(uri));
	kb_md5_str(uri, md5);
	kb_buf_printf(&b, "%s/%s.png", cache, md5);
	free(cache);
	return b.p;
}

#ifdef KDOS_HAVE_LIBPNG

/*
 * WHICH HELPER, BY EXTENSION AND NOT BY CONTENT. `file` would be a fourth
 * answer to a question the MIME table already has, and the helpers themselves
 * refuse what they cannot read — so a wrong guess costs a failed fork rather
 * than a wrong picture.
 */
enum { SRC_NONE = 0, SRC_PNG, SRC_VIDEO, SRC_PDF, SRC_MAGICK };

static int source_kind(const char *path)
{
	static const char *const VIDEO[] = { ".mp4", ".mkv", ".webm", ".avi",
					     ".mov", ".m4v", NULL };
	/* Everything ImageMagick reads that somebody keeps in a folder: the
	 * ordinary stills, and the camera raws it has delegates for. */
	static const char *const STILL[] = { ".jpg", ".jpeg", ".gif", ".bmp",
					     ".tif", ".tiff", ".webp", ".ppm",
					     ".pnm", ".xpm", ".ico", ".cr2",
					     ".nef", ".arw", ".dng", ".raf",
					     ".orf", NULL };
	const char *dot = strrchr(path, '.');

	if (!dot)
		return SRC_NONE;
	if (!strcasecmp(dot, ".png"))
		return SRC_PNG;
	if (!strcasecmp(dot, ".pdf"))
		return SRC_PDF;
	for (int i = 0; VIDEO[i]; i++)
		if (!strcasecmp(dot, VIDEO[i]))
			return SRC_VIDEO;
	for (int i = 0; STILL[i]; i++)
		if (!strcasecmp(dot, STILL[i]))
			return SRC_MAGICK;
	return SRC_NONE;
}

/*
 * Get a PNG for the source into `out`, forking a helper where one is needed.
 * Returns 0, or -1 with nothing written. A missing helper is a refusal and
 * not an error: the machine simply cannot make a picture of that kind of file.
 */
static int source_png(const char *path, int kind, const char *out)
{
	KbArgv a = { 0 };
	char px[16];

	snprintf(px, sizeof(px), "%d", THUMB_PX);
	switch (kind) {
	case SRC_PNG:
		return 0;		/* the source IS the picture */
	case SRC_VIDEO:
		if (!kb_have_prog("ffmpegthumbnailer"))
			return -1;
		kb_argv_add(&a, "ffmpegthumbnailer");
		kb_argv_add(&a, "-i");
		kb_argv_add(&a, path);
		kb_argv_add(&a, "-o");
		kb_argv_add(&a, out);
		kb_argv_add(&a, "-s");
		kb_argv_add(&a, px);
		break;
	case SRC_PDF:
		if (!kb_have_prog("pdftoppm"))
			return -1;
		/*
		 * THE FIRST PAGE ONLY, and `-singlefile` so the name is the
		 * one asked for: pdftoppm otherwise appends a page number and
		 * the file this returns would not be there.
		 */
		kb_argv_add(&a, "pdftoppm");
		kb_argv_add(&a, "-png");
		kb_argv_add(&a, "-f");
		kb_argv_add(&a, "1");
		kb_argv_add(&a, "-l");
		kb_argv_add(&a, "1");
		kb_argv_add(&a, "-singlefile");
		kb_argv_add(&a, "-scale-to");
		kb_argv_add(&a, px);
		kb_argv_add(&a, path);
		/* pdftoppm appends its own extension to the stem. */
		{
			char stem[1024];
			char *dot;

			snprintf(stem, sizeof(stem), "%s", out);
			dot = strrchr(stem, '.');
			if (dot)
				*dot = '\0';
			kb_argv_add(&a, stem);
		}
		break;
	case SRC_MAGICK:
		if (!kb_have_prog("magick"))
			return -1;
		/*
		 * `[0]` IS THE FIRST FRAME OR PAGE, so an animation and a raw
		 * with two images in it both give one picture rather than a
		 * numbered series of files.
		 *
		 * `-auto-orient` BEFORE THE RESIZE. A photograph carries its
		 * rotation in EXIF and a thumbnail that ignores it is the
		 * portrait shown sideways that every gallery has shipped once.
		 *
		 * `PNG:` names the FORMAT rather than trusting the extension,
		 * and `-strip` drops the profiles — a colour profile is larger
		 * than the picture at this size.
		 */
		kb_argv_add(&a, "magick");
		{
			char first[1100];

			snprintf(first, sizeof(first), "%.1000s[0]", path);
			kb_argv_addf(&a, "%s", first);
		}
		kb_argv_add(&a, "-auto-orient");
		kb_argv_add(&a, "-thumbnail");
		kb_argv_addf(&a, "%dx%d", THUMB_PX, THUMB_PX);
		kb_argv_add(&a, "-strip");
		kb_argv_addf(&a, "PNG:%s", out);
		break;
	default:
		return -1;
	}
	kb_argv_end(&a);
	return kb_run(&a) == 0 ? 0 : -1;
}

/* A box scale into at most THUMB_PX on the long side. Nearest-neighbour would
 * alias a photograph into noise at this size; averaging the source rectangle
 * is what makes a 4000-pixel image still readable at 128. */
static uint8_t *scale_rgb(const uint8_t *src, int sw, int sh, int *dw, int *dh)
{
	int ow = sw, oh = sh;
	uint8_t *dst;

	if (sw <= 0 || sh <= 0)
		return NULL;
	if (sw > THUMB_PX || sh > THUMB_PX) {
		if (sw >= sh) {
			ow = THUMB_PX;
			oh = (int)((long long)sh * THUMB_PX / sw);
		} else {
			oh = THUMB_PX;
			ow = (int)((long long)sw * THUMB_PX / sh);
		}
	}
	if (ow < 1)
		ow = 1;
	if (oh < 1)
		oh = 1;
	dst = kb_calloc(1, (size_t)ow * oh * 3);
	for (int y = 0; y < oh; y++) {
		int y0 = (int)((long long)y * sh / oh);
		int y1 = (int)((long long)(y + 1) * sh / oh);

		if (y1 <= y0)
			y1 = y0 + 1;
		for (int x = 0; x < ow; x++) {
			int x0 = (int)((long long)x * sw / ow);
			int x1 = (int)((long long)(x + 1) * sw / ow);
			unsigned long r = 0, g = 0, b = 0, k = 0;

			if (x1 <= x0)
				x1 = x0 + 1;
			for (int sy = y0; sy < y1 && sy < sh; sy++)
				for (int sx = x0; sx < x1 && sx < sw; sx++) {
					const uint8_t *p = src +
						((size_t)sy * sw + sx) * 3;
					r += p[0];
					g += p[1];
					b += p[2];
					k++;
				}
			if (!k)
				k = 1;
			dst[((size_t)y * ow + x) * 3 + 0] = (uint8_t)(r / k);
			dst[((size_t)y * ow + x) * 3 + 1] = (uint8_t)(g / k);
			dst[((size_t)y * ow + x) * 3 + 2] = (uint8_t)(b / k);
		}
	}
	*dw = ow;
	*dh = oh;
	return dst;
}

static uint8_t *read_png_rgb(const char *path, int *w, int *h)
{
	png_image in;
	uint8_t *px;

	memset(&in, 0, sizeof(in));
	in.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&in, path))
		return NULL;
	in.format = PNG_FORMAT_RGB;
	px = kb_calloc(1, PNG_IMAGE_SIZE(in));
	if (!png_image_finish_read(&in, NULL, px, 0, NULL)) {
		png_image_free(&in);
		free(px);
		return NULL;
	}
	*w = (int)in.width;
	*h = (int)in.height;
	png_image_free(&in);
	return px;
}

/*
 * The cache file, through the FULL write API — the simplified one has no way
 * to attach a text chunk, and the two keys are what make the entry trustable.
 */
static int write_cache_png(const char *out, const uint8_t *rgb, int w, int h,
			   const char *uri, const char *mtime)
{
	png_structp png;
	png_infop info;
	png_text text[2];
	FILE *f;
	KbBuf tmp = { 0 };
	int rc = -1;

	kb_buf_printf(&tmp, "%s.tmp", out);
	f = fopen(tmp.p, "wb");
	if (!f)
		goto out;
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info = png ? png_create_info_struct(png) : NULL;
	if (!png || !info)
		goto close;
	if (setjmp(png_jmpbuf(png)))
		goto destroy;

	png_init_io(png, f);
	png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8,
		     PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	memset(text, 0, sizeof(text));
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[0].key = (png_charp)"Thumb::URI";
	text[0].text = (png_charp)uri;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = (png_charp)"Thumb::MTime";
	text[1].text = (png_charp)mtime;
	png_set_text(png, info, text, 2);

	png_write_info(png, info);
	for (int y = 0; y < h; y++)
		png_write_row(png, (png_bytep)(rgb + (size_t)y * w * 3));
	png_write_end(png, NULL);
	rc = 0;
destroy:
	png_destroy_write_struct(&png, &info);
close:
	fclose(f);
	/*
	 * 0600 AND RENAMED INTO PLACE. The standard requires the mode — a
	 * thumbnail can reveal the content of a file whose own permissions
	 * hide it — and the rename is what keeps a reader from finding half a
	 * picture in a cache it shares.
	 */
	if (rc == 0) {
		chmod(tmp.p, 0600);
		if (rename(tmp.p, out) != 0)
			rc = -1;
	}
	if (rc != 0)
		unlink(tmp.p);
out:
	kb_buf_free(&tmp);
	return rc;
}

static int write_ppm(const char *out, const uint8_t *rgb, int w, int h)
{
	FILE *f = fopen(out, "wb");

	if (!f)
		return -1;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	if (fwrite(rgb, 3, (size_t)w * h, f) != (size_t)w * h) {
		fclose(f);
		unlink(out);
		return -1;
	}
	fclose(f);
	return 0;
}

/* The picture for a source, scaled, or NULL. The caller frees. */
static uint8_t *thumb_rgb(const char *abs, int *w, int *h)
{
	int kind = source_kind(abs);
	char work[1024];
	const char *png_path = abs;
	uint8_t *raw, *small;
	int rw = 0, rh = 0;

	if (kind == SRC_NONE)
		return NULL;
	if (kind != SRC_PNG) {
		snprintf(work, sizeof(work), "%s/kdos-thumb-%d.png",
			 kb_runtime_dir(), (int)getpid());
		if (source_png(abs, kind, work) != 0)
			return NULL;
		png_path = work;
	}
	raw = read_png_rgb(png_path, &rw, &rh);
	if (png_path != abs)
		unlink(png_path);
	if (!raw)
		return NULL;
	small = scale_rgb(raw, rw, rh, w, h);
	free(raw);
	return small;
}

#endif /* KDOS_HAVE_LIBPNG */

int kdt_thumb(int argc, char **argv)
{
	if (!argc)
		return usage();

	if (!strcmp(argv[0], "--path")) {
		char abs[1024];
		char *p;

		if (argc < 2 || !realpath(argv[1], abs))
			return usage();
		p = thumb_path(abs);
		printf("%s\n", p);
		free(p);
		return 0;
	}

#ifndef KDOS_HAVE_LIBPNG
	fprintf(stderr, "kdos thumb: this build has no PNG support\n");
	return 1;
#else
	if (!strcmp(argv[0], "--ppm")) {
		char abs[1024];
		uint8_t *rgb;
		int w = 0, h = 0, rc;

		if (argc < 3 || !realpath(argv[1], abs))
			return usage();
		rgb = thumb_rgb(abs, &w, &h);
		if (!rgb) {
			fprintf(stderr, "kdos thumb: nothing on this machine "
					"makes a picture of %s\n", argv[1]);
			return 1;
		}
		rc = write_ppm(argv[2], rgb, w, h);
		free(rgb);
		return rc == 0 ? 0 : 1;
	}

	int bad = 0;

	for (int i = 0; i < argc; i++) {
		char abs[1024], uri[2048], mtime[32];
		struct stat st;
		uint8_t *rgb;
		char *out;
		int w = 0, h = 0;

		if (!realpath(argv[i], abs) || stat(abs, &st) != 0) {
			fprintf(stderr, "kdos thumb: %s: no such file\n",
				argv[i]);
			bad = 1;
			continue;
		}
		rgb = thumb_rgb(abs, &w, &h);
		if (!rgb) {
			fprintf(stderr, "kdos thumb: nothing on this machine "
					"makes a picture of %s\n", argv[i]);
			bad = 1;
			continue;
		}
		kb_uri_file(abs, uri, sizeof(uri));
		snprintf(mtime, sizeof(mtime), "%lld",
			 (long long)st.st_mtime);
		out = thumb_path(abs);
		kdt_mkparent(out);
		if (write_cache_png(out, rgb, w, h, uri, mtime) != 0) {
			fprintf(stderr, "kdos thumb: cannot write %s\n", out);
			bad = 1;
		} else {
			printf("%s\n", out);
		}
		free(out);
		free(rgb);
	}
	return bad;
#endif
}
