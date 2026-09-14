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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "timers.h"
#include "bb.h"
#ifdef HAVE_LIBMIKMOD
#include <mikmod.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <time.h>
MODULE *module;
int bbsound;
void stop();
static int freqs[14] = {
  5512, 6615, 8000, 9600, 11025, 16000, 18900,
  22050, 27428, 32000, 33075, 37800, 44100, 48000
};

/*
 * THE MIXER IS FED FROM ITS OWN THREAD, ON A CLOCK THE RENDER LOOP CANNOT
 * REACH. A clock is half of it and the thread's scheduling policy is the
 * other half -- see sound_rt_promote().
 *
 * MikMod_Update is a PULL api: it hands the card as much as the card will
 * take, and it must be called often enough to keep the ring full. Upstream
 * called it from a 10ms timer in `syncgroup`, which is pumped by
 * tl_process_group() inside bbwait() -- and bbwait()'s caller has just
 * finished aa_flush(), which WRITES THE WHOLE FRAME TO THE TERMINAL AND
 * BLOCKS while the terminal drains it.
 *
 * That coupling is the bug, and it is invisible on a console. Under a
 * compositor the terminal is doing real work per frame -- shaping a screenful
 * of cells, uploading a texture, and here also a fullscreen shader pass -- so
 * the pty backs up, aa_flush sits in write(), no timer runs, the ring empties
 * and the music stutters. Minimise that same window and the terminal stops
 * rendering, drains instantly, and the sound is perfect: the tell that the
 * freeze is the coupling and not the mixer.
 *
 * So the mixer gets a thread with its own clock and the render loop cannot
 * reach it. libmikmod is built -pthread -D_REENTRANT, so MikMod_InitThreads()
 * answers 1 and MikMod_Update may be called from here; the library locks its
 * own state, so nothing in this file may take MikMod_Lock around a libmikmod
 * call — see the note in sound_thread().
 */
#define SOUND_TICK_NS 10000000L		/* 10ms — upstream's timer interval */

/*
 * THE MIXER THREAD ASKS FOR REAL TIME, AND PLAYS ON WITHOUT IT.
 *
 * A thread with its own clock is only as good as the clock. At SCHED_OTHER it
 * runs when the scheduler thinks it is due, and what else is due in this
 * process is a render thread drawing aalib and writing a megabyte a second of
 * escape sequences at a terminal. Every wake this thread loses past what the
 * ring holds is an underrun, and libmikmod sets no sw params, so the stream
 * stops on one: the default stop threshold is the buffer, an empty ring is
 * -EPIPE, and the driver's recovery re-prepares the stream and throws the
 * queue away. What a listener hears is a short pause, over and over, with the
 * music resuming cleanly each time. SCHED_FIFO takes the scheduler out of it.
 *
 * TEN, BECAUSE THE FEEDER MUST LOSE TO ITS OWN CONSUMER. Whatever drains this
 * ring -- dmix's slave, or the audio server where one is in the path -- has to
 * preempt the thread filling it, and a feeder that outranks its consumer
 * starves it. 10 is below every priority an audio server takes, below the 50 a
 * threaded interrupt takes on a preemptible kernel, and above every
 * SCHED_OTHER thread on the box.
 *
 * AND IT IS A REQUEST, NEVER A REQUIREMENT. kdos-getty raises RLIMIT_RTPRIO
 * and the session inherits it, so the grant succeeds here; in a container or
 * on a system that hands its users no real time, pthread_setschedparam answers
 * EPERM and this thread keeps running exactly as it did. A demo that would not
 * start because it could not have a scheduling policy is worse than one that
 * stutters.
 */
#define SOUND_RT_PRIO 10

/*
 * WHAT IS LEFT WHEN THE POLICY IS REFUSED, and a fallback rather than a
 * companion. A real-time mixer already preempts the render thread absolutely,
 * so nicing the render thread as well would only slow the picture for nothing.
 * It is also the half that cannot itself be refused -- lowering a thread's own
 * priority needs no privilege -- so the degraded path always has something to
 * do.
 */
#define SOUND_RENDER_NICE 5

