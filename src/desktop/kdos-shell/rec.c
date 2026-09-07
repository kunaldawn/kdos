/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-rec — pick an input, watch the level, keep the file
 *
 *   ╔═ Recording ═════════════════════════════════════════════════════╗
 *   ║ INPUT                                                           ║
 *   ║   Default            the card ALSA picks                        ║
 *   ║ > hw:1,0             ALC623 Analog                        MUTED ║
 *   ║ LEVEL                                                           ║
 *   ║   ....::##::.....                                               ║
 *   ║   0:12  384K  peak -14 dBFS                                     ║
 *   ║ RECORDINGS                                                      ║
 *   ║   2026-09-06-142233.wav                          1.4M    0:44   ║
 *   ╟─────────────────────────────────────────────────────────────────╢
 *   ║ [ Record ] [ Transcribe ] [ Play ] [ Delete ] [ Close ]          ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * A CARD IS NOT A MICROPHONE. The list is `kpr_sound_pcms()` filtered to the
 * PCMs that carry a capture stream, which is why an HDMI codec — four playback
 * PCMs and no capture — never appears. `kdos-devices` reads the same function
 * for the same reason; two surfaces must not give two answers to what a
 * microphone is.
 *
 * `sox` DOES THE PART ONLY `sox` CAN DO: open the device at whatever rate,
 * format and channel count it has, and resample to 16 kHz mono s16. That is
 * exactly what `whisper-cli` requires, so the file this writes is the file the
 * transcriber reads with no second conversion. The input is named on argv
 * (`-t alsa hw:C,D`), never left to `rec` or `-d`: sox's default-device probe
 * opens a card for PLAYBACK and so skips a card that has no DAC.
 *
 * THE LEVEL IS THIS PROGRAM'S OWN PEAK OVER THE BYTES IT WROTE, not sox's `-S`
 * meter, which is fourteen text steps two decibels apart on a throttled
 * repaint. `lvl[]` holds `max|s| / 32768` — a FRACTION — one entry per tick,
 * and `ktui_sparkline` is given `vmax = 1.0` so that a microphone's noise floor
 * draws as a floor. Autoscaling would paint silence as a solid bar, which is
 * the one reading a level meter must never give.
 *
 * ONE ENTRY PER TICK, NOT PER READ. A read's size is the pipe's, not time's,
 * and a sparkline whose column spacing is the scheduler's is a chart of the
 * scheduler.
 *
 * NO libm. dBFS is a table of integer peak thresholds at half-decibel
 * midpoints, so the printed number is the NEAREST decibel and nothing in this
 * tree gains a maths library for one logarithm.
 *
 * NOTHING ABOUT ANOTHER PROGRAM'S PRESENCE REACHES A DRAW. `kb_have_prog`
 * walks $PATH, which a golden does not freeze, so it is called only when a
 * child is actually started. `Transcribe` is enabled by the MODEL GATE alone.
 *
 * THE TWO LENGTH FIELDS ARE REWRITTEN EVERY TICK. A WAV header written only at
 * stop leaves a file no player will open whenever the machine goes away
 * mid-recording — which in the rig is every run.
 *
 * THE WORDS HAVE NEVER BEEN READ BACK. No speech model ships and the desktop
 * cannot fetch one, so the gate, the argv, the spawn and the exit status are
 * what this surface proves about transcription. Nothing here claims more.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shell.h"
#include "kchrome.h"
#include "kproc.h"

/* A recorder is a window you leave open, not an overlay. */
#define RC_COLS 76
#define RC_ROWS 24

/* The ring the meter draws from. Wider than any window's spark, so the number
 * beside the chart is windowed to the same columns the chart was drawn with
 * rather than to everything ever recorded. */
#define RC_HIST 240

/* 16 kHz mono s16 — `whisper-cli`'s required input, so no second conversion.
 * A tick is a tenth of a second, and `--input-buffer` is set to match so a
 * block arrives per tick instead of every quarter second. */
#define RC_RATE 16000
#define RC_TICK_MS 100
#define RC_TICK_SAMPLES (RC_RATE * RC_TICK_MS / 1000)

/* Where a packaged model would land. Overridable so the harness can point it
 * at a path that does not exist: a build host that happens to carry models
 * would otherwise draw a different frame than the committed golden. */
#ifndef KDOS_WHISPER_DIR
#define KDOS_WHISPER_DIR "/usr/share/whisper.cpp/models"
#endif

#define RC_MAX_IN 16
#define RC_MAX_FILE 128

struct rc_in {
	char id[16];			/* hw:C,D, or "default"            */
	char name[64];
};

struct rc_file {
	char name[NAME_MAX + 1];
	unsigned long long size;
	int secs;
};

static KtuiKeys keys;

static struct rc_in ins[RC_MAX_IN];
static int nin, sel_in;

static struct rc_file files[RC_MAX_FILE];
static int nfile, sel_file, file_top;

static double lvl[RC_HIST];
static int nlvl;
static int tick_peak;			/* max|s| this tick, i16 units     */
static unsigned char carry;		/* low byte of a split sample      */
static int have_carry;

