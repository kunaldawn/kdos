/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mpctl — the music, over mpd's own protocol
 *
 * A MEDIA KEY RUNS A PROGRAM, and this is that program. What "next" means
 * belongs to the player, so the session's chord table names a command and
 * nothing about the session knows what a track is.
 *
 * THE UNIX SOCKET AND NOTHING ELSE. mpd will listen on TCP if it is told to,
 * and a client that fell back to it would be a client that reached a music
 * daemon on somebody else's machine because a name resolved. The socket under
 * `$XDG_RUNTIME_DIR` belongs to this login and cannot be reached from another.
 *
 * THE RESPONSE ENDS AT `OK`, NOT AT END OF FILE. mpd holds the connection open
 * between commands, so a read that waited for EOF would wait forever — which
 * is the one difference between this and every other small client in this
 * directory.
 *
 * `watch` WRITES A LINE AND SLEEPS IN `idle`. Polling a music daemon for a
 * title that changes every few minutes is a wakeup a battery pays for; `idle`
 * is mpd's own answer and it costs nothing until something happens.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "kdos-tools.h"

/* One line of mpd's protocol is a key and a value; a title is the longest
 * thing that arrives and 512 is past any of them. */
#define MP_LINE 512

/* What the panel reads. Longer than the field is ever drawn, because the
 * truncation is the panel's decision and not this program's. */
#define MP_NOW 256

typedef struct {
	int fd;
	char buf[4096];
	size_t len;
} Mp;

static int mp_connect(Mp *m)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	int fd;

	m->len = 0;
	if (!rt || !*rt)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/mpd/socket", rt);
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	m->fd = fd;
	return 0;
}

/*
 * One line, newline stripped. Returns 1 for a line, 0 at end of file and -1 on
 * an error; a line longer than the buffer is truncated rather than split,
 * because a protocol line this program does not understand is one it drops.
 */
static int mp_line(Mp *m, char *out, size_t cap)
{
	for (;;) {
		char *nl = memchr(m->buf, '\n', m->len);
		ssize_t n;

		if (nl) {
			size_t got = (size_t)(nl - m->buf);
			size_t take = got < cap - 1 ? got : cap - 1;

			memcpy(out, m->buf, take);
			out[take] = '\0';
			m->len -= got + 1;
			memmove(m->buf, nl + 1, m->len);
			return 1;
		}
		if (m->len == sizeof(m->buf)) {
			m->len = 0;	/* a line nobody can use */
			continue;
		}
		n = read(m->fd, m->buf + m->len, sizeof(m->buf) - m->len);
		if (n == 0)
			return 0;
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		m->len += (size_t)n;
	}
}

static int mp_send(Mp *m, const char *cmd)
{
	size_t n = strlen(cmd);

	while (n) {
		ssize_t w = write(m->fd, cmd, n);

		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		cmd += w;
		n -= (size_t)w;
	}
	return 0;
}

/*
 * The greeting. Anything but `OK MPD ` on the first line is something else
 * listening on that path, and talking to it would be talking to a stranger.
 */
static int mp_hello(Mp *m)
{
	char line[MP_LINE];

	if (mp_line(m, line, sizeof(line)) != 1)
		return -1;
	return strncmp(line, "OK MPD ", 7) ? -1 : 0;
}

/* A control verb: send it, read to the terminator, and let `ACK` speak for
 * itself — mpd's error text names the command and the reason, and rewriting it
 * here would be a second, worse copy of it. */
static int mp_do(Mp *m, const char *cmd)
{
	char line[MP_LINE];
	int r;

	if (mp_send(m, cmd) != 0)
		return 1;
	while ((r = mp_line(m, line, sizeof(line))) == 1) {
		if (!strcmp(line, "OK"))
			return 0;
		if (!strncmp(line, "ACK ", 4)) {
			fprintf(stderr, "kdos-mpctl: %s\n", line + 4);
			return 1;
		}
	}
	return 1;
}

/* Control characters never reach the panel: the field is drawn into a cell
 * grid, and one escape in a track title would be a track title that moves the
 * cursor. */
static void sanitise(char *s)
{
	for (; *s; s++)
		if ((unsigned char)*s < 0x20 || *s == 0x7f)
			*s = ' ';
}

/*
 * One round trip for the state and the song, formatted into the line the panel
 * draws. A stopped player writes an EMPTY line rather than the word "stopped":
 * the panel shows no field at all then, which is what a bar with nothing
 * playing should look like.
 *
 * ASCII, AND THAT IS THE FILE'S CONTRACT. The reader is a cell grid whose glyph
 * tier this program cannot see — a console font with 512 glyphs draws `?` for
 * anything it lacks — so the line carries nothing that needs a tier to survive.
 */
