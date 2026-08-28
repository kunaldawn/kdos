/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — files and PATH
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "kbase.h"

int kb_read_file(const char *path, char *buf, size_t cap)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	ssize_t n = read(fd, buf, cap - 1);
	close(fd);
	if (n < 0)
		return -1;
	buf[n] = 0;
	return (int)n;
}

int kb_read_line_file(const char *path, char *buf, size_t cap)
{
	if (kb_read_file(path, buf, cap) < 0)
		return -1;
	char *nl = strchr(buf, '\n');
	if (nl)
		*nl = 0;
	return (int)strlen(buf);
}

int kb_write_file(const char *path, const char *data)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return -1;
	size_t n = strlen(data), off = 0;
	while (off < n) {
		ssize_t w = write(fd, data + off, n - off);
		if (w <= 0) {
			close(fd);
			return -1;
		}
		off += (size_t)w;
	}
	close(fd);
	return 0;
}

/*
 * A STATE FILE IS REPLACED, NEVER TRUNCATED IN PLACE. `kb_write_file` opens
 * O_TRUNC and then writes, so between those two the file is ZERO BYTES and a
 * failure anywhere after the truncate — ENOSPC, EIO, a signal, a crash, a
 * reader arriving mid-write — leaves it that way. Measured: a box profile
 * found 0 bytes with every key gone, which loses `base` and makes the box
 * unstartable, on a machine where nothing was wrong but the write.
 *
 * Temp file, fsync the FILE, fsync the DIRECTORY, rename. The directory fsync
 * is the step people leave out, and without it the rename can be lost while
 * the data survives — the same rule `kdos-bootctl` keeps for the A/B state
 * file on the ESP, applied to every state file that cannot afford to come
 * back empty.
 */
int kb_write_file_atomic(const char *path, const char *data)
{
	char tmp[4096], dirbuf[4096];
	size_t n = strlen(data), off = 0;
	int fd, dfd;
	char *slash;

	if (snprintf(tmp, sizeof(tmp), "%s.tmpXXXXXX", path) >= (int)sizeof(tmp))
		return -1;
	fd = mkstemp(tmp);
	if (fd < 0)
		return -1;
	if (fchmod(fd, 0644) != 0)
		goto fail;
	while (off < n) {
		ssize_t w = write(fd, data + off, n - off);
		if (w <= 0) {
			if (w < 0 && errno == EINTR)
				continue;
			goto fail;
		}
		off += (size_t)w;
	}
	if (fsync(fd) != 0)
		goto fail;
	if (close(fd) != 0) {
		fd = -1;
		goto fail;
	}
	fd = -1;
	if (rename(tmp, path) != 0)
		goto fail;

	/* The rename is only durable once the DIRECTORY entry is. */
	kb_strlcpy(dirbuf, path, sizeof(dirbuf));
	slash = strrchr(dirbuf, '/');
	if (slash) {
		*slash = 0;
		dfd = open(slash == dirbuf ? "/" : dirbuf,
			   O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (dfd >= 0) {
			fsync(dfd);
			close(dfd);
		}
	}
	return 0;
fail:
	if (fd >= 0)
		close(fd);
	unlink(tmp);
	return -1;
}

int kb_path_exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0;
}

int kb_have_prog(const char *name)
{
	if (strchr(name, '/'))
		return access(name, X_OK) == 0;

	const char *path = getenv("PATH");
	if (!path || !*path)
		path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";

	char buf[512];
	while (*path) {
		const char *end = strchr(path, ':');
		size_t len = end ? (size_t)(end - path) : strlen(path);
		if (len && len + strlen(name) + 2 < sizeof(buf)) {
			memcpy(buf, path, len);
			buf[len] = '/';
			strcpy(buf + len + 1, name);
			if (access(buf, X_OK) == 0)
				return 1;
		}
		if (!end)
			break;
		path = end + 1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

const char *kb_runtime_dir(void)
{
	const char *d = getenv("XDG_RUNTIME_DIR");
	return (d && *d) ? d : "/tmp";
}

const char *kb_home_dir(void)
{
	const char *d = getenv("HOME");
	return (d && *d) ? d : "/root";
}

char *kb_path_join(const char *a, const char *b)
{
	size_t n = strlen(a) + strlen(b) + 2;
	char *p = kb_calloc(1, n);
	snprintf(p, n, "%s/%s", a, b);
	return p;
}

int kb_mkdir_p(const char *path)
{
	char *tmp = kb_strdup(path);
	char *p;
	for (p = tmp + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
			free(tmp);
			return -1;
		}
		*p = '/';
	}
	if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
		free(tmp);
		return -1;
	}
	free(tmp);
	return 0;
}

int kb_lock_file(const char *path, int nonblock)
{
	int fd = open(path, O_WRONLY | O_CREAT, 0600);
	if (fd < 0)
		return -1;
	if (flock(fd, LOCK_EX | (nonblock ? LOCK_NB : 0)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}
