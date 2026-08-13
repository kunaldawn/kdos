/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos rebuild — the stick rebuilds the stick
 *
 * Every leg of this is old. The LFS LiveCD shipped its sources in 2005, FreeBSD
 * has shipped /usr/src for thirty years, and live-bootstrap builds a system from
 * a 357-byte seed. What none of them does is **rebuild the medium from the
 * medium**: boot the stick, and with no network at any point produce the ISO
 * that stick was made from.
 *
 * KDOS can, because of three properties it already had and one this adds:
 *
 *   - the repo builds offline — every tarball and every vendor bundle is in
 *     ports/, which is why there is nothing to download
 *   - KDOS can build KDOS — the shipped system carries gcc, binutils, make,
 *     meson, ninja, python3 and kpkg
 *   - packages are reproducible (P12), so a rebuild can be COMPARED to what it
 *     was built from rather than merely produced
 *   - and now: `make build KDOS_ISO_SOURCES=1` puts ports/, src/ and script/ on
 *     the ISO beside system.sfs
 *
 * What this command is: the honest front door to that. It finds the sources,
 * checks the machine can actually do the work, copies the tree somewhere
 * writable, compiles the orchestrator out of that tree, and runs it. What it is
 * NOT: a second build system. Everything after the checks is `kdosbuild`, the
 * same one `make build` runs, reading the same phase scripts.
 *
 * The checks matter more than the running. A live stick's root filesystem is an
 * overlay in RAM: a rebuild started there fills memory and dies hours in, with
 * the machine unusable and nothing to show. So the work directory must be
 * somewhere real, and this refuses rather than discovers that later.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "kdos-tools.h"

/* Measured against a full run: the tree itself is ~3 GB, build/fs another ~6,
 * the appbox image ~4, and the ISO on top. Refusing at 25 is refusing the runs
 * that would fail at hour four. */
#define NEED_GB 25

/* Where the sources might be, in the order worth trying: an explicit answer, a
 * booted stick, an installed copy, the directory we are standing in. */
static const char *const SOURCE_DIRS[] = {
	"/mnt/iso/sources",
	"/kdos",
	".",
};

static int looks_like_tree(const char *dir)
{
	char p[1024];
	snprintf(p, sizeof(p), "%s/script/kdosbuild.sh", dir);
	if (!kb_path_exists(p))
		return 0;
	snprintf(p, sizeof(p), "%s/ports/core", dir);
	if (!kb_is_dir(p))
		return 0;
	snprintf(p, sizeof(p), "%s/src/build/kdosbuild", dir);
	return kb_is_dir(p);
}

static const char *find_sources(void)
{
	static char found[1024];
	const char *env = getenv("KDOS_SOURCES");

	if (env && *env) {
		if (!looks_like_tree(env)) {
			fprintf(stderr, "kdos: KDOS_SOURCES=%s is not a KDOS "
					"tree\n", env);
			return NULL;
		}
		kb_strlcpy(found, env, sizeof(found));
		return found;
	}
	for (size_t i = 0; i < sizeof(SOURCE_DIRS) / sizeof(SOURCE_DIRS[0]); i++)
		if (looks_like_tree(SOURCE_DIRS[i])) {
			kb_strlcpy(found, SOURCE_DIRS[i], sizeof(found));
			return found;
		}
	return NULL;
}

static long long free_gb(const char *path)
{
	struct statvfs v;
	if (statvfs(path, &v) != 0)
		return -1;
	return (long long)((double)v.f_bavail * (double)v.f_frsize /
			   (1024.0 * 1024.0 * 1024.0));
}

/*
 * Is this path in RAM?
 *
 * A live stick's `/` is an overlay whose upper layer is a tmpfs, so it reports
 * gigabytes free and eats memory instead of disk. That is the one failure this
 * command exists to prevent, and it is invisible to a free-space check.
 */
