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

/* `KbUser` is the shared shape: this greeter and `kdos-users` must not give two
 * answers to who may log in. */
typedef KbUser Account;

static Account users[MAX_USERS];
static int nusers;

/* Who may log in — `kb_users()` is the one place that decides. */
static void users_load(void)
{
	nusers = kb_users(users, MAX_USERS);
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
/*
 * THE SESSIONS THIS MACHINE HAS, and the console is first because it is the
 * one that always works: it needs no GPU driver, no compositor and no seat,
 * which is the whole argument for this desktop existing. A row is offered only
 * if the program behind it is installed, so a machine built without kdos-comp
 * shows no choice rather than a choice that fails.
 *
 * XDG_SESSION_TYPE IS PART OF THE CHOICE, NOT A CONSTANT. It is set to
 * `wayland` on the graphical path by /etc/profile.d/10-wayland.sh — which a
 * LOGIN SHELL reads and this greeter never does, because it clears the
 * environment and execs the session directly. Hard-coding `tty` here and then
 * starting a compositor would hand every bus-activated service a session type
 * that is a lie, since kdos-desktop-start pushes this very variable into the
 * activation environment.
 */
static const struct {
	const char *name;
	const char *prog;
	const char *type;
} SESSIONS[] = {
	{ "Console", "/usr/local/bin/kdos-con-start", "tty" },
	{ "Desktop", "/usr/local/bin/kdos-desktop", "wayland" },
};
#define NSESSIONS ((int)(sizeof(SESSIONS) / sizeof(SESSIONS[0])))

static int sessions[NSESSIONS];		/* indices into SESSIONS, installed */
static int nsessions;

static void sessions_load(void)
{
	nsessions = 0;
	for (int i = 0; i < NSESSIONS; i++)
		if (access(SESSIONS[i].prog, X_OK) == 0)
			sessions[nsessions++] = i;
}

static void become(const Account *a, int ses)
{
	const char *prog = SESSIONS[0].prog, *type = SESSIONS[0].type;
	char run[64];

	if (ses >= 0 && ses < nsessions) {
		prog = SESSIONS[sessions[ses]].prog;
		type = SESSIONS[sessions[ses]].type;
	}

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
	setenv("XDG_SESSION_TYPE", type, 1);
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

	execl(prog, prog, (char *)NULL);
	fprintf(stderr, "kdos-con-login: cannot start the session\n");
	_exit(127);
}

/*
 * The login surface. One card, centred, and nothing else on the screen: the
 * only two answers it wants are which account and what the password is.
 */
/*
 * THE ROW IS PUSHED THROUGH THE CONTRACT, never written by hand — a surface
 * that drew its own bottom line would be one whose verbs stopped following the
 * widget. Nothing is dispatched through it: this surface's Esc clears the
 * password rather than closing anything, and there is no page and no menu
 * behind a login screen.
 */
static KtuiKeys gkeys;

static void greet_draw(int sel, int ses, const char *pass, const char *msg)
{
	int w, h;

	ktui_backend()->size(&w, &h);
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);

	int cw = 46;
	/* A ROW FOR THE MESSAGE, ALWAYS. It is transient and the card is not:
	 * a height that grew when something went wrong would move the card
	 * under the hand, and one that did not reserve the row wrote "Wrong
	 * password." across the bottom border. */
	int ch = 9 + (nusers > 1 ? nusers : 0) + (nsessions > 1 ? 2 : 0);
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

	/* DRAWN ONLY WHEN THERE IS A CHOICE. One installed session is not a
	 * question, and a row offering the only answer is a row that teaches
	 * the arrows do nothing. */
	if (nsessions > 1) {
		ktui_draw_text(cx + 3, y, 10, "Session:", KT_MID, KT_SURFACE, 0);
		ktui_draw_textf(cx + 13, y, cw - 16, KT_TEXT, KT_SURFACE, 0,
				"< %s >", SESSIONS[sessions[ses]].name);
		y += 2;
	}

	if (msg && *msg)
		ktui_draw_text(cx + 3, cy + ch - 2, cw - 6, msg, KT_WARN,
			       KT_SURFACE, 0);

	/*
	 * THE ROW THAT NAMES THE KEYS, on the login surface as on every other.
	 * The arrows are the half nobody would guess: which account and which
	 * session are both arrow keys, and a card that showed two names and
	 * no way to move between them teaches that they are a label.
	 *
	 * Esc says `clear` and not `close`: there is nothing behind this
	 * surface to go back to, and the verb has to be the one it does.
	 */
	/* ACROSS THE SCREEN AND NOT INSIDE THE CARD. Four hints need sixty
	 * columns and the card is forty-six: a row that had to fit inside it
	 * would drop the two nobody would guess. */
	ktui_hint_if(nusers > 1, "Up/Down", "account");
	ktui_hint_if(nsessions > 1, "Left/Right", "session");
	ktui_hint("Enter", "log in");
	ktui_hint("Esc", "clear");
	ktui_hint_row(&gkeys, krect(2, h - 2, w - 4, 1), KT_BG);

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
	int sel = 0, ses = 0;

	users_load();
	sessions_load();
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

		greet_draw(sel, ses, pass, msg);
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
		case KT_K_LEFT:
			/* The password is NOT cleared: changing which session
			 * to start is not changing who is starting it, and a
			 * typed password thrown away by an arrow key is a
			 * greeter that punishes a second thought. */
			if (nsessions > 1)
				ses = (ses + nsessions - 1) % nsessions;
			continue;
		case KT_K_RIGHT:
			if (nsessions > 1)
				ses = (ses + 1) % nsessions;
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
			become(&users[sel], ses);
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
 * ONE FRAME OF THE GREETER, OFFSCREEN, FROM A FIXTURE.
 *
 * WHY A FIXTURE AND NOT THE MACHINE. `kb_users()` reads /etc/passwd and
 * `sessions_load()` stats two programs, so a dump of the real thing says
 * whatever the machine it ran on happens to hold — which is a golden that
 * changes when somebody adds an account. The file names the accounts and the
 * sessions, so the frame is the same everywhere.
 *
 * IT IS READ ONLY HERE. This path composites and returns; it never reaches
 * `become()` and never asks for a password, so a variable naming accounts
 * cannot move a login. Every other entry into this file ignores it.
 *
 * THE ASCII TIER, like every other golden in `testing/goldens/`. A dump is a
 * comparison of text and the tier a dump renders in is the harness's, not the
 * screen's: on the real tty the same layout is drawn in the vt tier, because
 * kdos-getty has already loaded the 512-glyph console font by the time the
 * greeter runs.
 */
static void greet_fixture(const char *path)
{
	char *text = kb_read_all(path, NULL);

	nusers = 0;
	nsessions = 0;
	if (!text)
		return;
	for (char *p = text; *p;) {
		char *nl = strchr(p, '\n');
		char line[256];
		size_t len = nl ? (size_t)(nl - p) : strlen(p);

		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, p, len);
		line[len] = '\0';
		p = nl ? nl + 1 : p + strlen(p);

		if (!strncmp(line, "user ", 5) && nusers < MAX_USERS) {
			Account *a = &users[nusers++];
			char *sp = strchr(line + 5, ' ');

			memset(a, 0, sizeof(*a));
			if (sp)
				*sp++ = '\0';
			kb_strlcpy(a->name, line + 5, sizeof(a->name));
			kb_strlcpy(a->gecos, sp ? sp : "", sizeof(a->gecos));
			a->uid = 1000 + (uid_t)nusers;
			a->gid = a->uid;
		} else if (!strncmp(line, "session ", 8) &&
			   nsessions < NSESSIONS) {
			/* The INDEX into SESSIONS[], by name, because the row
			 * draws that table's label and not the file's. */
			for (int i = 0; i < NSESSIONS; i++)
				if (!strcmp(SESSIONS[i].name, line + 8))
					sessions[nsessions++] = i;
		}
	}
	free(text);
}

int con_greet_dump(int cols, int rows, const char *fixture, const char *msg)
{
	greet_fixture(fixture);
	if (!nusers) {
		fprintf(stderr, "kdos-con: %s named no accounts\n",
			fixture ? fixture : "(no fixture)");
		return 2;
	}
	if (ktui_offscreen_init(cols, rows) != 0)
		return 1;
	ktui_draw_init();
	greet_draw(0, 0, "hunter2", msg ? msg : "");
	ktui_draw_dump();
	return 0;
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
