# kdos-bb

`kdos-bb` is the ASCII-art demonstration KDOS ships: a hard fork of the AA-project's `bb`, frozen
and maintained in place. It plays an audio-visual demo in a text terminal, on a bare console or
under the compositor.

It is documented here for two reasons. It is a program the system ships, and the facts it
establishes about the ASCII-art library, the module player and terminal frame pacing constrain
anything else built on any of them.

## Synopsis

```
kdos-bb [options] [1|2|3]
```

A bare `kdos-bb` starts immediately: music is on and the mixer takes its defaults, so there is
nothing to answer before the demo runs. A trailing `1`, `2` or `3` starts at that stage.

| Option | Does |
|---|---|
| any unrecognised argument | Prints the summary of options and exits 1. There is no `-help` flag: the parser takes `-loop`, `-nosound`, `-mixer`, aalib's own options and a stage digit, and everything else falls to the usage line |
| `-loop` | Play in an infinite loop |
| `-nosound` | Run silent. With no player to ask, the closing scroll falls back to a fixed rate instead of following the music |
| `-mixer` | Show the sample rate and mixing settings, and wait for Continue |
| `-driver`, `-kbddriver`, `-mousedriver` | Select an aalib driver |
| `-width`, `-height`, `-min*`, `-max*`, `-rec*` | Geometry hints for the art library |
| `-dim`, `-bold`, `-reverse`, `-normal`, `-boldfont`, `-no<attr>` | Which character attributes the renderer may use |
| `-extended`, `-eight` | Use all 256 characters; use eight-bit ASCII |
| `-font <font>` | Name the console font where the library cannot determine it |
| `-inverse`, `-noinverse`, `-bright`, `-contrast`, `-gamma` | Image controls |
| `-nodither`, `-floyd_steinberg`, `-error_distribution`, `-random` | Dithering |

`man kdos-bb` carries the full list.

## Description

Upstream's demo, imported wholesale and never merged from again. `KDOS-FORK` at the root of the
port records the upstream tarball and its checksum, and lists every change with its reason.

| | |
|---|---|
| Upstream | `bb` 1.3rc1, the AA-project demo |
| Licence | GPL-2.0, kept — and the authors file with it, because the demo's own credits scroll is the authors' work |
| Carried | 44 C files, 16 headers, and the three music tracks |
| Dropped | The entire build-configuration apparatus, and upstream's distribution notes |

The build system is gone because it cannot work here. Upstream's configuration script probes the
compiler with a function definition in a style modern compilers reject, so it fails with the
thoroughly misleading "C compiler cannot create executables" and would need a handful of
suppressions to answer a question with one answer on this target. A static configuration header
states those answers and `build.sh` calls the compiler.

The rebranding is deliberate rather than incidental. The demo is the AA-group's, and the credits
scroll, the greetings and the history stay exactly as they are.

KDOS ships no demo of its own. This fork is the demo.

### Three memory rules the fork holds

Each is a constraint on the C, and each fails loudly on a modern toolchain and quietly on the one
this code was written for:

- **A buffer is cleared at the element size it was allocated at.** Allocating per one integer type
  and clearing per a wider one is the same size on the platform this was written for and twice the
  size on a modern one: the heap corrupts and a later stage aborts inside the allocator.
- **An overlapping move uses the overlapping-safe copy.** A scrolling buffer overlaps by every row
  but one. One C library survives the non-overlapping form; another is free not to.
- **A 32-bit-x86 calling-convention attribute is not expanded elsewhere.** Expanded on every
  declaration it warns on each.

Two of the three are found by sanitizers rather than by reading, so build the port under them
before trusting a change to any buffer here.

## The demo caps its own frame rate

A scene states a rate for its *control* and never for its *picture*. One draw cost twenty-five
milliseconds on the hardware this was written for, so the loop paced itself and nothing in it had
to; on anything modern it draws as fast as the machine turns it. Measured in a fifty-column window,
five to fifteen thousand frames a second; measured in a session terminal, seventeen megabytes a
second of escape sequences — for a display that can show sixty frames, and with the demo and the
session each spending a core on it.

Past that point the pseudo-terminal never empties. Every write comes apart mid-frame, the terminal
composes its window from a screen it has only half received, and what reaches the person is the top
of one frame over the bottom of the one before it: an animation that updates in horizontal bands
and looks like it is lagging.

