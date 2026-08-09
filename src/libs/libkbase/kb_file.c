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
