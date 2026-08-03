/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — terminal ownership
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

#include "kinstall.h"

int term_caps;
int term_w = 80, term_h = 24;
volatile sig_atomic_t term_resized;

static struct termios saved_tio;
static int tio_saved;
static int tty_fd = -1;		/* for ioctls when stdin is not the tty     */
static int out_fd = 1;
static int active;

static unsigned char saved_cmap[48];
static int cmap_saved;

static char *obuf;
static size_t obuf_len, obuf_cap;

/* ──────────────────────────────────────────────────────────────────────── */

void term_write(const char *s, size_t n)
{
	if (obuf_len + n + 1 > obuf_cap) {
		size_t cap = obuf_cap ? obuf_cap : 8192;
		while (cap < obuf_len + n + 1)
			cap *= 2;
		char *p = realloc(obuf, cap);
		if (!p)
			return;
		obuf = p;
		obuf_cap = cap;
	}
	memcpy(obuf + obuf_len, s, n);
	obuf_len += n;
}

void term_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0)
		term_write(buf, (size_t)n > sizeof(buf) - 1 ? sizeof(buf) - 1 : (size_t)n);
}

void term_flush(void)
{
	size_t off = 0;
	while (off < obuf_len) {
		ssize_t w = write(out_fd, obuf + off, obuf_len - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		off += (size_t)w;
	}
	obuf_len = 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void on_winch(int sig)
{
	(void)sig;
	term_resized = 1;
}

void term_size_refresh(void)
{
	struct winsize ws;
	if (ioctl(out_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
		term_w = ws.ws_col;
		term_h = ws.ws_row;
		return;
	}
	const char *c = getenv("COLUMNS"), *l = getenv("LINES");
	term_w = c ? atoi(c) : 80;
	term_h = l ? atoi(l) : 24;
	if (term_w < 20)
		term_w = 80;
	if (term_h < 8)
		term_h = 24;
}

static void detect_caps(void)
{
	const char *term = getenv("TERM");
	const char *ct = getenv("COLORTERM");

	term_caps = 0;

	if (term && !strcmp(term, "linux"))
		term_caps |= CAP_LINUXVT;

	if (ct && (strstr(ct, "truecolor") || strstr(ct, "24bit")))
		term_caps |= CAP_TRUECOLOR;
	else if (term && strstr(term, "256color"))
		term_caps |= CAP_256;
	else if (term && (strstr(term, "foot") || strstr(term, "kitty") ||
			  strstr(term, "alacritty") || strstr(term, "wezterm")))
		term_caps |= CAP_TRUECOLOR;

	/* A real VT is repainted through our own palette, so 24-bit SGR would
	 * only be thrown away — the kernel does not parse it. */
	if (term_caps & CAP_LINUXVT)
		term_caps &= ~(CAP_TRUECOLOR | CAP_256);

	if (!getenv("KDOS_ASCII")) {
		const char *l = getenv("LC_ALL");
		if (!l || !*l)
			l = getenv("LC_CTYPE");
		if (!l || !*l)
			l = getenv("LANG");
		/* musl is UTF-8 always and KDOS ships no other charmap; only an
		 * explicit non-UTF-8 locale or KDOS_ASCII drops us to ASCII. */
		if (!l || !*l || strstr(l, "UTF-8") || strstr(l, "utf8") ||
		    !strcmp(l, "C") || !strcmp(l, "POSIX"))
			term_caps |= CAP_UTF8;
	}
}

/* The console palette is ours for the duration and exactly restored after:
 * GIO_CMAP hands back what kdos-getty's setvtrgb loaded, PIO_CMAP puts the
 * installer's eight slots in its place. No external tool, no drift. */
static void palette_install(void)
{
	if (!(term_caps & CAP_LINUXVT) || tty_fd < 0)
		return;

	if (ioctl(tty_fd, GIO_CMAP, saved_cmap) == 0)
		cmap_saved = 1;

	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		Rgb c = ki_theme->slot[i & 7];
		cm[i * 3 + 0] = c.r;
		cm[i * 3 + 1] = c.g;
		cm[i * 3 + 2] = c.b;
	}
	ioctl(tty_fd, PIO_CMAP, cm);
}

static void palette_restore(void)
{
	if (cmap_saved && tty_fd >= 0)
		ioctl(tty_fd, PIO_CMAP, saved_cmap);
}

void term_repalette(void)
{
	if (!(term_caps & CAP_LINUXVT) || tty_fd < 0)
		return;
	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		Rgb c = ki_theme->slot[i & 7];
		cm[i * 3 + 0] = c.r;
		cm[i * 3 + 1] = c.g;
		cm[i * 3 + 2] = c.b;
	}
	ioctl(tty_fd, PIO_CMAP, cm);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void raw_on(void)
{
	struct termios t;
	if (tcgetattr(0, &t) < 0)
		return;
	if (!tio_saved) {
		saved_tio = t;
		tio_saved = 1;
	}
	t.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	t.c_oflag &= ~(unsigned)(OPOST);
	t.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t);
}

static void raw_off(void)
{
	if (tio_saved)
		tcsetattr(0, TCSANOW, &saved_tio);
}

static void emit(const char *s)
{
	term_write(s, strlen(s));
}

static void enter_screen(void)
{
	/* The Linux VT has no alternate buffer and ignores 1049; harmless. */
	emit("\033[?1049h\033[?25l\033[2J\033[H");
	if (term_caps & CAP_MOUSE)
		emit("\033[?1000h\033[?1002h\033[?1006h");
	term_flush();
}

static void leave_screen(void)
{
	if (term_caps & CAP_MOUSE)
		emit("\033[?1006l\033[?1002l\033[?1000l");
	emit("\033[0m\033[?25h\033[2J\033[H\033[?1049l");
	term_flush();
}

int term_init(int want_mouse)
{
	out_fd = 1;
	detect_caps();

	tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (tty_fd < 0)
		tty_fd = isatty(0) ? 0 : -1;

	if (want_mouse && !(term_caps & CAP_LINUXVT))
		term_caps |= CAP_MOUSE;

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_winch;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);

	term_size_refresh();
	raw_on();
	palette_install();
	enter_screen();
	active = 1;
	return 0;
}

void term_shutdown(void)
{
	if (!active)
		return;
	active = 0;
	leave_screen();
	palette_restore();
	raw_off();
	if (tty_fd > 2)
		close(tty_fd);
	tty_fd = -1;
}

void term_suspend(void)
{
	if (!active)
		return;
	active = 0;
	leave_screen();
	palette_restore();
	raw_off();
}

void term_resume(void)
{
	if (active)
		return;
	active = 1;
	raw_on();
	palette_install();
	enter_screen();
	term_size_refresh();
	draw_resize();
	draw_invalidate();
}
