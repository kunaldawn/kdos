/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos firmware sync — the firmware this machine asks for, installed
 *
 *   kdos firmware sync [--root DIR] [--dry-run [--json]] [--all]
 *
 *   firmware-nvidia module nouveau, pci 0000:01:00.0
 *   firmware-qualcomm the kernel could not load ath11k/WCN6855/hw2.0/amss.bin
 *
 * linux-firmware is split by vendor (firmware-intel, firmware-amd, …), and
 * the installation medium carries only the common ones. This finds the
 * vendor ports this machine's hardware needs and has not got, from two
 * places:
 *
 *   - every device under /sys/bus/<bus>/devices/<dev>, by its modalias:
 *     the module bound to it, and every module modules.alias (and, for a
 *     built-in driver, modules.builtin.modinfo) says would bind it. Each
 *     such module's `firmware:` lines (modinfo -F firmware) name the files
 *     its driver may request;
 *   - the kernel log, /dev/kmsg: every "Direct firmware load for <path>
 *     failed" and "firmware: failed to load <path>" line.
 *
 * A file present under <root>/lib/firmware (as itself, .zst or .xz) is
 * satisfied. Any other is looked up in /usr/share/kdos/firmware-map, which
 * the firmware-map port generates from the same tarball and classifier as
 * the vendor ports: one `<port> <path>` line per file and link they
 * install. A mapped file whose port is not installed in <root> names that
 * port. A module's list names every chip the driver supports, so a module
 * stands for its whole vendor port and never for one file; a kernel-log path
 * no port ships is reported, never guessed at.
 *
 * MODULE DEPENDENCIES ARE NOT FOLLOWED. btusb depends on btintel, btrtl,
 * btbcm and btmtk, whose lists between them name four vendors' Bluetooth
 * firmware; following the edge would install all four for any Bluetooth
 * dongle. The chip's own firmware request reaches the kernel log instead.
 *
 * Without --dry-run each port goes in through `kdos update install
 * --prebuilt [--root DIR] <port>`, after `kdos update fetch --binhost-only`,
 * one argument vector each: this command installs nothing itself, so a port
 * takes the same signed route, origin record and world set as any other.
 *
 * Exit: 0 nothing to install, or every install succeeded; 1 a dry run that
 * names a port, an install that failed, or a sync without root; 2 usage;
 * 3 no firmware map in DIR or on this system, so nothing could be decided.
 *
 * $KDOS_FIRMWARE_SYSROOT moves /sys, /lib/modules and /dev/kmsg for a test
 * and $KDOS_FIRMWARE_KVER the kernel release; modinfo is found on $PATH,
 * which is where a test puts its stand-in.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "kdos-tools.h"

#define FW_MAP "/usr/share/kdos/firmware-map"

typedef struct {
	char *path;		/* relative to /lib/firmware, no .zst */
	char *port;
} FwEntry;

typedef struct {
	FwEntry *v;
	int n;
	char *text;
} FwMap;

typedef struct {
	char *pat;
	char *mod;
	size_t lit;		/* bytes before the first wildcard */
} FwAlias;

typedef struct {
	char port[64];
	char reason[512];
} FwNeed;

typedef struct {
	char mod[64];
	char dev[160];		/* "pci 0000:01:00.0" */
} FwMod;

static const char *fw_sysroot(void)
{
	const char *e = getenv("KDOS_FIRMWARE_SYSROOT");
	return e && *e ? e : "";
}

static int fw_entry_cmp(const void *a, const void *b)
{
	return strcmp(((const FwEntry *)a)->path, ((const FwEntry *)b)->path);
}

/* `<port> <path>` lines; the path is the rest of the line, spaces and all. */
static int fw_map_load(FwMap *m, const char *path)
{
	memset(m, 0, sizeof(*m));
	m->text = kb_read_whole(path, NULL);
	if (!m->text)
		return -1;
	int cap = 0;
	for (char *l = m->text, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		char *sp = strchr(l, ' ');
		if (*l == '#' || !sp || !sp[1])
			continue;
		*sp = 0;
		if (m->n == cap) {
			cap = cap ? cap * 2 : 4096;
			m->v = kb_realloc(m->v, (size_t)cap * sizeof(*m->v));
		}
		m->v[m->n].port = l;
		m->v[m->n].path = sp + 1;
		m->n++;
	}
	qsort(m->v, (size_t)m->n, sizeof(*m->v), fw_entry_cmp);
	return 0;
}

