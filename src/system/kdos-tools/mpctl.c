/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mpctl — the music, over mpd's own protocol or MPRIS
 *
 *   kdos-mpctl toggle | stop | next | prev
 *   kdos-mpctl mpris
 *
 * mpd SPEAKS ITS OWN PROTOCOL AND NO MPRIS, and Plasma reaches a player only
 * through MPRIS: the media keys, the Media Player applet, the lock screen's
 * controls and KDE Connect all ask the session bus. `mpris` is the bridge —
 * a small service, started at login by its autostart entry, that owns
 * org.mpris.MediaPlayer2.mpd while mpd is running and answers for it, and
 * follows mpd's own `idle` so the applet's title and play state change when
 * mpd's do, without polling.
 *
 * The transport verbs are what a script or a KRunner line runs: they reach
 * mpd directly and, being asked anyway, the other MPRIS players too, ranked
 * with mpd on one scale (see `transport` below for which one gets the key).
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
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
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

/* More players than anybody runs at once; the rest are not considered. A bus
 * name is at most 255 bytes, and an MPRIS one longer than this is not read. */
#define MB_MAX 16
#define MB_NAME 128

/*
 * The player to send a key to, by PlaybackStatus: the first one Playing, else
 * the first Paused, else the first on the bus. Returns its rank — 3 Playing,
 * 2 Paused, 1 anything else — or 0 for no player; `transport` weighs mpd's
 * state on the same scale. The bus is kdos-tools' own (bus.c).
 */
static int mb_pick(KdtBus *b, char *out, size_t cap)
{
	char names[MB_MAX][MB_NAME];
	KdtMsg *reply = NULL;
	const char *s;
	int n = 0, best = 0;

	out[0] = '\0';
	if (kdt_bus_call(b, "org.freedesktop.DBus", "/org/freedesktop/DBus",
			 "org.freedesktop.DBus", "ListNames", &reply, NULL, 0,
			 "") != 0)
		return 0;
	if (kdt_bus_enter(b, reply, 'a', "s") > 0) {
		while (n < MB_MAX && kdt_bus_read(b, reply, "s", &s) > 0)
			if (s && !strncmp(s, MB_PREFIX, strlen(MB_PREFIX)) &&
			    strlen(s) < sizeof(names[0]))
				memcpy(names[n++], s, strlen(s) + 1);
		kdt_bus_leave(b, reply);
	}
	kdt_bus_free(b, reply);

	for (int i = 0; i < n && best < 3; i++) {
		char *st = kdt_bus_get_prop_s(b, names[i], MB_OBJ, MB_IFACE,
					      "PlaybackStatus", NULL, 0);
		int rank = 1;

		if (st && !strcmp(st, "Playing"))
			rank = 3;
		else if (st && !strcmp(st, "Paused"))
			rank = 2;
		free(st);
		if (rank > best) {
			best = rank;
			snprintf(out, cap, "%.*s", MB_NAME - 1, names[i]);
		}
	}
	return best;
}

static int mb_do(KdtBus *b, const char *player, const char *method)
{
	if (kdt_bus_call(b, player, MB_OBJ, MB_IFACE, method, NULL, NULL, 0,
			 "") != 0) {
		fprintf(stderr, "kdos-mpctl: %s did not take %s\n", player,
			method);
		return 1;
	}
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: kdos-mpctl toggle|stop|next|prev\n"
			"       kdos-mpctl mpris\n");
}

/* ── the MPRIS bridge ──────────────────────────────────────────────────── */

#define MPRIS_NAME "org.mpris.MediaPlayer2.mpd"
#define MPRIS_ROOT "org.mpris.MediaPlayer2"
#define MPRIS_RETRY_MS 5000

typedef struct {
	KdtBus *b;
	Mp ctl;			/* commands and `status`              */
	Mp idle;		/* parked in `idle`, says what moved  */
	int up;
	char state[16];		/* play, pause or stop                */
	int volume;		/* 0..100, or -1 for no mixer         */
	int random, repeat;
	long long elapsed_us, length_us;
	long song_id;
	char title[MP_LINE], artist[MP_LINE], album[MP_LINE], file[MP_LINE];
} Mpris;

