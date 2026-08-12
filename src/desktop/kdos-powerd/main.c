/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-powerd / kdos-power — suspend, poweroff, reboot
 *
 *     $ kdos-power suspend
 *     $ kdos-power poweroff
 *
 * Suspending needs a write to /sys/power/state and powering off needs
 * reboot(2). Both are root's, and the desktop is not root — so this is the
 * smallest thing that can sit between them: a root daemon on a unix socket in
 * /run, and a client that writes one word to it. ksvc supervises the daemon;
 * one binary answers to both names, dispatched on its own basename, the same
 * shape as kpkg and kdos-tools.
 *
 * WHY NOT THE OBVIOUS ALTERNATIVES. A setuid helper would put an
 * argv-parsing root process in every user's reach for the sake of three verbs.
 * polkit is installed here, but its answer is "ask the user's password", which a
 * lid-close cannot do. logind does not exist on KDOS and is not coming.
 *
 * THE AUTHORISATION IS SO_PEERCRED, and it is checked on the SOCKET rather than
 * trusted from the message. A client cannot lie about its uid — the kernel
 * fills the credentials in — and there is nothing in the protocol that names a
 * user, so there is nothing to forge. Who is allowed: root, and any member of
 * `wheel`. That is the same group the installer puts the human user in and the
 * same one polkit treats as admin, so "can poweroff" and "can administer this
 * machine" stay one answer.
 *
 * THE PROTOCOL IS ONE WORD PER CONNECTION. `suspend`, `poweroff`, `reboot`,
 * `ping`, then a one-line reply and the socket closes. No length prefixes, no
 * multiplexing, no state: a parser is an attack surface and this one is 30 bytes
 * of strcmp.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred */
#endif
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kbase.h"

#define KP_SOCKET "/run/kdos-powerd.sock"
#define KP_GROUP  "wheel"
#define KP_MAX    64

/*
 * The socket, overridable so the daemon and its authorisation can be exercised
 * without root — which is the only way this gets tested at all, since the real
 * path is in /run and the real actions power the machine off. It grants
 * nothing: authorisation is SO_PEERCRED on the connection, so pointing a client
 * at a different socket only ever reaches a different daemon, and pointing the
 * DAEMON somewhere else still needs privilege to actually suspend anything.
 */
static const char *sock_path(void)
{
	const char *p = getenv("KDOS_POWERD_SOCKET");
	return p && *p ? p : KP_SOCKET;
}

/* ── the allowed set ───────────────────────────────────────────────────── */

/*
 * Is `uid` root or a member of wheel?
 *
 * /etc/group and /etc/passwd are parsed directly rather than through
 * getgrnam()/initgroups(): musl's NSS is files-only anyway, and a daemon that
 * runs as root should not be loading a name-service module to answer a question
 * two file reads can. The group's OWN gid counts as well as its member list —
 * a user whose primary group is wheel never appears in the member list.
 */
static bool in_wheel(const char *name, gid_t primary)
{
	FILE *f = fopen("/etc/group", "r");
	if (!f)
		return false;

	bool ok = false;
	char line[4096];
	while (!ok && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *save = NULL;
		char *gname = strtok_r(line, ":", &save);
		if (!gname || strcmp(gname, KP_GROUP))
			continue;
		strtok_r(NULL, ":", &save);		/* the password field */
		char *gid_s = strtok_r(NULL, ":", &save);
		char *members = strtok_r(NULL, ":", &save);

		if (gid_s && (gid_t)strtoul(gid_s, NULL, 10) == primary)
			ok = true;

		for (char *m = members ? strtok_r(members, ",", &save) : NULL;
		     m && !ok; m = strtok_r(NULL, ",", &save))
			ok = !strcmp(m, name);
	}
	fclose(f);
	return ok;
}

static bool uid_allowed(uid_t uid)
{
	if (uid == 0)
		return true;
	struct passwd *pw = getpwuid(uid);
	if (!pw || !pw->pw_name)
		return false;
	return in_wheel(pw->pw_name, pw->pw_gid);
}

