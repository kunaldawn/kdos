/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   mkcharidx — the character-name index, built once at build time
 *
 * RUNS AT BUILD TIME AND SHIPS ITS OUTPUT. It links ICU; `kdos-shell` does
 * not and must not. ICU is thirty megabytes of shared library and a data blob,
 * and a panel that loaded it to answer "what is this character called" would
 * carry the whole of it into every one of the thirty surfaces the binary is.
 * The names do not change between releases of this image, so the work belongs
 * where the image is made.
 *
 * ONE FILE, MMAPPED, NO DECOMPRESSION. The reader maps it and searches it in
 * place; anything the reader had to unpack would have to be unpacked on every
 * summon of an accessory whose whole point is that it opens instantly.
 *
 * THE BLOB IS UPPER CASE, WHICH IS WHAT MAKES THE SEARCH CHEAP. Unicode names
 * are upper case by definition, so the reader upper-cases only the query — a
 * few characters — instead of forty thousand names per keystroke.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unicode/uchar.h>
#include <unicode/utypes.h>

#include "../charidx.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: mkcharidx OUT\n");
		return 2;
	}

	struct kchr_entry *ent = calloc(0x110000, sizeof(*ent));
	char *blob = malloc(0x110000 * 4);
	size_t blen = 0;
	uint32_t n = 0;

	if (!ent || !blob) {
		fprintf(stderr, "mkcharidx: out of memory\n");
		return 1;
	}

	for (UChar32 cp = 0x20; cp <= 0x10FFFF; cp++) {
		UErrorCode err = U_ZERO_ERROR;
		char name[256];
		/*
		 * EXTENDED, not U_UNICODE_CHAR_NAME. The extended form gives
		 * the control characters and the unnamed ranges a name in
		 * angle brackets instead of an empty string, and a search that
		 * silently had no row for U+0009 would look like a broken
		 * search rather than a character with no formal name.
		 */
		int32_t len = u_charName(cp, U_EXTENDED_CHAR_NAME, name,
					 (int32_t)sizeof(name), &err);

		if (U_FAILURE(err) || len <= 0)
			continue;
		if (name[0] == '<')
			continue;	/* a range placeholder, not a name */
		for (int32_t i = 0; i < len; i++)
			if (name[i] >= 'a' && name[i] <= 'z')
				name[i] = (char)(name[i] - 'a' + 'A');
		ent[n].cp = (uint32_t)cp;
		ent[n].off = (uint32_t)blen;
		ent[n].len = (uint32_t)len;
		memcpy(blob + blen, name, (size_t)len);
		blen += (size_t)len;
		n++;
	}

	FILE *f = fopen(argv[1], "wb");
	struct kchr_head h = { { 'K', 'C', 'H', 'R' }, KCHR_VERSION, n,
			       (uint32_t)blen };

	if (!f) {
		perror("mkcharidx");
		return 1;
	}
	if (fwrite(&h, sizeof(h), 1, f) != 1 ||
	    fwrite(ent, sizeof(*ent), n, f) != n ||
	    fwrite(blob, 1, blen, f) != blen || fclose(f) != 0) {
		fprintf(stderr, "mkcharidx: short write\n");
		return 1;
	}
	fprintf(stderr, "mkcharidx: %u names, %zu bytes of text\n", n, blen);
	return 0;
}
