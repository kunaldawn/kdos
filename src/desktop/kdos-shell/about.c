/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-about — what this machine is
 *
 *   ╔═ About KDOS ═════════════════════════════════════════════════════════╗
 *   ║  Machine  Build  Credits                                             ║
 *   ║                                                                      ║
 *   ║  ██╗  ██╗██████╗  ██████╗ ███████╗     KDOS 0.2                       ║
 *   ║  ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝     kernel   6.12.4 x86_64         ║
 *   ║  …                                     uptime   3h 12m 07s           ║
 *   ║                                                                      ║
 *   ║  memory   4.1 of 15.6 GiB in use                                     ║
 *   ║  ████████▓▓▓▓▓▒▒▒░░░░░░░░···································         ║
 *   ║  █ firefox-esr 1.9G  █ kdos-comp 412M  █ foot 80M  ░ other  · cache  ║
 *   ║                                                                      ║
 *   ║  no systemd · no xorg · musl · toybox · signed                       ║
 *   ╚══════════════════════════════════════════════════════════════════════╝
 *
 * THREE PAGES. Machine is the live one: uptime to the second and a memory
 * map in the manner of "About This Macintosh" — the three applications
 * holding the most, everything else in use, the page cache and what is free.
 * Build is where this image came from. Credits rolls every installed package.
 *
 * EVERY FACT IS READ, NOT FORKED. `/etc/os-release`, `/proc`, the package
 * database, the boot state and the keyrings are files this process can open;
 * spawning `fastfetch` to render them would put a second program's layout,
 * colours and ANSI on a surface that draws in slots, and would leave the
 * About window with no offscreen dump to golden.
 *
 * ONE ROOT, MOVABLE. $KDOS_ABOUT_ROOT prefixes every path read here and is
 * handed to libkproc as its proc/sys root, so a dump pointed at
 * testing/fixtures/about draws a recorded machine and can be goldened.
 *
 * MOTION IS ITS END STATE WHERE NOTHING MOVES. The facts type themselves in
 * on opening, the Konami code degausses the logo, and the credits roll; on a
 * dump, a terminal or with comp.conf's `motion = no`, all three are the
 * settled picture: every fact present, the logo still, the roll at its top.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/utsname.h>

#include "kbase.h"
#include "kproc.h"
#include "ksig.h"
#include "kwl.h"
#include "shell.h"

#define AB_COLS 78
#define AB_ROWS 19
#define AB_FACTS 16
#define AB_VAL 96
#define AB_KEYW 10		/* the key column of a fact row */
#define AB_TOP 3		/* the first row under the page strip */
#define AB_INTRO_MS 400
#define AB_WOBBLE_MS 1200
#define AB_ROLL_MS 600		/* one credits row per this long */
#define AB_APPS 3		/* applications named in the memory map */
#define AB_CREDIT_W 24		/* one credits cell: a name and its version */

enum { PG_MACHINE, PG_BUILD, PG_CREDITS, PG_N };

static const KtuiTab PAGES[PG_N] = {
	{ "Machine", NULL }, { "Build", NULL }, { "Credits", NULL }
};

struct fact {
	const char *key;
	char val[AB_VAL];
};

struct facts {
	struct fact f[AB_FACTS];
	int n;
};

struct app {
	char name[64];
	unsigned long long rss;
};

struct credit {
	char name[48];
	char ver[32];
};

static KtuiKeys keys;
static int page;

static struct facts machine, build;

/* The memory map, refreshed on every tick of the Machine page. */
static KprMem mem;
static int have_mem;
static struct app apps[AB_APPS];
static int napps;

static struct credit *credits;
static int ncredits;
static int roll;		/* the first credits row on screen */
static int rolling;		/* the roll advances on its own */

static char logo[SH_LOGO_LINES][SH_LOGO_BYTES];
static int logo_n, logo_w;

static KtuiAnim intro, wobble;

/* ── reading ───────────────────────────────────────────────────────────── */

/* `$KDOS_ABOUT_ROOT` + `path`, or `path` alone. Every file this surface opens
 * goes through here, or a fixture dump draws half the host. */
static const char *at(const char *path)
{
	static char buf[4][512];
	static int slot;
	const char *root = getenv("KDOS_ABOUT_ROOT");

	slot = (slot + 1) % 4;
	snprintf(buf[slot], sizeof(buf[slot]), "%s%s", root ? root : "",
		 path);
	return buf[slot];
}

