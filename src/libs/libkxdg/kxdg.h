/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkxdg — desktop entries and the mime cache
 *
 * Enough of the freedesktop file formats for KDOS to present ~90 alien apps
 * as its own, and no more. Deliberately matches what python's
 * RawConfigParser(strict=False) did with these files, because the launchers
 * it produced are committed to git.
 * ---------------------------------
 */

#ifndef KXDG_H
#define KXDG_H

#include "kbase.h"

typedef struct {
	char *key;
	char *val;
} KxdgPair;

typedef struct {
	KxdgPair *v;
	int n, cap;
} KxdgEntry;

/*
 * Read one named section — "Desktop Entry" — out of an INI-shaped file.
 * Sections other than the named one are ignored, which is the whole point:
 * a [Desktop Action ...] section must never shadow the entry's own Name or
 * Exec. A repeated key inside the section takes its LAST value, and keys are
 * matched case-insensitively, both matching configparser.
 *
 * Returns 0 if the section was found, -1 otherwise.
 */
int kxdg_load(KxdgEntry *e, const char *path, const char *section);
const char *kxdg_get(const KxdgEntry *e, const char *key, const char *def);
void kxdg_free(KxdgEntry *e);

/* "true"/"TRUE"/"True" -> 1. Anything else, including absent, -> def. */
int kxdg_bool(const KxdgEntry *e, const char *key, int def);

/* ────────────────────────────────────────────────────────────────────────
 * Exec (kxdg_exec.c)
 *
 * An `Exec=` value is NOT a whitespace-separated list, and treating it as one
 * is how alien apps fail to start with nothing on screen to say why. Two
 * things it carries that a `strtok(" ")` gets wrong, both measured against the
 * shipped appbox:
 *
 *   - QUOTING. `Exec="/usr/bin/gsmartcontrol-root"` is one argument and the
 *     quotes are not part of it; `sh -c "wesnoth-1.18 >/dev/null 2>&1"` is
 *     three. Split naively, the first exec's a file whose name begins with a
 *     quote and the second hands `sh` a fragment.
 *   - FIELD CODES. `%U`, `%F`, `%f`, `%u` are placeholders for what the user
 *     picked, and with nothing picked they must VANISH. Passed through
 *     literally, `mpv … -- %U` tries to play a file called `%U` and exits,
 *     `gimp-3.0 %U` opens an error dialog, and every one of those reads to a
 *     person as "the app does not launch". `%i %c %k %d %D %n %N %v %m` are
 *     dropped always — nothing here fills them in.
 *
 * `%f`/`%u` take ONE file (the first), `%F`/`%U` take all of them, which is
 * the spec and is also the difference between opening four images in one
 * viewer and opening four viewers.
 *
 * `nfiles < 0` keeps every field code VERBATIM and expands none of them —
 * what a tool that rewrites an Exec line rather than running it needs, so the
 * placeholders survive into the file it writes.
 *
 * The argv entries point INTO `store`, which the caller owns; nothing is
 * allocated. Returns the number of arguments written, or 0 when the line is
 * empty or does not fit.
 */
int kxdg_exec_split(const char *exec, const char *const *files, int nfiles,
		    char *store, size_t cap, const char **argv, int max);

/*
 * One argument, quoted so kxdg_exec_split reads it back as one argument.
 * Quotes only when it has to — a rewritten Exec line that put quotes round
 * every word would be correct and unreadable. Returns 0 on success.
 */
int kxdg_exec_quote(const char *arg, char *out, size_t n);

/* ────────────────────────────────────────────────────────────────────────
 * MIME (kxdg_mime.c)
 *
 * The freedesktop glob table, and the LONGEST matching suffix wins — get that
 * backwards and every `.tar.gz` opens in a decompressor. One copy, because
 * there were two inside kdos-shell alone the moment icons wanted one.
 *
 * KXDG_MIME_GLOBS is overridable so the selftest can point it at a fixture;
 * it is never read from the environment, because a program that resolved
 * types differently under one user's $MIMEGLOBS would open different
 * applications for the same double-click.
 * ──────────────────────────────────────────────────────────────────────── */

#ifndef KXDG_MIME_GLOBS
#define KXDG_MIME_GLOBS "/usr/share/mime/globs"
#endif

/* Returns 1 when a type was found for this BASENAME. */
int kxdg_mime_from_globs(const char *base, char *out, size_t n);
/* A path's type, with `inode/directory` for a directory and
 * `application/octet-stream` when nothing matched. Never fails. */
void kxdg_mime_for_path(const char *path, char *out, size_t n);
/* The icon names a type may be drawn with, most specific first. Returns how
 * many were written. */
int kxdg_mime_icon_names(const char *mime, char out[][64], int n);

/*
 * The files `app` most recently opened, newest first, from freedesktop's
 * recently-used.xbel. Returns how many were written.
 *
 * This is a jump list's data. Only paths that still EXIST are returned — a
 * destination that has been deleted is not a destination — and duplicates are
 * collapsed. A missing or unreadable store is simply an empty list.
 */
int kxdg_recent(const char *app, char out[][512], int max);

/* Every application's, newest first — what a Recent list on a menu wants,
 * where a jump list wants one program's. Same rules: only paths that still
 * exist, duplicates collapsed. */
int kxdg_recent_all(char out[][512], int max);