So the animation loop draws at most once every sixteen milliseconds, and the cap is a **deadline**
rather than a delay. That is the whole of what makes it a cap rather than a brake.

Timed from the end of one draw to the start of the next, the period is sixteen milliseconds plus
the draw, and the draw grows with the screen: 0.46 ms at 80x25 against 1.35 ms at 240x67. So the
period grows with the screen too, and it grows past the one thing it has to stay under —
`1000000 / 60`, the interval fourteen scenes state for their control. A scene in *waitmode* draws
only on a turn its control fired, so a period longer than the control's refuses every other tick
outright, and the next chance is a whole interval later. Measured over forty seconds of the same
scene in a session terminal:

| Session grid | End of draw to start of next | Deadline to deadline |
|---|---|---|
| 80x25 | 59.1 fps, 0.2% of frames held ≥25 ms | **60.6 fps**, 0.1% |
| 106x33 | 49.1 fps, **23.5%** | **60.6 fps**, 0.1% |
| 240x67 | 31.0 fps, **93.9%** | **60.6 fps**, 0.1% |

The judder is bimodal, and its size-dependence is the tell. At 106x33 there is nothing at all
between 18 ms and 30 ms: every frame is either 16.7 ms or 33.3 ms. Nor is it a shortage — one draw
is 1.35 ms against a 16 ms budget and the whole loop is under three per cent of one core. It is a
gate, not a cost.

### Why sixteen milliseconds

The grid must stay under the control grid and at or under every consumer's floor. The 666 µs it
leaves below `1000000 / 60` is the margin, and what the margin buys is that the deadline is never
*ahead* of the control tick it is checked against: a grid shorter than the control's falls 666 µs
further behind the tick every frame, and is never re-pegged closer than 666 µs behind. A tick is
refused only after a stall, never for the jitter of noticing one.

The consumer states no such constant at all. `kdos-term`'s draw is gated on the compositor's frame
callback, so its floor is whatever the output is wearing — 16.667 ms at sixty hertz. A frame
produced faster than the floor is bytes the consumer must read and parse for a picture nobody
sees.

Producing faster than the consumer is a beat, not extra smoothness. The surplus does not drop one
frame in sixteen quietly; it beats at the difference, and a beat is what an eye reads as judder. A
scene in waitmode is paced by its control and runs at sixty, which beats against neither consumer.
A scene stating a *positive* rate is not gated by its control at all — it draws on every turn — so
its picture runs at exactly the cap: 62.5, which is two and a half frames a second against a
sixty-hertz output and is the price of the margin above. Further under is more surplus; further
over is the halved rate. The number is not free in either direction.

A deadline more than a whole period behind is moved rather than chased. A stall leaves the grid
arbitrarily far behind, and catching up would draw every missed frame back to back: the
pseudo-terminal fills, the frames tear, and what the stall cost is paid twice. One frame is drawn
late and the grid restarts from the deadline that was met.

Nothing tests the gap for going backwards. The scene clock is an `int` of microseconds that turns
over at some thirty-five minutes, which `-loop` reaches, and the difference of two readings either
side of the turnover is modular — it comes out as the small number it really is. Driven across the
turnover the frame rate is unbroken at 61.8 fps with no gap over 18 ms. A test for a negative gap
would catch nothing but the servo below, which cannot produce one either.

The control keeps its own rate: a control handler is told how many intervals it covers, so a
dropped frame moves nothing in the animation, every scene still ends on the microsecond it always
did, and the beats stay in step with the music.

Measured across the same window of the same run, at a session grid of 192x54:

| | Uncapped | Capped |
|---|---|---|
| Demo writes | 17 MiB/s | 0.09 MiB/s |
| Session reads | 17 MiB/s | 0.09 MiB/s |
| Session CPU | 44% of a core | 1.6% |
| Demo CPU | 100% of a core | 0.7% |
| Frames the display is sent | 62/s | 62/s |
| Cells the display is sent | 33,000/s | 33,000/s |

The picture is identical and everything spent on it is gone. That is the shape of the whole class:
a producer faster than the screen is not a faster animation, it is the same animation with a torn
frame.

## Every frame is bracketed in synchronized output

