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

#endif /* KXDG_H */
