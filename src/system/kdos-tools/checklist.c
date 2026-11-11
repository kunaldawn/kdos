/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos checklist — the first-run checklist, which ticks itself
 *
 *   kdos checklist --json        {"version":1,"items":[{id,done,dismissed}…]}
 *   kdos checklist dismiss <id>  hide one item for good
 *   kdos checklist restore       bring every hidden one back
 *
 * EACH ITEM IS DONE WHEN THE MACHINE SAYS SO, never when a box was ticked:
 *
 *   backup      ~/.config/kdos/backup.conf names a repository and something
 *               to put in it, or Kup is on with a restic plan
 *   phone       KDE Connect has a paired device
 *   language    Formats were set, or a second keyboard layout added (the
 *               image is English-only, A1, so this is the regional half).
 *               A [Formats] LANG equal to /etc/locale.conf's (a C
 *               locale when that names none) is not a choice: Plasma writes the session's LANG there at every
 *               account's first login. Any other [Formats] key, or a LANG
 *               that differs, is.
 *   box         a box exists
 *   firmware    fwupd has no update waiting
 *   encryption  the root filesystem sits on LUKS
 *
 * AN ITEM WHOSE COMPONENT IS ABSENT IS LEFT OUT, not shown undone: a phone
 * item on a machine with no KDE Connect is a task nobody can finish. So is
 * encryption on a root that is not a block device, such as the live
 * medium's overlay.
 *
 * KDOS_CHECKLIST_ROOT, for tests, names a recorded root read in place of
 * / for etc/locale.conf and proc/self/mounts.
 *
 * FAST FROM CACHE. The tray asks every five seconds while anything is open,
 * so the two slow questions are remembered: fwupd's for six hours, the root
 * device's for the boot.
 *
 * Exit 0; 1 a failure, with one line on stderr a person can read; 2 usage.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

static const char *const IDS[] = { "backup", "phone", "language", "box",
				   "firmware", "encryption" };
#define NIDS ((int)(sizeof(IDS) / sizeof(IDS[0])))

#define FW_TTL (6 * 3600)

/* A system file, under KDOS_CHECKLIST_ROOT when that is set. */
static char *sys_path(const char *abs)
{
	const char *r = getenv("KDOS_CHECKLIST_ROOT");
	char *p;

	if (!r || !*r)
		return strdup(abs);
	p = malloc(strlen(r) + strlen(abs) + 1);
	if (p) {
		strcpy(p, r);
		strcat(p, abs);
	}
	return p;
}

/* ── backup ────────────────────────────────────────────────────────────── */

static int backup_conf_done(void)
{
	char *p = kdt_cfg_home("kdos/backup.conf"), *buf;
	int repo = 0, inc = 0;

	buf = kb_read_all(p, NULL);
	free(p);
	for (char *sp = NULL, *ln = buf ? strtok_r(buf, "\n", &sp) : NULL; ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char *h = strchr(ln, '#'), *k = ln, *eq;

		if (h)
			*h = '\0';
		if (!(eq = strchr(ln, '=')))
			continue;
		while (*k == ' ' || *k == '\t')
			k++;
		if (!strncmp(k, "repo", 4) && (k[4] == ' ' || k[4] == '=' ||
					       k[4] == '\t'))
			repo = eq[strspn(eq + 1, " \t") + 1] != '\0';
		if (!strncmp(k, "include", 7) && (k[7] == ' ' || k[7] == '=' ||
						  k[7] == '\t'))
			inc = inc || eq[strspn(eq + 1, " \t") + 1] != '\0';
	}
	free(buf);
	return repo && inc;
}

/* A Kup restic plan: [Plan/N] with Backup type=2, N from 1 to the plan
 * count, while Kup's backups are switched on, and with somewhere to write:
 * a folder URL for a filesystem destination (Destination type=0), a volume
 * for a removable drive (type 1, which an absent key also means). */
