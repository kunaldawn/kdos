# Testing

This chapter describes how to find out whether a change to KDOS works without waiting for a full
build, which takes hours. It is written for anyone changing the tree: a recipe, a library, a
desktop program, a shipped configuration file or a page of this book. Each harness under
`testing/` is described in turn with what it proves, what it cannot prove, how long it takes and
how to run it. Read [Developing](developing.md) first for the build and its iteration loops, which
these harnesses fit into.

KDOS has no unit-test framework and ships no test binaries. Its checks fall into three families:

- **Offline checks over the tree**, which need no build and no emulator. `preflight.sh` checks the
  wiring of the tree in two to three minutes. `selftest.sh` compiles the C libraries and every
  program that uses them and runs their assertions in a few minutes; two of its parts are the
  *goldens*, committed reference frames of every surface that can be drawn deterministically, and
  the *fixtures*, recorded system state that a program can be pointed at instead of the live
  machine. `docscheck.sh` checks this book.
- **A source check**, `make fetch-check`, which confirms every upstream source a recipe names is on
  disk and matches its hash.
- **Boot-time harnesses** built on the *rig*, a virtual machine that boots the real image, drives
  it and photographs it: `vnc-shot.py` is the rig's driver, and `quick.sh`, `usability.sh`,
  `packlane.sh`, `install-to-disk.sh` and `bios-boot.sh` are built on it or beside it.

