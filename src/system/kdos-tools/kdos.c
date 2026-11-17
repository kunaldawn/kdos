/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos — the front door
 *
 *   kdos help     commands, and the keys this desktop binds
 *   kdos theme    switch the accent across Plasma, Konsole, Qt 5 and the
 *                 terminal programs (theme.c)
 *   kdos status   packages, boxes, exported apps, the session
 *   kdos doctor   the checks that have actually caught something on this distro
 *   kdos app      install an application into a box
 *   kdos version
 *
 * The palette is libkcolor's and nothing else's. A second copy of the scheme
 * table here would be eight schemes and nine colours hand-kept in step with
 * the installer's, and the two would be edited separately.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/statfs.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/utsname.h>

#include <stdbool.h>
#include <time.h>

/* The battery is a file under /sys and libkproc is what reads it — the same
 * reader the resource monitor uses, so there is one answer and not two. */
#include "kproc.h"
#include "kdos-tools.h"

/* ANSI, but only when someone is looking. */
static const char *C_A = "", *C_B = "", *C_D = "", *C_W = "", *C_0 = "";

static void colours(void)
{
	if (!isatty(STDOUT_FILENO))
		return;
	C_A = "\033[1;32m";
	C_B = "\033[0;32m";
	C_D = "\033[2;32m";
	C_W = "\033[1;33m";
	C_0 = "\033[0m";
}

/* ──────────────────────────────────────────────────────────────────────── */

