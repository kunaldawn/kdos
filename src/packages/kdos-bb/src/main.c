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

#include <errno.h>
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
/*
 * Set by play() and cleared by stop(), which is also how load_song() clears
 * it. It says the player has been STARTED on `module`, which is the one thing
 * sngtime cannot say for itself -- see sound_clock().
 */
static int playing;
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
/* One tick on, carrying the nanoseconds. */
static void
sound_tick_add (struct timespec *t)
{
  t->tv_nsec += SOUND_TICK_NS;
  if (t->tv_nsec >= 1000000000L)
    {
      t->tv_nsec -= 1000000000L;
      t->tv_sec++;
    }
}

static volatile long sound_worst_gap;

/*
 * IT RECORDS AND IT DOES NOT REPORT, and that is the whole point of the
 * split. This runs on the real-time mixer thread, and stderr here is the
 * pseudo-terminal the demo is drawing on: a terminal that is behind makes
 * fprintf block in write(), which parks the highest-priority thread in the
 * process behind the lowest and produces exactly the stall the instrument
 * exists to find. So the mixer keeps a number and the render thread prints
 * it -- see sound_sync().
 *
 * The number is one long written by one thread and read by another with no
 * lock. A word is not torn, a reading one tick stale says the same thing,
 * and a lock here would be the same inversion by another road.
 */
static void
sound_gap_mark (void)
{
  static struct timespec prev;
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
      prev = now;
      return;
    }
  gap = (long) (now.tv_sec - prev.tv_sec) * 1000000L
    + (now.tv_nsec - prev.tv_nsec) / 1000;
  prev = now;
  if (gap > sound_worst_gap)
    sound_worst_gap = gap;
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
  struct timespec next, now;

  (void) unused;
  clock_gettime (CLOCK_MONOTONIC, &next);
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
       *
       * IT IS A DEADLINE AND NOT A DELAY, so the work above is inside the
       * period rather than added to it: a relative sleep placed here makes
       * the period `tick + however long the update took` and the feed slower
       * than the tick it was sized against. A deadline already past is moved
       * to one tick from now rather than chased -- catching up would be this
       * thread running without sleeping, which is the case the paragraph
       * above forbids.
       */
      sound_tick_add (&next);
      clock_gettime (CLOCK_MONOTONIC, &now);
      if (next.tv_sec < now.tv_sec
	  || (next.tv_sec == now.tv_sec && next.tv_nsec <= now.tv_nsec))
	{
	  next = now;
	  sound_tick_add (&next);
	}
      while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL)
	     == EINTR)
	;
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
	  bbflush ();
	  sleep (1);
	}
    }
  return 0;
#endif
}