static void fact(struct facts *fs, const char *key, const char *fmt, ...)
{
	if (fs->n >= AB_FACTS)
		return;

	struct fact *f = &fs->f[fs->n];
	va_list ap;

	f->key = key;
	va_start(ap, fmt);
	vsnprintf(f->val, sizeof(f->val), fmt, ap);
	va_end(ap);
	if (f->val[0])
		fs->n++;
}

/*
 * One `key="value"` out of a shell-style file, unquoted. os-release is the
 * only caller and its grammar is that small; a parser for the whole of it
 * would be a configuration library for one line.
 */
static int os_release(const char *key, char *out, size_t n)
{
	FILE *f = fopen(at("/etc/os-release"), "r");
	char line[256];
	size_t klen = strlen(key);
	int got = 0;

	out[0] = '\0';
	if (!f)
		return -1;
	while (!got && fgets(line, sizeof(line), f)) {
		if (strncmp(line, key, klen) || line[klen] != '=')
			continue;

		char *v = line + klen + 1;

		v[strcspn(v, "\r\n")] = '\0';
		if (*v == '"') {
			v++;
			v[strcspn(v, "\"")] = '\0';
		}
		snprintf(out, n, "%s", v);
		got = 1;
	}
	fclose(f);
	return got && out[0] ? 0 : -1;
}

/* A one-line /proc file through libkproc's root, newline stripped. */
static int proc_line(const char *leaf, char *out, size_t n)
{
	if (kpr_read_into_proc(out, n, "%s", leaf) < 0) {
		out[0] = '\0';
		return -1;
	}
	out[strcspn(out, "\n")] = '\0';
	return out[0] ? 0 : -1;
}

/* 412M, 1.9G, 15.5G: a decimal for gigabytes and up, where a whole number
 * would round a 16 GiB machine's free memory to nothing in particular. */
static const char *size_of(unsigned long long b, char *buf, size_t n)
{
	double v = (double)b;
	const char *u = "BKMGT";

	while (v >= 1024.0 && u[1]) {
		v /= 1024.0;
		u++;
	}
	if (v < 100.0 && (*u == 'G' || *u == 'T'))
		snprintf(buf, n, "%.1f%c", v, *u);
	else
		snprintf(buf, n, "%.0f%c", v, *u);
	return buf;
}

/* ── the boot slot ─────────────────────────────────────────────────────── */

/*
 * The running slot is the command line's `kdos_slot=`, which is the menu
 * entry's and therefore the kernel that actually booted. Everything else is
 * the state file kdos-bootctl owns, read here and never written: the trial a
 * person needs to know about before they reboot is in it.
 */
static void slot_fact(struct facts *fs)
{
	char cmd[1024], run = 0;

	if (proc_line("cmdline", cmd, sizeof(cmd)) == 0) {
		for (char *w = strtok(cmd, " "); w; w = strtok(NULL, " "))
			if (!strncmp(w, "kdos_slot=", 10) && w[10])
				run = w[10];
	}

	FILE *f = fopen(at("/boot/efi/EFI/kdos/bootstate"), "r");
	char line[256], active = 0, try = 0;
	int attempts = 0, have_b = 0;

	while (f && fgets(line, sizeof(line), f)) {
		char *eq = strchr(line, '=');

		if (line[0] == '#' || !eq)
			continue;

		char *v = eq + 1;

		while (*v == ' ' || *v == '\t')
			v++;
		v[strcspn(v, " \t\r\n")] = '\0';
		if (!strncmp(line, "active", 6))
			active = v[0];
		else if (!strncmp(line, "try", 3))
			try = v[0];
		else if (!strncmp(line, "attempts", 8))
			attempts = atoi(v);
		else if (!strncmp(line, "slot_b", 6))
			have_b = v[0] != '\0';
	}
	if (f)
		fclose(f);

	if (!active) {
		/* No state file: a live medium or a single-root install. */
		if (run)
			fact(fs, "slot", "%c", run);
		else
			fact(fs, "slot", "one root");
	} else if (try) {
		fact(fs, "slot", "%c on trial, %d boot%s left", try, attempts,
		     attempts == 1 ? "" : "s");
		fact(fs, "", "%c known good", active);
	} else if (run && run != active) {
		fact(fs, "slot", "%c, active is %c", run, active);
	} else {
		fact(fs, "slot", "%c known good%s", active,
		     have_b ? (active == 'a' ? ", b standby" : ", a standby")
			    : "");
	}
}

