/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-banner — the login banner, revealed the way a CRT would draw it.
 *
 * It paints the banner raster-line by raster-line with a brighter "beam" line
 * leading the fill, then a single reverse-video flash for the power-on thump.
 *
 *   kdos-banner            animate, then leave the banner on screen
 *   kdos-banner --plain    print it with no animation
 *
 * THE BANNER IS COMPOSED HERE, NOT BY FASTFETCH, and that is not a style
 * choice. Asked to draw a logo, fastfetch prints the logo block, then moves the
 * CURSOR BACK UP over it (ESC[<n>A) and writes each info line with an absolute
 * column jump. That output is not a sequence of raster lines: replaying it one
 * line at a time — which is what any animation does — leaves the cursor
 * drifting one row per line, so the logo gets overpainted and the whole block
 * is drawn twice, offset. So fastfetch runs with --logo none, which emits
 * plain lines, and the logo column is pasted on here. Side effects worth
 * keeping: the logo can never come out missing, and its height no longer has
 * to be smaller than the info column's.
 *
 * It gets out of the way when it should: no animation unless stdout is a tty,
 * and never when KDOS_NO_ANIM is set, TERM is dumb, or the terminal is too
 * short for the banner (scrolling mid-animation looks like a glitch, not a
 * feature). Any keypress skips the rest instantly.
 *
 * The frame delay used to come from bash's `read -t`, specifically to avoid a
 * fork of sleep per frame on every login. In C it is a nanosleep and the
 * question does not arise; the keypress check is a zero-timeout poll on stdin.
 * ---------------------------------
 */

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "kdos-tools.h"

#define BEAM   "\033[1;97m"
#define OFF    "\033[0m"
#define INDENT 2	/* left margin of the logo column                   */
#define GAP    3	/* blank columns between logo and info             */

#define MAX_LINES 256

static char *lines[MAX_LINES];
static int nlines;

static void push(char *s)
{
	if (nlines < MAX_LINES)
		lines[nlines++] = s;
	else
		free(s);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Visible text with every SGR sequence removed. The shell version did this in
 * pure bash on purpose: `tr -d '\033'` drops the escape byte and leaves the
 * "[1;32m" visible for a frame, and a sed per line is a fork per line on every
 * login. */
static char *strip_ansi(const char *s)
{
	char *out = kb_calloc(1, strlen(s) + 1);
	size_t o = 0;
	for (const char *p = s; *p;) {
		if (p[0] == '\033' && p[1] == '[') {
			p += 2;
			while (*p && ((*p >= '0' && *p <= '9') || *p == ';'))
				p++;
			if (*p)
				p++;	/* the final letter */
			continue;
		}
		out[o++] = *p++;
	}
	out[o] = 0;
	return out;
}

/* Display width in cells, not bytes: the logo is box-drawing and block
 * characters, every one of them a multi-byte UTF-8 sequence occupying one
 * cell. Counting bytes puts the info column three times too far right. */
static int cells(const char *s)
{
	int n = 0;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++)
		if ((*p & 0xc0) != 0x80)
			n++;
	return n;
}

static int term_cols(void)
{
	const char *c = getenv("COLUMNS");
	if (c && atoi(c) > 0)
		return atoi(c);
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col)
		return ws.ws_col;
	return 80;
}

static int term_rows(void)
{
	const char *l = getenv("LINES");
	if (l && atoi(l) > 0)
		return atoi(l);
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row)
		return ws.ws_row;
	return 24;
}

/* ──────────────────────────────────────────────────────────────────────── */

static char **split_lines(char *text, int *count)
{
	int n = 0;
	for (char *p = text; *p; p++)
		if (*p == '\n')
			n++;
	if (text[0] && text[strlen(text) - 1] != '\n')
		n++;

	char **v = kb_calloc((size_t)n + 1, sizeof(*v));
	int i = 0;
	char *start = text;
	for (char *p = text; *p && i < n; p++) {
		if (*p != '\n')
			continue;
		*p = 0;
		v[i++] = start;
		start = p + 1;
	}
	if (i < n && *start)
		v[i++] = start;
	*count = i;
	return v;
}

/*
 * fastfetch identifies the terminal by walking up the process tree, which from
 * here finds THIS program and reports "kdos-banner" as the terminal.
 * TERM_PROGRAM is checked first, so seed it from $TERM. --pipe false keeps the
 * colours when stdout is a pipe, which it is.
 */
static char *info_text(void)
{
	static char buf[1 << 16];
	buf[0] = 0;

	if (kb_have_prog("fastfetch")) {
		const char *tp = getenv("TERM_PROGRAM");
		if (!tp || !*tp) {
			const char *t = getenv("TERM");
			setenv("TERM_PROGRAM", t && *t ? t : "linux", 1);
		}
		KbArgv a = {0};
		kb_argv_add(&a, "fastfetch");
		kb_argv_add(&a, "--logo");
		kb_argv_add(&a, "none");
		kb_argv_add(&a, "--pipe");
		kb_argv_add(&a, "false");
		kb_argv_end(&a);
		if (kb_run_capture(&a, buf, sizeof(buf)) == 0 && buf[0])
			return buf;
		buf[0] = 0;
	}

	size_t n = 0;
	char *motd = kb_read_all("/etc/motd", &n);
	if (motd) {
		kb_strlcpy(buf, motd, sizeof(buf));
		free(motd);
	}
	return buf;
}

