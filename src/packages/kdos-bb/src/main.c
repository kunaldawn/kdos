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
MODULE *module;
int bbsound;
void stop();
static int freqs[14] = {
  5512, 6615, 8000, 9600, 11025, 16000, 18900,
  22050, 27428, 32000, 33075, 37800, 44100, 48000
};

/*
 * THE MIXER IS FED FROM ITS OWN THREAD, AND THAT IS THE WHOLE OF THE AUDIO
 * FIX.
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
 * rendering, drains instantly, and the sound is perfect: the tell that it was
 * never a mixer problem or a buffer-size problem.
 *
 * So the mixer gets a thread with its own clock and the render loop cannot
 * reach it. libmikmod is built -pthread -D_REENTRANT, so MikMod_InitThreads()
 * answers 1 and MikMod_Update may be called from here; the library locks its
 * own state, so nothing in this file may take MikMod_Lock around a libmikmod
 * call — see the note in sound_thread().
 */
#define SOUND_TICK_NS 10000000L		/* 10ms — upstream's timer interval */

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

static void *
sound_thread (void *unused)
{
  struct timespec tick = { 0, SOUND_TICK_NS };

  (void) unused;
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

void
play ()
{
#ifdef HAVE_LIBMIKMOD
  sound_debug (module ? "play: starting" : "play: no module");
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

int
main (int argc, char *argv[])
{
  int p = 0;
  int retval;

  bbinit (argc, argv);
#ifdef HAVE_LIBMIKMOD
  aa_puts (context, 0, p++, AA_SPECIAL, "Music?[Y/n]");
  aa_flush (context);
  if (tolower (aa_getkey (context, 1)) != 'n')
    {
      MikMod_RegisterAllDrivers ();
      MikMod_RegisterLoader (&load_s3m);
      /*md_mode |= DMODE_SOFT_MUSIC; */
      while (1)
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