char *kdt_cfg_home(const char *rest)
{
	const char *x = getenv("XDG_CONFIG_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".config");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

char *kdt_data_home(const char *rest)
{
	const char *x = getenv("XDG_DATA_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".local/share");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

/* Exported because `kdos thumb` writes into the same cache root and a second
 * answer to where that is would put its thumbnails somewhere nothing else
 * looks. */
char *kdt_cache_home(const char *rest)
{
	const char *x = getenv("XDG_CACHE_HOME");
	char *base = (x && *x) ? kb_strdup(x)
			       : kb_path_join(kb_home_dir(), ".cache");
	char *p = kb_path_join(base, rest);
	free(base);
	return p;
}

void kdt_mkparent(const char *path)
{
	char *copy = kb_strdup(path);
	char *slash = strrchr(copy, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(copy);
	}
	free(copy);
}

static void help_body(FILE *o)
{
	fprintf(o, "%s", C_A);
	fputs("██╗  ██╗██████╗  ██████╗ ███████╗\n"
	      "██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝\n"
	      "█████╔╝ ██║  ██║██║   ██║███████╗\n"
	      "██╔═██╗ ██║  ██║██║   ██║╚════██║\n"
	      "██║  ██╗██████╔╝╚██████╔╝███████║\n"
	      "╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝\n", o);
	fprintf(o, "%s   Kool Desktop Operating System%s\n\n", C_D, C_0);

	/*
	 * THE THREE LANES, first, because the most common question on this
	 * machine is which of them a thing belongs to — and the value of a
	 * list like this was never the commands, it is that one place shows
	 * the whole system's verbs.
	 */
	fprintf(o, "%sWHERE THINGS LIVE%s\n", C_A, C_0);
	fprintf(o, "  %-24s %s\n", "the host",
		"kpkg — compiled here from source, against musl");
	fprintf(o, "  %-24s %s\n", "applications",
		"kdos app — a box over a pack, or built here online");
	fprintf(o, "  %-24s %s\n", "environments",
		"kdos-box — a box you built is a file to give away");
	fprintf(o, "\n");

	fprintf(o, "%sCOMMANDS%s\n", C_A, C_0);
	static const char *CMDS[][2] = {
		{ "kdos why <path|port>", "what provides this, and why it is that way" },
		{ "kdos explain [topic]", "the reasons behind the system's design, browsable" },
		{ "kdos sandbox <p> -- <cmd>", "run a native app under Landlock" },
		{ "kdos app search <t>", "the applications here and on the medium" },
		{ "kdos app install <id>", "build one application here — also remove" },
		{ "kdos-box list", "environments: create, enter, freeze, export" },
		{ "kdos-fetch-app <name>", "install an alien app from a network" },
		{ "kdos theme [name]", "an accent by name, next, prev or list" },
		{ "kdos theme style <f>", "a style file: accent and CRT tuning, shareable" },
		{ "kdos theme --audit", "is every generated colour still the palette's?" },
		{ "kdos phosphor on|off", "Phosphor mode: the CRT pass, motion, retro accent" },
		{ "kdos desk kdos|dock|tiling", "lay the panels out again — `undo` puts them back" },
		{ "kdos tiling on|off", "auto-tiling (Krohnkite) — also toggle, --only <d>" },
		{ "kdos status", "packages, boxes, exported apps, the session" },
		{ "kdos doctor", "check the session for common breakage" },
		{ "kdos appid", "do launcher icons match the windows they open?" },
		{ "kdos hey list", "every window from a prompt — also boxes, outputs" },
		{ "kdos restarts", "what is running code an upgrade replaced" },
		{ "kdos update check", "what is behind the signed tree; --json for a UI" },
		{ "kdos update fetch", "the newest signed ports tree, binhost, slot image" },
		{ "kdos update install <port>", "one port and what it lacks: prebuilt, or --source" },
		{ "kdos update notes", "the release notes of the version running" },
		{ "kdos firmware sync", "the firmware this machine's hardware asks for" },
		{ "kdos image unpack", "a signed slot image into a root — also make, verify" },
		{ "kdos backup run", "one restic snapshot from ~/.config/kdos/backup.conf" },
		{ "kdos privacy", "who has the microphone, the camera, the screen" },
		{ "kdos oracle", "one recorded lesson, picked for today" },
		{ "kdos trash <file>", "the desktop's trash, from a prompt — also --restore" },
		{ "kdos verify <file>", "against its .sha256, SHA256SUMS and KDOS signature" },
		{ "kdos places", "Dolphin's Places — also `add DIR`" },
		{ "kdos toggle [name]", "stay-awake, night-light, dnd, reduced-motion" },
		{ "kdos notify <text>", "raise a notification: `make && kdos notify done`" },
		{ "kdos remind in 10m <t>", "a notification later — also at, tomorrow, ls" },
		{ "kdos settings [page]", "System Settings, at a page — appearance, network…" },
		{ "kdos timezone [Zone]", "the clock's zone — also hostname, locale, keymap" },
		{ "kdos clone [<dev>]", "the stick writes the stick — verified by read-back" },
		{ "kdos persist", "keep a live session's changes across a reboot" },
		{ "kdos-shot [region]", "a screenshot with Spectacle, to clipboard and file" },
		{ "kdos-sfx notify", "the machine's noises: login/notify/error/degauss" },
		{ "kdos-mpctl toggle", "mpd and the MPRIS players: toggle, stop, next, prev" },
		{ "kdos-fetch-static", "fetch a single verified static binary" },
		{ "kdos-power suspend", "suspend; also poweroff and reboot (elogind)" },
		{ "kdos-energy", "which app is spending the battery" },
		{ "kdos-doc [page]", "the KDOS book, in the Help Center" },
		{ "sudo kinstall", "install this live image onto a disk" },
		{ NULL, NULL }
	};
	for (int i = 0; CMDS[i][0]; i++)
		fprintf(o, "  %-26s %s\n", CMDS[i][0], CMDS[i][1]);
	fputc('\n', o);
}

/*
 * THE KEYS ARE READ, NOT TYPED. Every chord on this desktop is kglobalaccel's
 * — KDE's defaults, KDOS's layer over them and whatever the person rebound —
 * and keys.c reads the files it reads, so the sheet cannot name a key the
 * keyboard does not have; a table typed here would go on naming a key after
 * a rebind in System Settings had moved it.
 */
static void help_keys(FILE *o)
{
	fprintf(o, "%sKEYS%s  %s(System Settings > Keyboard > Shortcuts; "
		   "Meta+F1 shows the card)%s\n\n",
		C_A, C_0, C_D, C_0);
	if (!kdt_keys_print(o, C_B, C_B, C_0))
		fprintf(o, "  no global shortcut is recorded on this machine "
			   "yet — kglobalaccel writes them\n"
			   "  at the first Plasma login\n");
}

/*
 * Rendered to a buffer and fed to the pager's stdin, rather than written into
 * a popen(). popen() runs `/bin/sh -c`, and this tree's rule is that no C
 * program here spawns a shell — not because this particular string could be
 * attacked (it could not) but because a shell in the tree is a place the next
 * person puts a variable. kdosbuild's `O` key already opens $PAGER by argv;
 * this is the same answer: $PAGER is honoured, and `less` is the fallback when
 * it is unset or empty.
 */
static int cmd_help(int argc, char **argv)
{
	const char *pager = getenv("PAGER");
	int pg = 0, keys = 0;

	if (!pager || !*pager)
		pager = "less";
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--pager"))
			pg = 1;
		else if (!strcmp(argv[i], "--keys"))
			keys = 1;
		else {
			fprintf(stderr, "usage: kdos help [--keys] [--pager]\n");
			return 2;
		}
	}

	if (pg && kb_have_prog(pager)) {
		char *text = NULL;
		size_t len = 0;
		FILE *m = open_memstream(&text, &len);
		if (m) {
			if (!keys)
				help_body(m);
			help_keys(m);
			fclose(m);
			KbArgv a = {0};
			kb_argv_add(&a, pager);
			/* -R only if it IS less: another pager may not have it,
			 * and an unknown flag is an error rather than colour. */
			if (!strcmp(pager, "less"))
				kb_argv_add(&a, "-R");
			kb_argv_end(&a);
			int rc = kb_run_feed_tty(&a, text, len);
			free(text);
			if (rc == 0)
				return 0;
		}
	}
	if (!keys)
		help_body(stdout);
	help_keys(stdout);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* Is this process up? Defined with doctor's other probes below; declared here
 * because `kdos status` names the compositor and asks the same question. */
static int running(const char *exact, const char *contains);
/* Which session this process is in, as one line. One place decides, so
 * `kdos status` and `kdos version` cannot disagree. */
static const char *session_name(void);

/* Non-empty, non-comment lines. The alien-apps table is one line per app. */
static int count_lines(const char *path)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	int n = 0;

	if (!data)
		return 0;
	for (char *p = data; *p;) {
		char *nl = strchr(p, '\n');
		if (*p != '\n' && *p != '#')
			n++;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return n;
}

static int count_dir(const char *path, const char *suffix)
{
	int n = 0, total = 0;
	char **v = kb_listdir(path, &total);
	for (char **p = v; p && *p; p++) {
		/* A dot-name is never an entry: the package database keeps
		 * its sidecars and its lock beside the packages. */
		if ((*p)[0] == '.')
			continue;
		if (!suffix) {
			n++;
			continue;
		}
		size_t a = strlen(*p), b = strlen(suffix);
		if (a >= b && !strcmp(*p + a - b, suffix))
			n++;
	}
	kb_strv_free(v);
	return n;
}

static int count_containers(void)
{
	if (!kb_have_prog("podman"))
		return 0;
	char buf[1 << 15];
	KbArgv a = {0};
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || !buf[0])
		return 0;
	int n = 1;
	for (char *p = buf; *p; p++)
		if (*p == '\n')
			n++;
	return n;
}

static int cmd_status(int argc, char **argv)
{
	char *apps = kdt_data_home("applications");

	if (argc > 0 && !strcmp(argv[0], "--bar")) {
		printf("%d box · %d app\n", count_containers(),
		       count_dir(apps, ".desktop"));
		free(apps);
		return 0;
	}

	struct utsname u;
	uname(&u);
	printf("%sKDOS%s  %s %s\n\n", C_A, C_0, u.sysname, u.release);

	printf("%s%-16s%s %s\n", C_B, "Accent", C_0, kdt_current_accent());
	printf("%s%-16s%s %d\n", C_B, "Packages", C_0,
	       count_dir("/var/lib/kpkg/db", NULL));
	printf("%s%-16s%s %d\n", C_B, "Exported apps", C_0,
	       count_dir(apps, ".desktop"));
	/* The alien apps are a baked table, so this is a file read rather than a
	 * podman call — and it answers on a machine where the box has never been
	 * created, which is every machine before the first launch. */
	printf("%s%-16s%s %d\n", C_B, "Alien apps", C_0,
	       count_lines("/usr/share/kdos/alien-apps"));
	printf("%s%-16s%s %s\n", C_B, "Session", C_0, session_name());
	putchar('\n');

	printf("%sBOXES%s\n", C_A, C_0);
	if (!kb_have_prog("podman")) {
		printf("  podman not installed\n");
	} else {
		char buf[1 << 15];
		KbArgv a = {0};
		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "ps");
		kb_argv_add(&a, "-a");
		kb_argv_add(&a, "--format");
		kb_argv_add(&a, "  {{.Names}}\t{{.Status}}\t{{.Image}}");
		kb_argv_end(&a);
		if (kb_run_capture(&a, buf, sizeof(buf)) == 0 && buf[0])
			printf("%s\n", buf);
		else
			printf("  none — try: kdos app search <word>\n");
	}
	putchar('\n');

	printf("%sEXPORTED APPS%s\n", C_A, C_0);
	int found = 0;
	char **v = kb_listdir(apps, NULL);
	for (char **p = v; p && *p; p++) {
		size_t n = strlen(*p);
		if (n < 9 || strcmp(*p + n - 8, ".desktop"))
			continue;
		found = 1;
		printf("  %.*s\n", (int)(n - 8), *p);
	}
	kb_strv_free(v);
	if (!found)
		printf("  none\n");

	free(apps);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static void ok(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void warn_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Doctor's two funnels, and `--json` is a mode on them rather than a second
 * pass over the same checks. Every check reports through exactly one of these,
 * so the machine-readable output cannot drift from what a human is told.
 */
static int doctor_json;		/* emitting records rather than lines */
static const char *doctor_section = "";
static int doctor_first = 1;
static int doctor_warns;

static void doctor_record(const char *level, const char *fmt, va_list ap)
{
	char msg[512];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	KbBuf b = {0};
	kb_buf_printf(&b, "%s\n    {\"section\": ", doctor_first ? "" : ",");
	kb_json_str(&b, doctor_section);
	kb_buf_str(&b, ", \"level\": ");
	kb_json_str(&b, level);
	kb_buf_str(&b, ", \"message\": ");
	kb_json_str(&b, msg);
	kb_buf_str(&b, "}");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	doctor_first = 0;
}

static void ok(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("ok", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[ ok ]%s ", C_A, C_0);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

static void warn_(const char *fmt, ...)
{
	va_list ap;
	doctor_warns++;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("warn", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[warn]%s ", C_W, C_0);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

/*
 * The third level, and the Hardware section is what needs it. Half of what that
 * section asks cannot be answered in a VM: no SOF-capable audio controller, no
 * Wi-Fi, no NVIDIA GPU, no boot medium. `ok` there would be a green line for
 * something never tested, and `warn` would make every VM look broken. A check
 * that could not run says so and says why.
 */
static void skip_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void skip_(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (doctor_json) {
		doctor_record("skip", fmt, ap);
		va_end(ap);
		return;
	}
	printf("  %s[skip]%s ", C_W, C_0);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

/* A section header in text mode; a field on every record in JSON mode. A
 * machine consumer wants the grouping attached to the item, not emitted as a
 * separate thing it has to remember. */
static void doctor_head(const char *name)
{
	doctor_section = name;
	if (!doctor_json)
		printf("%s%s%s\n", C_B, name, C_0);
}

/* The blank line between sections is text-mode furniture. Left unguarded it
 * lands in the middle of the JSON array — which still parses, and looks like a
 * bug to anyone reading the output. */
static void doctor_gap(void)
{
	if (!doctor_json)
		putchar('\n');
}

/* `grep -q "^<prefix>"` — line-anchored, which matters: a bare substring
 * search would let a user named "os" match the line for "kdos". */
static int has_line_prefix(const char *path, const char *prefix)
{
	size_t n = 0, plen = strlen(prefix);
	char *data = kb_read_all(path, &n);
	if (!data)
		return 0;
	int hit = 0;
	for (char *line = data; line && *line && !hit;) {
		char *nl = strchr(line, '\n');
		if (!strncmp(line, prefix, plen))
			hit = 1;
		line = nl ? nl + 1 : NULL;
	}
	free(data);
	return hit;
}

/*
 * WHICH SESSION THIS IS, from elogind: the login1 session of this process,
 * its type and whether it is the active one on its seat — and, for a Wayland
 * session that is Plasma's, Plasma's version. A shell on tty2 has a session of
 * type tty, and an ssh login none at all (openssh is built without PAM), and
 * saying so is the true answer.
 */
static const char *session_name(void)
{
	static char line[128];
	KdtBus *b;
	KdtMsg *r = NULL;
	const char *obj = NULL;
	char path[256] = "", *type = NULL, *desk = NULL;
	int active = 0;

	if (line[0])
		return line;
	snprintf(line, sizeof(line), "none (no elogind session)");
	b = kdt_bus_open(1);
	if (b && kdt_bus_call(b, "org.freedesktop.login1",
			      "/org/freedesktop/login1",
			      "org.freedesktop.login1.Manager",
			      "GetSessionByPID", &r, NULL, 0, "u",
			      (unsigned)getpid()) == 0 &&
	    kdt_bus_read(b, r, "o", &obj) > 0 && obj)
		kb_strlcpy(path, obj, sizeof(path));
	kdt_bus_free(b, r);
	if (*path) {
		KdtMsg *v = NULL;

		type = kdt_bus_get_prop_s(b, "org.freedesktop.login1", path,
					  "org.freedesktop.login1.Session",
					  "Type", NULL, 0);
		desk = kdt_bus_get_prop_s(b, "org.freedesktop.login1", path,
					  "org.freedesktop.login1.Session",
					  "Desktop", NULL, 0);
		if (kdt_bus_get_prop(b, "org.freedesktop.login1", path,
				     "org.freedesktop.login1.Session",
				     "Active", "b", &v, NULL, 0) == 0)
			kdt_bus_read(b, v, "b", &active);
		kdt_bus_free(b, v);
	}
	kdt_bus_close(b);

	/* A login whose PAM stack was handed no session desktop leaves the
	 * record's Desktop empty; the session's own environment still names
	 * it, and only a wayland session is asked. */
	if (type && !strcmp(type, "wayland") && (!desk || !*desk)) {
		const char *env = getenv("XDG_SESSION_DESKTOP");

		if (!env || !*env)
			env = getenv("XDG_CURRENT_DESKTOP");
		if (env && *env) {
			free(desk);
			desk = kb_strdup(env);
		}
	}

	if (type && !strcmp(type, "wayland") && desk &&
	    (!strcmp(desk, "KDE") || !strncmp(desk, "plasma", 6))) {
		char ver[64] = "";
		KbArgv a = { 0 };

		kb_argv_add(&a, "plasmashell");
		kb_argv_add(&a, "--version");
		kb_argv_end(&a);
		if (kb_run_capture(&a, ver, sizeof(ver)) == 0 &&
		    !strncmp(ver, "plasmashell ", 12))
			snprintf(line, sizeof(line), "Plasma %.32s (Wayland)%s",
				 ver + 12, active ? "" : ", not active");
		else
			snprintf(line, sizeof(line), "Plasma (Wayland)%s",
				 active ? "" : ", not active");
	} else if (type && *type) {
		snprintf(line, sizeof(line), "%.40s%s%.40s%s%s", type,
			 desk && *desk ? " (" : "", desk && *desk ? desk : "",
			 desk && *desk ? ")" : "", active ? "" : ", not active");
	}
	free(type);
	free(desk);
	return line;
}

static int running(const char *exact, const char *contains)
{
	char buf[4096];
	KbArgv a = {0};
	kb_argv_add(&a, "pgrep");
	if (exact) {
		kb_argv_add(&a, "-x");
		kb_argv_add(&a, exact);
	} else {
		kb_argv_add(&a, "-f");
		kb_argv_add(&a, contains);
	}
	kb_argv_end(&a);
	return kb_run_capture(&a, buf, sizeof(buf)) == 0 && buf[0];
}

/* The kernel reports its Landlock ABI through the create syscall itself:
 * a NULL attr with LANDLOCK_CREATE_RULESET_VERSION (1<<0) returns the version
 * rather than a ruleset fd. musl declares __NR_landlock_create_ruleset (444)
 * and needs no wrapper, so this costs nothing to ask.
 *
 * -EOPNOTSUPP means the LSM is compiled in but not enabled in CONFIG_LSM or
 * lsm=, which is the failure worth naming: everything silently degrades to
 * "no sandbox" and nothing says so. */
static int landlock_abi(void)
{
	long v = syscall(__NR_landlock_create_ruleset, NULL, 0, 1UL);
	return v < 0 ? -errno : (int)v;
}

static int blob_has(const char *hay, size_t n, const char *needle)
{
	size_t m = strlen(needle);
	if (m == 0 || m > n)
		return 0;
	for (size_t i = 0; i + m <= n; i++)
		if (hay[i] == needle[0] && !memcmp(hay + i, needle, m))
			return 1;
	return 0;
}

/* The early microcode loader never touches a filesystem: before anything is
 * decompressed it scans the raw initrd for one literal path. So the honest
 * check is that same search against the image this machine actually boots.
 * CONFIG_MICROCODE_LATE_LOADING is off, so a rebuild that lost the ucode cpio
 * has no second chance and no symptom — the CPU just keeps running whatever
 * revision its firmware loaded, which is the state the errata are written
 * about. */
static void check_microcode(void)
{
	char *cpu = kb_read_all("/proc/cpuinfo", NULL);
	const char *want = NULL, *vendor = NULL;
	if (cpu && strstr(cpu, "GenuineIntel")) {
		want = "kernel/x86/microcode/GenuineIntel.bin";
		vendor = "Intel";
	} else if (cpu && strstr(cpu, "AuthenticAMD")) {
		want = "kernel/x86/microcode/AuthenticAMD.bin";
		vendor = "AMD";
	}
	free(cpu);
	if (!want) {
		warn_("unknown CPU vendor — cannot say which microcode applies");
		return;
	}

	char note[96] = "";
	char *v = kb_read_all("/sys/devices/system/cpu/cpu0/microcode/version",
			      NULL);
	if (v) {
		char rev[32];
		kb_strlcpy(rev, v, sizeof(rev));
		rev[strcspn(rev, "\r\n")] = '\0';
		if (rev[0])
			snprintf(note, sizeof(note), " (revision %s)", rev);
		free(v);
	}

	/* KDOS_INITRD moves the image the same way KDOS_PRIVACY_PROC moves the
	 * /proc walk: it is what lets selftest.sh assert both answers on a
	 * machine whose own /boot is not the subject. */
	const char *path = getenv("KDOS_INITRD");
	if (!path || !*path)
		path = "/boot/initramfs.cpio.gz";

	size_t n = 0;
	char *img = kb_read_all(path, &n);
	if (!img) {
		warn_("no %s — cannot tell whether microcode is carried", path);
		return;
	}
	int carried = blob_has(img, n, want);
	free(img);

	if (carried)
		ok("%s microcode in the initrd%s", vendor, note);
	else
		warn_("the initrd carries no %s microcode — this CPU runs "
		      "whatever the firmware loaded%s", vendor, note);
}

/*
 * The shipped image logs in as kdos/kdos and ships sshd enabled, so a machine
 * whose owner never ran passwd is a machine anyone on the network can log
 * into. The shipped hash cannot be compiled in (the source cannot read fs/ at
 * build time, and a copied literal goes stale the day the shadow file
 * changes), so the check IS the question: does "kdos" still hash to what
 * /etc/shadow holds?
 *
 * crypt() is weak so the host selftest — which links kdos-tools without
 * libcrypt — still links; on the target musl's libc carries it. Declared here
 * rather than via <crypt.h>, which a glibc host without libxcrypt lacks.
 */
#pragma weak crypt
extern char *crypt(const char *key, const char *salt);

static void check_default_password(void)
{
	/* Root-only: shadow is unreadable otherwise, and silence is correct —
	 * a non-root doctor cannot answer this and must not pretend to. */
	char *data = kb_read_all("/etc/shadow", NULL);
	if (!data || !crypt) {
		free(data);
		return;
	}

	char hash[128] = "";
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		if (!strncmp(line, "kdos:", 5)) {
			char *end = strchr(line + 5, ':');
			size_t l = end ? (size_t)(end - line - 5)
				       : strcspn(line + 5, "\n");
			if (l < sizeof(hash)) {
				memcpy(hash, line + 5, l);
				hash[l] = 0;
			}
			break;
		}
		line = nl ? nl + 1 : NULL;
	}
	free(data);

	/* No kdos user, or a locked/empty field: not this check's business. */
	if (!hash[0] || hash[0] == '!' || hash[0] == '*')
		return;

	char *h = crypt("kdos", hash);
	if (h && !strcmp(h, hash)) {
		/* Enabled the way rcS decides it: the script executable and
		 * no /etc/service.disabled marker for its name. */
		if (access(INIT_DIR "/70_sshd.sh", X_OK) == 0 &&
		    !kb_path_exists(DISABLED_DIR "/sshd"))
			warn_("the kdos password is still the shipped default "
			      "— and sshd is enabled, so anyone who can reach "
			      "this machine can log in (run: passwd)");
		else
			warn_("the kdos password is still the shipped default "
			      "(run: passwd)");
	} else {
		ok("the kdos password is not the shipped default");
	}
}


/* ------------------------------------------------------------------------
 * The Hardware section.
 *
 * Every check here covers a failure that is SILENT. A machine with no
 * regulatory.db has Wi-Fi; it just has no 5 GHz. A machine with no SOF
 * firmware has an audio device; it just makes no sound. A user outside
 * `dialout` has a /dev/ttyUSB0; they just cannot open it. None of those
 * announces itself, and all three read as broken hardware.
 * ------------------------------------------------------------------------ */

/* Read one small sysfs/proc file, trimmed. Returns NULL if absent. */
static char *hw_slurp(const char *path)
{
	size_t n = 0;
	char *d = kb_read_all(path, &n);
	if (!d)
		return NULL;
	while (n && (d[n - 1] == '\n' || d[n - 1] == ' ' || d[n - 1] == '\t'))
		d[--n] = 0;
	return d;
}

/* Does any directory entry under `dir` start with `pfx`? */
static int hw_dir_has_prefix(const char *dir, const char *pfx)
{
	DIR *d = opendir(dir);
	if (!d)
		return 0;
	struct dirent *e;
	size_t n = strlen(pfx);
	int found = 0;
	while ((e = readdir(d))) {
		if (!strncmp(e->d_name, pfx, n)) {
			found = 1;
			break;
		}
	}
	closedir(d);
	return found;
}

/*
 * The wireless regulatory database.
 *
 * kdos.config leaves CFG80211_CERTIFICATION_ONUS unset, so olddefconfig keeps
 * CONFIG_CFG80211_REQUIRE_SIGNED_REGDB at its default of y and the kernel will
 * load regulatory.db ONLY with a valid regulatory.db.p7s beside it. A database
 * regenerated locally from db.txt fails that check in silence and leaves the
 * radio exactly where it was: country 00, world-roaming, no 5 GHz DFS, reduced
 * TX power. So both files are checked, and the SIGNATURE is not optional.
 */
static void check_regdb(void)
{
	int have_db  = kb_path_exists("/lib/firmware/regulatory.db");
	int have_sig = kb_path_exists("/lib/firmware/regulatory.db.p7s");

	if (!have_db) {
		warn_("no /lib/firmware/regulatory.db — every radio is stuck "
		      "world-roaming: no 5 GHz DFS, reduced TX power");
		return;
	}
	if (!have_sig) {
		warn_("regulatory.db present but regulatory.db.p7s is not — "
		      "REQUIRE_SIGNED_REGDB rejects it silently, so this is "
		      "the same as having none");
		return;
	}

	/*
	 * The files being present is not the same as the kernel having loaded
	 * them. cfg80211 exposes the alpha2 it settled on; "00" is the world
	 * regulatory domain, which is what a failed load looks like.
	 */
	char *a2 = hw_slurp("/sys/module/cfg80211/parameters/ieee80211_regdom");
	if (a2 && *a2 && strcmp(a2, "00"))
		ok("regulatory.db signed and loaded (regdom %s)", a2);
	else if (!hw_dir_has_prefix("/sys/class/ieee80211", "phy"))
		skip_("regulatory.db and signature present; no wireless device "
		      "here to load them");
	else
		warn_("regulatory.db present but the regulatory domain is 00 "
		      "(world) — setting the time zone (kdos timezone "
		      "<Area/City>, or System Settings > Date & Time) writes "
		      "the zone's country to /etc/modprobe.d/kdos-regdom.conf; "
		      "iw reg set <CC> lasts this boot only");
	free(a2);
}

/*
 * Intel SOF audio firmware.
 *
 * CONFIG_SND_SOC_SOF=m binds the driver on every Tiger Lake and newer laptop,
 * and the firmware it then requests is not part of linux-firmware — upstream
 * ships it separately as thesofproject/sof-bin. Missing, it leaves a working
 * audio driver and no sound, which presents as the distro having no audio
 * support rather than as a missing file.
 */
static void check_sof(void)
{
	int sof_bound = hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-tgl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-mtl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-icl", "0000:")
		     || hw_dir_has_prefix("/sys/bus/pci/drivers/sof-audio-pci-intel-skl", "0000:");
	int have_fw  = kb_path_exists("/lib/firmware/intel/sof");
	int have_tpl = kb_path_exists("/lib/firmware/intel/sof-tplg");

	if (!sof_bound) {
		if (have_fw)
			ok("SOF firmware installed (no SOF audio device here)");
		else
			skip_("no SOF audio device on this machine — Intel DSP "
			      "firmware not required");
		return;
	}
	if (have_fw && have_tpl)
		ok("SOF firmware and topologies present");
	else if (have_fw)
		warn_("SOF firmware present but /lib/firmware/intel/sof-tplg "
		      "is missing — the DSP loads and no topology binds, which "
		      "is still silence");
	else
		warn_("an Intel SOF audio device is bound and "
		      "/lib/firmware/intel/sof is absent — this machine has no "
		      "sound (install the sof-firmware package)");
}

/*
 * The GPU firmware. nouveau on Turing and later cannot initialise without the
 * GSP blobs; without them the machine falls back to efifb, KWin falls back to
 * software rendering, and Phosphor mode's kdos_crt effect declines to load
 * there. The blobs are checked directly because the consequence
 * — a desktop that renders but does not look like KDOS — is several steps
 * removed from the cause.
 */
static void check_gpu_firmware(void)
{
	int nouveau = hw_dir_has_prefix("/sys/bus/pci/drivers/nouveau", "0000:");
	int have_gsp = kb_path_exists("/lib/firmware/nvidia");

	if (nouveau && !have_gsp)
		warn_("nouveau is bound and /lib/firmware/nvidia is absent — "
		      "Turing and later need GSP firmware to initialise at "
		      "all, so this falls back to efifb, then to the pixman "
		      "renderer, and the CRT pass declines (sudo kdos "
		      "firmware sync installs firmware-nvidia)");
	else if (nouveau)
		ok("nouveau bound with NVIDIA firmware present");
	else if (have_gsp)
		ok("NVIDIA GSP firmware installed (no nouveau device here)");
	else
		skip_("no NVIDIA GPU on this machine — GSP firmware not "
		      "required");
}

/*
 * DEVICE PRESENT BUT UNOPENABLE is the state this looks for, and the only way
 * to catch a udev rule that has stopped granting: the node exists, `ls -l`
 * looks plausible, and the program that wanted it reports something that
 * sounds like broken hardware.
 *
 * Walk the classes KDOS ships tools for and report each node the CALLING user
 * cannot open, naming the group that owns it — "add yourself to dialout" is an
 * instruction where "permission denied" is not.
 */
static void check_dev_access(void)
{
	static const char *globs[] = {
		"/dev/ttyUSB", "/dev/ttyACM", "/dev/video", "/dev/usbtmc", NULL
	};
	int checked = 0, denied = 0;
	char denied_names[512] = "";

	for (int g = 0; globs[g]; g++) {
		for (int i = 0; i < 8; i++) {
			char path[64];
			snprintf(path, sizeof(path), "%s%d", globs[g], i);
			if (!kb_path_exists(path))
				continue;
			checked++;
			if (access(path, R_OK | W_OK) == 0)
				continue;
			denied++;
			struct stat st;
			const char *grp = "?";
			char gbuf[64];
			if (!stat(path, &st)) {
				struct group *gr = getgrgid(st.st_gid);
				if (gr && gr->gr_name)
					grp = gr->gr_name;
				else {
					snprintf(gbuf, sizeof(gbuf), "gid %u",
						 (unsigned)st.st_gid);
					grp = gbuf;
				}
			}
			size_t used = strlen(denied_names);
			snprintf(denied_names + used, sizeof(denied_names) - used,
				 "%s%s (%s)", used ? ", " : "", path, grp);
		}
	}

	if (!checked)
		skip_("no serial, camera or instrument device plugged in — "
		      "nothing to check reachability against");
	else if (!denied)
		ok("all %d attached device nodes are reachable by this user",
		   checked);
	else
		warn_("%d of %d device nodes cannot be opened: %s — this user "
		      "is not in the owning group",
		      denied, checked, denied_names);
}

/*
 * The boot medium, after switch_root.
 *
 * The initramfs mounts the ISO at /mnt/iso and must MOVE that mount into the
 * new root; left behind it dies with the initramfs namespace and the booted
 * system cannot read the medium it is running from — `kdos rebuild` resolves
 * /mnt/iso/sources under it. On an installed system there is no boot medium
 * and this is not a fault.
 */
/*
 * BOXES AND PACKS. Half of this cannot be answered in a VM — there is often no
 * erofs module and no store — so those lines are `skip` with a reason. A green
 * line for something never tested is worse than an absent one; a `warn` for
 * every VM would make every VM look broken.
 */
/*
 * kdos-packd writes its status reply line by line and ends it with "ok", so
 * one read() returns whatever had arrived — sometimes the reply without its
 * route line. Read until the "ok" line, EOF, a full buffer or a second of
 * silence.
 */
static void packd_reply(int fd, char *buf, size_t cap)
{
	size_t n = 0;

	buf[0] = 0;
	while (n + 1 < cap) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		ssize_t r;

		if (poll(&p, 1, 1000) <= 0)
			break;
		r = read(fd, buf + n, cap - 1 - n);
		if (r <= 0)
			break;
		n += (size_t)r;
		buf[n] = 0;
		if ((n == 3 && !strcmp(buf, "ok\n")) ||
		    (n > 3 && !strcmp(buf + n - 4, "\nok\n")))
			break;
	}
}

static void check_packs(void)
{
	char buf[8192];
	int have_fs = 0;

	/* Can this kernel mount a pack at all? Everything else depends on it,
	 * and 59_packd.sh skips the daemon rather than respawning one that
	 * cannot do its job. */
	if (kb_read_file("/proc/filesystems", buf, sizeof(buf)) >= 0 &&
	    strstr(buf, "erofs"))
		have_fs = 1;
	if (have_fs)
		ok("erofs is available to the kernel");
	else if (kb_path_exists("/sbin/modprobe") || kb_path_exists("/usr/sbin/modprobe"))
		warn_("erofs is not loaded — `modprobe erofs`; without it "
		      "kdos-packd skips itself and no pack can be mounted");
	else
		skip_("no erofs in /proc/filesystems and no modprobe to try — "
		      "this kernel may not have the module at all");

	/* The daemon. Its absence is not a fault on a machine with no packs. */
	{
		struct sockaddr_un a;
		int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		int up = 0;

		if (fd >= 0) {
			memset(&a, 0, sizeof(a));
			a.sun_family = AF_UNIX;
			kb_strlcpy(a.sun_path, "/run/kdos-packd.sock",
				   sizeof(a.sun_path));
			if (connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0) {
				up = 1;
				buf[0] = 0;
				if (write(fd, "status\n", 7) == 7)
					packd_reply(fd, buf, sizeof(buf));
			}
			close(fd);
		}
		if (up) {
			char *route = strstr(buf, "route\t");
			if (route)
				ok("kdos-packd is answering (mount route: %.*s)",
				   (int)strcspn(route + 6, "\n"), route + 6);
			else
				ok("kdos-packd is answering");
		} else if (kb_path_exists("/var/lib/kdos/packs/base.kpack")) {
			warn_("there are packs in /var/lib/kdos/packs and "
			      "kdos-packd is not answering — `service status "
			      "packd`");
		} else {
			skip_("no packs installed and no kdos-packd — this "
			      "machine has no application catalogue");
		}
	}

	/*
	 * THE uid-1000 ASSUMPTION. Packs are built --force-uid=1000, which is
	 * the one human user this distro ships. A second user's boxes still
	 * run — their /usr reads as somebody else — and saying so is better
	 * than leaving it to be discovered.
	 */
	if (getuid() != 0 && getuid() != 1000)
		warn_("you are uid %u and packs are built for uid 1000 — a box "
		      "will run, but its /usr will not read as yours",
		      (unsigned)getuid());

	/* An overlay upper cannot live on overlayfs, and a live session's
	 * $HOME does. The fallback is tmpfs and it is not silent. */
	{
		struct statfs st;
		const char *home = kb_home_dir();

		if (statfs(home, &st) != 0)
			skip_("cannot stat %s to say where a box's writes would "
			      "go", home);
		else if ((unsigned long)st.f_type == 0x794c7630UL)
			warn_("$HOME is on overlayfs (a live session), so a "
			      "persistent box cannot exist — its upper falls "
			      "back to tmpfs and its changes end with the "
			      "session");
		else
			ok("$HOME can hold an overlay upper — a persistent box "
			   "keeps what you write");
	}

	/* A mounted pack whose FILE is gone is a box that works until it is
	 * restarted, which is the worst kind of broken. */
	{
		size_t len = 0;
		char *mounts = kb_read_all("/proc/mounts", &len);
		int missing = 0, mounted = 0;

		if (mounts) {
			char *line, *save;
			for (line = strtok_r(mounts, "\n", &save); line;
			     line = strtok_r(NULL, "\n", &save)) {
				char *sp = strchr(line, ' ');
				char id[128], path[512];
				if (!sp || strncmp(sp + 1,
						   "/var/lib/kdos/packs/mnt/", 24))
					continue;
				mounted++;
				kb_strlcpy(id, sp + 25, sizeof(id));
				id[strcspn(id, " ")] = 0;
				snprintf(path, sizeof(path),
					 "/var/lib/kdos/packs/%s.kpack", id);
				if (!kb_path_exists(path))
					missing++;
			}
			free(mounts);
		}
		if (!mounted)
			skip_("nothing is mounted from the pack store");
		else if (missing)
			warn_("%d of %d mounted pack(s) no longer have a file "
			      "behind them — a box using one works until it is "
			      "restarted", missing, mounted);
		else
			ok("%d pack(s) mounted, every file still present",
			   mounted);
	}
}

static void check_boot_medium(void)
{
	char *mounts = kb_read_all("/proc/mounts", NULL);
	int live = mounts && strstr(mounts, " / overlay ") != NULL;
	int iso  = mounts && strstr(mounts, " /mnt/iso ") != NULL;
	free(mounts);

	if (!live) {
		skip_("installed system — no boot medium to reach");
		return;
	}
	if (iso)
		ok("boot medium reachable at /mnt/iso");
	else
		warn_("live session and /mnt/iso is not mounted — the "
		      "initramfs did not move it across switch_root, so "
		      "nothing on the medium is readable by name");
}

/* ── the session, the services and the boot ──────────────────────────────
 *
 * A NAME ON A BUS IS ASKED, NOT A PROCESS LISTED, wherever there is a name:
 * a running daemon that never took its name serves nobody, and a name a
 * service file can activate is a service that will start when it is first
 * needed. "Owned" and "activatable" are both healthy; neither is the fault.
 */
enum { BUS_ABSENT, BUS_ACTIVATABLE, BUS_OWNED, BUS_UNKNOWN };

static void check_sysupdated(KdtBus *b);

static int bus_state(KdtBus *b, const char *name)
{
	KdtMsg *r = NULL;
	const char *s;
	int found = BUS_ABSENT;

	switch (kdt_bus_has_owner(b, name)) {
	case 1:
		return BUS_OWNED;
	case -1:
		return BUS_UNKNOWN;
	}
	if (kdt_bus_call(b, "org.freedesktop.DBus", "/org/freedesktop/DBus",
			 "org.freedesktop.DBus", "ListActivatableNames", &r,
			 NULL, 0, "") != 0)
		return BUS_UNKNOWN;
	if (kdt_bus_enter(b, r, 'a', "s") > 0) {
		while (kdt_bus_read(b, r, "s", &s) > 0)
			if (!strcmp(s, name))
				found = BUS_ACTIVATABLE;
		kdt_bus_leave(b, r);
	}
	kdt_bus_free(b, r);
	return found;
}

/* One line for one bus name: `what` names the thing, `hint` how to start it
 * or why it matters. */
static void bus_line(KdtBus *b, const char *name, const char *what,
		     const char *hint)
{
	switch (bus_state(b, name)) {
	case BUS_OWNED:
		ok("%s (%s)", what, name);
		break;
	case BUS_ACTIVATABLE:
		ok("%s (%s starts when first asked)", what, name);
		break;
	case BUS_UNKNOWN:
		skip_("%s: the bus did not answer", what);
		break;
	default:
		warn_("%s: nobody owns %s — %s", what, name, hint);
	}
}

/*
 * THE SESSION BUS, AND WHO STARTED IT. turnstile runs the per-user services
 * through ksvc, and the bus is one of them, at $XDG_RUNTIME_DIR/bus: a
 * session that found a bus anywhere else is talking to one turnstile does not
 * supervise, which outlives nothing and restarts nothing.
 */
static void check_session_bus(const char *rt)
{
	const char *addr = getenv("DBUS_SESSION_BUS_ADDRESS");
	char want[512];

	if (kb_path_exists("/run/turnstiled/control.sock"))
		ok("turnstiled is running (/run/turnstiled/control.sock)");
	else
		warn_("turnstiled is not running — no per-user services: no "
		      "bus, no audio (start with: service start turnstiled)");

	if (rt && *rt) {
		snprintf(want, sizeof(want), "unix:path=%s/bus", rt);
		if (addr && !strcmp(addr, want))
			ok("DBUS_SESSION_BUS_ADDRESS=%s", addr);
		else if (addr && *addr)
			warn_("DBUS_SESSION_BUS_ADDRESS=%s, not %s — this "
			      "shell's bus is not the session's", addr, want);
		else
			warn_("DBUS_SESSION_BUS_ADDRESS is not set — expected "
			      "%s", want);
	}

	/* The user table's own verdict, line for line as ksvc prints it. */
	{
		char out[2048] = "";
		KbArgv a = { 0 };

		kb_argv_add(&a, "service");
		kb_argv_add(&a, "--user");
		kb_argv_add(&a, "status");
		kb_argv_add(&a, "dbus");
		kb_argv_end(&a);
		kb_run_capture(&a, out, sizeof(out));
		int up = !strncmp(out, "[ OK ]", 6) || strstr(out, "\n[ OK ]");

		if (up)
			ok("the session bus is a supervised user service");
		else if (geteuid() == 0)
			skip_("root has no user services");
		else
			warn_("`service --user status dbus` does not say [ OK ] — "
			      "the session bus is not supervised");
	}
}

/*
 * elogind's view of this login. The seat, the device access, power-off and
 * suspend, and every polkit `allow_active` grant follow from an ACTIVE
 * session; an ssh login has none at all, because openssh is built without PAM,
 * and that is said rather than warned.
 */
static void check_login1(void)
{
	KdtBus *b = kdt_bus_open(1);
	KdtMsg *r = NULL;
	const char *obj = NULL;

	if (!b) {
		warn_("the system bus could not be reached — no elogind, no "
		      "polkit, no NetworkManager");
		return;
	}
	if (kdt_bus_has_owner(b, "org.freedesktop.login1") != 1) {
		warn_("elogind is not running — no session, no seat (start "
		      "with: service start elogind)");
		kdt_bus_close(b);
		return;
	}
	ok("elogind owns org.freedesktop.login1");
	if (kdt_bus_call(b, "org.freedesktop.login1", "/org/freedesktop/login1",
			 "org.freedesktop.login1.Manager", "GetSessionByPID",
			 &r, NULL, 0, "u", (unsigned)getpid()) != 0 ||
	    kdt_bus_read(b, r, "o", &obj) <= 0 || !obj) {
		skip_("this shell is in no elogind session (an ssh login "
		      "registers none)");
	} else {
		KdtMsg *v = NULL;
		int active = 0;
		char path[256];

		kb_strlcpy(path, obj, sizeof(path));
		if (kdt_bus_get_prop(b, "org.freedesktop.login1", path,
				     "org.freedesktop.login1.Session",
				     "Active", "b", &v, NULL, 0) == 0)
			kdt_bus_read(b, v, "b", &active);
		kdt_bus_free(b, v);
		if (active)
			ok("this session is the active one on its seat");
		else
			warn_("this session is not the active one — devices "
			      "and the allow_active grants belong to another");
	}
	kdt_bus_free(b, r);
	kdt_bus_close(b);
}

/*
 * PLASMA ITSELF. KWin and plasmashell are compared by `comm`, which the
 * kernel cuts at 15 characters (`kwin_wayland_wr` is the wrapper); the portal
 * and the secret service by the name each serves.
 */
static void check_plasma(void)
{
	KdtBus *b = kdt_bus_open(0);

	if (running("kwin_wayland", NULL))
		ok("kwin_wayland running");
	else
		warn_("kwin_wayland not running — no compositor in this "
		      "session");
	if (running("plasmashell", NULL))
		ok("plasmashell running");
	else
		warn_("plasmashell not running — no panel, no desktop (start "
		      "with: kstart plasmashell)");
	if (!b) {
		warn_("the session bus could not be reached");
		return;
	}
	bus_line(b, "org.freedesktop.impl.portal.desktop.kde",
		 "the KDE portal", "file dialogs, screen sharing and "
		 "screenshots from boxed applications fail");
	bus_line(b, "org.kde.ksmserver", "the session manager (ksmserver)",
		 "logout, shutdown and session restore do not work");
	/* A wallet switched off (the live medium's kwalletrc, or the user's
	 * own) leaves the secret service unowned on purpose; warning about it
	 * would fail the doctor on every live session. */
	{
		char *wv = kdt_kcfg_cascade("kwalletrc", "Wallet", "Enabled");
		int wallet = kdt_kcfg_bool(wv, 1);
		free(wv);
		if (wallet)
			bus_line(b, "org.freedesktop.secrets",
				 "the secret service (ksecretd)",
				 "passwords cannot be stored for any "
				 "application");
		else
			skip_("the secret service is switched off (kwalletrc "
			      "[Wallet] Enabled=false) — programs keep secrets "
			      "for this session only");
	}
	if (running("polkit-kde-auth", NULL))
		ok("the polkit agent is running (polkit-kde-authentication-"
		   "agent-1)");
	else
		warn_("no polkit agent — anything that asks for a password "
		      "to act as root is refused without asking");
	kdt_bus_close(b);
}

/* The protected names kdos-oomd uses, flagged where nothing of that name is
 * running and the name only runs at certain times. */
static void check_oomd_names(void)
{
	static const struct { const char *name, *when; } COND[] = {
		{ "sddm-greeter-qt", "the login screen" },
		{ "sddm-helper", "the login screen" },
		{ "kscreenlocker_g", "the lock screen" },
		{ "Xwayland", "an X11 application" },
	};
	char out[4096] = "", *save = NULL;
	KbArgv a = { 0 };
	KbBuf idle = { 0 }, cond = { 0 };
	int n = 0;

	kb_argv_add(&a, "/usr/sbin/kdos-oomd");
	kb_argv_add(&a, "--protected");
	kb_argv_end(&a);
	if (kb_run_capture(&a, out, sizeof(out)) != 0 || !out[0]) {
		skip_("kdos-oomd --protected gave no list");
		return;
	}
	for (char *l = strtok_r(out, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		size_t len = strlen(l);
		int c = -1;

		if (!len)
			continue;
		n++;
		if (l[len - 1] == '*' || running(l, NULL))
			continue;
		for (size_t i = 0; i < sizeof(COND) / sizeof(COND[0]); i++)
			if (!strcmp(l, COND[i].name))
				c = (int)i;
		if (c >= 0)
			kb_buf_printf(&cond, "%s%s (only with %s)",
				      cond.n ? ", " : "", l, COND[c].when);
		else
			kb_buf_printf(&idle, "%s%s", idle.n ? ", " : "", l);
	}
	ok("kdos-oomd protects %d names from the memory-pressure kill", n);
	if (cond.n)
		ok("conditional, not running now: %s", cond.p);
	if (idle.n)
		ok("protected but not running: %s", idle.p);
	kb_buf_free(&cond);
	kb_buf_free(&idle);
}

/*
 * THE MACHINE'S OWN DAEMONS, by the name each serves on the system bus and
 * by the socket kdos-powerd keeps beside it.
 */
static void check_services(void)
{
	KdtBus *b = kdt_bus_open(1);

	/* `service <verb> <name>` keys on the init SCRIPT — ksvc strips the
	 * numeric prefix and the .sh, so 55_powerd.sh is `powerd`. */
	if (kb_path_exists("/run/kdos-powerd.sock"))
		ok("kdos-powerd listening");
	else
		warn_("kdos-powerd not running — the firewall's named services, "
		      "the accent and the splash cannot be changed (start "
		      "with: service start powerd)");
	if (b) {
		if (kdt_bus_has_owner(b, "io.kdos.Power1") == 1)
			ok("kdos-powerd serves io.kdos.Power1");
		else
			warn_("nobody owns io.kdos.Power1 — the Firewall and "
			      "Look settings cannot reach kdos-powerd (start "
			      "with: service start powerd)");
		bus_line(b, "org.freedesktop.timedate1",
			 "the date and time service (kdos-settingsd)",
			 "Date & Time and the login screen's keyboard cannot "
			 "be set (start with: service start settingsd)");
		bus_line(b, "org.freedesktop.UDisks2", "udisks2",
			 "no disk or stick can be mounted from Dolphin");
		if (kb_path_exists("/usr/sbin/tlp-pd")) {
			if (kdt_bus_has_owner(b,
				"org.freedesktop.UPower.PowerProfiles") == 1)
				ok("tlp-pd serves the power profiles");
			else
				warn_("tlp-pd is installed but nobody owns "
				      "org.freedesktop.UPower.PowerProfiles — "
				      "the battery applet has no profiles "
				      "(start with: service start tlppd)");
		}
		check_sysupdated(b);
	} else {
		warn_("the system bus could not be reached");
	}
	kdt_bus_close(b);

	/*
	 * kdos-energyd, and WHY it is not running is the whole of what is worth
	 * reporting. A machine with no RAPL at all — most VMs — is a machine
	 * where per-app energy cannot be measured by anyone, and saying that is
	 * a different answer from "the daemon is not started".
	 */
	if (kb_path_exists("/run/kdos-energyd.sock")) {
		ok("kdos-energyd sampling (kdos-energy)");
	} else {
		int nrapl = 0;
		char **dom = kb_listdir("/sys/class/powercap", &nrapl);
		int any = 0;
		for (int i = 0; i < nrapl; i++) {
			char *e = kb_path_join("/sys/class/powercap", dom[i]);
			char *f = kb_path_join(e, "energy_uj");
			any |= kb_path_exists(f);
			free(f);
			free(e);
		}
		kb_strv_free(dom);
		if (any)
			warn_("kdos-energyd not running — no per-app energy "
			      "attribution (start with: service start "
			      "energyd)");
		else
			skip_("no RAPL energy domain on this machine — per-app "
			      "energy cannot be measured here at all");
	}

	/*
	 * The out-of-memory killer. Without it the boxes — the likeliest
	 * thing on this machine to eat the memory — take the desktop down
	 * with them, and the kernel's own OOM killer picks by badness rather
	 * than by who the user is looking at.
	 */
	if (kb_path_exists("/run/kdos-oomd.sock")) {
		ok("kdos-oomd watching /proc/pressure/memory");
		check_oomd_names();
	} else {
		warn_("kdos-oomd not running — nothing protects the session "
		      "under memory pressure (start with: service start "
		      "oomd)");
	}
}

/* One string field of a one-line JSON document: the value unescaped into
 * `out`, or 0 when the key is absent or null. update.json is written by
 * update.c with kb_json_str, so the escapes to undo are the ones that
 * writes. */
static int json_field(const char *doc, const char *key, char *out, size_t n)
{
	char pat[64];
	const char *p;
	size_t o = 0;

	snprintf(pat, sizeof(pat), "\"%s\":", key);
	p = doc ? strstr(doc, pat) : NULL;
	if (!p)
		return 0;
	p += strlen(pat);
	while (*p == ' ')
		p++;
	if (*p != '"')
		return 0;
	for (p++; *p && *p != '"' && o + 1 < n; p++) {
		if (*p == '\\' && p[1]) {
			p++;
			out[o++] = *p == 'n' ? ' ' : *p == 't' ? ' ' : *p;
			continue;
		}
		out[o++] = *p;
	}
	out[o] = '\0';
	return 1;
}

/*
 * THE UPDATE SERVICE, which Discover and the update tray talk to: both of its
 * names, and an answer on the one Discover pings before it shows anything. Its
 * last survey's problem, when it had one, is the line a person needs.
 */
static void check_sysupdated(KdtBus *b)
{
	int su = kdt_bus_has_owner(b, "org.freedesktop.sysupdate1");
	int sl = kdt_bus_has_owner(b, "io.kdos.Slots1");
	char *doc, problem[256], detail[512];

	if (!kb_path_exists("/usr/sbin/kdos-sysupdated"))
		return;
	if (su == 1 && sl == 1)
		ok("kdos-sysupdated owns org.freedesktop.sysupdate1 and "
		   "io.kdos.Slots1");
	else
		warn_("kdos-sysupdated does not own %s — Discover shows no "
		      "system update (start with: service start sysupdated)",
		      su != 1 ? "org.freedesktop.sysupdate1" : "io.kdos.Slots1");
	if (su == 1) {
		char err[256];

		if (kdt_bus_call(b, "org.freedesktop.sysupdate1",
				 "/org/freedesktop/sysupdate1",
				 "org.freedesktop.DBus.Peer", "Ping", NULL,
				 err, sizeof(err), "") == 0)
			ok("kdos-sysupdated answers");
		else
			warn_("kdos-sysupdated owns its name but does not "
			      "answer: %s", err);
	}
	doc = kb_read_all("/var/lib/kdos/update.json", NULL);
	if (doc && json_field(doc, "problem", problem, sizeof(problem))) {
		if (!json_field(doc, "detail", detail, sizeof(detail)))
			detail[0] = '\0';
		warn_("the last update check: %s%s%s", problem,
		      *detail ? " — " : "", detail);
	} else if (doc) {
		ok("the last update check found no problem");
	}
	free(doc);
}

/*
 * THE BOOT, AS FAR AS rcS AND THE CONFIRMATION GO. rcS writes its mark as
 * its very last line, so a missing one is a boot that wedged in a script;
 * and on a machine with A/B slots a boot whose userland and desktop both
 * marked themselves healthy but that was never confirmed is a mark-good that
 * failed, which rolls the slot back at the next boot.
 */
static void check_boot_chain(void)
{
	char *starts;

	if (kb_path_exists("/run/kdos/bless/userspace"))
		ok("rcS finished");
	else
		warn_("rcS did not finish — a boot script is still running "
		      "or wedged (see /var/log/messages)");
	if (kb_path_exists("/run/kdos/bootstate.json")) {
		if (kb_path_exists("/run/kdos/bless/userspace") &&
		    kb_path_exists("/run/kdos/bless/graphical") &&
		    !kb_path_exists("/run/kdos/bless/confirmed"))
			warn_("this boot answered for itself but was never "
			      "confirmed — the next boot rolls the slot back "
			      "(try: sudo kdos-bootctl mark-good)");
		else if (kb_path_exists("/run/kdos/bless/confirmed"))
			ok("this boot is confirmed");
	}

	/*
	 * kdos-dm records the seconds since boot of each start of the display
	 * manager in the last minute, and gives tty1 to a console login once
	 * sddm has exited three times in it — the rule it keeps, read the way
	 * it reads it, so the two cannot disagree about what happened.
	 */
	starts = kb_read_all("/run/kdos/kdos-dm.starts", NULL);
	if (starts) {
		long t[64], last = -1;
		int n = 0, recent = 0;

		for (char *l = starts, *nl; l && *l && n < 64; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = '\0';
			if (*l && strspn(l, "0123456789") == strlen(l))
				t[n++] = atol(l);
		}
		for (int i = 0; i < n; i++)
			if (t[i] > last)
				last = t[i];
		for (int i = 0; i < n; i++)
			if (last - t[i] < 60)
				recent++;
		if (recent - 1 >= 3) {
			char *log = kb_read_all("/var/log/sddm.log", NULL);

			warn_("display manager: sddm exited %d times; tty1 is a "
			      "console login", recent - 1);
			if (log && !doctor_json) {
				size_t len = strlen(log), i = len;
				int lines = 0;

				if (i && log[i - 1] == '\n')
					i--;
				while (i > 0 && !(log[i - 1] == '\n' &&
						  ++lines == 10))
					i--;
				fputs(log + i, stdout);
				if (len && log[len - 1] != '\n')
					putchar('\n');
			}
			free(log);
		} else {
			ok("sddm is running steadily");
		}
		free(starts);
	}
}

/*
 * THE BOX PLUMBING: the socket proxy every box's Wayland connection goes
 * through, the bus filter, and the box's init; then each running box with
 * the confinement kdos-appbox recorded when it started it — the honest
 * answer to "what can that box reach".
 */
static void check_box_plumbing(void)
{
	static const struct { const char *path, *what; } PARTS[] = {
		{ "/usr/bin/kdos-boxsock", "the Wayland socket each box gets" },
		{ "/usr/libexec/kdos/kdos-boxinit", "a box's own init" },
	};
	char *dir, buf[1 << 14] = "";
	int n = 0;
	char **v;

	for (size_t i = 0; i < sizeof(PARTS) / sizeof(PARTS[0]); i++) {
		if (kb_path_exists(PARTS[i].path))
			ok("%s present (%s)", PARTS[i].path, PARTS[i].what);
		else
			warn_("%s missing — %s cannot be made", PARTS[i].path,
			      PARTS[i].what);
	}
	if (kb_have_prog("xdg-dbus-proxy"))
		ok("xdg-dbus-proxy present (a box's filtered session bus)");
	else
		warn_("xdg-dbus-proxy missing — a box gets no session bus");

	if (kb_have_prog("podman")) {
		KbArgv a = { 0 };

		kb_argv_add(&a, "podman");
		kb_argv_add(&a, "ps");
		kb_argv_add(&a, "--filter");
		kb_argv_add(&a, "status=running");
		kb_argv_add(&a, "--format");
		kb_argv_add(&a, "{{.Names}}");
		kb_argv_end(&a);
		if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
			buf[0] = '\0';
	}
	dir = kb_path_join(kb_runtime_dir(), "kdos/boxes");
	v = kb_listdir(dir, &n);
	for (int i = 0; i < n; i++) {
		size_t l = strlen(v[i]);
		char name[256], *path, *conf;

		if (l < 7 || strcmp(v[i] + l - 6, ".state") || l - 6 >= sizeof(name))
			continue;
		memcpy(name, v[i], l - 6);
		name[l - 6] = '\0';
		/* A state file outlives a box that was killed; only a box
		 * podman says is running is reported. */
		{
			int live = 0;

			for (char *p = buf; *p && !live;) {
				size_t m = strcspn(p, "\n");

				live = m == strlen(name) && !strncmp(p, name, m);
				p += m;
				if (*p)
					p++;
			}
			if (!live)
				continue;
		}
		path = kb_path_join(dir, v[i]);
		conf = kdt_kcfg_get(path, NULL, "confinement");
		ok("box %s: %s", name, conf && *conf ? conf : "confinement "
		   "not recorded");
		free(conf);
		free(path);
	}
	kb_strv_free(v);
	free(dir);
}

/* The accent this login wears, and the global theme it needs. */
static void check_accent(void)
{
	const char *a = kdt_current_accent();
	const char *dirs = getenv("XDG_DATA_DIRS");
	char rel[128], *copy, *save = NULL;
	int found = 0;

	snprintf(rel, sizeof(rel), "plasma/look-and-feel/org.kdos.%s.desktop",
		 a);
	copy = kb_strdup(dirs && *dirs ? dirs : "/usr/local/share:/usr/share");
	for (char *d = strtok_r(copy, ":", &save); d && !found;
	     d = strtok_r(NULL, ":", &save)) {
		char *p = kb_path_join(d, rel);

		found = *d && kb_path_exists(p);
		free(p);
	}
	free(copy);
	if (found)
		ok("accent %s, its global theme installed", a);
	else
		warn_("accent %s has no global theme org.kdos.%s.desktop "
		      "installed — Plasma cannot wear it (reinstall: "
		      "kdos-plasma-look)", a, a);
}


static int cmd_doctor(int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			doctor_json = 1;
		else if (!strcmp(argv[i], "--cve")) {
			/*
			 * Delegated rather than inlined. The CVE answer is a
			 * table of findings with its own exit code and its own
			 * vintage to quote, and folding it into doctor's
			 * ok/warn lines would flatten "17 packages are behind a
			 * recorded fix" into one warning that says nothing.
			 */
			int rest = argc - i - 1;
			return kdt_cve(rest, argv + i + 1, C_A, C_W, C_0);
		} else {
			fprintf(stderr, "usage: kdos doctor [--json] [--cve]\n");
			return 2;
		}
	}

	if (doctor_json)
		printf("{\n  \"checks\": [");
	else
		printf("%sKDOS doctor%s\n\n", C_A, C_0);

	doctor_head("Kernel");
	int abi = landlock_abi();
	if (abi > 0)
		ok("Landlock ABI %d", abi);
	else if (abi == -ENOSYS)
		warn_("no Landlock — kernel too old or CONFIG_SECURITY_LANDLOCK off");
	else
		warn_("Landlock present but disabled — add it to CONFIG_LSM or "
		      "the lsm= cmdline");
	check_microcode();
	doctor_gap();



	doctor_head("Hardware");
	check_regdb();
	check_sof();
	check_gpu_firmware();
	check_dev_access();
	check_boot_medium();
	doctor_gap();

	doctor_head("Boxes");
	check_packs();
	check_box_plumbing();
	doctor_gap();

	doctor_head("Boot");
	check_boot_chain();
	doctor_gap();

	doctor_head("Session");
	/*
	 * The SOCKET, not the variable. A variable that outlived its compositor
	 * is the state this check exists to catch — a session that died leaves
	 * WAYLAND_DISPLAY behind in every shell that inherited it, and a doctor
	 * that reports it green sends the user looking anywhere but here.
	 */
	const char *wd = getenv("WAYLAND_DISPLAY");
	const char *wl_rt = getenv("XDG_RUNTIME_DIR");
	int in_session = 0;

	if (!wd || !*wd) {
		skip_("no WAYLAND_DISPLAY — not inside a Plasma session, so its "
		      "checks are skipped");
	} else {
		char sock[512];
		if (*wd == '/')
			snprintf(sock, sizeof(sock), "%s", wd);
		else if (wl_rt && *wl_rt)
			snprintf(sock, sizeof(sock), "%s/%s", wl_rt, wd);
		else
			snprintf(sock, sizeof(sock), "/run/user/%u/%s",
				 (unsigned)getuid(), wd);
		if (kb_path_exists(sock)) {
			ok("WAYLAND_DISPLAY=%s", wd);
			in_session = 1;
		} else {
			warn_("WAYLAND_DISPLAY=%s but %s does not exist — the "
			      "compositor it names is gone", wd, sock);
		}
	}

	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (rt && *rt && kb_is_dir(rt))
		ok("XDG_RUNTIME_DIR=%s", rt);
	else
		warn_("XDG_RUNTIME_DIR missing — pipewire and podman will "
		      "misbehave");
	check_session_bus(rt);
	check_login1();
	if (in_session)
		check_plasma();
	doctor_gap();

	doctor_head("Services");
	check_services();
	doctor_gap();

	doctor_head("Containers");
	/* A switch_root that chroot()s instead of MS_MOVEing the root leaves
	 * /proc/self/root short of /, so anything that JOINS a mount namespace
	 * lands on an empty tree. */
	char root[256] = {0};
	ssize_t rn = readlink("/proc/self/root", root, sizeof(root) - 1);
	if (rn > 0)
		root[rn] = 0;
	if (rn <= 0 || !strcmp(root, "/"))
		ok("mount namespace root is /");
	else
		warn_("mount namespace root is %s — podman exec and distrobox "
		      "enter will fail", root);

	if (geteuid() != 0) {
		const char *user = getenv("USER");
		char want[80];
		snprintf(want, sizeof(want), "%s:", user && *user ? user : "kdos");
		if (has_line_prefix("/etc/subuid", want))
			ok("subuid mapping present");
		else
			warn_("no /etc/subuid entry — rootless podman cannot map "
			      "users");
		if (has_line_prefix("/etc/subgid", want))
			ok("subgid mapping present");
		else
			warn_("no /etc/subgid entry");
		/* The mappings and the files are only what podman needs; asking
		 * podman itself is what proves its configuration and its store
		 * work for this account. `info` opens the store without running
		 * a container, and its refusal names what is wrong. */
		if (kb_have_prog("podman")) {
			KbArgv a = {0};
			kb_argv_add(&a, "podman");
			kb_argv_add(&a, "info");
			kb_argv_add(&a, "--format");
			kb_argv_add(&a, "{{.Store.GraphRoot}}");
			kb_argv_end(&a);
			char gr[512];
			if (kb_run_capture(&a, gr, sizeof(gr)) == 0 && gr[0]) {
				ok("podman store %s", gr);
			} else {
				char err[512];
				kb_run_feed_env(&a, NULL, 0, "", 0, err,
						sizeof(err));
				err[strcspn(err, "\n")] = 0;
				warn_("podman info fails — %s",
				      err[0] ? err : "no reason given");
			}
		}
	} else {
		warn_("running as root — boxed apps are meant to run as the "
		      "person logged in");
	}
	doctor_gap();

	doctor_head("Desktop");
	check_accent();

	/*
	 * Xwayland is started by KWin on demand, the first time an X11 client
	 * connects, so a session nothing X11 has used yet has no X socket and
	 * that is not a fault. What is checked is that KWin can start one at
	 * all: the binary, and /tmp/.X11-unix root-owned with the sticky bit,
	 * which KWin refuses to create — without it no X11 application opens.
	 */
	const char *disp = getenv("DISPLAY");
	struct stat xst;
	int xdir = stat("/tmp/.X11-unix", &xst) == 0 && S_ISDIR(xst.st_mode) &&
		   xst.st_uid == 0 && (xst.st_mode & 01777) == 01777;
	char xsock[64] = "";

	if (disp && disp[0] == ':')
		snprintf(xsock, sizeof(xsock), "/tmp/.X11-unix/X%.*s",
			 (int)strcspn(disp + 1, "."), disp + 1);
	if (xsock[0] && kb_path_exists(xsock))
		ok("Xwayland on %s (%s)", disp, xsock);
	else if (!kb_have_prog("Xwayland"))
		warn_("no Xwayland — X11-only applications will not start");
	else if (!xdir)
		warn_("/tmp/.X11-unix is missing or not root-owned 1777 — KWin "
		      "cannot start Xwayland, so X11 applications will not open");
	else
		ok("Xwayland installed and /tmp/.X11-unix ready — KWin starts "
		   "it for the first X11 application");

	/*
	 * The lock screen's other half. kscreenlocker checks a password through
	 * the `kde` PAM stack, and pam_unix reads /etc/shadow through the
	 * setuid unix_chkpwd: without the bit every password is refused and
	 * the session locks you OUT — recoverable only from another VT. A
	 * packaging mistake, an rsync without -p or a restore from a tarball
	 * all lose that bit silently, which is why it is checked here.
	 */
	if (kb_path_exists("/etc/pam.d/kde"))
		ok("the lock screen's PAM stack /etc/pam.d/kde is present");
	else
		warn_("/etc/pam.d/kde missing — the lock screen cannot check a "
		      "password");
	{
		static const char *const chk[] = { "/usr/bin/unix_chkpwd",
						   "/usr/sbin/unix_chkpwd",
						   "/sbin/unix_chkpwd", NULL };
		struct stat cst;
		int i;

		for (i = 0; chk[i] && stat(chk[i], &cst) != 0; i++)
			;
		if (!chk[i])
			warn_("unix_chkpwd missing — the lock screen can never "
			      "unlock");
		else if ((cst.st_mode & S_ISUID) && cst.st_uid == 0)
			ok("%s is setuid root", chk[i]);
		else
			warn_("%s is not setuid root — every password at the "
			      "lock screen will be refused (fix: chown root "
			      "and chmod 4755)", chk[i]);
	}

	/*
	 * The bus's activation helper, which needs its GROUP as well as its
	 * bit: dbus-daemon runs as messagebus and may execute the helper only
	 * through the group-execute permission, so a helper that is setuid but
	 * root:root is refused with EACCES on every activation. wpa_supplicant,
	 * fprintd, fwupd and boltd then never start, and the log says only
	 * Spawn.ExecFailed.
	 */
	struct stat hst;
	const struct group *mb = getgrnam("messagebus");
	static const char helper[] = "/usr/lib/dbus/dbus-daemon-launch-helper";
	if (stat(helper, &hst) != 0)
		warn_("%s missing — no D-Bus system service can be activated",
		      helper);
	else if ((hst.st_mode & S_ISUID) && hst.st_uid == 0 &&
		 (hst.st_mode & S_IXGRP) && mb && hst.st_gid == mb->gr_gid)
		ok("dbus-daemon-launch-helper is setuid root, group messagebus");
	else
		warn_("dbus-daemon-launch-helper is not setuid root with group "
		      "messagebus — no D-Bus system service can be activated "
		      "(fix: chown root:messagebus and chmod 4110)");

	/*
	 * AND THE TWO THIS DISTRO DOES NOT OWN, which are the ones every BOX
	 * depends on. podman runs `newuidmap` to write /proc/<pid>/uid_map for
	 * the user namespace a rootless container needs; without the bit it
	 * refuses with "should have setuid or have filecaps setuid" and exits
	 * 125 having printed nothing else, so every boxed application on the
	 * machine stops starting and nothing says why. `/etc/subuid` is
	 * checked with them because an empty one fails at the same call for a
	 * different reason.
	 */
	static const char *const uidmap[] = { "/usr/bin/newuidmap",
					      "/usr/bin/newgidmap", NULL };
	for (int i = 0; uidmap[i]; i++) {
		struct stat ust;
		if (stat(uidmap[i], &ust) != 0)
			warn_("%s missing — no rootless container can map a "
			      "uid, so no box will start", uidmap[i]);
		else if ((ust.st_mode & S_ISUID) && ust.st_uid == 0)
			ok("%s is setuid root", uidmap[i]);
		else
			warn_("%s is not setuid root — podman exits 125 and no "
			      "box starts (fix: chown root and chmod 4755)",
			      uidmap[i]);
	}
	if (kb_path_exists("/etc/subuid") && kb_path_exists("/etc/subgid"))
		ok("/etc/subuid and /etc/subgid are present");
	else
		warn_("/etc/subuid or /etc/subgid is missing — a rootless box "
		      "has no subordinate range to map into");

	/*
	 * The input method. Two halves, and only reporting both distinguishes
	 * "no IME installed" from "installed and not running" — which look
	 * identical from a text field that will not accept CJK.
	 */
	if (kb_have_prog("fcitx5")) {
		if (running("fcitx5", NULL))
			ok("fcitx5 is running — Ctrl+Space switches input method");
		else if (in_session)
			warn_("fcitx5 is installed but not running — no CJK "
			      "input this session (System Settings > Keyboard "
			      "> Virtual Keyboard selects it)");
	}

	const char *path = getenv("PATH");
	char *want = kb_path_join(kb_home_dir(), ".local/bin");
	if (path && strstr(path, want))
		ok("~/.local/bin on PATH");
	else
		warn_("~/.local/bin not on PATH — exported app wrappers will not "
		      "resolve");
	free(want);

	/* Root-only — a normal user cannot read shadow and gets no section at
	 * all rather than a check that pretends it looked. */
	if (access("/etc/shadow", R_OK) == 0) {
		doctor_gap();
		doctor_head("Security");
		check_default_password();
	}

	if (doctor_json)
		printf("\n  ],\n  \"warnings\": %d\n}\n", doctor_warns);
	/*
	 * Exit status carries the verdict in BOTH modes, so a script does not
	 * have to choose between reading the text and parsing the JSON.
	 */
	return doctor_warns ? 1 : 0;
}

static int cmd_version(void)
{
	struct utsname u;
	char rel[128] = "";
	char *os = kb_read_all("/etc/os-release", NULL);

	uname(&u);
	/* VERSION_ID is what the update machinery compares, so it is what is
	 * printed: a second notion of "the version" would be one that drifts. */
	for (char *l = os; l && *l;) {
		char *nl = strchr(l, '\n');

		if (nl)
			*nl = '\0';
		if (!strncmp(l, "VERSION_ID=", 11)) {
			const char *v = l + 11;

			if (*v == '"')
				v++;
			kb_strlcpy(rel, v, sizeof(rel));
			rel[strcspn(rel, "\"")] = '\0';
		}
		l = nl ? nl + 1 : NULL;
	}
	free(os);
	printf("%sKDOS%s — Kool Desktop Operating System\n", C_A, C_0);
	if (*rel)
		printf("  release  %s\n", rel);
	printf("  kernel   %s\n", u.release);
	printf("  libc     musl\n");
	printf("  userland GNU\n");
	printf("  session  %s\n", session_name());
	printf("  accent   %s\n", kdt_current_accent());
	return 0;
}

/*
 * `kdos settings [page]`: System Settings, opened at the page a word names.
 *
 * A FIXED MAP, NOT A MODULE NAME FROM THE CALLER. The words are the ones KDOS
 * has always used for its pages, and each maps to the KCM that now holds that
 * page; a word with no row opens System Settings at its start, which is a
 * better answer to a typo than an error. A KCM id from the command line is
 * still honoured when it is one (`kcm_*`), because that is how a person who
 * found the id elsewhere asks for it.
 */
static const struct {
	const char *page, *kcm;
} SETTINGS[] = {
	{ "appearance", "kcm_lookandfeel" },
	{ "look", "kcm_kdos_look" },
	{ "accent", "kcm_kdos_look" },
	{ "colors", "kcm_colors" },
	{ "fonts", "kcm_fonts" },
	{ "network", "kcm_networkmanagement" },
	{ "wifi", "kcm_networkmanagement" },
	{ "bluetooth", "kcm_bluetooth" },
	{ "sound", "kcm_pulseaudio" },
	{ "audio", "kcm_pulseaudio" },
	{ "display", "kcm_kscreen" },
	{ "screens", "kcm_kscreen" },
	{ "nightlight", "kcm_nightlight" },
	{ "power", "kcm_powerdevilprofilesconfig" },
	{ "keyboard", "kcm_keyboard" },
	{ "shortcuts", "kcm_keys" },
	{ "keys", "kcm_keys" },
	{ "mouse", "kcm_mouse" },
	{ "touchpad", "kcm_touchpad" },
	{ "time", "kcm_clock" },
	{ "date", "kcm_clock" },
	{ "region", "kcm_regionandlang" },
	{ "users", "kcm_users" },
	{ "printers", "kcm_printer_manager" },
	{ "notifications", "kcm_notifications" },
	{ "accessibility", "kcm_access" },
	{ "firewall", "kcm_kdos_firewall" },
	{ "boxes", "kcm_kdos_boxes" },
	{ "health", "kcm_kdos_health" },
	{ "about", "kcm_kdos_health" },
	{ "system", "kcm_kdos_system" },
	{ "updates", "kcm_kdos_system" },
	{ "backup", "kcm_kup" },
	{ "autostart", "kcm_autostart" },
	{ "defaults", "kcm_componentchooser" },
};

static int cmd_settings(int argc, char **argv)
{
	const char *av[3];
	const char *kcm = NULL;
	int n = 0;

	if (argc > 1) {
		fprintf(stderr, "usage: kdos settings [page]\n");
		return 2;
	}
	if (argc == 1 && argv[0][0]) {
		for (size_t i = 0; i < sizeof(SETTINGS) / sizeof(SETTINGS[0]); i++)
			if (!strcmp(argv[0], SETTINGS[i].page))
				kcm = SETTINGS[i].kcm;
		if (!kcm && !strncmp(argv[0], "kcm_", 4) &&
		    strspn(argv[0], "abcdefghijklmnopqrstuvwxyz0123456789_") ==
			    strlen(argv[0]))
			kcm = argv[0];
	}
	av[n++] = "systemsettings";
	if (kcm)
		av[n++] = kcm;
	av[n] = NULL;
	execvp(av[0], (char *const *)av);
	fprintf(stderr, "kdos settings: systemsettings is not installed\n");
	return 127;
}

/*
 * `kdos menu summon <route>` and `kdos menu toggle [<route>]`.
 *
 * A ROUTE IS A NAME FOR A PLACE IN THE SYSTEM, from `/etc/kdos/menu.conf` and
 * the person's copy of it, and this is what a script holds instead of a
 * chord: a chord is rebindable and a menu row moves, and neither is a thing
 * another program can refer to.
 *
 * KRUNNER IS WHAT A ROUTE OPENS INTO, showing the KDOS runner alone with the
 * route as its query, so the one row it answers is the place: Enter goes
 * there. Toggle with no route is KRunner's own toggle.
 */
static int cmd_menu(int argc, char **argv)
{
	const char *verb = argc > 0 ? argv[0] : NULL;
	const char *route = argc > 1 ? argv[1] : NULL;
	char err[256] = "", q[256];
	KdtBus *b;
	int rc;

	if (!verb || argc > 2 ||
	    (strcmp(verb, "summon") && strcmp(verb, "toggle")) ||
	    (!strcmp(verb, "summon") && !route) ||
	    (route && (!*route || strlen(route) > 200))) {
		fprintf(stderr, "usage: kdos menu summon <route>\n"
				"       kdos menu toggle [<route>]\n");
		return 2;
	}
	b = kdt_bus_open(0);
	if (!b) {
		fprintf(stderr, "kdos menu: the session bus could not be "
				"reached\n");
		return 1;
	}
	if (route) {
		snprintf(q, sizeof(q), "route:%s", route);
		rc = kdt_bus_call(b, "org.kde.krunner", "/App",
				  "org.kde.krunner.App", "querySingleRunner",
				  NULL, err, sizeof(err), "ss", "kdos", q);
	} else {
		rc = kdt_bus_call(b, "org.kde.krunner", "/App",
				  "org.kde.krunner.App", "toggleDisplay", NULL,
				  err, sizeof(err), "");
	}
	kdt_bus_close(b);
	if (rc != 0) {
		fprintf(stderr, "kdos menu: KRunner did not answer: %s\n",
			*err ? err : "no reply");
		return 1;
	}
	return 0;
}

/*
 * `kdos panel toggle`: the first panel hides itself or comes back, through
 * plasmashell's own scripting, so the panel's settings dialog shows the
 * same state afterwards.
 */
static int cmd_panel(int argc, char **argv)
{
	char err[256] = "", *out;
	KdtBus *b;

	if (argc != 1 || strcmp(argv[0], "toggle")) {
		fprintf(stderr, "usage: kdos panel toggle\n");
		return 2;
	}
	b = kdt_bus_open(0);
	out = kdt_bus_call_s(b, "org.kde.plasmashell", "/PlasmaShell",
			     "org.kde.PlasmaShell", "evaluateScript", err,
			     sizeof(err), "s",
			     "const p = panels()[0]; "
			     "if (p) p.hiding = (p.hiding === 'autohide') ? "
			     "'none' : 'autohide';");
	kdt_bus_close(b);
	if (!out) {
		fprintf(stderr, "kdos panel: plasmashell did not answer%s%s\n",
			*err ? ": " : "", err);
		return 1;
	}
	free(out);
	return 0;
}

/*
 * ── A NOTIFICATION FROM A PROMPT ────────────────────────────────────────
 *
 *   make && kdos notify "the build finished"
 *
 * A `Notify` on the session bus, sent and answered: a command typed at a
 * prompt can wait the moment it takes, and a person told nothing happened
 * deserves to know the notification server was not there. The fire-and-
 * forget sender is libkbase's kb_notify(), for the callers that must never
 * wait (a chord, a terminal's OSC 9).
 *
 * The notification centre's own verbs — dismiss, raise, the count and the
 * list — are Plasma's applet's now and have no command; asking for one is a
 * usage error, said as one.
 */
static int notify_send(const char *summary, const char *body)
{
	KdtBus *b = kdt_bus_open(0);
	KdtMsg *r = NULL;
	char err[256] = "";
	int rc;

	if (!b) {
		fprintf(stderr, "kdos notify: the session bus could not be "
				"reached\n");
		return 1;
	}
	/* sound-name is the freedesktop sound theme's event: a raw Notify
	 * names no notifyrc event, so without it Plasma plays nothing. Do Not
	 * Disturb and the Notifications page's sound switch still apply. */
	rc = kdt_bus_call(b, "org.freedesktop.Notifications",
			  "/org/freedesktop/Notifications",
			  "org.freedesktop.Notifications", "Notify", &r, err,
			  sizeof(err), "susssasa{sv}i", "kdos", 0u,
			  "dialog-information", summary, body, 0, 1,
			  "sound-name", "s", "message-new-instant", -1);
	kdt_bus_free(b, r);
	kdt_bus_close(b);
	if (rc != 0) {
		fprintf(stderr, "kdos notify: no notification server answered"
				"%s%s\n", *err ? ": " : "", err);
		return 1;
	}
	return 0;
}

/*
 * WHAT TIME IS IT, AS A NOTIFICATION. A chord runs a static command and
 * cannot format a string, which is why these are verbs rather than an
 * argument somebody has to write into a shortcut.
 */
static int notify_time(void)
{
	time_t now = time(NULL);
	struct tm tm;
	char hhmm[16], date[64];

	localtime_r(&now, &tm);
	strftime(hhmm, sizeof(hhmm), "%H:%M", &tm);
	strftime(date, sizeof(date), "%A %e %B %Y", &tm);
	return notify_send(hhmm, date);
}

/*
 * AND HOW MUCH CHARGE IS LEFT, read from the kernel through libkproc — the
 * same reader every other KDOS tool uses, so there is one answer.
 */
static int notify_battery(void)
{
	KprBattery *b = NULL;
	int n = kpr_power_list(&b);
	char sum[64], body[128];
	int mains = 0, found = 0;

	for (int i = 0; i < n; i++)
		if (!b[i].is_battery && b[i].online)
			mains = 1;

	for (int i = 0; i < n; i++) {
		if (!b[i].is_battery)
			continue;
		found = 1;
		snprintf(sum, sizeof(sum), "Battery %d%%", b[i].capacity);
		/* HEALTH IS WEAR AND IS A DIFFERENT NUMBER FROM CHARGE: a
		 * battery reporting 90% can hold 70% of what it held new, and
		 * a person deciding whether to unplug wants both. */
		if (b[i].health > 0)
			snprintf(body, sizeof(body), "%s, %d%% of its "
				 "original capacity",
				 b[i].state[0] ? b[i].state : "unknown",
				 (int)(b[i].health * 100.0 + 0.5));
		else
			snprintf(body, sizeof(body), "%s",
				 b[i].state[0] ? b[i].state : "unknown");
		break;
	}
	if (!found) {
		snprintf(sum, sizeof(sum), "%s",
			 mains ? "On mains" : "No battery");
		snprintf(body, sizeof(body),
			 "this machine reports no battery");
	}
	kpr_power_free(b);
	return notify_send(sum, body);
}

static int cmd_notify(int argc, char **argv)
{
	static const char *const GONE[] = { "--dismiss", "--dismiss-all",
					    "--raise", "--count", "--list" };

	if (argc == 1 && !strcmp(argv[0], "--time"))
		return notify_time();
	if (argc == 1 && !strcmp(argv[0], "--battery"))
		return notify_battery();
	if (argc == 1 && !strcmp(argv[0], "--dnd")) {
		char *av[] = { (char *)"dnd", NULL };

		return cmd_toggle(1, av);
	}
	for (size_t i = 0; argc >= 1 && i < sizeof(GONE) / sizeof(GONE[0]); i++)
		if (!strcmp(argv[0], GONE[i])) {
			fprintf(stderr, "kdos notify: %s is gone — the "
					"notification centre is Plasma's "
					"applet now\n", GONE[i]);
			return 2;
		}
	if (argc < 1 || argc > 2 || argv[0][0] == '-') {
		fprintf(stderr,
			"usage: kdos notify <summary> [body]\n"
			"       kdos notify --time | --battery | --dnd\n");
		return 2;
	}
	return notify_send(argv[0], argc > 1 ? argv[1] : "");
}

int kdos_main(int argc, char **argv)
{
	colours();

	const char *cmd = argc > 1 ? argv[1] : "help";
	int rest = argc - 2;
	char **restv = argv + 2;

	if (rest < 0)
		rest = 0;
	if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return cmd_help(rest, restv);
	if (!strcmp(cmd, "theme"))
		return kdt_theme_main(rest, restv);
	if (!strcmp(cmd, "phosphor"))
		return kdt_phosphor_main(rest, restv);
	if (!strcmp(cmd, "desk"))
		return kdt_desk_main(rest, restv);
	if (!strcmp(cmd, "tiling"))
		return kdt_tiling_main(rest, restv);
	if (!strcmp(cmd, "status"))
		return cmd_status(rest, restv);
	if (!strcmp(cmd, "doctor"))
		return cmd_doctor(rest, restv);
	if (!strcmp(cmd, "toggle"))
		return cmd_toggle(rest, restv);
	if (!strcmp(cmd, "panel"))
		return cmd_panel(rest, restv);
	if (!strcmp(cmd, "notify"))
		return cmd_notify(rest, restv);
	if (!strcmp(cmd, "why"))
		return why_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "explain"))
		return explain_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "sandbox"))
		return sandbox_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "appid"))
		return appid_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "restarts"))
		return restarts_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "march"))
		return march_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "rebuild"))
		return rebuild_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "clone"))
		return clone_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "persist"))
		return persist_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "cve"))
		return kdt_cve(rest, restv, C_A, C_W, C_0);
	if (!strcmp(cmd, "hey"))
		return hey_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "oracle"))
		return oracle_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "update"))
		return kdt_update(rest, restv, kdt_theme_main);
	if (!strcmp(cmd, "image"))
		return kdt_image_main(rest, restv);
	if (!strcmp(cmd, "firmware"))
		return kdt_firmware_main(rest, restv);
	if (!strcmp(cmd, "version") || !strcmp(cmd, "-V"))
		return cmd_version();
	if (!strcmp(cmd, "app"))
		return kdt_app(rest, restv);
	if (!strcmp(cmd, "trash"))
		return kdt_trash(rest, restv);
	if (!strcmp(cmd, "places"))
		return kdt_places(rest, restv);
	if (!strcmp(cmd, "thumb"))
		return kdt_thumb(rest, restv);
	if (!strcmp(cmd, "verify"))
		return kdt_verify_main(rest, restv);
	if (!strcmp(cmd, "speech"))
		return kdt_speech(rest, restv);
	if (!strcmp(cmd, "settings"))
		return cmd_settings(rest, restv);
	if (!strcmp(cmd, "menu"))
		return cmd_menu(rest, restv);
	if (!strcmp(cmd, "backup"))
		return kdt_backup_main(rest, restv);
	if (!strcmp(cmd, "privacy"))
		return kdt_privacy_main(rest, restv);
	if (!strcmp(cmd, "checklist"))
		return kdt_checklist_main(rest, restv);
	if (!strcmp(cmd, "timezone") || !strcmp(cmd, "hostname") ||
	    !strcmp(cmd, "locale") || !strcmp(cmd, "keymap") ||
	    !strcmp(cmd, "in-vm"))
		return kdt_machine_main(cmd, rest, restv);
	/*
	 * `kdos-share` IS THE NAME THE DESKTOP RUNS: Dolphin's Send with a
	 * Code Word action (servicemenus/kdos-share.desktop) and KRunner's
	 * share.file route
	 * both exec it, and this is the same program under the spelling a
	 * person types.
	 */
	if (!strcmp(cmd, "share"))
		return share_main(argc - 1, argv + 1);
	if (!strcmp(cmd, "remind"))
		return remind_main(argc - 1, argv + 1);

	fprintf(stderr, "%skdos:%s unknown command '%s' — try: kdos help\n", C_W,
		C_0, cmd);
	return 1;
}
