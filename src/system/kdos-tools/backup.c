/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos backup run — one restic snapshot, as ~/.config/kdos/backup.conf says
 *
 * WHAT THE NIGHTLY TIMER RUNS, and what a person runs at a prompt. Kup, in
 * System Settings, is the backup a person sets up with the mouse; this is the
 * one written in a file, and both may share one repository: Kup's own prune
 * forgets only the snapshots tagged `kup`, and every snapshot made here is
 * tagged `kdos`, so neither ever removes the other's.
 *
 * THE CONFIGURATION IS PARSED, NEVER SOURCED: `key = value` a line, `#` to the
 * end of a line a comment. `repo` and `password-file` are last-one-wins
 * (`password-file` defaults to ~/.config/kdos/backup.pass); `include` and
 * `exclude` repeat, eight of each at most. A file that is executed is a file
 * that can do anything this program can.
 *
 * THE PASSWORD IS A FILE AND NEVER AN ARGUMENT, and a file anyone else can
 * read is refused before restic is started: group or world readable is the
 * same disclosure as an argument vector, by a slower route, and 0644 is what
 * a text editor leaves. Refusing is the point — saying so after a successful
 * backup teaches nothing, because the key was readable while it ran.
 *
 * AN EXCLUDE IS PASSED AS IT WAS WRITTEN. It is a restic pattern the person
 * wrote, globs and all; escaping it would turn `*.iso` into a file named
 * `*.iso`. Each include and each exclude is one argument, so a folder with a
 * space in its name is one folder.
 *
 * WAITED FOR, with restic's own output on this process's: a timer's job is the
 * process the timer waits on, and restic's sentence (no repository, wrong
 * password, a file it could not read) is the diagnosis. The exit status is
 * restic's — 3 an incomplete snapshot, 10 no repository, 11 locked, 12 a wrong
 * password — or 1 for a refusal here, 2 for usage, 127 for no restic.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kdos-tools.h"

#define BK_MAX 8
#define BK_PATH 512

typedef struct {
	char conf[BK_PATH];
	char repo[BK_PATH];
	char pwfile[BK_PATH];
	char inc[BK_MAX][BK_PATH];
	char exc[BK_MAX][BK_PATH];
	int ninc, nexc;
} BkConf;

static char *trim(char *s)
{
	char *e;

	while (*s == ' ' || *s == '\t')
		s++;
	e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		*--e = '\0';
	return s;
}

/* 0, or -1 with the refusal printed: the file is named, and the reason. */
static int read_conf(BkConf *c)
{
	char *p = kdt_cfg_home("kdos/backup.conf"), *buf;

	memset(c, 0, sizeof(*c));
	snprintf(c->conf, sizeof(c->conf), "%s", p);
	free(p);
	/* kb_read_whole and not a fixed buffer: this is a file a PERSON edits,
	 * and a buffer that stopped seeing the end of one would drop the keys
	 * past the cut with nothing to say so. */
	buf = kb_read_whole(c->conf, NULL);
	if (!buf || !buf[0]) {
		free(buf);
		fprintf(stderr, "kdos backup: %s: no such file — name a "
				"repository and what goes in it\n", c->conf);
		return -1;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp); ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char *hash = strchr(ln, '#'), *eq, *k, *v;

		if (hash)
			*hash = '\0';
		if (!(eq = strchr(ln, '=')))
			continue;
		*eq = '\0';
		k = trim(ln);
		v = trim(eq + 1);
		if (!*k || !*v)
			continue;
		if (!strcmp(k, "repo"))
			snprintf(c->repo, sizeof(c->repo), "%s", v);
		else if (!strcmp(k, "password-file"))
			snprintf(c->pwfile, sizeof(c->pwfile), "%s", v);
		else if (!strcmp(k, "include") && c->ninc < BK_MAX)
			snprintf(c->inc[c->ninc++], BK_PATH, "%s", v);
		else if (!strcmp(k, "exclude") && c->nexc < BK_MAX)
			snprintf(c->exc[c->nexc++], BK_PATH, "%s", v);
	}
	free(buf);
	if (!c->pwfile[0]) {
		p = kdt_cfg_home("kdos/backup.pass");
		snprintf(c->pwfile, sizeof(c->pwfile), "%s", p);
		free(p);
	}
	if (!c->repo[0]) {
		fprintf(stderr, "kdos backup: %s names no repo\n", c->conf);
		return -1;
	}
	if (!c->ninc) {
		fprintf(stderr, "kdos backup: %s names no include path\n",
			c->conf);
		return -1;
	}
	return 0;
}

static int password_ok(const BkConf *c)
{
	struct stat st;

	if (stat(c->pwfile, &st) != 0) {
		fprintf(stderr, "kdos backup: %s: no password file — put the "
				"repository's password in it at mode 0600\n",
			c->pwfile);
		return 0;
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		fprintf(stderr, "kdos backup: %s is mode %04o — it must be "
				"0600, `chmod 600` it\n",
			c->pwfile, (unsigned)(st.st_mode & 07777));
		return 0;
	}
	return 1;
}

static int run(void)
{
	BkConf c;
	KbArgv a = { 0 };
	int rc;

	if (read_conf(&c) != 0 || !password_ok(&c))
		return 1;
	if (!kb_have_prog("restic")) {
		fprintf(stderr, "kdos backup: restic is not installed\n");
		return 127;
	}
	kb_argv_add(&a, "restic");
	kb_argv_add(&a, "-r");
	kb_argv_add(&a, c.repo);
	kb_argv_add(&a, "--password-file");
	kb_argv_add(&a, c.pwfile);
	kb_argv_add(&a, "backup");
	kb_argv_add(&a, "--tag");
	kb_argv_add(&a, "kdos");
	for (int i = 0; i < c.ninc; i++)
		kb_argv_add(&a, c.inc[i]);
	for (int i = 0; i < c.nexc; i++) {
		kb_argv_add(&a, "--exclude");
		kb_argv_add(&a, c.exc[i]);
	}
	kb_argv_end(&a);
	{
		/* restic's own reasons reach the terminal or the timer's log. */
		int was = kb_proc_verbose;

		kb_proc_verbose = 1;
		rc = kb_run_tty(&a);
		kb_proc_verbose = was;
	}
	return rc;
}

int kdt_backup_main(int argc, char **argv)
{
	if (argc == 1 && !strcmp(argv[0], "run"))
		return run();
	if (argc == 1 && (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help"))) {
		printf("usage: kdos backup run\n");
		return 0;
	}
	fprintf(stderr, "usage: kdos backup run\n");
	return 2;
}
