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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <aalib.h>
#include "bb.h"

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
    sound_sync();
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

/*
 * ONE FRAME IS ONE UNIT, WHICH THE FRAME ITSELF CANNOT SAY.
 *
 * aa_flush() hands the picture to ncurses' refresh(), and refresh() writes
 * ONE write() PER SCREEN ROW -- 21 writes and 2.0 KB at 75x19, 69 writes and
 * 17.2 KB at 236x63. A pseudo-terminal holds 12288 bytes, so at a full screen
 * the frame is larger than the pipe it crosses and CANNOT cross in one piece
 * however this program is written. A consumer composing on its own clock
 * therefore reads a screen that is half this frame and half the last one:
 * modelled against a 16ms compose, 7.5% of composes show a torn frame and
 * between eighteen and twenty-four per cent of frames are never shown whole.
 *
 * DECSET 2026 IS HOW A PRODUCER SAYS SO, and the bracket rather than the
 * write is the fix precisely because the write cannot be made atomic.
 * Between the set and the reset the terminal keeps showing the screen it
 * already had and composes nothing it receives, so the rows arrive in as many
 * writes as they like and the picture changes once, whole. kdos-term honours
 * it through libkvt's kvt_term_sync_hold(), under a watchdog: a producer that
 * sets the hold and dies must not freeze the window. A terminal that honours neither ignores a
 * private mode it does not know and gets exactly the stream it gets today.
 *
 * ON THE SAME STREAM AS THE FRAME, WHICH IS WHY IT IS stdout AND WHY BOTH
 * ENDS ARE FLUSHED. aalib's curses driver is initscr() on stdout, so
 * refresh() writes to this very FILE; an escape sent down any other fd
 * arrives in an order nothing defines, and a hold that lands after the rows
 * it was meant to cover is worse than none. A stdout that is not a terminal
 * is not bracketed at all -- there is nothing there to hold, and the two
 * sequences would be bytes in somebody's capture.
 */
#define BB_SYNC_BEGIN "\033[?2026h"
#define BB_SYNC_END   "\033[?2026l"

void bbflush(void)
{
    static int tty = -1;

    if (tty < 0)
	tty = isatty(1);
    if (tty) {
	fputs(BB_SYNC_BEGIN, stdout);
	fflush(stdout);
    }
    aa_flush(context);
    if (tty) {
	fputs(BB_SYNC_END, stdout);
	fflush(stdout);
    }
}

/*
 * HOW OFTEN THE ANIMATION LOOP MAY DRAW.
 *
 * A scene states a rate for its CONTROL and never for its picture: one draw
 * cost twenty-five milliseconds on the hardware this was written for, so the
 * loop paced itself and nothing here had to. It does not pace itself now.
 * Without this cap the loop draws as fast as the machine turns it -- measured
 * at five to fifteen thousand frames a second on a fifty-column window, and
 * seventeen megabytes a second of escape sequences for a terminal that can
 * show sixty frames. A terminal that cannot drain that keeps the pty full,
 * every write comes apart mid-frame, and what reaches the screen is the top
 * of one frame over the bottom of the one before it.
 *
 * IT IS A DEADLINE AND NOT A DELAY, AND THAT IS THE WHOLE OF WHAT MAKES IT
 * A CAP RATHER THAN A BRAKE. Timed from the END of a draw, the period is
 * this number PLUS whatever the draw cost, and the draw grows with the
 * screen: 0.46ms at 80x25 against 1.35ms at 240x67. So the period grows with
 * the screen too, and it grows past the one thing it must stay under --
 * 1000000 / 60, the interval fourteen scenes state for their control. In
 * waitmode the picture is drawn only on a turn the control fired, so a
 * period longer than the control's refuses every other tick outright and the
 * next chance is a whole interval later. Measured at 106x33, end-to-end
 * timing: 49fps with a quarter of the frames held for 33ms and NOTHING
 * between 18 and 30 -- the bimodal 16.7/33.3 alternation that an eye reads as
 * judder, and the reason it is worse on a bigger screen. Deadline to
 * deadline the draw is inside the period instead of added to it, and the
 * rate is the same at every size.
 *
 * SIXTEEN, WHICH IS UNDER THE CONTROL GRID AND AT OR UNDER EVERY CONSUMER'S
 * FLOOR. The 666us it leaves below 1000000 / 60 is the margin, and what the
 * margin buys is that the deadline is never AHEAD of the control tick it is
 * checked against: a grid shorter than the control's falls 666us further
 * behind the tick every frame, and the re-peg below puts it back no closer
 * than 666us behind. A tick is therefore refused only after a stall, never
 * for the jitter of noticing one. kdos-term has no such constant at all,
 * because its draw is gated on the compositor's frame callback, so its floor
 * is the output's -- 16.667 at sixty hertz. A frame
 * produced faster than the floor is not a frame anybody sees: it is bytes
 * the consumer must still read and parse. Nor is the surplus dropped quietly
 * -- a producer beats against its consumer at the difference, and a beat is
 * what an eye reads as judder. 16000 leaves two and a half a second against
 * sixty hertz, which is the price of the margin above.
 *
 * FURTHER UNDER IS SURPLUS AND FURTHER OVER IS JUDDER, which is the whole of
 * why the number is not free either way. A scene stating a POSITIVE rate is
 * not gated by its control at all -- it draws on every turn -- so its picture
 * runs at exactly this rate, and every frame of it past the consumer's is
 * bytes nobody composes and a beat against what does. Over 1000000 / 60 the
 * margin above is gone and the rate halves.
 */
