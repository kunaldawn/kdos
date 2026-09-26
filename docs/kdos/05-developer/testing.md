# Testing

This page is for anyone changing KDOS who wants to know whether the change works before a full
build — which takes hours — tells them. It describes each test harness in `testing/`: what it
proves, what it cannot, how long it takes, and how to run it.

KDOS has no unit-test framework and ships no test binaries. What it has instead is:

- a **preflight** check over the wiring of the tree, in about two minutes;
- a **self-test** over the C libraries and every program that uses them, in about three minutes;
- **goldens**, committed reference frames of every surface that can be drawn deterministically;
- **fixtures**, recorded system state that a reader can be pointed at instead of the live machine;
- a **virtual-machine rig** that boots the real image, drives it and photographs it.

If you are new here, read [What each tool proves](#what-each-tool-proves) and
[Which one to run](#which-one-to-run), then the section for the harness you need. Terms such as
*surface*, *golden*, *fixture* and *rig* are defined in the [glossary](../06-reference/glossary.md).

## What each tool proves

| Tool | Proves | Cannot prove | Takes |
|---|---|---|---|
| `testing/preflight.sh` | The wiring: every reference resolves, every option exists, every script parses | That anything works | About two minutes |
| `testing/selftest.sh` | The libraries' invariants, that every consumer still compiles, and that surfaces still lay out | That the system boots | About three minutes on a bare host |
| Goldens | That a surface's geometry and colours have not drifted | That it is usable | Part of the self-test |
| Fixtures | That a reading or a decision is correct against recorded state | That the reading is correct on a live machine | Part of the self-test |
| `testing/vnc-shot.py` | That a real session does a thing, photographed | Anything the renderer in use cannot show | Minutes per boot |
| `testing/usability.sh` | That the desktop can be driven by hand — hover, click, chord — photographed step by step | Nothing on its own: it asserts nothing and a person reads the result | About six minutes |
| `testing/docscheck.sh` | That this book still links up, states the present, and keeps its page contract | Anything a reader has to judge | Under a second |
| `ports/fetch --check` | That every source a recipe names is on disk and matches its hash | That the sources build | Seconds, offline |
| `testing/packlane.sh` | The application lane end to end on a booted machine | | Minutes |
| `testing/install-to-disk.sh` | That the installer installs | | Minutes |
| `testing/bios-boot.sh` | That the image boots on a machine with no UEFI firmware at all | Anything about the UEFI path | About two minutes |

None of them proves the build works. A package manager can only really be tested by building the
distribution with it.

## Which one to run

| You changed | Run |
|---|---|
| A phase list, a recipe, a `depends` line, anything under `script/` or `fs/` | `testing/preflight.sh` |
| A library under `src/libs/`, or a program of ours | `testing/selftest.sh` |
| A parser | `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh` |
| A surface's layout or colours | The self-test in the build image (`testing/devdeps-image.sh`); if the change is intended, regenerate with the command under [Regenerating](#regenerating), then read `git diff testing/goldens/` |
| A recipe's `sha256 =` line or its sources | `make fetch-check` |
| Something only a running desktop shows | `testing/quick.sh`, then the rig on a real ISO |
| A page of this book | `bash testing/docscheck.sh` |

## preflight.sh

Everything a full build would catch, minus the build: 48 checks, run with

```sh
testing/preflight.sh
```

It builds its own copy of `kpkg` first, then runs the checks. It exits 0 when every check passes
and 1 when any fails, and prints each failure with the file to look at.

Nine checks read the built tree under `build/fs`: seven skip entirely without one, and two run a
reduced check and say what they left out. A skipped check prints `skipped — no build tree` rather than passing
quietly, so a green run accounts for every check in the file instead of hiding the ones that
declined to run.

| Group | Checks |
|---|---|
| Packages | Every package named in a phase list has a port; ports built from one tarball agree on its version; every list resolves to a dependency order with clean output and valid tokens; every `depends` entry names a port that exists; every port of ours is built by something; the build tree carries no package whose port is gone |
| Recipes | Every port has a build script and it parses; every recipe parses as metadata; every one declares a name, version and release; every recipe and build script carries the KDOS banner; every source a port declares is named by a checksum and non-empty where it is on disk — one not yet fetched is reported as a `make fetch` note, not a failure |
| Build options | Every meson option a recipe passes is one that port defines, checked against the tarball's own option file, with the two closed-value types validated; every recipe that runs cargo builds against a shared C library; every consumer of a shared library generates the Wayland protocols it includes |
| Sources | Every source file in one of *our* ports is compiled by its recipe, unless that recipe globs its own `$PORT_SRC` directory — a glob of the shared `libk*` trees does not exempt it; a first source whose members are `./`-prefixed is accounted for; a flat first source is unpacked by its own recipe |
| The sources archive | No archive a recipe hashes is tracked by git; `.gitignore` ignores every archive suffix under `ports/core` and the source cache, and none of the archive fixtures under `testing/fixtures`; `ports/srclib.sh`, `ports/fetch`, `ports/publish` and `script/hooks/pre-push` exist, are executable where they must be, and pass `bash -n`. An unset `core.hooksPath` is printed as a note, not a failure |
| Shipped configuration | The shipped compositor configuration keeps labwc's default bindings; every command it, the menus and `menu.conf`'s routes name exists; every program `fs/etc/inittab` names is on the image; every filesystem the installer offers, the initramfs can mount; the ISO step builds every boot path (BIOS and UEFI, disc and written stick); the built `kinstall` is the installer in this tree |
| Shell | All shipped and build shell is syntactically valid; a script a recipe ships inside a `KDOS_SH` heredoc parses too, and every program it names as the first word of a line is one the image carries; no build script names a command inside double quotes and runs it; every helper the Makefile runs is on disk |
| Consistency | The build tree's root carries nothing but a root filesystem; every flag one shell tool passes another is one it accepts; every daemon an init script starts is installed by a port; the rootfs carries no script whose interpreter is gone; nothing points at a removed file; every `port:`, `path:`, `see:` and `cite:` a recorded reason names still resolves; no chroot step reads the ports tree through `/kdos/ports`; the catalogue's rows match the tree; the application store is wired everywhere it has to be; a desktop toggle has one flag and only `libkbase` builds its path; a frame that opens the synchronized-output bracket closes it on every path; a literal colour is set at the render boundary and nowhere else; the control centre's row table agrees with the files it writes |
| Shipped data | `mc`'s shipped rows name programs that exist; every `kdos-*` handler in `mimeapps` is shipped; every help page names a document that ships; the generated `aerc` styleset is one `aerc` will load |
| Chrome | Every glyph in `libktui`'s UTF-8 table is one the shipped console font can draw; every icon name a surface asks for resolves on the image; every desktop entry's icon and command exist on the image |

Four of those catch a whole class of failure that never reaches a compiler.

**Every source file in one of our ports is compiled by its recipe.** A file the build script
neither names nor matches passes every gate on a development machine and then fails to *link* in
the build, because the self-test globs those directories. A whole page can be exercised by the
harness and be absent from the shipped binary.

**Every meson option is one the port defines.** meson fails at setup on an unknown option, before
a line is compiled, and there is no universal spelling. Without this check a misspelt option is an
hour-long round trip through the build to find.

**Every chrome glyph is one `ter-kdos32n` carries.** The toolkit picks its UTF-8 table whenever the
terminal reports UTF-8, which the Linux console does, and the console font holds 512 glyphs. An
entry that font lacks renders on `tty1` as a blank: not a fallback and not an error — the cell is
written, the flush succeeds, and a piece of the desktop's own chrome is missing. `▓`, the half
blocks `▀ ▄` and the double tees `╠ ╣ ╦ ╩` all look reasonable in an editor and none of them is in
the font. The check reads the shipped font itself, because a hand-kept list of what it carries is
the thing that goes stale. Goldens cannot catch this: they are dumps at the ASCII tier, where every
one of those glyphs resolves to something.

**Every flag one shell tool passes another is one it accepts.** These tools start each other by
name, and an unknown argument prints a usage line to an error stream nobody reads and exits before
a surface exists. The result is a control that silently does nothing, invisible to a compiler and
to a reference frame. The check is deliberately crude: it finds the argument literals, takes every
long flag, and requires it to appear in the target's own source.

Preflight cannot prove the build works. It can prove the build will not fail for one of the dull
reasons. Run it after touching a phase list, a recipe, or anything under the build scripts.

## selftest.sh

The host-side regression suite for `src/libs/` and everything built on it.

```sh
testing/selftest.sh
```

It:

1. compiles every library with the host compiler with warnings as errors;
2. runs the shared assertion program, `src/libs/selftest.c`, against them;
3. compiles the consumers, to prove the headers still agree with their callers;
4. resolves a port, to prove the ports tree still parses;
5. audits a generated theme;
6. runs `kdos-portup --selftest` and a fixture-backed version check, offline;
7. renders every surface offscreen and compares it against its golden;
8. runs the build orchestrator end to end against a synthetic tree (`kdosbuild --selftest`).

It needs no container and no network, and takes about three minutes on a bare development machine,
which skips the blocks it lacks libraries for (see [What it does not cover](#what-it-does-not-cover)).

Its assertions are the libraries' invariants. Each is a claim that was measured once; the suite is
what notices when it stops being true. One is counter-intuitive: the two colour-mixing functions
are asserted to *disagree*, because each generated file was written against exactly one of them.

A test whose only subject is code nothing ships is not coverage. It is a second implementation with
an audience of one, and it reports green while the behaviour a person sees goes untested. When the
last shipped caller of something goes, its assertions go with it.

### The window-model contract

`libkwm`'s block asserts nothing of its own. It replays `testing/fixtures/wm/geometry.txt`, whose
106 rows each cite the line of `kdos-comp` the expected value was read from. A failure means the
library and the compositor disagree, which is exactly what keeping the window model outside the
compositor exists to catch.

Add a case by adding a row and citing its line, never by writing an assertion in `selftest.c`. A
row whose expected value came from taste rather than from the source is worse than no row: it makes
the library the authority over the behaviour it is meant to reproduce.

The `geom` rows are pure arithmetic over the formula in `view_get_edge_snap_box`, so they can be
re-derived mechanically. That gives two independent checks: re-deriving catches a row computed
wrong by hand, and replaying the file against the library catches a row transcribed with a flag
inverted. When the two disagree, correct the fixture — the compositor's source is the authority.

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite is clean this way and must stay clean. AddressSanitizer and UndefinedBehaviorSanitizer
catch what a plain run cannot — for example a size field in an archive header overflowing a signed
type and becoming an enormous unsigned length, a copy called with a null source, or a read past the
end of a buffer that was last resized smaller than the loop assumes.

A variadic `printf`-style wrapper guards its own format string, in `libkbase` and in the two
programs that have one of their own. A null format is undefined in `vfprintf` anyway, and the
sanitizer build's whole-program analysis cannot prove one non-null — so without the guard
`-Wformat-overflow` refuses to build the suite, and the sanitized run becomes a run nobody can do.

Leak checking is off by default (`ASAN_OPTIONS=detect_leaks=0`) and on for the library assertions
and the image fuzzer. Every program here owns its parsed state until it exits, which a leak checker
reports as a leak; the library suite is the one binary whose subject is code called repeatedly.

The image parsers have a fuzzer, `testing/fixtures/img/fuzz.c`. It is its own driver rather than a
block in `selftest.c` so that its corpus can run under both sanitizers without rebuilding the rest
of the suite. It makes two passes: every fixture decoded under a budget, then every fixture mutated
a byte at a time and truncated at every length, because a hand-written corpus only exercises the
paths its author thought of. A decoder must answer NULL or an image for any input, and must never
read past its end.

### What it does not cover

Several blocks need libraries a bare development machine does not have, and are skipped by name
when they are missing:

- `kdos-comp`'s own source files (`src/desktop/kdos-comp/src/kdos-*.c`) need `wlroots-0.20`, `glesv2`, `egl`, `libxml2`, `cairo` and `pango`;
- `libkwl`, `kdos-lock` and `kdos-shell` need `fcft`, `pixman`, `xkbcommon` and `wayland-client`;
- `kdos-shell` additionally needs sd-bus, ALSA, `libpipewire-0.3` and the `ext-workspace-v1`
  protocol;
- `libkimg`'s decoders need `libpng`, `libjpeg`, `libwebp` and `libsixel`.

A bare host still reaches every section and exits clean; what it loses is inside them, and each
block it cannot build says so. [The machine where nothing is skipped](#the-machine-where-nothing-is-skipped)
is the build image that runs them all.

A skip that can never be lifted is a test nobody runs. Every conditional block keeps one rule: a
missing tool or library is a skip with a name, decided before the block starts, so its absence
cannot take the rest of the suite with it.

The shell compiles here with format-truncation warnings disabled and nothing else relaxed. That
program truncates on purpose: every label goes into a fixed number of cells, so a cut label is the
intended behaviour. Where truncating *is* a defect — a socket path, a device node — the code holds
it with an explicit bound rather than relying on a warning that cannot tell the two apart.

### The cached host helpers

The scripts under `ports/` compile helpers on first use and cache them: `ports/.kpkgbin` (the
recipe reader), `ports/.portup` and `ports/.portup-tools` (the version checker). A binary built
against one C library cannot run under the other, so a host run straight after a container run
must delete them first.

The failure does not say so. The recipe reader fails, the version checker cannot render a
candidate, and the suite reports that it reproduced no outcome — which reads as a regression in the
version checker.

A container run as root also leaves those files owned by root, so deleting them as yourself fails
and the stale binary survives. Delete them from a container rather than with `sudo`.

## Goldens

A **golden** is a committed reference frame: a surface rendered offscreen and compared byte for
byte. There are 193 of them under `testing/goldens/`, at twelve sizes, covering the shell's
surfaces, all eleven resource-monitor pages plus its detail page, the terminal, the cell-level
frames, and nine replayed terminal recordings. `testing/goldens/README` states the rules for adding
one.

| Kind | Catches | Count |
|---|---|---|
| Text frames (`--dump`) | Geometry: overflow, misalignment, a control drawn past its rectangle | 175 |
| Cell frames (`--dump-cells`, `cells-*.txt`) | Colour and attribute drift as well | 9 |
| Replayed streams (`vt-*.txt`) | A change in the terminal's state machine, against bytes real programs wrote — the characters, the attributes, the cells that named a colour of their own, the hyperlinks and the prompt marks | 9 |

A text frame is the character in each cell, one line per row. A cell frame is one line per
non-blank cell — `row col U+XXXX fg bg attr` — so a selection that lost its accent fill, or a label
that dropped to an unreadable colour slot, changes it even when the text frame is identical.

A terminal's frame is taken by running a command to completion. `kdos-term --dump` waits for the
child and consumes everything it wrote before drawing, because a frame taken while a program is
still writing differs every time. Two of the four terminal frames hold a picture: a dump has no
pixels, so a picture renders as its fallback character in its top-left cell and blanks under the
rest — exactly what a text console shows. What the frame asserts is the shape: how many rows the
picture took, and where the cursor was left.

Commit at least two sizes for anything with a layout, because a geometry defect usually shows at
one width only. The monitor's pages carry three, including a narrow one that forces its sidebar to
degrade.

### How the dump harness is built

The shell's goldens come from one test program, `dumpcheck`, which the self-test builds in the
block headed `the shell's front ends draw offscreen, and the boxes line up` in
`testing/selftest.sh`. It is linked from three parts:

1. `testing/fixtures/shell/dumpmain.c`, the test driver, which also stubs everything that would
   need a display: `libkwl`, `libkdisp`, the icon layer, the plate API and the privacy indicators;
2. the **base source list** (`DFRONTS` in the script): the files every surface needs, always
   linked — `shell.c`, `cal.c`, `menu.c`, `osd.c`, and shared helpers such as `fav.c`,
   `mountd.c`, `routes.c` and `libkchrome`'s drawing code;
3. the **candidates**: every other surface's source file, each checked on its own with a
   compile-only pass. A candidate that does not compile is skipped by name and costs only its own
   golden.

The candidates that pass are then linked together with the base list in one command. That single
link is why a link error in any one of them skips every candidate's golden, as the rules below
describe.

### Rules the harness keeps

**A cell frame's verdict is acted on separately.** A comparison whose answer nothing reads cannot
fail, so the cell half has its own check and its own message — a colour drift and a geometry drift
are fixed by looking at different things.

**The icon layer is stubbed to "no picture".** A golden is the character grid, so a layout that
only lines up once the pictures load is a broken layout.

**Container photographs are spaced out.** `libkcell` takes its cell width from the font's widest
advance, which for a font with CJK coverage — the fontconfig default in a build container — is
about twice the Latin advance. The glyphs are then drawn at the left of cells twice their width.
The shipped image's console font has one advance for everything, so this is the container's font,
not the painter. Judge a picture on the image, or pass `--font`.

**A golden that differs from `HEAD` may be uncommitted work, not drift.** Goldens are regenerated
as the tree changes and are often modified in the working tree ahead of the source change that goes
with them. `git checkout --` on one throws away what it records, and the next run reports a
difference that looks like a flake. `git diff HEAD` on the *source* tells the two apart; if it does
not, run the dump twice.

**A surface that needs a library the harness lacks is skipped by name.** `kdos-peek` decodes with
`libkimg` and probes archives with `libarchive`, so both the shell-wide compile and the dump
harness admit it only when `libarchive`, `libpng`, `libjpeg` and `libwebp` are all present, and
say so when they are not. Gating the *whole* shell compile on them would lose the other forty-odd
files on a machine missing one development package.

**`libkdisp` is stubbed, not linked, so adding a `kdisp_` entry point needs a stub.** The stubs are
in `testing/fixtures/shell/dumpmain.c`, and no compiler check ties them to the header. A new entry
point without a stub fails as `undefined reference` and skips every surface frame behind it.
Linking the real `libkdisp` beside the stubs is not the fix: that is some thirty *multiple
definition* errors. Add the stub, and make it answer the state a dump really has —
`kdisp_win_supported()` is 0, because a dump renders one frame with no compositor, and a stub that
invented two windows would make the frames assert a fiction.

**One missing stub costs every surface golden, not only its own.** The link is one command over
the base list and every admitted candidate, so `undefined reference to kch_px_bare` in `notifyd.c` skips the panel, the desktop,
the calendar and the forty others with it — and the suite prints one NOTE and passes. A green run
with a name missing from `ls testing/goldens/` is this failure.

**The opposite mistake is a *multiple definition*.** A stub for a symbol one of the linked files
defines collides, and the message contains neither `undefined` nor `error`, so a filter looking for
those words reports half the breakage. `osd.c` defines `sh_volume_get` and `sh_mic_muted`, which
`panel.c` calls, so it is in the base list beside `cal.c` and `shell.c` rather than among the
candidates. A file that is sometimes linked and sometimes stubbed collides on exactly the
machines where the harness otherwise works.

**A shared helper belongs in the base source list.** `mountd.c` is the one `kdos-mountd` client
both `kdos-devices` and `kdos-disks` use; leaving it out of the base list makes those surfaces fail
to link, which reads as a defect in them and skips every surface golden.

**A dump's font list comes from `$KDOS_FONT_LIST`.** On a display, the list of monospaced families
is fontconfig's — one name on the image, hundreds on a developer's machine — so `dumpmain.c` reads
its rows from that file, one family per line. The suite writes the file into its output directory
rather than committing it under `testing/fixtures/shell`, where a fixture added for one surface
would move another's frames. Two goldens depend on it: `theme-font`, over names with a space,
escaped punctuation and a size key, proving the name column is cut and the sample is not; and
`theme-font-none`, taken with the variable unset, which is the page's empty state.

**A surface that exits non-zero costs its own golden, not the run.** `set -e` is on, so without
care one surface refusing a flag would end the suite — every later check, including the ones that
say whether the goldens are committed at all, never running. `golden()` and `cells_golden()`
catch the status and record it, and the candidate compile loop admits each file on its own for the
same reason.

**Every frame carries the row that names its keys.** The suite checks this against the committed
goldens, because a blank bottom row is a surface whose keys nobody can find. The exemptions are
named, each with its reason: the taskbar, the tooltip, the savers and the two menus are drawn on
the desktop rather than in a window, a saver closes on any key and a tooltip answers none, so a row
naming `Esc` on either would teach a key that does nothing. `menu.c` is the one file that holds a
`KtuiKeys` and draws no row, for the reason its own header gives.

**A golden holding a sixel picture is guarded on the sixel decoder.** pixman alone builds a
`libkimg` with no sixel support, and running the picture test against it produces a diff that
blames the terminal.

**A golden may not depend on what the host has.** `kdos-disks` draws what `kdos-mountd`
publishes, and `kdos-print` runs `lpstat` and `lpinfo`. Each gets a fixed input instead: the disks
window a socket path that does not exist, so it draws the refusal every machine without the daemon
shows, and the printers window `--fixture` over recorded answers under `testing/fixtures/print`.

**A surface that talks D-Bus gets a real peer on a private bus.** A mock of our own side passes on
exactly the mistakes a real conversation exposes:

- `kdos-net`: `testing/fixtures/net/nmobjstub.c` serves the one `GetManagedObjects` call the
  surface makes, on a private system bus the block starts and stops. It is an sd-bus *filter*
  rather than an object vtable, because sd-bus owns `org.freedesktop.DBus.ObjectManager` and
  refuses a manual vtable for it with `EINVAL`. An access point lives under
  `/org/freedesktop/NetworkManager/AccessPoint/<n>` and a device under `.../Devices/<n>`, so they
  must be associated by the device's own list, not by path prefix; the recording gives the second
  radio a network the first cannot see, so a guessed association fails the golden.
- `kdos-traymenu`: `testing/fixtures/traymenu/menustub.c` builds a `com.canonical.dbusmenu` layout
  — signature `(ia{sv}av)`, recursive, with a variant per child — with a real sd-bus on a private
  session bus. A reader that miscounts a container leaves the cursor somewhere it cannot name and
  every later row is nonsense while the frame still draws. The tree carries everything the parser
  can get wrong: a mnemonic underscore to strip, a separator, a disabled row, a submenu with three
  children, two toggle states, and a row marked `visible: false` that must not appear. The
  surface's `--open ID` and `--pick ID` let a dump reach the submenu and send an `Event` with no
  keyboard; the stub prints the id it received, which is how a pick is asserted.
- `kdos-netagent`: `testing/fixtures/netagent/nmstub.c` is NetworkManager's end — a bus name, an
  `AgentManager` and one `GetSecrets` built from the argument order libnm sends — on a private
  system bus, because an agent registered with the host's own NetworkManager would be asked for
  the passphrases of the machine running the tests. `agentcheck.c` links the real `netagent.c`
  against a scripted display, so the keystrokes are the test's and everything else is shipped
  code. What it checks is not photographable: the flag set before anybody is asked, the exact
  `a{sa{sv}}` a secret is returned in, and error names that carry no `.Error.`.

**A parser gets a fixture *and* the shipped file.** `kdos-appbox catalogue --selftest` runs over
`testing/fixtures/catalogue/catalogue`, which is small enough to reason about and carries every row
type including the awkward ones: a base with its own image, a two-deep runtime chain, a row whose
package list is `-`, a row with no `meta`, and a member in two groups. It pins the rules — a chain
is base-first, a `-` package list is empty, an expansion removes duplicates. The shipped
`src/packages/kdos-appbox/catalogue` is also parsed, because a hand-added row the parser rejects
is a store that opens empty with no error on screen. `KDOS_CATALOGUE` is the variable that points
the parser at either file.

### Regenerating

Regenerate goldens by setting `KDOS_GOLDEN_UPDATE=1` on the self-test, where the Wayland
dependencies exist — the build image, not a bare host:

```sh
docker run --rm -v "$PWD:/kdos" -w /kdos \
    kdos-devdeps:latest sh -c 'fc-cache -f; KDOS_GOLDEN_UPDATE=1 testing/selftest.sh'
```

Then read `git diff testing/goldens/` before committing. That diff is the review: it is a picture
of what the change did to the screen.

The terminal's frames are the exception: `kdos-term` builds console-only on any host.

The image needs a font and GNU `tar`, and neither absence looks like itself. `fcft` resolves
`monospace` through fontconfig, so with no font installed it answers `failed to match font`, which
reads as a `libkcell` failure and stops the run at the first rasteriser block. With busybox's
`tar`, the reproducible-build block answers "the synthetic port did not build", and every surface
golden is gated behind that block, so all of them are skipped without a word. The `os-dev` build
image has the packaging toolchain and no `wayland-client`, so it skips the surface dumps entirely.

## Fixtures

A **fixture** is recorded system state that a program can be pointed at instead of the live
machine. It is what makes a monitor, an attribution engine or a kill-selection policy testable at
all. The seam is the same everywhere: the process and system filesystems sit behind a movable root,
or a variable moves one directory walk. There are 38 fixture directories under
`testing/fixtures/`.

| Fixture | Records | Makes testable |
|---|---|---|
| `res` | A process and system tree | Every monitor page, deterministically |
| `res/*/sys/class/net/*/device` | A `uevent` file inside the directory | Whether an interface is real. The test is the directory's presence, and git stores no empty directory, so the file keeps the directory in every clone |
| `stutter` | Two snapshots half a second apart, plus the frame events between them | That the application is named with its box, the blocked process comes first, pressure is quoted, and exactly one event is blamed on the compositor |
| `energy` | Four recorded power and process trees | The nesting rule, the counter wrap, the roll-up onto one application, and the short-lived residue |
| `oomd` | Trees arranged so only the memory budget produces the right answer | That the budget check matters — its host process is larger than anything in either box |
| `mountd` | A block-device tree plus two hand-built superblocks | The acceptance and both refusals. The internal disk carries a real superblock, so a broken check shows up as an extra row rather than as nothing |
| `privacy` | Three processes: one holding a camera twice, one an audio device that must be ignored | The camera indicator, on a machine with no camera |
| `portup` | Recorded upstream responses — a registry index, git tag lists and branch heads, feeds, a releases API answer, directory listings — and, under `ports/`, the recipes seven ports were recorded at | Every discovery adapter and filter in `kdos-portup --selftest`, and all three outcomes end to end, offline and unaffected by bumps to the live recipes |
| `cve` | Four ports and a five-row database | A version behind two fixes, one that only looks behind because of a packaging revision, a name mapping, and a package the database has never heard of |
| `update` | Recorded answers from `kdos update check --json`, `kdos cve --json` and `kdos-bootctl status` | `kdos-update`'s frames, through `KDOS_UPDATE_JSON`, `KDOS_CVE_JSON` and `KDOS_SLOT_TEXT` |
| `clone` | Hand-built image headers | The two-record length rule |
| `tray` | A second *process* that behaves like a real tray item | The whole protocol conversation |
| `traymenu`, `net`, `netagent` | D-Bus peers on a private bus | See [Rules the harness keeps](#rules-the-harness-keeps) |
| `shell` | The dump harness, its stubs, and a frozen home and configuration | Every surface's layout |
| `shell/rec` | Two PCM lines — one playback-only, one with a capture stream — and 25,600 bytes of raw signed 16-bit audio: four ticks at half full scale, then four of zeros | That the input list is *filtered* rather than merely listed, and that the level is computed from the samples. The recorder's `--meter` prints one line per tick and `--write` produces a WAV compared byte for byte against `shell/Recordings/2026-01-01-000000.wav` |
| `print`, `devices`, `display`, `firewall`, `backup` | Recorded command output | The printers, devices, display, firewall and backup surfaces, without the host's own state |
| `vt` | What `vim`, `htop`, `mc`, `less` and `tmux` wrote to an 80×24 terminal, plus four hand-written streams | That the terminal's state machine still produces the same screen |
| `img` | Images, and `fuzz.c` beside them | `libkimg`: every fixture decoded, then mutated and truncated |
| `catalogue` | A small catalogue with every row type | The catalogue parser |
| `wm` | `geometry.txt`, 106 rows cited to the compositor | See [The window-model contract](#the-window-model-contract) |
| `polkit` | A rule harness and a stub | What the shipped polkit rules grant, and what they must not |
| `term` | A display stub | `kdos-term`'s Wayland path |
| `pack`, `box`, `deco`, `openwith`, `places`, `recent`, `tone`, `cellclip`, `ascii`, `iconpng`, `oblique`, `view` | Inputs or a small driver of their own | Their respective units |

A recorded stream is not a running program, so beside the `vt` fixtures the suite also opens real
programs on a real pseudo-terminal and presses one key in each: `less`, `nvim`, `htop`, `top`,
`mc`, `lf`, `tmux`, `nano` and `taskwarrior-tui`. The names are the ones this system installs — a
row naming `vim` would exit 127 on every KDOS machine and be skipped, a green tick for a test that
never ran. A program the host lacks is skipped by that same 127, so the block asserts where the
programs are and stays quiet where they are not.

To know a fixture tests something, disable the check it guards and watch it fail. Both checks the
`mountd` fixture guards fail that way.

The tray fixture is a second process on purpose. The protocol is a conversation between two peers
on a message bus, and a mock of either side passes on the mistakes only a real peer exposes.

### The terminal fixtures

The terminal fixtures are never re-recorded. A re-recording picks up a different program version,
a different terminfo and a different hostname, so a fixture that regenerated itself would be a
test that changed its own question. The hostname and clock inside them are part of the recording.

Each stops on a live frame rather than on the program's exit. A stream ending with the alternate
screen being restored renders an empty grid, and so does a parser that gave up on the first byte,
so the self-test refuses an empty grid outright.

Four of the nine are hand-written, because no recording contains what they pin:

| File | Contains |
|---|---|
| `malformed.esc` | An unterminated CSI, a parameter past any bound, an OSC with no terminator, a truncated UTF-8 lead byte, a surrogate, a DCS carrying rubbish |
| `attrs.esc` | The style SGRs and their offs, the underline shapes and colour, and the four spellings of a colour |
| `links.esc` | `OSC 8` around a run, the empty form that closes it, the same address twice, a scheme the whitelist refuses, and a `params` field that must be ignored |
| `prompts.esc` | `OSC 133` marking two finished commands and a third still running |

`vtrender.c` replays a stream in small uneven chunks, because a pseudo-terminal splits escape
sequences across reads, and a parser that only handles a whole sequence passes a single-write test
and corrupts a real terminal. Its output is five blocks:

1. the characters;
2. the attributes, one letter per cell — a style has no character to show, so a golden holding
   only text could not tell an italic comment from an upright one; an underline's shape is its own
   digit;
3. which cells carry a colour of their own;
4. which cells are a hyperlink, as the link's id, so one run reads as one link and the same address
   twice reads as the same digit;
5. one character per *row* for the prompt marks.

No colour value is ever in a golden. A value moves with the theme, and a palette change reading as
"vim drifted" would be a test that changed its own question. The third block holds the *decision*
instead — the sixteen named colours reduce to slots and follow `kdos theme`, and everything above
them is a literal the program chose — so it drifts only when that rule does.

The same care applies to the terminal's own input. `ktui_input_next` reads descriptor 0, so the
bracketed-paste block puts a pipe there and writes the sequence in pieces: a terminator split
across two reads is the case that turns a paste into one that never ends when the tail is taken for
text. The block restores descriptor 0 before it returns, or every later block that reads a
terminal would read the pipe.

## The machine where nothing is skipped

`selftest.sh` runs everywhere and skips what it cannot build, saying so each time. On a bare host
it skips most of the interesting half: `libkimg`'s decoders, the sd-bus blocks, `fcft`, the Wayland
consumers. The build image is the machine where none of those skips appear, so it is the only run
that exercises every surface dump and the goldens behind them.

```sh
testing/devdeps-image.sh                    # builds kdos-devdeps, then runs the suite in it
testing/devdeps-image.sh bash               # or a shell in it
```

`testing/Dockerfile.devdeps` carries exactly what the script probes for. It is Alpine, because the
target is musl and a feature-test difference is better met here than in a phase build. Two guards
it does not satisfy: `wlroots`, which no distribution packages and which the next section builds,
and `busctl`, which Alpine ships in no package — so the portal block stays skipped there although
its other halves are present.

Two of its packages are there for what they unblock: `font-dejavu` with `fontconfig`, and GNU
`tar`. Without them the first rasteriser block and the reproducible-build block fail, each failure
gates everything after it, and between them they hide two thirds of the suite while reporting only
their own one-line error.

## Compiling the compositor without a full build

`kdos-comp` needs `wlroots-0.20`, which no distribution packages, so the self-test reports its
block as skipped. It is still buildable without a full build, because the tree's own recipe names
the source: `ports/core/wlroots/wlroots-0.20.2.tar.gz`, which `make fetch` puts in place.

```sh
FROM kdos-devdeps:latest
RUN apk add --no-cache meson ninja pango-dev libdrm-dev libinput-dev libseat-dev \
      mesa-dev libxkbcommon-dev wayland-dev wayland-protocols hwdata-dev \
      libdisplay-info-dev libxcb-dev xcb-util-wm-dev xcb-util-renderutil-dev \
      xcb-util-image-dev libx11-dev xcb-util-dev xwayland xwayland-dev
# then meson setup / compile / install the wlroots tarball with -Dxwayland=enabled
```

`kdos-comp` then configures, compiles and links in that image. Two concessions matter when reading
a failure there:

- `-Dicon=disabled`. `libsfdo` is packaged nowhere available, and the `icon` feature requires it.
  Code guarded by `HAVE_LIBSFDO` is not compiled, so a change in it is not covered.
- `-D_GNU_SOURCE`. `kdos-thumb.c` calls `fileno`, and musl's feature-test defaults differ from what
  the real phase build gets.

Build the unmodified tree first. With a baseline binary in hand, every error after a change belongs
to the change. This proves the compositor is type-correct and links; it does not prove a window
lands where a person expects, which is still the rig's job.

## Running a shipped program without booting

`build/fs` is a complete musl root, so a program already installed there can be run directly — no
ISO, no emulator, seconds rather than minutes:

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

**Everything it writes stays in `build/fs`, and the ISO is built from `build/fs`.** A bind mount
creates its own mountpoint, and a program run under the chroot writes to `/root`, `/tmp` and
wherever else it likes. None of that is owned by a package or by `fs/`, so neither the orphan sweep
nor the `fs/` manifest removes it, and it ships. Bind inputs read-only under `/tmp`, clean up
afterwards, and run `testing/preflight.sh`: it refuses a `build/fs` whose top level is not a root
filesystem.

## The QEMU rig

The rig boots a real KDOS image in QEMU, drives it with keys, pointer and commands, and
photographs the screen. Use it for what nothing offline can show: the phosphor pass (the
compositor's CRT-imitating shader, which runs only under the GLES2 renderer; see
[kdos-comp](../04-programs/kdos-comp.md#the-phosphor-pass)), a real mode set, window management,
a boxed application.

Prefer a surface's own `--dump` over a photograph whenever it answers the question. A dump hands
out the surface's exact composited grid, so a check on it is a text diff — no boot, no framebuffer,
no tolerance for antialiasing. But a dump proves a character, never a colour: `--dump` writes the
codepoint in each cell and discards the colours, so text drawn in the background's own colour —
present, and invisible on every screen — dumps identically to text a person can read. Use
`--dump-cells` when the colour matters.

`testing/vnc-shot.py` is the driver. It boots headless with the serial console and the QEMU
monitor on sockets, types through the monitor, and reads the framebuffer over the
remote-framebuffer (VNC) protocol. It boots `build/iso-build/kdos.iso`.

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

`make build` with packaging takes about seven and a half minutes, five and a half of them writing
the ISO. Repacking the whole medium to carry a 200 KB binary makes every look-at-it cycle twelve
minutes. `testing/quick.sh` avoids that:

```sh
testing/quick.sh kdos-comp,kdos-shell -- --keys meta_l-ret --sleep 3 \
                                        --shot /kdos/build/shots/x.png
```

It builds the named ports into `build/fs` with no packaging (about 1m09s), tars exactly the files
those ports own, and hands them to a booted ISO on a raw disk, where `testing/quickpatch.sh`
unpacks them over the live medium's in-memory overlay and restarts the session. Everything after
`--` is passed to `vnc-shot.py`. A run takes about three minutes, or 1m37s with
`KDOS_QUICK_KEEP=1` and `KDOS_QUICK_NOBUILD=1`.

| Variable | Does |
|---|---|
| `KDOS_QUICK_PHASES` | Widen the build, e.g. `04_phase4,05_desktop` |
| `KDOS_QUICK_NOBUILD=1` | Reuse what is already in `build/fs` |
| `KDOS_QUICK_KEEP=1` | Do not restart the session |
| `KDOS_QUICK_FILES` | Extra paths under `build/fs` to carry — a configuration file, a chord table |
| `KDOS_QUICK_SKEL=1` | Copy `/etc/skel/.config` over the live user's configuration in the guest |

The file list comes from the package database — `build/fs/var/lib/kpkg/db/<port>` is what that
port installed — so a program that gains a new name or data file is carried without anyone adding
it.

`KDOS_QUICK_KEEP=1` is right whenever the program under test is started on demand. Every
`kdos-shell` surface is started fresh by the chord that opens it, so the new binary runs with no
restart and the run is a minute shorter. It is wrong for `kdos-comp`, which is the session.

`KDOS_QUICK_FILES` carries a file from the tree's `fs/`: those paths are installed into `build/fs`
by the file-system step and belong to no package, so the database does not list them. The
file-system step must have run for them to be there. Add `KDOS_QUICK_SKEL=1` for a file under
`/etc/skel`, because skel seeds a *new* account and the desktop user's home was seeded at install
time — a chord table patched in skel changes nothing for the person logged in.

What the loop cannot carry:

- A new port, a kernel change or an initramfs change. Use the real build.
- Anything that owns a D-Bus name. `quickpatch.sh` restarts the session and stops the shell
  surfaces, and a notification raised in a restarted session has never been seen to draw, while
  the same call on a freshly booted ISO does. Verify the bus on a real boot.
- Evidence about the shipped image. What `make build` writes is the ISO; this is the loop you
  iterate in *before* taking the photograph that counts.

Two details the script handles for you. The compositor's socket file outlives the process that
created it, so "the socket exists" is true a millisecond after the kill and later steps would run
against the replaced binaries; the script waits for a *different* process id and removes the stale
socket. And `/etc/inittab` respawns `tty1`, which does not come back when the chain is killed, so
the script starts `kdos-desktop` itself, as the login shell would have.

### The flags

| Flag | Does |
|---|---|
| `--shot <file>` | Photograph the screen now, into this path |
| `--out <file>` | Where the final shot goes when no `--shot` was given (default `/tmp/kdos-shot.ppm`) |
| `--keys <keys>` | A monitor `sendkey`, e.g. `meta_l-a` |
| `--chord <keys>` | Hold the modifiers and tap the key over VNC, e.g. `super+shift+space`. Use it for anything with two modifiers: `sendkey` presses and releases the whole combination at once, and such a chord never arrives |
| `--type <text>` | Type into whatever has the focus, then Return |
| `--text <text>` | Type and stop, with no Return — for a search field or filter, where Return would act on the first match |
| `--mouse X,Y` | Move the pointer, in absolute pixels |
| `--click X,Y[,BTN]` | Move there and click; `BTN` is 1, 2 or 3 |
| `--drag X1,Y1,X2,Y2[,BTN]` | Press at the first point, move to the second with the button held, release |
| `--press X,Y[,BTN]`, `--release [X,Y]` | A drag held open across later steps, so a `--shot` between two `--press`es photographs it in progress. A button stays down only while its VNC connection is open, so the whole gesture uses one connection — which is why this is two flags and not a longer `--drag` |
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
| `--venus` | With `--gl`, also publish a Vulkan capability set. Does not work on an NVIDIA host — see below |
| `--audio` | Give the guest an HDA controller, so audio paths reach a real device |
| `--disk <qcow2>` | Attach a virtio disk — the target an unattended `kinstall` writes to |
| `--no-cdrom` | Leave the ISO off, so the disk is what boots |
| `--boot-disk` | Boot the disk with the ISO still attached, so an installed system can still reach the medium |
| `--data-disk <file>` | Attach a raw file as a plain virtio disk, to carry a large file into a guest with no network. Input only |
| `--scratch <file>` | Attach a raw file the guest writes a tar onto — how files come back out |
| `--usb <file>` | Attach a raw disk image as a USB stick |
| `--no-session` | Accepted and ignored: `tty1` already starts the desktop, so the steps drive the one the boot brought up |
| `--keep` | Leave the emulator running after the steps |
| `--serial-log <file>` | Where the serial console is logged (default `/tmp/kdos-serial.log`) |
| `--vnc-port <n>` | The VNC port (default 5909) |

Several of those answer questions a screenshot alone cannot.

`--audio` gives the guest a sound device. Without it the sound library fails to initialise and
every audio path in the guest is untestable. The rig uses `testing/qemu-audio.sh`, the same probe
every `make run*` target uses, to pick a real host backend where the container has one and
`-audiodev none` where it does not. The rig image carries neither PipeWire nor PulseAudio, so it
gets the null backend. A null backend consumes samples on a timer and a real one consumes them as a
device does, so a guest whose timing follows its own playback runs to a different clock on the
two; reproduce what `make run-hw` does in the accelerated image (`testing/qemu-hw`), which carries
both. The device is playback-only (`hda-output`), so recording is tested through `snd-aloop`
instead — see below.

`--soak` lets the session run between launch and measurement, because a monitor's cost over its
first few seconds is its start-up, which is not what is being measured.

`--root-script` is the form for a check too long to be one command. The script travels encoded,
in short lines — a terminal in canonical mode silently drops everything past its line limit, and
an encoded payload contains nothing the shell acts on before decoding — and the rig waits for a
marker the *guest* prints, so a step taking minutes is waited for rather than cut off. A plain
command waits a fixed short time, so anything longer must be a script.

**Vulkan in the rig.** `--gl` alone starts `virtio-vga-gl` with no Vulkan capability set, so the
guest's virtio driver opens nothing and every Vulkan tool silently falls back to lavapipe, the CPU
rasteriser. `vulkaninfo --summary` naming `llvmpipe` with `PHYSICAL_DEVICE_TYPE_CPU` is the tell,
and a `vkcube` or `vkgears` rate taken that way is a number about the host's cores. `--venus` adds
`venus=true` and blob resources, which requires the guest's memory to come from a shared `memfd`,
so it also changes the machine's memory backing. On an NVIDIA host neither end works: with
`--gpus all` (the only way the container has a host Vulkan device to forward to) QEMU aborts during
guest boot with exit 134; without it the guest boots, the virtio driver loads, and instance
creation fails with `ERROR_OUT_OF_HOST_MEMORY`, taking lavapipe with it. `--venus` therefore stays
opt-in, and a Vulkan number from this rig is lavapipe's until `vulkaninfo` names a GPU.

### Three things the rig has to get right

- A display dump answers "no surface" under accelerated graphics, so the framebuffer is read over
  VNC instead. VNC's encoding negotiation has an exact field layout, and one extra padding field
  desynchronises the stream so every later read blocks forever.
- Driving the session means a *login* shell (`su - kdos -c`). A plain user switch leaves the
  container tooling resolving the home directory to `/`, and every call fails on a permission
  error.
- A plain virtual display puts the compositor on software rendering, so the phosphor pass is off
  and not in the photograph. What is photographed is the cell grid underneath it. Use `--gl`, or
  the accelerated image, to see the pass.

### A real capture device, with no emulator flag

The emulated codec has no input, so nothing `--audio` gives can produce a capture signal.
`snd-aloop` can, and it is already on the image (`CONFIG_SND_ALOOP=m`, `snd-aloop.ko.zst`
shipped). Loading it gives the guest a card named `Loopback` with two PCMs, each with eight
playback and eight capture subdevices, cross-wired: what is written to `hw:Loopback,0` is read from
`hw:Loopback,1`.

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
  96,000 bytes peaking at 16382 — half of full scale, the `vol 0.5` that was played.
- The rates need not match. The player negotiates 48 kHz and the capture asks for 16 kHz signed
  16-bit; ALSA's plug layer converts, `sox` warns that it cannot use the format natively, and the
  bytes that arrive are correct. What matters is that the player opens first, so the capture side
  has a stream to read.
- `sync` before the harness stops the emulator, or a file written in the guest comes back
  zero-length.

### Moving a large file into the guest

Use a plain virtual disk (`--data-disk`), not a USB device. Emulated USB storage runs at under two
megabytes a second against a virtual disk's several hundred: a large pack takes over an hour one
way and seconds the other. Use `--usb` only when the test needs a genuinely removable device, which
is the removable-media daemon's whole subject.

A file is read back by block copy and truncated to its exact length, because a raw drive is rounded
up to a sector boundary.

### The host may not have an emulator

The rig runs unmodified inside a container image that carries one, with the repository bind
mounted and hardware virtualisation passed through. The image is built from this tree:

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest from testing/Dockerfile.qemu
```

It needs the network once; everything afterwards runs offline against the ISO. The image carries
QEMU, the OVMF firmware an EFI boot needs, and python3 — nothing else, because `vnc-shot.py`
speaks VNC using only the standard library. There is no pip step, so the only versions that matter
are the distribution's QEMU and OVMF.

Because the image is built from the tree, a photograph is reproducible from any clone.

### Harness traps

Each is a rule with its consequence.

- `pkill -f PATTERN` also matches any shell loop whose own command line contains `PATTERN` — a
  `while pgrep -f PATTERN` wait loop, for example — so it kills your own monitor along with the
  target.
- An exact-name kill cannot match a process name past 15 characters, so a restart that uses one
  restarts nothing and the old program keeps answering.
- The harness stops the emulator without a shutdown, so a root script must `sync` after writing to
  the disk, or the rewritten files are zero-length on the next boot.
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
- The accelerated image (`testing/qemu-hw`) needs no Python of its own. Start its emulator with the
  serial and monitor devices on Unix sockets in a bind-mounted directory, `chmod 666` them from
  inside once QEMU has made them — the container runs as root and the driver does not — and drive
  it from the host by importing `vnc-shot.py`'s own `Serial` and `Monitor`. `Super+Return` opens a
  terminal on the desktop, so `Monitor.type()` is enough to start a program.

## Checking the sources

Upstream source files are not in git; `make fetch` puts each one in its port directory, verified
against the recipe's `sha256 =` line (see
[Where sources come from](developing.md#where-sources-come-from)). Three checks keep that honest:

| Check | Run | Answers |
|---|---|---|
| `make fetch-check` (`ports/fetch --check [port…]`) | Offline, seconds | Every hashed source that git does not track is on disk and matches its hash; it lists what is missing or corrupt and exits 1 if anything is |
| Preflight's source checks | `testing/preflight.sh` | No recipe-hashed archive is tracked by git, `.gitignore` keeps fetched sources and the cache out of commits, and the fetch and publish scripts parse |
| The pre-push hook | Enabled with `git config core.hooksPath script/hooks` | A push naming a source hash the archive does not hold is refused; see [Writing ports](writing-ports.md#the-pre-push-hook) |

A file git tracks is never treated as an archived source: `ports/fetch` and `--check` skip it, and
preflight fails with "recipe-hashed archives are tracked by git — git rm --cached them". Until
such files leave the index, `make fetch-check` reports them as neither present nor missing. A
count of 0 in `All N archived sources present and verified` means every source is still tracked by
git, so nothing was checked.

## docscheck.sh

The authoring check for this book.

```sh
bash testing/docscheck.sh                     # every page, plus README.md
bash testing/docscheck.sh docs/kdos/05-developer/testing.md   # just these
```

It reports:

| Report | Means |
|---|---|
| `DEADLINK` | A relative link whose target file does not exist. The part after `#` is not checked |
| `HISTORY` | A phrase from the script's own list that almost always records the past rather than the present |
| `NOTITLE` | A page under `docs/kdos/` whose first line is not a `# ` title |
| `NOSEEALSO` | A page with no `## See also` section |
| `UNLISTED` | A page the book's index, `docs/kdos/README.md`, does not name (full runs only) |
| `MISSING` | A path given on the command line that does not exist |

It prints `docscheck: ok` and exits 0 when nothing is reported, and exits 1 otherwise. It cannot
catch a paragraph that narrates history in its own words, or a link to a heading that was renamed.

## Measuring frame rate

Three different numbers are called "fps", and a measurement that does not say which one it took
answers a question nobody asked.

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
a presentation event or from the frame clock. `kdos stutter` is the front end. A static screen
produces no frame events, so something has to be animating before that socket says anything.

### The overlay, which is already on every machine

mesa is built here with `-D gallium-extra-hud=true`, so a Gallium driver draws its own overlay over
any GL or GLES client, with no extra software and no change to the program:

```sh
GALLIUM_HUD=fps es2gears_wayland
GALLIUM_HUD=fps+frametime glmark2-es2-wayland     # both curves in one pane
GALLIUM_HUD=simple,fps es2gears_wayland           # text, no graph
GALLIUM_HUD=csv+fps+frametime vkgears             # values to stdout, for a script
GALLIUM_HUD=help es2gears_wayland                 # every name this driver can draw
```

In the syntax, `+` shares a pane, `,` opens a pane below, `;` opens a column, and `.w`/`.h`/`.x`/`.y`
size and place one. `GALLIUM_HUD_PERIOD` is the update interval in seconds; `0` means every frame.

It is a GL instrument. The frame sources are in every Gallium build; `gallium-extra-hud` adds the
disk, network and CPU-frequency ones. A Vulkan program draws no overlay — `vkgears` prints its own
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
| Does this machine have a Vulkan driver, and which | `vulkaninfo --summary` — run it first under an emulator, because a Vulkan tool falls back to lavapipe on the CPU without saying so |
| Which EGL renderer, extensions and configs a client gets | `eglinfo` |
| What an X11 client sees through Xwayland | `glxinfo` and `glxgears`, which exist only in a box |

### Desktop GL on the host, and why glxgears is not

Desktop GL is reached through EGL, never GLX. `glmark2-wayland` binds `EGL_OPENGL_API` and then
opens the GL entry-point library by a legacy name — `libGL.so` first, `libGL.so.1` second — and
prints `Error loading GL library` if neither answers. `libglvnd` is built `glx=disabled`, so it
builds no `libGL`; its recipe adds `libGL.so` as a filename alias of `libOpenGL.so.0`, which
carries the same dispatch table and every `gl*` entry point.

Three facts about that alias, each checkable:

- **It is a filename, not a SONAME.** `libOpenGL.so.0.0.0` has `SONAME libOpenGL.so.0`, and no
  library records `libGL.so` as a dependency. `-lGL` therefore *links*: `ld` finds the alias and
  records `DT_NEEDED libOpenGL.so.0`, which runs. A link that wants `glX*` fails naming the symbol,
  which is the honest answer.
- **`libGL.so.1` must not exist.** libepoxy treats any `libGL.so.1` it can open as the GLX provider
  and resolves `glXGetCurrentContext` from it, aborting when it is missing. Pointing `.1` at
  glvnd's `libOpenGL` kills the process on the second entry point a client resolves:
  `glXGetCurrentContext() not found: … undefined symbol`, `SIGABRT`, exit 134. On this image
  `Xwayland` is what links libepoxy. It reproduces on any machine with libepoxy and EGL: make a
  desktop-GL context current, call `glGetString` then `glGetIntegerv` through epoxy, and run it
  with `LD_LIBRARY_PATH` pointing at a directory holding `libGL.so.1 -> libOpenGL.so.0`.
- **Two programs on the image ask for the unsuffixed name, and both are served.**
  `glmark2-wayland` wants exactly this; `eglinfo`'s bundled loader lists `libGL.so.1` then
  `libGL.so` but is never reached, because `eglinfo` loads through
  `gladLoadGLLoader(eglGetProcAddress)`. `libgstgl` and libepoxy name only `libGL.so.1`, so they
  see no desktop GL library on this host and have nothing to mis-resolve.

The alias is not a GLX provider, and there is no GLX provider on this host.

An image carries the alias only if it was packed after `libglvnd` was built with it, and
`glmark2-wayland` fails with `Error loading GL library` on an image without it. To see what an
image has:

```sh
unsquashfs -ll build/iso_root/system.sfs | grep -E 'libGL|libOpenGL'
ls build/fs/usr/lib/libGL.so                 # the same question of the build tree
```

mesa is built `-D glx=disabled -D platforms=wayland`: no GLX and no X11 EGL platform, so `glxgears`
and `glxinfo` cannot be linked on this host at all. Xwayland is the single X exception, and those
two programs live in a box:

```sh
kdos app install app.mesa-utils
kdos-appbox -b app.mesa-utils run glxgears
kdos-appbox -b app.mesa-utils run glxinfo
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
  `/var/lib/kdos/packs` when there is no medium, and reports every check that needs one — a base,
  `rt-gtk`, the index, `app.keepassxc` (`PACKLANE_APP`) and `app.gimp` (`PACKLANE_QTAPP`) — as
  failed;
- `install-to-disk.sh` asks the installer for `app.zathura app.kcalc alpine` unless
  `INSTALL_PACKS` names others.

Run them against a medium or a store you have put those packs into.

| Harness | Runs | Does |
|---|---|---|
| `packlane.sh` | In the guest, desktop up | The application lane end to end: the daemon, the keyring, an install from the medium or the store, the launchers, a box, and the telemetry — reporting pass, fail or skip with a reason, and carrying on past a failure |
| `install-to-disk.sh` | In the guest, with `--disk` | Runs the installer unattended into the attached disk; the second half boots it with `--no-cdrom` |
| `bios-boot.sh` | On the host | Boots the ISO as a USB stick under SeaBIOS, with no OVMF anywhere — `vnc-shot.py` always boots UEFI, so it cannot answer this |
| `appsweep.sh` | In the guest, `--boot-disk` | For each catalogue application: install it off the medium, launch it through its shim, wait for the compositor to report a window, photograph it, and remove the box. Usage `appsweep.sh <outdev> <runtime\|all> [max]` |
| `appreport.sh` | On the host, needs ImageMagick | Reads what the sweep wrote to the scratch disk and renders it as a table and a contact sheet, flagging windows that mapped but painted nothing. Usage `appreport.sh <scratch.img> [outdir]` |
| `appbox-smoke.sh` | In the session, as the desktop user (`su - kdos -c 'WAYLAND_DISPLAY=wayland-0 testing/appbox-smoke.sh'`) | Does every boxed application's launcher start something: a cheap `command -v` pass (`--probe`), then real launches; names on the command line limit it to those |
| `hostcheck.sh` | In the guest | Whether the shipped binary a caller resolves by name is the one that can do the job — for example which `blkid` answers first on the search path |
| `oomd-fire.sh` | In the guest | Puts the machine under real memory pressure and reads `kdos-oomd`'s own status for the kill count and victim. Victim *selection* is tested offline by `kdos-oomd --fixture`; this proves the daemon wakes |
| `usability.sh` | On the host | Drives the desktop the way a person does — optionally at a given size, e.g. `usability.sh 1920x1080` — and leaves a numbered contact sheet; `testing/usability.md` is the checklist to read it against |
| `bootcheck/` | On the host | Plumbing for scripted boots with no display: `boot.sh [soft\|gl]` starts QEMU detached, `guest.py` runs a command on the serial console, `type.py` types into the console, `sock.py` owns one QEMU socket |
| `quick.sh`, `quickpatch.sh` | Host, then guest | [The fast loop](#the-fast-loop) |
| `qemu-audio.sh` | Sourced by the run targets and the rig | Picks a working audio backend rather than hard-coding one, because QEMU aborts at start-up on a backend its build lacks, and sets the mixer's rate, because QEMU's default is 44100 and the guest drives the codec at 48000 |
| `qemu-hw/` | On the host, needs Docker and the NVIDIA container toolkit | The containerised QEMU with accelerated (virgl) graphics behind `make run-hw` and `make rundisk-hw`; `probe.py` and `verify.py` check the GPU path |
| `rig-image.sh`, `devdeps-image.sh` | On the host | Build the rig image (`kdos-qemu-py`) and the build image (`kdos-devdeps`) |
| `barcheck.c`, `boxcheck.c` | Compiled by the self-test | The scrollbar's drawing against its hit test at every position, and the shell's two box questions against a fixture store |
| `prepare_base.py`, `test_runner.py` | On the host | Build a minimal root filesystem as a container image (`kdos-base-test`, from `Dockerfile.base`) and build individual ports against it, recording results in `testing/test_results.json`; `report_gen.py` summarises them and `mini_build.py` is the build step `prepare_base.py` runs |

## What is not tested

Stated so nobody assumes otherwise.

Nothing here tests the build itself, which takes hours and a container. The self-test runs the
orchestrator against a synthetic tree, which proves its logic, not that the distribution builds.

The memory daemon fires only under `testing/oomd-fire.sh`, a rig run rather than a self-test block,
because it needs a booted machine with real memory to exhaust.

The application lane — `packlane.sh` and `appsweep.sh` — cannot run against the shipped ISO alone,
because the ISO carries no application packs. Nothing tests the lane until someone provides packs;
see [The other harnesses](#the-other-harnesses).

Fourteen of `kdos-shell`'s names have no committed frame: `about`, `ascii`, `audio`, `bt`, `cal`,
`calc`, `clip`, `devices`, `ime`, `mediad`, `note`, `slit`, `time` and `users`. `ascii`, `ime` and
`mediad` have no dump at all; the rest draw something that depends on the host, which
`testing/goldens/README` names one by one — `cal` draws the current month, `kdos-slit` renders the
output of commands it runs, the mixer lists the machine's ALSA cards, and `kdos-users` reads its
password file. A golden pinned with a trick nobody can reproduce is worse than a missing one.

A list of pages that should have goldens must not skip the ones that have none yet. A loop that
checks for a committed golden and skips a page without one makes a newly listed page unreachable:
no golden, so skipped, so never given one, and the suite reports a clean run over a page nothing
has looked at. Being on that list is the claim that the page should have a golden, so a missing
one fails.

A golden that no `golden` call compares is worse than none: it agrees with the surface only until
the surface changes, and it looks to the next person like evidence that was checked. Every
committed frame is compared by a call.

The compositor and the shell are not compiled by the self-test on a bare host; use the build image.

Nothing that runs unattended has hovered a button. Defects such as a taskbar two rows above the
bottom of the screen, a tooltip that swallows the click on the button it describes, a Start button
whose label vanishes under the pointer, or an icon layer eighty columns wide on a 160-column screen
are green in `preflight.sh` and green in `selftest.sh`. `usability.sh` drives those paths and
photographs them; reading the result is a person's job.

A golden cannot see a translucent window. `window_opacity` writes the blend as the cell's literal
colour and leaves the slot as drawn, and a golden records the slot — so a window at 70 per cent and
one at 100 give identical goldens. Only the rig can show it.

The pointer's own pixels are in no test. The compositor draws the cursor, and a photograph of a
moving pointer is the only place to look at it.

## See also

- [Developing](developing.md) — the build and iteration loops these fit into
- [Writing desktop software](writing-desktop-software.md) — dumps and reference frames from the author's side
- [The C libraries](c-libraries.md) — what the assertions cover
- [Build troubleshooting](build-troubleshooting.md) — when preflight is not enough
- [Writing ports](writing-ports.md) — recipes, sources and the pre-push hook
- [Glossary](../06-reference/glossary.md) — the terms this page uses
- [Known gaps](../06-reference/known-gaps.md) — the full list of what does not exist