/*
 * KDOS_BB_DEBUG=1 reports which way the mixer is being fed. Silent otherwise:
 * this is a demo and its stderr is the terminal it is drawing on. The same
 * shape as KDOS_COMP_DEBUG and KDOS_PANEL_DEBUG.
 */
static void
sound_debug (const char *what)
{
  if (getenv ("KDOS_BB_DEBUG"))
    fprintf (stderr, "kdos-bb: %s\n", what);
}

static int sound_threaded;		/* MikMod_InitThreads() said yes    */
static pthread_t sound_tid;
static volatile int sound_running;
tl_timer *update_timer;			/* only the un-threaded fallback    */

static void
update_sound (void *data)
{
  if (Player_Active ())
    MikMod_Update ();
  else if (module)
    Player_SetPosition (0);
}

/*
 * HOW LATE THE MIXER IS ALLOWED TO BE, AND WHY THAT IS THE ONLY NUMBER WORTH
 * HAVING. What the ring holds is the whole of the budget: a gap longer than it
 * is silence and a restart from empty, and a gap shorter than it costs nothing
 * at all. The two sound completely different and look identical from the
 * render loop, and no library below reports either -- under dmix the kernel
 * counts no xrun, because the underrun is raised to the client in userspace.
 * So the gap itself is the instrument.
 */
static void
sound_gap_mark (void)
{
  static struct timespec prev, since;
  static long worst;
  static int on = -1, started;
  struct timespec now;
  long gap;

  if (on < 0)
    on = getenv ("KDOS_BB_DEBUG") ? 1 : 0;
  if (!on)
    return;
  clock_gettime (CLOCK_MONOTONIC, &now);
  if (!started)
    {
      started = 1;
      prev = since = now;
      return;
    }
  gap = (long) (now.tv_sec - prev.tv_sec) * 1000000L
    + (now.tv_nsec - prev.tv_nsec) / 1000;
  prev = now;
  if (gap > worst)
    worst = gap;
  if ((long) (now.tv_sec - since.tv_sec) < 5)
    return;
  fprintf (stderr, "kdos-bb: mixer worst gap %ld us\n", worst);
  worst = 0;
  since = now;
}

/*
 * PROMOTED FROM INSIDE THE THREAD, NOT THROUGH THE CREATE ATTRIBUTES.
 * PTHREAD_EXPLICIT_SCHED makes the policy a CONDITION of pthread_create, and
 * this file's answer to a failed create is the 10ms timer in play() -- the
 * very feeder the thread exists to replace. Asking here makes a refusal a
 * no-op.
 *
 * setpriority's PRIO_PROCESS takes a TID on Linux and the initial thread's TID
 * is the PID, so getpid() names the render thread. A 0 would name this one,
 * which is the opposite of what the fallback is for.
 */
static void
sound_rt_promote (void)
{
  struct sched_param sp = { 0 };
  int prio = SOUND_RT_PRIO;
  int lo = sched_get_priority_min (SCHED_FIFO);
  int hi = sched_get_priority_max (SCHED_FIFO);

  if (lo >= 0 && prio < lo)
    prio = lo;
  if (hi >= 0 && prio > hi)
    prio = hi;
  sp.sched_priority = prio;
  if (pthread_setschedparam (pthread_self (), SCHED_FIFO, &sp) == 0)
    {
      sound_debug ("mixer thread is SCHED_FIFO");
      return;
    }
  if (setpriority (PRIO_PROCESS, getpid (), SOUND_RENDER_NICE) == 0)
    sound_debug ("no real time - render thread niced instead");
  else
    sound_debug ("no real time and no nice - mixer is SCHED_OTHER");
}

