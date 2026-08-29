/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-devices — the camera, the microphone, and what is plugged in
 *
 *   ╔═ devices ═══════════════════════════════════════════════════════╗
 *   ║ CAMERAS                                                         ║
 *   ║ ▶ /dev/video0  Integrated Camera   uvcvideo    free             ║
 *   ║   /dev/video2  USB Camera          uvcvideo    IN USE by firefox║
 *   ║ MICROPHONES                                              muted  ║
 *   ║   hw:0  HDA Intel PCH             capture 62%                   ║
 *   ║ INPUT                                                           ║
 *   ║   AT Translated Set 2 keyboard                                  ║
 *   ╟─────────────────────────────────────────────────────────────────╢
 *   ║ p preview   m mute all mics   Esc                               ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * KDE has no good answer for this either, which is why it is here. The panel
 * already tells you WHICH application is holding the camera (privacy.c); this
 * is the surface that tells you which cameras exist, whether they work, and
 * lets you shut every microphone on the machine off with one key.
 *
 * NO LIBRARY. V4L2 is a set of ioctls on /dev/videoN and the kernel uapi
 * header is the API — v4l-utils would be a port for a struct definition. Who
 * is HOLDING a camera comes from walking the fd links under /proc, the way
 * privacy.c's camera half already does: almost nothing takes a webcam through
 * the portal, so PipeWire would report nothing at all.
 *
 * THE PREVIEW IS ASCII, and that is not a joke — it is libkcell's shape-vector
 * renderer (`kcell_ascii_image`), the same engine behind `Super+A` and
 * `kdos-shot --text`, pointed at one grabbed frame. A camera test that draws
 * the camera as characters on a phosphor desktop is the most in-character
 * thing in this program and it costs about forty lines, because the renderer
 * was already here.
 *
 * OPENING A CAMERA TO PREVIEW IT *IS* USING IT. The privacy lamp lights for
 * this program exactly as it would for anything else, the fd is closed the
 * moment the frame is taken, and there is no continuous preview — a device
 * manager that quietly held a webcam open would be the precise thing
 * privacy.c exists to expose.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "kcell.h"
#include "kwl.h"
#include "shell.h"

#define DV_COLS 76
#define DV_ROWS 26
#define DV_MAX_CAM 8
#define DV_MAX_MIC 8
#define DV_MAX_INPUT 16
#define DV_MAX_MEDIA 16
#define DV_NAME 64
#define DV_MOUNTD_SOCKET "/run/kdos-mountd.sock"
/*
 * A `/dev` node's path. Sized for what a V4L2 node actually is — `/dev/video0`
 * — rather than for what readdir may return, because widening it pushes
 * 260-byte paths through every row and label in this file for a name that
 * cannot occur. The scan REFUSES a name that would not fit instead; see
 * scan_cams(). One definition, because the scan buffer and the row that keeps
 * the result have to agree.
 */
#define DV_DEVPATH 32
#define DV_DEVNAME (int)(DV_DEVPATH - sizeof("/dev/"))

struct dv_cam {
	char path[DV_DEVPATH];
	char name[DV_NAME];
	char driver[24];
	char holder[DV_NAME];	/* empty when nothing has it open */
	int holder_pid;
};

struct dv_mic {
	char id[16];
	char name[DV_NAME];
};

struct dv_input {
	char name[DV_NAME];
	char kind[16];
};

/*
 * A removable filesystem, as kdos-mountd reports it. The INDEX is the daemon's
 * own row number and is the only thing sent back — this program never names a
 * device or a mountpoint, because the protocol has no way to say one.
 */
struct dv_media {
	int idx;
	char kname[32];
	char label[DV_NAME];
	char fstype[24];
	char size[16];
	char mnt[256];
};

static struct dv_cam cams[DV_MAX_CAM];
static int ncam;
static struct dv_mic mics[DV_MAX_MIC];
static int nmic;
static struct dv_input inputs[DV_MAX_INPUT];
static int ninput;
static struct dv_media media[DV_MAX_MEDIA];
static int nmedia;
static char media_why[96];
/* What `kdos app update --check` said, once per refresh: a stick that IS
 * newer than the disk is the offline update story, and it had no surface. */
