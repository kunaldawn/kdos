/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos desk, kdos tiling — the panels and the window layout
 *
 *   kdos desk                          kdos, dock or tiling
 *   kdos desk status --json            the Welcome page's question
 *   kdos desk kdos|dock|tiling         lay the panels out again
 *   kdos desk undo                     put the last panels back
 *
 *   kdos tiling [status] [--json]      is auto-tiling on
 *   kdos tiling on|off|toggle [--only <desk>[,<desk>…]]
 *
 * A DESK IS A PLASMA LAYOUT TEMPLATE, `org.kdos.desk.<name>`, applied by
 * plasmashell's own scripting: every panel goes and the template lays the new
 * ones out. That throws away whatever the person arranged, so the panels file
 * is copied to two places first — the undo set, which `kdos desk undo` puts
 * back, and a dated backup nothing ever removes — and the backup's path is
 * the last line printed, because the page that ran this shows it.
 *
 * AUTO-TILING IS KROHNKITE, A KWIN SCRIPT, switched by its kwinrc key and
 * KWin's reconfigure, which loads or unloads scripts to match. Off by default
 * (D6). The tiling desk turns it on and the other two turn it off, so a desk
 * is the whole arrangement and not half of one.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

#define APPLETS "plasma-org.kde.plasma.desktop-appletsrc"
#define SHELLRC "plasmashellrc"

static const char *const DESKS[] = { "kdos", "dock", "tiling" };

/* ── tiling ────────────────────────────────────────────────────────────── */

static int tiling_on(void)
{
	char *v = kdt_kcfg_cascade("kwinrc", "Plugins", "krohnkiteEnabled");
	int on = kdt_kcfg_bool(v, 0);

	free(v);
	return on;
}

/* KWin re-reads kwinrc and loads or unloads every script to match. No KWin
 * on the bus is not a failure: the key is what the next login reads. */
static void kwin_reconfigure(void)
{
	KdtBus *b = kdt_bus_open(0);

	if (b && kdt_bus_has_owner(b, "org.kde.KWin") == 1)
		kdt_bus_call(b, "org.kde.KWin", "/KWin", "org.kde.KWin",
			     "reconfigure", NULL, NULL, 0, "");
	kdt_bus_close(b);
}

/*
 * `--only`: Krohnkite tiles every virtual desktop except those named in its
 * ignoreVDesktop key, so tiling on the named ones alone is the list of every
 * OTHER desktop's name. The names come from KWin, which is why the option
 * needs a running session.
 */
static int tiling_only(const char *want, char *err, size_t errn)
{
	KdtBus *b = kdt_bus_open(0);
	KdtMsg *r = NULL;
	KbBuf others = { 0 };
	int found = 0, rc;

	if (!b || kdt_bus_get_prop(b, "org.kde.KWin", "/VirtualDesktopManager",
				   "org.kde.KWin.VirtualDesktopManager",
				   "desktops", "a(uss)", &r, err, errn) != 0) {
		if (!*err)
			snprintf(err, errn, "--only needs a running Plasma "
					    "session to name the desktops");
		kdt_bus_close(b);
		return -1;
	}
	if (kdt_bus_enter(b, r, 'a', "(uss)") > 0) {
		unsigned pos;
		const char *id, *name;

		while (kdt_bus_read(b, r, "(uss)", &pos, &id, &name) > 0) {
			char *list = kb_strdup(want), *save = NULL;
			int hit = 0;

			for (char *t = strtok_r(list, ",", &save); t;
			     t = strtok_r(NULL, ",", &save))
				if (!strcmp(t, name))
					hit = 1;
			free(list);
			if (hit) {
				found++;
				continue;
			}
			kb_buf_printf(&others, "%s%s", others.n ? "," : "",
				      name);
		}
		kdt_bus_leave(b, r);
	}
	kdt_bus_free(b, r);
	kdt_bus_close(b);
	if (!found) {
		snprintf(err, errn, "no virtual desktop is named %s", want);
		kb_buf_free(&others);
		return -1;
	}
	rc = kdt_kcfg_set("kwinrc", "Script-krohnkite", "ignoreVDesktop",
			  others.n ? others.p : "", 0);
	kb_buf_free(&others);
	if (rc != 0)
		snprintf(err, errn, "kwriteconfig6 could not write kwinrc");
	return rc;
}

