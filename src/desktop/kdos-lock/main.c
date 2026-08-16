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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "ktui.h"
#include "kwl.h"

#define LK_MAX_PASS 256
#define LK_BOX_W    46
#define LK_BOX_H    9
#define LK_LOGO_MAX 40

/* Long enough that a script cannot try a dictionary through this UI, short
 * enough that a mistyped password is not a punishment. kdos-checkpass sleeps a
 * second of its own, which is the delay an attacker cannot skip. */
#define LK_FAIL_MS  1200

static char pass[LK_MAX_PASS];
static int npass;			/* bytes */
static int nchars;			/* characters — what the dots show */
static const char *status;
static int failed;

/*
 * The logo as COLOURED RUNS, not as one string.
 *
 * It used to be stripped to plain text and painted in a single slot that
 * breathed between text and dim on a four-second clock. That was defensible
 * while logo.txt was one green: there was nothing else in the file to draw.
 * Now that the mascot carries a dark body, a white belly, an amber beak and a
 * glow ring, stripping the colour is throwing the picture away — photographed
 * on a booted ISO, the lock screen showed a featureless dark-green mass where
 * tty1 two seconds earlier had shown a penguin.
 *
 * The colours become libktui SLOTS rather than literal RGB, so the lock screen
 * follows the accent like everything else: an amber desktop gets an amber
 * penguin without a second copy of the palette living here.
 */
#define LK_MAX_RUNS 48

struct lk_run {
	int slot;
	char *text;
};

static struct lk_run logo_runs[LK_LOGO_MAX][LK_MAX_RUNS];
static int logo_nruns[LK_LOGO_MAX];
static int logo_n, logo_w;

static void pass_clear(void)
{
	/* Not just npass = 0: the bytes are a password and this process stays
	 * alive across attempts. */
	memset(pass, 0, sizeof(pass));
	npass = 0;
	nchars = 0;
}

/*
 * The accent the desktop is wearing, read the way every kdos-shell front end
 * reads it (sh_theme_from_cache) — replicated rather than linked, because this
 * program must stay a lock screen and nothing else.
 */
static void theme_from_cache(void)
{
	char path[512], name[64];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return;

	FILE *f = fopen(path, "r");
	if (!f)
		return;
	if (fgets(name, sizeof(name), f)) {
		name[strcspn(name, "\n")] = '\0';
		if (*name)
			ktui_theme_set(name);
	}
	fclose(f);
}

/* ── the logo ──────────────────────────────────────────────────────────── */

/*
 * The SGR runs are parsed rather than stripped: the WHOLE sequence is
 * consumed either way, or `[1;32m` stays visible as text, but the code it
 * carries now picks the slot the run is drawn in. See lk_run.
 */

/*
 * genlogo.py's five materials, as slots. The file is generated and its palette
 * is fixed there, so this table is small and closed: dark body, glow ring,
 * belly and eyes, beak and feet, and the wordmark's two tones. Anything
 * unrecognised reads as text, which is what an uncoloured banner was.
 */
static int slot_for(const char *sgr, size_t n)
{
	struct { const char *code; int slot; } map[] = {
		{ "1;30", KT_DIM },	/* the black body   */
		{ "0;32", KT_ACCENT },	/* the glow ring    */
		{ "1;32", KT_TEXT },	/* wordmark, bright */
		{ "1;37", KT_TEXT },	/* belly, eyes      */
		{ "0;33", KT_WARN },	/* beak, feet       */
		{ "2;32", KT_DIM },	/* tagline          */
	};
	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		if (n == strlen(map[i].code) && !strncmp(sgr, map[i].code, n))
			return map[i].slot;
	return KT_TEXT;
}

static void logo_line(const char *p, size_t l)
{
	struct lk_run *runs = logo_runs[logo_n];
	int nruns = 0, slot = KT_TEXT, width = 0;
	char buf[512];
	size_t k = 0;

	for (size_t i = 0; i <= l; i++) {
		int flush = (i == l);
		int newslot = slot;

		if (!flush && p[i] == '\x1b') {
			size_t j = i + 1;
			if (j < l && p[j] == '[') {
				size_t s = ++j;
				while (j < l && (p[j] < 0x40 || p[j] > 0x7e))
					j++;
				newslot = (j > s && p[j] == 'm')
					? slot_for(p + s, j - s) : KT_TEXT;
				i = j;		/* the loop's ++ steps past 'm' */
				flush = 1;
			}
		}

		if (flush) {
			if (k && nruns < LK_MAX_RUNS) {
				buf[k] = '\0';
				runs[nruns].slot = slot;
				runs[nruns].text = kb_strdup(buf);
				width += ktui_utf8_width(buf);
				nruns++;
			}
			k = 0;
			slot = newslot;
			continue;
		}
		if (k + 1 < sizeof(buf))
			buf[k++] = p[i];
	}

	logo_nruns[logo_n] = nruns;
	if (width > logo_w)
		logo_w = width;
	logo_n++;
}

