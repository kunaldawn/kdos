/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos march — per-package -march, kept only where it was measured to win
 *
 *   kdos march probe            what this CPU can actually run
 *   kdos march run <port>...    build both ways, benchmark, decide
 *   kdos march report           the ledger: kept, reverted, unmeasurable
 *
 * Gentoo optimizes blind. CachyOS optimizes by tier for a population rather than
 * for a machine. Clear Linux, which did runtime dispatch properly, was shut down
 * in July 2025 — and musl closes that route anyway: no glibc-hwcaps, and IFUNC
 * is contested upstream. That leaves rebuild-per-machine as the only path, which
 * makes the interesting question not "which flags" but **"did they help HERE"**.
 *
 * The published numbers are why this is a measurement loop and not a flag list.
 * On x86-64-v3, measured elsewhere: flac +20%, vorbis +21%, zstd decompress
 * +16% — and bzip2 **-7%**, python **-3%**, lz4 **-2.9% with over 10% more power
 * drawn**. A distro that shipped v3 everywhere would ship those regressions and
 * never know.
 *
 * So: build the port twice on THIS machine, run its own benchmark against both,
 * and keep the flags only where the win is larger than the noise. Everything
 * else is reverted and SAID so — a report that only lists winners is a report
 * that has learned nothing.
 *
 * Three rules this file exists to keep:
 *
 *   - A port with no `bench =` line is UNMEASURABLE, never a winner. Most ports
 *     have no meaningful benchmark, and assuming a win for them is exactly the
 *     blind optimisation this replaces.
 *   - The MEDIAN of several runs, not the mean and not one run. One run measures
 *     the scheduler; the mean measures the worst outlier.
 *   - A win under the noise floor is not a win. The floor is measured, not
 *     assumed: it comes from the spread of the baseline's own runs.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kpkg.h"

#define MARCH_RUNS_DEF  5	/* odd, so the median is a real sample        */
#define MARCH_RUNS_MAX  31
#define MARCH_MIN_WIN   3.0	/* per cent, before the noise floor is added  */
#define MARCH_LEDGER    "/var/lib/kdos/march.ledger"

/* ── what the CPU can run ──────────────────────────────────────────────── */

/*
 * The psABI levels, as the flags in /proc/cpuinfo spell them. A machine that
 * cannot run a level must never be measured against it: the build would succeed
 * and the benchmark would die with SIGILL, which is a confusing way to learn
 * something a file already knew.
 */
static const struct {
	const char *level;
	const char *flags[8];
} LEVELS[] = {
	{ "x86-64-v2", { "sse4_2", "popcnt", NULL } },
	{ "x86-64-v3", { "avx2", "bmi2", "fma", "movbe", NULL } },
	{ "x86-64-v4", { "avx512f", "avx512bw", "avx512dq", "avx512vl", NULL } },
};
#define NLEVELS ((int)(sizeof(LEVELS) / sizeof(LEVELS[0])))

static int cpu_has(const char *cpuinfo, const char *flag)
{
	size_t n = strlen(flag);
	for (const char *p = strstr(cpuinfo, flag); p; p = strstr(p + 1, flag)) {
		char before = p == cpuinfo ? ' ' : p[-1];
		char after = p[n];
		/* Whole word: "sse4_2" must not match inside "sse4_2x". */
		if ((before == ' ' || before == '\t' || before == ':') &&
		    (after == ' ' || after == '\n' || after == 0))
			return 1;
	}
	return 0;
}

static int best_level(char *out, size_t cap)
{
	char *info = kb_read_all("/proc/cpuinfo", NULL);
	int best = -1;

	out[0] = 0;
	if (!info)
		return -1;
	for (int i = 0; i < NLEVELS; i++) {
		int ok = 1;
		for (int j = 0; LEVELS[i].flags[j]; j++)
			if (!cpu_has(info, LEVELS[i].flags[j]))
				ok = 0;
		if (ok)
			best = i;
	}
	free(info);
	if (best >= 0)
		kb_strlcpy(out, LEVELS[best].level, cap);
	return best;
}

static int cmd_probe(void)
{
	char *info = kb_read_all("/proc/cpuinfo", NULL);
	char best[32];

	if (!info) {
		fprintf(stderr, "kdos: cannot read /proc/cpuinfo\n");
		return 2;
	}
	for (int i = 0; i < NLEVELS; i++) {
		int ok = 1;
		printf("  %-12s", LEVELS[i].level);
		for (int j = 0; LEVELS[i].flags[j]; j++)
			if (!cpu_has(info, LEVELS[i].flags[j])) {
				ok = 0;
				printf(" missing %s", LEVELS[i].flags[j]);
			}
		printf("%s\n", ok ? " yes" : "");
	}
	free(info);
	best_level(best, sizeof(best));
	printf("\nhighest usable: %s\n", best[0] ? best : "x86-64 (baseline)");
	printf("nothing is built with it until `kdos march run <port>` measures "
	       "a win\n");
	return 0;
}

