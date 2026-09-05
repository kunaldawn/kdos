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
| `testing/docscheck.sh` | That the book still links up and states the present | Anything a reader has to judge for themselves | Seconds |
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

### The window-model contract

`libkwm`'s block asserts nothing of its own. It **replays
`testing/fixtures/wm/geometry.txt`**, every row of which was derived by reading a named line of
`kdos-comp` and cites it. A failure means the library and the compositor have parted company, which
is the one thing sharing a window model between two desktops exists to prevent.

**Add a case by adding a row and citing its line**, never by writing an assertion in
`selftest.c`. A row whose expected value came from taste rather than from the source is worse than
no row: it makes the library authoritative over the behaviour it was supposed to reproduce.

The file is also self-checking in one direction — the `geom` rows are pure arithmetic over the
formula in `view_get_edge_snap_box`, so they can be re-derived mechanically. Doing that caught a row
computed wrong by hand; replaying them against the library then caught four rows transcribed with a
flag inverted. **Both times the fixture was wrong and was fixed, not the code.**

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite is clean that way and stays that way. It found three real defects a plain run could not
see: an archive size field overflowing a signed type — where the negative became an unsigned length
and a read was asked for an impossible number of bytes into a small stack buffer; a copy called with
a null source on every entry's first key; and a read past the end of the cell buffer in the sprite
table's eviction check, which walked `ktui_w * ktui_h` cells of a buffer that is only as large as it
was last resized to.

**A variadic printf wrapper guards its own format**, in libkbase and in the two programs with one
of their own. A null format is undefined in `vfprintf` anyway, and the sanitiser build's
interprocedural pass cannot prove one non-null across a whole program compiled in a single line —
so without the guard `-Wformat-overflow` refuses to build the suite at all, and the sanitiser run
becomes a run nobody can do.

**Leak checking is off by default and on for the library assertions alone.** Every program here owns
its parsed state until it exits, which a leak checker reports as a leak and turns into a false
failure; the library suite is the one binary whose subject is code called repeatedly.

A fuzzer over the image parsers is what found both defects above, in minutes, and it **is**
committed: `testing/fixtures/img/fuzz.c`. Its own driver rather than a block in `selftest.c`,
because the corpus has to run under ASan and UBSan and the rest of the suite does not need
rebuilding to do that. Two passes — every fixture decoded under a budget, then every fixture
mutated a byte at a time and truncated at every length, because a corpus somebody wrote by hand
only ever exercises the paths they thought of. A decoder must answer NULL or an image for any bytes
at all, and must never read past the end of them.

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

**Seventy-six frames** across six sizes, covering the shell's front ends, all ten monitor pages plus
its detail page, the console desktop, the terminal, the cell-level frames, and the six replayed
terminal recordings.

**One of them is the same surface on the other desktop.** `start-console` is the Start menu with
`$KDOS_CON` set, which is what a program started inside a console session inherits — and two rows
differ because of it. A menu whose console-only entries no frame ever drew would be a menu whose
console-only entries nothing checks.

| Kind | Catches | Count |
|---|---|---|
| Text frames (`--dump`) | Geometry: overflow, misalignment, a control drawn past its rectangle | Most |
| Cell frames (`--dump-cells`) | **Colour** and attribute drift as well | Four |
| Replayed streams (`vt-*`) | A change in the state machine, against bytes real programs wrote | Six |

**A terminal's frame is taken by running a command to completion.** `kdos-term --dump` settles the
child and consumes everything it wrote before drawing — a frame taken while a program is still
writing is a different frame every time it is taken. One of the three holds a **picture**: a dump
has no pixels, so a sprite renders as its fallback in the picture's top-left cell and as blanks
under the rest, which is exactly what a tty and a view with no pixel library show. What the frame
asserts is the shape — how many rows the picture took, and where the cursor was left afterwards.

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

**`kdos-view --shot` draws at the font's MAX advance, so a container photograph is spaced out.**
`libkcell` takes its cell width from `max_advance.x`, which for a font with CJK coverage — the
fontconfig default on a build container — is about twice the Latin advance. The glyphs are then
drawn at the left of a cell twice their width. The shipped image's console font has one advance for
everything, so this is the borrowed font talking and not the painter; judge a shot on the image, or
pass `--font`.

