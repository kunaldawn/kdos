# kdos-bb

The ASCII-art demo that ships with KDOS: a frozen hard fork of the AA-project's `bb`, rebranded
and fixed in place. It is on this page for two reasons — it is a program the system ships, and the
facts it establishes about the ASCII-art library and the module player **constrain anything else
built on either**.

```sh
kdos-bb
```

## What it is

Upstream's demo, imported wholesale and **never merged from again**. `KDOS-FORK` at the root of
the port records the upstream tarball and its checksum, and lists every change with its reason.

| | |
|---|---|
| Upstream | `bb` 1.3rc1, the AA-project demo |
| Licence | GPL-2.0, kept — **and the authors file with it**, because the demo's own credits scroll is the authors' work |
| Carried | The sources and headers the binary is actually built from, plus the three music tracks |
| Dropped | The entire build-configuration apparatus, and upstream's distribution notes |

**The build system went because it could not work here.** Upstream's configuration script probes
the compiler with a function definition in a style modern compilers reject, so it failed with the
thoroughly misleading "C compiler cannot create executables" and would have needed a handful of
suppressions to answer a question with one answer on this target. A static configuration header
states those answers and the build script calls the compiler.

**The rebranding is deliberate rather than incidental.** The demo is the AA-group's, and the
credits scroll, the greetings and the history stay exactly as they are.

## Four defects fixed in place

Two were found with sanitizers rather than by reading:

- **A buffer was cleared with the wrong element size** — allocated per one integer type and
  cleared per a wider one. The same size on the platform it was written for, twice the size on a
  modern one: the heap corrupts and a later stage aborts inside the allocator.
- **A scrolling buffer was moved with a copy that forbids overlap**, with source and destination
  overlapping by every row but one. One C library survived it; another is free not to.
- **A calling-convention attribute that is 32-bit-x86 only** was expanded on every declaration,
  warning on each.

The third is the frame rate and the fourth is the audio arrangement, both below.

## The demo caps its own frame rate

**A scene states a rate for its control and never for its picture.** One draw cost twenty-five
milliseconds on the hardware this was written for, so the loop paced itself and nothing in it had
to; on anything modern it draws as fast as the machine turns it. Measured in a fifty-column window,
**five to fifteen thousand frames a second**; measured in a session terminal, **seventeen megabytes
a second of escape sequences** — for a display that can show sixty frames, and with the demo and the
session each spending a core on it.

Past that point **the pseudo-terminal never empties**. Every write comes apart mid-frame, the
terminal composes its window from a screen it has only half received, and what reaches the person
is the top of one frame over the bottom of the one before it: an animation that updates in
horizontal bands and looks like it is lagging.

So the animation loop draws at most once every **fifteen milliseconds** — not sixteen and two
thirds, because a scene whose control runs at exactly sixty would land a hair inside an exact
sixty-frame budget every other turn and be halved to thirty. **The control keeps its own rate**: a
control handler is told how many intervals it covers, so a dropped frame moves nothing in the
animation, every scene still ends on the microsecond it always did, and the beats stay in step with
the music.

Measured across the same window of the same run, at a session grid of 192x54:

| | Before | After |
|---|---|---|
| Demo writes | 17 MiB/s | 0.09 MiB/s |
| Session reads | 17 MiB/s | 0.09 MiB/s |
| Session CPU | 44% of a core | 1.6% |
| Demo CPU | 100% of a core | 0.7% |
| Frames the display is sent | 62/s | 62/s |
| Cells the display is sent | 33,000/s | 33,000/s |

**The picture is identical and everything spent on it is gone.** That is the shape of the whole
class: a producer faster than the screen is not a faster animation, it is the same animation with a
torn frame.

## The animation is tuned to the music, and the scene clock follows the player

The scenes run to an **absolute deadline** — `timestuff()` computes its end as
`start + duration` and chains the starts from one peg at the top of each track, so a beat that
runs late is *skipped* and never stretched. The mixer runs on the **sound card's** clock. They
agree because the demo was composed that way, and measured at a 50x19 terminal they agree to the
frame:

| Block | Animation | Its track |
|---|---|---|
| Stage 1 and 2 | 287.74 s | `bb.s3m` 287.70 s |
| The credits and the beat after them | 111.50 s | `bb2.s3m` 111.50 s |
| The extro | paced by the player | `bb3.s3m` 278.60 s |

**The schedule does not depend on the terminal's size.** Every sweep that crosses the screen
divides a fixed budget by however many steps that width needs, so the same beats land on the same
millisecond at 50 columns and at 106.

**A skipped beat is paid for in frames, and the music cannot pay that way.** The module advances
one tick per tick *inside the mixer*, every sample it renders is written, and nothing here ever
seeks the player forward. So a moment the card spends not playing — silence after a late wake, a
stream re-prepared from empty after an underrun, a card whose own rate is not quite the one the
module was rendered at — is a moment the picture took and the music did not. Left alone that gap
only ever grows, and a listener hears the demo running away from the track.