/* ── the memory map ────────────────────────────────────────────────────── */

static int by_rss(const void *a, const void *b)
{
	const struct app *x = a, *y = b;

	return x->rss < y->rss ? 1 : x->rss > y->rss ? -1 : strcmp(x->name,
								   y->name);
}

/*
 * THE SAME GROUPING kdos-res's Apps page draws: a boxed process belongs to
 * its box and a host process to its own comm, and memory is resident set
 * size. Two windows on one desktop disagreeing about what firefox holds is
 * worse than either number. RSS counts a shared page in every process that
 * maps it, so the named segments are clamped to what is in use and "other"
 * is the remainder rather than a second sum.
 */
static void sample_memory(void)
{
	KprSample s;
	struct app *all = NULL;
	int nall = 0, cap = 0;

	have_mem = kpr_mem_read(&mem) == 0 && mem.total > 0;
	napps = 0;
	if (kpr_sample_take(&s, KPR_WANT_STATUS | KPR_WANT_BOX) != 0)
		return;
	for (int i = 0; i < s.n; i++) {
		const KprProc *p = &s.p[i];
		const char *name = p->box[0] ? p->box : p->comm;
		int j;

		if (p->ppid == 2 || p->pid == 2 || p->pid == 1 || !name[0])
			continue;
		for (j = 0; j < nall && strcmp(all[j].name, name); j++)
			;
		if (j == nall) {
			if (nall == cap) {
				cap = cap ? cap * 2 : 64;
				all = realloc(all, (size_t)cap * sizeof(*all));
				if (!all)
					break;
			}
			snprintf(all[nall].name, sizeof(all[nall].name), "%s",
				 name);
			all[nall].rss = 0;
			nall++;
		}
		all[j].rss += p->rss;
	}
	kpr_sample_free(&s);
	if (all) {
		qsort(all, (size_t)nall, sizeof(*all), by_rss);
		napps = nall < AB_APPS ? nall : AB_APPS;
		memcpy(apps, all, (size_t)napps * sizeof(*all));
	}
	free(all);
}

/* ── the facts ─────────────────────────────────────────────────────────── */

static void uptime_text(char *out, size_t n)
{
	unsigned long long s = kpr_uptime_s();

	if (!s)
		snprintf(out, n, "unknown");
	else if (s >= 86400)
		snprintf(out, n, "%llud %lluh %llum", s / 86400,
			 (s % 86400) / 3600, (s % 3600) / 60);
	else
		snprintf(out, n, "%lluh %02llum %02llus", s / 3600,
			 (s % 3600) / 60, s % 60);
}

/* The uptime row is rewritten on every tick; the rest of the page is read
 * once, because none of it changes while the window is open. */
static struct fact *uptime_row;

static void gather_machine(void)
{
	struct facts *fs = &machine;
	char buf[AB_VAL], ver[AB_VAL], arch[32];
	struct utsname u;
	KprCpu c = { 0 };

	fs->n = 0;
	if (os_release("NAME", buf, sizeof(buf)) == 0) {
		if (os_release("VERSION", ver, sizeof(ver)) != 0)
			ver[0] = '\0';
		fact(fs, "", "%s%s%s", buf, ver[0] ? " " : "", ver);
	}

	/* proc/sys/kernel through the root, uname() where the kernel has no
	 * `arch` file: the fixture records both, the host may answer only
	 * the call. */
	if (proc_line("sys/kernel/arch", arch, sizeof(arch)) != 0)
		snprintf(arch, sizeof(arch), "%s",
			 uname(&u) == 0 ? u.machine : "");
	if (proc_line("sys/kernel/osrelease", buf, sizeof(buf)) == 0)
		fact(fs, "kernel", "%s %s", buf, arch);

	if (kpr_cpu_read(&c) == 0) {
		fact(fs, "cpu", "%s", c.model);
		fact(fs, "threads", "%d", kpr_cpu_online(&c));
		kpr_cpu_free(&c);
	}

	uptime_row = fs->n < AB_FACTS ? &fs->f[fs->n] : NULL;
	uptime_text(buf, sizeof(buf));
	fact(fs, "uptime", "%s", buf);

	slot_fact(fs);
	fact(fs, "session", "kdos-comp, %s", sh_term());
}