**A golden that differs from `HEAD` may be uncommitted work, not drift.** These frames are
regenerated as the tree changes and are often modified in the working tree ahead of the source
change that lands with them — so `git checkout --` on one throws away the very thing it records, and
the next run reports a difference that looks like a flake. `git diff HEAD` on the **source** is what
tells the two apart; run the dump twice if you still cannot.

**A front end that needs a library the harness does not offer is skipped BY NAME.** `kdos-peek`
decodes with `libkimg` and probes archives with `libarchive`, so both the shell-wide compile and the
dump harness admit it only when `libarchive`, `libpng`, `libjpeg` and `libwebp` are all present, and
say so out loud when they are not. Gating the *whole* shell compile on them instead would take the
other forty files with it on a host that simply lacks one dev package.

**`libkdisp` is stubbed, not linked, and adding a `kdisp_` entry point breaks the harness.** The
stubs are in `testing/fixtures/shell/dumpmain.c` and there is no compiler check that they cover the
header — a new one links as "undefined reference" and every front-end frame behind it is skipped.
Linking the real `libkdisp` beside them is not the fix; it is thirty *multiple definition* errors.
Add the stub. A stub answers the state a dump actually has: `kdisp_win_supported()` is 0 because a
dump renders one frame with no compositor, and a stub that invented two windows would make the
frames assert a fiction.

Regenerating is a variable on the self-test, and must be done where the Wayland dependencies exist
— a build container, not a bare host. The terminal's are the exception: it builds console-only on
any host, which is the point of that build.

**Six surfaces have no dump and therefore no frame**: the panel itself, the run box, the prompt,
the notification daemon, the on-screen display and the desktop. That is a stated gap, not an
oversight in this page.

### embedcheck

The parent half of `kdos-cage --embed`, as a test — and a **second process** for the reason
`decocheck` is one: a headless wlroots output, a software renderer, a `memfd` and `SCM_RIGHTS` are
real kernel and library behaviours, and a mock would only assert about itself.

```sh
embedcheck --size 640x480 --out frame.ppm -- kdos-term -e /bin/sh -c '...'
embedcheck --size 640x480 --key 28     -- kdos-term -e /bin/sh -c 'read x; ...'
```

Two assertions, and neither is the obvious one:

- **Not "the frame is not black".** The compositor paints a background, so a frame with nothing
  rendered into it is a uniform colour that is not black either. What proves a guest drew is that
  the frame has **more than one colour** in it.
- **Input is a frame that changed.** The key is typed into a frame that was already drawn, and a
  later frame must differ from it. "The guest changed" is the only thing a parent holding pixels can
  observe about input having arrived — and it is enough, because nothing else moves in a still
  terminal.

Running it needs a linked `kdos-cage` and a guest to render. The suite compiles it, which is what
stops it rotting.

**A guest that draws nothing is a passing compositor and a failing test**, so pick one whose output
you know: a client painting a checkerboard tells you the frame is right pixel for pixel, and a
terminal whose window happens to be the theme's own background colour tells you nothing at all. That
is worth knowing before spending an afternoon on the compositor.

### The sprite wire

A picture crossing `libkcon` is checked **byte for byte**, in the surface test, against a source
buffer whose stride is wider than its width. Checking the metadata is not enough: a picture that
arrives one pixel out of step, or with a length field where its first pixel should be, still has the
right size, the right slot and the right fallback — so a test that looked only at those would pass
while every photograph on the desktop was shifted.

### Goldens and the decoders they need

A golden holding a **sixel** picture is guarded on the sixel decoder being compiled in, not on
`libkimg` being compiled at all. pixman alone builds a `libkimg` with no sixel in it, and running the
picture test against that build produces a diff that blames the terminal.

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
| `vt` | What `vim`, `htop`, `mc`, `less` and `tmux` wrote to an 80x24 pty, plus a hand-written malformed stream | That the libtsm fork's state machine still produces the same screen |
| `pack`, `box`, `deco`, `openwith`, `recent`, `tone`, `cellclip`, `ascii` | | Their respective units |
| `img` | Images, and `fuzz.c` beside them | `libkimg` — every fixture decoded, then mutated and truncated |
| `cast` | A recorded PipeWire stream | `kdos-view --cast`, which rasterises through the same cell painter |
| `embed` | A guest's frames | `kdos-cage --embed` cutting them into sprites |

**Both traps a fixture guards were confirmed to bite** by building the daemon with each check
disabled — which is the only way to know a test is testing something.

