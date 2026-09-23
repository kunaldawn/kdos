# Testing

KDOS has no unit-test framework and ships no test binaries. What it has instead is a preflight
check, a self-test over the libraries and their consumers, committed reference frames, recorded
system-state fixtures, and a virtual machine rig that drives a real session.

This page covers what each harness proves, what it cannot, and how to run it.

## What each tool proves

| Tool | Proves | Cannot | Roughly |
|---|---|---|---|
| `testing/preflight.sh` | The wiring: every reference resolves, every option exists, every script parses | That anything works | Seconds |
| `testing/selftest.sh` | The libraries' invariants, that every consumer still compiles, and that surfaces still lay out | That the system boots | Half a minute |
| Reference frames | That a surface's geometry and colours have not drifted | That it is usable | Included above |
| Fixtures | That a reading or a decision is correct against recorded state | That the reading is correct live | Included above |
| `testing/vnc-shot.py` | That a real session does a thing, photographed | Anything the renderer used cannot show | Minutes per boot |
| `testing/usability.sh` | That the desktop can be driven by a hand — hover, click, chord — photographed step by step | Nothing: it asserts nothing and is read by a person | Six minutes |
| `testing/docscheck.sh` | That the book still links up and states the present | Anything a reader has to judge for themselves | Seconds |
| `testing/packlane.sh` | The application lane end to end on a booted machine | | Minutes |
| `testing/install-to-disk.sh` | That the installer installs | | Minutes |
| `testing/bios-boot.sh` | That the image boots on a machine with no UEFI firmware at all | Anything about the UEFI path | ~2 minutes |

None of them proves the build works. A package manager can only really be tested by building the
distribution with it.

## preflight.sh

Everything a full build would catch, minus the build: forty-seven checks, in seconds. Ten of them
read the built tree under `build/fs`, and on a tree with no build each prints `skipped — no build
tree` instead of passing quietly, so a green run accounts for every check the file contains rather
than hiding the ones that declined to run.

| Group | Checks |
|---|---|
| Packages | Every package named in a phase list has a port; every list resolves to a dependency order with clean output and valid tokens; every dependency names a port that exists; the build tree carries no package whose port is gone |
| Recipes | Every port has a build script and it parses; every recipe parses as metadata; every one declares a name, version and release; every source a port declares is in the port directory, non-empty and named by a checksum; every port of ours is built by something |
| Build options | Every meson option a recipe passes is one that port defines, checked against the tarball's own option file, with the two closed-value types validated |
| Sources | Every source file in one of *our* ports is compiled by its recipe, unless that recipe globs its own `$PORT_SRC` directory — a glob of the shared `libk*` trees does not exempt it; a first source whose members are prefixed is accounted for; a flat first source is unpacked by its own recipe; every port's archive is in the tree through LFS |
| Shipped configuration | The shipped compositor configuration keeps its default bindings; every command it, the menus and `menu.conf`'s routes name exists; every program `fs/etc/inittab` names is on the image; every filesystem the installer offers, the initramfs can mount |
| Shell | All shipped and build shell is syntactically valid; a script a recipe ships inside a `KDOS_SH` heredoc parses too, and every program it names as the first word of a line is one the image carries; no build script names a command inside double quotes and runs it; every helper the makefile runs is on disk |
| Consistency | The build tree's root carries nothing but a root filesystem; every flag one shell tool passes another is one it accepts; every daemon an init script starts is installed by a port; the rootfs carries no script whose interpreter is gone; nothing points at a removed file; every `port:`, `path:`, `see:` and `cite:` a recorded reason names still resolves; every recipe carries the banner; no chroot step reads the ports tree through the wrong path; the catalogue's rows match the tree; a desktop toggle has one flag and only `libkbase` builds its path; a frame that opens the synchronized bracket closes it on the dropped write and on the way out; a literal colour is set at the render boundary and nowhere else |
| Chrome | Every glyph in `libktui`'s UTF-8 table is one the shipped console font can draw; every icon name and command a shipped desktop entry asks for resolves on the image |

Four of those deserve singling out, because each is a whole class of failure that never reaches a
compiler.

Every source file in one of our ports is compiled by its recipe. A file the build script
neither names nor matches passes every gate on a development host and fails to *link* in the build,
because the self-test globs those directories — so a whole page can be exercised by the harness and
be absent from the shipped binary.

Every meson option is one the port defines. meson fails at setup on an unknown option, before a
line is compiled, and there is no universal spelling. Without this check a wrong spelling is an
hour-long round trip through the build to find.

Every chrome glyph is one `ter-kdos32n` carries. The toolkit picks its UTF-8 table whenever the
backend reports UTF-8, which the Linux console does, and the console font holds 512 glyphs — so an
entry that font lacks renders on `tty1` as a blank. Not a fallback and not an error: the cell is
written, the flush succeeds, and a piece of the desktop's own chrome is missing. `▓`, the
half blocks `▀ ▄` and the double tees `╠ ╣ ╦ ╩` all look perfectly reasonable in an editor and are
none of them in the font. The check reads the shipped font, because a list of what it carries is
the thing that goes stale. Goldens cannot catch this: they are slot dumps at the ASCII tier, where
every one of those glyphs resolves to something.

Every flag one shell tool passes another is one it accepts. These tools spawn each other by
name, and an unknown argument prints a usage line to an error stream nobody reads and exits before
a surface exists. The result is a control that silently does nothing, invisible to a compile and to
a reference frame. The check is crude on purpose: find the argument literals, take every long flag,
and require it to appear in the target's own source.

Preflight cannot prove the build works. It can prove the build will not fail for one of the dull
reasons. Run it after touching a phase list, a recipe, or anything under the build scripts.

## selftest.sh

The host-side regression net for `src/libs/`.

It compiles every library with the host compiler under warnings-as-errors, runs the shared
assertion program against them, compiles the consumers to prove the headers still agree, resolves a
port to prove the ports tree still parses, audits a generated theme, renders every front end
offscreen and compares against committed frames, and runs the orchestrator end to end against a
synthetic tree.

No container, no network, about half a minute — most of it the artwork the theme audit generates
three times, which is the price of testing that claim against the real vendored assets.

Its assertions are the invariants, most of which were established by comparing against the
implementations these libraries replaced. Each is a claim that was measured once; the suite is what
notices when it stops being true. One is counter-intuitive and worth knowing: the two colour-mixing
functions are asserted to *disagree*, because each generated file was written against exactly one
of them.

A test whose only subject is code nothing ships is not coverage — it is a second implementation
with an audience of one, and it reports green while the behaviour a person sees is untested. When
the last shipped caller of something goes, its assertions go with it.

### The window-model contract

`libkwm`'s block asserts nothing of its own. It replays `testing/fixtures/wm/geometry.txt`, every
row of which was derived by reading a named line of `kdos-comp` and cites it. A failure means the
library and the compositor have parted company, which is the one thing keeping the window model out
of the compositor exists to catch.

Add a case by adding a row and citing its line, never by writing an assertion in `selftest.c`. A
row whose expected value came from taste rather than from the source is worse than no row: it makes
the library authoritative over the behaviour it was supposed to reproduce.

