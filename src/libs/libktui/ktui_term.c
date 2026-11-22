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
#include <sys/wait.h>

#include "kbase.h"
#include "ktui.h"

int ktui_caps;
int ktui_w = 80, ktui_h = 24;
volatile sig_atomic_t ktui_resized;

static struct termios saved_tio;
static int tio_saved;
static int tty_fd = -1;		/* for ioctls when stdin is not the tty     */
static int pal_fd = -1;		/* the VT the console palette is set on     */
static int out_fd = 1;
static int active;

static unsigned char saved_cmap[48];
static int cmap_saved;

static char *obuf;
static size_t obuf_len, obuf_cap;
static int write_ms = -1;	/* -1 blocks; >=0 bounds one flush         */
static int dropped;
/* The host terminal hung up: every write since has failed. Sticky, because a
 * terminal that has gone does not come back and the consumer may not look
 * until its next turn round the loop. */
static int hungup;

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
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				/* THE HOST TERMINAL WENT AWAY. An `ssh` drop
				 * hangs up the pty and every write after it
				 * fails; without a record of that, a
				 * full-screen program spins at its poll
				 * timeout writing frames into a descriptor
				 * with nothing on the other end, and never
				 * decides to leave. */
				hungup = 1;
				break;
			}
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

int ktui_term_hungup(void)
{
	return hungup;
}

/*
 * THE OTHER HALF OF THE SAME FACT, from the reading side. A terminal that has
 * gone is usually discovered by a write that fails, but a program that only
 * reads — a viewer waiting on a key — discovers it as an end of file on stdin
 * and has the same one question to ask afterwards.
 */
