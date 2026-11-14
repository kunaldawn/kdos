/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   facts.c — what the system says, asked the way a person would ask it
 *
 *   kdos restarts --json       processes still running replaced code
 *   kdos cve --json            installed packages with known CVEs
 *   kdos-bootctl status --json the slots, the trial, the failed slot; read
 *                              from /run/kdos/bootstate.json when this user
 *                              cannot open the state file itself
 *   /run/kdos-oomd.sock        `status`: how many processes kdos-oomd has
 *                              killed since it started, and the last
 *   /var/lib/kdos/apps-pending the applications the installer was asked
 *                              for and could not install; a file, because
 *                              it is the installer's record and Discover
 *                              reads the same one ($KDOS_APPS_PENDING
 *                              moves it for both)
 *   kdos firmware sync --dry-run --json
 *                              the vendor firmware ports this machine's
 *                              devices and kernel log ask for and it has
 *                              not got
 *
 * THE COMMANDS ARE THE INTERFACE. Each of them already answers in JSON for
 * the System Versions page, Discover's backend and `kdos doctor`; reading
 * their files directly here would be a second parser of each format to keep
 * in step. They are run by name through $PATH, which is also how the tests
 * put stubs in front of them.
 *
 * A COMMAND THAT DOES NOT ANSWER CHANGES NOTHING. `ok` stays 0 and every
 * decision in decide.c treats that as "no news", never as "the problem went
 * away": a vanished secdb must not withdraw a CVE notice.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "kxdg.h"
#include "notice.h"

/* ── parsing ───────────────────────────────────────────────────────────── */

int kn_parse_restarts(const char *json, KnRestarts *out)
{
	const char *procs;

	memset(out, 0, sizeof(*out));
	procs = json_get(json, "processes");
	if (!procs || *json_ws(procs) != '[')
		return 0;
	for (const char *e = json_first(procs); e && out->n < KN_PROCS;
	     e = json_next(e)) {
		KnProc *p = &out->p[out->n];

		p->pid = (int)json_int(json_get(e, "pid"), 0);
		json_str(json_get(e, "comm"), p->comm, sizeof(p->comm));
		json_str(json_get(e, "exe"), p->exe, sizeof(p->exe));
		if (p->pid > 0 && (p->comm[0] || p->exe[0]))
			out->n++;
	}
	out->ok = 1;
	return 1;
}

int kn_parse_cve(const char *json, KnCve *out)
{
	const char *f;

	memset(out, 0, sizeof(*out));
	f = json_get(json, "findings");
	if (!f || *json_ws(f) != '[')
		return 0;
	for (const char *e = json_first(f); e && out->n < KN_FINDINGS;
	     e = json_next(e)) {
		KnFinding *k = &out->f[out->n];

		json_str(json_get(e, "port"), k->port, sizeof(k->port));
		json_str(json_get(e, "version"), k->version, sizeof(k->version));
		json_str(json_get(e, "fixed_in"), k->fixed, sizeof(k->fixed));
		json_str(json_get(e, "cves"), k->cves, sizeof(k->cves));
		if (!k->port[0])
			continue;
		/* Each comma-separated id is one vulnerability; an entry with
		 * none recorded still counts as one known problem. */
		{
			int ids = 0;

			for (char *c = k->cves; *c;) {
				while (*c == ',' || *c == ' ')
					c++;
				if (!*c)
					break;
				ids++;
				while (*c && *c != ',')
					c++;
			}
			out->ncve += ids ? ids : 1;
		}
		out->n++;
	}
	out->ok = 1;
	return 1;
}

static void slot_field(const char *json, const char *key, char *out, size_t n)
{
	const char *v = json_get(json, key);

	out[0] = '\0';
	if (v && !json_is_null(v))
		json_str(v, out, n);
	/* A slot is "a" or "b" and nothing else. */
	if (strcmp(out, "a") && strcmp(out, "b"))
		out[0] = '\0';
}

int kn_parse_boot(const char *json, KnBoot *out)
{
	const char *slots, *conf;

	memset(out, 0, sizeof(*out));
	conf = json_get(json, "configured");
	if (!conf || strncmp(json_ws(conf), "true", 4))
		return 0;
	slot_field(json, "running", out->running, sizeof(out->running));
	slot_field(json, "active", out->active, sizeof(out->active));
	slot_field(json, "trying", out->trying, sizeof(out->trying));
	slot_field(json, "failed", out->failed, sizeof(out->failed));
	slots = json_get(json, "slots");
	for (int i = 0; i < 2 && slots; i++) {
		const char *s = json_get(slots, i ? "b" : "a");
		const char *v;

		if (!s)
			continue;
		if ((v = json_get(s, "version")) && !json_is_null(v))
			json_str(v, out->ver[i], sizeof(out->ver[i]));
		if ((v = json_get(s, "build")) && !json_is_null(v))
			json_str(v, out->build[i], sizeof(out->build[i]));
	}
	out->ok = 1;
	return 1;
}

