/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos app — the applications on this machine, and the ones it can build
 * ---------------------------------
 *
 *     $ kdos app search image
 *     app.gimp    Graphics  installed  92.0M  Create images and edit photographs
 *     app.krita   Graphics  available  180.0M Digital painting
 *     $ kdos app install creative
 *
 * THE CLI HALF OF THE STORE, over the same catalogue the surface reads. Every
 * verb that builds, removes, exports or imports is `kdos-appbox` run as a
 * child: one implementation, so a command line and a button cannot disagree
 * about what installing means.
 *
 * IT NEVER NAMES A PATH TO A DAEMON. `import` hands kdos-packd a filename
 * inside the daemon's own staging directory and nothing else — that is rule 3,
 * and it is what keeps something reachable from `wheel` from being
 * `mount /dev/sda2 /etc`.
 *
 * WHAT IS INSTALLED IS ASKED ONCE, of `kdos-appbox catalogue`. A second idea
 * of installed-ness here would be a second answer that drifts from the one the
 * store surface draws.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kbase.h"
/* The entry writer quotes each field through the same library that reads one,
 * so a command survives the round trip through a file two other programs
 * parse. */
#include "kxdg.h"
#include "kpack.h"
#include "kpkg.h"
#include "kdos-tools.h"

#define APP_MAX 512
/* A pending selection is a person's tick list, not a catalogue dump. */
#define CAT_MAX_PENDING 256

/* ── the verbs ─────────────────────────────────────────────────────────── */

/*
 * Every verb that BUILDS is kdos-appbox run as a child. One implementation, so
 * a command line and a button cannot disagree about what installing means —
 * and kb_proc_verbose, because this is a person at a prompt and podman's own
 * sentence is the diagnosis when a build refuses.
 */
static int appbox_run(const char *verb, const char *arg1, const char *arg2,
		      int nrest, char **rest)
{
	KbArgv a = {0};

	kb_proc_verbose = 1;
	kb_argv_add(&a, "kdos-appbox");
	kb_argv_add(&a, verb);
	if (arg1)
		kb_argv_add(&a, arg1);
	if (arg2)
		kb_argv_add(&a, arg2);
	for (int i = 0; i < nrest; i++)
		kb_argv_add(&a, rest[i]);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

/*
 * THE CATALOGUE IS ASKED ONCE, of `kdos-appbox catalogue`, and its answer
 * already carries the install state. A second idea of installed-ness here
 * would be a second answer that drifts from the one the store surface draws.
 *
 * Tab-separated: id, name, category, bytes, parent, state, tagline.
 */
#define CAT_FIELDS 7

static char *catalogue_text(void)
{
	KbArgv a = {0};
	KbBuf out = {0};

	kb_argv_add(&a, "kdos-appbox");
	kb_argv_add(&a, "catalogue");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0 || !out.p) {
		kb_buf_free(&out);
		return NULL;
	}
	return out.p;
}

static int split_row(char *line, char *f[CAT_FIELDS])
{
	int n = 0;

	f[n++] = line;
	for (char *p = line; *p && n < CAT_FIELDS; p++)
		if (*p == '\t') {
			*p = 0;
			f[n++] = p + 1;
		}
	return n;
}

static void cat_row_print(char *f[CAT_FIELDS])
{
	printf("  %-22s %-12s %-10s %9s  %s\n", f[0], f[2], f[5],
	       kb_human_size(strtoull(f[3], NULL, 10)), f[6]);
}

/*
 * `--all` is every row; the default is what is INSTALLED. A catalogue of 182
 * applications printed whenever somebody types `kdos app list` buries the
 * handful they actually have.
 */
static int cmd_list(int all)
{
	char *text = catalogue_text(), *line, *save;
	int n = 0;

	if (!text) {
		fprintf(stderr, "kdos app: no catalogue — is kdos-appbox "
				"installed?\n");
		return 2;
	}
	printf("  %-22s %-12s %-10s %9s  %s\n", "ID", "CATEGORY", "STATE",
	       "SIZE~", "");
	for (line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[CAT_FIELDS];

		if (split_row(line, f) < CAT_FIELDS)
			continue;
		if (!all && strcmp(f[5], "installed"))
			continue;
		cat_row_print(f);
		n++;
	}
	if (!n)
		printf("  nothing%s\n",
		       all ? "" : " installed — `kdos app list --all`");
	free(text);
	return 0;
}