static int is_volatile(const char *path)
{
	size_t len = 0;
	char *mounts = kb_read_all("/proc/mounts", &len);
	int bad = 0;

	if (!mounts)
		return 0;
	/*
	 * realpath into a buffer of OUR choosing is a fortify abort waiting to
	 * happen: glibc knows it may write PATH_MAX and refuses anything
	 * smaller, loudly and at runtime. Letting it allocate is the only
	 * version that is correct at every path length.
	 */
	char *real = realpath(path, NULL);
	char *fallback = NULL;
	if (!real) {
		fallback = kb_strdup(path);
		real = fallback;
	}

	/* The longest mount point that is a prefix of the path is the one the
	 * path is on. Anything shorter is an ancestor and says nothing. */
	size_t best = 0;
	char besttype[64] = "";
	for (char *line = mounts; line && *line;) {
		char *nl = strchr(line, '\n');
		char buf[1024];
		size_t n = nl ? (size_t)(nl - line) : strlen(line);

		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, line, n);
		buf[n] = 0;
		line = nl ? nl + 1 : NULL;

		char *dev = strtok(buf, " ");
		char *mnt = dev ? strtok(NULL, " ") : NULL;
		char *type = mnt ? strtok(NULL, " ") : NULL;
		if (!mnt || !type)
			continue;
		size_t ml = strlen(mnt);
		if (strncmp(real, mnt, ml))
			continue;
		if (ml > 1 && real[ml] && real[ml] != '/')
			continue;
		if (ml >= best) {
			best = ml;
			kb_strlcpy(besttype, type, sizeof(besttype));
		}
	}
	free(mounts);
	free(real);
	if (!strcmp(besttype, "tmpfs") || !strcmp(besttype, "ramfs") ||
	    !strcmp(besttype, "overlay"))
		bad = 1;
	return bad;
}

static int have_all(const char *const *tools, const char **missing)
{
	for (int i = 0; tools[i]; i++)
		if (!kb_have_prog(tools[i])) {
			*missing = tools[i];
			return 0;
		}
	return 1;
}

