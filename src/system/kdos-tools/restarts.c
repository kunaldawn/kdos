/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos restarts — what is running code you deleted
 *
 *   firefox-esr   (pid 2841)  nss, firefox-esr
 *   gimp          (pid 3122)  babl
 *
 * After an upgrade, every process that had the old shared library mapped keeps
 * running it: the file is unlinked but the inode survives while a mapping holds
 * it. That is not a bug — it is what makes an upgrade safe on a running system —
 * but it does mean a security fix is not actually in effect until the process
 * restarts, and nothing tells you which processes those are.
 *
 * THREE OWNERS, ONE JOIN, and the join is the whole feature:
 *
 *   - kpkg has an EXACT file manifest. Not a heuristic about which package
 *     probably owns /usr/lib/libfoo.so.1 — the database says.
 *   - /proc/<pid>/maps marks a mapping whose file has been unlinked with the
 *     literal suffix ` (deleted)`. That is the kernel telling you, not an
 *     inference from mtimes.
 *   - the desktop knows which of those processes has a window.
 *
 * Prior art has the pieces and not the join: `needrestart` is Debian-only and
 * heuristic, `zypper ps` has had an open bug since 2018 asking it to say
 * "reboot needed", KDE has an open request for the notification alone, and
 * Fedora sidesteps the question by rebooting into an offline updater.
 *
 * WHAT IT WILL NOT DO is guess. A mapping the database does not claim is
 * reported as owned by nothing rather than attributed to a likely package, and
 * a process whose maps cannot be read (another user's, or one that exited
 * mid-scan) is skipped rather than assumed clean.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kpkg.h"

#define MAX_PROCS 256
#define MAX_PKGS_PER_PROC 12

struct hit {
	int pid;
	char comm[64];
	char exe[256];
	const char *pkgs[MAX_PKGS_PER_PROC];
	int npkgs;
};

static struct hit hits[MAX_PROCS];
static int nhits;

static int is_number(const char *s)
{
	if (!*s)
		return 0;
	for (; *s; s++)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

static void read_first_line(const char *path, char *out, size_t len)
{
	FILE *f = fopen(path, "r");
	out[0] = '\0';
	if (!f)
		return;
	if (fgets(out, (int)len, f))
		out[strcspn(out, "\n")] = '\0';
	fclose(f);
}

static void add_pkg(struct hit *h, const char *pkg)
{
	for (int i = 0; i < h->npkgs; i++)
		if (!strcmp(h->pkgs[i], pkg))
			return;
	if (h->npkgs < MAX_PKGS_PER_PROC)
		h->pkgs[h->npkgs++] = pkg;
}

/*
 * One process. Returns 1 if it is running deleted code.
 *
 * The `(deleted)` suffix is the kernel's, and it is exact: the mapping refers to
 * an inode with no remaining name. A file that was REPLACED rather than removed
 * also shows this way, because a package manager that installs over a running
 * binary unlinks the old inode — which is precisely the upgrade case this exists
 * to catch.
 */
static int scan_proc(const char *pid_s, const KpOwned *owned, struct hit *h)
{
	char path[512], line[4096];

	snprintf(path, sizeof(path), "/proc/%s/maps", pid_s);
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;	/* another user's, or it exited mid-scan */

	h->pid = atoi(pid_s);
	h->npkgs = 0;
	int found = 0;

	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		size_t n = strlen(line);
		static const char SUF[] = " (deleted)";
		const size_t suflen = sizeof(SUF) - 1;
		if (n <= suflen || strcmp(line + n - suflen, SUF))
			continue;

		/* The path is the sixth field and may itself contain spaces, so
		 * it is found by skipping five fields rather than by tokenising
		 * the whole line. */
		const char *p = line;
		for (int field = 0; field < 5 && p; field++) {
			p = strchr(p, ' ');
			while (p && *p == ' ')
				p++;
		}
		if (!p || *p != '/')
			continue;	/* anonymous, or a special mapping */

		char file[1024];
		size_t flen = (size_t)(line + n - suflen - p);
		if (flen >= sizeof(file))
			continue;
		memcpy(file, p, flen);
		file[flen] = '\0';

		/*
		 * A deleted mapping under /memfd:, /dev/zero or /SYSV is normal
		 * and permanent — shared memory, not an upgraded library. Only
		 * something a package could own is interesting.
		 */
		if (strncmp(file, "/usr/", 5) && strncmp(file, "/lib", 4) &&
		    strncmp(file, "/bin/", 5) && strncmp(file, "/sbin/", 6) &&
		    strncmp(file, "/opt/", 5))
			continue;

		found = 1;
		/* The database stores paths without the leading slash. */
		const char *owner = owned ? kp_owned_owner(owned, file + 1) : NULL;
		if (owner)
			add_pkg(h, owner);
	}
	fclose(f);

	if (!found)
		return 0;

	snprintf(path, sizeof(path), "/proc/%s/comm", pid_s);
	read_first_line(path, h->comm, sizeof(h->comm));
	snprintf(path, sizeof(path), "/proc/%s/exe", pid_s);
	ssize_t r = readlink(path, h->exe, sizeof(h->exe) - 1);
	h->exe[r > 0 ? r : 0] = '\0';
	return 1;
}

