/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-backup — what is in the repository, one key to add to it, and the
 *   way back out of it
 *
 *   ╔═ backup ════════════════════════════════════════════════════════╗
 *   ║ /var/backup/kdos — 2 snapshots, newest 1 day ago                ║
 *   ║ ─────────────────────────────────────────────────────────────── ║
 *   ║ ▶ e399cdcf  2026-09-07 03:17  nightly   /home/kdos /etc         ║
 *   ║   a818ff96  2026-09-01 03:17            /home/kdos              ║
 *   ╟─────────────────────────────────────────────────────────────────╢
 *   ║ Enter open   b back up now   r refresh   Esc Close              ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * restic DOES THE BACKUP AND THE RESTORE. This lists what is in the
 * repository, starts a backup, and — in the restore view a snapshot opens —
 * lists that snapshot's folders and restores one entry. It does not
 * deduplicate, encrypt or prune, because restic already does all three and a
 * surface that reimplemented any of them would be a second answer that
 * drifts.
 *
 * A RESTORE NEVER WRITES OVER THE ORIGINAL. It lands in
 * `~/Restored/<snapshot>/` with the entry's full path beneath it, so what is
 * restored sits beside what is there now and the person compares and moves.
 * A button that put last week's file over this week's would be the one
 * control on this surface that can lose work, and it is the one the person
 * presses under pressure.
 *
 * THE ENTRY IS AN `--include` PATTERN, ESCAPED. restic reads the argument as a
 * glob, so a file named `a*b` would otherwise restore every file it matches;
 * `*`, `?`, `[` and `\` are escaped with a backslash, which restic's matcher
 * honours.
 *
 * THE PASSWORD IS A FILE AND NEVER AN ARGUMENT. `--password-file` is restic's
 * own flag; /proc/<pid>/cmdline is world-readable, so `--password` would put
 * the key to every backup this machine has on a line any account can read.
 * There is no keyring on this desktop to put it in instead: nothing here
 * provides the Secret Service, and `pass` needs a GnuPG key the installer
 * does not make.
 *
 * AND THE FILE'S MODE IS CHECKED BEFORE IT IS USED. A password file at 0644 is
 * the same disclosure by a different route, and it is the mode a text editor or
 * a copy from another machine leaves. This refuses to run rather than reporting
 * it afterwards: a backup that succeeded is not the moment to learn the key was
 * readable.
 *
 * ONE CONFIG FILE, PARSED AND NEVER SOURCED. `~/.config/kdos/backup.conf`
 * names the repository and what goes in it, one `key = value` per line. The
 * paths become an argument vector; a shell in the middle would turn a directory
 * with a space in it into two.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define BK_COLS 76
#define BK_ROWS 22
#define BK_MAX 64
#define BK_PATHS 160

struct snap {
	char id[16];		/* short_id, which is what restic prints */
	char when[20];		/* YYYY-MM-DD HH:MM, from the ISO stamp */
	char tags[40];
	char paths[BK_PATHS];
};

static struct snap snaps[BK_MAX];
static int nsnap;
static char repo[256];
static char pwfile[256];
static char inc[8][256];
static int ninc;
static char exc[8][256];
static int nexc;
static char why[192];
static char status[160];
static KtuiTable tbl;
static KtuiKeys keys;
/* Recorded `restic snapshots --json`, so a golden does not depend on a
 * repository the machine running it does not have. */
static const char *fixture;

static void set_status(const char *fmt, const char *arg)
{
	snprintf(status, sizeof(status), fmt, arg ? arg : "");
}

/* ── the configuration ─────────────────────────────────────────────────── */

static void conf_path(char *out, size_t n, const char *leaf)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%s/kdos/%s", cfg, leaf);
	else
		snprintf(out, n, "%s/.config/kdos/%s", home ? home : "", leaf);
}

/*
 * `key = value`, one per line, `#` to end of line a comment. `include` and
 * `exclude` may repeat; everything else is last-one-wins.
 *
 * PARSED, NOT SOURCED, which is the same rule a kpkgbuild keeps: a config file
 * that is executed is a config file that can do anything the program can.
 */