/* `status` and `currentsong`, into the model. */
static int mpris_refresh(Mpris *x)
{
	char line[MP_LINE];
	int r;

	x->state[0] = x->title[0] = x->artist[0] = x->album[0] =
		x->file[0] = '\0';
	x->volume = -1;
	x->random = x->repeat = 0;
	x->elapsed_us = x->length_us = 0;
	x->song_id = -1;
	if (mp_send(&x->ctl, "status\n") != 0)
		return -1;
	while ((r = mp_line(&x->ctl, line, sizeof(line))) == 1) {
		if (!strcmp(line, "OK"))
			break;
		if (!strncmp(line, "ACK ", 4))
			return -1;
		if (!strncmp(line, "state: ", 7))
			snprintf(x->state, sizeof(x->state), "%.15s", line + 7);
		else if (!strncmp(line, "volume: ", 8))
			x->volume = atoi(line + 8);
		else if (!strncmp(line, "random: ", 8))
			x->random = atoi(line + 8);
		else if (!strncmp(line, "repeat: ", 8))
			x->repeat = atoi(line + 8);
		else if (!strncmp(line, "elapsed: ", 9))
			x->elapsed_us = (long long)(atof(line + 9) * 1e6);
		else if (!strncmp(line, "duration: ", 10))
			x->length_us = (long long)(atof(line + 10) * 1e6);
		else if (!strncmp(line, "songid: ", 8))
			x->song_id = atol(line + 8);
	}
	if (r != 1)
		return -1;
	if (mp_send(&x->ctl, "currentsong\n") != 0)
		return -1;
	while ((r = mp_line(&x->ctl, line, sizeof(line))) == 1) {
		if (!strcmp(line, "OK"))
			return 0;
		if (!strncmp(line, "ACK ", 4))
			return -1;
		if (!strncmp(line, "Title: ", 7))
			snprintf(x->title, sizeof(x->title), "%s", line + 7);
		else if (!strncmp(line, "Artist: ", 8))
			snprintf(x->artist, sizeof(x->artist), "%s", line + 8);
		else if (!strncmp(line, "Album: ", 7))
			snprintf(x->album, sizeof(x->album), "%s", line + 7);
		else if (!strncmp(line, "file: ", 6))
			snprintf(x->file, sizeof(x->file), "%s", line + 6);
	}
	return -1;
}

static const char *mpris_status(const Mpris *x)
{
	return !strcmp(x->state, "play")    ? "Playing"
	       : !strcmp(x->state, "pause") ? "Paused"
					    : "Stopped";
}

static void mpris_trackid(const Mpris *x, char *out, size_t n)
{
	if (x->song_id >= 0)
		snprintf(out, n, "/org/mpd/Track/%ld", x->song_id);
	else
		snprintf(out, n, "/org/mpris/MediaPlayer2/TrackList/NoTrack");
}

static void mpris_metadata(Mpris *x, KdtMsg *m)
{
	KdtBus *b = x->b;
	char tid[64];
	/* A track with no Title tag is still a track; its file name is what
	 * every mpd client shows for it. */
	const char *title = *x->title ? x->title
			    : *x->file ? (strrchr(x->file, '/')
					  ? strrchr(x->file, '/') + 1 : x->file)
				       : "";

	mpris_trackid(x, tid, sizeof(tid));
	kdt_bus_open_container(b, m, 'a', "{sv}");
	kdt_bus_append(b, m, "{sv}", "mpris:trackid", "o", tid);
	if (x->length_us > 0)
		kdt_bus_append(b, m, "{sv}", "mpris:length", "x",
			       (int64_t)x->length_us);
	if (*title)
		kdt_bus_append(b, m, "{sv}", "xesam:title", "s", title);
	if (*x->artist)
		kdt_bus_append(b, m, "{sv}", "xesam:artist", "as", 1,
			       x->artist);
	if (*x->album)
		kdt_bus_append(b, m, "{sv}", "xesam:album", "s", x->album);
	kdt_bus_close_container(b, m);
}

