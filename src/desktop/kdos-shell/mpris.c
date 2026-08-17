/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝  ╚═════╝ ╚═════╝ ╚══════╝
 * ---------------------------------
 *   mpris.c — what is playing, and the two keys that stop it
 *
 *   … │ ▶ Miles Davis — So What │ VOL 62%  41%  21:07
 *
 * The one panel widget every desktop has and this one did not. It matters more
 * here than elsewhere for a reason that is specific to KDOS: every media player
 * on this machine lives inside the appbox, and a boxed application's window is
 * on another workspace or minimised the moment you go back to work. MPRIS is
 * the only thing on this system that can pause it without finding it first —
 * and the box shares the session bus, so it works with no extra plumbing at
 * all.
 *
 * ONE PLAYER, THE ONE THAT IS PLAYING. A panel is one row: a list of players
 * would be a menu, and what a person aims at this cell for is "stop that". A
 * player that is actually Playing wins over one that is Paused, and the first
 * name on the bus breaks the tie.
 *
 * NOTHING BLOCKS THE FRAME. Two async round trips on a timer — ListNames to
 * find the players, GetAll to read the one that answers — and every action
 * (PlayPause, Next, Previous) is fire-and-forget. The tray taught this file
 * both halves of that lesson before it existed: no synchronous call from
 * inside a bus callback, and one GetAll rather than three Gets, because three
 * against a wedged player is three timeouts of dead panel.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "shell.h"

#define MP_PREFIX "org.mpris.MediaPlayer2."
#define MP_OBJ "/org/mpris/MediaPlayer2"
#define MP_IFACE "org.mpris.MediaPlayer2.Player"
#define MP_TIMEOUT_US 300000
#define MP_REFRESH_S 2

struct sh_mpris {
	sd_bus *bus;
	int owns_bus;			/* we opened it, so we close it */
	char name[128];			/* the bus name of the player     */
	char title[96];
	char artist[96];
	int playing;
	int have;
	time_t last;
	int pending;
};

/* ── reading one player ────────────────────────────────────────────────── */

/*
 * Metadata is a{sv} where `xesam:title` is a string and `xesam:artist` is an
 * ARRAY of strings — the one field everybody gets wrong, because reading it as
 * a string silently yields nothing and a panel with no artist looks like a
 * panel with no metadata.
 */
static void read_metadata(sd_bus_message *m, struct sh_mpris *p)
{
	if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL, *s = NULL;
		char t = 0;

		if (sd_bus_message_read_basic(m, 's', &key) < 0 ||
		    sd_bus_message_peek_type(m, &t, &contents) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		if (!strcmp(key, "xesam:title") && contents &&
		    !strcmp(contents, "s") &&
		    sd_bus_message_enter_container(m, 'v', "s") > 0) {
			if (sd_bus_message_read_basic(m, 's', &s) >= 0 && s)
				snprintf(p->title, sizeof(p->title), "%s", s);
			sd_bus_message_exit_container(m);
		} else if (!strcmp(key, "xesam:artist") && contents &&
			   !strcmp(contents, "as") &&
			   sd_bus_message_enter_container(m, 'v', "as") > 0) {
			if (sd_bus_message_enter_container(m, 'a', "s") > 0) {
				if (sd_bus_message_read_basic(m, 's', &s) > 0 &&
				    s)
					snprintf(p->artist, sizeof(p->artist),
						 "%s", s);
				sd_bus_message_exit_container(m);
			}
			sd_bus_message_exit_container(m);
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

static int props_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	struct sh_mpris *p = userdata;

	(void)e;
	p->pending = 0;
	if (sd_bus_message_is_method_error(reply, NULL))
		return 0;
	if (sd_bus_message_enter_container(reply, 'a', "{sv}") <= 0)
		return 0;

	p->title[0] = p->artist[0] = '\0';
	p->playing = 0;
	while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL, *s = NULL;
		char t = 0;

		if (sd_bus_message_read_basic(reply, 's', &key) < 0 ||
		    sd_bus_message_peek_type(reply, &t, &contents) < 0) {
			sd_bus_message_exit_container(reply);
			break;
		}
		if (!strcmp(key, "PlaybackStatus") && contents &&
		    !strcmp(contents, "s") &&
		    sd_bus_message_enter_container(reply, 'v', "s") > 0) {
			if (sd_bus_message_read_basic(reply, 's', &s) >= 0 && s)
				p->playing = !strcmp(s, "Playing");
			sd_bus_message_exit_container(reply);
		} else if (!strcmp(key, "Metadata") && contents &&
			   !strcmp(contents, "a{sv}") &&
			   sd_bus_message_enter_container(reply, 'v',
							  "a{sv}") > 0) {
			read_metadata(reply, p);
			sd_bus_message_exit_container(reply);
		} else {
			sd_bus_message_skip(reply, "v");
		}
		sd_bus_message_exit_container(reply);
	}
	sd_bus_message_exit_container(reply);
	p->have = p->title[0] != '\0';
	return 0;
}

static void ask_props(struct sh_mpris *p)
{
	if (!p->bus || !p->name[0] || p->pending)
		return;
	if (sd_bus_call_method_async(p->bus, NULL, p->name, MP_OBJ,
				     "org.freedesktop.DBus.Properties",
				     "GetAll", props_reply, p, "s",
				     MP_IFACE) >= 0)
		p->pending = 1;
}

/* ── finding a player ──────────────────────────────────────────────────── */

static int names_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	struct sh_mpris *p = userdata;
	const char *s = NULL;
	char first[128] = "";

	(void)e;
	p->pending = 0;
	if (sd_bus_message_is_method_error(reply, NULL))
		return 0;
	if (sd_bus_message_enter_container(reply, 'a', "s") <= 0)
		return 0;
	while (sd_bus_message_read_basic(reply, 's', &s) > 0) {
		if (!s || strncmp(s, MP_PREFIX, strlen(MP_PREFIX)))
			continue;
		if (!first[0])
			snprintf(first, sizeof(first), "%s", s);
		/* The one we were already watching keeps the cell: a player
		 * that is paused must not be replaced by whichever other name
		 * happens to sort first. */
		if (p->name[0] && !strcmp(p->name, s)) {
			first[0] = '\0';
			snprintf(first, sizeof(first), "%s", s);
			break;
		}
	}
	sd_bus_message_exit_container(reply);

	if (!first[0]) {
		p->name[0] = '\0';
		p->have = 0;
		p->title[0] = '\0';
		return 0;
	}
	if (strcmp(p->name, first))
		snprintf(p->name, sizeof(p->name), "%s", first);
	ask_props(p);
	return 0;
}