**The tray fixture is a second process on purpose.** The protocol is a conversation between two
peers on a message bus, and a mock of either side would have passed on both of the silent bugs that
implementation had.

**The terminal fixtures are never re-derived.** A re-recording picks up a different program
version, a different terminfo and a different hostname, so a fixture that regenerated itself would
be a test that changed its own question — the hostname and the clock inside them are part of the
recording, not something live. Each stops on a **live frame** rather than on the program's exit: a
stream ending with the alternate screen being restored renders to an empty grid, and so does a
parser that gave up on the first byte, so the self-test refuses an empty grid outright.
`vtrender.c` replays one in **small uneven chunks**, because a pty splits escape sequences across
reads and a parser that only works on a whole sequence passes a single-write test and corrupts a
real terminal.

**The same rule applies to the terminal's own input.** `ktui_input_next` reads descriptor 0, so the
bracketed-paste block stands a pipe there and writes the sequence in pieces on purpose: a
terminator split across two reads is the case that turns a paste into a session-long one when the
tail is taken for text. **It restores descriptor 0 before it returns** — a block that redirected
stdin and left it redirected takes every later block that reads a terminal with it.

`testing/fixtures/be` is an empty leftover and records nothing.

## The machine where nothing is skipped

`selftest.sh` runs everywhere and skips what it cannot build, saying so each time. What it skips on
a bare host is most of the interesting half — `libkimg`'s four decoders, the sd-bus blocks, `fcft`,
the Wayland consumers, `libkkms` — and a block that is skipped on every machine is a block nobody
runs.

```sh
testing/devdeps-image.sh                    # builds the image, then runs the suite in it
testing/devdeps-image.sh bash               # or a shell in it
```

`testing/Dockerfile.devdeps` carries exactly what the script probes for. It is Alpine because the
target is musl and a feature-test difference is worth meeting here rather than in a phase build.
**Two guards it still does not satisfy, named in the file rather than left to be found:** `wlroots`,
which no distribution packages and which the next section builds, and `busctl`, which Alpine ships
in no package — so the portal block stays skipped there even though both its other halves are
present.

## Compiling the compositor without a full build

`kdos-comp` needs `wlroots-0.20`, which no distribution packages and which the self-test therefore
reports as skipped. That is not the same as unbuildable: **the tree already carries the source**.

```sh
# ports/core/wlroots/wlroots-0.20.2.tar.gz is a release asset, fetched by bootstrap
FROM kdos-devdeps:latest
RUN apk add --no-cache meson ninja pango-dev libdrm-dev libinput-dev libseat-dev \
      mesa-dev libxkbcommon-dev wayland-dev wayland-protocols hwdata-dev \
      libdisplay-info-dev libxcb-dev xcb-util-wm-dev xcb-util-renderutil-dev \
      xcb-util-image-dev libx11-dev xcb-util-dev xwayland xwayland-dev
# then meson setup / compile / install the tarball with -Dxwayland=enabled
```

`kdos-comp` then configures, compiles and **links** in that image. Two concessions, and both matter
when reading a failure:

- **`-Dicon=disabled`.** `libsfdo` is packaged nowhere available, and the `icon` feature is what
  requires it. Anything guarded by `HAVE_LIBSFDO` is not compiled, so a change in that code is not
  covered here.
- **`-D_GNU_SOURCE`.** `kdos-thumb.c` calls `fileno`, and musl's feature-test defaults differ from
  the ones the real phase build gets.

**Build the unmodified tree first.** With a baseline binary in hand, every error after a change
belongs to the change. What this proves is that a port is type-correct and links; it does not prove
a window lands where a person expects, which is still the rig's job.

## The QEMU rig

**A dump proves a character, never a colour.** `kdos-view --dump` writes the codepoint in each cell
and throws the foreground and background away, so text drawn in the background's own slot — present,
and invisible on every screen — dumps identically to text a person can read. A check on what a
surface *drew* asserts the cell's colours as well as its character; a dump answers "is it there",
not "can it be seen".

**For a cell surface, prefer `kdos-view --dump` over a photograph.** A console session hands out its
exact composited grid, so a check on what a surface drew is a text diff rather than an image
comparison — no boot, no framebuffer, no tolerance for antialiasing. Without a size it takes the
session's own grid, so taking the picture does not resize the desktop. The rig stays necessary for
anything the renderer cannot show: the phosphor pass, a real modeset, a Wayland client.