static void fw_map_free(FwMap *m)
{
	free(m->v);
	free(m->text);
}

/* The port that ships `name`, or NULL. A name with a wildcard (a few
 * drivers declare one) is the first map path it matches. */
static const char *fw_map_port(const FwMap *m, const char *name)
{
	if (strpbrk(name, "*?[")) {
		for (int i = 0; i < m->n; i++)
			if (!fnmatch(name, m->v[i].path, FNM_PATHNAME))
				return m->v[i].port;
		return NULL;
	}
	FwEntry key = { .path = (char *)name };
	FwEntry *e = bsearch(&key, m->v, (size_t)m->n, sizeof(*m->v),
			     fw_entry_cmp);
	return e ? e->port : NULL;
}

static int fw_safe_name(const char *s)
{
	if (!*s || *s == '/' || strstr(s, "..") || strlen(s) > 255)
		return 0;
	for (; *s; s++)
		if ((unsigned char)*s < 0x20 || *s == 0x7f)
			return 0;
	return 1;
}

/* The kernel looks in updates/ before the tree, and takes each name as it is
 * or with the compressed suffix its loader was built for. */
static int fw_present(const char *root, const char *name)
{
	static const char *const dirs[] = { "/lib/firmware/updates/",
					    "/lib/firmware/" };
	static const char *const ext[] = { "", ".zst", ".xz" };
	char p[1024];

	if (strpbrk(name, "*?["))
		return 0;
	for (int d = 0; d < 2; d++)
		for (int e = 0; e < 3; e++) {
			snprintf(p, sizeof(p), "%s%s%s%s", root, dirs[d], name,
				 ext[e]);
			if (!access(p, F_OK))
				return 1;
		}
	return 0;
}

static int fw_installed(const char *root, const char *port)
{
	char p[512];
	snprintf(p, sizeof(p), "%s/var/lib/kpkg/db/%s", root, port);
	return !access(p, F_OK);
}

static FwNeed *g_need;
static int g_nneed;
static char **g_unshipped;
static int g_nunshipped;

static void fw_need_add(const char *port, const char *reason)
{
	for (int i = 0; i < g_nneed; i++)
		if (!strcmp(g_need[i].port, port))
			return;
	g_need = kb_realloc(g_need, (size_t)(g_nneed + 1) * sizeof(*g_need));
	kb_strlcpy(g_need[g_nneed].port, port, sizeof(g_need[0].port));
	kb_strlcpy(g_need[g_nneed].reason, reason, sizeof(g_need[0].reason));
	g_nneed++;
}

static void fw_unshipped_add(const char *path)
{
	for (int i = 0; i < g_nunshipped; i++)
		if (!strcmp(g_unshipped[i], path))
			return;
	g_unshipped = kb_realloc(g_unshipped, (size_t)(g_nunshipped + 1) *
				 sizeof(*g_unshipped));
	g_unshipped[g_nunshipped++] = kb_strdup(path);
}

/* ── the kernel log ────────────────────────────────────────────────────── */

static void fw_klog_line(const FwMap *m, const char *root, char *line)
{
	static const char *const pre[] = { "Direct firmware load for ",
					   "firmware: failed to load " };
	char *msg = line;

	/* A /dev/kmsg record is `prio,seq,usec,flags;text`; its dictionary
	 * lines start with a space and carry no message. */
	if (*line == ' ')
		return;
	char *semi = strchr(line, ';');
	if (semi && strspn(line, "0123456789,-c") == (size_t)(semi - line))
		msg = semi + 1;
	for (int i = 0; i < 2; i++) {
		char *at = strstr(msg, pre[i]);
		if (!at)
			continue;
		at += strlen(pre[i]);
		char *end = strstr(at, i ? " (" : " failed");
		if (!end)
			continue;
		*end = 0;
		if (!fw_safe_name(at) || fw_present(root, at))
			return;
		const char *port = fw_map_port(m, at);
		if (!port)
			fw_unshipped_add(at);
		else if (!fw_installed(root, port)) {
			char why[512];
			snprintf(why, sizeof(why),
				 "the kernel could not load %s", at);
			fw_need_add(port, why);
		}
		return;
	}
}

/* 1 when the log was read. As a user under kernel.dmesg_restrict the open
 * fails, and the devices alone decide. */