static pid_t kid = -1;
static int kid_fd = -1, kid_err = -1;
static int wav_fd = -1;
static unsigned long long wav_bytes;
static char wav_path[PATH_MAX];
static long long rec_start_ms;

static char model[PATH_MAX];		/* empty when there is none        */
static char model_where[PATH_MAX];	/* what the subtitle names         */
static char note[160];
static char transcript[4096];
static int showing_transcript;
static int asking;			/* the delete confirmation         */

/* Rows the last draw put the lists on, for the pointer. */
static int in_y, in_rows, fl_y, fl_rows;

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ── the level ──────────────────────────────────────────────────────────── */

/*
 * Peak thresholds at half-decibel midpoints, so the first entry a peak clears
 * names the NEAREST decibel rather than the floor. DB_PEAK[d] is
 * round(32768 * 10^(-(d + 0.5) / 20)); a fraction of exactly 0.5 is -6.02 dB
 * and must print -6, which a floor table would call -7.
 */
static const int DB_PEAK[61] = {
	 30935,  27571,  24573,  21900,  19519,  17396,  15504,  13818,
	 12315,  10976,   9783,   8719,   7771,   6925,   6172,   5501,
	  4903,   4370,   3894,   3471,   3093,   2757,   2457,   2190,
	  1952,   1740,   1550,   1382,   1232,   1098,    978,    872,
	   777,    693,    617,    550,    490,    437,    389,    347,
	   309,    276,    246,    219,    195,    174,    155,    138,
	   123,    110,     98,     87,     78,     69,     62,     55,
	    49,     44,     39,     35,     31,
};

/*
 * `f` is the fraction the ring holds, not a sample. ASCII throughout: a dump
 * renders at the ascii tier, where anything else becomes a question mark.
 */
static const char *rec_db_text(double f)
{
	static char s[24];
	int peak = (int)(f * 32768.0 + 0.5);

	if (peak >= 32767)
		return "CLIP";
	if (peak >= DB_PEAK[0])
		return "0 dBFS";
	for (int d = 1; d < 61; d++)
		if (peak >= DB_PEAK[d]) {
			snprintf(s, sizeof(s), "-%d dBFS", d);
			return s;
		}
	return peak > 0 ? "-60 dBFS or less" : "-inf dBFS";
}

/*
 * Fold a block of captured bytes into this tick's peak and append them to the
 * file. AN ODD TRAILING BYTE IS CARRIED: a read that splits a sample and is
 * treated as two samples reports a spike at whatever the low byte happened to
 * be.
 */
static void rec_take(const unsigned char *b, size_t n)
{
	size_t i = 0;

	if (wav_fd >= 0 && n) {
		if (write(wav_fd, b, n) < 0)
			snprintf(note, sizeof(note), "write failed: %s",
				 strerror(errno));
		wav_bytes += n;
	}
	if (have_carry && n) {
		int s = (short)((unsigned)carry | ((unsigned)b[0] << 8));
		int a = s < 0 ? -s : s;

		if (a > tick_peak)
			tick_peak = a;
		have_carry = 0;
		i = 1;
	}
	for (; i + 1 < n; i += 2) {
		int s = (short)((unsigned)b[i] | ((unsigned)b[i + 1] << 8));
		int a = s < 0 ? -s : s;

		if (a > tick_peak)
			tick_peak = a;
	}
	if (i < n) {
		carry = b[i];
		have_carry = 1;
	}
}

/* One entry, and the tick's accumulator starts again. */
static void rec_tick(void)
{
	double f = (double)tick_peak / 32768.0;

	if (f > 1.0)
		f = 1.0;
	if (nlvl == RC_HIST) {
		memmove(lvl, lvl + 1, sizeof(lvl) - sizeof(lvl[0]));
		nlvl--;
	}
	lvl[nlvl++] = f;
	tick_peak = 0;
}

/* ── the WAV ────────────────────────────────────────────────────────────── */

