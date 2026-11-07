/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   bless.c — a boot is confirmed by two marks, not by one
 *
 * A SLOT IS CONFIRMED ONLY WHEN BOTH MARKS EXIST, tested together:
 *
 *   /run/kdos/bless/userspace   the end of rcS: every service started
 *   /run/kdos/bless/graphical   a Plasma session answered for a minute
 *                               (io.kdos.Slots1.Bless, from kdos-notice
 *                               --bless), or the login greeter stayed up
 *                               for a minute on a machine nobody logs in to
 *
 * and then this daemon runs `kdos-bootctl mark-good`, once per boot, writing
 * /run/kdos/bless/confirmed after it succeeds. Either mark alone confirms
 * nothing: a userland that boots and a desktop that crashes is the failure a
 * trial exists to catch, and the attempt counter rolls it back only if
 * nothing confirmed it. A machine whose inittab starts no display manager
 * has no graphical half to wait for, and the first mark is enough there.
 *
 * THE MARKS ARE ROOT'S FILES IN A ROOT DIRECTORY. Nothing under a user's
 * runtime directory is read, and a mark that is not a regular file owned by
 * root is no mark: a session reaches the second mark only through Bless,
 * which polkit grants to the active local session and to nobody else. The
 * greeter's half counts only processes of SDDM's own account, `sddm`.
 *
 * THE GREETER COUNTS ONLY ON A MACHINE NOBODY LOGS IN TO. With SDDM set to
 * log a user in, or once any user session has existed this boot, the
 * session's own Bless is the confirmation: a greeter that came back because
 * the session crashed must not confirm the slot that crashed it.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sysupdated.h"

#define BLESS_DIR    "/run/kdos/bless"
#define SESSIONS_DIR "/run/systemd/sessions"

static int ifd = -1, wd_bless = -1, wd_sessions = -1;
static time_t boot_reloaded;

static void mark_path(const char *name, char *out, size_t cap)
{
	char rel[128];
	snprintf(rel, sizeof(rel), BLESS_DIR "/%s", name);
	su_path(rel, out, cap);
}

/* A mark is a regular file root owns — this daemon's own uid under
 * --session, where nothing runs as root. */
static int mark_ok(const char *name)
{
	char p[1024];
	struct stat st;
	mark_path(name, p, sizeof(p));
	if (lstat(p, &st) != 0 || !S_ISREG(st.st_mode))
		return 0;
	return st.st_uid == 0 || (su.session && st.st_uid == geteuid());
}

int bless_has(const char *mark)
{
	return mark_ok(mark);
}

static int write_mark(const char *name)
{
	char p[1024], line[32];
	mark_path(name, p, sizeof(p));
	int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
		      0644);
	if (fd < 0)
		return errno == EEXIST && mark_ok(name) ? 0 : -1;
	int n = snprintf(line, sizeof(line), "%lld\n", (long long)time(NULL));
	ssize_t w = write(fd, line, (size_t)n);
	close(fd);
	return w == n ? 0 : -1;
}

/* The basename of a command's first word. */
static const char *cmd_base(const char *cmd, char *out, size_t cap)
{
	size_t n = strcspn(cmd, " \t");
	if (n >= cap)
		n = cap - 1;
	memcpy(out, cmd, n);
	out[n] = 0;
	const char *s = strrchr(out, '/');
	return s ? s + 1 : out;
}

/*
 * Does this machine start a display manager? An inittab entry whose process
 * is kdos-dm or sddm, not switched `off`, and an executable /usr/bin/sddm —
 * kdos-dm starts a console login instead when sddm is missing, and a console
 * login has no graphical half to wait for.
 */