static int kup_done(void)
{
	char *path = kdt_cfg_home("kuprc"), *on, *count;
	int n, done = 0;

	on = kdt_kcfg_get(path, "Kup settings", "Backups enabled");
	count = kdt_kcfg_get(path, "Kup settings", "Number of backups");
	n = count ? atoi(count) : 0;
	if (kdt_kcfg_bool(on, 0))
		for (int i = 1; i <= n && i < 100 && !done; i++) {
			char grp[32], *type, *dt, *dest;

			snprintf(grp, sizeof(grp), "Plan/%d", i);
			type = kdt_kcfg_get(path, grp, "Backup type");
			dt = kdt_kcfg_get(path, grp, "Destination type");
			dest = kdt_kcfg_get(path, grp,
					    dt && !strcmp(dt, "0")
						    ? "Filesystem destination path"
						    : "External drive UUID");
			done = type && !strcmp(type, "2") && dest && *dest;
			free(type);
			free(dt);
			free(dest);
		}
	free(on);
	free(count);
	free(path);
	return done;
}

/* ── phone ─────────────────────────────────────────────────────────────── */

static int phone_done(void)
{
	KdtBus *b = kdt_bus_open(0);
	KdtMsg *r = NULL;
	const char *s;
	int n = 0;

	if (b && kdt_bus_call(b, "org.kde.kdeconnect", "/modules/kdeconnect",
			      "org.kde.kdeconnect.daemon", "devices", &r, NULL,
			      0, "bb", 0, 1) == 0 &&
	    kdt_bus_enter(b, r, 'a', "s") > 0) {
		while (kdt_bus_read(b, r, "s", &s) > 0)
			n++;
		kdt_bus_leave(b, r);
	}
	kdt_bus_free(b, r);
	kdt_bus_close(b);
	return n > 0;
}

/* ── language ──────────────────────────────────────────────────────────── */

/* The system's LANG, from /etc/locale.conf, into out; "" when it names
 * none. A value may be quoted, as locale.conf allows. */
static void system_lang(char *out, size_t n)
{
	char *p = sys_path("/etc/locale.conf");
	char *data = p ? kb_read_all(p, NULL) : NULL;

	free(p);
	out[0] = '\0';
	for (char *sp = NULL, *ln = data ? strtok_r(data, "\n", &sp) : NULL;
	     ln; ln = strtok_r(NULL, "\n", &sp)) {
		size_t l;

		ln += strspn(ln, " \t");
		if (strncmp(ln, "LANG=", 5))
			continue;
		ln += 5;
		l = strcspn(ln, " \t\r#");
		if (l >= 2 && (ln[0] == '"' || ln[0] == '\'') &&
		    ln[l - 1] == ln[0]) {
			ln++;
			l -= 2;
		}
		snprintf(out, n, "%.*s", (int)l, ln);
	}
	free(data);
}

/* With no locale.conf the session's LANG is the C library's default, one
 * of these. */
static int c_locale(const char *v, size_t l)
{
	static const char *const C[] = { "C", "C.UTF-8", "C.utf8", "POSIX" };

	for (size_t i = 0; i < sizeof(C) / sizeof(C[0]); i++)
		if (l == strlen(C[i]) && !strncmp(v, C[i], l))
			return 1;
	return 0;
}

static int language_done(void)
{
	char *p = kdt_cfg_home("plasma-localerc"), *data, *layouts;
	char sys[128];
	int done = 0, in = 0;

	system_lang(sys, sizeof(sys));
	data = kb_read_all(p, NULL);
	free(p);
	for (char *sp = NULL, *ln = data ? strtok_r(data, "\n", &sp) : NULL;
	     ln && !done; ln = strtok_r(NULL, "\n", &sp)) {
		char *eq;

		if (ln[0] == '[') {
			in = !strncmp(ln, "[Formats]", 9);
			continue;
		}
		if (!in || !(eq = strchr(ln, '=')))
			continue;
		if (!strncmp(ln, "LANG", 4) &&
		    ln + 4 + strspn(ln + 4, " \t") == eq) {
			char *v = eq + 1 + strspn(eq + 1, " \t");
			size_t l = strcspn(v, " \t\r");

			done = !(l == strlen(sys) && !strncmp(v, sys, l)) &&
			       !(!*sys && c_locale(v, l));
		} else {
			done = 1;
		}
	}
	free(data);
	layouts = kdt_kcfg_user("kxkbrc", "Layout", "LayoutList");
	if (layouts && strchr(layouts, ','))
		done = 1;
	free(layouts);
	return done;
}

