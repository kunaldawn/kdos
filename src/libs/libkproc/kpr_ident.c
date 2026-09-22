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
 * conmon's argv carries `-n <name>`. The parse exists once because three
 * walks arrive at it: the two below, which differ only in whether they hold a
 * sample or read /proc as they climb, and kdos-energyd's, which fuses the box
 * question with an attribution policy that must not live in a library. What
 * they do on arrival is the same, and a second copy is a second answer to
 * "what is this box called".
 *
 * The scan is over the NUL separators rather than a strstr of the whole blob:
 * a container actually named "-n" would otherwise match the flag and the next
 * argument would be read as the box.
 */
/*
 * Is this comm the boundary of a box? One predicate, because the NAME of
 * podman's supervisor is a fact about podman rather than about any one
 * consumer, and a program that spelled it itself would keep working right up
 * until podman renamed it.
 */
int kpr_is_box_boundary(const char *comm)
{
	return comm && !strcmp(comm, "conmon");
}

/*
 * ONE SUPERVISOR ANSWERS FOR EVERY PROCESS IN ITS BOX, so the answer is
 * remembered against its pid. A box holds dozens of processes and the parent
 * walk arrives at the same conmon for each of them; without this its cmdline
 * is read and parsed once per process per sample.
 *
 * THE TABLE LIVES FOR ONE SAMPLE. kpr_conmon_forget() empties it at the start
 * of a pass, so a pid the kernel has handed to something else cannot answer
 * with the box the process that had it belonged to.
 */
#define CONMON_CACHE 16

static struct {
	int pid;
	char name[128];
	int set;
} conmon_cache[CONMON_CACHE];
static int conmon_cursor;

void kpr_conmon_forget(void)
{
	memset(conmon_cache, 0, sizeof(conmon_cache));
	conmon_cursor = 0;
}

int kpr_conmon_name(int pid, char *out, size_t cap)
{
	size_t len = 0;
	char path[512];

	for (int i = 0; i < CONMON_CACHE; i++)
		if (conmon_cache[i].set && conmon_cache[i].pid == pid) {
			kb_strlcpy(out, conmon_cache[i].name, cap);
			return conmon_cache[i].name[0] != '\0';
		}

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

	/* A supervisor that names no box is remembered too: that is the
	 * answer every process under a plain parent gets, and re-deriving it
	 * is the case that cost the most. */
	conmon_cache[conmon_cursor].pid = pid;
	kb_strlcpy(conmon_cache[conmon_cursor].name, found ? out : "",
		   sizeof(conmon_cache[conmon_cursor].name));
	conmon_cache[conmon_cursor].set = 1;
	conmon_cursor = (conmon_cursor + 1) % CONMON_CACHE;
	return found;
}

/*
 * The same answer for a caller with no sample in hand, reading /proc as it
 * climbs. kdos stutter, kdos-oomd and kdos-teams all walk this way — none of
 * them holds a table of every pid — and they share this walk rather than each
 * carrying a copy, so they cannot disagree about where a box begins.
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
		int is_conmon = kpr_is_box_boundary(open + 1);
		int ppid = 0;
		sscanf(close + 1, " %*c %d", &ppid);
		int self = cur;
		free(st);

		if (is_conmon)
			return kpr_conmon_name(self, out, cap);
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
		if (kpr_is_box_boundary(p->comm))
			return kpr_conmon_name(p->pid, out, cap);
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
 *
 * A UID THAT RESOLVES TO NOTHING IS CACHED TOO, as the number it answers
 * with. Only a name found in the file was remembered, so every row owned by a
 * uid the file does not name — which is most of a container's process list —
 * re-read and re-parsed the whole file, once per row per redraw. The same is
 * true of a full table: the answer went back to reading the file every time.
 */
#define IDENT_CACHE 64

static char *ident_slot(int uid)
{
	static struct { int uid; char name[32]; int set; } cache[IDENT_CACHE];
	static int n, cursor;

	for (int i = 0; i < n; i++)
		if (cache[i].set && cache[i].uid == uid)
			return cache[i].name;

	int slot;

	if (n < IDENT_CACHE) {
		slot = n++;
	} else {
		/* A ring rather than a table that stops taking entries: one
		 * that filled and then refused sent every later uid back to
		 * reading the file, which is the case this exists for. */
		slot = cursor;
		cursor = (cursor + 1) % IDENT_CACHE;
	}
	cache[slot].uid = uid;
	cache[slot].set = 1;
	cache[slot].name[0] = '\0';
	return cache[slot].name;
}

const char *kpr_user_of(int uid)
{
	if (uid < 0)
		return "-";

	char *slot = ident_slot(uid);

	if (*slot)
		return slot;

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
			if (atoi(c2 + 1) == uid) {
				*c1 = 0;
				kb_strlcpy(slot, line, 32);
				free(pw);
				return slot;
			}
cont:
			if (nl)
				*nl = '\n';
		}
		free(pw);
	}
	/* Not in the file. The number IS the answer, and remembering it is
	 * what stops the next redraw reading the file again. */
	snprintf(slot, 32, "%d", uid);
	return slot;
}