/* `ok trigger '…', kills 3, last: firefox-esr (appbox www) (pid 9, 812 MB)` */
int kn_parse_oom(const char *line, KnOom *out)
{
	const char *k, *l;

	memset(out, 0, sizeof(*out));
	if (strncmp(line, "ok ", 3) || !(k = strstr(line, ", kills ")))
		return 0;
	out->kills = atoi(k + 8);
	if ((l = strstr(k, ", last: "))) {
		kb_strlcpy(out->last, l + 8, sizeof(out->last));
		out->last[strcspn(out->last, "\r\n")] = '\0';
	}
	out->ok = 1;
	return 1;
}

int kn_parse_firmware(const char *json, KnFirmware *out)
{
	const char *ports;

	memset(out, 0, sizeof(*out));
	ports = json_get(json, "ports");
	if (!ports || *json_ws(ports) != '[')
		return 0;
	for (const char *e = json_first(ports); e && out->n < KN_FW_PORTS;
	     e = json_next(e)) {
		json_str(json_get(e, "port"), out->port[out->n],
			 sizeof(out->port[0]));
		json_str(json_get(e, "reason"), out->reason[out->n],
			 sizeof(out->reason[0]));
		if (out->port[out->n][0])
			out->n++;
	}
	out->ok = 1;
	return 1;
}

/* ── asking ────────────────────────────────────────────────────────────── */

/* The command's standard output, whatever its exit status: `kdos restarts`
 * and `kdos cve` exit 1 when they found something, which is the answer. */
static char *capture(const char *const *argv)
{
	KbArgv a = { 0 };
	KbBuf out = { 0 };
	int st;

	for (int i = 0; argv[i]; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);
	st = kb_run_capture_buf(&a, &out);
	if (st < 0 || st >= 126) {
		kb_buf_free(&out);
		return NULL;
	}
	return out.p;
}

void kn_read_restarts(KnRestarts *out)
{
	static const char *const argv[] = { "kdos", "restarts", "--json", NULL };
	char *j = capture(argv);

	if (!j || !kn_parse_restarts(j, out))
		memset(out, 0, sizeof(*out));
	free(j);
	if (out->ok)
		kn_name_procs(out);
}

void kn_read_cve(KnCve *out)
{
	static const char *const argv[] = { "kdos", "cve", "--json", NULL };
	char *j = capture(argv);

	if (!j || !kn_parse_cve(j, out))
		memset(out, 0, sizeof(*out));
	free(j);
}

void kn_read_boot(KnBoot *out)
{
	static const char *const argv[] = { "kdos-bootctl", "status", "--json",
					    NULL };
	char *j = capture(argv);

	if (!j || !kn_parse_boot(j, out))
		memset(out, 0, sizeof(*out));
	free(j);
}

/* `kdos firmware sync --dry-run` exits 1 when it names a port, which is
 * the answer; capture() takes any status a program can return. */
void kn_read_firmware(KnFirmware *out)
{
	static const char *const argv[] = { "kdos", "firmware", "sync",
					    "--dry-run", "--json", NULL };
	char *j = capture(argv);

	if (!j || !kn_parse_firmware(j, out))
		memset(out, 0, sizeof(*out));
	free(j);
}

void kn_read_oom(KnOom *out)
{
	const char *path = getenv("KDOS_OOMD_SOCK");
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	struct timeval tv = { .tv_sec = 2 };
	char buf[512];
	size_t got = 0;
	int fd;

	memset(out, 0, sizeof(*out));
	if (!path || !*path)
		path = "/run/kdos-oomd.sock";
	if (strlen(path) >= sizeof(sa.sun_path))
		return;
	strcpy(sa.sun_path, path);
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 ||
	    send(fd, "status\n", 7, MSG_NOSIGNAL) != 7) {
		close(fd);
		return;
	}
	while (got + 1 < sizeof(buf)) {
		ssize_t r = read(fd, buf + got, sizeof(buf) - 1 - got);

		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	close(fd);
	buf[got] = '\0';
	kn_parse_oom(buf, out);
}

void kn_boot_id(char *out, size_t n)
{
	const char *fixed = getenv("KDOS_NOTICE_BOOT_ID");

	if (fixed && *fixed) {
		kb_strlcpy(out, fixed, n);
		return;
	}
	if (kb_read_line_file("/proc/sys/kernel/random/boot_id", out, n) < 0)
		kb_strlcpy(out, "-", n);
}

/* ── naming ────────────────────────────────────────────────────────────── */

/*
 * THE SESSION'S OWN PROGRAMS, by process or executable name. A process here
 * running replaced code is fixed by logging out and in, not by restarting an
 * application, so the notice says that instead. A name cut at 15 characters
 * is how the kernel reports it, and is listed in that form.
 */
