# Testing

What each harness proves, what it cannot, and how to run it. KDOS has no unit-test framework and
ships no test binaries; what it has instead is a preflight check, a self-test over the libraries
and their consumers, committed reference frames, recorded system-state fixtures, and a virtual
machine rig that drives a real session.

## What each tool proves

| Tool | Proves | Cannot | Roughly |
|---|---|---|---|
| `testing/preflight.sh` | The **wiring**: every reference resolves, every option exists, every script parses | That anything works | Seconds |
| `testing/selftest.sh` | The libraries' invariants, that every consumer still compiles, and that surfaces still lay out | That the system boots | Half a minute |
| Reference frames | That a surface's geometry and colours have not drifted | That it is usable | Included above |
| Fixtures | That a reading or a decision is correct against recorded state | That the reading is correct live | Included above |
| `testing/vnc-shot.py` | That a **real session** does a thing, photographed | Anything the renderer used cannot show | Minutes per boot |
| `testing/packlane.sh` | The application lane end to end on a booted machine | | Minutes |
| `testing/install-to-disk.sh` | That the installer installs | | Minutes |

**None of them proves the build works.** A package manager can only really be tested by building
the distribution with it.

## preflight.sh

Everything a full build would catch, minus the build. Twenty-eight checks, in seconds:

| Group | Checks |
|---|---|
| Packages | Every package named in a phase list has a port; every list resolves to a dependency order with clean output and valid tokens; every dependency names a port that exists; the build tree carries no package whose port is gone |
| Recipes | Every port has a build script and it parses; every recipe parses as metadata; every one declares a name, version and release; every source a port ships is named by a checksum; every port of ours is built by something |
| Build options | Every meson option a recipe passes is one that port defines, checked against the tarball's own option file, with the two closed-value types validated |
| Sources | Every source file in one of **our** ports is compiled by its recipe; a first source whose members are prefixed is accounted for; a flat first source is unpacked by its own recipe |
| Shipped configuration | The shipped compositor configuration keeps the default bindings; every command it and the menu name exists; every filesystem the installer offers, the initramfs can mount |
| Shell | All shipped and build shell is syntactically valid; no build script **names a command inside double quotes and runs it**; every helper the makefile runs is on disk and none shadows its own output |
| Consistency | Every flag one shell tool passes another is one it accepts; every daemon an init script starts is installed by a port; the rootfs carries no script whose interpreter is gone; nothing points at a removed file; every recipe carries the banner; no chroot step reads the ports tree through the wrong path; the catalogue's rows match the tree |

Three of those deserve singling out, because each is a whole class of failure that never reaches a
compiler:

- **Every source file in one of our ports is compiled by its recipe.** A file the build script
  neither names nor matches passes every gate on a development host and fails to **link** in the
  build — because the self-test globs those directories, so a whole page can be exercised by the
  harness and be absent from the shipped binary.
- **Every meson option is one the port defines.** meson fails at **setup** on an unknown option,
  before a line is compiled, and there is no universal spelling. Several were found the slow way
  before this check existed, each an hour-long round trip.
- **Every flag one shell tool passes another is one it accepts.** These tools spawn each other by
  name, and an unknown argument prints a usage line to an error stream nobody reads and exits
  **before a surface exists**. The result is a control that silently does nothing, invisible to a
  compile and to a reference frame. The check is crude on purpose: find the argument literals, take
  every long flag, and require it to appear in the target's own source.

**It cannot prove the build works. It can prove the build will not fail for one of the dull
reasons.** Run it after touching a phase list, a recipe, or anything under the build scripts.

## selftest.sh

The host-side regression net for `src/libs/`.

It compiles every library with the host compiler under warnings-as-errors, runs the shared
assertion program against them, compiles the consumers to prove the headers still agree, resolves
a port to prove the ports tree still parses, audits a generated theme, renders every front end
offscreen and compares against committed frames, and runs the orchestrator end to end against a
synthetic tree.