/* ── timing ────────────────────────────────────────────────────────────── */

static double now_s(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static int dcmp(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}

/*
 * Run `cmd` N times and return the median, with the spread of the samples in
 * `spread` (as a percentage of the median).
 *
 * The spread IS the noise floor. A machine with a busy background, a thermal
 * limit or a noisy filesystem produces a wide one, and a "win" smaller than
 * that is not a measurement — it is the machine breathing.
 */
/* The shell prelude every bench line runs under: the port's own binaries and
 * libraries first, so a bare `lz4` is the one just built. */
static void bench_shell(char *out, size_t cap, const char *root, const char *cmd)
{
	/*
	 * EXPORTED, not prefixed. `VAR=x cmd` applies to one simple command, and
	 * a bench line is a pipeline or an && list by nature — so the prefix
	 * form set PATH for the `dd` and left the `lz4` after it running the
	 * SYSTEM's copy, which is the most convincing way to measure nothing at
	 * all.
	 */
	snprintf(out, cap,
		 "cd '%s' && export PATH='%s/usr/bin:%s/usr/sbin:'$PATH "
		 "LD_LIBRARY_PATH='%s/usr/lib'; %s",
		 root, root, root, root, cmd);
}

/*
 * `bench_setup =` runs ONCE per build and is not timed.
 *
 * It exists because the first bench line written here measured
 * `dd if=/dev/urandom` more than it measured lz4: 64 MiB out of the kernel's
 * RNG dominated the sample and pushed the noise floor to 13.5%, which is wider
 * than any answer worth having. A fixture belongs outside the stopwatch.
 */
static int bench_setup(const char *cmd, const char *root)
{
	if (!cmd || !*cmd)
		return 0;
	char full[4096];
	bench_shell(full, sizeof(full), root, cmd);

	KbArgv a = {0};
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, full);
	kb_argv_end(&a);
	return kb_run(&a);
}

/* More runs narrow the noise floor, which is the only lever a person has on a
 * machine they cannot quiet down. */
static int march_runs(void)
{
	const char *e = getenv("KDOS_MARCH_RUNS");
	int n = e && *e ? atoi(e) : MARCH_RUNS_DEF;
	if (n < 3)
		n = 3;
	if (n > MARCH_RUNS_MAX)
		n = MARCH_RUNS_MAX;
	return n | 1;		/* odd, so the median is a real sample */
}

static double bench(const char *cmd, const char *pkgdir, double *spread)
{
	double t[MARCH_RUNS_MAX];
	int runs = march_runs();

	for (int i = 0; i < runs; i++) {
		KbArgv a = {0};
		char full[4096];
		/* One shell, because a benchmark line is a pipeline by nature
		 * and comes from a recipe in this tree — never from a user. */
		bench_shell(full, sizeof(full), pkgdir, cmd);
		kb_argv_add(&a, "sh");
		kb_argv_add(&a, "-c");
		kb_argv_add(&a, full);
		kb_argv_end(&a);

		/* A failing benchmark has to say WHY: its stderr is the only
		 * evidence, and swallowing it turns "the bench line is wrong"
		 * into "unmeasurable" with no way to tell them apart. */
		int was = kb_proc_verbose;
		kb_proc_verbose = 1;
		double t0 = now_s();
		int rc = kb_run(&a);
		t[i] = now_s() - t0;
		kb_proc_verbose = was;
		if (rc != 0) {
			fprintf(stderr, "  benchmark failed (exit %d): %s\n",
				rc, cmd);
			return -1;
		}
	}
	qsort(t, (size_t)runs, sizeof(t[0]), dcmp);
	double med = t[runs / 2];
	if (spread)
		*spread = med > 0 ? (t[runs - 1] - t[0]) / med * 100.0 : 0;
	return med;
}

/*
 * The decision, alone, so it can be tested without building anything:
 * `kdos march decide <baseline> <optimised> <noise%>` prints the verdict.
 *
 * The rule is one line and the whole discipline is in it — a win counts only
 * when it clears BOTH a fixed floor (3%, below which nobody would notice) and
 * the machine's own measured noise. Everything else is reverted, including a
 * positive number that is smaller than the spread it came out of.
 */
static const char *march_decide(double base, double opt, double noise,
				double *win_out)
{
	double win = base > 0 ? (base - opt) / base * 100.0 : 0;
	if (win_out)
		*win_out = win;
	if (win > MARCH_MIN_WIN + noise)
		return "kept";
	return "reverted";
}

/* ── the ledger ────────────────────────────────────────────────────────── */

