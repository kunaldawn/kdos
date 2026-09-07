/* The character-name index, written by mkcharidx and mmapped by chars.c.
 *
 * THE STRUCTS ARE THE FILE FORMAT, so both sides agree by including this
 * rather than by two copies of the same offsets. It is written and read on the
 * same machine's build, so it is native-endian and unpacked on purpose —
 * a portable encoding would cost a decode pass over forty thousand entries
 * that this file exists to avoid. `KCHR_VERSION` is what makes a stale index
 * from an older image an error rather than a wrong answer. */

#ifndef KDOS_CHARIDX_H
#define KDOS_CHARIDX_H

#include <stdint.h>

#define KCHR_VERSION 1
#define KCHR_PATH "/usr/share/kdos/charnames.idx"

struct kchr_head {
	char magic[4];		/* "KCHR"                                  */
	uint32_t version;
	uint32_t count;		/* entries following this header           */
	uint32_t blob;		/* bytes of name text after the entries    */
};

struct kchr_entry {
	uint32_t cp;
	uint32_t off;		/* into the blob                           */
	uint32_t len;
};

#endif