static void put32(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
	p[2] = (unsigned char)((v >> 16) & 0xff);
	p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void put16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

/*
 * The canonical 44-byte PCM header. Twelve fixed fields for one fixed format
 * is not a file format library, and writing it here removes a second `sox`, a
 * second pipe and two more reap paths.
 */
static int wav_open(const char *path)
{
	unsigned char h[44];
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	if (fd < 0)
		return -1;
	memcpy(h, "RIFF", 4);
	put32(h + 4, 36);
	memcpy(h + 8, "WAVEfmt ", 8);
	put32(h + 16, 16);			/* fmt chunk size          */
	put16(h + 20, 1);			/* PCM                     */
	put16(h + 22, 1);			/* mono                    */
	put32(h + 24, RC_RATE);
	put32(h + 28, RC_RATE * 2);		/* byte rate               */
	put16(h + 32, 2);			/* block align             */
	put16(h + 34, 16);			/* bits                    */
	memcpy(h + 36, "data", 4);
	put32(h + 40, 0);
	if (write(fd, h, sizeof(h)) != (ssize_t)sizeof(h)) {
		close(fd);
		return -1;
	}
	return fd;
}

/*
 * Both length fields, every tick. The alternative is a file that is unopenable
 * whenever the machine goes away mid-recording, which cannot be told apart
 * from a recorder that captured nothing.
 */
static void wav_sync_len(void)
{
	unsigned char n[4];

	if (wav_fd < 0)
		return;
	put32(n, (unsigned long)(wav_bytes + 36));
	if (pwrite(wav_fd, n, 4, 4) != 4)
		return;
	put32(n, (unsigned long)wav_bytes);
	if (pwrite(wav_fd, n, 4, 40) != 4)
		return;
}

/* ── the inputs ─────────────────────────────────────────────────────────── */

/*
 * `Default` first and always: it is the row that works while the session's
 * PipeWire holds the card, through pipewire-alsa. A `hw:` device is the one
 * that answers when nothing else has the card, which is the console and the
 * rig.
 */
static void scan_inputs(void)
{
	int n = 0;
	KprSoundPcm *p = kpr_sound_pcms(&n);

	nin = 0;
	snprintf(ins[nin].id, sizeof(ins[nin].id), "%s", "default");
	snprintf(ins[nin].name, sizeof(ins[nin].name), "%s",
		 "the card ALSA picks");
	nin++;
	for (int i = 0; i < n && nin < RC_MAX_IN; i++) {
		if (!p[i].capture)
			continue;
		snprintf(ins[nin].id, sizeof(ins[nin].id), "%s", p[i].id);
		snprintf(ins[nin].name, sizeof(ins[nin].name), "%s", p[i].name);
		nin++;
	}
	kpr_sound_free(p);
	if (sel_in >= nin)
		sel_in = nin ? nin - 1 : 0;
}

/* ── ~/Recordings ───────────────────────────────────────────────────────── */

static int rec_dir(char *out, size_t cap)
{
	const char *home = getenv("HOME");

	if (!home || !*home)
		return -1;
	snprintf(out, cap, "%s/Recordings", home);
	return 0;
}

static int by_name(const void *a, const void *b)
{
	/* Newest first: the names are a sortable timestamp, so reverse
	 * lexicographic IS reverse chronological and costs no stat. */
	return strcmp(((const struct rc_file *)b)->name,
		      ((const struct rc_file *)a)->name);
}

static void scan_files(void)
{
	char dir[PATH_MAX];
	DIR *d;
	struct dirent *e;

	nfile = 0;
	if (rec_dir(dir, sizeof(dir)) != 0)
		return;
	d = opendir(dir);
	if (!d)
		return;
	while ((e = readdir(d)) && nfile < RC_MAX_FILE) {
		char path[PATH_MAX];
		struct stat st;
		size_t l = strlen(e->d_name);

		if (l < 5 || strcmp(e->d_name + l - 4, ".wav"))
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		struct rc_file *f = &files[nfile++];

		snprintf(f->name, sizeof(f->name), "%s", e->d_name);
		f->size = (unsigned long long)st.st_size;
		/* The header is 44 bytes and the format is fixed, so a
		 * duration needs no parse. */
		f->secs = st.st_size > 44
				  ? (int)((st.st_size - 44) / (RC_RATE * 2))
				  : 0;
	}
	closedir(d);
	qsort(files, (size_t)nfile, sizeof(files[0]), by_name);
	if (sel_file >= nfile)
		sel_file = nfile ? nfile - 1 : 0;
}

/* ── the model ──────────────────────────────────────────────────────────── */

/*
 * A ggml file starts `lmgg`. THE GATE IS THE MAGIC, NOT THE NAME: a
 * half-finished download is then "no model" rather than a crash behind an
 * enabled button.
 */
static int is_model(const char *path)
{
	char m[4];
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t n;

	if (fd < 0)
		return 0;
	n = read(fd, m, 4);
	close(fd);
	return n == 4 && !memcmp(m, "lmgg", 4);
}

/* First `ggml-*.bin` in sorted name order that passes the gate. Sorted, not
 * newest: an mtime is not deterministic in a fixture and a name is. */
static int model_in_dir(const char *dir, char *out, size_t cap)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char best[NAME_MAX + 1] = "";

	if (!d)
		return 0;
	while ((e = readdir(d))) {
		char path[PATH_MAX];
		size_t l = strlen(e->d_name);

		if (l < 10 || strncmp(e->d_name, "ggml-", 5) ||
		    strcmp(e->d_name + l - 4, ".bin"))
			continue;
		if (*best && strcmp(e->d_name, best) >= 0)
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (is_model(path))
			snprintf(best, sizeof(best), "%s", e->d_name);
	}
	closedir(d);
	if (!*best)
		return 0;
	snprintf(out, cap, "%s/%s", dir, best);
	return 1;
}

/*
 * $KDOS_WHISPER_MODEL is the WHOLE answer when it is set — no directory is
 * searched behind it. Somebody who named a model and got a different one has
 * been lied to.
 */