A frame cannot be made atomic, which is exactly why the bracket is the fix. The ASCII-art library
hands the picture to the curses library's refresh, and refresh writes one write per screen row —
21 writes and 2.0 KB at 75x19, 69 writes and 17.2 KB at 236x63. A pseudo-terminal holds 12288
bytes, so at a full screen the frame is larger than the pipe it crosses and cannot cross in one
piece however the program is written. A consumer composing on its own clock therefore reads a
screen that is half this frame and half the last one: modelled against a consumer composing on its
own sixty-hertz clock, 7.5% of composes show a torn frame and between eighteen and twenty-four per
cent of frames are never shown whole.

So the demo says where a frame begins and ends. Each frame is written between `CSI ? 2026 h` and
`CSI ? 2026 l` — synchronized output. Between the set and the reset a terminal keeps showing the
screen it already had and composes nothing it receives, so the rows arrive in as many writes as
they like and the picture changes once, whole.

This is a contract with two ends, and one end alone changes nothing. The producer brackets its
frames; the consumer honours the mode. `kdos-term` honours it through `libkvt`'s
`kvt_term_sync_hold()`, and any other consumer of the vte honours it the same way — both under a
150 ms watchdog, because a producer that sets the hold and then dies must not freeze the window.

It degrades safely in both directions. A terminal that never sets a hold composes exactly as it
does without the mode; a hold that outlives the watchdog composes anyway; and a terminal that has
never heard of the mode ignores a private mode it does not know.

The escape goes out on the same stream as the frame, which is why it is standard output and why
both ends are flushed: the library's curses driver is initialised on standard output, so the
refresh writes to that very stream. An escape sent down any other descriptor arrives in an order
nothing defines, and a hold that lands *after* the rows it was meant to cover is worse than none. A
standard output that is not a terminal is not bracketed at all.

Measured on a captured pseudo-terminal at 236x63 — frames that exceed the 12288-byte pipe — over
twelve seconds: 741 frames, 100.00% of the bytes inside a bracket, zero nesting errors, biggest
bracketed frame 20178 bytes. Every frame goes through one wrapper and nothing calls the library's
flush directly.

## The animation is tuned to the music

The scenes run to an absolute deadline. `timestuff()` computes its end as `start + duration` and
chains the starts from one peg at the top of each track, so a beat that runs late is *skipped* and
never stretched. The mixer runs on the sound card's clock. They agree because the demo was composed
that way, and measured at a 50x19 terminal they agree to the frame:

| Block | Animation | Its track |
|---|---|---|
| Stage 1 and 2 | 287.74 s | `bb.s3m` 287.70 s |
| The credits and the beat after them | 111.50 s | `bb2.s3m` 111.50 s |
| The extro | paced by the player | `bb3.s3m` 278.60 s |

The schedule does not depend on the terminal's size. Every sweep that crosses the screen divides a
fixed budget by however many steps that width needs, so the same beats land on the same millisecond
at 50 columns and at 106.

A skipped beat is paid for in frames, and the music cannot pay that way. The module advances one
tick per tick inside the mixer, every sample it renders is written, and nothing here ever seeks the
player forward. So a moment the card spends not playing — silence after a late wake, a stream
re-prepared from empty after an underrun, a card whose own rate is not quite the one the module was
rendered at — is a moment the picture took and the music did not. Left alone that gap only grows,
and a listener hears the demo running away from the track.

### The servo

The scene clock follows the player. `sound_sync()` compares the two every 200 ms and puts the error
into `tl_slowdown_timer()`, which is subtracted from every later reading of the clock.

The correction is a rate and never a lump, because the frame loop is paced off the same clock. A
correction handed over in one piece lands inside *one* frame interval, and that frame is as long as
the piece: ten milliseconds against a sixteen-millisecond budget is a frame taking twenty-seven. So
the error is looked at every 200 ms and paid out continuously — every call takes the slice of it
that the time since the previous call is worth, clamped to 5% of that same time. The clamp is what
makes it a slew rather than a jump; a jump on this clock skips or repeats a scene outright.

The player's position is a staircase and not a clock, and a servo fed the raw reading works flat
out on the quantisation rather than on the disagreement. `sngtime` moves one whole mixer buffer at
a time — measured at the shipped quantum, a 90.0 ms riser every 97.7 ms, with 88.3% of the mixer's
ten-millisecond ticks advancing it by nothing at all. Fed that raw, a replica applied 5.37
corrections a second of 7.74 ms each against two clocks whose true disagreement was 0.12%. So the
reading is carried forward from the last step actually seen, and at most one riser: past that the
player has not paused between buffers, it has stopped, and an estimate that kept climbing would
hide the very stall the servo exists to pay for. What is left is smoothed with an exponential
average — 10% of the error per look, about two seconds of memory — before any of it is paid.

