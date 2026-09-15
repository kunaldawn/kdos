/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-con — a script is the keys somebody typed, played back
 *
 * `Super+Shift+r` starts recording what reaches the focused window and stops
 * on the next press, asking for a letter to keep it under; `Super+Alt+r` then
 * that letter types it into whatever has the focus now. The rate is the rate
 * it was typed at, capped, so a replay looks like typing rather than a paste.
 *
 * WHAT THIS IS NOT, AND THE REFUSALS THAT KEEP IT THAT WAY:
 *
 *   A SCRIPT IS KEYS INTO A WINDOW AND NEVER A COMMAND. The file holds key
 *   names and modifiers; there is no field that could name a program, so
 *   nothing here can start one. A macro file that could would be a file that
 *   runs code, written by whatever a person happened to type into.
 *
 *   NOTHING IS RECORDED WHILE THE SCREEN IS LOCKED. A lock is typed into with
 *   a password, and a recorder running underneath one would write it to a
 *   file. The refusal is checked when recording STARTS and again for every
 *   key, because a lock can appear in the middle of a recording. The greeter
 *   needs no rule of its own: it is a separate process, and no session — and
 *   so no recorder — exists while it is up.
 *
 *   NOTHING IS PLAYED INTO A LOCK either: a script that could type at a lock
 *   screen is a script that can be made to guess at one. A replay in progress
 *   is dropped when the screen locks under it.
 *
 *   THE SESSION IS THE ONLY THING THAT CAN DO EITHER. There is no `kdos con
 *   script` verb, so a client on the surface socket can neither start a
 *   recording nor play one — the same two rules the key verb keeps.
 *
 *   THE DIRECTORY IS 0700 AND THE FILES ARE 0600. Whatever a person types
 *   into a terminal can end up in one.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "ktui.h"
#include "con.h"

/* A recording is a person's hand, so the cap is what a hand can do: a
 * thousand keys is several minutes of typing and far past what anybody binds
 * to one letter. */
#define SCR_MAX_KEYS 1000
/* The gap between two keys on replay, in milliseconds. The rate that was typed
 * is kept because a program reading it — a shell's line editor, vim's pending
 * command — behaves differently when keys arrive all at once; the cap is what
 * stops a replay pausing for as long as somebody once stopped to think. */
#define SCR_MAX_GAP_MS 20

struct scrkey {
	int key, mods;
	int gap_ms;
};

static struct scrkey rec[SCR_MAX_KEYS];
static int nrec;
static int learning;
static int prompt;		/* the letter prompt is up */
static int play_armed;
static double last_at;
static char status[96];

/* The replay, and where it has got to. `pidx < nplay` IS the playing flag:
 * a separate one is a second thing to clear. */
static struct scrkey play[SCR_MAX_KEYS];
static int nplay, pidx;
static double next_at;

static int locked_out(void)
{
	/* `locked` OUTLIVES THE LOCK SURFACE, so both are asked: a lock whose
	 * client crashed leaves a locked session with no window, and a
	 * recorder running underneath one is a recorder writing a password to
	 * a file. The greeter is a separate process — `kdos-con-login` runs
	 * instead of a session, never inside one — so there is nothing here to
	 * ask about it. */
	return S.locked || S.lock != NULL;
}

static int scr_dir(char *out, size_t n)
{
	const char *home = getenv("XDG_CONFIG_HOME");
	char base[512];

	if (home && *home)
		snprintf(base, sizeof(base), "%s", home);
	else if ((home = getenv("HOME")) && *home)
		snprintf(base, sizeof(base), "%s/.config", home);
	else
		return 0;
	return snprintf(out, n, "%s/kdos-con/scripts", base) < (int)n;
}

