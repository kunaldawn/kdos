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
 * THE PROTOCOL IS ONE LINE PER CONNECTION. `suspend`, `poweroff`, `reboot`,
 * `ping` are a bare word; `timezone <zone>` is the one verb with an argument,
 * then a one-line reply and the socket closes. No length prefixes, no
 * multiplexing, no state: a parser is an attack surface and this one is a
 * handful of strcmp.
 *
 * THE TIMEZONE IS HERE AND NOT IN A SECOND DAEMON because it is the same
 * question: writing `/etc/localtime` and `/etc/profile.d/20-timezone.sh` is
 * root's, the person doing it is the one administering the machine, and
 * `wheel` is already the answer to who that is. A second socket with a second
 * authorisation rule would be a second answer to one question.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred */
#endif
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
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
 * The membership test itself is libkbase's, because kdos-energyd gates its
 * socket on exactly the same question and two copies of a security decision
 * eventually disagree about one of them.
 */
static bool in_wheel(const char *name, gid_t primary)
{
	return kb_user_in_group(name, primary, KP_GROUP) != 0;
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

/*
 * WHICH ACCOUNT tty1 LOGS IN WITHOUT ASKING, or none.
 *
 * `/etc/kdos/con.conf` is root's and the choice is an administrator's, which
 * is the same question `wheel` already answers — so it is a verb here rather
 * than a second daemon or a setuid writer for two lines.
 *
 * THE ACCOUNT MUST BE ONE A GREETER WOULD OFFER. `kb_users()` is the one place
 * that decides who may log in, and pointing autologin at a name it would not
 * list is a machine that boots to a login nobody can complete — a service
 * account with `nologin`, or a name that is not there at all.
 *
 * BOTH KEYS ARE REWRITTEN TOGETHER. `greet` and `autologin` are one setting
 * seen twice: `greet = no` with no `autologin` is a tty1 that logs in as
 * whatever the default happens to be, and an `autologin` under `greet = yes`
 * is a line that does nothing and reads as though it does.
 */
static int set_autologin(const char *who, char *out, size_t nout)
{
	const char *etc = getenv("KDOS_POWERD_ETC");
	char path[320], tmp[336], buf[16384], next[16384];
	int off = !strcmp(who, "off");
	FILE *f;

	if (!etc || !*etc)
		etc = "/etc";
	if (!off) {
		KbUser u[64];
		int n = kb_users(u, 64), i;

		for (i = 0; i < n; i++)
			if (!strcmp(u[i].name, who))
				break;
		if (i == n) {
			snprintf(out, nout, "err no such account\n");
			return -1;
		}
	}

	snprintf(path, sizeof(path), "%s/kdos/con.conf", etc);
	if (kb_read_file(path, buf, sizeof(buf)) <= 0) {
		snprintf(out, nout, "err cannot read con.conf\n");
		return -1;
	}

	next[0] = '\0';

	size_t used = 0;

	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp); ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char row[512];

		/* The KEY only, and leading space is not part of it: a comment
		 * mentioning `greet` must not be rewritten into a setting. */
		if (!strncmp(ln, "greet", 5) && strchr(ln, '='))
			snprintf(row, sizeof(row), "greet = %s",
				 off ? "yes" : "no");
		else if (!strncmp(ln, "autologin", 9) && strchr(ln, '='))
			snprintf(row, sizeof(row), "autologin = %s",
				 off ? "kdos" : who);
		else
			snprintf(row, sizeof(row), "%s", ln);
		int k = snprintf(next + used, sizeof(next) - used, "%s\n", row);

		/* A file that would not fit is REFUSED rather than truncated:
		 * writing half a config leaves a machine whose login settings
		 * are whatever survived. */
		if (k < 0 || (size_t)k >= sizeof(next) - used) {
			snprintf(out, nout, "err con.conf is too large\n");
			return -1;
		}
		used += (size_t)k;
	}

	snprintf(tmp, sizeof(tmp), "%s/kdos/con.conf.new", etc);
	f = fopen(tmp, "w");
	if (!f) {
		snprintf(out, nout, "err cannot write con.conf\n");
		return -1;
	}
	fputs(next, f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		snprintf(out, nout, "err cannot write con.conf\n");
		return -1;
	}
	snprintf(out, nout, "ok %s\n", off ? "off" : who);
	return 0;
}