/* Paste the logo column and the info column into one array of raster lines. */
static void compose(void)
{
	const char *logopath = getenv("KDOS_BANNER_LOGO");
	if (!logopath || !*logopath)
		logopath = "/usr/share/kdos/logo.txt";

	size_t len = 0;
	char *logotext = kb_read_all(logopath, &len);
	int nlogo = 0;
	char **logo = logotext ? split_lines(logotext, &nlogo) : NULL;

	char *raw = info_text();
	int ninfo = 0;
	char **info = raw[0] ? split_lines(raw, &ninfo) : NULL;

	if (!nlogo) {
		for (int i = 0; i < ninfo; i++)
			push(kb_strdup(info[i]));
		return;
	}

	char **bare = kb_calloc((size_t)nlogo, sizeof(*bare));
	int w = 0;
	for (int i = 0; i < nlogo; i++) {
		bare[i] = strip_ansi(logo[i]);
		int c = cells(bare[i]);
		if (c > w)
			w = c;
	}

	int iw = 0;
	for (int i = 0; i < ninfo; i++) {
		char *v = strip_ansi(info[i]);
		int c = cells(v);
		if (c > iw)
			iw = c;
		free(v);
	}

	/* Side by side only if it fits. A 1280x800 TTY with the 16x32 console
	 * font is 80 columns; the logo plus the info column is wider than that,
	 * and a banner that wraps mid-line looks like a crash, not a boot. */
	if (INDENT + w + GAP + iw > term_cols()) {
		for (int i = 0; i < nlogo; i++) {
			KbBuf b = {0};
			kb_buf_printf(&b, "%*s%s", INDENT, "", logo[i]);
			push(b.p);
		}
		push(kb_strdup(""));
		for (int i = 0; i < ninfo; i++)
			push(kb_strdup(info[i]));
		return;
	}

	int n = nlogo > ninfo ? nlogo : ninfo;
	for (int i = 0; i < n; i++) {
		const char *l = i < nlogo ? logo[i] : "";
		const char *b = i < nlogo ? bare[i] : "";
		const char *r = i < ninfo ? info[i] : "";
		KbBuf line = {0};
		if (!*r) {
			/* Nothing to the right: no trailing padding, or a
			 * "blank" line ends up full of spaces and the beam
			 * flashes an empty bar. */
			kb_buf_printf(&line, "%*s%s", INDENT, "", l);
		} else {
			kb_buf_printf(&line, "%*s%s%*s%s", INDENT, "", l,
				      w - cells(b) + GAP, "", r);
		}
		push(line.p);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static void nap(double s)
{
	struct timespec ts = { (time_t)s, (long)((s - (double)(time_t)s) * 1e9) };
	nanosleep(&ts, NULL);
}

static int key_waiting(void)
{
	struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
	return poll(&p, 1, 0) == 1 && (p.revents & POLLIN);
}

static void plain(int from)
{
	for (int i = from; i < nlines; i++)
		printf("%s\n", lines[i]);
}

static void animate(double delay)
{
	struct termios saved, raw;
	int have_tio = tcgetattr(STDIN_FILENO, &saved) == 0;
	if (have_tio) {
		raw = saved;
		raw.c_lflag &= ~(unsigned)(ICANON | ECHO);
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	}

	fputs("\033[?25l", stdout);	/* hide the cursor while painting */
	for (int i = 0; i < nlines; i++) {
		/* Skip on any keypress: an impatient login should not be
		 * punished. */
		if (key_waiting()) {
			char drain[64];
			while (read(STDIN_FILENO, drain, sizeof(drain)) > 0)
				;
			plain(i);
			break;
		}
		/* The beam: draw the incoming line bright, then rewrite it in
		 * its own colours so only the leading edge glows. */
		char *b = strip_ansi(lines[i]);
		printf("%s%s%s\r", BEAM, b, OFF);
		free(b);
		printf("%s\n", lines[i]);
		fflush(stdout);
		nap(delay);
	}
	fputs("\033[?25h", stdout);	/* cursor back */

	/* Power-on thump: one frame of reverse video. Two escapes, no redraw. */
	fputs("\033[?5h", stdout);
	fflush(stdout);
	nap(0.04);
	fputs("\033[?5l", stdout);
	fflush(stdout);

	if (have_tio)
		tcsetattr(STDIN_FILENO, TCSANOW, &saved);
}

/*
 * One recorded lesson under the banner, in the secondary colour (SGR 33 — the
 * slot /etc/vtrgb and the generated foot palette both point at the scheme's
 * secondary; never bold, because on a 512-glyph VT the intensity bit selects
 * the second font page).
 *
 * NOT printed in --plain, and not when stdout is redirected. `kdos-banner
 * --plain` is asserted byte-identical against the shell version it replaced,
 * and a login line that varies by day and pid would make every diff of it a
 * failure. The animated login is the surface this is for.
 */
static void oracle_line(void)
{
	char *line = kdt_oracle_line();
	if (!line)
		return;
	if (cells(line) + INDENT <= term_cols())
		printf("\n%*s\033[33m%s\033[0m\n", INDENT, "", line);
	free(line);
}

int banner_main(int argc, char **argv)
{
	compose();
	if (!nlines)
		return 0;

	const char *term = getenv("TERM");
	if ((argc > 1 && !strcmp(argv[1], "--plain")) || !isatty(STDOUT_FILENO) ||
	    getenv("KDOS_NO_ANIM") || !term || !*term || !strcmp(term, "dumb")) {
		plain(0);
		return 0;
	}

	/* One row spare for the prompt: on a 33-row TTY the banner is close to
	 * the full height, and animating something that scrolls looks broken.
	 * The oracle costs two more rows, so it is height-guarded with them. */
	int want = nlines + 2;
	if (nlines >= term_rows()) {
		plain(0);
		return 0;
	}

	const char *d = getenv("KDOS_BANNER_DELAY");
	double delay = d && *d ? atof(d) : 0.012;
	animate(delay);
	if (want < term_rows())
		oracle_line();
	return 0;
}