/* /proc/version: `Linux version R (who@host) (COMPILER, LINKER) #N … DATE`.
 * The compiler is the second parenthesis up to its comma; the date is the
 * last six words, of which the month, the day and the year are shown. */
static void kernel_build(struct facts *fs)
{
	char v[512];

	if (proc_line("version", v, sizeof(v)) != 0)
		return;

	char *cc = strstr(v, ") (");

	if (cc) {
		cc += 3;

		char *end = strchr(cc, ',');

		if (end) {
			char save = *end;

			*end = '\0';
			fact(fs, "compiler", "%s", cc);
			*end = save;
		}
	}

	char *w[64];
	int nw = 0;

	for (char *t = strtok(v, " "); t && nw < 64; t = strtok(NULL, " "))
		w[nw++] = t;
	if (nw >= 6 && strlen(w[nw - 1]) == 4)
		fact(fs, "kernel", "built %s %s %s", w[nw - 5], w[nw - 4],
		     w[nw - 1]);
}

/* One keyring directory as a fact: each key's id, which is the identifier
 * `kdos-pack` and `kpkg` print, and the name its file gives it. */
static void keyring(struct facts *fs, const char *key, const char *dir)
{
	KsigRing r;

	if (ksig_ring_load(&r, at(dir)) <= 0) {
		fact(fs, key, "no key trusted");
		return;
	}
	for (int i = 0; i < r.n; i++)
		fact(fs, i ? "" : key, "%s %s", r.key[i].id, r.key[i].name);
}

static void gather_build(void)
{
	struct facts *fs = &build;
	char buf[AB_VAL];

	fs->n = 0;

	/*
	 * BUILD_ID IS THE COMMIT THE IMAGE WAS BUILT FROM, `-dirty` when the
	 * tree had changes git had not recorded; KDOS_BUILD_DATE is that
	 * commit's date. Both are stamped by the image phase. An image built
	 * without them says so rather than leaving the rows out, because a
	 * missing stamp is itself what a bug report needs to know.
	 */
	if (os_release("BUILD_ID", buf, sizeof(buf)) == 0)
		fact(fs, "build", "%s", buf);
	else
		fact(fs, "build", "unstamped");
	if (os_release("KDOS_BUILD_DATE", buf, sizeof(buf)) == 0)
		fact(fs, "committed", "%s", buf);

	kernel_build(fs);
	fact(fs, "libc", "musl");
	fact(fs, "userland", "toybox");
	fact(fs, "packages", "%d", ncredits);
	keyring(fs, "host keys", "/etc/kdos/keys");
	keyring(fs, "pack keys", "/etc/kdos/keys/packs");
}

/* ── the credits ───────────────────────────────────────────────────────── */

static int by_name(const void *a, const void *b)
{
	return strcmp(((const struct credit *)a)->name,
		      ((const struct credit *)b)->name);
}

/*
 * Every installed package, by name, with the version its database entry's
 * first line records. Sorted, because readdir order is the filesystem's and
 * a roll that reshuffled between two machines would be two different lists.
 */
static void gather_credits(void)
{
	DIR *d = opendir(at("/var/lib/kpkg/db"));
	struct dirent *e;
	int cap = 0;

	ncredits = 0;
	if (!d)
		return;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.')
			continue;
		if (ncredits == cap) {
			int ncap = cap ? cap * 2 : 256;
			struct credit *nc = realloc(credits,
						    (size_t)ncap * sizeof(*nc));

			if (!nc)
				break;
			credits = nc;
			cap = ncap;
		}

		struct credit *c = &credits[ncredits];
		char path[600], line[96];
		FILE *f;

		snprintf(c->name, sizeof(c->name), "%s", e->d_name);
		c->ver[0] = '\0';
		snprintf(path, sizeof(path), "%s/%s",
			 at("/var/lib/kpkg/db"), e->d_name);
		f = fopen(path, "r");
		if (f) {
			if (fgets(line, sizeof(line), f))
				sscanf(line, "%31s", c->ver);
			fclose(f);
		}
		ncredits++;
	}
	closedir(d);
	if (ncredits)
		qsort(credits, (size_t)ncredits, sizeof(*credits), by_name);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * The facts type themselves in: row i starts when the intro is i/n of the
 * way through and is whole by the time the next one starts. Worth its end
 * value wherever nothing moves, which is every row whole.
 */
