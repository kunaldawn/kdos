/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * kdos-res — one program, three faces.
 *
 * The same page, the same layout and the same numbers on tty1 and in a window,
 * because there is only one renderer: a grid of character cells. libktui's
 * backend vtable decides who paints it — the terminal, libkwl, or an offscreen
 * buffer for --dump — and nothing above that line knows which.
 *
 * What this has that no other monitor on this machine has is IDENTITY: every
 * fat application here is its own podman container, and libkproc's conmon walk
 * turns a pid into `firefox-esr (appbox kdos-apps)`. That is the rollup the
 * Applications page draws and the reason this exists beside btop.
 */

#ifndef RES_H
#define RES_H

#include <stddef.h>
#include <stdint.h>

#include "kbase.h"
#include "kchrome.h"
#include "kcolor.h"
#include "kicon.h"
#include "kproc.h"
#include "ktui.h"

/* ── the pages ───────────────────────────────────────────────────────────
 *
 * The ids are the only spelling: --page takes them, res.conf's sort keys use
 * them and the goldens are named after them. Two spellings of one page is how
 * a --page flag silently opens the default instead.
 */
enum res_page_id {
	RP_APPLICATIONS = 0,
	RP_PROCESSES,
	RP_CPU,
	RP_MEMORY,
	RP_GPU,
	RP_DRIVES,
	RP_NETWORK,
	RP_BATTERIES,
	RP_ENERGY,
	RP_NPAGES
};

typedef struct ResPage {
	const char *id;		/* "cpu"  — the only spelling            */
	const char *name;	/* "CPU"  — what the sidebar shows       */
	const char *icon;	/* an atlas name, or NULL                */
	/*
	 * The headline reading for the band. The FRAME draws the band, full
	 * width, above the sidebar — a page that drew its own would start at
	 * the body's origin and paint over the sidebar beside it.
	 */
	/*
	 * Run once per frame BEFORE the band. A headline that counts rows the
	 * body has not built yet reports the previous frame's answer, and on
	 * the first frame reports zero.
	 */
	void (*prepare)(void);
	const char *(*headline)(void);
	void (*draw)(int x, int y, int w, int h);
	int  (*click)(int mx, int my, int btn);	/* 1 if it consumed it   */
	/*
	 * A WHEEL NOTCH IS NOT A CLICK, and a page that has no list has
	 * nothing to scroll. It arrives as a PRESS of KT_MB_WHEEL_UP/DOWN, so
	 * a frame that hands it to `click` selects the row under the pointer
	 * instead of scrolling — and over the sidebar it changes page. The
	 * frame routes it here instead, and a NULL means this page ignores it.
	 */
	int  (*wheel)(int up);			/* 1 if it consumed it   */
	int  (*key)(int k);			/* 1 if it consumed it   */
	/*
	 * THE POINTER, in the page's OWN coordinates — the frame subtracts its
	 * body origin once so no page carries a second copy of it.
	 *
	 * `motion` is what lets a row light under the hand; without it a table
	 * of forty rows is a picture, and the only way to find out that a row
	 * is a control is to click one. `release` exists for the one gesture
	 * that spans events: a drag of the scrollbar, which Wayland reports as
	 * plain motion with no button state at all, so the PRESS is what has
	 * to be remembered.
	 */
	void (*motion)(int mx, int my);
	void (*release)(void);
} ResPage;

extern const ResPage RES_PAGES[RP_NPAGES];

/* ── the shared state every page reads ───────────────────────────────── */
typedef struct {
	KprCpu cpu, cpu_prev;
	int cpu_have_prev;
	KprMem mem;
	KprPsi psi_cpu, psi_mem, psi_io;

	KprSample sample, prev;
	int have_prev;
	unsigned sample_flags;	/* what the visible page needs           */

	KprHist h_cpu, h_mem, h_swap;
	KprHist *h_core;	/* [cpu.ncpu]                            */

	unsigned long long tick;	/* how many samples so far       */
	unsigned long long started_ms;
	int overran;		/* a tick took longer than its interval  */
	int fixture;		/* reading a recorded machine            */
} ResState;

extern ResState R;

/* ── configuration ───────────────────────────────────────────────────── */
typedef struct {
	int interval_ms;
	int units_1024;
	int fahrenheit;
	int cpu_of_machine;	/* percent of the machine, not of a core */
	int memory_pss;
	int kernel_threads;
	int virtual_drives;
	int virtual_net;
	int icons;
	char sort[24];
	char columns[128];
} ResConf;

extern ResConf RC;

void res_conf_load(void);
const char *res_conf_path(void);

/* ── the frame ───────────────────────────────────────────────────────── */
/*
 * The ONE draw entry point. Every face calls this and nothing else, so a
 * layout cannot differ between the window, the terminal and a golden.
 */