`testing/vnc-shot.py` boots a real image, drives it, and reads the framebuffer. It is how anything
a dump cannot see gets looked at: the compositor, the wallpaper, the icon layer, a popup anchored to
the wrong corner.

It boots headless with a serial socket and a monitor socket, types on the first terminal through the
monitor, and reads the framebuffer over the remote-framebuffer protocol.

**Which session is already there decides how the rig is driven.** `tty1` runs `kdos-con-login`,
which autologins and starts `kdos-con-start`, so **the cell desktop is up before any step runs**.

`--keys` is a monitor `sendkey`, so it reaches whatever owns the **active VT** — which is that
desktop. `--cmd` runs on the serial console as the desktop user, and `--root-cmd` as root, so
neither disturbs what is on screen.

### The fast loop, for iterating

**`make build` with packaging is seven and a half minutes and five and a half of them are the ISO.**
Repacking 32 GB to carry a 200 KB binary made every look-at-it-on-screen cycle twelve minutes.

```sh
testing/quick.sh kdos-con,kdos-shell -- --keys meta_l-ret --sleep 3 \
                                        --shot /kdos/build/shots/x.png
```

It builds the named ports into `build/fs` with **no packaging** (measured 1m09s), tars exactly the
files those ports own, and hands them to a booted ISO on a raw disk, where `quickpatch.sh` untars
them over the live medium's RAM overlay and restarts the session. **Measured 3 minutes**, or
**1m37s** with `KDOS_QUICK_KEEP=1` and `KDOS_QUICK_NOBUILD=1`.

| Variable | Does |
|---|---|
| `KDOS_QUICK_PHASES` | widen the build, e.g. `04_phase4,05_desktop` |
| `KDOS_QUICK_NOBUILD=1` | reuse what is already in `build/fs` |
| `KDOS_QUICK_KEEP=1` | do not restart the session |

**The file list is the package database's** — `build/fs/var/lib/kpkg/db/<port>` is what that port
installed — so a program that grew a new name or a new data file is carried without anyone
remembering to add it.

**`KDOS_QUICK_KEEP=1` is right whenever the program under test is spawned.** Every `kdos-shell`
surface is started fresh by the chord that opens it, so the new binary runs with no restart at all
and the run is a minute shorter. It is wrong for `kdos-con` and `kdos-view`, which are the session.

Four things it cannot carry, each with what to do instead:

- **Anything under `fs/`** — a config file, a chord table, a service script. Those are installed by
  `01_phase1:00_file_system.sh` and are in no package's file list. Use the real build.
- **A new port, a kernel or an initramfs change.** Same reason.
- **Anything that owns a D-Bus name.** `quickpatch.sh` restarts the session and kills the shell
  surfaces, and even so a notification raised in a restarted session has never been seen to draw
  while the same call on a booted ISO does. Verify the bus on a real boot.
- **Evidence about the shipped image.** What `make build` writes is the ISO; this is the loop you
  iterate in *before* you take the photograph a wave's verify line asks for.

Two traps it removes, both measured the hard way:

- **The session's socket file outlives the process that bound it**, so "the socket exists" is true
  one millisecond after the kill and the steps then run against the binaries the patch replaced.
  The wait is for a *different pid*, and the stale sockets are removed — `kdos-con-start`'s own
  readiness test is that file, so leaving it makes the icon layer and the notification daemon start
  before the new session has bound anything, and they are not supervised.
- **`/etc/inittab` respawns `tty1` and it does not come back** when the chain is killed. The script
  starts `kdos-con-login` itself, which is `kdos-con` under another name and does exactly what the
  getty would.

### Photographing the console desktop

Every wave of console work verifies this way, so the recipe is here once rather than rediscovered
each time:

```sh
R="docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
   python3 testing/vnc-shot.py --size 1280x800"

$R --no-session \
   --sleep 40 --keys esc --sleep 2 --shot /kdos/build/shots/con-desktop.png \
   --keys meta_l-ret --sleep 4 --shot /kdos/build/shots/con-terminal.png
```

Five rules, each with the consequence of getting it wrong:

- **`--no-session`, and the wait is a `--sleep`.** `--wait` settles a *graphical* session and is
  skipped entirely when none is started, so a `--wait 45` before the first `--shot` photographs the
  boot banner at seven seconds of uptime. Forty seconds of `--sleep` is what the ISO takes to reach
  a drawn desktop.