static const char *const ROOT_PROPS[] = {
	"CanQuit", "CanRaise", "HasTrackList", "Identity",
	"SupportedUriSchemes", "SupportedMimeTypes",
};
static const char *const PLAYER_PROPS[] = {
	"PlaybackStatus", "LoopStatus", "Rate", "Shuffle", "Metadata",
	"Volume", "Position", "MinimumRate", "MaximumRate", "CanGoNext",
	"CanGoPrevious", "CanPlay", "CanPause", "CanSeek", "CanControl",
};

/* One property's value, as the variant a Get answers with. 0, or -1 for a
 * name the interface does not have. */
static int mpris_prop(Mpris *x, KdtMsg *m, const char *iface,
		      const char *name)
{
	KdtBus *b = x->b;
	int has = *x->state && strcmp(x->state, "stop");

	if (!strcmp(iface, MPRIS_ROOT)) {
		if (!strcmp(name, "Identity"))
			return kdt_bus_append(b, m, "v", "s",
					      "Music Player Daemon");
		if (!strcmp(name, "SupportedUriSchemes"))
			return kdt_bus_append(b, m, "v", "as", 1, "file");
		if (!strcmp(name, "SupportedMimeTypes"))
			return kdt_bus_append(b, m, "v", "as", 0);
		if (!strcmp(name, "CanQuit") || !strcmp(name, "CanRaise") ||
		    !strcmp(name, "HasTrackList"))
			return kdt_bus_append(b, m, "v", "b", 0);
		return -1;
	}
	if (strcmp(iface, MPRIS_ROOT ".Player"))
		return -1;
	if (!strcmp(name, "PlaybackStatus"))
		return kdt_bus_append(b, m, "v", "s", mpris_status(x));
	if (!strcmp(name, "LoopStatus"))
		return kdt_bus_append(b, m, "v", "s",
				      x->repeat ? "Playlist" : "None");
	if (!strcmp(name, "Rate") || !strcmp(name, "MinimumRate") ||
	    !strcmp(name, "MaximumRate"))
		return kdt_bus_append(b, m, "v", "d", 1.0);
	if (!strcmp(name, "Shuffle"))
		return kdt_bus_append(b, m, "v", "b", x->random);
	if (!strcmp(name, "Volume"))
		return kdt_bus_append(b, m, "v", "d",
				      x->volume < 0 ? 0.0 : x->volume / 100.0);
	if (!strcmp(name, "Position"))
		return kdt_bus_append(b, m, "v", "x", (int64_t)x->elapsed_us);
	if (!strcmp(name, "Metadata")) {
		kdt_bus_open_container(b, m, 'v', "a{sv}");
		mpris_metadata(x, m);
		return kdt_bus_close_container(b, m);
	}
	if (!strcmp(name, "CanSeek"))
		return kdt_bus_append(b, m, "v", "b", has);
	if (!strcmp(name, "CanGoNext") || !strcmp(name, "CanGoPrevious") ||
	    !strcmp(name, "CanPlay") || !strcmp(name, "CanPause") ||
	    !strcmp(name, "CanControl"))
		return kdt_bus_append(b, m, "v", "b", 1);
	return -1;
}

static void mpris_all(Mpris *x, KdtMsg *m, const char *iface)
{
	const char *const *names = !strcmp(iface, MPRIS_ROOT) ? ROOT_PROPS
								: PLAYER_PROPS;
	size_t n = !strcmp(iface, MPRIS_ROOT)
			   ? sizeof(ROOT_PROPS) / sizeof(ROOT_PROPS[0])
			   : sizeof(PLAYER_PROPS) / sizeof(PLAYER_PROPS[0]);

	kdt_bus_open_container(x->b, m, 'a', "{sv}");
	for (size_t i = 0; i < n; i++) {
		kdt_bus_open_container(x->b, m, 'e', "sv");
		kdt_bus_append(x->b, m, "s", names[i]);
		mpris_prop(x, m, iface, names[i]);
		kdt_bus_close_container(x->b, m);
	}
	kdt_bus_close_container(x->b, m);
}

