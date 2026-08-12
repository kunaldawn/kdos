/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-lock — the lock screen
 *
 *          ╔══════════════════════════════════════╗
 *          ║  KDOS                        21:04   ║
 *          ║                                      ║
 *          ║  kdos@kdos                           ║
 *          ║  password  ●●●●●●●●▏                 ║
 *          ║                                      ║
 *          ║  Enter to unlock                     ║
 *          ╚══════════════════════════════════════╝
 *
 * An ext-session-lock-v1 client, and the protocol is the whole design: the
 * COMPOSITOR owns the locked state, so this process dying leaves the screen
 * locked rather than open. That inverts the usual risk — the dangerous bug in a
 * lock screen is not "it crashed", it is "it unlocked". There is exactly one
 * call that unlocks anything (`kwl_unlock`) and exactly one thing that reaches
 * it: kdos-checkpass answering 0.
 *
 * FOUR RULES IT IS BUILT AROUND:
 *
 *   - NO PASSWORD IS ACCEPTED BEFORE THE COMPOSITOR CONFIRMS THE LOCK. Until
 *     `locked` arrives the session may still be on screen; a prompt that took
 *     input then would be collecting a password from a machine that is not yet
 *     secured. The field refuses keys and says so.
 *   - THE PASSWORD NEVER REACHES argv. It is piped to kdos-checkpass on stdin
 *     (kb_run_feed), because /proc/<pid>/cmdline is world-readable.
 *   - THE BUFFER IS WIPED after every attempt, on success and on failure, and
 *     the length is capped so a held-down key cannot grow it without bound.
 *   - THERE IS NO "SWITCH USER", NO POWER MENU AND NO NOTIFICATION AREA. Every
 *     one of those is a way to reach something else from a locked screen, and
 *     each has been a CVE somewhere. Suspend and poweroff live behind
 *     kdos-powerd, which a locked session has no reason to reach.
 *
 * It draws with libktui through libkwl, so it is the same cell grid as the
 * panel and the installer, in the same eight colours.
 */

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "ktui.h"
#include "kwl.h"

#define LK_MAX_PASS 256
#define LK_BOX_W    46
#define LK_BOX_H    9

/* Long enough that a script cannot try a dictionary through this UI, short
 * enough that a mistyped password is not a punishment. kdos-checkpass sleeps a
 * second of its own, which is the delay an attacker cannot skip. */
#define LK_FAIL_MS  1200

static char pass[LK_MAX_PASS];
static int npass;
static const char *status;
static int failed;

static void pass_clear(void)
{
	/* Not just npass = 0: the bytes are a password and this process stays
	 * alive across attempts. */
	memset(pass, 0, sizeof(pass));
	npass = 0;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void draw(const char *user, const char *host)
{
	int w = ktui_w, h = ktui_h;
	if (w < 20 || h < 6)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);

	KRect box = krect((w - LK_BOX_W) / 2, (h - LK_BOX_H) / 2, LK_BOX_W,
			  LK_BOX_H);
	if (box.x < 0 || box.y < 0)
		box = krect(0, 0, w, h);

	ktui_draw_fill(box, KT_SURFACE);
	ktui_draw_box(box, NULL, failed ? KT_ERR : KT_ACCENT, KT_SURFACE, 1);

	char clock[16];
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(clock, sizeof(clock), "%H:%M", &tm);

	ktui_draw_text(box.x + 2, box.y + 1, box.w - 4, "KDOS", KT_ACCENT,
		       KT_SURFACE, KT_A_NONE);
	/* Ends two cells short of the right border: text_right takes an origin
	 * and a width, not an end column. */
	ktui_draw_text_right(box.x, box.y + 1, box.w - 2, clock, KT_DIM,
			     KT_SURFACE, KT_A_NONE);

	char who[80];
	snprintf(who, sizeof(who), "%s@%s", user, host);
	ktui_draw_text(box.x + 2, box.y + 3, box.w - 4, who, KT_TEXT,
		       KT_SURFACE, KT_A_NONE);

	/*
	 * A dot per character and a caret at the end. NOT a fixed-width mask:
	 * seeing the length change is the feedback that tells you the keyboard
	 * is reaching the prompt at all, and a lock screen that looks dead is
	 * one people reboot.
	 */
	char dots[LK_BOX_W];
	int room = box.w - 14;
	if (room < 4)
		room = 4;
	int shown = npass < room ? npass : room;
	int k = 0;
	for (int i = 0; i < shown && k + 3 < (int)sizeof(dots); i++) {
		/* U+25CF BLACK CIRCLE — in the console font's charset as well
		 * as in any desktop font, unlike the usual bullet operators. */
		dots[k++] = '\xe2';
		dots[k++] = '\x97';
		dots[k++] = '\x8f';
	}
	dots[k] = '\0';

	ktui_draw_text(box.x + 2, box.y + 4, 10, "password", KT_DIM, KT_SURFACE,
		       KT_A_NONE);
	ktui_draw_text(box.x + 12, box.y + 4, box.w - 14, dots, KT_ACCENT,
		       KT_SURFACE, KT_A_NONE);
	if (shown < room)
		ktui_draw_text(box.x + 12 + shown, box.y + 4, 2,
			       ktui_glyph[KT_G_FULL], KT_ACCENT, KT_SURFACE,
			       KT_A_NONE);

	ktui_draw_text(box.x + 2, box.y + 6, box.w - 4,
		       status ? status : "Enter to unlock",
		       failed ? KT_ERR : KT_DIM, KT_SURFACE, KT_A_NONE);

	ktui_draw_flush();
}