static int tiling_set(int on, const char *only)
{
	char err[256] = "";

	if (on && only) {
		if (tiling_only(only, err, sizeof(err)) != 0) {
			fprintf(stderr, "kdos tiling: %s\n", err);
			return 1;
		}
	} else if (on) {
		char *cur = kdt_kcfg_user("kwinrc", "Script-krohnkite",
					  "ignoreVDesktop");

		if (cur && kdt_kcfg_set("kwinrc", "Script-krohnkite",
					"ignoreVDesktop", NULL, 0) != 0) {
			free(cur);
			fprintf(stderr, "kdos tiling: kwriteconfig6 could not "
					"write kwinrc\n");
			return 1;
		}
		free(cur);
	}
	if (kdt_kcfg_set("kwinrc", "Plugins", "krohnkiteEnabled",
			 on ? "true" : "false", 0) != 0) {
		fprintf(stderr, "kdos tiling: kwriteconfig6 could not write "
				"kwinrc\n");
		return 1;
	}
	kwin_reconfigure();
	return 0;
}

int kdt_tiling_main(int argc, char **argv)
{
	const char *verb = argc > 0 ? argv[0] : "status";
	const char *only = NULL;
	int json = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			json = 1;
		else if (!strcmp(argv[i], "--only") && i + 1 < argc)
			only = argv[++i];
		else
			goto usage;
	}
	if (!strcmp(verb, "--json")) {
		verb = "status";
		json = 1;
	}
	if (!strcmp(verb, "status")) {
		if (only)
			goto usage;
		if (json) {
			KdtBus *b = kdt_bus_open(0);
			int loaded = b ? kdt_bus_call_b(b, "org.kde.KWin",
					"/Scripting",
					"org.kde.kwin.Scripting",
					"isScriptLoaded", NULL, 0, "s",
					"krohnkite") == 1 : 0;

			kdt_bus_close(b);
			printf("{\"tiling\": %s, \"loaded\": %s}\n",
			       tiling_on() ? "true" : "false",
			       loaded ? "true" : "false");
		} else {
			printf("%s\n", tiling_on() ? "on" : "off");
		}
		return 0;
	}
	if (json)
		goto usage;
	if (!strcmp(verb, "on"))
		return tiling_set(1, only);
	if (!strcmp(verb, "off")) {
		if (only)
			goto usage;
		return tiling_set(0, NULL);
	}
	if (!strcmp(verb, "toggle")) {
		int on = !tiling_on(), rc = tiling_set(on, on ? only : NULL);

		if (!rc)
			printf("%s\n", on ? "on" : "off");
		return rc;
	}
usage:
	fprintf(stderr, "usage: kdos tiling [status] [--json]\n"
			"       kdos tiling on|off|toggle "
			"[--only <desk>[,<desk>...]]\n");
	return 2;
}

/* ── desks ─────────────────────────────────────────────────────────────── */

static char *desk_current(void)
{
	char *v = kdt_kcfg_cascade("kdosrc", "Desk", "Layout");

	for (size_t i = 0; v && i < sizeof(DESKS) / sizeof(DESKS[0]); i++)
		if (!strcmp(v, DESKS[i]))
			return v;
	free(v);
	return kb_strdup("kdos");
}

static char *undo_dir(void)
{
	return kdt_cfg_home("kdos/desk-undo");
}