static void read_conf(void)
{
	char path[256];
	char *buf;

	repo[0] = pwfile[0] = '\0';
	ninc = nexc = 0;
	conf_path(path, sizeof(path), "backup.conf");
	/* kb_read_whole and not kb_read_file: this is a file a PERSON edits,
	 * and a fixed buffer stops seeing the end of one silently — the keys
	 * past the cut fall back to their defaults and nothing says why. */
	buf = kb_read_whole(path, NULL);
	if (!buf || !buf[0]) {
		free(buf);
		snprintf(why, sizeof(why),
			 "no %s — name a repository and what goes in it", path);
		return;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp); ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char *hash = strchr(ln, '#');
		char *eq;

		if (hash)
			*hash = '\0';
		if (!(eq = strchr(ln, '=')))
			continue;
		*eq = '\0';

		char *k = ln, *v = eq + 1;

		while (*k == ' ' || *k == '\t')
			k++;
		for (char *e = k + strlen(k); e > k && (e[-1] == ' ' || e[-1] == '\t');)
			*--e = '\0';
		while (*v == ' ' || *v == '\t')
			v++;
		for (char *e = v + strlen(v); e > v && (e[-1] == ' ' || e[-1] == '\t');)
			*--e = '\0';
		if (!*k || !*v)
			continue;
		if (!strcmp(k, "repo"))
			snprintf(repo, sizeof(repo), "%s", v);
		else if (!strcmp(k, "password-file"))
			snprintf(pwfile, sizeof(pwfile), "%s", v);
		else if (!strcmp(k, "include") && ninc < 8)
			snprintf(inc[ninc++], sizeof(inc[0]), "%s", v);
		else if (!strcmp(k, "exclude") && nexc < 8)
			snprintf(exc[nexc++], sizeof(exc[0]), "%s", v);
	}
	free(buf);
	if (!pwfile[0])
		conf_path(pwfile, sizeof(pwfile), "backup.pass");
}

/*
 * THE MODE IS THE CHECK, AND IT RUNS BEFORE THE PASSWORD IS USED.
 *
 * Group or world readable is the same disclosure as putting it in argv, by a
 * slower route, and 0644 is what a text editor leaves. Refusing is the whole
 * point: reporting it after a successful backup teaches nothing, because the
 * key was already readable while the backup ran.
 */
static int password_ok(void)
{
	struct stat st;

	if (stat(pwfile, &st) != 0) {
		snprintf(why, sizeof(why),
			 "no password file at %s — `restic key` made the "
			 "repository's", pwfile);
		return 0;
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		snprintf(why, sizeof(why),
			 "%s is mode %04o — it must be 0600, `chmod 600` it",
			 pwfile, (unsigned)(st.st_mode & 07777));
		return 0;
	}
	return 1;
}

/* ── reading the repository ────────────────────────────────────────────── */

/*
 * The same deliberately dumb scan update.c uses, and for the same reason: a
 * JSON library in the panel binary to read one document restic writes would be
 * a dependency bought for nothing. It finds `"key":` and copies to the closing
 * quote — no field here is a package name, a path or a stamp that carries an
 * escaped quote.
 */
static const char *jstr(const char *p, const char *key, char *out, size_t n)
{
	char pat[48];
	const char *q;

	out[0] = '\0';
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	if (!(q = strstr(p, pat)))
		return NULL;
	q += strlen(pat);
	while (*q == ' ')
		q++;
	if (*q != '"')
		return q;
	size_t k = 0;

	for (q++; *q && *q != '"' && k + 1 < n; q++)
		out[k++] = *q;
	out[k] = '\0';
	return *q ? q + 1 : q;
}

/* A JSON array of strings, joined with a space into one column. `paths` is
 * always present and `tags` is not — a snapshot taken without one has no key
 * at all rather than an empty array. */
static void jlist(const char *p, const char *key, char *out, size_t n)
{
	char pat[48];
	const char *q;
	size_t k = 0;

	out[0] = '\0';
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	if (!(q = strstr(p, pat)))
		return;
	q += strlen(pat);
	while (*q == ' ')
		q++;
	if (*q != '[')
		return;
	for (q++; *q && *q != ']';) {
		if (*q != '"') {
			q++;
			continue;
		}
		if (k && k + 1 < n)
			out[k++] = ' ';
		for (q++; *q && *q != '"' && k + 1 < n; q++)
			out[k++] = *q;
		if (*q)
			q++;
	}
	out[k] = '\0';
}

