/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * kdos-resctl — the second setuid binary KDOS ships, and the whole of what it
 * may do.
 *
 * kdos-checkpass exists because /etc/shadow is root-only and the lock screen
 * must not be. This exists because a task manager that cannot end a stuck root
 * daemon, and a Memory page that cannot say what kind of DIMMs are in the
 * machine, are each half a feature. The mitigation is that it is short enough
 * to read at a sitting and has nothing an attacker can aim.
 *
 * THREE VERBS, NO PATHS, NO OPTIONS:
 *
 *   dmi                          the SMBIOS table, whose path is COMPILED IN
 *   signal <pid> TERM|KILL|STOP|CONT
 *   renice <pid> <-20..19>
 *
 * Nothing in argv is ever opened. The only argument that reaches a syscall is
 * a pid and a small enum, so there is no path to traverse and no string to
 * quote. Any other argv is a usage error and exit 2.
 *
 * THE CALLER MUST BE IN `wheel`, BY REAL UID — the same gate kdos-powerd and
 * kdos-energyd apply, and the same group that already has polkit admin and a
 * root login on tty2 on this distro.
 *
 * Exit codes: 0 done · 1 refused · 2 usage · 3 the operation itself failed.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "kbase.h"

/*
 * Overridable at BUILD time only, never at run time: the gate cannot be an
 * environment variable a caller sets. The default is the group that already
 * carries polkit admin and a root login on tty2 on this distro.
 */
#ifndef RESCTL_GROUP
#define RESCTL_GROUP "wheel"
#endif
/* Compiled in. Nothing in argv is ever opened, so there is no path to aim. */
#define DMI_TABLE "/sys/firmware/dmi/tables/DMI"

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_pidfd_send_signal
#define SYS_pidfd_send_signal 424
#endif

static int refuse(const char *why)
{
	fprintf(stderr, "kdos-resctl: %s\n", why);
	return 1;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-resctl dmi\n"
		"       kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>\n"
		"       kdos-resctl renice <pid> <-20..19>\n");
	return 2;
}

/*
 * The gate. By REAL uid, so that being setuid does not make the check pass:
 * geteuid() here is root by construction and would authorise everybody.
 */
static int authorised(void)
{
	uid_t ruid = getuid();
	if (ruid == 0)
		return 1;

	char *pw = kb_read_all("/etc/passwd", NULL);
	if (!pw)
		return 0;
	char name[64] = "";
	gid_t primary = (gid_t)-1;
	for (char *line = pw, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		char *c1 = strchr(line, ':');
		if (!c1)
			continue;
		char *c2 = strchr(c1 + 1, ':');
		if (!c2)
			continue;
		if ((uid_t)atoi(c2 + 1) == ruid) {
			*c1 = 0;
			kb_strlcpy(name, line, sizeof(name));
			char *c3 = strchr(c2 + 1, ':');
			if (c3)
				primary = (gid_t)atoi(c3 + 1);
			break;
		}
	}
	free(pw);
	if (!name[0])
		return 0;
	return kb_user_in_group(name, primary, RESCTL_GROUP);
}

/*
 * The signal, through a pidfd taken FIRST.
 *
 * Between reading a pid off a screen and acting on it the process can exit and
 * the number be reused, and the kill would then land on whatever inherited it.
 * A pidfd is a handle to THAT process; a kernel without the syscalls falls back
 * to kill(2), which is the race this exists to avoid but is better than
 * refusing to work at all.
 */
static int do_signal(int pid, const char *what)
{
	int sig;
	if (!strcmp(what, "TERM"))
		sig = SIGTERM;
	else if (!strcmp(what, "KILL"))
		sig = SIGKILL;
	else if (!strcmp(what, "STOP"))
		sig = SIGSTOP;
	else if (!strcmp(what, "CONT"))
		sig = SIGCONT;
	else
		return usage();

	/*
	 * pid 1 is refused. Stopping init is not a task-manager operation;
	 * kdos-powerd is what signals pid 1, and it does it for reboot and
	 * poweroff only.
	 */
	if (pid <= 1)
		return refuse("pid 1 is not a task-manager target");

	int fd = (int)syscall(SYS_pidfd_open, pid, 0);
	if (fd >= 0) {
		long rc = syscall(SYS_pidfd_send_signal, fd, sig, NULL, 0);
		close(fd);
		if (rc == 0)
			return 0;
		fprintf(stderr, "kdos-resctl: %s\n", strerror(errno));
		return 3;
	}
	if (kill(pid, sig) == 0)
		return 0;
	fprintf(stderr, "kdos-resctl: %s\n", strerror(errno));
	return 3;
}

