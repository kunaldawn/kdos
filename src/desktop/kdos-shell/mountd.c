/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The session's side of kdos-mountd, once
 *
 * THREE SURFACES ASK THIS DAEMON — the device manager, the media watcher and
 * the disks window — and each had written its own connect, its own timeout and
 * its own parse of the same six tab-separated fields. Three parses of one
 * format is two of them being wrong the day a column is added.
 *
 * A SHORT CONNECTION PER REQUEST. One line per connection is the daemon's own
 * rule, `subscribe` excepted, and it is what makes an index safe: a row number
 * is only true of the list it came with, so a caller that acts on one must
 * have asked for that list in the same breath.
 *
 * A ONE-SECOND CEILING on a local socket that answers in microseconds. The
 * daemon's work is a handful of file reads; anything slower than this is a
 * daemon that is wedged, and a surface that waited on it would stop drawing.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "shell.h"

#define SH_MOUNTD_SOCKET "/run/kdos-mountd.sock"

int sh_mountd_ask(const char *req, char *out, size_t n)
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	const char *path = getenv("KDOS_MOUNTD_SOCKET");
	size_t got = 0;
	ssize_t r;

	/* A caller with nothing to read passes NULL: `eject` is sent from a
	 * toast's action handler, which has no room to show a reply and no
	 * frame to draw it in. */
	if (out && n)
		out[0] = '\0';
	if (fd < 0)
		return -1;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
		 path && *path ? path : SH_MOUNTD_SOCKET);

	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}
	dprintf(fd, "%s\n", req);
	/* HALF-CLOSED after the write: the daemon reads a line and then the
	 * exact byte count a secret-carrying verb named, and a peer that never
	 * signals it is done writing leaves it waiting on a read for a verb
	 * that carries nothing. */
	shutdown(fd, SHUT_WR);
	while (out && got + 1 < n && (r = read(fd, out + got, n - got - 1)) > 0)
		got += (size_t)r;
	if (out && n)
		out[got] = '\0';
	close(fd);
	return 0;
}

int sh_mountd_list(ShMountRow *out, int max, char *why, size_t nwhy)
{
	char buf[8192];
	int n = 0;

	if (why && nwhy)
		why[0] = '\0';
	if (sh_mountd_ask("list", buf, sizeof(buf)) != 0) {
		if (why && nwhy)
			snprintf(why, nwhy, "kdos-mountd is not running "
					    "(service start 58_mountd)");
		return 0;
	}
	for (char *p = buf; *p && n < max;) {
		char *nl = strchr(p, '\n');

		if (nl)
			*nl = '\0';

		ShMountRow *m = &out[n];

		memset(m, 0, sizeof(*m));
		/* `index\tkname\tlabel\tfstype\tsize\tmount`, and a `-` where
		 * the daemon had nothing to say. The mount column is last and
		 * may be empty, so five fields is a complete row. */
		if (sscanf(p,
			   "%d\t%31[^\t]\t%63[^\t]\t%23[^\t]\t%15[^\t]\t%255[^\n]",
			   &m->idx, m->kname, m->label, m->fstype, m->size,
			   m->mnt) >= 5) {
			if (!strcmp(m->label, "-"))
				m->label[0] = '\0';
			if (!strcmp(m->mnt, "-"))
				m->mnt[0] = '\0';
			n++;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	return n;
}

int sh_mountd_shares(ShShareRow *out, int max, char *why, size_t nwhy)
{
	char buf[8192];
	int n = 0;

	if (why && nwhy)
		why[0] = '\0';
	if (sh_mountd_ask("shares", buf, sizeof(buf)) != 0) {
		if (why && nwhy)
			snprintf(why, nwhy, "kdos-mountd is not running "
					    "(service start 58_mountd)");
		return 0;
	}
	for (char *p = buf; *p && n < max;) {
		char *nl = strchr(p, '\n');

		if (nl)
			*nl = '\0';

		ShShareRow *r = &out[n];

		memset(r, 0, sizeof(*r));
		/* `index\tunc\tmountpoint`, and the `ok` the daemon ends with
		 * has neither tab, so it falls out here rather than needing a
		 * test of its own. */
		if (sscanf(p, "%d\t%351[^\t]\t%255[^\n]", &r->idx, r->unc,
			   r->mnt) == 3)
			n++;
		if (!nl)
			break;
		p = nl + 1;
	}
	return n;
}

int sh_mountd_do(int idx, const char *verb, char *out, size_t nout)
{
	char req[64], buf[512];

	snprintf(req, sizeof(req), "%s %d", verb, idx);
	if (sh_mountd_ask(req, buf, sizeof(buf)) != 0) {
		snprintf(out, nout, "kdos-mountd is not running");
		return -1;
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	snprintf(out, nout, "%.*s", (int)nout - 1, buf);
	/* `ok ` and `err ` are the daemon's whole vocabulary, and a caller
	 * that read only the message could not tell them apart. */
	return strncmp(buf, "ok", 2) == 0 ? 0 : -1;
}