#define BB_FRAME_US 16000

void timestuff(int rate, void (*control) (int), void (*draw) (void), int maxtime)
{
    int waitmode = 0, t;
    /*
     * THE DEADLINE FOR THE NEXT FRAME, ON THE SCENE CLOCK, which never
     * restarts -- so the cap carries across consecutive calls and a scene
     * split into five of these is not handed a free frame at each seam.
     */
    static int nextdraw;
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
	if (!called && waitmode) {
	    tl_sleep(t);
	    continue;
	}

	/*
	 * THE CONTROL KEEPS ITS RATE AND ONLY THE PICTURE IS CAPPED. A
	 * control handler is told how many intervals it covers, so a dropped
	 * frame moves nothing in the animation and the scene still ends on
	 * `endtime` -- which is what keeps every beat in step with the music.
	 */
	{
	    /*
	     * THE INT SUBTRACTION IS THE WRAP HANDLING AND NOTHING ELSE IS
	     * NEEDED. The scene clock is an int of microseconds and turns over
	     * after some thirty-five minutes, which `-loop` reaches; the
	     * difference of two readings either side of that is modular and
	     * comes out as the small number it really is, so a test for a
	     * negative gap would catch nothing but the servo -- which cannot
	     * produce one either, because its trim is capped at a fraction of
	     * the time that has actually passed.
	     */
	    int due = nextdraw - TIME;

	    if (draw != NULL && due <= 0) {
		/*
		 * THE READING THE DEADLINE WAS MET ON, KEPT ACROSS THE DRAW.
		 * Every line below measures from here and none of them from
		 * the clock afterwards: a reading taken after the draw is
		 * later by the cost of the draw, and anything paced off it
		 * has that cost added to the period. Measured at 240x67, a
		 * 1.35ms draw against a 16000 period and a 16666 control
		 * grid is the difference between sixty frames a second and
		 * thirty-one.
		 */
		int at = TIME;

		draw();
		tl_update_time();
		TIME = tl_lookup_timer(scenetimer);
		nextdraw += BB_FRAME_US;

		/*
		 * A DEADLINE MORE THAN A WHOLE PERIOD BEHIND IS MOVED RATHER
		 * THAN CHASED. The grid walks slowly behind the clock by
		 * design -- the period is shorter than the control interval
		 * it has to stay under -- and it is let walk, because every
		 * one of those frames is drawn. What must not be chased is a
		 * STALL: catching up would draw every missed frame back to
		 * back, the pty fills, the frames tear, and what the stall
		 * cost is paid twice. One frame is drawn late and the grid
		 * restarts from the deadline that was met.
		 */
		if (nextdraw - at <= 0)
		    nextdraw = at + BB_FRAME_US;
		continue;
	    }

	    /*
	     * A WAITMODE LOOP IS WOKEN BY ITS OWN CONTROL TIMER on the next
	     * turn and must not sleep past it. This one has no such timer to
	     * wake it, so it sleeps to whichever comes first -- without that
	     * it spins a core waiting for a frame it is not yet allowed to
	     * draw.
	     *
	     * AND NEVER PAST THE END OF THE SCENE, which is bbwait()'s rule
	     * and is here for its reason: the beat a scene ends on is the
	     * beat the music is on, and a loop that overslept its own last
	     * frame would hand the next scene a late start.
	     */
	    if (!waitmode) {
		/* tl_process_group() answers -1 for a group with no live
		 * timer, and tl_sleep() refuses that outright -- so the
		 * frame's own deadline is the bound whenever the group has
		 * nothing to offer, or this loop spins. */
		int wait = t < 0 ? due : (due > 0 && due < t ? due : t);

		if (wait > endtime - TIME)
		    wait = endtime - TIME;
		if (wait > 0)
		    tl_sleep(wait);
	    }
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
      if (t < 0 || t > BB_WAIT_SLICE_US)
	t = BB_WAIT_SLICE_US;
      if (t > wait)
	t = wait;
      if (t > 0)
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
	bbflush();
    }
    bbwait(maxtime);
}

static int stage = 1;

int bbnosound, bbmixer;

