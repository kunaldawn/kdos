/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   `kdos speech` — transcription and reading aloud, on this machine
 *
 *   kdos speech transcribe [--notify] [--] FILE...   a .txt beside each file
 *   kdos speech say [--voice NAME] [--] [TEXT...]    read TEXT, or stdin, aloud
 *   kdos speech sync                                 Klipper's Read Aloud action
 *   kdos speech list            the transcription models, and which are here
 *   kdos speech get base.en     fetch one into the user's own data directory
 *   kdos speech where           the directories searched, and the models in each
 *   kdos speech remove NAME     delete one
 *
 * EVERYTHING RUNS HERE AND NOTHING IS SENT ANYWHERE: whisper.cpp hears and
 * Piper speaks, each from a model file on the disk. Dolphin's Transcribe
 * action runs `transcribe`, and Klipper's Read Aloud action runs `say`.
 *
 * ONE MODEL SHIPS: base.en, from the whisper-model-base-en port, in
 * /usr/share/whisper.cpp/models. Every other one is a personal choice —
 * language, size and the trade between the two, from 32 MB to 3 GB — so the
 * image carries the English default and this carries the way to get the
 * rest.
 *
 * IT WRITES TO THE USER'S DATA DIRECTORY AND NEVER TO /usr. A model is found
 * at $KDOS_WHISPER_MODEL, then in $XDG_DATA_HOME/whisper.cpp/models, then in
 * /usr/share/whisper.cpp/models; `get` writes the second, which needs no
 * privilege and is what a per-user choice should be. A model in the third is
 * a model somebody packaged.
 *
 * THE DOWNLOAD IS VERIFIED AGAINST A SHA256 IN THIS FILE, and that is the
 * whole of the trust: Hugging Face serves these over TLS and signs nothing,
 * so what a checksum here buys is that the bytes are the bytes this tree was
 * written against — a substituted or truncated file is refused. It does not
 * make the upstream trustworthy, and the rule the rest of the system keeps
 * applies: see the security model on unsigned content.
 *
 * A NAME IS LOOKED UP IN THE TABLE AND NEVER INTERPOLATED INTO A URL. The
 * argument reaches this from a command line and from a settings page; a name
 * pasted into a URL is a name that can name another host.
 * ---------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kdos-tools.h"

/*
 * WHERE THEY COME FROM. Upstream's own repository, which is what
 * `models/download-ggml-model.sh` reaches for — the same bytes by the same
 * route, so a model fetched here and one fetched by that script are the same
 * file.
 */
#define HF_BASE "https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

/* A ggml file starts `lmgg`. A checksum says the bytes arrived and the magic says they are a
 * model, and a download that passed one and failed the other is worth saying
 * out loud rather than leaving for the button to discover. */
#define GGML_MAGIC "lmgg"

struct model {
	const char *name;
	long long bytes;
	const char *sha256;
};

/*
 * THE CATALOGUE, SMALLEST FIRST, and it is a subset rather than the whole
 * repository. What is here is every English-only size, the three smallest
 * multilingual ones, and the two large ones worth the disk; the quantised
 * `q5_1` rows are the ones that matter on a laptop, at about 40% of the size
 * for a difference most people cannot hear.
 *
 * `.en` IS ENGLISH-ONLY AND IT IS BETTER AT IT. A multilingual model of the
 * same size spends capacity on languages it is not being asked for, so
 * `base.en` beats `base` on English and cannot do anything else at all.
 */
static const struct model MODELS[] = {
	{ "tiny.en-q5_1",    32166155, "c77c5766f1cef09b6b7d47f21b546cbddd4157886b3b5d6d4f709e91e66c7c2b" },
	{ "tiny.en",         77704715, "921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f" },
	{ "tiny",            77691713, "be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21" },
	{ "base.en-q5_1",    59721011, "4baf70dd0d7c4247ba2b81fafd9c01005ac77c2f9ef064e00dcf195d0e2fdd2f" },
	{ "base.en",        147964211, "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002" },
	{ "base",           147951465, "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe" },
	{ "small.en-q5_1",  190098681, "bfdff4894dcb76bbf647d56263ea2a96645423f1669176f4844a1bf8e478ad30" },
	{ "small.en",       487614201, "c6138d6d58ecc8322097e0f987c32f1be8bb0a18532a3f88f734d1bbf9c41e5d" },
	{ "small",          487601967, "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b" },
	{ "medium.en",     1533774781, "cc37e93478338ec7700281a7ac30a10128929eb8f427dda2e865faa8f6da4356" },
	{ "large-v3-turbo",1624555275, "1fc70f774d38eb169993ac391eea357ef47c88757ef72ee5943879b7e8e2bc69" },
	{ "large-v3",      3095033483, "64d182b440b98d5203c4f9bd541544d84c605196c4f7b845dfa11fb23594d1e2" },
};
#define NMODELS ((int)(sizeof(MODELS) / sizeof(MODELS[0])))