static int undo_present(void)
{
	char *d = undo_dir();
	int n = 0;
	char **v = kb_listdir(d, &n);

	kb_strv_free(v);
	free(d);
	return n > 0;
}

static int plasmashell_up(KdtBus **out);

/*
 * WHAT "ARRANGED BY HAND" MEANS: the panels, where each sits, and the applets
 * on each in their order — asked of plasmashell itself, read-only. Not the
 * panels file's time: plasmashell rewrites that file for a popup's size and
 * other applet state, so its mtime moves when nothing was arranged.
 */
static const char PANELS_JS[] =
	"panels().forEach(p => {"
	" p.currentConfigGroup = ['General'];"
	" let o = String(p.readConfig('AppletOrder', '')).split(';')"
	"  .filter(x => x).map(Number);"
	" let w = p.widgets();"
	" let ids = o.length ? o : w.map(x => x.id);"
	" print(p.location + '|' + p.alignment + '|' + ids.map(i => {"
	"  let x = p.widgetById(i); return x ? x.type : ''; }).join(',') + "
	"'\\n'); });";

/* The description's digest, or NULL when plasmashell is not answering. */
static char *panels_digest(KdtBus *b)
{
	char err[256] = "", hex[65], *out;
	KbSha256 h;

	out = kdt_bus_call_s(b, "org.kde.plasmashell", "/PlasmaShell",
			     "org.kde.PlasmaShell", "evaluateScript", err,
			     sizeof(err), "s", PANELS_JS);
	if (!out)
		return NULL;
	kb_sha256_init(&h);
	kb_sha256_update(&h, out, strlen(out));
	kb_sha256_final(&h, hex);
	free(out);
	return kb_strdup(hex);
}

/* [Desk] LaidOutPanels: the digest of the panels a desk laid out. */
static void record_panels(KdtBus *b)
{
	char *d = panels_digest(b);

	if (d)
		kdt_kcfg_set("kdosrc", "Desk", "LaidOutPanels", d, 0);
	free(d);
}

static int desk_status(int json)
{
	char *desk = desk_current();
	char *laid = kdt_kcfg_user("kdosrc", "Desk", "LaidOut");
	char *want = kdt_kcfg_user("kdosrc", "Desk", "LaidOutPanels");
	char *undo = undo_dir();
	KdtBus *b = NULL;
	int edited = 0;

	/*
	 * Outside a session nothing can be asked, and a desk laid out by its
	 * template at the first login has a LaidOut and no digest yet: the
	 * first status after it takes the panels as laid out and records
	 * them, so Welcome's first look at an untouched desk records it.
	 */
	if (laid && *laid && plasmashell_up(&b)) {
		char *now = panels_digest(b);

		if (now && want && *want)
			edited = strcmp(now, want) != 0;
		else if (now)
			kdt_kcfg_set("kdosrc", "Desk", "LaidOutPanels", now, 0);
		free(now);
	}
	kdt_bus_close(b);
	if (json) {
		KbBuf o = { 0 };

		kb_buf_str(&o, "{\"desk\": ");
		kb_json_str(&o, desk);
		kb_buf_printf(&o, ", \"panels_edited\": %s, \"undo\": ",
			      edited ? "true" : "false");
		if (undo_present())
			kb_json_str(&o, undo);
		else
			kb_buf_str(&o, "null");
		kb_buf_str(&o, "}\n");
		fwrite(o.p, 1, o.n, stdout);
		kb_buf_free(&o);
	} else {
		printf("%s\n", desk);
	}
	free(desk);
	free(laid);
	free(want);
	free(undo);
	return 0;
}

/* Copy `file` from the config home into `dir` when it exists; a file the
 * session has not written yet is not an error. */
static int keep(const char *file, const char *dir)
{
	char *src = kdt_cfg_home(file), *dst = kb_path_join(dir, file);
	int rc = 0;

	if (kb_path_exists(src))
		rc = kb_copy_file(src, dst);
	free(src);
	free(dst);
	return rc;
}