static const char *const session_progs[] = {
	"kwin_wayland", "kwin_wayland_wr", "kwin_wayland_wrapper", "Xwayland",
	"plasmashell", "ksmserver", "kded6", "kglobalacceld", "krunner",
	"ksecretd", "kwalletd6", "kactivitymanagerd", "kactivitymanage",
	"plasma_session", "startplasma-wayland", "startplasma-way",
	"xdg-desktop-portal", "xdg-desktop-portal-kde", "xdg-desktop-por",
	"polkit-kde-authentication-agent-1", "polkit-kde-auth",
	"org_kde_powerdevil", "org_kde_powerde", "kaccess", "baloo_file",
	"pipewire", "pipewire-pulse", "wireplumber", "dbus-daemon",
	"dbus-broker", "kdos-notice", "fcitx5", NULL,
};

static int is_session(const KnProc *p)
{
	const char *base = kb_basename(p->exe);

	for (int i = 0; session_progs[i]; i++)
		if (!strcmp(p->comm, session_progs[i]) ||
		    (*base && !strcmp(base, session_progs[i])))
			return 1;
	return 0;
}

typedef struct {
	char exec[128];		/* basename of Exec's program, or TryExec's */
	char wmclass[128];
	char name[128];
} AppRow;

static int data_dirs(char dirs[][512], int max)
{
	const char *x = getenv("XDG_DATA_HOME"), *d = getenv("XDG_DATA_DIRS");
	char buf[2048];
	int n = 0;

	if (x && *x == '/')
		kb_strlcpy(dirs[n++], x, 512);
	else
		snprintf(dirs[n++], 512, "%s/.local/share", kb_home_dir());
	kb_strlcpy(buf, d && *d ? d : "/usr/local/share:/usr/share", sizeof(buf));
	for (char *s = buf, *e; s && *s && n < max; s = e) {
		e = strchr(s, ':');
		if (e)
			*e++ = '\0';
		if (*s == '/')
			kb_strlcpy(dirs[n++], s, 512);
	}
	return n;
}

/* The program an Exec line runs, as a basename; `env A=B prog` is prog. */
static void exec_program(const char *exec, char *out, size_t n)
{
	char buf[512], *save = NULL;

	out[0] = '\0';
	kb_strlcpy(buf, exec, sizeof(buf));
	for (char *t = strtok_r(buf, " \t", &save); t;
	     t = strtok_r(NULL, " \t", &save)) {
		if (!strcmp(t, "env") || strchr(t, '='))
			continue;
		if (*t == '"')
			t++;
		t[strcspn(t, "\"")] = '\0';
		kb_strlcpy(out, kb_basename(t), n);
		return;
	}
}

static AppRow *apps_index(int *count)
{
	char dirs[17][512];
	int nd = data_dirs(dirs, 17), n = 0;
	AppRow *v = NULL;

	for (int i = 0; i < nd; i++) {
		char dir[600];
		char **names;

		if (snprintf(dir, sizeof(dir), "%s/applications", dirs[i]) >=
		    (int)sizeof(dir))
			continue;
		names = kb_listdir(dir, NULL);
		for (char **f = names; f && *f && n < 4096; f++) {
			char path[1200];
			KxdgEntry e = { 0 };
			AppRow r = { 0 };
			size_t l = strlen(*f);

			if (l < 9 || strcmp(*f + l - 8, ".desktop"))
				continue;
			if (snprintf(path, sizeof(path), "%s/%s", dir, *f) >=
			    (int)sizeof(path) ||
			    kxdg_load(&e, path, "Desktop Entry") != 0)
				continue;
			if (!kxdg_bool(&e, "Hidden", 0)) {
				const char *te = kxdg_get(&e, "TryExec", "");

				exec_program(*te ? te : kxdg_get(&e, "Exec", ""),
					     r.exec, sizeof(r.exec));
				kb_strlcpy(r.wmclass, kxdg_get(&e, "StartupWMClass", ""),
					   sizeof(r.wmclass));
				kb_strlcpy(r.name, kxdg_get(&e, "Name", ""),
					   sizeof(r.name));
				if (r.exec[0] && r.name[0]) {
					v = kb_realloc(v, (size_t)(n + 1) * sizeof(*v));
					v[n++] = r;
				}
			}
			kxdg_free(&e);
		}
		kb_strv_free(names);
	}
	*count = n;
	return v;
}

void kn_name_procs(KnRestarts *r)
{
	int n = 0;
	AppRow *apps = NULL;

	for (int i = 0; i < r->n; i++) {
		KnProc *p = &r->p[i];
		const char *base = kb_basename(p->exe);

		p->desktop = is_session(p);
		if (p->desktop)
			continue;
		if (!apps)
			apps = apps_index(&n);
		for (int k = 0; k < n && !p->name[0]; k++)
			if ((*base && !strcmp(apps[k].exec, base)) ||
			    !strcmp(apps[k].exec, p->comm) ||
			    (apps[k].wmclass[0] &&
			     !strcasecmp(apps[k].wmclass, p->comm)))
				kb_strlcpy(p->name, apps[k].name, sizeof(p->name));
	}
	free(apps);
}

void kn_read_pending(KnPending *out)
{
	const char *p = getenv("KDOS_APPS_PENDING");
	char *text = kb_read_whole(p && *p ? p : "/var/lib/kdos/apps-pending",
				   NULL);

	kn_parse_pending(text, out);
	free(text);
}