/* Record an open. `mime` NULL is derived from the path.
 *
 * ONE ENTRY PER URI: an existing bookmark for this file is cut out and a fresh
 * one appended, so the store holds it once and at the end — which is what
 * "newest first" reads as on the way back. The oldest past the cap are dropped
 * on the same pass, because nothing else on this system prunes the file.
 *
 * TEMP AND RENAME. The store is shared with every other program on the machine
 * that keeps recents, and a half-written one is one they all lose. Returns 0
 * written, -1 refused — a relative path, or no home to write into. */
int kxdg_recent_add(const char *app, const char *path, const char *mime);

/* ────────────────────────────────────────────────────────────────────────
 * Places
 *
 * ONE READER FOR THE WHOLE DESKTOP. The desktop folder, the Places menu and
 * the chooser's sidebar all resolve here, so a renamed user directory moves
 * all three together. There is no xdg-user-dirs on this system: KDOS seeds
 * `~/.config/user-dirs.dirs` from `/etc/skel` and it is the user's to edit.
 * ──────────────────────────────────────────────────────────────────────── */

enum { KXDG_PLACES_MAX = 32 };

typedef struct {
	char name[64];
	char path[512];
} KxdgPlace;

/* One XDG user directory by its key — "DESKTOP", "DOCUMENTS", "DOWNLOAD",
 * "MUSIC", "PICTURES", "VIDEOS". Writes the default under $HOME first, so a
 * caller that ignores the return value still has a usable path. Returns 0 for
 * a key this system does not carry. */
int kxdg_user_dir(const char *key, char *out, size_t n);

/* The places column: Home, the user directories that EXIST, then the rows in
 * `~/.config/kdos/places` (`Name = /path`, `$HOME` expanded) that are not
 * already listed. A directory that is not there is never returned — the user
 * directories are created on demand, and a row that opens an error is worse
 * than a row that is not offered. Returns how many were written. */
int kxdg_places(KxdgPlace *out, int max);

/* The directories a shell has been in, from `zoxide query -l`, newest in
 * frecency first. Empty when zoxide is absent or its database is — which is
 * the same answer, and is why the absence is not reported.
 *
 * SEPARATE from kxdg_places() on purpose: kxdg_places_add() decides whether a
 * directory is already a place by asking kxdg_places(), so folding a frecency
 * guess into that answer would make *Add to Places* refuse a folder somebody
 * had merely visited. Pass what the caller already collected as `have` and a
 * directory is not offered twice. Returns how many were written. */
int kxdg_places_recent(KxdgPlace *out, int max, const KxdgPlace *have,
		       int nhave);

/* Append one to `~/.config/kdos/places`. Returns 0 written, 1 already there,
 * -1 refused — a path that does not exist, or a name carrying `=` or a
 * newline, which would split the row somewhere else on the next read. */
int kxdg_places_add(const char *name, const char *path);

/* ────────────────────────────────────────────────────────────────────────
 * The verbs on a file
 *
 * ONE TABLE FOR THREE SURFACES. The desktop's icons, the chooser's rows and
 * `mc`'s `F2` all ask "what can I do with this", and three tables meant a verb
 * landed on one and not the others — which reads as a surface being
 * incomplete rather than as three lists.
 *
 * A ROW WHOSE PROGRAM IS ABSENT IS NOT OFFERED, so a verb still being built
 * turns on when it ships with no edit to any caller. `mc`'s F2 reads a text
 * file and can ask none of this, which is why that file carries only what
 * exists today and `testing/preflight.sh` refuses a row that does not.
 * ──────────────────────────────────────────────────────────────────────── */

enum { KXDG_VERB_OPEN = 0, KXDG_VERB_PEEK, KXDG_VERB_EDIT, KXDG_VERB_TERM,
       KXDG_VERB_FIND, KXDG_VERB_PLACE, KXDG_VERB_SHARE, KXDG_VERB_GIT,
       KXDG_VERB_EXTRACT, KXDG_VERB_TRASH, KXDG_VERB_MAX };

/* Which kind of thing a verb is offered on. */
#define KXDG_V_FILE 1
#define KXDG_V_DIR  2
#define KXDG_V_BOTH (KXDG_V_FILE | KXDG_V_DIR)

typedef struct {
	int id;			/* KXDG_VERB_*; dispatch on this, never on i */
	const char *label;	/* "&Move to Trash" — `&` marks the letter  */
	int flags;		/* KXDG_V_*                                 */
	int present;		/* its program is on this machine           */
} KxdgVerb;

int kxdg_verb_count(void);
/* Fills `out` for row i. `present` is resolved on every call rather than
 * cached: a surface is long-lived, and a table resolved once would hide a verb
 * for the life of the desktop after its program was installed. */
int kxdg_verb_at(int i, KxdgVerb *out);
/* Whether this verb belongs on this thing right now. */
int kxdg_verb_shown(const KxdgVerb *v, const char *path, int isdir);

/* The argument vector for a verb, never a command line. `term` is the
 * terminal a wrapped verb runs in — `kb_terminal()` — and `store` holds what
 * the vector points at, because an argv of pointers into a stack frame is a
 * vector that outlives what it names. Returns the count, or 0 when the verb
 * cannot be built. */
int kxdg_verb_argv(int id, const char *path, int isdir, const char *term,
		   char *store, size_t cap, const char **argv, int max);

#endif /* KXDG_H */
