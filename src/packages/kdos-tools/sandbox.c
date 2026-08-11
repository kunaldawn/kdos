/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos sandbox — Landlock for NATIVE apps
 *
 *   kdos sandbox [opts] -- <cmd> [args...]
 *   kdos sandbox --explain [opts]
 *
 * The appbox has had enforceable per-app confinement since the box profiles
 * landed; native programs — foot, the file manager, our own tools — have had
 * nothing. This is the other half, and it needs no container, no root and no
 * library: three syscalls in libkbase.
 *
 * Two rules carried over from the box profiles, because they are the whole
 * reason that design is trustworthy:
 *
 *   - KDOS does not offer confinement it cannot enforce. Every option here
 *     maps onto a Landlock access bit. There is no `--no-ipc`, because
 *     Landlock cannot do it and a flag that silently does nothing is worse
 *     than a missing one.
 *   - What cannot be enforced is REPORTED, not hidden. On a kernel below ABI
 *     4 `--no-network` cannot be applied at all, and --explain says so rather
 *     than letting the caller believe the network is closed.
 *
 * And one that is specific to Landlock: a ruleset is invisible from outside
 * the process. Nothing can inspect a running program to see what it may
 * touch. `--explain` describes what WILL be asked for; it is not a readback.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "kdos-tools.h"

#define MAXP 64

/* Leading and trailing blanks, in place. */
static char *trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	char *e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		*--e = 0;
	return s;
}

typedef struct {
	const char *ro[MAXP];
	int nro;
	const char *rw[MAXP];
	int nrw;
	int no_network;
	unsigned short tcp[MAXP];	/* connect ports allowed when net is off */
	int ntcp;
} Plan;

/* Without these nothing execs: the loader, the libraries and the config the
 * libraries read. Best-effort by design — a path that is not on this system
 * is skipped, because the set has to cover a live ISO and an installed disk
 * without the caller knowing which one they are on. A path the USER names is
 * a hard error instead. */
static const char *BASE_RO[] = {
	"/usr", "/lib", "/lib64", "/bin", "/sbin", "/etc", "/proc", NULL
};

/* Named individually rather than allowing /dev wholesale. Every one of these
 * is opened for WRITING by ordinary programs — `>/dev/null` is the obvious
 * one, and it was a shell that found this: with /dev missing, the first
 * redirection in the sandboxed command died with "cannot create /dev/null"
 * and the program looked broken rather than confined.
 *
 * /dev itself is deliberately NOT here. Allowing the whole tree read-write
 * hands over every disk, every input device and every DRM node, which is
 * most of what a sandbox is for. */
static const char *BASE_RW[] = {
	"/dev/null", "/dev/zero", "/dev/full", "/dev/random", "/dev/urandom",
	"/dev/tty", "/dev/ptmx", "/dev/pts", NULL
};

static void add(const char **v, int *n, const char *p)
{
	if (*n >= MAXP)
		kb_die("sandbox: more than %d paths", MAXP);
	v[(*n)++] = p;
}

static int parse_profile(Plan *pl, const char *name)
{
	char *dir = kdt_cfg_home("kdos/sandbox");
	char leaf[256];
	snprintf(leaf, sizeof(leaf), "%s.conf", name);
	char *path = kb_path_join(dir, leaf);
	free(dir);

	size_t n = 0;
	char *text = kb_read_all(path, &n);
	if (!text) {
		fprintf(stderr, "kdos sandbox: no profile '%s' (%s)\n", name, path);
		free(path);
		return -1;
	}
	free(path);

	/* Parsed, never sourced — the same rule the phase env files and the box
	 * profiles follow, for the same reason: a config file must not be able
	 * to run anything. */
	for (char *line = text; line && *line;) {
		char *nl = strchr(line, '\n');
		if (nl)
			*nl = 0;
		char *s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s && *s != '#') {
			char *eq = strchr(s, '=');
			if (eq) {
				*eq = 0;
				char *key = trim(s);
				char *val = trim(eq + 1);
				if (!strcmp(key, "read"))
					add(pl->ro, &pl->nro, kb_strdup(val));
				else if (!strcmp(key, "write"))
					add(pl->rw, &pl->nrw, kb_strdup(val));
				else if (!strcmp(key, "network"))
					pl->no_network = !strcmp(val, "off");
				else if (!strcmp(key, "tcp-connect")) {
					if (pl->ntcp < MAXP)
						pl->tcp[pl->ntcp++] =
							(unsigned short)atoi(val);
				} else
					fprintf(stderr, "kdos sandbox: %s: "
						"unknown key '%s'\n", name, key);
			}
		}
		line = nl ? nl + 1 : NULL;
	}
	return 0;
}