static void scan_snaps(void)
{
	static char buf[131072];
	KbArgv a = { 0 };

	nsnap = 0;
	why[0] = '\0';
	read_conf();
	if (why[0])
		return;
	if (!repo[0]) {
		snprintf(why, sizeof(why),
			 "backup.conf names no `repo`%s", "");
		return;
	}
	if (fixture) {
		char path[512];

		snprintf(path, sizeof(path), "%s/snapshots.json", fixture);
		buf[0] = '\0';
		if (kb_read_file(path, buf, sizeof(buf)) < 0) {
			snprintf(why, sizeof(why), "no recording at %s", path);
			return;
		}
	} else {
		if (!password_ok())
			return;
		kb_argv_add(&a, "restic");
		kb_argv_add(&a, "-r");
		kb_argv_add(&a, repo);
		kb_argv_add(&a, "--password-file");
		kb_argv_add(&a, pwfile);
		kb_argv_add(&a, "snapshots");
		kb_argv_add(&a, "--json");
		kb_argv_end(&a);
		buf[0] = '\0';
		if (kb_run_capture(&a, buf, sizeof(buf)) < 0 || !buf[0]) {
			snprintf(why, sizeof(why),
				 "restic answered nothing — is %s a "
				 "repository?", repo);
			return;
		}
	}
	if (buf[0] != '[') {
		/* restic's own message, which names the case: no repository,
		 * a wrong password, a lock somebody else holds. */
		buf[strcspn(buf, "\n")] = '\0';
		snprintf(why, sizeof(why), "%.180s", buf);
		return;
	}
	/*
	 * ONE RECORD PER `"time":`, AND THE SPAN ENDS AT THE NEXT ONE.
	 *
	 * A snapshot object contains a nested `summary` object, so a scan for
	 * `{` finds every record TWICE — once at its own brace and once at
	 * summary's — and the second pass reads the tail of one record and the
	 * head of the next. Bounding each record explicitly is what makes a
	 * forward search for a key mean "this snapshot's".
	 */
	for (char *p = buf; nsnap < BK_MAX;) {
		struct snap *s = &snaps[nsnap];
		char iso[48];
		char *rec = strstr(p, "\"time\":");
		char *nxt;
		char save = 0;

		if (!rec)
			break;
		nxt = strstr(rec + 7, "\"time\":");
		if (nxt) {
			save = *nxt;
			*nxt = '\0';
		}

		memset(s, 0, sizeof(*s));
		jstr(rec, "short_id", s->id, sizeof(s->id));
		jstr(rec, "time", iso, sizeof(iso));
		/* `2026-09-07T03:17:09.000+00:00` → `2026-09-07 03:17`. The
		 * stamp is fixed-width up to the minute, so this is a copy
		 * rather than a parse — and a shorter one is left as it is
		 * rather than read past. */
		if (strlen(iso) >= 16) {
			memcpy(s->when, iso, 16);
			s->when[10] = ' ';
			s->when[16] = '\0';
		} else {
			snprintf(s->when, sizeof(s->when), "%s", iso);
		}
		jlist(rec, "tags", s->tags, sizeof(s->tags));
		jlist(rec, "paths", s->paths, sizeof(s->paths));
		if (s->id[0])
			nsnap++;
		if (!nxt)
			break;
		*nxt = save;
		p = nxt;
	}
	/*
	 * NEWEST FIRST. restic lists oldest first, and the snapshot a person
	 * is looking for is almost always the last one taken.
	 */
	for (int i = 0, j = nsnap - 1; i < j; i++, j--) {
		struct snap t = snaps[i];

		snaps[i] = snaps[j];
		snaps[j] = t;
	}
	ktui_table_clamp(&tbl, nsnap, ktui_h > 4 ? ktui_h - 4 : 1);
}

/*
 * Start a backup and do not wait for it. A first run over a home directory is
 * minutes to hours, and a surface that blocked on it would be a frozen window
 * with no way to tell a slow backup from a hung one. The result is visible the
 * way everything else here is: a snapshot appears in the list.
 */