int rebuild_main(int argc, char **argv)
{
	const char *work = NULL;
	int dry = 0, iso_only = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!strcmp(argv[i], "--iso-only"))
			iso_only = 1;
		else if (argv[i][0] != '-')
			work = argv[i];
		else {
			fprintf(stderr,
				"usage: kdos rebuild [--dry-run] [--iso-only] "
				"<work-directory>\n"
				"\n"
				"  Rebuilds KDOS from the sources on this "
				"machine, with no network.\n"
				"  The work directory needs ~%d GB and must be "
				"on real storage.\n", NEED_GB);
			return 2;
		}
	}

	const char *src = find_sources();
	if (!src) {
		fprintf(stderr,
			"kdos: no KDOS tree found.\n"
			"      A stick built with KDOS_ISO_SOURCES=1 carries "
			"one at /mnt/iso/sources;\n"
			"      otherwise point KDOS_SOURCES at a checkout.\n");
		return 1;
	}
	printf("sources    %s\n", src);

	char stamp[1024];
	snprintf(stamp, sizeof(stamp), "%.1000s/SOURCES", src);
	char *info = kb_read_all(stamp, NULL);
	if (info) {
		for (char *l = info; l && *l;) {
			char *nl = strchr(l, '\n');
			size_t n = nl ? (size_t)(nl - l) : strlen(l);
			if (*l != '#')
				printf("           %.*s\n", (int)n, l);
			l = nl ? nl + 1 : NULL;
		}
		free(info);
	}

	if (!work) {
		fprintf(stderr, "kdos: say where to work — `kdos rebuild "
				"/mnt/disk/rebuild`\n");
		return 2;
	}

	/* Everything that would fail later, asked now. */
	const char *missing = NULL;
	static const char *const BUILD_TOOLS[] = { "cc", "make", "bash", "tar",
						   "xz", NULL };
	static const char *const ISO_TOOLS[] = { "mksquashfs", "xorriso",
						 "mkfs.fat", NULL };
	if (!have_all(BUILD_TOOLS, &missing)) {
		fprintf(stderr, "kdos: %s is missing — this image cannot "
				"build\n", missing);
		return 1;
	}
	if (!have_all(ISO_TOOLS, &missing))
		printf("note       %s is missing; packages will build but no "
		       "ISO will come out\n", missing);

	kb_mkdir_p(work);
	if (!kb_is_dir(work)) {
		fprintf(stderr, "kdos: cannot create %s\n", work);
		return 1;
	}
	if (is_volatile(work)) {
		fprintf(stderr,
			"kdos: %s is in RAM (tmpfs/overlay).\n"
			"      A live stick's root is an overlay, so a rebuild "
			"there fills memory and\n"
			"      dies hours in. Mount a disk and point this at "
			"it.\n", work);
		return 1;
	}
	long long gb = free_gb(work);
	printf("work       %s (%lld GB free)\n", work, gb);
	if (gb >= 0 && gb < NEED_GB) {
		fprintf(stderr, "kdos: %lld GB free, %d needed\n", gb, NEED_GB);
		return 1;
	}

	/*
	 * The tree is COPIED rather than built in place. On a stick the source
	 * is a read-only ISO9660 and the build writes into it constantly; on an
	 * installed system, building in place would edit the tree the machine
	 * boots from.
	 */
	char tree[1200];
	snprintf(tree, sizeof(tree), "%s/kdos", work);
	printf("plan       copy the tree to %s\n", tree);
	printf("           compile the orchestrator from src/build/kdosbuild\n");
	printf("           run %s\n", iso_only ? "the packaging phase only"
					       : "every phase");
	if (dry) {
		printf("\n--dry-run: nothing was copied and nothing was "
		       "built.\n");
		return 0;
	}

	if (!kb_is_dir(tree)) {
		KbArgv a = {0};
		char from[1100];
		snprintf(from, sizeof(from), "%s/.", src);
		kb_argv_add(&a, "cp");
		kb_argv_add(&a, "-a");
		kb_argv_add(&a, from);
		kb_argv_add(&a, tree);
		kb_argv_end(&a);
		kb_mkdir_p(tree);
		printf("\ncopying the tree (this takes a while and no "
		       "network)...\n");
		if (kb_run_tty(&a) != 0) {
			fprintf(stderr, "kdos: copying the tree failed\n");
			return 1;
		}
	} else {
		printf("\nreusing the tree already at %s\n", tree);
	}

	/*
	 * The orchestrator is compiled here rather than shipped: it links
	 * nothing but libc, it is a two-second `cc`, and compiling it out of
	 * the tree being built is what guarantees the build is driven by the
	 * sources on this machine rather than by a binary from somewhere else.
	 * Same shape as `ports/fetch` and `ports/update`.
	 */
	char bin[1300];
	snprintf(bin, sizeof(bin), "%s/kdosbuild", work);
	{
		KbArgv a = {0};
		char inc[1300], glob[1300], libs[1300], libb[1300];
		snprintf(inc, sizeof(inc), "-I%s/src/libs/libkbase", tree);
		snprintf(glob, sizeof(glob), "%s/src/build/kdosbuild", tree);
		snprintf(libs, sizeof(libs), "%s/src/libs/libkbuild", tree);
		snprintf(libb, sizeof(libb), "%s/src/libs/libktui", tree);
		printf("compiling the orchestrator...\n");
		kb_argv_add(&a, "sh");
		kb_argv_add(&a, "-c");
		/* One shell, for its globs alone, and every path it sees is one
		 * this program built. There is no user input anywhere in it. */
		char cmd[16384];
		snprintf(cmd, sizeof(cmd),
			 "cc -O2 -std=gnu11 -D_GNU_SOURCE -o '%.900s' "
			 "-I'%.900s' -I'%.900s' -I'%.900s' "
			 "-I'%.900s/src/libs/libkcolor' "
			 "'%.900s'/*.c '%.900s/src/libs/libkbase'/*.c "
			 "'%.900s'/*.c '%.900s'/*.c "
			 "'%.900s/src/libs/libkcolor'/*.c",
			 bin, glob, inc + 2, libs, tree, glob, tree, libs, libb,
			 tree);
		kb_argv_add(&a, cmd);
		kb_argv_end(&a);
		if (kb_run_tty(&a) != 0) {
			fprintf(stderr, "kdos: could not compile kdosbuild\n");
			return 1;
		}
	}

	if (chdir(tree) != 0) {
		fprintf(stderr, "kdos: cannot enter %s\n", tree);
		return 1;
	}

	KbArgv a = {0};
	kb_argv_add(&a, bin);
	kb_argv_add(&a, "--script-dir");
	kb_argv_add(&a, "script");
	kb_argv_add(&a, "--build-dir");
	kb_argv_add(&a, "build");
	kb_argv_add(&a, "--fresh");
	if (iso_only) {
		kb_argv_add(&a, "--phases");
		kb_argv_add(&a, "06_packaging");
	}
	kb_argv_end(&a);
	printf("\nrunning the build. Nothing from here on needs a network.\n\n");
	return kb_run_tty(&a);
}
