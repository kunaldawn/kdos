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
 * Who a process belongs to, and which box it is in.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

/*
 * The conmon walk: a pid to the name of the container it runs in.
 *
 * cgroups are the textbook answer and are unusable here. With no systemd,
 * rootless podman gets no cgroup delegation and the whole box frequently sits
 * in `0::/`, which says nothing. conmon is podman's per-container supervisor,
 * it carries `-n <name>` in its own argv, and the process inside the box is
 * its descendant — so the walk goes UP the parent chain and conmon is the
 * boundary of the box.
 *
 * The walk is bounded: a corrupted or hostile ppid chain must not spin, and
 * eight hops is deeper than any real container nesting here.
 */
/*
 * conmon's argv carries `-n <name>`. Pulled out of the two walks below so the
 * parse exists once: the walk differs (one has a sample in hand, one reads
 * /proc) and what it does when it arrives does not.
 *
 * The scan is over the NUL separators rather than a strstr of the whole blob:
 * a container actually named "-n" would otherwise match the flag and the next
 * argument would be read as the box.
 */
static int conmon_name(int pid, char *out, size_t cap)
{
	size_t len = 0;
	char path[512];
	snprintf(path, sizeof(path), "%s/%d/cmdline", kpr_proc(), pid);
	char *cmd = kb_read_all(path, &len);
	if (!cmd)
		return 0;

	int found = 0;
	for (char *a = cmd; (size_t)(a - cmd) < len && *a; ) {
		size_t alen = strlen(a);
		/* Both spellings: podman is invoked with `-n` and conmon
		 * accepts `--name`, and a walk that knew only one would come
		 * back empty against the other. */
		if ((!strcmp(a, "-n") || !strcmp(a, "--name")) &&
		    (size_t)(a - cmd) + alen + 1 < len) {
			kb_strlcpy(out, a + alen + 1, cap);
			found = 1;
			break;
		}
		a += alen + 1;
	}
	free(cmd);
	return found;
}

/*
 * The same answer for a caller with no sample in hand, reading /proc as it
 * climbs. kdos stutter, kdos-oomd and kdos-teams all walk this way — none of
 * them holds a table of every pid — and each carried its own copy with its own
 * hop bound before this existed.
 *
 * KPR_BOX_HOPS is one number rather than four: an app in a box sits two or
 * three below conmon, and the bound is what stops a /proc that is lying from
 * spinning here.
 */
int kpr_box_of_pid(int pid, char *out, size_t cap)
{
	if (out && cap)
		out[0] = 0;
	if (!out || !cap)
		return 0;

	/* Start at the PARENT: conmon runs on the host and supervises the box
	 * from outside it, so matching it against itself would report the
	 * supervisor as a member of what it supervises. */
	char *st = kpr_slurp_proc("%d/stat", pid);
	if (!st)
		return 0;
	char *close = strrchr(st, ')');
	int cur = 0;
	if (close)
		sscanf(close + 1, " %*c %d", &cur);
	free(st);

	for (int hop = 0; hop < KPR_BOX_HOPS && cur > 1; hop++) {
		st = kpr_slurp_proc("%d/stat", cur);
		if (!st)
			return 0;
		char *open = strchr(st, '(');
		close = strrchr(st, ')');
		if (!open || !close || close < open) {
			free(st);
			return 0;
		}
		*close = 0;
		int is_conmon = !strcmp(open + 1, "conmon");
		int ppid = 0;
		sscanf(close + 1, " %*c %d", &ppid);
		int self = cur;
		free(st);

		if (is_conmon)
			return conmon_name(self, out, cap);
		cur = ppid;
	}
	return 0;
}

int kpr_box_of(const KprSample *s, int pid, char *out, size_t cap)
{
	if (out && cap)
		out[0] = 0;
	if (!s || !out || !cap)
		return 0;

	/*
	 * The walk starts at the PARENT, not at pid itself. conmon runs on the
	 * host and supervises the box from outside it; matching it against
	 * itself would report the supervisor as a member of the container it
	 * supervises, and put it in the application's rollup.
	 */
	const KprProc *self = kpr_find_pid(s, pid);
	if (!self)
		return 0;

	int cur = self->ppid;
	for (int hop = 0; hop < KPR_BOX_HOPS && cur > 1; hop++) {
		const KprProc *p = kpr_find_pid(s, cur);
		if (!p)
			return 0;
		if (!strcmp(p->comm, "conmon"))
			return conmon_name(p->pid, out, cap);
		cur = p->ppid;
	}
	return 0;
}

/*
 * uid to name, cached.
 *
 * There is no NSS on this system: /etc/passwd is the whole of the user
 * database, so this reads it once and answers from memory. An unknown uid
 * comes back as its own number rather than as "unknown" — the number is a
 * fact, and a table of "unknown" rows for every service account is not
 * useful.
 */
#define IDENT_CACHE 64

const char *kpr_user_of(int uid)
{
	static struct { int uid; char name[32]; } cache[IDENT_CACHE];
	static int n;
	static char num[16];

	if (uid < 0)
		return "-";
	for (int i = 0; i < n; i++)
		if (cache[i].uid == uid)
			return cache[i].name;

	/*
	 * The user database is a READING like any other, so it moves with the
	 * fixture: a name resolved from the developer's own /etc/passwd makes
	 * a recorded machine render differently on every host, which is the
	 * one thing a golden frame cannot tolerate.
	 */
	const char *pwp = getenv("KPR_PASSWD");
	char *pw = kb_read_all(pwp && *pwp ? pwp : "/etc/passwd", NULL);
	if (pw) {
		for (char *line = pw, *next; line && *line; line = next) {
			char *nl = strchr(line, '\n');
			next = nl ? nl + 1 : NULL;
			if (nl)
				*nl = 0;
			/* name:passwd:uid:... */
			char *c1 = strchr(line, ':');
			if (!c1)
				goto cont;
			char *c2 = strchr(c1 + 1, ':');
			if (!c2)
				goto cont;
			if (atoi(c2 + 1) == uid && n < IDENT_CACHE) {
				*c1 = 0;
				cache[n].uid = uid;
				kb_strlcpy(cache[n].name, line,
					   sizeof(cache[n].name));
				n++;
				free(pw);
				return cache[n - 1].name;
			}
cont:
			if (nl)
				*nl = '\n';
		}
		free(pw);
	}
	snprintf(num, sizeof(num), "%d", uid);
	return num;
}