static void *
sound_thread (void *unused)
{
  struct timespec tick = { 0, SOUND_TICK_NS };

  (void) unused;
  sound_rt_promote ();
  while (sound_running)
    {
      /*
       * NO MikMod_Lock HERE, AND WRAPPING THESE IN ONE IS A DEADLOCK.
       *
       * libmikmod's mutexes are plain PTHREAD_MUTEX_INITIALIZER — not
       * recursive — and every call below already takes `vars` for itself:
       * MikMod_Update, Player_Active and Player_SetPosition all open with
       * MUTEX_LOCK(vars). Taking it first in the caller means the library
       * then blocks on a mutex this very thread is holding, and the process
       * stops dead with the demo frozen mid-frame.
       *
       * MikMod_Lock is for protecting YOUR OWN access to libmikmod's exported
       * VARIABLES — md_mode, md_mixfreq — which is a different thing from
       * calling its functions. That internal locking is exactly what
       * MikMod_InitThreads() is promising, and it is what serialises this
       * thread against the main one.
       *
       * What it does not cover is `module`, which is ours: stop() joins this
       * thread before it frees it, so the read below is never a dangling one.
       */
      if (Player_Active ())
	MikMod_Update ();
      else if (module)
	Player_SetPosition (0);
      sound_gap_mark ();
      /*
       * THE SLEEP IS UNCONDITIONAL, AND AT SCHED_FIFO THAT IS THE WHOLE SAFETY
       * ARGUMENT. Every path through the body above falls into it. Nothing
       * here may grow an early `continue` and nothing may make the sleep
       * conditional: either is a real-time thread spinning, which on one core
       * is a box that stops answering.
       */
      nanosleep (&tick, NULL);
    }
  return NULL;
}

/*
 * Answers whether the caller must fall back to the timer. A pthread_create
 * that fails must leave the mixer pumped by SOMETHING — reporting the failure
 * only through `sound_threaded` was a hole: play() had already chosen the
 * thread branch by then, so the timer was never created and nothing fed the
 * card at all.
 */
static int
sound_thread_start (void)
{
  if (!sound_threaded)
    return 0;
  if (sound_running)
    return 1;
  sound_running = 1;
  if (pthread_create (&sound_tid, NULL, sound_thread, NULL) != 0)
    {
      sound_running = 0;
      sound_threaded = 0;
      sound_debug ("pthread_create failed — falling back to the timer");
      return 0;
    }
  sound_debug ("mixer thread started");
  return 1;
}

/* Joined BEFORE the module is freed, never after. */
static void
sound_thread_stop (void)
{
  if (!sound_running)
    return;
  sound_running = 0;
  pthread_join (sound_tid, NULL);
}
#endif

int
load_song (char *name)
{
#ifdef HAVE_LIBMIKMOD
  finish_stuff = 0;
  if (bbsound)
    {
      char *name2;
      name2 = malloc (strlen (name) + strlen(SOUNDDIR)+2);
      sprintf(name2, "%s/%s",SOUNDDIR, name);
      stop ();
      if (module != NULL)
	Player_Free (module);
      module = Player_Load (name2, 64, 0);
      if (!module)
        module = Player_Load (name, 64, 0);
      free (name2);
      sound_debug (module ? "module loaded" : "module FAILED to load");
      if (module == NULL)
	{
	  aa_printf (context, 0, 0, AA_SPECIAL,
		     "Failed to load module:%s",
		     MikMod_strerror (MikMod_errno));
	  aa_flush (context);
	  sleep (1);
	}
    }
  return 0;
#endif
}

/*
 * WHERE THE DEMO IS AGAINST WHERE THE TRACK IS.
 *
 * THE ANIMATION IS TUNED TO THE MUSIC AND NOTHING COUPLES THEM AT RUNTIME.
 * The scenes run on the wall clock and the mixer runs on the sound card's,
 * and they agree for exactly as long as the card's clock is the one the
 * module was measured against. Measured at a 50x19 terminal: stage one and
 * two are 287.74s against bb.s3m's 287.70s, and the credits with the beat
 * that follows them are 111.50s against bb2.s3m's 111.50s. That is a demo
 * tuned to the frame -- and a card playing one per cent fast is three
 * seconds out by the credits with nothing on the screen to say so.
 *
 * This says so. Under KDOS_BB_DEBUG it reports, every five seconds, how far
 * the demo is into the current track and how far the PLAYER is. Both are the
 * same fraction of the same module, so they diverge only when the audio
 * clock and the wall clock disagree -- which is the one thing that can put
 * this demo out of step and the one thing a listener cannot attribute.
 */