There is no deadband and one would not be free. Forgiving small errors is forgiving the drift,
which is the whole of what this function exists to fix. The average does the smoothing without
giving anything up; a 2 ms deadband on top of it buys no drift at all and costs peak-to-peak in the
picture.

The slew limit has a measured floor. It must exceed the rate the two clocks *steadily* disagree at,
or the servo saturates and the gap resumes growing at whatever is left over. What the player
renders and what the card plays are not the same second: measured on the emulated card this is
tested against, the module's own timeline runs 1.2% fast against the audio that comes out of it.
Below that floor the number is free — a scene that runs a twentieth fast while it catches up has no
pitch to give it away.

The phase each track started with is kept, not corrected to zero. What is *heard* is behind what is
*rendered* by whatever the rings below hold, and the demo cannot see that number; correcting to
zero would put the picture ahead of the sound by exactly the buffer it cannot measure. Only the
growth is taken out.

The servo removes accumulated phase and not only rate, and that turns on one variable. The rate is
set so that an error standing still would be gone in one 200 ms interval, but the clamp refuses to
pay more than 5% of any stretch of time — so whenever the error is larger than that, the rest is
left standing, and it is `base`, the peg the error is measured from, that carries the unpaid
remainder. Exactly two things may move it, each a phase that is genuinely new rather than an error:

- **A track that has started or restarted**, which is the `music < prev` reading — `bb3.s3m` is
  rewound in place when it falls inactive.
- **The scene clock's integer turnover**, which `__lookup_timer()` reaches at some thirty-five
  minutes because it builds its answer as `1000000 ×` whole seconds in an `int`. `-loop` reaches
  it, and the phase either side is not comparable.

Anything else that re-pegs `base` is a defect however good its reason looks, and it does not
present as a bad frame — it presents as the demo finishing seconds away from its music. Two shapes
are easy to write and both do exactly that:

- **Measuring anything on `TIME`.** The interval, the average's time constant and the turnover test
  are all on the **raw** reading — `TIME` plus everything the servo has handed
  `tl_slowdown_timer()`, which is what `__lookup_timer()` answered and goes backwards on nothing but
  the turnover. A clock the servo is bending is a ruler the servo is stretching: the trim is a rate,
  and measured against a ruler running 4% slow it under-pays by exactly the rate it is correcting.
  The turnover test is worse on the bent clock, because each spurious firing re-pegs `base`, throws
  away the error not yet paid and forgives a stall for good — after which the drift is bounded by
  nothing, since it is the *single largest* stall that sets it.
- **Letting a loaded module answer zero.** `Player_Load()` leaves `sngtime` at zero and so does the
  first tick of a track, so `sound_clock()` carries a flag of its own and answers **no music** until
  `play()` has started the player. The window is not small: `bb.s3m` is loaded at the top of stage 1
  and `scene1()` reaches `play()` twenty-four seconds of scene clock later, and a standing zero
  across it reads as a player twenty-four seconds behind — which the servo then slews the whole
  picture to catch.

Measured on a replica over 280 s of scene clock losing 85 ms of audio every 3 s, with a draw cost
of 1.35 ms:

| `sound_sync()` | fps | Frame interval, s.d. | Peak to peak | Doubled frames | Corrections | Drift |
|---|---|---|---|---|---|---|
| One clamped lump every 200 ms | 56.4 | 1.802 ms | 11.385 ms | 1.81/s | 4.9/s of 9883 µs | **+0.100 s** |
| Looked at every 200 ms, paid continuously | **60.0** | **0.377 ms** | **5.773 ms** | **0.00/s** | 258.0/s of 162 µs | **+0.100 s** |
| …the same, but with a 2 ms deadband | 60.0 | 0.402 ms | **18.941 ms** | 0.00/s | 258.8/s of 161 µs | +0.101 s |
| …the same, but measured on `TIME` | 62.7 | 0.571 ms | 5.384 ms | 0.00/s | 187.6/s of 159 µs | **+11.980 s** |

The drift is identical and the picture is four to five times steadier, which is the only shape of
the answer worth having. The deadband buys nothing and loses peak-to-peak; the bent clock loses
twelve seconds.

