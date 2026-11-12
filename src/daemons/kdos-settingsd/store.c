/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — the files, and what a valid value in each is
 *
 * EVERY SETTING HAS ONE FILE, AND IT IS THE FILE THE REST OF THE MACHINE
 * ALREADY READS. /etc/hostname is what 05_hostname.sh sets at boot,
 * /etc/keymap what kdos-getty loads on every console, /etc/localtime and
 * profile.d/20-timezone.sh what musl and every login shell follow,
 * /etc/locale.conf what SDDM puts in the session's environment. A copy kept
 * anywhere else would be a second answer to one question, and the next boot
 * would believe the other one.
 *
 * NOTHING IS CACHED. Every getter reads its file, so a value the installer
 * or a person with an editor wrote is what the next caller sees.
 *
 * EVERY VALUE IS CHECKED BEFORE IT IS WRITTEN, against a character set and,
 * where the machine carries the list, against the list: a zone against the
 * zone tree, a console keymap against kbd's keymaps, an XKB layout against
 * xkeyboard-config's rules. A name that passes the character rule and names
 * nothing is refused rather than written, because a keymap nobody can load
 * and a layout no compositor can compile both fail silently later, at the
 * login screen, where nobody can type the way out.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "settingsd.h"

#define KS_PATH 4096

/* ── where the tree is ───────────────────────────────────────────────────── */

const char *ks_etc(void)
{
	const char *e = getenv("KDOS_SETTINGSD_ETC");

	return e && *e ? e : "/etc";
}

const char *ks_share(void)
{
	const char *e = getenv("KDOS_SETTINGSD_SHARE");

	return e && *e ? e : "/usr/share";
}

bool ks_test_tree(void)
{
	const char *e = getenv("KDOS_SETTINGSD_ETC");

	return e && *e;
}

const char *ks_sys(void)
{
	const char *e = getenv("KDOS_SETTINGSD_SYS");

	return e && *e ? e : "/sys";
}

static bool join(char *out, size_t n, const char *a, const char *b)
{
	int k = snprintf(out, n, "%s/%s", a, b);

	return k >= 0 && (size_t)k < n;
}

bool ks_etc_path(char *out, size_t n, const char *rel)
{
	return join(out, n, ks_etc(), rel);
}

bool ks_share_path(char *out, size_t n, const char *rel)
{
	return join(out, n, ks_share(), rel);
}

bool ks_sys_path(char *out, size_t n, const char *rel)
{
	return join(out, n, ks_sys(), rel);
}

/* ── small character rules ───────────────────────────────────────────────
 *
 * ASCII by hand rather than <ctype.h>: the locale this daemon runs in is
 * whatever rcS had, and a hostname rule that widened with LC_CTYPE would
 * accept a letter `sethostname` passes through and DNS does not.
 */

static bool is_alnum(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9');
}

static bool in_set(const char *s, const char *extra, size_t max)
{
	size_t n = strlen(s);

	if (n > max)
		return false;
	for (size_t i = 0; i < n; i++)
		if (!is_alnum(s[i]) && !strchr(extra, s[i]))
			return false;
	return true;
}

static void trim(char *s)
{
	size_t n = strlen(s), a = 0;

	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
		s[--n] = 0;
	while (s[a] == ' ' || s[a] == '\t')
		a++;
	if (a)
		memmove(s, s + a, n - a + 1);
}

/* ── environment-style files ───────────────────────────────────────────── */

static void env_push(KsEnv *e, char *line)
{
	if (e->n == e->cap) {
		e->cap = e->cap ? e->cap * 2 : 16;
		e->line = kb_realloc(e->line, (size_t)e->cap * sizeof(*e->line));
	}
	e->line[e->n++] = line;
}

int ks_env_load(KsEnv *e, const char *path)
{
	char *buf;

	memset(e, 0, sizeof(*e));
	buf = kb_read_whole(path, NULL);
	if (!buf)
		return errno == ENOENT ? 0 : -1;
	for (char *p = buf; *p;) {
		char *nl = strchr(p, '\n');
		size_t k = nl ? (size_t)(nl - p) : strlen(p);
		char *l = kb_calloc(1, k + 1);

		memcpy(l, p, k);
		env_push(e, l);
		if (!nl)
			break;
		p = nl + 1;
	}
	free(buf);
	return 0;
}

void ks_env_free(KsEnv *e)
{
	for (int i = 0; i < e->n; i++)
		free(e->line[i]);
	free(e->line);
	memset(e, 0, sizeof(*e));
}

/* The value after `KEY=` on this line, or NULL when the line assigns some
 * other key or none. An `export ` prefix is a shell habit os-release allows. */
static const char *line_value(const char *l, const char *key)
{
	size_t k = strlen(key);

	while (*l == ' ' || *l == '\t')
		l++;
	if (!strncmp(l, "export ", 7)) {
		l += 7;
		while (*l == ' ' || *l == '\t')
			l++;
	}
	if (strncmp(l, key, k) || l[k] != '=')
		return NULL;
	return l + k + 1;
}

/* Shell quoting, the subset os-release and machine-info use: double quotes
 * with backslash escapes, single quotes taken literally, a bare word ending at
 * white space. A value that does not fit is no value: a prefix of a hostname
 * can itself be a valid hostname, and the wrong one. */
static bool unquote(const char *v, char *out, size_t n)
{
	size_t o = 0;
	char q = 0;

	if (*v == '"' || *v == '\'')
		q = *v++;
	for (; *v; v++) {
		char c = *v;

		if (q && c == q)
			break;
		if (!q && (c == ' ' || c == '\t' || c == '\r'))
			break;
		if (q == '"' && c == '\\' && v[1] && strchr("\\\"$`", v[1]))
			c = *++v;
		if (o + 1 >= n)
			return false;
		out[o++] = c;
	}
	out[o] = 0;
	return true;
}

bool ks_env_get(const KsEnv *e, const char *key, char *out, size_t n)
{
	out[0] = 0;
	for (int i = 0; i < e->n; i++) {
		const char *v = line_value(e->line[i], key);

		if (!v)
			continue;
		if (!unquote(v, out, n)) {
			out[0] = 0;
			return false;
		}
		return true;
	}
	return false;
}

/* Written bare when every character is one no shell treats specially, so a
 * reader that splits at `=` and takes the rest verbatim still reads the
 * value; quoted, with the four characters double quotes leave live escaped,
 * otherwise. */
static char *render(const char *key, const char *v)
{
	KbBuf b = { 0 };
	bool bare = in_set(v, "_.,:@/+%-", (size_t)-1);

	kb_buf_str(&b, key);
	kb_buf_str(&b, "=");
	if (bare) {
		kb_buf_str(&b, v);
		return b.p;
	}
	kb_buf_str(&b, "\"");
	for (const char *c = v; *c; c++) {
		if (strchr("\\\"$`", *c))
			kb_buf_add(&b, "\\", 1);
		kb_buf_add(&b, c, 1);
	}
	kb_buf_str(&b, "\"");
	return b.p;
}

void ks_env_set(KsEnv *e, const char *key, const char *value)
{
	bool done = false;
	int w = 0;

	for (int i = 0; i < e->n; i++) {
		if (!line_value(e->line[i], key)) {
			e->line[w++] = e->line[i];
			continue;
		}
		free(e->line[i]);
		if (value && !done) {
			e->line[w++] = render(key, value);
			done = true;
		}
	}
	e->n = w;
	if (value && !done)
		env_push(e, render(key, value));
}

int ks_env_save(const KsEnv *e, const char *path)
{
	KbBuf b = { 0 };
	int r;

	if (!e->n)
		return unlink(path) == 0 || errno == ENOENT ? 0 : -1;
	for (int i = 0; i < e->n; i++) {
		kb_buf_str(&b, e->line[i]);
		kb_buf_str(&b, "\n");
	}
	r = kb_write_file_atomic(path, b.p);
	kb_buf_free(&b);
	return r;
}