static const char MPRIS_XML[] =
	"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
	"1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/"
	"introspect.dtd\">\n<node>\n"
	" <interface name=\"org.freedesktop.DBus.Introspectable\">"
	"<method name=\"Introspect\"><arg type=\"s\" direction=\"out\"/>"
	"</method></interface>\n"
	" <interface name=\"org.freedesktop.DBus.Properties\">"
	"<method name=\"Get\"><arg type=\"s\" direction=\"in\"/>"
	"<arg type=\"s\" direction=\"in\"/><arg type=\"v\" "
	"direction=\"out\"/></method><method name=\"GetAll\"><arg "
	"type=\"s\" direction=\"in\"/><arg type=\"a{sv}\" "
	"direction=\"out\"/></method><method name=\"Set\"><arg type=\"s\" "
	"direction=\"in\"/><arg type=\"s\" direction=\"in\"/><arg "
	"type=\"v\" direction=\"in\"/></method><signal "
	"name=\"PropertiesChanged\"><arg type=\"s\"/><arg "
	"type=\"a{sv}\"/><arg type=\"as\"/></signal></interface>\n"
	" <interface name=\"org.mpris.MediaPlayer2\"><method name=\"Raise\"/>"
	"<method name=\"Quit\"/><property name=\"CanQuit\" type=\"b\" "
	"access=\"read\"/><property name=\"CanRaise\" type=\"b\" "
	"access=\"read\"/><property name=\"HasTrackList\" type=\"b\" "
	"access=\"read\"/><property name=\"Identity\" type=\"s\" "
	"access=\"read\"/><property name=\"SupportedUriSchemes\" "
	"type=\"as\" access=\"read\"/><property "
	"name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>"
	"</interface>\n"
	" <interface name=\"org.mpris.MediaPlayer2.Player\">"
	"<method name=\"Next\"/><method name=\"Previous\"/>"
	"<method name=\"Pause\"/><method name=\"PlayPause\"/>"
	"<method name=\"Stop\"/><method name=\"Play\"/>"
	"<method name=\"Seek\"><arg type=\"x\" direction=\"in\"/></method>"
	"<method name=\"SetPosition\"><arg type=\"o\" direction=\"in\"/>"
	"<arg type=\"x\" direction=\"in\"/></method>"
	"<method name=\"OpenUri\"><arg type=\"s\" direction=\"in\"/>"
	"</method><signal name=\"Seeked\"><arg type=\"x\"/></signal>"
	"<property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>"
	"<property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>"
	"<property name=\"Rate\" type=\"d\" access=\"read\"/>"
	"<property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>"
	"<property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>"
	"<property name=\"Volume\" type=\"d\" access=\"readwrite\"/>"
	"<property name=\"Position\" type=\"x\" access=\"read\"/>"
	"<property name=\"MinimumRate\" type=\"d\" access=\"read\"/>"
	"<property name=\"MaximumRate\" type=\"d\" access=\"read\"/>"
	"<property name=\"CanGoNext\" type=\"b\" access=\"read\"/>"
	"<property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>"
	"<property name=\"CanPlay\" type=\"b\" access=\"read\"/>"
	"<property name=\"CanPause\" type=\"b\" access=\"read\"/>"
	"<property name=\"CanSeek\" type=\"b\" access=\"read\"/>"
	"<property name=\"CanControl\" type=\"b\" access=\"read\"/>"
	"</interface>\n</node>\n";

/* A verb to mpd on the control connection, answered on the bus. */
static int mpris_verb(Mpris *x, KdtMsg *call, const char *cmd)
{
	KdtMsg *r;

	if (mp_do(&x->ctl, cmd) != 0)
		return kdt_bus_reply_error(x->b, call,
					   "org.freedesktop.DBus.Error.Failed",
					   "mpd refused the command");
	r = kdt_bus_new_reply(x->b, call);
	kdt_bus_send(x->b, r);
	return 1;
}