static int scr_path(int letter, char *out, size_t n)
{
	char dir[512];

	/* ONE LETTER, AND ONLY A LETTER. The letter becomes a file name, so
	 * anything else is a path somebody chose. */
	if (letter < 'a' || letter > 'z')
		return 0;
	if (!scr_dir(dir, sizeof(dir)))
		return 0;
	return snprintf(out, n, "%s/%c", dir, letter) < (int)n;
}

int scr_learning(void)
{
	return learning;
}

int scr_prompt_active(void)
{
	return prompt;
}

int scr_play_armed(void)
{
	return play_armed;
}

const char *scr_status(void)
{
	return status[0] ? status : NULL;
}

void scr_play_arm(void)
{
	play_armed = 1;
	kb_strlcpy(status,
		   " play which script?   a letter plays it   Escape cancels",
		   sizeof(status));
}

void scr_learn_toggle(void)
{
	if (locked_out()) {
		/* SAID, rather than silently ignored: a chord that does
		 * nothing and explains nothing is one a person presses again
		 * harder. */
		con_notice("not while the screen is locked");
		return;
	}

	if (!learning) {
		learning = 1;
		nrec = 0;
		last_at = 0;
		kb_strlcpy(status,
			   " RECORDING every key that reaches a window   "
			   "Super+Shift+r stops",
			   sizeof(status));
		return;
	}

	learning = 0;
	if (!nrec) {
		status[0] = '\0';
		con_notice("nothing was typed, so nothing was kept");
		return;
	}
	prompt = 1;
	snprintf(status, sizeof(status),
		 " %d keys recorded — bind to which letter?   "
		 "Escape throws it away", nrec);
}

void scr_note(const KtuiEvent *ev)
{
	double now = kb_now_s();

	if (!learning || !ev || ev->type != KT_EVT_KEY)
		return;
	/* CHECKED AGAIN, not only at the start: a lock can appear in the
	 * middle of a recording, and the keys after it are the ones that
	 * matter. */
	if (locked_out()) {
		learning = 0;
		nrec = 0;
		status[0] = '\0';
		return;
	}
	if (nrec >= SCR_MAX_KEYS) {
		learning = 0;
		prompt = 1;
		snprintf(status, sizeof(status),
			 " %d keys, the limit — bind to which letter?   "
			 "Escape throws it away", nrec);
		return;
	}

	rec[nrec].key = ev->key;
	rec[nrec].mods = ev->mods;
	rec[nrec].gap_ms = last_at > 0 ? (int)((now - last_at) * 1000.0) : 0;
	if (rec[nrec].gap_ms < 0)
		rec[nrec].gap_ms = 0;
	last_at = now;
	nrec++;
}

static void scr_save(int letter)
{
	char path[600], dir[512];
	FILE *f;
	int fd;

	if (!scr_path(letter, path, sizeof(path)) ||
	    !scr_dir(dir, sizeof(dir)))
		return;
	kb_mkdir_p(dir);
	chmod(dir, 0700);

	/* 0600 FROM THE MOMENT IT EXISTS. Created with the mode rather than
	 * chmod'd afterwards: between the two there is a file somebody else
	 * can read, and what is in it is whatever was typed. */
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return;
	f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		return;
	}
	fprintf(f, "# kdos-con script: keys, not a command\n");
	for (int i = 0; i < nrec; i++) {
		char name[64];

		/* THE FIRST COLUMN IS FOR THE EYE and the last three are what
		 * is replayed: a chord spelled the way `keys.conf` spells it,
		 * so a person reading the file can tell what it types without
		 * decoding a key number. It is not parsed back. */
		keys_chord_name(rec[i].key, rec[i].mods, name, sizeof(name));
		fprintf(f, "%s\t%d\t%d\t%d\n", name, rec[i].mods, rec[i].key,
			rec[i].gap_ms);
	}
	fclose(f);
}