static int typed(int i, int n, int len)
{
	float t = ktui_anim_value(&intro) * (float)n - (float)i;

	if (t >= 1.0f)
		return len;
	if (t <= 0.0f)
		return 0;
	return (int)(t * (float)len);
}

/* One cell of a glyph tier: the console font's own character where it has
 * one, the ascii stand-in where it does not. */
static void glyph(int x, int y, int g, int fg)
{
	ktui_draw_text(x, y, 1, ktui_glyph[g], fg, KT_SURFACE, KT_A_NONE);
}

/* `s` cut to `n` bytes on a character boundary. */
static void cut(char *dst, size_t cap, const char *s, int n)
{
	int i = 0;

	while (s[i] && i < n) {
		i++;
		while ((s[i] & 0xC0) == 0x80)
			i++;
	}
	if ((size_t)i >= cap)
		i = (int)cap - 1;
	memcpy(dst, s, (size_t)i);
	dst[i] = '\0';
}

static void draw_facts(const struct facts *fs, int x, int y, int w, int rows)
{
	int n = fs->n < rows ? fs->n : rows;

	for (int i = 0; i < n; i++) {
		const struct fact *f = &fs->f[i];
		char shown[AB_VAL];
		int len = (int)strlen(f->val);
		int got = typed(i, n, len);

		cut(shown, sizeof(shown), f->val, got);

		/* The row with no key leads, in the accent: it is the answer
		 * to "what is this", and the rest are its details. A keyless
		 * row after the first continues the row above it. */
		if (i == 0 && !f->key[0]) {
			ktui_draw_text(x, y + i, w, shown, KT_ACCENT,
				       KT_SURFACE, KT_A_BOLD);
		} else {
			ktui_draw_text(x, y + i, AB_KEYW, f->key, KT_MID,
				       KT_SURFACE, KT_A_NONE);
			ktui_draw_text(x + AB_KEYW, y + i, w - AB_KEYW, shown,
				       KT_TEXT, KT_SURFACE, KT_A_NONE);
		}
		if (got > 0 && got < len) {
			int cx = x + (i == 0 && !f->key[0] ? 0 : AB_KEYW) +
				 ktui_utf8_width(shown);

			if (cx < x + w)
				glyph(cx, y + i, KT_G_FULL, KT_ACCENT);
		}
	}
}

/*
 * The logo, and the degauss: alternate rows thrown left and right by a
 * shrinking amount, fringed in the two warm slots, settling on the accent
 * with every row back in its column.
 */
static void draw_logo(int x, int y, int rows)
{
	float v = ktui_anim_value(&wobble);
	int left = ktui_anim_left(&wobble);
	float decay = AB_WOBBLE_MS ? (float)left / AB_WOBBLE_MS : 0.0f;
	int moving = ktui_anim_running(&wobble) && v != 0.0f;

	for (int i = 0; i < logo_n && i < rows; i++) {
		int dx = 0, fg = KT_ACCENT;

		if (moving) {
			dx = (int)(v * 3.0f * decay + 0.5f);
			if (i % 2)
				dx = -dx;
			fg = i % 2 ? KT_WARN : KT_ERR;
		}
		if (x + dx < 1)
			dx = 1 - x;
		ktui_draw_text(x + dx, y + i, logo_w + 2, logo[i], fg,
			       KT_SURFACE, KT_A_NONE);
	}
}

struct seg {
	unsigned long long bytes;
	int fg;
	int glyph;
	const char *name;
};

static const int APP_FG[AB_APPS] = { KT_ACCENT, KT_WARN, KT_TEXT };

