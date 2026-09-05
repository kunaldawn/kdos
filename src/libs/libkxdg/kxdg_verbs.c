/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The verbs on a file, in one table.
 *
 * THREE SURFACES ASK THE SAME QUESTION. The desktop's icons, the file
 * chooser's rows and `mc`'s `F2` all offer "what can I do with this", and
 * three tables meant a verb landed on one surface and not the others — which
 * reads as the surface being incomplete rather than as three lists.
 *
 * A VERB NAMES A PROGRAM, AND THE PROGRAM IS RESOLVED. `kdos-peek`,
 * `kdos-find` and `kdos share` are named here and are not built yet; a row
 * whose program is absent is not offered, so those turn on when they ship
 * with no edit to any caller. That is the whole reason the availability lives
 * in the table rather than in each surface's `show` callback.
 *
 * THE TABLE BUILDS AN ARGUMENT VECTOR, never a command line. libkxdg already
 * owns the one correct way to turn a desktop entry's Exec into an argv, and a
 * verb that handed a surface a string to split would be a second one.
 *
 * `mc`'s `F2` READS A TEXT FILE and cannot ask any of this, which is why that
 * file carries only the verbs whose programs exist today and
 * `testing/preflight.sh` refuses one that does not.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kbase.h"
#include "kxdg.h"

/*
 * The order is the order they are offered, and it is the order of how often a
 * hand reaches for them rather than any grouping: open, then the two ways of
 * looking, then the things that change something.
 */
static const struct {
	int id;
	const char *label;	/* `&` marks the accelerator */
	const char *prog;	/* what must exist for the row to be offered */
	int flags;
} VERBS[] = {
	{ KXDG_VERB_OPEN,  "&Open",              "kdos-appbox", KXDG_V_BOTH },
	{ KXDG_VERB_PEEK,  "Pee&k",              "kdos-peek",   KXDG_V_FILE },
	{ KXDG_VERB_EDIT,  "&Edit",              NULL,          KXDG_V_FILE },
	{ KXDG_VERB_TERM,  "Open &Terminal Here", NULL,         KXDG_V_BOTH },
	{ KXDG_VERB_FIND,  "&Find Here",         "kdos-find",   KXDG_V_DIR  },
	{ KXDG_VERB_PLACE, "Add to &Places",     NULL,          KXDG_V_DIR  },
	{ KXDG_VERB_SHARE, "&Share",             "kdos-share",  KXDG_V_BOTH },
	{ KXDG_VERB_GIT,   "&Git Status Here",   "lazygit",     KXDG_V_BOTH },
	{ KXDG_VERB_EXTRACT, "E&xtract Here",    "kdos-openarchive", KXDG_V_FILE },
	{ KXDG_VERB_TRASH, "&Move to Trash",     NULL,          KXDG_V_BOTH },
};
#define NVERBS ((int)(sizeof(VERBS) / sizeof(VERBS[0])))

int kxdg_verb_count(void)
{
	return NVERBS;
}

int kxdg_verb_at(int i, KxdgVerb *out)
{
	if (i < 0 || i >= NVERBS || !out)
		return 0;
	out->id = VERBS[i].id;
	out->label = VERBS[i].label;
	out->flags = VERBS[i].flags;
	/*
	 * ASKED EVERY TIME rather than cached. A surface is long-lived and a
	 * program can be installed under it; a table resolved once at start
	 * would hide a verb for the life of the desktop.
	 */
	out->present = !VERBS[i].prog || kb_have_prog(VERBS[i].prog);
	return 1;
}

int kxdg_verb_shown(const KxdgVerb *v, const char *path, int isdir)
{
	if (!v || !v->present || !path || !*path)
		return 0;
	if (isdir && !(v->flags & KXDG_V_DIR))
		return 0;
	if (!isdir && !(v->flags & KXDG_V_FILE))
		return 0;
	return 1;
}

/*
 * The argv for a verb, into the caller's own storage.
 *
 * `store` holds what the vector points at — a terminal's identity argument,
 * the directory a verb acts in — because an argv of pointers into a stack
 * frame is a vector that outlives what it names. The caller keeps both until
 * the spawn.
 */
int kxdg_verb_argv(int id, const char *path, int isdir, const char *term,
		   char *store, size_t cap, const char **argv, int max)
{
	char dir[1024];
	int n = 0;

	if (!path || !*path || !argv || max < 4)
		return 0;

	/* The directory a verb acts in: the path itself for a folder, the one
	 * holding it for a file — which is what "here" means when the cursor
	 * is on a document. */
	snprintf(dir, sizeof(dir), "%s", path);
	if (!isdir) {
		char *slash = strrchr(dir, '/');

		if (slash && slash != dir)
			*slash = '\0';
		else if (slash)
			dir[1] = '\0';
	}

	switch (id) {
	case KXDG_VERB_OPEN:
		argv[n++] = "kdos-appbox";
		argv[n++] = "open";
		argv[n++] = path;
		break;
	case KXDG_VERB_PEEK:
		argv[n++] = "kdos-peek";
		argv[n++] = path;
		break;
	case KXDG_VERB_EDIT:
		/* THE EDITOR IS $EDITOR AND IT IS A TERMINAL PROGRAM, so it is
		 * wrapped in one. `bash.bashrc` sets the variable; a machine
		 * that has not is given the editor every image carries. */
		if (!term || !*term)
			return 0;
		argv[n++] = term;
		argv[n++] = "-e";
		argv[n++] = getenv("EDITOR") ? getenv("EDITOR") : "nano";
		argv[n++] = path;
		break;
	case KXDG_VERB_TERM:
		if (!term || !*term)
			return 0;
		if (snprintf(store, cap, "%s", dir) >= (int)cap)
			return 0;
		argv[n++] = term;
		argv[n++] = "-D";
		argv[n++] = store;
		break;
	case KXDG_VERB_FIND:
		if (snprintf(store, cap, "%s", dir) >= (int)cap)
			return 0;
		argv[n++] = "kdos-find";
		argv[n++] = store;
		break;
	case KXDG_VERB_PLACE:
		argv[n++] = "kdos";
		argv[n++] = "places";
		argv[n++] = "add";
		argv[n++] = path;
		break;
	case KXDG_VERB_SHARE:
		argv[n++] = "kdos-share";
		argv[n++] = path;
		break;
	case KXDG_VERB_GIT:
		if (!term || !*term)
			return 0;
		if (snprintf(store, cap, "%s", dir) >= (int)cap)
			return 0;
		argv[n++] = term;
		argv[n++] = "-D";
		argv[n++] = store;
		argv[n++] = "-e";
		argv[n++] = "lazygit";
		break;
	case KXDG_VERB_EXTRACT:
		/* THE ARCHIVE, NOT THE DIRECTORY. `kdos-openarchive` extracts
		 * beside the file it is given and sets its own working
		 * directory, so this hands it the path and nothing else. */
		argv[n++] = "kdos-openarchive";
		argv[n++] = path;
		break;
	case KXDG_VERB_TRASH:
		argv[n++] = "kdos";
		argv[n++] = "trash";
		argv[n++] = path;
		break;
	default:
		return 0;
	}
	if (n >= max)
		return 0;
	argv[n] = NULL;
	return n;
}
