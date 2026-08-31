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

## Three defects fixed in place

Two were found with sanitizers rather than by reading:

- **A buffer was cleared with the wrong element size** — allocated per one integer type and
  cleared per a wider one. The same size on the platform it was written for, twice the size on a
  modern one: the heap corrupts and a later stage aborts inside the allocator.
- **A scrolling buffer was moved with a copy that forbids overlap**, with source and destination
  overlapping by every row but one. One C library survived it; another is free not to.
- **A calling-convention attribute that is 32-bit-x86 only** was expanded on every declaration,
  warning on each.

The third fix is the audio arrangement, below.

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

## Two library facts that outlive this program

State these as rules for anything else built on the same libraries.

**The ASCII-art library's console driver writes cells into a device node an ordinary user cannot
open.** Its automatic initialisation answers a failed *recommended* driver by sweeping its own
driver list — landing on the plain-output driver, which scrolls a fresh block of text up the
terminal every frame. **Anything on this library must recommend the curses driver as well as the
console one.**

**Register all the module loaders, not just one format.** The public-domain music available online
is spread across several tracker formats; with a single loader registered, a track fails inside the
load call and the program plays silence.

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

There is no sound server on a console and none is wanted — the desktop user is already in the audio
group.

## Debugging

```sh
KDOS_BB_DEBUG=1 kdos-bb
```

Reports which way the mixer is being fed — thread or timer — and whether the module loaded at all.
Silent otherwise, because the demo's error output is the terminal it is drawing on.

That trace is what caught the self-deadlock above: the log stopped at the line before playback
started.

## There is no KDOS demoscene

A from-scratch demo was written for this project and then removed at the maintainer's request. It
is not coming back, and a stale reference to one is a leftover rather than a plan. This fork is
what ships.

## See also

- [Decisions](../01-philosophy/decisions.md) — why a fork rather than writing one
- [Administration](../02-user-guide/administration.md) — audio on the host
- [Boot and init](../03-architecture/boot-and-init.md) — the init scripts named above
- [Testing](../05-developer/testing.md) — the rig flag that gives a guest a sound device