static int do_renice(int pid, int nice)
{
	if (pid <= 1)
		return refuse("pid 1 is not a task-manager target");
	if (nice < -20 || nice > 19)
		return refuse("nice is -20..19");
	if (setpriority(PRIO_PROCESS, (id_t)pid, nice) == 0)
		return 0;
	fprintf(stderr, "kdos-resctl: %s\n", strerror(errno));
	return 3;
}

/*
 * The SMBIOS table. Opened as root and PARSED AS THE CALLER: privilege is
 * dropped the moment it is no longer needed, so the parser — the only part
 * here that reads attacker-shaped bytes — never runs with it.
 */
static int do_dmi(void)
{
	int fd = open(DMI_TABLE, O_RDONLY | O_CLOEXEC);
	int err = errno;

	if (setresuid(getuid(), getuid(), getuid()) != 0) {
		if (fd >= 0)
			close(fd);
		return refuse("could not drop privilege");
	}
	if (fd < 0) {
		fprintf(stderr, "kdos-resctl: %s: %s\n", DMI_TABLE,
			strerror(err));
		return 3;
	}

	unsigned char *buf = kb_calloc(1, 1 << 16);
	ssize_t n = read(fd, buf, 1 << 16);
	close(fd);
	if (n <= 0) {
		free(buf);
		return refuse("the SMBIOS table is empty");
	}

	/*
	 * Type 17 is a memory device. Walked rather than indexed: the table is
	 * a chain of variable-length structures each followed by a
	 * double-NUL-terminated string pool, and every bound is checked
	 * because these bytes come from firmware.
	 */
	ssize_t off = 0;
	int found = 0;
	while (off + 4 <= n) {
		unsigned char type = buf[off];
		unsigned char len = buf[off + 1];
		if (len < 4 || off + len > n)
			break;

		const char *strs = (const char *)(buf + off + len);
		ssize_t sp = off + len;
		while (sp + 1 < n && !(buf[sp] == 0 && buf[sp + 1] == 0))
			sp++;
		if (sp + 1 >= n)
			break;

		if (type == 17 && len >= 0x15) {
			unsigned size = (unsigned)(buf[off + 0x0d] << 8 |
						   buf[off + 0x0c]);
			if (size && size != 0xffff) {
				unsigned mb = (size & 0x8000)
					      ? (size & 0x7fff) / 1024
					      : size;
				unsigned speed = (unsigned)(buf[off + 0x16] << 8 |
							    buf[off + 0x15]);
				printf("slot\t%u MB\t%u MT/s\n", mb, speed);
				found++;
			}
		}
		(void)strs;
		off = sp + 2;
		if (type == 127)
			break;
	}
	free(buf);
	if (!found)
		printf("no populated memory device in the SMBIOS table\n");
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-resctl");

	/*
	 * Nothing is read from the environment and stdio is not trusted: a
	 * setuid program inheriting a closed descriptor 1 would have its
	 * output land in whatever it opened next.
	 */
	for (int fd = 0; fd < 3; fd++)
		if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
			int nul = open("/dev/null", O_RDWR);
			if (nul != fd && nul >= 0)
				close(nul);
		}

	if (argc < 2)
		return usage();
	if (!authorised())
		return refuse("not permitted");

	if (!strcmp(argv[1], "dmi") && argc == 2)
		return do_dmi();
	if (!strcmp(argv[1], "signal") && argc == 4)
		return do_signal(atoi(argv[2]), argv[3]);
	if (!strcmp(argv[1], "renice") && argc == 4)
		return do_renice(atoi(argv[2]), atoi(argv[3]));
	return usage();
}