The file is self-checking in one direction. The `geom` rows are pure arithmetic over the formula in
`view_get_edge_snap_box`, so they can be re-derived mechanically. Two different mistakes are caught
by two different passes: mechanical re-derivation catches a row computed wrong by hand, and
replaying the file against the library catches a row transcribed with a flag inverted. When they
disagree, the fixture is what gets corrected — the source is the authority.

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite is clean that way and stays that way. It catches what a plain run cannot: an archive size
field overflowing a signed type, where the negative becomes an unsigned length and a read is asked
for an impossible number of bytes into a small stack buffer; a copy called with a null source on an
entry's first key; and a read past the end of the cell buffer in the sprite table's eviction check,
which walks `ktui_w * ktui_h` cells of a buffer only as large as it was last resized to.

A variadic printf wrapper guards its own format, in `libkbase` and in the two programs with one of
their own. A null format is undefined in `vfprintf` anyway, and the sanitiser build's
interprocedural pass cannot prove one non-null across a whole program compiled in a single line —
so without the guard `-Wformat-overflow` refuses to build the suite at all, and the sanitiser run
becomes a run nobody can do.

Leak checking is off by default and on for the library assertions alone. Every program here owns
its parsed state until it exits, which a leak checker reports as a leak and turns into a false
failure; the library suite is the one binary whose subject is code called repeatedly.

A fuzzer over the image parsers is committed as `testing/fixtures/img/fuzz.c` — its own driver
rather than a block in `selftest.c`, because the corpus has to run under ASan and UBSan and the
rest of the suite does not need rebuilding to do that. Two passes: every fixture decoded under a
budget, then every fixture mutated a byte at a time and truncated at every length, because a corpus
somebody wrote by hand only ever exercises the paths they thought of. A decoder must answer NULL or
an image for any bytes at all, and must never read past the end of them.

### What it does not cover

The compositor and the shell compile here only where their Wayland dependencies exist, which a bare
development host does not have — so on most machines those blocks report as skipped.

A skip that cannot be lifted is a test nobody runs. Three shapes make a whole block unreachable on
any ordinary host: a library name decided *below* the block that read it, a tool assumed universal
with no guard so its absence takes the rest of the suite with it, and an include path only one
machine has. A missing tool is a skip with a name, which is the rule every conditional block keeps.

What lifting those found: several address truncations, a device path built in a buffer far too
small for a real device name — where truncating opens the wrong node — a dead list, and reference
frames stale by a whole page.

The shell compiles here with format-truncation warnings disabled and nothing else relaxed. That
program truncates on purpose: every label goes into a fixed number of cells, so a truncated label
is the intended behaviour. Where truncating *is* a defect it is not a label — a socket path, a
device node — and those are held by explicit bounds rather than by a warning that cannot tell the
two apart.

### The cached host helpers

`ports/` caches compiled helpers, and a binary built against one C library cannot execute under the
other, so a host run straight after a container run must clear them first.

The failure does not say any of that. The metadata reader fails, so the version checker
cannot render a candidate and the suite reports that it reproduced no outcome — which reads as a
regression in the version checker.

A container run as root also leaves those directories root-owned, so the removal fails on
permissions and the stale binary survives. Delete them from a container rather than reaching for
elevated privileges.

## Goldens

Committed reference frames: a surface rendered offscreen and compared byte for byte. There are 193
of them under `testing/goldens/`, across twelve sizes, covering the shell's front ends, all eleven
monitor pages plus its detail page, the terminal, the cell-level frames, and the nine replayed
terminal recordings.

| Kind | Catches | Count |
|---|---|---|
| Text frames (`--dump`) | Geometry: overflow, misalignment, a control drawn past its rectangle | 175 |
| Cell frames (`--dump-cells`) | Colour and attribute drift as well | 9 |
| Replayed streams (`vt-*`) | A change in the state machine, against bytes real programs wrote — the characters, the attributes, the cells that named a colour of their own, the hyperlinks and the prompt marks | 9 |

A terminal's frame is taken by running a command to completion. `kdos-term --dump` settles the
child and consumes everything it wrote before drawing, because a frame taken while a program is
still writing is a different frame every time it is taken. Two of the four hold a picture: a dump
has no pixels, so a sprite renders as its fallback in the picture's top-left cell and as blanks
under the rest, which is exactly what a tty and a view with no pixel library show. What the frame
asserts is the shape — how many rows the picture took, and where the cursor was left afterwards.

What the frames assert is everything a stack claims to cost nothing: two tabs on one title row, the
live one carrying the ring number and the resting one carrying none — `win_index()` answers 0 for a
hidden window, so a number on both would mean a member the ring can still step to — and one taskbar
row for the two of them, which is the half a reader would not think to look at and the half that
breaks first.

Commit two sizes minimum for anything with a layout, because a geometry defect is usually a defect
at one width. The monitor's pages carry three, including a narrow one that forces its sidebar to
degrade.

### Rules the harness keeps

A cell frame's verdict must actually be acted on. A comparison whose answer nothing reads is a
comparison that cannot fail, so the cell half needs its own check with its own message — a colour
drift and a geometry drift are fixed by looking at different things.

The dump harness stubs the icon layer to "no picture", so a reference frame is the character grid
and a layout that only lines up once the pictures load is a layout that is broken. That harness
must stub the *whole* interface a front end calls: a missing stub is a link failure that reads as
"the front ends do not compile" and takes every frame behind it.

A rasteriser draws at the font's maximum advance, so a container photograph is spaced out.
`libkcell` takes its cell width from `max_advance.x`, which for a font with CJK coverage — the
fontconfig default on a build container — is about twice the Latin advance, and the glyphs are then
drawn at the left of a cell twice their width. The shipped image's VT font has one advance for
everything, so this is the borrowed font talking and not the painter. Judge a shot on the image, or
pass `--font`.

A golden that differs from `HEAD` may be uncommitted work rather than drift. These frames are
regenerated as the tree changes and are often modified in the working tree ahead of the source
change that lands with them, so `git checkout --` on one throws away the very thing it records and
the next run reports a difference that looks like a flake. `git diff HEAD` on the *source* is what
tells the two apart; run the dump twice if you still cannot.

A front end that needs a library the harness does not offer is skipped by name. `kdos-peek` decodes
with `libkimg` and probes archives with `libarchive`, so both the shell-wide compile and the dump
harness admit it only when `libarchive`, `libpng`, `libjpeg` and `libwebp` are all present, and say
so out loud when they are not. Gating the *whole* shell compile on them instead would take the
other forty-odd files with it on a host that lacks one dev package.

`libkdisp` is stubbed, not linked, and adding a `kdisp_` entry point breaks the harness. The stubs
are in `testing/fixtures/shell/dumpmain.c` and there is no compiler check that they cover the
header, so a new one links as "undefined reference" and every front-end frame behind it is skipped.
Linking the real `libkdisp` beside them is not the fix; it is thirty *multiple definition* errors.
Add the stub, and make it answer the state a dump actually has: `kdisp_win_supported()` is 0 because
a dump renders one frame with no compositor, and a stub that invented two windows would make the
frames assert a fiction.