static void find_model(void)
{
	const char *env = getenv("KDOS_WHISPER_MODEL");
	const char *dh = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	char dir[PATH_MAX];

	model[0] = '\0';
	if (env && *env) {
		snprintf(model_where, sizeof(model_where), "%s", env);
		if (is_model(env))
			snprintf(model, sizeof(model), "%s", env);
		return;
	}
	if (dh && *dh)
		snprintf(dir, sizeof(dir), "%s/whisper.cpp/models", dh);
	else if (home && *home)
		snprintf(dir, sizeof(dir), "%s/.local/share/whisper.cpp/models",
			 home);
	else
		dir[0] = '\0';
	snprintf(model_where, sizeof(model_where), "%s", dir);
	if (*dir && model_in_dir(dir, model, sizeof(model)))
		return;
	snprintf(model_where, sizeof(model_where), "%s", KDOS_WHISPER_DIR);
	model_in_dir(KDOS_WHISPER_DIR, model, sizeof(model));
}

/* ── the child ──────────────────────────────────────────────────────────── */

static void kid_stop(void)
{
	if (kid > 0) {
		/* SIGINT to the GROUP: sox closes the ALSA device on it, and
		 * a partly-copied stop leaves one holding the card so the next
		 * launch says "busy" with nothing on screen to explain it. */
		kill(-kid, SIGINT);
		waitpid(kid, NULL, 0);
	}
	if (kid_fd >= 0)
		close(kid_fd);
	if (kid_err >= 0)
		close(kid_err);
	kid = -1;
	kid_fd = -1;
	kid_err = -1;
}

/*
 * Its own session, stdout and stderr each on their own nonblocking pipe. The
 * two are separate because "device busy" and "a dead recorder that says
 * nothing" are different problems and only one of them is the person's fault.
 */
static int kid_start(const char *const argv[])
{
	int out[2], err[2];
	pid_t pid;

	kid_stop();
	if (!kb_have_prog(argv[0])) {
		snprintf(note, sizeof(note), "%s is not on this machine",
			 argv[0]);
		return -1;
	}
	if (pipe(out) != 0)
		return -1;
	if (pipe(err) != 0) {
		close(out[0]);
		close(out[1]);
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
		return -1;
	}
	if (pid == 0) {
		close(out[0]);
		close(err[0]);
		dup2(out[1], STDOUT_FILENO);
		dup2(err[1], STDERR_FILENO);
		if (out[1] > STDERR_FILENO)
			close(out[1]);
		if (err[1] > STDERR_FILENO)
			close(err[1]);
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(out[1]);
	close(err[1]);
	fcntl(out[0], F_SETFL, O_NONBLOCK);
	fcntl(err[0], F_SETFL, O_NONBLOCK);
	kid_fd = out[0];
	kid_err = err[0];
	kid = pid;
	return 0;
}

/* The child's last line of stderr becomes the note. Drained every tick: a
 * surface that let the 64 KB pipe fill would stall the recorder it is
 * metering. */
static void kid_drain_err(void)
{
	char buf[1024];
	ssize_t n;

	if (kid_err < 0)
		return;
	while ((n = read(kid_err, buf, sizeof(buf) - 1)) > 0) {
		char *p;

		buf[n] = '\0';
		while ((p = strrchr(buf, '\n')) && p == buf + strlen(buf) - 1)
			*p = '\0';
		p = strrchr(buf, '\n');
		snprintf(note, sizeof(note), "%s", p ? p + 1 : buf);
	}
}

static void rec_start(void)
{
	char dir[PATH_MAX], stamp[32];
	time_t t = time(NULL);
	struct tm tm;
	const char *dev = nin ? ins[sel_in].id : "default";
	const char *argv[20];
	int n = 0;

	note[0] = '\0';
	if (sh_mic_muted()) {
		snprintf(note, sizeof(note),
			 "the input is muted: press m first");
		return;
	}
	if (rec_dir(dir, sizeof(dir)) != 0 || kb_mkdir_p(dir) != 0) {
		snprintf(note, sizeof(note), "no ~/Recordings to write into");
		return;
	}
	localtime_r(&t, &tm);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d-%H%M%S", &tm);
	snprintf(wav_path, sizeof(wav_path), "%s/%s.wav", dir, stamp);
	wav_fd = wav_open(wav_path);
	if (wav_fd < 0) {
		snprintf(note, sizeof(note), "cannot write %s", wav_path);
		return;
	}
	wav_bytes = 0;
	nlvl = 0;
	tick_peak = 0;
	have_carry = 0;

	argv[n++] = "sox";
	argv[n++] = "-q";
	/* A tenth of a second, so a block arrives per tick. The default of
	 * 8192 frames is a quarter of a second and the meter steps. */
	argv[n++] = "--input-buffer";
	argv[n++] = "3200";
	argv[n++] = "-t";
	argv[n++] = "alsa";
	argv[n++] = dev;
	argv[n++] = "-t";
	argv[n++] = "raw";
	argv[n++] = "-e";
	argv[n++] = "signed";
	argv[n++] = "-b";
	argv[n++] = "16";
	argv[n++] = "-c";
	argv[n++] = "1";
	argv[n++] = "-r";
	argv[n++] = "16000";
	argv[n++] = "-";
	argv[n] = NULL;
	if (kid_start(argv) != 0) {
		close(wav_fd);
		wav_fd = -1;
		unlink(wav_path);
		return;
	}
	rec_start_ms = now_ms();
}

static void rec_finish(void)
{
	kid_stop();
	if (wav_fd >= 0) {
		wav_sync_len();
		close(wav_fd);
		wav_fd = -1;
	}
	scan_files();
}

/* ── transcription ──────────────────────────────────────────────────────── */

/*
 * Batch, over a closed file. `-of` names the output so it is ours rather than
 * whisper's guess, and `-np -nt` drop the progress bar and the timestamps that
 * a pane of prose does not want.
 *
 * THE EXIT STATUS IS THE RESULT. What the words say is not checked here and
 * has never been checked on this tree, because no model ships.
 */
static void transcribe(void)
{
	char dir[PATH_MAX], wav[PATH_MAX], base[PATH_MAX], txt[PATH_MAX];
	char ncpu[16];
	const char *argv[16];
	int n = 0, st = 0;
	pid_t pid;
	size_t l;

	note[0] = '\0';
	if (!*model || !nfile)
		return;
	if (rec_dir(dir, sizeof(dir)) != 0)
		return;
	snprintf(wav, sizeof(wav), "%s/%s", dir, files[sel_file].name);
	snprintf(base, sizeof(base), "%s", wav);
	l = strlen(base);
	if (l > 4)
		base[l - 4] = '\0';
	snprintf(txt, sizeof(txt), "%s.txt", base);
	snprintf(ncpu, sizeof(ncpu), "%d", (int)sysconf(_SC_NPROCESSORS_ONLN));

	argv[n++] = "whisper-cli";
	argv[n++] = "-m";
	argv[n++] = model;
	argv[n++] = "-f";
	argv[n++] = wav;
	argv[n++] = "-otxt";
	argv[n++] = "-of";
	argv[n++] = base;
	argv[n++] = "-np";
	argv[n++] = "-nt";
	argv[n++] = "-t";
	argv[n++] = ncpu;
	argv[n] = NULL;

	if (!kb_have_prog(argv[0])) {
		snprintf(note, sizeof(note), "%s is not on this machine",
			 argv[0]);
		return;
	}
	snprintf(note, sizeof(note), "transcribing %s…", files[sel_file].name);
	pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
			close(null);
		}
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		snprintf(note, sizeof(note), "%s failed", argv[0]);
		return;
	}

	char *body = kb_read_whole(txt, NULL);

	if (!body) {
		snprintf(note, sizeof(note), "%s wrote nothing", argv[0]);
		return;
	}
	snprintf(transcript, sizeof(transcript), "%s", body);
	free(body);
	showing_transcript = 1;
	note[0] = '\0';
}

