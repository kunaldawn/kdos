/* libkimg under the sanitisers, and under mutation.
 *
 * Its own driver rather than a block in selftest.c, because the corpus has to
 * run against ASan and UBSan and the rest of the suite does not need to be
 * rebuilt to do it. Two passes:
 *
 *   1. Every fixture, decoded under a budget.
 *   2. Every fixture MUTATED — one byte at a time, then truncated at every
 *      length — because a corpus of files somebody wrote by hand only ever
 *      exercises the paths they thought of. A decoder must answer NULL or an
 *      image for any bytes at all; it must never read past the end of them.
 *
 * Deterministic: the mutations are a fixed walk, not a random one, so a
 * failure here is a failure that reproduces.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kimg.h"

static int decodes, refusals;

static void run(const unsigned char *p, size_t n, int type)
{
	KimgBudget b = { .max_w = 256, .max_h = 256,
			 .max_bytes = 256 * 256 * 4 };
	pixman_image_t *img = kimg_decode(p, n, type, &b);

	if (img) {
		/* Touch every pixel: a decoder that returned an image
		 * describing more memory than it allocated is only caught by
		 * reading it. */
		int w = pixman_image_get_width(img);
		int h = pixman_image_get_height(img);
		uint32_t *bits = pixman_image_get_data(img);
		unsigned long sum = 0;

		for (long i = 0; i < (long)w * h; i++)
			sum += bits[i];
		(void)sum;
		pixman_image_unref(img);
		decodes++;
	} else {
		refusals++;
	}
}

int main(int argc, char **argv)
{
	for (int a = 1; a < argc; a++) {
		FILE *f = fopen(argv[a], "rb");
		unsigned char *buf;
		long n;

		if (!f) {
			fprintf(stderr, "fuzz: no %s\n", argv[a]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		n = ftell(f);
		fseek(f, 0, SEEK_SET);
		buf = malloc((size_t)n);
		if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
			fclose(f);
			free(buf);
			return 1;
		}
		fclose(f);

		int type = strstr(argv[a], ".six") ? KIMG_SIXEL : KIMG_AUTO;

		run(buf, (size_t)n, type);

		/* Truncated at every length. */
		for (long L = 0; L < n; L++)
			run(buf, (size_t)L, type);

		/* One byte at a time, flipped to three values that break
		 * length fields and magic without needing a random source. */
		for (long i = 0; i < n; i++) {
			unsigned char save = buf[i];
			static const unsigned char V[] = { 0x00, 0xff, 0x7f };

			for (int v = 0; v < 3; v++) {
				buf[i] = V[v];
				run(buf, (size_t)n, type);
			}
			buf[i] = save;
		}
		free(buf);
	}

	printf("libkimg fuzz: %d decoded, %d refused, no crash\n",
	       decodes, refusals);
	return 0;
}