struct sh_mpris *sh_mpris_init(void *existing_bus)
{
	struct sh_mpris *p = calloc(1, sizeof(*p));

	if (!p)
		return NULL;
	if (existing_bus) {
		/* The tray already holds a session-bus connection and one
		 * process needs one, not two: a second connection is a second
		 * fd, a second unique name and a second thing to dispatch. */
		p->bus = existing_bus;
	} else if (sd_bus_open_user(&p->bus) < 0) {
		free(p);
		return NULL;
	} else {
		p->owns_bus = 1;
	}
	return p;
}

void sh_mpris_free(struct sh_mpris *p)
{
	if (!p)
		return;
	if (p->owns_bus && p->bus)
		sd_bus_unref(p->bus);
	free(p);
}

void sh_mpris_dispatch(struct sh_mpris *p)
{
	time_t now;

	if (!p || !p->bus)
		return;
	/* The bus is pumped by whoever owns it — the tray, normally. Pumping
	 * a shared connection twice per frame is harmless and pumping it zero
	 * times when nobody else does is a widget that never updates. */
	if (p->owns_bus)
		sd_bus_process(p->bus, NULL);

	now = time(NULL);
	if (now - p->last < MP_REFRESH_S || p->pending)
		return;
	p->last = now;
	if (p->name[0]) {
		ask_props(p);
		return;
	}
	if (sd_bus_call_method_async(p->bus, NULL, "org.freedesktop.DBus",
				     "/org/freedesktop/DBus",
				     "org.freedesktop.DBus", "ListNames",
				     names_reply, p, NULL) >= 0)
		p->pending = 1;
}

int sh_mpris_have(const struct sh_mpris *p)
{
	return p && p->have;
}

int sh_mpris_playing(const struct sh_mpris *p)
{
	return p && p->playing;
}

const char *sh_mpris_title(const struct sh_mpris *p)
{
	return p && p->title[0] ? p->title : "";
}

const char *sh_mpris_artist(const struct sh_mpris *p)
{
	return p && p->artist[0] ? p->artist : "";
}

/* Fire and forget, like every call to a tray item: a player that does not
 * answer must not cost a frame, and there is nothing in the reply to read. */
void sh_mpris_action(struct sh_mpris *p, const char *method)
{
	if (!p || !p->bus || !p->name[0])
		return;
	sd_bus_call_method_async(p->bus, NULL, p->name, MP_OBJ, MP_IFACE,
				 method, NULL, NULL, NULL);
}
