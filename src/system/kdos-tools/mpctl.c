/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mpctl — the music, over mpd's own protocol or MPRIS
 *
 * A MEDIA KEY RUNS A PROGRAM, and this is that program. What "next" means
 * belongs to the player, so the session's chord table names a command and
 * nothing about the session knows what a track is.
 *
 * TWO KINDS OF PLAYER. mpd speaks its own protocol on a socket; mpv, cmus and
 * every boxed player speak MPRIS on the session bus. The compositor's binding
 * takes the key before a focused player could see it, so the transport verbs
 * reach both from here — see `transport` below for which one gets the key. `now`
 * and `watch` are mpd's alone: the panel reads MPRIS itself, and the file they
 * write is how mpd reaches the same cell.
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
 *
 * AND IT OUTLIVES mpd. The session starts one watcher per login, and mpd is
 * started, stopped and restarted under it; a watcher that left with mpd would
 * leave the panel empty for the rest of the login. It exits only when the
 * session's runtime directory does.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
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
		/* send, not write: a daemon that closed the socket between two
		 * commands must be a failed send, not a SIGPIPE that kills the
		 * watcher waiting to reconnect. */
		ssize_t w = send(m->fd, cmd, n, MSG_NOSIGNAL);

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
 * Where the panel looks. The directory is NOT created here: it belongs to the
 * session and a producer that got there first with the usual 0755 would leave
 * it group-readable. No directory means no session, and no session means
 * nothing to tell.
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

/* How long a watcher with no mpd to talk to waits before it looks again: a
 * daemon started after the watcher is shown within this, and between looks the
 * watcher costs one wakeup. */
#define MP_RETRY 5

/*
 * One connection's worth: `idle` blocks until mpd has something to say, so the
 * loop costs nothing between tracks. `player` is a track change and `mixer` is
 * the volume — the two events the line can show — and asking for more would be
 * waking for database updates nobody is drawing. Returns when mpd goes away.
 */
static void mp_follow(Mp *m, const char *path)
{
	char now[MP_NOW], line[MP_LINE];

	for (;;) {
		int r;

		if (mp_now(m, now, sizeof(now)) != 0)
			return;
		kb_write_file_atomic(path, now);

		if (mp_send(m, "idle player mixer\n") != 0)
			return;
		while ((r = mp_line(m, line, sizeof(line))) == 1)
			if (!strcmp(line, "OK") || !strncmp(line, "ACK ", 4))
				break;
		if (r != 1)
			return;
	}
}

/*
 * Connect, follow, and when mpd goes away empty the line and wait for the next
 * one. The empty line is what takes the field off the panel: a title left in
 * the file would be drawn as playing by a daemon that no longer exists.
 */
static int mp_watch(void)
{
	char path[256];

	if (now_path(path, sizeof(path)) != 0) {
		fprintf(stderr, "kdos-mpctl: no session to tell\n");
		return 1;
	}
	for (;;) {
		Mp m = { .fd = -1 };

		if (mp_connect(&m) == 0) {
			if (mp_hello(&m) == 0) {
				mp_follow(&m, path);
				kb_write_file_atomic(path, "");
			}
			close(m.fd);
		}
		sleep(MP_RETRY);
		if (now_path(path, sizeof(path)) != 0)
			return 0;	/* the login ended */
	}
}

/* mpd's `state:` alone — `play`, `pause` or `stop` — for deciding who gets a
 * key. An empty state is a daemon that did not say, and counts as not
 * playing. */
static int mp_state(Mp *m, char *state, size_t cap)
{
	char line[MP_LINE];
	int r;

	state[0] = '\0';
	if (mp_send(m, "status\n") != 0)
		return -1;
	while ((r = mp_line(m, line, sizeof(line))) == 1) {
		if (!strcmp(line, "OK"))
			return 0;
		if (!strncmp(line, "ACK ", 4))
			return -1;
		if (!strncmp(line, "state: ", 7))
			snprintf(state, cap, "%s", line + 7);
	}
	return -1;
}

/* ── MPRIS ─────────────────────────────────────────────────────────────── */

#define MB_PREFIX "org.mpris.MediaPlayer2."
#define MB_OBJ "/org/mpris/MediaPlayer2"
#define MB_IFACE "org.mpris.MediaPlayer2.Player"

/* A player that does not answer within this costs the key and nothing else;
 * sd-bus's own default is 25 seconds of a key that seems to do nothing. */
#define MB_TIMEOUT_US 2000000ULL

