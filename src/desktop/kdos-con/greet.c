/* kdos-con — the greeter and the tty1 login. See con.h.
 *
 * IT DRAWS ON THE TTY, NOT ON A KMS DEVICE. kdos-getty has already loaded the
 * console font and the KDOS palette by the time this runs, so the tty backend
 * costs nothing and, more to the point, keeps kdos-con free of libdrm, fcft
 * and pixman. The session that holds every window must come up on a machine
 * whose GPU driver does not; a greeter that did a modeset would have made this
 * binary depend on the one thing the split exists to survive. The modeset is
 * kdos-view's, after the login.
 *
 * THE PRIVILEGED HALF NEVER HANDLES A HASH. On submit this forks, the child
 * drops to the candidate account and execs kdos-checkpass with the typed
 * password on stdin. kdos-checkpass takes no arguments and checks the caller's
 * real uid — after the drop the caller IS the candidate — so nothing here can
 * be aimed at root and no crypt implementation is linked into the greeter.
 */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "con.h"
#include "kbase.h"

#define MAX_USERS 32
#define MAX_PASS  256

typedef struct {
	char name[64];
	char gecos[64];
	char home[128];
	char shell[64];
	uid_t uid;
	gid_t gid;
} Account;

static Account users[MAX_USERS];
static int nusers;

/*
 * Who may log in. uid >= 1000 and a shell that is not a refusal — the same two
 * tests every login screen makes, and the reason `nobody` and the service
 * accounts are not offered.
 */
static void users_load(void)
{
	struct passwd *pw;

	setpwent();
	while (nusers < MAX_USERS && (pw = getpwent()) != NULL) {
		if (pw->pw_uid < 1000 || pw->pw_uid >= 65534)
			continue;
		if (!pw->pw_shell || strstr(pw->pw_shell, "nologin") ||
		    strstr(pw->pw_shell, "/false"))
			continue;

		Account *a = &users[nusers++];

		snprintf(a->name, sizeof(a->name), "%s", pw->pw_name);
		snprintf(a->home, sizeof(a->home), "%s", pw->pw_dir);
		snprintf(a->shell, sizeof(a->shell), "%s", pw->pw_shell);
		a->uid = pw->pw_uid;
		a->gid = pw->pw_gid;

		/* The GECOS field is a comma-separated record and only its
		 * first field is a name; printing the whole thing puts an
		 * office number on the login screen. */
		snprintf(a->gecos, sizeof(a->gecos), "%s",
			 pw->pw_gecos ? pw->pw_gecos : "");
		char *comma = strchr(a->gecos, ',');

		if (comma)
			*comma = '\0';
	}
	endpwent();
}

/*
 * Ask kdos-checkpass, as the candidate. Returns its exit code: 0 correct,
 * 1 wrong, 2 could not tell. The three are kept apart all the way to the
 * message, because reporting "wrong password" for an unreadable /etc/shadow is
 * how a user is locked out of a working account while looking in the wrong
 * place.
 */
static int check_password(const Account *a, const char *pass)
{
	int pipefd[2];

	if (pipe(pipefd) != 0)
		return 2;

	pid_t p = fork();

	if (p < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return 2;
	}

	if (p == 0) {
		close(pipefd[1]);
		if (dup2(pipefd[0], 0) < 0)
			_exit(2);
		close(pipefd[0]);

		/* ORDER IS THE WHOLE SECURITY PROPERTY: groups, then gid,
		 * then uid. setuid first would drop the privilege the other
		 * two need, leaving a process with the user's uid and root's
		 * groups. */
		if (initgroups(a->name, a->gid) != 0)
			_exit(2);
		if (setgid(a->gid) != 0)
			_exit(2);
		if (setuid(a->uid) != 0)
			_exit(2);
		if (setuid(0) == 0)
			_exit(2);	/* the drop did not take */

		execl("/usr/bin/kdos-checkpass", "kdos-checkpass",
		      (char *)NULL);
		_exit(2);
	}

	close(pipefd[0]);
	/* SIGPIPE would kill the greeter if the child died before reading. */
	signal(SIGPIPE, SIG_IGN);
	(void)!write(pipefd[1], pass, strlen(pass));
	close(pipefd[1]);

	int st = 0;

	if (waitpid(p, &st, 0) < 0)
		return 2;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}

