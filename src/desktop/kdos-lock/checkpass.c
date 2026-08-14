/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-checkpass — setuid root, one question, one answer
 *
 *     printf '%s' "$password" | kdos-checkpass   ->  exit 0 = correct
 *
 * This is the ONLY privileged part of the lock screen, and it exists because
 * /etc/shadow is root-only and `kdos-lock` must not be. Same shape and same
 * threat model as every distro's kcheckpass. Review it as the setuid binary it
 * is: everything below is either an input it must not trust or a privilege it
 * must not keep.
 *
 * FIVE DECISIONS, EACH ONE REMOVING AN ATTACK RATHER THAN ADDING A FEATURE:
 *
 *   1. IT TAKES NO ARGUMENTS. Not even a user name. The account checked is
 *      always the one belonging to the REAL uid of the caller, so there is no
 *      "which user" to get wrong, no way to aim it at root, and nothing an
 *      attacker can vary. A `kdos-checkpass root` would be an oracle for
 *      root's password, one guess per exec, from any unprivileged process.
 *   2. THE PASSWORD ARRIVES ON STDIN. argv is world-readable through
 *      /proc/<pid>/cmdline for as long as the process lives; a password there
 *      is a password published.
 *   3. PRIVILEGE IS DROPPED THE MOMENT THE HASH IS READ. crypt() runs
 *      unprivileged, so a defect in it is not a defect in a root process.
 *   4. THE COMPARISON IS CONSTANT TIME over the whole hash. crypt() output is
 *      not a secret, but a `strcmp` here is a free timing signal and the fix
 *      is four lines.
 *   5. A LOCKED OR PASSWORDLESS ACCOUNT ALWAYS FAILS. `!`, `*` and an EMPTY
 *      hash all mean "this account has no password to check", and the answer
 *      to "is this the right password" for such an account is no — an empty
 *      hash accepting an empty string would unlock the machine with Enter.
 *
 * Exit codes: 0 correct, 1 wrong, 2 could not tell (no such user, no shadow
 * entry, unreadable file). A caller must treat 2 as failure and say why — "the
 * password is wrong" for a machine with a broken /etc/shadow sends the user
 * looking in the wrong place.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <crypt.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PASS 512

/* Never grows a `verbose` flag: everything this prints is a fact about a
 * password attempt, and the only consumer is a lock screen that already knows
 * what it asked. */
static int fail(const char *why, int code)
{
	fprintf(stderr, "kdos-checkpass: %s\n", why);
	return code;
}

/* Constant time over `n` bytes. Returns 0 when equal. */
static int ct_diff(const char *a, const char *b, size_t n)
{
	unsigned char d = 0;
	for (size_t i = 0; i < n; i++)
		d |= (unsigned char)a[i] ^ (unsigned char)b[i];
	return d;
}

/* The shadow entry for `user`, or NULL. Read with a plain file parse rather
 * than getspnam() so there is no NSS in a setuid process — musl's is a stub for
 * anything but files, and an NSS module is code this binary would be running as
 * root on somebody else's terms. */
static int shadow_hash(const char *user, char *out, size_t cap)
{
	FILE *f = fopen("/etc/shadow", "r");
	if (!f)
		return -1;

	size_t ulen = strlen(user);
	char line[1024];
	int found = -1;
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, user, ulen) || line[ulen] != ':')
			continue;
		char *hash = line + ulen + 1;
		char *end = strchr(hash, ':');
		if (end)
			*end = '\0';
		hash[strcspn(hash, "\n")] = '\0';
		if (strlen(hash) >= cap)
			break;		/* not a hash we can hold, so not one
					 * we will pretend to have checked */
		snprintf(out, cap, "%s", hash);
		found = 0;
		break;
	}
	fclose(f);
	/* The line is on the stack of a process that is about to exit, but it
	 * held a password hash; clearing it costs nothing. */
	memset(line, 0, sizeof(line));
	return found;
}

/* Read the whole of stdin, up to MAX_PASS. A trailing newline is stripped
 * because `printf '%s\n'` and `printf '%s'` must mean the same thing — the
 * alternative is a caller whose password silently gains a byte. */
static int read_pass(char *out, size_t cap)
{
	size_t n = 0;
	while (n < cap - 1) {
		ssize_t r = read(STDIN_FILENO, out + n, cap - 1 - n);
		if (r < 0)
			return -1;
		if (r == 0)
			break;
		n += (size_t)r;
	}
	out[n] = '\0';
	if (n && out[n - 1] == '\n')
		out[--n] = '\0';
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1)
		return fail("takes no arguments — it checks the calling user's "
			    "own password, read from stdin", 2);
	(void)argv;

	/*
	 * The REAL uid, not the effective one. The effective uid is root here by
	 * definition; using it would check root's password for every caller.
	 */
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	if (!pw || !pw->pw_name)
		return fail("no passwd entry for the calling uid", 2);

	char user[64];
	snprintf(user, sizeof(user), "%s", pw->pw_name);

	char hash[256];
	if (shadow_hash(user, hash, sizeof(hash)) != 0)
		return fail("no shadow entry", 2);

	/*
	 * Locked, disabled or passwordless. Checked BEFORE the password is even
	 * read: there is no answer to give and no reason to hold one in memory.
	 */
	if (!hash[0] || hash[0] == '!' || hash[0] == '*')
		return fail("the account has no password — it cannot be "
			    "unlocked this way", 2);

	char pass[MAX_PASS];
	if (read_pass(pass, sizeof(pass)) != 0)
		return fail("could not read the password from stdin", 2);

	/* Everything privileged is done: the hash is in hand. From here on this
	 * is an ordinary process owned by the caller. */
	if (setuid(uid) != 0)
		return fail("could not drop privileges", 2);

	char *got = crypt(pass, hash);
	memset(pass, 0, sizeof(pass));
	if (!got)
		return fail("crypt() refused the hash", 2);

	size_t n = strlen(hash);
	int ok = strlen(got) == n && ct_diff(got, hash, n) == 0;
	memset(hash, 0, sizeof(hash));

	if (ok)
		return 0;

	/*
	 * A second per wrong answer, here rather than only in the caller: this
	 * is the process an attacker would drive in a loop, and the delay has to
	 * be on the side that cannot be skipped by not using the lock screen.
	 */
	struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
	nanosleep(&ts, NULL);
	return 1;
}
