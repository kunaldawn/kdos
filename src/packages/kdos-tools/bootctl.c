/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-bootctl — two root slots, and a boot that can change its mind
 *
 *   kdos-bootctl status                what the state file says
 *   kdos-bootctl set-slot a <uuid>     record where a slot lives
 *   kdos-bootctl try <slot> [n]        boot that slot n times, then give up
 *   kdos-bootctl select                DECIDE and count down (the initramfs)
 *   kdos-bootctl mark-good             this boot worked (the end of rcS)
 *
 * The shape is RAUC's state machine and none of its dependencies: a file with
 * `active`, `try` and `attempts` in it, one decision, and one place that
 * decrements. What it replaces is the thing rEFInd does not have — **boot
 * counting**. systemd-boot counts by renaming files with `+N-M` suffixes;
 * rEFInd has nothing of the sort, so the counting is ours, and it belongs in the
 * INITRAMFS rather than in `rcS`: a kernel that boots into a wedged userland
 * must still be caught, and `rcS` in that userland never runs to say so.
 *
 * THE STATE FILE LIVES ON THE ESP, WHICH IS FAT AND HAS NO JOURNAL. A torn write
 * here bricks the machine — not "fails to update", bricks: the initramfs cannot
 * tell which slot to boot. So every write is temp file, `fsync` the file,
 * `fsync` the DIRECTORY, then `rename`. The directory fsync is the step people
 * leave out, and without it the rename can be lost while the data survives.
 *
 * A file that cannot be parsed is treated as ABSENT, never as partial: half a
 * state file that looked complete is exactly how a machine ends up booting a
 * slot that was never installed. Absent means "boot the root the kernel command
 * line already names", which is what a machine with no A/B setup does anyway.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kdos-tools.h"

#define BOOTSTATE_DEFAULT "/boot/efi/EFI/kdos/bootstate"
#define TRY_ATTEMPTS 3

typedef struct {
	char slot[2][80];	/* a, b — the root UUID of each, "" if unset */
	int active;		/* 0 = a, 1 = b                              */
	int trying;		/* -1 when not trying, else the slot index   */
	int attempts;
} BootState;

static const char *slot_name(int i)
{
	return i == 1 ? "b" : "a";
}

static int slot_index(const char *s)
{
	if (!s)
		return -1;
	if (!strcmp(s, "a") || !strcmp(s, "A"))
		return 0;
	if (!strcmp(s, "b") || !strcmp(s, "B"))
		return 1;
	return -1;
}

static const char *state_path(void)
{
	/* Overridable so the state machine can be exercised without an ESP —
	 * the same reason `kdos stutter` can be pointed at a fixture /proc. */
	const char *e = getenv("KDOS_BOOTSTATE");
	return (e && *e) ? e : BOOTSTATE_DEFAULT;
}

/* ── reading ───────────────────────────────────────────────────────────── */

static int state_load(BootState *st)
{
	memset(st, 0, sizeof(*st));
	st->trying = -1;
	st->active = 0;

	size_t len = 0;
	char *data = kb_read_all(state_path(), &len);
	if (!data)
		return -1;

	int saw_active = 0;
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		char buf[256];
		size_t n = nl ? (size_t)(nl - line) : strlen(line);

		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, line, n);
		buf[n] = 0;
		line = nl ? nl + 1 : NULL;

		char *eq = strchr(buf, '=');
		if (!buf[0] || buf[0] == '#' || !eq)
			continue;
		*eq = 0;
		char *k = buf, *v = eq + 1;
		while (*k == ' ' || *k == '\t')
			k++;
		for (char *e = k + strlen(k); e > k && (e[-1] == ' ' || e[-1] == '\t');)
			*--e = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		v[strcspn(v, " \t\r")] = 0;

		if (!strcmp(k, "slot_a"))
			kb_strlcpy(st->slot[0], v, sizeof(st->slot[0]));
		else if (!strcmp(k, "slot_b"))
			kb_strlcpy(st->slot[1], v, sizeof(st->slot[1]));
		else if (!strcmp(k, "active")) {
			int i = slot_index(v);
			if (i < 0) {
				free(data);
				return -1;	/* unparsable: treat as absent */
			}
			st->active = i;
			saw_active = 1;
		} else if (!strcmp(k, "try")) {
			st->trying = v[0] ? slot_index(v) : -1;
		} else if (!strcmp(k, "attempts")) {
			st->attempts = atoi(v);
		}
	}
	free(data);

	/* The one field without which nothing else means anything. */
	if (!saw_active)
		return -1;
	if (st->trying >= 0 && !st->slot[st->trying][0])
		st->trying = -1;	/* trying a slot that has no root */
	return 0;
}

