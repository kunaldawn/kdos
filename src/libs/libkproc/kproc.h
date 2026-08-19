/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkproc — every reading this distro takes of its own machine
 * ---------------------------------
 *
 * Links libkbase and NOTHING ELSE. That is a constraint, not an accident:
 * kdos-oomd is a root daemon that links libkbase alone, and kdos-shell's panel
 * must never gain a dependency that can block. A library that reads files and
 * calls out to nothing is safe in both.
 *
 * Three properties decide the shape of everything below.
 *
 * ONE ROOT, MOVABLE. Every path is built from kpr_proc() and kpr_sys(), which
 * default to /proc and /sys. kpr_root_set() repoints them, and that is the
 * whole of --fixture: selection logic and arithmetic this consequential is
 * only testable against a recorded machine.
 *
 * SNAPSHOT AND DELTA, never a running total in a global. kpr_sample_take()
 * fills a KprSample and a rate is computed from two of them. Two consumers at
 * different cadences are then possible, which a library holding its own "last
 * value" cannot support.
 *
 * A MISSING READING IS NOT ZERO. KPR_UNREADABLE is the sentinel for a counter
 * the caller may not read, and a renderer must draw it as an em dash. A column
 * of confident zeroes is worse than an empty column, because somebody will act
 * on it.
 */

#ifndef KPROC_H
#define KPROC_H

#include <stddef.h>
#include <sys/types.h>

#include "kbase.h"

/*
 * The sentinel. (unsigned long long)-1 rather than 0 so that "root's io, which
 * this user may not read" and "a process that has done no io" stay distinct
 * all the way to the screen.
 */
#define KPR_UNREADABLE ((unsigned long long)-1)

/*
 * What a walk is allowed to read, per rule "a monitor that is the load is a
 * bug". `stat` is always read; everything else costs another open per process
 * and is asked for only by a page that displays it. KPR_WANT_GPU is the
 * expensive one — an fd/ readdir per process — and is off unless a GPU column
 * is on screen.
 */
#define KPR_WANT_CMDLINE 0x01
#define KPR_WANT_IO      0x02
#define KPR_WANT_BOX     0x04
#define KPR_WANT_GPU     0x08
#define KPR_WANT_STATUS  0x10	/* threads, uid, swap — /proc/<pid>/status */

/* virtualisation, from DMI sys_vendor plus cpuinfo's hypervisor flag */
enum { KPR_VIRT_NONE = 0, KPR_VIRT_UNKNOWN, KPR_VIRT_KVM, KPR_VIRT_QEMU,
       KPR_VIRT_VMWARE, KPR_VIRT_VBOX, KPR_VIRT_HYPERV, KPR_VIRT_XEN };

/* ── the root: the --fixture seam, and the only global state here ────────── */
void kpr_root_set(const char *proc, const char *sys);
const char *kpr_proc(void);
const char *kpr_sys(void);

/* Read a whole file under the proc or sys root. malloc'd, or NULL. /proc files
 * report st_size 0, so these read to real EOF rather than trusting stat. */