static void play_sel(void)
{
	char dir[PATH_MAX], wav[PATH_MAX];
	const char *argv[6];
	int n = 0;
	pid_t pid;

	note[0] = '\0';
	if (!nfile || rec_dir(dir, sizeof(dir)) != 0)
		return;
	snprintf(wav, sizeof(wav), "%s/%s", dir, files[sel_file].name);
	argv[n++] = "play";
	argv[n++] = "-q";
	argv[n++] = wav;
	argv[n] = NULL;
	if (!kb_have_prog(argv[0])) {
		snprintf(note, sizeof(note), "%s is not on this machine",
			 argv[0]);
		return;
	}
	/* DOUBLE FORK, so nothing has to be reaped later. The surface stays
	 * live while the file plays, and a `waitpid(-1)` in the loop would
	 * race the recorder's own child for its exit status. */
	pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		if (fork() != 0)
			_exit(0);
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
			close(null);
		}
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	waitpid(pid, NULL, 0);
}

static void delete_sel(void)
{
	char dir[PATH_MAX], path[PATH_MAX];
	size_t l;

	if (!nfile || rec_dir(dir, sizeof(dir)) != 0)
		return;
	snprintf(path, sizeof(path), "%s/%s", dir, files[sel_file].name);
	unlink(path);
	l = strlen(path);
	if (l > 4) {
		snprintf(path + l - 4, sizeof(path) - (l - 4), ".txt");
		unlink(path);
	}
	showing_transcript = 0;
	transcript[0] = '\0';
	scan_files();
}

/* ── the frame ──────────────────────────────────────────────────────────── */

enum { RB_REC = 0, RB_TRANS, RB_PLAY, RB_DEL, RB_CLOSE, RB_N };

