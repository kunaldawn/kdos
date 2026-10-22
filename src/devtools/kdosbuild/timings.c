/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdosbuild — the per-step timings file
 *
 * build/snapshots/timings.json holds the seconds each step last took when it
 * did its work, keyed `<phase directory>/<step title>`: a script step by its
 * title (`00_cross/binutils`, `10_bootstrap/linux headers`), a port by its
 * name (`41_system/zlib`). A phase's wall clock is under "phases", by its
 * directory name. docs/kfs reads "steps" for its SBU column, and one SBU is
 * `00_cross/binutils`.
 *
 * EVERY RECORD IS A MERGE. The file is read, the one key is set, and the whole
 * is written back atomically, so a --continue-from, --phases or --steps run
 * changes the keys it measured and keeps every other. Which steps are
 * measured is the manager's rule (manager.c, `measured`); this file only
 * stores what it is handed.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdosbuild.h"

/* The two sections, in the order they are written. */
static const char *const tm_section[] = { "steps", "phases" };
#define TM_NSECTION 2

typedef struct {
	char *key;
	double secs;
} TmRec;

typedef struct {
	TmRec *v;
	int n, cap;
} TmList;

static TmRec *tm_find(TmList *l, const char *key)
{
	for (int i = 0; i < l->n; i++)
		if (!strcmp(l->v[i].key, key))
			return &l->v[i];
	return NULL;
}

/* Sets `key`, appending it when new: the file keeps the order the keys were
 * first measured in, which is the build's order. */
static void tm_set(TmList *l, const char *key, double secs)
{
	TmRec *at = tm_find(l, key);
	if (!at) {
		if (l->n == l->cap) {
			l->cap = l->cap ? l->cap * 2 : 256;
			l->v = kb_realloc(l->v, (size_t)l->cap * sizeof(*l->v));
		}
		at = &l->v[l->n++];
		at->key = kb_strdup(key);
	}
	at->secs = secs;
}

static void tm_free(TmList *l)
{
	for (int i = 0; i < l->n; i++)
		free(l->v[i].key);
	free(l->v);
	memset(l, 0, sizeof(*l));
}

/* A file that is absent or does not parse whole reads as empty: the figures
 * are a record of past builds, not state a build depends on, and refusing to
 * build over them would cost more than losing them. */
static void tm_load(const char *path, TmList out[TM_NSECTION])
{
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	if (!text)
		return;
	KjNode *root = kj_parse(text);
	free(text);
	if (!root)
		return;
	for (int s = 0; s < TM_NSECTION; s++) {
		const KjNode *sec = kj_get(root, tm_section[s]);
		if (!sec || sec->type != KJ_OBJ)
			continue;
		for (const KjNode *c = sec->child; c; c = c->next)
			if (c->key && c->type == KJ_NUM)
				tm_set(&out[s], c->key, c->num);
	}
	kj_free(root);
}

static int tm_save(const char *path, TmList in[TM_NSECTION])
{
	char dir[1024];
	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(dir);
	}

	KbBuf b = {0};
	kb_buf_str(&b, "{\n");
	for (int s = 0; s < TM_NSECTION; s++) {
		kb_buf_printf(&b, " \"%s\": {", tm_section[s]);
		for (int i = 0; i < in[s].n; i++) {
			kb_buf_str(&b, i ? ",\n  " : "\n  ");
			kb_json_str(&b, in[s].v[i].key);
			kb_buf_printf(&b, ": %.2f", in[s].v[i].secs);
		}
		kb_buf_printf(&b, "%s}%s\n", in[s].n ? "\n " : "",
			      s + 1 < TM_NSECTION ? "," : "");
	}
	kb_buf_str(&b, "}\n");
	int rc = kb_write_file_atomic(path, b.p);
	kb_buf_free(&b);
	return rc;
}

int tm_record(const char *path, const char *section, const char *key,
	      double secs)
{
	int s = 0;
	while (s < TM_NSECTION && strcmp(tm_section[s], section))
		s++;
	if (s == TM_NSECTION || !key || !*key || secs < 0)
		return -1;

	TmList l[TM_NSECTION];
	memset(l, 0, sizeof(l));
	tm_load(path, l);
	/* Rounded as it is written, so the value in memory is the one a
	 * reader gets back. */
	tm_set(&l[s], key, (double)(long long)(secs * 100 + 0.5) / 100.0);
	int rc = tm_save(path, l);
	for (int i = 0; i < TM_NSECTION; i++)
		tm_free(&l[i]);
	return rc;
}

