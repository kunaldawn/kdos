/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-sfx — the machine's four noises, synthesized
 *
 *   kdos-sfx login | notify | error | degauss
 *
 * NO SAMPLE FILES, no sound theme, no decoder and no `freedesktop` lookup —
 * the same derive-everything shape genlogo.py has for the mascot. Four
 * waveforms are four functions, the whole corpus is under a kilobyte of C, and
 * there is nothing to install, ship, theme or keep in step with an accent.
 *
 * Square and triangle only, integer phase accumulators, integer envelopes: no
 * libm. That is not an aesthetic — kdos-tools is compiled by the host selftest
 * with no -lm on the command line, and a sin() here would be a link failure on
 * a machine that has one library fewer than this one.
 *
 * SILENCE IS THE FAILURE MODE. A machine with no audio device, no player and
 * no session exits 0 without a word: this is a decoration, and a decoration
 * that prints an error every time you log in is a decoration people delete.
 *
 * And every call is fire-and-forget from the CALLER's point of view, which
 * this program has to arrange for itself — a caller execs it and would
 * otherwise wait for the sound to finish. It forks, the parent returns
 * immediately, and the child does the playing.
 * ---------------------------------
 */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "kdos-tools.h"

#define RATE 44100
#define MAXSAMP (RATE * 2)		/* two seconds is the hard ceiling */

/* ── synthesis ─────────────────────────────────────────────────────────── */

/*
 * One tone, mixed into `buf` at `off`. `duty` in 1/256ths selects the shape:
 * 128 is a square, 0 asks for a triangle — the two waveforms a machine with a
 * one-bit speaker could make, which is the vocabulary this is written in.
 *
 * The envelope is a linear decay with a short integer attack. An abrupt start
 * on a square wave is a click, and a click at login is indistinguishable from
 * a driver fault.
 */
static void tone(int16_t *buf, int nbuf, int off, int freq, int ms, int amp,
		 int duty)
{
	int n = RATE * ms / 1000;
	if (off >= nbuf)
		return;
	if (off + n > nbuf)
		n = nbuf - off;

	/* Phase in 1/65536ths of a period, so a whole cycle is one 16-bit wrap
	 * and no division is needed per sample. */
	unsigned step = (unsigned)(((long long)freq << 16) / RATE);
	unsigned phase = 0;
	int attack = RATE / 400;		/* 2.5 ms */

	for (int i = 0; i < n; i++, phase += step) {
		unsigned ph = phase & 0xffff;
		int v;
		if (duty)
			v = (ph < (unsigned)(duty << 8)) ? amp : -amp;
		else
			v = ph < 0x8000 ? (int)(ph * 2 * (unsigned)amp / 0x8000) - amp
					: amp - (int)((ph - 0x8000) * 2 *
						      (unsigned)amp / 0x8000);

		int env = (n - i) * 256 / (n ? n : 1);		/* decay */
		if (i < attack)
			env = env * i / (attack ? attack : 1);
		int s = buf[off + i] + v * env / 256;
		buf[off + i] = (int16_t)(s > 32000 ? 32000
						   : s < -32000 ? -32000 : s);
	}
}

/* Four voices, and what each one is FOR is why it sounds the way it does. */
static int synth(const char *what, int16_t *buf, int nbuf)
{
	memset(buf, 0, (size_t)nbuf * sizeof(*buf));

	if (!strcmp(what, "login")) {
		/* Three ascending fifths — the interval a power-on chime has
		 * used since the machines this desktop is imitating. */
		tone(buf, nbuf, 0,               220, 170, 5000, 128);
		tone(buf, nbuf, RATE * 165 / 1000, 330, 170, 5000, 128);
		tone(buf, nbuf, RATE * 330 / 1000, 495, 200, 5500, 128);
		return RATE * 530 / 1000;
	}
	if (!strcmp(what, "notify")) {
		/* One blip, triangle: it has to be noticeable and must not be
		 * the loudest thing in the room forty times a day. */
		tone(buf, nbuf, 0, 880, 70, 4200, 0);
		return RATE * 80 / 1000;
	}
	if (!strcmp(what, "error")) {
		/* Low and square, which is what "buzz" means. */
		tone(buf, nbuf, 0, 110, 220, 6000, 128);
		tone(buf, nbuf, 0, 111, 220, 3000, 128);	/* beating */
		return RATE * 230 / 1000;
	}
	if (!strcmp(what, "degauss")) {
		/* 40 Hz, decaying — the thoomp a CRT makes when it demagnetises
		 * itself, and the sound `kdos theme` fires the shader wobble
		 * with. Below most laptop speakers' range on purpose: on a
		 * machine that cannot reproduce it you feel nothing, which is
		 * also what a real degauss does through a desk. */
		tone(buf, nbuf, 0, 40, 400, 9000, 0);
		tone(buf, nbuf, 0, 61, 260, 3500, 0);
		return RATE * 420 / 1000;
	}
	return -1;
}

/* ── the route ────────────────────────────────────────────────────────── */

