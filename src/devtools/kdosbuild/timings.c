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
 * EVERY FIGURE NAMES THE MACHINE THAT MEASURED IT. "machine" maps each key of
 * "steps" and "phases" (a step key has a '/', a phase key none) to an id of
 * "machines", which holds what a run was given to build with: the processor's
 * model name, the CPUs this process may run on, MemTotal in bytes and
 * KDOS_JOBS as the build was given it (0 when unset, and every step then
 * derives it from the CPUs and memory, script/env/common.env). An SBU is a
 * ratio of two figures and holds only between figures of one machine, so a
 * reader must be able to tell which figures share one. A record made with no
 * machine set (tm_use_machine) drops the key's entry rather than leaving an
 * older run's machine on a newer figure, and a machine no key names is not
 * written back.
 *
 * EVERY RECORD IS A MERGE. The file is read, the one key is set, and the whole
 * is written back atomically, so a --continue-from, --phases or --steps run
 * changes the keys it measured and keeps every other. Which steps are
 * measured is the manager's rule (manager.c, `measured`); this file only
 * stores what it is handed.
 * ---------------------------------
 */

#include <ctype.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdosbuild.h"

/* The numeric sections, in the order they are written; "machines" follows
 * them. */
static const char *const tm_section[] = { "steps", "phases", "machine" };
#define TM_NSECTION 3
#define TM_MACHINE 2

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

static void tm_del(TmList *l, const char *key)
{
	TmRec *at = tm_find(l, key);
	if (!at)
		return;
	free(at->key);
	memmove(at, at + 1, (size_t)(l->v + l->n - (at + 1)) * sizeof(*at));
	l->n--;
}

/* ── The machines ── */

typedef struct {
	int id;
	TmMachine m;
} TmMach;

typedef struct {
	TmMach *v;
	int n, cap;
} TmMachList;

static TmMachine tm_cur;
static int tm_have_cur;

void tm_use_machine(const TmMachine *m)
{
	tm_have_cur = m != NULL;
	if (m)
		tm_cur = *m;
}

static int tm_same(const TmMachine *a, const TmMachine *b)
{
	return !strcmp(a->cpu, b->cpu) && a->cpus == b->cpus &&
	       a->memory == b->memory && a->jobs == b->jobs;
}

static void tm_mach_add(TmMachList *l, int id, const TmMachine *m)
{
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->v = kb_realloc(l->v, (size_t)l->cap * sizeof(*l->v));
	}
	l->v[l->n].id = id;
	l->v[l->n].m = *m;
	l->n++;
}

/* The id `m` has in the file, given a new one past the highest when it is
 * not there yet. */
static int tm_mach_id(TmMachList *l, const TmMachine *m)
{
	int top = 0;
	for (int i = 0; i < l->n; i++) {
		if (tm_same(&l->v[i].m, m))
			return l->v[i].id;
		if (l->v[i].id > top)
			top = l->v[i].id;
	}
	tm_mach_add(l, top + 1, m);
	return top + 1;
}

/* What this process is building with. The model name is the first
 * `model name` of /proc/cpuinfo, whitespace collapsed, "unknown" on a
 * processor that has none; a field that cannot be read is 0. */
void tm_machine_probe(TmMachine *out)
{
	memset(out, 0, sizeof(*out));
	kb_strlcpy(out->cpu, "unknown", sizeof(out->cpu));
	FILE *f = fopen("/proc/cpuinfo", "re");
	char line[512];
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, "model name", 10))
				continue;
			const char *v = strchr(line, ':');
			if (!v)
				continue;
			size_t n = 0;
			for (v++; *v && n + 1 < sizeof(out->cpu); v++) {
				if (isspace((unsigned char)*v)) {
					if (n && out->cpu[n - 1] != ' ')
						out->cpu[n++] = ' ';
				} else {
					out->cpu[n++] = *v;
				}
			}
			while (n && out->cpu[n - 1] == ' ')
				n--;
			out->cpu[n] = 0;
			if (!n)
				kb_strlcpy(out->cpu, "unknown", sizeof(out->cpu));
			break;
		}
		fclose(f);
	}
	cpu_set_t set;
	CPU_ZERO(&set);
	if (sched_getaffinity(0, sizeof(set), &set) == 0)
		out->cpus = CPU_COUNT(&set);
	f = fopen("/proc/meminfo", "re");
	if (f) {
		long long kb;
		while (fgets(line, sizeof(line), f))
			if (sscanf(line, "MemTotal: %lld kB", &kb) == 1) {
				out->memory = kb * 1024;
				break;
			}
		fclose(f);
	}
	const char *jv = getenv("KDOS_JOBS");
	out->jobs = jv && *jv ? atoi(jv) : 0;
	if (out->jobs < 0)
		out->jobs = 0;
}