A dump's font list comes out of `$KDOS_FONT_LIST`, because the real one is the host's. Under a
display, `kdisp_font_count` is fontconfig's monospaced families — one name in the image and hundreds
on a developer's box — so `dumpmain.c` reads its rows from that file, one family per line, and the
suite writes the file into `$OUT` rather than committing it under `testing/fixtures/shell`, where a
fixture added to enrich one surface moves another's frames. Two goldens ride on it: `theme-font`
over names carrying a space, escaped punctuation and a size key, so the frame proves the name column
is cut and the sample is not; and `theme-font-none`, taken with the variable unset, which is the
page's empty branch.

A surface that exits non-zero costs its own golden and not the run. `set -e` is on, so a surface
refusing a flag would end the suite where it stands — every check below it, including the ones that
say whether the goldens are committed at all, never running, and the failure reading as "the suite
stopped" rather than as one bad frame. `golden()` and `cells_golden()` catch the status and record
it, which is the rule the candidate compile loop keeps: each is admitted on its own.

One missing stub costs every front-end golden, not its own. The link is one command over the whole
family, so `undefined reference to kch_px_bare` in `notifyd.c` skips the panel, the desktop, the
calendar and the other forty with it — and the suite says so in one NOTE and then passes. That is
the shape to expect: a run that is green and a `ls testing/goldens/` that is missing a name is this
failure, not a surface nobody wrote a golden for.

The other half of that link is the opposite mistake. A stub for a symbol one of the linked files
now defines is a *multiple definition*, which carries neither the word `undefined` nor the word
`error` — a filter looking for those two reports only half the breakage and sends the reader
hunting for a missing library. `osd.c` is the worked example: it defines `sh_volume_get` and
`sh_mic_muted`, which `panel.c` calls, so it is in the always-linked set beside `cal.c` and
`shell.c` rather than in the candidate loop. A file that is sometimes linked and sometimes stubbed
collides on exactly the hosts where the harness otherwise works.

Every frame that exists carries the row that names its keys, and the suite checks that against the
committed goldens rather than against a fresh dump — a blank bottom row is a surface whose keys
nobody can find, whatever it drew the day the golden was written. Furniture is exempt and is named,
not patterned: the taskbar, the tooltip, the savers and the two menus are drawn on the desktop
rather than in a window, a saver closes on any key and a tooltip answers none, so a row naming `Esc`
on either would teach a key that does nothing. `menu.c` is the one file that holds a `KtuiKeys` and
draws no row, for the reason its own header gives. A pattern broad enough to skip these would also
skip a real surface, which is why each is a name with a reason beside it.

A golden holding a sixel picture is guarded on the sixel decoder being compiled in, not on
`libkimg` being compiled at all. pixman alone builds a `libkimg` with no sixel in it, and running
the picture test against that build produces a diff that blames the terminal.

### Regenerating

Regeneration is a variable on the self-test, and must be done where the Wayland dependencies exist
— a build container, not a bare host:

```sh
docker run --rm -v "$PWD:/kdos" -w /kdos \
    kdos-devdeps:latest sh -c 'fc-cache -f; KDOS_GOLDEN_UPDATE=1 testing/selftest.sh'
```

The terminal's frames are the exception: `kdos-term` builds console-only on any host, which is the
point of that build.

A font and GNU `tar` are not optional in that image, and neither absence looks like itself. `fcft`
resolves `monospace` through fontconfig, so no font at all answers `failed to match font` — which
reads as a `libkcell` failure and stops the run at the first rasteriser block. Busybox's `tar` makes
the reproducible-build block answer "the synthetic port did not build", and every front-end golden
is gated behind that block, so all of them are skipped in silence. Both are in the Dockerfile for
that reason. `os-dev` has the packaging toolchain and no `wayland-client`, so it skips the front-end
dumps entirely.

## Fixtures

A fixture is recorded system state that a reader can be pointed at instead of the live machine.
This is what makes a monitor, an attribution engine or a kill-selection policy testable at all. The
seam is the same everywhere: the process and system filesystems live behind a movable root, or a
variable moves one walk.

| Fixture | Records | Makes testable |
|---|---|---|
| `res` | A process and system tree | Every monitor page, deterministically |
| `stutter` | Two snapshots half a second apart, plus the frame events between them | That the application is named with its box, the blocked process is first, pressure is quoted, and exactly one event is blamed on the compositor |
| `energy` | Four recorded power and process trees | The nesting rule, the counter wrap, the roll-up onto one application, and the short-lived residue |
| `oomd` | A tree arranged so only the memory budget can produce the right answer | That the budget check is load-bearing — its host process is larger than anything in either box |
| `mountd` | A block-device tree plus two hand-built superblocks | The acceptance, and both refusals — the internal disk carries a real superblock so a broken check shows up as an extra row rather than as nothing |
| `privacy` | Three processes, one holding a camera twice, one an audio device that must be ignored | The camera half, on a machine with no camera |
| `portup` | Recorded upstream responses — a registry index, git tag lists and branch heads, feeds, a releases API answer (cut to the fields read), directory listings, the pages a listing stands in for or links to — and, under `ports/`, the recipes seven ports were recorded at | Every discovery adapter and filter in `--selftest`, and all three outcomes end to end, offline and unmoved by bumps to the live recipes |
| `cve` | Four ports and a five-row database | A pin behind two fixes, one that only looks behind because of a packaging revision, a name mapping, and a package the database never heard of |
| `clone` | Hand-built image headers | The two-record length rule |
| `tray` | A second *process* that behaves like a real tray item | The whole protocol conversation |
| `shell` | The dump harness and its stubs | Every front end's layout |
| `res/*/sys/class/net/*/device` | A `uevent` file, because the directory is the reading | Whether an interface is real — the test is the presence of that directory, and git stores no empty one, so a bare marker directory is absent from every clone and the frames that depend on it cannot be reproduced |
| `shell/rec` | Two PCM lines — one playback-only, one with a capture stream — and 25 600 bytes of raw signed 16-bit: four ticks at half full scale, then four of exact zeros | That the input list is *filtered* rather than merely listed, and that the level is arithmetic over samples. The recorder's `--meter` prints one line per tick and `--write` produces a WAV compared byte for byte against `shell/Recordings/2026-01-01-000000.wav` |
| `vt` | What `vim`, `htop`, `mc`, `less` and `tmux` wrote to an 80x24 pty, plus four hand-written streams | That the libtsm fork's state machine still produces the same screen |
| `img` | Images, and `fuzz.c` beside them | `libkimg` — every fixture decoded, then mutated and truncated |
| `pack`, `box`, `deco`, `openwith`, `recent`, `tone`, `cellclip`, `ascii` | | Their respective units |

A recorded stream is not a running program. Beside the `vt` fixtures the suite opens real programs
on a real pty and presses one key each: `less`, `nvim`, `htop`, `top`, `mc`, `lf`, `tmux`, `nano`
and `taskwarrior-tui`. The names are the ones this system installs — a row naming `vim` exits 127
on every KDOS machine and is skipped, which is a green tick for a test that never ran. A program
the host has not got is skipped by that same 127, so the block asserts where the programs are and
stays silent where they are not.

Both traps the `mountd` fixture guards were confirmed to bite by building the daemon with each
check disabled, which is the only way to know a test is testing something.

The tray fixture is a second process on purpose. The protocol is a conversation between two peers
on a message bus, and a mock of either side would pass on the silent bugs that implementation
carried.

### The terminal fixtures