int restarts_main(int argc, char **argv)
{
	int quiet = 0, json = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q"))
			quiet = 1;
		else if (!strcmp(argv[i], "--json"))
			json = 1;
		else {
			fprintf(stderr, "usage: kdos restarts [--quiet] [--json]\n");
			return 2;
		}
	}

	KpConf c;
	kp_conf_load(&c);
	KpOwned *owned = kp_owned_load(&c);

	DIR *d = opendir("/proc");
	struct dirent *e;
	int truncated = 0;
	if (!d) {
		fprintf(stderr, "kdos restarts: /proc is not mounted\n");
		return 1;
	}
	while ((e = readdir(d))) {
		if (!is_number(e->d_name))
			continue;
		if (nhits == MAX_PROCS) {
			/* Reported, never silent. A list that stops at 256 and
			 * says nothing reads as "that is all of them", which is
			 * the one thing this tool must not imply. */
			truncated = 1;
			break;
		}
		if (scan_proc(e->d_name, owned, &hits[nhits]))
			nhits++;
	}
	closedir(d);

	if (json) {
		KbBuf b = {0};
		kb_buf_printf(&b, "{\n  \"truncated\": %s,\n  \"processes\": [",
			      truncated ? "true" : "false");
		for (int i = 0; i < nhits; i++) {
			const struct hit *h = &hits[i];
			kb_buf_printf(&b, "%s\n    {\"pid\": %d, \"comm\": ",
				      i ? "," : "", h->pid);
			kb_json_str(&b, h->comm);
			kb_buf_str(&b, ", \"exe\": ");
			kb_json_str(&b, h->exe);
			kb_buf_str(&b, ", \"packages\": [");
			for (int j = 0; j < h->npkgs; j++) {
				if (j)
					kb_buf_str(&b, ", ");
				kb_json_str(&b, h->pkgs[j]);
			}
			kb_buf_str(&b, "]}");
		}
		kb_buf_printf(&b, "%s  ],\n  \"count\": %d\n}\n",
			      nhits ? "\n" : "", nhits);
		fwrite(b.p, 1, b.n, stdout);
		kb_buf_free(&b);
		kp_owned_free(owned);
		return nhits ? 1 : 0;
	}

	if (nhits == 0) {
		if (!quiet)
			printf("Nothing is running deleted code.\n");
		kp_owned_free(owned);
		return 0;
	}

	if (!quiet) {
		printf("These are running code that has been replaced or "
		       "removed.\nThey keep the old version until they "
		       "restart:\n\n");
		for (int i = 0; i < nhits; i++) {
			const struct hit *h = &hits[i];
			printf("  %-16s (pid %d)  ", h->comm, h->pid);
			if (h->npkgs == 0) {
				/* Named as unowned rather than guessed at — see
				 * the header. */
				printf("no installed package owns it");
			} else {
				for (int j = 0; j < h->npkgs; j++)
					printf("%s%s", j ? ", " : "",
					       h->pkgs[j]);
			}
			printf("\n");
		}
		printf("\n%d process%s. Restarting them puts the new code in "
		       "effect.\n", nhits, nhits == 1 ? "" : "es");
		if (truncated)
			printf("(stopped at %d — there are more)\n", MAX_PROCS);
	}

	kp_owned_free(owned);
	/* Exit 1 when something needs restarting, so this can gate a script and
	 * so the panel can call it without parsing anything. */
	return 1;
}