/* The one a person who has not chosen should get: English, small enough to
 * download over a domestic line, good enough to read back. */
#define DEFAULT_MODEL "base.en"

static const struct model *find_model(const char *name)
{
	for (int i = 0; i < NMODELS; i++)
		if (!strcmp(MODELS[i].name, name))
			return &MODELS[i];
	return NULL;
}

/*
 * $XDG_DATA_HOME/whisper.cpp/models, or $HOME/.local/share/... — the second
 * place a model is searched for. NOT the third:
 * /usr/share is root's and a model is a per-user choice. The third is only
 * read, so `list` and `where` report the model the image carries.
 */
#define SYSTEM_MODEL_DIR "/usr/share/whisper.cpp/models"

static char *model_dir(void)
{
	const char *dh = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");

	if (dh && *dh)
		return kb_path_join(dh, "whisper.cpp/models");
	if (home && *home)
		return kb_path_join(home, ".local/share/whisper.cpp/models");
	kb_die("neither $XDG_DATA_HOME nor $HOME is set");
	return NULL;
}

static char *model_path_in(const char *dir, const char *name)
{
	char leaf[128];

	snprintf(leaf, sizeof(leaf), "ggml-%s.bin", name);
	return kb_path_join(dir, leaf);
}

static char *model_path(const char *name)
{
	char *dir = model_dir();
	char *p = model_path_in(dir, name);

	free(dir);
	return p;
}

static int is_ggml(const char *path)
{
	char m[4];
	FILE *f = fopen(path, "rb");
	size_t n;

	if (!f)
		return 0;
	n = fread(m, 1, 4, f);
	fclose(f);
	return n == 4 && !memcmp(m, GGML_MAGIC, 4);
}

/* Megabytes, because a model is never usefully described in bytes and a
 * column of nine-digit numbers is a column nobody reads. */
static void human(long long b, char *out, size_t n)
{
	if (b >= 1024LL * 1024 * 1024)
		snprintf(out, n, "%.1f GB", (double)b / (1024 * 1024 * 1024));
	else
		snprintf(out, n, "%lld MB", b / (1024 * 1024));
}

static int cmd_list(void)
{
	char *dir = model_dir();

	printf("Models for whisper.cpp, in %s\n\n", dir);
	for (int i = 0; i < NMODELS; i++) {
		char *p = model_path(MODELS[i].name);
		char *sp = model_path_in(SYSTEM_MODEL_DIR, MODELS[i].name);
		char sz[32];
		int here = is_ggml(p), shipped = is_ggml(sp);

		human(MODELS[i].bytes, sz, sizeof(sz));
		printf("  %-16s %9s  %s\n", MODELS[i].name, sz,
		       here ? "installed" : shipped ? "shipped" : "");
		free(p);
		free(sp);
	}
	printf("\n  kdos speech get %s\n", DEFAULT_MODEL);
	free(dir);
	return 0;
}

static int cmd_where(void)
{
	char *dir = model_dir();
	const char *env = getenv("KDOS_WHISPER_MODEL");

	if (env && *env) {
		printf("%s\n", env);
		printf("  named by $KDOS_WHISPER_MODEL, which is the whole "
		       "answer: no directory is searched behind it\n");
		return is_ggml(env) ? 0 : 1;
	}
	/* In search order: any model in the first directory is used before
	 * every model in the second. */
	const char *dirs[2] = { dir, SYSTEM_MODEL_DIR };

	for (int d = 0; d < 2; d++) {
		printf("%s\n", dirs[d]);
		for (int i = 0; i < NMODELS; i++) {
			char *p = model_path_in(dirs[d], MODELS[i].name);

			if (is_ggml(p))
				printf("  %s\n", p);
			free(p);
		}
	}
	free(dir);
	return 0;
}