The drift is bounded rather than merely small. The same run at 1200 s: one lump every 200 ms,
+0.030 s at 56.4 fps with s.d. 1.809 ms; looked at and paid continuously, +0.030 s at 60.0 fps with
s.d. 0.379 ms; with a 2 ms deadband, +0.030 s with peak-to-peak 10.028 ms against 7.462 ms. What
varies between runs is the phase the track started at, which is one quantum and is kept on purpose.

The correction is robust to what it is fed: across a quantum sweep from 0 to 170 ms the frame
interval's s.d. stays between 0.371 ms and 0.405 ms, and a 500 ms stall every 30 s gives 0.481 ms
with the same drift.

The position it follows is the player's own `sngtime`, which advances a tick at a time at whatever
tempo the module asks for. `song_progress()` is the wrong clock for this: its rows-per-pattern is
nominal, so its reading carries a skew that belongs to the module — up to 63 thousandths on
`bb2.s3m`, which at a track length is seconds of phase that are not there.

The animation's own schedule is untouched by any of it. Measured under a pseudo-terminal with the
mixer off, stage 1 and 2 reach the credits at 311.740156 s against 311.740147 s: nine microseconds
over five minutes.

## The closing text turns a page at a time

The extro places the document where the *player* is, not where a clock is, so it lands on its last
page as the track lands on its last pattern — at any mixer rate and on any terminal.

It moves a screen at a time and never a line. Each step of the position costs a morph, a whole
second of cross-fade between the old page and the new one. The document is some fifteen screens
against a track of four and a half minutes, so a line at a time asks for a line every three
quarters of a second: the next step begins before the last has landed, the page is permanently in
motion, and nothing on it can be read.

A screen at a time is one morph and then a still page. Measured at 106x33 — 24 visible rows of a
365-line document — that is 16 turns over the track, 16.4 seconds a page of which one second is the
morph. Silent, it is 150 seconds over the same 16 turns, and a capture shows the screen unchanged
for 122 of 159 seconds.

The turns get one slot more than they need, so the last page arrives early and is still there while
the track finishes. It is the page with the most to say and the only one nothing follows.

A shorter screen is more turns of less text, so the hold shrinks exactly as fast as the reading
does and no minimum has to be stated.

## The mixer runs on its own thread

The module player's update call is a **pull** interface: it has to be called often enough to keep
the sound card's ring buffer full. Calling it from a short timer in the same group the frame loop
pumps puts it behind a caller that has just returned from flushing a whole frame to the terminal,
blocking while the terminal drains it.

On a bare console that is invisible. Under a compositor the terminal is shaping a screenful of
cells and uploading a texture every frame, so the pseudo-terminal backs up, the flush sits in a
write, no timer runs, the ring empties, and the music stutters. Minimise the same window and it is
perfect, which is the tell that it is neither a mixer problem nor a buffer-size problem.

So the mixer gets a thread with its own clock, and the render loop cannot reach it.

### The locking rule that goes with it

Do not wrap a library call in the library's own lock. The obvious defensive pattern — lock, call,
unlock — is a self-deadlock: the library's mutexes are not recursive, and its playback and update
calls take that same lock themselves. The process stops dead with the demo frozen mid-frame and one
thread.

That internal locking is exactly what the library's thread-initialisation call promises. The
exported lock is for protecting *your own* access to the library's exported **variables**, which is
a different thing.

What the library does not cover is the fork's own module pointer, so stopping joins the thread
before freeing the module, and the main path joins before shutting the library down.

### The ring buffer stays at upstream's size

It is the margin, not the defect. A shorter ring lowers the delay between a sample being mixed and
being heard, and buys that by having less slack when something starves the feeder. The feeder is
what goes wrong, and the mixer thread is what fixes it; shrinking the ring on top of that would
trade a delay nobody can point at for a crackle everybody can hear.

There is no runtime lever either — the audio driver hardcodes the buffer time and its command-line
hook is an empty function.

## Three library facts that outlive this program

State these as rules for anything else built on the same libraries.

- **Recommend the curses driver as well as the console one.** The ASCII-art library's console
  driver writes cells into a device node an ordinary user cannot open, and its automatic
  initialisation answers a failed *recommended* driver by sweeping its own driver list — landing on
  the plain-output driver, which scrolls a fresh block of text up the terminal every frame.
- **Register all the module loaders, not one format.** The public-domain music available online is
  spread across several tracker formats; with a single loader registered, a track fails inside the
  load call and the program plays silence.
