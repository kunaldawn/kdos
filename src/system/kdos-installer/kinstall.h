/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — the terminal wizard
 *
 * The probe, the answers and the install itself are kinstall-core's
 * (src/system/kinstall-core), which the graphical installer drives as well.
 * The terminal, the cell buffer, the input layer and every widget live in
 * libktui. What is left here is the wizard: its pages, its chrome and the
 * loop, for a serial line, a bare console and an unattended run.
 * ---------------------------------
 */

#ifndef KINSTALL_H
#define KINSTALL_H

#include "kinstall-core.h"
#include "ktui.h"

/* ────────────────────────────────────────────────────────────────────────
 * Pages
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct Page {
	const char *id;
	const char *title;
	const char *subtitle;
	void (*enter)(void);
	void (*draw)(KRect body);
	/* Return 0 to allow Next, or fill err and return non-zero. */
	int (*validate)(char *err, size_t n);
	int (*event)(KtuiEvent *ev);
	int hide_nav;
} Page;

extern Page *ki_pages[];
extern int ki_npages;
extern int ki_page;

int page_index(const char *id);
void page_goto(int n);
void nav_next(void);
void nav_back(void);

extern int ki_quit;
extern int ki_help;

#endif /* KINSTALL_H */