/*
 * THE FETCH. curl writes to a `.part` beside the destination and the rename
 * happens only after both checks pass, so an interrupted download is never a
 * file the magic check has to reject — and a model directory never holds a
 * truncated file under the name of a good one.
 */
static int cmd_get(const char *name)
{
	const struct model *m = find_model(name);
	char *dir, *dest, *part, url[512], sum[256] = { 0 }, sz[32];
	KbArgv c = { 0 }, s = { 0 };
	char *sp;

	if (!m) {
		fprintf(stderr, "kdos speech: no model named '%s' — "
				"try: kdos speech list\n", name);
		return 1;
	}
	if (!kb_have_prog("curl")) {
		fprintf(stderr, "kdos speech: curl is not on $PATH\n");
		return 1;
	}

	dir = model_dir();
	dest = model_path(name);
	if (is_ggml(dest)) {
		printf("%s is already here: %s\n", name, dest);
		return 0;
	}
	if (kb_mkdir_p(dir) != 0)
		kb_die("cannot create %s: %s", dir, strerror(errno));

	part = kb_path_join(dir, "download.part");
	snprintf(url, sizeof(url), "%s/ggml-%s.bin", HF_BASE, m->name);
	human(m->bytes, sz, sizeof(sz));
	printf("==> %s (%s)\n    %s\n", m->name, sz, url);

	kb_proc_verbose = 1;
	kb_argv_add(&c, "curl");
	/* -f so an HTML error page is not written into the model file, -L
	 * because the repository redirects to its object store, and no -C:
	 * a resumed download of a file that changed upstream is a file that
	 * fails the checksum with no way to say which half is wrong. */
	kb_argv_add(&c, "-fL");
	kb_argv_add(&c, "--progress-bar");
	kb_argv_add(&c, "-o");
	kb_argv_add(&c, part);
	kb_argv_add(&c, "--");
	kb_argv_add(&c, url);
	kb_argv_end(&c);
	if (kb_run_tty(&c) != 0) {
		unlink(part);
		fprintf(stderr, "kdos speech: the download failed\n");
		return 1;
	}

	printf("==> Verifying\n");
	kb_argv_add(&s, "sha256sum");
	kb_argv_add(&s, "--");
	kb_argv_add(&s, part);
	kb_argv_end(&s);
	if (kb_run_capture(&s, sum, sizeof(sum)) != 0) {
		unlink(part);
		kb_die("sha256sum failed");
	}
	sp = strchr(sum, ' ');
	if (sp)
		*sp = '\0';
	if (strcmp(sum, m->sha256)) {
		unlink(part);
		fprintf(stderr, "kdos speech: sha256 mismatch — the file was "
				"NOT installed\n  expected: %s\n  actual:   "
				"%s\n", m->sha256, sum);
		return 1;
	}
	if (!is_ggml(part)) {
		unlink(part);
		fprintf(stderr, "kdos speech: the file is not a ggml model\n");
		return 1;
	}
	if (rename(part, dest) != 0) {
		unlink(part);
		kb_die("cannot put it at %s: %s", dest, strerror(errno));
	}
	printf("==> %s\n", dest);
	printf("    `kdos speech transcribe` uses it from now on.\n");
	return 0;
}

static int cmd_remove(const char *name)
{
	char *dest;

	if (!find_model(name)) {
		fprintf(stderr, "kdos speech: no model named '%s'\n", name);
		return 1;
	}
	dest = model_path(name);
	if (unlink(dest) != 0) {
		fprintf(stderr, "kdos speech: %s: %s\n", dest,
			strerror(errno));
		return 1;
	}
	printf("removed %s\n", dest);
	return 0;
}

static int speech_usage(int rc);

/* ── transcribe ────────────────────────────────────────────────────────── */

/*
 * The model to hear with, in search order: $KDOS_WHISPER_MODEL, then the
 * person's directory, then the image's. Within one directory the largest
 * catalogue model wins: a person who fetched a bigger one fetched it to use.
 */
static char *find_whisper(void)
{
	const char *env = getenv("KDOS_WHISPER_MODEL");
	char *dirs[2] = { model_dir(), kb_strdup(SYSTEM_MODEL_DIR) };
	char *hit = NULL;

	if (env && *env && is_ggml(env)) {
		free(dirs[0]);
		free(dirs[1]);
		return kb_strdup(env);
	}
	for (int d = 0; d < 2 && !hit; d++)
		for (int i = NMODELS - 1; i >= 0 && !hit; i--) {
			char *p = model_path_in(dirs[d], MODELS[i].name);

			if (is_ggml(p))
				hit = p;
			else
				free(p);
		}
	free(dirs[0]);
	free(dirs[1]);
	return hit;
}