static int cmd_search(const char *q)
{
	char *text = catalogue_text(), *line, *save;
	int n = 0;

	if (!text)
		return 2;
	for (line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[CAT_FIELDS];

		if (split_row(line, f) < CAT_FIELDS)
			continue;
		if (!strcasestr(f[0], q) && !strcasestr(f[1], q) &&
		    !strcasestr(f[2], q) && !strcasestr(f[6], q))
			continue;
		cat_row_print(f);
		n++;
	}
	free(text);
	if (!n) {
		printf("  nothing matches '%s'\n", q);
		return 1;
	}
	return 0;
}

/*
 * THE SIZE IS LABELLED AN ESTIMATE, here and on every surface. What apt
 * resolves on the day depends on the snapshot, and shared runtime layers are
 * counted once on disk however many applications name them — a number
 * presented as fact that the install then contradicts is worse than none.
 */
static int cmd_info(const char *id)
{
	char *text = catalogue_text(), *line, *save;
	int found = 0;

	if (!text)
		return 2;
	for (line = strtok_r(text, "\n", &save); line && !found;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[CAT_FIELDS];

		if (split_row(line, f) < CAT_FIELDS || strcmp(f[0], id))
			continue;
		found = 1;
		printf("%s\n", f[0]);
		printf("  name        %s\n", f[1]);
		printf("  category    %s\n", f[2]);
		printf("  state       %s\n", f[5]);
		printf("  built on    %s\n", f[4]);
		printf("  size        %s (an estimate)\n",
		       kb_human_size(strtoull(f[3], NULL, 10)));
		if (f[6][0])
			printf("  %s\n", f[6]);
	}
	free(text);
	if (!found) {
		fprintf(stderr, "kdos app: %s is not in the catalogue\n", id);
		return 1;
	}
	return 0;
}

/*
 * WHAT THE INSTALLER CHOSE, OFFERED RATHER THAN BUILT. kinstall writes the
 * ticked set here when it could neither import nor reach a network; building
 * it is minutes of apt and belongs to a person who asked for it now.
 *
 * The file is removed only on a CLEAN run. A partial one leaves it, so a
 * second attempt does the rest rather than forgetting what was wanted.
 */
#define PENDING_PATH "/var/lib/kdos/apps-pending"

static int cmd_pending(void)
{
	char buf[8192];
	char *argv[CAT_MAX_PENDING];
	char *line, *save;
	int n = 0, rc;

	if (kb_read_file(PENDING_PATH, buf, sizeof(buf)) <= 0) {
		printf("nothing pending\n");
		return 0;
	}
	for (line = strtok_r(buf, "\n", &save); line && n < CAT_MAX_PENDING;
	     line = strtok_r(NULL, "\n", &save)) {
		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		line[strcspn(line, " \t\r")] = 0;
		argv[n++] = line;
	}
	if (!n) {
		printf("nothing pending\n");
		return 0;
	}
	printf("==> %d chosen during installation\n", n);
	rc = appbox_run("install", NULL, NULL, n, argv);
	if (rc == 0)
		unlink(PENDING_PATH);
	else
		fprintf(stderr, "kdos app: %s kept — run `kdos app install "
				"--pending` again for the rest\n", PENDING_PATH);
	return rc;
}

static char *user_table(void)
{
	char *p = kb_calloc(1, 512);
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		snprintf(p, 512, "%s/kdos/alien-apps", data);
	else
		snprintf(p, 512, "%s/.local/share/kdos/alien-apps", kb_home_dir());
	return p;
}

/*
 * The click that installs is the click that opens. The shim is what actually
 * runs it — a launcher's own name in `~/.local/bin`, written by genlaunchers —
 * so this resolves the id to a shim and execs it.
 */