static int need_graphical(void)
{
	char p[1024];
	int found = 0;

	su_path("/usr/bin/sddm", p, sizeof(p));
	if (access(p, X_OK) != 0)
		return 0;
	su_path("/etc/inittab", p, sizeof(p));
	char *data = kb_read_all(p, NULL);
	for (char *l = data; l && *l && !found;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l && *l != '#') {
			/* id:runlevels:action:process */
			char *f[4] = { l, NULL, NULL, NULL };
			for (int i = 1; i < 4; i++) {
				char *c = f[i - 1] ? strchr(f[i - 1], ':') : NULL;
				if (!c)
					break;
				*c = 0;
				f[i] = c + 1;
			}
			if (f[3] && strcmp(f[2], "off")) {
				char word[512];
				const char *b = cmd_base(f[3], word, sizeof(word));
				found = !strcmp(b, "kdos-dm") || !strcmp(b, "sddm");
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return found;
}

/* The [Autologin] User= of one SDDM configuration file, into `user` when
 * the file sets one; a later file overrides an earlier one, as in SDDM. */
static void sddm_autologin_file(const char *path, char *user, size_t cap)
{
	char *data = kb_read_all(path, NULL);
	int in = 0;

	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l == '[')
			in = !strncmp(l, "[Autologin]", 11);
		else if (in && !strncmp(l, "User", 4)) {
			char *v = l + 4;
			while (*v == ' ' || *v == '\t')
				v++;
			if (*v == '=') {
				v++;
				while (*v == ' ' || *v == '\t')
					v++;
				v[strcspn(v, " \t\r")] = 0;
				kb_strlcpy(user, v, cap);
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
}

static void sddm_autologin_dir(const char *rel, char *user, size_t cap)
{
	char d[1024];
	int n = 0;
	su_path(rel, d, sizeof(d));
	char **names = kb_listdir(d, &n);
	/* kb_listdir sorts, which is the order SDDM reads a directory in. */
	for (int i = 0; names && i < n; i++) {
		size_t l = strlen(names[i]);
		if (l < 6 || strcmp(names[i] + l - 5, ".conf"))
			continue;
		char *p = kb_path_join(d, names[i]);
		sddm_autologin_file(p, user, cap);
		free(p);
	}
	kb_strv_free(names);
}

static int autologin_configured(void)
{
	char user[64] = "", p[1024];
	sddm_autologin_dir("/usr/lib/sddm/sddm.conf.d", user, sizeof(user));
	sddm_autologin_dir("/etc/sddm.conf.d", user, sizeof(user));
	su_path("/etc/sddm.conf", p, sizeof(p));
	sddm_autologin_file(p, user, sizeof(user));
	return user[0] != 0;
}

/* Has a user session existed this boot? elogind keeps one file per session
 * under /run/systemd/sessions, with its CLASS. */
static void scan_sessions(void)
{
	char d[1024];
	int n = 0;

	if (su.user_session_seen)
		return;
	su_path(SESSIONS_DIR, d, sizeof(d));
	char **names = kb_listdir(d, &n);
	for (int i = 0; names && i < n && !su.user_session_seen; i++) {
		if (names[i][0] == '.')
			continue;
		char *p = kb_path_join(d, names[i]);
		char *data = kb_read_all(p, NULL);
		for (char *l = data; l && *l;) {
			char *nl = strchr(l, '\n');
			if (nl)
				*nl = 0;
			if (!strcmp(l, "CLASS=user")) {
				su.user_session_seen = 1;
				su_log("a user session exists: the greeter no "
				       "longer confirms this boot");
				break;
			}
			l = nl ? nl + 1 : NULL;
		}
		free(data);
		free(p);
	}
	kb_strv_free(names);
}

static void watch_sessions(void)
{
	char d[1024];
	if (ifd < 0 || wd_sessions >= 0)
		return;
	su_path(SESSIONS_DIR, d, sizeof(d));
	wd_sessions = inotify_add_watch(ifd, d, IN_CREATE | IN_MOVED_TO);
}

void bless_setup(void)
{
	char d[1024];
	ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	su_path(BLESS_DIR, d, sizeof(d));
	if (ifd >= 0)
		wd_bless = inotify_add_watch(ifd, d, IN_CREATE | IN_MOVED_TO |
						      IN_CLOSE_WRITE | IN_ATTRIB);
	watch_sessions();
	scan_sessions();
	su.blessed = mark_ok("confirmed");
	boot_reloaded = time(NULL);
}

int bless_inotify_fd(void)
{
	return ifd;
}

void bless_on_inotify(void)
{
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	int bless = 0, sessions = 0;
	for (;;) {
		ssize_t n = read(ifd, buf, sizeof(buf));
		if (n <= 0)
			break;
		for (char *p = buf; p < buf + n;) {
			struct inotify_event *e = (struct inotify_event *)p;
			if (e->wd == wd_bless)
				bless = 1;
			else if (e->wd == wd_sessions)
				sessions = 1;
			p += sizeof(*e) + e->len;
		}
	}
	if (sessions)
		scan_sessions();
	if (bless)
		bless_evaluate();
}

/*
 * Both marks, or the one this machine has, then mark-good. A mark-good that
 * fails — an ESP it could not write — is tried again at the next mark or
 * after half a minute, and the boot stays unconfirmed until it succeeds,
 * which is the safe way for it to fail: the attempt counter then rolls back.
 */
void bless_evaluate(void)
{
	time_t now = time(NULL);

	if (su.blessed || !mark_ok("userspace"))
		return;
	if (need_graphical() && !mark_ok("graphical"))
		return;
	if (su.markgood_tried && now - su.markgood_tried < 30)
		return;
	su.markgood_tried = now;

	const char *argv[] = { "kdos-bootctl", "mark-good", NULL };
	KbBuf out = {0};
	int rc = run_sync(argv, &out, NULL);
	if (out.p && out.p[0])
		su_log("mark-good: %.*s", (int)strcspn(out.p, "\n"), out.p);
	kb_buf_free(&out);
	if (rc != 0) {
		su_log("mark-good failed (status %d); this boot is not "
		       "confirmed yet", rc);
		return;
	}
	write_mark("confirmed");
	su.blessed = 1;
	su_log("both marks present: this boot is confirmed");
	state_reload_boot();
	state_emit();
}

/* io.kdos.Slots1.Bless, after polkit said yes. */
int bless_graphical(void)
{
	if (write_mark("graphical") != 0) {
		su_log("cannot write the graphical mark");
		return -1;
	}
	bless_evaluate();
	return 0;
}

/* The uid of SDDM's greeter account, `sddm` in <root>/etc/passwd, or -1.
 * Read from the file and not through getpwnam() so --root is honoured. */
static uid_t greeter_account(void)
{
	char p[1024];
	uid_t uid = (uid_t)-1;
	su_path("/etc/passwd", p, sizeof(p));
	char *pw = kb_read_all(p, NULL);
	if (!pw)
		return uid;
	for (char *line = pw; line && *line;) {
		char *nl = strchr(line, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(line, "sddm:", 5)) {
			const char *f = strchr(line + 5, ':');
			char *end;
			if (f && f[1] >= '0' && f[1] <= '9') {
				unsigned long v = strtoul(f + 1, &end, 10);
				if (*end == ':')
					uid = (uid_t)v;
			}
			break;
		}
		line = nl ? nl + 1 : NULL;
	}
	free(pw);
	return uid;
}

/*
 * The greeter and its compositor: an `sddm-greeter` and a `kwin_wayland`
 * both running as SDDM's greeter account. The names alone prove nothing,
 * since any local account can start two processes called that; the account
 * is one only SDDM runs as, so a greeter of anyone else's earns no mark.
 */
static void find_greeter(pid_t *greeter, pid_t *kwin)
{
	DIR *d;
	struct dirent *de;
	uid_t guid = greeter_account();
	pid_t kw[64];
	int nk = 0;

	*greeter = *kwin = 0;
	if (guid == (uid_t)-1 || !(d = opendir(su.proc)))
		return;
	while ((de = readdir(d))) {
		char p[1024], word[256];
		if (de->d_name[0] < '1' || de->d_name[0] > '9')
			continue;
		snprintf(p, sizeof(p), "%s/%s/cmdline", su.proc, de->d_name);
		char *cmd = kb_read_all(p, NULL);
		if (!cmd)
			continue;
		const char *b = cmd_base(cmd, word, sizeof(word));
		int is_greeter = !strncmp(b, "sddm-greeter", 12);
		int is_kwin = !strcmp(b, "kwin_wayland");
		free(cmd);
		if (!is_greeter && !is_kwin)
			continue;
		snprintf(p, sizeof(p), "%s/%s/status", su.proc, de->d_name);
		char *status = kb_read_all(p, NULL);
		const char *u = status ? strstr(status, "\nUid:") : NULL;
		uid_t uid = u ? (uid_t)strtoul(u + 5, NULL, 10) : (uid_t)-1;
		free(status);
		pid_t pid = (pid_t)atoi(de->d_name);
		if (uid != guid)
			continue;
		if (is_greeter && !*greeter)
			*greeter = pid;
		else if (is_kwin && nk < 64)
			kw[nk++] = pid;
	}
	closedir(d);
	if (*greeter && nk)
		*kwin = kw[0];
	else
		*greeter = 0;
}

static void greeter_watch(void)
{
	pid_t g, k;
	time_t now = time(NULL);

	find_greeter(&g, &k);
	if (!g) {
		su.greeter_pid = su.greeter_kwin = 0;
		su.greeter_since = 0;
		return;
	}
	if (g != su.greeter_pid || k != su.greeter_kwin) {
		su.greeter_pid = g;
		su.greeter_kwin = k;
		su.greeter_since = now;
		return;
	}
	if (now - su.greeter_since >= su.bless_after) {
		su_log("the greeter (%d) and its kwin_wayland (%d) stayed up "
		       "%d s: writing the graphical mark", (int)g, (int)k,
		       su.bless_after);
		if (write_mark("graphical") == 0)
			bless_evaluate();
	}
}

void bless_tick(void)
{
	time_t now = time(NULL);

	watch_sessions();
	scan_sessions();
	if (!su.blessed) {
		if (need_graphical() && !mark_ok("graphical") &&
		    !su.user_session_seen && !autologin_configured())
			greeter_watch();
		if (su.markgood_tried)
			bless_evaluate();
	}
	/* The boot state can change under this daemon — `kdos update` or
	 * `kdos-bootctl` typed at a shell — and the Slots1 properties follow
	 * it within half a minute. */
	if (now - boot_reloaded >= 30) {
		boot_reloaded = now;
		state_reload_boot();
		state_emit();
	}
}