**So the scene clock follows the player.** `sound_sync()` compares the two every 200 ms and puts
the error into `tl_slowdown_timer()`, which is subtracted from every later reading of the clock —
**at most 5% of the interval**, so the picture runs a touch slow or a touch fast and never jumps.
A jump would skip or repeat a scene outright.

**The limit has a measured floor.** It must exceed the rate the two clocks *steadily* disagree at,
or the servo saturates and the gap resumes growing at whatever is left over. What the player
renders and what the card plays are not the same second: measured on the emulated card this is
tested against, the module's own timeline runs **1.2% fast** against the audio that comes out of
it. Below that floor the number is free — a scene that runs a twentieth fast while it catches up
has no pitch to give it away.

**The phase each track started with is kept, not corrected to zero.** What is *heard* is behind
what is *rendered* by whatever the rings below hold, and the demo cannot see that number;
correcting to zero would put the picture ahead of the sound by exactly the buffer it cannot
measure. Only the growth is taken out.

The position it follows is the player's own `sngtime`, which advances a tick at a time at whatever
tempo the module asks for. `song_progress()` is the wrong clock for this: its rows-per-pattern is
nominal, so its reading carries a skew that belongs to the module — up to 63 thousandths on
`bb2.s3m`, which at a track length is seconds of phase that are not there.

## The closing text turns a page at a time

The extro places the document where the **player** is, not where a clock is, so it lands on its
last page as the track lands on its last pattern — at any mixer rate and on any terminal.

**It moves a screen at a time and never a line.** Each step of the position costs a **morph**, a
whole second of cross-fade between the old page and the new one. The document is some fifteen
screens against a track of four and a half minutes, so a line at a time asks for a line every
three quarters of a second: the next step begins before the last has landed, the page is
permanently in motion, and nothing on it can be read.

A screen at a time is one morph and then a **still page**. Measured at 106x33 — 24 visible rows of
a 365-line document — that is 16 turns over the track, **16.4 seconds a page** of which one second
is the morph. Silent, it is 150 seconds over the same 16 turns, and a capture shows the screen
unchanged for 122 of 159 seconds.

**The turns get one slot more than they need**, so the last page arrives early and is still there
while the track finishes. It is the page with the most to say and the only one nothing follows.

A shorter screen is more turns of less text, so the hold shrinks exactly as fast as the reading
does and no minimum has to be stated.

## The mixer runs on its own thread

**This is the fix, and the reasoning generalises.**

The module player's update call is a **pull** interface: it has to be called often enough to keep
the sound card's ring buffer full. Upstream called it from a short timer in the same group the
frame loop pumps — and that loop's caller has just returned from flushing a whole frame to the
terminal, **blocking while the terminal drains it**.

On a bare console that is invisible. Under a compositor the terminal is shaping a screenful of
cells and uploading a texture every frame, so the pseudo-terminal backs up, the flush sits in a
write, no timer runs, the ring empties, and the music stutters.

**Minimise the same window and it is perfect.** That is the tell that it was never a mixer problem
or a buffer-size problem. So the mixer gets a thread with its own clock and the render loop cannot
reach it.

### The locking rule that must go with it

**Do not wrap a library call in the library's own lock.** The obvious defensive pattern —
lock, call, unlock — is a **self-deadlock**: the library's mutexes are not recursive, and its
playback and update calls take that same lock themselves. The process stops dead with the demo
frozen mid-frame and one thread.

That internal locking is exactly what the library's thread-initialisation call promises. The
exported lock is for protecting *your own* access to the library's exported **variables**, which is
a different thing.

What the library does not cover is the fork's own module pointer, so stopping **joins the thread
before freeing the module**, and the main path joins before shutting the library down.

## The ring buffer stays at upstream's size

**It is the margin, not the bug.** A shorter ring lowers the delay between a sample being mixed
and being heard, and buys that by having less slack when something starves the feeder. The feeder
was what was wrong, and the mixer thread is the fix; shrinking the ring on top of it would trade a
delay nobody can point at for a crackle everybody can hear.

There is no runtime lever either — the audio driver hardcodes the buffer time and its
command-line hook is an empty function.

## Three library facts that outlive this program

State these as rules for anything else built on the same libraries.

**The ASCII-art library's console driver writes cells into a device node an ordinary user cannot
open.** Its automatic initialisation answers a failed *recommended* driver by sweeping its own
driver list — landing on the plain-output driver, which scrolls a fresh block of text up the
terminal every frame. **Anything on this library must recommend the curses driver as well as the
console one.**

**Register all the module loaders, not just one format.** The public-domain music available online
is spread across several tracker formats; with a single loader registered, a track fails inside the
load call and the program plays silence.