void res_draw_frame(void);
int  res_page_index(const char *id);
void res_page_set(int idx);
int  res_page_current(void);
/* Route input at the frame level: the sidebar, the page cycle and F1 first,
 * then the page's own handler. Returns 1 when the program should exit. */
int  res_frame_key(int k);
int  res_frame_click(int mx, int my, int btn);
int  res_frame_wheel(int up);
void res_frame_motion(int mx, int my);
void res_frame_release(void);

/* ── sampling ────────────────────────────────────────────────────────── */
/*
 * What the VISIBLE page needs, and nothing more. A device page walks no
 * per-process files at all; the GPU column costs an fd/ readdir per process
 * and is asked for only while a GPU column is on screen.
 */
unsigned res_wanted_flags(void);
void res_sample(void);

/* ── small shared helpers ────────────────────────────────────────────── */
const char *res_size(unsigned long long bytes);	/* honours units_1024   */
const char *res_temp(double celsius);		/* honours fahrenheit    */
/* An unreadable counter renders as an em dash, never as 0. */
const char *res_counter(unsigned long long v);
const char *res_none(void);
const char *res_cpu_headline(void);
const char *res_mem_headline(void);
const char *res_proc_headline(void);
void res_draw_procs(int x, int y, int w, int h);
int  res_procs_key(int k);
int  res_procs_click(int mx, int my, int btn);
int  res_procs_wheel(int up);
void res_procs_motion(int mx, int my);
void res_procs_release(void);
void res_procs_prepare(void);
void res_app_prepare(void);
const char *res_app_headline(void);
void res_draw_apps(int x, int y, int w, int h);
int  res_app_key(int k);
int  res_app_wheel(int up);
int  res_app_click(int mx, int my, int btn);
void res_app_motion(int mx, int my);
void res_app_release(void);
void res_drive_prepare(void);
const char *res_drive_headline(void);
void res_draw_drives(int x, int y, int w, int h);
int  res_drive_key(int k);
int  res_drive_wheel(int up);
int  res_drive_click(int mx, int my, int btn);
void res_drive_motion(int mx, int my);
void res_net_prepare(void);
const char *res_net_headline(void);
void res_draw_net(int x, int y, int w, int h);
int  res_net_key(int k);
int  res_net_wheel(int up);
int  res_net_click(int mx, int my, int btn);
void res_net_motion(int mx, int my);
/* Fed from res_sample(): a rate needs two readings an interval apart, and a
 * page's prepare() runs once per FRAME. */
void res_dev_sample(void);
int  res_act_signal(const KprProc *p, int sig);
int  res_act_renice(const KprProc *p, int nice);
const char *res_act_helper_why(void);
const char *res_act_why_disabled(const KprProc *p);
void res_energy_prepare(void);
const char *res_energy_headline(void);
void res_draw_energy(int x, int y, int w, int h);
int  res_energy_key(int k);
void res_gpu_prepare(void);
const char *res_gpu_headline(void);
void res_draw_gpu(int x, int y, int w, int h);
void res_batt_prepare(void);
const char *res_batt_headline(void);
void res_draw_batt(int x, int y, int w, int h);
void res_theme_from_cache(void);
/* The one header band. Returns the first BODY row. */
void res_page_placeholder(int x, int y, int w, int h, const char *name);
void res_graph(int id, KRect r, const KprHist *h, const char *label,
	       const char *reading);
/* A mirrored pair on ONE shared axis: `a` above the midline in the accent,
 * `b` below it in the secondary. Summing the two hides the direction, which
 * is the only thing anybody opens a rate chart to see. */
void res_graph2(int id, KRect r, const KprHist *a, const KprHist *b,
		const char *label, const char *reading);
void res_draw_cpu(int x, int y, int w, int h);

/* ── the detail page ─────────────────────────────────────────────────────
 *
 * One subject, full screen, opened with Enter from a list. It owns the
 * VERBS: ending or renicing a process needs its name on the screen and a
 * confirm that says what will happen, and a list row is neither.
 */
void res_detail_open_proc(int pid);
void res_detail_open_app(const char *name, const char *box, int pid);
/* A device page supplies its own already-formatted facts: the subject is
 * that page's reading, and a second reader here would be a second answer. */
void res_detail_open_facts(const char *title, const char *sub,
			   const char *const *lines, int n);
int  res_detail_active(void);
void res_detail_close(void);
const char *res_detail_title(void);
const char *res_detail_subtitle(void);
void res_detail_draw(int x, int y, int w, int h);
int  res_detail_key(int k);
int  res_detail_click(int mx, int my, int btn);
int  res_detail_wheel(int up);
void res_detail_sample(void);
unsigned res_detail_wants(void);

/* The ONE confirm modal, shared by every verb — see page.c. */
void res_confirm(const char *title, const char *msg, const char *yes,
		 void (*on_yes)(void));
int  res_confirm_active(void);
void res_draw_mem(int x, int y, int w, int h);

#endif /* RES_H */