static int mpris_call(KdtMsg *m, void *ud, void *err)
{
	Mpris *x = ud;
	KdtBus *b = x->b;
	const char *iface = kdt_bus_interface(b, m);
	const char *member = kdt_bus_member(b, m);
	KdtMsg *r;
	char cmd[64];

	(void)err;
	if (!x->up)
		return kdt_bus_reply_error(b, m,
					   "org.freedesktop.DBus.Error.Failed",
					   "mpd is not running");
	if (!strcmp(iface, "org.freedesktop.DBus.Introspectable") &&
	    !strcmp(member, "Introspect")) {
		r = kdt_bus_new_reply(b, m);
		kdt_bus_append(b, r, "s", MPRIS_XML);
		kdt_bus_send(b, r);
		return 1;
	}
	if (!strcmp(iface, "org.freedesktop.DBus.Properties")) {
		const char *pi = NULL, *pn = NULL;

		if (!strcmp(member, "GetAll") &&
		    kdt_bus_read(b, m, "s", &pi) > 0) {
			if (strcmp(pi, MPRIS_ROOT) &&
			    strcmp(pi, MPRIS_ROOT ".Player"))
				return kdt_bus_reply_error(b, m,
					"org.freedesktop.DBus.Error.UnknownInterface",
					"no such interface");
			r = kdt_bus_new_reply(b, m);
			mpris_all(x, r, pi);
			kdt_bus_send(b, r);
			return 1;
		}
		if (!strcmp(member, "Get") &&
		    kdt_bus_read(b, m, "ss", &pi, &pn) > 0) {
			r = kdt_bus_new_reply(b, m);
			if (mpris_prop(x, r, pi, pn) != 0) {
				kdt_bus_free(b, r);
				return kdt_bus_reply_error(b, m,
					"org.freedesktop.DBus.Error.UnknownProperty",
					"no such property");
			}
			kdt_bus_send(b, r);
			return 1;
		}
		if (!strcmp(member, "Set") &&
		    kdt_bus_read(b, m, "ss", &pi, &pn) > 0) {
			if (!strcmp(pn, "Volume")) {
				double v = 0;

				if (kdt_bus_enter(b, m, 'v', "d") <= 0 ||
				    kdt_bus_read(b, m, "d", &v) <= 0)
					return kdt_bus_reply_error(b, m,
						"org.freedesktop.DBus.Error.InvalidArgs",
						"Volume is a double");
				snprintf(cmd, sizeof(cmd), "setvol %d\n",
					 v < 0 ? 0 : v > 1 ? 100
						 : (int)(v * 100 + 0.5));
				return mpris_verb(x, m, cmd);
			}
			if (!strcmp(pn, "Shuffle")) {
				int v = 0;

				if (kdt_bus_enter(b, m, 'v', "b") <= 0 ||
				    kdt_bus_read(b, m, "b", &v) <= 0)
					return kdt_bus_reply_error(b, m,
						"org.freedesktop.DBus.Error.InvalidArgs",
						"Shuffle is a boolean");
				return mpris_verb(x, m, v ? "random 1\n"
							  : "random 0\n");
			}
			if (!strcmp(pn, "LoopStatus")) {
				const char *v = NULL;

				if (kdt_bus_enter(b, m, 'v', "s") <= 0 ||
				    kdt_bus_read(b, m, "s", &v) <= 0)
					return kdt_bus_reply_error(b, m,
						"org.freedesktop.DBus.Error.InvalidArgs",
						"LoopStatus is a string");
				return mpris_verb(x, m, strcmp(v, "None")
							? "repeat 1\n"
							: "repeat 0\n");
			}
			return kdt_bus_reply_error(b, m,
				"org.freedesktop.DBus.Error.PropertyReadOnly",
				"read-only property");
		}
		return 0;
	}
	if (!strcmp(iface, MPRIS_ROOT) &&
	    (!strcmp(member, "Raise") || !strcmp(member, "Quit"))) {
		r = kdt_bus_new_reply(b, m);
		kdt_bus_send(b, r);
		return 1;
	}
	if (strcmp(iface, MPRIS_ROOT ".Player"))
		return 0;
	if (!strcmp(member, "Next"))
		return mpris_verb(x, m, "next\n");
	if (!strcmp(member, "Previous"))
		return mpris_verb(x, m, "previous\n");
	if (!strcmp(member, "Pause"))
		return mpris_verb(x, m, "pause 1\n");
	if (!strcmp(member, "Stop"))
		return mpris_verb(x, m, "stop\n");
	if (!strcmp(member, "Play"))
		return mpris_verb(x, m, !strcmp(x->state, "pause")
					    ? "pause 0\n" : "play\n");
	if (!strcmp(member, "PlayPause"))
		return mpris_verb(x, m, !strcmp(x->state, "play") ? "pause 1\n"
					: !strcmp(x->state, "pause")
						? "pause 0\n" : "play\n");
	if (!strcmp(member, "Seek")) {
		int64_t off = 0;

		if (kdt_bus_read(b, m, "x", &off) <= 0)
			return 0;
		snprintf(cmd, sizeof(cmd), "seekcur %+.3f\n", off / 1e6);
		return mpris_verb(x, m, cmd);
	}
	if (!strcmp(member, "SetPosition")) {
		const char *tid = NULL;
		int64_t pos = 0;
		char now[64];

		if (kdt_bus_read(b, m, "ox", &tid, &pos) <= 0)
			return 0;
		mpris_trackid(x, now, sizeof(now));
		/* The spec's rule: a position for another track is ignored. */
		if (strcmp(tid, now) || pos < 0) {
			r = kdt_bus_new_reply(b, m);
			kdt_bus_send(b, r);
			return 1;
		}
		snprintf(cmd, sizeof(cmd), "seekcur %.3f\n", pos / 1e6);
		return mpris_verb(x, m, cmd);
	}
	if (!strcmp(member, "OpenUri"))
		return kdt_bus_reply_error(b, m,
			"org.freedesktop.DBus.Error.NotSupported",
			"mpd plays what is in its own library");
	return 0;
}

