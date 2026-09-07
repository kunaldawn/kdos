/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mediad — a stick goes in, and the desktop says so
 *
 * THE DAEMON NOTICES AND THE SESSION SPEAKS. kdos-mountd learns about a device
 * from the kernel's uevent broadcast, but it is root, it starts before anybody
 * logs in, and it has no session bus to raise a toast on — `Notify` lives at
 * `$XDG_RUNTIME_DIR/bus`, which belongs to a login that may not exist yet. So
 * the daemon says only `changed`, and this program, which IS the session,
 * decides what that means and says it.
 *
 * SUBSCRIBED, NOT POLLED. The panel refuses to ask the daemon anything on the
 * frame path and that refusal stands: this is one connection that stays open
 * and carries nothing until something happens. It is not a tick.
 *
 * THE INDEX IS RE-READ BEFORE IT IS USED, TWICE. A row number is only true of
 * the list it came with — kdos-mountd rebuilds that list on every request — so
 * the toast is built from a fresh list, and the button, which may be clicked
 * minutes later, finds its device by KERNEL NAME in a list read at the click.
 * Acting on the number the toast was built with would eject whatever had
 * arrived since.
 * ---------------------------------
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd. Selected here
 * rather than in the build, the same way notifyd.c and tray.c do it, so this
 * can be compiled on a machine that is not KDOS. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kbase.h"
#include "shell.h"

#define MD_MAX 32		/* rows kdos-mountd will ever publish */
#define MD_NAME 64

struct md_dev {
	char kname[MD_NAME];
	char label[MD_NAME];
	char mnt[256];
};

/* What was attached last time `changed` arrived. The diff is what turns a
 * "something moved" into "this stick just went in". */
static struct md_dev md_seen[MD_MAX];
static int md_nseen;

/* The notifications this program raised and the device each was about, so a
 * click can be told from somebody else's toast. */
static struct {
	uint32_t id;
	char kname[MD_NAME];
} md_toast[MD_MAX];
static int md_ntoast;

static const char *md_sock(void)
{
	const char *p = getenv("KDOS_MOUNTD_SOCKET");

	return p && *p ? p : "/run/kdos-mountd.sock";
}

/* One short connection, one verb, the whole reply. Every request except
 * `subscribe` is answered and closed, which is what makes this safe to do from
 * a click handler. */
static int md_ask(const char *verb, char *out, size_t n)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	size_t got = 0;
	ssize_t r;

	if (out && n)
		out[0] = '\0';
	if (fd < 0)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", md_sock());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", verb);
	shutdown(fd, SHUT_WR);
	while (out && got + 1 < n &&
	       (r = read(fd, out + got, n - got - 1)) > 0)
		got += (size_t)r;
	if (out && n)
		out[got] = '\0';
	close(fd);
	return 0;
}

/*
 * The list, parsed. `index<TAB>kname<TAB>label<TAB>fstype<TAB>size<TAB>mount`,
 * with `-` for a label or a mountpoint there is none of, then `ok`.
 */
static int md_list(struct md_dev *v, int max)
{
	char buf[8192];
	int n = 0;

	if (md_ask("list", buf, sizeof(buf)) != 0)
		return 0;
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp);
	     ln && n < max; ln = strtok_r(NULL, "\n", &sp)) {
		char *f[6] = {0};
		int nf = 0;

		if (!strcmp(ln, "ok"))
			break;
		for (char *s2 = NULL, *t = strtok_r(ln, "\t", &s2);
		     t && nf < 6; t = strtok_r(NULL, "\t", &s2))
			f[nf++] = t;
		if (nf < 6)
			continue;
		snprintf(v[n].kname, sizeof(v[n].kname), "%s", f[1]);
		snprintf(v[n].label, sizeof(v[n].label), "%s",
			 strcmp(f[2], "-") ? f[2] : f[1]);
		snprintf(v[n].mnt, sizeof(v[n].mnt), "%s",
			 strcmp(f[5], "-") ? f[5] : "");
		n++;
	}
	return n;
}

/* The row a kernel name is on RIGHT NOW, or -1. Every verb takes an index and
 * every index is a moment old; this is the only way to use one safely. */
static int md_row(const char *kname)
{
	struct md_dev now[MD_MAX];
	int n = md_list(now, MD_MAX);

	for (int i = 0; i < n; i++)
		if (!strcmp(now[i].kname, kname))
			return i;
	return -1;
}

static int md_was_seen(const char *kname)
{
	for (int i = 0; i < md_nseen; i++)
		if (!strcmp(md_seen[i].kname, kname))
			return 1;
	return 0;
}

/*
 * A TOAST WITH TWO BUTTONS. The actions array is id/label pairs flattened into
 * one list, which is what the protocol says and what kdos-notifyd's own reader
 * expects; the ids are this program's to choose and are matched back on the
 * click. The reply carries the notification id, so unlike `kb_notify()` — which
 * double-forks gdbus and throws the id away — a click here can be told from
 * anybody else's.
 */