static int fw_klog(const FwMap *m, const char *root)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/dev/kmsg", fw_sysroot());
	int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return 0;
	struct stat st;
	if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
		/* One record per read, from the oldest the ring still
		 * holds; EPIPE is a record overwritten while reading. */
		char rec[8192];
		for (;;) {
			ssize_t r = read(fd, rec, sizeof(rec) - 1);
			if (r < 0 && (errno == EINTR || errno == EPIPE))
				continue;
			if (r <= 0)
				break;
			rec[r] = 0;
			rec[strcspn(rec, "\n")] = 0;
			fw_klog_line(m, root, rec);
		}
		close(fd);
		return 1;
	}
	close(fd);
	char *text = kb_read_whole(path, NULL);
	if (!text)
		return 0;
	for (char *l = text, *nl; l && *l; l = nl) {
		nl = strchr(l, '\n');
		if (nl)
			*nl++ = 0;
		fw_klog_line(m, root, l);
	}
	free(text);
	return 1;
}

/* ── the devices and their modules ─────────────────────────────────────── */

static FwAlias *g_alias;
static int g_nalias;

static void fw_alias_add(const char *pat, const char *mod)
{
	static int cap;
	if (g_nalias == cap) {
		cap = cap ? cap * 2 : 8192;
		g_alias = kb_realloc(g_alias, (size_t)cap * sizeof(*g_alias));
	}
	FwAlias *a = &g_alias[g_nalias++];
	a->pat = kb_strdup(pat);
	a->mod = kb_strdup(mod);
	a->lit = strcspn(pat, "*?[");
}

/* modules.alias, and the built-in drivers' `<mod>.alias=` entries, which
 * modules.alias does not carry. 0 when neither could be read. */
static int fw_aliases(const char *kdir)
{
	char p[800];
	int got = 0;

	snprintf(p, sizeof(p), "%s/modules.alias", kdir);
	char *t = kb_read_whole(p, NULL);
	if (t) {
		got = 1;
		for (char *l = t, *nl; l && *l; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = 0;
			char pat[512], mod[64];
			if (sscanf(l, "alias %511s %63s", pat, mod) == 2)
				fw_alias_add(pat, mod);
		}
		free(t);
	}
	size_t len = 0;
	snprintf(p, sizeof(p), "%s/modules.builtin.modinfo", kdir);
	t = kb_read_all(p, &len);
	if (t) {
		got = 1;
		for (size_t o = 0; o < len; o += strlen(t + o) + 1) {
			char *e = t + o, *dot = strchr(e, '.');
			if (!dot || strncmp(dot, ".alias=", 7) ||
			    (size_t)(dot - e) >= 64)
				continue;
			char mod[64];
			memcpy(mod, e, (size_t)(dot - e));
			mod[dot - e] = 0;
			fw_alias_add(dot + 7, mod);
		}
		free(t);
	}
	return got;
}

static FwMod *g_mods;
static int g_nmods;

/* Module names are compared with '-' and '_' as one, as the kernel does. */
static int fw_mod_eq(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if (*a != *b && !((*a == '-' || *a == '_') &&
				  (*b == '-' || *b == '_')))
			return 0;
	return *a == *b;
}

static void fw_mod_add(const char *mod, const char *dev)
{
	for (int i = 0; i < g_nmods; i++)
		if (fw_mod_eq(g_mods[i].mod, mod))
			return;
	g_mods = kb_realloc(g_mods, (size_t)(g_nmods + 1) * sizeof(*g_mods));
	kb_strlcpy(g_mods[g_nmods].mod, mod, sizeof(g_mods[0].mod));
	kb_strlcpy(g_mods[g_nmods].dev, dev, sizeof(g_mods[0].dev));
	g_nmods++;
}