double tm_lookup(const char *path, const char *section, const char *key)
{
	TmList l[TM_NSECTION];
	memset(l, 0, sizeof(l));
	tm_load(path, l);
	double v = -1;
	for (int s = 0; s < TM_NSECTION; s++) {
		if (strcmp(tm_section[s], section))
			continue;
		TmRec *at = tm_find(&l[s], key);
		if (at)
			v = at->secs;
	}
	for (int i = 0; i < TM_NSECTION; i++)
		tm_free(&l[i]);
	return v;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* --selftest: the merge, over fixture files in a temporary directory       */

static int tfail;

static void tok(int cond, const char *what)
{
	if (!cond) {
		tfail++;
		printf("  FAIL  %s\n", what);
	}
}

static int near(double a, double b)
{
	return a > b - 0.001 && a < b + 0.001;
}

int tm_selftest(void)
{
	tfail = 0;
	const char *tmp = getenv("TMPDIR");
	char dir[512];
	snprintf(dir, sizeof(dir), "%s/kdosbuild-timings.XXXXXX",
		 tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(dir)) {
		printf("  FAIL  no temporary directory for the timings test\n");
		return 1;
	}
	char path[640];
	snprintf(path, sizeof(path), "%s/snapshots/timings.json", dir);

	/* No file and no directory: the first record makes both. */
	tok(tm_record(path, "steps", "00_cross/binutils", 43.414) == 0,
	    "the first record writes the file");
	tok(near(tm_lookup(path, "steps", "00_cross/binutils"), 43.41),
	    "a figure is stored to two decimals");

	/* A file an earlier run wrote: a record changes one key and keeps the
	 * rest, in both sections, in their order. */
	const char *fixture =
		"{\n \"steps\": {\n  \"00_cross/binutils\": 43.41,\n"
		"  \"10_bootstrap/linux headers\": 7.48,\n"
		"  \"20_selfhost/gcc\": 629.82\n },\n"
		" \"phases\": {\n  \"00_cross\": 381.09\n }\n}";
	kb_write_all(path, fixture, strlen(fixture));
	tok(tm_record(path, "steps", "41_system/zlib", 1.5) == 0,
	    "a record over an existing file succeeds");
	tok(near(tm_lookup(path, "steps", "00_cross/binutils"), 43.41) &&
	    near(tm_lookup(path, "steps", "10_bootstrap/linux headers"), 7.48) &&
	    near(tm_lookup(path, "steps", "20_selfhost/gcc"), 629.82),
	    "a record keeps every figure it did not measure");
	tok(near(tm_lookup(path, "phases", "00_cross"), 381.09),
	    "a step record keeps the phase figures");
	tok(near(tm_lookup(path, "steps", "41_system/zlib"), 1.5),
	    "a new key is added");
	tm_record(path, "steps", "20_selfhost/gcc", 600);
	tok(near(tm_lookup(path, "steps", "20_selfhost/gcc"), 600),
	    "a measured key is replaced, not averaged");
	tm_record(path, "phases", "41_system", 3600);
	tok(near(tm_lookup(path, "phases", "41_system"), 3600) &&
	    near(tm_lookup(path, "phases", "00_cross"), 381.09),
	    "a phase record merges too");

	size_t len = 0;
	char *text = kb_read_all(path, &len);
	const char *a = text ? strstr(text, "\"00_cross/binutils\"") : NULL;
	const char *z = text ? strstr(text, "\"41_system/zlib\"") : NULL;
	const char *g = text ? strstr(text, "\"20_selfhost/gcc\"") : NULL;
	tok(a && g && z && a < g && g < z,
	    "keys keep the order they were first measured in");
	KjNode *root = text ? kj_parse(text) : NULL;
	tok(root != NULL, "the file written parses");
	kj_free(root);
	free(text);

	/* A key a title can carry, quoted and back. */
	tm_record(path, "steps", "70_image/a \"quoted\" title", 2);
	tok(near(tm_lookup(path, "steps", "70_image/a \"quoted\" title"), 2),
	    "a key with a quote in it survives the round trip");

	tok(tm_record(path, "elsewhere", "x", 1) < 0 &&
	    tm_record(path, "steps", "", 1) < 0 &&
	    tm_record(path, "steps", "x", -1) < 0,
	    "an unknown section, an empty key and a negative time are refused");

	/* A file that does not parse is started over, not refused. */
	kb_write_all(path, "{\"steps\": {", 11);
	tok(tm_record(path, "steps", "60_kernel/linux", 900) == 0 &&
	    near(tm_lookup(path, "steps", "60_kernel/linux"), 900) &&
	    tm_lookup(path, "steps", "00_cross/binutils") < 0,
	    "a corrupt file is replaced by what this run measured");

	kb_rmtree(dir);
	printf(tfail ? "timings: %d failed\n" : "timings: ok\n", tfail);
	return tfail ? 1 : 0;
}
