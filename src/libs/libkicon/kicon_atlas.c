/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkicon — the atlas
 *
 * One file, mmap'd, holding every icon of the KDOS theme at every size it was
 * rasterised at. The directory is sorted by (name, size), so a lookup is a
 * binary search and the file is read lazily by the pager — a panel that draws
 * six icons touches six pages of a four-megabyte file.
 *
 * WHY A FILE AND NOT A DIRECTORY OF PNGs. 90 names at 5 sizes is 450 files,
 * and the lookup for one of them is 450 stat() calls at worst spread over
 * several theme directories, on a machine whose whole aesthetic argument is
 * that the panel redraws in a millisecond. It also makes the artwork ONE
 * LFS object instead of 450, which is the difference between a clone and an
 * afternoon.
 *
 * NOTHING IS TRUSTED. The header, every directory entry and every blob extent
 * are checked against the mapped length before anything is dereferenced —
 * this file is on disk and is exactly the sort of thing that survives a torn
 * write. A malformed atlas is an ABSENT atlas, never a partial one, which is
 * the same rule kb_snap.c keeps about a manifest.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kicon_int.h"

#define KIA_MAGIC "KIA1"

struct kia_ent {
	uint16_t nlen;
	uint16_t size;
	uint32_t noff;		/* offset of the name, from the file start */
	uint32_t off;		/* offset of the blob                       */
	uint32_t len;
};

static const uint8_t *map;
static size_t maplen;
static const struct kia_ent *dir;
static uint32_t ndir;

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
	return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

int ki_atlas_ok(void)
{
	return map != NULL;
}

int ki_atlas_count(void)
{
	return (int)ndir;
}

void ki_atlas_close(void)
{
	if (map)
		munmap((void *)map, maplen);
	free((void *)dir);
	map = NULL;
	dir = NULL;
	maplen = 0;
	ndir = 0;
}

int ki_atlas_open(const char *path)
{
	ki_atlas_close();

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < 8 ||
	    (uint64_t)st.st_size > (uint64_t)256 * 1024 * 1024) {
		close(fd);
		return -1;
	}

	const uint8_t *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE,
				fd, 0);
	close(fd);
	if (m == MAP_FAILED)
		return -1;

	if (memcmp(m, KIA_MAGIC, 4) != 0)
		goto bad;

	uint32_t n = rd32(m + 4);
	/* 16 bytes on the wire per entry; the name offsets follow. */
	if (n == 0 || n > 100000 ||
	    (uint64_t)8 + (uint64_t)n * 16 > (uint64_t)st.st_size)
		goto bad;

	struct kia_ent *d = calloc(n, sizeof(*d));
	if (!d)
		goto bad;

	const uint8_t *p = m + 8;
	for (uint32_t i = 0; i < n; i++, p += 16) {
		d[i].nlen = rd16(p);
		d[i].size = rd16(p + 2);
		d[i].noff = rd32(p + 4);
		d[i].off = rd32(p + 8);
		d[i].len = rd32(p + 12);
		/* Every extent inside the file, or the whole atlas is absent.
		 * The additions are done in 64 bits on purpose: two uint32s
		 * that wrap look like a perfectly reasonable extent. */
		if ((uint64_t)d[i].noff + d[i].nlen > (uint64_t)st.st_size ||
		    (uint64_t)d[i].off + d[i].len > (uint64_t)st.st_size ||
		    d[i].nlen == 0 || d[i].len == 0) {
			free(d);
			goto bad;
		}
	}

	map = m;
	maplen = (size_t)st.st_size;
	dir = d;
	ndir = n;
	return 0;
bad:
	munmap((void *)m, (size_t)st.st_size);
	return -1;
}

static int ent_cmp_name(const struct kia_ent *e, const char *name, size_t nl)
{
	size_t l = e->nlen < nl ? e->nlen : nl;
	int c = memcmp(map + e->noff, name, l);
	if (c)
		return c;
	if (e->nlen == nl)
		return 0;
	return e->nlen < nl ? -1 : 1;
}

const void *ki_atlas_find(const char *name, int want, size_t *len, int *size)
{
	if (!map || !name)
		return NULL;

	size_t nl = strlen(name);

	/* Lower bound on the name; entries for one name are contiguous and
	 * ascending in size, which is what makes "smallest >= want" a walk of
	 * a handful of rows rather than a scan. */
	uint32_t lo = 0, hi = ndir;
	while (lo < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		if (ent_cmp_name(&dir[mid], name, nl) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	if (lo >= ndir || ent_cmp_name(&dir[lo], name, nl) != 0)
		return NULL;

	const struct kia_ent *best = &dir[lo];
	for (uint32_t i = lo; i < ndir && ent_cmp_name(&dir[i], name, nl) == 0;
	     i++) {
		/* Upscaling a small icon looks like a small icon; downscaling
		 * a large one looks like the icon. So the first size at or
		 * above what was asked for wins, and the largest there is when
		 * nothing reaches it. */
		if (dir[i].size >= want) {
			best = &dir[i];
			break;
		}
		if (dir[i].size > best->size)
			best = &dir[i];
	}

	if (len)
		*len = best->len;
	if (size)
		*size = best->size;
	return map + best->off;
}