The terminal fixtures are never re-derived. A re-recording picks up a different program version, a
different terminfo and a different hostname, so a fixture that regenerated itself would be a test
that changed its own question — the hostname and the clock inside them are part of the recording,
not something live.

Each stops on a live frame rather than on the program's exit. A stream ending with the alternate
screen being restored renders to an empty grid, and so does a parser that gave up on the first
byte, so the self-test refuses an empty grid outright.

Four of the nine are hand-written, because no recording happens to contain what they pin:
`malformed.esc` (an unterminated CSI, a parameter past any bound, an OSC with no terminator, a
truncated UTF-8 lead byte, a surrogate, a DCS carrying rubbish), `attrs.esc` (the style SGRs with
their offs, the underline shapes and colour, and the four spellings of a colour), `links.esc` (`OSC
8` around a run, the empty form that closes it, the same address twice, a scheme the whitelist
refuses, and a `params` field that must be ignored) and `prompts.esc` (`OSC 133` marking two
finished commands and a third still running).

`vtrender.c` replays one in small uneven chunks, because a pty splits escape sequences across reads
and a parser that only works on a whole sequence passes a single-write test and corrupts a real
terminal. Its output is five blocks: the characters; the attributes, one letter per cell — a style
has no character to show, so a golden holding only the text could not tell an italic comment from
an upright one, and an underline's shape is its own digit; which cells carry a colour of their own;
which cells are a hyperlink, as the id itself, so that one run reads as one link and the same
address twice reads as the same digit; and one character per *row* for the prompt marks.

No colour value is ever in a golden. A value moves with the theme, and a palette change reading as
"vim drifted" would be a test that changed its own question. What the third block holds is the
*decision* — the sixteen named colours reduce to slots and follow `kdos theme`, and everything
above them is a literal the program chose — so it drifts only when that rule does.

The same rule applies to the terminal's own input. `ktui_input_next` reads descriptor 0, so the
bracketed-paste block stands a pipe there and writes the sequence in pieces on purpose: a
terminator split across two reads is the case that turns a paste into a session-long one when the
tail is taken for text. It restores descriptor 0 before it returns — a block that redirected stdin
and left it redirected takes every later block that reads a terminal with it.

## The machine where nothing is skipped

`selftest.sh` runs everywhere and skips what it cannot build, saying so each time. What it skips on
a bare host is most of the interesting half: `libkimg`'s four decoders, the sd-bus blocks, `fcft`,
the Wayland consumers. A block skipped on every machine is a block nobody runs.

A bare host still reaches every section and exits clean. What it loses is inside them, and each
block it cannot build says so by name. The build image is where none of those lines appear, so it
is the only run that exercises every front-end dump and the surface goldens behind them.

```sh
testing/devdeps-image.sh                    # builds the image, then runs the suite in it
testing/devdeps-image.sh bash               # or a shell in it
```

`testing/Dockerfile.devdeps` carries exactly what the script probes for. It is Alpine because the
target is musl and a feature-test difference is worth meeting here rather than in a phase build.
Two guards it still does not satisfy are named in the file rather than left to be found: `wlroots`,
which no distribution packages and which the next section builds, and `busctl`, which Alpine ships
in no package — so the portal block stays skipped there even though both its other halves are
present.

Two of its packages are there for what they unblock rather than for what they are. Alpine ships no
font at all, and `fcft` resolves `monospace` through fontconfig — with none, the first rasteriser
block answers `failed to match font` and the run stops there. Its `tar` is busybox's, and the
reproducible-build block answers "the synthetic port did not build" with it. Each failure gates
everything after it, so between them they hide two thirds of the suite while reporting nothing but
their own one-line error.

## Compiling the compositor without a full build

`kdos-comp` needs `wlroots-0.20`, which no distribution packages and which the self-test therefore
reports as skipped. That is not the same as unbuildable: the tree already carries the source.

```sh
# ports/core/wlroots/wlroots-0.20.2.tar.gz is a release asset, fetched by bootstrap
FROM kdos-devdeps:latest
RUN apk add --no-cache meson ninja pango-dev libdrm-dev libinput-dev libseat-dev \
      mesa-dev libxkbcommon-dev wayland-dev wayland-protocols hwdata-dev \
      libdisplay-info-dev libxcb-dev xcb-util-wm-dev xcb-util-renderutil-dev \
      xcb-util-image-dev libx11-dev xcb-util-dev xwayland xwayland-dev
# then meson setup / compile / install the tarball with -Dxwayland=enabled
```

`kdos-comp` then configures, compiles and links in that image. Two concessions matter when reading
a failure:

- `-Dicon=disabled`. `libsfdo` is packaged nowhere available, and the `icon` feature is what
  requires it. Anything guarded by `HAVE_LIBSFDO` is not compiled, so a change in that code is not
  covered here.
- `-D_GNU_SOURCE`. `kdos-thumb.c` calls `fileno`, and musl's feature-test defaults differ from the
  ones the real phase build gets.

Build the unmodified tree first. With a baseline binary in hand, every error after a change belongs
to the change. What this proves is that a port is type-correct and links; it does not prove a
window lands where a person expects, which is still the rig's job.

## Running a shipped program without booting

`build/fs` is a complete musl root, so a program already installed there can be run directly — no
ISO, no emulator, seconds rather than minutes:

```sh
docker run --rm -v $PWD/build/fs:/rootfs -v /path/to/inputs:/rootfs/in:ro \
    alpine chroot /rootfs /bin/sh -c 'w3m -dump /in/page.html'
```

This is how a filter chain, a converter or any other program that reads a file and writes text is
checked against the binaries that ship rather than the host's. Three limits, each of which has
changed a reading:

- There is no `/proc` and no `/sys`. Anything that reads either behaves differently; w3m prints a
  garbage-collector warning here that it does not print on the machine.
- `unshare` is refused inside the chroot, so a program that probes for a namespace takes its
  fallback path. That makes the fallback easy to exercise and the namespace path impossible to —
  and loosening the container does not help: measured, `--privileged`, `seccomp=unconfined`,
  `apparmor=unconfined` and `--cap-add SYS_ADMIN` all still answer `Operation not permitted` for a
  `chroot`ed process, while the same command outside the chroot succeeds under any of them.
- Nothing is supervised and no session exists. A program that wants `$XDG_RUNTIME_DIR`, a bus or a
  terminal is the rig's job, not this one.

Everything it writes stays in `build/fs`, and the ISO is built from `build/fs`. A bind mount creates
its own mountpoint — `-v inputs:/rootfs/in` leaves `build/fs/in` behind — and a program run under
the chroot writes to `/root`, `/tmp` and wherever else it likes. None of that is owned by a package
or by `fs/`, so neither the orphan sweep nor the fs-manifest guard removes it, and it ships. Bind
inputs read-only under `/tmp`, clean up after a run, and let `testing/preflight.sh` check the root:
it refuses a `build/fs` whose top level is not a root filesystem.

## The QEMU rig

A dump proves a character, never a colour. A `--dump` writes the codepoint in each cell and throws
the foreground and background away, so text drawn in the background's own slot — present, and
invisible on every screen — dumps identically to text a person can read. A check on what a surface
*drew* asserts the cell's colours as well as its character; a dump answers "is it there", not "can
it be seen".