static void backup_now(void)
{
	KbArgv a = { 0 };

	if (!repo[0] || !password_ok()) {
		set_status("%s", why[0] ? why : "not configured");
		return;
	}
	if (!ninc) {
		set_status("backup.conf names no `include` path%s", "");
		return;
	}
	kb_argv_add(&a, "restic");
	kb_argv_add(&a, "-r");
	kb_argv_add(&a, repo);
	kb_argv_add(&a, "--password-file");
	kb_argv_add(&a, pwfile);
	kb_argv_add(&a, "backup");
	for (int i = 0; i < ninc; i++)
		kb_argv_add(&a, inc[i]);
	for (int i = 0; i < nexc; i++) {
		kb_argv_add(&a, "--exclude");
		kb_argv_add(&a, exc[i]);
	}
	kb_argv_end(&a);
	kb_run_detach(&a);
	set_status("backing up — the snapshot appears when it finishes%s", "");
}

/* ── the restore view ──────────────────────────────────────────────────── */

#define BK_ENT 512

struct ent {
	char name[128];
	char path[512];
	long long size;		/* -1 for a folder                         */
	char when[17];		/* YYYY-MM-DD HH:MM                         */
};

static struct ent ents[BK_ENT];
static int nent;
static int browsing;		/* the restore view is up                  */
static int bsnap;		/* which snapshot it shows                 */
static char cwd[512];		/* the folder inside the snapshot          */
static KtuiTable etbl;
static ShJob job;
static char job_target[512];

/*
 * One string value out of one JSON record, UNESCAPED. A file name is the one
 * field here that may carry any character at all — a quote, a backslash, a
 * non-ASCII letter restic writes as `\\u00e9` — and the path it yields is
 * handed back to restic, so a name read short would restore something else.
 * `\\uXXXX` becomes UTF-8; a surrogate pair becomes one code point.
 */
static int jget(const char *rec, const char *key, char *out, size_t n)
{
	char pat[48];
	const char *q;
	size_t k = 0;

	out[0] = '\0';
	snprintf(pat, sizeof(pat), "\"%s\":\"", key);
	if (!(q = strstr(rec, pat)))
		return 0;
	for (q += strlen(pat); *q && *q != '"' && k + 5 < n; q++) {
		unsigned cp;

		if (*q != '\\') {
			out[k++] = *q;
			continue;
		}
		q++;
		switch (*q) {
		case 'n': out[k++] = '\n'; break;
		case 't': out[k++] = '\t'; break;
		case 'r': out[k++] = '\r'; break;
		case 'b': out[k++] = '\b'; break;
		case 'f': out[k++] = '\f'; break;
		case 'u':
			if (sscanf(q + 1, "%4x", &cp) != 1)
				return 0;
			q += 4;
			if (cp >= 0xd800 && cp < 0xdc00 && q[1] == '\\' &&
			    q[2] == 'u') {
				unsigned lo;

				if (sscanf(q + 3, "%4x", &lo) == 1 &&
				    lo >= 0xdc00 && lo < 0xe000) {
					cp = 0x10000 + ((cp - 0xd800) << 10) +
					     (lo - 0xdc00);
					q += 6;
				}
			}
			if (cp < 0x80) {
				out[k++] = (char)cp;
			} else if (cp < 0x800) {
				out[k++] = (char)(0xc0 | (cp >> 6));
				out[k++] = (char)(0x80 | (cp & 0x3f));
			} else if (cp < 0x10000) {
				out[k++] = (char)(0xe0 | (cp >> 12));
				out[k++] = (char)(0x80 | ((cp >> 6) & 0x3f));
				out[k++] = (char)(0x80 | (cp & 0x3f));
			} else {
				out[k++] = (char)(0xf0 | (cp >> 18));
				out[k++] = (char)(0x80 | ((cp >> 12) & 0x3f));
				out[k++] = (char)(0x80 | ((cp >> 6) & 0x3f));
				out[k++] = (char)(0x80 | (cp & 0x3f));
			}
			break;
		case '\0':
			return 0;
		default:
			out[k++] = *q;	/* \" \\ \/ */
			break;
		}
	}
	out[k] = '\0';
	/* A value cut short is refused rather than used: it names a path
	 * that is not in the snapshot. */
	return *q == '"';
}

/* The folder a path is in: `/home/kdos` for `/home/kdos/notes.txt`, `/` for
 * `/home`. */
