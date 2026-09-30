# kdos-bb

`kdos-bb` is the audio-visual demo that ships with KDOS. It is a hard fork of `bb`, the
AA-project's 1997 demo: a sequence of animated scenes drawn entirely in text characters by AAlib,
the ASCII-art library, and set to three tracker modules played by libmikmod. A tracker module
stores music as instrument samples plus patterns of notes, and the player renders it into audio in
real time; all three tracks are in the S3M format (`.s3m`). It runs in any text terminal, whether
a bare virtual console, a `kdos-term` window under the compositor or an ssh session.

The chapter has two audiences. To watch the demo you need only [Running it](#running-it),
[Synopsis](#synopsis) and [Files](#files). The rest is for anyone who works on this program or
builds something else on the same foundations (AAlib for the picture, libmikmod for the music, a
pseudo-terminal between the program and the screen). It explains how the fork paces its frames,
how it keeps a frame from tearing, how it keeps the picture in step with the music, and the
measurements that set each limit. Start that part at
[How the code is organised](#how-the-code-is-organised), which says where each piece lives and
defines the terms the later sections use. The chapters on the
[session](../03-architecture/session.md#audio) (for audio routing) and on
[kdos-term](kdos-term.md) (for the terminal that honours synchronized output) are useful
background.

## Running it

```sh
kdos-bb                 # the whole demo, with music
kdos-bb 2               # start at stage 2: the credits
kdos-bb -loop           # play in an endless loop
kdos-bb -nosound        # silent
```

A bare `kdos-bb` starts at once. Music is on and the mixer takes its defaults, so there is nothing
to answer first. The demo has no launcher in the Start menu; run it from a terminal. The music plays
through whatever ALSA's `default` device names, which in a desktop session is PipeWire; see
[Audio on a bare console](#audio-on-a-bare-console) for a login with no session.

The demo is in three **stages**, one per soundtrack, and a digit on the command line picks where it
starts:

| Stage | Starts at | Plays |
|---|---|---|
| 1 | The beginning | The scenes and the four author biographies to `bb.s3m`, the credits to `bb2.s3m`, then the closing text to `bb3.s3m` |
| 2 | The credits | The credits, then the closing text |
| 3 | The closing text | Only the closing text |

Stage 2 enters the program at the third author biography with the skip flag already raised, so the
remaining scenes of the first block are passed over at once and the demo settles at the credits,
where the next track is loaded.

### Keys

| When | Key | Does |
|---|---|---|
| During the demo | `s`, `S`, `Backspace` | Skip to the next soundtrack: the rest of the current block is passed over |
| During the demo | `q`, `Esc` | Quit |
| In the closing text | Any key | Stops the pages turning by themselves; from then on you turn them |
| In the closing text | `b`, `k`, `↑`, `Backspace` (letters in either case) | Back half a screen |
| In the closing text | `f`, `j`, `Space`, `↓`, `←` (letters in either case) | Forward half a screen |
| In the closing text | `1`, `2`, `3` | Switch to the first, second or third soundtrack |
| In the closing text | `q`, `Q`, `Esc` | Fade out and leave |

Left alone, the closing text turns its own pages and fades out when the scroll is done (see
[The closing text turns a page at a time](#the-closing-text-turns-a-page-at-a-time)), and the
program exits.

## Synopsis

```
kdos-bb [aalib-options] [-loop] [-nosound] [-mixer] [N]
```

| Option | Does |
|---|---|
| `-loop` | Play in an endless loop from the chosen stage, never reaching the closing text (see below) |
| `-nosound` | Run silent. With no player to follow, the closing text turns its pages over a fixed 150 seconds instead of following the music |
| `-mixer` | Before starting, show libmikmod's mixing switches and sample rate and wait for Continue (see below) |
| `N` | A single digit, the stage to start at. `2` or `3` starts at that stage; `1`, and any other digit from `4` to `8`, starts at the beginning. `0` and `9` are unrecognised |
| `-driver`, `-kbddriver`, `-mousedriver` | Select an AAlib output, keyboard or mouse driver |
| `-width`, `-height`, `-minwidth`, `-minheight`, `-maxwidth`, `-maxheight`, `-recwidth`, `-recheight` | Geometry hints for AAlib |
| `-dim`, `-bold`, `-reverse`, `-normal`, `-boldfont`, `-no<attr>` | Which character attributes the renderer may use |
| `-extended`, `-eight` | Use all 256 characters; use eight-bit ASCII |
| `-font <font>` | Name the console font where AAlib cannot determine it |
| `-inverse`, `-noinverse`, `-bright <val>`, `-contrast <val>`, `-gamma <val>` | Image controls |
| `-nodither`, `-floyd_steinberg`, `-error_distribution`, `-random <val>` | Dithering |
| `-dimmul <val>`, `-boldmul <val>` | Brightness factors for dim and bold characters |

AAlib's own options are consumed first, and AAlib also reads the same options from the `AAOPTS`
environment variable. Anything left that is not one of the demo's options prints a usage summary
and exits with status 1. There is no `-help` flag as such: `kdos-bb -help` shows the summary by
being unrecognised. `man kdos-bb` lists every option.

The loop that `-loop` makes turns back before the closing text, so what it repeats depends on the
stage. From stage 1 it repeats everything up to and including the credits. From stage 2 the first
pass is the credits alone, and every later pass plays from the third author biography through the
credits, with `bb2.s3m` still the loaded track until the credits load it again; the skip flag that
stage 2 starts with is cleared when the credits load their track and is not raised again. From stage
3 the loop has nothing to play: it shows nothing, reads no keys, spins a core and never ends, and
it has to be stopped from outside, for example with `kill`.

The `-mixer` screen lists seven switches numbered `0` to `6` (16-bit output, stereo, the software
mixer, the high-quality mixer, surround, interpolation, reversed stereo), then `7` for the sample
rate and `8` for Continue. A digit toggles a switch; `7` steps through fourteen rates from 5512 to
48000 Hz. Without `-mixer` the demo mixes at 48000 Hz, the rate every sink on the image runs at, so
no rate converter sits in the mixer thread's path.

If the sound system cannot initialise, the demo prints `Sound initialization failed` on the screen
for a second and runs silent. If a module cannot be loaded it prints `Failed to load module` for a
second and that track is silent. Modules are looked for in `/usr/share/kdos-bb` first and then by
bare name in the working directory.

| Exit status | Meaning |
|---|---|
| 0 | The demo finished, or you quit it |
| 1 | An unrecognised argument; the summary was printed |
| 2 | AAlib could not initialise an output driver |
| 3 | AAlib could not initialise the keyboard |

## Files

| Path | What |
|---|---|
| `/usr/bin/kdos-bb` | The program |
| `/usr/share/kdos-bb/bb.s3m`, `bb2.s3m`, `bb3.s3m` | The three soundtrack modules |
| `/usr/share/man/man1/kdos-bb.1` | The manual page |
| `/usr/share/licenses/kdos-bb/COPYING`, `AUTHORS` | The licence and the upstream authors |

The port is `src/art/kdos-bb`, version 1.3.0. It depends on `aalib` (1.4rc5), `libmikmod`
(3.3.14) and `ncurses`, and it is built in the `41_system` phase, named in that phase's
`packages.d/src-art.txt` beside the other `src/art` ports. `aalib` is on the
[`image-libs` shelf](../06-reference/ports-catalogue.md#image-libs) of the ports catalogue; its
other consumers are `aview` and `gimp` on `graphics`, `gst-plugins-good` on `media-frameworks`
and `gphoto2` on `mobile`. How `src/art` joins the ports tree for that phase is in [How KDOS is built](../05-developer/how-kdos-is-built.md).

## The fork

The fork takes no merges from upstream. `KDOS-FORK` at the root of the port names the upstream
tarball and its checksum and lists every difference from it, file by file, with its reason. The
reasons for forking rather than writing a new demo are in
[Decisions](../01-philosophy/decisions.md#freezing-a-demo-rather-than-writing-one).

| | |
|---|---|
| Upstream | `bb` 1.3rc1, the AA-project demo |
| Licence | GPL-2.0, kept, with the authors file beside it, because the demo's own credits scroll is the authors' work |
| Carried | The C sources and headers the binary is built from (43 `.c` files and 15 headers from upstream, counted in the tree; with the two added files below, `src/` holds 44 and 16) and the three tracks under `data/` |
| Added | `KDOS-FORK`, `kpkgbuild`, `build.sh`, `src/aconfig.h`, the manual page, and `src/kdostux.c`, the KDOS mascot as a compressed greyscale image, generated by `genimg.py` (also added) on a development host and never by the build |
| Dropped | The whole autotools apparatus and upstream's distribution notes |

The upstream build system cannot work here. Its configuration script probes the compiler with a
K&R-style `main()` that GCC 14 rejects, so it fails with the misleading "C compiler cannot create
executables". A static configuration header, `src/aconfig.h`, states the answers for this target
(Linux, musl, x86-64) instead, and `build.sh` calls the compiler directly on the source list. The
demo's C relies on implicit declarations and untyped parameters throughout, which GCC 14 treats as
errors, so `build.sh` passes six `-Wno-` flags rather than rewriting the code.

The demo itself is the AA-project's: the credits scroll, the greetings and the author biographies are
theirs and are kept as they are. The fork differs from upstream in the following ways:

- It starts without asking. Upstream's music and mixer questions are the `-nosound` and `-mixer`
  flags.
- Frame pacing, synchronized output, the mixer thread, music synchronisation and the closing text
  are the fork's, each described in its own section below.
- It shows the KDOS mascot in two beats that already show a logo: the twelfth of the nineteen flash
  words in the opening, and the logo beat of the closing text. The mascot is drawn through the
  demo's own picture path, so it is scaled, dithered and strobed like the photographs.
- It rewords screens where the original text would be false on KDOS. Each keeps its original count
  of entries, because the count is the clock: each entry is a fixed slice of the soundtrack. The
  credits array stays at 166 entries; the fork's own credit takes the four slots after the last
  contributor.

## How the code is organised

The rest of this chapter is for contributors. The code is under `src/art/kdos-bb/src/`:

| File | What it holds |
|---|---|
| `bb.c` | Argument parsing, the stages, the keys during the demo, the frame loop `timestuff()` with its cap (`BB_FRAME_US`), and `bbflush()`, which writes each frame inside synchronized output |
| `main.c` | The music: loading and playing a module, the mixer thread, `sound_clock()` (where the player is), `sound_sync()` (the servo), `song_progress()` (thousandths through a module) and the `-mixer` screen |
| `timers.c` | The scene clock: `__lookup_timer()` reads it and `tl_slowdown_timer()` holds it back |
| `credits2.c` | The closing text, its scroll and its keys |
| `textform.c` | The text of the closing document |
| `scene*.c`, `credits.c`, `messager.c` | The individual scenes, the credits and the biography screens |
| `kdostux.c` | The mascot image, generated |

Terms used below:

- A **scene** is one part of the demo. `timestuff()` runs it for a fixed time with two callbacks:
  its **control**, which moves the animation on at a rate the scene states, and its **draw**, which
  puts the current picture on the screen.
- A scene in **waitmode** draws only on a turn of the loop where its control ran. A scene enters
  waitmode by stating a negative rate, and a scene with no control is put in waitmode at 40 a
  second whatever rate it states. A scene stating a positive rate with a control of its own is not
  gated by its control.
- The **scene clock** is the microsecond count every scene is timed against. `TIME` in `bb.c` is
  its current reading after any correction. It is an `int`, and `__lookup_timer()` builds it as
  `1000000 ×` whole seconds, so it **turns over** after some thirty-five minutes, a length `-loop`
  reaches. The difference of two readings either side of the turnover wraps modulo 2^32 and comes
  out as the true small difference.
- The **servo** is `sound_sync()`: it compares the scene clock with how far the music player has
  got, and trims the scene clock (slowing it, or speeding it when the player runs ahead) so the
  picture stays with the music.
- A **slew** is a correction paid out as a bounded rate over time rather than applied at once. A
  **deadband** is a band of error small enough to be left uncorrected.
- A **quantum** is one mixer buffer. The player's position moves a whole quantum at a time, so a
  plot of it over time is a staircase; each step is a **riser**.
- A **replica** is a standalone copy of the frame loop and the servo, driven by a simulated player
  and clock, used to measure how they behave under a given fault.

## Three memory rules the fork holds

Each is a constraint on the C code. Breaking any of them is harmless on 32-bit x86 with glibc, the
platform the demo was written for, and on x86-64 or with musl it corrupts memory or floods the build
with warnings. A build that happens to run on another system therefore proves nothing here.

- **A buffer is cleared at the element size it was allocated at.** `clear_zbuff()` in `tex.c`
  allocates per `int` and must clear per `int`. Clearing per `long` is the same size on 32-bit x86
  and twice the size on x86-64: the heap corrupts and stage 2 aborts inside the allocator.
- **An overlapping move uses `memmove`.** The scroll in `messager.c` moves a buffer onto itself
  shifted by one row. glibc tolerates `memcpy` there; musl is not required to.
- **A 32-bit-x86 calling-convention attribute is not expanded elsewhere.** `REGISTERS(n)` in
  `config.h` expands to nothing, because `regparm` is x86-32 only and warns on every declaration.

The first two are the kind of fault a sanitizer finds and a reader misses, so build the port under
AddressSanitizer and UndefinedBehaviorSanitizer before trusting a change to any buffer here.

## The demo caps its own frame rate

A scene states a rate for its control and never for its picture. On the hardware the demo was
written for, one draw took about twenty-five milliseconds and the loop paced itself; on current
hardware, without a cap, the loop draws as fast as the machine allows. Measured in a fifty-column
window that is five to fifteen thousand frames a second, and measured in a terminal emulator it is
seventeen mebibytes a second of escape sequences, for a display that shows sixty frames, with the
demo and the terminal each spending a core on it.

Past that point the pseudo-terminal never empties. Every write comes apart mid-frame, the terminal
composes its window from a screen it has only half received, and the person watching sees the top
of one frame over the bottom of the one before it: an animation that updates in horizontal bands
and appears to lag.

The animation loop therefore draws at most once every sixteen milliseconds (`BB_FRAME_US`,
16000 µs), and the cap is a deadline rather than a delay: it is measured from one frame's start to
the next, not from the end of a draw.

Timed from the end of one draw to the start of the next, the period would be sixteen milliseconds
plus the draw, and the draw grows with the screen: 0.46 ms at 80x25 against 1.35 ms at 240x67. The
period would grow with the screen too, past the one limit it has to stay under: `1000000 / 60`, the
control interval stated by fourteen `timestuff(-60, ...)` calls (counted in the tree). A
scene in waitmode draws only on a turn its control fired, so a period longer than the control's
refuses every other tick, and the next chance is a whole interval later. Measured over forty
seconds of the same scene in a terminal emulator:

| Terminal grid | End of draw to start of next | Deadline to deadline |
|---|---|---|
| 80x25 | 59.1 fps, 0.2% of frames held ≥25 ms | 60.6 fps, 0.1% |
| 106x33 | 49.1 fps, 23.5% | 60.6 fps, 0.1% |
| 240x67 | 31.0 fps, 93.9% | 60.6 fps, 0.1% |

Timed deadline to deadline, the rate is the same at every size; timed from the end of the draw, the
share of frames held rises from almost none at 80x25 to nearly all at 240x67.

The judder is bimodal, and its dependence on screen size is the evidence for the cause. At 106x33
there is nothing between 18 ms and 30 ms: every frame is either 16.7 ms or 33.3 ms. It is not a
shortage of time either, since one draw is 1.35 ms against a 16 ms budget and the whole loop uses
under three per cent of one core. The loss comes from the gate, not from the cost of drawing.

The deadline reading is taken before the draw and kept across it; a reading taken after the draw
is later by the cost of the draw and would add that cost back to the period. The deadline is a
`static` in `timestuff()` on a clock that never restarts, so it carries across consecutive calls
and a scene split into several calls is not handed a free frame at each seam. A loop that is not in
waitmode sleeps until whichever comes first of the frame deadline and the next timer, and never
past the end of the scene.

### Why sixteen milliseconds

The frame grid must stay under the control grid and at or under every consumer's floor. The 666 µs
it leaves below `1000000 / 60` is the margin, and the margin guarantees that the deadline is never
*ahead* of the control tick it is checked against: a grid shorter than the control's falls 666 µs
further behind the tick every frame, and is never re-pegged closer than 666 µs behind. A tick is
refused only after a stall, never because of the jitter of noticing one.

The consumer states no such constant. `kdos-term`'s draw is gated on the compositor's frame
callback, so its floor is whatever the output runs at: 16.667 ms at sixty hertz. A frame produced
faster than the floor is bytes the consumer must read and parse for a picture nobody sees.

Producing faster than the consumer does not add smoothness. The surplus is not dropped evenly; it
beats against the consumer's rate at the difference, and a beat is what an eye reads as judder. A
scene in waitmode draws at its control's rate, which is sixty a second for the fourteen
`timestuff(-60, ...)` calls and beats against nothing at sixty hertz. A scene stating a positive
rate with a control of its own draws whenever the deadline has passed, so its picture runs at
exactly the cap: 62.5 frames a second, which beats at two and a half a second against a sixty-hertz
output; that beat is the price of the margin above. A shorter period would mean more surplus and
a longer one would halve the rate, so the number cannot move far in either direction.

A deadline more than a whole period behind is moved rather than chased. A stall leaves the grid
arbitrarily far behind, and catching up would draw every missed frame back to back: the
pseudo-terminal fills, the frames tear, and the stall is paid for twice. Instead one frame is drawn
late and the grid restarts from the deadline that was met.

The frame gate has no test for a negative gap and needs none. Across the scene clock's turnover (see
[the terms](#how-the-code-is-organised)) the gap is the difference of two readings, which wraps and
comes out as the true small difference. Driven across the turnover, the frame rate stays at 61.8 fps
with no gap over 18 ms. The servo cannot produce a negative gap either, because its trim is capped
at a fraction of the time that has actually passed.

The control keeps its own rate. A control handler is told how many intervals it covers, so a
dropped frame moves nothing in the animation, every scene still ends on the same microsecond, and
the beats stay in step with the music.

Measured across the same window of the same run, at a terminal grid of 192x54:

| | Uncapped | Capped |
|---|---|---|
| Demo writes | 17 MiB/s | 0.09 MiB/s |
| Terminal reads | 17 MiB/s | 0.09 MiB/s |
| Terminal CPU | 44% of a core | 1.6% |
| Demo CPU | 100% of a core | 0.7% |
| Frames the display is sent | 62/s | 62/s |
| Cells the display is sent | 33,000/s | 33,000/s |

The picture is identical and the cost of producing it has almost disappeared. The same holds for
any program of this kind: frames produced faster than the display refreshes are not shown, and
they tear the frames that are.

## Every frame is bracketed in synchronized output

A frame cannot be written atomically, and that is why a bracket is needed. AAlib hands the picture
to the curses library's refresh, and refresh issues one write per screen row: 21 writes and 2.0 KB
at 75x19, 69 writes and 17.2 KB at 236x63. A pseudo-terminal holds 12288 bytes, so at full screen
the frame is larger than the pipe it crosses and cannot cross in one piece however the program is
written. A consumer composing on its own clock therefore reads a screen that is half this frame and
half the last one. Modelled against a consumer composing at sixty hertz, 7.5% of composes show a
torn frame and between eighteen and twenty-four per cent of frames are never shown whole.

The demo therefore marks where each frame begins and ends. `bbflush()` writes every frame between
`ESC [ ? 2026 h` and `ESC [ ? 2026 l`, the set and reset of **synchronized output** (DEC private
mode 2026). Between the two, a terminal that supports the mode keeps showing the screen it already
had and composes nothing it receives, so the rows may arrive in as many writes as they need and the
picture changes once, whole.

The mode is a contract with two ends, and one end alone changes nothing: the producer brackets its
frames and the consumer honours the mode. In this tree the consumer is `kdos-term`, which asks
`libkvt`'s `kvt_term_sync_hold()` before each draw. That function holds the frame back for at most
150 ms after the mode is set, so a producer that sets the hold and then dies cannot freeze the
window.

The arrangement degrades safely in both directions. A producer that never sets the mode is composed
exactly as before; a hold that outlives the watchdog is composed anyway; and a terminal that does
not know the mode ignores it, as it ignores any private mode it does not recognise.

The escape sequences go out on the same stream as the frame, which is why they are written to
standard output and why the stream is flushed at both ends. AAlib's curses driver is initialised on
standard output, so refresh writes to that very stream. An escape sent down any other descriptor
arrives in an order nothing defines, and a hold that lands *after* the rows it was meant to cover is
worse than none. When standard output is not a terminal, no bracket is written at all.

Measured on a captured pseudo-terminal at 236x63 (frames larger than the 12288-byte pipe) over
twelve seconds: 741 frames, 100.00% of the bytes inside a bracket, no nesting errors, largest
bracketed frame 20178 bytes. Every frame goes through `bbflush()`; nothing calls AAlib's
`aa_flush()` directly.

## The animation is tuned to the music

The scenes run to an absolute deadline. `timestuff()` computes each scene's end as
`start + duration` and chains the starts from one peg at the top of each track, so a beat that runs
late is *skipped* and never stretched. The mixer, meanwhile, runs on the sound card's clock. The two
agree because the demo was composed that way, and measured at a 50x19 terminal they agree to the
frame:

| Block | Animation | Its track |
|---|---|---|
| The scenes and biographies (stage 1) | 287.74 s | `bb.s3m` 287.70 s |
| The credits and the beat after them | 111.50 s | `bb2.s3m` 111.50 s |
| The closing text | paced by the player | `bb3.s3m` 278.60 s |

The first block is loaded at the top of stage 1 and started 24 s of scene clock later, when the
opening scene calls `play()`, so the credits begin 311.74 s into a run.

The schedule does not depend on the terminal's size. Every sweep that crosses the screen divides a
fixed time budget by however many steps that width needs, so the same beats land on the same
millisecond at 50 columns and at 106.

A skipped beat is paid for in frames, and the music cannot pay that way. The module advances one
tick per tick inside the mixer, every sample it renders is written, and nothing in the demo seeks
the player forward. A moment the card spends not playing (silence after a late wake, a stream
re-prepared from empty after an underrun, a card whose own rate differs slightly from the one the
module was rendered at) is therefore a moment the picture took and the music did not. Left alone,
that gap only grows, and a listener hears the demo running away from its track.

### The servo

The scene clock follows the player. `sound_sync()` runs on every turn of the render loop; it looks
at the error between the two every 200 ms (`BB_SYNC_US`) and feeds the correction into
`tl_slowdown_timer()`, which is subtracted from every later reading of the scene clock.

The correction is a rate and never a lump, because the frame loop is paced off the same clock. A
correction handed over in one piece lands inside one frame interval, and that frame is as long as
the piece: ten milliseconds against a sixteen-millisecond budget is a frame taking twenty-seven. So
the error is looked at every 200 ms and paid out continuously: every call takes the slice of it
that the time since the previous call is worth, clamped to 5% of that same time (`BB_SYNC_PPM`,
50000 parts per million). The clamp makes the correction a slew rather than a jump; a jump on this
clock would skip or repeat a scene outright. It also keeps the scene clock monotonic, which the
frame gate depends on.

The player's position is a staircase, not a clock, and a servo fed the raw reading spends its
effort on the quantisation rather than on the real disagreement. libmikmod's `sngtime` moves one
whole mixer buffer at a time: measured at the shipped quantum, a 90.0 ms riser every 97.7 ms, with
88.3% of the mixer's ten-millisecond ticks advancing it by nothing at all. Fed that raw, a replica
applied 5.37 corrections a second of 7.74 ms each against two clocks whose true disagreement was
0.12%. So the reading is carried forward from the last step actually seen, by at most one riser.
Past that the player has not paused between buffers, it has stopped, and an estimate that kept
climbing would hide the very stall the servo exists to pay for. The riser is taken from the player
as observed rather than stated as a constant, because it depends on the card. What is left is
smoothed with an exponential average (`BB_SYNC_ALPHA`, 10% of the error per look, about two seconds
of memory) before any of it is paid.

There is no deadband, and one would not be free. Forgiving small errors means forgiving the drift,
which is exactly what the servo exists to remove. The average already does the smoothing without
giving anything up; a 2 ms deadband on top of it reduces drift by nothing and costs steadiness in
the picture, measured as the frame interval's peak-to-peak spread (the longest interval minus the
shortest).

The slew limit has a measured floor. It must exceed the rate at which the two clocks *steadily*
disagree, or the servo saturates and the gap resumes growing at whatever is left over. What the
player renders and what the card plays are not the same second: measured on the emulated card the
demo is tested against, the module's own timeline runs 1.2% fast against the audio that comes out
of it. Above that floor the number is free, because a scene that runs a twentieth fast while it
catches up has no pitch to give it away.

The phase each track started with is kept rather than corrected to zero. What is *heard* lags what
is *rendered* by whatever the audio buffers below the player hold, and the demo cannot see that
number; correcting to zero would put the picture ahead of the sound by exactly that buffer. Only the
growth of the error is taken out.

The servo removes accumulated phase and not only rate, and that depends on one variable in
`sound_sync()`: `base`, the reference point the error is measured from. The rate is set so that a
standing error would be gone in one 200 ms interval, but the clamp pays at most 5% of any stretch of
time, so a larger error is paid over several looks and `base` is what remembers the unpaid
remainder. Exactly two events reset `base`, because each starts a phase that is new rather
than an error:

- A track that has not started or has restarted: there is no music to follow (`sound_clock()`
  answers −1, which it does before `play()`, under `-nosound` and with no module loaded), or the
  raw player reading is lower than the previous one. `bb3.s3m` is rewound in place when it falls
  inactive. The correction already made is kept across the reset: it is time the machine lost, and
  the next track runs on the same machine.
- The scene clock's [integer turnover](#how-the-code-is-organised), which `-loop` reaches. The
  phase either side of it is not comparable.

Resetting `base` at any other moment throws away the unpaid error. It does not show as a bad frame;
it shows as the demo finishing seconds away from its music. Two plausible changes would reset it
that way:

- Measuring the servo's own timing on `TIME`. The look interval, the average's time constant and the
  turnover test all use the raw reading: `TIME` plus everything the servo has handed
  `tl_slowdown_timer()`, which is what `__lookup_timer()` answered and which goes backwards only at
  the turnover. `TIME` runs up to 5% slow or fast while the servo trims it, so a rate measured
  against `TIME` under-pays by exactly the rate being corrected. The turnover test is worse on the bent
  clock, because each spurious firing re-pegs `base`, discards the error not yet paid and forgives a
  stall permanently, after which the drift is bounded by nothing, since it is the single largest
  stall that sets it.
- Taking a loaded but unstarted module's position of zero as real. `Player_Load()` leaves `sngtime`
  at zero, and so does the first tick of a track, so `sound_clock()` keeps a flag of its own and
  answers "no music" (−1) until `play()` has started the player. The window is not small: `bb.s3m`
  is loaded at the top of stage 1 and `scene1()` reaches `play()` twenty-four seconds of scene clock
  later, and a standing zero across that window reads as a player twenty-four seconds behind, which
  the servo would then slew the whole picture to catch.

Measured on a replica over 280 s of scene clock losing 85 ms of audio every 3 s, with a draw cost of
1.35 ms:

| `sound_sync()` | fps | Frame interval, standard deviation | Frame interval, peak to peak | Doubled frames | Corrections | Drift |
|---|---|---|---|---|---|---|
| One clamped lump every 200 ms | 56.4 | 1.802 ms | 11.385 ms | 1.81/s | 4.9/s of 9883 µs | +0.100 s |
| Looked at every 200 ms, paid continuously | 60.0 | 0.377 ms | 5.773 ms | 0.00/s | 258.0/s of 162 µs | +0.100 s |
| …the same, but with a 2 ms deadband | 60.0 | 0.402 ms | 18.941 ms | 0.00/s | 258.8/s of 161 µs | +0.101 s |
| …the same, but measured on `TIME` | 62.7 | 0.571 ms | 5.384 ms | 0.00/s | 187.6/s of 159 µs | +11.980 s |

The second row is the one the fork ships. Paying continuously leaves the drift unchanged, doubles no
frames, and makes the frame interval four to five times steadier. The deadband buys nothing and
loses peak-to-peak steadiness; timing on the bent clock loses twelve seconds.

The drift is bounded, not merely small. The same run at 1200 s gives +0.030 s at 56.4 fps with a
standard deviation of 1.809 ms for one lump every 200 ms, and +0.030 s at 60.0 fps and 0.379 ms
when paid continuously; with a 2 ms deadband it is +0.030 s with peak-to-peak 10.028 ms against
7.462 ms. What varies between runs is the phase the track started at, which is one quantum and is
kept deliberately.

The correction is insensitive to what it is fed. Across a quantum sweep from 0 to 170 ms the frame
interval's standard deviation stays between 0.371 ms and 0.405 ms, and a 500 ms stall every 30 s
gives 0.481 ms with the same drift.

The position the servo follows is the player's own `sngtime`, in units of 1/1024 s, which advances
a tick at a time at whatever tempo the module asks for. It is read without libmikmod's lock: it is
one aligned word, and taking the lock would put the render thread behind the real-time mixer.
`song_progress()` is the wrong clock for this purpose, because its reading carries a skew that
belongs to the module (see [What the player should read](#what-the-player-should-read)), which over
a track is seconds of phase that are not there.

The animation's own schedule is untouched by any of this. Measured under a pseudo-terminal with the
mixer off, stage 1 reaches the credits at 311.740156 s against 311.740147 s: nine microseconds over
five minutes.

## The closing text turns a page at a time

The closing text places the document where the *player* is, not where a clock is.
`song_progress()` answers thousandths through `bb3.s3m`, and the text is placed at the same fraction
of its own length, so it keeps pace with the track at any mixer rate and on any terminal. The scroll
finishes at 940 thousandths (`SCROLL_ENDS_AT`) rather than 1000, so it ends shortly before the
track does. The track loops when the player falls idle, so a reading that goes backwards by more
than 50 thousandths is taken to mean the track has ended, and the scroll goes straight to the end.

It moves a screen at a time and never a line. Each change of position costs a morph, a one-second
cross-fade between the old page and the new one. The document is some fifteen screens against a
track of four and a half minutes, so a line at a time would ask for a new line every three quarters
of a second: the next step would begin before the last had landed, the page would be permanently in
motion, and nothing on it could be read.

A screen at a time is one morph and then a still page. Measured at 106x33 (24 visible rows of a
375-line document), that is 15 turns over 16 slots of the track, 16.4 seconds a slot of which one
second is the morph. Silent, the scroll runs over a fixed 150 seconds (`SCROLL_NOSOUND`) with the
same 15 turns, and a capture shows the screen unchanged for 122 of 159 seconds.

The turns are spread over one slot more than they need, so the last page arrives a slot early and
stays on screen before the scroll is done. It is the page with the most to say and the only one
nothing follows. When the scroll is done the closing text fades out and the program exits.

A shorter screen means more turns of less text each, so the time on each page shrinks exactly as
fast as the amount to read does, and no minimum has to be stated.

Any key turns the automatic scroll off for the rest of the scene. A scroll that pulled the page back
under a reader who had just paged up would be worse than none; once a person is driving, they keep
control, and only `q` or `Esc` then ends the scene.

## The mixer runs on its own thread

The module player's update call, `MikMod_Update()`, is a **pull** interface: it has to be called
often enough to keep the sound card's ring buffer full. Upstream's `bb` calls it from a
ten-millisecond timer in the same timer group the frame loop pumps, which puts it behind a caller
that has just flushed a whole frame to the terminal and is blocked while the terminal drains it.

On a bare console that coupling is invisible. Under a compositor the terminal is shaping a
screenful of cells and uploading a texture every frame, so the pseudo-terminal backs up, the flush
sits in a write, no timer runs, the ring empties, and the music stutters. Minimising the same window
makes the sound perfect, which shows that the fault is neither in the mixer nor in the buffer size.

The mixer therefore gets a thread with its own clock, which the render loop cannot reach. The
thread wakes on an absolute ten-millisecond deadline (`SOUND_TICK_NS`), so the time an update takes
is inside the period rather than added to it, and a deadline already missed is moved forward
rather than chased. The sleep at the end of each turn is unconditional: at real-time priority, a
path that skipped it would be a thread spinning, which on one core stops the machine answering.

The thread asks for real-time scheduling, `SCHED_FIFO` at priority 10 (`SOUND_RT_PRIO`). At the
ordinary policy it runs only when the scheduler considers it due, and the other runnable thread in
the process is a render thread writing escape sequences as fast as it may. Every wake-up lost past
what the ring holds is an underrun, and libmikmod sets no software parameters, so the stream stops
on one and the driver's recovery re-prepares it from empty: a short pause in the music, repeated.
Priority 10 is below every priority an audio server or a threaded interrupt handler takes
(whatever drains the ring must be able to preempt the thread filling it) and above every ordinary
thread. KDOS grants real-time priority to a console login through `kdos-getty`, which raises the
real-time priority limit to 95 before the login starts and the session inherits it; see
[the session](../03-architecture/session.md). The request is never a requirement:

| What the system allows | What the demo does |
|---|---|
| Real-time scheduling | The mixer thread runs at `SCHED_FIFO` 10 |
| No real-time scheduling | The mixer thread sets the render thread's nice value to 5 (`SOUND_RENDER_NICE`) instead, which needs no privilege |
| Neither | The mixer thread runs at the ordinary policy |
| No thread at all | The mixer is fed from a ten-millisecond timer on the render loop |

The last row covers a libmikmod built without thread support, where `MikMod_InitThreads()` answers
0, and a thread that could not be created. `KDOS_BB_DEBUG` reports which of these happened; see
[Debugging](#debugging).

The `libmikmod` port carries two patches to its ALSA driver that this arrangement relies on.
`alsa-nonblocking-update.patch` opens the stream non-blocking and makes each update hand over as
many periods as the card accepts (at most eight) and then return, so an update never sleeps inside
the audio write. `alsa-null-close.patch` stops the driver closing a null handle when the `default`
device cannot be opened, so a machine without a usable device gets silence rather than an abort.

### The locking rule that goes with it

The fork never wraps a libmikmod call in libmikmod's own lock. That defensive pattern (lock, call,
unlock) deadlocks the process against itself: the library's mutexes are not recursive, and
`MikMod_Update()`, `Player_Active()` and `Player_SetPosition()` each take the same lock
themselves. The process stops dead, with the demo frozen mid-frame.

That internal locking is what `MikMod_InitThreads()` promises. The exported `MikMod_Lock()` is for
protecting the caller's own access to the library's exported variables such as `md_mode` and
`md_mixfreq`, which is a different thing. For the same reason `sound_clock()` checks a flag of its
own rather than calling `Player_Active()` on every turn of the render loop: each `Player_*` call
takes the mutex the real-time thread holds during an update, and the mutex has no priority
inheritance.

What the library does not protect is the fork's own module pointer, so stopping a track joins the
mixer thread before freeing the module, and the program joins it again before shutting the library
down.

### The ring buffer stays at upstream's size

libmikmod's ALSA driver asks for a 250 ms buffer in 50 ms periods, fixed in its code, and the fork
leaves that alone. The buffer is the margin, not the defect. A shorter ring lowers the delay
between a sample being mixed and being heard, and pays for it with less slack when something
starves the feeder. The feeder is what goes wrong, and the mixer thread is what fixes it; shrinking
the ring as well would lower a latency that is already inaudible and would add underruns, which
are audible as crackles.

There is no runtime setting for it: the driver's command-line hook is an empty function, and the
demo passes an empty parameter string.

## Rules for other AAlib and libmikmod programs

These rules apply to anything else built on the same libraries. `kdos why kdos-bb` shows the first
two on an installed system; see [the kdos command](kdos-command.md#kdos-why-and-kdos-explain).

- **Name the curses driver, not only the console one.** AAlib's `aa_autoinit()` tries any
  recommended driver first (from `-driver` or `AAOPTS`), then sweeps its compiled-in list in order
  and takes the first that initialises. On KDOS that list is `linux`, `curses`, `stdout`, `stderr`.
  The `linux` driver writes cells into `/dev/vcsa<n>`, which an ordinary user cannot open, and the
  last two print a fresh block of text for every frame, which scrolls up the terminal. A program
  that recommends a driver should recommend `curses` as well as `linux`, so that the outcome of a
  failed console driver is not left to the sweep.
- **Register every module loader your music needs.** Public-domain tracker music is spread across
  several formats (`.xm`, `.it`, `.mod` as well as `.s3m`), and a track whose loader is not
  registered fails inside `Player_Load()` and plays silence. This demo registers only the S3M
  loader, because all three of its tracks are S3M; a program that plays music from anywhere else
  should call `MikMod_RegisterAllLoaders()`.
- **Cap the frame rate in the program.** AAlib does no pacing: its flush writes whatever is in the text buffer
  every time it is called, and nothing in it knows what a screen refresh is. A program that states
  no cap writes at whatever rate its own arithmetic happens to run.

## Audio on a bare console

Two behaviours in the boot scripts make sound work at all, and neither is specific to this program:

- Device coldplug replays devices as additions. `/etc/init.d/01_udev.sh` runs `udevadm trigger` with
  `--action=add`. The default replays every device as a *change*, and the rule that loads a driver
  from a device alias skips anything that is not an addition, so a plain trigger loads no module,
  the audio controller stays unclaimed, and the sound library reports an unknown device.
- Sound state falls back to initialising when restoring fails. `/etc/init.d/50_alsa.sh` runs
  `alsactl restore` and, if that fails, `alsactl init`. A live image has no saved state, and a
  failed restore leaves the hardware as the kernel left it, which for Intel HD Audio (HDA) is muted
  at zero. `alsactl init` returns a distinct status when it matched a generic rule rather than a
  card-specific one, which is a success here, so its status is ignored.

In a desktop session the sound server is PipeWire, which the session starts (`kdos_session_audio`
in `/usr/local/lib/kdos/session-common.sh`, called from `kdos-desktop-start`). ALSA routes its
`default` device to PipeWire only because `/etc/alsa/conf.d/99-kdos-pipewire.conf` says so, since
the directory PipeWire installs its own drop-in into is not one alsa-lib reads. See
[the session](../03-architecture/session.md#audio).

That matters to this demo more than to most programs. libmikmod's ALSA driver opens the literal PCM
name `default` and accepts no options, so it goes wherever that name points. A login with no session
(a plain getty on another virtual console, a serial console, an ssh login) has no PipeWire, and
`default` then fails to open. The same configuration file lets an environment variable choose the
device instead. `kdos_card` is the sound card itself, reached through alsa-lib's own software chain:
format and sample-rate conversion (`plug`), software volume (`softvol`) and software mixing of
several streams (`dmix`), as [the session](../03-architecture/session.md#audio) describes. To use
it:

```sh
KDOS_ALSA_DEFAULT=kdos_card kdos-bb
```

`kdos_card` and PipeWire exclude each other: while one holds the card the other cannot open it.
An undefined name fails with `Unknown PCM <name>` and an absent PipeWire daemon with
`Host is down`.

## Debugging

```sh
KDOS_BB_DEBUG=1 kdos-bb 2>~/sync.log
```

The demo's error output is the terminal it is drawing on, so redirect it.

The trace reports which way the mixer is being fed (`mixer thread is SCHED_FIFO`,
`no real time - render thread niced instead`, `no real time and no nice - mixer is SCHED_OTHER`, or
a fall back to the timer), whether libmikmod has thread support, and whether each module loaded. It
is silent otherwise. It also exposes the self-deadlock described under
[the locking rule](#the-locking-rule-that-goes-with-it): the log stops at the line before playback
starts.

Every five seconds of raw scene clock it reports the demo's clock against the player's position,
with what the servo has had to take out and how long the mixer thread went unscheduled:

```
kdos-bb: sync  demo   80.01s, player  278/1000, held  -829016 us, err   1620 us, worst gap 10102 us
```

| Field | Meaning |
|---|---|
| `demo` | Seconds into the current track, on the scene clock |
| `player` | How far through the module the player is, in thousandths, as `song_progress()` reads it |
| `held` | The whole trim the servo has applied to the scene clock since the demo began, signed |
| `err` | The phase the servo has not yet taken out, at its largest since the previous line |
| `worst gap` | The longest the mixer thread went unscheduled since the previous line |

`held` is positive when the scene clock has been slowed, which is audio time this machine has lost,
and negative when it has been sped up because the player runs ahead of the picture, as in the
sample line. A smooth climb is a card running at the wrong rate; a staircase is a stream that
stopped and restarted, and each step is one stutter.

A bounded `err` means the correction is keeping up. An `err` that climbs line after line means the
slew limit is below the rate at which the two clocks disagree.

A `worst gap` longer than the ring is silence and a shorter one costs nothing, and the two look
identical from the render loop, so the number is printed rather than a verdict. It is recorded on
the mixer thread and printed on the render thread, because a real-time thread writing to the
terminal could block behind it and stall the highest-priority thread in the process.

### What the player should read

There is no formula for it. `song_progress()` assumes 64 rows per pattern, so its reading is skewed
by an amount that belongs to the *module*: `bb.s3m` reads high, `bb3.s3m` reads low and shrinking,
`bb2.s3m` reads low and growing, by up to 63 thousandths. The figures below are what an offline
render of each track reports at the same point, and so what a correctly playing one reports too:

| Demo seconds into the track | 20 | 40 | 60 | 80 | 100 | 140 | 180 | 220 | 260 |
|---|---|---|---|---|---|---|---|---|---|
| `bb.s3m` — stage 1 | 83 | 151 | 218 | 292 | 353 | 495 | 636 | 771 | 906 |
| `bb2.s3m` — the credits | 166 | 333 | 500 | 666 | 834 | — | — | — | — |
| `bb3.s3m` — the closing text | 59 | 132 | 205 | 278 | 350 | 496 | 641 | 787 | 932 |

A player reading above its row is ahead of the animation; one reading below is behind it. Measured
on the shipped image with sound, the closing text reports 60 at 20 s and 278 at 80 s against 59 and
278, in step to the thousandth.

## Building and changing it

The port compiles only where AAlib and libmikmod are installed, which means inside the build, not on
a bare development host. After a change under `src/art/kdos-bb`, rebuild the one port and
repackage:

```sh
make build BUILD_ARGS="--phases 41_system,70_image --rebuild kdos-bb"
```

To hear the music in the test rig the guest needs a sound device, which `testing/vnc-shot.py`
gives it with `--audio`; without one `MikMod_Init()` fails and the demo runs silent. See
[Testing](../05-developer/testing.md). Before trusting a change to a buffer, build the sources once
with `-fsanitize=address,undefined` (see [the memory rules](#three-memory-rules-the-fork-holds)),
and before trusting a change to pacing or synchronisation, compare the `KDOS_BB_DEBUG` trace with
the table above.

## See also

- [Decisions](../01-philosophy/decisions.md#freezing-a-demo-rather-than-writing-one) — why a fork
  rather than a new demo
- [The session](../03-architecture/session.md#audio) — how ALSA's `default` reaches PipeWire, and
  the quantum the servo copes with
- [Boot and init](../03-architecture/boot-and-init.md) — the init scripts named above
- [Administration](../02-user-guide/administration.md) — audio on the host
- [kdos-term](kdos-term.md) — the terminal that honours synchronized output
- [The kdos command](kdos-command.md#kdos-why-and-kdos-explain) — `kdos why`, which carries the
  library rules above
- [Testing](../05-developer/testing.md) — the rig flag that gives a guest a sound device
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the phase that builds this port
  with the rest of the userland
- [The ports catalogue](../06-reference/ports-catalogue.md#srcart) — the port beside the other
  `src/art` ports

<!-- book-nav -->
---

*Part IV — Programs, chapter 29.* Previous: [28. The kdos command](kdos-command.md) · [Contents](../README.md) · Next: [30. How KDOS is built](../05-developer/how-kdos-is-built.md)