static char updates_line[96];
static int updates_n;
static int sel;
static char status[128];

/* The preview, as cells. Held until something else is previewed or Esc. */
#define PV_MAX (64 * 32)
static uint32_t pv_cp[PV_MAX];
static uint32_t pv_tint[PV_MAX];
static int pv_cols, pv_rows;
static char pv_from[32];

/* `$KDOS_PRIVACY_PROC` moves the /proc walk, the same seam privacy.c uses —
 * which is what makes the "who holds the camera" half testable on a machine
 * with no camera. */
static const char *proc_root(void)
{
	const char *p = getenv("KDOS_PRIVACY_PROC");
	return p && *p ? p : "/proc";
}

/* ── who is holding it ─────────────────────────────────────────────────── */

static void find_holder(struct dv_cam *c)
{
	char dpath[512];
	DIR *d = opendir(proc_root());
	struct dirent *e;

	c->holder[0] = '\0';
	c->holder_pid = 0;
	if (!d)
		return;
	while ((e = readdir(d)) && !c->holder[0]) {
		if (e->d_name[0] < '0' || e->d_name[0] > '9')
			continue;
		int pid = atoi(e->d_name);
		snprintf(dpath, sizeof(dpath), "%s/%s/fd", proc_root(),
			 e->d_name);
		DIR *fd = opendir(dpath);
		struct dirent *f;
		if (!fd)
			continue;
		while ((f = readdir(fd))) {
			/* A directory plus a name is a path, so it is sized
			 * as one: `dpath` is itself built from sysfs and a
			 * short buffer here silently compares the wrong
			 * link against the device it is looking for. */
			char link[PATH_MAX], target[PATH_MAX];
			ssize_t n;

			if (f->d_name[0] == '.')
				continue;
			snprintf(link, sizeof(link), "%s/%s", dpath, f->d_name);
			n = readlink(link, target, sizeof(target) - 1);
			if (n <= 0)
				continue;
			target[n] = '\0';
			if (strcmp(target, c->path))
				continue;
			/* The process's own comm — the same answer privacy.c
			 * gives for a camera, where there is no application
			 * name to be had. */
			char cpath[600], comm[DV_NAME] = "";
			snprintf(cpath, sizeof(cpath), "%s/%d/comm",
				 proc_root(), pid);
			if (sh_read_line(cpath, comm, sizeof(comm)) == 0 &&
			    comm[0])
				snprintf(c->holder, sizeof(c->holder), "%s",
					 comm);
			else
				snprintf(c->holder, sizeof(c->holder), "pid %d",
					 pid);
			c->holder_pid = pid;
			break;
		}
		closedir(fd);
	}
	closedir(d);
}

/* ── the camera list ───────────────────────────────────────────────────── */

static void scan_cameras(void)
{
	DIR *d = opendir("/dev");
	struct dirent *e;

	ncam = 0;
	if (!d)
		return;
	while ((e = readdir(d)) && ncam < DV_MAX_CAM) {
		if (strncmp(e->d_name, "video", 5))
			continue;
		/*
		 * REFUSED, NOT TRUNCATED — kdos-packd's rule, and it bites
		 * harder here: a truncated name still open()s, just the WRONG
		 * node, so the preview shows a device nobody asked for. The
		 * explicit precision is what lets the compiler see that the
		 * copy fits; the refusal above it is what makes that true
		 * rather than merely quiet.
		 */
		char path[DV_DEVPATH];

		if (strlen(e->d_name) > (size_t)DV_DEVNAME)
			continue;
		snprintf(path, sizeof(path), "/dev/%.*s", DV_DEVNAME,
			 e->d_name);

		int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		struct v4l2_capability cap;
		memset(&cap, 0, sizeof(cap));
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
			uint32_t caps = cap.capabilities & V4L2_CAP_DEVICE_CAPS
						? cap.device_caps
						: cap.capabilities;
			/*
			 * A UVC camera exposes TWO nodes — the capture one and
			 * a metadata one — and listing the metadata node as a
			 * camera is a row that can never show a picture.
			 */
			if (caps & V4L2_CAP_VIDEO_CAPTURE) {
				struct dv_cam *c = &cams[ncam++];
				memset(c, 0, sizeof(*c));
				snprintf(c->path, sizeof(c->path), "%s", path);
				snprintf(c->name, sizeof(c->name), "%s",
					 (const char *)cap.card);
				snprintf(c->driver, sizeof(c->driver), "%s",
					 (const char *)cap.driver);
			}
		}
		close(fd);
	}
	closedir(d);
	for (int i = 0; i < ncam; i++)
		find_holder(&cams[i]);
}