static int cmd_launch(const char *id)
{
	char *text = catalogue_text(), *line, *save;
	char state[32] = "";
	char *ut, *buf;
	char shim[128] = "", first[128] = "";
	const char *want = !strncmp(id, "app.", 4) ? id + 4 : id;
	char path[512];

	if (!text)
		return 2;
	for (line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *f[CAT_FIELDS];

		if (split_row(line, f) < CAT_FIELDS || strcmp(f[0], id))
			continue;
		kb_strlcpy(state, f[5], sizeof(state));
		break;
	}
	free(text);
	if (!state[0]) {
		fprintf(stderr, "kdos app: %s is not in the catalogue\n", id);
		return 1;
	}
	if (strcmp(state, "installed")) {
		char *one[1];
		int rc;

		one[0] = (char *)id;
		rc = appbox_run("install", NULL, NULL, 1, one);
		if (rc != 0)
			return rc;
	}

	ut = user_table();
	buf = kb_calloc(1, 1 << 18);
	if (kb_read_file(ut, buf, 1 << 18) > 0)
		for (line = strtok_r(buf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *tab = strchr(line, '\t'), *tab2;
			if (*line == '#' || !tab || !(tab2 = strchr(tab + 1, '\t')))
				continue;
			*tab = 0;
			if (strcmp(tab2 + 1, id))
				continue;
			if (!first[0])
				snprintf(first, sizeof(first), "%s", line);
			if (!strcmp(line, want)) {
				snprintf(shim, sizeof(shim), "%s", line);
				break;
			}
		}
	free(buf);
	free(ut);
	if (!shim[0])
		snprintf(shim, sizeof(shim), "%s", first);
	if (!shim[0]) {
		fprintf(stderr, "kdos app: %s carries no launcher — it is a "
				"command; see `kdos app info %s`\n", id, id);
		return 1;
	}
	snprintf(path, sizeof(path), "%s/.local/bin/%s", kb_home_dir(), shim);
	execl(path, shim, (char *)NULL);
	fprintf(stderr, "kdos app: cannot run %s: %s\n", path, strerror(errno));
	return 1;
}

/* ── kdos app tui ───────────────────────────────────────────────────────
 *
 * A TERMINAL PROGRAM BECOMES AN APPLICATION, without anybody editing a file
 * by hand. Everything the catalogue's own entries carry, written the way the
 * spec says to write it: `Terminal=true`, an `Exec` quoted per field rather
 * than concatenated, and the two KDOS keys that say how the window should
 * open.
 *
 * `X-KDOS-TUI=true` IS WHAT `rm` CHECKS. This command deletes only a file it
 * wrote — a slug is a person's word and the same word can name an entry the
 * image shipped, so the marker is what keeps `kdos app tui rm` from being a
 * way to delete somebody else's application.
 *
 * AND THE SLUG IS PREFIXED for the other half of that: a file called
 * `mc.desktop` in the user's directory SHADOWS the one in `/usr/share`, so a
 * name that happened to collide would take the shipped entry off the menu
 * rather than adding a row beside it.
 */
#define TUI_PREFIX "kdos-tui-"

/* The whole command's usage, which every wrong shape here answers with. */
static int usage(void);

/* ~/.local/share/applications, made if it is not there — the same directory
 * `genlaunchers --user` writes into, and the one XDG says a person's own
 * entries live in. */
static char *tui_dir(void)
{
	const char *xdg = getenv("XDG_DATA_HOME");
	char *base = xdg && *xdg ? kb_strdup(xdg)
				 : kb_path_join(kb_home_dir(), ".local/share");
	char *dir = kb_path_join(base, "applications");

	free(base);
	kb_mkdir_p(dir);
	return dir;
}

/*
 * A DISPLAY NAME BECOMES A FILENAME. Lower case, and anything that is not a
 * letter or a digit becomes one dash — a filename with a space in it is a
 * filename half the tools that read this directory will hand to a shell.
 */
static int tui_slug(const char *name, char *out, size_t n)
{
	size_t o = 0;
	int last_dash = 1;	/* so a leading run produces nothing */

	for (const char *p = name; *p && o + 1 < n; p++) {
		unsigned char c = (unsigned char)*p;

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9')) {
			out[o++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
			last_dash = 0;
			continue;
		}
		if (last_dash)
			continue;
		out[o++] = '-';
		last_dash = 1;
	}
	while (o && out[o - 1] == '-')
		o--;
	out[o] = '\0';
	return o > 0;
}

/*
 * THE EXEC LINE, QUOTED PER FIELD AND NEVER CONCATENATED.
 *
 * The command is split by the library that READS an Exec and each word is
 * quoted by the one that writes one, so a path with a space in it survives the
 * round trip through a file two other programs parse.
 *
 * `%` IS DOUBLED. A percent begins a field code in an Exec line, so a literal
 * one has to be `%%` — `kxdg_exec_quote` does not do it, because it is
 * quoting a word rather than writing an Exec, and a filename with a percent in
 * it would otherwise reach the launcher as half a field code.
 */
static int tui_exec(const char *cmd, char *out, size_t n)
{
	char store[1024];
	const char *av[32];
	int argc = kxdg_exec_split(cmd, NULL, -1, store, sizeof(store), av, 32);
	size_t o = 0;

	if (argc <= 0)
		return 0;
	for (int i = 0; i < argc; i++) {
		char q[512];

		if (kxdg_exec_quote(av[i], q, sizeof(q)) != 0)
			return 0;
		for (const char *p = q; *p; p++) {
			if (o + 3 >= n)
				return 0;
			if (*p == '%')
				out[o++] = '%';
			out[o++] = *p;
		}
		if (i + 1 < argc) {
			if (o + 2 >= n)
				return 0;
			out[o++] = ' ';
		}
	}
	out[o] = '\0';
	return 1;
}

/* True when this file is one this command wrote. */
static int tui_ours(const char *path, char *name, size_t nn)
{
	KxdgEntry e = { 0 };
	int ours;

	if (kxdg_load(&e, path, "Desktop Entry") != 0)
		return 0;
	ours = kxdg_bool(&e, "X-KDOS-TUI", 0);
	if (ours && name)
		snprintf(name, nn, "%s", kxdg_get(&e, "Name", ""));
	kxdg_free(&e);
	return ours;
}

static int tui_add(int argc, char **argv)
{
	const char *name = NULL, *cmd = NULL;
	const char *icon = "system-run", *cat = "Utility";
	const char *size = NULL;
	int flt = 0;
	char slug[128], exec[2048], path[1024];
	char *dir;
	FILE *f;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--float")) {
			flt = 1;
		} else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
			int c = 0, r = 0;

			size = argv[++i];
			if (sscanf(size, "%dx%d", &c, &r) != 2 || c < 4 ||
			    r < 2) {
				fprintf(stderr, "kdos app tui: --size wants "
						"COLSxROWS, at least 4x2\n");
				return 2;
			}
		} else if (!strcmp(argv[i], "--icon") && i + 1 < argc) {
			icon = argv[++i];
		} else if (!strcmp(argv[i], "--category") && i + 1 < argc) {
			cat = argv[++i];
		} else if (argv[i][0] == '-') {
			return usage();
		} else if (!name) {
			name = argv[i];
		} else if (!cmd) {
			cmd = argv[i];
		} else {
			return usage();
		}
	}
	if (!name || !cmd)
		return usage();
	if (!tui_slug(name, slug, sizeof(slug))) {
		fprintf(stderr, "kdos app tui: '%s' has no letters or digits "
				"to make a filename from\n", name);
		return 2;
	}
	if (!tui_exec(cmd, exec, sizeof(exec))) {
		fprintf(stderr, "kdos app tui: cannot write '%s' as an Exec "
				"line\n", cmd);
		return 2;
	}

	dir = tui_dir();
	snprintf(path, sizeof(path), "%s/" TUI_PREFIX "%s.desktop", dir, slug);
	free(dir);

	f = fopen(path, "we");
	if (!f) {
		fprintf(stderr, "kdos app tui: %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	fprintf(f, "[Desktop Entry]\n");
	fprintf(f, "Type=Application\n");
	fprintf(f, "Name=%s\n", name);
	fprintf(f, "Exec=%s\n", exec);
	fprintf(f, "Icon=%s\n", icon);
	fprintf(f, "Terminal=true\n");
	fprintf(f, "Categories=%s;\n", cat);
	fprintf(f, "X-KDOS-TUI=true\n");
	if (flt)
		fprintf(f, "X-KDOS-Float=true\n");
	if (size)
		fprintf(f, "X-KDOS-Size=%s\n", size);
	fclose(f);

	printf("%s%s\n", TUI_PREFIX, slug);
	return 0;
}

static int tui_rm(const char *slug)
{
	char *dir = tui_dir();
	char path[1024];

	if (strchr(slug, '/')) {
		free(dir);
		fprintf(stderr, "kdos app tui: a slug is a name, not a path\n");
		return 2;
	}
	/* Either spelling: what `add` printed, or the same without the prefix
	 * it added — a person reading `ls` sees the whole filename. */
	if (!strncmp(slug, TUI_PREFIX, sizeof(TUI_PREFIX) - 1))
		snprintf(path, sizeof(path), "%s/%s.desktop", dir, slug);
	else
		snprintf(path, sizeof(path), "%s/" TUI_PREFIX "%s.desktop",
			 dir, slug);
	free(dir);

	if (!tui_ours(path, NULL, 0)) {
		fprintf(stderr, "kdos app tui: %s is not one this command "
				"wrote\n", path);
		return 2;
	}
	if (unlink(path) != 0) {
		fprintf(stderr, "kdos app tui: %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	return 0;
}

static int tui_ls(void)
{
	char *dir = tui_dir();
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d) {
		free(dir);
		return 0;
	}
	while ((e = readdir(d))) {
		char path[1024], name[256];
		size_t len = strlen(e->d_name);

		if (len < 9 || strcmp(e->d_name + len - 8, ".desktop"))
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (!tui_ours(path, name, sizeof(name)))
			continue;
		printf("%-32.*s  %s\n", (int)(len - 8), e->d_name, name);
		n++;
	}
	closedir(d);
	free(dir);
	if (!n)
		printf("no terminal applications added here\n");
	return 0;
}

static int cmd_tui(int argc, char **argv)
{
	if (argc < 1)
		return usage();
	if (!strcmp(argv[0], "add"))
		return tui_add(argc - 1, argv + 1);
	if (!strcmp(argv[0], "rm") && argc > 1)
		return tui_rm(argv[1]);
	if (!strcmp(argv[0], "ls"))
		return tui_ls();
	return usage();
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos app list [--all]\n"
		"       kdos app search <text>\n"
		"       kdos app info <id>\n"
		"       kdos app groups\n"
		"       kdos app install <id|group>... [--dry-run]\n"
		"       kdos app install --pending  what the installer chose\n"
		"       kdos app launch <id>        install if needed, then run it\n"
		"       kdos app remove <id|group>...\n"
		"       kdos app export <file.ktar> <id|group>...\n"
		"       kdos app import <file.ktar> [<id>...]\n"
		"       kdos app tui add <name> <command> [--float]\n"
		"                       [--size COLSxROWS] [--icon NAME]\n"
		"                       [--category X]\n"
		"       kdos app tui rm <slug> | ls\n"
		"\nInstalling builds the application here, which needs a network.\n"
		"An exported set installs offline and is verified at the mount.\n");
	return 2;
}

int kdt_app(int argc, char **argv)
{
	const char *cmd;

	if (argc < 1)
		return usage();
	cmd = argv[0];

	if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return usage();

	/*
	 * BEFORE THE DAEMON GATE. `tui` writes a file in this person's own
	 * data directory and asks `kdos-packd` nothing — a verb that needed
	 * the pack daemon to be running, and the caller to be in `wheel`,
	 * before it could add a menu row for `ncdu` would be a verb refused
	 * for a reason that has nothing to do with it.
	 */
	if (!strcmp(cmd, "tui"))
		return cmd_tui(argc - 1, argv + 1);

	/*
	 * NO DAEMON GATE. kdos-packd is needed only by `import`, which is the
	 * one verb that mounts anything; listing what this machine can build
	 * is a file read, and refusing it because a daemon is down would
	 * refuse the store on every machine that has never imported a set.
	 */
	if (!strcmp(cmd, "list"))
		return cmd_list(argc > 1 && !strcmp(argv[1], "--all"));
	if (!strcmp(cmd, "search") && argc > 1)
		return cmd_search(argv[1]);
	if ((!strcmp(cmd, "info") || !strcmp(cmd, "show")) && argc > 1)
		return cmd_info(argv[1]);
	if (!strcmp(cmd, "groups"))
		return appbox_run("catalogue", "--groups", NULL, 0, NULL);
	if (!strcmp(cmd, "launch") && argc > 1)
		return cmd_launch(argv[1]);

	if (!strcmp(cmd, "install") || !strcmp(cmd, "remove")) {
		const char *verb = !strcmp(cmd, "install") ? "install"
							   : "uninstall";
		if (argc > 1 && !strcmp(argv[1], "--pending"))
			return cmd_pending();
		if (argc < 2)
			return usage();
		return appbox_run(verb, NULL, NULL, argc - 1, argv + 1);
	}
	if ((!strcmp(cmd, "export") || !strcmp(cmd, "import")) && argc > 1)
		return appbox_run(cmd, argv[1], NULL, argc - 2, argv + 2);

	return usage();
}