/* FILE.txt, or FILE.2.txt, FILE.3.txt… — never a text a person already has.
 * Returns the stem whisper-cli is given (it adds the .txt itself). */
static char *text_stem(const char *file)
{
	char path[4096];

	snprintf(path, sizeof(path), "%s.txt", file);
	if (!kb_path_exists(path)) {
		snprintf(path, sizeof(path), "%s", file);
		return kb_strdup(path);
	}
	for (int n = 2; n < 1000; n++) {
		snprintf(path, sizeof(path), "%s.%d.txt", file, n);
		if (!kb_path_exists(path)) {
			snprintf(path, sizeof(path), "%s.%d", file, n);
			return kb_strdup(path);
		}
	}
	return NULL;
}

/*
 * One file: decoded by ffmpeg to the 16 kHz mono PCM whisper reads, into a
 * private file in the runtime directory that is removed on every path out,
 * then heard. Returns the text file's path, or NULL with `why` filled.
 */
static char *transcribe_one(const char *model, const char *file, char *why,
			    size_t n)
{
	char wav[512], *stem, *out = NULL;
	KbArgv f = { 0 }, w = { 0 };
	int fd;

	if (!kb_path_exists(file)) {
		snprintf(why, n, "%s: no such file", file);
		return NULL;
	}
	if (!(stem = text_stem(file))) {
		snprintf(why, n, "%s: no free name for its text", file);
		return NULL;
	}
	snprintf(wav, sizeof(wav), "%s/kdos-transcribe.XXXXXX.wav",
		 kb_runtime_dir());
	fd = mkstemps(wav, 4);
	if (fd < 0) {
		snprintf(why, n, "%.300s: %s", wav, strerror(errno));
		free(stem);
		return NULL;
	}
	close(fd);
	kb_argv_add(&f, "ffmpeg");
	kb_argv_add(&f, "-nostdin");
	kb_argv_add(&f, "-v");
	kb_argv_add(&f, "error");
	kb_argv_add(&f, "-y");
	kb_argv_add(&f, "-i");
	kb_argv_add(&f, file);
	kb_argv_add(&f, "-ar");
	kb_argv_add(&f, "16000");
	kb_argv_add(&f, "-ac");
	kb_argv_add(&f, "1");
	kb_argv_add(&f, "-c:a");
	kb_argv_add(&f, "pcm_s16le");
	kb_argv_add(&f, wav);
	kb_argv_end(&f);
	if (kb_run(&f) != 0) {
		snprintf(why, n, "%s: ffmpeg could not decode it",
			 kb_basename(file));
		goto done;
	}
	kb_argv_add(&w, "whisper-cli");
	kb_argv_add(&w, "-m");
	kb_argv_add(&w, model);
	kb_argv_add(&w, "-np");
	kb_argv_add(&w, "-otxt");
	kb_argv_add(&w, "-of");
	kb_argv_add(&w, stem);
	kb_argv_add(&w, "-f");
	kb_argv_add(&w, wav);
	kb_argv_end(&w);
	if (kb_run(&w) != 0) {
		snprintf(why, n, "%s: whisper-cli failed", kb_basename(file));
		goto done;
	}
	out = kb_calloc(strlen(stem) + 5, 1);
	sprintf(out, "%s.txt", stem);
done:
	unlink(wav);
	free(stem);
	return out;
}