static void md_offer(sd_bus *bus, const struct md_dev *d)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *rep = NULL;
	char body[160];
	uint32_t id = 0;

	snprintf(body, sizeof(body), "%s is ready", d->label);
	if (sd_bus_call_method(bus, "org.freedesktop.Notifications",
			       "/org/freedesktop/Notifications",
			       "org.freedesktop.Notifications", "Notify",
			       &err, &rep, "susssasa{sv}i",
			       "kdos-mediad", (uint32_t)0, "drive-removable-media",
			       "Removable media", body,
			       4, "open", "Open", "eject", "Eject",
			       0, (int32_t)-1) < 0) {
		sd_bus_error_free(&err);
		return;
	}
	if (sd_bus_message_read(rep, "u", &id) >= 0 && md_ntoast < MD_MAX) {
		md_toast[md_ntoast].id = id;
		snprintf(md_toast[md_ntoast].kname,
			 sizeof(md_toast[md_ntoast].kname), "%s", d->kname);
		md_ntoast++;
	}
	sd_bus_message_unref(rep);
	sd_bus_error_free(&err);
}

/* Mount if it is not mounted, then open the mountpoint in whatever opens a
 * directory. The opener is the desktop's, so this program decides nothing
 * about what a folder is. */
static void md_open(const char *kname)
{
	struct md_dev now[MD_MAX];
	int n, row = -1;

	n = md_list(now, MD_MAX);
	for (int i = 0; i < n; i++)
		if (!strcmp(now[i].kname, kname))
			row = i;
	if (row < 0)
		return;
	if (!now[row].mnt[0]) {
		char verb[32];

		snprintf(verb, sizeof(verb), "mount %d", row);
		md_ask(verb, NULL, 0);
		n = md_list(now, MD_MAX);
		row = -1;
		for (int i = 0; i < n; i++)
			if (!strcmp(now[i].kname, kname))
				row = i;
		if (row < 0 || !now[row].mnt[0])
			return;
	}
	KbArgv a = { 0 };
	kb_argv_add(&a, "kdos-appbox");
	kb_argv_add(&a, "open");
	kb_argv_add(&a, now[row].mnt);
	kb_argv_end(&a);
	kb_run_detach(&a);
}

static void md_eject(const char *kname)
{
	int row = md_row(kname);
	char verb[32];

	if (row < 0)
		return;
	snprintf(verb, sizeof(verb), "eject %d", row);
	md_ask(verb, NULL, 0);
}

static int md_action(sd_bus_message *m, void *user, sd_bus_error *err)
{
	uint32_t id = 0;
	const char *act = NULL;

	(void)user;
	(void)err;
	if (sd_bus_message_read(m, "us", &id, &act) < 0)
		return 0;
	for (int i = 0; i < md_ntoast; i++) {
		if (md_toast[i].id != id)
			continue;
		if (!strcmp(act, "open"))
			md_open(md_toast[i].kname);
		else if (!strcmp(act, "eject"))
			md_eject(md_toast[i].kname);
		/* One toast, one click: kdos-notifyd drops the toast as it
		 * emits, so the id can never come again. */
		md_toast[i] = md_toast[--md_ntoast];
		return 0;
	}
	return 0;
}

int mediad_main(int argc, char **argv)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	sd_bus *bus = NULL;
	int sub, r;

	(void)argc;
	(void)argv;

	sub = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sub < 0)
		return 1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", md_sock());
	if (connect(sub, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		/* No daemon is not an error worth a restart loop: a machine
		 * with no kdos-mountd has no removable media to announce. */
		close(sub);
		return 0;
	}
	dprintf(sub, "subscribe\n");

	if (sd_bus_open_user(&bus) < 0) {
		close(sub);
		return 1;
	}
	r = sd_bus_add_match(bus, NULL,
			     "type='signal',"
			     "interface='org.freedesktop.Notifications',"
			     "member='ActionInvoked'",
			     md_action, NULL);
	if (r < 0) {
		sd_bus_unref(bus);
		close(sub);
		return 1;
	}

	/* What is already attached is not news. A toast for every stick that
	 * was in the machine before the session started would be four toasts
	 * at login saying nothing happened. */
	md_nseen = md_list(md_seen, MD_MAX);

	for (;;) {
		struct pollfd fds[2] = {
			{ .fd = sub, .events = POLLIN },
			{ .fd = sd_bus_get_fd(bus), .events = POLLIN },
		};
		char buf[256];
		ssize_t n;

		while (sd_bus_process(bus, NULL) > 0)
			;
		if (poll(fds, 2, -1) < 0)
			break;
		if (fds[1].revents & POLLIN)
			continue;	/* processed at the top */
		if (!(fds[0].revents & (POLLIN | POLLHUP)))
			continue;

		n = read(sub, buf, sizeof(buf) - 1);
		if (n <= 0)
			break;		/* the daemon went away */
		buf[n] = '\0';
		if (!strstr(buf, "changed"))
			continue;

		struct md_dev now[MD_MAX];
		int nn = md_list(now, MD_MAX);

		for (int i = 0; i < nn; i++)
			if (!md_was_seen(now[i].kname))
				md_offer(bus, &now[i]);
		memcpy(md_seen, now, sizeof(now));
		md_nseen = nn;
	}

	sd_bus_unref(bus);
	close(sub);
	return 0;
}