/*
 * `kdos-powerd --explain <user>` — would that user be permitted, and why.
 *
 * A refused power key is otherwise unattributable: the daemon's stderr goes to
 * the supervisor's log, which is the one place a user watching a dead
 * Super+power will not look. It reads the same two files the gate does and
 * needs no privilege, so it is also how the wheel parse gets tested.
 */
static int explain(const char *user)
{
	struct passwd *pw = getpwnam(user);
	if (!pw) {
		printf("%s: no passwd entry — refused\n", user);
		return 1;
	}
	if (pw->pw_uid == 0) {
		printf("%s: uid 0 — permitted\n", user);
		return 0;
	}
	bool ok = in_wheel(pw->pw_name, pw->pw_gid);
	printf("%s: uid %u, primary gid %u, %s %s — %s\n", user,
	       (unsigned)pw->pw_uid, (unsigned)pw->pw_gid,
	       ok ? "in" : "not in", KP_GROUP,
	       ok ? "permitted" : "refused");
	return ok ? 0 : 1;
}

/* ── the actions ───────────────────────────────────────────────────────── */

/*
 * `sync` before every one of them. There is no filesystem unmount path here —
 * the daemon is not init — so the data that has not reached the disk when the
 * machine suspends or powers off is the data that is lost.
 */
static int do_suspend(void)
{
	sync();
	int fd = open("/sys/power/state", O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	/*
	 * `mem` is suspend-to-RAM. Not `disk`: hibernation needs a resume=
	 * kernel argument and a swap device big enough for RAM, neither of
	 * which KDOS's initramfs sets up, and a hibernate that cannot resume is
	 * a poweroff that eats your session.
	 */
	ssize_t w = write(fd, "mem", 3);
	close(fd);
	return w == 3 ? 0 : -1;
}

static int do_reboot(int cmd)
{
	sync();
	/*
	 * Ask init first: /etc/init.d has services with stop actions, and
	 * pulling the power out from under NetworkManager and the appbox's
	 * containers is how a filesystem ends up dirty. SIGTERM to pid 1 is
	 * what toybox init answers with a shutdown, and reboot(2) is the
	 * fallback for an init that ignored it.
	 */
	kill(1, cmd == RB_POWER_OFF ? SIGUSR2 : SIGTERM);
	sleep(5);
	reboot(cmd);
	return -1;		/* only reached if reboot(2) itself failed */
}

/* ── the daemon ────────────────────────────────────────────────────────── */

static int serve(void)
{
	const char *path = sock_path();
	if (geteuid() != 0) {
		/*
		 * Refused on the real socket, allowed on a test one — and said
		 * out loud, because an unprivileged daemon answers `ok` to a
		 * suspend it cannot perform.
		 */
		if (!strcmp(path, KP_SOCKET)) {
			fprintf(stderr, "kdos-powerd: must run as root\n");
			return 1;
		}
		fprintf(stderr, "kdos-powerd: not root — suspend, poweroff and "
				"reboot will fail\n");
	}

	int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0) {
		fprintf(stderr, "kdos-powerd: socket: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	/* A socket left behind by a previous run would make bind() fail with
	 * EADDRINUSE forever; the supervisor would then restart us in a loop. */
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-powerd: bind %s: %s\n", path,
			strerror(errno));
		close(srv);
		return 1;
	}
	/*
	 * 0666 on the socket, with SO_PEERCRED as the real gate. The filesystem
	 * mode cannot express "wheel only" without a group the socket would have
	 * to be chowned to, and a mode that LOOKED like the authorisation would
	 * invite someone to weaken the credential check because "the mode
	 * already handles it".
	 */
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		fprintf(stderr, "kdos-powerd: listen: %s\n", strerror(errno));
		close(srv);
		return 1;
	}

	for (;;) {
		int c = accept(srv, NULL, NULL);
		if (c < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		struct ucred cred = {0};
		socklen_t len = sizeof(cred);
		if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0 ||
		    !uid_allowed(cred.uid)) {
			/* Refused with a reason, and logged: an unexplained
			 * dead power key is unattributable, and this is the
			 * message that attributes it. */
			fprintf(stderr, "kdos-powerd: refused uid %u (not root "
				"and not in %s)\n", (unsigned)cred.uid,
				KP_GROUP);
			(void)!write(c, "err not permitted\n", 18);
			close(c);
			continue;
		}

		char buf[KP_MAX] = {0};
		ssize_t n = read(c, buf, sizeof(buf) - 1);
		if (n <= 0) {
			close(c);
			continue;
		}
		buf[n] = '\0';
		buf[strcspn(buf, "\r\n")] = '\0';

		const char *reply = "err unknown command\n";

		if (!strcmp(buf, "ping")) {
			reply = "ok\n";
		} else if (!strcmp(buf, "suspend")) {
			/* Answered BEFORE the machine goes down, or the client
			 * waits for a reply from a suspended kernel and reports
			 * a failure that did not happen. */
			(void)!write(c, "ok\n", 3);
			close(c);
			if (do_suspend() != 0)
				fprintf(stderr, "kdos-powerd: suspend failed: "
					"%s\n", strerror(errno));
			continue;
		} else if (!strcmp(buf, "poweroff")) {
			(void)!write(c, "ok\n", 3);
			close(c);
			do_reboot(RB_POWER_OFF);
			continue;
		} else if (!strcmp(buf, "reboot")) {
			(void)!write(c, "ok\n", 3);
			close(c);
			do_reboot(RB_AUTOBOOT);
			continue;
		}

		(void)!write(c, reply, strlen(reply));
		close(c);
	}

	close(srv);
	unlink(path);
	return 1;
}

