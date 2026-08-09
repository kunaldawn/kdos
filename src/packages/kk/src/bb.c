/*
 * BB: The portable demo
 *
 * (C) 1997 by AA-group (e-mail: aa@horac.ta.jcu.cz)
 *
 * 3rd August 1997
 * version: 1.2 [final3]
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licences as by published
 * by the Free Software Foundation; either version 2; or (at your option)
 * any later version
 *
 * This program is distributed in the hope that it will entertaining,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILTY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Publis License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <aalib.h>
#include "bb.h"
#include "kk.h"

int finish_stuff, starttime, endtime;
int dual = 0;
static int quitnow = 0;
int loopmode;
aa_context *context;
aa_renderparams *params;
int TIME;
tl_timer *scenetimer;
struct font *font;

double getwidth(double size)
{
    double height = aa_imgheight(context) / size;
    double width = height * (double) aa_imgwidth(context) * 0.75 / aa_imgheight(context) * aa_mmheight(context) / aa_mmwidth(context);
    return (width);
}

void centerprint(int x, int y, double size, int color, char *text, int mode)
{
    if (!dual || !mode) {
	double height = aa_imgheight(context) / size;
	double width = height * (double) aa_imgwidth(context) * 0.75 / aa_imgheight(context) * aa_mmheight(context) / aa_mmwidth(context);
	print(x - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
    }
    else {
	if (mode & 1) {
	    double height = aa_imgheight(context) / size;
	    double width = height * (double) aa_imgwidth(context) * 0.75 / aa_imgheight(context) * aa_mmheight(context) / aa_mmwidth(context);
	    print(x / 2 - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
	}
	if (mode & 2) {
	    double height = aa_imgheight(context) / size;
	    double width = height * (double) aa_imgwidth(context) * 0.75 / aa_imgheight(context) * aa_mmheight(context) / aa_mmwidth(context);
	    print(aa_imgwidth(context) / 2 + x / 2 - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
	}
    }
}

void centerprinth(int x, int y, double size, int color, char *text, int mode)
{
    if (!mode || !dual) {
	double width = aa_imgwidth(context) / size;
	double height = width * (double) aa_imgheight(context) * 1.333 / aa_imgwidth(context) * aa_mmwidth(context) / aa_mmheight(context);
	print(x - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
    }
    else {
	if (mode & 1) {
	    double width = aa_imgwidth(context) / size / 2;
	    double height = width * (double) aa_imgheight(context) * 1.333 / aa_imgwidth(context) * aa_mmwidth(context) / aa_mmheight(context);
	    print(x / 2 - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
	}
	if (mode & 1) {
	    double width = aa_imgwidth(context) / size / 2;
	    double height = width * (double) aa_imgheight(context) * 1.333 / aa_imgwidth(context) * aa_mmwidth(context) / aa_mmheight(context);
	    print(aa_imgwidth(context) / 2 + x / 2 - (width * strlen(text)) / 2, y - height / 2, width, height, font, color, text);
	}
    }
}

static void (*control1) (int);
static int called = 0;

static void mycontrol(void *data, int i)
{
    called = 1;
    if (control1 != NULL)
	control1(i);
}
static void mycontrol2(void *data, int i)
{
   ((void (*) (int))data)(i);
}

int bbupdate()
{
    int ch;
    tl_update_time();
    TIME = tl_lookup_timer(scenetimer);
    tl_process_group (syncgroup, NULL);
    ch = aa_getkey(context, 0);
    switch (ch) {
    case 's':
    case 'S':
    case AA_BACKSPACE:
	finish_stuff = 1;
	break;
    case AA_ESC:
    case 'q':
	finish_stuff = 1, quitnow = 1;
    }
    return (ch);
}

void timestuff(int rate, void (*control) (int), void (*draw) (void), int maxtime)
{
    int waitmode = 0, t;
    tl_timer *timer;
    bbupdate();
    /*starttime = TIME; */
    endtime = starttime + maxtime;
    timer = tl_create_timer();
    if (control == NULL) {
	rate = -40;
    }
    if (rate < 0) {
	waitmode = 1, rate = -rate;
	control1 = control;
	tl_set_multihandler(timer, mycontrol, NULL);
    }
    else
	tl_set_multihandler(timer, mycontrol2, control);
    tl_set_interval(timer, 1000000 / rate);
    tl_add_timer(syncgroup, timer);
    tl_reset_timer(timer);
    tl_slowdown_timer (timer, starttime - TIME);
    if (control != NULL)
	control(1);
    while (!finish_stuff && TIME < endtime) {
	called = 0;
	bbupdate();
	t = tl_process_group(syncgroup, NULL);
	if (TIME > endtime)
	    break;
	if (!called && waitmode)
	    tl_sleep(t);
	else {
	    if (draw != NULL)
		draw();
	}
    }
    starttime = endtime;
    tl_free_timer(timer);
}

void
bbwait (int maxtime)
{
  int wait;
  if (finish_stuff)
    return;
  bbupdate ();
  endtime = starttime + maxtime;

  wait = endtime - TIME;
  while (wait > 0)
    {
      int t;
      bbupdate ();
      t = tl_process_group (syncgroup, NULL);
      wait = endtime - TIME;
      if (wait < t)
	t = wait;
      tl_sleep (t);
    }
  starttime = endtime;
}