/* ── one frame, as characters ──────────────────────────────────────────── */

static void yuyv_to_argb(const uint8_t *src, uint32_t *dst, int w, int h)
{
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x += 2) {
			const uint8_t *p = src + (size_t)(y * w + x) * 2;
			int y0 = p[0], u = p[1] - 128, y1 = p[2], v = p[3] - 128;

			for (int k = 0; k < 2; k++) {
				int yy = k ? y1 : y0;
				int r = yy + ((91881 * v) >> 16);
				int g = yy - ((22554 * u + 46802 * v) >> 16);
				int b = yy + ((116130 * u) >> 16);
				r = r < 0 ? 0 : r > 255 ? 255 : r;
				g = g < 0 ? 0 : g > 255 ? 255 : g;
				b = b < 0 ? 0 : b > 255 ? 255 : b;
				dst[(size_t)y * w + x + k] =
					0xff000000u | ((uint32_t)r << 16) |
					((uint32_t)g << 8) | (uint32_t)b;
			}
		}
}

static void grey_to_argb(const uint8_t *src, uint32_t *dst, int w, int h)
{
	for (int i = 0; i < w * h; i++) {
		uint32_t g = src[i];
		dst[i] = 0xff000000u | (g << 16) | (g << 8) | g;
	}
}

static void rgb24_to_argb(const uint8_t *src, uint32_t *dst, int w, int h)
{
	for (int i = 0; i < w * h; i++)
		dst[i] = 0xff000000u | ((uint32_t)src[i * 3] << 16) |
			 ((uint32_t)src[i * 3 + 1] << 8) |
			 (uint32_t)src[i * 3 + 2];
}

/*
 * Grab exactly one frame and turn it into cells.
 *
 * Three pixel formats and no more: YUYV is what essentially every UVC webcam
 * gives, GREY is what an IR sensor gives, and RGB24 is what a virtual device
 * gives. MJPEG is deliberately NOT decoded — that would be a JPEG decoder in
 * a device manager, and a camera that only offers MJPEG says so in the status
 * line instead of showing a wrong picture.
 */