int scr_prompt_key(const KtuiEvent *ev)
{
	if (!ev || ev->type != KT_EVT_KEY)
		return 0;

	if (prompt) {
		prompt = 0;
		status[0] = '\0';
		if (ev->key == KT_K_ESC) {
			con_notice("the recording was thrown away");
			return 1;
		}
		if (ev->key >= 'a' && ev->key <= 'z') {
			scr_save(ev->key);
			con_notice("kept — Super+Alt+r then that letter plays it");
		} else {
			con_notice("a script is kept under a letter a to z");
		}
		return 1;
	}

	if (play_armed) {
		play_armed = 0;
		status[0] = '\0';
		if (ev->key == KT_K_ESC)
			return 1;
		if (!scr_play(ev->key))
			con_notice("no script under that letter");
		return 1;
	}
	return 0;
}

int scr_play(int letter)
{
	char path[600];
	char *text;

	/* NOT INTO A LOCK. A script that could type at a lock screen is one
	 * that can be made to guess at a password, at whatever rate the file
	 * says. */
	if (locked_out()) {
		con_notice("not while the screen is locked");
		return 1;
	}
	if (!scr_path(letter, path, sizeof(path)))
		return 0;
	text = kb_read_whole(path, NULL);
	if (!text)
		return 0;

	nplay = 0;
	for (char *line = text, *nl; line && *line && nplay < SCR_MAX_KEYS;
	     line = nl) {
		int mods = 0, key = 0, gap = 0;
		char name[64];

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		if (*line == '#' || !*line)
			continue;
		/* The name column is read and thrown away: the three numbers
		 * after it are the key, and a file whose name and numbers
		 * disagree replays the numbers. */
		if (sscanf(line, "%63[^\t]\t%d\t%d\t%d", name, &mods, &key,
			   &gap) != 4 || key <= 0)
			continue;
		if (gap < 0)
			gap = 0;
		play[nplay].key = key;
		play[nplay].mods = mods;
		play[nplay].gap_ms = gap > SCR_MAX_GAP_MS ? SCR_MAX_GAP_MS :
							    gap;
		nplay++;
	}
	free(text);

	if (!nplay)
		return 0;
	pidx = 0;
	next_at = kb_now_s();
	snprintf(status, sizeof(status), " playing script %c   Escape stops it",
		 letter);
	return 1;
}

int scr_playing(void)
{
	return pidx < nplay;
}

void scr_stop(void)
{
	nplay = pidx = 0;
	if (!learning && !prompt && !play_armed)
		status[0] = '\0';
}

/*
 * ONE KEY A TURN, FROM THE SESSION'S OWN LOOP — never a sleep inside a replay.
 *
 * The loop polls on a twenty-millisecond tick and that tick is what paces a
 * script: sleeping through a replay instead would hold the whole desktop, so
 * nothing would repaint until the last key and a person would watch a frozen
 * screen produce a finished paragraph. Delivering one key per due moment keeps
 * every window drawing while the script types into one of them.
 */
void scr_pump(void)
{
	double now;

	if (pidx >= nplay)
		return;
	/* A LOCK ENDS A REPLAY IN PROGRESS, not only one about to start: the
	 * screen can lock on its own timer while a script is still typing. */
	if (locked_out()) {
		scr_stop();
		return;
	}

	now = kb_now_s();
	while (pidx < nplay && now >= next_at) {
		/*
		 * INTO THE WINDOW, NEVER THROUGH THE CHORD TABLE. A replay
		 * that went through the session's own keys would fire whatever
		 * the recording happened to contain — a workspace switch, a
		 * quit — from a file. What was recorded reached a window, and
		 * that is where it goes back.
		 */
		KtuiEvent ev = { 0 };

		ev.type = KT_EVT_KEY;
		ev.key = play[pidx].key;
		ev.mods = play[pidx].mods;
		con_key_to_window(&ev);
		pidx++;
		next_at = now + (double)(pidx < nplay ? play[pidx].gap_ms : 0)
			  / 1000.0;
	}
	if (pidx >= nplay)
		scr_stop();
}