/*
 * THE UNDO SET: the panels file, plasmashellrc and the one kwinrc key a desk
 * changes, plus the desk it replaced and that desk's panels digest. Replaced
 * whole, so `undo` always means "before the last desk", never a mix of two.
 */
static int save_undo(void)
{
	char *dir = undo_dir(), *conf, *kro;
	char *kwin = kdt_cfg_home("kwinrc");
	char *prev = kdt_kcfg_user("kdosrc", "Desk", "Layout");
	char *dig = kdt_kcfg_user("kdosrc", "Desk", "LaidOutPanels");
	KbBuf b = { 0 };
	int rc = 0;

	kb_rmtree(dir);
	if (kb_mkdir_p(dir) != 0) {
		free(dir);
		free(kwin);
		free(prev);
		free(dig);
		return -1;
	}
	rc |= keep(APPLETS, dir);
	rc |= keep(SHELLRC, dir);
	kro = kdt_kcfg_get_raw(kwin, "Plugins", "krohnkiteEnabled");
	kb_buf_str(&b, "[Desk]\n");
	if (kro)
		kb_buf_printf(&b, "krohnkiteEnabled=%s\n", kro);
	if (prev)
		kb_buf_printf(&b, "Layout=%s\n", prev);
	if (dig)
		kb_buf_printf(&b, "LaidOutPanels=%s\n", dig);
	conf = kb_path_join(dir, "desk-undo.conf");
	rc |= kb_write_file_atomic(conf, b.p);
	kb_buf_free(&b);
	free(conf);
	free(kro);
	free(kwin);
	free(prev);
	free(dig);
	free(dir);
	return rc ? -1 : 0;
}

/* The dated copy of the panels file, which nothing removes: the undo set
 * holds one step, and a person may want the panels from three desks ago. */
static char *backup(void)
{
	char stamp[32], *base, *dir, *src;
	time_t now = time(NULL);
	struct tm tm;

	gmtime_r(&now, &tm);
	strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &tm);
	base = kdt_data_home("kdos/desk-backup");
	dir = kb_path_join(base, stamp);
	free(base);
	if (kb_mkdir_p(dir) != 0) {
		free(dir);
		return NULL;
	}
	src = kdt_cfg_home(APPLETS);
	if (kb_path_exists(src)) {
		char *dst = kb_path_join(dir, APPLETS);

		if (kb_copy_file(src, dst) != 0) {
			free(dst);
			free(src);
			free(dir);
			return NULL;
		}
		free(dst);
	}
	free(src);
	return dir;
}

static int plasmashell_up(KdtBus **out)
{
	KdtBus *b = kdt_bus_open(0);

	if (b && kdt_bus_has_owner(b, "org.kde.plasmashell") == 1) {
		*out = b;
		return 1;
	}
	kdt_bus_close(b);
	*out = NULL;
	return 0;
}

static int desk_apply(const char *name)
{
	KdtBus *b = NULL;
	char js[160], err[256] = "", *dir, *out;

	if (!plasmashell_up(&b)) {
		if (kdt_kcfg_set("kdosrc", "Desk", "Layout", name, 0) != 0) {
			fprintf(stderr, "kdos desk: kwriteconfig6 could not "
					"write kdosrc\n");
			return 1;
		}
		printf("desk %s is laid out at the next login\n", name);
		return 0;
	}
	/* The undo set first: it records the desk being replaced. */
	if (save_undo() != 0 || !(dir = backup())) {
		kdt_bus_close(b);
		fprintf(stderr, "kdos desk: the panels could not be backed up "
				"first, so nothing was changed\n");
		return 1;
	}
	if (kdt_kcfg_set("kdosrc", "Desk", "Layout", name, 0) != 0) {
		kdt_bus_close(b);
		free(dir);
		fprintf(stderr, "kdos desk: kwriteconfig6 could not write "
				"kdosrc\n");
		return 1;
	}
	snprintf(js, sizeof(js),
		 "panels().forEach(p => p.remove()); "
		 "loadTemplate('org.kdos.desk.%s');", name);
	out = kdt_bus_call_s(b, "org.kde.plasmashell", "/PlasmaShell",
			     "org.kde.PlasmaShell", "evaluateScript", err,
			     sizeof(err), "s", js);
	if (!out) {
		kdt_bus_close(b);
		fprintf(stderr, "kdos desk: plasmashell refused the layout: "
				"%s\n", *err ? err : "no answer");
		free(dir);
		return 1;
	}
	free(out);
	record_panels(b);
	kdt_bus_close(b);
	if (tiling_set(!strcmp(name, "tiling"), NULL) != 0) {
		free(dir);
		return 1;
	}
	printf("%s\n", dir);
	free(dir);
	return 0;
}

