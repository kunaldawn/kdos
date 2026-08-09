/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — whole files and whole trees
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kbase.h"

char *kb_read_all(const char *path, size_t *len)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	struct stat st;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
		close(fd);
		return NULL;
	}

	/* st_size is a HINT, not the length. Every file under /proc reports 0 —
	 * reading to st_size returns an empty string for /proc/mounts and
	 * /proc/self/mountinfo, which reads as "nothing is mounted". So the
	 * size only sizes the first allocation; the loop runs to real EOF. */
	size_t cap = st.st_size > 0 ? (size_t)st.st_size : 4096;
	char *buf = kb_calloc(1, cap + 1);
	size_t got = 0;
	for (;;) {
		if (got == cap) {
			size_t ncap = cap * 2;
			char *nb = kb_calloc(1, ncap + 1);
			memcpy(nb, buf, got);
			free(buf);
			buf = nb;
			cap = ncap;
		}
		ssize_t r = read(fd, buf + got, cap - got);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			close(fd);
			free(buf);
			return NULL;
		}
		if (r == 0)
			break;
		got += (size_t)r;
	}
	close(fd);
	buf[got] = 0;
	if (len)
		*len = got;
	return buf;
}

int kb_write_all(const char *path, const void *data, size_t len)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return -1;
	const char *p = data;
	size_t off = 0;
	while (off < len) {
		ssize_t w = write(fd, p + off, len - off);
		if (w <= 0) {
			if (w < 0 && errno == EINTR)
				continue;
			close(fd);
			return -1;
		}
		off += (size_t)w;
	}
	return close(fd) < 0 ? -1 : 0;
}

int kb_is_dir(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int kb_is_link(const char *p)
{
	struct stat st;
	return lstat(p, &st) == 0 && S_ISLNK(st.st_mode);
}

int kb_copy_file(const char *src, const char *dst)
{
	size_t n = 0;
	char *buf = kb_read_all(src, &n);
	if (!buf)
		return -1;
	int rc = kb_write_all(dst, buf, n);
	free(buf);
	return rc;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int cmp_name(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

char **kb_listdir(const char *path, int *count)
{
	DIR *d = opendir(path);
	if (!d) {
		if (count)
			*count = 0;
		return NULL;
	}

	int cap = 64, n = 0;
	char **v = kb_calloc((size_t)cap, sizeof(*v));
	struct dirent *e;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		if (n + 1 >= cap) {
			cap *= 2;
			char **nv = kb_calloc((size_t)cap, sizeof(*nv));
			memcpy(nv, v, (size_t)n * sizeof(*v));
			free(v);
			v = nv;
		}
		v[n++] = kb_strdup(e->d_name);
	}
	closedir(d);

	qsort(v, (size_t)n, sizeof(*v), cmp_name);
	v[n] = NULL;
	if (count)
		*count = n;
	return v;
}

void kb_strv_free(char **v)
{
	if (!v)
		return;
	for (char **p = v; *p; p++)
		free(*p);
	free(v);
}

/* Depth-first, and it never follows a symlink out of the tree: unlink()
 * removes the link itself, and only a real directory is recursed into. */
int kb_rmtree(const char *path)
{
	struct stat st;
	if (lstat(path, &st) < 0)
		return errno == ENOENT ? 0 : -1;

	if (!S_ISDIR(st.st_mode))
		return unlink(path);

	char **names = kb_listdir(path, NULL);
	if (names) {
		for (char **p = names; *p; p++) {
			char *child = kb_path_join(path, *p);
			kb_rmtree(child);
			free(child);
		}
		kb_strv_free(names);
	}
	return rmdir(path);
}

/* ──────────────────────────────────────────────────────────────────────── */

void kb_buf_add(KbBuf *b, const void *s, size_t n)
{
	if (b->n + n + 1 > b->cap) {
		size_t cap = b->cap ? b->cap : 4096;
		while (cap < b->n + n + 1)
			cap *= 2;
		char *np = kb_calloc(1, cap);
		memcpy(np, b->p, b->n);
		free(b->p);
		b->p = np;
		b->cap = cap;
	}
	memcpy(b->p + b->n, s, n);
	b->n += n;
	b->p[b->n] = 0;
}

void kb_buf_str(KbBuf *b, const char *s)
{
	kb_buf_add(b, s, strlen(s));
}

/* Measure, then format — never truncate. A fixed stack buffer here silently
 * cut a generated btop theme in half at 1024 bytes, and the file it produced
 * was still valid enough to look fine at a glance. */
void kb_buf_printf(KbBuf *b, const char *fmt, ...)
{
	char tmp[1024];
	va_list ap, ap2;

	va_start(ap, fmt);
	va_copy(ap2, ap);
	int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	if (n < 0) {
		va_end(ap2);
		return;
	}
	if ((size_t)n < sizeof(tmp)) {
		va_end(ap2);
		kb_buf_add(b, tmp, (size_t)n);
		return;
	}

	char *big = kb_calloc(1, (size_t)n + 1);
	vsnprintf(big, (size_t)n + 1, fmt, ap2);
	va_end(ap2);
	kb_buf_add(b, big, (size_t)n);
	free(big);
}

void kb_buf_free(KbBuf *b)
{
	free(b->p);
	b->p = NULL;
	b->n = b->cap = 0;
}