static void parent_of(const char *path, char *out, size_t n)
{
	const char *sl = strrchr(path, '/');

	if (!sl || sl == path)
		snprintf(out, n, "/");
	else
		snprintf(out, n, "%.*s", (int)(sl - path), path);
}

static int ent_cmp(const void *a, const void *b)
{
	const struct ent *x = a, *y = b;

	/* Folders first, then by name — the order a file manager keeps, so
	 * the eye finds a folder where it expects one. */
	if ((x->size < 0) != (y->size < 0))
		return x->size < 0 ? -1 : 1;
	return strcmp(x->name, y->name);
}

/*
 * THE CHILDREN OF `cwd`, AND NOTHING ELSE. `restic ls <id> <dir>` answers
 * the folder itself and its children, and the recorded fixture is the whole
 * tree, so one filter — a record's parent is `cwd` — serves both and a
 * recording cannot pass where the live listing would not.
 */
static void scan_dir(void)
{
	KbBuf out = { 0 };
	char *buf = NULL;
	const struct snap *s = &snaps[bsnap];

	nent = 0;
	status[0] = '\0';
	if (fixture) {
		char path[512];

		snprintf(path, sizeof(path), "%s/ls-%s.json", fixture, s->id);
		buf = kb_read_whole(path, NULL);
		if (!buf) {
			snprintf(status, sizeof(status), "no recording at %.120s",
				 path);
			return;
		}
	} else {
		KbArgv a = { 0 };

		if (!password_ok()) {
			snprintf(status, sizeof(status), "%s", why);
			return;
		}
		kb_argv_add(&a, "restic");
		kb_argv_add(&a, "-r");
		kb_argv_add(&a, repo);
		kb_argv_add(&a, "--password-file");
		kb_argv_add(&a, pwfile);
		kb_argv_add(&a, "ls");
		kb_argv_add(&a, "--json");
		kb_argv_add(&a, s->id);
		kb_argv_add(&a, cwd);
		kb_argv_end(&a);
		if (kb_run_capture_buf(&a, &out) < 0 || !out.p) {
			snprintf(status, sizeof(status), "restic listed nothing "
							 "for %.100s", cwd);
			kb_buf_free(&out);
			return;
		}
		buf = out.p;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp);
	     ln && nent < BK_ENT; ln = strtok_r(NULL, "\n", &sp)) {
		struct ent *e = &ents[nent];
		char type[16], dir[512], iso[48];
		const char *sz;

		if (!strstr(ln, "\"struct_type\":\"node\"") ||
		    !jget(ln, "path", e->path, sizeof(e->path)) ||
		    !jget(ln, "name", e->name, sizeof(e->name)))
			continue;
		parent_of(e->path, dir, sizeof(dir));
		if (strcmp(dir, cwd) || !strcmp(e->path, cwd))
			continue;
		jget(ln, "type", type, sizeof(type));
		e->size = -1;
		if (strcmp(type, "dir")) {
			sz = strstr(ln, "\"size\":");
			e->size = sz ? atoll(sz + 7) : 0;
		}
		jget(ln, "mtime", iso, sizeof(iso));
		snprintf(e->when, sizeof(e->when), "%.16s", iso);
		if (strlen(e->when) == 16)
			e->when[10] = ' ';
		nent++;
	}
	free(buf);
	qsort(ents, (size_t)nent, sizeof(ents[0]), ent_cmp);
	etbl.sel = 0;
	etbl.top = 0;
}

static void browse_open(int which)
{
	if (which < 0 || which >= nsnap)
		return;
	bsnap = which;
	browsing = 1;
	/* A snapshot of one folder opens IN it — that is where every file
	 * is. One of several opens at the root, where all of them are. */
	if (!strchr(snaps[which].paths, ' ') && snaps[which].paths[0] == '/')
		snprintf(cwd, sizeof(cwd), "%s", snaps[which].paths);
	else
		snprintf(cwd, sizeof(cwd), "/");
	scan_dir();
}

static int browse_up(void *user)
{
	(void)user;
	return browsing;
}

static void browse_close(void *user)
{
	(void)user;
	browsing = 0;
	status[0] = '\0';
}

static void browse_parent(void)
{
	char up[512];

	if (!strcmp(cwd, "/"))
		return;
	parent_of(cwd, up, sizeof(up));
	snprintf(cwd, sizeof(cwd), "%s", up);
	scan_dir();
}