static void logo_load(void)
{
	size_t len;
	char *data = kb_read_all("/usr/share/kdos/logo.txt", &len);
	if (!data)
		return;

	const char *p = data;
	while (*p && logo_n < LK_LOGO_MAX) {
		const char *nl = strchr(p, '\n');
		size_t l = nl ? (size_t)(nl - p) : strlen(p);
		logo_line(p, l);
		p = nl ? nl + 1 : p + l;
	}
	free(data);

	/* Trailing blank lines would push the box down for nothing. */
	while (logo_n > 0 && logo_nruns[logo_n - 1] == 0)
		logo_n--;
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
	int cramped = box.x < 0 || box.y < 0;
	if (cramped)
		box = krect(0, 0, w, h);

	/*
	 * The logo above the prompt, when the whole stack fits — taller than
	 * the surface means NO logo, never a scrolled one (the kdos-banner
	 * rule). Drawn run by run in the colours genlogo.py put in the file;
	 * the four-second breathe went with the monochrome it was compensating
	 * for.
	 */
	if (!cramped && logo_n && logo_w <= w &&
	    logo_n + 1 + LK_BOX_H <= h) {
		int top = (h - (logo_n + 1 + LK_BOX_H)) / 2;
		int lx = (w - logo_w) / 2;
		for (int i = 0; i < logo_n; i++) {
			int x = lx;
			for (int r = 0; r < logo_nruns[i]; r++) {
				const char *t = logo_runs[i][r].text;
				int tw = ktui_utf8_width(t);
				ktui_draw_text(x, top + i, tw, t,
					       logo_runs[i][r].slot, KT_BG,
					       KT_A_NONE);
				x += tw;
			}
		}
		box.y = top + logo_n + 1;
	}

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
	int shown = nchars < room ? nchars : room;
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

/* ── input ─────────────────────────────────────────────────────────────── */

static int utf8_encode(uint32_t cp, char out[4])
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xc0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3f));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xe0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		out[2] = (char)(0x80 | (cp & 0x3f));
		return 3;
	}
	out[0] = (char)(0xf0 | (cp >> 18));
	out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
	out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
	out[3] = (char)(0x80 | (cp & 0x3f));
	return 4;
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
	theme_from_cache();
	logo_load();

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

	int done = 0, announced = 0;
	while (!done && !kwl_should_close()) {
		/*
		 * The LOCK READY handshake: kdos-power waits on this exact
		 * line before it sends `suspend`, so a machine never sleeps
		 * with its session still on screen. Once, when the compositor
		 * confirms — never on this process's own say-so.
		 */
		if (!announced && kwl_lock_engaged()) {
			printf("locked\n");
			fflush(stdout);
			announced = 1;
		}

		draw(user, host);

		KtuiEvent ev;
		/* One second, so the clock stays right without a redraw loop —
		 * shorter until the lock is confirmed, so the handshake above
		 * is not a second late for a suspend that is waiting on it. */
		if (!ktui_backend()->poll_event(&ev, announced ? 1000 : 100))
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
			if (npass) {
				/* The whole final UTF-8 sequence, lead byte
				 * and continuations together — a backspace
				 * that ate one byte of a two-byte character
				 * left a password nobody typed. */
				int i = npass - 1;
				while (i > 0 &&
				       ((unsigned char)pass[i] & 0xc0) == 0x80)
					i--;
				memset(pass + i, 0, (size_t)(npass - i));
				npass = i;
				if (nchars)
					nchars--;
			}
			break;
		case KT_K_ESC:
			pass_clear();
			break;
		default:
			/*
			 * Printable characters only, and the cap is silent: a
			 * held-down key must not be able to grow the buffer, and
			 * a message saying the password is too long would be a
			 * message about its length. The codepoint goes in as
			 * UTF-8 — the byte sequence kdos-checkpass will read is
			 * the one the user's shell would have fed passwd —
			 * whole or not at all: a cap that split a sequence
			 * would store a password that cannot be retyped.
			 */
			if (ev.key >= 32 && ev.key < KT_K_SPECIAL &&
			    ev.key != KT_K_BACKSPACE) {
				char seq[4];
				int sl = utf8_encode((uint32_t)ev.key, seq);
				if (npass + sl < LK_MAX_PASS) {
					memcpy(pass + npass, seq, (size_t)sl);
					npass += sl;
					nchars++;
				}
			}
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