static int songstart;

void
sound_sync (void)
{
#ifdef HAVE_LIBMIKMOD
  static int last, on = -1;
  int prog;
  double demo;

  /* Read once: this is called from bbupdate(), which runs every turn of
   * every scene's loop. */
  if (on < 0)
    on = getenv ("KDOS_BB_DEBUG") ? 1 : 0;
  if (!on || !bbsound)
    return;
  if (TIME - last < 5000000 && TIME >= last)
    return;
  last = TIME;
  prog = song_progress ();
  if (prog < 0)
    return;

  /*
   * TWO NUMBERS AND NO ARITHMETIC, because the arithmetic would be wrong.
   *
   * The obvious third number -- the track length these two imply -- is not
   * honest: song_progress()'s rows-per-pattern is NOMINAL, so its reading is
   * skewed by an amount that is the MODULE's and not a constant. Measured
   * against an offline render: bb.s3m reads 1 to 14 thousandths HIGH and
   * bb3.s3m reads 13 LOW, both shrinking to nothing by the end, while
   * bb2.s3m reads up to 63 LOW and never converges at all. An implied length
   * therefore drifts across a track that is playing perfectly, and a reader
   * would chase it.
   *
   * So the two raw numbers go out and the book carries what the player
   * SHOULD read at each of them, measured per track. See kdos-bb.md.
   */
  demo = (TIME - songstart) / 1000000.0;
  fprintf (stderr, "kdos-bb: sync  demo %7.2fs, player %4d/1000\n",
	   demo, prog);
#endif
}

void
play ()
{
#ifdef HAVE_LIBMIKMOD
  sound_debug (module ? "play: starting" : "play: no module");
  songstart = TIME;
  if (module != NULL)
    {
      Player_Start (module);
      update_sound (NULL);
      if (!sound_thread_start () && !update_timer)
	{
	  update_timer = tl_create_timer ();
	  tl_set_handler (update_timer, update_sound, NULL);
	  tl_set_interval (update_timer, 10000);
	  tl_add_timer (syncgroup, update_timer);
	  tl_reset_timer (update_timer);
	}
    }
#endif
}

void
stop ()
{
#ifdef HAVE_LIBMIKMOD
  if (bbsound)
    {
      /* The join comes FIRST. The mixer thread dereferences `module` every
       * 10ms, so freeing it while that thread is alive is a use-after-free
       * that would present as a crash somewhere inside libmikmod. */
      sound_thread_stop ();
      if (module)
        Player_Free (module);
      module = NULL;
      if (update_timer)
	{
	  tl_free_timer (update_timer);
	  update_timer = NULL;
	}
    }
#endif
}
#ifdef HAVE_LIBMIKMOD
struct table
{
  int v;
  char *c;
};
static struct table stable[] = { {DMODE_16BITS, "16 bit output"},
{DMODE_STEREO, "Stereo output"},
{DMODE_SOFT_MUSIC, "Process music via software mixer"},
{DMODE_HQMIXER, "Use high-quality (slower) software mixer"},
{DMODE_SURROUND, "Surround sound"},
{DMODE_INTERP, "Interpolation"},
{DMODE_REVERSE, "Reverse Stereo"},
{0, NULL}
};

int cont;
int srate;
int
ptable ()
{
  int i;
  for (i = 0; stable[i].c; i++)
    aa_printf (context, 0, i, AA_SPECIAL, "%i:%s - %-40s", i,
	       md_mode & stable[i].v ? "Yes" : "No ", stable[i].c);
  aa_printf (context, 0, i, AA_SPECIAL,
	     "%i:Sample rate: %i                            ", i, md_mixfreq);
  aa_printf (context, 0, i + 1, AA_SPECIAL,
	     "%i:Continue                                      ", i + 1);
  srate = i;
  cont = i + 1;
}
#endif