static int cmd_transcribe(int argc, char **argv)
{
	int notify = 0, first = 0, failed = 0, done = 0;
	char *model;
	KbBuf names = { 0 }, errs = { 0 };

	for (; first < argc; first++) {
		if (!strcmp(argv[first], "--notify"))
			notify = 1;
		else if (!strcmp(argv[first], "--")) {
			first++;
			break;
		} else if (argv[first][0] == '-')
			return speech_usage(2);
		else
			break;
	}
	if (first >= argc)
		return speech_usage(2);
	if (!(model = find_whisper())) {
		fprintf(stderr, "kdos speech: no transcription model — "
				"kdos speech get %s\n", DEFAULT_MODEL);
		if (notify)
			kb_notify("kdos", "Transcription failed",
				  "no transcription model is installed");
		return 1;
	}
	for (int i = first; i < argc; i++) {
		char why[512] = "", *txt = transcribe_one(model, argv[i], why,
							  sizeof(why));

		if (txt) {
			printf("%s\n", txt);
			kb_buf_printf(&names, "%s%s", done ? ", " : "",
				      kb_basename(txt));
			done++;
			free(txt);
		} else {
			fprintf(stderr, "kdos speech: %s\n", why);
			kb_buf_printf(&errs, "%s%s", failed ? "; " : "", why);
			failed++;
		}
	}
	if (notify) {
		if (failed)
			kb_notify("kdos", "Transcription failed", errs.p);
		else
			kb_notify("kdos", "Transcription finished", names.p);
	}
	kb_buf_free(&names);
	kb_buf_free(&errs);
	free(model);
	return failed ? 1 : 0;
}

/* ── say ───────────────────────────────────────────────────────────────── */

#define SYSTEM_VOICE_DIR "/usr/share/piper/voices"

/* A voice is <name>.onnx with its <name>.onnx.json beside it; the person's
 * own directory first, then the image's. */
static char *voice_dir(void)
{
	return kdt_data_home("piper/voices");
}

static char *find_voice(const char *want)
{
	char *dirs[2] = { voice_dir(), kb_strdup(SYSTEM_VOICE_DIR) };
	char *hit = NULL;

	for (int d = 0; d < 2 && !hit; d++) {
		int n = 0;
		char **v = kb_listdir(dirs[d], &n);

		for (int i = 0; i < n && !hit; i++) {
			size_t l = strlen(v[i]);
			char *p, *js;

			if (l < 6 || strcmp(v[i] + l - 5, ".onnx"))
				continue;
			if (want && (strncmp(v[i], want, l - 5) ||
				     strlen(want) != l - 5))
				continue;
			p = kb_path_join(dirs[d], v[i]);
			js = kb_calloc(strlen(p) + 6, 1);
			sprintf(js, "%s.json", p);
			if (kb_path_exists(js))
				hit = p;
			else
				free(p);
			free(js);
		}
		kb_strv_free(v);
	}
	free(dirs[0]);
	free(dirs[1]);
	return hit;
}

static int cmd_say(int argc, char **argv)
{
	const char *want = NULL;
	char *voice, wav[512];
	KbBuf text = { 0 };
	KbArgv p = { 0 }, w = { 0 };
	int i = 0, fd, rc = 1;

	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--voice") && i + 1 < argc)
			want = argv[++i];
		else if (!strcmp(argv[i], "--")) {
			i++;
			break;
		} else if (argv[i][0] == '-')
			return speech_usage(2);
		else
			break;
	}
	if (want && (!*want || strchr(want, '/')))
		return speech_usage(2);
	if (!(voice = find_voice(want))) {
		fprintf(stderr, "kdos speech: %s — a voice is <name>.onnx with "
				"its .onnx.json, in %s or %s\n",
			want ? "no such voice" : "no voice is installed",
			"~/.local/share/piper/voices", SYSTEM_VOICE_DIR);
		return 1;
	}
	for (int k = i; k < argc; k++)
		kb_buf_printf(&text, "%s%s", k > i ? " " : "", argv[k]);
	if (i >= argc) {
		char buf[4096];
		size_t got;

		while ((got = fread(buf, 1, sizeof(buf), stdin)) > 0)
			kb_buf_add(&text, buf, got);
	}
	if (!text.n) {
		free(voice);
		kb_buf_free(&text);
		return 0;
	}
	snprintf(wav, sizeof(wav), "%s/kdos-say.XXXXXX.wav", kb_runtime_dir());
	if ((fd = mkstemps(wav, 4)) < 0) {
		fprintf(stderr, "kdos speech: %s: %s\n", wav, strerror(errno));
		goto out;
	}
	close(fd);
	/* The text goes in on stdin, never in an argument: a clipboard can
	 * hold a password. */
	kb_argv_add(&p, "piper");
	kb_argv_add(&p, "-m");
	kb_argv_add(&p, voice);
	kb_argv_add(&p, "-f");
	kb_argv_add(&p, wav);
	kb_argv_end(&p);
	if (kb_run_feed(&p, text.p, text.n) != 0) {
		fprintf(stderr, "kdos speech: piper could not speak with %s\n",
			kb_basename(voice));
		goto out;
	}
	kb_argv_add(&w, "pw-play");
	kb_argv_add(&w, wav);
	kb_argv_end(&w);
	rc = kb_run(&w) == 0 ? 0 : 1;
	if (rc)
		fprintf(stderr, "kdos speech: pw-play could not play it\n");