/*
 * SET THE MACHINE'S TIMEZONE, from a name in the shipped zoneinfo tree.
 *
 * THE NAME IS VALIDATED AS A PATH COMPONENT SET AND THEN AS A FILE, in that
 * order. A zone is `Area/City` or `Area/Sub/City`, so a slash is legal — which
 * makes `../../etc/shadow` legal-looking too, and the first check is what
 * stops it: letters, digits, `+`, `-`, `_` and `/`, no dot at all, no leading
 * or doubled slash. The second is that the file must exist under
 * `/usr/share/zoneinfo`, so a name that passes the first and names nothing is
 * refused rather than symlinked to.
 *
 * BOTH HALVES ARE WRITTEN OR NEITHER IS. `/etc/localtime` is what a program
 * reading the zoneinfo tree follows; `TZ` in the profile is what musl reads
 * when it is set, and it is set on every KDOS login — so writing only the
 * symlink leaves `date` reporting the OLD zone for the life of every shell
 * that had already sourced the profile, which reads as the setting having
 * done nothing.
 */
static const char *KP_ZONEDIR = "/usr/share/zoneinfo";

static bool zone_name_ok(const char *z)
{
	size_t n = strlen(z);

	if (!n || n > 64 || z[0] == '/' || z[n - 1] == '/')
		return false;
	for (size_t i = 0; i < n; i++) {
		char c = z[i];

		if (c == '/' && z[i + 1] == '/')
			return false;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '+' || c == '-' ||
		      c == '_' || c == '/'))
			return false;
	}
	return true;
}