static void browse_enter(void)
{
	const struct ent *e = etbl.sel >= 0 && etbl.sel < nent
				      ? &ents[etbl.sel] : NULL;

	if (e && e->size < 0) {
		snprintf(cwd, sizeof(cwd), "%s", e->path);
		scan_dir();
	}
}

/* `*`, `?`, `[` and `\` are glob syntax to restic's `--include`; each gets
 * the backslash its matcher reads as "this character, literally". */
static void glob_escape(const char *in, char *out, size_t n)
{
	size_t k = 0;

	for (; *in && k + 2 < n; in++) {
		if (strchr("*?[\\", *in))
			out[k++] = '\\';
		out[k++] = *in;
	}
	out[k] = '\0';
}

/*
 * RESTORE THE SELECTED ENTRY — a file, or a folder and everything in it —
 * into `~/Restored/<snapshot>/`. restic recreates the entry's full path under
 * that folder, so two restores of different entries from one snapshot land
 * side by side and neither overwrites anything that was there before.
 *
 * It runs as a job the window keeps drawing past: a folder of photographs is
 * minutes, and restic's own last line is the progress.
 */
static void restore_selected(void)
{
	static char pat[1100];
	const struct ent *e = etbl.sel >= 0 && etbl.sel < nent
				      ? &ents[etbl.sel] : NULL;
	const char *home = getenv("HOME");
	const char *av[16];
	int n = 0;

	if (!e || job.running)
		return;
	if (fixture) {
		snprintf(status, sizeof(status), "a recording restores nothing");
		return;
	}
	if (!password_ok()) {
		snprintf(status, sizeof(status), "%s", why);
		return;
	}
	snprintf(job_target, sizeof(job_target), "%s/Restored/%s",
		 home ? home : "", snaps[bsnap].id);
	glob_escape(e->path, pat, sizeof(pat));
	av[n++] = "restic";
	av[n++] = "-r";
	av[n++] = repo;
	av[n++] = "--password-file";
	av[n++] = pwfile;
	av[n++] = "restore";
	av[n++] = snaps[bsnap].id;
	av[n++] = "--target";
	av[n++] = job_target;
	av[n++] = "--include";
	av[n++] = pat;
	av[n] = NULL;
	job.line = NULL;
	if (sh_job_start(&job, av, NULL) != 0) {
		snprintf(status, sizeof(status), "%s", job.last[0] ? job.last
					: "restic will not start");
		return;
	}
	snprintf(status, sizeof(status), "restoring %.60s into %.80s", e->name,
		 job_target);
}

/* What a finished restore says: where the files are, or restic's reason. */
static void job_finished(void)
{
	if (job.status == 0)
		snprintf(status, sizeof(status), "restored into %.120s",
			 job_target);
	else
		snprintf(status, sizeof(status), "restic stopped: %.120s",
			 job.last[0] ? job.last : "no reason given");
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol BK_COL[] = { { NULL, 0, 0 } };

static void bk_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct snap *s = &snaps[idx];
	int list_w = *(const int *)user;
	int on = bg == KT_ACCENT;

	(void)col;
	(void)x;
	(void)w;
	ktui_draw_text(3, y, 10, s->id, fg, bg, KT_A_NONE);
	ktui_draw_text(14, y, 17, s->when, on ? KT_SURFACE : KT_MID, bg,
		       KT_A_NONE);
	ktui_draw_text(32, y, 10, s->tags, on ? KT_SURFACE : KT_ACCENT, bg,
		       KT_A_NONE);
	ktui_draw_text(43, y, list_w - 45, s->paths, fg, bg, KT_A_NONE);
}

static const KtuiCol EN_COL[] = { { "NAME", 0, 0 }, { "SIZE", 9, 0 },
				  { "MODIFIED", 17, 0 } };
#define EN_NCOL 3

static void en_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct ent *e = &ents[idx];
	int on = bg == KT_ACCENT;
	char sz[16];

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_textf(x, y, w, e->size < 0 && !on ? KT_ACCENT : fg,
				bg, KT_A_NONE, "%s%s", e->name,
				e->size < 0 ? "/" : "");
		break;
	case 1:
		if (e->size < 0)
			snprintf(sz, sizeof(sz), "-");
		else
			snprintf(sz, sizeof(sz), "%s",
				 kb_human_size((unsigned long long)e->size));
		ktui_draw_text_right(x, y, w - 1, sz, on ? KT_SURFACE : KT_MID,
				     bg, KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, e->when, on ? KT_SURFACE : KT_MID, bg,
			       KT_A_NONE);
		break;
	}
}