static void draw_memory(int x, int y, int w)
{
	char a[16], b[16], line[128];

	if (!have_mem) {
		ktui_draw_text(x, y, w, "memory   not readable", KT_MID,
			       KT_SURFACE, KT_A_NONE);
		return;
	}

	unsigned long long used = mem.total > mem.available
				  ? mem.total - mem.available : 0;
	unsigned long long cache = mem.available > mem.free
				   ? mem.available - mem.free : 0;
	unsigned long long left = used;
	struct seg s[AB_APPS + 3];
	int ns = 0;

	snprintf(line, sizeof(line), "%s of %s in use", size_of(used, a,
		 sizeof(a)), size_of(mem.total, b, sizeof(b)));
	ktui_draw_text(x, y, AB_KEYW, "memory", KT_MID, KT_SURFACE, KT_A_NONE);
	ktui_draw_text(x + AB_KEYW, y, w - AB_KEYW, line, KT_TEXT, KT_SURFACE,
		       KT_A_NONE);

	for (int i = 0; i < napps; i++) {
		unsigned long long r = apps[i].rss < left ? apps[i].rss : left;

		s[ns++] = (struct seg){ r, APP_FG[i], KT_G_FULL, apps[i].name };
		left -= r;
	}
	s[ns++] = (struct seg){ left, KT_MID, KT_G_SHADE_MED, "other" };
	s[ns++] = (struct seg){ cache, KT_DIM, KT_G_SHADE, "cache" };
	s[ns++] = (struct seg){ mem.free, KT_DIM, KT_G_DOT, "free" };

	/* Each segment ends where its running total lands, so rounding never
	 * adds up to a bar one cell too long or short. */
	unsigned long long acc = 0;
	int cx = 0;

	for (int i = 0; i < ns; i++) {
		acc += s[i].bytes;

		int end = (int)((double)acc / (double)mem.total * w + 0.5);

		if (end > w)
			end = w;
		for (; cx < end; cx++)
			glyph(x + cx, y + 1, s[i].glyph, s[i].fg);
	}
	for (; cx < w; cx++)
		glyph(x + cx, y + 1, KT_G_DOT, KT_DIM);

	/* The legend, wrapped onto a second row where it does not fit. */
	int lx = 0, ly = y + 2;

	for (int i = 0; i < ns; i++) {
		char item[96];
		int iw;

		snprintf(item, sizeof(item), "%s %s", s[i].name,
			 size_of(s[i].bytes, a, sizeof(a)));
		iw = 2 + ktui_utf8_width(item);
		if (lx && lx + iw > w) {
			if (ly == y + 3)
				break;
			lx = 0;
			ly++;
		}
		glyph(x + lx, ly, s[i].glyph, s[i].fg);
		ktui_draw_text(x + lx + 2, ly, w - lx - 2, item, KT_MID,
			       KT_SURFACE, KT_A_NONE);
		lx += iw + 2;
	}
}

static int credit_cols(int w)
{
	int c = w / AB_CREDIT_W;

	return c < 1 ? 1 : c;
}

static int credit_rows(int w)
{
	int c = credit_cols(w);

	return (ncredits + c - 1) / c;
}

static void draw_credits(int x, int y, int w, int rows)
{
	char head[96];
	int cols = credit_cols(w), body = rows - 2;

	snprintf(head, sizeof(head), "KDOS stands on %d project%s", ncredits,
		 ncredits == 1 ? "" : "s");
	ktui_draw_text(x, y, w, head, KT_ACCENT, KT_SURFACE, KT_A_BOLD);
	if (!ncredits) {
		ktui_draw_text(x, y + 2, w, "no package database", KT_MID,
			       KT_SURFACE, KT_A_NONE);
		return;
	}
	for (int r = 0; r < body; r++) {
		int row = roll + r;

		if (row >= credit_rows(w))
			break;
		for (int c = 0; c < cols; c++) {
			int i = row * cols + c;

			if (i >= ncredits)
				break;

			int cx = x + c * AB_CREDIT_W;
			int nw = ktui_utf8_width(credits[i].name);

			ktui_draw_text(cx, y + 2 + r, AB_CREDIT_W - 1,
				       credits[i].name, KT_TEXT, KT_SURFACE,
				       KT_A_NONE);
			if (nw + 1 < AB_CREDIT_W - 1)
				ktui_draw_text(cx + nw + 1, y + 2 + r,
					       AB_CREDIT_W - nw - 2,
					       credits[i].ver, KT_MID,
					       KT_SURFACE, KT_A_NONE);
		}
	}
}

/* The line under the memory map: what KDOS is built not to have and what it
 * is built on. Separated by the glyph tier's bullet, so a console font with no
 * middle dot draws its own stand-in rather than a replacement mark. */