No container, no network, about half a minute — most of it the artwork the theme audit generates
three times, which is the price of testing that claim against the **real** vendored assets.

**Its assertions are the invariants**, most of which were established by comparing against the
implementations these libraries replaced. Each is a claim that was measured once; the suite is what
notices when it stops being true.

Two are worth knowing because they are counter-intuitive:

- **The two colour-mixing functions are asserted to *disagree***, because each generated file was
  written against exactly one of them.
- **The archive writer is checked by handing its output to the real archive tool.**

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite is clean that way and stays that way. It found two real defects a plain run could not
see: an archive size field overflowing a signed type — where the negative became an unsigned length
and a read was asked for an impossible number of bytes into a small stack buffer — and a copy called
with a null source on every entry's first key.

**Leak checking is off by default and on for the library assertions alone.** Every program here owns
its parsed state until it exits, which a leak checker reports as a leak and turns into a false
failure; the library suite is the one binary whose subject is code called repeatedly.

A coverage-guided fuzzer over the parsers is what found both defects above, in minutes. It is not
committed — the corpus is worth more than the driver, and neither has a home in a tree that ships
no test binaries — but it is worth rewriting after any change to a parser.

### What it does not cover

The compositor and the shell compile here **only where their Wayland dependencies exist**, which a
bare development host does not have — so on most machines those blocks report as skipped.

**A skip that cannot be lifted is a test nobody runs.** Three things in this file once made whole
blocks unreachable on any ordinary host: a library name decided *below* the block that read it, a
tool assumed universal with no guard so its absence took the rest of the suite with it, and an
include path only one machine had. **A missing tool is a skip with a name**, which is the rule every
conditional block keeps.

What lifting those found: several address truncations, a device path built in a buffer far too small
for a real device name — where truncating **opens the wrong node** — a dead list, and reference
frames stale by a whole page.

**The shell compiles here with format-truncation warnings disabled and nothing else relaxed.** That
program truncates on purpose: every label goes into a fixed number of **cells**, so a truncated
label is the intended behaviour. Where truncating *is* a defect it is not a label — a socket path, a
device node — and those are held by explicit bounds rather than by a warning that cannot tell the
two apart.

### The cached host helpers

`ports/` caches compiled helpers. **A binary built against one C library cannot execute under the
other**, so a host run straight after a container run must clear them first.

The failure does not say any of that: the metadata reader simply fails, so the version checker
cannot render a candidate and the suite reports that it reproduced no outcome — which reads as a
regression in the version checker.

And a container run as root leaves those directories **root-owned**, so the removal fails on
permissions and the stale binary survives. **Delete them from a container** rather than reaching for
elevated privileges.

## Goldens

Committed reference frames: a surface rendered offscreen and compared byte for byte.

**Sixty-three frames** across five sizes, covering the shell's front ends, all ten monitor pages
plus its detail page, and the cell-level frames.

| Kind | Catches | Count |
|---|---|---|
| Text frames (`--dump`) | Geometry: overflow, misalignment, a control drawn past its rectangle | Most |
| Cell frames (`--dump-cells`) | **Colour** and attribute drift as well | Four |

**Two sizes minimum for anything with a layout**, because a geometry defect is usually a defect at
one width. The monitor's pages carry three, including a narrow one that forces its sidebar to
degrade.

**A cell frame's verdict must actually be acted on.** A comparison whose answer nothing reads is a
comparison that cannot fail — the cell half needs its own check with its own message, because a
colour drift and a geometry drift are fixed by looking at different things.

**The dump harness stubs the icon layer to "no picture".** So a reference frame is the **character
grid**, and a layout that only lines up once the pictures load is a layout that is broken. That
harness must stub the **whole** interface a front end calls: a missing stub is a link failure that
reads as "the front ends do not compile" and takes every frame behind it.

Regenerating is a variable on the self-test, and must be done where the Wayland dependencies exist
— a build container, not a bare host.