/*
 * Probed, and the SESSION answer cached in $XDG_RUNTIME_DIR — the same pattern
 * kdos-appbox uses for the Qt theme labels, and for the same reason: a probe
 * per notification is a fork per notification. See route() for why only that
 * one answer may be remembered.
 *
 * Two routes and no third. pw-cat when pipewire is up, because that is what
 * owns the device inside a session; aplay otherwise, which is the TTY case
 * (`bb` and the boot chime run there, with no pipewire anywhere). Linking
 * alsa-lib directly would put a real -l on kdos-tools — a package the host
 * selftest compiles with libkbase, libkcolor, libkpkg and libkxdg and nothing
 * else — to save one exec on a path that already forked.
 */
static const char *probe_route(void)
{
	const char *rt = kb_runtime_dir();
	char *pw = kb_path_join(rt, "pipewire-0");
	int have_pw = kb_path_exists(pw) && kb_have_prog("pw-cat");
	free(pw);
	if (have_pw)
		return "pw-cat";
	if (kb_have_prog("aplay") && kb_path_exists("/dev/snd"))
		return "aplay";
	return "none";
}

static const char *route(void)
{
	static char cached[16];
	char *path = kb_path_join(kb_runtime_dir(), "kdos-sfx.route");

	/*
	 * Only `pw-cat` is cached, and only while its socket is still there.
	 * $XDG_RUNTIME_DIR is made at boot and OUTLIVES the tty→desktop
	 * transition, so caching the other two answers — which is what the
	 * first chime from a bare TTY probes, before any pipewire exists —
	 * kept the whole session off pipewire for the rest of the uptime, or
	 * silent. What the cache saves is the PATH walk for `pw-cat`; the stat
	 * that confirms it costs nothing next to the fork this is about to do.
	 */
	if (kb_read_line_file(path, cached, sizeof(cached)) > 0 &&
	    !strcmp(cached, "pw-cat")) {
		char *pw = kb_path_join(kb_runtime_dir(), "pipewire-0");
		int up = kb_path_exists(pw);
		free(pw);
		if (up) {
			free(path);
			return cached;
		}
	}
	const char *r = probe_route();
	kb_strlcpy(cached, r, sizeof(cached));
	/* Best effort: an unwritable runtime dir costs a probe per call and
	 * nothing else, so it is not worth reporting. */
	if (!strcmp(r, "pw-cat"))
		kb_write_file(path, "pw-cat\n");
	free(path);
	return cached;
}

static void player_argv(KbArgv *a, const char *r)
{
	if (!strcmp(r, "pw-cat")) {
		kb_argv_add(a, "pw-cat");
		kb_argv_add(a, "--playback");
		kb_argv_add(a, "--raw");
		kb_argv_add(a, "--rate");
		kb_argv_addf(a, "%d", RATE);
		kb_argv_add(a, "--channels");
		kb_argv_add(a, "1");
		kb_argv_add(a, "--format");
		kb_argv_add(a, "s16");
		kb_argv_add(a, "-");
	} else {
		kb_argv_add(a, "aplay");
		kb_argv_add(a, "-q");
		kb_argv_add(a, "-t");
		kb_argv_add(a, "raw");
		kb_argv_add(a, "-f");
		kb_argv_add(a, "S16_LE");
		kb_argv_add(a, "-r");
		kb_argv_addf(a, "%d", RATE);
		kb_argv_add(a, "-c");
		kb_argv_add(a, "1");
		kb_argv_add(a, "-");
	}
	kb_argv_end(a);
}

/* ── the front end ─────────────────────────────────────────────────────── */

int sfx_main(int argc, char **argv)
{
	const char *what = argc > 1 ? argv[1] : NULL;
	if (!what || !strcmp(what, "-h") || !strcmp(what, "--help")) {
		fprintf(stderr, "usage: kdos-sfx {login|notify|error|degauss}\n"
				"       kdos-sfx --route   what it would play "
				"through\n");
		return 2;
	}
	if (!strcmp(what, "--route")) {
		printf("%s\n", route());
		return 0;
	}

	/* $KDOS_NO_SFX is the mute, and it is checked before anything is
	 * synthesized so a machine that does not want noises pays nothing. */
	const char *mute = getenv("KDOS_NO_SFX");
	if (mute && *mute)
		return 0;

	static int16_t buf[MAXSAMP];
	int n = synth(what, buf, MAXSAMP);
	if (n < 0) {
		fprintf(stderr, "kdos-sfx: no sound named '%s'\n", what);
		return 2;
	}

	const char *r = route();
	if (!strcmp(r, "none"))
		return 0;		/* silence, quietly */

	/*
	 * The caller must never wait on this. The parent returns the moment the
	 * child exists; the child is reparented to init and nobody collects it,
	 * which is the correct disposal for a process whose exit status carries
	 * no information anybody can act on.
	 */
	pid_t pid = fork();
	if (pid < 0)
		return 0;
	if (pid > 0)
		return 0;

	setsid();
	signal(SIGHUP, SIG_IGN);

	/* stdout and stderr go away: a player that complains about a busy
	 * device must not write into whatever terminal the caller had. */
	int null = open("/dev/null", O_WRONLY | O_CLOEXEC);
	if (null >= 0) {
		dup2(null, STDOUT_FILENO);
		dup2(null, STDERR_FILENO);
		if (null > STDERR_FILENO)
			close(null);
	}

	KbArgv a = {0};
	player_argv(&a, r);
	kb_run_feed(&a, (const char *)buf, (size_t)n * sizeof(*buf));
	_exit(0);
}