static void ledger_append(const char *port, const char *level, double base,
			  double opt, double win, const char *verdict)
{
	const char *path = getenv("KDOS_MARCH_LEDGER");
	if (!path || !*path)
		path = MARCH_LEDGER;

	char *copy = kb_strdup(path);
	char *slash = strrchr(copy, '/');
	if (slash) {
		*slash = 0;
		kb_mkdir_p(copy);
	}
	free(copy);

	FILE *f = fopen(path, "a");
	if (!f)
		return;
	fprintf(f, "%s %s %.4f %.4f %+.1f %s\n", port, level, base, opt, win,
		verdict);
	fclose(f);
}

static int cmd_report(void)
{
	const char *path = getenv("KDOS_MARCH_LEDGER");
	if (!path || !*path)
		path = MARCH_LEDGER;

	char *data = kb_read_all(path, NULL);
	if (!data) {
		printf("nothing measured yet — `kdos march run <port>`\n");
		return 1;
	}

	int kept = 0, reverted = 0, unmeasured = 0;
	printf("%-24s %-12s %10s %10s %8s\n", "port", "level", "baseline",
	       "optimised", "change");
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		char buf[512];
		size_t n = nl ? (size_t)(nl - line) : strlen(line);

		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, line, n);
		buf[n] = 0;
		line = nl ? nl + 1 : NULL;
		if (!buf[0])
			continue;

		char port[128] = "", level[32] = "", verdict[32] = "";
		double base = 0, opt = 0, win = 0;
		if (sscanf(buf, "%127s %31s %lf %lf %lf %31s", port, level,
			   &base, &opt, &win, verdict) != 6)
			continue;
		printf("%-24s %-12s %9.3fs %9.3fs %+7.1f%%  %s\n", port, level,
		       base, opt, win, verdict);
		if (!strcmp(verdict, "kept"))
			kept++;
		else if (!strcmp(verdict, "reverted"))
			reverted++;
		else
			unmeasured++;
	}
	free(data);

	/*
	 * The summary line the roadmap asks for by name: "tried v3 on 40 ports,
	 * kept 22, reverted 18". A list of winners alone would be a sales
	 * pitch; the reverts are the evidence that the measuring is real.
	 */
	printf("\n%d kept, %d reverted, %d unmeasurable\n", kept, reverted,
	       unmeasured);
	return 0;
}

/* ── the run ───────────────────────────────────────────────────────────── */

/*
 * Unpack the package that was just built, so the benchmark runs against THIS
 * build's binaries and nothing on the system's PATH. Without this the two runs
 * measure the installed copy twice and agree perfectly, which is the most
 * convincing way to be wrong.
 */
static int unpack(const char *pkgdir, const char *root)
{
	char **files = kb_listdir(pkgdir, NULL);
	char *pkg = NULL;

	for (char **p = files; p && *p; p++) {
		size_t n = strlen(*p);
		if (n > 7 && !strcmp(*p + n - 7, ".tar.xz"))
			pkg = kb_path_join(pkgdir, *p);
	}
	kb_strv_free(files);
	if (!pkg)
		return -1;

	kb_mkdir_p(root);
	KbArgv a = {0};
	kb_argv_add(&a, "tar");
	kb_argv_add(&a, "-xf");
	kb_argv_add(&a, pkg);
	kb_argv_add(&a, "-C");
	kb_argv_add(&a, (char *)root);
	kb_argv_end(&a);
	int rc = kb_run(&a);
	free(pkg);
	return rc;
}