out:
	unlink(wav);
	free(voice);
	kb_buf_free(&text);
	return rc;
}

/* ── sync: Klipper's Read Aloud action ─────────────────────────────────── */

/*
 * KLIPPER HAS NO PER-ACTION CONDITION, so the action exists only while a
 * voice does: added when one is installed, removed — exactly that action,
 * found by its description and command — when the last one goes. Every
 * other action is the person's and is not touched; one that sat after ours
 * moves down a place, which is how Klipper itself numbers them.
 *
 * Written through kwriteconfig6, because klipperrc is a file KConfig writes
 * whole; Klipper reads it when the clipboard applet next starts.
 */
#define RA_DESC "Read Aloud"
#define RA_CMD "kdos speech say -- %s"

typedef struct {
	char *group, *key, *val;
} KEnt;

/* Every key of the person's klipperrc, raw, in file order. */
static KEnt *klipper_read(int *n)
{
	char *p = kdt_cfg_home("klipperrc"), *data = kb_read_all(p, NULL);
	char group[256] = "";
	KEnt *e = NULL;
	int cap = 0;

	free(p);
	*n = 0;
	for (char *l = data, *next; l && *l; l = next) {
		char *nl = strchr(l, '\n'), *eq;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		if (l[0] == '[') {
			char *end = strrchr(l, ']');

			if (end) {
				*end = '\0';
				snprintf(group, sizeof(group), "%s", l + 1);
			}
			continue;
		}
		if (!*group || l[0] == '#' || !(eq = strchr(l, '=')))
			continue;
		*eq = '\0';
		if (*n == cap) {
			cap = cap ? cap * 2 : 32;
			e = kb_realloc(e, (size_t)cap * sizeof(*e));
		}
		e[*n].group = kb_strdup(group);
		e[*n].key = kb_strdup(l);
		e[*n].val = kb_strdup(eq + 1);
		(*n)++;
	}
	free(data);
	return e;
}

static const char *kent(KEnt *e, int n, const char *group, const char *key)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(e[i].group, group) && !strcmp(e[i].key, key))
			return e[i].val;
	return NULL;
}

/* A key spelled with a flag (`Commandline[$e]`) is written back as the key
 * KConfig reads it as. */
static void bare_key(const char *k, char *out, size_t n)
{
	snprintf(out, n, "%s", k);
	out[strcspn(out, "[")] = '\0';
}

/* Is Action_<i> ours? */
static int ours(KEnt *e, int n, int i)
{
	char g[64];
	const char *d, *c;

	snprintf(g, sizeof(g), "Action_%d", i);
	d = kent(e, n, g, "Description");
	snprintf(g, sizeof(g), "Action_%d/Command_0", i);
	c = kent(e, n, g, "Commandline[$e]");
	if (!c)
		c = kent(e, n, g, "Commandline");
	return d && c && !strcmp(d, RA_DESC) && !strcmp(c, RA_CMD);
}

static int set(const char *g, const char *k, const char *v)
{
	return kdt_kcfg_set("klipperrc", g, k, v, 0);
}

/* Every key of `from` (and its commands) written into `to`, raw values
 * unescaped for kwriteconfig6 to escape again. */
static int move_action(KEnt *e, int n, int from, int to)
{
	char fg[64], tg[96], key[128];
	int rc = 0;

	snprintf(fg, sizeof(fg), "Action_%d", from);
	for (int i = 0; i < n; i++) {
		const char *g = e[i].group;
		size_t fl = strlen(fg);
		char *v;

		if (strncmp(g, fg, fl) || (g[fl] && g[fl] != '/'))
			continue;
		snprintf(tg, sizeof(tg), "Action_%d%s", to, g + fl);
		bare_key(e[i].key, key, sizeof(key));
		v = kdt_kcfg_unescape(e[i].val);
		rc |= set(tg, key, v);
		free(v);
	}
	return rc;
}