**And the library does no pacing of its own.** Its flush writes whatever is in the text buffer,
every time it is called; nothing in it knows what a screen refresh is. **Anything on this library
owns its own frame cap**, or it writes at the rate its own arithmetic happens to run at.

## Audio on a bare console

Two stacked requirements, both in init scripts, and neither is about this program specifically:

- **Device coldplug must replay devices as additions.** The default replays every device as a
  *change*, and the rule that loads a driver from a device alias skips anything that is not an
  addition — so a plain trigger loads **no module at all**, the audio controller stays unclaimed,
  and the sound library reports an unknown device.
- **Sound state must fall back to initialising when restoring fails.** A live image has no saved
  state, and a failed restore leaves the hardware exactly as the kernel did: muted at zero. The
  initialise call returns a distinct status when it matched a generic rule, which is a success
  here, so its status is deliberately ignored.

**The console has a sound server, and reaching it is a configuration file rather than a given.**
`kdos-con-start` starts PipeWire exactly as the graphical session does, but ALSA only routes
`default` to it because `/etc/alsa/conf.d/99-kdos-pipewire.conf` says so — the directory PipeWire
installs its own drop-in into is not one alsa-lib reads. See [the session](../03-architecture/session.md#audio).

That matters to this demo more than to most: libmikmod opens the literal PCM name `default` and
parses no options, so it goes wherever that name points. On the card chain it holds the device
exclusively, which locks every other program and the daemon out of it for as long as the demo runs.
On a login with no session and therefore no daemon, `KDOS_ALSA_DEFAULT=kdos_card kdos-bb` is the
way to play at all.

## Debugging

```sh
KDOS_BB_DEBUG=1 kdos-bb
```

Reports which way the mixer is being fed — thread or timer — and whether the module loaded at all.
Silent otherwise, because the demo's error output is the terminal it is drawing on.

That trace is what caught the self-deadlock above: the log stopped at the line before playback
started.

**And every five seconds it reports the demo's clock against the player's position**, with what
the servo has had to take out and how long the mixer thread went unscheduled:

```
kdos-bb: sync  demo   80.01s, player  278/1000, held  -829016 us, err   1620 us, worst gap 10102 us
```

**`held` is the audio time this machine has lost**, summed since the demo began — the exact figure
the scene clock has been slowed by to stay with the player. A smooth climb is a card running at
the wrong rate; a staircase is a stream that stopped and restarted, and each step is one stutter.

**`err` is the phase the servo has not taken out yet**, at its worst since the previous line, and
it is the one number that says whether the correction is keeping up. Bounded means it is; a figure
that climbs line after line means the slew limit is below the rate the two clocks disagree at and
the demo is coming apart regardless.

**`worst gap` is the longest the mixer thread went unscheduled** since the previous line. Longer
than the ring below it is silence and shorter costs nothing at all, and the two look identical
from the render loop — so the number is printed rather than a verdict. It is recorded on the mixer
thread and printed on the render thread: a real-time thread that writes to the terminal it is
drawing on parks the highest-priority thread in the process behind the lowest.

The demo's error output is the terminal it is drawing on, so redirect it:

```sh
KDOS_BB_DEBUG=1 kdos-bb 2>/tmp/sync.log
```

**What the player should read.** Not a formula — `song_progress()`'s rows-per-pattern is nominal,
so its reading is skewed by an amount that belongs to the *module*: `bb.s3m` reads high, `bb3.s3m`
reads low and shrinking, `bb2.s3m` reads low and growing. These are what an offline render of each
track reports at the same point, so they are what a correctly playing one reports too:

| Demo seconds into the track | 20 | 40 | 60 | 80 | 100 | 140 | 180 | 220 | 260 |
|---|---|---|---|---|---|---|---|---|---|
| `bb.s3m` — stage 1 and 2 | 83 | 151 | 218 | 292 | 353 | 495 | 636 | 771 | 906 |
| `bb2.s3m` — the credits | 166 | 333 | 500 | 666 | 834 | — | — | — | — |
| `bb3.s3m` — the extro | 59 | 132 | 205 | 278 | 350 | 496 | 641 | 787 | 932 |

**A player reading above its row is ahead of the animation; one reading below is behind it.**
Measured on the shipped image with sound, the extro reported 60 at 20 s and 278 at 80 s against
59 and 278 — in step to the thousandth.

## There is no KDOS demoscene

A from-scratch demo was written for this project and then removed at the maintainer's request. It
is not coming back, and a stale reference to one is a leftover rather than a plan. This fork is
what ships.

## See also

- [Decisions](../01-philosophy/decisions.md) — why a fork rather than writing one
- [Administration](../02-user-guide/administration.md) — audio on the host
- [Boot and init](../03-architecture/boot-and-init.md) — the init scripts named above
- [Testing](../05-developer/testing.md) — the rig flag that gives a guest a sound device