/*
 * THE EXTRO SCROLL IS PACED BY THE SONG, NOT BY A CLOCK.
 *
 * Answers thousandths through the module, or -1 when there is nothing to
 * ask. Position is taken as order-plus-row rather than order alone: an order
 * is several seconds at these tempos, and a scroll that only moved once per
 * pattern would step rather than flow.
 *
 * 64 is nominal, not measured -- S3M patterns are usually 64 rows and may be
 * shorter. A short pattern makes the fraction run slightly ahead inside that
 * one pattern and it is correct again at the boundary, which is invisible in
 * a scroll and not worth a Player_GetRow-against-pattern-length lookup.
 *
 * bb3.s3m LOOPS: update_sound() calls Player_SetPosition(0) when the player
 * falls inactive, so the answer wraps to 0 rather than reaching 1000. The
 * caller treats a fraction that went BACKWARDS as "the song is over", which
 * is the only honest reading of it.
 */
int
song_progress (void)
{
#ifdef HAVE_LIBMIKMOD
  int pos, row, n;
  if (!bbsound || module == NULL || !Player_Active ())
    return -1;
  n = module->numpos;
  if (n <= 0)
    return -1;
  pos = Player_GetOrder ();
  row = Player_GetRow ();
  if (pos < 0)
    pos = 0;
  if (pos > n)
    pos = n;
  if (row < 0 || row > 63)
    row = 0;
  return (pos * 64 + row) * 1000 / (n * 64);
#else
  return -1;
#endif
}

int
main (int argc, char *argv[])
{
  int p = 0;
  int retval;

  bbinit (argc, argv);
#ifdef HAVE_LIBMIKMOD
  /*
   * MUSIC IS ON AND THE DEMO STARTS. Upstream asked here and then held the
   * screen in the mixer table until Continue was pressed -- two questions
   * whose answers were always yes and default, in front of the one thing
   * this program does. `-nosound` and `-mixer` are the two escapes, and the
   * scroll in the extro reads the player, so a silent run is a different
   * pace rather than a broken one.
   */
  if (!bbnosound)
    {
      MikMod_RegisterAllDrivers ();
      MikMod_RegisterLoader (&load_s3m);
      /* MIXED AT THE RATE THE SINK RUNS AT. The library's own default is
       * 44100 and every sink on this image is 48000, so the default puts a
       * rate converter in the path -- run by whichever thread calls the
       * update, which is the one thread here with a deadline. `-mixer` may
       * still change it; this only moves the starting point. */
      md_mixfreq = 48000;
      /*md_mode |= DMODE_SOFT_MUSIC; */
      while (bbmixer)
	{
	  int k;
          aa_resize(context);
	  ptable ();
	  aa_flush (context);
	  k = aa_getkey (context, 1);
	  if (k >= '0' && k <= '0' + cont)
	    {
	      k -= '0';
	      if (k == cont)
		break;
	      if (k == srate)
		{
		  int i;
		  for (i = 0; i < 14; i++)
		    if (freqs[i] == md_mixfreq)
		      break;
		  md_mixfreq = freqs[(i + 1) % 14];
		}
	      if (k < srate)
		md_mode ^= stable[k].v;
	    }
	}
      bbsound = 1;
      /* Asked before MikMod_Init, which is where the docs put it. A 0 here
       * is not an error: the timer path below is the honest fallback for a
       * libmikmod built without threads. */
      sound_threaded = MikMod_InitThreads ()? 1 : 0;
      sound_debug (sound_threaded ? "libmikmod is thread-safe"
				  : "libmikmod has no thread support");
      if (MikMod_Init (""))
	{
	  aa_printf (context, 0, p++, AA_SPECIAL,
		     "Sound initialization failed:%s",
		     MikMod_strerror (MikMod_errno));
	  aa_flush (context);
	  bbsound = 0;
	  sleep (1);
	}
    }
#endif
  aa_resize(context);
  retval = bb ();
#ifdef HAVE_LIBMIKMOD
  if (bbsound)
    {
      /* Before MikMod_Exit for the same reason stop() joins before the free:
       * the driver is torn down here and the mixer thread must not be inside
       * it when that happens. */
      sound_thread_stop ();
      MikMod_Exit ();
    }
#endif
  return retval;
}