static int drop_action(KEnt *e, int n, int idx)
{
	char g[64], key[128];
	int rc = 0;

	snprintf(g, sizeof(g), "Action_%d", idx);
	for (int i = 0; i < n; i++) {
		size_t gl = strlen(g);

		if (strncmp(e[i].group, g, gl) ||
		    (e[i].group[gl] && e[i].group[gl] != '/'))
			continue;
		bare_key(e[i].key, key, sizeof(key));
		rc |= set(e[i].group, key, NULL);
	}
	return rc;
}

static int cmd_sync(void)
{
	char *voice = find_voice(NULL);
	int n = 0, count, at = -1, rc = 0;
	KEnt *e = klipper_read(&n);
	const char *cs = kent(e, n, "General", "Number of Actions");
	char num[16], g[64];

	count = cs ? atoi(cs) : 0;
	for (int i = 0; i < count && at < 0; i++)
		if (ours(e, n, i))
			at = i;

	if (voice && at < 0) {
		snprintf(g, sizeof(g), "Action_%d", count);
		rc |= set(g, "Automatic", "false");
		rc |= set(g, "Description", RA_DESC);
		rc |= set(g, "Number of commands", "1");
		rc |= set(g, "Regexp", "(?s).+");
		snprintf(g, sizeof(g), "Action_%d/Command_0", count);
		rc |= set(g, "Commandline", RA_CMD);
		rc |= set(g, "Description", RA_DESC);
		rc |= set(g, "Enabled", "true");
		rc |= set(g, "Icon", "audio-speakers-symbolic");
		rc |= set(g, "Output", "0");
		snprintf(num, sizeof(num), "%d", count + 1);
		rc |= set("General", "Number of Actions", num);
		if (!rc)
			printf("Klipper offers Read Aloud\n");
	} else if (!voice && at >= 0) {
		for (int i = at + 1; i < count; i++) {
			rc |= drop_action(e, n, i - 1);
			rc |= move_action(e, n, i, i - 1);
		}
		rc |= drop_action(e, n, count - 1);
		snprintf(num, sizeof(num), "%d", count - 1);
		rc |= set("General", "Number of Actions", num);
		if (!rc)
			printf("Klipper no longer offers Read Aloud: no voice "
			       "is installed\n");
	}
	for (int i = 0; i < n; i++) {
		free(e[i].group);
		free(e[i].key);
		free(e[i].val);
	}
	free(e);
	free(voice);
	if (rc) {
		fprintf(stderr, "kdos speech: kwriteconfig6 could not write "
				"klipperrc\n");
		return 1;
	}
	return 0;
}

/* ── the verb ──────────────────────────────────────────────────────────── */

static int speech_usage(int rc)
{
	FILE *o = rc ? stderr : stdout;

	fprintf(o,
	       "usage: kdos speech <command>\n"
	       "\n"
	       "  transcribe [--notify] [--] FILE...   FILE.txt beside each\n"
	       "  say [--voice NAME] [--] [TEXT...]    read aloud (stdin with"
	       " no TEXT)\n"
	       "  sync             Klipper's Read Aloud action, while a voice"
	       " exists\n"
	       "  list             every transcription model, and which are"
	       " here\n"
	       "  get NAME         download one into your own data directory\n"
	       "  where            the directories searched, and what is in each\n"
	       "  remove NAME      delete one\n"
	       "\n"
	       "KDOS ships %s in " SYSTEM_MODEL_DIR ", which\n"
	       "transcribe uses when none of yours is here.\n", DEFAULT_MODEL);
	return rc;
}

int kdt_speech(int argc, char **argv)
{
	const char *cmd = argc > 0 ? argv[0] : "list";

	if (!strcmp(cmd, "transcribe"))
		return cmd_transcribe(argc - 1, argv + 1);
	if (!strcmp(cmd, "say"))
		return cmd_say(argc - 1, argv + 1);
	if (!strcmp(cmd, "sync") && argc == 1)
		return cmd_sync();
	if (!strcmp(cmd, "list"))
		return cmd_list();
	if (!strcmp(cmd, "where"))
		return cmd_where();
	if (!strcmp(cmd, "get"))
		return cmd_get(argc > 1 ? argv[1] : DEFAULT_MODEL);
	if (!strcmp(cmd, "remove") && argc > 1)
		return cmd_remove(argv[1]);
	if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return speech_usage(0);
	fprintf(stderr, "kdos speech: unknown command '%s'\n", cmd);
	return speech_usage(2);
}
