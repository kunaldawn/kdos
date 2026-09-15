/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-term — one terminal, on either desktop
 *
 * libkvt is the state machine, libkdisp picks the server, and libkimg is the
 * only thing here that touches an untrusted picture. ONE BINARY: the same
 * source is an xdg-toplevel under kdos-comp and a cell surface under kdos-con,
 * because both are the same character grid and libkdisp is what makes the
 * difference a vtable rather than a second program.
 *
 * IT DOES NOT REPLACE foot. foot is the default terminal everywhere in this
 * tree and stays there until this one has run on real hardware — the same
 * decision kdos-res made beside btop.
 * ---------------------------------
 */

#ifndef KDOS_TERM_H
#define KDOS_TERM_H

#include <stddef.h>
#include <stdint.h>

#include "kbase.h"
#include "kcolor.h"
#include "kdisp.h"
#include "ktui.h"
#include "kvt.h"

/*
 * ~/.config/kdos/term.conf — `key = value`, the res.conf shape.
 */
typedef struct {
	char shell[256];	/* what an argument-less kdos-term runs      */
	char font[128];		/* fontconfig name; empty is the default     */
	int cols, rows;		/* the size asked for on first configure     */
	int scrollback;		/* lines kept above the screen               */
	int images;		/* decode pictures at all                    */
	int image_max;		/* the cap on ONE payload, in kilobytes      */
	int image_cells;	/* the widest a picture may be, in cells     */
	/*
	 * Ask before an UNBRACKETED paste that carries a newline. With
	 * bracketed paste on the child decides for itself; with it off the
	 * bytes execute, which is the one place a terminal can be made to act
	 * as the user. On by default, and a key rather than a constant because
	 * somebody pasting into a program that never enables bracketing all
	 * day will want it off.
	 */
	int paste_guard;
} TermConf;

extern TermConf TC;

void term_conf_load(void);
const char *term_conf_path(void);

/* ── the terminal ──────────────────────────────────────────────────────── */

typedef struct {
	struct kvt_term *t;
	int cols, rows;

	/*
	 * The selection in progress, and where the last click landed. It is
	 * libkvt's shape because libkvt decides what a drag means — this
	 * program only says where a finished selection goes.
	 */
	KvtUi ui;

	/*
	 * The OSC 8 link the pointer is over, 0 for none. Every cell sharing
	 * the id is underlined while it is set — the whole run, so a person
	 * can see where the address ends — and nothing is underlined when the
	 * pointer is elsewhere, so a screen of links does not become a screen
	 * of underlines.
	 */
	unsigned int hover;
} Term;

extern Term T;

/* ── input.c ───────────────────────────────────────────────────────────── */

/* A key becomes the bytes xterm-256color describes. Answers whether it was
 * consumed; a chord this terminal owns is handled before it gets here. */
int term_key(const KtuiEvent *ev);
void term_mouse(const KtuiEvent *ev);
/* Whatever the backend has finished receiving, into the child. */
void term_paste_pending(void);

/* ── pic.c ─────────────────────────────────────────────────────────────── */

/* Register the image callback on the terminal, or leave the three protocols
 * off. Off is what a build with no decoder and `images = no` both mean. */
void term_pic_init(void);
/* Re-state the picture geometry XTSMGRAPHICS reports. Called whenever the grid
 * changes: half the bound is the number of columns and rows. */
void term_pic_geom(void);
/* Hand every sprite this program registered back to pixman. */
void term_pic_shutdown(void);
/*
 * Advance every running animation whose frame is due, and answer how many
 * milliseconds until the next one — or -1 when nothing is animating. The loop
 * uses it as its poll timeout, which is what makes an idle terminal wait on its
 * descriptors and an animating one wake on its clock.
 */
int term_pic_tick(void);
/*
 * THE UNICODE PLACEHOLDERS IN A RENDERED FRAME, TURNED INTO THE PICTURES THEY
 * NAME.
 *
 * `tmux` cannot pass the kitty graphics protocol through, so `yazi` and `timg`
 * inside one transmit the image and then draw `U+10EEEE` cells where it should
 * appear, with the image's id in each cell's FOREGROUND COLOUR. Without this
 * the picture is transmitted, never placed, and every one of those cells is a
 * codepoint no font has.
 *
 * AFTER THE RENDER AND NOT INSIDE IT. libkvt holds no sprites and no store —
 * it decodes nothing, by rule — so the id is meaningless to it; the map from
 * an id to the tiles it registered is this program's, and this is where the
 * two meet.
 */
void term_pic_placeholders(KtuiCell *buf, int w, int h);

#endif /* KDOS_TERM_H */