void ks_env_read1(const char *path, const char *key, char *out, size_t n)
{
	KsEnv e;

	out[0] = 0;
	if (ks_env_load(&e, path) != 0)
		return;
	ks_env_get(&e, key, out, n);
	ks_env_free(&e);
}

/* ── values ──────────────────────────────────────────────────────────────── */

/*
 * A HOSTNAME IS WHAT sethostname(2) AND DNS BOTH ACCEPT: at most 64 bytes,
 * dot-separated labels of letters, digits and `-` (RFC 1123), none empty,
 * none longer than 63, none starting or ending with `-`. That is the rule
 * systemd-hostnamed applies and the one plasma-setup checks before it calls,
 * so a name either accepts is taken here, and an `_`, which DHCP servers and
 * resolvers refuse, never reaches /etc/hostname.
 */
bool ks_hostname_ok(const char *s)
{
	size_t n = strlen(s), lab = 0;

	if (!n || n > KS_HOST_MAX)
		return false;
	for (size_t i = 0; i < n; i++) {
		char c = s[i];

		if (c == '.') {
			if (!lab || s[i - 1] == '-')
				return false;
			lab = 0;
			continue;
		}
		if (!is_alnum(c) && c != '-')
			return false;
		if (!lab && c == '-')
			return false;
		if (++lab > 63)
			return false;
	}
	return lab && s[n - 1] != '-';
}

/*
 * FREE TEXT IS UTF-8 WITH NO CONTROL CHARACTER, at most 255 bytes. Strict
 * UTF-8: no overlong form, no surrogate, nothing past U+10FFFF — every one of
 * those is a way to smuggle a byte a later reader treats as a delimiter. A
 * newline is a control character, and in machine-info it would end the
 * assignment and start another.
 */
bool ks_text_ok(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	if (strlen(s) >= KS_TEXT_MAX)
		return false;
	while (*p) {
		unsigned c = *p;
		int more;
		unsigned cp, min;

		if (c < 0x20 || c == 0x7f)
			return false;
		if (c < 0x80) {
			p++;
			continue;
		}
		if ((c & 0xe0) == 0xc0) {
			more = 1, cp = c & 0x1f, min = 0x80;
		} else if ((c & 0xf0) == 0xe0) {
			more = 2, cp = c & 0x0f, min = 0x800;
		} else if ((c & 0xf8) == 0xf0) {
			more = 3, cp = c & 0x07, min = 0x10000;
		} else {
			return false;
		}
		p++;
		for (int i = 0; i < more; i++, p++) {
			if ((*p & 0xc0) != 0x80)
				return false;
			cp = (cp << 6) | (*p & 0x3f);
		}
		if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
			return false;
		/* C1 controls are controls too. */
		if (cp >= 0x80 && cp < 0xa0)
			return false;
	}
	return true;
}

/* An icon name is a file name in an icon theme, without the extension. */
bool ks_icon_ok(const char *s)
{
	return in_set(s, "_.-", 64) && s[0] != '.' && s[0] != '-';
}

static const char *const KS_CHASSIS[] = {
	"desktop", "laptop", "convertible", "server", "tablet", "handset",
	"watch", "embedded", "vm", "container", NULL
};

bool ks_chassis_ok(const char *s)
{
	if (!*s)
		return true;
	for (int i = 0; KS_CHASSIS[i]; i++)
		if (!strcmp(s, KS_CHASSIS[i]))
			return true;
	return false;
}

bool ks_deployment_ok(const char *s)
{
	return in_set(s, "_.:-", 64);
}

/* ── hostname ──────────────────────────────────────────────────────────── */

void ks_static_hostname(char *out, size_t n)
{
	char p[KS_PATH], line[KS_HOST_MAX + 64];

	out[0] = 0;
	if (!ks_etc_path(p, sizeof(p), "hostname") ||
	    kb_read_line_file(p, line, sizeof(line)) < 0)
		return;
	trim(line);
	if (ks_hostname_ok(line))
		kb_strlcpy(out, line, n);
}

int ks_set_static_hostname(const char *name)
{
	char p[KS_PATH], body[KS_HOST_MAX + 2];

	if (!ks_etc_path(p, sizeof(p), "hostname"))
		return -1;
	if (!*name)
		return unlink(p) == 0 || errno == ENOENT ? 0 : -1;
	snprintf(body, sizeof(body), "%s\n", name);
	return kb_write_file_atomic(p, body);
}

/*
 * THE MACHINE'S OWN NAME RESOLVES THROUGH /etc/hosts. musl reads that file and
 * nothing else before the DNS, and there is no nss-myhostname to answer for
 * the kernel's name, so the `127.0.1.1` line has to name it: left naming the
 * old one, everything that resolves its own name — sudo first — waits on the
 * DNS for a name it does not have. One such line, rewritten in place, every
 * other line kept; the installer writes the same line the same way.
 */
static bool hosts_line(const char *l)
{
	return !strncmp(l, "127.0.1.1", 9) && (l[9] == ' ' || l[9] == '\t');
}

int ks_set_hosts_name(const char *name)
{
	char p[KS_PATH], want[KS_HOST_MAX + 16];
	bool seen = false, changed = false;
	KsEnv e;
	int w = 0, r = 0;

	if (!ks_etc_path(p, sizeof(p), "hosts") || ks_env_load(&e, p) != 0)
		return -1;
	snprintf(want, sizeof(want), "127.0.1.1   %s", name);
	if (!e.n) {
		env_push(&e, kb_strdup("127.0.0.1   localhost"));
		env_push(&e, kb_strdup("::1         localhost ip6-localhost ip6-loopback"));
	}
	for (int i = 0; i < e.n; i++) {
		char *l = e.line[i];

		if (!hosts_line(l)) {
			e.line[w++] = l;
			continue;
		}
		if (seen) {
			free(l);
			changed = true;
			continue;
		}
		seen = true;
		if (strcmp(l, want)) {
			free(l);
			l = kb_strdup(want);
			changed = true;
		}
		e.line[w++] = l;
	}
	e.n = w;
	if (!seen) {
		env_push(&e, kb_strdup(want));
		changed = true;
	}
	if (changed)
		r = ks_env_save(&e, p);
	ks_env_free(&e);
	return r;
}

/*
 * os-release under /etc first, the way the specification orders it; the copy
 * under /usr/lib only for the real tree, so a fixture's answer never depends
 * on the machine it runs on.
 */
void ks_os_release(const char *key, char *out, size_t n)
{
	char p[KS_PATH];

	out[0] = 0;
	if (ks_etc_path(p, sizeof(p), "os-release") && kb_path_exists(p)) {
		ks_env_read1(p, key, out, n);
		return;
	}
	if (!ks_test_tree())
		ks_env_read1("/usr/lib/os-release", key, out, n);
}

/*
 * THE NAME A MACHINE HAS WHEN NOBODY HAS NAMED IT: `DEFAULT_HOSTNAME=` from
 * os-release, else `kdos`, which is the name the image ships in /etc/hostname.
 * plasma-setup compares the current name with this one to decide whether to
 * ask for a name, so it has to be the image's own and not `localhost`.
 */
void ks_default_hostname(char *out, size_t n)
{
	char v[KS_HOST_MAX + 1];

	ks_os_release("DEFAULT_HOSTNAME", v, sizeof(v));
	kb_strlcpy(out, ks_hostname_ok(v) ? v : "kdos", n);
}

/* ── the firmware ─────────────────────────────────────────────────────────
 *
 * DMI's identity files, through ks_sys() so the fixture supplies its own.
 * Firmware pads these with spaces and fills the ones a vendor left unset
 * with a placeholder; a placeholder is no vendor, no model and no serial,
 * and reads as "" rather than being shown as one.
 */
static const char *const KS_DMI_UNSET[] = {
	"To be filled by O.E.M.", "Default string", "System manufacturer",
	"System Product Name", "System Version", "System Serial Number",
	"Not Specified", "Not Applicable", "None", "O.E.M.", "OEM",
	"Chassis Asset Tag", "No Asset Tag", "Asset Tag", "Invalid", NULL
};