void bbflushwait(int maxtime)
{
    int wait;
    if (finish_stuff)
	return;
    bbupdate();
    wait = maxtime + starttime - TIME;
    if (wait > 0) {
	aa_flush(context);
    }
    bbwait(maxtime);
}

static int stage = 1;

int bbinit(int argc, char **argv)
{
    aa_defparams.supported|= AA_NORMAL_MASK | AA_BOLD_MASK | AA_DIM_MASK;
    aa_parseoptions(NULL, NULL, &argc, argv);

    /* Argument checking moved to kk_parse_args, which has already removed
     * everything that is ours; whatever survives aa_parseoptions here is a
     * typo, and saying so beats initializing a display over it. */
    if (argc != 1) {
	printf("kk: unrecognized argument: %s\n"
	       "Try 'kk --help'.\n", argv[1]);
	exit(1);
    }

    /* The vcsa driver is roughly free and the curses one goes through the
     * terminal, so ask for 'linux' first and let aa_autoinit fall through to
     * curses when /dev/vcsa<n> is not writable — which, for a non-root user
     * on KDOS, is the normal case. --driver pins one for measuring. */
    /* BOTH drivers go on the recommended list, curses first so that
     * aa_recommendhidisplay's insert-at-head leaves the order linux, curses.
     * Naming only 'linux' is not enough: when it fails, aa_autoinit falls out
     * of the recommended list into its own aa_drivers[] sweep, and that sweep
     * lands on 'stdout' — which renders every frame as a fresh block of text
     * scrolling up the terminal. Measured on a pty, not assumed.
     *
     * Braces are load-bearing: aalib spells aa_recommendhidisplay as a macro
     * with a trailing semicolon, so an unbraced if/else does not compile. */
    if (kk.driver) {
	/* --driver pins one and does NOT fall back: it exists to measure a
	 * specific path, and a silent substitution would measure the other. */
	aa_recommendhidisplay(kk.driver);
    } else {
	aa_recommendhidisplay("curses");
	aa_recommendhidisplay("linux");
    }

    context = aa_autoinit(&aa_defparams);
    if (!context) {
	printf("Failed to initialize aalib\n");
	exit(2);
    }
    kk_note_driver(context->driver ? context->driver->shortname : NULL);

    if (!aa_autoinitkbd(context, 0)) {
	aa_close(context);
	printf("Failed to initialize keyboard\n");
	exit(3);
    }
    loopmode = kk.loop;
    stage = kk.scene;
    aa_hidecursor(context);
    return 1;
}

int bb(void)
{
    aa_gotoxy(context, 0, 0);
    introscreen();
    params = aa_getrenderparams();
    aa_render(context, params, 0, 0, 1, 1);
    font = uncompressfont( /*context->params.font */ &aa_font16);
    scenetimer = tl_create_timer();
    srand(time(NULL));
    if (stage != 1)
	finish_stuff = 1;
    do
	switch (stage) {
	default:
	case 1:
	    load_song(SONG_INTRO);
	    bbupdate();
	    starttime = endtime = TIME;

	    aa_resize (context);
	    kdos_intro();
	    aa_resize (context);
	    scene1();
	    aa_resize (context);
	    scene3();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    fx_matrix();
	    aa_resize (context);
	    vezen(&kd1, &kd2, &kd3, &kd4);
	    messager("KUNAL DAWN known as KD, kunaldawn\n"
		     "the one who types `make build` and waits\n"
		     "\n"
		     "     - builds every byte of this system from a tarball\n"
		     "     - musl and toybox, no systemd, no Xorg, no GTK, no Qt\n"
		     "     - 391 ports, one package manager, written in C\n"
		     "     - the whole distro compiles itself, offline\n"
		     "\n"
		     "     - the fat applications live in a container, and know it\n"
		     "\n"
		     "                    I use KDOS btw.\n"
		     "\n"
		     "Contact address: github.com/kunaldawn");
	    devezen2();
	    aa_resize (context);
	    scene4();
	    aa_resize (context);
	    scene2();
	    aa_resize (context);
	    if (quitnow)
		goto quit;
	    fx_tunnel();
	    aa_resize (context);
	    scene8();
	    aa_resize (context);
	    scene6();
	    aa_resize (context);
	case 2:
	    if (quitnow)
		goto quit;
	    load_song(SONG_MIDDLE);
	    bbupdate();
	    starttime = endtime = TIME;
	    aa_resize (context);
	    kdos_penguin();
	    aa_resize (context);
	    if (quitnow)
		goto quit;
	    scene7();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    fx_rotozoom();
	    aa_resize (context);
	    scene5();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    fx_scope();
	    aa_resize (context);
	    scene10();
	    aa_resize (context);
	    devezen4();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    credits();
	    if (quitnow)
		goto quit;
	case 3:
	    if (loopmode)
		break;
	    load_song(SONG_CREDITS);
	    aa_resize (context);
	    credits2();
	}
    while (loopmode);
  quit:;
    aa_close(context);
    return (0);
}