/* The snapshot list draws its own columns from the frame's edge; the restore
 * view is a table with a header, inset like every other one. */
static KRect list_rect(void)
{
	return browsing ? krect(2, 3, ktui_w - 4, ktui_h - 6)
			: krect(1, 3, ktui_w - 2, ktui_h - 6);
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	int list_w = w;
	char sub[192];

	if (w < 30 || h < 8)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "backup", KT_ACCENT, KT_BG, 1);

	if (browsing)
		snprintf(sub, sizeof(sub), "%s  %s — %d entr%s", snaps[bsnap].id,
			 cwd, nent, nent == 1 ? "y" : "ies");
	else if (why[0])
		snprintf(sub, sizeof(sub), "%s", why);
	else if (nsnap)
		snprintf(sub, sizeof(sub), "%s — %d snapshot%s, newest %s",
			 repo, nsnap, nsnap == 1 ? "" : "s", snaps[0].when);
	else
		snprintf(sub, sizeof(sub), "%s — no snapshots yet", repo);
	ktui_draw_text(2, 1, w - 4, sub, !browsing && why[0] ? KT_ERR : KT_MID,
		       KT_BG, KT_A_NONE);
	ktui_draw_hline(1, 2, w - 2, KT_G_HL, KT_DIM, KT_BG);

	if (browsing)
		ktui_table_draw(list_rect(), &etbl, nent, EN_COL, EN_NCOL,
				en_cell, NULL, NULL, -1);
	else
		ktui_table_draw(list_rect(), &tbl, nsnap, BK_COL, 1, bk_cell,
				NULL, &list_w, -1);

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	if (status[0]) {
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_BG);
		ktui_draw_text(2, h - 2, w - 4, status, KT_MID, KT_BG,
			       KT_A_NONE);
	} else if (browsing) {
		const struct ent *e = etbl.sel >= 0 && etbl.sel < nent
					      ? &ents[etbl.sel] : NULL;

		ktui_hint_if(e && e->size < 0, "Enter", "open");
		ktui_hint_if(e != NULL, "t", "restore");
		ktui_hint_if(strcmp(cwd, "/") != 0, "Backspace", "up");
		ktui_hint("Esc", ktui_esc_verb(&keys));
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_BG);
	} else {
		ktui_hint_if(nsnap > 0, "Enter", "open");
		ktui_hint("b", "back up now");
		ktui_hint("r", "refresh");
		ktui_hint("Esc", ktui_esc_verb(&keys));
		ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_BG);
	}
	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

static void on_key(int k)
{
	int rows = ktui_h > 6 ? ktui_h - 6 : 1;

	if (browsing) {
		switch (k) {
		case KT_K_ENTER:
		case KT_K_RIGHT:
			browse_enter();
			break;
		case KT_K_BACKSPACE:
		case KT_K_LEFT:
			browse_parent();
			break;
		case 't':
			restore_selected();
			break;
		case 'r':
			scan_dir();
			break;
		default:
			ktui_table_key(&etbl, nent, rows, k, NULL, NULL);
			break;
		}
		return;
	}
	switch (k) {
	case KT_K_ENTER:
		browse_open(tbl.sel);
		break;
	case 'b':
		backup_now();
		break;
	case 'r':
		scan_snaps();
		break;
	default:
		ktui_table_key(&tbl, nsnap, rows, k, NULL, NULL);
		break;
	}
}

