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
 * keeping: the logo can never come out missing, and its height does not have
 * to be smaller than the info column's.
 *
 * It gets out of the way when it should: no animation unless stdout is a tty,
 * and never when KDOS_NO_ANIM is set, TERM is dumb, or the terminal is too
 * short for the banner (scrolling mid-animation looks like a glitch, not a
 * feature). A terminal whose height is known and too short for the logo, the
 * fetch and a prompt row gets the fetch alone, and one still too short for the
 * fetch and a prompt row gets the fetch's head: whole blank-line-separated
 * sections are dropped from the end (the tagline, HARDWARE, SOFTWARE, ...)
 * until the rest and a prompt row fit, or, if no section boundary fits, the
 * fetch is cut at one row short of the screen. The user@host line, the rule,
 * SYSTEM and the OS line come first, so an 80x25 console always keeps them;
 * printed whole, the 30-line fetch scrolls them off its top. Any keypress
 * skips the rest instantly.
 *
 * The frame delay is a nanosleep and the keypress check a zero-timeout poll
 * on stdin: nothing in the animation forks, on a path that runs at every
 * login and once per frame.
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

/* Visible text with every SGR sequence removed, in process and by hand:
 * dropping the escape byte alone leaves the "[1;32m" visible for a frame, and
 * an external filter per line is a fork per line on every login. */
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

/* 0 when no height is known (stdout redirected and $LINES unset): a banner
 * written to a file or a pipe has no screen to fit and keeps its logo. */
static int term_rows(void)
{
	const char *l = getenv("LINES");
	if (l && atoi(l) > 0)
		return atoi(l);
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row)
		return ws.ws_row;
	return 0;
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
 * fastfetch takes its parent process for the shell and walks on from there to
 * the terminal, and this program is that parent: without FFTS_IGNORE_PARENT=1
 * the banner reports "kdos-banner" as the shell. --pipe false keeps the
 * colours when stdout is a pipe, which it is.
 */
static char *info_text(void)
{
	static char buf[1 << 16];
	buf[0] = 0;

	if (kb_have_prog("fastfetch")) {
		setenv("FFTS_IGNORE_PARENT", "1", 1);
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

/* A fetch line with nothing visible on it: the break between two sections. */
static int blank_line(const char *s)
{
	char *v = strip_ansi(s);
	int blank = 1;
	for (const char *p = v; *p; p++)
		if (*p != ' ' && *p != '\t' && *p != '\r')
			blank = 0;
	free(v);
	return blank;
}

/* The fetch, or as much of its head as leaves a row for the prompt on a
 * screen `rows` high (0: no height known, all of it). The cut falls on the
 * last section break that fits, so a section is shown whole or not at all;
 * with no break that fits it falls one row short of the screen. */
static void push_fetch(char **info, int ninfo, int rows)
{
	int keep = ninfo;
	if (rows > 0 && ninfo + 1 > rows) {
		keep = rows > 1 ? rows - 1 : 1;
		for (int i = rows - 1; i > 0; i--) {
			if (i < ninfo && blank_line(info[i])) {
				keep = i;
				break;
			}
		}
	}
	for (int i = 0; i < keep && i < ninfo; i++)
		push(kb_strdup(info[i]));
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

	int rows = term_rows();
	if (!nlogo) {
		push_fetch(info, ninfo, rows);
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
	int stacked = INDENT + w + GAP + iw > term_cols();

	/* The logo only with a row to spare for the prompt: stacked it costs
	 * its own height and a blank line, side by side whatever it is taller
	 * than the fetch. Without that room the fetch is the part kept, since
	 * it carries the user@host and OS lines a scrolled screen would lose. */
	int need = stacked ? nlogo + 1 + ninfo : (nlogo > ninfo ? nlogo : ninfo);
	if (rows > 0 && ninfo && need + 1 > rows) {
		push_fetch(info, ninfo, rows);
		return;
	}

	if (stacked) {
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
	 * the full height, and animating something that scrolls looks broken. */
	int rows = term_rows();
	if (rows <= 0)
		rows = 24;
	if (nlines >= rows) {
		plain(0);
		return 0;
	}

	const char *d = getenv("KDOS_BANNER_DELAY");
	double delay = d && *d ? atof(d) : 0.012;
	animate(delay);
	return 0;
}
