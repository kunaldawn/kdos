/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — terminal ownership
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

#include "kbase.h"
#include "ktui.h"

int ktui_caps;
int ktui_w = 80, ktui_h = 24;
volatile sig_atomic_t ktui_resized;

static struct termios saved_tio;
static int tio_saved;
static int tty_fd = -1;		/* for ioctls when stdin is not the tty     */
static int out_fd = 1;
static int active;

static unsigned char saved_cmap[48];
static int cmap_saved;

static char *obuf;
static size_t obuf_len, obuf_cap;
static int write_ms = -1;	/* -1 blocks; >=0 bounds one flush         */
static int dropped;

/* ──────────────────────────────────────────────────────────────────────── */

void ktui_term_write(const char *s, size_t n)
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

void ktui_term_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0)
		ktui_term_write(buf, (size_t)n > sizeof(buf) - 1 ? sizeof(buf) - 1 : (size_t)n);
}

/* A FRAME IS DISPOSABLE AND THE WORK BEHIND IT IS NOT. A terminal that stops
 * reading — a scrollback pager, a paused pty, a wedged ssh — otherwise blocks
 * this write for as long as it likes, and a caller that must also service a
 * pipe (kdosbuild draining tar's member names while it archives) deadlocks
 * against a child that fills its 64 K pipe and stops working.
 *
 * So a flush may be given a deadline. Past it the rest of the buffer is
 * DROPPED and the frame is reported lost, because the alternative is stalling
 * the program to redraw a progress bar nobody is reading.
 *
 * A dropped frame leaves the diff renderer's belief about the screen ahead of
 * the screen itself, so ktui_term_flush_dropped() is what the draw layer asks
 * before deciding whether the next paint has to be a full one.
 */
void ktui_term_flush(void)
{
	size_t off = 0;
	int restore = -1;
	double deadline = 0;

	/* O_NONBLOCK rather than poll() alone, because POLLOUT promises only
	 * that SOME room exists: a blocking write of a whole frame into a
	 * nearly full pipe or tty buffer still waits for the rest of it to
	 * drain. The flag goes back the way it was before this returns, so
	 * nothing else holding this terminal ever sees a short write it did
	 * not ask for. */
	if (write_ms >= 0) {
		deadline = kb_now_s() + write_ms / 1000.0;
		int fl = fcntl(out_fd, F_GETFL);
		if (fl >= 0 && !(fl & O_NONBLOCK) &&
		    fcntl(out_fd, F_SETFL, fl | O_NONBLOCK) == 0)
			restore = fl;
	}

	while (off < obuf_len) {
		ssize_t w = write(out_fd, obuf + off, obuf_len - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				break;
			if (write_ms < 0) {
				/* Someone else made this fd non-blocking; the
				 * rest of the frame has nowhere to go. */
				dropped = 1;
				break;
			}
			double left = deadline - kb_now_s();
			if (left <= 0) {
				dropped = 1;
				break;
			}
			struct pollfd pf = { out_fd, POLLOUT, 0 };
			if (poll(&pf, 1, (int)(left * 1000) + 1) < 0 &&
			    errno != EINTR)
				break;
			continue;
		}
		off += (size_t)w;
	}

	if (restore >= 0)
		fcntl(out_fd, F_SETFL, restore);
	obuf_len = 0;
}

void ktui_term_set_write_timeout(int ms)
{
	write_ms = ms;
}

int ktui_term_flush_dropped(void)
{
	int d = dropped;
	dropped = 0;
	return d;
}

static const char CLIP_B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Hand-rolled rather than a library call — this file links nothing but libc,
 * so there is no <b64.h> to reach for and pulling one in for a single
 * OSC 52 write would break that property for everything else in libktui. */