int bbinit(int argc, char **argv)
{
    int i;
    aa_defparams.supported|= AA_NORMAL_MASK | AA_BOLD_MASK | AA_DIM_MASK;
    aa_parseoptions(NULL, NULL, &argc, argv);
    /*
     * The demo starts on its own. Upstream asked "Music?[Y/n]" and then sat
     * in the mixer settings table until somebody pressed Continue, which is
     * two prompts in front of a demo whose whole point is that you start it
     * and watch it. Both defaults are what everyone picked anyway, so they
     * are the behaviour and the two escapes are flags.
     */
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-loop"))
	    loopmode = 1;
	else if (!strcmp(argv[i], "-nosound"))
	    bbnosound = 1;
	else if (!strcmp(argv[i], "-mixer"))
	    bbmixer = 1;
	else if (argv[i][0] > '0' && argv[i][0] <= '8' && !argv[i][1])
	    stage = atol(argv[i]);
	else {
	    printf("Usage: kdos-bb [aaoptions] [-loop] [-nosound] [-mixer] [number]\n\n");
	    printf("Options:\n"
		   "  -loop          play demo in infinite loop\n"
		   "  -nosound       run silent; the extro scroll falls back to a fixed rate\n"
		   "  -mixer         show the sample rate and mixing settings before starting\n"
		   "  number         start at stage 1, 2 or 3\n\n"
		   "AAlib options:\n%s\n", aa_help);
	    exit(1);
	}
    }
    context = aa_autoinit(&aa_defparams);
    if (!context) {
	printf("Failed to initialize aalib\n");
	exit(2);
    }
    if (!aa_autoinitkbd(context, 0)) {
	aa_close(context);
	printf("Failed to initialize keyboard\n");
	exit(3);
    }
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
	    load_song("bb.s3m");
	    bbupdate();
	    starttime = endtime = TIME;

	    aa_resize (context);
	    scene1();
	    aa_resize (context);
	    scene3();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    vezen(&fk1, &fk2, &fk3, &fk4);
	    messager("FILIP KUPSA known as FK, Tingle Notions, Dawn Music\n"
		"birth: June 22 1979, Tabor, Czech Republic, sex: male\n"
		     "\n"
	     "1992 - Changed his piano for 386/mp.com/pc-speaker music\n"
		     "1993 - Got his first Sound Blaster\n"
		     "1995 - Changed his SB for a new GUS technology\n"
		     "1996 - Composed his first great hits\n"
		     "1996 - FAT recomposition made by Windows 95\n"
		     "1997 - Released his musac in BB\n"
		     "\n"
		     "1998 - Got retired\n"
		     "\n"
		     "Contact address: via KT");
	    devezen2();
	    aa_resize (context);
	    scene4();
	    aa_resize (context);
	    scene2();
	    aa_resize (context);
	    if (quitnow)
		goto quit;
	    vezen(&ms1, &ms2, &ms3, &ms4);
	    messager("MOJMIR SVOBODA known as MS, TiTania, MSS, Bill\n"
		     "birth: ??, Tabor, Czech Republic, sex: ? male ?\n"
		     "\n"
		     "1993 - Installed Linux on his 386sx/25 + 40MB HDD\n"
		     "1994 - Removed Linux to make space for Doom\n"
		   "1995 - Reinstalled Linux on his 486Dx4/120 + 850MB\n"
		     "1996 - Removed Linux to make space for Windows 95\n"
		     "\n"
		     "1997 - Removed Windows 95 to make space for aalib\n"
		     "\n"
		     "Contact address: titania@mbox.vol.cz");
	    devezen3();
	    aa_resize (context);
	    scene8();
	    aa_resize (context);
	    scene6();
	    aa_resize (context);
	case 2:
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    vezen(&kt1, &kt2, &kt3, &kt4);
	    messager("KAMIL TOMAN known as KT, Kato, Whale, Bart\n"
		 "birth: May 19 1979, Tabor, Czech Republic, sex: male\n"
		     "\n"
		     "1993 - Became a linux extremist\n"
		     "1993 - Successful attempt to establish a secret organization\n"
		     "       Commandline Brotherhood\n"
		     "1995 - Action 'koules' - a secret project to train brotherhood\n"
		     "       members - covered under a game design\n"
		     "\n"
		 "1998 - Heading a new wave of command line revolution\n"
		     "\n"
		     "Contact address: toman@horac.ta.jcu.cz");
	    bbupdate();
	    starttime = endtime = TIME;
	    devezen1();
	    aa_resize (context);
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    scene7();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    scene5();
	    if (quitnow)
		goto quit;
	    aa_resize (context);
	    scene10();
	    vezen(&hh1, &hh2, &hh3, &hh4);
	    messager("JAN HUBICKA known as HH, Jahusoft, HuJaSoft, JHS, UNIX, Honza\n"
		  "birth: Apr 1 1978, Tabor, Czech Republic, sex: male\n"
		     "\n"
		     "1991 - Installed underground hackers OS Linux\n"
		     "1995 - Headed Action 'koules'\n"
		     "1996 - Famous troan XaoS to convert all windows instalations\n"
		     "       into Linux\n"
		     "\n"
		     "1998 - Secret plan to make `Text Windows` system to confuse users\n"
		 "2001 - Planning an assassination of dictator Bill G.\n"
		     "\n"
		     "Contact address: hubicka@paru.cas.cz");
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
	    aa_resize (context);
	    credits2();
	}
    while (loopmode);
  quit:;
    aa_close(context);
    return (0);
}