/* A file that is absent or does not parse whole reads as empty: the figures
 * are a record of past builds, not state a build depends on, and refusing to
 * build over them would cost more than losing them. */
static void tm_load(const char *path, TmList out[TM_NSECTION], TmMachList *mach)
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
	const KjNode *ms = kj_get(root, "machines");
	if (ms && ms->type == KJ_OBJ)
		for (const KjNode *c = ms->child; c; c = c->next) {
			int id = c->key ? atoi(c->key) : 0;
			if (id < 1 || c->type != KJ_OBJ)
				continue;
			TmMachine m;
			memset(&m, 0, sizeof(m));
			kb_strlcpy(m.cpu, kj_str(c, "cpu", "unknown"), sizeof(m.cpu));
			m.cpus = (int)kj_num(c, "cpus", 0);
			m.memory = (long long)kj_num(c, "memory", 0);
			m.jobs = (int)kj_num(c, "jobs", 0);
			tm_mach_add(mach, id, &m);
		}
	kj_free(root);
}

static int tm_save(const char *path, TmList in[TM_NSECTION],
		   const TmMachList *mach)
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
			if (s == TM_MACHINE)
				kb_buf_printf(&b, ": %d", (int)in[s].v[i].secs);
			else
				kb_buf_printf(&b, ": %.2f", in[s].v[i].secs);
		}
		kb_buf_printf(&b, "%s},\n", in[s].n ? "\n " : "");
	}
	kb_buf_str(&b, " \"machines\": {");
	int any = 0;
	for (int i = 0; i < mach->n; i++) {
		const TmMach *e = &mach->v[i];
		int used = 0;
		for (int k = 0; k < in[TM_MACHINE].n && !used; k++)
			used = (int)in[TM_MACHINE].v[k].secs == e->id;
		if (!used)
			continue;
		kb_buf_printf(&b, "%s\"%d\": {\"cpu\": ", any ? ",\n  " : "\n  ",
			      e->id);
		kb_json_str(&b, e->m.cpu);
		kb_buf_printf(&b, ", \"cpus\": %d, \"memory\": %lld, \"jobs\": %d}",
			      e->m.cpus, e->m.memory, e->m.jobs);
		any = 1;
	}
	kb_buf_printf(&b, "%s}\n}\n", any ? "\n " : "");
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
	if (s == TM_NSECTION || s == TM_MACHINE || !key || !*key || secs < 0)
		return -1;

	TmList l[TM_NSECTION];
	TmMachList mach = {0};
	memset(l, 0, sizeof(l));
	tm_load(path, l, &mach);
	/* Rounded as it is written, so the value in memory is the one a
	 * reader gets back. */
	tm_set(&l[s], key, (double)(long long)(secs * 100 + 0.5) / 100.0);
	if (tm_have_cur)
		tm_set(&l[TM_MACHINE], key, tm_mach_id(&mach, &tm_cur));
	else
		tm_del(&l[TM_MACHINE], key);
	int rc = tm_save(path, l, &mach);
	for (int i = 0; i < TM_NSECTION; i++)
		tm_free(&l[i]);
	free(mach.v);
	return rc;
}

double tm_lookup(const char *path, const char *section, const char *key)
{
	TmList l[TM_NSECTION];
	TmMachList mach = {0};
	memset(l, 0, sizeof(l));
	tm_load(path, l, &mach);
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
	free(mach.v);
	return v;
}