Prefer a surface's own `--dump` over a photograph. It hands out that surface's exact composited
grid, so a check on what it drew is a text diff rather than an image comparison — no boot, no
framebuffer, no tolerance for antialiasing. The rig stays necessary for anything a dump cannot
show: the phosphor pass, a real modeset, window management, a boxed client.

`testing/vnc-shot.py` boots a real image, drives it, and reads the framebuffer. It boots headless
with a serial socket and a monitor socket, types on the first terminal through the monitor, and
reads the framebuffer over the remote-framebuffer protocol.

The session is already there when the rig starts driving. `/etc/inittab` respawns `kdos-getty` on
`tty1`, which starts `kdos-login`; that autologins, and `.bash_profile` starts `kdos-desktop` — so
the desktop is up before any step runs.

`--keys` is a monitor `sendkey`, so it reaches whatever owns the active VT, which is that desktop.
`--cmd` runs on the serial console as the desktop user, and `--root-cmd` as root, so neither
disturbs what is on screen.

### The fast loop

`make build` with packaging is seven and a half minutes, five and a half of them the ISO. Repacking
the whole medium to carry a 200 KB binary makes every look-at-it-on-screen cycle twelve minutes.

```sh
testing/quick.sh kdos-comp,kdos-shell -- --keys meta_l-ret --sleep 3 \
                                        --shot /kdos/build/shots/x.png
```

`quick.sh` builds the named ports into `build/fs` with no packaging (measured 1m09s), tars exactly
the files those ports own, and hands them to a booted ISO on a raw disk, where `quickpatch.sh`
untars them over the live medium's RAM overlay and restarts the session. Measured 3 minutes, or
1m37s with `KDOS_QUICK_KEEP=1` and `KDOS_QUICK_NOBUILD=1`.

| Variable | Does |
|---|---|
| `KDOS_QUICK_PHASES` | Widen the build, e.g. `04_phase4,05_desktop` |
| `KDOS_QUICK_NOBUILD=1` | Reuse what is already in `build/fs` |
| `KDOS_QUICK_KEEP=1` | Do not restart the session |
| `KDOS_QUICK_FILES` | Extra paths under `build/fs` to carry — a config file, a chord table |
| `KDOS_QUICK_SKEL=1` | Copy `/etc/skel/.config` over the live user's config in the guest |

The file list is the package database's — `build/fs/var/lib/kpkg/db/<port>` is what that port
installed — so a program that grew a new name or a new data file is carried without anyone
remembering to add it.

`KDOS_QUICK_KEEP=1` is right whenever the program under test is spawned. Every `kdos-shell` surface
is started fresh by the chord that opens it, so the new binary runs with no restart at all and the
run is a minute shorter. It is wrong for `kdos-comp`, which is the session.

`KDOS_QUICK_FILES` is how a file from the tree's `fs/` rides along: those paths are installed into
`build/fs` by the file-system step and belong to no package, so the database walk does not find
them. The file-system step must have run for them to be there. `KDOS_QUICK_SKEL=1` goes with it for
a skel file, because `/etc/skel` seeds a *new* account and the desktop user's home was seeded at
install time — a chord table patched in skel changes nothing for the person logged in.

Three things the loop still cannot carry:

- A new port, a kernel change or an initramfs change. Use the real build.
- Anything that owns a D-Bus name. `quickpatch.sh` restarts the session and kills the shell
  surfaces, and even so a notification raised in a restarted session has never been seen to draw
  while the same call on a booted ISO does. Verify the bus on a real boot.
- Evidence about the shipped image. What `make build` writes is the ISO; this is the loop you
  iterate in *before* you take the photograph.

Two traps it removes, both measured the hard way. The compositor's socket file outlives the process
that bound it, so "the socket exists" is true one millisecond after the kill and the steps then run
against the binaries the patch replaced — the wait is for a *different* pid, and the stale socket is
removed. And `/etc/inittab` respawns `tty1`, which does not come back when the chain is killed, so
the script starts `kdos-desktop` itself, which is what the login shell's profile would have done.

### The flags

| Flag | Does |
|---|---|
| `--disk`, `--boot-disk` | Attach and boot a disk image instead of the ISO |
| `--no-cdrom` | Leave the ISO off, so the disk is what boots |
| `--no-session` | Do not start a session. `tty1` is already the desktop, so this is the flag every check of it wants; without it the run opens a terminal with Super+Return, types `kdos-desktop` into it and waits — four more minutes before the first step |
| `--shot <file>`, `--out <file>` | Capture the framebuffer, and the default capture path |
| `--keys`, `--chord`, `--click x,y`, `--mouse x,y`, `--drag x1,y1,x2,y2` | Drive it |
| `--press x,y[,btn]`, `--release [x,y]` | A drag held open across later steps, so a `--shot` between two `--press`es photographs it in progress. A button is down only while its RFB client is connected, so the whole gesture travels one socket — which is why this is two flags and not a longer `--drag` |
| `--sweep` | A pointer sweep across the screen |
| `--type`, `--text` | Type into whatever has the focus — `--type` ends with Return, `--text` does not |
| `--sleep`, `--wait`, `--soak <s>` | Timing |
| `--cmd`, `--root-cmd` | Run something in the guest |
| `--root-script <file>` | Send a local script in and run it as root |
| `--console-cmd` | Type on the first terminal *instead of* starting a session |
| `--audio` | Give the guest a sound controller |
| `--data-disk <file>` | Carry one file's bytes into a guest with no network |
| `--gl` | A GPU for GL — and no Vulkan, so a Vulkan tool run beside it measures lavapipe |
| `--venus` | Ask for Vulkan on the GPU as well. Measured broken on an NVIDIA host at both ends — see below |
| `--scratch`, `--usb`, `--keep`, `--serial-log`, `--size`, `--vnc-port`, `--script-timeout` | The rest |

Five of those answer questions a screenshot alone cannot.

`--audio` gives a real device as far as the guest is concerned, with the samples going nowhere.
Without it the sound library fails to initialise and every audio path in the guest is untestable,
which is not the same as untested. It is `hda-output` — playback only, so it gives the guest no
capture device and no capture signal. Recording is tested through `snd-aloop` instead, which needs
no emulator flag at all.

`--console-cmd` photographs a program at the console font and the low glyph tier it has to read in.
A window under a compositor is a different renderer answering a different question.

`--soak` lets the session run between launch and measurement, because a monitor's own cost over a
few seconds is its startup, and startup is exactly what is not being measured.

`--venus` is the only way to ask for Vulkan on a GPU, and on an NVIDIA host neither end of it works.
`--gl` alone starts `virtio-vga-gl` with no Vulkan capset, so the guest's virtio ICD opens nothing
and every Vulkan tool lands on lavapipe, the CPU rasteriser, without saying so — `vulkaninfo
--summary` naming `llvmpipe` with `PHYSICAL_DEVICE_TYPE_CPU` is the tell, and a `vkcube` or
`vkgears` rate taken that way is a number about the host's cores. `--venus` adds `venus=true` and
blob resources, which needs the guest's RAM out of a shared `memfd`, so it also changes the
machine's memory backing. Measured: with `--gpus all`, which is the only way the container has a
host Vulkan device to proxy to, QEMU aborts during guest boot, exit 134; without it the guest boots,
the virtio ICD loads, and instance creation dies with it — `vulkaninfo` answers
`ERROR_OUT_OF_HOST_MEMORY` and even lavapipe is gone. So it stays opt-in, the default device does
not change, and a Vulkan number from this rig is labelled lavapipe until `vulkaninfo` names a GPU.