void ks_dmi(const char *file, char *out, size_t n)
{
	char rel[128], p[KS_PATH];

	out[0] = 0;
	snprintf(rel, sizeof(rel), "class/dmi/id/%s", file);
	if (!ks_sys_path(p, sizeof(p), rel) ||
	    kb_read_line_file(p, out, n) < 0) {
		out[0] = 0;
		return;
	}
	trim(out);
	for (int i = 0; KS_DMI_UNSET[i]; i++)
		if (!strcasecmp(out, KS_DMI_UNSET[i]))
			out[0] = 0;
	if (!ks_text_ok(out))
		out[0] = 0;
}

/*
 * `MM/DD/YYYY` (DMI's bios_date) or `YYYY-MM-DD` (os-release's SUPPORT_END)
 * as microseconds at midnight UTC on that day. Anything else, or a day that
 * is not on the calendar, is no date: a caller reports UINT64_MAX, systemd's
 * "unknown", rather than a misread day.
 */
bool ks_parse_date(const char *s, uint64_t *usec)
{
	static const int DAYS[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	struct tm tm = { 0 };
	int y, m, d, k = 0;
	time_t t;

	if (sscanf(s, "%2d/%2d/%4d%n", &m, &d, &y, &k) != 3 || s[k]) {
		k = 0;
		if (sscanf(s, "%4d-%2d-%2d%n", &y, &m, &d, &k) != 3 || s[k])
			return false;
	}
	if (y < 1970 || y > 9999 || m < 1 || m > 12 || d < 1 ||
	    d > DAYS[m - 1] ||
	    (m == 2 && d == 29 && (y % 4 || (y % 100 == 0 && y % 400))))
		return false;
	tm.tm_year = y - 1900;
	tm.tm_mon = m - 1;
	tm.tm_mday = d;
	t = timegm(&tm);
	if (t == (time_t)-1)
		return false;
	*usec = (uint64_t)t * 1000000ULL;
	return true;
}

/*
 * A VIRTUAL MACHINE REPORTS A DESKTOP CHASSIS while being none, so it is
 * asked about first. An x86 CPU answers for itself: every hypervisor sets
 * the `hypervisor` flag, and a bare-metal cloud machine whose firmware names
 * the cloud does not, so where /proc/cpuinfo has a `flags` line it is the
 * whole answer. Elsewhere the firmware is asked: a hypervisor node in sysfs
 * (Xen on ARM), or a DMI vendor and product only a hypervisor reports —
 * Microsoft and Google make laptops too, so each counts only beside its
 * virtual product's name. Xen's control domain runs under the hypervisor
 * and is the machine itself, as systemd counts it. /proc is the host's own,
 * so a fixture that replaced /sys is never asked about the host's CPU.
 */
static bool xen_dom0(void)
{
	char v[128];

	return !getenv("KDOS_SETTINGSD_SYS") &&
	       kb_read_line_file("/proc/xen/capabilities", v, sizeof(v)) > 0 &&
	       strstr(v, "control_d") != NULL;
}

static bool in_vm(void)
{
	static const struct {
		const char *vendor, *product;
	} VM_DMI[] = {
		{ "QEMU", NULL }, { "Bochs", NULL }, { "innotek", NULL },
		{ "VMware", NULL }, { "Parallels", NULL }, { "Xen", NULL },
		{ "Microsoft", "Virtual Machine" },
		{ "Google", "Google Compute Engine" },
		{ NULL, "KVM" }, { NULL, "VirtualBox" },
	};
	char ven[128], prod[128], line[8192], p[KS_PATH];
	bool flags = false, hv = false;
	FILE *f;

	if (!getenv("KDOS_SETTINGSD_SYS") && (f = fopen("/proc/cpuinfo", "re"))) {
		while (!flags && fgets(line, sizeof(line), f)) {
			if (strncmp(line, "flags", 5))
				continue;
			flags = true;
			hv = strstr(line, " hypervisor") != NULL;
		}
		fclose(f);
		if (flags)
			return hv && !xen_dom0();
	}
	if (ks_sys_path(p, sizeof(p), "hypervisor/type") &&
	    kb_read_line_file(p, ven, sizeof(ven)) > 0 && ven[0])
		return !xen_dom0();
	ks_dmi("sys_vendor", ven, sizeof(ven));
	ks_dmi("product_name", prod, sizeof(prod));
	/* EC2 names its bare-metal instances `<type>.metal`. */
	if (!strcmp(ven, "Amazon EC2")) {
		size_t k = strlen(prod);

		return k < 6 || strcmp(prod + k - 6, ".metal");
	}
	for (size_t i = 0; i < sizeof(VM_DMI) / sizeof(VM_DMI[0]); i++)
		if ((!VM_DMI[i].vendor ||
		     !strncmp(ven, VM_DMI[i].vendor, strlen(VM_DMI[i].vendor))) &&
		    (!VM_DMI[i].product || !strcmp(prod, VM_DMI[i].product)))
			return true;
	return false;
}

/*
 * The ACPI power-management profile, the chassis's second witness where DMI
 * carries a type with no row or no DMI at all; then a device tree's own
 * `chassis-type`, which names the form factor in systemd's words already.
 */
static const char *acpi_chassis(void)
{
	char p[KS_PATH], v[32];

	if (!ks_sys_path(p, sizeof(p), "firmware/acpi/pm_profile") ||
	    kb_read_line_file(p, v, sizeof(v)) <= 0)
		return "";
	switch (strtol(v, NULL, 10)) {
	case 1: case 3: case 6:
		return "desktop";
	case 2:
		return "laptop";
	case 4: case 5: case 7:
		return "server";
	case 8:
		return "tablet";
	default:
		return "";
	}
}

static void dt_chassis(char *out, size_t n)
{
	char p[KS_PATH];

	out[0] = 0;
	if (!ks_sys_path(p, sizeof(p), "firmware/devicetree/base/chassis-type") ||
	    kb_read_line_file(p, out, n) <= 0 || !ks_chassis_ok(out) ||
	    !strcmp(out, "vm") || !strcmp(out, "container"))
		out[0] = 0;
}

/*
 * The firmware's SMBIOS chassis type, mapped the way systemd-hostnamed maps
 * it. A type with no row is no answer, and the caller then shows a plain
 * `computer` icon rather than a guessed form factor.
 */
static const char *dmi_chassis(void)
{
	char p[KS_PATH], v[32];
	long t;

	if (!ks_sys_path(p, sizeof(p), "class/dmi/id/chassis_type") ||
	    kb_read_line_file(p, v, sizeof(v)) <= 0)
		return "";
	t = strtol(v, NULL, 10);
	switch (t) {
	case 0x03: case 0x04: case 0x06: case 0x07: case 0x0d:
	case 0x23: case 0x24:
		return "desktop";
	case 0x08: case 0x09: case 0x0a: case 0x0e:
		return "laptop";
	case 0x0b:
		return "handset";
	case 0x11: case 0x1c: case 0x1d:
		return "server";
	case 0x1e:
		return "tablet";
	case 0x1f: case 0x20:
		return "convertible";
	case 0x21: case 0x22:
		return "embedded";
	default:
		return "";
	}
}

/* systemd-hostnamed's order: what machine-info says, then a virtual machine,
 * then DMI, ACPI and the device tree, the first one with an answer. */
void ks_chassis(char *out, size_t n)
{
	char p[KS_PATH], v[32];
	const char *c;

	if (ks_etc_path(p, sizeof(p), "machine-info")) {
		ks_env_read1(p, "CHASSIS", v, sizeof(v));
		if (*v && ks_chassis_ok(v)) {
			kb_strlcpy(out, v, n);
			return;
		}
	}
	if (in_vm()) {
		kb_strlcpy(out, "vm", n);
		return;
	}
	c = dmi_chassis();
	if (!*c)
		c = acpi_chassis();
	if (*c) {
		kb_strlcpy(out, c, n);
		return;
	}
	dt_chassis(v, sizeof(v));
	kb_strlcpy(out, v, n);
}

/*
 * /etc/machine-id as the 16 bytes it spells: 32 lowercase hex digits and a
 * newline (40_dbus.sh links it to dbus's copy). Anything else is no id, and
 * MachineID is then an empty array rather than sixteen guessed bytes.
 */
static bool hex_id(const char *s, uint8_t out[16])
{
	int k = 0;

	for (const char *c = s; *c && k < 32; c++) {
		int v;

		if (*c == '-')
			continue;
		if (*c >= '0' && *c <= '9')
			v = *c - '0';
		else if (*c >= 'a' && *c <= 'f')
			v = *c - 'a' + 10;
		else if (*c >= 'A' && *c <= 'F')
			v = *c - 'A' + 10;
		else
			return false;
		if (k % 2 == 0)
			out[k / 2] = (uint8_t)(v << 4);
		else
			out[k / 2] |= (uint8_t)v;
		k++;
	}
	return k == 32;
}

bool ks_machine_id(uint8_t out[16])
{
	char p[KS_PATH], line[64];

	if (!ks_etc_path(p, sizeof(p), "machine-id") ||
	    kb_read_line_file(p, line, sizeof(line)) < 0)
		return false;
	trim(line);
	return strlen(line) == 32 && !strchr(line, '-') && hex_id(line, out);
}

/* A UUID in its dashed form (the boot id, DMI's product_uuid). */
bool ks_uuid_parse(const char *s, uint8_t out[16])
{
	return strlen(s) == 36 && s[8] == '-' && s[13] == '-' && s[18] == '-' &&
	       s[23] == '-' && hex_id(s, out);
}

/* ── time zone ───────────────────────────────────────────────────────────
 *
 * A ZONE NAME IS CHECKED BY CHARACTER BEFORE IT IS LOOKED UP. A zone is
 * `Area/City` or `Area/Sub/City`, so a slash is legal — which makes
 * `../../etc/shadow` legal-looking too, and this rule is what stops it:
 * letters, digits, `+`, `-`, `_` and `/`, no dot anywhere, no leading,
 * trailing or doubled slash. Only then is the zone tree asked.
 */
static bool zone_name_ok(const char *z)
{
	size_t n = strlen(z);

	if (!n || n >= KS_NAME_MAX || z[0] == '/' || z[n - 1] == '/')
		return false;
	for (size_t i = 0; i < n; i++) {
		if (z[i] == '/' && z[i + 1] == '/')
			return false;
		if (!is_alnum(z[i]) && !strchr("+-_/", z[i]))
			return false;
	}
	return true;
}

static bool zone_file(char *out, size_t n, const char *zone)
{
	int k = snprintf(out, n, "%s/zoneinfo/%s", ks_share(), zone);

	return k >= 0 && (size_t)k < n;
}

void ks_timezone(char *out, size_t n)
{
	char link[KS_PATH], target[KS_PATH], pre[KS_PATH];
	static const char *const KNOWN[] = {
		"/usr/share/zoneinfo/", "../usr/share/zoneinfo/", NULL
	};
	const char *z = NULL;
	ssize_t k;

	out[0] = 0;
	if (!ks_etc_path(link, sizeof(link), "localtime"))
		return;
	k = readlink(link, target, sizeof(target) - 1);
	if (k < 0) {
		/* No link at all is UTC, as it is to musl and glibc. A regular
		 * file is a zone nobody can name. */
		if (errno == ENOENT)
			kb_strlcpy(out, "UTC", n);
		return;
	}
	target[k] = 0;
	if (ks_share_path(pre, sizeof(pre), "zoneinfo/") &&
	    !strncmp(target, pre, strlen(pre)))
		z = target + strlen(pre);
	for (int i = 0; !z && KNOWN[i]; i++)
		if (!strncmp(target, KNOWN[i], strlen(KNOWN[i])))
			z = target + strlen(KNOWN[i]);
	if (z && zone_name_ok(z))
		kb_strlcpy(out, z, n);
}

/* A zone file is a regular file that starts with the TZif magic: a directory
 * of the tree (`Europe`) and a table beside the zones (`zone.tab`) are both
 * names that exist and are not zones. */
int ks_zone_check(const char *zone)
{
	char p[KS_PATH], magic[4];
	struct stat st;
	int fd;
	ssize_t r;

	if (!zone_name_ok(zone))
		return -1;
	if (!zone_file(p, sizeof(p), zone))
		return -2;
	fd = open(p, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -2;
	r = fstat(fd, &st) == 0 && S_ISREG(st.st_mode)
		? read(fd, magic, sizeof(magic)) : -1;
	close(fd);
	return r == 4 && !memcmp(magic, "TZif", 4) ? 0 : -2;
}

/*
 * THE WI-FI COUNTRY FOLLOWS THE ZONE. cfg80211 starts in the world
 * regulatory domain until something names a country, and the world rules
 * keep every 5 GHz DFS channel closed and cap transmit power — on the drivers
 * that never take a country from an access point's beacons, for good.
 * `zone.tab` maps each zone to exactly one country, so the zone somebody has
 * just chosen is the best answer this machine has. A zone with no country
 * (UTC, `Etc/…`) removes the file rather than leaving an old country behind.
 * The installer writes the same file.
 */
static bool zone_country(const char *zone, char cc[3])
{
	char p[KS_PATH], line[512];
	bool found = false;
	FILE *f;

	if (!ks_share_path(p, sizeof(p), "zoneinfo/zone.tab"))
		return false;
	f = fopen(p, "re");
	if (!f)
		return false;
	while (!found && fgets(line, sizeof(line), f)) {
		char c[3], z[KS_NAME_MAX];

		/* `CC<TAB>coordinates<TAB>zone[<TAB>comment]` */
		if (line[0] == '#' ||
		    sscanf(line, "%2[A-Z]\t%*[^\t]\t%127[^\t\n]", c, z) != 2 ||
		    strlen(c) != 2 || strcmp(z, zone))
			continue;
		memcpy(cc, c, 3);
		found = true;
	}
	fclose(f);
	return found;
}

static void fsync_dir(const char *dir)
{
	int fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

	if (fd >= 0) {
		fsync(fd);
		close(fd);
	}
}

/*
 * BOTH HALVES ARE WRITTEN OR THE CALL FAILS. `/etc/localtime` is what a
 * program reading the zoneinfo tree follows; `TZ` in the profile is what musl
 * reads where it is set, and every login sets it once an installer or this
 * daemon has chosen a zone — so writing only the link leaves every new login
 * with the TZ an installer answer file or an older zone wrote, and `date`
 * reporting that zone, which reads as the setting having done nothing.
 *
 * THE COLON FORM POINTS MUSL AT THE LINK ITSELF, the one value that cannot
 * drift from it, and it is the form Qt reads as "follow /etc/localtime"
 * rather than as a zone of that name. It is also what lets a running program
 * follow the change: KDOS's musl re-reads the file a path-form TZ names on
 * the next tzset() once the link is replaced, which a zone name such as
 * Europe/Berlin never does. The link is swapped by rename(), so tzset()
 * never finds it missing.
 */
int ks_set_timezone(const char *zone, char cc[3])
{
	char zi[KS_PATH], link[KS_PATH], tmp[KS_PATH], dir[KS_PATH];
	char prof[KS_PATH], reg[KS_PATH], body[512];

	cc[0] = 0;
	if (!zone_file(zi, sizeof(zi), zone) ||
	    !ks_etc_path(link, sizeof(link), "localtime") ||
	    !ks_etc_path(tmp, sizeof(tmp), "localtime.kdos-settingsd") ||
	    !ks_etc_path(dir, sizeof(dir), "profile.d") ||
	    !ks_etc_path(prof, sizeof(prof), "profile.d/20-timezone.sh") ||
	    !ks_etc_path(reg, sizeof(reg), "modprobe.d/kdos-regdom.conf"))
		return -1;

	unlink(tmp);
	if (symlink(zi, tmp) != 0 || rename(tmp, link) != 0) {
		unlink(tmp);
		return -1;
	}
	fsync_dir(ks_etc());

	kb_mkdir_p(dir);
	if (kb_write_file_atomic(prof,
		"# Written by kdos-settingsd (org.freedesktop.timedate1).\n"
		"# musl reads TZ before /etc/localtime; the colon form names\n"
		"# the link, so both say the same zone, and a running program\n"
		"# follows the link when it is replaced. A zone name here\n"
		"# would hold every program on that zone.\n"
		"export TZ=':/etc/localtime'\n") != 0)
		return -1;

	if (!zone_country(zone, cc)) {
		unlink(reg);
		return 0;
	}
	if (ks_etc_path(dir, sizeof(dir), "modprobe.d"))
		kb_mkdir_p(dir);
	snprintf(body, sizeof(body),
		 "# Written by kdos-settingsd from the timezone (%s): the country\n"
		 "# the Wi-Fi radio's channels and transmit power are set for.\n"
		 "options cfg80211 ieee80211_regdom=%s\n", zone, cc);
	/* The regulatory domain is the radio's, and a machine with no radio
	 * still has a time zone: failing here does not fail the zone. */
	kb_write_file_atomic(reg, body);
	return 0;
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static void strv_push(char ***v, int *n, int *cap, const char *s)
{
	if (*n + 1 >= *cap) {
		*cap = *cap ? *cap * 2 : 512;
		*v = kb_realloc(*v, (size_t)*cap * sizeof(**v));
	}
	(*v)[(*n)++] = kb_strdup(s);
	(*v)[*n] = NULL;
}

/*
 * EVERY NAME tzdata.zi CARRIES: its `Z name …` zones and the `L target name`
 * links, which are the old names (`US/Eastern`) a saved setting may still
 * use. zone1970.tab is the fallback where tzdata.zi is absent; UTC is always
 * there, because it is in neither table as a row of its own.
 */
char **ks_list_timezones(int *count)
{
	char p[KS_PATH], line[1024];
	char **v = NULL;
	int n = 0, cap = 0, w = 0;
	FILE *f = NULL;
	bool zi = false;

	if (ks_share_path(p, sizeof(p), "zoneinfo/tzdata.zi") &&
	    (f = fopen(p, "re")))
		zi = true;
	else if (ks_share_path(p, sizeof(p), "zoneinfo/zone1970.tab"))
		f = fopen(p, "re");
	while (f && fgets(line, sizeof(line), f)) {
		char a[KS_NAME_MAX], b[KS_NAME_MAX];
		const char *name = NULL;

		if (zi) {
			if ((line[0] == 'Z' || line[0] == 'z') &&
			    sscanf(line + 1, " %127s", a) == 1)
				name = a;
			else if ((line[0] == 'L' || line[0] == 'l') &&
				 sscanf(line + 1, " %127s %127s", a, b) == 2)
				name = b;
		} else if (line[0] != '#' &&
			   sscanf(line, "%*s %*s %127s", a) == 1) {
			name = a;
		}
		if (name && zone_name_ok(name))
			strv_push(&v, &n, &cap, name);
	}
	if (f)
		fclose(f);
	strv_push(&v, &n, &cap, "UTC");
	qsort(v, (size_t)n, sizeof(*v), cmp_str);
	for (int i = 0; i < n; i++) {
		if (w && !strcmp(v[w - 1], v[i])) {
			free(v[i]);
			continue;
		}
		v[w++] = v[i];
	}
	v[w] = NULL;
	*count = w;
	return v;
}

/* ── network time ─────────────────────────────────────────────────────────
 *
 * NTP IS CHRONY UNDER ksvc, and "enabled" is what the next boot does: the
 * service runs unless `/etc/service.disabled/chrony` exists, the marker rcS
 * reads and `service disable chrony` writes. The marker is this daemon's to
 * write as well, the way kdos-powerd writes sshd's, so the setting and the
 * boot can never read two different places. The script is looked for by its
 * name and not its number, because the number is the boot order and may
 * move.
 */
static bool chrony_script(void)
{
	char dir[KS_PATH], p[KS_PATH];
	char **v;
	bool found = false;

	if (!ks_etc_path(dir, sizeof(dir), "init.d"))
		return false;
	v = kb_listdir(dir, NULL);
	for (char **f = v; f && *f && !found; f++) {
		const char *s = *f;

		if (strlen(s) != 12 || s[0] < '0' || s[0] > '9' ||
		    s[1] < '0' || s[1] > '9' || strcmp(s + 2, "_chrony.sh"))
			continue;
		found = join(p, sizeof(p), dir, s) && access(p, X_OK) == 0;
	}
	kb_strv_free(v);
	return found;
}

bool ks_ntp_available(void)
{
	return chrony_script() && kb_have_prog("chronyd");
}

static bool ntp_marker(char *out, size_t n)
{
	return ks_etc_path(out, n, "service.disabled/chrony");
}

bool ks_ntp_enabled(void)
{
	char p[KS_PATH];

	return ks_ntp_available() && ntp_marker(p, sizeof(p)) &&
	       !kb_path_exists(p);
}

int ks_set_ntp_marker(bool enabled)
{
	char p[KS_PATH], dir[KS_PATH];

	if (!ntp_marker(p, sizeof(p)) ||
	    !ks_etc_path(dir, sizeof(dir), "service.disabled"))
		return -1;
	if (enabled)
		return unlink(p) == 0 || errno == ENOENT ? 0 : -1;
	kb_mkdir_p(dir);
	return kb_write_file(p, "");
}

/* ── locale ──────────────────────────────────────────────────────────────
 *
 * /etc/locale.conf IS READ BY SDDM, which splits each line at every `=` and
 * puts the text between the first and the second, verbatim, into the
 * environment of the greeter and of every session it starts. So the file
 * holds `NAME=value` lines and nothing else: a quoted value would reach the
 * session with its quotes, a value holding an `=` would be cut there, and a
 * comment carrying one would become a variable with a name like `# …`. The
 * values are held to a character set that needs no quoting and has no `=`.
 */
static const char *const KS_LOCALE[KS_LOCALE_KEYS] = {
	"LANG", "LANGUAGE", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
	"LC_MONETARY", "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS",
	"LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION"
};

const char *ks_locale_key(int i)
{
	return i >= 0 && i < KS_LOCALE_KEYS ? KS_LOCALE[i] : NULL;
}

/* `en_GB.UTF-8`, `sr_RS@latin`, `C.UTF-8`; LANGUAGE is a colon-separated
 * list of such names. */
static bool locale_value_ok(int key, const char *v)
{
	return *v && *v != '.' && *v != '-' &&
	       in_set(v, key == 1 ? "_.@-:" : "_.@-", KS_NAME_MAX - 1);
}

bool ks_locale_assign_ok(const char *a, int *key, bool *empty)
{
	const char *eq = strchr(a, '=');

	if (!eq)
		return false;
	for (int i = 0; i < KS_LOCALE_KEYS; i++) {
		size_t k = strlen(KS_LOCALE[i]);

		if ((size_t)(eq - a) != k || strncmp(a, KS_LOCALE[i], k))
			continue;
		*key = i;
		*empty = !eq[1];
		return *empty || locale_value_ok(i, eq + 1);
	}
	return false;
}

static bool locale_path(char *out, size_t n)
{
	return ks_etc_path(out, n, "locale.conf");
}

int ks_locale_get(char out[KS_LOCALE_KEYS][KS_ASSIGN_MAX])
{
	char p[KS_PATH], v[KS_NAME_MAX];
	KsEnv e;
	int n = 0;

	if (!locale_path(p, sizeof(p)) || ks_env_load(&e, p) != 0)
		return 0;
	for (int i = 0; i < KS_LOCALE_KEYS; i++)
		if (ks_env_get(&e, KS_LOCALE[i], v, sizeof(v)) &&
		    locale_value_ok(i, v))
			snprintf(out[n++], KS_ASSIGN_MAX, "%s=%s",
				 KS_LOCALE[i], v);
	ks_env_free(&e);
	return n;
}

int ks_locale_set(char vals[KS_LOCALE_KEYS][KS_NAME_MAX])
{
	char p[KS_PATH];
	KbBuf b = { 0 };
	int r;

	if (!locale_path(p, sizeof(p)))
		return -1;
	for (int i = 0; i < KS_LOCALE_KEYS; i++) {
		if (!vals[i][0])
			continue;
		kb_buf_printf(&b, "%s=%s\n", KS_LOCALE[i], vals[i]);
	}
	if (!b.p)
		return unlink(p) == 0 || errno == ENOENT ? 0 : -1;
	r = kb_write_file_atomic(p, b.p);
	kb_buf_free(&b);
	return r;
}

/* ── console keymaps ──────────────────────────────────────────────────── */

/* kbd's names: `de-latin1-nodeadkeys`, `fr_CH-latin1`, `br-abnt2`. No slash:
 * loadkeys takes a path as readily as a name. */
bool ks_keymap_name_ok(const char *s)
{
	return *s && *s != '.' && *s != '-' && in_set(s, "_.+-", 63);
}

/* `<name>.map`, compressed or not, anywhere in the keymaps tree: kbd sorts
 * them into `i386/qwertz/…` and the name loadkeys takes is the file's. */
static bool keymap_in(const char *dir, const char *name, int depth)
{
	static const char *const EXT[] = {
		".map", ".map.gz", ".map.bz2", ".map.xz", ".map.zst", NULL
	};
	size_t nl = strlen(name);
	struct dirent *d;
	bool found = false;
	DIR *dh;

	if (depth > 4 || !(dh = opendir(dir)))
		return false;
	while (!found && (d = readdir(dh))) {
		char p[KS_PATH];
		struct stat st;

		if (d->d_name[0] == '.')
			continue;
		if (!strncmp(d->d_name, name, nl))
			for (int i = 0; EXT[i] && !found; i++)
				found = !strcmp(d->d_name + nl, EXT[i]);
		if (found || !join(p, sizeof(p), dir, d->d_name) ||
		    stat(p, &st) != 0 || !S_ISDIR(st.st_mode))
			continue;
		found = keymap_in(p, name, depth + 1);
	}
	closedir(dh);
	return found;
}

bool ks_keymap_installed(const char *name)
{
	char p[KS_PATH];

	if (!ks_keymap_name_ok(name))
		return false;
	return (ks_share_path(p, sizeof(p), "keymaps") && keymap_in(p, name, 0)) ||
	       (ks_share_path(p, sizeof(p), "kbd/keymaps") &&
		keymap_in(p, name, 0));
}

void ks_keymap(char *out, size_t n)
{
	char p[KS_PATH], line[KS_NAME_MAX];

	out[0] = 0;
	if (!ks_etc_path(p, sizeof(p), "keymap") ||
	    kb_read_line_file(p, line, sizeof(line)) < 0)
		return;
	trim(line);
	if (ks_keymap_name_ok(line))
		kb_strlcpy(out, line, n);
}

int ks_set_keymap(const char *name)
{
	char p[KS_PATH], body[KS_NAME_MAX + 2];

	if (!ks_etc_path(p, sizeof(p), "keymap"))
		return -1;
	if (!*name)
		return unlink(p) == 0 || errno == ENOENT ? 0 : -1;
	snprintf(body, sizeof(body), "%s\n", name);
	return kb_write_file_atomic(p, body);
}

/* ── XKB ─────────────────────────────────────────────────────────────────
 *
 * /etc/kdos/xkb.conf, never /etc/X11: there is no X server on this system
 * (CLAUDE.md rule 4) and kpkg refuses a payload under etc/X11. Four keys in
 * shell syntax, values from a character set that needs no quoting, so a
 * script may source the file as it is.
 */
#define KS_XKB_FILE "kdos/xkb.conf"
#define KS_XKB_CHARS "_+,:.-"

static void xkb_field(const KsEnv *e, const char *key, char *out)
{
	if (!ks_env_get(e, key, out, KS_XKB_MAX) ||
	    !in_set(out, KS_XKB_CHARS, KS_XKB_MAX - 1))
		out[0] = 0;
}

void ks_xkb_stored(KsXkb *x)
{
	char p[KS_PATH];
	KsEnv e;

	memset(x, 0, sizeof(*x));
	if (!ks_etc_path(p, sizeof(p), KS_XKB_FILE) || ks_env_load(&e, p) != 0)
		return;
	xkb_field(&e, "XKBMODEL", x->model);
	xkb_field(&e, "XKBLAYOUT", x->layout);
	xkb_field(&e, "XKBVARIANT", x->variant);
	xkb_field(&e, "XKBOPTIONS", x->options);
	ks_env_free(&e);
}

/*
 * AN EMPTY LAYOUT FOLLOWS THE CONSOLE. The installer writes the console
 * keymap and nothing else, and the shipped xkb.conf leaves the layout empty,
 * so every installed machine already has a keyboard at the login screen: the
 * console's, through the table. An explicit layout, once set, is kept as
 * set.
 */
void ks_xkb_effective(KsXkb *x)
{
	char km[KS_NAME_MAX];

	ks_xkb_stored(x);
	if (x->layout[0])
		return;
	ks_keymap(km, sizeof(km));
	if (!ks_vt_to_xkb(km, x->layout, sizeof(x->layout), x->variant,
			  sizeof(x->variant))) {
		x->layout[0] = 0;
		x->variant[0] = 0;
	}
}

int ks_set_xkb(const KsXkb *x)
{
	char p[KS_PATH], dir[KS_PATH];
	KsEnv e;
	int r;

	if (!ks_etc_path(p, sizeof(p), KS_XKB_FILE) ||
	    !ks_etc_path(dir, sizeof(dir), "kdos") || ks_env_load(&e, p) != 0)
		return -1;
	ks_env_set(&e, "XKBMODEL", x->model);
	ks_env_set(&e, "XKBLAYOUT", x->layout);
	ks_env_set(&e, "XKBVARIANT", x->variant);
	ks_env_set(&e, "XKBOPTIONS", x->options);
	kb_mkdir_p(dir);
	r = ks_env_save(&e, p);
	ks_env_free(&e);
	return r;
}

/* Is `name` a row of `! <section>` in a rules list? For a variant the row's
 * description starts `<layout>:`, which is how the list says whose it is. */
static bool lst_has(const char *buf, const char *section, const char *name,
		    const char *layout)
{
	size_t nl = strlen(name), sl = strlen(section);
	bool in = false;

	for (const char *l = buf; *l;) {
		const char *e = strchr(l, '\n');
		const char *end = e ? e : l + strlen(l);

		if (*l == '!') {
			const char *s = l + 1;

			while (*s == ' ')
				s++;
			in = (size_t)(end - s) >= sl && !strncmp(s, section, sl) &&
			     (s + sl == end || s[sl] == ' ' || s[sl] == '\r');
		} else if (in) {
			const char *s = l;

			while (*s == ' ' || *s == '\t')
				s++;
			if ((size_t)(end - s) > nl && !strncmp(s, name, nl) &&
			    (s[nl] == ' ' || s[nl] == '\t')) {
				const char *d = s + nl;
				size_t ll = layout ? strlen(layout) : 0;

				while (*d == ' ' || *d == '\t')
					d++;
				if (!layout || (!strncmp(d, layout, ll) && d[ll] == ':'))
					return true;
			}
		}
		if (!e)
			break;
		l = e + 1;
	}
	return false;
}

/* Field `i` of a comma list into out; false past the last. */
static bool nth(const char *list, int i, char *out, size_t n)
{
	const char *s = list;

	for (; i > 0; i--) {
		s = strchr(s, ',');
		if (!s)
			return false;
		s++;
	}
	size_t k = strcspn(s, ",");

	if (k >= n)
		k = n - 1;
	memcpy(out, s, k);
	out[k] = 0;
	return true;
}

static int count_fields(const char *list)
{
	int n = *list ? 1 : 0;

	for (; *list; list++)
		n += *list == ',';
	return n;
}

static bool symbols_dir(char *out, size_t n)
{
	return ks_share_path(out, n, "X11/xkb/symbols") && kb_is_dir(out);
}

static bool symbols_have(const char *layout)
{
	char dir[KS_PATH], p[KS_PATH];

	return !symbols_dir(dir, sizeof(dir)) ||
	       (join(p, sizeof(p), dir, layout) && kb_path_exists(p));
}

/* xkeyboard-config's rules list, evdev's or the base one; NULL when neither
 * is installed. The caller frees it. */
static char *rules_list(void)
{
	char p[KS_PATH];
	char *lst = NULL;

	if (ks_share_path(p, sizeof(p), "X11/xkb/rules/evdev.lst"))
		lst = kb_read_whole(p, NULL);
	if (!lst && ks_share_path(p, sizeof(p), "X11/xkb/rules/base.lst"))
		lst = kb_read_whole(p, NULL);
	return lst;
}

const char *ks_xkb_check(const KsXkb *x)
{
	char f[KS_XKB_MAX], lay[KS_XKB_MAX];
	const char *bad = NULL;
	int nlay = count_fields(x->layout), nvar = count_fields(x->variant);
	char *lst = NULL;

	if (!in_set(x->model, KS_XKB_CHARS, KS_XKB_MAX - 1) ||
	    !in_set(x->layout, KS_XKB_CHARS, KS_XKB_MAX - 1) ||
	    !in_set(x->variant, KS_XKB_CHARS, KS_XKB_MAX - 1) ||
	    !in_set(x->options, KS_XKB_CHARS, KS_XKB_MAX - 1))
		return "a field holds a character no XKB name uses";
	if (!x->layout[0] && x->variant[0])
		return "a variant needs a layout";
	/* XKB has four groups: a fifth layout is silently dropped. */
	if (nlay > 4)
		return "XKB takes at most four layouts";
	if (nvar > nlay)
		return "more variants than layouts";
	for (int i = 0; i < nlay; i++)
		if (nth(x->layout, i, f, sizeof(f)) && !f[0])
			return "an empty layout in the list";

	lst = rules_list();
	if (!lst) {
		/* No rules list: the layouts can still be held to the symbols
		 * xkeyboard-config installed, which is what a compositor
		 * compiles from. */
		for (int i = 0; i < nlay && !bad; i++)
			if (nth(x->layout, i, f, sizeof(f)) && !symbols_have(f))
				bad = "a layout xkeyboard-config does not carry";
		return bad;
	}
	if (x->model[0] && !lst_has(lst, "model", x->model, NULL))
		bad = "a model xkeyboard-config does not list";
	for (int i = 0; i < nlay && !bad; i++)
		if (nth(x->layout, i, f, sizeof(f)) &&
		    !lst_has(lst, "layout", f, NULL))
			bad = "a layout xkeyboard-config does not list";
	for (int i = 0; i < nvar && !bad; i++)
		if (nth(x->variant, i, f, sizeof(f)) && f[0] &&
		    nth(x->layout, i, lay, sizeof(lay)) &&
		    !lst_has(lst, "variant", f, lay))
			bad = "a variant its layout does not have";
	for (int i = 0; x->options[0] && i < count_fields(x->options) && !bad; i++)
		if (nth(x->options, i, f, sizeof(f)) &&
		    (!strchr(f, ':') || !lst_has(lst, "option", f, NULL)))
			bad = "an option xkeyboard-config does not list";
	free(lst);
	return bad;
}

/*
 * THE CONSOLE AND XKB SPELL LAYOUTS DIFFERENTLY. kbd names a map after a
 * language or a country and an encoding (`de-latin1`, `uk`, `sv-latin1`); XKB
 * names a layout after a country code (`de`, `gb`, `se`). Most console names
 * start with their XKB layout, which is the two-letter rule after the table;
 * the rows are the names that rule reads wrongly — to XKB `et` is Amharic and
 * not Estonian, `la` Lao and not Latin American, and `azerty`, `keypad`,
 * `backspace` and `carpalx` would be Azerbaijani, Kenyan, Bosnian and
 * Canadian — and the maps whose XKB variant has a name of its own. A row with
 * no layout is a map that says nothing about one: kbd's include files and a
 * Sun or Apple keyboard model's maps. kbd's whole tree goes through this
 * table in no fixture, so a row is added by measuring the name against
 * `keymaps/` and the layout and variant against rules/evdev.lst. Order
 * matters: a longer name is listed before the shorter one it starts with.
 */
static const struct {
	const char *vt;
	bool prefix;
	const char *layout, *variant;
} KS_VT_XKB[] = {
	{ "applkey",		false, NULL,	NULL },
	{ "backspace",		false, NULL,	NULL },
	{ "ctrl",		false, NULL,	NULL },
	{ "euro",		true,  NULL,	NULL },
	{ "keypad",		false, NULL,	NULL },
	{ "unicode",		false, NULL,	NULL },
	{ "windowkeys",		false, NULL,	NULL },
	{ "pc110",		false, NULL,	NULL },
	{ "carpalx",		true,  NULL,	NULL },
	{ "sun",		true,  NULL,	NULL },
	{ "apple-",		true,  NULL,	NULL },
	{ "defkeymap",		true,  "us",	"" },
	{ "us-acentos",		false, "us",	"intl" },
	{ "us",			false, "us",	"" },
	{ "uk",			true,  "gb",	"" },
	{ "slovene",		false, "si",	"" },
	{ "croat",		false, "hr",	"" },
	{ "la-latin1",		false, "latam",	"" },
	{ "sv-latin1",		false, "se",	"" },
	{ "et-nodeadkeys",	false, "ee",	"nodeadkeys" },
	{ "et",			false, "ee",	"" },
	{ "cf",			false, "ca",	"" },
	{ "sr-latin",		false, "rs",	"latin" },
	{ "sr-cy",		false, "rs",	"" },
	{ "kazakh",		false, "kz",	"" },
	{ "kyrgyz",		false, "kg",	"" },
	{ "ky_alt_sh",		true,  "kg",	"" },
	{ "ttwin",		true,  "ru",	"tt" },
	{ "bashkir",		false, "ru",	"bak" },
	{ "fa",			false, "ir",	"" },
	{ "azerty",		false, "fr",	"" },
	{ "neo",		false, "de",	"neo" },
	{ "mod-dh-iso-uk",	true,  "gb",	"colemak_dh" },
	{ "mod-dh",		true,  "us",	"colemak_dh" },
	{ "en-latin9",		false, "us",	"colemak" },
	{ "dvorak-l",		false, "us",	"dvorak-l" },
	{ "dvorak-r",		false, "us",	"dvorak-r" },
	{ "dvorak-programmer",	false, "us",	"dvp" },
	{ "dvorak-ukp",		false, "gb",	"dvorakukp" },
	{ "dvorak-uk",		false, "gb",	"dvorak" },
	{ "dvorak-ca-fr",	false, "ca",	"fr-dvorak" },
	{ "dvorak-de",		false, "de",	"dvorak" },
	{ "dvorak-es",		false, "es",	"dvorak" },
	{ "dvorak-fr",		false, "fr",	"dvorak" },
	{ "dvorak-la",		false, "latam",	"dvorak" },
	{ "dvorak-no",		false, "no",	"dvorak" },
	{ "dvorak-sv",		true,  "se",	"dvorak" },
	{ "ANSI-dvorak",	false, "us",	"dvorak" },
	{ "dvorak",		true,  "us",	"dvorak" },
	{ "sg",			true,  "ch",	"" },
	{ "de_CH",		true,  "ch",	"" },
	{ "fr_CH",		true,  "ch",	"fr" },
	{ "de-latin1-nodeadkeys", false, "de",	"nodeadkeys" },
	{ "fr-bepo-latin9",	false, "fr",	"bepo_latin9" },
	{ "fr-bepo",		false, "fr",	"bepo" },
	{ "fr-afnor",		false, "fr",	"afnor" },
	{ "fr-latin9",		false, "fr",	"latin9" },
	{ "bg_pho",		true,  "bg",	"phonetic" },
	{ "il-phonetic",	false, "il",	"phonetic" },
	{ "sk-prog-qwerty",	false, "sk",	"qwerty" },
	{ "sk-qwerty",		false, "sk",	"qwerty" },
	{ "ro_std",		false, "ro",	"std" },
	{ "lv-tilde",		false, "lv",	"tilde" },
	{ "tralt",		false, "tr",	"alt" },
	{ "trf",		false, "tr",	"f" },
	{ "tr_f",		true,  "tr",	"f" },
};
#define KS_VT_XKB_N ((int)(sizeof(KS_VT_XKB) / sizeof(KS_VT_XKB[0])))

/* A map for another maker's keyboard is named `<model>-<map>`, and the map
 * after the dash is the layout; a Mac's has an XKB variant of its own. */
static const char *const KS_VT_MODEL[] = { "mac-", "amiga-", "atari-", NULL };

/*
 * A DERIVED LAYOUT IS ONLY GIVEN WHEN xkeyboard-config CARRIES IT. A layout a
 * compositor cannot compile does not fail loudly: KWin logs it and falls back
 * to US, the password field included, and nothing on the screen says why.
 * The layout is held to the rules list's layouts (a symbols file such as
 * `pc` is no layout), else to the symbols where no list is installed; a
 * variant the list does not give that layout is dropped, and the layout
 * kept. Where neither is installed nothing can be verified, and the table's
 * answer stands.
 */
bool ks_vt_to_xkb(const char *keymap, char *layout, size_t ln, char *variant,
		  size_t vn)
{
	const char *l = NULL, *v = "", *name = keymap;
	bool found = false, mac = false;
	char two[3], *lst;

	layout[0] = 0;
	variant[0] = 0;
	for (int i = 0; KS_VT_MODEL[i]; i++) {
		size_t k = strlen(KS_VT_MODEL[i]);

		if (!strncmp(name, KS_VT_MODEL[i], k)) {
			mac = i == 0;
			name += k;
			break;
		}
	}
	if (!name[0])
		return false;
	for (int i = 0; i < KS_VT_XKB_N && !found; i++) {
		size_t k = strlen(KS_VT_XKB[i].vt);

		if (KS_VT_XKB[i].prefix ? !strncmp(name, KS_VT_XKB[i].vt, k)
					: !strcmp(name, KS_VT_XKB[i].vt)) {
			found = true;
			l = KS_VT_XKB[i].layout;
			v = KS_VT_XKB[i].variant;
		}
	}
	if (found && !l)
		return false;
	if (!l) {
		if (strlen(name) < 2 || name[0] < 'a' || name[0] > 'z' ||
		    name[1] < 'a' || name[1] > 'z')
			return false;
		memcpy(two, name, 2);
		two[2] = 0;
		l = two;
	}
	if (mac && !v[0])
		v = "mac";
	lst = rules_list();
	if (lst) {
		found = lst_has(lst, "layout", l, NULL);
		if (found && v[0] && !lst_has(lst, "variant", v, l))
			v = "";
		free(lst);
		if (!found)
			return false;
	} else if (!symbols_have(l)) {
		return false;
	}
	kb_strlcpy(layout, l, ln);
	kb_strlcpy(variant, v, vn);
	return true;
}

/*
 * THE OTHER WAY, FOR `convert`: the console map nearest an XKB layout. The
 * rows are the inverse of the table above, tried for the layout and its
 * variant, then for the layout alone, so a variant the console has no map
 * for (`de(mac)`) still finds the layout's; after them, a map with the
 * layout's own name, then its `-latin1` form, which is how kbd names most of
 * them. Each candidate must be installed, and a layout with no console map at
 * all leaves the console as it was.
 */
static const struct {
	const char *layout, *variant, *vt;
} KS_XKB_VT[] = {
	{ "us",		"dvorak",	"dvorak" },
	{ "us",		"dvorak-l",	"dvorak-l" },
	{ "us",		"dvorak-r",	"dvorak-r" },
	{ "us",		"dvp",		"dvorak-programmer" },
	{ "us",		"intl",		"us-acentos" },
	{ "us",		"colemak",	"en-latin9" },
	{ "us",		"colemak_dh",	"mod-dh-ansi-us" },
	{ "gb",		"dvorak",	"dvorak-uk" },
	{ "gb",		"dvorakukp",	"dvorak-ukp" },
	{ "gb",		"colemak_dh",	"mod-dh-iso-uk" },
	{ "de",		"nodeadkeys",	"de-latin1-nodeadkeys" },
	{ "de",		"neo",		"neo" },
	{ "de",		"dvorak",	"dvorak-de" },
	{ "ch",		"fr",		"fr_CH" },
	{ "fr",		"bepo",		"fr-bepo" },
	{ "fr",		"bepo_latin9",	"fr-bepo-latin9" },
	{ "fr",		"afnor",	"fr-afnor" },
	{ "fr",		"latin9",	"fr-latin9" },
	{ "fr",		"dvorak",	"dvorak-fr" },
	{ "es",		"dvorak",	"dvorak-es" },
	{ "latam",	"dvorak",	"dvorak-la" },
	{ "no",		"dvorak",	"dvorak-no" },
	{ "se",		"dvorak",	"dvorak-sv-a1" },
	{ "ca",		"fr-dvorak",	"dvorak-ca-fr" },
	{ "ee",		"nodeadkeys",	"et-nodeadkeys" },
	{ "rs",		"latin",	"sr-latin" },
	{ "ru",		"bak",		"bashkir" },
	{ "ru",		"tt",		"ttwin_alt-UTF-8" },
	{ "sk",		"qwerty",	"sk-qwerty" },
	{ "bg",		"phonetic",	"bg_pho-utf8" },
	{ "tr",		"f",		"trf" },
	{ "tr",		"alt",		"tralt" },
	{ "us",		"",		"us" },
	{ "gb",		"",		"uk" },
	{ "ch",		"",		"sg" },
	{ "si",		"",		"slovene" },
	{ "hr",		"",		"croat" },
	{ "latam",	"",		"la-latin1" },
	{ "br",		"",		"br-abnt2" },
	{ "se",		"",		"sv-latin1" },
	{ "ee",		"",		"et" },
	{ "ca",		"",		"cf" },
	{ "rs",		"",		"sr-cy" },
	{ "kz",		"",		"kazakh" },
	{ "kg",		"",		"kyrgyz" },
	{ "ir",		"",		"fa" },
	{ "jp",		"",		"jp106" },
	{ "sk",		"",		"sk-qwertz" },
	{ "tr",		"",		"trq" },
	{ "bg",		"",		"bg_bds-utf8" },
	{ "tj",		"",		"tj_alt-UTF8" },
};
#define KS_XKB_VT_N ((int)(sizeof(KS_XKB_VT) / sizeof(KS_XKB_VT[0])))

bool ks_xkb_to_vt(const char *layout, const char *variant, char *keymap,
		  size_t kn)
{
	char l[KS_XKB_MAX], v[KS_XKB_MAX], c[KS_XKB_MAX + 8];

	keymap[0] = 0;
	if (!nth(layout, 0, l, sizeof(l)) || !l[0])
		return false;
	if (!nth(variant, 0, v, sizeof(v)))
		v[0] = 0;
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < KS_XKB_VT_N; i++) {
			if (strcmp(l, KS_XKB_VT[i].layout) ||
			    strcmp(pass ? "" : v, KS_XKB_VT[i].variant) ||
			    !ks_keymap_installed(KS_XKB_VT[i].vt))
				continue;
			kb_strlcpy(keymap, KS_XKB_VT[i].vt, kn);
			return true;
		}
		if (!v[0])
			break;
	}
	if (ks_keymap_installed(l)) {
		kb_strlcpy(keymap, l, kn);
		return true;
	}
	snprintf(c, sizeof(c), "%s-latin1", l);
	if (ks_keymap_installed(c)) {
		kb_strlcpy(keymap, c, kn);
		return true;
	}
	return false;
}