/*
 * Become the account and start its session. Does not return: after the drop
 * there is no way back, which is the point.
 */
static void become(const Account *a)
{
	char run[64];

	if (initgroups(a->name, a->gid) != 0 || setgid(a->gid) != 0 ||
	    setuid(a->uid) != 0 || setuid(0) == 0) {
		fprintf(stderr, "kdos-con-login: cannot become %s\n", a->name);
		_exit(1);
	}

	/* A LOGIN ENVIRONMENT, not an inherited one. init's environment has
	 * root's HOME, and a session started with it writes the user's
	 * configuration into /root — where the user cannot read it back. */
	snprintf(run, sizeof(run), "/run/user/%u", (unsigned)a->uid);
	clearenv();
	setenv("HOME", a->home, 1);
	setenv("USER", a->name, 1);
	setenv("LOGNAME", a->name, 1);
	setenv("SHELL", a->shell, 1);
	setenv("PATH", "/usr/local/bin:/usr/local/sbin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
	setenv("XDG_RUNTIME_DIR", run, 1);
	setenv("XDG_SESSION_TYPE", "tty", 1);
	setenv("XDG_CURRENT_DESKTOP", "KDOS", 1);
	setenv("TERM", "xterm-256color", 1);
	/* The same reason as the terminal's own child: libktui reads this to
	 * decide whether the palette is truecolour, and the session's is. */
	setenv("COLORTERM", "truecolor", 1);
	/* And which terminal, the third tier of a picture program's probe. The
	 * greeter's shell is the same engine as every other window here. */
	setenv("TERM_PROGRAM", "kdos-term", 1);
	setenv("TERM_PROGRAM_VERSION", KVT_TERM_VERSION, 1);

	if (chdir(a->home) != 0 && chdir("/") != 0)
		_exit(1);

	execl("/usr/local/bin/kdos-con-start", "kdos-con-start",
	      (char *)NULL);
	fprintf(stderr, "kdos-con-login: cannot start the session\n");
	_exit(127);
}

/*
 * The login surface. One card, centred, and nothing else on the screen: the
 * only two answers it wants are which account and what the password is.
 */
static void greet_draw(int sel, const char *pass, const char *msg)
{
	int w, h;

	ktui_backend()->size(&w, &h);
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);

	int cw = 46;
	int ch = 8 + (nusers > 1 ? nusers : 0);
	int cx = (w - cw) / 2, cy = (h - ch) / 2;

	if (cx < 0)
		cx = 0;
	if (cy < 0)
		cy = 0;

	KRect card = krect(cx, cy, cw, ch);

	ktui_draw_shadow(card);
	ktui_draw_fill(card, KT_SURFACE);
	ktui_draw_box(card, "KDOS", KT_ACCENT, KT_SURFACE, /* dbl */ 1);

	int y = cy + 2;

	if (nusers > 1) {
		for (int i = 0; i < nusers; i++, y++) {
			int on = i == sel;

			ktui_draw_textf(cx + 3, y, cw - 6,
					on ? KT_ACCENT : KT_MID,
					on ? KT_DIM : KT_SURFACE, 0,
					"%s%s", on ? "> " : "  ",
					users[i].gecos[0] ? users[i].gecos
							  : users[i].name);
		}
		y++;
	} else if (nusers == 1) {
		ktui_draw_text(cx + 3, y, cw - 6,
			       users[0].gecos[0] ? users[0].gecos
						 : users[0].name,
			       KT_MID, KT_SURFACE, 0);
		y += 2;
	}

	/* MASKED, AND THE MASK IS THE LENGTH. Showing nothing at all leaves a
	 * user unable to tell a dead keyboard from a working one. */
	char stars[41];
	size_t n = strlen(pass);

	if (n > 40)
		n = 40;
	memset(stars, '*', n);
	stars[n] = '\0';

	ktui_draw_text(cx + 3, y, 10, "Password:", KT_MID, KT_SURFACE, 0);
	ktui_draw_fill(krect(cx + 13, y, cw - 16, 1), KT_DIM);
	ktui_draw_text(cx + 13, y, cw - 16, stars, KT_TEXT, KT_DIM, 0);
	y += 2;

	if (msg && *msg)
		ktui_draw_text(cx + 3, y, cw - 6, msg, KT_WARN, KT_SURFACE, 0);

	ktui_draw_flush();
}