/* ── writing, the only part that can brick a machine ───────────────────── */

static int state_save(const BootState *st)
{
	const char *path = state_path();
	char tmp[1024], dir[1024];

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	else
		kb_strlcpy(dir, ".", sizeof(dir));

	KbBuf b = {0};
	kb_buf_printf(&b,
		      "# KDOS boot state. Written by kdos-bootctl; the initramfs\n"
		      "# reads it before it mounts a root filesystem.\n"
		      "slot_a   = %s\n"
		      "slot_b   = %s\n"
		      "active   = %s\n"
		      "try      = %s\n"
		      "attempts = %d\n",
		      st->slot[0], st->slot[1], slot_name(st->active),
		      st->trying >= 0 ? slot_name(st->trying) : "",
		      st->attempts);

	int rc = -1;
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		goto out;
	if (write(fd, b.p, b.n) != (ssize_t)b.n) {
		close(fd);
		unlink(tmp);
		goto out;
	}
	/* The data, then the directory entry. FAT has no journal: without the
	 * second fsync the rename can be lost while the bytes survive, and the
	 * machine comes up reading the OLD state — which is the good failure —
	 * or, with the fsyncs in the other order, a zero-length file, which is
	 * the one that bricks it. */
	if (fsync(fd) != 0) {
		close(fd);
		unlink(tmp);
		goto out;
	}
	close(fd);

	if (rename(tmp, path) != 0) {
		unlink(tmp);
		goto out;
	}
	int dfd = open(dir, O_RDONLY | O_DIRECTORY);
	if (dfd >= 0) {
		fsync(dfd);
		close(dfd);
	}
	rc = 0;
out:
	kb_buf_free(&b);
	return rc;
}

/* ── the commands ──────────────────────────────────────────────────────── */

static int cmd_status(const BootState *st, int have, int json)
{
	if (!have) {
		if (json)
			printf("{\"configured\": false}\n");
		else
			printf("no boot state at %s — this machine has one "
			       "root and no rollback\n", state_path());
		return 1;
	}
	if (json) {
		printf("{\"configured\": true, \"active\": \"%s\", "
		       "\"trying\": %s%s%s, \"attempts\": %d, "
		       "\"slot_a\": \"%s\", \"slot_b\": \"%s\"}\n",
		       slot_name(st->active),
		       st->trying >= 0 ? "\"" : "null",
		       st->trying >= 0 ? slot_name(st->trying) : "",
		       st->trying >= 0 ? "\"" : "", st->attempts,
		       st->slot[0], st->slot[1]);
		return 0;
	}
	printf("active   %s  %s\n", slot_name(st->active),
	       st->slot[st->active][0] ? st->slot[st->active] : "(no root)");
	printf("other    %s  %s\n", slot_name(!st->active),
	       st->slot[!st->active][0] ? st->slot[!st->active] : "(no root)");
	if (st->trying >= 0)
		printf("trying   %s, %d attempt(s) left\n",
		       slot_name(st->trying), st->attempts);
	else
		printf("trying   nothing — this slot is confirmed good\n");
	return 0;
}

/*
 * `select` is the initramfs's whole job here: decide, count down, and print the
 * UUID to boot. Everything it can do is one of three things —
 *
 *   trying and attempts left   spend one and boot the candidate
 *   trying and out of attempts ROLL BACK: forget the candidate, boot active
 *   not trying                 boot active
 *
 * — and the counting happens BEFORE the boot, not after, because a boot that
 * hangs must still consume its attempt. That is the whole reason this is not in
 * `rcS`.
 */