`--root-script` is the form for a check too long to be one command. It travels encoded, in short
lines — a terminal in canonical mode drops everything past its line limit, silently, and an encoded
payload contains nothing the shell acts on before it is decoded — and the rig waits for a marker the
*guest* echoes, so a step taking minutes is waited for rather than truncated. A plain command waits
a fixed short time, so anything longer must be a script.

### Three things the rig has to get right

- A display dump answers "no surface" under accelerated graphics, so the framebuffer is read over
  the network protocol instead — whose encoding negotiation has an exact field layout, and an extra
  padding field desynchronises the stream so every later read blocks for ever.
- Driving the session means a *login* shell. A plain switch-user leaves the container tooling
  resolving the home directory to the filesystem root, and every call fails on a permission error.
- A plain virtual display puts the compositor on software rendering, so the phosphor pass declines
  and is not in the photograph. What is photographed is the cell grid underneath it.

### A real capture device, with no emulator flag

The emulated codec has no ADC, so nothing `--audio` gives can produce a capture signal. `snd-aloop`
is the answer and it is already on the image (`CONFIG_SND_ALOOP=m`, `snd-aloop.ko.zst` shipped): a
root script loads it and the guest gains card `Loopback` with two PCMs, each carrying eight playback
and eight capture subdevices, cross-wired — what is written to `hw:Loopback,0` is read from
`hw:Loopback,1`.

```sh
modprobe snd-aloop
sox -n -r 16000 -c 1 -t alsa hw:Loopback,0 synth 8 sine 200-2000 vol 0.5 &
sleep 2
sox -q -t alsa hw:Loopback,1 -t raw -e signed -b 16 -c 1 -r 16000 /tmp/cap.raw trim 0 3
```

Run it under `--no-session`: the run starts no second session and PipeWire never starts, so the
loopback's capture side is free.

- A swept tone is the only version of this test that can fail. Silence and a flat baseline pass
  identically whether a level is computed from the samples or hardcoded to zero. The three seconds
  above come back as exactly 96 000 bytes peaking at 16382 — half of full scale, which is the `vol
  0.5` that was played.
- The rates need not match. The player above negotiates 48 kHz and the capture asks for 16 kHz
  signed 16-bit; ALSA's plug layer converts, `sox` warns that it cannot encode the format natively,
  and the bytes that arrive are correct. What must be true is that the player opens first, so there
  is a stream for the capture side to read.
- `sync` before the harness kills the emulator, or a file written in the guest comes back
  zero-length.

### The transport for a large artefact

Use a plain virtual disk, not a USB device. Emulated USB storage measured at under two megabytes a
second against a virtual disk's several hundred — a large pack took over an hour one way and seconds
the other. Use the USB path only when the test needs a genuinely removable device, which is the
removable-media daemon's whole subject.

The file is read back by block copy and truncated to its exact length, because a raw drive is
rounded up to a sector boundary.

### The host may not have an emulator

The rig runs unmodified inside a container image that carries one, with the repository bind mounted
and hardware virtualisation passed through. The image is built from this tree, by
`testing/rig-image.sh` out of `testing/Dockerfile.qemu`:

```sh
testing/rig-image.sh          # network, once; everything after it is offline
```

It carries QEMU, the OVMF firmware an EFI boot needs, and python3 — and nothing else, because
`vnc-shot.py` speaks the remote-framebuffer protocol out of the standard library. There is no pip
step and no wheel to pin, so the only versions that matter are the distribution's QEMU and OVMF.

Building the image here rather than describing a machine's history is what makes a photograph
reproducible from a clone: an image that exists only where it was first built is a test nobody else
can run.

### Harness traps

Each is a rule with its consequence.

- A build started inside a backgrounded call dies with it.
- A pattern kill matching a monitoring loop's own search term kills the monitor.
- An exact-name kill cannot match a name past the process-name length limit, so a restart that used
  one restarts nothing and the stale program keeps answering.
- The harness kills the emulator without a shutdown, so a root script must synchronise after
  writing to the disk or the filesystem leaves the rewritten files zero-length on the next boot.
- A pipeline that reads a supervised service's output never returns, because the supervisor keeps
  the pipe open.
- A static screen produces no frame events, so anything about dropped frames needs something
  animating first.
- `--audio` gives the guest a real backend where the container has one, and `-audiodev none` only
  where it does not — the same `testing/qemu-audio.sh` probe every `make run*` uses. A null sink
  consumes samples on a timer and a host one consumes them as a device does, so a guest whose pacing
  follows its own playback position runs to a different clock on the two. The rig image carries
  neither PipeWire nor PulseAudio, so it gets the null sink; the accelerated image
  (`testing/qemu-hw`) carries both, and reproducing what `make run-hw` does means running there.
  That container needs no Python of its own: start its emulator with the serial and monitor chardevs
  on unix sockets in a bind-mounted directory, `chmod 666` them from inside once QEMU has made them
  — the container is root and the driver is not — and drive it from the host by importing
  `vnc-shot.py`'s own `Serial` and `Monitor`. `Super+Return` opens a terminal on the desktop, so
  `Monitor.type()` is enough to start a program and no pointer is needed.
- A step costs seconds, so anything with a timeout must be photographed with no sleep before it.
  Typing is one character at a time and a `--shot` is a full framebuffer over VNC: `date` either
  side of four shots measured sixty-eight seconds. A five-second toast, a pulse, a tooltip's own
  delay — a `--sleep` before the shot photographs the desktop the thing has already left, and the
  picture looks exactly like the feature being broken.
- A golden may not depend on what the host happens to have. Two surfaces would: `kdos-disks` draws
  what `kdos-mountd` published, and `kdos-print` runs `lpstat` and `lpinfo`. Each is given a fixed
  input instead — the disks window a socket path that is not there, so it draws the refusal every
  machine without the daemon shows, and the printers window `--fixture` over recorded
  `lpstat`/`lpinfo` answers under `testing/fixtures/print`. A machine with CUPS set up and one
  without draw different frames and neither is wrong, which is what makes the recording the only
  honest reference.
- A surface that needs a system bus can be goldened once the test starts the bus. `kdos-net` is the
  worked example, and the missing coverage hid a defect that made the surface useless: an
  AccessPoint is exported under `/org/freedesktop/NetworkManager/AccessPoint/<n>` and a device under
  `.../Devices/<n>`, so the path-prefix test that associated them matched nothing and every network
  was dropped from a list that still drew its radios. `testing/fixtures/net/nmobjstub.c` serves the
  one `GetManagedObjects` the surface makes, on a private system bus the block starts and kills. It
  is an sd-bus *filter* rather than an object vtable: sd-bus owns `org.freedesktop.DBus.ObjectManager`
  and refuses a manual vtable for it with `EINVAL`. The recording gives the second radio a network
  the first cannot see, so a guess at the association fails the golden instead of passing by luck.