int backup_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *open_id = NULL, *open_dir = NULL;
	int dump = 0, once = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dump"))
			dump = 1;
		/* What a timer line runs: back up and exit, no window. The
		 * scheduled half of this is J.13's table and not a scheduler
		 * of ours — one `snooze` per job under the supervisor. */
		else if (!strcmp(argv[i], "--once"))
			once = 1;
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
			fixture = argv[++i];
		/* The restore view on a snapshot, and a folder inside it —
		 * what a golden of that view is drawn from. */
		else if (!strcmp(argv[i], "--open") && i + 1 < argc)
			open_id = argv[++i];
		else if (!strcmp(argv[i], "--dir") && i + 1 < argc)
			open_dir = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-backup [--font NAME] "
					"[--fixture DIR] [--open SNAPSHOT "
					"[--dir PATH]] [--once] [--dump]\n");
			return 2;
		}
	}

	if (once) {
		KbArgv a = { 0 };

		read_conf();
		if (why[0] || !repo[0] || !password_ok()) {
			fprintf(stderr, "kdos-backup: %s\n",
				why[0] ? why : "backup.conf names no repo");
			return 1;
		}
		if (!ninc) {
			fprintf(stderr, "kdos-backup: backup.conf names no "
					"include path\n");
			return 1;
		}
		/*
		 * WAITED FOR, unlike the window's `b`. A timer's job is the
		 * process the supervisor is watching: detaching here would
		 * report success the instant restic started, and a backup that
		 * failed every night would look like one that ran.
		 */
		kb_argv_add(&a, "restic");
		kb_argv_add(&a, "-r");
		kb_argv_add(&a, repo);
		kb_argv_add(&a, "--password-file");
		kb_argv_add(&a, pwfile);
		kb_argv_add(&a, "backup");
		for (int i = 0; i < ninc; i++)
			kb_argv_add(&a, inc[i]);
		for (int i = 0; i < nexc; i++) {
			kb_argv_add(&a, "--exclude");
			kb_argv_add(&a, exc[i]);
		}
		kb_argv_end(&a);
		return kb_run(&a) == 0 ? 0 : 1;
	}

	keys.doc = "backup";
	keys.help = sh_help;
	ktui_keys_layer(&keys, "Close", NULL, NULL, NULL);
	ktui_keys_layer(&keys, "Back", browse_up, browse_close, NULL);
	scan_snaps();
	if (open_id) {
		for (int i = 0; i < nsnap; i++)
			if (!strcmp(snaps[i].id, open_id))
				browse_open(i);
		if (browsing && open_dir) {
			snprintf(cwd, sizeof(cwd), "%s", open_dir);
			scan_dir();
		}
	}

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(BK_COLS, BK_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = BK_COLS,
		.rows = BK_ROWS,
		.title = "backup",
		.app_id = "kdos-backup",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-backup: no compositor\n");
		return 1;
	}
	ktui_draw_init();
	kch_px_popup(KT_BG);
	sh_theme_watch();

	while (!kdisp_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}
		if (job.running && sh_job_pump(&job)) {
			if (job.running)
				snprintf(status, sizeof(status), "%.150s",
					 job.last);
			else
				job_finished();
		}
		draw_frame();

		KtuiEvent ev;

		/* A quarter-second tick while restic is running, so its
		 * progress moves; a second otherwise. */
		if (!ktui_backend()->poll_event(&ev, job.running ? 250 : 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (ev.type == KT_EVT_MOUSE) {
			int rows = ktui_h > 6 ? ktui_h - 6 : 1;
			int r;

			/*
			 * ONE POINTER RULE: a press moves the caret, a press
			 * on the row it is already on opens it, the wheel
			 * walks and the right button is Back — which in the
			 * restore view is the folder above, and at a
			 * snapshot's top the list.
			 */
			if (browsing) {
				r = ktui_table_event(list_rect(), &etbl, nent,
						     rows, EN_NCOL, EN_COL, &ev,
						     NULL, NULL);
				if (r == KTUI_TABLE_PICKED)
					browse_enter();
				else if (r == KTUI_TABLE_CLOSE) {
					if (strcmp(cwd, "/") && strcmp(cwd,
					    snaps[bsnap].paths))
						browse_parent();
					else
						browse_close(NULL);
				}
				continue;
			}
			r = ktui_table_event(list_rect(), &tbl, nsnap, rows, 1,
					     BK_COL, &ev, NULL, NULL);
			if (r == KTUI_TABLE_PICKED)
				browse_open(tbl.sel);
			else if (r == KTUI_TABLE_CLOSE)
				break;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		if (!job.running)
			status[0] = '\0';
		on_key(ev.key);
	}
	/* A restore in flight is left to finish: the window closing is not
	 * the person changing their mind about the files. */
	kdisp_shutdown();
	return 0;
}
