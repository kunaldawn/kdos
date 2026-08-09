/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — a minimal ustar stream reader and writer
 *
 * Enough tar to take the appbox image apart and put it back together, and no
 * more. The only archives this ever sees are `podman save` output: regular
 * files with short names, no devices, no hard links, no sparse members.
 *
 * GNU long names ('L') are handled because they cost four lines. A pax
 * extended header ('x'/'g') is skipped with its payload — if podman ever
 * starts emitting one for a path this will notice loudly rather than
 * silently produce a broken archive.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"

#define BLK 512

typedef struct {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
	char pad[12];
} TarHdr;

/* ──────────────────────────────────────────────────────────────────────── */

static int read_full(int fd, void *buf, size_t n)
{
	char *p = buf;
	size_t got = 0;
	while (got < n) {
		ssize_t r = read(fd, p + got, n - got);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (r == 0)
			break;
		got += (size_t)r;
	}
	return (int)got;
}

static int write_full(int fd, const void *buf, size_t n)
{
	const char *p = buf;
	size_t off = 0;
	while (off < n) {
		ssize_t w = write(fd, p + off, n - off);
		if (w <= 0) {
			if (w < 0 && errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)w;
	}
	return 0;
}

static long long from_octal(const char *s, size_t n)
{
	/* GNU base-256: the top bit of the first byte marks a binary size,
	 * which is how a member over 8 GB is expressed. */
	if ((unsigned char)s[0] & 0x80) {
		long long v = (unsigned char)s[0] & 0x7f;
		for (size_t i = 1; i < n; i++)
			v = (v << 8) | (unsigned char)s[i];
		return v;
	}
	long long v = 0;
	for (size_t i = 0; i < n && s[i] >= '0' && s[i] <= '7'; i++)
		v = v * 8 + (s[i] - '0');
	return v;
}

static void to_octal(char *dst, size_t n, long long v)
{
	memset(dst, '0', n - 1);
	dst[n - 1] = 0;
	for (size_t i = n - 1; i-- > 0;) {
		dst[i] = (char)('0' + (v & 7));
		v >>= 3;
		if (!v)
			break;
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

int kb_tar_open(KbTarIn *t, const char *path)
{
	memset(t, 0, sizeof(*t));
	t->fd = open(path, O_RDONLY | O_CLOEXEC);
	return t->fd < 0 ? -1 : 0;
}

void kb_tar_close(KbTarIn *t)
{
	if (t->fd >= 0)
		close(t->fd);
	t->fd = -1;
}

int kb_tar_next(KbTarIn *t, KbTarEntry *e)
{
	char longname[512];
	int have_long = 0;

	for (;;) {
		/* Skip whatever is left of the previous member, payload and
		 * its padding both. */
		while (t->remain > 0) {
			char skip[4096];
			size_t n = t->remain > (long long)sizeof(skip)
					   ? sizeof(skip)
					   : (size_t)t->remain;
			if (read_full(t->fd, skip, n) != (int)n)
				return -1;
			t->remain -= (long long)n;
		}
		if (t->pad) {
			char skip[BLK];
			if (read_full(t->fd, skip, (size_t)t->pad) != t->pad)
				return -1;
			t->pad = 0;
		}

		TarHdr h;
		int got = read_full(t->fd, &h, sizeof(h));
		if (got == 0)
			return 0;
		if (got != (int)sizeof(h))
			return -1;
		if (h.name[0] == 0)
			return 0;	/* end-of-archive block */

		long long size = from_octal(h.size, sizeof(h.size));
		long long pad = (BLK - (size % BLK)) % BLK;

		if (h.typeflag == 'L') {
			/* GNU long name: the payload IS the next member's
			 * name. */
			size_t n = size < (long long)sizeof(longname) - 1
					   ? (size_t)size
					   : sizeof(longname) - 1;
			if (read_full(t->fd, longname, n) != (int)n)
				return -1;
			longname[n] = 0;
			t->remain = size - (long long)n;
			t->pad = (int)pad;
			have_long = 1;
			continue;
		}
		if (h.typeflag == 'x' || h.typeflag == 'g') {
			kb_warn("tar: pax header in the stream is being ignored");
			t->remain = size;
			t->pad = (int)pad;
			continue;
		}

		if (have_long) {
			kb_strlcpy(e->name, longname, sizeof(e->name));
		} else if (h.prefix[0] && !memcmp(h.magic, "ustar", 5)) {
			char pre[156], nm[101];
			kb_strlcpy(pre, h.prefix, sizeof(pre));
			memcpy(nm, h.name, 100);
			nm[100] = 0;
			snprintf(e->name, sizeof(e->name), "%s/%s", pre, nm);
		} else {
			size_t n = strnlen(h.name, sizeof(h.name));
			memcpy(e->name, h.name, n);
			e->name[n] = 0;
		}
		have_long = 0;

		e->size = size;
		e->typeflag = h.typeflag ? h.typeflag : '0';
		t->remain = size;
		t->pad = (int)pad;
		return 1;
	}
}

int kb_tar_read(KbTarIn *t, void *buf, size_t n)
{
	if (t->remain <= 0)
		return 0;
	if ((long long)n > t->remain)
		n = (size_t)t->remain;
	int got = read_full(t->fd, buf, n);
	if (got > 0)
		t->remain -= got;
	return got;
}

/* ──────────────────────────────────────────────────────────────────────── */

int kb_tar_put_header(int fd, const char *name, long long size)
{
	TarHdr h;
	memset(&h, 0, sizeof(h));

	size_t n = strlen(name);
	if (n >= sizeof(h.name)) {
		kb_warn("tar: name too long for ustar: %s", name);
		return -1;
	}
	memcpy(h.name, name, n);

	memcpy(h.mode, "0000644", 8);
	memcpy(h.uid, "0000000", 8);
	memcpy(h.gid, "0000000", 8);
	to_octal(h.size, sizeof(h.size), size);
	to_octal(h.mtime, sizeof(h.mtime), 0);
	h.typeflag = '0';
	memcpy(h.magic, "ustar", 6);
	memcpy(h.version, "00", 2);

	/* The checksum is computed with its own field read as spaces. */
	memset(h.chksum, ' ', sizeof(h.chksum));
	unsigned sum = 0;
	const unsigned char *p = (const unsigned char *)&h;
	for (size_t i = 0; i < sizeof(h); i++)
		sum += p[i];
	to_octal(h.chksum, 7, sum);
	h.chksum[6] = 0;
	h.chksum[7] = ' ';

	return write_full(fd, &h, sizeof(h));
}

int kb_tar_pad(int fd, long long size)
{
	long long pad = (BLK - (size % BLK)) % BLK;
	if (!pad)
		return 0;
	char zero[BLK];
	memset(zero, 0, sizeof(zero));
	return write_full(fd, zero, (size_t)pad);
}

int kb_tar_finish(int fd)
{
	char zero[BLK * 2];
	memset(zero, 0, sizeof(zero));
	return write_full(fd, zero, sizeof(zero));
}