**Six surfaces have no dump and therefore no frame**: the panel itself, the run box, the prompt,
the notification daemon, the on-screen display and the desktop. That is a stated gap, not an
oversight in this page.

## Fixtures

A **recorded** system state that a reader can be pointed at instead of the live machine. This is
what makes a monitor, an attribution engine or a kill-selection policy testable at all.

The seam is the same everywhere: the process and system filesystems live behind a movable root, or
a variable moves one walk.

| Fixture | Records | Makes testable |
|---|---|---|
| `res` | A process and system tree | Every monitor page, deterministically |
| `stutter` | Two snapshots half a second apart, plus the frame events between them | That the application is named with its box, the blocked process is first, pressure is quoted, and exactly one event is blamed on the compositor |
| `energy` | Four recorded power and process trees | The nesting rule, the counter wrap, the roll-up onto one application, and the short-lived residue |
| `oomd` | A tree arranged so **only** the memory budget can produce the right answer | That the budget check is load-bearing — its host process is larger than anything in either box |
| `mountd` | A block-device tree plus two hand-built superblocks | The acceptance, and **both refusals** — the internal disk carries a real superblock so a broken check shows up as an extra row rather than as nothing |
| `privacy` | Three processes, one holding a camera twice, one an audio device that must be ignored | The camera half, on a machine with no camera |
| `portup` | Recorded upstream responses for six ports, one per discovery path | All three outcomes, offline |
| `cve` | Four ports and a five-row database | A pin behind two fixes, one that only looks behind because of a packaging revision, a name mapping, and a package the database never heard of |
| `clone` | Hand-built image headers | The two-record length rule |
| `tray` | A second **process** that behaves like a real tray item | The whole protocol conversation |
| `shell` | The dump harness and its stubs | Every front end's layout |
| `pack`, `box`, `deco`, `openwith`, `recent`, `tone`, `cellclip`, `ascii` | | Their respective units |

**Both traps a fixture guards were confirmed to bite** by building the daemon with each check
disabled — which is the only way to know a test is testing something.

**The tray fixture is a second process on purpose.** The protocol is a conversation between two
peers on a message bus, and a mock of either side would have passed on both of the silent bugs that
implementation had.

`testing/fixtures/be` is an empty leftover and records nothing.

## The QEMU rig

`testing/vnc-shot.py` boots a real image, drives it, and reads the framebuffer. It is how anything
a dump cannot see gets looked at: the compositor, the wallpaper, the icon layer, a popup anchored to
the wrong corner.

It boots headless with a serial socket and a monitor socket, **types the session start on the first
terminal through the monitor** — the session is started by hand on this distribution, and a
compositor launched from a serial line gets no seat — waits for the compositor, and reads the
framebuffer over the remote-framebuffer protocol.

### The flags

| Flag | Does |
|---|---|
| `--disk`, `--boot-disk` | Attach and boot a disk image instead of the ISO |
| `--no-cdrom` | Leave the ISO off, so the **disk** is what boots |
| `--no-session` | Do not start a compositor |
| `--shot <file>` | Capture the framebuffer |
| `--keys`, `--click x,y`, `--mouse x,y` | Drive it |
| `--sleep`, `--wait`, `--soak <s>` | Timing |
| `--cmd`, `--root-cmd` | Run something in the guest |
| `--root-script <file>` | Send a **local** script in and run it as root |
| `--console-cmd` | Type on the first terminal **instead of** starting a session |
| `--audio` | Give the guest a sound controller with a null backend |
| `--data-disk <file>` | Carry one file's bytes into a guest with no network |
| `--scratch`, `--usb`, `--keep`, `--serial-log`, `--size`, `--gl`, `--vnc-port`, `--session-env`, `--script-timeout` | The rest |

Four of those answer questions a screenshot alone cannot:

- **`--audio`** gives a real device as far as the guest is concerned, with the samples going
  nowhere. Without it the sound library fails to initialise and every audio path in the guest is
  **untestable, which is not the same as untested**.
