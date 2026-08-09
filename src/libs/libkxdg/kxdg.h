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

#endif /* KXDG_H */