static int mp_now(Mp *m, char *out, size_t cap)
{
	char line[MP_LINE], artist[MP_LINE] = "", title[MP_LINE] = "";
	char state[MP_LINE] = "";
	int r;

	out[0] = '\0';
	if (mp_send(m, "command_list_begin\nstatus\ncurrentsong\n"
			"command_list_end\n") != 0)
		return -1;
	while ((r = mp_line(m, line, sizeof(line))) == 1) {
		if (!strcmp(line, "OK"))
			break;
		if (!strncmp(line, "ACK ", 4))
			return -1;
		if (!strncmp(line, "state: ", 7))
			snprintf(state, sizeof(state), "%s", line + 7);
		else if (!strncmp(line, "Artist: ", 8))
			snprintf(artist, sizeof(artist), "%s", line + 8);
		else if (!strncmp(line, "Title: ", 7))
			snprintf(title, sizeof(title), "%s", line + 7);
		else if (!strncmp(line, "file: ", 6) && !*title)
			snprintf(title, sizeof(title), "%s", line + 6);
	}
	if (r != 1)
		return -1;
	if (strcmp(state, "play") && strcmp(state, "pause"))
		return 0;	/* stopped: no field */

	sanitise(artist);
	sanitise(title);
	if (*artist && *title)
		snprintf(out, cap, "%s %s - %s",
			 strcmp(state, "play") ? "||" : ">", artist, title);
	else if (*title)
		snprintf(out, cap, "%s %s",
			 strcmp(state, "play") ? "||" : ">", title);
	return 0;
}

/*
 * Where the panel looks. The directory is NOT created here: `kdos-con` makes
 * it 0700 before anything else runs and refuses one with any group or other
 * bit set, so a producer that got there first with the usual 0755 would stop
 * the desktop from starting. No directory means no session, and no session
 * means nothing to tell.
 */
static int now_path(char *buf, size_t cap)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	struct stat st;
	char dir[192];

	if (!rt || !*rt)
		return -1;
	snprintf(dir, sizeof(dir), "%s/kdos", rt);
	if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
		return -1;
	snprintf(buf, cap, "%s/nowplaying", dir);
	return 0;
}

/*
 * `idle` blocks until mpd has something to say, so the loop costs nothing
 * between tracks. `player` is a track change and `mixer` is the volume — the
 * two events the line can show — and asking for more would be waking for
 * database updates nobody is drawing.
 */
static int mp_watch(Mp *m)
{
	char path[256], now[MP_NOW], line[MP_LINE];


	if (now_path(path, sizeof(path)) != 0) {
		fprintf(stderr, "kdos-mpctl: no session to tell\n");
		return 1;
	}
	for (;;) {
		int r;

		if (mp_now(m, now, sizeof(now)) != 0)
			return 1;
		kb_write_file_atomic(path, now);

		if (mp_send(m, "idle player mixer\n") != 0)
			return 1;
		while ((r = mp_line(m, line, sizeof(line))) == 1)
			if (!strcmp(line, "OK") || !strncmp(line, "ACK ", 4))
				break;
		if (r != 1)
			return 0;	/* mpd went away; so do we */
	}
}

static void usage(void)
{
	fprintf(stderr,
		"usage: kdos-mpctl toggle|stop|next|prev|now|watch\n");
}

int mpctl_main(int argc, char **argv)
{
	const char *verb = argc > 1 ? argv[1] : NULL;
	Mp m = { 0 };
	int rc;

	if (!verb || !strcmp(verb, "-h") || !strcmp(verb, "--help")) {
		usage();
		return verb ? 0 : 1;
	}
	if (mp_connect(&m) != 0 || mp_hello(&m) != 0) {
		fprintf(stderr, "kdos-mpctl: no mpd on this login\n");
		if (m.fd > 0)
			close(m.fd);
		return 1;
	}

	if (!strcmp(verb, "toggle"))
		rc = mp_do(&m, "pause\n");	/* no argument: mpd toggles */
	else if (!strcmp(verb, "stop"))
		rc = mp_do(&m, "stop\n");
	else if (!strcmp(verb, "next"))
		rc = mp_do(&m, "next\n");
	else if (!strcmp(verb, "prev"))
		rc = mp_do(&m, "previous\n");
	else if (!strcmp(verb, "now")) {
		char now[MP_NOW];

		rc = mp_now(&m, now, sizeof(now)) == 0 ? 0 : 1;
		if (!rc && *now)
			printf("%s\n", now);
	} else if (!strcmp(verb, "watch")) {
		rc = mp_watch(&m);
	} else {
		usage();
		rc = 1;
	}
	close(m.fd);
	return rc;
}
