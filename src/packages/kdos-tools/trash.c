/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos trash — deleting at a prompt means what deleting on the desktop means
 * ---------------------------------
 *
 *     $ kdos trash notes.txt
 *     trashed notes.txt
 *     $ kdos trash --list
 *       notes.txt          1.2K  2026-08-25 14:02  /home/kdos/notes.txt
 *     $ kdos trash --restore notes.txt
 *     restored to /home/kdos/notes.txt
 *
 * `kdos-desk` has had a Trash icon since it had icons and the CLI had no verb
 * for it, so `rm` at a prompt and Delete on the desktop were two different
 * operations on one machine — one recoverable, one not. THE IMPLEMENTATION IS
 * libkbase's (`kb_trash_*`) and is shared with kdos-desk verbatim: two copies
 * of the freedesktop spec would be two answers to what deleting means, and the
 * one nobody is looking at is the one that drifts.
 *
 * `--empty` is the only irreversible verb here and is the only one that asks.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kdos-tools.h"

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos trash <file>...        move to the trash\n"
		"       kdos trash --list           what is in it\n"
		"       kdos trash --restore <name> put one back where it came from\n"
		"       kdos trash --rm <name>      delete one for good\n"
		"       kdos trash --empty [-y]     delete all of it\n"
		"\nThe same trash kdos-desk shows: ~/.local/share/Trash.\n");
	return 2;
}

static int cmp_when(const void *a, const void *b)
{
	const KbTrashItem *x = a, *y = b;
	int c = strcmp(y->when, x->when);	/* newest first */

	return c ? c : strcmp(x->name, y->name);
}

static int cmd_list(void)
{
	KbTrashItem *v;
	int n = kb_trash_list(&v);

	if (n < 0) {
		fprintf(stderr, "kdos trash: no trash directory\n");
		return 1;
	}
	if (n == 0) {
		printf("the trash is empty\n");
		return 0;
	}
	qsort(v, (size_t)n, sizeof(*v), cmp_when);
	for (int i = 0; i < n; i++) {
		char when[20] = "";

		/* `2026-08-25T14:02:11` reads better with a space in it, and
		 * the seconds are noise in a listing. */
		if (strlen(v[i].when) >= 16) {
			memcpy(when, v[i].when, 16);
			when[10] = ' ';
			when[16] = '\0';
		}
		printf("  %-28s %8s  %-16s  %s\n", v[i].name,
		       kb_human_size(v[i].bytes), when,
		       *v[i].orig ? v[i].orig : "(no record — cannot restore)");
	}
	printf("\n  %d item%s\n", n, n == 1 ? "" : "s");
	free(v);
	return 0;
}

static int cmd_restore(const char *name)
{
	char to[KB_TRASH_PATH];

	if (kb_trash_restore(name, to, sizeof(to)) != 0) {
		/*
		 * The three ways this fails are three different problems and
		 * a caller told only "failed" looks in the wrong place for
		 * every one of them.
		 */
		if (errno == ENOENT)
			fprintf(stderr, "kdos trash: %s is not in the trash, "
				"or has no record to restore it by\n", name);
		else if (errno == EEXIST)
			fprintf(stderr, "kdos trash: %s already exists where "
				"this came from\n", name);
		else
			fprintf(stderr, "kdos trash: could not restore %s: %s\n",
				name, strerror(errno));
		return 1;
	}
	printf("restored to %s\n", to);
	return 0;
}

static int confirmed(const char *q)
{
	char buf[16];

	/* Not a tty: there is nobody to ask, and assuming yes on a pipe is how
	 * a script empties somebody's trash. */
	if (!isatty(0)) {
		fprintf(stderr, "kdos trash: %s — pass -y to mean it\n", q);
		return 0;
	}
	printf("%s [y/N] ", q);
	fflush(stdout);
	if (!fgets(buf, sizeof(buf), stdin))
		return 0;
	return buf[0] == 'y' || buf[0] == 'Y';
}

int kdt_trash(int argc, char **argv)
{
	int rc = 0, put = 0;

	if (argc < 1)
		return usage();
	if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help"))
		return usage();

	if (!strcmp(argv[0], "--list") || !strcmp(argv[0], "-l"))
		return cmd_list();
	if (!strcmp(argv[0], "--restore")) {
		if (argc < 2)
			return usage();
		for (int i = 1; i < argc; i++)
			rc |= cmd_restore(argv[i]);
		return rc;
	}
	if (!strcmp(argv[0], "--rm")) {
		if (argc < 2)
			return usage();
		for (int i = 1; i < argc; i++) {
			if (kb_trash_remove(argv[i]) != 0) {
				fprintf(stderr, "kdos trash: could not delete %s: %s\n",
					argv[i], strerror(errno));
				rc = 1;
			}
		}
		return rc;
	}
	if (!strcmp(argv[0], "--empty")) {
		int yes = argc > 1 && !strcmp(argv[1], "-y");
		int n;

		if (!yes && !confirmed("Empty the trash? This cannot be undone."))
			return 1;
		n = kb_trash_empty();
		if (n < 0) {
			fprintf(stderr, "kdos trash: no trash directory\n");
			return 1;
		}
		printf("emptied the trash (%d item%s)\n", n, n == 1 ? "" : "s");
		return 0;
	}

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-')
			return usage();
		if (kb_trash_put(argv[i]) != 0) {
			/*
			 * EXDEV is the one worth naming: rename(2) cannot
			 * cross a filesystem and a copy-then-delete here would
			 * be a file operation with no undo of its own.
			 */
			if (errno == EXDEV)
				fprintf(stderr, "kdos trash: %s is on another "
					"filesystem than the trash — move it first\n",
					argv[i]);
			else
				fprintf(stderr, "kdos trash: could not trash %s: %s\n",
					argv[i], strerror(errno));
			rc = 1;
			continue;
		}
		put++;
	}
	if (put)
		printf("trashed %d item%s\n", put, put == 1 ? "" : "s");
	return rc;
}