- A nested variant tree needs a real server, for the same reason. `kdos-traymenu` reads a
  `com.canonical.dbusmenu` layout, whose signature is `(ia{sv}av)` — recursive, with a variant per
  child. What goes wrong there is not the drawing: a reader that miscounts a container leaves
  sd-bus's cursor somewhere it cannot name, every row after the mistake is nonsense, and the frame
  still draws. So `testing/fixtures/traymenu/menustub.c` builds the tree with a real sd-bus on a
  private session bus, the real reader reads it, and the golden is what the two agree on. One tree
  carries everything the parser can get wrong — a mnemonic underscore to strip, a separator, a
  disabled row, a submenu with three children, two toggle states, and a row marked `visible: false`
  that must not appear. The surface's own `--open ID` and `--pick ID` are what let a dump reach the
  submenu and send an `Event` without a keyboard; the stub prints the id it was given, which is how
  a pick is asserted with no display.
- A D-Bus contract needs its own end of the wire, not a mock of ours. `kdos-netagent` answers
  NetworkManager, and none of what it must get right is photographable: the flag that has to be set
  before anybody is asked, the exact `a{sa{sv}}` a secret comes back in, and error names that carry
  no `.Error.` in them. `testing/fixtures/netagent/nmstub.c` is the other end — a bus name, an
  AgentManager and one `GetSecrets` built from the argument order libnm sends — on a private system
  bus started for the case, because an agent registered against the host's own NetworkManager would
  be asked for the passphrases of the machine running the tests. `agentcheck.c` links the real
  `netagent.c` against a scripted display, so the keystrokes are the test's and everything else is
  the shipped code.
- A parser gets a fixture *and* the shipped file. `catalogue --selftest` runs `cat_selftest()` over
  `testing/fixtures/catalogue/catalogue`, which is small enough to reason about and carries every
  row type including the awkward ones: a base with its own image, a two-deep runtime chain, a row
  whose package list is `-`, a row with no `meta`, and a member in two groups. The fixture pins the
  rules — a chain is base-first, a `-` package list presents as empty, an expand dedupes. It cannot
  pin the other half: the shipped `src/packages/kdos-appbox/catalogue` is what a surface will
  actually read, and a row added by hand that the parser rejects is a store that opens empty with no
  error on the screen. Both runs are in `selftest.sh`, and `KDOS_CATALOGUE` is the override that
  exists for them.
- A shared helper a front end calls belongs in the harness's base source list, not in the candidate
  loop. `mountd.c` is the one `kdos-mountd` client `kdos-devices` and `kdos-disks` both use, and
  leaving it out reports "the new front ends do not link" — which reads as a defect in those files
  and silently skips every surface golden.
- A `--root-cmd` or `--root-script` round trip outlives a five-second toast. It runs over the serial
  console and waits for a prompt, which takes longer than the notification it raised stays on
  screen, so the shot that follows photographs an empty desktop and the notification path reads as
  broken. Raise a toast with `--type`, which goes over the keyboard and is quick, or ask for a
  timeout longer than the rest of the run.

## Measuring frame rate

Three different numbers are called "fps", and a measurement that does not say which one it took
answers a question nobody asked.

| Number | Counts | Decided by |
|---|---|---|
| Render rate | Frames the application finished drawing | The driver and the GPU, and whether the swap waits for a refresh |
| Compose rate | Frames `kdos-comp` built out of its clients | The compositor, which paces off the output's own frame events |
| Present rate | Frames that turned into light | The mode, and the presentation path under it |

Under `make run-hw` the virtio-gpu virtual display is 60 Hz. A number above 60 there is a render
rate: those frames were drawn and thrown away, and nothing measured in the emulator can show more
than sixty frames a second reaching a screen. Raising a software cap raises the first number and
cannot raise the third. What a cap costs on a 144 Hz panel is a claim about real hardware and has to
be measured on it.

The present rate is the one the machine reports on its own. The compositor writes a line to
[`kdos-frames.sock`](../06-reference/filesystem-and-ipc.md) for every frame that missed, with the
output's refresh interval, the lateness, its own render cost, and whether the gap was measured from
a presentation event or from the frame clock. `kdos stutter` is the front end. A static screen
produces no frame events, so something has to be animating before that socket says anything at all.

### The overlay, which is already on every machine

mesa here is built `-D gallium-extra-hud=true`, so a Gallium driver draws its own overlay over any
GL or GLES client with no extra software installed and no change to the program:

```sh
GALLIUM_HUD=fps es2gears_wayland
GALLIUM_HUD=fps+frametime glmark2-es2-wayland     # both curves in one pane
GALLIUM_HUD=simple,fps es2gears_wayland           # text, no graph
GALLIUM_HUD=csv+fps+frametime vkgears             # values to stdout, for a script
GALLIUM_HUD=help es2gears_wayland                 # every name THIS driver can draw
```

The syntax comes from the driver: `+` shares a pane, `,` opens a pane below, `;` opens a column, and
`.w`/`.h`/`.x`/`.y` size and place one. `GALLIUM_HUD_PERIOD` is the update interval in seconds and
`0` means every frame.

It is a GL instrument. The frame sources are in every Gallium build; `gallium-extra-hud` adds the
disk, network and CPU-frequency ones beside them. A Vulkan program draws no HUD — `vkgears` prints
its own rate instead, and `vkcube --c <n>` runs a fixed number of frames so an external clock can do
the arithmetic.

### Taking the cap off

A GL client that waits for a refresh is measuring the display, not the machine. `vblank_mode` is a
driconf option, and an environment variable of the same name overrides both the default and any
`drirc`, so it needs no cooperation from the program:

```sh
vblank_mode=0 es2gears_wayland          # never synchronise, ignore the application's choice
MESA_VK_WSI_PRESENT_MODE=immediate vkgears
```

`glmark2` asks for swap interval 0 itself unless `--swap-mode fifo` is given, so its score is a
render rate by construction and is comparable between machines rather than between panels.

### Which tool answers which question

| Question | Tool |
|---|---|
| How fast can this machine draw a trivial scene | `es2gears_wayland`, printing `N frames in X seconds` every five seconds |
| How fast can it draw real ones, as one comparable score | `glmark2-es2-wayland`, or `glmark2-wayland` for desktop GL |
| The same with no compositor in the way | `glmark2-es2-drm` / `glmark2-drm` from a text console while no compositor holds the display, which they then drive themselves; `glmark2-es2-gbm` / `glmark2-gbm` render offscreen and need no display at all |
| The same for Vulkan | `vkgears`, or `vkcube` for a swapchain that can be told its present mode |
| Does this machine have a Vulkan driver at all, and which | `vulkaninfo --summary` — run it first under an emulator, because a Vulkan tool falls back to lavapipe on the CPU without saying so |
| Which EGL renderer, extensions and configs a client gets | `eglinfo` |
| What an X11 client sees through Xwayland | `glxinfo` and `glxgears`, which exist only in a box |

### Desktop GL on the host, and why glxgears is not

Desktop GL is reached through EGL, never GLX. `glmark2-wayland` binds `EGL_OPENGL_API` and then
dlopens the entrypoint library by a legacy name — `libGL.so` first, `libGL.so.1` second — and prints
`Error loading GL library` if neither answers. `libglvnd` is built `glx=disabled`, so it builds no
`libGL`; its recipe adds `libGL.so` as a filename alias of `libOpenGL.so.0`, which carries the same
dispatch table and every `gl*` entrypoint.

Three facts about that alias, each measurable:

- It is a filename, not a SONAME. `libOpenGL.so.0.0.0` says `SONAME libOpenGL.so.0` and nothing
  else does; `libGL.so` is a directory entry in no `DT_NEEDED` anywhere. `-lGL` therefore *links* —
  `ld` searches `libGL.so`, finds the alias, and records `DT_NEEDED libOpenGL.so.0`, which is a
  program that runs. A link that wanted `glX*` fails naming the symbol, which is the honest answer.
- `libGL.so.1` is the spelling that must not exist. libepoxy treats a `libGL.so.1` it can open as
  the GLX provider and then resolves `glXGetCurrentContext` from that handle with abort-on-missing.
  Point the `.1` name at a glvnd `libOpenGL` and the second bootstrap entrypoint a client resolves
  kills the process: `glXGetCurrentContext() not found: … undefined symbol`, `SIGABRT`, exit 134. On
  this image `Xwayland` is what links libepoxy. It reproduces on any machine with libepoxy and EGL,
  no guest needed: make a desktop-GL context current, call `glGetString` and then `glGetIntegerv`
  through epoxy's dispatch, and run it once with `LD_LIBRARY_PATH` pointing at a directory holding
  `libGL.so.1 -> libOpenGL.so.0`.
- Two programs on the image ask for the unsuffixed name, and both are served by it:
  `glmark2-wayland`, which wants exactly this, and `eglinfo`, whose bundled glad loader lists
  `libGL.so.1` then `libGL.so` but is never reached — `eglinfo` loads through
  `gladLoadGLLoader(eglGetProcAddress)`. `libgstgl` and libepoxy name only `libGL.so.1`, so they see
  no desktop GL library on this host and there is nothing for them to mis-resolve.

The alias is not a GLX provider, and there is no GLX provider on this host.

An image carries the alias only if it was packed after a `libglvnd` rebuild — the recipe change
reaches nothing on its own, and `glmark2-wayland` fails at `Error loading GL library` on any image
without it. What an image actually has is one command:

```sh
unsquashfs -ll build/iso_root/system.sfs | grep -E 'libGL|libOpenGL'
ls build/fs/usr/lib/libGL.so                 # the same question of the build tree
```

mesa here is built `-D glx=disabled -D platforms=wayland`. There is no GLX and no X11 EGL platform,
so `glxgears` and `glxinfo` cannot be linked against this host at all, and an instruction to run one
is an instruction to a different distribution. Xwayland is the single X carve-out, and the two
programs live in a box:

```sh
kdos app install app.mesa-utils
kdos-appbox -b app.mesa-utils run glxgears
kdos-appbox -b app.mesa-utils run glxinfo
```

That pack is also the host-versus-box measurement: its `es2gears_x11` runs the same test as the
host's own `es2gears_wayland`, through Xwayland and a container, and the difference between the two
numbers is what that path costs.

### Where the ceilings are

`kdos-comp` has no frame constant of its own. It paces off wlroots output frame events and
therefore follows whatever mode is set, so a number that stops at a round figure is the mode, the
driver or the client, and never a ceiling written down here.

## The other harnesses

| Harness | Does |
|---|---|
| `packlane.sh` | The application lane end to end on a booted machine: the daemon, the keyring, an install from the medium, the launchers, a box, and the telemetry — reporting pass, fail or skip with a reason |
| `install-to-disk.sh` | Runs the installer into a disk image, on its own terminal, with a heartbeat |
| `bios-boot.sh` | Boots the ISO as a USB stick under SeaBIOS, with no OVMF anywhere |
| `appsweep.sh`, `appreport.sh` | Launch every catalogue application and render the results as a table and a contact sheet |
| `bootcheck/` | Boot verification |
| `prepare_base.py`, `test_runner.py` | Build a minimal root filesystem as a container image and build individual ports against it |
| `qemu-audio.sh` | Probe for a working audio backend rather than hardcoding one, because the emulator aborts at startup on a backend its build lacks — and name the mixer's rate, because QEMU's own default is 44100 and the guest drives the codec at 48000 |
| `qemu-hw/` | The containerised emulator with accelerated graphics |
| `oomd-fire.sh` | Fires the memory daemon on a booted machine with real memory to exhaust |
| `hostcheck.sh` | Host-side checks |
| `appbox-smoke.sh`, `boxcheck.c`, `barcheck.c` | Box and bar checks |
| `usability.sh` | Drives the desktop the way a person does and leaves a numbered contact sheet; `testing/usability.md` is the checklist to read it against |

## What is not tested

Stated so nobody assumes otherwise.

The memory daemon fires under `testing/oomd-fire.sh`, which is a rig run rather than a self-test
block: it needs a booted machine with real memory to exhaust. Victim selection is exercised against
recorded state by `kdos-oomd --fixture`; the script proves the daemon wakes at all, which a fixture
cannot.

Fourteen of `kdos-shell`'s names carry no committed frame: `about`, `ascii`, `audio`, `bt`, `cal`,
`calc`, `clip`, `devices`, `ime`, `mediad`, `note`, `slit`, `time` and `users`. Some of
those have no dump at all; the rest are surfaces whose reading is the host's, which
`testing/goldens/README` names one by one — `cal` draws the current month, the apps and places
menus read the host's `/usr/share/applications` and `/proc/mounts`, `kdos-slit` renders the output
of forked commands, the mixer enumerates the machine's ALSA cards and `kdos-users` walks its
password file. A golden pinned with a hack nobody can reproduce is worse than a missing one.

A list of goldened pages must not skip the ones with no golden yet. A loop that tests for a
committed golden and skips past a page that has none makes a page *added* to the list unreachable:
no golden, therefore skipped, therefore never given one, and the suite reports a clean run over a
page nothing has ever looked at. Being named in that list is the claim that the page should have a
golden, so a missing one fails.

A golden no `golden` call drives is worse than none. Nothing compares it, so it agrees with the
surface only until the surface changes, and it reads to the next person as evidence that was
checked. Every committed frame is driven by a call.

The compositor and the shell are not compiled by the self-test on a bare host.

Nothing that runs on its own has ever hovered a button. Every defect the usability sweep exists to
catch — a taskbar two rows above the bottom of the screen, a tooltip that swallowed the click on the
button it described, a Start button whose label vanished under the pointer, an icon layer eighty
columns wide on a hundred-and-sixty column screen — is green in `preflight.sh` and green in
`selftest.sh`. `usability.sh` drives those paths and photographs them; reading the result is still a
person's job.

A golden cannot see a translucent window. `window_opacity` writes the blend as the cell's literal
colour and leaves the slot exactly as the window was drawn in, and a golden is that same slot dump —
so a frame at 70 per cent and a frame at 100 are byte-identical goldens. The pixel path is the only
place it can be looked at, which means the rig.

The pointer's own pixels are in no test. The compositor draws the cursor, and a photograph of a
moving pointer is the only place it can be looked at.

Nothing here tests the build, which takes hours and a container.

## See also

- [Developing](developing.md) — the loops these fit into
- [Writing desktop software](writing-desktop-software.md) — dumps and reference frames from the author's side
- [The C libraries](c-libraries.md) — what the assertions cover
- [Build troubleshooting](build-troubleshooting.md) — when preflight is not enough
- [Known gaps](../06-reference/known-gaps.md) — the full list of what does not exist