/* ── authentication ────────────────────────────────────────────────────── */

/*
 * The single point at which this program can unlock anything.
 *
 * kdos-checkpass distinguishes wrong (1) from cannot-tell (2), and so does
 * this: "wrong password" for a machine with an unreadable /etc/shadow sends the
 * user looking in the wrong place, and they cannot read the daemon's stderr
 * from behind a lock screen.
 */
static int try_unlock(void)
{
	KbArgv a = {0};
	kb_argv_add(&a, "kdos-checkpass");
	kb_argv_end(&a);

	int rc = kb_run_feed(&a, pass, (size_t)npass);
	pass_clear();

	if (rc == 0) {
		kwl_unlock();
		return 1;
	}
	failed = 1;
	status = rc == 1 ? "Wrong password" : "Cannot check the password";
	return 0;
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
	kb_set_progname("kdos-lock");

	if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("usage: kdos-lock\n\n"
		       "Locks the session through ext-session-lock-v1. The\n"
		       "compositor keeps the screen locked even if this process\n"
		       "dies, so there is nothing to kill your way out of.\n");
		return 0;
	}

	char user[64] = "user", host[64] = "kdos";
	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_name)
		snprintf(user, sizeof(user), "%s", pw->pw_name);
	if (gethostname(host, sizeof(host)) != 0)
		snprintf(host, sizeof(host), "kdos");
	host[sizeof(host) - 1] = '\0';

	KwlConfig cfg = {
		.role = KWL_ROLE_LOCK,
		.app_id = "kdos-lock",
		.keyboard = 1,
	};
	if (kwl_init(&cfg) != 0) {
		/*
		 * No compositor, no session-lock global, or no font. Exiting
		 * non-zero without having locked anything is the only safe
		 * failure: a lock screen that cannot draw must not lock a
		 * screen the user then cannot type into.
		 */
		fprintf(stderr, "kdos-lock: cannot create a lock surface — is "
				"kdos-comp running?\n");
		return 1;
	}
	ktui_draw_init();

	int done = 0;
	while (!done && !kwl_should_close()) {
		draw(user, host);

		KtuiEvent ev;
		/* One second, so the clock stays right without a redraw loop. */
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;
		if (ev.type != KT_EVT_KEY)
			continue;

		/* Refuse input until the compositor says the session is
		 * actually locked — see the header. */
		if (!kwl_lock_engaged()) {
			status = "locking...";
			continue;
		}
		failed = 0;
		status = NULL;

		switch (ev.key) {
		case KT_K_ENTER:
			if (npass)
				done = try_unlock();
			break;
		case KT_K_BACKSPACE:
			if (npass)
				pass[--npass] = '\0';
			break;
		case KT_K_ESC:
			pass_clear();
			break;
		default:
			/*
			 * Printable characters only, and the cap is silent: a
			 * held-down key must not be able to grow the buffer, and
			 * a message saying the password is too long would be a
			 * message about its length.
			 */
			if (ev.key >= 32 && ev.key < KT_K_SPECIAL &&
			    ev.key != KT_K_BACKSPACE && npass < LK_MAX_PASS - 1)
				pass[npass++] = (char)ev.key;
			break;
		}

		if (failed) {
			draw(user, host);
			struct timespec ts = {
				.tv_sec = LK_FAIL_MS / 1000,
				.tv_nsec = (LK_FAIL_MS % 1000) * 1000000L,
			};
			nanosleep(&ts, NULL);
		}
	}

	pass_clear();
	/*
	 * Read BEFORE the shutdown. kwl_shutdown() zeroes libkwl's state, so
	 * asking afterwards always answered "not refused" — a second lock
	 * client exited 0 while the compositor's log said it had been refused,
	 * which is precisely the disagreement this exit code exists to prevent.
	 */
	int refused = kwl_lock_finished();
	kwl_shutdown();
	if (refused)
		fprintf(stderr, "kdos-lock: the session is already locked by "
				"another client\n");
	/* A refused lock is not a crash, but it did not lock anything either,
	 * and a caller that treats 0 as "the session is locked" must not be
	 * told that. */
	return refused ? 1 : 0;
}
