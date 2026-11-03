# Status

This chapter states how mature each part of KDOS is and what each verdict rests on. It is for
anyone deciding whether to try KDOS, what to rely on once it is running, or where a contribution
would help most. It assumes the vocabulary of the
[Architecture overview](../03-architecture/overview.md); read that first if terms such as
*KWin*, *box* or *pack* are new.

Every status table below has three columns: the subsystem, a verdict, and the evidence. The evidence
is always something a reader can check: a test in `testing/selftest.sh`, `testing/plasmatest.sh` or
`testing/preflight.sh`, a recorded *fixture* (a frozen copy of the machine state a program reads,
such as a `/proc` tree or a device tree), a script run on a booted image, or daily use. Where the
evidence is weaker than the verdict suggests, the row says so. Other terms, such as *lane* and
*rig*, are defined in the [Glossary](glossary.md).

The companion chapter [Known gaps](known-gaps.md) lists what does not exist at all.

## Release line

KDOS is at **v0.2**. The book states the version only here and in the
[repository README](../../../README.md); the system reads it from `fs/etc/os-release`
(`VERSION="0.2"`).

The v0.2 line is a desktop built on KDE Plasma 6.8, KDE Frameworks and all of KDE Gear, with KDOS's
own packaging, boxes, updates, daemons and identity around it, and its interfaces are stable enough
to describe, which is what this book does. It comes with no support commitment, no migration
guarantee between lines and no tested hardware matrix: whoever runs it is the integrator. See
[Why KDOS](../01-philosophy/why-kdos.md#the-trade).

No v0.2 release has been cut and there is no `v0.2` tag. The releases published on `kunaldawn/kdos`
are `v0.1`, which carries the v0.1 image, and the numbered `sources-<N>` pre-releases, which are
the source archive described under [Host and packaging](#host-and-packaging) and carry no image. A v0.2 image
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
produced by it. Its least exercised piece is the source archive: its index names 2,614 archived
files, and its scripts and index are checked for soundness, but no harness publishes to it or
fetches from it end to end.

The desktop is the least proved part. Plasma, KWin, the Frameworks and the Gear applications are
upstream's, unforked, at the versions their recipes pin, and KDOS's own parts of it, the settings
pages, widgets, KWin effects and scripts, the Discover backend, the runner, the graphical installer
and the root daemons behind them, each carry a test suite run in the development image. No image
carrying this desktop has been booted: the login, the session, Phosphor mode and the update tray
are described from their code and their tests, not from a running machine. The rows below say
which.

The [pack lane](glossary.md#lane), from an installed pack to a composed box, works
end to end on a booted machine that holds packs, exercised by a harness that reports each skip with
its reason rather than passing silently.

The natively ported applications are unproved in the same way. The lists of the userland phases,
`40_lang` to `44_apps`, name 2,173 recipes for the languages, the system, the graphical stacks,
the toolkits and the applications, every source fetched and hashed, and no application has been
started on an image. A package in the build tree is current when its recipe's `kp_recipe_hash`
equals the one recorded under `/var/lib/kpkg/db/.recipe/`, and the next build rebuilds every package
that is not. See [Applications and boxes](#applications-and-boxes) below. KDOS builds for x86_64
only.

## Host and packaging

The host is everything that builds and installs KDOS itself: the ports tree, `kpkg`, the build
orchestrator and the source archive. See [Packaging](../03-architecture/packaging.md) and
[How KDOS is built](../05-developer/how-kdos-is-built.md).

| Subsystem | Status | Evidence |
|---|---|---|
| The ports tree | Stable | 2,432 recipes, 2,389 of them upstream ports on 102 shelves, listed by shelf in the [Ports catalogue](ports-catalogue.md). Preflight checks that every recipe parses, sits where the layout says under a name no other port has, and that every dependency resolves |
| kpkg, the package manager | Stable | It has built the whole tree. The self-test builds and installs synthetic ports through it: ownership under merged `/usr`, the shared indexes a package feeds, and skip-if-installed comparing the recipe hash are each asserted, and a staged tree that breaks a packaging rule is refused with that rule's message |
| Reproducible packages | Stable | The self-test builds one synthetic port twice, the second time with umask `077`, `TZ=Asia/Kolkata`, `XZ_OPT='-e --check=sha256 -T1'` and a 300 MiB `XZ_DEFAULTS` memory limit, and requires the two packages to be byte-identical, owned by uid and gid 0 with epoch modification times, and equal to their tar recompressed on two threads with `xz -9 --block-size=32MiB`. It also installs the port through `kpkg install`, once with the deleted `xz -0` package and once with a kept cache that must match the `-9` bytes. Full-size ports are not rebuilt twice by any harness |
| The build system | Stable | It builds the distribution. The self-test drives `kdosbuild` headless over a synthetic two-phase tree: a build, a snapshot, a restore that resumes after it, plan narrowing (which suppresses snapshots and sets `KDOS_REPLAY`), a deliberate failure that stops the build without a snapshot, and the `--json` event stream; a second fixture checks that a chroot package phase runs no step for a port installed and current, and does run one for a forced, an edited (before the phase or during it) or an unfindable port. `kdosbuild --selftest` separately asserts the view geometry, the log classifier, the retry of a failed phase, the jobserver and the merge of `build/snapshots/timings.json`, with the machine each figure is filed under. The KDE ports of `43_toolkits`, `44_apps` and `50_desktop` have not been through a full build |
| Snapshots and plans | Beta | Used by every incremental build, and by the synthetic-tree run above. Layered snapshots have been through synthetic trees only: the self-test's three-phase fixture, and a ten-phase tree emulating the distribution's phases with package installs, in which every phase's restore, including after a restore into fresh inodes and after a retaken phase, matched the tree it came from entry by entry. No real build has written a layered set yet |
| Upstream sources | In progress | `ports/sources.idx` names 2,614 files archived in the `sources-<N>` releases: every recipe-hashed source git does not carry in a port directory, and files whose hash no recipe names. Preflight checks the scripts and the index; no harness publishes to or fetches from the archive |
| The pack format | Stable | Malformed footers, both footer formats, signature states and a flipped payload byte are each asserted by the library self-test |
| The binary host | In progress | Signing, the index and deltas are asserted against a synthetic port, and `kdos update` installing only what a signed index covers is asserted against a fixture binhost. `make build KDOS_MAKE_BINHOST=1` writes a signed one to `build/binhost/` with its release index, and `ports/publish --release` publishes it sharded across releases of at most 1,000 assets, which has run only as `--dry-run`; no release carries one |
| Updates | In progress | The self-test signs a slot image, rebuilds it from a seed root and its delta, refuses a tampered one, carries a machine's configuration into an armed slot, writes a newer image into the inactive slot, and arms only a slot an acquire filled. The release index is asserted to split 2,001 files 1,000, 1,000 and 1 in path order, to find a slot image through its sharded, signed form, and to be refused unsigned, signed by an untrusted key, edited, for another release, with no `releases` root, or naming a file of other bytes; a binhost index or manifest it does not name is never offered. `kdos-sysupdated` is driven over fixture slots the way Discover calls it, its API diffed against Discover's own. `kpkg fetch`'s archive-then-upstream order and its refusal of a copy with the wrong hash are asserted. No release carries a signed tree, binhost or slot image yet, and no update has been applied on an installed machine. See [Known gaps](known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree) |
| The application catalogue | Stable | 73 applications and 2 datasets over 7 runtimes and 2 base packs, offered in 7 groups (17 `group` lines). Preflight checks the catalogue's rows against the tree. Applications are built on demand on the machine; the image carries the catalogue and no applications |

What each of the shortened rows above rests on:

- **The ports tree.** Of the 2,432 recipes, 2 are absent from the build tree's package
  database once a build has run every phase, and the database holds no name that no recipe gives.
  The two are `kdos-installer`, which `10_bootstrap` compiles by script and never records as a
  package, and `texlive-doc`, whose recipe `ports/unbuilt` lists as built by no phase. Every other
  port is installed by a package phase, as a name on its list or as a dependency of one that no
  earlier phase installed: 15 by `20_selfhost` (its list names 9), 127 by `30_foundation` (its
  list names 138, 11 of them installed by `20_selfhost`), 22 by `31_compilers`, 184 by `40_lang`,
  1,040 by `41_system`, 188 by `42_graphics`, 339 by `43_toolkits`, 423 by `44_apps`, 90 by
  `50_desktop` and 2 by `60_kernel`; `python3 testing/phaseclosure.py` prints these figures, and
  `testing/bookcounts.py` fails on a recipe that no phase installs unless `ports/unbuilt` names it.
  On a built tree, the absent names are what `comm -23` prints when it compares the names of the
  directories holding a `kpkgbuild` (`ports/core/*/*/kpkgbuild` and `src/*/*/kpkgbuild`), sorted
  under `LC_ALL=C`, against `ls build/fs/var/lib/kpkg/db`.
  `testing/preflight.sh` checks that every recipe parses as metadata, declares
  a name, version and release, has a `build.sh` that parses, carries the KDOS banner and names a
  hash for every source; that every recipe sits on a shelf `ports/shelves` lists, under a name no
  other port has; that every `depends` names a port that exists; that every phase's list resolves
  to a dependency order; and, through `testing/phaseclosure.py`, that every package phase from
  `30_foundation` on installs exactly the ports its list names, and that each port of `41_system`
  to `44_apps` is in the phase its closure gives it.
- **Upstream sources.** `ports/fetch` (run by `make fetch`) resolves every recipe hash from the port
  directory, the cache `ports/.srccache`, the `kunaldawn/kdos` archive or upstream, and regenerates
  a port's own vendor bundle when none of those holds it. `ports/publish` uploads new sources and
  can freeze a release's hash list; `script/hooks/pre-push` refuses a push that breaks the ports
  layout, adds or changes a patch without a `Subject:` line, leaves either book failing
  `testing/docscheck.sh` or `testing/kfscheck.py`, or names a hash the archive does not hold,
  checking every part of a file stored in parts. `ports/sources.idx` is in format 2, so `make
  fetch` can take every file from the archive when neither the port directory nor the cache holds
  it, and asks upstream only when the archive gives no copy that verifies.
  Preflight checks that the four scripts (`ports/srclib.sh`, `ports/fetch`, `ports/publish` and
  the hook) parse, that
  git tracks no recipe-hashed archive, that the ignore rules cover every source suffix, and that the
  index holds every format-2 rule: its first line, well-formed lines, known shelf tags, one line per
  hash, unique asset names within a release, and hash order. The pre-push hook runs only in a clone that has enabled it. See
  [Packaging](../03-architecture/packaging.md#where-sources-come-from).
- **The pack format.** The library self-test asserts that a footer with a wrong magic, an offset
  past the end of the file or too little file to hold it is refused; that a pack whose footer
  declares format 1 (the current writer declares format 2) still opens, verifies and can be
  restamped, which rewrites its footer to the current format; that an unsigned pack, an empty
  keyring and an untrusted key are three distinct answers; and that one flipped payload byte fails
  the hash before the signature is consulted.
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
| Boot and init | Stable | Boots from the ISO to a console. Preflight checks that the ISO script builds every boot path: BIOS and UEFI, from a disc (El Torito) and from a written stick (the partition table); and that every program `fs/etc/inittab` names is on the image. The self-test draws both splash motions and checks that the command line picks one |
| The login | Experimental | `kdos-dm` runs SDDM's Wayland greeter, which SDDM calls experimental, and falls back to a console prompt; `kdos-session-wrapper` starts Plasma on the one session bus. elogind, turnstile and the user services are new to the image: the self-test runs the turnstile backend's start and stop order and the per-user timers, and nothing of the graphical login has run on a booted image. See [Known gaps](known-gaps.md#the-login-screen-runs-on-a-wayland-greeter-upstream-calls-experimental) |
| A/B root slots | In progress | The state machine is complete and asserted on the build host: each slot boots its own kernel, the menu follows the state, a bad update led by the menu rolls itself back after three boots, and on UEFI a candidate gets one boot through `BootNext`. A trial is kept only when both halves of its bless are in, the end of `rcS` and a Plasma session that stayed up, which `kdos-notice`'s and `kdos-sysupdated`'s self-tests assert. The installer makes and fills slot B on an LVM erase install. On an installed virtual machine with UEFI firmware, slot B's own kernel booted through `BootNext`, a menu-led trial confirmed, and a hung trial rolled back to the confirmed slot; no physical firmware has run a trial; see [Known gaps](known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only) |
| Encrypted root | In progress | The initramfs unlock path is exercised against stub tools, with no encrypted volume and without root privileges. The initramfs unlocks the chosen slot's own container, and rolling from slot B back to A is asserted to hand back A's container. A second encrypted slot has never been booted |
| kinstall, the installer | In progress | The terminal wizard installs, exercised on a disk by `testing/install-to-disk.sh`. `kinstall --dump probe` and `--dump plan` print what it sees and what it would run without installing anything; the self-test asserts the plan against saved answers, the event stream the graphical installer reads, the Plasma files it writes for the new machine and the A/B layout. Preflight checks that every filesystem it offers can be mounted by the initramfs. The graphical installer, `kdos-installer-gui`, has its QtTest suite and has not been run on a booted image |

## The desktop

See [The session](../03-architecture/session.md) and the chapters of
[The programs](../04-programs/README.md).

| Subsystem | Status | Evidence |
|---|---|---|
| Plasma, KWin and the Gear applications | Experimental | Upstream's, at Plasma 6.8.2, Frameworks 6.31 and Gear 26.08.2, each pinned by a recipe with its flags explained in its `kfs.md`. KWin carries three KDOS patches, and `plasma-workspace`, `libksysguard` and others carry their own. None of it has been built into an image and started; `testing/plasma-probe.sh` and `testing/session-cost.sh` are the harnesses that say whether the session is whole and what it costs |
| KDOS's settings pages | In progress | Boxes, Firewall, Health, KDOS Look and System Versions each have a QtTest suite in `make plasmatest`, run against fake daemons that serve the same bus names. The root daemons behind them are asserted by the self-test |
| The Plasma integration | In progress | `kded-kdos`, the KDOS runner, `kdos-notice`, the library worker, the System Monitor plugin, the privacy and checklist widgets, Transcribe, the key card and the KWin scripts each have their tests: QtTest where they are Qt, the self-test where they are C (`kdos-runner` and `kdos-notice` are). `testing/defaultscheck.py` and `testing/xdgcheck.sh` read every KDOS default for a KDE program back, `testing/lnfcheck.py` holds the global themes and desks to what Plasma reads, and `testing/chordcheck.py` finds a chord bound twice |
| Discover and system updates | In progress | `kdos-discover`'s backend and notifier have QtTest suites over fixture catalogues and a fake `io.kdos.Slots1`. `kdos-sysupdated` answers a stock Discover's calls over fixture slots in the self-test. No update has reached Discover on a booted image |
| Phosphor mode | Experimental | `kdos phosphor` is asserted to put back every key it changed. The KDOS motion effects have run in a headless KWin on software rendering with animations forced, which shows the motion and says nothing about its cost on a GPU; the CRT pass refuses software rendering and has not been seen. See [Known gaps](known-gaps.md#phosphor-mode-needs-hardware-opengl) |
| The `kdos` command | In progress | The self-test compiles `kdos-tools` and runs most of the 42 subcommands against fixtures or a fake session bus: `app`, `appid`, `backup`, `checklist`, `clone`, `cve`, `desk`, `doctor`, `help`, `hey`, `hostname`, `keymap`, `locale`, `march`, `menu`, `notify`, `panel`, `persist`, `phosphor`, `places`, `privacy`, `rebuild`, `remind`, `share`, `speech`, `theme`, `thumb`, `tiling`, `timezone`, `toggle` and `update` among them. See [The kdos command](../04-programs/kdos-command.md) |
| The theme system | Stable | `kdos-theme` generates every accent's colour scheme, Konsole profile, qt5ct palette, wallpaper and greeter look at build time and fails the build on a scheme below its contrast floor. `kdos theme --audit` re-runs the terminal programs' generators and compares, and the self-test requires an edited, a deleted and a stray file each to be caught and repaired, and an accent change in a session to move only what is KDOS's |
| Accessibility | Experimental | `testing/atspi/` dumps each KDOS surface's AT-SPI tree and compares it with a committed golden on a booted image; `make atspi-selftest` checks the dump format on fixtures, and no golden has been taken from an image yet. Orca has not been heard on one |

## Applications and boxes

See [Packs and boxes](../03-architecture/packs-and-boxes.md) and
[kdos-appbox](../04-programs/kdos-appbox.md).

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-packd | Beta | The self-test runs `kdos-packd --fixture`, which solves and composes without mounting: it asserts the lowerdir order (application, runtime, base), signature verification, the refusal to compose a data pack into a box root, and that a store holding `<id>.kpack` and a kept `<id>-<version>.kpack` lists the installed one alone. On a booted machine that already holds packs, `testing/packlane.sh` installs a pack, mounts it read-only, composes a box and reports each skip with its reason. The image carries no packs, so they must be imported before that harness can pass. Rollback is not exercised by any harness |
| Boxes | Stable | One box per application. The self-test asserts that a box profile prints the podman flag behind every key, reports an unknown key by name, and says which keys it cannot enforce rather than reporting success; that a launch hands a box only its own sockets or does not start; and that a throwaway box gets one file, no network and nothing of the host |
| kdos-appbox | Stable | Every boxed launcher goes through it. The self-test asserts launcher generation from an image's desktop entries, that a boxed launcher is named with the app id KWin gives its windows, that a second box's launcher cannot collide with the default's, and that `kdos-appbox open` resolves a path to the program that opens it |
| Native applications | Experimental | 762 recipes in `43_toolkits` and `44_apps`, with their dependencies in `40_lang` to `42_graphics`, from the X11 client libraries, GTK, Qt 6 and 5, QtWebEngine, KDE Frameworks 6, WebKitGTK and wxWidgets up to the browsers, office suites, KDE Gear, creative, science, radio and CAD applications, games and emulators, each named in the list of the userland phase that builds it, `40_lang` to `44_apps`, in the file of its shelf. Every source is fetched and its hash recorded. Each is installed by the phase whose list names it, and no application has been started on an image. See [Known gaps](known-gaps.md#the-native-applications-have-not-been-run) |
| kdos-boxsock | In progress | Every boxed client is tagged through it with the `security-context-v1` protocol (which marks a Wayland connection as coming from a sandbox), and KWin, through KDOS's `sandbox-policy.patch`, grants a box a global only when its profile names it. The self-test compiles it against the protocol; the patched KWin has not refused a box on a booted image |

## Daemons

See [The daemons](../04-programs/daemons.md).

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-powerd | Beta | The self-test asserts through `--explain` and a fixture `/etc` that root and `wheel` may change the firewall, the accent and the splash without a prompt, that anyone else is asked of polkit, and that a group whose name merely starts with `wheel` is not admitted; and the `io.kdos.Power1` face over a test bus |
| kdos-settingsd | In progress | The self-test keeps `hostname1`, `timedate1` and `locale1` in the files KDOS reads, over a fixture tree, with every consumer's call checked against the KDE 6.8.2 sources; the bus half runs only where sd-bus headers exist |
| kdos-sysupdated | In progress | See *Discover and system updates* above |
| kdos-energyd | In progress | Asserted against four recorded power and process trees: nested RAPL domains (the processor's energy counters, where one counter can include another) are not double-counted, a wrapped counter is handled, a boxed application's processes roll up to it, and exited processes are accounted separately. Its report reaches `kdos-energy` and System Monitor's KDOS sensors, and the readings are relative by design |
| kdos-oomd | Beta | Victim selection is asserted against three recorded trees, one arranged so that only the memory budget can produce the right answer, and one of them replayed with a `meminfo` at 80% available, where no process is chosen. It has fired on a real machine: `testing/oomd-fire.sh` on a 4 GB guest took `full` memory pressure (the kernel's PSI measure of time during which every task stalled) to 158 ms against a 150 ms trigger, and the socket went from `kills 0` to `kills 1, last: python3 (pid 1615, 3644 MB)`. No run has had to choose between several large processes, and the Plasma processes it protects have not been in a run |

## Libraries, tooling and documentation

| Subsystem | Status | Evidence |
|---|---|---|
| The C libraries | Stable | 9 libraries, compiled with warnings as errors, with one shared assertion program (`src/libs/selftest.c`) and a check that every consumer still compiles against them. The suite can be run under the address and undefined-behaviour sanitizers with `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh`; see [The C libraries](../05-developer/c-libraries.md) |
| The test harness | In progress | 74 preflight checks, 92 self-test blocks, the Qt suite in `make plasmatest`, 30 recorded fixture sets. The rig's Plasma harnesses (`make session-probe`, `make shots`, `make atspi`) have not been run against an image of this desktop |
| The QEMU rig | Stable | Drives a real session, photographs it, and runs scripts inside the guest |
| The documentation | In progress | This book. Structural facts are taken from the tree; some measurements, such as the `kdos-oomd` run above, are quoted rather than re-taken. See [Known gaps](known-gaps.md#documentation) |

Four shipped programs carry no verdict here, because no harness exercises their behaviour: the box
init `kdos-boxinit`; the boot splash `kdos-splash` as it runs at boot, which the self-test replaces
with `true` when it runs the initramfs; the terminal demo `kdos-bb`, described in
[kdos-bb](../04-programs/kdos-bb.md); and the Welcome Center pages. The icons, wallpapers, sounds,
login theme and add-ons (`kdos-icons`, `kdos-wallpapers`, `kdos-sounds`, `kdos-sddm-theme`,
`kdos-addons`) are artwork and carry no verdict.

## Scale

Each row below gives the command or source location that produced it. Run the command at the top of
a clean checkout and you should get the same number.

| Measurement | Value | Command |
|---|---|---|
| Port recipes | 2,432 | `find ports/core src/system src/art src/desktop src/daemons -name kpkgbuild \| wc -l` |
| — in `ports/core` | 2,389 | `find ports/core -name kpkgbuild \| wc -l` |
| — in `src/system` | 8 | `find src/system -name kpkgbuild \| wc -l` |
| — in `src/art` | 8 | `find src/art -name kpkgbuild \| wc -l` |
| — in `src/desktop` | 21 | `find src/desktop -name kpkgbuild \| wc -l` |
| — in `src/daemons` | 6 | `find src/daemons -name kpkgbuild \| wc -l` |
| Shelves | 102 | `grep -cvE '^(#\|$)' ports/shelves` |
| Archived source files | 2,614 | `grep -cE '^[0-9a-f]{64} ' ports/sources.idx` |
| Catalogue applications | 73 | `grep -c '^app ' src/system/kdos-appbox/catalogue` |
| Catalogue datasets | 2 | `grep -c '^data ' src/system/kdos-appbox/catalogue` |
| Catalogue runtimes | 7 | `grep -c '^runtime ' src/system/kdos-appbox/catalogue` |
| Catalogue base packs | 2 | `grep -c '^base ' src/system/kdos-appbox/catalogue` |
| Catalogue `group` lines | 17 | `grep -c '^group ' src/system/kdos-appbox/catalogue` |
| Catalogue groups | 7 | `awk '$1=="group"{print $2}' src/system/kdos-appbox/catalogue \| sort -u \| wc -l` |
| Kernel | 7.2.11 | `grep '^version' ports/core/*/linux/kpkgbuild` |
| C libraries written here | 9 | `ls -d src/libs/*/ \| wc -l` |
| `kdos` subcommands | 42 | the dispatch in `kdos_main()`, `src/system/kdos-tools/kdos.c`, counting `help` |
| Names `kdos-tools` answers to | 12 | the `TOOLS[]` table in `src/system/kdos-tools/main.c` |
| Names `kpkg` answers to | 5 | the `TOOLS[]` table in `src/system/kdos-kpkg/main.c` |
| KDOS settings pages | 5 | `ls -d src/desktop/kcm-kdos-*/ \| wc -l` |
| Qt test suites | 18 | `ls -d src/desktop/*/tests/ \| wc -l` |
| Accents | 8 | the `KCOL_SCHEMES` macro in `src/libs/libkcolor/kcolor.h` |
| Build phases | 13 | `ls -d script/phases/*/ \| wc -l` |
| Preflight checks | 74 | `grep -c '==>' testing/preflight.sh`, less the one that builds `kpkg` |
| Self-test blocks | 92 | `grep -c '^echo "==>' testing/selftest.sh` |
| Recorded fixture sets | 30 | `ls -d testing/fixtures/*/ \| wc -l` |
| Distinct `KDOS_*` variables read | 159 | the command below |

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