/*
 * THE SCENE CLOCK FOLLOWS THE PLAYER, BECAUSE A LOST MOMENT NEVER COMES BACK.
 *
 * A scene runs to an absolute deadline -- timestuff() computes `endtime` as
 * `starttime + maxtime` and chains the starts from one peg at the top of the
 * track -- so time the machine loses is paid for in FRAMES: the picture skips
 * and arrives at the same beat. The music cannot pay that way. The module
 * advances one tick per tick inside the mixer, every sample it renders is
 * written, and nothing in this program ever seeks the player forward. So a
 * moment the card spends not playing -- silence injected after a late wake,
 * a stream re-prepared from empty after an underrun -- is a moment the
 * picture took and the music did not, and the two are further apart
 * afterwards than before. It never closes again on its own.
 *
 * THE CORRECTION IS A RATE AND NEVER A LUMP. tl_slowdown_timer() is
 * subtracted from every later reading of the scene clock, and bb.c paces the
 * next frame off that same clock -- so a correction handed over in one piece
 * lands inside ONE frame interval, and that frame is as long as the piece.
 * Ten milliseconds of it against a sixteen millisecond budget is a frame
 * taking twenty-seven, and a few of those a second are the whole of what an
 * eye reads as judder: measured, the corrections accounted for 99.6% of the
 * variance in the frame interval, 3.660ms of it against a 16.000ms budget.
 * So the error is LOOKED AT every BB_SYNC_US and PAID OUT CONTINUOUSLY --
 * every call takes the slice of it that the time since the previous call is
 * worth, clamped to BB_SYNC_PPM of that same time. The clamp is what makes
 * it a slew rather than a jump, and a jump on this clock skips or repeats a
 * scene outright.
 *
 * WHICH MAKES `base` THE ONE THING THAT MUST NOT MOVE. A slew takes out at
 * most five per cent and leaves the rest standing as error for the next look
 * to carry, so the servo only ever closes a gap ACROSS calls: it is
 * `base` that remembers the part not yet paid. Anything that re-pegs it
 * discards that remainder, and a servo that can only trim RATE and never
 * remove accumulated PHASE does not converge at all -- the picture and the
 * music settle a whole stall apart and stay there. Exactly two things may
 * re-peg it, and each is a phase that is genuinely new rather than an error:
 * a track that has started or restarted, and the scene clock's int turnover.
 * A THIRD ONE IS A BUG however good its reason looks, and it will not show as
 * a bad frame -- it shows as the demo finishing seconds away from its music.
 *
 * FIVE PER CENT, AND THE FLOOR UNDER THAT NUMBER IS A MEASUREMENT. The limit
 * has to exceed the STEADY rate the two clocks disagree at, or the servo
 * saturates and the gap resumes growing at whatever is left over: what is
 * rendered and what is played are not the same second, and the difference is
 * 1.2% on the emulated card this is tested against. Below that floor the
 * number is free, because an eye reads no tempo in a scene that runs a
 * twentieth fast while it catches up -- there is no pitch to give it away --
 * and the debug line's `err` is what says whether the budget was enough.
 *
 * THE PHASE THE TRACK STARTED WITH IS KEPT, not corrected to zero. What is
 * HEARD is behind what is RENDERED by whatever the rings below hold, and
 * this program cannot see that number; correcting the error to zero would
 * put the picture ahead of the sound by exactly the buffer it cannot
 * measure. The offset the machine began with is left alone and only its
 * growth is taken out.
 *
 * `sngtime` IS THE ONLY HONEST POSITION HERE. It is the player's own
 * timeline, advanced a tick at a time as the mixer renders, in 2^-10 of a
 * second and at whatever tempo the module asks for. song_progress() is the
 * wrong one: its rows-per-pattern is nominal, so its reading carries a skew
 * that is the MODULE's -- up to 63 thousandths on bb2.s3m, which at a track
 * length is seconds of phase that are not there. Read without the library's
 * lock on purpose: it is one aligned word, a reading one tick stale is
 * twenty milliseconds inside a servo that moves by two, and taking
 * MikMod_Lock here would park the render thread behind the real-time mixer.
 *
 * AND IT IS A STAIRCASE RATHER THAN A CLOCK, WHICH IS WHAT THE SERVO WOULD
 * OTHERWISE SATURATE ON. sngtime moves one whole mixer buffer at a time --
 * measured at the shipped quantum, a 90.0ms riser every 97.7ms, with 88.3% of
 * the mixer's ten-millisecond ticks advancing it by nothing at all. Read raw
 * it is therefore up to a whole riser of error that does not exist, and a
 * servo fed the raw reading works flat out on the QUANTISATION: measured,
 * 5.37 corrections a second of 7.74ms each against two clocks whose true
 * disagreement was 0.12%. So the reading is CARRIED FORWARD from the last
 * step actually seen, and AT MOST ONE RISER -- past that the player has not
 * merely paused between buffers, it has stopped, and an estimate that kept
 * climbing would hide the very stall the servo exists to pay for. What is
 * left is smoothed with an exponential average before any of it is paid.
 *
 * THE AVERAGE IS WHY THERE IS NO DEADBAND, and a deadband would not be free:
 * measured against the same jitter, a 2ms one buys no drift at all -- +0.030s
 * against +0.030s over 1200s -- and widens the frame interval peak to peak
 * from 7.462ms to 10.028ms. Forgiving a small error is forgiving the drift,
 * which is the bug this whole function exists to fix.
 */
