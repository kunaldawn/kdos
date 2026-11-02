/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-sfx — the machine's noises, synthesized
 *
 *   kdos-sfx login | notify | error | degauss
 *   kdos-sfx render <dir> [--format oga|wav]
 *
 * NO SAMPLE FILES IN THE TREE — the same derive-everything shape genlogo.py
 * has for the mascot. Each sound is a few lines of waveform, and `render`
 * writes them out once, at build time, as the KDOS sound theme's files
 * (kdos-sounds runs it): the freedesktop names Plasma plays —
 * desktop-login, desktop-logout, message-new-instant, dialog-error,
 * dialog-warning, complete, device-added, device-removed — so the session
 * and this command make the same noise from the same numbers.
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
#include <stdint.h>
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

/* The voices, and what each one is FOR is why it sounds the way it does. */
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
	if (!strcmp(what, "logout")) {
		/* The login chime backwards: the same fifths, descending. */
		tone(buf, nbuf, 0,               495, 170, 5500, 128);
		tone(buf, nbuf, RATE * 165 / 1000, 330, 170, 5000, 128);
		tone(buf, nbuf, RATE * 330 / 1000, 220, 200, 5000, 128);
		return RATE * 530 / 1000;
	}
	if (!strcmp(what, "warning")) {
		/* Two of the notify blip's kind, lower: asks for a look, not
		 * for alarm. */
		tone(buf, nbuf, 0,                660, 60, 4500, 0);
		tone(buf, nbuf, RATE * 100 / 1000, 660, 60, 4500, 0);
		return RATE * 165 / 1000;
	}
	if (!strcmp(what, "complete")) {
		/* A rising pair: something finished, and finished well. */
		tone(buf, nbuf, 0,               660, 70, 4200, 0);
		tone(buf, nbuf, RATE * 70 / 1000, 990, 70, 4200, 0);
		return RATE * 145 / 1000;
	}
	if (!strcmp(what, "added") || !strcmp(what, "removed")) {
		/* Up for a device arriving, down for one leaving, so the two
		 * can be told apart without looking. */
		int up = !strcmp(what, "added");

		tone(buf, nbuf, 0, up ? 440 : 660, 60, 4000, 128);
		tone(buf, nbuf, RATE * 60 / 1000, up ? 660 : 440, 60, 4000,
		     128);
		return RATE * 125 / 1000;
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

/* ── the sound theme ─────────────────────────────────────────────────── */

/* The freedesktop name each voice is installed under. */
static const struct {
	const char *file, *voice;
} THEME[] = {
	{ "desktop-login", "login" },
	{ "desktop-logout", "logout" },
	{ "message-new-instant", "notify" },
	{ "dialog-error", "error" },
	{ "dialog-warning", "warning" },
	{ "complete", "complete" },
	{ "device-added", "added" },
	{ "device-removed", "removed" },
};

static void le32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

/* RIFF/WAVE, PCM s16le mono: the 44-byte header and the samples, nothing
 * else, so two renders of one voice are the same bytes. */
static int write_wav(const char *path, const int16_t *buf, int n)
{
	unsigned char h[44];
	uint32_t data = (uint32_t)n * 2;
	FILE *f;
	int ok;

	memcpy(h, "RIFF", 4);
	le32(h + 4, 36 + data);
	memcpy(h + 8, "WAVEfmt ", 8);
	le32(h + 16, 16);
	h[20] = 1; h[21] = 0;			/* PCM */
	h[22] = 1; h[23] = 0;			/* mono */
	le32(h + 24, RATE);
	le32(h + 28, RATE * 2);
	h[32] = 2; h[33] = 0;			/* block align */
	h[34] = 16; h[35] = 0;			/* bits */
	memcpy(h + 36, "data", 4);
	le32(h + 40, data);
	if (!(f = fopen(path, "wbe")))
		return -1;
	ok = fwrite(h, 1, sizeof(h), f) == sizeof(h);
	for (int i = 0; ok && i < n; i++) {
		unsigned char s[2] = { (unsigned char)(buf[i] & 0xff),
				       (unsigned char)((buf[i] >> 8) & 0xff) };

		ok = fwrite(s, 1, 2, f) == 2;
	}
	return fclose(f) == 0 && ok ? 0 : -1;
}

/*
 * THE BUILD'S FORM IS OGG VORBIS, through oggenc fed the raw samples. A fixed
 * stream serial keeps two builds of the package byte-identical; oggenc picks
 * a random one otherwise. It runs inside a build.sh, so a failure is loud.
 */
static int render(int argc, char **argv)
{
	const char *dir = NULL, *fmt = "oga";
	static int16_t buf[MAXSAMP];

	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--format") && i + 1 < argc)
			fmt = argv[++i];
		else if (argv[i][0] != '-' && !dir)
			dir = argv[i];
		else
			dir = NULL, i = argc;
	}
	if (!dir || (strcmp(fmt, "oga") && strcmp(fmt, "wav"))) {
		fprintf(stderr, "usage: kdos-sfx render <dir> "
				"[--format oga|wav]\n");
		return 2;
	}
	if (kb_mkdir_p(dir) != 0) {
		fprintf(stderr, "kdos-sfx: cannot create %s\n", dir);
		return 1;
	}
	for (size_t i = 0; i < sizeof(THEME) / sizeof(THEME[0]); i++) {
		int n = synth(THEME[i].voice, buf, MAXSAMP);
		char path[4096];

		snprintf(path, sizeof(path), "%s/%s.%s", dir, THEME[i].file,
			 fmt);
		if (!strcmp(fmt, "wav")) {
			if (write_wav(path, buf, n) != 0) {
				fprintf(stderr, "kdos-sfx: cannot write %s\n",
					path);
				return 1;
			}
			continue;
		}
		{
			KbArgv a = { 0 };
			int was = kb_proc_verbose, rc;

			kb_argv_add(&a, "oggenc");
			kb_argv_add(&a, "-Q");
			kb_argv_add(&a, "-r");
			kb_argv_add(&a, "-B");
			kb_argv_add(&a, "16");
			kb_argv_add(&a, "-C");
			kb_argv_add(&a, "1");
			kb_argv_add(&a, "-R");
			kb_argv_addf(&a, "%d", RATE);
			kb_argv_add(&a, "-q");
			kb_argv_add(&a, "4");
			kb_argv_add(&a, "--serial");
			kb_argv_add(&a, "1");
			kb_argv_add(&a, "-o");
			kb_argv_add(&a, path);
			kb_argv_add(&a, "-");
			kb_argv_end(&a);
			kb_proc_verbose = 1;
			rc = kb_run_feed(&a, (const char *)buf,
					 (size_t)n * sizeof(*buf));
			kb_proc_verbose = was;
			if (rc != 0 || !kb_path_exists(path)) {
				fprintf(stderr, "kdos-sfx: oggenc could not "
						"write %s\n", path);
				return 1;
			}
		}
	}
	return 0;
}

/* ── the front end ─────────────────────────────────────────────────────── */

int sfx_main(int argc, char **argv)
{
	const char *what = argc > 1 ? argv[1] : NULL;
	if (!what || !strcmp(what, "-h") || !strcmp(what, "--help")) {
		fprintf(stderr, "usage: kdos-sfx {login|notify|error|degauss|"
				"logout|warning|complete|added|removed}\n"
				"       kdos-sfx --route   what it would play "
				"through\n"
				"       kdos-sfx render <dir> "
				"[--format oga|wav]\n");
		return 2;
	}
	/* The mute does not apply: rendering is a build writing files, not
	 * a machine making a noise. */
	if (!strcmp(what, "render"))
		return render(argc, argv);
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
