/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-energyd — per-app Energy Impact, relative and never in watt-hours
 */

#ifndef KDOS_ENERGYD_H
#define KDOS_ENERGYD_H

#include <stdbool.h>
#include <stddef.h>

#include "kbase.h"

#define KE_SOCKET	"/run/kdos-energyd.sock"
#define KE_GROUP	"wheel"
#define KE_MAX_PROC	4096
#define KE_MAX_APP	256
#define KE_MAX_DOMAIN	8
/* `app (appbox box)`: two 63-byte names and the words between them, whole. */
#define KE_NAME		144

/* ── RAPL ──────────────────────────────────────────────────────────────── */

typedef struct {
	char name[32];			/* package-0, psys, …                   */
	char path[1024];		/* the powercap directory               */
	unsigned long long range;	/* max_energy_range_uj — it WRAPS       */
	unsigned long long last;	/* the previous reading                 */
	bool have_last;
} KeDomain;

typedef struct {
	KeDomain d[KE_MAX_DOMAIN];
	int n;
	bool gpu_inside;		/* an `uncore` subdomain exists         */
	bool readable;			/* energy_uj could actually be read     */
	char why[1280];			/* why not, when it could not           */
} KeRapl;

int ke_rapl_open(KeRapl *r);
/* Energy since the last call, in µJ, wrap-corrected. -1 before the first. */
long long ke_rapl_delta(KeRapl *r);

/* ── one /proc sample ──────────────────────────────────────────────────── */

typedef struct {
	int pid;
	int ppid;
	char comm[40];
	unsigned long long cpu;		/* utime + stime, clock ticks           */
	unsigned long long gpu_ns;	/* Σ drm-engine-* from fdinfo           */
} KeProc;

typedef struct {
	KeProc *p;
	int n;
	double when;			/* CLOCK_MONOTONIC seconds              */
	unsigned long long busy;	/* /proc/stat busy ticks, ALL processes */
	bool gpu_seen;			/* any drm-engine key anywhere          */
} KeSample;

void ke_sample_take(KeSample *s);
void ke_sample_free(KeSample *s);

/* ── the ledger ────────────────────────────────────────────────────────── */

/*
 * Per app, TWO weighted sums rather than one energy — because the idle floor is
 * not known until the end.
 *
 * The floor can only fall (it is a minimum over every window seen), so applying
 * it as each window is folded in charges the early windows a floor that later
 * turns out to be too high — and the first window, which is charged the whole of
 * its own power, contributes nothing at all. That is exactly backwards: the
 * first window is usually the busiest, because something was just launched.
 *
 * So each window contributes an app's share `w` of that window's energy (`we`)
 * and of its duration (`wt`), and the report computes `we − floor·wt` with the
 * floor as it finally stands. Same arithmetic, applied once, to everything.
 */
typedef struct {
	char name[KE_NAME];		/* the app, and its box when it has one  */
	char app[64];			/* the app alone                        */
	char box[64];			/* the box alone; empty when unboxed    */
	double we;			/* Σ share × window energy   (µJ)       */
	double wt;			/* Σ share × window duration (s)        */
	double gpu_ns;
	double cpu_ticks;
} KeApp;

typedef struct {
	KeApp app[KE_MAX_APP];
	int n;
	double short_we, short_wt;	/* busy ticks no surviving pid claims     */
	double total_uj;		/* everything RAPL reported               */
	double floor_uw;		/* the lowest average power seen          */
	bool have_floor;
	double started;			/* CLOCK_MONOTONIC seconds                */
	double window;			/* seconds of samples folded in           */
	int samples;
	int suspended;			/* windows left out: the machine slept    */
	bool gpu_seen;
} KeLedger;

/*
 * A window longer than this in CLOCK_BOOTTIME than in CLOCK_MONOTONIC had the
 * machine asleep inside it. RAPL counters run from wherever the firmware left
 * them after a resume, so the delta across one is a wrap that never happened,
 * or a reset read as one, and is dropped rather than charged to whatever ran
 * in the ten seconds around the lid.
 */
#define KE_SLEPT_S	1.0

/* Fold one window into the ledger. `energy_uj` is the RAPL delta over it, and
 * `slept_s` how long the machine was suspended inside it. */
void ke_ledger_add(KeLedger *l, const KeSample *prev, const KeSample *cur,
		   long long energy_uj, double slept_s);
void ke_ledger_report(const KeLedger *l, const KeRapl *r, KbBuf *out,
		      bool json);

/* ── identity ──────────────────────────────────────────────────────────── */

/* Names the app a pid belongs to, and its box: `app` always, `box` empty
 * when the process is in none. Public for the sake of the selftest. */
void ke_identity_of(const KeSample *s, int idx, char *app, size_t appcap,
		    char *box, size_t boxcap);
/* The two as one ledger name: `app (appbox box)`, or `app`. */
void ke_name_of(const KeSample *s, int idx, char *out, size_t cap);
void ke_apps_load(void);
long ke_hz(void);

/* The roots of the /proc and /sys walks, moved by the fixtures. */
const char *ke_proc(void);
const char *ke_powercap(void);

#endif