/* More players than anybody runs at once; the rest are not considered. A bus
 * name is at most 255 bytes, and an MPRIS one longer than this is not read. */
#define MB_MAX 16
#define MB_NAME 128

/*
 * libbasu IS OPENED AT RUN TIME, NOT LINKED. This binary is also ksvc,
 * kdos-getty and the kdos-bootctl the initramfs copies with a hand-kept
 * library list, so a linked libbasu would be one more library every one of
 * them needs before it can start, for the sake of a media key. Without basu
 * there is no bus client, and the verbs fall to mpd alone. The handles are
 * sd-bus's `sd_bus *` and `sd_bus_message *`, carried as void pointers so this
 * file needs no sd-bus header either.
 */
typedef struct {
	void *lib;
	void *bus;
	int (*open_user)(void **bus);
	void *(*close_unref)(void *bus);
	int (*set_timeout)(void *bus, uint64_t usec);
	int (*call)(void *bus, const char *dest, const char *path,
		    const char *iface, const char *member, void *err,
		    void **reply, const char *types, ...);
	int (*get_string)(void *bus, const char *dest, const char *path,
			  const char *iface, const char *member, void *err,
			  char **ret);
	int (*enter)(void *msg, char type, const char *contents);
	int (*leave)(void *msg);
	int (*read_basic)(void *msg, char type, void *p);
	void *(*msg_unref)(void *msg);
} Mb;

static void mb_close(Mb *b)
{
	if (b->bus && b->close_unref)
		b->close_unref(b->bus);
	if (b->lib)
		dlclose(b->lib);
	memset(b, 0, sizeof(*b));
}

static int mb_open(Mb *b)
{
	memset(b, 0, sizeof(*b));
	b->lib = dlopen("libbasu.so.0", RTLD_NOW | RTLD_LOCAL);
	if (!b->lib)
		return -1;
	*(void **)&b->open_user = dlsym(b->lib, "sd_bus_open_user");
	*(void **)&b->close_unref = dlsym(b->lib, "sd_bus_flush_close_unref");
	*(void **)&b->set_timeout = dlsym(b->lib,
					  "sd_bus_set_method_call_timeout");
	*(void **)&b->call = dlsym(b->lib, "sd_bus_call_method");
	*(void **)&b->get_string = dlsym(b->lib, "sd_bus_get_property_string");
	*(void **)&b->enter = dlsym(b->lib, "sd_bus_message_enter_container");
	*(void **)&b->leave = dlsym(b->lib, "sd_bus_message_exit_container");
	*(void **)&b->read_basic = dlsym(b->lib, "sd_bus_message_read_basic");
	*(void **)&b->msg_unref = dlsym(b->lib, "sd_bus_message_unref");
	if (!b->open_user || !b->close_unref || !b->set_timeout || !b->call ||
	    !b->get_string || !b->enter || !b->leave || !b->read_basic ||
	    !b->msg_unref || b->open_user(&b->bus) < 0) {
		b->bus = NULL;
		mb_close(b);
		return -1;
	}
	b->set_timeout(b->bus, MB_TIMEOUT_US);
	return 0;
}

/*
 * The player to send a key to, by PlaybackStatus: the first one Playing, else
 * the first Paused, else the first on the bus. Returns its rank — 3 Playing,
 * 2 Paused, 1 anything else — or 0 for no player; `transport` weighs mpd's
 * state on the same scale.
 */
static int mb_pick(Mb *b, char *out, size_t cap)
{
	char names[MB_MAX][MB_NAME];
	void *reply = NULL;
	const char *s;
	int n = 0, best = 0;

	out[0] = '\0';
	if (b->call(b->bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		    "org.freedesktop.DBus", "ListNames", NULL, &reply,
		    NULL) < 0)
		return 0;
	if (b->enter(reply, 'a', "s") > 0) {
		while (n < MB_MAX && b->read_basic(reply, 's', &s) > 0)
			if (s && !strncmp(s, MB_PREFIX, strlen(MB_PREFIX)) &&
			    strlen(s) < sizeof(names[0]))
				memcpy(names[n++], s, strlen(s) + 1);
		b->leave(reply);
	}
	b->msg_unref(reply);

	for (int i = 0; i < n && best < 3; i++) {
		char *st = NULL;
		int rank = 1;

		if (b->get_string(b->bus, names[i], MB_OBJ, MB_IFACE,
				  "PlaybackStatus", NULL, &st) >= 0 && st) {
			if (!strcmp(st, "Playing"))
				rank = 3;
			else if (!strcmp(st, "Paused"))
				rank = 2;
		}
		free(st);
		if (rank > best) {
			best = rank;
			snprintf(out, cap, "%.*s", MB_NAME - 1, names[i]);
		}
	}
	return best;
}