int ktui_clip_copy(const char *text)
{
	if (!text || (ktui_caps & KT_CAP_LINUXVT))
		return 0;

	size_t n = strlen(text);
	ktui_term_write("\033]52;c;", 7);
	for (size_t i = 0; i < n; i += 3) {
		unsigned char b0 = (unsigned char)text[i];
		unsigned char b1 = i + 1 < n ? (unsigned char)text[i + 1] : 0;
		unsigned char b2 = i + 2 < n ? (unsigned char)text[i + 2] : 0;
		char out[4];
		out[0] = CLIP_B64[b0 >> 2];
		out[1] = CLIP_B64[((b0 & 0x3) << 4) | (b1 >> 4)];
		out[2] = i + 1 < n ? CLIP_B64[((b1 & 0xf) << 2) | (b2 >> 6)] : '=';
		out[3] = i + 2 < n ? CLIP_B64[b2 & 0x3f] : '=';
		ktui_term_write(out, 4);
	}
	ktui_term_write("\007", 1);
	ktui_term_flush();
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void on_winch(int sig)
{
	(void)sig;
	ktui_resized = 1;
}

void ktui_term_size_refresh(void)
{
	struct winsize ws;
	if (ioctl(out_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
		ktui_w = ws.ws_col;
		ktui_h = ws.ws_row;
		return;
	}
	const char *c = getenv("COLUMNS"), *l = getenv("LINES");
	ktui_w = c ? atoi(c) : 80;
	ktui_h = l ? atoi(l) : 24;
	if (ktui_w < 20)
		ktui_w = 80;
	if (ktui_h < 8)
		ktui_h = 24;
}

static void detect_caps(void)
{
	const char *term = getenv("TERM");
	const char *ct = getenv("COLORTERM");

	ktui_caps = 0;

	if (term && !strcmp(term, "linux"))
		ktui_caps |= KT_CAP_LINUXVT;

	if (ct && (strstr(ct, "truecolor") || strstr(ct, "24bit")))
		ktui_caps |= KT_CAP_TRUECOLOR;
	else if (term && strstr(term, "256color"))
		ktui_caps |= KT_CAP_256;
	else if (term && (strstr(term, "foot") || strstr(term, "kitty") ||
			  strstr(term, "alacritty") || strstr(term, "wezterm")))
		ktui_caps |= KT_CAP_TRUECOLOR;
	/* Anything else that is a terminal at all gets 256. Without this the
	 * last branch of emit_sgr is reached, and that branch is a FIXED table
	 * of ANSI codes that never consults ktui_theme — so the palette is the
	 * terminal's, not ours, and `kdos theme`/[T] change nothing at all.
	 * `make build` walks straight into it: docker run -it sets TERM=xterm
	 * and does not forward COLORTERM, so the build TUI rendered in the
	 * terminal's own eight colours and the theme key looked broken.
	 * Every emulator that understands `\033[38;5;N` has for two decades;
	 * `dumb` and an unset TERM are the ones that genuinely do not. */
	else if (term && *term && strcmp(term, "dumb"))
		ktui_caps |= KT_CAP_256;

	/* A real VT is repainted through our own palette, so 24-bit SGR would
	 * only be thrown away — the kernel does not parse it. */
	if (ktui_caps & KT_CAP_LINUXVT)
		ktui_caps &= ~(KT_CAP_TRUECOLOR | KT_CAP_256);

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
			ktui_caps |= KT_CAP_UTF8;
	}
}

/* The console palette is ours for the duration and exactly restored after:
 * GIO_CMAP hands back what kdos-getty's setvtrgb loaded, PIO_CMAP puts the
 * installer's eight slots in its place. No external tool, no drift. */
static void palette_install(void)
{
	if (!(ktui_caps & KT_CAP_LINUXVT) || tty_fd < 0)
		return;

	if (ioctl(tty_fd, GIO_CMAP, saved_cmap) == 0)
		cmap_saved = 1;

	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		KRgb c = ktui_theme->slot[i & 7];
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

void ktui_term_repalette(void)
{
	if (!(ktui_caps & KT_CAP_LINUXVT) || tty_fd < 0)
		return;
	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		KRgb c = ktui_theme->slot[i & 7];
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
	ktui_term_write(s, strlen(s));
}

/*
 * WHETHER THE KITTY KEYBOARD PROTOCOL WAS PUSHED ONTO THIS TERMINAL'S STACK.
 *
 * Popped exactly where it was pushed and only if it was. A pop that never had
 * a push corrupts the stack of a terminal that was already using the protocol
 * for something else; a push that is never popped leaves the person's terminal
 * in a mode their shell does not understand, with no way back short of
 * `reset` — which is a worse failure than a chord that does not fire.
 */
static int kkbd_pushed;

/*
 * ASK FIRST, AND PUSH ONLY WHAT ANSWERS.
 *
 * `CSI ? u` asks a terminal which flags it has set. A terminal that
 * implements the protocol replies `CSI ? <flags> u`; one that does not replies
 * nothing at all, and the xterm modifier encoding stays as the fallback. There
 * is no capability database entry for this and no TERM value that implies it,
 * so asking is the only way to know.
 *
 * THE REPLY IS CONSUMED HERE OR IT IS TYPED INTO THE DESKTOP. It arrives on
 * standard input like any other key, and the decoder has no case for it, so a
 * reply left in the buffer reaches the session as stray characters.
 *
 * The Linux VT answers nothing and is skipped outright rather than waited on:
 * it is what a `--tty` view on tty1 runs in, so this timeout would be paid on
 * the most common console of all.
 */
static void kkbd_push(void)
{
	char buf[64];
	size_t n = 0;

	kkbd_pushed = 0;
	if (ktui_caps & KT_CAP_LINUXVT)
		return;

	emit("\033[?u");
	ktui_term_flush();

	while (n < sizeof(buf) - 1) {
		struct pollfd p = { 0, POLLIN, 0 };
		int r = poll(&p, 1, 60);

		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		if (read(0, buf + n, 1) != 1)
			break;
		if (buf[n++] == 'u')
			break;
	}

	if (n >= 4 && buf[0] == 0x1b && buf[1] == '[' && buf[2] == '?' &&
	    buf[n - 1] == 'u') {
		/* Flag 1: disambiguate escape codes. It is the one this
		 * desktop needs — it is what makes Super arrive at all — and
		 * asking for more would be asking for reports nothing reads. */
		emit("\033[>1u");
		ktui_term_flush();
		kkbd_pushed = 1;
	}
}

static void enter_screen(void)
{
	/* The Linux VT has no alternate buffer and ignores 1049; harmless. */
	emit("\033[?1049h\033[?25l\033[2J\033[H");
	/*
	 * BRACKETED PASTE, ALWAYS. Without it a paste arrives as the keys it
	 * spells and a line beginning with a chord runs the chord — and a view
	 * in somebody's terminal has no other way to be handed text at all,
	 * because the host terminal owns the clipboard and this program never
	 * sees the menu. A terminal that does not implement it ignores the
	 * mode and nothing changes.
	 */
	emit("\033[?2004h");
	if (ktui_caps & KT_CAP_MOUSE)
		emit("\033[?1000h\033[?1002h\033[?1006h");
	ktui_term_flush();
	kkbd_push();
}

/*
 * THE CURSOR IS HIDDEN BY enter_screen AND SHOWN ONLY FOR A CARET. A terminal
 * cursor parked wherever the last write left it is a distraction on a screen
 * this library is painting cell by cell; one placed deliberately is the caret.
 */
void ktui_term_caret(int x, int y)
{
	static int last_x = -2, last_y = -2;
	char seq[48];

	if (x == last_x && y == last_y)
		return;
	last_x = x;
	last_y = y;

	if (x < 0 || y < 0) {
		emit("\033[?25l");
		ktui_term_flush();
		return;
	}

	/* One-based, row first, which is what every terminal has meant by CUP
	 * since the VT100. */
	snprintf(seq, sizeof(seq), "\033[%d;%dH\033[?25h", y + 1, x + 1);
	emit(seq);
	ktui_term_flush();
}

static void leave_screen(void)
{
	if (kkbd_pushed) {
		emit("\033[<1u");
		kkbd_pushed = 0;
	}
	if (ktui_caps & KT_CAP_MOUSE)
		emit("\033[?1006l\033[?1002l\033[?1000l");
	emit("\033[?2004l");
	emit("\033[0m\033[?25h\033[2J\033[H\033[?1049l");
	ktui_term_flush();
}

int ktui_term_init(int want_mouse)
{
	out_fd = 1;
	detect_caps();

	tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (tty_fd < 0)
		tty_fd = isatty(0) ? 0 : -1;

	if (want_mouse && !(ktui_caps & KT_CAP_LINUXVT))
		ktui_caps |= KT_CAP_MOUSE;

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_winch;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);

	ktui_term_size_refresh();
	raw_on();
	palette_install();
	enter_screen();
	active = 1;
	return 0;
}

void ktui_term_shutdown(void)
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

void ktui_term_suspend(void)
{
	if (!active)
		return;
	active = 0;
	leave_screen();
	palette_restore();
	raw_off();
}

void ktui_term_resume(void)
{
	if (active)
		return;
	active = 1;
	raw_on();
	palette_install();
	enter_screen();
	ktui_term_size_refresh();
	ktui_draw_resize();
	ktui_draw_invalidate();
}
