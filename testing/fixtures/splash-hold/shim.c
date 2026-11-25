/*
 * A display for kdos-splash's hand-off hold, preloaded into `kdos-splash run`
 * and its clients by selftest.sh.
 *
 * Every path under /dev resolves in $SPLASH_DEV, and the splash's font is
 * $SPLASH_FONT. fb0 there is a plain file the splash maps; tty0 and tty1 are
 * plain files whose ioctls are answered here:
 *
 *   $SPLASH_DEV/master   a compositor is DRM master: a pan is refused with
 *                        EBUSY and the VTs are in VT_PROCESS mode
 *   $SPLASH_DEV/text     the VTs are in KD_TEXT
 *   $SPLASH_DEV/text-after-master
 *                        the VTs are in KD_TEXT once a pan has been refused
 *                        with EBUSY, as when a login typed before the
 *                        margin ends the greeter's session
 *   $SPLASH_DEV/dri/card0
 *                        a DRM card with one CRTC, lit by fbdev's
 *                        framebuffer; with $SPLASH_DEV/frame present it
 *                        scans out another from its second read on, as when
 *                        the compositor's first frame replaces fbdev's
 *   a tty descriptor whose file is no longer the one its name opens (the test
 *   renames it and creates a new one) is hung up: every ioctl on it is EIO,
 *   as on a VT whose session start hung it up.
 *
 * $SPLASH_TIMESCALE runs CLOCK_MONOTONIC that many times faster and divides
 * every usleep by it, so a hold of a minute ends in seconds.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#define FB_W 320
#define FB_H 200

static int busy_answered, crtc_reads;

static const char *map(const char *path, char *buf, size_t n)
{
	const char *dev = getenv("SPLASH_DEV"), *font = getenv("SPLASH_FONT");
	if (!path)
		return path;
	if (dev && (!strcmp(path, "/dev") || !strncmp(path, "/dev/", 5))) {
		snprintf(buf, n, "%s%s", dev, path + 4);
		return buf;
	}
	if (font && !strcmp(path, "/usr/share/kdos/splash.psf"))
		return font;
	return path;
}

static int devfile(const char *name)
{
	char p[4096];
	snprintf(p, sizeof(p), "%s/%s", getenv("SPLASH_DEV"), name);
	return access(p, F_OK) == 0;
}

int open(const char *path, int flags, ...)
{
	static int (*real)(const char *, int, ...);
	char buf[4096];
	mode_t mode = 0;
	if (!real)
		real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
	if (flags & (O_CREAT | O_TMPFILE)) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return real(map(path, buf, sizeof(buf)), flags, mode);
}

int open64(const char *path, int flags, ...)
{
	mode_t mode = 0;
	if (flags & (O_CREAT | O_TMPFILE)) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return open(path, flags, mode);
}

int __open_2(const char *path, int flags) { return open(path, flags); }
int __open64_2(const char *path, int flags) { return open(path, flags); }

FILE *fopen(const char *path, const char *mode)
{
	static FILE *(*real)(const char *, const char *);
	char buf[4096];
	if (!real)
		real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
	return real(map(path, buf, sizeof(buf)), mode);
}

FILE *fopen64(const char *path, const char *mode) { return fopen(path, mode); }

int mkfifo(const char *path, mode_t mode)
{
	static int (*real)(const char *, mode_t);
	char buf[4096];
	if (!real)
		real = (int (*)(const char *, mode_t))dlsym(RTLD_NEXT, "mkfifo");
	return real(map(path, buf, sizeof(buf)), mode);
}

int unlink(const char *path)
{
	static int (*real)(const char *);
	char buf[4096];
	if (!real)
		real = (int (*)(const char *))dlsym(RTLD_NEXT, "unlink");
	return real(map(path, buf, sizeof(buf)));
}

static double scale(void)
{
	const char *s = getenv("SPLASH_TIMESCALE");
	double v = s ? atof(s) : 1.0;
	return v > 0.0 ? v : 1.0;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
	static int (*real)(clockid_t, struct timespec *);
	static double base = -1.0;
	if (!real)
		real = (int (*)(clockid_t, struct timespec *))dlsym(RTLD_NEXT, "clock_gettime");
	int r = real(id, ts);
	if (r != 0 || id != CLOCK_MONOTONIC)
		return r;
	double t = ts->tv_sec + ts->tv_nsec / 1e9;
	if (base < 0.0)
		base = t;
	t = base + (t - base) * scale();
	ts->tv_sec = (time_t)t;
	ts->tv_nsec = (long)((t - (double)ts->tv_sec) * 1e9);
	return 0;
}

int usleep(useconds_t us)
{
	struct timespec ts;
	double s = us / 1e6 / scale();
	ts.tv_sec = (time_t)s;
	ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
	return nanosleep(&ts, NULL);
}

/* The file a descriptor holds: "fb0", "tty0", "tty1", "dri/card0" or "", and whether the
 * name now opens another file (the tty was hung up). */