/* ── box ───────────────────────────────────────────────────────────────── */

static int box_done(void)
{
	char *d = kdt_cfg_home("kdos/boxes");
	int n = 0, any = 0;
	char **v = kb_listdir(d, &n);

	for (int i = 0; i < n && !any; i++) {
		size_t l = strlen(v[i]);

		any = l > 5 && !strcmp(v[i] + l - 5, ".conf");
	}
	kb_strv_free(v);
	free(d);
	return any;
}

/* ── the cached two ────────────────────────────────────────────────────── */

static char *cache_path(const char *leaf)
{
	char rel[64];

	snprintf(rel, sizeof(rel), "kdos/checklist-%s", leaf);
	return kdt_cache_home(rel);
}

/* fwupdmgr's own exit status: 2 is "no updates available". */
static int firmware_done(void)
{
	char *p = cache_path("firmware"), line[64] = "";
	long when = 0;
	int val = 0, done;
	KbArgv a = { 0 };

	if (kb_read_line_file(p, line, sizeof(line)) > 0 &&
	    sscanf(line, "%ld %d", &when, &val) == 2 &&
	    (long)time(NULL) - when < FW_TTL && (long)time(NULL) >= when) {
		free(p);
		return val;
	}
	kb_argv_add(&a, "fwupdmgr");
	kb_argv_add(&a, "get-updates");
	kb_argv_add(&a, "--no-unreported-check");
	kb_argv_add(&a, "--no-metadata-check");
	kb_argv_end(&a);
	{
		char out[256];

		done = kb_run_capture(&a, out, sizeof(out)) == 2;
	}
	snprintf(line, sizeof(line), "%ld %d\n", (long)time(NULL), done);
	kdt_mkparent(p);
	kb_write_file_atomic(p, line);
	free(p);
	return done;
}

/* The root's source device, from the mount table; 0 when the root is
 * not on a block device. */
static int root_device(char *out, size_t n)
{
	char *mp = sys_path("/proc/self/mounts");
	char *m = mp ? kb_read_all(mp, NULL) : NULL;
	int found = 0;

	free(mp);
	for (char *sp = NULL, *ln = m ? strtok_r(m, "\n", &sp) : NULL; ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		char dev[256], mnt[256];

		if (sscanf(ln, "%255s %255s", dev, mnt) == 2 &&
		    !strcmp(mnt, "/")) {
			found = dev[0] == '/';
			if (found)
				snprintf(out, n, "%s", dev);
		}
	}
	free(m);
	return found;
}

/* `lsblk -s` lists a device and every device under it; a crypto_LUKS among
 * them is the root sitting on an encrypted volume. Remembered with the boot's
 * id, which changes at every boot and at nothing else. */
static int encryption_done(void)
{
	char *p = cache_path("encryption"), line[128] = "", boot[64] = "";
	char dev[256], out[4096] = "";
	int done = 0, val;
	KbArgv a = { 0 };

	kb_read_line_file("/proc/sys/kernel/random/boot_id", boot,
			  sizeof(boot));
	if (kb_read_line_file(p, line, sizeof(line)) > 0) {
		char id[64];

		if (sscanf(line, "%63s %d", id, &val) == 2 && *boot &&
		    !strcmp(id, boot)) {
			free(p);
			return val;
		}
	}
	if (root_device(dev, sizeof(dev))) {
		kb_argv_add(&a, "lsblk");
		kb_argv_add(&a, "-s");
		kb_argv_add(&a, "-n");
		kb_argv_add(&a, "-o");
		kb_argv_add(&a, "FSTYPE");
		kb_argv_add(&a, dev);
		kb_argv_end(&a);
		if (kb_run_capture(&a, out, sizeof(out)) == 0)
			done = strstr(out, "crypto_LUKS") != NULL;
	}
	if (*boot) {
		snprintf(line, sizeof(line), "%s %d\n", boot, done);
		kdt_mkparent(p);
		kb_write_file_atomic(p, line);
	}
	free(p);
	return done;
}

