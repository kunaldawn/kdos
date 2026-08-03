/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — helpers, palettes
 * ---------------------------------
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kinstall.h"

/* The same table `kdos theme` carries, in the same field order:
 * primary dim secondary urgent deep text variant primary_dark backdrop.
 * One palette for the distro; the installer is not allowed its own taste. */
#define RGB(x) { (uint8_t)(0x##x >> 16), (uint8_t)((0x##x >> 8) & 0xff), (uint8_t)(0x##x & 0xff) }

const Theme ki_themes[] = {
	{ "phosphor", "PHOSPHOR", {
		[CL_BG] = RGB(02120a), [CL_ERR] = RGB(ff3131),
		[CL_ACCENT] = RGB(39ff14), [CL_WARN] = RGB(ffb000),
		[CL_DIM] = RGB(12401f), [CL_MID] = RGB(1f8f0c),
		[CL_SURFACE] = RGB(04120a), [CL_TEXT] = RGB(b8ffc8) } },
	{ "amber", "AMBER", {
		[CL_BG] = RGB(140b02), [CL_ERR] = RGB(ff3131),
		[CL_ACCENT] = RGB(ffb000), [CL_WARN] = RGB(ff6b1a),
		[CL_DIM] = RGB(4a2f00), [CL_MID] = RGB(8a5f00),
		[CL_SURFACE] = RGB(160d02), [CL_TEXT] = RGB(ffd9a0) } },
	{ "ice", "ICE", {
		[CL_BG] = RGB(021a26), [CL_ERR] = RGB(ff4d6d),
		[CL_ACCENT] = RGB(4dd7ff), [CL_WARN] = RGB(7aa2ff),
		[CL_DIM] = RGB(123a4a), [CL_MID] = RGB(1f7fa0),
		[CL_SURFACE] = RGB(031420), [CL_TEXT] = RGB(bfeeff) } },
	{ "bone", "BONE", {
		[CL_BG] = RGB(161512), [CL_ERR] = RGB(ff5a5a),
		[CL_ACCENT] = RGB(f4f0e2), [CL_WARN] = RGB(b9b3a1),
		[CL_DIM] = RGB(3a3833), [CL_MID] = RGB(8f8b7e),
		[CL_SURFACE] = RGB(141412), [CL_TEXT] = RGB(ded9cb) } },
};

int ki_ntheme = (int)(sizeof(ki_themes) / sizeof(ki_themes[0]));
const Theme *ki_theme = &ki_themes[0];

int theme_set(const char *name)
{
	for (int i = 0; i < ki_ntheme; i++) {
		if (!strcmp(ki_themes[i].name, name)) {
			ki_theme = &ki_themes[i];
			return 0;
		}
	}
	return -1;
}

/* ──────────────────────────────────────────────────────────────────────── */

double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

void *xcalloc(size_t n, size_t sz)
{
	void *p = calloc(n, sz);
	if (!p) {
		term_shutdown();
		fprintf(stderr, "kinstall: out of memory\n");
		_exit(1);
	}
	return p;
}

char *xstrdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = xcalloc(1, n);
	memcpy(p, s, n);
	return p;
}

void strlcpy_(char *d, const char *s, size_t n)
{
	if (!n)
		return;
	size_t i = 0;
	for (; i + 1 < n && s[i]; i++)
		d[i] = s[i];
	d[i] = 0;
}

int str_ieq(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

const char *basename_(const char *p)
{
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}

int read_file(const char *path, char *buf, size_t cap)
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

/* First line, newline stripped. */
int read_line_file(const char *path, char *buf, size_t cap)
{
	if (read_file(path, buf, cap) < 0)
		return -1;
	char *nl = strchr(buf, '\n');
	if (nl)
		*nl = 0;
	return (int)strlen(buf);
}

int write_file(const char *path, const char *data)
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

int path_exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0;
}

int have_prog(const char *name)
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