int tm_lookup_machine(const char *path, const char *key, TmMachine *out)
{
	TmList l[TM_NSECTION];
	TmMachList mach = {0};
	memset(l, 0, sizeof(l));
	tm_load(path, l, &mach);
	int id = 0;
	TmRec *at = tm_find(&l[TM_MACHINE], key);
	if (at)
		for (int i = 0; i < mach.n; i++)
			if (mach.v[i].id == (int)at->secs) {
				id = mach.v[i].id;
				if (out)
					*out = mach.v[i].m;
			}
	for (int i = 0; i < TM_NSECTION; i++)
		tm_free(&l[i]);
	free(mach.v);
	return id;
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
	/* The fixtures name no machine until the machine test sets one; a
	 * Manager made earlier in the run set this host's. */
	TmMachine saved = tm_cur;
	int saved_have = tm_have_cur;
	tm_use_machine(NULL);
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

	/* The machine: every figure is filed under the one in use, a second
	 * machine gets its own id, and a figure re-measured with none set
	 * names none. The fixture above names no machine, as a file of a
	 * build that recorded none does. */
	TmMachine a1, a2, got;
	memset(&a1, 0, sizeof(a1));
	kb_strlcpy(a1.cpu, "Fixture CPU \"A\"", sizeof(a1.cpu));
	a1.cpus = 16;
	a1.memory = 34359738368LL;
	a2 = a1;
	a2.jobs = 6;
	tok(tm_lookup_machine(path, "00_cross/binutils", NULL) == 0,
	    "a figure from a file with no machines names none");
	tm_use_machine(&a1);
	tm_record(path, "steps", "00_cross/binutils", 44);
	tm_record(path, "phases", "00_cross", 380);
	tok(tm_lookup_machine(path, "00_cross/binutils", &got) == 1 &&
	    tm_same(&got, &a1),
	    "a figure names the machine in use, its record read back whole");
	tok(tm_lookup_machine(path, "00_cross", NULL) == 1,
	    "a phase figure names its machine too");
	tok(tm_lookup_machine(path, "20_selfhost/gcc", NULL) == 0 &&
	    near(tm_lookup(path, "steps", "20_selfhost/gcc"), 600),
	    "a figure not re-measured keeps naming no machine");
	tm_use_machine(&a2);
	tm_record(path, "steps", "41_system/zlib", 1.6);
	tok(tm_lookup_machine(path, "41_system/zlib", &got) == 2 &&
	    got.jobs == 6 &&
	    tm_lookup_machine(path, "00_cross/binutils", NULL) == 1,
	    "a second machine is a second id, and the first keeps its own");
	const char *rec1 = "\"1\": {\"cpu\": \"Fixture CPU \\\"A\\\"\", "
			   "\"cpus\": 16, \"memory\": 34359738368, \"jobs\": 0}";
	text = kb_read_all(path, &len);
	tok(text && strstr(text, rec1), "a machine some figure names is written");
	free(text);
	tm_record(path, "steps", "00_cross/binutils", 45);
	tm_record(path, "phases", "00_cross", 390);
	text = kb_read_all(path, &len);
	tok(text && !strstr(text, rec1) && strstr(text, "\"jobs\": 6"),
	    "a machine no figure names any more is not written back");
	free(text);
	tm_use_machine(NULL);
	tm_record(path, "steps", "41_system/zlib", 1.7);
	tok(tm_lookup_machine(path, "41_system/zlib", NULL) == 0 &&
	    tm_lookup_machine(path, "00_cross/binutils", NULL) == 2,
	    "a figure recorded with no machine names none");
	tok(tm_record(path, "machine", "41_system/zlib", 1) < 0,
	    "the machine section is not a figure");
	TmMachine probe;
	tm_machine_probe(&probe);
	tok(probe.cpu[0] && probe.cpus >= 1,
	    "the probe names a processor and at least one CPU");

	/* A file that does not parse is started over, not refused. */
	kb_write_all(path, "{\"steps\": {", 11);
	tok(tm_record(path, "steps", "60_kernel/linux", 900) == 0 &&
	    near(tm_lookup(path, "steps", "60_kernel/linux"), 900) &&
	    tm_lookup(path, "steps", "00_cross/binutils") < 0,
	    "a corrupt file is replaced by what this run measured");

	kb_rmtree(dir);
	tm_cur = saved;
	tm_have_cur = saved_have;
	printf(tfail ? "timings: %d failed\n" : "timings: ok\n", tfail);
	return tfail ? 1 : 0;
}