static const char *kind(int fd, int *hungup)
{
	char link[64], target[4096], now[4096];
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	ssize_t n = readlink(link, target, sizeof(target) - 1);
	*hungup = 0;
	if (n <= 0)
		return "";
	target[n] = '\0';
	const char *dev = getenv("SPLASH_DEV");
	size_t dl = dev ? strlen(dev) : 0;
	if (!dev || strncmp(target, dev, dl) || target[dl] != '/')
		return "";
	const char *base = target + dl + 1;
	static const char *names[] = { "fb0", "tty0", "tty1", "dri/card0" };
	for (size_t i = 0; i < 4; i++) {
		size_t l = strlen(names[i]);
		if (strncmp(base, names[i], l) || (base[l] && base[l] != '.'))
			continue;
		struct stat held, named;
		snprintf(now, sizeof(now), "%s/%s", dev, names[i]);
		if (fstat(fd, &held) == 0 &&
		    (stat(now, &named) != 0 || named.st_ino != held.st_ino))
			*hungup = 1;
		return names[i];
	}
	return "";
}

int ioctl(int fd, unsigned long req, ...)
{
	static int (*real)(int, unsigned long, ...);
	va_list ap;
	va_start(ap, req);
	void *arg = va_arg(ap, void *);
	va_end(ap);
	if (!real)
		real = (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");

	int hungup;
	const char *k = kind(fd, &hungup);
	unsigned int r = (unsigned int)req;

	if (!strcmp(k, "fb0")) {
		if (r == FBIOGET_VSCREENINFO) {
			struct fb_var_screeninfo *v = arg;
			memset(v, 0, sizeof(*v));
			v->xres = v->xres_virtual = FB_W;
			v->yres = v->yres_virtual = FB_H;
			v->bits_per_pixel = 32;
			v->red.offset = 16;   v->red.length = 8;
			v->green.offset = 8;  v->green.length = 8;
			v->blue.offset = 0;   v->blue.length = 8;
			return 0;
		}
		if (r == FBIOGET_FSCREENINFO) {
			struct fb_fix_screeninfo *f = arg;
			memset(f, 0, sizeof(*f));
			f->line_length = FB_W * 4;
			f->smem_len = FB_W * 4 * FB_H;
			return 0;
		}
		if (r == FBIOPAN_DISPLAY) {
			if (devfile("master")) {
				busy_answered = 1;
				errno = EBUSY;
				return -1;
			}
			return 0;
		}
		errno = EINVAL;
		return -1;
	}
	if (!strcmp(k, "dri/card0")) {
		if (r == (unsigned int)DRM_IOCTL_MODE_GETRESOURCES) {
			struct drm_mode_card_res *res = arg;
			if (res->count_crtcs >= 1 && res->crtc_id_ptr)
				*(uint32_t *)(uintptr_t)res->crtc_id_ptr = 31;
			res->count_crtcs = 1;
			res->count_fbs = res->count_connectors = res->count_encoders = 0;
			return 0;
		}
		if (r == (unsigned int)DRM_IOCTL_MODE_GETCRTC) {
			struct drm_mode_crtc *c = arg;
			c->fb_id = crtc_reads++ > 0 && devfile("frame") ? 2 : 1;
			return 0;
		}
		errno = EINVAL;
		return -1;
	}
	if (*k) {	/* tty0, tty1 */
		if (hungup) {
			errno = EIO;
			return -1;
		}
		if (r == KDGETMODE) {
			*(int *)arg = devfile("text") ||
				      (busy_answered && devfile("text-after-master"))
				      ? KD_TEXT : KD_GRAPHICS;
			return 0;
		}
		if (r == VT_GETMODE) {
			struct vt_mode *m = arg;
			memset(m, 0, sizeof(*m));
			m->mode = devfile("master") ? VT_PROCESS : VT_AUTO;
			return 0;
		}
		return 0;
	}
	return real(fd, req, arg);
}
