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

/*
 * THE LAUNCH HALF OF AN ENTRY, read once and in one place.
 *
 * Whatever starts an entry from C — `kdos-appbox open` — reads it through this
 * one struct, so a key added here reaches every such caller at once, and a
 * caller that parsed the keys itself would start the same entry differently.
 * The fields are what deciding HOW to start something needs; everything else
 * about an entry (its icon, its categories, its mime types) belongs to
 * whoever is drawing it.
 */
#define KXDG_LAUNCH_EXEC 512
typedef struct {
	char exec[KXDG_LAUNCH_EXEC];	/* Exec, FIELD CODES INTACT — see
					   kxdg_exec_split                  */
	char name[128];			/* Name                             */
	int terminal;			/* Terminal=true                    */
} KxdgLaunch;

/*
 * Fill one from a loaded entry. Returns 0 when the entry can be started at
 * all — Type=Application with an Exec — and -1 otherwise, so a caller's
 * "is this a row" test and its read are the same call.
 *
 * NoDisplay and Hidden are NOT tested here: they say whether an entry belongs
 * in a MENU, which is a question for whoever is drawing one, and the mime
 * route opens NoDisplay entries on purpose.
 */
int kxdg_launch_read(const KxdgEntry *e, KxdgLaunch *out);

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
 * backwards and every `.tar.gz` opens in a decompressor. One copy, so every
 * program that opens a file gives the same answer.
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

/*
 * WHAT A COMMAND-LINE ARGUMENT IS. One place decides, because the chooser and
 * the opener must not disagree about the same word.
 *
 * A scheme makes it a URL, typed `x-scheme-handler/<scheme>`. That is the only
 * thing that can be true of `mailto:a@b.c`, whose basename otherwise matches
 * the `*.C` glob — case-insensitively — and resolves to C++ source, a mail
 * address handed to an editor. `file:` is the exception: it names a path, so
 * the path is unwrapped and typed like any other. A name that `stat()`s is a
 * path whatever it looks like, so a real `notes:2026.txt` is not read as a
 * scheme; `stat` follows symlinks, so a dangling one is not a name that is
 * there.
 *
 * THE RETURN VALUE IS WHAT THE CALLER MUST PASS ON — the unwrapped path for
 * `file:`, the argument itself otherwise — and it is READ-ONLY. It points into
 * `arg`, except for a `file:` URL naming no path at all, where it is a static
 * `/`; either way it outlives the call. Re-resolving the raw argument
 * afterwards would open something other than the thing that was typed.
 *
 * PERCENT-ESCAPES ARE NOT DECODED, and a user sees it:
 * `file:///home/kdos/My%20Report.pdf`, which is what a conforming caller emits
 * for a name with a space, resolves to a path that does not exist and opens
 * nothing.
 */
const char *kxdg_mime_for_arg(const char *arg, char *out, size_t n);
/* The icon names a type may be drawn with, most specific first. Returns how
 * many were written. */
int kxdg_mime_icon_names(const char *mime, char out[][64], int n);

/* ────────────────────────────────────────────────────────────────────────
 * Handlers (kxdg_handlers.c)
 *
 * WHAT OPENS A TYPE, from one resolver. `kdos-appbox open` acts on the answer
 * and the Settings window's Apps page shows it; a second resolver would let
 * the page name one program while a double-click starts another.
 * ──────────────────────────────────────────────────────────────────────── */

/* Which list answered: the user's [Default Applications], the user's
 * [Added Associations], the shipped defaults along $XDG_CONFIG_DIRS, or an
 * installed entry's MimeType through a mimeinfo.cache. */
enum { KXDG_LAYER_NONE = 0, KXDG_LAYER_USER, KXDG_LAYER_ADDED,
       KXDG_LAYER_SYSTEM, KXDG_LAYER_CACHE };

typedef struct {
	char id[192];		/* the desktop id a list named              */
	char path[600];		/* the installed entry it resolved to       */
	int layer;		/* KXDG_LAYER_* that named it first         */
} KxdgHandler;

/*
 * Every installed handler for `mime`, best first. Returns how many were
 * written, up to `max`.
 *
 * `*defaulted` (may be NULL) is 1 when a [Default Applications] line answered,
 * the user's or the shipped one, rather than merely the first thing that
 * claimed the type: a default or a single candidate opens without asking, two
 * or more with no default put the chooser up. The user's default stops the
 * search, so it is then the only row.
 */
int kxdg_handlers(const char *mime, KxdgHandler *out, int max, int *defaulted);
/* A layer as a person reads it: "your default", "KDOS default", … */
const char *kxdg_layer_name(int layer);

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

#endif /* KXDG_H */