/* ── the client ────────────────────────────────────────────────────────── */

static int usage(void)
{
	fprintf(stderr, "usage: kdos-power suspend|poweroff|reboot|ping\n");
	return 2;
}

static int client(int argc, char **argv)
{
	if (argc != 2)
		return usage();
	const char *cmd = argv[1];
	if (strcmp(cmd, "suspend") && strcmp(cmd, "poweroff") &&
	    strcmp(cmd, "reboot") && strcmp(cmd, "ping"))
		return usage();

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return 1;

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-power: no kdos-powerd — `service "
				"kdos-powerd start`\n");
		close(fd);
		return 1;
	}

	char line[64];
	int n = snprintf(line, sizeof(line), "%s\n", cmd);
	if (write(fd, line, (size_t)n) != n) {
		close(fd);
		return 1;
	}

	char reply[64] = {0};
	ssize_t r = read(fd, reply, sizeof(reply) - 1);
	close(fd);
	if (r <= 0) {
		/*
		 * The daemon closed without answering. For suspend and poweroff
		 * that is indistinguishable from "it worked and the machine went
		 * away", so it is not reported as a failure — a poweroff that
		 * prints an error on its way down is a bug report about nothing.
		 */
		return strcmp(cmd, "ping") ? 0 : 1;
	}
	if (!strncmp(reply, "ok", 2))
		return 0;
	reply[strcspn(reply, "\r\n")] = '\0';
	fprintf(stderr, "kdos-power: %s\n", reply);
	return 1;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-power");

	/* Dispatch on the basename, busybox-style: one binary, two names. The
	 * daemon and the client share the socket path and the command words, and
	 * two programs would be two places to keep them the same. */
	const char *me = strrchr(argv[0], '/');
	me = me ? me + 1 : argv[0];

	if (!strcmp(me, "kdos-powerd")) {
		if (argc == 3 && !strcmp(argv[1], "--explain"))
			return explain(argv[2]);
		if (argc > 1) {
			fprintf(stderr, "usage: kdos-powerd [--explain USER]\n");
			return 2;
		}
		return serve();
	}
	return client(argc, argv);
}