void ktui_term_mark_hungup(void)
{
	hungup = 1;
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

	/* Konsole answers 24-bit SGR whatever TERM it was given, and the TERM
	 * it gives is a plain `xterm-256color`; a profile that unset
	 * COLORTERM would otherwise draw in 256 colours. KONSOLE_VERSION is
	 * the variable only Konsole sets. */
	if ((ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"))) ||
	    (getenv("KONSOLE_VERSION") && term && strcmp(term, "linux")))
		ktui_caps |= KT_CAP_TRUECOLOR;
	else if (term && strstr(term, "256color"))
		ktui_caps |= KT_CAP_256;
	else if (term && (strstr(term, "konsole") || strstr(term, "foot") ||
			  strstr(term, "kitty") || strstr(term, "alacritty") ||
			  strstr(term, "wezterm")))
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

/*
 * THE PALETTE IS SET THROUGH A VT, WHICH /dev/tty MAY NOT BE. sudo runs its
 * command on a fresh pseudo-terminal (use_pty), so TERM is still linux and
 * the screen is still the console, but GIO_CMAP and PIO_CMAP fail on the pts
 * and the screen keeps kdos-getty's palette: its slots 4-6 are VT blue,
 * magenta and cyan where the installer's rules and hints belong. The colour
 * map is console-wide, so any VT descriptor sets the one on screen: the
 * terminal itself when it answers GIO_CMAP, else the VT sudo was started
 * from ($SUDO_TTY, when it names /dev/ttyN), else /dev/tty0 when stdin is a
 * pts under a sudo that set no SUDO_TTY. A sudo whose SUDO_TTY names a pts,
 * and a pts outside sudo (ssh with TERM=linux), are not on this machine's
 * screen and get no palette.
 */
static int palette_fd_open(void)
{
	unsigned char probe[48];
	const char *st = getenv("SUDO_TTY");
	int fd;

	if (tty_fd >= 0 && ioctl(tty_fd, GIO_CMAP, probe) == 0)
		return tty_fd;
	if (st && !strncmp(st, "/dev/tty", 8) && st[8] >= '0' && st[8] <= '9') {
		fd = open(st, O_RDWR | O_NOCTTY | O_CLOEXEC);
		if (fd >= 0 && ioctl(fd, GIO_CMAP, probe) == 0)
			return fd;
		if (fd >= 0)
			close(fd);
	}
	if (!st && getenv("SUDO_UID") && isatty(0)) {
		const char *n = ttyname(0);

		if (n && !strncmp(n, "/dev/pts/", 9)) {
			fd = open("/dev/tty0", O_RDWR | O_NOCTTY | O_CLOEXEC);
			if (fd >= 0 && ioctl(fd, GIO_CMAP, probe) == 0)
				return fd;
			if (fd >= 0)
				close(fd);
		}
	}
	return -1;
}

/* The console palette is ours for the duration and exactly restored after:
 * GIO_CMAP hands back what kdos-getty's setvtrgb loaded, PIO_CMAP puts the
 * installer's eight slots in its place. No external tool, no drift. */
static void palette_install(void)
{
	if (!(ktui_caps & KT_CAP_LINUXVT))
		return;
	pal_fd = palette_fd_open();
	if (pal_fd < 0)
		return;

	if (ioctl(pal_fd, GIO_CMAP, saved_cmap) == 0)
		cmap_saved = 1;

	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		KRgb c = ktui_theme->slot[i & 7];
		cm[i * 3 + 0] = c.r;
		cm[i * 3 + 1] = c.g;
		cm[i * 3 + 2] = c.b;
	}
	ioctl(pal_fd, PIO_CMAP, cm);
}

static void palette_restore(void)
{
	if (cmap_saved && pal_fd >= 0)
		ioctl(pal_fd, PIO_CMAP, saved_cmap);
	cmap_saved = 0;
	if (pal_fd >= 0 && pal_fd != tty_fd)
		close(pal_fd);
	pal_fd = -1;
}

void ktui_term_repalette(void)
{
	if (!(ktui_caps & KT_CAP_LINUXVT) || pal_fd < 0)
		return;
	unsigned char cm[48];
	for (int i = 0; i < 16; i++) {
		KRgb c = ktui_theme->slot[i & 7];
		cm[i * 3 + 0] = c.r;
		cm[i * 3 + 1] = c.g;
		cm[i * 3 + 2] = c.b;
	}
	ioctl(pal_fd, PIO_CMAP, cm);
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
 * ASK FIRST, AND USE ONLY WHAT ANSWERS.
 *
 * Two facts about a terminal follow from no `TERM` value and no capability
 * database entry, so both are asked for, and asked for together:
 *
 *   `CSI ? u`         which kitty keyboard flags it has set. A terminal that
 *                     implements the protocol replies `CSI ? <flags> u`; one
 *                     that does not replies nothing at all and the xterm
 *                     modifier encoding stays as the fallback. It is what
 *                     makes Super arrive at all.
 *   `CSI ? 2026 $ p`  DECRQM for synchronized output. The reply carries the
 *                     mode and its state: 0 is "not recognised" and 4 is
 *                     "permanently reset", so only 1, 2 and 3 mean a frame
 *                     can be bracketed.
 *
 * ONE WRITE AND ONE WAIT. Asking in turn would pay the timeout twice on a
 * terminal that answers neither, and the second wait would swallow a slow
 * reply to the first query as if it were its own. The answers come back in
 * the order the queries went out, so they are told apart by scanning the
 * buffer rather than by turn-taking.
 *
 * THE REPLIES ARE CONSUMED HERE OR THEY ARE TYPED INTO THE DESKTOP. They
 * arrive on standard input like any other key, and the decoder has no case for
 * them, so a reply left in the buffer reaches the session as stray characters.
 *
 * The Linux VT answers nothing and is skipped outright rather than waited on:
 * it is what a `--tty` view on tty1 runs in, so this timeout would be paid on
 * the most common console of all.
 */
static void probe_terminal(void)
{
	char buf[96];
	size_t n = 0;

	kkbd_pushed = 0;
	ktui_caps &= ~KT_CAP_SYNC;
	if (ktui_caps & KT_CAP_LINUXVT)
		return;

	emit("\033[?u\033[?2026$p");
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
		/* DECRQM went out second, so its terminator ends the wait for
		 * both. A terminal that answers only the first still waits out
		 * the one timeout, and one that answers neither still waits
		 * out exactly one. */
		if (buf[n++] == 'y')
			break;
	}
	buf[n] = 0;

	for (size_t i = 0; i + 3 < n; i++) {
		if (buf[i] != 0x1b || buf[i + 1] != '[' || buf[i + 2] != '?')
			continue;

		size_t j = i + 3;
		while (j < n && ((buf[j] >= '0' && buf[j] <= '9') ||
				 buf[j] == ';' || buf[j] == '$'))
			j++;
		if (j >= n)
			break;

		if (buf[j] == 'u') {
			/* Flag 1: disambiguate escape codes. It is the one this
			 * desktop needs — it is what makes Super arrive at all
			 * — and asking for more would be asking for reports
			 * nothing reads. */
			emit("\033[>1u");
			kkbd_pushed = 1;
		} else if (buf[j] == 'y') {
			const char *semi = memchr(buf + i, ';', j - i);
			int val = semi ? atoi(semi + 1) : 0;

			if (atoi(buf + i + 3) == 2026 && val >= 1 && val <= 3)
				ktui_caps |= KT_CAP_SYNC;
		}
	}

	ktui_term_flush();
}

/* Where ktui_term_caret() last put the terminal's cursor; negative is hidden. */
static int caret_x = -2, caret_y = -2;

static void enter_screen(void)
{
	/* The Linux VT has no alternate buffer and ignores 1049; harmless. */
	emit("\033[?1049h\033[?25l\033[2J\033[H");
	/* The escape above hid the cursor, so a caret asked for at the
	 * position it held before a suspend must be written again. */
	caret_x = caret_y = -1;
	/*
	 * BRACKETED PASTE, ALWAYS. Without it a paste arrives as the keys it
	 * spells and a line beginning with a chord runs the chord — and a
	 * surface in somebody's terminal has no other way to be handed text,
	 * because the host terminal owns the clipboard and this program never
	 * sees the menu. A terminal that does not implement it ignores the
	 * mode and nothing changes.
	 */
	emit("\033[?2004h");
	if (ktui_caps & KT_CAP_MOUSE)
		emit("\033[?1000h\033[?1002h\033[?1006h");
	ktui_term_flush();
	probe_terminal();
}

/*
 * THE CURSOR IS HIDDEN BY enter_screen AND SHOWN ONLY FOR A CARET. A terminal
 * cursor parked wherever the last write left it is a distraction on a screen
 * this library is painting cell by cell; one placed deliberately is the caret.
 */
void ktui_term_caret(int x, int y)
{
	const KtuiBackend *b = ktui_backend();
	char seq[48];

	if (x == caret_x && y == caret_y)
		return;
	caret_x = x;
	caret_y = y;

	/* A display server places it, and the escape is never written: this
	 * surface's stdout is not the screen it is drawn on. */
	if (b && b->caret) {
		b->caret(x, y);
		return;
	}

	/*
	 * AND OFFSCREEN THERE IS NO TERMINAL TO PLACE ONE ON. stdout is where
	 * the dump goes, so the escape lands INSIDE the frame — which is how
	 * a committed reference frame came to begin with a cursor move, and
	 * how a frame taken with no size imposed came to differ from the same
	 * frame taken with one depending on which message arrived first.
	 */
	if (ktui_offscreen())
		return;

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

/*
 * ON A LINUX VT A HIDDEN CURSOR IS HIDDEN AGAIN IN EVERY FRAME. The console's
 * cursor state belongs to the virtual console, not to this program, and
 * anything else that writes to it (a kernel message, another program on the
 * same VT, a reset) can turn it back on at wherever the last frame's write
 * left it. Six bytes a frame keep that cell from showing a stray block; a
 * terminal emulator keeps its own mode and is not sent them.
 */
void ktui_term_caret_hold(void)
{
	const KtuiBackend *b = ktui_backend();

	if (!(ktui_caps & KT_CAP_LINUXVT) || (b && b->caret) ||
	    ktui_offscreen() || (caret_x >= 0 && caret_y >= 0))
		return;
	emit("\033[?25l");
}

static void leave_screen(void)
{
	if (kkbd_pushed) {
		emit("\033[<1u");
		kkbd_pushed = 0;
	}
	if (ktui_caps & KT_CAP_MOUSE)
		emit("\033[?1006l\033[?1002l\033[?1000l");
	/* A frame whose close was dropped left the mode on, and a terminal
	 * handed back inside a synchronized block shows nothing the shell
	 * writes into it. Closing here costs one sequence and is the only
	 * place that runs whether or not another frame ever follows. */
	if (ktui_caps & KT_CAP_SYNC)
		emit("\033[?2026l");
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

/*
 * THE NOTE IS THE LAST THING ON THE RESTORED TERMINAL BEFORE THE PROGRAM,
 * so it says how to come back while nothing else is on the screen. The child
 * starts with SHLVL=2: bash.bashrc draws its banner only in the first shell
 * of a terminal (SHLVL 1), and a banner tall enough to fill a 25-row console
 * would scroll the note away before the prompt appears.
 */
int ktui_run_console_note(const char *note, char *const argv[])
{
	ktui_term_suspend();
	ktui_input_suspend();
	if (note && *note)
		printf("\033[0m%s\n\n", note);
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0) {
		setenv("SHLVL", "2", 1);
		execvp(argv[0], argv);
		_exit(127);
	}
	int st = 0;
	if (pid > 0)
		waitpid(pid, &st, 0);
	fputs("\n\033[0m-- press Enter to return --", stdout);
	fflush(stdout);
	int c;
	while ((c = getchar()) != '\n' && c != EOF)
		;
	ktui_input_resume();
	ktui_term_resume();
	return st;
}