static int mb_do(Mb *b, const char *player, const char *method)
{
	void *reply = NULL;

	if (b->call(b->bus, player, MB_OBJ, MB_IFACE, method, NULL, &reply,
		    NULL) < 0) {
		fprintf(stderr, "kdos-mpctl: %s did not take %s\n", player,
			method);
		return 1;
	}
	b->msg_unref(reply);
	return 0;
}

static void usage(void)
{
	fprintf(stderr,
		"usage: kdos-mpctl toggle|stop|next|prev|now|watch\n");
}

/*
 * WHO GETS THE KEY. mpd and the MPRIS players are ranked on one scale —
 * playing 3, paused 2, anything else 1 — and the highest takes it, mpd on a
 * tie: the key is aimed at the music that can be heard, then at the music
 * that was paused, and only then at whatever is on the bus. A stopped mpd
 * therefore yields to a Paused MPRIS player, and keeps the key over a stopped
 * one — its queue is the music this login set up. One player answers each
 * key, never two, or a toggle would start one while it paused the other.
 *
 * TOGGLE ON A STOPPED mpd IS `play`. mpd's argument-less `pause` flips play
 * and pause and does nothing at all in the stopped state, so the Play key
 * would be a key that did nothing.
 */
static int transport(Mp *m, int have_mpd, const char *verb)
{
	static const struct { const char *verb, *mpd, *mpris; } V[] = {
		{ "toggle", "pause\n", "PlayPause" },	/* no argument: mpd toggles */
		{ "stop", "stop\n", "Stop" },
		{ "next", "next\n", "Next" },
		{ "prev", "previous\n", "Previous" },
	};
	char state[MP_LINE] = "", player[MB_NAME] = "";
	const char *cmd;
	Mb b = { 0 };
	int got = 0, mpd_rank = 0, rc;
	size_t i;

	for (i = 0; i < sizeof(V) / sizeof(V[0]); i++)
		if (!strcmp(verb, V[i].verb))
			break;
	if (i == sizeof(V) / sizeof(V[0]))
		return -1;
	cmd = V[i].mpd;
	if (have_mpd) {
		if (mp_state(m, state, sizeof(state)) != 0)
			state[0] = '\0';
		mpd_rank = !strcmp(state, "play") ? 3 :
			   !strcmp(state, "pause") ? 2 : 1;
		if (mpd_rank == 1 && i == 0)
			cmd = "play\n";
		if (mpd_rank == 3)
			return mp_do(m, cmd);
	}
	if (mb_open(&b) == 0)
		got = mb_pick(&b, player, sizeof(player));
	if (have_mpd && mpd_rank >= got) {
		rc = mp_do(m, cmd);
	} else if (got) {
		rc = mb_do(&b, player, V[i].mpris);
	} else {
		fprintf(stderr, "kdos-mpctl: no player on this login\n");
		rc = 1;
	}
	if (b.lib)
		mb_close(&b);
	return rc;
}

int mpctl_main(int argc, char **argv)
{
	const char *verb = argc > 1 ? argv[1] : NULL;
	Mp m = { .fd = -1 };
	int rc, have_mpd;

	if (!verb || !strcmp(verb, "-h") || !strcmp(verb, "--help")) {
		usage();
		return verb ? 0 : 1;
	}
	if (!strcmp(verb, "watch"))
		return mp_watch();	/* it connects, and reconnects, itself */
	have_mpd = mp_connect(&m) == 0 && mp_hello(&m) == 0;
	if (!have_mpd && m.fd >= 0) {
		close(m.fd);
		m.fd = -1;
	}

	if (!strcmp(verb, "now")) {
		char now[MP_NOW];

		if (!have_mpd) {
			fprintf(stderr, "kdos-mpctl: no mpd on this login\n");
			return 1;
		}
		rc = mp_now(&m, now, sizeof(now)) == 0 ? 0 : 1;
		if (!rc && *now)
			printf("%s\n", now);
	} else if ((rc = transport(&m, have_mpd, verb)) < 0) {
		usage();
		rc = 1;
	}
	if (have_mpd)
		close(m.fd);
	return rc;
}