char *kpr_slurp_proc(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *kpr_slurp_sys(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* First line as a long long; def when absent or unparseable. */
long long kpr_num_sys(long long def, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* ── CPU ─────────────────────────────────────────────────────────────────── */
typedef struct {
	unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
} KprCpuTimes;

typedef struct {
	int ncpu;			/* logical                         */
	int ncore, npkg;		/* from topology/, never counted   */
	char model[96], arch[16];
	int virt;			/* KPR_VIRT_*                      */
	long khz_max;
	KprCpuTimes total;
	KprCpuTimes *per;		/* [ncpu]                          */
	long *khz;			/* [ncpu], -1 where unreadable     */
	double temp_c;			/* -1 when no sensor answered      */
	char governor[24];		/* empty when cpufreq has none     */
} KprCpu;

int  kpr_cpu_read(KprCpu *c);
void kpr_cpu_free(KprCpu *c);
/* Busy fraction 0..1 over the DELTA. Computed from the two samples and never
 * from an absolute, which would report the average since boot. */
double kpr_cpu_busy(const KprCpuTimes *prev, const KprCpuTimes *cur);

/* ── memory ──────────────────────────────────────────────────────────────── */
typedef struct {
	unsigned long long total, available, free, cached, buffers, shmem;
	unsigned long long swap_total, swap_free, dirty, reclaimable;
} KprMem;

/* `available` is MemAvailable. MemTotal - MemFree is not the used figure:
 * Linux spends every spare page on cache, and that arithmetic reports a
 * healthy machine at 95%. */
int kpr_mem_read(KprMem *m);

/* ── pressure ────────────────────────────────────────────────────────────── */
typedef struct {
	int present;			/* 0 when the kernel has PSI off   */
	double some10, some60, some300;
	double full10, full60, full300;
} KprPsi;

/* what is "cpu", "io" or "memory". A kernel without PSI yields present = 0,
 * which is a different thing from a stall of zero and renders differently. */
int kpr_psi_read(const char *what, KprPsi *out);

/* ── processes ───────────────────────────────────────────────────────────── */
typedef struct {
	int pid, ppid, uid, threads, nice, on_cpu;
	char state;			/* R S D Z T ...                   */
	char comm[24];
	char *cmdline;			/* space-joined, may be NULL        */
	char *exe;			/* readlink of exe, may be NULL    */
	unsigned long long utime, stime, starttime;
	unsigned long long rss, swap;
	unsigned long long rd_bytes, wr_bytes;	/* KPR_UNREADABLE if denied */
	unsigned long long gpu_ns, gpu_mem;
	char box[64];			/* the conmon box, or empty        */
} KprProc;

typedef struct {
	KprProc *p;
	int n;
	unsigned long long wall_ms;	/* monotonic, for the rate divisor */
	int nkthread;			/* how many were kernel threads    */
} KprSample;

int  kpr_sample_take(KprSample *s, unsigned flags);
void kpr_sample_free(KprSample *s);
/* Percent of ONE core. A caller wanting percent-of-machine divides by ncpu;
 * which convention is in force belongs in the column header, because the two
 * differ by a factor of ncpu and both are common. */
double kpr_proc_cpu(const KprProc *prev, const KprProc *cur,
		    unsigned long long wall_ms);
const KprProc *kpr_find_pid(const KprSample *s, int pid);
long kpr_hz(void);
unsigned long long kpr_mono_ms(void);

/* ── identity ────────────────────────────────────────────────────────────── */
/* The conmon walk. cgroups are the textbook answer and are unusable here: with
 * no systemd, rootless podman gets no cgroup delegation and the box often sits
 * in `0::/`. conmon carries `-n <name>` in its argv and is the boundary of the
 * box. Returns 1 and fills out, or 0. */
/*
 * How far up the parent chain the box walk climbs. ONE number: an app in a box
 * sits two or three below conmon, and four copies of this walk previously
 * carried four different bounds.
 */
#define KPR_BOX_HOPS 16

int kpr_box_of(const KprSample *s, int pid, char *out, size_t cap);
/* The same answer with no sample in hand, reading /proc as it climbs. */
int kpr_box_of_pid(int pid, char *out, size_t cap);
/* /etc/passwd, cached. There is no NSS here. Never NULL — an unknown uid comes
 * back as its own number. */
const char *kpr_user_of(int uid);

/* ── block devices ───────────────────────────────────────────────────────── */
typedef struct {
	char name[32], model[64];
	unsigned long long size;	/* bytes                           */
	int rotational, removable, virt;
	unsigned long long rd_sectors, wr_sectors, io_ticks;
	double temp_c;			/* -1 when no hwmon answered       */
} KprDisk;

/* Whole disks only: /proc/diskstats lists sda beside sda1 and summing the lot
 * counts every byte two or three times. A device with a `partition` attribute
 * is skipped; loop/ram/zram are flagged `virt` and left for the caller to
 * filter, so a user who asked for them still gets them. */
int  kpr_block_list(KprDisk **out);
void kpr_block_free(KprDisk *d);
/* A diskstats sector is 512 bytes BY DEFINITION of that interface, whatever
 * the drive's own sector size. queue/hw_sector_size is the classic way to be
 * eight times wrong on a 4K disk. */
#define KPR_SECTOR 512ULL

/* ── network ─────────────────────────────────────────────────────────────── */
typedef struct {
	char name[32], mac[24], driver[32];
	int up;				/* IFF_UP, the kernel's flags word */
	int carrier;			/* operstate: is a cable in it     */
	int loopback, virt, mtu;
	long speed_mbit;		/* -1 when the driver has none     */
	unsigned long long rx_bytes, tx_bytes, rx_pkts, tx_pkts;
	unsigned long long rx_err, tx_err, rx_drop, tx_drop;
} KprIface;

int  kpr_net_list(KprIface **out);
void kpr_net_free(KprIface *n);

/* ── power ───────────────────────────────────────────────────────────────── */
typedef struct {
	char name[32], tech[24], state[24];
	int is_battery, online;		/* online: AC adapters             */
	unsigned long long energy_now, energy_full, energy_full_design;
	long voltage_uv, current_ua, power_uw;
	int capacity, cycles;
	double health;			/* full / full_design, -1 unknown  */
} KprBattery;

int  kpr_power_list(KprBattery **out);
void kpr_power_free(KprBattery *b);

/* ── GPUs ────────────────────────────────────────────────────────────────── */
typedef struct {
	char name[32], driver[32], model[64];
	/*
	 * -1 when the driver publishes no utilisation, which is the ordinary
	 * case: only amdgpu and NVML have one. A renderer must then show
	 * ENGINE TIME and label it as such, never a percentage.
	 */
	int busy_percent;
	unsigned long long mem_total, mem_used;	/* KPR_UNREADABLE if none */
	double temp_c, power_w;			/* -1 when no sensor      */
	long freq_mhz;				/* -1 when unpublished    */
} KprGpu;

int  kpr_drm_list(KprGpu **out);
void kpr_drm_free(KprGpu *g);
/* 1 when a libnvidia-ml.so.1 was opened, 0 on a stock KDOS. */
int  kpr_nvml_probe(void);
int  kpr_nvml_read(int idx, KprGpu *g);

/* ── the sample ring, the axis and the smoothing ─────────────────────────── */
#define KPR_HIST 128

typedef struct {
	double v[KPR_HIST];
	int n;				/* how many are filled             */
	unsigned long long seq;		/* absolute sample number          */
	double scale;			/* the current axis                */
	int pinned;			/* 1 for a 0..100 percentage       */
} KprHist;

void   kpr_hist_init(KprHist *h, int pinned);
void   kpr_hist_push(KprHist *h, double v);
double kpr_hist_at(const KprHist *h, int i);	/* 0 = oldest kept         */
double kpr_hist_peak(const KprHist *h);
/* Grow the moment a sample does not fit, because a clipped chart is a lie;
 * shrink only under a THIRD of the scale, because one threshold in each
 * direction oscillates between two rungs for a series sitting on the boundary.
 * A pinned ring stays 0..100 and never rescales. */
double kpr_hist_scale(KprHist *h);
/* A three-point mean, for the PLOTTED series only. The printed number is never
 * smoothed: that would be lying about the instant. */
double kpr_hist_smooth(const KprHist *h, int i);

#endif /* KPROC_H */