static int rec_buttons(int w, int row)
{
	struct kch_button b[RB_N];

	b[RB_REC].label = kid > 0 ? "Stop" : "Record";
	b[RB_REC].enabled = nin > 0;
	b[RB_TRANS].label = "Transcribe";
	/* THE MODEL GATE ALONE. Whether `whisper-cli` is on $PATH is not asked
	 * here: $PATH is not frozen in a golden, and a button that changes
	 * shade with the host's packages cannot have a reference frame. */
	b[RB_TRANS].enabled = *model != '\0' && nfile > 0 && kid < 0;
	b[RB_PLAY].label = "Play";
	b[RB_PLAY].enabled = nfile > 0 && kid < 0;
	b[RB_DEL].label = "Delete";
	b[RB_DEL].enabled = nfile > 0 && kid < 0;
	b[RB_CLOSE].label = "Close";
	b[RB_CLOSE].enabled = 1;
	return kch_buttons(w, row, b, RB_N, -1);
}

static void draw_inputs(int y, int rows, int w)
{
	int muted = sh_mic_muted();

	for (int i = 0; i < rows; i++) {
		int idx = i;

		if (idx >= nin)
			break;
		int on = idx == sel_in;
		int fg = on ? KT_ACCENT : KT_TEXT;

		ktui_draw_text(2, y + i, 2, on ? ">" : " ", KT_ACCENT,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_text(4, y + i, 18, ins[idx].id, fg, KT_SURFACE,
			       on ? KT_A_BOLD : KT_A_NONE);
		ktui_draw_text(22, y + i, w - 32, ins[idx].name, KT_MID,
			       KT_SURFACE, KT_A_NONE);
		/* A recorder that quietly records a muted input is the worst
		 * thing this surface can do, so the state is on the row and
		 * Record refuses while it is up. */
		if (muted)
			ktui_draw_text_right(0, y + i, w - 2, "MUTED", KT_WARN,
					     KT_SURFACE, KT_A_NONE);
	}
	if (!nin)
		ktui_draw_text(4, y, w - 8, "no capture device on this machine",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
}

static void draw_level(int y, int w)
{
	int spark_w = w - 8;
	double peak;
	int mm, ss;

	if (spark_w < 4)
		spark_w = 4;
	if (spark_w > RC_HIST)
		spark_w = RC_HIST;
	/* vmax PINNED. `lvl[]` is already a fraction of full scale, and an
	 * autoscaled meter paints a noise floor as a solid bar. */
	ktui_sparkline(krect(4, y, spark_w, 1), lvl, nlvl, 1.0, KT_SURFACE);
	peak = ktui_sparkline_peak(lvl, nlvl, spark_w);

	long long ms = kid > 0 ? now_ms() - rec_start_ms : 0;

	if (!nlvl)
		ms = 0;
	mm = (int)(ms / 60000);
	ss = (int)(ms / 1000 % 60);
	ktui_draw_textf(4, y + 1, w - 8, KT_MID, KT_SURFACE, KT_A_NONE,
			"%d:%02d  %s  peak %s", mm, ss,
			kb_human_size(wav_bytes), rec_db_text(peak));
}

static void draw_files(int y, int rows, int w)
{
	for (int i = 0; i < rows; i++) {
		int idx = file_top + i;

		if (idx >= nfile)
			break;
		int on = idx == sel_file;
		char dur[16];

		snprintf(dur, sizeof(dur), "%d:%02d", files[idx].secs / 60,
			 files[idx].secs % 60);
		ktui_draw_text(2, y + i, 2, on ? ">" : " ", KT_ACCENT,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_text(4, y + i, w - 24, files[idx].name,
			       on ? KT_ACCENT : KT_TEXT, KT_SURFACE,
			       on ? KT_A_BOLD : KT_A_NONE);
		ktui_draw_text_right(0, y + i, w - 12,
				     kb_human_size(files[idx].size), KT_MID,
				     KT_SURFACE, KT_A_NONE);
		ktui_draw_text_right(0, y + i, w - 2, dur, KT_MID, KT_SURFACE,
				     KT_A_NONE);
	}
	if (!nfile)
		ktui_draw_text(4, y, w - 8, "nothing recorded yet", KT_DIM,
			       KT_SURFACE, KT_A_NONE);
}

/* The transcript replaces the RECORDINGS group at the same height, so a
 * reference frame of one is a reference frame of both shapes. */
static void draw_transcript(int y, int rows, int w)
{
	const char *p = transcript;
	int row = 0;

	/* An empty pane reads as a broken feature. A model that ran and
	 * produced nothing is a real answer and has to be given as one. */
	if (!*p) {
		ktui_draw_text(4, y, w - 8, "the model returned no words",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
		return;
	}
	while (*p && row < rows) {
		int take = 0, last = 0;

		while (p[take] && p[take] != '\n' && take < w - 8) {
			if (p[take] == ' ')
				last = take;
			take++;
		}
		if (p[take] && p[take] != '\n' && last)
			take = last;
		ktui_draw_text(4, y + row, take, p, KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		p += take;
		while (*p == ' ' || *p == '\n')
			p++;
		row++;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	char sub[256];

	if (w < 40 || h < 14) {
		ktui_toosmall("kdos-rec", 40, 14);
		ktui_draw_flush();
		return;
	}
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	sh_frame(w, h, "Recording", KT_ACCENT, KT_SURFACE, 1);

	/* The subtitle names the model that was found, or the directory that
	 * was searched. A greyed control that will not say where it looked
	 * teaches that the feature is broken. */
	{
		const char *dev = nin ? ins[sel_in].id : "no input";
		const char *nm = nin ? ins[sel_in].name : "";
		const char *slash = *model ? strrchr(model, '/') : NULL;

		if (*model)
			snprintf(sub, sizeof(sub), "%s %.28s · 16 kHz mono · %s",
				 dev, nm, slash ? slash + 1 : model);
		else
			snprintf(sub, sizeof(sub),
				 "%s %.28s · 16 kHz mono · no model in %s", dev,
				 nm, model_where);
	}
	int body_y = kch_header(w, "audio-input-microphone", "Recording", sub,
				kicon_enabled());

	int rows_left = h - 3 - body_y;
	int want_in = nin < 1 ? 1 : nin > 6 ? 6 : nin;
	int n_in = want_in;

	/* INPUT gives its rows back first: a level with no chart and a list
	 * with no files are both useless, and the meter is why this window is
	 * open. */
	while (n_in > 1 && rows_left - (1 + n_in) - 3 - 1 < 1)
		n_in--;

	int in_hdr = body_y;
	int lv_hdr, lv_y, fl_hdr;

	in_y = in_hdr + 1;
	in_rows = n_in;
	lv_hdr = in_y + in_rows;
	lv_y = lv_hdr + 1;
	fl_hdr = lv_y + 2;
	fl_y = fl_hdr + 1;
	fl_rows = h - 3 - fl_y;
	if (fl_rows < 1)
		fl_rows = 1;

	kch_group(2, in_hdr, w - 4, "Input");
	draw_inputs(in_y, in_rows, w);
	kch_group(2, lv_hdr, w - 4, "Level");
	draw_level(lv_y, w);
	kch_group(2, fl_hdr, w - 4,
		  showing_transcript ? "Transcript" : "Recordings");
	if (showing_transcript)
		draw_transcript(fl_y, fl_rows, w);
	else {
		kch_list_clamp(&file_top, sel_file, nfile, fl_rows, 1);
		draw_files(fl_y, fl_rows, w);
		kch_scrollbar(0, w - 2, fl_y, fl_rows, nfile, file_top,
			      KT_SURFACE);
	}

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);

	int bx = rec_buttons(w, h - 2);
	int room = bx - 3;

	if (asking) {
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_SURFACE);
		if (room >= 8)
			ktui_draw_textf(2, h - 2, room, KT_WARN, KT_SURFACE,
					KT_A_NONE, "delete %s?  y/n",
					nfile ? files[sel_file].name : "");
	} else if (note[0]) {
		/* The message outranks the row and shares its cells; the row
		 * is still called, with an empty rect, because that is what
		 * clears the pool. */
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_SURFACE);
		if (room >= 8)
			ktui_draw_text(2, h - 2, room, note, KT_WARN,
				       KT_SURFACE, KT_A_NONE);
	} else if (room > 0) {
		ktui_hint("Enter", kid > 0 ? "stop" : "record");
		ktui_hint("m", sh_mic_muted() ? "unmute" : "mute");
		ktui_hint_if(*model != '\0', "t", "transcribe");
		ktui_hint("Esc", ktui_esc_verb(&keys));
		ktui_hint_row(&keys, krect(2, h - 2, room, 1), KT_SURFACE);
	}
	ktui_draw_flush();
}

/* ── the Esc ladder ─────────────────────────────────────────────────────── */

static int live_up(void *user)
{
	(void)user;
	return kid > 0;
}

static void live_close(void *user)
{
	(void)user;
	rec_finish();
}

static int ask_up(void *user)
{
	(void)user;
	return asking;
}

static void ask_cancel(void *user)
{
	(void)user;
	asking = 0;
}

/* ── the test seams ─────────────────────────────────────────────────────── */

/*
 * Drive the same arithmetic the live meter runs from a file of raw s16, one
 * tick per RC_TICK_SAMPLES. This is the answer to "the rig has no microphone":
 * the peak, the normalisation, the ring and the WAV writer are proved where a
 * capture device is not needed, and only the plumbing is left for the guest.
 *
 * `report` prints one line per tick; the golden path drains silently before
 * its single draw so the frame is the only thing on stdout.
 *
 * NEITHER SEAM TOUCHES $HOME. `out` is the only file written and the caller
 * names it.
 */
static int meter_file(const char *path, const char *out, int report)
{
	unsigned char buf[RC_TICK_SAMPLES * 2];
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	int tick = 0;

	if (fd < 0) {
		fprintf(stderr, "kdos-rec: %s: %s\n", path, strerror(errno));
		return 1;
	}
	if (out) {
		wav_fd = wav_open(out);
		if (wav_fd < 0) {
			fprintf(stderr, "kdos-rec: %s: %s\n", out,
				strerror(errno));
			close(fd);
			return 1;
		}
	}
	for (;;) {
		ssize_t n = read(fd, buf, sizeof(buf));
		int peak;

		if (n <= 0)
			break;
		rec_take(buf, (size_t)n);
		peak = tick_peak;
		rec_tick();
		if (report)
			printf("%d %d %s\n", tick, peak,
			       rec_db_text((double)peak / 32768.0));
		tick++;
	}
	close(fd);
	if (wav_fd >= 0) {
		wav_sync_len();
		close(wav_fd);
		wav_fd = -1;
	}
	return 0;
}

/* ── main ───────────────────────────────────────────────────────────────── */

static void usage(void)
{
	fprintf(stderr,
		"usage: kdos-rec [--font NAME] [--input hw:C,D] [--dump]\n"
		"                [--fixture DIR] [--meter FILE] [--write OUT]\n");
}

int rec_main(int argc, char **argv)
{
	const char *font = NULL, *fixture = NULL, *meter = NULL, *out = NULL;
	const char *want = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--input") && i + 1 < argc)
			want = argv[++i];
		else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
			fixture = argv[++i];
		else if (!strcmp(argv[i], "--meter") && i + 1 < argc)
			meter = argv[++i];
		else if (!strcmp(argv[i], "--write") && i + 1 < argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else {
			usage();
			return 2;
		}
	}
	if (fixture) {
		char p[PATH_MAX];

		snprintf(p, sizeof(p), "%s/proc", fixture);
		kpr_root_set(p, NULL);
	}

	find_model();
	scan_inputs();
	if (want)
		for (int i = 0; i < nin; i++)
			if (!strcmp(ins[i].id, want)) {
				sel_in = i;
				break;
			}

	/* The meter with no frame: the arithmetic on stdout and nothing else. */
	if (meter && !dump)
		return meter_file(meter, out, 1);

	scan_files();

	/* Registration order IS the order Esc unwinds, innermost last. Escape
	 * with a live child stops the recorder rather than closing a window
	 * that is holding the microphone. */
	ktui_keys_layer(&keys, "Stop", live_up, live_close, NULL);
	ktui_keys_layer(&keys, "Cancel", ask_up, ask_cancel, NULL);
	keys.doc = "rec";
	keys.help = sh_help;

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = RC_COLS,
		.rows = RC_ROWS,
		.app_id = "kdos-rec",
		/* The numbers this surface's own too-small check uses: one
		 * answer to the smallest grid it can compose on, told to the
		 * session that decides the size rather than only found out
		 * after it has decided. */
		.min_cols = 40,
		.min_rows = 14,
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		if (meter)
			meter_file(meter, out, 0);
		ktui_offscreen_init(RC_COLS, RC_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-rec: no display server\n");
		return 1;
	}
	ktui_draw_init();

	long long last_tick = now_ms();

	while (!kdisp_should_close()) {
		if (kid > 0) {
			unsigned char buf[8192];
			ssize_t n;
			int gone = 0;

			while ((n = read(kid_fd, buf, sizeof(buf))) > 0)
				rec_take(buf, (size_t)n);
			if (n == 0)
				gone = 1;
			kid_drain_err();
			if (now_ms() - last_tick >= RC_TICK_MS) {
				last_tick = now_ms();
				rec_tick();
				wav_sync_len();
			}
			if (gone) {
				rec_finish();
				if (!note[0])
					snprintf(note, sizeof(note),
						 "the recorder stopped");
			}
		}
		draw();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, kid > 0 ? 50 : 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				break;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		if (asking) {
			if (ev.key == 'y' || ev.key == 'Y') {
				delete_sel();
				asking = 0;
			} else if (ev.key == 'n' || ev.key == 'N') {
				asking = 0;
			}
			continue;
		}

		switch (ev.key) {
		case KT_K_UP:
			if (sel_in > 0)
				sel_in--;
			break;
		case KT_K_DOWN:
			if (sel_in + 1 < nin)
				sel_in++;
			break;
		case KT_K_PGUP:
			if (sel_file > 0)
				sel_file--;
			break;
		case KT_K_PGDN:
			if (sel_file + 1 < nfile)
				sel_file++;
			break;
		case KT_K_ENTER:
			if (kid > 0)
				rec_finish();
			else
				rec_start();
			break;
		default:
			if (ev.mods & (KT_MOD_CTRL | KT_MOD_ALT))
				break;
			switch (ev.key) {
			case 'm':
				sh_mic_toggle();
				break;
			case 't':
				if (*model && kid < 0)
					transcribe();
				break;
			case 'p':
				if (kid < 0)
					play_sel();
				break;
			case 'd':
				if (nfile && kid < 0)
					asking = 1;
				break;
			case 'r':
				showing_transcript = 0;
				find_model();
				scan_inputs();
				scan_files();
				note[0] = '\0';
				break;
			}
			break;
		}
	}

	rec_finish();
	kdisp_shutdown();
	return 0;
}