static void fw_devices(void)
{
	char bdir[512];
	snprintf(bdir, sizeof(bdir), "%s/sys/bus", fw_sysroot());
	char **buses = kb_listdir(bdir, NULL);

	for (char **b = buses; b && *b; b++) {
		char ddir[1024];
		snprintf(ddir, sizeof(ddir), "%s/%s/devices", bdir, *b);
		char **devs = kb_listdir(ddir, NULL);
		for (char **d = devs; d && *d; d++) {
			char p[1400], alias[512], label[160];
			snprintf(label, sizeof(label), "%s %s", *b, *d);
			snprintf(p, sizeof(p), "%s/%s/driver/module", ddir, *d);
			char link[512];
			ssize_t n = readlink(p, link, sizeof(link) - 1);
			if (n > 0) {
				link[n] = 0;
				fw_mod_add(kb_basename(link), label);
			}
			snprintf(p, sizeof(p), "%s/%s/modalias", ddir, *d);
			if (kb_read_line_file(p, alias, sizeof(alias)) <= 0)
				continue;
			for (int i = 0; i < g_nalias; i++) {
				const FwAlias *a = &g_alias[i];
				if (strncmp(a->pat, alias, a->lit) ||
				    fnmatch(a->pat, alias, 0))
					continue;
				fw_mod_add(a->mod, label);
			}
		}
		kb_strv_free(devs);
	}
	kb_strv_free(buses);
}

/* Each module's firmware list, through modinfo. 1 when modinfo ran. */
static int fw_modules(const FwMap *m, const char *root, const char *kver)
{
	const char *prog = kb_have_prog("modinfo") ? "modinfo"
						   : "/usr/sbin/modinfo";
	int ran = 0;

	for (int i = 0; i < g_nmods; i++) {
		KbArgv a = { 0 };
		KbBuf out = { 0 };
		kb_argv_add(&a, prog);
		if (*fw_sysroot()) {
			kb_argv_add(&a, "-b");
			kb_argv_add(&a, fw_sysroot());
		}
		kb_argv_add(&a, "-k");
		kb_argv_add(&a, kver);
		kb_argv_add(&a, "-F");
		kb_argv_add(&a, "firmware");
		kb_argv_add(&a, "--");
		kb_argv_add(&a, g_mods[i].mod);
		kb_argv_end(&a);
		int st = kb_run_capture_buf(&a, &out);
		if (st >= 0 && st < 126)
			ran = 1;
		for (char *l = out.p, *nl; st == 0 && l && *l; l = nl) {
			nl = strchr(l, '\n');
			if (nl)
				*nl++ = 0;
			if (!fw_safe_name(l) || fw_present(root, l))
				continue;
			const char *port = fw_map_port(m, l);
			if (port && !fw_installed(root, port)) {
				char why[512];
				snprintf(why, sizeof(why), "module %s, %s",
					 g_mods[i].mod, g_mods[i].dev);
				fw_need_add(port, why);
			}
		}
		kb_buf_free(&out);
	}
	return ran;
}

/* ── the command ───────────────────────────────────────────────────────── */

static int fw_need_cmp(const void *a, const void *b)
{
	return strcmp(((const FwNeed *)a)->port, ((const FwNeed *)b)->port);
}

static int fw_usage(void)
{
	fprintf(stderr, "usage: kdos firmware sync [--root DIR] "
			"[--dry-run [--json]] [--all]\n");
	return 2;
}