static void preview(struct dv_cam *c)
{
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	void *mem[2] = { NULL, NULL };
	size_t len[2] = { 0, 0 };
	uint32_t *argb = NULL;
	int fd, type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int w = 320, h = 240;

	pv_cols = pv_rows = 0;
	fd = open(c->path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		snprintf(status, sizeof(status), "%s: cannot open (in use?)",
			 c->path);
		return;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) {
		snprintf(status, sizeof(status), "%s: no format we can read",
			 c->path);
		goto out;
	}
	w = (int)fmt.fmt.pix.width;
	h = (int)fmt.fmt.pix.height;
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV &&
	    fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_GREY &&
	    fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
		snprintf(status, sizeof(status),
			 "%s offers only compressed frames — no preview",
			 c->path);
		goto out;
	}

	memset(&req, 0, sizeof(req));
	req.count = 2;
	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 1) {
		snprintf(status, sizeof(status), "%s: no buffers", c->path);
		goto out;
	}

	for (unsigned i = 0; i < req.count && i < 2; i++) {
		memset(&buf, 0, sizeof(buf));
		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0)
			goto out;
		mem[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
			      MAP_SHARED, fd, (off_t)buf.m.offset);
		len[i] = buf.length;
		if (mem[i] == MAP_FAILED) {
			mem[i] = NULL;
			goto out;
		}
		if (ioctl(fd, VIDIOC_QBUF, &buf) != 0)
			goto out;
	}
	if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
		snprintf(status, sizeof(status), "%s: will not stream",
			 c->path);
		goto out;
	}

	/* Two seconds, then give up. A camera that needs longer than that to
	 * hand over its first frame is a camera something else is using. */
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	if (poll(&pfd, 1, 2000) <= 0) {
		snprintf(status, sizeof(status), "%s: no frame in 2s", c->path);
		ioctl(fd, VIDIOC_STREAMOFF, &type);
		goto out;
	}

	memset(&buf, 0, sizeof(buf));
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_DQBUF, &buf) == 0 && buf.index < 2 &&
	    mem[buf.index]) {
		argb = malloc((size_t)w * h * 4);
		if (argb) {
			if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
				yuyv_to_argb(mem[buf.index], argb, w, h);
			else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
				grey_to_argb(mem[buf.index], argb, w, h);
			else
				rgb24_to_argb(mem[buf.index], argb, w, h);

			/*
			 * Cells that are twice as tall as they are wide, so the
			 * sampling grid has to be about twice as wide as it is
			 * tall or the picture stretches — the same constraint
			 * genlogo.py keeps for the mascot.
			 */
			int cw = w / 48, ch = h / 18;
			if (cw < 1)
				cw = 1;
			if (ch < 1)
				ch = 1;
			if (kcell_ascii_image(argb, w, h, w, cw, ch, pv_cp,
					      pv_tint, &pv_cols,
					      &pv_rows) != 0) {
				pv_cols = pv_rows = 0;
				snprintf(status, sizeof(status),
					 "no font: the preview needs one");
			} else {
				snprintf(pv_from, sizeof(pv_from), "%s",
					 c->path);
				snprintf(status, sizeof(status),
					 "%s: %dx%d frame", c->path, w, h);
			}
		}
	}
	ioctl(fd, VIDIOC_STREAMOFF, &type);
out:
	free(argb);
	for (int i = 0; i < 2; i++)
		if (mem[i])
			munmap(mem[i], len[i]);
	close(fd);
}

/* ── microphones and input devices ─────────────────────────────────────── */

/*
 * The capture cards, from /proc/asound/cards — the same file `aplay -l` reads
 * and one this program can read without linking ALSA a second time. osd.c owns
 * the mixer and this owns the LIST.
 */
static void scan_mics(void)
{
	FILE *f = fopen("/proc/asound/cards", "r");
	char line[256];

	nmic = 0;
	if (!f)
		return;
	while (fgets(line, sizeof(line), f) && nmic < DV_MAX_MIC) {
		int idx = -1;
		char rest[200] = "";

		/* ` 0 [PCH            ]: HDA-Intel - HDA Intel PCH` */
		if (sscanf(line, " %d [%*[^]]]: %199[^\n]", &idx, rest) != 2)
			continue;
		struct dv_mic *m = &mics[nmic++];
		snprintf(m->id, sizeof(m->id), "hw:%d", idx);
		snprintf(m->name, sizeof(m->name), "%s", rest);
	}
	fclose(f);
}

/*
 * /proc/bus/input/devices, read for its names and what each device can do.
 * READ ONLY: the knobs that matter (tap-to-click, natural scrolling, pointer
 * acceleration) are labwc's `<libinput>` block in rc.xml, and a second place
 * to set them would be a second answer.
 */
static void scan_inputs(void)
{
	FILE *f = fopen("/proc/bus/input/devices", "r");
	char line[512], name[DV_NAME] = "";

	ninput = 0;
	if (!f)
		return;
	while (fgets(line, sizeof(line), f) && ninput < DV_MAX_INPUT) {
		if (!strncmp(line, "N: Name=", 8)) {
			char *q = strchr(line + 8, '"');
			if (q) {
				char *e = strrchr(q + 1, '"');
				if (e)
					*e = '\0';
				snprintf(name, sizeof(name), "%s", q + 1);
			}
		} else if (!strncmp(line, "H: Handlers=", 12) && name[0]) {
			struct dv_input *d = &inputs[ninput++];
			snprintf(d->name, sizeof(d->name), "%s", name);
			if (strstr(line, "mouse"))
				snprintf(d->kind, sizeof(d->kind), "pointer");
			else if (strstr(line, "kbd"))
				snprintf(d->kind, sizeof(d->kind), "keyboard");
			else
				snprintf(d->kind, sizeof(d->kind), "input");
			name[0] = '\0';
		}
	}
	fclose(f);
}