- **Own your own frame cap.** The library does no pacing: its flush writes whatever is in the text
  buffer, every time it is called, and nothing in it knows what a screen refresh is. A program that
  states no cap writes at the rate its own arithmetic happens to run at.

## Audio on a bare console

Two stacked requirements, both in init scripts, and neither is about this program specifically:

- **Device coldplug must replay devices as additions.** The default replays every device as a
  *change*, and the rule that loads a driver from a device alias skips anything that is not an
  addition — so a plain trigger loads no module at all, the audio controller stays unclaimed, and
  the sound library reports an unknown device.
- **Sound state must fall back to initialising when restoring fails.** A live image has no saved
  state, and a failed restore leaves the hardware exactly as the kernel did: muted at zero. The
  initialise call returns a distinct status when it matched a generic rule, which is a success
  here, so its status is deliberately ignored.

The console has a sound server, and reaching it is a configuration file rather than a given.
`kdos-desktop-start` starts PipeWire, but ALSA only routes `default` to it because
`/etc/alsa/conf.d/99-kdos-pipewire.conf` says so: the directory PipeWire installs its own drop-in
into is not one alsa-lib reads. See [the session](../03-architecture/session.md#audio).

That matters to this demo more than to most. libmikmod opens the literal PCM name `default` and
parses no options, so it goes wherever that name points. On the card chain it holds the device
exclusively, which locks every other program and the daemon out of it for as long as the demo runs.
On a login with no session and therefore no daemon, `KDOS_ALSA_DEFAULT=kdos_card kdos-bb` is the
way to play at all.

## Debugging

```sh
KDOS_BB_DEBUG=1 kdos-bb 2>/tmp/sync.log
```

The demo's error output is the terminal it is drawing on, so redirect it.

The trace reports which way the mixer is being fed — thread or timer — and whether the module
loaded at all. It is silent otherwise. It is also what catches the self-deadlock described above:
the log stops at the line before playback starts.

Every five seconds it reports the demo's clock against the player's position, with what the servo
has had to take out and how long the mixer thread went unscheduled:

```
kdos-bb: sync  demo   80.01s, player  278/1000, held  -829016 us, err   1620 us, worst gap 10102 us
```

**`held` is the audio time this machine has lost**, summed since the demo began — the exact figure
the scene clock has been slowed by to stay with the player. A smooth climb is a card running at the
wrong rate; a staircase is a stream that stopped and restarted, and each step is one stutter.

**`err` is the phase the servo has not taken out yet**, at its worst since the previous line, and
it is the one number that says whether the correction is keeping up. Bounded means it is; a figure
that climbs line after line means the slew limit is below the rate the two clocks disagree at and
the demo is coming apart regardless.

**`worst gap` is the longest the mixer thread went unscheduled** since the previous line. Longer
than the ring below it is silence and shorter costs nothing at all, and the two look identical from
the render loop — so the number is printed rather than a verdict. It is recorded on the mixer
thread and printed on the render thread: a real-time thread that writes to the terminal it is
drawing on parks the highest-priority thread in the process behind the lowest.

### What the player should read

Not a formula. `song_progress()`'s rows-per-pattern is nominal, so its reading is skewed by an
amount that belongs to the *module*: `bb.s3m` reads high, `bb3.s3m` reads low and shrinking,
`bb2.s3m` reads low and growing. These figures are what an offline render of each track reports at
the same point, so they are what a correctly playing one reports too:

| Demo seconds into the track | 20 | 40 | 60 | 80 | 100 | 140 | 180 | 220 | 260 |
|---|---|---|---|---|---|---|---|---|---|
| `bb.s3m` — stage 1 and 2 | 83 | 151 | 218 | 292 | 353 | 495 | 636 | 771 | 906 |
| `bb2.s3m` — the credits | 166 | 333 | 500 | 666 | 834 | — | — | — | — |
| `bb3.s3m` — the extro | 59 | 132 | 205 | 278 | 350 | 496 | 641 | 787 | 932 |

A player reading above its row is ahead of the animation; one reading below is behind it. Measured
on the shipped image with sound, the extro reports 60 at 20 s and 278 at 80 s against 59 and 278 —
in step to the thousandth.

## See also

- [Decisions](../01-philosophy/decisions.md) — why a fork rather than writing one
- [Administration](../02-user-guide/administration.md) — audio on the host
- [Boot and init](../03-architecture/boot-and-init.md) — the init scripts named above
- [Testing](../05-developer/testing.md) — the rig flag that gives a guest a sound device