static int set_timezone(const char *zone, char *out, size_t nout)
{
	const char *dir = getenv("KDOS_POWERD_ZONEDIR");
	const char *etc = getenv("KDOS_POWERD_ETC");
	char zi[320], link[320], prof[320], tmp[336];
	FILE *f;

	if (!zone_name_ok(zone)) {
		snprintf(out, nout, "err not a zone name\n");
		return -1;
	}
	if (!dir || !*dir)
		dir = KP_ZONEDIR;
	if (!etc || !*etc)
		etc = "/etc";
	snprintf(zi, sizeof(zi), "%s/%s", dir, zone);
	if (!kb_path_exists(zi)) {
		snprintf(out, nout, "err no such zone\n");
		return -1;
	}

	snprintf(link, sizeof(link), "%s/localtime", etc);
	snprintf(tmp, sizeof(tmp), "%s/localtime.new", etc);
	unlink(tmp);
	if (symlink(zi, tmp) != 0 || rename(tmp, link) != 0) {
		unlink(tmp);
		snprintf(out, nout, "err cannot write localtime\n");
		return -1;
	}

	snprintf(prof, sizeof(prof), "%s/profile.d/20-timezone.sh", etc);
	snprintf(tmp, sizeof(tmp), "%s/profile.d/20-timezone.sh.new", etc);
	f = fopen(tmp, "w");
	if (!f) {
		snprintf(out, nout, "err cannot write the profile\n");
		return -1;
	}
	fprintf(f,
		"# Written by kdos-powerd.\n"
		"# `/etc/localtime` is what a program reading the zoneinfo\n"
		"# tree follows; this is what musl reads, and it wins where it\n"
		"# is set. Both say the same zone or `date` and the desktop\n"
		"# disagree.\n"
		"export TZ=':/etc/localtime'\n");
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, prof) != 0) {
		unlink(tmp);
		snprintf(out, nout, "err cannot write the profile\n");
		return -1;
	}
	snprintf(out, nout, "ok %s\n", zone);
	return 0;
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
		} else if (!strncmp(buf, "autologin ", 10)) {
			char msg[128];

			set_autologin(buf + 10, msg, sizeof(msg));
			(void)!write(c, msg, strlen(msg));
			close(c);
			continue;
		} else if (!strncmp(buf, "timezone ", 9)) {
			char msg[128];

			set_timezone(buf + 9, msg, sizeof(msg));
			(void)!write(c, msg, strlen(msg));
			close(c);
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

/*
 * Is a lock client already up? The everyday suspend is a lid close on a
 * session kdos-idle locked minutes ago, and a SECOND ext-session-lock client
 * is refused by the compositor — it exits 1 without ever printing `locked`, so
 * the wait below would run its whole deadline and then blame the lock screen
 * for a session that was locked all along. That warning is the one thing that
 * makes an unlocked resume attributable, and it must not cry wolf.
 */
static bool lock_running(void)
{
	int n = 0;
	char **names = kb_listdir("/proc", &n);
	if (!names)
		return false;
	bool found = false;
	for (int i = 0; i < n && !found; i++) {
		if (names[i][0] < '1' || names[i][0] > '9')
			continue;
		char path[64];
		size_t len = 0;
		snprintf(path, sizeof(path), "/proc/%s/comm", names[i]);
		char *comm = kb_read_all(path, &len);
		if (!comm)
			continue;	/* vanished, or not a pid at all */
		comm[strcspn(comm, "\n")] = '\0';
		found = !strcmp(comm, "kdos-lock");
		free(comm);
	}
	kb_strv_free(names);
	return found;
}

/*
 * Lock the session before it suspends — a resume that hands back an unlocked
 * desktop is the failure this exists to remove. Client-side only: the daemon
 * protocol stays one word per connection, and the daemon has no session to
 * lock anyway.
 *
 * kdos-lock prints exactly "locked\n" once the COMPOSITOR has confirmed the
 * session locked (the ext-session-lock handshake), and then stays running as
 * the lock client — so this waits for the line, never for the process. The
 * suspend goes ahead REGARDLESS after two seconds: a broken lock screen must
 * not turn the suspend key into a no-op, and the timeout is logged so the
 * unlocked resume is attributable. Closing our end of the pipe afterwards is
 * safe because that line is kdos-lock's only stdout write.
 */
static void lock_before_suspend(void)
{
	int pfd[2];
	if (lock_running())
		return;
	if (pipe2(pfd, O_CLOEXEC) < 0)
		return;

	pid_t pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		return;
	}
	if (pid == 0) {
		static char *lock_argv[] = { "kdos-lock", NULL };
		/* Its own session: the lock client outlives this process, and
		 * sharing our process group would hand it the terminal's ^C —
		 * an abandoned lock with no prompt on it. */
		setsid();
		dup2(pfd[1], STDOUT_FILENO);
		execvp(lock_argv[0], lock_argv);
		_exit(127);
	}
	close(pfd[1]);

	char buf[64] = {0};
	size_t got = 0;
	bool locked = false;
	double deadline = kb_now_s() + 2.0;

	while (got < sizeof(buf) - 1) {
		int wait_ms = (int)((deadline - kb_now_s()) * 1000.0);
		if (wait_ms <= 0)
			break;
		struct pollfd p = { .fd = pfd[0], .events = POLLIN };
		int rc = poll(&p, 1, wait_ms);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc <= 0)
			break;
		ssize_t n = read(pfd[0], buf + got, sizeof(buf) - 1 - got);
		if (n <= 0)	/* EOF: kdos-lock is gone (or was never here) */
			break;
		got += (size_t)n;
		buf[got] = '\0';
		if (strstr(buf, "locked\n")) {
			locked = true;
			break;
		}
	}
	close(pfd[0]);
	if (!locked)
		fprintf(stderr, "kdos-power: no lock confirmation within 2s — "
				"suspending anyway\n");
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-power [--no-lock] suspend|poweroff|reboot|ping\n"
		"       kdos-power timezone <Area/City>\n"
		"       kdos-power autologin <user>|off\n");
	return 2;
}

