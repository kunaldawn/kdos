/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   `kdos speech` — the transcription model, which the image does not carry
 *
 *   kdos speech list            what there is, and which one is here
 *   kdos speech get base.en     fetch one into the user's own data directory
 *   kdos speech where           the directory, and the model that would be used
 *   kdos speech remove NAME     delete one
 *
 * NO MODEL SHIPS. The smallest useful one is 32 MB and the one most people
 * want is 148 MB, against an image measured in hundreds; and a speech model
 * is the single most personal choice in the catalogue — language, size and
 * the trade between the two. So the image carries `whisper-cli` and this
 * carries the way to get a model for it.
 *
 * IT WRITES TO THE USER'S DATA DIRECTORY AND NEVER TO /usr. `kdos-rec`
 * searches $KDOS_WHISPER_MODEL, then $XDG_DATA_HOME/whisper.cpp/models, then
 * /usr/share/whisper.cpp/models; this writes the second, which needs no
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
 * argument reaches this from a command line and, through kdos-rec, from a
 * button; a name pasted into a URL is a name that can name another host.
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

/* A ggml file starts `lmgg`. The same gate kdos-rec applies, applied here as
 * well: a checksum says the bytes arrived and the magic says they are a
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
 * $XDG_DATA_HOME/whisper.cpp/models, or $HOME/.local/share/... — kdos-rec's
 * second search path, spelled the same way it spells it. NOT the third:
 * /usr/share is root's and a model is a per-user choice.
 */
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

static char *model_path(const char *name)
{
	char *dir = model_dir();
	char leaf[128];

	snprintf(leaf, sizeof(leaf), "ggml-%s.bin", name);
	return kb_path_join(dir, leaf);
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
		char sz[32];
		int here = is_ggml(p);

		human(MODELS[i].bytes, sz, sizeof(sz));
		printf("  %-16s %9s  %s\n", MODELS[i].name, sz,
		       here ? "installed" : "");
		free(p);
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
	printf("%s\n", dir);
	for (int i = 0; i < NMODELS; i++) {
		char *p = model_path(MODELS[i].name);

		if (is_ggml(p))
			printf("  %s\n", p);
		free(p);
	}
	free(dir);
	return 0;
}

/*
 * THE FETCH. curl writes to a `.part` beside the destination and the rename
 * happens only after both checks pass, so an interrupted download is never a
 * file kdos-rec's gate has to reject — and a model directory never holds a
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
	printf("    Transcribe in kdos-rec is live the next time it opens.\n");
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

static int usage(void)
{
	printf("usage: kdos speech <command>\n"
	       "\n"
	       "  list             every model this knows, and which are here\n"
	       "  get NAME         download one into your own data directory\n"
	       "  where            the directory searched, and what is in it\n"
	       "  remove NAME      delete one\n"
	       "\n"
	       "No speech model ships with KDOS. `whisper-cli` does, and\n"
	       "kdos-rec's Transcribe is greyed until one of these is here.\n"
	       "\n"
	       "  kdos speech get %s\n", DEFAULT_MODEL);
	return 0;
}

int kdt_speech(int argc, char **argv)
{
	const char *cmd = argc > 0 ? argv[0] : "list";

	if (!strcmp(cmd, "list"))
		return cmd_list();
	if (!strcmp(cmd, "where"))
		return cmd_where();
	if (!strcmp(cmd, "get"))
		return cmd_get(argc > 1 ? argv[1] : DEFAULT_MODEL);
	if (!strcmp(cmd, "remove") && argc > 1)
		return cmd_remove(argv[1]);
	if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help"))
		return usage();
	fprintf(stderr, "kdos speech: unknown command '%s'\n", cmd);
	usage();
	return 1;
}