#define BB_SYNC_US   200000	/* how often the error is looked at        */
#define BB_SYNC_PPM   50000	/* the most of any interval the trim bends */
#define BB_SYNC_ALPHA     10	/* hundredths of the error the average takes */

/*
 * The player's timeline in microseconds, or -1 when there is no music to
 * follow. Bounded by the scene clock's own wrap: both are microseconds in an
 * int and a track outlasting thirty-five minutes has already wrapped one.
 */
static int
sound_clock (void)
{
#ifdef HAVE_LIBMIKMOD
  /*
   * NOT Player_Active(), AND THAT IS THE WHOLE REASON THIS READS A FIELD.
   * This runs on every turn of the render loop, and every Player_* call
   * opens with MUTEX_LOCK(vars) -- the mutex MikMod_Update() holds on the
   * real-time mixer thread, a plain one with no priority inheritance. Asking
   * here would put the mixer behind the render thread whenever the render
   * thread holds it, which is the inversion this program's whole shape
   * exists to avoid. A player that has fallen inactive simply stops
   * advancing sngtime, and the caller re-pegs on a reading that went
   * backwards, so nothing needs to be asked.
   *
   * A MODULE THAT IS LOADED BUT NOT STARTED HAS NO POSITION, AND ITS ZERO IS
   * NOT ONE. `playing` is the difference, and it has to be a flag of ours
   * because sngtime cannot tell the two apart: Player_Load() leaves it at
   * zero and so does the first tick of the track. The window between them is
   * not small -- bb() loads bb.s3m at the top of stage 1 and scene1() reaches
   * play() twenty-four seconds of scene clock later -- and a standing zero
   * across it reads to the servo as a player that has fallen twenty-four
   * seconds behind, which it then slews the whole picture to catch.
   */
  if (!bbsound || module == NULL || !playing)
    return -1;
  return (int) (((long long) module->sngtime * 1000000) >> 10);
#else
  return -1;
#endif
}

static int songstart;