/* One word, one connection: the whole protocol. 0 when the daemon answered
 * `ok`, 1 otherwise, with the reason on stderr. */
static int request(const char *cmd)
{
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

static int client(int argc, char **argv)
{
	const char *cmd = NULL;
	bool no_lock = false;

	const char *arg = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--no-lock"))
			no_lock = true;
		else if (!cmd)
			cmd = argv[i];
		else if (!arg)
			arg = argv[i];
		else
			return usage();
	}
	if (!cmd)
		return usage();

	/*
	 * `timezone` IS THE ONE VERB WITH AN ARGUMENT, and it is joined here
	 * rather than in the daemon's parser: the request line is `timezone
	 * <zone>` and a zone name carries no space, so one buffer and one
	 * strncmp on the far side is the whole protocol change.
	 */
	char line[KP_MAX];

	if (!strcmp(cmd, "timezone") || !strcmp(cmd, "autologin")) {
		if (!arg)
			return usage();
		if (snprintf(line, sizeof(line), "%s %s", cmd, arg) >=
		    (int)sizeof(line)) {
			fprintf(stderr, "kdos-power: argument too long\n");
			return 2;
		}
		return request(line);
	}
	if (arg)
		return usage();
	if (strcmp(cmd, "suspend") && strcmp(cmd, "poweroff") &&
	    strcmp(cmd, "reboot") && strcmp(cmd, "ping"))
		return usage();

	/* KDOS_NO_LOCK_ON_SUSPEND=1 is the scriptable escape hatch — the same
	 * skip the flag gives, for a caller that cannot edit its own argv. */
	const char *skip = getenv("KDOS_NO_LOCK_ON_SUSPEND");
	if (!strcmp(cmd, "suspend") && !no_lock && !(skip && *skip)) {
		/*
		 * ASK FIRST, LOCK SECOND. `ping` runs the same SO_PEERCRED gate
		 * suspend does, so a caller outside `wheel` — or a machine with
		 * no kdos-powerd at all — is refused BEFORE the screen is
		 * locked. Locking and then not suspending is a password prompt
		 * in exchange for nothing, off one click on the panel's power
		 * item.
		 */
		if (request("ping") != 0)
			return 1;
		lock_before_suspend();
	}

	return request(cmd);
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
		/*
		 * THE TIMEZONE WRITE WITHOUT THE SOCKET, which is the only way
		 * it gets tested at all: the gate is SO_PEERCRED on a
		 * connection and cannot be exercised without two uids, so the
		 * verb's own rules — what a zone name may contain, that the
		 * file must exist, that both halves are written — would
		 * otherwise be asserted by nothing.
		 *
		 * IT GRANTS NOTHING. It is this binary run by whoever ran it,
		 * writing to an `/etc` that user could already write to; on
		 * the real path that is root's and this changes neither who
		 * may connect nor what the daemon does for them.
		 */
		if (argc == 3 && !strcmp(argv[1], "--set-autologin")) {
			char msg[128];
			int rc = set_autologin(argv[2], msg, sizeof(msg));

			fputs(msg, rc == 0 ? stdout : stderr);
			return rc == 0 ? 0 : 1;
		}
		if (argc == 3 && !strcmp(argv[1], "--set-timezone")) {
			char msg[128];
			int rc = set_timezone(argv[2], msg, sizeof(msg));

			fputs(msg, rc == 0 ? stdout : stderr);
			return rc == 0 ? 0 : 1;
		}
		if (argc > 1) {
			fprintf(stderr, "usage: kdos-powerd [--explain USER]\n"
					"       kdos-powerd --set-timezone "
					"<Area/City>\n"
					"       kdos-powerd --set-autologin "
					"<user>|off\n");
			return 2;
		}
		return serve();
	}
	return client(argc, argv);
}