static int build_port(const char *portdir, const char *pkgdir,
		      const char *extra_cflags)
{
	KbArgv a = {0};
	char cmd[4096];
	const char *base = getenv("CFLAGS");

	snprintf(cmd, sizeof(cmd),
		 "cd '%s' && CFLAGS='%s %s' CXXFLAGS='%s %s' "
		 "PACKAGE_DIR='%s' kpkgbuild",
		 portdir, base ? base : "-O2 -pipe", extra_cflags,
		 base ? base : "-O2 -pipe", extra_cflags, pkgdir);
	kb_argv_add(&a, "sh");
	kb_argv_add(&a, "-c");
	kb_argv_add(&a, cmd);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

static int run_one(const KpConf *c, const char *port, const char *level)
{
	char *portdir = kp_port_dir(c, port);
	if (!portdir) {
		fprintf(stderr, "kdos: no port named %s\n", port);
		return 1;
	}

	/*
	 * How this port is measured is the port's own business, declared as
	 * `bench = <shell line>` in its recipe. Without one there is nothing to
	 * measure, and the honest answer is to say so rather than to guess from
	 * a category or a name.
	 */
	char benchline[512];
	kp_recipe_key(portdir, "bench", benchline, sizeof(benchline));
	if (!benchline[0]) {
		printf("%-24s unmeasurable — no `bench =` line in the recipe\n",
		       port);
		ledger_append(port, level, 0, 0, 0, "unmeasurable");
		free(portdir);
		return 0;
	}

	char work[512];
	const char *tmp = getenv("TMPDIR");
	snprintf(work, sizeof(work), "%s/kdos-march.XXXXXX",
		 tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(work)) {
		free(portdir);
		return 1;
	}

	char basedir[600], optdir[600];
	snprintf(basedir, sizeof(basedir), "%s/base", work);
	snprintf(optdir, sizeof(optdir), "%s/opt", work);
	kb_mkdir_p(basedir);
	kb_mkdir_p(optdir);

	char flag[64];
	snprintf(flag, sizeof(flag), "-march=%s", level);

	printf("%s: building twice (baseline, then %s)...\n", port, flag);
	int rc = 1;
	if (build_port(portdir, basedir, "") != 0) {
		fprintf(stderr, "%s: the baseline build failed\n", port);
		goto out;
	}
	if (build_port(portdir, optdir, flag) != 0) {
		fprintf(stderr, "%s: the %s build failed — that is a RESULT, "
				"not an error: this port cannot use it\n",
			port, flag);
		ledger_append(port, level, 0, 0, 0, "unbuildable");
		rc = 0;
		goto out;
	}

	char baseroot[700], optroot[700];
	snprintf(baseroot, sizeof(baseroot), "%s/base-root", work);
	snprintf(optroot, sizeof(optroot), "%s/opt-root", work);
	if (unpack(basedir, baseroot) != 0 || unpack(optdir, optroot) != 0) {
		fprintf(stderr, "%s: could not unpack a built package\n", port);
		ledger_append(port, level, 0, 0, 0, "unmeasurable");
		rc = 0;
		goto out;
	}

	char setupline[512];
	kp_recipe_key(portdir, "bench_setup", setupline, sizeof(setupline));
	bench_setup(setupline, baseroot);
	bench_setup(setupline, optroot);

	double bs = 0, os_ = 0;
	printf("%s: benchmarking %d runs each...\n", port, march_runs());
	double b = bench(benchline, baseroot, &bs);
	double o = bench(benchline, optroot, &os_);
	if (b <= 0 || o <= 0) {
		ledger_append(port, level, 0, 0, 0, "unmeasurable");
		rc = 0;
		goto out;
	}

	double floor_ = bs > os_ ? bs : os_;
	double win = 0;
	const char *verdict = march_decide(b, o, floor_, &win);

	printf("%-24s %s  baseline %.3fs  %s %.3fs  %+.1f%% "
	       "(noise %.1f%%) -> %s\n",
	       port, level, b, flag, o, win, floor_, verdict);
	if (!strcmp(verdict, "reverted") && win > 0)
		printf("%-24s   the win is inside the noise; that is not a "
		       "win\n", "");
	if (floor_ > 5.0)
		printf("%-24s   this machine's noise floor is %.1f%% — close "
		       "things down or raise KDOS_MARCH_RUNS\n", "", floor_);
	ledger_append(port, level, b, o, win, verdict);
	rc = 0;
out:
	kb_rmtree(work);
	free(portdir);
	return rc;
}

int march_main(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : "probe";

	if (!strcmp(cmd, "probe"))
		return cmd_probe();
	if (!strcmp(cmd, "report"))
		return cmd_report();
	if (!strcmp(cmd, "decide")) {
		if (argc < 5) {
			fprintf(stderr, "usage: kdos march decide <baseline> "
					"<optimised> <noise%%>\n");
			return 2;
		}
		double win = 0;
		const char *v = march_decide(atof(argv[2]), atof(argv[3]),
					     atof(argv[4]), &win);
		printf("%+.1f%% %s\n", win, v);
		return strcmp(v, "kept") == 0 ? 0 : 1;
	}
	if (strcmp(cmd, "run")) {
		fprintf(stderr, "usage: kdos march {probe|run <port>...|"
				"report}\n");
		return 2;
	}

	char level[32];
	if (best_level(level, sizeof(level)) < 0 || !level[0]) {
		fprintf(stderr, "kdos: this CPU is baseline x86-64 — there is "
				"nothing to measure\n");
		return 1;
	}
	const char *want = getenv("KDOS_MARCH_LEVEL");
	if (want && *want)
		kb_strlcpy(level, want, sizeof(level));

	if (!kb_have_prog("kpkgbuild")) {
		fprintf(stderr, "kdos: kpkgbuild is not on PATH\n");
		return 1;
	}

	KpConf c;
	kp_conf_load(&c);

	printf("measuring against %s, %d runs per build\n\n", level,
	       march_runs());
	int rc = 0;
	for (int i = 2; i < argc; i++)
		if (run_one(&c, argv[i], level) != 0)
			rc = 1;
	if (argc <= 2)
		fprintf(stderr, "kdos: name at least one port\n");
	return rc;
}