/* What moved, told to whoever watches: the whole Player state each time,
 * because it is a handful of values and a client merges what it is sent. */
static void mpris_changed(Mpris *x)
{
	static const char *const MOVED[] = { "PlaybackStatus", "Metadata",
					     "Volume", "Shuffle",
					     "LoopStatus", "CanSeek" };
	KdtMsg *s = kdt_bus_new_signal(x->b, MB_OBJ,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged");

	if (!s)
		return;
	kdt_bus_append(x->b, s, "s", MPRIS_ROOT ".Player");
	kdt_bus_open_container(x->b, s, 'a', "{sv}");
	for (size_t i = 0; i < sizeof(MOVED) / sizeof(MOVED[0]); i++) {
		kdt_bus_open_container(x->b, s, 'e', "sv");
		kdt_bus_append(x->b, s, "s", MOVED[i]);
		mpris_prop(x, s, MPRIS_ROOT ".Player", MOVED[i]);
		kdt_bus_close_container(x->b, s);
	}
	kdt_bus_close_container(x->b, s);
	kdt_bus_append(x->b, s, "as", 0);
	kdt_bus_send(x->b, s);
}

static void mpris_down(Mpris *x)
{
	if (x->up)
		kdt_bus_release_name(x->b, MPRIS_NAME);
	if (x->ctl.fd >= 0)
		close(x->ctl.fd);
	if (x->idle.fd >= 0)
		close(x->idle.fd);
	x->ctl.fd = x->idle.fd = -1;
	x->up = 0;
}

/* Two connections: one answers commands, the other waits in `idle`, which
 * holds its connection until something changes. */
static int mpris_up(Mpris *x)
{
	if (mp_connect(&x->ctl) != 0 || mp_hello(&x->ctl) != 0 ||
	    mp_connect(&x->idle) != 0 || mp_hello(&x->idle) != 0 ||
	    mpris_refresh(x) != 0 ||
	    mp_send(&x->idle, "idle player mixer options\n") != 0 ||
	    kdt_bus_request_name(x->b, MPRIS_NAME) != 0) {
		mpris_down(x);
		return -1;
	}
	x->up = 1;
	return 0;
}

static volatile sig_atomic_t mpris_stop;

static void mpris_term(int sig)
{
	(void)sig;
	mpris_stop = 1;
}

/*
 * THE BRIDGE'S LIFE IS THE LOGIN'S. It outlives mpd: with no mpd it holds no
 * name and looks again every five seconds, so mpd started, stopped and
 * restarted under it keeps its media keys. It ends with SIGTERM or with the
 * session bus.
 */
static int mpris_main(void)
{
	Mpris x = { 0 };

	x.ctl.fd = x.idle.fd = -1;
	x.b = kdt_bus_open(0);
	if (!x.b) {
		fprintf(stderr, "kdos-mpctl: the session bus could not be "
				"reached\n");
		return 1;
	}
	if (kdt_bus_serve(x.b, MB_OBJ, mpris_call, &x) != 0) {
		fprintf(stderr, "kdos-mpctl: %s could not be served\n",
			MB_OBJ);
		kdt_bus_close(x.b);
		return 1;
	}
	signal(SIGTERM, mpris_term);
	signal(SIGINT, mpris_term);
	signal(SIGPIPE, SIG_IGN);
	while (!mpris_stop) {
		struct pollfd p[2];
		short ev = 0;
		int n = 1, r;

		if (!x.up)
			mpris_up(&x);
		p[0].fd = kdt_bus_fd(x.b, &ev);
		p[0].events = ev;
		p[0].revents = 0;
		if (x.up) {
			p[1].fd = x.idle.fd;
			p[1].events = POLLIN;
			p[1].revents = 0;
			n = 2;
		}
		r = poll(p, (nfds_t)n, x.up ? -1 : MPRIS_RETRY_MS);
		if (r < 0 && errno != EINTR)
			break;
		if (r > 0 && p[0].revents && kdt_bus_dispatch(x.b) != 0)
			break;
		if (n == 2 && r > 0 && p[1].revents) {
			char line[MP_LINE];
			int got;

			while ((got = mp_line(&x.idle, line, sizeof(line))) == 1 &&
			       strcmp(line, "OK"))
				;
			if (got != 1 || mpris_refresh(&x) != 0 ||
			    mp_send(&x.idle, "idle player mixer options\n") != 0) {
				mpris_down(&x);
				continue;
			}
			mpris_changed(&x);
		}
		/* Anything a handler queued is sent before the next wait. */
		if (kdt_bus_dispatch(x.b) != 0)
			break;
	}
	mpris_down(&x);
	kdt_bus_close(x.b);
	return 0;
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
	KdtBus *b = NULL;
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
	b = kdt_bus_open(0);
	if (b)
		got = mb_pick(b, player, sizeof(player));
	if (have_mpd && mpd_rank >= got) {
		rc = mp_do(m, cmd);
	} else if (got) {
		rc = mb_do(b, player, V[i].mpris);
	} else {
		fprintf(stderr, "kdos-mpctl: no player on this login\n");
		rc = 1;
	}
	kdt_bus_close(b);
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
	if (!strcmp(verb, "mpris"))
		return argc == 2 ? mpris_main() : (usage(), 2);
	have_mpd = mp_connect(&m) == 0 && mp_hello(&m) == 0;
	if (!have_mpd && m.fd >= 0) {
		close(m.fd);
		m.fd = -1;
	}
	if ((rc = transport(&m, have_mpd, verb)) < 0) {
		usage();
		rc = 1;
	}
	if (have_mpd)
		close(m.fd);
	return rc;
}