static void explain(const Plan *pl, int abi)
{
	printf("Landlock ABI %d\n\n", abi);

	printf("readable\n");
	for (const char **b = BASE_RO; *b; b++)
		printf("  %-28s %s\n", *b,
		       kb_path_exists(*b) ? "(base)" : "(base, absent — skipped)");
	for (int i = 0; i < pl->nro; i++)
		printf("  %s\n", pl->ro[i]);

	printf("\nwritable\n");
	for (const char **b = BASE_RW; *b; b++)
		if (kb_path_exists(*b))
			printf("  %-28s %s\n", *b, "(base)");
	for (int i = 0; i < pl->nrw; i++)
		printf("  %s\n", pl->rw[i]);

	printf("\nnetwork\n");
	if (!pl->no_network)
		printf("  unrestricted — not policed by this profile\n");
	else if (abi < 4)
		printf("  REQUESTED OFF BUT NOT ENFORCED: TCP restriction needs "
		       "Landlock ABI 4, this kernel is %d\n", abi);
	else {
		printf("  TCP bind and connect denied\n");
		for (int i = 0; i < pl->ntcp; i++)
			printf("  except connect to port %u\n", pl->tcp[i]);
	}

	printf("\nnot enforceable here, at any ABI\n");
	printf("  UDP                          Landlock has no UDP rules "
	       "before ABI 10 (Linux 7.2)\n");
	printf("  unix sockets by path         only abstract ones are scoped, "
	       "and only at ABI 6+\n");
	printf("  processes, IPC, devices      namespace concerns — that is what "
	       "an appbox profile is for\n");
	printf("\nThis is what will be REQUESTED. A Landlock ruleset cannot be "
	       "read back\nfrom outside the process, so nothing can report what "
	       "a running program\nactually holds — including this command.\n");
}

int sandbox_main(int argc, char **argv)
{
	Plan pl = {0};
	int i = 1, do_explain = 0;
	const char *profile = NULL;

	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--"))
			{ i++; break; }
		else if (!strcmp(argv[i], "--explain"))
			do_explain = 1;
		else if (!strcmp(argv[i], "--no-network"))
			pl.no_network = 1;
		else if (!strcmp(argv[i], "--read") && i + 1 < argc)
			add(pl.ro, &pl.nro, argv[++i]);
		else if (!strcmp(argv[i], "--write") && i + 1 < argc)
			add(pl.rw, &pl.nrw, argv[++i]);
		else if (!strcmp(argv[i], "--tcp") && i + 1 < argc) {
			if (pl.ntcp < MAXP)
				pl.tcp[pl.ntcp++] =
					(unsigned short)atoi(argv[++i]);
		} else if (argv[i][0] != '-' && !profile)
			profile = argv[i];
		else {
			fprintf(stderr,
				"usage: kdos sandbox [profile] [--read P] "
				"[--write P] [--no-network] [--tcp PORT]\n"
				"                    [--explain] -- <cmd> "
				"[args...]\n");
			return 2;
		}
	}

	if (profile && parse_profile(&pl, profile))
		return 1;

	int abi = kb_landlock_abi();
	if (do_explain) {
		if (abi <= 0) {
			printf("Landlock unavailable (%s) — nothing would be "
			       "enforced.\n", strerror(abi ? -abi : ENOSYS));
			return 1;
		}
		explain(&pl, abi);
		return 0;
	}

	if (i >= argc) {
		fprintf(stderr, "kdos sandbox: nothing to run (missing -- cmd)\n");
		return 2;
	}

	/* Refuse rather than run unconfined. A sandbox command that silently
	 * executes without a sandbox is the one failure mode that would make
	 * every other guarantee here worthless. */
	if (abi <= 0)
		kb_die("no Landlock (%s) — refusing to run unconfined",
		       strerror(abi ? -abi : ENOSYS));
	if (pl.no_network && abi < 4)
		kb_die("--no-network needs Landlock ABI 4, kernel has %d — "
		       "refusing to run with the network open", abi);

	KbLandlock ll;
	int r = kb_landlock_new(&ll, pl.no_network);
	if (r)
		kb_die("landlock_create_ruleset: %s", strerror(-r));

	for (const char **b = BASE_RO; *b; b++)
		kb_landlock_allow(&ll, *b, 0);	/* best effort, see BASE_RO */
	for (const char **b = BASE_RW; *b; b++)
		kb_landlock_allow(&ll, *b, 1);

	for (int k = 0; k < pl.nro; k++)
		if ((r = kb_landlock_allow(&ll, pl.ro[k], 0)))
			kb_die("read %s: %s", pl.ro[k], strerror(-r));
	for (int k = 0; k < pl.nrw; k++)
		if ((r = kb_landlock_allow(&ll, pl.rw[k], 1)))
			kb_die("write %s: %s", pl.rw[k], strerror(-r));
	for (int k = 0; k < pl.ntcp; k++)
		kb_landlock_allow_tcp(&ll, pl.tcp[k], 1);

	if ((r = kb_landlock_enforce(&ll)))
		kb_die("landlock_restrict_self: %s", strerror(-r));

	/* execvp, not a shell: the command and its arguments arrive from argv
	 * and must never be re-parsed by anything. */
	execvp(argv[i], &argv[i]);
	kb_die("exec %s: %s", argv[i], strerror(errno));
	return 127;
}