static int cmd_select(BootState *st, int have)
{
	if (!have)
		return 1;	/* nothing to say; the caller keeps root= */

	int boot = st->active;
	if (st->trying >= 0) {
		if (st->attempts > 0) {
			st->attempts--;
			boot = st->trying;
			if (st->attempts == 0)
				fprintf(stderr, "bootctl: last attempt at slot "
						"%s\n", slot_name(st->trying));
		} else {
			fprintf(stderr, "bootctl: slot %s failed %d times — "
					"rolling back to %s\n",
				slot_name(st->trying), TRY_ATTEMPTS,
				slot_name(st->active));
			st->trying = -1;
			st->attempts = 0;
		}
		/* Written before anything boots. A machine that loses power
		 * here comes up with the old state and simply tries again,
		 * which is the failure worth having. */
		if (state_save(st) != 0)
			fprintf(stderr, "bootctl: WARNING: could not write the "
					"boot state — this boot will not "
					"count\n");
	}

	if (!st->slot[boot][0])
		return 1;
	printf("%s\n", st->slot[boot]);
	return 0;
}

/*
 * `mark-good` is the other half, and it runs at the END of rcS — after the
 * services that would tell you the userland is broken have started. Confirming
 * earlier would confirm a system that has not yet failed rather than one that
 * has succeeded.
 */
static int cmd_mark_good(BootState *st, int have)
{
	if (!have)
		return 0;	/* nothing to confirm is not a failure */
	if (st->trying < 0)
		return 0;

	st->active = st->trying;
	st->trying = -1;
	st->attempts = 0;
	if (state_save(st) != 0) {
		fprintf(stderr, "bootctl: cannot write the boot state\n");
		return 1;
	}
	printf("slot %s confirmed good\n", slot_name(st->active));
	return 0;
}

int bootctl_main(int argc, char **argv)
{
	BootState st;
	int have = state_load(&st) == 0;
	const char *cmd = argc > 1 ? argv[1] : "status";

	if (!strcmp(cmd, "status"))
		return cmd_status(&st, have, argc > 2 && !strcmp(argv[2], "--json"));
	if (!strcmp(cmd, "select"))
		return cmd_select(&st, have);
	if (!strcmp(cmd, "mark-good"))
		return cmd_mark_good(&st, have);

	if (!strcmp(cmd, "set-slot")) {
		if (argc < 4) {
			fprintf(stderr, "usage: kdos-bootctl set-slot <a|b> "
					"<uuid>\n");
			return 2;
		}
		int i = slot_index(argv[2]);
		if (i < 0) {
			fprintf(stderr, "bootctl: no slot named '%s'\n", argv[2]);
			return 2;
		}
		if (!have) {
			/* First write on a machine that had no state: the slot
			 * being described is the one running, so it is active
			 * and confirmed. */
			memset(&st, 0, sizeof(st));
			st.trying = -1;
			st.active = i;
		}
		kb_strlcpy(st.slot[i], argv[3], sizeof(st.slot[i]));
		return state_save(&st) == 0 ? 0 : 1;
	}

	if (!strcmp(cmd, "try")) {
		if (argc < 3) {
			fprintf(stderr, "usage: kdos-bootctl try <a|b> [n]\n");
			return 2;
		}
		if (!have) {
			fprintf(stderr, "bootctl: no boot state to update\n");
			return 1;
		}
		int i = slot_index(argv[2]);
		if (i < 0) {
			fprintf(stderr, "bootctl: no slot named '%s'\n", argv[2]);
			return 2;
		}
		if (!st.slot[i][0]) {
			/* Refused rather than recorded: a `try` pointing at a
			 * slot with no root is a state file that will send the
			 * next boot nowhere. */
			fprintf(stderr, "bootctl: slot %s has no root "
					"filesystem\n", slot_name(i));
			return 1;
		}
		if (i == st.active) {
			fprintf(stderr, "bootctl: slot %s is already active\n",
				slot_name(i));
			return 1;
		}
		st.trying = i;
		st.attempts = argc > 3 ? atoi(argv[3]) : TRY_ATTEMPTS;
		if (st.attempts < 1)
			st.attempts = 1;
		if (state_save(&st) != 0)
			return 1;
		printf("will boot slot %s up to %d time(s), then roll back to "
		       "%s\n", slot_name(i), st.attempts,
		       slot_name(st.active));
		return 0;
	}

	fprintf(stderr,
		"usage: kdos-bootctl {status [--json]|select|mark-good|\n"
		"                     set-slot <a|b> <uuid>|try <a|b> [n]}\n");
	return 2;
}