void
sound_sync (void)
{
  static int base, last, held, said, kept, on = -1;
  static int prev = -1;
  static int edge, edgeat, riser;	/* where sngtime last stepped, when, by */
  static int avg, trimmed;		/* the smoothed error, and the last trim */
  int music = sound_clock ();
  int now, est, want, cap, err, dt;

  /* Read once: this is called from bbupdate(), which runs every turn of
   * every scene's loop. */
  if (on < 0)
    on = getenv ("KDOS_BB_DEBUG") ? 1 : 0;

  /*
   * THE SERVO IS TIMED ON THE CLOCK IT HAS NOT YET BENT. `held` is the whole
   * of what this function has handed tl_slowdown_timer(), and nothing else in
   * the demo ever slows or resets scenetimer, so TIME + held is exactly what
   * __lookup_timer() answered -- the raw elapsed reading, which only ever
   * goes backwards on the int turnover below.
   *
   * TIME ITSELF WILL NOT DO, for the interval, for the average or for the
   * turnover. A clock the servo is bending runs up to BB_SYNC_PPM slow, so
   * BB_SYNC_US of it is not a fifth of a second, the average's time constant
   * is not the two seconds its cadence was chosen for, and the trim below is
   * a rate against a ruler that the trim itself is stretching. The turnover
   * test is worse: on the bent clock it fires on the servo's own output, and
   * each firing re-pegs `base`, throws away the error not yet paid and
   * forgives a stall for good -- after which the drift is bounded by nothing,
   * because it is the SINGLE largest stall that sets it.
   *
   * THIS AND sound_clock()'s STANDING ZERO ARE ONE TEST, measured on a
   * replica of this function over 280s of scene clock losing 85ms of audio
   * every 3s. With the raw reading here and the no-music flag there, +0.100s
   * of drift; timed on TIME instead, +11.980s. The turnover re-pegs exactly
   * once in a run that reaches it, driven across it at speed.
   */
  now = TIME + held;

  /*
   * HOW LONG THE TRIM BELOW IS PAYING FOR. It is the gap since the PREVIOUS
   * CALL and not since the last look, because the trim runs on every call:
   * the clamp is a rate, so what it is a rate of has to be the time that has
   * actually gone by since it last moved the clock.
   */
  dt = now - trimmed;
  trimmed = now;

  /*
   * A TRACK THAT HAS NOT STARTED, OR HAS RESTARTED, IS A NEW PHASE AND NOT
   * AN ERROR. Each module is loaded fresh and begins at zero, and bb3.s3m is
   * rewound in place when it falls inactive -- so a reading that went
   * backwards re-pegs rather than asking the servo to take out a whole
   * track. It is the RAW reading that decides, never the estimate below:
   * the estimate only ever climbs, so a rewind read through it is a track
   * running away rather than a track starting again. The correction already
   * made is KEPT: it is time the machine lost and the next track inherits
   * the machine.
   */
  if (music < 0 || prev < 0 || music < prev)
    {
      base = TIME - (music < 0 ? 0 : music);
      edge = music < 0 ? 0 : music;
      edgeat = now;
      riser = 0;
      avg = 0;
      last = now;
      prev = music;
      return;
    }
  prev = music;

  /*
   * A RAW CLOCK THAT WENT BACKWARDS IS A WRAP AND NOT AN ERROR. __lookup_timer()
   * builds its answer as 1000000 * whole seconds in an int, which turns over
   * at some thirty-five minutes -- a length `-loop` reaches -- and lands about
   * 4295 seconds behind. The phase either side of that is not comparable, so
   * it is taken again rather than handed to the servo as an hour of error.
   *
   * IT IS THE RAW READING THAT IS TESTED, so the only thing that can trip
   * this is the turnover: the servo's own trim is already in `held` and
   * cancels out of `now`.
   */
  if (now < last)
    {
      base = TIME - music;
      edge = music;
      edgeat = now;
      avg = 0;
      last = now;
      return;
    }

  /*
   * WHERE THE PLAYER WOULD BE IF IT MOVED SMOOTHLY, which is the position
   * the error has to be measured against -- see the staircase above. The
   * last step that was actually seen is carried forward at real time, and
   * NEVER PAST ONE RISER: the cap is what keeps a stall visible.
   *
   * THE RISER IS TAKEN FROM THE PLAYER AND NOT STATED HERE, because it is
   * the mixer's buffer and the buffer is the card's: it moves with the
   * quantum, and a number written down here would be right on one machine.
   * A first riser has not been seen yet, so the estimate is the raw reading
   * until the player has stepped twice.
   */
  if (music != edge)
    {
      int r = music - edge;

      if (r > 0 && r < BB_SYNC_US)
	riser = r;
      edge = music;
      edgeat = now;
    }
  est = now - edgeat;
  if (est > riser)
    est = riser;
  est += edge;

  /*
   * THE ERROR IS LOOKED AT ON THE INTERVAL AND PAID OUT BETWEEN THE LOOKS.
   * The interval is what gives the AVERAGE a fixed cadence to have a time
   * constant in: the loop's own cadence is anything from two milliseconds to
   * a scene change, and an average stepped once per call would have a memory
   * that changed with the screen size. BB_SYNC_ALPHA of a fifth of a second
   * is about two seconds of it -- longer than any riser, shorter than any
   * scene.
   *
   * POSITIVE MEANS THE PICTURE IS AHEAD, and tl_slowdown_timer() is
   * subtracted from the clock, so the sign carries straight through.
   */
  if (now - last >= BB_SYNC_US)
    {
      err = TIME - (base + est);
      if (err > kept || -err > kept)
	kept = err > 0 ? err : -err;
      avg += (err - avg) * BB_SYNC_ALPHA / 100;
      last = now;
    }

  /*
   * AND THE WHOLE OF THE AVERAGE OVER ONE INTERVAL, WHICH IS WHY THIS STILL
   * REMOVES PHASE. The rate is set so that an error standing still would be
   * gone in BB_SYNC_US; anything the clamp refuses to pay now is still
   * standing in `base` for the next look to find. A gentler rate leaves a
   * standing phase offset in proportion to it, and a servo that cannot
   * remove phase does not converge at all.
   *
   * THE CLAMP IS A RATE LIMIT AT BOTH ENDS, so no single step is more than
   * BB_SYNC_PPM of the time it is paying for: at a two-millisecond cadence
   * that is a hundred microseconds against a sixteen-millisecond frame, and
   * it cannot be seen. It is also what keeps TIME monotone, which bb.c's
   * frame gate depends on -- ninety-five per cent of a positive gap is still
   * a positive gap.
   */
  want = (int) ((long long) avg * dt / BB_SYNC_US);
  cap = (int) ((long long) dt * BB_SYNC_PPM / 1000000);
  if (want > cap)
    want = cap;
  else if (want < -cap)
    want = -cap;
  if (want)
    {
      tl_slowdown_timer (scenetimer, want);
      held += want;
    }

  /*
   * THE THROTTLE IS ON THE RAW READING TOO, for the reason the servo is: TIME
   * is the clock the trim is bending, so five seconds of it is not five
   * seconds and the line would come out at whatever rate the servo happens to
   * be running at. `now` goes backwards on the turnover and on nothing else,
   * so the second half of this test means what it says.
   */
  if (on && (now - said >= 5000000 || now < said))
    {
      int prog = song_progress ();

      said = now;

      /*
       * NO ARITHMETIC ON THE FIRST TWO, because the arithmetic would be
       * wrong. The track length they imply is not honest:
       * song_progress()'s rows-per-pattern is NOMINAL, so its reading is
       * skewed by an amount that is the MODULE's -- bb.s3m reads 1 to 14
       * thousandths high and bb2.s3m up to 63 low, and neither is a
       * constant. So the two raw numbers go out and the book carries what
       * the player should read at each of them, measured per track.
       *
       * `held` IS the arithmetic and it is exact: the whole of what the
       * servo has taken out since the demo began, which is the audio time
       * this machine has lost. A smooth climb is a clock running at the
       * wrong rate; a staircase is a stream that stopped and restarted.
       *
       * `err` is the phase the servo has NOT taken out yet, at its worst
       * since the last line, and it is the one number that says whether the
       * correction is keeping up: bounded means it is, and a figure that
       * climbs line after line means BB_SYNC_PPM is smaller than the rate
       * the two clocks disagree at and the demo is coming apart anyway.
       *
       * `worst gap` is the longest the mixer thread went unscheduled since
       * the last line. Longer than the ring below is silence; shorter costs
       * nothing, and the two look identical from here, which is why the
       * number is printed rather than a verdict.
       */
      fprintf (stderr,
	       "kdos-bb: sync  demo %7.2fs, player %4d/1000,"
	       " held %+8d us, err %6d us, worst gap %5ld us\n",
	       (TIME - songstart) / 1000000.0, prog, held, kept,
	       sound_worst_gap);
      kept = 0;
      sound_worst_gap = 0;
    }
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
      /* SET WITH THE START AND NEVER BEFORE IT. Until this line sound_clock()
       * answers "no music", so the servo holds its correction where it is
       * instead of chasing a position the player has not begun to advance. */
      playing = 1;
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
  /* CLEARED OUTSIDE THE GUARD, because it must be false whenever `module` is
   * not a started player -- including the `-nosound` path, where nothing
   * below runs at all. load_song() clears it through here before it loads the
   * next track. */
  playing = 0;
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
	  bbflush ();
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
	  bbflush ();
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
