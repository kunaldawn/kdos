/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   com.canonical.Unity.LauncherEntry — badges and progress on a task button
 *
 * The count on a mail client, the bar under a download. Windows 7 added both
 * to its taskbar buttons and gave the reason — "a program can now easily show
 * an icon or progress in context of its taskbar button" — and on Linux the
 * protocol that carries them is Ubuntu's, which outlived Unity: Nautilus,
 * Thunar, Steam, browsers and Qt Creator all emit it, and KDE's icontasks and
 * dash-to-dock consume it.
 *
 * IT MATTERS MORE HERE THAN ELSEWHERE, twice over. Every fat application on
 * this distro runs inside a container that shares the session bus, so a
 * download progressing INSIDE the appbox reports on the host's taskbar with no
 * plumbing of ours. And the bar is icons-only: a badge is the only compact way
 * left to say "three unread".
 *
 * The protocol is one signal and no method calls, no bus name to own and
 * nothing to reply to:
 *
 *   Update(s app_uri, a{sv} properties)
 *
 * `app_uri` is `application://<desktop id>.desktop`, and the properties are a
 * partial update — only what changed is sent, so state is accumulated per
 * application rather than replaced.
 *
 * Nothing here blocks: it is a signal handler on the tray's existing bus and
 * the panel's dispatch already pumps it.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection tray.c and notifyd.c make, and for the same
 * reason: this has to be runnable somewhere other than KDOS. */
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

#define UN_MAX 16

struct un_entry {
	char id[128];		/* the desktop id, without `.desktop` */
	long count;
	int count_visible;
	int progress;		/* 0..100, -1 when not shown */
	int urgent;
	int used;
};

static struct un_entry entries[UN_MAX];
static sd_bus_slot *un_slot;

static struct un_entry *find(const char *id, int make)
{
	struct un_entry *free_slot = NULL;

	for (int i = 0; i < UN_MAX; i++) {
		if (entries[i].used && !strcasecmp(entries[i].id, id))
			return &entries[i];
		if (!entries[i].used && !free_slot)
			free_slot = &entries[i];
	}
	if (!make || !free_slot)
		return NULL;
	memset(free_slot, 0, sizeof(*free_slot));
	free_slot->used = 1;
	free_slot->progress = -1;
	snprintf(free_slot->id, sizeof(free_slot->id), "%s", id);
	return free_slot;
}

/*
 * The properties are a PARTIAL update — the specification says an emitter
 * sends only what changed — so each key is applied on its own and the rest of
 * the entry is left alone. Replacing the whole entry would clear a count every
 * time a progress tick arrived.
 */
static int on_update(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	const char *uri = NULL;
	struct un_entry *e;
	char id[128];
	const char *dot;

	(void)userdata;
	(void)err;
	if (sd_bus_message_read(m, "s", &uri) < 0 || !uri)
		return 0;
	if (strncmp(uri, "application://", 14))
		return 0;
	snprintf(id, sizeof(id), "%s", uri + 14);
	dot = strstr(id, ".desktop");
	if (dot)
		id[dot - id] = '\0';
	if (!id[0])
		return 0;

	e = find(id, 1);
	if (!e)
		return 0;

	if (sd_bus_message_enter_container(m, 'a', "{sv}") < 0)
		return 0;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL;
		const char *sig = NULL;

		if (sd_bus_message_read(m, "s", &key) < 0 || !key)
			break;
		if (sd_bus_message_peek_type(m, NULL, &sig) < 0 || !sig) {
			sd_bus_message_exit_container(m);
			continue;
		}
		/*
		 * The types are the ones the specification names, and an
		 * emitter that sends something else is skipped rather than
		 * guessed at: `count` is x, `progress` is d, the visibility
		 * flags are b. A double read as an int64 is not a smaller
		 * number, it is a different one.
		 */
		if (!strcmp(key, "count") && !strcmp(sig, "x")) {
			int64_t v = 0;

			sd_bus_message_read(m, "v", "x", &v);
			e->count = (long)v;
		} else if (!strcmp(key, "count-visible") &&
			   !strcmp(sig, "b")) {
			int v = 0;

			sd_bus_message_read(m, "v", "b", &v);
			e->count_visible = v;
		} else if (!strcmp(key, "progress") && !strcmp(sig, "d")) {
			double v = 0;

			sd_bus_message_read(m, "v", "d", &v);
			if (v < 0)
				v = 0;
			if (v > 1)
				v = 1;
			e->progress = (int)(v * 100 + 0.5);
		} else if (!strcmp(key, "progress-visible") &&
			   !strcmp(sig, "b")) {
			int v = 0;

			sd_bus_message_read(m, "v", "b", &v);
			if (!v)
				e->progress = -1;
			else if (e->progress < 0)
				e->progress = 0;
		} else if (!strcmp(key, "urgent") && !strcmp(sig, "b")) {
			int v = 0;

			sd_bus_message_read(m, "v", "b", &v);
			e->urgent = v;
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	return 0;
}

void sh_unity_init(void *bus)
{
	if (!bus || un_slot)
		return;
	/*
	 * Matched by INTERFACE with no sender, which is how Unity itself did
	 * it: any application on the bus may emit this and there is no service
	 * to own or to wait for. A match that named a sender would work for
	 * nothing.
	 */
	sd_bus_add_match((sd_bus *)bus, &un_slot,
			 "type='signal',"
			 "interface='com.canonical.Unity.LauncherEntry',"
			 "member='Update'",
			 on_update, NULL);
}

int sh_unity_get(const char *id, long *count, int *progress, int *urgent)
{
	const struct un_entry *e;

	if (!id || !*id)
		return 0;
	e = find(id, 0);
	if (!e)
		return 0;
	if (count)
		*count = e->count_visible ? e->count : 0;
	if (progress)
		*progress = e->progress;
	if (urgent)
		*urgent = e->urgent;
	return (e->count_visible && e->count > 0) || e->progress >= 0 ||
	       e->urgent;
}