/*
 * `greet = yes`. Returns only on a failure that leaves the machine without a
 * session — every success path execs.
 */
static int greeter(void)
{
	char pass[MAX_PASS] = "";
	const char *msg = "";
	int sel = 0;

	users_load();
	if (!nusers) {
		fprintf(stderr,
			"kdos-con-login: no account with a uid of 1000 or\n"
			"                more and a real shell. Nothing to\n"
			"                log in to; use tty2.\n");
		return 1;
	}

	if (ktui_draw_init() != 0) {
		fprintf(stderr, "kdos-con-login: cannot draw on this tty\n");
		return 1;
	}

	for (;;) {
		KtuiEvent ev;

		greet_draw(sel, pass, msg);
		if (ktui_backend()->poll_event(&ev, 1000) <= 0)
			continue;
		if (ev.type != KT_EVT_KEY)
			continue;

		size_t n = strlen(pass);

		switch (ev.key) {
		case KT_K_UP:
			if (sel > 0)
				sel--;
			pass[0] = '\0';
			msg = "";
			continue;
		case KT_K_DOWN:
			if (sel + 1 < nusers)
				sel++;
			pass[0] = '\0';
			msg = "";
			continue;
		case KT_K_BACKSPACE:
			if (n)
				pass[n - 1] = '\0';
			continue;
		case KT_K_ESC:
			pass[0] = '\0';
			msg = "";
			continue;
		case KT_K_ENTER:
			break;
		default:
			if (ev.key >= 32 && ev.key < 127 && n + 1 < sizeof(pass)) {
				pass[n] = (char)ev.key;
				pass[n + 1] = '\0';
			}
			continue;
		}

		switch (check_password(&users[sel], pass)) {
		case 0:
			/* Leave the tty as it was found: the session is about
			 * to draw on it, and a greeter that kept the alternate
			 * screen would hand over one it does not own. */
			ktui_draw_clear();
			ktui_draw_flush();
			become(&users[sel]);
			return 1;	/* become() does not return */
		case 1:
			msg = "Wrong password.";
			break;
		default:
			/* NOT "wrong password". kdos-checkpass could not tell,
			 * which is a broken /etc/shadow or a missing entry —
			 * a different problem in a different place. */
			msg = "Cannot check this account (see tty2).";
			break;
		}
		memset(pass, 0, sizeof(pass));
	}
}

/*
 * `kdos-con-login TTY`, from kdos-getty in /etc/inittab.
 *
 * `greet = no` hands the tty to agetty --autologin, which is the live medium's
 * answer: a machine with one account and no password has nothing to ask, and
 * going through agetty keeps utmp, lastlog and the shell's own profile on the
 * path they are on everywhere else. `greet = yes` asks.
 */
int con_login(const char *tty)
{
	if (!kcon_conf_bool("greet", 0)) {
		const char *who = kcon_conf_str("autologin", "kdos");

		execl("/sbin/agetty", "agetty", "--autologin", who,
		      "--noclear", tty, "38400", "linux", (char *)NULL);
		fprintf(stderr, "kdos-con-login: no agetty (%s)\n",
			strerror(errno));
		return 127;
	}
	return greeter();
}