- **`--console-cmd`** photographs a program at the console font and the low glyph tier it has to
  read in. A window under a compositor is a different renderer answering a different question.
- **`--soak`** lets the session run between launch and measurement, because a monitor's own cost
  over a few seconds is its startup, and startup is exactly what is not being measured.
- **`--root-script`** is the form for a check too long to be one command. It travels **encoded, in
  short lines** — a terminal in canonical mode drops everything past its line limit, silently, and
  an encoded payload contains nothing the shell acts on before it is decoded — and the rig waits for
  a marker the **guest** echoes, so a step taking minutes is waited for rather than truncated. A
  plain command waits a fixed short time, so anything longer must be a script.

### Three things it has to get right

- **A display dump answers "no surface" under accelerated graphics**, so the framebuffer is read
  over the network protocol instead — whose encoding negotiation has an exact field layout, and an
  extra padding field desynchronises the stream so every later read blocks forever.
- **Driving the session means a *login* shell.** A plain switch-user leaves the container tooling
  resolving the home directory to the filesystem root, and every call fails on a permission error.
- **A plain virtual display puts the compositor on software rendering**, so the phosphor pass
  declines and **is not in the photograph**. What is photographed is the cell grid underneath it.

### The transport for a large artefact

**A plain virtual disk, not a USB device.** Emulated USB storage measured at under two megabytes a
second against a virtual disk's several hundred — a large pack took over an hour one way and seconds
the other. Use the USB path **only** when the test needs a genuinely removable device, which is the
removable-media daemon's whole subject.

The file is read back by block copy and truncated to its exact length, because a raw drive is
rounded up to a sector boundary.

### The host may not have an emulator

The rig runs unmodified inside a container image that already carries one, with the repository bind
mounted and hardware virtualisation passed through.

### Harness traps

Each is a rule with its consequence:

- **A build started inside a backgrounded call dies with it.**
- **A pattern kill matching a monitoring loop's own search term kills the monitor.**
- **An exact-name kill cannot match a name past the process-name length limit**, so a restart that
  used one restarts nothing and the stale program keeps answering.
- **The harness kills the emulator without a shutdown**, so a root script must synchronise after
  writing to the disk or the filesystem leaves the rewritten files **zero-length** on the next boot.
- **A pipeline that reads a supervised service's output never returns**, because the supervisor
  keeps the pipe open.
- **A static screen produces no frame events**, so anything about dropped frames needs something
  animating first.

## The other harnesses

| Harness | Does |
|---|---|
| `packlane.sh` | The application lane end to end on a booted machine: the daemon, the keyring, an install from the medium, the launchers, a box, and the telemetry — reporting pass, fail or **skip with a reason** |
| `install-to-disk.sh` | Runs the installer into a disk image, on its own terminal, with a heartbeat |
| `appsweep.sh`, `appreport.sh` | Launch every catalogue application and render the results as a table and a contact sheet |
| `bootcheck` | Boot verification |
| `prepare_base.py`, `test_runner.py` | Build a minimal root filesystem as a container image and build individual ports against it |
| `qemu-audio.sh` | Probe for a working audio backend rather than hardcoding one, because the emulator aborts at startup on a backend its build lacks |
| `qemu-hw/` | The containerised emulator with accelerated graphics |

## What is not tested

Stated so nobody assumes otherwise:

- **The memory daemon has never fired for real.** Its victim selection is exercised against recorded
  state; a genuine pressure stall is the test that matters.
- **Six shell surfaces have no dump and no reference frame.**
- **The compositor and the shell are not compiled by the self-test on a bare host.**
- **Nothing here tests the build**, which takes hours and a container.

## See also

- [Developing](developing.md) — the loops these fit into
- [Writing desktop software](writing-desktop-software.md) — dumps and reference frames from the author's side
- [The C libraries](c-libraries.md) — what the assertions cover
- [Build troubleshooting](build-troubleshooting.md) — when preflight is not enough
- [Known gaps](../06-reference/known-gaps.md) — the full list of what does not exist