/* ── removable media, through kdos-mountd ──────────────────────────────────
 *
 * A SHORT connection per request, from a surface that is up for as long as
 * somebody is looking at it. The panel deliberately has no media widget yet:
 * the rule there is that nothing blocks the frame, and a socket round trip per
 * tick is exactly what that rule is about. Here the surface is already waiting
 * for a keystroke.
 */
static int mountd_ask(const char *req, char *out, size_t n)
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	const char *path = getenv("KDOS_MOUNTD_SOCKET");
	size_t got = 0;
	ssize_t r;

	out[0] = '\0';
	if (fd < 0)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
		 path && *path ? path : DV_MOUNTD_SOCKET);
	/* A one-second ceiling on a local socket that answers in microseconds:
	 * the daemon is a `scan()` of a handful of file reads, and anything
	 * slower than this is a daemon that is wedged. */
	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", req);
	while (got + 1 < n && (r = read(fd, out + got, n - got - 1)) > 0)
		got += (size_t)r;
	out[got] = '\0';
	close(fd);
	return 0;
}

static void scan_updates(void);

static void scan_media(void)
{
	char buf[8192];

	nmedia = 0;
	media_why[0] = '\0';
	if (mountd_ask("list", buf, sizeof(buf)) != 0) {
		snprintf(media_why, sizeof(media_why),
			 "kdos-mountd is not running (service start 58_mountd)");
		return;
	}
	for (char *p = buf; *p && nmedia < DV_MAX_MEDIA;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		struct dv_media *m = &media[nmedia];
		memset(m, 0, sizeof(*m));
		/* `index\tkname\tlabel\tfstype\tsize\tmount`, and a `-` where
		 * the daemon had nothing to say. */
		if (sscanf(p, "%d\t%31[^\t]\t%63[^\t]\t%23[^\t]\t%15[^\t]\t%255[^\n]",
			   &m->idx, m->kname, m->label, m->fstype, m->size,
			   m->mnt) >= 5) {
			if (!strcmp(m->label, "-"))
				m->label[0] = '\0';
			if (!strcmp(m->mnt, "-"))
				m->mnt[0] = '\0';
			nmedia++;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
}

static void media_action(int i, const char *verb)
{
	char req[64], buf[512];

	if (i < 0 || i >= nmedia)
		return;
	snprintf(req, sizeof(req), "%s %d", verb, media[i].idx);
	if (mountd_ask(req, buf, sizeof(buf)) != 0) {
		snprintf(status, sizeof(status), "kdos-mountd is not running");
		return;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	snprintf(status, sizeof(status), "%.100s", buf);
	scan_media();
	scan_updates();
}

/*
 * A STICK THAT IS NEWER THAN THE DISK. `kdos app update --check` answers in
 * one line and an exit status; it reads the medium's own index and costs one
 * daemon round trip, which is fine for a program that is already waiting for a
 * keystroke and would not be fine on a panel tick. Run once per refresh.
 */
static void scan_updates(void)
{
	char buf[256];
	KbArgv a = {0};

	updates_line[0] = 0;
	updates_n = 0;
	kb_argv_add(&a, "kdos");
	kb_argv_add(&a, "app");
	kb_argv_add(&a, "update");
	kb_argv_add(&a, "--check");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) < 0)
		return;
	buf[strcspn(buf, "\r\n")] = 0;
	if (sscanf(buf, "%d update", &updates_n) != 1)
		updates_n = 0;
	if (updates_n > 0)
		snprintf(updates_line, sizeof(updates_line), "%s — Enter to apply", buf);
}

/* ── the row list ──────────────────────────────────────────────────────── */

enum { R_HEAD = 0, R_CAM, R_MIC, R_INPUT, R_MEDIA, R_UPDATE };

struct drow {
	int kind;
	int idx;
	const char *head;
};

static struct drow rows[2 + DV_MAX_CAM + DV_MAX_MIC + DV_MAX_INPUT +
			DV_MAX_MEDIA + 6];
static int nrows;

static void build_rows(void)
{
	nrows = 0;
	rows[nrows].kind = R_HEAD;
	rows[nrows++].head = "CAMERAS";
	for (int i = 0; i < ncam; i++) {
		rows[nrows].kind = R_CAM;
		rows[nrows++].idx = i;
	}
	rows[nrows].kind = R_HEAD;
	rows[nrows++].head = "MICROPHONES";
	for (int i = 0; i < nmic; i++) {
		rows[nrows].kind = R_MIC;
		rows[nrows++].idx = i;
	}
	rows[nrows].kind = R_HEAD;
	rows[nrows++].head = "REMOVABLE MEDIA";
	for (int i = 0; i < nmedia; i++) {
		rows[nrows].kind = R_MEDIA;
		rows[nrows++].idx = i;
	}
	rows[nrows].kind = R_HEAD;
	rows[nrows++].head = "UPDATES ON THE MEDIUM";
	if (updates_n > 0) {
		rows[nrows].kind = R_UPDATE;
		rows[nrows++].idx = 0;
	}
	rows[nrows].kind = R_HEAD;
	rows[nrows++].head = "INPUT";
	for (int i = 0; i < ninput; i++) {
		rows[nrows].kind = R_INPUT;
		rows[nrows++].idx = i;
	}
	if (sel >= nrows)
		sel = nrows - 1;
	if (sel < 0)
		sel = 0;
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	int pv_w = pv_cols && pv_rows ? pv_cols + 2 : 0;
	int list_w = pv_w ? w - pv_w - 1 : w;
	int body = h - 4;

	if (w < 30 || h < 8)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "devices", KT_ACCENT, KT_BG, 1);

	for (int i = 0; i < body && i < nrows; i++) {
		const struct drow *r = &rows[i];
		int y = 1 + i;
		int on = i == sel && r->kind != R_HEAD;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		if (r->kind == R_HEAD) {
			ktui_draw_text(2, y, list_w - 4, r->head, KT_ACCENT,
				       KT_BG, KT_A_NONE);
			/*
			 * An empty section with nothing beside it is
			 * indistinguishable from a section that failed to
			 * read. Every one of these says which it is.
			 */
			const char *empty = NULL;
			if (!strcmp(r->head, "CAMERAS") && !ncam)
				/* Short enough for the POPUP form: the panel
				 * opens this at fifty-six columns and the
				 * section text is drawn at column twenty, so
				 * a sentence here comes out as `can ca`. */
				empty = "none — no /dev/video device";
			else if (!strcmp(r->head, "MICROPHONES") && !nmic)
				empty = "none — no sound card is present";
			else if (!strcmp(r->head, "REMOVABLE MEDIA") && !nmedia)
				empty = media_why[0] ? media_why
						     : "nothing plugged in";
			else if (!strcmp(r->head, "UPDATES ON THE MEDIUM") && !updates_n)
				empty = "up to date";
			else if (!strcmp(r->head, "INPUT") && !ninput)
				empty = "none";
			if (empty)
				ktui_draw_text(20, y, list_w - 22, empty,
					       media_why[0] &&
						       !strcmp(r->head,
							       "REMOVABLE MEDIA")
					       ? KT_ERR : KT_DIM,
					       KT_BG, KT_A_NONE);
			continue;
		}
		ktui_draw_fill(krect(1, y, list_w - 2, 1), bg);
		if (r->kind == R_CAM) {
			const struct dv_cam *c = &cams[r->idx];
			ktui_draw_text(3, y, 14, c->path, fg, bg, KT_A_NONE);
			ktui_draw_text(18, y, 24, c->name, fg, bg, KT_A_NONE);
			ktui_draw_text(43, y, 10, c->driver,
				       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
			if (c->holder[0])
				ktui_draw_textf(54, y, list_w - 56,
						on ? KT_SURFACE : KT_WARN, bg,
						KT_A_NONE, "in use by %s",
						c->holder);
			else
				ktui_draw_text(54, y, 8, "free",
					       on ? KT_SURFACE : KT_MID, bg,
					       KT_A_NONE);
		} else if (r->kind == R_MIC) {
			const struct dv_mic *m = &mics[r->idx];
			ktui_draw_text(3, y, 8, m->id, fg, bg, KT_A_NONE);
			ktui_draw_text(12, y, list_w - 24, m->name, fg, bg,
				       KT_A_NONE);
			if (r->idx == 0)
				ktui_draw_text(list_w - 10, y, 8,
					       sh_mic_muted() ? "MUTED"
							      : "live",
					       on	       ? KT_SURFACE
					       : sh_mic_muted() ? KT_WARN
								: KT_MID,
					       bg, KT_A_NONE);
		} else if (r->kind == R_MEDIA) {
			const struct dv_media *m = &media[r->idx];
			ktui_draw_text(3, y, 10, m->kname, fg, bg, KT_A_NONE);
			ktui_draw_text(14, y, 20,
				       m->label[0] ? m->label : "(no label)",
				       fg, bg, KT_A_NONE);
			ktui_draw_text(35, y, 8, m->fstype,
				       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
			ktui_draw_text(44, y, 8, m->size,
				       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
			ktui_draw_text(53, y, list_w - 55,
				       m->mnt[0] ? m->mnt : "not mounted",
				       on	   ? KT_SURFACE
				       : m->mnt[0] ? KT_ACCENT
						   : KT_MID,
				       bg, KT_A_NONE);
		} else if (r->kind == R_UPDATE) {
			ktui_draw_text(3, y, list_w - 5, updates_line,
				       on ? KT_SURFACE : KT_ACCENT, bg, KT_A_NONE);
		} else {
			const struct dv_input *d = &inputs[r->idx];
			ktui_draw_text(3, y, 10, d->kind,
				       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
			ktui_draw_text(14, y, list_w - 16, d->name, fg, bg,
				       KT_A_NONE);
		}
	}

	/* ── the preview pane ── */
	if (pv_w) {
		int x0 = w - pv_w;
		ktui_draw_vline(x0 - 1, 1, h - 4, KT_G_VL, KT_DIM, KT_BG);
		for (int y = 0; y < pv_rows && y + 1 < h - 3; y++)
			for (int x = 0; x < pv_cols && x0 + x < w - 1; x++) {
				size_t o = (size_t)y * pv_cols + x;
				/*
				 * The tint is thrown away and the accent used
				 * instead: eight colour slots is what this
				 * desktop draws in, and a mean-colour image
				 * inside a phosphor frame is the one thing on
				 * screen that would not belong to it.
				 */
				ktui_draw_cell(x0 + x, 1 + y, pv_cp[o],
					       KT_ACCENT, KT_BG, KT_A_NONE);
			}
		ktui_draw_text(x0, h - 4, pv_w, pv_from, KT_MID, KT_BG,
			       KT_A_NONE);
	}

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	ktui_draw_text(2, h - 2, w - 4,
		       status[0] ? status
				 : "Enter mount  u eject  p preview  "
				   "r rescan  Esc",
		       status[0] ? KT_MID : KT_DIM, KT_BG, KT_A_NONE);
	ktui_draw_flush();
}

static void rescan(void)
{
	scan_cameras();
	scan_mics();
	scan_inputs();
	scan_media();
	scan_updates();
	build_rows();
}

int devices_main(int argc, char **argv)
{
	const char *font = NULL;
	int at_x = -1, at_y = 0;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		/* Anchored above the applet that opened it. A panel readout
		 * whose window appears in the middle of the screen reads as a
		 * separate application rather than as part of the bar. */
		if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[i + 1]);
			at_y = atoi(argv[i + 2]);
			i += 2;
		} else
		if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr,
				"usage: kdos-devices [--font NAME] [--dump]\n");
			return 2;
		}
	}

	rescan();
	/* The first selectable row, not the heading above it. */
	sel = ncam ? 1 : 0;

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(DV_COLS, DV_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	/* Anchored means popup, centred means window — see the same block in
	 * net.c, which is where that split is written down. */
	int popup = at_x >= 0;
	KwlConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = popup ? KWL_ROLE_OVERLAY : KWL_ROLE_TOPLEVEL,
		.cols = popup ? 56 : DV_COLS,
		.rows = popup ? 18 : DV_ROWS,
		.corner = popup ? KWL_CORNER_BOTTOM_LEFT : KWL_CORNER_CENTER,
		.margin_x = popup ? at_x : 0,
		.margin_y = popup ? at_y : 0,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Devices",
		.app_id = "kdos-devices",
		.font = font,
		.keyboard = 1,
		.dismiss_on_unfocus = popup,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr,
			"kdos-devices: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_BG);

	while (!kwl_should_close()) {
		sh_theme_poll();
		draw_frame();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type == KT_EVT_MOUSE) {
			int idx = ev.my - 1;
			if (ev.press == KT_MP_DRAG) {
				if (idx >= 0 && idx < nrows &&
				    rows[idx].kind != R_HEAD)
					sel = idx;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			/*
			 * THE WHEEL, which this one surface did not have at
			 * all — the only list on the desktop a pointer could
			 * not move through. The list is short by construction
			 * (the devices this machine has), so it is a cursor
			 * step and never a page scroll; headings are skipped,
			 * as they are for the arrow keys.
			 */
			if (ev.btn == KT_MB_WHEEL_UP) {
				while (sel > 0 && rows[--sel].kind == R_HEAD)
					;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				while (sel + 1 < nrows &&
				       rows[++sel].kind == R_HEAD)
					;
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn == KT_MB_LEFT && idx >= 0 && idx < nrows &&
			    rows[idx].kind != R_HEAD) {
				int was = sel;

				sel = idx;
				if (rows[idx].kind == R_CAM) {
					preview(&cams[rows[idx].idx]);
				} else if (rows[idx].kind == R_MEDIA &&
					   was == idx) {
					/*
					 * A SECOND CLICK ACTIVATES — pick.c's
					 * rule, and here it is what makes a
					 * stick mountable with a pointer at
					 * all: the verb was on a key nobody
					 * is told about. Mount what is not
					 * mounted, open what is, exactly as
					 * Enter does.
					 */
					const struct dv_media *m =
						&media[rows[idx].idx];
					if (!m->mnt[0]) {
						media_action(rows[idx].idx,
							     "mount");
					} else {
						const char *argv[] = {
							"kdos-appbox", "open",
							m->mnt, NULL
						};
						sh_spawn(argv);
					}
				}
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		switch (ev.key) {
		case KT_K_ESC:
			if (pv_cols) {
				pv_cols = pv_rows = 0;
				status[0] = '\0';
				break;
			}
			goto done;
		case KT_K_UP:
			while (sel > 0 && rows[--sel].kind == R_HEAD)
				;
			break;
		case KT_K_DOWN:
			while (sel + 1 < nrows && rows[++sel].kind == R_HEAD)
				;
			break;
		case KT_K_ENTER:
		case 'p':
			if (sel < nrows && rows[sel].kind == R_CAM) {
				preview(&cams[rows[sel].idx]);
			} else if (sel < nrows && rows[sel].kind == R_UPDATE) {
				const char *argv[] = { "foot", "-e", "kdos", "app",
						       "update", NULL };
				sh_spawn(argv);
			} else if (sel < nrows && rows[sel].kind == R_MEDIA) {
				/* Enter is the obvious verb for the state it
				 * is in: mount what is not mounted, open what
				 * is. Ejecting is `u`, because a key that
				 * sometimes unmounts is a key nobody trusts. */
				const struct dv_media *m = &media[rows[sel].idx];
				if (!m->mnt[0]) {
					media_action(rows[sel].idx, "mount");
				} else {
					const char *argv[] = { "kdos-appbox",
							       "open", m->mnt,
							       NULL };
					sh_spawn(argv);
				}
			}
			break;
		case 'u':
			if (sel < nrows && rows[sel].kind == R_MEDIA)
				media_action(rows[sel].idx, "unmount");
			break;
		case 'm':
			sh_mic_toggle();
			snprintf(status, sizeof(status), "%s",
				 sh_mic_muted() ? "every microphone muted"
						: "microphones live");
			break;
		case 'r':
			rescan();
			break;
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	return 0;
}