A *surface* is one window or popup of the desktop drawn by KDOS. The terms *surface*, *dump*,
*golden*, *fixture* and *rig* are also defined in the [glossary](../06-reference/glossary.md).
Start with [What each tool proves](#what-each-tool-proves) and
[Which one to run](#which-one-to-run), then read the section for the harness you need.

## What each tool proves

| Tool | Proves | Cannot prove | Takes |
|---|---|---|---|
| `testing/preflight.sh` | The wiring: every reference resolves, every option exists, every script parses | That anything works | Two to three minutes |
| `testing/selftest.sh` | The libraries' invariants, that every consumer still compiles, that the build orchestrator and package manager behave, and that surfaces still lay out | That the system boots | A few minutes on a bare host |
| Goldens | That a surface's geometry and colour slots have not drifted | That it is usable | Part of the self-test |
| Fixtures | That a reading or a decision is correct against recorded state | That the reading is correct on a live machine | Part of the self-test |
| `testing/vnc-shot.py` | That a real session does a thing, photographed | Anything the renderer in use cannot show | Minutes per boot |
| `testing/quick.sh` | That a changed program behaves on a booted desktop, without repacking the ISO | Anything about the shipped image | About three minutes |
| `testing/usability.sh` | That the desktop can be driven by hand (hover, click, chord), photographed step by step | Nothing on its own: it asserts nothing and a person reads the result | About six minutes |
| `testing/docscheck.sh` | That this book's links resolve, that it avoids the common historical phrasings, and that each page keeps its contract | Anything a reader has to judge | Under a second |
| `make fetch-check` | That every source a recipe names is on disk and matches its hash | That the sources build | About a minute, offline |
| `testing/packlane.sh` | The pack lane (installing boxed applications from packs; see [Packs and boxes](../03-architecture/packs-and-boxes.md#two-lanes-one-box)) end to end on a booted machine | Anything without application packs to install | Minutes |
| `testing/install-to-disk.sh` | That the installer installs | That the installed system boots, which is the second half of the run | Minutes |
| `testing/bios-boot.sh` | That the image boots on a machine with no UEFI firmware at all | Anything about the UEFI path | About two minutes |

None of them proves the build works. A package manager and a build orchestrator are only fully
tested by building the distribution with them.

## Which one to run

| You changed | Run |
|---|---|
| A phase list, a recipe, a `depends` line, anything under `script/` or `fs/` | `testing/preflight.sh` |
| A library under `src/libs/`, or a program of KDOS's own | `testing/selftest.sh` |
| A parser | `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh` |
| A surface's layout or colours | The self-test in the development image (`testing/devdeps-image.sh`); if the change is intended, regenerate with the command under [Regenerating](#regenerating), then read `git diff testing/goldens/` |
| A recipe's `sha256 =` line or its sources | `make fetch-check` |
| Something only a running desktop shows | `testing/quick.sh`, then the rig on a real ISO |
| A page of this book | `bash testing/docscheck.sh` |
| Nothing, but a build finished and a phase is about to be built with `--port-jobs` | `python3 testing/depdrift.py` in the development image |

## preflight.sh

Preflight checks the tree's wiring without building anything. It runs 56 checks:

```sh
testing/preflight.sh
```

It first compiles its own copy of `kpkg` (the package manager) from `src/`, into a temporary
directory, and invokes it under the five names it is installed as. It then runs the checks, each
headed by a `==>` line. It exits 0 when every check passes and 1 when any fails, prints each
failure with the file to look at, and ends with either `preflight clean` or the number of problems
found.

Ten checks read the built tree under `build/fs`. Six of them skip entirely without one: orphaned
packages, the built `kinstall`, icon names, chrome glyphs, desktop entries, and the libraries the
desktop's own programs link (which also skips on a host with no `readelf`). Four run a reduced
check and say what they left out: the programs a recipe's embedded scripts name, the programs
`fs/etc/inittab` names, the `mimeapps` rows (only the `kdos-*` handlers are checked, against `fs/`
and `src/`), and the check on the build tree's root (its udev grants, daemon accounts and group
membership are read from `fs/`; the file modes, rule ownership and stray-entry checks are
skipped). A skipped check prints `skipped — no build tree` rather than passing quietly, so a
green run accounts for every check in the file. A partial `build/fs` counts as a build tree, and a
check that finds its directory there reports what is missing from it.

| Group | Checks |
|---|---|
| Layout | Every recipe under `ports/core` sits exactly at `ports/core/<shelf>/<name>/kpkgbuild`, and `ports/core` holds nothing but shelves; `ports/shelves` lists every shelf once, each with a description, an id of lowercase letters, digits and `-`, not `libs` or `core` and not a port's name, and each a non-empty directory; `src/` holds only its six areas and every port of KDOS's own sits at `src/<area>/<name>/`; the orphan sweep searches every `src/` area that holds a recipe; every bare name is one port across the shelves and the `src/` areas; every `name =` is its directory's name; every `group =` family sits on one shelf; every file of a `packages.d/` is `00-order.txt`, a listed shelf's `<shelf>.txt` or a `src-<area>.txt`, and names only ports filed there, and no phase has both `packages.txt` and `packages.d/` |
| Packages | Each package phase from `30_foundation` on installs exactly the ports its list names, computed by `testing/phaseclosure.py` (see [Every phase installs exactly its list](build-system.md#every-phase-installs-exactly-its-list)); every package named in a phase list has a port; ports built from one tarball (`linux` and `perf`) agree on its version and hash; every list resolves to a dependency order with clean output and valid tokens; every `depends` entry names a port that exists; every port of KDOS's own is built by something; the build tree carries no package whose port is gone |
| Recipes | Every port has a build script and it parses; every recipe parses as metadata; every one declares a name, version and release; every recipe and build script carries the KDOS banner; every source a port declares is named by a checksum and is non-empty where it is on disk (one not yet fetched is reported as a `make fetch` note, not a failure) |
| Build options | Every meson option a recipe passes is one that port defines, checked against the tarball's own option file, with the two closed-value option types validated; every recipe that runs `meson setup` names its buildtype; every `go build` and `go install` passes `-s` and `-w` in its `-ldflags`, read with its backslash continuations joined; every recipe that runs cargo builds against a shared C library; every consumer of a shared library generates the Wayland protocols it includes |
| Sources | Every source file in a port of KDOS's own is compiled by its recipe, unless that recipe globs its own `$PORT_SRC` directory (a glob of the shared `libk*` trees does not exempt it); a first source whose members are `./`-prefixed is accounted for; a flat first source is unpacked by its own recipe |
| The sources archive | No archive a recipe hashes is tracked by git; `.gitignore` ignores every archive suffix under `ports/core` and the source cache, and none of the archive fixtures under `testing/fixtures`; `ports/srclib.sh`, `ports/fetch`, `ports/publish` and `script/hooks/pre-push` exist, are executable where they must be, and pass `bash -n`; `ports/sources.idx` holds every format-2 rule: `# kdos-sources-index 2` on line 1, each line `<sha256> <tag> <asset> <port>/<file>` with an optional `parts=<N>:…` of 2 to 99 hashes, each tag `src-<a listed shelf>` or `src-attic`, one line per hash, no part hash equal to a line's hash, no two asset names in one tag equal ignoring case (a split file's parts and its bare name counted), and hash order. An unset `core.hooksPath` is printed as a note, not a failure |
| Shipped configuration | The shipped compositor configuration keeps labwc's default bindings; every command it, the menus and `menu.conf`'s routes name exists; every program `fs/etc/inittab` names is on the image; every filesystem the installer offers, the initramfs can mount; the ISO step builds every boot path (BIOS and UEFI, disc and written stick); the built `kinstall` is the installer in this tree |
| Permissions and accounts | `/etc/shadow` on the built tree is mode 600 or 640 and the file-system step narrows it; the polkit and udev rule directories and files are owned by root; no udev rule sets `GROUP=`, `MODE=` or `OWNER=` on a device class that has no node, and every group a rule grants is one the desktop user is in; every account a shipped daemon drops to exists in `fs/etc/passwd`; the desktop user is in the groups its surfaces need |
| Shell | All shipped and build shell is syntactically valid; every file a build script, a `phase.env` or a shared environment file sources exists, and a step sources its own phase's `phase.env`; a script a recipe ships inside a `KDOS_SH` heredoc parses too, and every program it names as the first word of a line is one the image carries; no build script names a command inside double quotes and runs it; every helper the Makefile runs is on disk |
| Consistency | The build tree's root carries nothing but a root filesystem; no installed ELF file of a port under `src/` names a GTK, libadwaita, WebKitGTK, Qt, KDE Frameworks, wxWidgets, FLTK, Tk or Xlib library in its `NEEDED` entries, so the desktop links no toolkit while the applications may; every flag one shell tool passes another is one it accepts; every daemon an init script starts is installed by a port; the root filesystem carries no script whose interpreter is gone; nothing points at a removed file; every `port:`, `path:`, `see:` and `cite:` a recorded reason names still resolves; no chroot step reads the ports tree through `/kdos/ports`; the compiler cache sets no `base_dir`, checks the compiler by a string rather than a modification time, has its `KDOS_CCACHE` switch both passed by the `Makefile` and named by `script/chroot/exec.sh`, and puts no masquerade directory on a `PATH`; the catalogue's rows match the tree; the application store is wired everywhere it has to be; a desktop toggle has one flag and only `libkbase` builds its path; only `kdos_crt_scanout()` in the compositor writes the scene's direct-scanout switch; a frame that opens the synchronized-output bracket closes it on every path; a literal colour is set at the render boundary and nowhere else; the control centre's row table agrees with the files it writes |
| Shipped data | `mc`'s shipped rows name programs that exist; every `kdos-*` handler in the shipped `mimeapps` lists is on the image; every help page names a document that ships; the generated `aerc` styleset is one `aerc` will load |
| Chrome | Every glyph in `libktui`'s UTF-8 table is one the shipped console font can draw; every icon name a surface asks for resolves on the image; every visible desktop entry's icon and command exist on the image, and no two visible entries share a `Name=` |

Preflight cannot prove the build works. It proves the build will not fail for a mechanical reason
such as a missing port, a misspelt option or a script that does not parse. Run it after touching a
phase list, a recipe, or anything under the build scripts.

### Checks that catch what a compiler cannot

Four of these checks catch a whole class of failure that never reaches a compiler.

The first is that every source file in a port of KDOS's own is compiled by its recipe. A file the
build script neither names nor matches passes every gate on a development machine, because the
self-test globs those directories, and then fails to *link* in the build. A whole page can be
exercised by the harness and be absent from the shipped binary.

The second is that every meson option is one the port defines. meson fails at setup on an unknown
option, before a line is compiled, and there is no universal spelling. Without this check a
misspelt option costs an hour-long round trip through the build to find.

The third is that every chrome glyph is one `ter-kdos32n` carries. The toolkit picks its UTF-8
glyph table whenever the terminal reports UTF-8, which the Linux console does, and the console font
holds 512 glyphs. An entry that font lacks renders on `tty1` as a blank. It is not a fallback and
not an error: the cell is written, the flush succeeds, and a piece of the desktop's own chrome is
missing. `▓`, the half blocks `▀ ▄` and the double tees `╠ ╣ ╦ ╩` all look reasonable in an editor
and none of them is in the font. The check reads the shipped font itself, because a hand-kept list
of what it carries is the thing that goes stale. Goldens cannot catch this: they are dumps at the
ASCII [glyph tier](../06-reference/glossary.md), where every one of those glyphs resolves to
something.

The fourth is that every flag one shell tool passes another is one it accepts. The shell's tools
start each other by name, and an unknown argument prints a usage line to an error stream nobody
reads and exits before a surface exists. The result is a control that silently does nothing,
invisible to a compiler and to a reference frame. The check is deliberately crude: it finds the
argument literals, takes every long flag, and requires it to appear in the target's own source.

## selftest.sh

The self-test is the host-side regression suite for `src/libs/` and everything built on it.

```sh
testing/selftest.sh
```

It compiles everything with the host compiler into a temporary directory, with
`-Wall -Wextra -Werror`, and runs each block in turn, printing a `==>` heading per block. The
blocks fall into these groups, in roughly this order:

1. **The libraries.** Every library is compiled and the shared assertion program,
   `src/libs/selftest.c`, runs against them. Where `libkimg`'s decoders are present, the image
   fuzzer runs next (see below).
2. **The consumers.** `kinstall`, `kdos-appbox`, `kdos-theme`, `kdos-tools`, `kdos-kpkg`, the
   daemons, `kdosbuild`, `kdos-portup`, `kdos-packd` and `kdos-pack` are compiled against the
   libraries to prove the headers still agree with their callers, and several run their own
   `--selftest` against fixtures. The Wayland consumers (`libkwl`, `kdos-lock`, `kdos-shell`,
   KDOS's own source files added to the compositor and `kdos-boxsock`) follow, each gated on its
   libraries.
3. **Boxes, packs and the terminal.** Box profiles, the pack store and its signature checks, and
   the terminal emulator library replaying recorded streams.
4. **Packaging and the build.** The version checker against a recorded corpus, offline; the ports
   tree resolving through `kpkgdepends`, and `libkpkg`'s lookup over a fixture tree of shelves (a
   port found one shelf down or loose beside the shelves, a name filed twice or nested below its
   shelf refused, a shelf with no port ignored); a package built twice being byte-identical; the
   shared indexes, file ownership, skip-if-installed, the package store (a hit installs the same
   files without running `build.sh`; a dependency's changed bytes, a changed `CFLAGS`, a
   dependency without `.pkgsha`, a truncated stored file and `-f` all build; a job count does
   not; an undeclared link is recorded as `X:` and a change to it misses; check mode logs a
   port that differs on every build and exits 0; `store gc` evicts the oldest), the binary
   host's signed index and delta packages; `kdosbuild` running a synthetic two-phase build end to end;
   `kdosbuild` writing a three-phase build's snapshots as a full archive and then layers, each
   restored and compared entry by entry with the tree its phase left, a retaken phase written full
   with the old chain held until nothing needs it, a deleted base held for its dependants and
   freed after them, a schema-3 snapshot restored and layered on, and a snapshot whose base is gone
   refused as unusable; and `kdosbuild` running no
   step for a port installed and current in a chroot package phase over a real database, never
   announcing one as running, and running it after all when its recipe is edited while an earlier
   step of the phase runs; and `kdosbuild --port-jobs` over a stub `kpkg`: the order run first,
   a level's ports building side by side, committed together in serial order before the next level
   starts, each given its share of `KDOS_JOBS` and its CPU window, an ownership inversion named, a
   failure letting its running sibling finish and committing nothing, every step reported exactly
   once, and `--port-jobs 1` running exactly the serial commands.
5. **The system programs.** Timers, reminders, the boot slots and their rollback, the initramfs's
   encrypted-root unlock, the installer's plan, the polkit rules, the power, energy,
   out-of-memory and mount daemons, the tray and the file-chooser portal.
6. **The surfaces.** Every surface of the shell rendered offscreen, the key-row contract (every
   frame draws a bottom row naming its keys; see [The key row](#the-key-row)), then every golden
   compared (see [Goldens](#goldens)).
7. **The last checks.** The recording indicator, the CVE comparison, the four list views, the
   [tone ladder](../03-architecture/design-language.md#the-pixel-layer-under-the-cells) (the rest,
   hover and focused tones a plate is painted in), and `kdos theme --audit` over the shipped
   artwork.

It needs no container and no network. On a bare development machine it takes a few minutes and
skips, by name, the blocks it lacks libraries for (see
[What it does not cover](#what-it-does-not-cover)).

The suite keeps its own state directory: it sets `XDG_STATE_HOME` to a directory inside its
temporary output, so a surface that remembers where its window was does not read back what a
previous run, or the developer's own desktop, left behind.

Its assertions are the libraries' invariants. Each states a property that was measured, and the
suite notices when it stops being true. One is counter-intuitive: the two colour-mixing functions
are asserted to *disagree*, because each generated file was written against exactly one of them.

A test whose only subject is code nothing ships is not coverage. It reports green while the
behaviour a person sees goes untested. When the last shipped caller of something goes, its
assertions go with it.

### The partial paint

`libkwl` repaints a buffer in part and reports only the changed cells as damage. Over a backdrop
it keeps a buffer's backdrop, repaints the cells over whatever pixels the picture moved since that
buffer was painted, and lays each repainted cell back on its band of the cached picture, which
`libkchrome` re-rasterises only where its op list changed (see
[Presenting a frame](writing-desktop-software.md#presenting-a-frame)). All of it fails as stale
pixels, never as a crash, so `testing/fixtures/kwl/paintcheck.c` checks it against what it must
equal. It includes `kwl.c`, so the real flush runs with its real state, and defines
`wl_proxy_marshal_flags()` itself, which every generated protocol call reaches: the simulated
compositor copies into its own screen exactly the damage each commit names and then requires the
screen to equal the buffer it was handed, a buffer that claims an opaque region to be opaque
in every pixel, and the surface size it derives (the viewport destination where one is set, else
the buffer over its scale) to be the logical size the surface was configured to. A seeded walk drives it through hovers, two translucent plates swapping which is
on top, a plate at any pixel position, plates dropped from the middle of the list, a plate on the
rule or past the last cell, display text two rows high whose figure changes (on the backdrop's own
slot, on a band of another slot, centred), clock ticks, a caret, edits in double-width text, the
taskbar's alpha, a retint, night light, a popup's backdrop or a flat body replacing the bar's, no
backdrop, a resize, a scale change to a whole number or to a fraction (a buffer of device pixels
behind a viewport), the list scrolled by a line, back and by a page (with shade
characters and plates that travel with their rows) and a compositor holding a buffer, at two font
sizes whose cell heights differ in parity. A `scroll` run ends each walk with the list scrolled
under an op past the last cell over a bare body, and then over a flat body under changing display
text. Each seed runs twice, once as shipped and once under `KDOS_PAINT_FULL=1`, and every
committed buffer must hash the same in both; a run in which no commit repainted a moved picture in
part fails as well, and so does a run with no commit at a fractional scale, or a `scroll` run in
which no paint moved a band, or none moved one over a backdrop. Before the walks it checks the
[fractional scale](c-libraries.md#the-fractional-scale)'s conversions — every device pixel up to
600 at five scales lies in the logical pixel it is converted to and inside the extent that covers
it, and so does a pointer position at every 37th 256th of a pixel — and which path each scale takes:
1.5 and 1.25 with a viewport load the font at the device size with a cell no larger than the named
cell times the scale, 2 is the integer path over the font as named, and 1.5 without a viewport is
rounded up to 2. Before each walk it checks `libkcell`'s scroll on its own: a list scrolled up by
three rows and down by two under a fixed first row must yield exactly the band that moved, leave
only the exposed rows (and the one fed by the half-clipped last row) for the diff, and repaint to
the same pixels as a full paint; three lines of text scrolled under eight blank rows must yield
the two text rows, not the longer run of blanks; and two changed cells must not be taken for a
scroll. Each of these makes it fail: dropping the
band restore, the per-buffer key comparison, the screen-key check, the span widening, the popup's
opaque test, the moved cells, the moved damage, the op order (a set comparison instead of the
common subsequence), the vanished ops, the grid bound on moved pixels, the step from a
continuation to its lead, a cache clip one pixel off, and, for a scrolled band, the backdrop's
row comparison, the moved-pixel check on a band's source rows, the rule offset, the stale mark on
a row the picture differs under, the shade phase, the order of the moves, the shadow moving with
the pixels, the stale mark on a row fed by a clipped one, the diff taken again after a move, a
band one row too long, or scoring a shift by its length instead of the rows it spares.

A `glide` run declares the list a gliding view (`kwl_list_view`) on every frame, drives the
animation clock by hand and puts the library's own glide frames between the surface's, so the walk
also requires every commit in between two positions to be damaged and to hash the same under
`KDOS_PAINT_FULL=1`. Each glide walk ends with a list alone on the surface, scrolled by a line, by
three, back, by several while one is under way and by more than it holds, with glide frames between
the draws: every commit's list must equal the list painted independently from its cells at the
position the glide presented, the rows at `top` moved by the lag and the rows it opens taken from
the row before. Far past the end nothing may be owed or committed, and a list that stops being
declared while it is in between must get the commit that puts its own picture back. A glide run
in which no commit presented a list in between fails. Dropping the glide's damage, the stale mark
in the shadow, the continuation from the lag on the screen (starting each step from rest), the
reset of the lag once it lands, the commit owed to a list that stops being declared, or the gate that lets
a glide frame through with no cell changed, taking the copy from the buffer about to be painted
instead of the one on the screen, or reading the old picture a row off, each makes it fail. It runs
where `libkwl` is compiled, which is the development container and not a bare host.

### The inspector

`KDOS_INSPECT=1` draws a developer overlay over every `libkwl` surface (see
[Presenting a frame](writing-desktop-software.md#presenting-a-frame)): the rows each commit
changed, tinted and fading; a panel of frame numbers; and outlines round a frame surface's hit
rects. It is how to see, live and with no rig photograph, which rows repaint on every frame and
whether a surface is throttled or stalled. Start any surface under it:

```sh
KDOS_INSPECT=1 kdos-res
KDOS_INSPECT=1 kdos-settings        # a frame surface: its hit rects are outlined too
```

The overlay is pixels in the shared-memory buffer only, over a copy of the cells, so no `--dump`,
golden or `tty1` frame ever carries it, and it makes every commit a full paint and a full damage
while it is on. Two checks hold it to that. `testing/fixtures/kwl/inspcheck.c` drives `kwl_insp.c`
with the clock handed in: the panel lands on a copy and the frame handed in is untouched, a wide
glyph the panel would cut loses both halves, a changed row is tinted over exactly its changed cells
and fades out in a second, a full commit lights every row, the numbers are right and the overlay's
own refreshes feed none of them, the refreshes come a tenth of a second apart while a tint fades and
stop on a still surface, and the hit outlines take their slots and leave the rects' insides alone.
Counting the refreshes' own commits, pacing the fade from anything but the last paint, dropping the
wide-glyph rule, ignoring `full`, or not fading each make it fail. `KDOS_INSPECT=0` and an unset
variable must both leave it off. And `paintcheck` runs once more with the overlay on, where every
commit must still leave its simulated screen equal to the buffer it was handed: an overlay commit
that was not damaged whole fails there.

### The frame clock

An animation's ticks fail in two directions, and neither crashes: an idle surface that wakes, asks
for frames or commits when nothing moves, and a moving one that freezes because no frame was on
its way. `testing/fixtures/kwl/tickcheck.c` includes `libkwl`'s own source, as `paintcheck` does,
records every commit and frame callback at the wire, and makes the display's socket a pipe the test
writes to when it wants the compositor to answer, so the real `kwl_poll_event()` runs, `poll()`
and all. An idle wait must run its whole length with no event, no commit and no frame asked for,
and an ordinary commit's answered callback must not become a tick. With an animation live, a wait
must ask for a frame with one empty commit, return the answer as a `KT_EVT_TICK` at once, turn an
unanswered callback into a tick at the stall bound and drop it, owe exactly one frame after the
end, and fall silent after it even when the loop never draws. It also reads `comp.conf`'s `motion`
as the compositor does: the last line wins, a commented line is a comment, and a changed file is
read again. Ticking idle, committing idle, owing forever, owing nothing, keeping the stalled
callback, skipping the empty commit, returning the tick as a timeout, not cutting the wait, the
first line winning and not re-reading each make it fail.

The same fixture drives the wheel and the coast through the real pointer handlers. On a version 8
seat a high-resolution wheel's thirds of a detent must add up to one tick, two detents in one frame
must be one tick, and a half one way and a half back nothing; the raw stream must carry the frame's
own `value120` and the natural-scrolling bit; on a version 5 seat the discrete count still makes one
tick a frame; the duplicate gate must drop a second detent inside its window and keep a finger's two
ticks a millisecond apart. A flick must coast in the finger's direction at its release speed, slow
down and end before the time limit, travelling about a quarter of a second of the release speed; a
press must stop it; a finger held still before lifting, one too slow, and `motion = no` must not
coast; the pointer leaving must stop it; and a wait during a coast must return its ticks or run its
whole length. Gating fingers, honouring a frame's whole count, not resetting on a reversal, no
decay, no release window, returning a coast step as a timeout, or a press that does not stop it
each make it fail. The curves, the end value on the terminal,
a capture backend and with motion off, the live window and the pulse are `test_ktui_anim` in
`src/libs/selftest.c`.

### The selection plate check

The plate a menu's selected row stands on slides to a new row (`kch_px_row_anim`, see
[libkchrome](c-libraries.md#libkchrome)), and a golden is a cell frame with the pixel layer stubbed,
so a plate in the wrong place for a few frames reaches no golden. `testing/fixtures/rowanim/rowcheck.c`
includes `kch_px.c`, stubs the `libkwl` calls it makes, installs a backend whose only answer is
whether it keeps a frame clock, and drives time through `ktui_anim_set_clock()`. At rest the plate
must be the ops `kch_px_row()` records. A new item must start on the old row, never go back or past
the new one, be past half way before half time (an ease-out), land at `KCH_ROW_ANIM_MS` and keep
the clock live until then and not after; a move into another column must slide `x` and stretch `w`;
a redirect mid-slide must set out from where the plate stands. The same item somewhere else, even
mid-slide, a plate missing from the list before, a changed cell size, a backdrop installed afresh,
no frame clock and motion off must each land at once, and a move onto the row the plate already
stands on must start nothing. Four keys are four plates, and a fifth must evict the one asked least
recently, in whichever slot, and disturb no other. Dropping the scroll landing, the list count, the
cell size, the clear on install, the mid-air start, the ease-out, the same-row guard, the
least-recent eviction or the bar's place on the plate each makes it fail. It needs `fcft` and
`pixman`, so it runs in the development container.

### The display text check

Display text (`kch_px_text` and `kch_display_text`, see [libkchrome](c-libraries.md#libkchrome))
reaches a golden only as its cell form, so `testing/fixtures/disptext/textcheck.c` includes
`kch_px.c` with the `libkwl` calls stubbed, links `kch_chrome.c` for real, loads a cell font and
replays into images of its own. With no backdrop it must record nothing, and `kch_display_text()`
must fill the rectangle with its band and draw the string on its first row, aligned. Under a popup
the rectangle's cells must go blank on the backdrop's slot, with one text op two rows of the cell
tall, preceded by a flat rectangle of the band's slot only when that is another slot. The key must
follow the string and not its offset in the pool, and the differ must answer exactly the changed
text's rectangle. A string past the pool, or text and its band with one op free, must be refused
with nothing recorded and no cell touched. Replayed at scale 1 and 2, no pixel may be inked outside
the rectangle, even for a string too long for it; right- and left-aligned ink must sit at the side
asked for; and a replay under a clip region must equal the whole replay inside the clip and leave
everything outside it untouched. A flat body must be its slot's colour, claim opaque, and answer
the same rows until display text reaches them. Dropping the glyph clip, the string from the
comparison, the alignment, the row height, the band, or the flat body's row answer, or comparing
the pool offset, each makes it fail. It needs `fcft`, `pixman` and a font, so it runs in the
development container.

### The tile lifetime check

A pixel tile larger than 16×16 cells is a grid of sprites over views of one canvas (see
[libkchrome](c-libraries.md#libkchrome)), and both ways it can break are silent: a block cut at the
wrong origin shows another block's pixels, and a view the sprite table's evictor unrefs once too
often is freed while the tile still names it. `testing/fixtures/tile/tilecheck.c` links
`kch_tile.c` with `libkcell` and `libktui`, stubs the display's cell size and the icon switch, and
reads the pixel every drawn cell resolves to against what was rasterised, while a destroy function
hung on each view counts its frees. It runs a 40×20 tile (six blocks) under `kcell_tile_free`, the
evictor the shell registers, through alternating halves, a full table evicting the undrawn half, the
published half evicted while undrawn and put back with no raster, a cleared table, and a byte budget
that refuses a half, including the case where every put succeeds by evicting a block the same commit
had just put. A 16×2 tile, the meters strip's shape, must be one sprite under its old key, and
without an evictor a refused put must take back the blocks put before it. After a reset every view
must have been freed exactly once. Taking no reference for the table, taking one on a re-put,
dropping the evicted-half check, the whole-half check after the puts or the take-back on a refusal,
cutting blocks at the wrong origin, or letting `kch_tile_slot()` answer for a half with a block
missing each makes it fail. It needs `fcft` and `pixman`, so it runs in the development container
and not on a bare host; built with AddressSanitizer on glibc, a missing reference is also a
use-after-free report.

### The chart pixel check

Every golden is a cell frame, and a chart is pixels only where a display has them, so the
antialiased chart the panel's meters and `kdos-res` draw (see [libkchrome](c-libraries.md#libkchrome))
reaches no golden. `testing/fixtures/plot/plotcheck.c` links `kch_plot.c` and `kch_tile.c` with
`libkcell` and `libktui`, sets a palette of its own so a change to a shipped theme moves nothing, and
checks two kinds of claim. The first holds whatever the arithmetic: a series, a line, a fill or a
chart writes nothing outside its rectangle or the clip; every pixel stays premultiplied; a series at
rest is exactly its bottom row at the rest weight; a sample above zero lifts the trace off that
row; a series at full scale fills its band; a mirrored series is the upright one flipped; the oldest
value reaches the left edge only when held; a line is the same bytes drawn from either end, a steep
line is the shallow one transposed, and a line's total coverage is its width times its length; a
nested clip cannot widen its parent, the ninth push is refused and a clear empties the stack; the
pair's midline is at half the height and the gridlines are at the sample numbers they are keyed to;
`kch_plot()` keeps its slot for unchanged content and alternates for a new sample number; and a
marked sample is its own column top to bottom, over everything and nowhere else, and part of the
hash. The
second is a digest of whole canvases for fixed inputs. The marks are fixed point from one conversion
per input, so a digest is the same on every build; one that moves is a picture that changed, and a
change made on purpose updates the digest in the file, which `plotcheck --print` prints. Removing
the one-pixel floor, the rest weight, the mirror, the hold, the slope term or the transposition of
a steep line, the pixman half of the clip, the clear's reset, the premultiplication, the right and
bottom bound, the interpolation between samples, the sample number from the hash, the midline gap
or the chart's own clip, or moving a gridline by a pixel, each makes it fail. It needs `fcft` and
`pixman`, so it runs in the development container and not on a bare host; it is also clean under
AddressSanitizer and UndefinedBehaviorSanitizer on glibc.

### The window-model contract

`libkwm`'s block asserts nothing of its own. It replays `testing/fixtures/wm/geometry.txt`, whose
106 rows are the window model as `kdos-comp` implements it: tile transitions, tiled geometry,
placement, dragging, clipping and workspace adjustment. Each section of the file names the
compositor function and the lines its rows were read from, and `testing/fixtures/wm/README`
states the rule that a row which cannot be traced to a line does not belong in the file. A failure
means the library and the compositor disagree, which is the disagreement that keeping the window
model outside the compositor is meant to expose.

Add a case by adding a row and citing its source, never by writing an assertion in `selftest.c`. A
row whose expected value was not read from the compositor's source makes the library the authority
over the behaviour it is meant to reproduce.

The 26 `geom` rows are pure arithmetic over the formula in `view_get_edge_snap_box`, so they can
be re-derived mechanically. That gives two independent checks: re-deriving catches a row computed
wrong by hand, and replaying the file against the library catches a row transcribed with a flag
inverted. When the two disagree, correct the fixture; the compositor's source is the authority.

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite must stay clean under both sanitizers. AddressSanitizer and UndefinedBehaviorSanitizer
catch what a plain run takes for a pass, for example a size field in an archive header overflowing
a signed type and becoming an enormous unsigned length, a copy called with a null source, or a read
past the end of a buffer that was last resized smaller than the loop assumes.

A variadic `printf`-style wrapper guards its own format string, in `libkbase` and in the two
programs that have one of their own (`kdos-kpkg` and `kdos-packd`). A null format is undefined in
`vfprintf` anyway, and the sanitizer build's whole-program analysis cannot prove one non-null, so
without the guard `-Wformat-overflow` refuses to build the suite and the sanitized run cannot be
done at all.

Leak checking is off by default (`ASAN_OPTIONS=detect_leaks=0`) and on for the library assertions
and the image fuzzer. Every program under test owns its parsed state until it exits, which a leak
checker reports as a leak and turns into a non-zero exit; the library suite is the one binary whose
subject is code called repeatedly inside a long-lived process.

The image parsers have a fuzzer, `testing/fixtures/img/fuzz.c`. It is its own driver rather than a
block in `selftest.c`, so that its corpus can run under both sanitizers without rebuilding the rest
of the suite. It makes two passes: every fixture decoded under a budget, then every fixture
mutated a byte at a time and truncated at every length, because a hand-written corpus only
exercises the paths its author thought of. A decoder must answer NULL or an image for any input,
and must never read past its end.

### What it does not cover

Several blocks need libraries a bare development machine does not have, and are skipped by name
when they are missing:

- KDOS's own source files added to the compositor (`src/desktop/kdos-comp/src/kdos-*.c`) need
  `wlroots-0.20`, `glesv2`, `egl`, `wayland-server`, `pixman`, `libdrm`, `libpng`, `libxml2`,
  `cairo`, `pango` and `glib`;
- `libkwl`, `kdos-lock` and `kdos-shell` need `fcft`, `pixman`, `xkbcommon` and `wayland-client`;
- `kdos-shell` additionally needs an sd-bus (basu or libsystemd), ALSA, `libpipewire-0.3` and the
  `ext-workspace-v1` protocol;
- `libkimg` needs `pixman`, and each of its decoders its own library: `libpng`, `libjpeg`,
  `libwebp`, `libsixel` and `libnsgif`;
- the tray, portal, network and D-Bus menu blocks need an sd-bus and `dbus-daemon`, and the portal
  block also `busctl`; the polkit block needs `duk`; the MIME blocks need `shared-mime-info`.

A bare host still reaches every section and exits clean. What it loses is inside them, and each
block it cannot build says so.
[The machine where nothing is skipped](#the-machine-where-nothing-is-skipped) is the development
image that runs them.

Every conditional block keeps one rule: a missing tool or library is a skip with a name, decided
before the block starts, so its absence cannot take the rest of the suite with it.

The shell, `kdos-res`, the terminal and the harnesses that link them compile here with
format-truncation warnings disabled and nothing else relaxed. Those programs truncate on purpose:
every label goes into a fixed number of cells, so a cut label is the intended behaviour. Where
truncating *is* a defect, such as a socket path or a device node, the code holds it with an explicit
bound rather than relying on a warning that cannot tell the two apart.

### Reading pkgstore-check.log

A build with `KDOS_PKG_STORE=check` builds every port the [package
store](../03-architecture/packaging.md#check-mode) would have reused, and appends one stanza to
`build/logs/pkgstore-check.log` for each whose package differs from the stored one:

```text
<port> <key12> stored=<sha256> built=<sha256>
  - <mode>  <sha256 or link target>  <path>     the stored package's member
  + <mode>  <sha256 or link target>  <path>     the rebuilt one
```

Up to ten differing members follow each line, in the fingerprint `kpkg verify` uses. A member that
differs on every rebuild, such as an embedded date or a random seed, is a reproducibility defect in
the port; confirm it with `kpkg verify --repro <port>`. A port that differs only after a change
elsewhere read an input the key does not cover, usually an undeclared dependency. The port's own
log names each undeclared library link as `<port> links <owner> without declaring it`. An empty or
absent log means every hit rebuilt to the same bytes.

### The cached host helpers

The scripts under `ports/` compile helpers on first use and cache them: `ports/.kpkgbin` (the
recipe reader), and `ports/.portup` and `ports/.portup-tools` (the version checker). A binary built
against one C library cannot run under the other, so a host run straight after a container run
must delete them first.

The failure does not say so. The recipe reader fails, the version checker cannot render a
candidate, and the suite reports that it reproduced no outcome, which reads as a regression in the
version checker.

A container run as root also leaves those files owned by root, so deleting them as yourself fails
and the stale binary survives. Delete them from a container rather than with `sudo`.

## Goldens

A **golden** is a committed reference frame: a surface rendered offscreen and compared byte for
byte. There are 217 of them under `testing/goldens/`. They cover the shell's surfaces, all eleven
resource-monitor pages plus its detail page and a chart read one sample at a time, the terminal, the cell-level frames, nine replayed
terminal streams, and fourteen frames of a surface driven through its own loop. Text frames are
committed at twelve different sizes and driven frames at three, two of them their own.
`testing/goldens/README` states the rules for adding one.

| Kind | Catches | Count |
|---|---|---|
| Text frames (`--dump`) | Geometry: overflow, misalignment, a control drawn past its rectangle | 185 |
| Cell frames (`--dump-cells`, `cells-*.txt`) | Colour-slot and attribute drift as well | 9 |
| Replayed streams (`vt-*.txt`) | A change in the terminal's state machine, against bytes real programs wrote: the characters, the attributes, the cells that named a colour of their own, the hyperlinks and the prompt marks | 9 |
| Driven frames (`drive-*.txt`) | A loop that does not answer a key, a loop without the resize step, `ktui_keys()` asked after the surface's own keys, and a change in the runner's frame opt-in | 14 |

A text frame is the character in each cell, one line per row. A cell frame is one line per
non-blank cell, `row col U+XXXX fg bg attr`, so a selection that lost its accent fill, or a label
that dropped to an unreadable colour slot, changes it even when the text frame is identical. Every
dump is drawn at the ASCII glyph tier, because a surface drawing straight to a buffer has no
terminal to report richer capabilities.

A driven frame is a text frame taken at the end of a script. A `--dump` draws once and never runs
the loop, so nothing in it can show an event handler. With `$KDOS_DUMP_KEYS` set, the harness's
`kdisp_init()` answers with a display that reads the variable as a script of keys, clicks, ticks
and resizes (the steps are listed in
[Writing desktop software](writing-desktop-software.md#looking-at-it-without-a-screen)); the
surface takes its live path, and the frame it last presented is printed when it shuts down,
followed by `-- N of M events read`. `keydrive` in `testing/selftest.sh` commits these. Every
script ends in `esc tick`, so a surface that closes on `Esc` stops one short of the total and one
where `Esc` closed a rung first reads them all, and every script resizes once, so the frame is at
the new size only if the resize step ran. No script runs a command whose answer depends on the host
(a toggle, a delete, a check). A surface that saves when it closes, as the note does on `Esc`
after an edit, is given a data directory of its own, made empty for the run, so the save is
exercised and the next run does not load it back. The six `drive-runner-*` frames are of a surface defined in the
harness itself, on `sh_run()` with and without `.frame`, which is where the frame opt-in is pinned:
no shipped front end takes it.

A driven frame is a golden of the loop as it is, not a proof that it is right. A migration of a
surface onto the runner keeps its frames byte-identical and its driven frame identical to the one
its own loop drew before the move.

A terminal's frame is taken by running a command to completion: `kdos-term --dump WxH -e …` waits
for the child and consumes everything it wrote before drawing, because a frame taken while a
program is still writing differs every time. The terminal is built for this against a stub of
`libkwl` (`testing/fixtures/term/kwlstub.c`), whose display probe answers "no display", so the
dump takes the same path it takes on the shipped binary. Two of the four terminal frames hold a
picture. A dump has no pixels, so a picture renders as its fallback character in its top-left cell
and blanks under the rest, which is what a text console shows. What the frame asserts is the
shape: how many rows the picture took, and where the cursor was left.

Commit at least two sizes for anything with a layout, because a geometry defect usually shows at
one width only. The monitor's pages carry three sizes each, including a narrow one that forces its
sidebar to degrade.

### How the dump harness is built

The shell's goldens come from one test program, `dumpcheck`, which the self-test builds in the
block headed `the shell's front ends draw offscreen, and the boxes line up` in
`testing/selftest.sh`. It is linked from three parts:

1. `testing/fixtures/shell/dumpmain.c`, the test driver, which also stubs everything that would
   need a display or a bus: `libkdisp` and the three `libkwl` entry points, the icon layer, the
   pixel plates and tiles, the tray, MPRIS and the privacy indicators. It wraps
   `ktui_offscreen_init` so that `KDOS_DUMP_SIZE=WxH` overrides the size a surface asks for, holds
   the scripted display `$KDOS_DUMP_KEYS` selects, and defines the runner's own test surface,
   `runner`;
2. the **base source list** (`DFRONTS` in the script): the files every surface needs, always
   linked. These are `shell.c`, `cal.c`, `menu.c`, `pick.c`, `osd.c`, `apps.c`, and shared helpers
   such as `fav.c`, `mountd.c`, `routes.c`, `chords.c` and `libkchrome`'s drawing code;
3. the **candidates**: every other surface's source file, 46 of them, each checked on its own
   with a compile-only pass. A candidate that does not compile is skipped by name and costs only
   its own golden.

The candidates that pass are then linked together with the base list in one command. If that link
fails, the harness links the base list alone, so the base surfaces still draw, and prints a NOTE
naming every candidate whose goldens are skipped as a result. That single link is why a link error
in any one candidate skips every candidate's golden, as the rules below describe.

### Rules the harness keeps

The rules fall into six groups: how a frame is compared, how the dump harness is linked, which
inputs a golden may not take from the host, the key row, surfaces that talk D-Bus, and parsers.

#### Comparing frames

A cell frame's verdict is acted on separately. A comparison whose answer nothing reads cannot fail,
so the cell half has its own check and its own message. A colour drift and a geometry drift are
fixed by looking at different things.

A surface that exits non-zero costs its own golden, not the run. `set -e` is on, so without care
one surface refusing a flag would end the suite, and every later check, including the ones that say
whether the goldens are committed at all, would never run. `golden()` and `cells_golden()` catch
the status and record it, and the candidate compile loop admits each file on its own for the same
reason.

A list of pages that should have goldens does not skip the ones that have none yet. A loop that
checks for a committed golden and skips a page without one makes a newly listed page unreachable:
no golden, so skipped, so never given one, and the suite reports a clean run over a page nothing
has looked at. Being on that list is the claim that the page should have a golden, so a missing
one fails.

Every committed frame is compared by a `golden` call. A golden that no call compares agrees with
the surface only until the surface changes, and it looks to the next person like evidence that was
checked.

A golden that differs from `HEAD` may be uncommitted work, not drift. Goldens are regenerated as
the tree changes and are often modified in the working tree ahead of the source change that goes
with them. `git checkout --` on one throws away what it records, and the next run reports a
difference that looks like a flake. `git diff HEAD` on the *source* tells the two apart; if it does
not, run the dump twice.

#### Linking the dump harness

`libkdisp` is stubbed, not linked, so adding a `kdisp_` entry point needs a stub. The stubs are in
`testing/fixtures/shell/dumpmain.c`, and no compiler check ties them to the header. A new entry
point without a stub fails as `undefined reference` and skips every candidate's frame. Linking the
real `libkdisp` beside the stubs is not the fix, because 38 of the 41 stubbed names are defined
there too and each is a *multiple definition* error. Add the stub, and make it answer the state a
dump really has: `kdisp_win_supported()` is 0, because a dump renders one frame with no compositor,
and a stub that invented two windows would make the frames assert a fiction.

One missing stub costs every candidate's golden, not only its own. The link is one command over the
base list and every admitted candidate, so a missing stub for a symbol one candidate calls (were
`kch_px_bare`, which `notifyd.c` uses, dropped from `dumpmain.c`) skips the panel, the desktop, the
settings and every other candidate with it, and the suite prints one NOTE and carries on. A green
run with a name missing from the list of surfaces it drew is this failure.

The opposite mistake is a *multiple definition*. A stub for a symbol one of the linked files
defines collides, and the message contains neither `undefined` nor `error`, so a filter looking for
those words reports half the breakage. `osd.c` defines `sh_volume_get` and `sh_mic_muted`, which
`panel.c` calls, so it is in the base list beside `cal.c` and `shell.c` rather than among the
candidates. A file that is sometimes linked and sometimes stubbed collides on exactly the machines
where the harness otherwise works.

A shared helper belongs in the base source list. `mountd.c` is the one `kdos-mountd` client that
both `kdos-devices` and `kdos-disks` use, and `job.c` is the long-child runner `kdos-backup`,
`kdos-burn` and `kdos-verify` share. Leaving either out of the base list makes those surfaces fail
to link, which reads as a defect in them and skips every candidate's golden.

A surface that needs a library the harness lacks is skipped by name. `kdos-peek` and `kdos-pix`
decode with `libkimg` and probe archives with `libarchive`, so both the shell-wide compile and the
dump harness admit them only when `libarchive`, `libpng`, `libjpeg` and `libwebp` are all present,
and say so when they are not. Gating the *whole* shell compile on those libraries would lose the
other sixty-odd files on a machine missing one development package.

#### Inputs a golden may not take from the host

A golden may not depend on what the host has. `kdos-disks` draws what `kdos-mountd` publishes, and
`kdos-print` runs `lpstat` and `lpinfo`. Each gets a fixed input instead: the disks window is given
a socket path that does not exist (`KDOS_MOUNTD_SOCKET`), so it draws the refusal every machine
without the daemon shows, and the printers window is given `--fixture` over recorded answers under
`testing/fixtures/print`.

The icon layer, `libkicon`, is stubbed in the dump harness to answer "no picture" for every icon
name. A golden is the character grid, so a layout that only lines up once the pictures load is a
broken layout.

A dump's font list comes from `$KDOS_FONT_LIST`. On a display, the list of monospaced families is
fontconfig's: one name on the image, hundreds on a developer's machine. So `dumpmain.c` reads its
rows from the file that variable names, one family per line. The suite writes the file into its
output directory rather than committing it under `testing/fixtures/shell`, where a fixture added
for one surface would move another's frames. Two goldens depend on it: `theme-font`, over names
with a space, escaped punctuation and a size key, proving the name column is cut and the sample is
not; and `theme-font-none`, taken with the variable unset, which is the page's empty state.

A container's font is not the image's. The rasteriser blocks resolve `monospace` through
fontconfig, which in the development image is DejaVu, not the shipped console font. `libkcell`
takes its cell width from the advance of `M` rather than from the face's widest glyph, so a font
with wide CJK or symbol glyphs does not widen the grid, but glyph shapes and metrics still differ.
Judge how a picture looks on the image.

A golden holding a sixel picture is guarded on the sixel decoder. A `libkimg` built with pixman
alone has no sixel support, and running the picture test against it produces a diff that blames the
terminal.

#### The key row

Every frame carries a bottom row that names its keys. The suite checks this against the committed
80×24 goldens, because a blank bottom row is a surface whose keys nobody can find. The exemptions
are named in the script, each with its reason: the panel, the Start menu and its routes, the system
and tray menus, the desktop, the tooltip, the notification toasts, the on-screen displays and the
screen savers. These are drawn on the desktop rather than in a window, and a saver closes on any
key while a tooltip answers none, so a row naming `Esc` on either would teach a key that does
nothing. The suite also requires every shell source that holds a `KtuiKeys` to draw it through
`ktui_hint_row`. `menu.c` is the one file that holds a `KtuiKeys` and draws no row, for the reason
its own header gives: a menu answers only the keys its shape implies.

#### Surfaces that talk D-Bus

A surface that talks D-Bus gets a real peer on a private bus. A mock written from the surface's
own assumptions passes on exactly the mistakes a real conversation exposes:

- `kdos-net`: `testing/fixtures/net/nmobjstub.c` serves the one `GetManagedObjects` call the
  surface makes, on a private system bus the block starts and stops. It is an sd-bus *filter*
  rather than an object vtable, because sd-bus owns `org.freedesktop.DBus.ObjectManager` and
  refuses a manual vtable for it with `EINVAL`. An access point lives under
  `/org/freedesktop/NetworkManager/AccessPoint/<n>` and a device under `.../Devices/<n>`, so they
  must be associated by the device's own list, not by path prefix. The recording gives the second
  radio a network the first cannot see, so a guessed association fails the golden.
- `kdos-traymenu`: `testing/fixtures/traymenu/menustub.c` builds a `com.canonical.dbusmenu`
  layout (signature `(ia{sv}av)`, recursive, with a variant per child) with a real sd-bus on a
  private session bus. A reader that miscounts a container leaves the cursor somewhere it cannot
  name, and every later row is nonsense while the frame still draws. The tree carries everything
  the parser can get wrong: a mnemonic underscore to strip, a separator, a disabled row, a submenu
  with three children, two toggle states, and a row marked `visible: false` that must not appear.
  The surface's `--open ID` and `--pick ID` let a dump reach the submenu and send an `Event` with
  no keyboard; the stub prints the id it received, which is how a pick is asserted.
- `kdos-netagent`: `testing/fixtures/netagent/nmstub.c` is NetworkManager's end (a bus name, an
  `AgentManager` and one `GetSecrets` built from the argument order libnm sends) on a private
  system bus, because an agent registered with the host's own NetworkManager would be asked for
  the passphrases of the machine running the tests. `agentcheck.c` links the real `netagent.c`
  against a scripted display, so the keystrokes are the test's and everything else is shipped
  code. What it checks cannot be photographed: the flag set before anybody is asked, the exact
  `a{sa{sv}}` a secret is returned in, and error names that carry no `.Error.`.

#### Parsers

A parser gets a fixture *and* the shipped file. `kdos-appbox catalogue --selftest` runs over
`testing/fixtures/catalogue/catalogue`, which is small enough to reason about and carries every row
type including the awkward ones: a base with its own image, a two-deep runtime chain, a row whose
package list is `-`, a row with no `meta`, and a member in two groups. It pins the rules: a chain
is base-first, a `-` package list is empty, and an expansion removes duplicates. The shipped
`src/system/kdos-appbox/catalogue` is also parsed, because a hand-added row the parser rejects
is a store that opens empty with no error on screen. `KDOS_CATALOGUE` is the variable that points
the parser at either file.

### Regenerating

Regenerate goldens by setting `KDOS_GOLDEN_UPDATE=1` on the self-test, where the Wayland
dependencies exist, which means the development image rather than a bare host:

```sh
docker run --rm -v "$PWD:/kdos" -w /kdos \
    kdos-devdeps:latest sh -c 'fc-cache -f; KDOS_GOLDEN_UPDATE=1 testing/selftest.sh'
```

Then read `git diff testing/goldens/` before committing. That diff is the review: it is a picture
of what the change did to the screen.

The terminal's frames are the exception: `kdos-term` builds against its display stub on any host,
so its goldens regenerate anywhere.

The image must carry a font and GNU `tar`, and must have the Wayland development libraries; see
[The machine where nothing is skipped](#the-machine-where-nothing-is-skipped).

## Fixtures

A **fixture** is recorded system state that a program can be pointed at instead of the live
machine. It is what makes a monitor, an attribution engine or a kill-selection policy testable at
all. The seam is the same everywhere: the process and system filesystems sit behind a movable root,
or a variable moves one directory walk. There are 49 fixture directories under
`testing/fixtures/`.

| Fixture | Records | Makes testable |
|---|---|---|
| `res` | A process and system tree, and a second one (`next/`) taken later | Every monitor page, deterministically, including the rates that need two samples |
| `res/*/sys/class/net/*/device` | A `uevent` file inside the directory | Whether an interface is real. The test is the directory's presence, and git stores no empty directory, so the file keeps the directory in every clone |
| `stutter` | Two snapshots half a second apart, plus the frame events between them | That the application is named with its box, the blocked process comes first, pressure is quoted, and exactly one event is blamed on the compositor |
| `energy` | Four recorded power and process trees | The nesting rule, the counter wrap, the roll-up onto one application, and the short-lived residue |
| `oomd` | Three trees arranged so only the memory budget produces the right answer | That the budget check matters: the host process is larger than anything in either box |
| `mountd` | A block-device tree plus two hand-built superblocks | The acceptance and both refusals. The internal disk carries a real superblock, so a broken check shows up as an extra row rather than as nothing |
| `privacy` | Three processes: one holding a camera twice, one an audio device that must be ignored | The camera indicator, on a machine with no camera |
| `portup` | Recorded upstream responses (a registry index, git tag lists and branch heads, feeds, a releases API answer, directory listings) and, under `ports/`, the recipes seven ports were recorded at | Every discovery adapter and filter in `kdos-portup --selftest`, and all three outcomes end to end, offline and unaffected by bumps to the live recipes |
| `cve` | Four ports and a five-row database | A version behind two fixes, one that only looks behind because of a packaging revision, a name mapping, and a package the database has never heard of |
| `update` | Recorded answers from `kdos update check --json`, `kdos cve --json` and `kdos-bootctl status` | `kdos-update`'s frames, through `KDOS_UPDATE_JSON`, `KDOS_CVE_JSON` and `KDOS_SLOT_TEXT` |
| `clone` | Three hand-built image headers | The two-record length rule |
| `tray` | A second *process* that behaves like a real tray item | The whole protocol conversation |
| `traymenu`, `net`, `netagent` | D-Bus peers on a private bus | See [Surfaces that talk D-Bus](#surfaces-that-talk-d-bus) |
| `shell` | The dump harness, its stubs, and a frozen home and configuration | Every surface's layout |
| `shell/rec` | Two PCM lines (one playback-only, one with a capture stream), 25,600 bytes of raw signed 16-bit audio (four ticks at half full scale, then four of zeros), and a stand-in speech model | That the input list is *filtered* rather than merely listed, and that the level is computed from the samples. The recorder's `--meter` prints one line per tick and `--write` produces a WAV compared byte for byte against `shell/Recordings/2026-01-01-000000.wav` |
| `print`, `devices`, `display`, `firewall`, `backup` | Recorded command output | The printers, devices, display, firewall and backup surfaces, without the host's own state |
| `burn` | A `/sys/block` tree with one optical drive, its vendor and model space-padded as a SCSI inquiry pads them, and one disk that is not a drive | That `kdos-burn` lists the drive and not the disk |
| `verify` | A `SHA256SUMS` naming one file that matches its line and one that does not | A row of each verdict in `kdos-verify`'s frame |
| `vt` | What `vim`, `htop`, `mc`, `less` and `tmux` wrote to an 80×24 terminal, four hand-written streams, the recorder `record.py` and the replayer `vtrender.c` | That the terminal's state machine still produces the same screen |
| `img` | Valid, truncated, zero-length, oversized and malformed images in each decoded format, and `fuzz.c` beside them | `libkimg`: every fixture decoded, then mutated and truncated |
| `catalogue` | A small catalogue with every row type | The catalogue parser |
| `wm` | `geometry.txt`, 106 rows read from the compositor's source | See [The window-model contract](#the-window-model-contract) |
| `polkit` | A rule harness and a stub, run under `duk` | What the shipped polkit rules grant, and what they must not |
| `term` | A stub of `libkwl` | `kdos-term --dump` on a host with no Wayland |
| `kwl` | `paintcheck.c` and `tickcheck.c`, which include `libkwl`'s own source and stand a simulated compositor under it, and `inspcheck.c`, which drives `kwl_insp.c` with the clock handed in | That a partial repaint produces the same pixels as a full one and that the damage covers every changed pixel; that a gliding list is its own picture at the position presented; that the frame clock ticks while an animation runs and is silent otherwise; that the wheel counts detents and a flick coasts; that the `KDOS_INSPECT` overlay stays off the cells and goes still on a still surface; see [The partial paint](#the-partial-paint), [The frame clock](#the-frame-clock) and [The inspector](#the-inspector) |
| `rowanim` | `rowcheck.c`, which includes `libkchrome`'s `kch_px.c` with the `libkwl` calls stubbed and the clock driven by hand | That the travelling selection plate slides, lands on time, and lands at once wherever there is nothing on the screen to slide from; see [The selection plate check](#the-selection-plate-check) |
| `disptext` | `textcheck.c`, which includes `libkchrome`'s `kch_px.c` with the `libkwl` calls stubbed and links `kch_chrome.c` | That display text records one op in its rectangle or its cell form, keys by its string, and replays inside its rectangle at both scales and under a clip; see [The display text check](#the-display-text-check) |
| `plot` | `plotcheck.c`, which links `libkchrome`'s `kch_plot.c` and `kch_tile.c` with a palette of its own | That the chart's data marks, clip and tile keep their bounds and symmetries and draw the same bytes for the same input; see [The chart pixel check](#the-chart-pixel-check) |
| `tile` | `tilecheck.c`, which links `libkchrome`'s `kch_tile.c` with a stubbed display | That a pixel tile of any size draws every block's pixels at the right cells and that the sprite table's evictor never frees a view the tile still holds; see [The tile lifetime check](#the-tile-lifetime-check) |
| `motion`, `sched` | `motioncheck.c` and `schedcheck.c`, which include the compositor's `kdos-motion.c` and `kdos-sched.c`, and `motion/stub/`, the two labwc headers both read | The compositor's fades and window transitions against wlroots' scene graph, and render-late scheduling against wlroots' output signals and a real event loop; see [Compiling the compositor without a full build](#compiling-the-compositor-without-a-full-build) |
| `crt` | `scopecheck.c`, which links the compositor's `kdos-crt-pass.c` and wlroots' damage ring and draws on a surfaceless EGL context | That the phosphor pass drawn over only each frame's damage matches a whole-output draw byte for byte; see [Compiling the compositor without a full build](#compiling-the-compositor-without-a-full-build) |
| `fontpolicy` | `fontcheck.c`, which links `libkcell` | The cell-size arithmetic everywhere, and the font loads, the three fractional-scale cells among them, only where Terminus and its `Terminus (TTF)` twin are installed; see [The cell's size](c-libraries.md#the-cells-size) |
| `pack` | The metadata of six packs, not the packs | What `kdos-packd` would mount and what it refuses: which stack, which cannot, which is signed |
| `box` | Four box profiles | What a profile says it enforced and what it could not |
| `openwith`, `places`, `recent` | A MIME glob table and files, a home with a renamed desktop folder, a recently-used list | File-type resolution, the places list and the recent list |
| `ascii`, `cellclip`, `deco`, `iconpng`, `oblique`, `tone`, `view` | A small driver of their own | The ASCII engine's claims; that painting never writes outside the buffer; that a window has a frame; tray icon data decoded from bytes; the synthetic slant; the tone ladder in every accent; the four list views |

A recorded stream is not a running program, so beside the `vt` fixtures the library assertions
also open real programs on a real pseudo-terminal, press one key in each, and check that the
screen changed and that a resize reflows it: `less`, `nvim`, `htop`, `top`, `mc`, `lf`,
`taskwarrior-tui`, `tmux` and `nano`. The names are the ones this system installs. A row naming
`vim` would exit 127 on every KDOS machine and be skipped, a green tick for a test that never ran.
A program the host lacks is skipped by that same 127, so the block asserts where the programs are
and stays quiet where they are not.

To know a fixture tests something, disable the check it guards and watch it fail. Both checks the
`mountd` fixture guards fail that way.

The tray fixture is a second process on purpose. The protocol is a conversation between two peers
on a message bus, and a mock of either side passes on the mistakes only a real peer exposes.

### The terminal fixtures

The terminal fixtures are never re-recorded. A re-recording picks up a different program version,
a different terminfo and a different hostname, so a fixture that regenerated itself would be a
test that changed its own question. The hostname and clock inside them are part of the recording.
`testing/fixtures/vt/record.py` is kept to show how they were made, not to be re-run.

Each recording stops on a live frame rather than on the program's exit. A stream ending with the
alternate screen being restored renders an empty grid, and so does a parser that gave up on the
first byte, so the self-test refuses an empty grid outright.

Four of the nine streams are hand-written, because no recording contains what they pin:

| File | Contains |
|---|---|
| `malformed.esc` | An unterminated CSI, a parameter past any bound, an OSC with no terminator, a truncated UTF-8 lead byte, a surrogate, a DCS carrying rubbish |
| `attrs.esc` | The style SGRs and their offs, the underline shapes and colour, and the four spellings of a colour |
| `links.esc` | `OSC 8` around a run, the empty form that closes it, the same address twice, a scheme the whitelist refuses, and a `params` field that must be ignored |
| `prompts.esc` | `OSC 133` marking two finished commands, one that succeeded and one that failed, and a third still running |

`vtrender.c` replays a stream in small uneven chunks, because a pseudo-terminal splits escape
sequences across reads, and a parser that only handles a whole sequence passes a single-write test
and corrupts a real terminal. Its output is five blocks, separated by `--`:

1. the characters;
2. the attributes, one letter per cell. A style has no character to show, so a golden holding only
   text could not tell an italic comment from an upright one; an underline's shape is its own
   digit;
3. which cells carry a colour of their own;
4. which cells are a hyperlink, as the link's id, so one run reads as one link and the same address
   twice reads as the same digit;
5. one character per *row* for the prompt marks.

No colour value is ever in a golden. A value moves with the theme, and a palette change reading as
"vim drifted" would be a test that changed its own question. The third block holds the *decision*
instead: the sixteen named colours reduce to slots and follow `kdos theme`, and everything above
them is a literal the program chose, so the block drifts only when that rule does.

The same care applies to the terminal's own input. `ktui_input_next` reads descriptor 0, so the
bracketed-paste block puts a pipe there and writes the sequence in pieces: a terminator split
across two reads is the case that turns a paste into one that never ends when the tail is taken for
text. The block restores descriptor 0 before it returns, or every later block that reads a
terminal would read the pipe.

## The machine where nothing is skipped

`selftest.sh` runs everywhere and skips what it cannot build, saying so each time. On a bare host
it skips most of the interesting half: `libkimg`'s decoders, the sd-bus blocks, `fcft` and the
Wayland consumers. The *development image*, `kdos-devdeps`, is where nearly none of those skips
appear, so it is the run that exercises every surface dump and the goldens behind them. It is not
the *build container*, `os-dev`, which `make build` builds from the repository's `Dockerfile` and
runs the build in; that one carries the packaging toolchain but not the Wayland development
libraries, so the self-test skips the surface dumps there.

```sh
testing/devdeps-image.sh                    # builds kdos-devdeps, then runs the suite in it
testing/devdeps-image.sh bash               # any arguments are run in the image instead
```

`testing/Dockerfile.devdeps` carries what the script probes for, plus four of the programs the
pseudo-terminal block drives: `htop`, `mc`, `tmux` and `less`, with `top` coming from busybox. It
also installs `vim`, which no block drives. `nvim`, `lf`, `taskwarrior-tui` and `nano` are not
installed, so their rows skip with exit 127 there too. It is Alpine, because the target is musl and
a feature-test difference is better met here than in a phase build. It does not satisfy every
guard: `wlroots-0.20`, which no distribution packages and which the next section builds; `busctl`,
which Alpine ships in no package, so the portal block stays skipped although its other halves are
present; and `libnsgif`, which it does not install, so `libkimg` is built there without its GIF
decoder.

Two of its packages are there for what they unblock: `font-dejavu` with `fontconfig`, and GNU
`tar`. The absence of either is reported as something else. `fcft` resolves `monospace` through
fontconfig, so with no font installed it answers `failed to match font`, which reads as a
`libkcell` failure and stops the run at the first rasteriser block. With busybox's `tar`, the
reproducible-build block answers "the synthetic port did not build", and every surface golden is
gated behind that block, so all of them are skipped without a word. Between them the two hide most
of the suite while reporting only their own one-line error.

## Compiling the compositor without a full build

`kdos-comp` needs `wlroots-0.20`, which no distribution packages, so the self-test reports its
block as skipped. One compositor block runs regardless: the phosphor pass's scope check,
`testing/fixtures/crt/scopecheck.c`. It links the pass's shader and region code
(`kdos-crt-pass.c`, which touches no wlroots) with wlroots' damage ring compiled straight from the
port's tarball, and draws on a surfaceless EGL context, which Mesa's software rasteriser in the
development image provides with no display. Every scripted frame drawn over only its damage must
match a whole-output draw byte for byte, and the check fails unless its two deliberately broken
modes fail too. It compares buffer contents, not the damage a commit reports. A bare host without
EGL and GLES2 development files skips it.

The compositor's fades have a check that does need `wlroots-0.20`:
`testing/fixtures/motion/motioncheck.c` compiles `kdos-motion.c` against wlroots' own scene graph,
standing in for the two labwc headers it reads with stubs in `testing/fixtures/motion/stub/`. It
maps a layer surface, unmaps it half way through its fade in the order wlroots really uses, lets
the client exit, removes the output and tears the compositor down. It checks that the close fade's
snapshot stands directly above the output's layer tree and outside it, that it keeps the
surface's buffers until the fade ends and then releases them, that it lets the pointer through,
that a hidden subsurface is left out, and that every fade ends on its exact end value. It then
builds a window's tree (decoration rectangles, an inactive decoration switched off, the content
tree) and plays a window transition's open, with labwc moving the window part way through; a
close, whose snapshot must copy the showing border as a one-pixel buffer that refuses the pointer
and leave the rest out; a workspace switch's snapshot below its anchor; a switch straight back, in
which the window must take over its own leaving snapshot's alpha and offset and the snapshot must
go, while a snapshot whose window has gone fades out untouched; and `motion` switched off
mid-transition. A window must always end exactly at rest and exactly opaque.

`testing/fixtures/sched/schedcheck.c` does the same for render-late scheduling. It compiles
`kdos-sched.c` with the same stubs, stands a `wlr_output` up by hand with its presentation, commit
and destroy signals, and runs a real event loop. It checks the prediction's arithmetic; that a
deferred frame holds the output's pending flag for the wait and lowers it before the composite;
that any other commit cancels the timer, a buffer-less one (variable refresh switched on for a
fullscreen window) included, since it too queues a page flip, and that one switching the output off
also lowers the flag; that a later frame event that is refused cancels it too; that a
stale presentation, variable refresh, a tearing frame, a budget past the blank and an output that
is not DRM all composite at once; and that the timer goes with its output and at shutdown. Its
refresh is 200 ms rather than a display's, so a loaded machine cannot make a millisecond timer miss
a check.

Like the graft compile, both run in the image described next once wlroots is installed there, and
are skipped everywhere else.

`kdos-comp` is still buildable without a full build, because the tree's own recipe names the
source: `wlroots-0.20.2.tar.gz` in the `wlroots` port's directory, which `make fetch` puts in place.
Build it in an image derived from the development image:

```dockerfile
FROM kdos-devdeps:latest
RUN apk add --no-cache meson ninja pango-dev libdrm-dev libinput-dev libseat-dev \
      mesa-dev libxkbcommon-dev wayland-dev wayland-protocols hwdata-dev \
      libdisplay-info-dev libxcb-dev xcb-util-wm-dev xcb-util-renderutil-dev \
      xcb-util-image-dev libx11-dev xcb-util-dev xwayland xwayland-dev
# then meson setup / compile / install the wlroots tarball with -Dxwayland=enabled
```

The recipe (the `wlroots` port's `build.sh`) also applies `idle-capture-frame.patch` and builds with
`-Drenderers=gles2,vulkan` and `-Dcolor-management=enabled`. Those two options additionally need
the Vulkan headers, `glslang` and `lcms2`, which the image above does not carry; apply the patch
when the change under test touches screen capture.

`kdos-comp` then configures, compiles and links in that image. Two concessions matter when reading
a failure there:

- `-Dicon=disabled`. `libsfdo` is packaged nowhere available, and the `icon` feature requires it.
  Code guarded by `HAVE_LIBSFDO` is not compiled, so a change in it is not covered.
- `-D_GNU_SOURCE`. `kdos-thumb.c` calls `fileno`, and musl's feature-test defaults differ from what
  the real phase build gets.

Build the unmodified tree first. With a baseline binary in hand, every error after a change belongs
to the change. This proves the compositor is type-correct and links. It does not prove a window
lands where a person expects, which remains the rig's job.

## Running a shipped program without booting

`build/fs` is a complete musl root, so a program already installed there can be run directly, with
no ISO and no emulator, in seconds rather than minutes:

```sh
docker run --rm -v $PWD/build/fs:/rootfs -v /path/to/inputs:/rootfs/tmp/in:ro \
    alpine chroot /rootfs /bin/sh -c 'w3m -dump /tmp/in/page.html'
```

This checks a filter chain, a converter or any program that reads a file and writes text against
the binaries that ship rather than the host's. It has three limits:

- There is no `/proc` and no `/sys`. Anything that reads either behaves differently; `w3m`, for
  one, prints a garbage-collector warning here that it does not print on the machine.
- `unshare` is refused inside the chroot, so a program that probes for a namespace takes its
  fallback path. The fallback is easy to exercise and the namespace path impossible, and loosening
  the container does not help: `--privileged`, `seccomp=unconfined`, `apparmor=unconfined` and
  `--cap-add SYS_ADMIN` all still answer `Operation not permitted` for a `chroot`ed process.
- Nothing is supervised and no session exists. A program that wants `$XDG_RUNTIME_DIR`, a bus or a
  terminal needs the rig.

Everything the program writes stays in `build/fs`, and the ISO is built from `build/fs`. A bind
mount creates its own mountpoint, and a program run under the chroot writes to `/root`, `/tmp` and
wherever else it likes. None of that is owned by a package or by `fs/`, so neither the
[orphan sweep](build-system.md#sweeping-orphaned-packages) nor the
[`fs/` manifest](build-system.md#syncing-fs) removes it, and it ships. Bind inputs read-only under
`/tmp`, clean up afterwards, and run `testing/preflight.sh`: it refuses a `build/fs` whose top
level is not a root filesystem.

## The QEMU rig

The rig boots a real KDOS image in QEMU, drives it with keys, pointer and commands, and
photographs the screen. Use it for what nothing offline can show: the phosphor pass (the
compositor's CRT-imitating shader, which runs only under the GLES2 renderer; see
[kdos-comp](../04-programs/kdos-comp.md#the-phosphor-pass)), a real mode set, window management,
and a boxed application.

Prefer a surface's own `--dump` over a photograph whenever it answers the question. A dump hands
out the surface's exact composited grid, so a check on it is a text diff, with no boot, no
framebuffer and no tolerance for antialiasing. But a dump proves a character, never a colour:
`--dump` writes the codepoint in each cell and discards the colours, so text drawn in the
background's own colour, present and invisible on every screen, dumps identically to text a person
can read. Use `--dump-cells` when the colour matters.

`testing/vnc-shot.py` is the driver. It boots `build/iso-build/kdos.iso` headless under UEFI
firmware, with the serial console and the QEMU monitor on Unix sockets, types through the monitor,
and reads the framebuffer over the remote-framebuffer (VNC) protocol. Steps run in the order they
are given on the command line, so one boot can open a surface, photograph it, close it and open
the next.

The desktop is already up when the rig starts driving. `/etc/inittab` respawns `kdos-getty` on
`tty1`, which starts `kdos-login`; that logs in automatically, and the login shell's
`.bash_profile` starts `kdos-desktop`.

`--keys` is a monitor `sendkey`, so it reaches whatever owns the active console, which is that
desktop. `--cmd` runs on the serial console as the desktop user, and `--root-cmd` as root, so
neither disturbs what is on screen.

The first login opens the first-run key card (`kdos-keys --first-run`) over the desktop, so a
run's first `--shot` photographs the card rather than what you asked for. Start a run with
`--keys esc --sleep 1` to dismiss it before any shot, as `testing/quick.sh` does.

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest; needs the network, once
docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
    python3 testing/vnc-shot.py --audio --size 1920x1080 \
    --wait 24 --keys esc --sleep 1 --cmd 'kdos-res' --sleep 4 --shot build/shots/x.png
```

The shot is written as raw PPM whatever its extension; convert it before opening it as a PNG.

### The fast loop

`make build` with packaging spends most of a small change's time writing the ISO, even with
`KDOS_ISO_COMP=zstd:3`. Repacking the whole medium to carry a 200 KB binary makes every
look-at-it cycle wait on it. `testing/quick.sh` avoids that:

```sh
testing/quick.sh kdos-comp,kdos-shell -- --keys meta_l-ret --sleep 3 \
                                        --shot /kdos/build/shots/x.png
```

It needs an ISO from one full build. It rebuilds the named ports into `build/fs` with no packaging
and no snapshot (`--phases 50_desktop --rebuild <ports> --no-snapshot`, about 1m09s), tars exactly
the files those ports own into `build/fix.tar`, and boots the ISO at 1280×800 with that tar on a
raw disk. There `testing/quickpatch.sh` unpacks it over the live medium's in-memory overlay and
restarts the session. Everything after `--` is passed to `vnc-shot.py`, after the patch and after
the key card is dismissed. A run takes about three minutes, or 1m37s with `KDOS_QUICK_KEEP=1` and
`KDOS_QUICK_NOBUILD=1`.

| Variable | Does |
|---|---|
| `KDOS_QUICK_PHASES` | The phases to build (default `50_desktop`); widen it to the phase of a port outside the desktop, e.g. `42_graphics,50_desktop` |
| `KDOS_QUICK_NOBUILD=1` | Reuse what is already in `build/fs` |
| `KDOS_QUICK_KEEP=1` | Do not restart the session |
| `KDOS_QUICK_FILES` | Extra paths under `build/fs` to carry, such as a configuration file or a chord table |
| `KDOS_QUICK_SKEL=1` | Copy `/etc/skel/.config` over the live user's configuration in the guest |

The file list comes from the package database: `build/fs/var/lib/kpkg/db/<port>` is what that
port installed, so a program that gains a new name or data file is carried without anyone adding
it.

`KDOS_QUICK_KEEP=1` is right whenever the program under test is started on demand. Every
`kdos-shell` surface is started fresh by the chord that opens it, so the new binary runs with no
restart and the run is a minute shorter. It is wrong for `kdos-comp`, which is the session.

`KDOS_QUICK_FILES` carries a file from the tree's `fs/`: those paths are installed into `build/fs`
by the file-system step and belong to no package, so the database does not list them. The
file-system step must have run for them to be there. Add `KDOS_QUICK_SKEL=1` for a file under
`/etc/skel`, because skel seeds a *new* account and the desktop user's home was seeded at install
time, so a chord table patched in skel changes nothing for the person logged in.

What the loop cannot carry:

- A new port, a kernel change or an initramfs change. Use the real build.
- Anything that owns a D-Bus name. A program that owns one does not come back cleanly when
  `quickpatch.sh` restarts the session: the script kills `kdos-notifyd` for that reason, and a
  notification raised after the restart does not draw, while the same call on a freshly booted
  ISO does. Verify anything involving the bus on a real boot.
- Evidence about the shipped image. What `make build` writes is the ISO; this is the loop to
  iterate in *before* taking the photograph that counts.

Two details the script handles. The compositor's socket file outlives the process that created
it, so "the socket exists" is true a millisecond after the kill and later steps would run against
the replaced binaries; the script waits for a *different* process id and removes the stale socket.
And nothing brings the session back on its own: the login shell on `tty1` does not exec the
desktop, so it survives the kill and init has nothing to respawn. The script starts `kdos-desktop`
itself, as the login shell's profile would have.

### The flags

| Flag | Does |
|---|---|
| `--shot <file>` | Photograph the screen now, into this path |
| `--out <file>` | Where the final shot goes when no `--shot` was given (default `/tmp/kdos-shot.ppm`) |
| `--keys <keys>` | A monitor `sendkey`, e.g. `meta_l-a` |
| `--chord <keys>` | Hold the modifiers and tap the key over VNC, e.g. `super+shift+space`. Use it for anything with two modifiers: `sendkey` presses and releases the whole combination at once, and such a chord never arrives |
| `--type <text>` | Type into whatever has the focus, then Return |
| `--text <text>` | Type and stop, with no Return, for a search field or filter where Return would act on the first match |
| `--mouse X,Y` | Move the pointer, in absolute pixels |
| `--click X,Y[,BTN]` | Move there and click; `BTN` is 1, 2 or 3 |
| `--drag X1,Y1,X2,Y2[,BTN]` | Press at the first point, move to the second with the button held, release |
| `--press X,Y[,BTN]`, `--release [X,Y]` | A drag held open across later steps, so a `--shot` between two `--press`es photographs it in progress. A button stays down only while its VNC connection is open, so the whole gesture uses one connection, which is why this is two flags and not a longer `--drag` |
| `--sweep X1,Y1,X2,Y2[,N[,MS[,BTN]]]` | A drag at a real mouse's report rate: `N` interpolated motions `MS` milliseconds apart (default 120, 2 and button 1). `--drag` and `--press` space events 150 ms apart, so a defect that needs a fast hand needs this |
| `--sleep <s>` | Wait this many seconds before the next step |
| `--wait <s>` | Seconds to let the session settle after boot (default 25) |
| `--soak <s>` | Let the session run after every step and before the final shot (default 0) |
| `--cmd <cmd>` | Run a command in the desktop session |
| `--root-cmd <cmd>` | Run a command as root on the serial console and print its output |
| `--root-script <file>` | Send a local script into the guest and run it as root |
| `--script-timeout <s>` | How long a `--root-script` may take (default 900) |
| `--console-cmd <text>` | Type this on `tty1` during start-up, before any step. `tty1` is the desktop, so it reaches the icon layer's type-ahead, not a shell; to run a command in a terminal window use `--keys meta_l-ret` then `--type` |
| `--size WxH` | The guest's screen (default `1280x800`, virtio-gpu's own default) |
| `--gl` | `virtio-vga-gl` on a headless EGL display, so the compositor gets GLES2 and the phosphor pass runs. Needs `/dev/dri`. No Vulkan |
| `--venus` | With `--gl`, also publish a Vulkan capability set. Does not work on an NVIDIA host; see below |
| `--audio` | Give the guest an HDA controller, so audio paths reach a real device |
| `--disk <qcow2>` | Attach a virtio disk, the target an unattended `kinstall` writes to |
| `--no-cdrom` | Leave the ISO off, so the disk is what boots |
| `--boot-disk` | Boot the disk with the ISO still attached, so an installed system can still reach the medium |
| `--data-disk <file>` | Attach a raw file as a plain virtio disk, to carry a large file into a guest with no network. Input only |
| `--scratch <file>` | Attach a raw file the guest writes a tar onto, which is how files come back out |
| `--usb <file>` | Attach a raw disk image as a USB stick |
| `--no-session` | Accepted and ignored: `tty1` already starts the desktop, so the steps drive the one the boot brought up |
| `--keep` | Leave the emulator running after the steps |
| `--serial-log <file>` | Where the serial console is logged (default `/tmp/kdos-serial.log`) |
| `--vnc-port <n>` | The VNC port (default 5909) |

Several of those answer questions a screenshot alone cannot.

`--audio` gives the guest a sound device. Without it the sound library fails to initialise and
every audio path in the guest is untestable. The rig asks `testing/qemu-audio.sh`, the same probe
every `make run*` target uses, for a real host backend, and falls back to `-audiodev none` when it
finds none. The rig image carries neither PipeWire nor PulseAudio, so it gets the null backend. A
null backend consumes samples on a timer and a real one consumes them as a device does, so a guest
whose timing follows its own playback runs to a different clock on the two. To reproduce what
`make run-hw` does, use the accelerated image (`testing/qemu-hw`), which carries PipeWire. The
device is playback-only (`hda-output`), so recording is tested through `snd-aloop` instead; see
below.

`--soak` lets the session run between launch and measurement, because a monitor's cost over its
first few seconds is its start-up, which is not what is being measured.

`--root-script` is the form for a check too long to be one command. The script travels
base64-encoded, in short lines, because a terminal in canonical mode silently drops everything past
its line limit and an encoded payload contains nothing the shell acts on before decoding. The rig
then waits for a marker the *guest* prints, so a step taking minutes is waited for rather than cut
off. A plain command waits a fixed short time, so anything longer must be a script.

### Vulkan in the rig

`--gl` alone starts `virtio-vga-gl` with no Vulkan capability set, so the guest's virtio driver
opens nothing and every Vulkan tool silently falls back to lavapipe, the CPU rasteriser.
`vulkaninfo --summary` naming `llvmpipe` with `PHYSICAL_DEVICE_TYPE_CPU` is the sign, and a `vkcube` or
`vkgears` rate taken that way is a number about the host's cores. `--venus` adds `venus=true` and
blob resources, which requires the guest's memory to come from a shared `memfd`, so it also changes
the machine's memory backing. On an NVIDIA host neither end works: with `--gpus all` (the only way
the container has a host Vulkan device to forward to) QEMU aborts during guest boot with exit 134;
without it the guest boots, the virtio driver loads, and instance creation fails with
`ERROR_OUT_OF_HOST_MEMORY`, taking lavapipe with it. `--venus` therefore stays opt-in, and a Vulkan
number from this rig is lavapipe's until `vulkaninfo` names a GPU.

### Driving the accelerated image

The accelerated image (`testing/qemu-hw`) needs no Python of its own. Start its emulator with the
serial and monitor devices on Unix sockets in a bind-mounted directory, `chmod 666` them from inside
once QEMU has made them (the container runs as root and the driver does not), and drive it from the
host by importing `vnc-shot.py`'s own `Serial` and `Monitor`. `Super+Return` opens a terminal on
the desktop, so `Monitor.type()` is enough to start a program.

### How the rig reads the screen

A monitor `screendump` answers "no surface" under accelerated graphics, so the framebuffer is read
over VNC instead. VNC's encoding negotiation has an exact field layout, and one extra padding field
desynchronises the stream so every later read blocks forever.

### A real capture device, with no emulator flag

The emulated codec has no input, so nothing `--audio` gives can produce a capture signal.
`snd-aloop` can, and it is already on the image (`CONFIG_SND_ALOOP=m` in the kernel configuration,
with modules compressed with zstd). Loading it gives the guest a card named `Loopback` with two
PCMs, each with eight playback and eight capture subdevices, cross-wired: what is written to
`hw:Loopback,0` is read from `hw:Loopback,1`.

```sh
modprobe snd-aloop
sox -n -r 16000 -c 1 -t alsa hw:Loopback,0 synth 8 sine 200-2000 vol 0.5 &
sleep 2
sox -q -t alsa hw:Loopback,1 -t raw -e signed -b 16 -c 1 -r 16000 /tmp/cap.raw trim 0 3
sync
```

Run it as a `--root-script`, with nothing else holding the loopback's capture side.

- Use a swept tone. Silence and a flat signal pass identically whether a level is computed from the
  samples or hard-coded to zero; a sweep does not. The three seconds above come back as exactly
  96,000 bytes peaking at 16382, which is half of full scale, the `vol 0.5` that was played.
- The rates need not match. The player negotiates 48 kHz and the capture asks for 16 kHz signed
  16-bit; ALSA's plug layer converts, `sox` warns that it cannot use the format natively, and the
  bytes that arrive are correct. What matters is that the player opens first, so the capture side
  has a stream to read.
- End with `sync`; see [Harness traps](#harness-traps).

### Moving a large file into the guest

Use a plain virtual disk (`--data-disk`), not a USB device. Emulated USB storage runs at under two
megabytes a second against a virtual disk's several hundred: a large pack takes over an hour one
way and seconds the other. Use `--usb` only when the test needs a removable device, which is the
removable-media daemon's subject.

A file is read back by block copy and truncated to its exact length, because a raw drive is rounded
up to a sector boundary.

### The host may not have an emulator

The rig runs unmodified inside a container image that carries one, with the repository bind
mounted and hardware virtualisation passed through. The image is built from this tree:

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest from testing/Dockerfile.qemu
```

It needs the network once; everything afterwards runs offline against the ISO. The image is Debian
with QEMU, its disk utilities, the OVMF firmware an EFI boot needs, and python3, and nothing else,
because `vnc-shot.py` speaks VNC using only the standard library. There is no pip step, so the only
versions that matter are the distribution's QEMU and OVMF.

Because the image is built from the tree, a photograph is reproducible from any clone.

### Harness traps

The following behaviours of the rig and the guest cause false results.

- Driving the session means a *login* shell (`su - kdos -c`). A plain user switch leaves the
  container tooling resolving the home directory to `/`, and every call fails on a permission
  error.
- A plain virtual display puts the compositor on software rendering, so the phosphor pass is off
  and not in the photograph. What is photographed is the cell grid underneath it. Use `--gl`, or
  the accelerated image, to see the pass.
- `pkill -f PATTERN` also matches any shell loop whose own command line contains `PATTERN`, such as
  a `while pgrep -f PATTERN` wait loop, so it kills the monitoring loop itself along with the
  target.
- An exact-name kill cannot match a process name past 15 characters, so a restart that uses one
  restarts nothing and the old program keeps answering.
- The harness stops the emulator without a shutdown, so a root script must `sync` after writing,
  or a file it wrote comes back zero-length, whether it is read out of the guest or found on the
  next boot.
- A pipeline that reads a supervised service's output never returns, because the supervisor keeps
  the pipe open.
- A static screen produces no frame events, so anything about dropped frames needs something
  animating first.
- A step costs seconds, so anything with a timeout must be photographed with no sleep before it.
  Typing goes one character at a time and a `--shot` is a full framebuffer over VNC: four shots take
  about a minute. With a five-second toast, a pulse or a tooltip's own delay, a `--sleep` before
  the shot photographs the desktop after the thing has gone, and the picture looks exactly like the
  feature being broken.
- A `--root-cmd` or `--root-script` round trip outlasts a five-second toast. It runs over the
  serial console and waits for a prompt, which takes longer than the notification it raised stays
  on screen. Raise a toast with `--type`, which goes over the keyboard and is quick, or ask for a
  timeout longer than the rest of the run.

## Checking the sources

Upstream source files are not in git. `make fetch` resolves each one from the local source cache,
the project's source archive or upstream, verifies it against the recipe's `sha256 =` line, and
puts it in its port directory (see
[Where sources come from](developing.md#where-sources-come-from)). Three checks verify it:

| Check | Run | Answers |
|---|---|---|
| `make fetch-check` (`ports/fetch --check [port…]`) | Offline, about a minute | Every hashed source that git does not track is on disk and matches its hash; it lists what is missing or corrupt and exits 1 if anything is |
| Preflight's source checks | `testing/preflight.sh` | No recipe-hashed archive is tracked by git, `.gitignore` keeps fetched sources and the cache out of commits, the fetch and publish scripts parse, and `ports/sources.idx` is a well-formed format-2 index |
| The pre-push hook | Enabled with `git config core.hooksPath script/hooks` | Offline first, a pushed tip whose ports layout breaks is refused: a recipe not exactly at `ports/core/<shelf>/<name>/`, a shelf missing from `ports/shelves` or named `libs`, `core` or after a port, or a bare name held twice across `ports/core` and `src/<area>/` (`KDOS_SKIP_LAYOUT_CHECK=1` skips it). Then a push naming a source hash that `ports/sources.idx` and the archive do not hold, or one of whose parts the archive lacks, is refused, as is one made while the archive is unreachable (`KDOS_SKIP_PUBLISH_CHECK=1` skips that check only); see [Writing ports](writing-ports.md#the-pre-push-hook) |

A file git tracks, such as a patch or a configuration file beside a recipe, is never treated as an
archived source: `ports/fetch` and `--check` skip it. A recipe-hashed archive that git tracks fails
preflight with "recipe-hashed archives are tracked by git — git rm --cached them", and
`make fetch-check` reports it as neither present nor missing. A successful check ends with
`All N archived sources present and verified`; a count of 0 there means nothing was checked.

## docscheck.sh

`testing/docscheck.sh` is the authoring check for this book.

```sh
bash testing/docscheck.sh                                     # every page, plus README.md
bash testing/docscheck.sh docs/kdos/05-developer/testing.md   # only the named pages
```

It reports:

| Report | Means |
|---|---|
| `DEADLINK` | A relative link whose target file exists neither beside the page nor from the repository root. The part after `#` is not checked |
| `HISTORY` | A line containing a phrase from the list in the script's `BAD` variable, phrases that almost always record the past rather than state the present. At most five lines are printed per page |
| `NOTITLE` | A page under `docs/kdos/` whose first line is not a `# ` title |
| `NOSEEALSO` | A page under `docs/kdos/` with no `## See also` section |
| `UNLISTED` | A page the book's index, `docs/kdos/README.md`, does not name (full runs only) |
| `MISSING` | A path given on the command line that does not exist |

It prints `docscheck: ok` and exits 0 when nothing is reported, and exits 1 otherwise. It cannot
catch a paragraph that narrates history in its own words, or a link to a heading that was renamed.

## Measuring frame rate

Three different numbers are called "fps", and a measurement that does not say which one it took
cannot be compared with anything.

| Number | Counts | Decided by |
|---|---|---|
| Render rate | Frames the application finished drawing | The driver and the GPU, and whether the swap waits for a refresh |
| Compose rate | Frames `kdos-comp` built out of its clients | The compositor, which paces off the output's own frame events |
| Present rate | Frames that turned into light | The display mode, and the presentation path under it |

Under `make run-hw` the virtio-gpu virtual display runs at 60 Hz. A number above 60 there is a
render rate: those frames were drawn and thrown away, and nothing measured in the emulator can show
more than sixty frames a second reaching a screen. Raising a software cap raises the first number
and cannot raise the third. What a cap costs on a 144 Hz panel is a claim about real hardware and
has to be measured there.

The present rate is the one the machine reports on its own. The compositor writes a line to
[`kdos-frames.sock`](../06-reference/filesystem-and-ipc.md) for every frame that missed, with the
output's refresh interval, the lateness, its own render cost, and whether the gap was measured from
a presentation event or from the frame clock. `kdos stutter` is the front end. The socket says
nothing while the screen is static; see [Harness traps](#harness-traps).

### The overlay, which is already on every machine

mesa is built here with `-D gallium-extra-hud=true`, so a Gallium driver draws its own overlay over
any GL or GLES client, with no extra software and no change to the program:

```sh
GALLIUM_HUD=fps es2gears_wayland
GALLIUM_HUD=fps+frametime glmark2-es2-wayland     # both curves in one pane
GALLIUM_HUD=simple,fps es2gears_wayland           # text, no graph
GALLIUM_HUD=csv+fps+frametime es2gears_wayland    # values to stdout, for a script
GALLIUM_HUD=help es2gears_wayland                 # every name this driver can draw
```

In the syntax, `+` shares a pane, `,` opens a pane below, `;` opens a column, and `.w`/`.h`/`.x`/`.y`
size and place one. `GALLIUM_HUD_PERIOD` is the update interval in seconds; `0` means every frame.

It is a GL instrument. The frame sources are in every Gallium build; `gallium-extra-hud` adds the
disk, network and CPU-frequency ones. A Vulkan program draws no overlay: `vkgears` prints its own
rate instead, and `vkcube --c <n>` runs a fixed number of frames so an external clock can do the
arithmetic.

### Taking the cap off

A GL client that waits for a refresh is measuring the display, not the machine. `vblank_mode` is a
driconf option, and an environment variable of the same name overrides both the default and any
`drirc`, so it needs no cooperation from the program:

```sh
vblank_mode=0 es2gears_wayland          # never synchronise, ignoring the application's choice
MESA_VK_WSI_PRESENT_MODE=immediate vkgears
```

`glmark2` asks for swap interval 0 itself unless `--swap-mode fifo` is given, so its score is a
render rate by construction and is comparable between machines rather than between panels.

### Which tool answers which question

| Question | Tool |
|---|---|
| How fast can this machine draw a trivial scene | `es2gears_wayland`, which prints `N frames in X seconds` every five seconds |
| How fast can it draw real ones, as one comparable score | `glmark2-es2-wayland`, or `glmark2-wayland` for desktop GL |
| The same with no compositor in the way | `glmark2-es2-drm` / `glmark2-drm` from a text console while no compositor holds the display; `glmark2-es2-gbm` / `glmark2-gbm` render offscreen and need no display |
| The same for Vulkan | `vkgears`, or `vkcube` for a swapchain whose present mode can be chosen |
| Does this machine have a Vulkan driver, and which | `vulkaninfo --summary`. Run it first under an emulator, because a Vulkan tool falls back to lavapipe on the CPU without saying so |
| Which EGL renderer, extensions and configs a client gets | `eglinfo` |
| What an X11 client sees through Xwayland | `glxinfo`, `glxgears` and `es2gears_x11`, which exist only in a box |

`es2gears_wayland`, `eglinfo` and `vkgears` come from the `mesa-demos` port, the six `glmark2`
flavours from `glmark2`, and `vulkaninfo` and `vkcube` from `vulkan-tools`.

### Desktop GL and GLX

A Wayland client reaches desktop GL through EGL, never GLX. `glmark2-wayland` binds
`EGL_OPENGL_API` and then opens the GL entry-point library by name, `libGL.so` first and
`libGL.so.1` second, and prints `Error loading GL library` if neither answers.

`libglvnd` is built with `x11` and `glx` enabled, so it installs `libGL`, `libGLX`, `libOpenGL`,
`libEGL` and `libGLES` with their `gl.pc` and `glx.pc`. `libGLX` dispatches to mesa's
`libGLX_mesa`. mesa is built `-D glx=dri -D platforms=wayland,x11`, which is what an X11 client
under Xwayland draws with: `libGLX_mesa` behind glvnd's `libGL` and `libGLX`, and the Vulkan X11
WSI. Xwayland is built `-Dglx=true` (with `-Dglamor=true` and `-Ddri3=true`), so an X11 client's
`glXChooseVisual` finds the GLX extension. libepoxy is built `-Dglx=yes` and resolves `glX*`
through `libGL.so.1` at run time. A Wayland client never loads `libGLX_mesa`, but `libEGL_mesa`
and the Vulkan drivers link `libxcb` and `libX11` for their X11 platform, so the X client libraries
are on the host either way.

To confirm what an image carries, after a build's `70_image` phase has written `build/iso_root`:

```sh
unsquashfs -ll build/iso_root/system.sfs | grep -E 'libGL|libOpenGL|libGLX'
ls build/fs/usr/lib/libGL.so*                # the same question of the build tree
```

The `mesa-demos` port is built for Wayland alone (`-Dx11=disabled`), so `glxgears`, `glxinfo` and
the X11 EGL demos are not on the host. They come from a box: the `app.mesa-utils` catalogue row
builds Debian's `mesa-utils` on the base image and puts `glxgears`, `glxinfo` and `es2gears_x11` on
the host's path as shims. Those three names are the ones the host does not carry, so a shim never
shadows a host binary; any other program in the box is reached with
`kdos-appbox -b app.mesa-utils run`:

```sh
kdos app install app.mesa-utils
es2gears_x11                                  # through EGL and Xwayland
glxinfo                                       # the GLX extension Xwayland serves
glxgears                                      # through GLX and Xwayland
kdos-appbox -b app.mesa-utils run eglinfo     # the boxed copy, not the host's
```

That box is also the host-versus-box measurement: its `es2gears_x11` runs the same test as the
host's `es2gears_wayland`, through Xwayland and a container, and the difference between the two
numbers is what that path costs.

### Where the ceilings are

`kdos-comp` has no frame-rate constant of its own. It paces off wlroots output frame events and so
follows whatever mode is set; a number that stops at a round figure is the mode, the driver or the
client, never a ceiling written into KDOS.

## The other harnesses

"In the guest" means the script runs as root inside a booted KDOS, sent there with
`vnc-shot.py --root-script`.

Three of these need application packs that the shipped ISO does not carry. The medium holds the
catalogue but no application packs (see
[Where a pack comes from](../03-architecture/packs-and-boxes.md#where-a-pack-comes-from)), so on a
stock ISO:

- `appsweep.sh` reads the pack index at `/mnt/iso/packs/PACKAGES`, finds fewer than ten packs, and
  refuses to sweep with exit status 2;
- `packlane.sh` looks for packs on the medium at `/mnt/iso/packs`, or in the store at
  `/var/lib/kdos/packs` when there is no medium, and reports every check that needs one (a base,
  `rt-gtk`, the index, `app.vorta` (`PACKLANE_APP`) and `app.meld` (`PACKLANE_QTAPP`)) as
  failed;
- `install-to-disk.sh` asks the installer for `app.zathura app.kcalc alpine` unless
  `INSTALL_PACKS` names others.

Run them against a medium or a store you have put those packs into.

| Harness | Runs | Does |
|---|---|---|
| `packlane.sh` | In the guest, desktop up | The pack lane end to end: the daemon, the keyring, an install from the medium or the store, the launchers, a box on a pack base, the telemetry and a timed launch, reporting pass, fail or skip with a reason and carrying on past a failure. It exercises no rollback, and its checks for a base and `rt-gtk` in the store fail until packs have been imported |
| `install-to-disk.sh` | In the guest, with `--disk` | Runs the installer unattended into the attached disk, with its interface on `/dev/tty3`. Every 15 seconds it prints a heartbeat: the seconds elapsed, the space used on the target mounted at `/mnt`, and the last line of `/var/log/kinstall.log`, so that a slow copy can be told from a wedged one. It gives up after `INSTALL_CAP` seconds (default 5400) and prints the log's last 40 lines at the end. The second half boots the disk with `--no-cdrom` |
| `bios-boot.sh` | On the host | Boots the ISO as a USB stick on an xHCI controller under SeaBIOS, with no OVMF anywhere. `vnc-shot.py` always boots UEFI, so it cannot answer this. The proof is the serial log reaching `switch_root`, not the exit status |
| `appsweep.sh` | In the guest, `--boot-disk` | For each catalogue application: install it off the medium, launch it through its shim, wait for the compositor to report a window, photograph it, and remove the box. Usage `appsweep.sh <outdev> <runtime\|all> [max]` |
| `appreport.sh` | On the host, needs ImageMagick | Reads what the sweep wrote to the scratch disk and renders it as a table and a contact sheet, flagging windows that mapped but painted nothing. Usage `appreport.sh <scratch.img> [outdir]` |
| `appbox-smoke.sh` | In the session, as the desktop user (`su - kdos -c 'WAYLAND_DISPLAY=wayland-0 testing/appbox-smoke.sh'`) | Does every boxed application's launcher start something: a cheap `command -v` pass (`--probe`), then real launches; names on the command line limit it to those |
| `hostcheck.sh` | In the guest | Whether the shipped binary a caller resolves by name is the one that can do the job, for example which `blkid` answers first on the search path, asked against a real LUKS container |
| `oomd-fire.sh` | In the guest | Puts the machine under real memory pressure and reads `kdos-oomd`'s own status for the kill count and victim. Victim *selection* is tested offline by `kdos-oomd --fixture`; this proves the daemon wakes |
| `usability.sh` | On the host | Drives the desktop the way a person does, with the pointer and chords only, optionally at a given size (e.g. `usability.sh 1920x1080`), and leaves a numbered contact sheet under `build/shots/usability` (`KDOS_USABILITY_OUT`); `testing/usability.md` is the checklist to read it against |
| `bootcheck/` | On the host | Plumbing for scripted boots with no display: `boot.sh [soft\|gl]` starts QEMU detached, `guest.py` runs a command on the serial console, `type.py` types into the console, `sock.py` owns one QEMU socket. `bootcheck/README.md` describes them |
| `quick.sh`, `quickpatch.sh` | Host, then guest | [The fast loop](#the-fast-loop) |
| `qemu-audio.sh` | Sourced by the run targets and the rig | Picks a working audio backend rather than hard-coding one, because QEMU aborts at start-up on a backend its build lacks, and sets the mixer's rate, because QEMU's default is 44100 and the guest drives the codec at 48000 |
| `qemu-hw/` | On the host, needs Docker and the NVIDIA container toolkit | The containerised QEMU with accelerated (virgl) graphics behind `make run-hw` and `make rundisk-hw` (`run.sh iso` and `run.sh disk`); `probe.py` and `verify.py` check the GPU path |
| `rig-image.sh`, `devdeps-image.sh` | On the host | Build the rig image (`kdos-qemu-py`, from `Dockerfile.qemu`) and the development image (`kdos-devdeps`, from `Dockerfile.devdeps`) |
| `barcheck.c`, `boxcheck.c` | Compiled by the self-test | The scrollbar's drawing against its hit test at every position, and the shell's two box questions against a fixture store |

`testing/notes/` holds the measurements the pack format rests on (`packs-w0.txt`: trailing bytes
after an EROFS image, forced ownership, and the other assumptions). It is read, not run.

`prepare_base.py`, `mini_build.py`, `test_runner.py` and `report_gen.py` build ports one at a time,
each in a fresh container, outside `make build`. They answer whether a single recipe builds against
a base system on its own, without the state an incremental build tree has accumulated; they do not
replace the build. They need Docker, and `prepare_base.py` also runs `sudo`:

- `prepare_base.py` builds the `os-dev` build container, runs phases `00_cross` to `31_compilers`
  into `build_test/` through `mini_build.py` inside it, reading each package phase's
  `packages.txt` or `packages.d/`, packs the result as `testing/kdos_fs.tar.gz`, writes
  `testing/Dockerfile.base` and builds the image `kdos-base-test` from it;
- `test_runner.py` runs `kpkg install -f <port>` for each port under `ports/core`, found one shelf
  down and named by its bare name (or the one named by `--package`), in a fresh `kdos-base-test`
  container, recording results in `testing/test_results.json` and logs under `testing/logs/`;
- `report_gen.py` summarises those results as `testing/report.html`.

### Undeclared link dependencies

`testing/depdrift.py` reads a built tree and names every package that links against, or whose
`.pc` file requires, a package outside its declared `depends` closure. Run it from the repository
root after a build, in a container with Python (`kdos-devdeps` or `os-dev`), and before trusting a
phase built with `kdosbuild --port-jobs`:

```sh
python3 testing/depdrift.py --root build/fs        # --repo <repo> overrides each phase's PORT_REPO; --strict exits 1 on a DRIFT line
```

It maps each path to its owner from the manifests in `<root>/var/lib/kpkg/db`, reads the
`DT_NEEDED` sonames of every ELF64 file a package owns and the `Requires` and `Requires.private`
modules of every `.pc` file, and resolves them under `usr/lib`, `lib` and `usr/local/lib` and the
`pkgconfig` directories. The declared closure and each port's phase and serial position come from
the recipes and phase lists, resolved as `phaseclosure.py` resolves them. Each undeclared owner is
classified:

| Class | Printed as | Means |
|---|---|---|
| Same phase | `DRIFT` | The owner is installed by the package's own phase outside its order run. Only the serial order made it present; under `--port-jobs` it can be absent. Add it to `depends` |
| Order run | `note` | The owner is installed by the same phase from its order run |
| Earlier phase | `note` | The owner is installed by an earlier phase, so it is present either way |
| Unplaced | `note` | The package or the owner is in no phase list |

It is advisory and preflight does not run it, because it needs a built tree. Exit status is 0
unless `--strict` is given and a `DRIFT` line was printed. A dependency loaded with `dlopen`, run
as a program or included only as a header is invisible to it.

### Debug information in the built tree

`testing/debuginfo.sh` lists the installed files that carry DWARF. `kpkg` strips nothing, so a file
has a `.debug_info` section exactly when its port's flags produced one, and no port is meant to.
Run it on a built root after changing a recipe's compile or link flags, or after adding a port
whose build system defaults to `-g`:

```sh
testing/debuginfo.sh              # build/fs
testing/debuginfo.sh <root>
```

It reads every ELF file under `usr/` with `readelf -S`, leaving out kernel modules, firmware, the
cross-target sysroots (`arm-none-eabi`, `avr`, `riscv*-elf`) and BPF objects, and groups what it
finds by the package whose manifest in `<root>/var/lib/kpkg/db` names the file, largest package
first, with each file's size and its `.debug_info` size. A file no manifest names is listed under
`(unowned)`. It changes nothing. Exit status is 1 when it reports a file and 0 when it reports none;
preflight does not run it, because it needs a built tree.

## What is not tested

This section lists what the harnesses above do not cover, so that a green run is not read as
evidence for it.

Nothing here tests the build itself ([How KDOS is built](how-kdos-is-built.md)), which takes hours
and a container. The self-test runs the orchestrator against a synthetic tree, which proves its
logic, not that the distribution builds.

The memory daemon fires only under `testing/oomd-fire.sh`, a rig run rather than a self-test block,
because it needs a booted machine with real memory to exhaust.

The pack lane (the [import lane](../06-reference/glossary.md), exercised by `packlane.sh` and
`appsweep.sh`) cannot run against the shipped ISO alone, because the packaging step bakes no
application packs into it. Nothing tests the lane until someone provides packs, and nothing tests a
pack rollback on a booted machine; see [The other harnesses](#the-other-harnesses).

Of the 55 names `kdos-shell` answers to, fourteen have no committed frame: `about`, `ascii`,
`audio`, `bt`, `cal`, `calc`, `clip`, `devices`, `ime`, `mediad`, `note`, `slit`, `time` and
`users`. `ascii`, `ime` and `mediad` have no dump at all. `clip` cannot be linked into the dump
harness, because it wants a protocol the harness does not generate. Most of the rest draw something
that depends on the host, as `testing/goldens/README` and the comment beside the golden list in
`selftest.sh` record: `cal` draws the current month, `kdos-slit` renders the output of commands it
runs, the mixer lists the machine's ALSA cards, and `kdos-users` reads its password file. `cal`
and the scanner half of `devices` are asserted by content checks rather than by a golden. A
golden is committed only for a dump that is the same on any host, so those surfaces have none.

The compositor and the shell are not compiled by the self-test on a bare host; use the development
image, and for the compositor the wlroots build in
[Compiling the compositor without a full build](#compiling-the-compositor-without-a-full-build).

Nothing that runs unattended has hovered a button. Defects such as a taskbar two rows above the
bottom of the screen, a tooltip that swallows the click on the button it describes, a Start button
whose label vanishes under the pointer, or an icon layer eighty columns wide on a 160-column screen
are green in `preflight.sh` and green in `selftest.sh`. `usability.sh` drives those paths and
photographs them; reading the result is a person's job.

A golden cannot see a translucent window. A surface that is seen through sets
`KDispConfig.opacity`, which the display applies to the one colour slot the surface's body is drawn
in; a dump records the slot, so a window at 70 per cent and one at 100 give identical goldens. The
same holds for `ktui_draw_blend`, which writes a blend as a cell's literal colour and leaves its
slot as drawn. Only the rig can show either.

The chart's pixels are held to properties and to digests of themselves, so a test says a picture
is unchanged and never that it reads well. The pixel tier of `kdos-res` (its tiles, and the sweep
that gives back the tile of a chart whose page is not on screen) runs only in a window, which no
dump reaches; the rig is the place to look at both.

The pointer's own pixels are in no test. The compositor draws the cursor, and a photograph of a
moving pointer is the only place to look at it.

## See also

- [Developing](developing.md): the build and iteration loops these fit into
- [How KDOS is built](how-kdos-is-built.md): the full build these harnesses let you avoid
- [Writing desktop software](writing-desktop-software.md): dumps and reference frames from the author's side
- [The C libraries](c-libraries.md): what the assertions cover
- [Build troubleshooting](build-troubleshooting.md): when preflight is not enough
- [Writing ports](writing-ports.md): recipes, sources and the pre-push hook
- [The ports catalogue](../06-reference/ports-catalogue.md): every port `test_runner.py` can build one at a time
- [Glossary](../06-reference/glossary.md): the terms this chapter uses
- [Known gaps](../06-reference/known-gaps.md): the full list of what does not exist

<!-- book-nav -->
---

*Part V — Building and developing, chapter 37.* Previous: [36. Writing desktop software](writing-desktop-software.md) · [Contents](../README.md) · Next: [38. The ports catalogue](../06-reference/ports-catalogue.md)