static int fw_sync(int argc, char **argv)
{
	int dry = 0, json = 0, all = 0;
	const char *root = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--dry-run"))
			dry = 1;
		else if (!strcmp(argv[i], "--json"))
			json = 1;
		else if (!strcmp(argv[i], "--all"))
			all = 1;
		else if (!strcmp(argv[i], "--root") && i + 1 < argc &&
			 argv[i + 1][0] == '/')
			root = argv[++i];
		else
			return fw_usage();
	}
	if (json && !dry)
		return fw_usage();

	/* "" is the running root: every path below is root + "/lib/…". */
	char rbuf[1024];
	kb_strlcpy(rbuf, root ? root : "", sizeof(rbuf));
	for (size_t n = strlen(rbuf); n > 0 && rbuf[n - 1] == '/'; n--)
		rbuf[n - 1] = 0;

	/* The map of the root being filled; the running system's when that
	 * root has none yet. */
	FwMap map;
	char mpath[1100];
	snprintf(mpath, sizeof(mpath), "%s%s", rbuf, FW_MAP);
	if (fw_map_load(&map, mpath) != 0 &&
	    (!*rbuf || fw_map_load(&map, FW_MAP) != 0)) {
		fprintf(stderr, "kdos firmware: no %s: install firmware-map, "
				"or name every vendor port with --all from "
				"a system that has it\n", mpath);
		return 3;
	}

	int klog = 0, mods = 0;
	if (all) {
		for (int i = 0; i < map.n; i++)
			if (!fw_installed(rbuf, map.v[i].port))
				fw_need_add(map.v[i].port, "--all");
	} else {
		klog = fw_klog(&map, rbuf);
		struct utsname u;
		const char *kver = getenv("KDOS_FIRMWARE_KVER");
		if ((!kver || !*kver) && uname(&u) == 0)
			kver = u.release;
		char kdir[700];
		snprintf(kdir, sizeof(kdir), "%s/lib/modules/%s", fw_sysroot(),
			 kver ? kver : "");
		if (kver && fw_aliases(kdir)) {
			fw_devices();
			mods = fw_modules(&map, rbuf, kver);
		}
	}
	qsort(g_need, (size_t)g_nneed, sizeof(*g_need), fw_need_cmp);

	int rc = 0;
	if (dry && json) {
		KbBuf b = { 0 };
		kb_buf_str(&b, "{\"ports\":[");
		for (int i = 0; i < g_nneed; i++) {
			kb_buf_str(&b, i ? ",{\"port\":" : "{\"port\":");
			kb_json_str(&b, g_need[i].port);
			kb_buf_str(&b, ",\"reason\":");
			kb_json_str(&b, g_need[i].reason);
			kb_buf_str(&b, "}");
		}
		kb_buf_str(&b, "],\"unshipped\":[");
		for (int i = 0; i < g_nunshipped; i++) {
			if (i)
				kb_buf_str(&b, ",");
			kb_json_str(&b, g_unshipped[i]);
		}
		kb_buf_printf(&b, "],\"kernel_log\":%s,\"modules\":%s}\n",
			      klog ? "true" : "false",
			      mods ? "true" : "false");
		fputs(b.p, stdout);
		kb_buf_free(&b);
		rc = g_nneed ? 1 : 0;
	} else if (dry) {
		for (int i = 0; i < g_nneed; i++)
			printf("%s %s\n", g_need[i].port, g_need[i].reason);
		for (int i = 0; i < g_nunshipped; i++)
			fprintf(stderr, "kdos firmware: the kernel asked for "
					"%s, which no port ships\n",
				g_unshipped[i]);
		if (!all && !klog)
			fprintf(stderr, "kdos firmware: the kernel log was "
					"not readable; the devices alone "
					"decided\n");
		rc = g_nneed ? 1 : 0;
	} else if (geteuid() != 0) {
		fprintf(stderr, "kdos firmware sync installs packages and "
				"needs root; --dry-run says what it would "
				"install\n");
		rc = 1;
	} else if (!g_nneed) {
		printf("every firmware this machine asks for is installed\n");
	} else {
		KbArgv f = { 0 };
		kb_argv_add(&f, "kdos");
		kb_argv_add(&f, "update");
		kb_argv_add(&f, "fetch");
		kb_argv_add(&f, "--binhost-only");
		kb_argv_end(&f);
		if (kb_run_tty(&f) != 0)
			fprintf(stderr, "kdos firmware: the release index "
					"could not be fetched; installing "
					"from the one cached here\n");
		for (int i = 0; i < g_nneed; i++) {
			printf("==> firmware: %s (%s)\n", g_need[i].port,
			       g_need[i].reason);
			fflush(stdout);
			KbArgv a = { 0 };
			kb_argv_add(&a, "kdos");
			kb_argv_add(&a, "update");
			kb_argv_add(&a, "install");
			kb_argv_add(&a, "--prebuilt");
			if (root) {
				kb_argv_add(&a, "--root");
				kb_argv_add(&a, root);
			}
			kb_argv_add(&a, g_need[i].port);
			kb_argv_end(&a);
			if (kb_run_tty(&a) != 0) {
				fprintf(stderr, "kdos firmware: %s was not "
						"installed\n", g_need[i].port);
				rc = 1;
			}
		}
	}

	for (int i = 0; i < g_nunshipped; i++)
		free(g_unshipped[i]);
	free(g_unshipped);
	free(g_need);
	for (int i = 0; i < g_nalias; i++) {
		free(g_alias[i].pat);
		free(g_alias[i].mod);
	}
	free(g_alias);
	free(g_mods);
	fw_map_free(&map);
	return rc;
}

int kdt_firmware_main(int argc, char **argv)
{
	if (argc >= 1 && !strcmp(argv[0], "sync"))
		return fw_sync(argc - 1, argv + 1);
	return fw_usage();
}