/* ── dismissals ────────────────────────────────────────────────────────── */

static char *dismiss_path(void)
{
	return kdt_cfg_home("kdos/checklist");
}

static int dismissed(const char *id)
{
	char *p = dismiss_path(), *v = kdt_kcfg_get(p, "Dismissed", id);
	int on = kdt_kcfg_bool(v, 0);

	free(v);
	free(p);
	return on;
}

static int known(const char *id)
{
	for (int i = 0; i < NIDS; i++)
		if (!strcmp(IDS[i], id))
			return 1;
	return 0;
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

static int present(int i)
{
	switch (i) {
	case 0: return kb_have_prog("restic") ||
		       kb_path_exists("/usr/bin/kup-daemon");
	case 1: return kb_have_prog("kdeconnect-cli") ||
		       kb_path_exists("/usr/lib/libexec/kdeconnectd");
	case 2: return 1;
	case 3: return kb_have_prog("kdos-box");
	case 4: return kb_have_prog("fwupdmgr");
	default: {
		char dev[256];

		return kb_have_prog("lsblk") && root_device(dev, sizeof(dev));
	}
	}
}

static int done_of(int i)
{
	switch (i) {
	case 0: return backup_conf_done() || kup_done();
	case 1: return phone_done();
	case 2: return language_done();
	case 3: return box_done();
	case 4: return firmware_done();
	default: return encryption_done();
	}
}

static int list_json(void)
{
	KbBuf b = { 0 };
	int first = 1;

	kb_buf_str(&b, "{\"version\":1,\"items\":[");
	for (int i = 0; i < NIDS; i++) {
		if (!present(i))
			continue;
		kb_buf_printf(&b, "%s{\"id\":\"%s\",\"done\":%s,"
				  "\"dismissed\":%s}",
			      first ? "" : ",", IDS[i],
			      done_of(i) ? "true" : "false",
			      dismissed(IDS[i]) ? "true" : "false");
		first = 0;
	}
	kb_buf_str(&b, "]}\n");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	return 0;
}

static int restore(void)
{
	char *p = dismiss_path(), *data = kb_read_all(p, NULL);
	KbBuf out = { 0 };
	int skip = 0, rc = 0;

	if (!data) {
		free(p);
		return 0;
	}
	for (char *l = data, *next; l && *l; l = next) {
		char *nl = strchr(l, '\n');

		next = nl ? nl + 1 : NULL;
		if (l[0] == '[')
			skip = !strncmp(l, "[Dismissed]", 11);
		if (!skip)
			kb_buf_add(&out, l, nl ? (size_t)(nl - l + 1)
					       : strlen(l));
	}
	if (kb_write_file_atomic(p, out.n ? out.p : "") != 0) {
		fprintf(stderr, "kdos checklist: cannot write %s\n", p);
		rc = 1;
	}
	kb_buf_free(&out);
	free(data);
	free(p);
	return rc;
}

static int usage(void)
{
	fprintf(stderr, "usage: kdos checklist --json\n"
			"       kdos checklist dismiss <id>\n"
			"       kdos checklist restore\n");
	return 2;
}

int kdt_checklist_main(int argc, char **argv)
{
	if (argc == 0) {
		/* For a person: the same list as words. */
		for (int i = 0; i < NIDS; i++)
			if (present(i))
				printf("[%c] %s%s\n", done_of(i) ? 'x' : ' ',
				       IDS[i],
				       dismissed(IDS[i]) ? " (dismissed)" : "");
		return 0;
	}
	if (argc == 1 && !strcmp(argv[0], "--json"))
		return list_json();
	if (argc == 1 && !strcmp(argv[0], "restore"))
		return restore();
	if (argc == 2 && !strcmp(argv[0], "dismiss")) {
		char *p;

		if (!known(argv[1]))
			return usage();
		p = dismiss_path();
		kdt_mkparent(p);
		kdt_ini_set(p, "Dismissed", argv[1], "true");
		free(p);
		return 0;
	}
	return usage();
}