static int run_wait(const char *p0, const char *p1)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, p0);
	kb_argv_add(&a, p1);
	kb_argv_end(&a);
	return kb_run(&a);
}

static int desk_undo(void)
{
	char *dir = undo_dir(), *conf, *kro, *prev, *dig;
	int rc = 0;

	if (!undo_present()) {
		fprintf(stderr, "nothing to undo\n");
		free(dir);
		return 1;
	}
	/* plasmashell writes its files when it quits, so it is stopped
	 * before they are put back and started after. */
	run_wait("kquitapp6", "plasmashell");
	for (int i = 0; i < 2; i++) {
		const char *file = i ? SHELLRC : APPLETS;
		char *src = kb_path_join(dir, file), *dst = kdt_cfg_home(file);

		if (kb_path_exists(src) && kb_copy_file(src, dst) != 0)
			rc = 1;
		free(src);
		free(dst);
	}
	conf = kb_path_join(dir, "desk-undo.conf");
	kro = kdt_kcfg_get(conf, "Desk", "krohnkiteEnabled");
	prev = kdt_kcfg_get(conf, "Desk", "Layout");
	dig = kdt_kcfg_get(conf, "Desk", "LaidOutPanels");
	if (kdt_kcfg_set("kwinrc", "Plugins", "krohnkiteEnabled", kro, 0) ||
	    kdt_kcfg_set("kdosrc", "Desk", "Layout", prev, 0) ||
	    kdt_kcfg_set("kdosrc", "Desk", "LaidOutPanels", dig, 0))
		rc = 1;
	free(kro);
	free(prev);
	free(dig);
	free(conf);

	{
		KbArgv a = { 0 };

		kb_argv_add(&a, "kstart");
		kb_argv_add(&a, "plasmashell");
		kb_argv_end(&a);
		kb_run_detach(&a);
	}
	kwin_reconfigure();
	if (rc) {
		fprintf(stderr, "kdos desk: not every file could be put back; "
				"the set is kept in %s\n", dir);
		free(dir);
		return 1;
	}
	kb_rmtree(dir);
	free(dir);
	printf("restored\n");
	return 0;
}

int kdt_desk_main(int argc, char **argv)
{
	const char *verb = argc > 0 ? argv[0] : NULL;

	if (!verb)
		return desk_status(0);
	if (!strcmp(verb, "status")) {
		if (argc == 1)
			return desk_status(0);
		if (argc == 2 && !strcmp(argv[1], "--json"))
			return desk_status(1);
	} else if (!strcmp(verb, "undo") && argc == 1) {
		return desk_undo();
	} else if (argc == 1) {
		for (size_t i = 0; i < sizeof(DESKS) / sizeof(DESKS[0]); i++)
			if (!strcmp(verb, DESKS[i]))
				return desk_apply(verb);
	}
	fprintf(stderr, "usage: kdos desk [status [--json]]\n"
			"       kdos desk kdos|dock|tiling\n"
			"       kdos desk undo\n");
	return 2;
}
