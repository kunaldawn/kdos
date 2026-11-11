/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   zeal.c — the library's Zeal docsets
 *
 * Each docset carries its own index, `Contents/Resources/docSet.dsidx`, a
 * SQLite database in one of two shapes: Dash's `searchIndex(name, type,
 * path)` or the Core Data tables `ZTOKEN` and `ZTOKENTYPE`. Both are read
 * the way Zeal reads them, opened read-only and kept open between queries.
 * A row opens Zeal at `<family>:<name>`, the docset's own search key.
 *
 * A PREFIX, NEVER A SUBSTRING. Either is one pass over the table, since
 * LIKE is blind to case and a docset's name index is not; a prefix is what
 * keeps the answer to the three shortest names that begin with what was
 * typed, where a substring would match half a docset on three letters.
 *
 * The runner is always built with SQLite (build.sh); a developer host
 * without its headers compiles this file to a source that finds nothing, so
 * the rest of the matcher still builds and is tested there.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "runner.h"

#if __has_include(<sqlite3.h>)
#include <sqlite3.h>

#define ZEAL_SHOWN 5

typedef struct {
	char name[128];		/* CFBundleName                     */
	char family[64];	/* DocSetPlatformFamily: Zeal's key */
	sqlite3 *db;
	int dash;		/* searchIndex; else Core Data      */
} Docset;

static Docset *g_docs;
static int g_ndocs;
static char g_doc_sig[1200];

static void docs_free(void)
{
	for (int i = 0; i < g_ndocs; i++)
		if (g_docs[i].db)
			sqlite3_close(g_docs[i].db);
	free(g_docs);
	g_docs = NULL;
	g_ndocs = 0;
}

/* Where Zeal keeps docsets: `[docsets] path=` in Zeal.conf when set, else
 * its own default under the data directory. */
static void docset_dir(char *out, size_t n)
{
	char conf[1024], *text, *l, *save = NULL;
	int in = 0;

	rn_config_home(conf, sizeof(conf));
	kb_strlcpy(conf + strlen(conf), "/Zeal/Zeal.conf",
		   sizeof(conf) - strlen(conf));
	rn_data_home(out, n);
	kb_strlcpy(out + strlen(out), "/Zeal/Zeal/docsets", n - strlen(out));
	if (!(text = kb_read_whole(conf, NULL)))
		return;
	for (l = strtok_r(text, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
		if (*l == '[')
			in = !strncmp(l, "[docsets]", 9);
		else if (in && !strncmp(l, "path=", 5) && l[5] == '/')
			kb_strlcpy(out, l + 5, n);
	}
	free(text);
}

static void plist_value(const char *plist, const char *key, char *out, size_t n)
{
	char want[96];
	const char *p, *s, *e;

	snprintf(want, sizeof(want), "<key>%s</key>", key);
	if (!(p = strstr(plist, want)) || !(s = strstr(p, "<string>")))
		return;
	s += 8;
	if (!(e = strstr(s, "</string>")) || (size_t)(e - s) >= n)
		return;
	memcpy(out, s, (size_t)(e - s));
	out[e - s] = '\0';
	rn_html_text(out);
}

static void dir_sig(const char *path, char *out, size_t n)
{
	struct stat st;

	if (stat(path, &st) == 0)
		snprintf(out, n, "%s|%lx:%lld.%ld", path, (unsigned long)st.st_ino,
			 (long long)st.st_mtim.tv_sec, (long)st.st_mtim.tv_nsec);
	else
		snprintf(out, n, "%s|-", path);
}

static void docs_load(void)
{
	char dir[1024], sig[1200];
	char **names;

	docset_dir(dir, sizeof(dir));
	dir_sig(dir, sig, sizeof(sig));
	if (!strcmp(sig, g_doc_sig))
		return;
	docs_free();
	kb_strlcpy(g_doc_sig, sig, sizeof(g_doc_sig));
	names = kb_listdir(dir, NULL);
	for (char **n = names; n && *n && g_ndocs < 64; n++) {
		char path[1200], *plist;
		Docset d = { 0 };
		size_t l = strlen(*n);
		sqlite3_stmt *st;

		if (l < 8 || strcmp(*n + l - 7, ".docset"))
			continue;
		/* The directory name less `.docset`, until Info.plist names
		 * it better. */
		if (l - 7 < sizeof(d.name)) {
			memcpy(d.name, *n, l - 7);
			d.name[l - 7] = '\0';
		}
		snprintf(path, sizeof(path), "%s/%s/Contents/Info.plist", dir, *n);
		if ((plist = kb_read_whole(path, NULL))) {
			plist_value(plist, "CFBundleName", d.name, sizeof(d.name));
			plist_value(plist, "DocSetPlatformFamily", d.family,
				    sizeof(d.family));
			free(plist);
		}
		snprintf(path, sizeof(path),
			 "%s/%s/Contents/Resources/docSet.dsidx", dir, *n);
		if (!kb_path_exists(path))
			continue;
		if (sqlite3_open_v2(path, &d.db, SQLITE_OPEN_READONLY, NULL) !=
		    SQLITE_OK) {
			sqlite3_close(d.db);
			continue;
		}
		if (sqlite3_prepare_v2(d.db,
				       "SELECT 1 FROM sqlite_master WHERE "
				       "type='table' AND name='searchIndex'",
				       -1, &st, NULL) == SQLITE_OK) {
			d.dash = sqlite3_step(st) == SQLITE_ROW;
			sqlite3_finalize(st);
		}
		g_docs = kb_realloc(g_docs, (size_t)(g_ndocs + 1) * sizeof(*g_docs));
		g_docs[g_ndocs++] = d;
	}
	kb_strv_free(names);
}

void rn_zeal_match(const RnQuery *q, RnList *out)
{
	char like[600];
	size_t o = 0;
	int shown = 0;

	/* Shorter than three characters, a prefix over a docset of a hundred
	 * thousand symbols answers with noise. */
	if (strlen(q->raw) < 3)
		return;
	docs_load();
	for (const char *p = q->raw; *p && o + 3 < sizeof(like); p++) {
		if (*p == '%' || *p == '_' || *p == '\\')
			like[o++] = '\\';
		like[o++] = *p;
	}
	like[o++] = '%';
	like[o] = '\0';
	for (int i = 0; i < g_ndocs && shown < ZEAL_SHOWN; i++) {
		sqlite3_stmt *st;
		const char *sql = g_docs[i].dash
			? "SELECT name, type FROM searchIndex WHERE name LIKE ?1 "
			  "ESCAPE '\\' ORDER BY length(name) LIMIT 3"
			: "SELECT t.ZTOKENNAME, y.ZTYPENAME FROM ZTOKEN t "
			  "LEFT JOIN ZTOKENTYPE y ON t.ZTOKENTYPE = y.Z_PK "
			  "WHERE t.ZTOKENNAME LIKE ?1 ESCAPE '\\' "
			  "ORDER BY length(t.ZTOKENNAME) LIMIT 3";

		if (sqlite3_prepare_v2(g_docs[i].db, sql, -1, &st, NULL) != SQLITE_OK)
			continue;
		sqlite3_bind_text(st, 1, like, -1, SQLITE_TRANSIENT);
		while (sqlite3_step(st) == SQLITE_ROW && shown < ZEAL_SHOWN) {
			const char *name = (const char *)sqlite3_column_text(st, 0);
			const char *type = (const char *)sqlite3_column_text(st, 1);
			RnMatch *m;
			char key[400];

			if (!name || !*name || !(m = rn_add(out)))
				break;
			kb_strlcpy(m->text, name, sizeof(m->text));
			snprintf(m->subtext, sizeof(m->subtext), "%s%s%s",
				 g_docs[i].name, type ? " · " : "", type ? type : "");
			kb_strlcpy(m->icon, "zeal", sizeof(m->icon));
			kb_strlcpy(m->category, "Offline library", sizeof(m->category));
			m->catrel = RN_CAT_LOW;
			m->rel = strcasecmp(name, q->raw) ? 0.4 : 0.6;
			if (g_docs[i].family[0])
				snprintf(key, sizeof(key), "%s:%s", g_docs[i].family, name);
			else
				kb_strlcpy(key, name, sizeof(key));
			rn_arg(m, "zeal");
			rn_arg(m, key);
			shown++;
		}
		sqlite3_finalize(st);
	}
}

void rn_zeal_reset(void)
{
	g_doc_sig[0] = '\0';
}

int rn_zeal_built(void)
{
	return 1;
}

#else

void rn_zeal_match(const RnQuery *q, RnList *out)
{
	(void)q;
	(void)out;
}

void rn_zeal_reset(void)
{
}

int rn_zeal_built(void)
{
	return 0;
}

#endif