static const char *const MOTTO[] = {
	"no systemd", "no xorg", "musl", "toybox", "signed"
};
#define MOTTO_N ((int)(sizeof(MOTTO) / sizeof(MOTTO[0])))

static void draw_motto(int x, int y, int w)
{
	int cx = 0;

	for (int i = 0; i < MOTTO_N; i++) {
		int mw = ktui_utf8_width(MOTTO[i]);

		if (i) {
			glyph(x + cx + 1, y, KT_G_BULLET, KT_DIM);
			cx += 3;
		}
		if (cx + mw > w)
			break;
		ktui_draw_text(x + cx, y, w - cx, MOTTO[i], KT_MID, KT_SURFACE,
			       KT_A_NONE);
		cx += mw;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int lx = 2, kx = 2 + (logo_w ? logo_w + 3 : 0);
	int body = h - AB_TOP - 3;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "About KDOS", KT_ACCENT, KT_SURFACE,
		      1);
	ktui_tabs_draw(krect(2, 1, w - 4, 1), PAGES, PG_N, page, -1, 0);

	if (page == PG_CREDITS) {
		draw_credits(2, AB_TOP, w - 4, body);
	} else {
		draw_logo(lx, AB_TOP, body);
		if (page == PG_MACHINE) {
			int below = AB_TOP + (logo_n > machine.n ? logo_n
								  : machine.n);

			draw_facts(&machine, kx, AB_TOP, w - kx - 2,
				   h - AB_TOP - 3);
			if (below + 6 <= h - 2) {
				draw_memory(2, below + 1, w - 4);
				draw_motto(2, below + 5, w - 4);
			}
		} else {
			draw_facts(&build, kx, AB_TOP, w - kx - 2, body);
		}
	}

	ktui_hint("Left/Right", "page");
	if (page == PG_CREDITS)
		ktui_hint("Up/Down", "scroll");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
}

/* ── events ────────────────────────────────────────────────────────────── */

static const int KONAMI[] = {
	KT_K_UP, KT_K_UP, KT_K_DOWN, KT_K_DOWN,
	KT_K_LEFT, KT_K_RIGHT, KT_K_LEFT, KT_K_RIGHT, 'b', 'a'
};
#define KONAMI_N ((int)(sizeof(KONAMI) / sizeof(KONAMI[0])))
static int konami;

/* Every key still does its own job on the way through the code: Left and
 * Right flip pages and cancel each other out, so the code ends on the page
 * it began on. */
static void konami_step(int k)
{
	if (k == KONAMI[konami])
		konami++;
	else
		konami = k == KONAMI[0];
	if (konami == KONAMI_N) {
		konami = 0;
		ktui_anim_start(&wobble, 0.0f, 1.0f, AB_WOBBLE_MS,
				KT_EASE_IN_OUT, 4);
	}
}

static void scroll_to(int r)
{
	int max = credit_rows(ktui_w - 4) - (ktui_h - AB_TOP - 5);

	if (r > max)
		r = max;
	roll = r < 0 ? 0 : r;
}

static int credits_key(int k)
{
	int page_rows = ktui_h - AB_TOP - 5;

	switch (k) {
	case KT_K_UP:	scroll_to(roll - 1); break;
	case KT_K_DOWN:	scroll_to(roll + 1); break;
	case KT_K_PGUP:	scroll_to(roll - page_rows); break;
	case KT_K_PGDN:	scroll_to(roll + page_rows); break;
	case KT_K_HOME:	scroll_to(0); break;
	case KT_K_END:	scroll_to(credit_rows(ktui_w - 4)); break;
	default:	return 0;
	}
	return 1;
}

