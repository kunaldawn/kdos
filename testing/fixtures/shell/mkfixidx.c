/*
 * A character index of eight entries, for the golden.
 *
 * IT INCLUDES THE SAME HEADER THE READER DOES, so the file format has one
 * definition and a change to it breaks this rather than producing a golden of
 * a surface reading its own garbage. The real index is built from ICU at build
 * time and is six megabytes; a golden cannot depend on ICU being on the host,
 * and a frame drawn from six megabytes of Unicode is a frame nobody can read a
 * diff of.
 */

#include <stdio.h>
#include <string.h>

#include "charidx.h"

static const struct {
	unsigned cp;
	const char *name;
} ROW[] = {
	{ 0x2190, "LEFTWARDS ARROW" },
	{ 0x2191, "UPWARDS ARROW" },
	{ 0x2192, "RIGHTWARDS ARROW" },
	{ 0x2193, "DOWNWARDS ARROW" },
	{ 0x21D2, "RIGHTWARDS DOUBLE ARROW" },
	{ 0x00B1, "PLUS-MINUS SIGN" },
	{ 0x00A9, "COPYRIGHT SIGN" },
	{ 0x2026, "HORIZONTAL ELLIPSIS" }
};
#define NROW ((int)(sizeof(ROW) / sizeof(ROW[0])))

int main(int argc, char **argv)
{
	struct kchr_entry ent[NROW];
	char blob[512];
	size_t blen = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: mkfixidx OUT\n");
		return 2;
	}
	for (int i = 0; i < NROW; i++) {
		size_t n = strlen(ROW[i].name);

		ent[i].cp = ROW[i].cp;
		ent[i].off = (uint32_t)blen;
		ent[i].len = (uint32_t)n;
		memcpy(blob + blen, ROW[i].name, n);
		blen += n;
	}

	FILE *f = fopen(argv[1], "wb");
	struct kchr_head h = { { 'K', 'C', 'H', 'R' }, KCHR_VERSION, NROW,
			       (uint32_t)blen };

	if (!f)
		return 1;
	if (fwrite(&h, sizeof(h), 1, f) != 1 ||
	    fwrite(ent, sizeof(ent[0]), NROW, f) != NROW ||
	    fwrite(blob, 1, blen, f) != blen || fclose(f) != 0)
		return 1;
	return 0;
}
