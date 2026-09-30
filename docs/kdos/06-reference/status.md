# Status

This chapter states how mature each part of KDOS is and what each verdict rests on. It is for
anyone deciding whether to try KDOS, what to rely on once it is running, or where a contribution
would help most. It assumes the vocabulary of the
[Architecture overview](../03-architecture/overview.md); read that first if terms such as
*surface*, *box* or *pack* are new.

Every status table below has three columns: the subsystem, a verdict, and the evidence. The evidence
is always something a reader can check: a test in `testing/selftest.sh` or `testing/preflight.sh`, a
committed *golden* (a text dump of what a program draws, compared byte for byte on every self-test
run), a recorded *fixture* (a frozen copy of the machine state a program reads, such as a `/proc`
tree or a device tree), a script run on a booted image, or daily use. Where the evidence is weaker
than the verdict suggests, the row says so. Other terms, such as *lane* and *rig*, are defined in
the [Glossary](glossary.md).

The companion chapter [Known gaps](known-gaps.md) lists what does not exist at all.

## Release line

KDOS is at **v0.2**. The book states the version only here and in the
[repository README](../../../README.md); the system reads it from `fs/etc/os-release`
(`VERSION="0.2"`).

The v0.2 line installs, boots, runs a desktop and runs applications, and its interfaces are stable
enough to describe, which is what this book does. It comes with no support commitment, no migration
guarantee between lines and no tested hardware matrix: whoever runs it is the integrator. See
[Why KDOS](../01-philosophy/why-kdos.md#the-trade).

No v0.2 release has been cut and there is no `v0.2` tag. The releases published on `kunaldawn/kdos`
are `v0.1`, which carries the v0.1 image, and the `src-<shelf>` pre-releases, one per shelf, which
are the source archive described under [Host and packaging](#host-and-packaging) and carry no image. A v0.2 image
is one you build from the repository; see [Getting started](../02-user-guide/getting-started.md).
How a release is made is in [Developing](../05-developer/developing.md#cutting-a-release).

## Reading the verdicts

The verdicts, from most to least mature:

| Verdict | Means |
|---|---|
| Stable | Used daily, exercised by the test harness, and changes to it are additive |
| Beta | Complete and exercised under real conditions, but not across the range of cases it must handle |
| In progress | Works, and is incomplete or unexercised in the places the row names |
| Experimental | Present, and not to be relied upon |

## Overall maturity

The host (the ports tree, the package manager, the build system and reproducibility) is the most
mature part of KDOS and the most thoroughly checked, because every other part of the system is
produced by it. Its least exercised piece is the source archive, whose scripts and index are
checked for soundness but are not exercised end to end by any harness.

The desktop is stable in daily use. Its libraries are well covered. Its surfaces are covered by
goldens rather than by interaction tests, and 14 of the shell's 54 are covered by neither; 3 of
those have no offscreen dump at all.

The pack lane (see the [Glossary](glossary.md)), from an installed pack to a composed box, works
end to end on a booted machine that holds packs, exercised by a harness that reports each skip with
its reason rather than passing silently.

The least mature parts of the desktop are the terminal `kdos-term`, the input-method window
`kdos-ime`, touch input, and drag and drop, all rated Experimental below. The resource daemons sit
between these and the stable core: their arithmetic is asserted against recorded state, and
`kdos-oomd` has made its one irreversible decision on a live machine once, with a single obvious
candidate. KDOS builds for x86_64 only.

The natively ported applications are the least proved part. The lists of the userland phases,
`40_lang` to `44_apps`, name 1,870 recipes for the languages, the system, the graphical stacks,
the toolkits and the applications, every source fetched and hashed; 859 of them have not been
through a build: 55 of 195 in `40_lang`, 302 of 967 in `41_system`, 75 of 186 in `42_graphics`,
183 of 242 in `43_toolkits` and 244 of 280 in `44_apps`. No application has been started on an
image. Of the recipes that do build, 73 carry changes that no build has carried out, among them
`pinentry` drawing a Qt dialog and `libdvdread` linking `libdvdcss`. See
[Applications and boxes](#applications-and-boxes) below.

Screen reading of KDOS's own windows has no row, because there is nothing to rate: a KDOS surface
exposes no tree of accessible objects, so Orca, which reads the native applications, reads none of
the desktop's own windows. `libktui` records what each focused control is, once per frame; nothing
carries that record out of the process or reads it outside the library's self-test. See
[Known gaps](known-gaps.md#desktop) and [Accessibility](../02-user-guide/accessibility.md).

## Host and packaging

The host is everything that builds and installs KDOS itself: the ports tree, `kpkg`, the build
orchestrator and the source archive. See [Packaging](../03-architecture/packaging.md) and
[How KDOS is built](../05-developer/how-kdos-is-built.md).

| Subsystem | Status | Evidence |
|---|---|---|
| The ports tree | Stable | 2,024 recipes, 2,000 of them upstream ports on 102 shelves, listed by shelf in the [Ports catalogue](ports-catalogue.md). Preflight checks that every recipe parses, sits where the layout says under a name no other port has, and that every dependency resolves |
| kpkg, the package manager | Stable | It has built the whole tree. The self-test builds and installs synthetic ports through it: ownership under merged `/usr`, the shared indexes a package feeds, and skip-if-installed comparing the recipe hash are each asserted |
| Reproducible packages | Stable | The self-test builds one synthetic port twice, the second time with umask `077`, `TZ=Asia/Kolkata`, `XZ_OPT='-e --check=sha256 -T1'` and a 300 MiB `XZ_DEFAULTS` memory limit, and requires the two packages to be byte-identical, owned by uid and gid 0 with epoch modification times, and equal to their tar recompressed on two threads with `xz -9 --block-size=32MiB`. It also installs the port through `kpkg install`, once with the deleted `xz -0` package and once with a kept cache that must match the `-9` bytes. Full-size ports are not rebuilt twice by any harness |
| The build system | Stable | It builds the distribution. The self-test drives `kdosbuild` headless over a synthetic two-phase tree: a build, a snapshot, a restore that resumes after it, plan narrowing (which suppresses snapshots and sets `KDOS_REPLAY`), a deliberate failure that stops the build without a snapshot, and the `--json` event stream; a second fixture checks that a chroot package phase runs no step for a port installed and current, and does run one for a forced, an edited (before the phase or during it) or an unfindable port. `kdosbuild --selftest` separately asserts the view geometry and the log classifier |
| Snapshots and plans | Beta | Used by every incremental build, and by the synthetic-tree run above. Layered snapshots have been through synthetic trees only: the self-test's three-phase fixture, and a ten-phase tree emulating the distribution's phases with package installs, in which every phase's restore, including after a restore into fresh inodes and after a retaken phase, matched the tree it came from entry by entry. No real build has written a layered set yet |
| Upstream sources | In progress | The format-2 index names no file yet, so every source comes from upstream until `ports/publish` fills it. Preflight checks the scripts and the index; no harness publishes to or fetches from the archive |
| The pack format | Stable | Malformed footers, both footer formats, signature states and a flipped payload byte are each asserted by the library self-test |
| The binary host | In progress | Signing, the index and deltas are asserted against a synthetic port. `make build KDOS_MAKE_BINHOST=1` writes a signed one to `build/binhost/`; no public binary host exists |
| The application catalogue | Stable | 73 applications and 2 datasets over 7 runtimes and 2 base packs, offered in 7 groups (17 `group` lines). Preflight checks the catalogue's rows against the tree. Applications are built on demand on the machine; the image carries the catalogue and no applications |

What each of the shortened rows above rests on:

- **The ports tree.** Of the 2,024 recipes, 889 are absent from the build tree's package
  database. 859 of them are in the lists of `40_lang` to `44_apps` and have not been through a
  build. Of the other 30, 22 are `50_desktop` ports and 2 are `60_kernel` ports, absent because
  this build tree stops before those phases, and 6 are recipes no phase list names. The figure is
  the recipe names absent from the package database: the names of the directories holding a
  `kpkgbuild` (`ports/core/*/*/kpkgbuild` and `src/*/*/kpkgbuild`), sorted under `LC_ALL=C`, and
  `comm -23` of them against `ls build/fs/var/lib/kpkg/db`.
  `testing/preflight.sh` checks that every recipe parses as metadata, declares
  a name, version and release, has a `build.sh` that parses, carries the KDOS banner and names a
  hash for every source; that every recipe sits on a shelf `ports/shelves` lists, under a name no
  other port has; that every `depends` names a port that exists; that every phase's list resolves
  to a dependency order; and, through `testing/phaseclosure.py`, that every package phase from
  `30_foundation` on installs exactly the ports its list names.
- **Upstream sources.** `ports/fetch` (run by `make fetch`) resolves every recipe hash from the port
  directory, the cache `ports/.srccache`, the `kunaldawn/kdos` archive or upstream, and regenerates
  a port's own vendor bundle when none of those holds it. `ports/publish` uploads new sources and
  can freeze a release's hash list; `script/hooks/pre-push` refuses a push that breaks the ports
  layout or names an unarchived hash, checking every part of a file stored in parts.
  `ports/sources.idx` is in format 2 and lists no file until `ports/publish` fills it; until then
  `make fetch` takes every file from upstream. Preflight checks that the four scripts parse, that
  git tracks no recipe-hashed archive, that the ignore rules cover every source suffix, and that the
  index holds every format-2 rule: its first line, well-formed lines, known shelf tags, one line per
  hash, unique asset names within a release, and hash order. The pre-push hook runs only in a clone that has enabled it. See
  [Packaging](../03-architecture/packaging.md#where-sources-come-from).
- **The pack format.** The library self-test asserts that a footer with a wrong magic, an offset
  past the end of the file or too little file to hold it is refused; that a pack whose footer
  declares format 1 (the current writer declares format 2) still opens, verifies and can be
  restamped, which rewrites its footer to the current format; that an unsigned pack, an empty
  keyring and an untrusted key are three distinct answers; and that one flipped payload byte fails
  the hash before the signature is consulted. The shell self-test asserts that a pack's image hash
  does not depend on its version, so an image that has not changed is recognised when its pack is
  built again.
- **The binary host.** Signing, the index, the build-configuration match and package deltas are
  asserted against a synthetic port. The refusals asserted are an empty keyring, an edited index, a
  package whose hash moved, a tampered package and a signature from an untrusted key; a different
  build configuration is asserted to be "no match" rather than a refusal. A delta is asserted to be
  smaller than its package, to reconstruct it byte for byte, and to leave nothing behind when
  tampered with. See [Packaging](../03-architecture/packaging.md#the-binary-host).

## Boot and system

See [Boot and init](../03-architecture/boot-and-init.md) and [kinstall](../04-programs/kinstall.md).

| Subsystem | Status | Evidence |
|---|---|---|
| Boot and init | Stable | Boots from the ISO. Preflight checks that the ISO script builds every boot path: BIOS and UEFI, from a disc (El Torito) and from a written stick (the partition table); and that every program `fs/etc/inittab` names is on the image. The self-test drives `kdos-bootctl` through an update that is never confirmed over three boots, and requires the rollback to the working slot |
| A/B root slots | In progress | The state machine is complete and asserted on the build host: each slot boots its own kernel, the menu follows the state, and on UEFI a candidate gets one boot through `BootNext` (the firmware variable naming the entry to boot once). `kdos update apply` installs into the mounted inactive slot, deploys its kernel and marks it to try. Nothing creates or first fills the second slot, and per-slot kernels have not been booted; see [Known gaps](known-gaps.md#boot-and-updates) |
| Encrypted root | In progress | The initramfs unlock path is exercised against stub tools, with no encrypted volume and without root privileges. The initramfs unlocks the chosen slot's own container, and rolling from slot B back to A is asserted to hand back A's container. A second encrypted slot has never been booted |
| kinstall, the installer | Stable | Installs. Exercised on a disk by `testing/install-to-disk.sh`. `kinstall --dump probe` and `--dump plan` print what it sees and what it would run without installing anything; the self-test asserts the plan against saved answers. Preflight checks that every filesystem it offers can be mounted by the initramfs. The unattended path is the one exercised: an interactive install runs to the end but stays on the Summary page with no progress shown, and a second press of *BEGIN INSTALL* starts a second install on the same disk; see [Known gaps](known-gaps.md#an-interactive-install-stays-on-the-summary-page) |

## The desktop

See [The session](../03-architecture/session.md) and the chapters of
[The programs](../04-programs/README.md).

| Subsystem | Status | Evidence |
|---|---|---|
| The compositor | Stable | A frozen fork of labwc 0.20.0, in daily use. Its twenty-three KDOS source files are less exercised than the labwc base |
| kdos-shell | Stable | 54 surfaces under 55 names. 40 of them have committed goldens, the panel and the desktop included. The 14 without are `kdos-ascii`, `kdos-cal`, `kdos-mediad`, `kdos-about`, `kdos-calc`, `kdos-time`, `kdos-users`, `kdos-note`, `kdos-slit`, `kdos-audio`, `kdos-bt`, `kdos-devices`, `kdos-clip` and `kdos-ime`; of those, `kdos-ascii`, `kdos-mediad` and `kdos-ime` have no offscreen dump at all. A surface that fails to compile drops out of the golden run with a line naming it; a link failure in any candidate drops every candidate surface and prints a NOTE naming each, so neither passes unnoticed; see [Testing](../05-developer/testing.md#how-the-dump-harness-is-built) |
| kdos-res | Stable | Eleven pages, each with goldens at three sizes, plus the detail page, taken against a recorded system state |
| The phosphor pass | Stable | On by default (`crt` in the compositor's configuration). Its input and output can be dumped to image files without a screen through `KDOS_CRT_DUMP`, and the self-test checks offscreen that redrawing only the damaged part gives the same picture as redrawing the whole screen. It appears in no photograph from the [rig](../05-developer/testing.md) (the QEMU harness that boots a real image and photographs it), because the rig's virtual display puts the compositor on software rendering, where the pass switches itself off |
| The `kdos` command | In progress | The self-test compiles `kdos-tools` and runs 10 of the 31 subcommands against fixtures: `app`, `clone`, `cve`, `doctor`, `march`, `rebuild`, `remind`, `stutter`, `theme` and `thumb`. `share` is exercised under its other name, `kdos-share`, with `croc` stubbed. `hey` is run on a booted machine by `testing/packlane.sh` and `testing/appsweep.sh`, and `notify` by `testing/usability.sh`. The remaining 18 are not run by any harness. See [The kdos command](../04-programs/kdos-command.md) |
| The theme system | Stable | `kdos theme --audit` re-runs the generators and compares; the self-test generates a themed home directory, requires it to audit clean, then requires an edited file, a deleted file, a stray file and a re-pointed alias each to be caught and repaired by re-running the accent. The tone ladder that gives the panel a legible middle tone is asserted in all eight accents |
| The portal backend | Stable | Every boxed application's file dialog goes through `xdg-desktop-portal-kdos`, and the dialog opens over the window that asked: `parent_window` is imported through `xdg-foreign` (the Wayland protocol that lets one client name another's window) and the compositor centres the dialog on its parent. The self-test asserts that the FileChooser portal keeps serving while a dialog is open |
| kdos-term | Experimental | Four goldens of its own and nine replay goldens of its state machine. `foot` is the default terminal; `kdos-term` has not been used as one day to day |
| The window model | In progress | `libkwm` reproduces a 106-row contract taken by reading the compositor line by line (`testing/fixtures/wm/geometry.txt`), replayed by the self-test. `kdos-comp` calls eight of its entries: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`, `kwm_ws_adjacent`, `kwm_edge_check`, `kwm_edge_best`, `kwm_clip_add` and `kwm_clip_sub`. That a window lands where a person expects is asserted against the fixture and has never been photographed |
| The display interface | In progress | `libkdisp` is the one place a program picks a display server. 65 C source and header files across `kdos-shell`, `kdos-res`, `kdos-lock` and `kdos-term` name it (and the four ports' build scripts link it), with 50 `kdisp_init()` call sites and 38 distinct `kdisp_*` names between them; ten `kdos-shell` front ends reach it through the one call in `sh_run()`. One implementation is registered, `kwl_impl` in `libkwl`; the interface is what keeps a second one a matter of linking rather than a branch in every surface |
| kdos-ime | Experimental | The input-method candidate window. fcitx5 has never been run against it; it has no golden and no offscreen dump |
| Touch | Experimental | One gesture recogniser in `libktui` — tap, long press, drag, scroll, pinch, edge swipe — asserted against driven sequences with the timestamps supplied, so a long press is tested without waiting for one. `libkwl` binds `wl_touch` and feeds it. It has never run against a real touchscreen, and no rig pass uses a virtual touch device |
| Surface motion | Experimental | `KtuiAnim` in `libktui` and the frame clock in `libkwl`. The self-test drives the curves, the end value on the terminal, a capture backend and with `motion = no`, and the clock itself through the real `kwl_poll_event()` under a simulated compositor: silent when idle, a tick per frame while something moves, one more after it ends. Three animations are shipped, the panel's launch pulse, the menus' sliding selection plate (`kch_px_row_anim`, whose slide, landing rules and eviction the self-test drives with the clock in hand) and the gliding list (`kwl_list_view`, whose every frame in between the self-test checks against the list painted independently at the position presented, through the real flush); a touchpad flick's coast is driven by the self-test through the real pointer handlers. None has been seen on a display, and the coast has not been felt on a touchpad: the rig cannot time a one-second pulse or a 100 ms slide without shots taken mid-flight, and has no touchpad. See [the design language](../03-architecture/design-language.md#motion) |
| Drag and drop | Experimental | Both directions are implemented in `libkwl`: `kwl_drag_start` offers a data source of the one MIME type its caller names, and the receiving side ranks an offer's types `text/uri-list`, then `text/plain;charset=utf-8`, then `text/plain`, and reads the best after the drop. No test asserts either direction, nothing has been dragged with a pointer, and no rig pass covers it |

What the shortened rows above rest on:

- **The compositor.** `kdos-comp` is a frozen fork of labwc 0.20.0. The KDOS additions are the 23
  files `src/desktop/kdos-comp/src/kdos-*.c`: per-box sandbox grants, the box chip (a small square in
  the box's colour at the left of a window's title), window grouping and position memory, the phosphor
  pass, supervised chrome, the idle and lid policy, thumbnails, the command socket, the fades and
  window transitions, and render-late frame scheduling. The self-test compiles them where wlroots is
  installed, and there also plays the fades, the window transitions and the scheduler against
  wlroots itself. Window transitions and render-late scheduling are off by default and have not
  been seen on a real display: the rig cannot time a 150 ms transition without shots taken
  mid-flight, and has no vertical blank for the scheduler to aim at. Preflight checks that the shipped `rc.xml`
  keeps labwc's default bindings and that every command in it exists. See
  [kdos-comp](../04-programs/kdos-comp.md).
- **kdos-term.** One binary that opens as a Wayland window under the compositor, runs on a text
  console with `--tty`, and renders offscreen with `--dump`. Its four goldens are each taken by
  running it: a command's output, colour and cursor addressing, a kitty inline image and a sixel.
  Its state machine, `libkvt`, is a hard fork of libtsm 4.7.1, and nine goldens replay streams
  recorded from `vim`, `htop`, `mc`, `less` and `tmux`, plus hand-written malformed, attribute,
  hyperlink and prompt-mark streams. The image decoder is exercised under mutation. See
  [kdos-term](../04-programs/kdos-term.md).
- **kdos-ime.** The candidate window is drawn as cells. It owns `org.kde.impanel` and answers both
  halves of the kimpanel protocol (the D-Bus interface an input-method engine uses to hand its
  window to the desktop): signals for the preedit, the text still being composed, and a method
  call for the candidate list. Both are written against the shapes fcitx5 5.1 sends, read out of
  its source; the port ships 5.1.22. The engines ship: `fcitx5`, `fcitx5-anthy`,
  `fcitx5-chinese-addons` and `fcitx5-hangul` are in `50_desktop`'s package list, and
  `/etc/skel/.config/fcitx5/profile` puts `keyboard-us`, `pinyin`, `anthy` and `hangul` in one
  group.

## Applications and boxes

See [Packs and boxes](../03-architecture/packs-and-boxes.md) and
[kdos-appbox](../04-programs/kdos-appbox.md).

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-packd | Beta | The self-test runs `kdos-packd --fixture`, which solves and composes without mounting: it asserts the lowerdir order (application, runtime, base), signature verification, and the refusal to compose a data pack into a box root. On a booted machine that already holds packs, `testing/packlane.sh` installs a pack, mounts it read-only, composes a box and reports each skip with its reason. The image carries no packs, so they must be imported before that harness can pass. Rollback is not exercised by any harness |
| Boxes | Stable | One box per application, in daily use. The self-test asserts that a box profile prints the podman flag behind every key, reports an unknown key by name, and says which keys it cannot enforce rather than reporting success |
| kdos-appbox | Stable | Every application launcher on the system goes through it. The self-test asserts launcher generation from an image's desktop entries, that a second box's launcher cannot collide with the default's, and that `kdos-appbox open` resolves a path to the program that opens it |
| Native applications | Experimental | 522 recipes in `43_toolkits` and `44_apps`, with their dependencies in `40_lang` to `42_graphics`, from the X11 client libraries, GTK, Qt 6 and 5, QtWebEngine, KDE Frameworks 6, WebKitGTK and wxWidgets up to the browsers, office suites, creative, science, radio and CAD applications, games and emulators, each named in the list of the userland phase that builds it, `40_lang` to `44_apps`, in the file of its shelf. Every source is fetched and its hash recorded; the published source archive holds almost none of them. Of those 522, 427 have not been through a build, and no application has been started on an image. See [Known gaps](known-gaps.md#the-native-applications-have-not-been-built) |
| kdos-boxsock | Stable | Every boxed client is tagged through it with the `security-context-v1` protocol (which marks a Wayland connection as coming from a sandbox), and the compositor grants a box an interface only when its profile names it. The self-test compiles it against the protocol |

## Daemons

See [The daemons](../04-programs/daemons.md).

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-powerd | Stable | In daily use by the desktop's power actions. The self-test asserts through `--explain` that only root, members of `seat` (the group of the person at the machine) and of `wheel` are admitted, and that a group whose name merely starts with `wheel` is not |
| kdos-mountd | Stable | Asserted against a recorded device tree with hand-built superblocks: the removable stick and the data disc are offered, the internal disk is refused, exactly five of the fixture's six devices are eligible (the stick, an encrypted stick, a live medium's two partitions and the data disc), and a stick claimed by `/etc/fstab` stops being offered |
| kdos-energyd | In progress | Asserted against four recorded power and process trees: nested RAPL domains (the processor's energy counters, where one counter can include another) are not double-counted, a wrapped counter is handled, a boxed application's processes roll up to it, and exited processes are accounted separately. Its report appears on the Energy page of `kdos-res` and, from a right-click on the panel's resource indicator, in `kdos-status`; the panel shows no energy reading of its own, and the readings are relative by design |
| kdos-oomd | Beta | Victim selection is asserted against three recorded trees, one arranged so that only the memory budget can produce the right answer. It has fired on a real machine: `testing/oomd-fire.sh` on a 4 GB guest took `full` memory pressure (the kernel's PSI measure of time during which every task stalled) to 158 ms against a 150 ms trigger, and the socket went from `kills 0` to `kills 1, last: python3 (pid 1615, 3644 MB)`. No run has had to choose between several large processes |

## Libraries, tooling and documentation

| Subsystem | Status | Evidence |
|---|---|---|
| The C libraries | Stable | 17 libraries, compiled with warnings as errors, with one shared assertion program (`src/libs/selftest.c`) and a check that every consumer still compiles against them. The suite can be run under the address and undefined-behaviour sanitizers with `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`; see [The C libraries](../05-developer/c-libraries.md) |
| The test harness | Stable | 53 preflight checks, 217 committed goldens, 49 recorded fixture sets |
| The QEMU rig | Stable | Drives a real session, photographs it, and runs scripts inside the guest |
| The documentation | In progress | This book. Structural facts are taken from the tree; some measurements, such as the `kdos-oomd` run above, are quoted rather than re-taken. See [Known gaps](known-gaps.md#documentation) |

Five shipped programs carry no verdict here, because no harness exercises their behaviour: the
screen locker `kdos-lock`, whose client half and setuid helper `kdos-checkpass` the self-test
compiles, and nothing more; the screen recorder `kdos-record`; the box init `kdos-boxinit`; the boot
splash `kdos-splash`, which the self-test replaces with `true` when it runs the initramfs; and the
terminal demo `kdos-bb`, described in [kdos-bb](../04-programs/kdos-bb.md). The cursor, icon and
GTK themes (`kdos-cursors`, `kdos-icons`, `kdos-gtk-theme`) are artwork and carry no verdict.

## Scale

Each row below gives the command or source location that produced it. Run the command at the top of
a clean checkout and you should get the same number.

| Measurement | Value | Command |
|---|---|---|
| Port recipes | 2,024 | `find ports/core src/system src/art src/desktop src/daemons -name kpkgbuild \| wc -l` |
| — in `ports/core` | 2,000 | `find ports/core -name kpkgbuild \| wc -l` |
| — in `src/system` | 5 | `find src/system -name kpkgbuild \| wc -l` |
| — in `src/art` | 6 | `find src/art -name kpkgbuild \| wc -l` |
| — in `src/desktop` | 8 | `find src/desktop -name kpkgbuild \| wc -l` |
| — in `src/daemons` | 5 | `find src/daemons -name kpkgbuild \| wc -l` |
| Shelves | 102 | `grep -cvE '^(#\|$)' ports/shelves` |
| Archived source files | 0 | `grep -cE '^[0-9a-f]{64} ' ports/sources.idx` |
| Catalogue applications | 73 | `grep -c '^app ' src/system/kdos-appbox/catalogue` |
| Catalogue datasets | 2 | `grep -c '^data ' src/system/kdos-appbox/catalogue` |
| Catalogue runtimes | 7 | `grep -c '^runtime ' src/system/kdos-appbox/catalogue` |
| Catalogue base packs | 2 | `grep -c '^base ' src/system/kdos-appbox/catalogue` |
| Catalogue `group` lines | 17 | `grep -c '^group ' src/system/kdos-appbox/catalogue` |
| Catalogue groups | 7 | `awk '$1=="group"{print $2}' src/system/kdos-appbox/catalogue \| sort -u \| wc -l` |
| Kernel | 7.2.7 | `grep '^version' ports/core/*/linux/kpkgbuild` |
| C libraries written here | 17 | `ls -d src/libs/*/ \| wc -l` |
| Names `kdos-shell` answers to | 55 | the `TOOLS[]` table in `src/desktop/kdos-shell/main.c` |
| Distinct `kdos-shell` surfaces | 54 | the distinct entry points in that table |
| `kdos` subcommands | 31 | the dispatch in `kdos_main()`, `src/system/kdos-tools/kdos.c`, counting `help` |
| Names `kdos-tools` answers to | 12 | the `TOOLS[]` table in `src/system/kdos-tools/main.c` |
| Names `kpkg` answers to | 5 | the `TOOLS[]` table in `src/system/kdos-kpkg/main.c` |
| `kdos-res` pages | 11 | `RES_PAGES[]` in `src/desktop/kdos-res/pages.c` |
| Control-centre pages | 9 | the `CAT_NAMES[]` table in `src/desktop/kdos-shell/settings.c` |
| Accents | 8 | the `KCOL_SCHEMES` macro in `src/libs/libkcolor/kcolor.h` |
| Build phases | 13 | `ls -d script/phases/*/ \| wc -l` |
| Preflight checks | 53 | `grep -c '==>' testing/preflight.sh`, less the one that builds `kpkg` |
| Committed goldens | 217 | `ls testing/goldens/*.txt \| wc -l` |
| — cell frames | 9 | `ls testing/goldens/cells-*.txt \| wc -l` |
| — `libkvt` replays | 9 | `ls testing/goldens/vt-*.txt \| wc -l` |
| — `kdos-term` frames | 4 | `ls testing/goldens/term-*.txt \| wc -l` |
| — `kdos-res` frames | 37 | `ls testing/goldens/res-*.txt \| wc -l` |
| Recorded fixture sets | 49 | `ls -d testing/fixtures/*/ \| wc -l` |
| Distinct `KDOS_*` variables read | 136 | the command below |

The variable count is the union of `getenv("KDOS_*")` calls under `src/` and `$KDOS_*` references in
the build scripts, the sources, the root filesystem, the harnesses, the source-archive scripts and
the `Makefile`:

```sh
{ grep -rhoE 'getenv\("KDOS_[A-Z0-9_]+' src | sed 's/getenv("//'
  grep -rhoE '\$\{?KDOS_[A-Z0-9_]+' script src fs testing ports/fetch ports/publish \
      ports/srclib.sh ports/update Makefile | sed 's/^\${\{0,1\}//'
} | sort -u | wc -l
```

## See also

- [Known gaps](known-gaps.md) — what does not exist at all
- [Testing](../05-developer/testing.md) — what each harness proves, and what it cannot
- [Why KDOS](../01-philosophy/why-kdos.md) — the trade this chapter measures
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — the choices whose maturity this chapter rates
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build the host rows describe
- [The ports catalogue](ports-catalogue.md) — every recipe counted under *Scale*, by shelf

<!-- book-nav -->
---

*Part VI — Reference, chapter 44.* Previous: [43. Known gaps](known-gaps.md) · [Contents](../README.md) · Next: [45. Glossary](glossary.md)