- **`esc` closes the welcome card, and nothing else does.** It opens focused on first login over
  the top-left of the grid. Its hint row says *Any key close*, and the key has to reach it: a chord
  the session binds is taken by the session first.
- **The harness writes raw PPM whatever the extension says.** Convert before comparing, or an image
  library reads the file by its magic and the diff is against a header.
- **Read the display's own report before reading the screen.** `$XDG_RUNTIME_DIR/kdos-view.log`
  carries one line naming the mode, the CRTC, the connector, the seat state, the cell size and the
  grid — a desktop that comes up on the wrong output or at the wrong size says so there, where the
  photograph only shows that it looks wrong.
- **A grid that is not redrawn looks identical to one that is.** Two shots a minute apart with the
  clock reading different minutes is the cheapest proof that flushes are reaching the screen.
- **`pkill -t tty1` selects nothing** — toybox's `pkill` has no terminal predicate, and it fails
  silently. Find the pid with `ps -eo pid,tty,comm` and kill that.
- **Restarting the login chain does not free the screen.** The session and its view outlive the
  login shell by design, and the view holds DRM master — so a new `kdos-con-login` draws its
  greeter onto a screen it does not own and the photograph shows the old desktop. End `kdos-view`,
  `kdos-con` and `kdos-con-start` alongside the shell.
- **`greet` cannot be tested on the live medium by editing `/etc`**: the setting is read at login,
  and the overlay resets on reboot, so a boot-time test of the greeter needs an installed system.

**The graphical session is started on a VT and never down the serial line**: a compositor launched
from a serial console gets no seat and dies asking for one. Its entry point from the console desktop
is the Start-menu row that allocates a free terminal and switches to it.

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

The rig runs unmodified inside a container image that carries one, with the repository bind mounted
and hardware virtualisation passed through. **The image is built from this tree**, by
`testing/rig-image.sh` out of `testing/Dockerfile.qemu`:

```sh
testing/rig-image.sh          # network, once; everything after it is offline
```

It carries QEMU, the OVMF firmware an EFI boot needs, and python3 — and nothing else, because
`vnc-shot.py` speaks the remote-framebuffer protocol out of the standard library. There is no pip
step and no wheel to pin, so the only versions that matter are the distribution's QEMU and OVMF.

Building it here rather than describing a machine's history is what makes a photograph reproducible
from a clone: an image that exists only where it was first built is a test nobody else can run.

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
- **Surface goldens regenerate in `kdos-devdeps`, with the host's fonts bound in.** The image
  carries the Wayland dependencies the front-end dumps need but no fonts at all, and `asciicheck`
  aborts the run before it reaches them — so `fcft: monospace: failed to match font` reads as a
  libkcell failure when it is a missing mount:

  ```sh
  docker run --rm -v /usr/share/fonts:/usr/share/fonts:ro -v "$PWD:/kdos" -w /kdos \
      kdos-devdeps:latest sh -c 'fc-cache -f; KDOS_GOLDEN_UPDATE=1 testing/selftest.sh'
  ```

  `os-dev` has the packaging toolchain and no `wayland-client`, so it skips the front-end dumps
  entirely; neither image runs the whole suite, and `kdos-devdeps` still has no `fakeroot` for the
  reproducible-build check.
- **A `--root-cmd` or `--root-script` round trip outlives a five-second toast.** It runs over the
  serial console and waits for a prompt, which takes longer than the notification it raised stays
  on screen, so the shot that follows photographs an empty desktop and the notification path reads
  as broken. Raise a toast with `--type`, which goes over the keyboard and is quick, or ask for a
  timeout longer than the rest of the run.

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
- **`kdos-settings` has its `golden` calls and no committed goldens.** The first run in a container
  that can reach the front-end dumps writes them. **A golden no `golden` call drives is worse than
  none:** nothing compares it, so it agrees with the surface only until the surface changes, and it
  reads to the next person as evidence that was checked.
- **The compositor and the shell are not compiled by the self-test on a bare host.**
- **Nothing here tests the build**, which takes hours and a container.

## See also

- [Developing](developing.md) — the loops these fit into
- [Writing desktop software](writing-desktop-software.md) — dumps and reference frames from the author's side
- [The C libraries](c-libraries.md) — what the assertions cover
- [Build troubleshooting](build-troubleshooting.md) — when preflight is not enough
- [Known gaps](../06-reference/known-gaps.md) — the full list of what does not exist
