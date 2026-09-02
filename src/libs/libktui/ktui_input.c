/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — keys and pointers
 *
 * Two mouse backends, because a terminal emulator and a bare VT have nothing
 * in common here. Under foot (or any xterm-alike) the terminal reports
 * clicks as SGR-1006 escapes. On tty1 there is no reporting at all — the
 * Linux console never grew it, and gpm is not a KDOS package — so the
 * installer reads /dev/input/event* itself and draws its own pointer. That
 * is the whole reason a KDOS install can be done with a mouse on a console
 * with no X, no Wayland and no gpm.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "kbase.h"
#include "ktui.h"

#define MAX_MICE 8
#define IBUF 512

typedef struct {
	int fd;
	int absolute;
	int amin_x, amax_x, amin_y, amax_y;
} Mouse;

static Mouse mice[MAX_MICE];
static int nmice;

static unsigned char ibuf[IBUF];
static int ilen;

static int pointer_on;
static double ptr_fx, ptr_fy;	/* sub-cell accumulator, VT backend        */
static int cell_w = 8, cell_h = 16;
static double ptr_seen;

static int esc_pending;
static double esc_at;

/* ──────────────────────────────────────────────────────────────────────── */

static int bit_set(const unsigned long *bits, int n)
{
	return (bits[n / (8 * (int)sizeof(long))] >>
		(n % (8 * (int)sizeof(long)))) & 1;
}

static void cell_metrics(void)
{
	char buf[64];
	if (kb_read_line_file("/sys/class/graphics/fb0/virtual_size", buf,
			   sizeof(buf)) > 0) {
		int px = 0, py = 0;
		if (sscanf(buf, "%d,%d", &px, &py) == 2 && px > 0 && py > 0 &&
		    ktui_w > 0 && ktui_h > 0) {
			cell_w = px / ktui_w;
			cell_h = py / ktui_h;
		}
	}
	if (cell_w < 4)
		cell_w = 8;
	if (cell_h < 4)
		cell_h = 16;
}

static void mice_open(void)
{
	DIR *d = opendir("/dev/input");
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && nmice < MAX_MICE) {
		if (strncmp(e->d_name, "event", 5))
			continue;
		char path[300];
		snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
		int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;

		unsigned long types[(EV_MAX + 8 * sizeof(long)) / (8 * sizeof(long))];
		unsigned long keys[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))];
		unsigned long rel[(REL_MAX + 8 * sizeof(long)) / (8 * sizeof(long))];
		unsigned long abs_[(ABS_MAX + 8 * sizeof(long)) / (8 * sizeof(long))];
		unsigned long props[(INPUT_PROP_MAX + 8 * sizeof(long)) / (8 * sizeof(long))];
		memset(types, 0, sizeof(types));
		memset(keys, 0, sizeof(keys));
		memset(rel, 0, sizeof(rel));
		memset(abs_, 0, sizeof(abs_));
		memset(props, 0, sizeof(props));

		if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0 ||
		    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) {
			close(fd);
			continue;
		}
		/* A pointer is something with a left button and either relative
		 * or absolute axes. Keyboards fail the first test, and the VT
		 * is already giving us their keystrokes on stdin anyway. */
		if (!bit_set(keys, BTN_LEFT)) {
			close(fd);
			continue;
		}
		/* Old kernels fail this ioctl; zeroed props keep the old
		 * behaviour there. */
		ioctl(fd, EVIOCGPROP(sizeof(props)), props);
		int prop_pointer = bit_set(props, INPUT_PROP_POINTER);
		int prop_direct = bit_set(props, INPUT_PROP_DIRECT);

		Mouse m;
		memset(&m, 0, sizeof(m));
		m.fd = fd;

		int has_rel = 0;
		if (bit_set(types, EV_REL)) {
			ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel)), rel);
			has_rel = bit_set(rel, REL_X) && bit_set(rel, REL_Y);
		}
		/* A clickpad is BTN_LEFT + ABS and no REL, exactly the shape of
		 * an absolute tablet — but its coordinates are finger positions
		 * on the PAD, and mapping them teleports the pointer to every
		 * finger-down. INPUT_PROP_POINTER is how the kernel says so; a
		 * skipped touchpad beats a teleporting one. QEMU's usb-tablet
		 * carries no properties at all and must stay absolute, so the
		 * gate is POINTER, with INPUT_PROP_DIRECT explicitly allowed. */
		if (prop_pointer && !prop_direct && !has_rel) {
			close(fd);
			continue;
		}
		if (!has_rel && bit_set(types, EV_ABS)) {
			ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_)), abs_);
			if (bit_set(abs_, ABS_X) && bit_set(abs_, ABS_Y)) {
				struct input_absinfo ai;
				m.absolute = 1;
				if (ioctl(fd, EVIOCGABS(ABS_X), &ai) == 0) {
					m.amin_x = ai.minimum;
					m.amax_x = ai.maximum;
				}
				if (ioctl(fd, EVIOCGABS(ABS_Y), &ai) == 0) {
					m.amin_y = ai.minimum;
					m.amax_y = ai.maximum;
				}
				if (m.amax_x <= m.amin_x || m.amax_y <= m.amin_y)
					m.absolute = 0;
			}
		}
		if (!has_rel && !m.absolute) {
			close(fd);
			continue;
		}
		mice[nmice++] = m;
	}
	closedir(d);
}