static int on_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_MOUSE) {
		if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
			if (page == PG_CREDITS) {
				rolling = 0;
				scroll_to(roll + (ev->btn == KT_MB_WHEEL_UP
						  ? -3 : 3));
			}
			return SH_EV_TAKEN;
		}
		if (ev->press != KT_MP_PRESS)
			return SH_EV_PASS;
		if (ev->btn == KT_MB_RIGHT)
			return SH_EV_CLOSE;

		int t = ktui_tabs_hit(krect(2, 1, ktui_w - 4, 1), PAGES, PG_N,
				      0, ev->mx, ev->my);

		if (t >= 0)
			page = t;
		return SH_EV_TAKEN;
	}
	if (ev->type != KT_EVT_KEY)
		return SH_EV_PASS;

	int k = ev->key;

	konami_step(k);
	if (k == KT_K_ENTER || k == 'q')
		return SH_EV_CLOSE;
	if (k == KT_K_TAB) {
		page = (page + 1) % PG_N;
		return SH_EV_TAKEN;
	}
	if (k == KT_K_BTAB) {
		page = (page + PG_N - 1) % PG_N;
		return SH_EV_TAKEN;
	}
	if (ktui_tabs_key(&page, PG_N, 0, k))
		return SH_EV_TAKEN;

	/* Any key stops the roll; the keys that scroll then scroll. */
	if (page == PG_CREDITS) {
		rolling = 0;
		credits_key(k);
	}
	return SH_EV_TAKEN;
}

/* A second, for the uptime; a roll step while the credits move. */
static int timeout(void)
{
	return page == PG_CREDITS && rolling ? AB_ROLL_MS : 1000;
}

static void tick(void)
{
	if (page == PG_CREDITS && rolling) {
		/* At the last row the roll starts again from the top. */
		if (roll >= credit_rows(ktui_w - 4) - (ktui_h - AB_TOP - 5))
			roll = 0;
		else
			roll++;
		return;
	}
	if (page == PG_MACHINE) {
		if (uptime_row)
			uptime_text(uptime_row->val, sizeof(uptime_row->val));
		sample_memory();
	}
}

static int about_arg(int argc, char **argv, int *i)
{
	if (strcmp(argv[*i], "--page") || *i + 1 >= argc)
		return 0;

	const char *p = argv[++*i];

	for (int n = 0; n < PG_N; n++)
		if (!strcasecmp(p, PAGES[n].name)) {
			page = n;
			return 1;
		}
	return 0;
}

static void about_start(int dump)
{
	const char *root = getenv("KDOS_ABOUT_ROOT");

	if (root && *root) {
		static char proc[512], sys[512];

		snprintf(proc, sizeof(proc), "%s/proc", root);
		snprintf(sys, sizeof(sys), "%s/sys", root);
		kpr_root_set(proc, sys);
	}
	gather_credits();
	gather_machine();
	gather_build();
	sample_memory();
	rolling = !dump;
}

static void about_ready(void)
{
	ktui_anim_start(&intro, 0.0f, 1.0f, AB_INTRO_MS, KT_EASE_LINEAR, 0);
	/* The roll is motion: where nothing moves the credits stand still at
	 * their top, and the keys scroll them. */
	rolling = rolling && ktui_anim_moving();
}

int about_main(int argc, char **argv)
{
	/*
	 * THE WORDMARK, NOT THE WHOLE FILE: logo.txt is the login banner's and
	 * carries the wordmark, a blank line and the mascot. The block before
	 * the first blank line is what sits beside the facts; the mascot would
	 * make the card taller than an eighty-by-twenty-four screen.
	 */
	if (sh_logo_load(at("/usr/share/kdos/logo.txt"), logo, SH_LOGO_LINES,
			 &logo_n, &logo_w) == 0) {
		logo_w = 0;
		for (int i = 0; i < logo_n; i++) {
			if (!logo[i][0]) {
				logo_n = i;
				break;
			}
			if (ktui_utf8_width(logo[i]) > logo_w)
				logo_w = ktui_utf8_width(logo[i]);
		}
	} else {
		logo_n = logo_w = 0;	/* no artwork is not a failure */
	}

	/* A dialog, not a dropdown: it is read rather than picked from, and
	 * clicking the window behind it to check something must not take it
	 * away. */
	const ShSurface s = {
		.cfg = {
			.role = KDISP_ROLE_OVERLAY,
			.cols = AB_COLS,
			.rows = AB_ROWS,
			.app_id = "kdos-about",
			.keyboard = 1,
		},
		.usage = "[--font NAME] [--page machine|build|credits] [--dump]",
		.keys = &keys,
		.popup = 1,
		.popup_bg = KT_SURFACE,
		.arg = about_arg,
		.start = about_start,
		.ready = about_ready,
		.draw = draw,
		.event = on_event,
		.timeout = timeout,
		.tick = tick,
	};

	return sh_run(&s, argc, argv);
}