int ktui_input_init(int want_mouse)
{
	ilen = 0;
	if (want_mouse && (ktui_caps & KT_CAP_LINUXVT)) {
		cell_metrics();
		mice_open();
		if (nmice) {
			ktui_caps |= KT_CAP_MOUSE;
			ptr_fx = ktui_w / 2.0;
			ptr_fy = ktui_h / 2.0;
		}
	}
	return 0;
}

void ktui_input_shutdown(void)
{
	for (int i = 0; i < nmice; i++)
		close(mice[i].fd);
	nmice = 0;
}

void ktui_input_suspend(void)
{
	pointer_on = 0;
}

void ktui_input_resume(void)
{
	cell_metrics();
	ilen = 0;
}

int ktui_input_mouse_visible(int *x, int *y)
{
	if (!pointer_on || !nmice)
		return 0;
	/* The pointer fades out rather than sitting on the screen forever;
	 * a stuck inverse cell reads as a rendering bug. */
	if (kb_now_s() - ptr_seen > 8.0) {
		pointer_on = 0;
		return 0;
	}
	*x = (int)ptr_fx;
	*y = (int)ptr_fy;
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int evdev_read(KtuiEvent *ev)
{
	struct input_event ie;
	for (int i = 0; i < nmice; i++) {
		while (read(mice[i].fd, &ie, sizeof(ie)) == (ssize_t)sizeof(ie)) {
			if (ie.type == EV_REL) {
				if (ie.code == REL_X) {
					ptr_fx += (double)ie.value / cell_w * 2.0;
				} else if (ie.code == REL_Y) {
					ptr_fy += (double)ie.value / cell_h * 2.0;
				} else if (ie.code == REL_WHEEL) {
					ev->type = KT_EVT_MOUSE;
					ev->btn = ie.value > 0 ? KT_MB_WHEEL_UP
							       : KT_MB_WHEEL_DOWN;
					ev->press = KT_MP_PRESS;
					ev->mx = (int)ptr_fx;
					ev->my = (int)ptr_fy;
					ptr_seen = kb_now_s();
					pointer_on = 1;
					return 1;
				}
				if (ie.code == REL_X || ie.code == REL_Y) {
					pointer_on = 1;
					ptr_seen = kb_now_s();
				}
			} else if (ie.type == EV_ABS && mice[i].absolute) {
				if (ie.code == ABS_X) {
					double f = (double)(ie.value - mice[i].amin_x) /
						   (mice[i].amax_x - mice[i].amin_x);
					ptr_fx = f * ktui_w;
					pointer_on = 1;
					ptr_seen = kb_now_s();
				} else if (ie.code == ABS_Y) {
					double f = (double)(ie.value - mice[i].amin_y) /
						   (mice[i].amax_y - mice[i].amin_y);
					ptr_fy = f * ktui_h;
					pointer_on = 1;
					ptr_seen = kb_now_s();
				}
			} else if (ie.type == EV_KEY) {
				int b = -1;
				if (ie.code == BTN_LEFT)
					b = KT_MB_LEFT;
				else if (ie.code == BTN_RIGHT)
					b = KT_MB_RIGHT;
				else if (ie.code == BTN_MIDDLE)
					b = KT_MB_MIDDLE;
				if (b < 0)
					continue;
				pointer_on = 1;
				ptr_seen = kb_now_s();
				ev->type = KT_EVT_MOUSE;
				ev->btn = b;
				ev->press = ie.value ? KT_MP_PRESS : KT_MP_RELEASE;
				ev->mx = (int)ptr_fx;
				ev->my = (int)ptr_fy;
				return 1;
			}
		}
	}

	if (ptr_fx < 0)
		ptr_fx = 0;
	if (ptr_fy < 0)
		ptr_fy = 0;
	if (ptr_fx > ktui_w - 1)
		ptr_fx = ktui_w - 1;
	if (ptr_fy > ktui_h - 1)
		ptr_fy = ktui_h - 1;
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void ibuf_drop(int n)
{
	if (n >= ilen) {
		ilen = 0;
		return;
	}
	memmove(ibuf, ibuf + n, (size_t)(ilen - n));
	ilen -= n;
}

static int csi_mods(int p)
{
	int m = 0;
	p -= 1;
	if (p & 1)
		m |= KT_MOD_SHIFT;
	if (p & 2)
		m |= KT_MOD_ALT;
	if (p & 4)
		m |= KT_MOD_CTRL;
	return m;
}

/* Returns 1 if a complete sequence was decoded, 0 if more bytes are needed,
 * -1 if the buffer holds a lone ESC that is not (yet) a sequence. */
static int decode(KtuiEvent *ev)
{
	if (!ilen)
		return 0;

	unsigned char c = ibuf[0];

	if (c != 0x1b) {
		if (c == 0) {
			ibuf_drop(1);
			return 0;
		}
		ev->type = KT_EVT_KEY;
		ev->mods = 0;
		if (c < 0x20 && c != KT_K_ENTER && c != KT_K_TAB && c != 10) {
			ev->key = c + 'a' - 1;
			ev->mods = KT_MOD_CTRL;
			ibuf_drop(1);
			return 1;
		}
		if (c == 10)
			c = KT_K_ENTER;
		if (c == 8)
			c = KT_K_BACKSPACE;
		if (c < 0x80) {
			ev->key = c;
			ibuf_drop(1);
			return 1;
		}
		/* UTF-8 body: needs the whole sequence before it is a key. */
		int need = (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
		if (ilen < need)
			return 0;
		uint32_t cp;
		char tmp[5];
		memcpy(tmp, ibuf, (size_t)need);
		tmp[need] = 0;
		ktui_utf8_next(tmp, &cp);
		ev->key = (int)cp;
		ibuf_drop(need);
		return 1;
	}

	if (ilen == 1)
		return -1;

	/* ESC O x — application cursor / F1..F4 */
	if (ibuf[1] == 'O') {
		if (ilen < 3)
			return 0;
		ev->type = KT_EVT_KEY;
		ev->mods = 0;
		switch (ibuf[2]) {
		case 'A': ev->key = KT_K_UP; break;
		case 'B': ev->key = KT_K_DOWN; break;
		case 'C': ev->key = KT_K_RIGHT; break;
		case 'D': ev->key = KT_K_LEFT; break;
		case 'H': ev->key = KT_K_HOME; break;
		case 'F': ev->key = KT_K_END; break;
		case 'P': ev->key = KT_K_F1; break;
		case 'Q': ev->key = KT_K_F2; break;
		case 'R': ev->key = KT_K_F3; break;
		case 'S': ev->key = KT_K_F4; break;
		default: ibuf_drop(3); return 0;
		}
		ibuf_drop(3);
		return 1;
	}

	if (ibuf[1] != '[') {
		/* ESC <key> is Alt+key. */
		ev->type = KT_EVT_KEY;
		ev->key = ibuf[1];
		ev->mods = KT_MOD_ALT;
		ibuf_drop(2);
		return 1;
	}

	/* CSI */
	int i = 2;
	int sgr_mouse = 0;
	if (i < ilen && ibuf[i] == '<') {
		sgr_mouse = 1;
		i++;
	}
	int par[6] = { 0, 0, 0, 0, 0, 0 }, np = 0, seen = 0;
	while (i < ilen && ((ibuf[i] >= '0' && ibuf[i] <= '9') || ibuf[i] == ';')) {
		if (ibuf[i] == ';') {
			if (np < 5)
				np++;
			seen = 1;
		} else {
			par[np] = par[np] * 10 + (ibuf[i] - '0');
			seen = 1;
		}
		i++;
	}
	if (i >= ilen)
		return 0;
	if (seen)
		np++;
	unsigned char fin = ibuf[i];
	int seqlen = i + 1;

	if (sgr_mouse && (fin == 'M' || fin == 'm')) {
		int b = par[0];
		ev->type = KT_EVT_MOUSE;
		ev->mx = par[1] - 1;
		ev->my = par[2] - 1;
		ev->mods = 0;
		if (b & 4)
			ev->mods |= KT_MOD_SHIFT;
		if (b & 8)
			ev->mods |= KT_MOD_ALT;
		if (b & 16)
			ev->mods |= KT_MOD_CTRL;
		int motion = b & 32;
		int code = b & 3;
		if (b & 64) {
			ev->btn = code == 0 ? KT_MB_WHEEL_UP : KT_MB_WHEEL_DOWN;
			ev->press = KT_MP_PRESS;
		} else {
			ev->btn = code == 0 ? KT_MB_LEFT : code == 1 ? KT_MB_MIDDLE
								  : KT_MB_RIGHT;
			ev->press = fin == 'm' ? KT_MP_RELEASE
					       : motion ? KT_MP_DRAG : KT_MP_PRESS;
			if (motion && code == 3) {
				ev->btn = KT_MB_MOVE;
				ev->press = KT_MP_DRAG;
			}
		}
		ibuf_drop(seqlen);
		return 1;
	}

	ev->type = KT_EVT_KEY;
	ev->mods = np >= 2 ? csi_mods(par[1]) : 0;

	switch (fin) {
	case 'A': ev->key = KT_K_UP; break;
	case 'B': ev->key = KT_K_DOWN; break;
	case 'C': ev->key = KT_K_RIGHT; break;
	case 'D': ev->key = KT_K_LEFT; break;
	case 'H': ev->key = KT_K_HOME; break;
	case 'F': ev->key = KT_K_END; break;
	case 'Z': ev->key = KT_K_BTAB; break;
	case '~':
		switch (par[0]) {
		case 1: ev->key = KT_K_HOME; break;
		case 2: ev->key = KT_K_INS; break;
		case 3: ev->key = KT_K_DEL; break;
		case 4: ev->key = KT_K_END; break;
		case 5: ev->key = KT_K_PGUP; break;
		case 6: ev->key = KT_K_PGDN; break;
		case 11: ev->key = KT_K_F1; break;
		case 12: ev->key = KT_K_F2; break;
		case 13: ev->key = KT_K_F3; break;
		case 14: ev->key = KT_K_F4; break;
		case 15: ev->key = KT_K_F5; break;
		case 17: ev->key = KT_K_F6; break;
		case 18: ev->key = KT_K_F7; break;
		case 19: ev->key = KT_K_F8; break;
		case 20: ev->key = KT_K_F9; break;
		case 21: ev->key = KT_K_F10; break;
		case 23: ev->key = KT_K_F11; break;
		case 24: ev->key = KT_K_F12; break;
		default: ibuf_drop(seqlen); return 0;
		}
		break;
	default:
		ibuf_drop(seqlen);
		return 0;
	}
	ibuf_drop(seqlen);
	return 1;
}

int ktui_input_next(KtuiEvent *ev, int timeout_ms)
{
	memset(ev, 0, sizeof(*ev));

	if (ktui_resized) {
		ktui_resized = 0;
		ktui_term_size_refresh();
		ktui_draw_resize();
		ktui_draw_invalidate();
		ev->type = KT_EVT_RESIZE;
		return 1;
	}

	int r = decode(ev);
	if (r == 1)
		return 1;
	if (r == -1) {
		/* A lone ESC only becomes the Escape key once nothing follows
		 * it for a beat — otherwise every arrow key would fire it. */
		if (!esc_pending) {
			esc_pending = 1;
			esc_at = kb_now_s();
		} else if (kb_now_s() - esc_at > 0.04) {
			esc_pending = 0;
			ibuf_drop(1);
			ev->type = KT_EVT_KEY;
			ev->key = KT_K_ESC;
			return 1;
		}
		if (timeout_ms < 0 || timeout_ms > 20)
			timeout_ms = 20;
	}

	if (evdev_read(ev))
		return 1;

	struct pollfd pfd[1 + MAX_MICE];
	int n = 0;
	pfd[n].fd = 0;
	pfd[n].events = POLLIN;
	n++;
	for (int i = 0; i < nmice; i++) {
		pfd[n].fd = mice[i].fd;
		pfd[n].events = POLLIN;
		n++;
	}

	int pr = poll(pfd, (nfds_t)n, timeout_ms);
	if (pr < 0) {
		if (errno == EINTR) {
			ev->type = KT_EVT_TICK;
			return 0;
		}
		ev->type = KT_EVT_TICK;
		return 0;
	}
	if (pr == 0) {
		ev->type = KT_EVT_TICK;
		return 0;
	}

	if (pfd[0].revents & POLLIN) {
		ssize_t got = read(0, ibuf + ilen, (size_t)(IBUF - ilen));
		if (got > 0) {
			ilen += (int)got;
			esc_pending = 0;
		}
		if (decode(ev) == 1)
			return 1;
	}

	if (evdev_read(ev))
		return 1;

	ev->type = KT_EVT_TICK;
	return 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * Drops
 *
 * Held rather than delivered in the event, because a drop is a position AND a
 * payload. The event carries the position; this carries the payload, once.
 * ──────────────────────────────────────────────────────────────────────── */

static char *drop_buf;
static size_t drop_len;

void ktui_drop_push(const char *utf8, size_t len)
{
	free(drop_buf);
	drop_buf = NULL;
	drop_len = 0;

	if (!utf8 || !len)
		return;

	drop_buf = malloc(len + 1);
	if (!drop_buf)
		return;

	memcpy(drop_buf, utf8, len);
	drop_buf[len] = '\0';
	drop_len = len;
}

const char *ktui_drop_take(size_t *len)
{
	static char *held;

	/* The previous take's buffer is freed HERE rather than by the caller:
	 * a surface that acts on a drop and returns to its loop has no other
	 * moment to do it, and freeing on the next take is that moment. */
	free(held);
	held = drop_buf;
	drop_buf = NULL;

	if (len)
		*len = drop_len;
	drop_len = 0;
	return held;
}
