# Status

This page says how mature each part of KDOS is, and what each of those verdicts rests on. It is for
anyone deciding whether to try KDOS, what to rely on once it is running, or where contributing would
help most.

Every row has three columns: the subsystem, a verdict, and the evidence. The evidence is always
something you can check yourself — a passing test, a committed reference frame (a *golden*: a
text dump of what a program draws, compared byte for byte), a recorded fixture, a measurement on
real hardware, or daily use. A verdict without evidence would be an opinion. Where
the evidence is weaker than the verdict suggests, the row says so.

Two companion pages cover what this one does not: [Known gaps](known-gaps.md) lists what does not
exist at all, and [Roadmap](roadmap.md) says where work is heading. Terms such as *surface*, *box*,
*pack*, *fixture*, *lane* and *rig* are defined in the [Glossary](glossary.md).

## Release line

KDOS is at **v0.2**. The version string appears in three places: this page, the
[repository README](../../../README.md), and `fs/etc/os-release` (`VERSION="0.2"`).

The v0.2 line installs, boots, runs a desktop and runs applications, and its interfaces are stable
enough to describe, which is what this book does. It comes with no support commitment, no migration
guarantee between lines and no tested hardware matrix: whoever runs it is the integrator. See
[Why KDOS](../01-philosophy/why-kdos.md#the-trade).

No v0.2 release has been cut. There is no `v0.2` tag, and the only release published on
`kunaldawn/kdos` is v0.1's. A v0.2 image is one you build from the repository — see
[Getting started](../02-user-guide/getting-started.md). How a release is made is in
[Developing](../05-developer/developing.md#cutting-a-release).

## Reading the verdicts

| Verdict | Means |
|---|---|
| Stable | Used daily, exercised by the test harness, and changes to it are additive |
| In progress | Works, and is incomplete or unexercised in the places the row names |
| Beta | Complete, and its central decision has been made correctly once under real conditions |
| Experimental | Present, and not to be relied upon |
| Not started | Named in the tree, with nothing behind the name |

## Host and packaging

| Subsystem | Status | Evidence |
|---|---|---|
| The ports tree | Stable | 1,038 recipes. `testing/preflight.sh` checks that every one parses, declares its metadata, has a build script that is valid bash, and names a hash for every source |
| kpkg, the package manager | Stable | It has built the whole tree. Its dependency resolver is checked against its predecessor for every port individually and for every phase list. Reproducibility is checked by building a port twice in a deliberately hostile environment |
| Reproducible packages | Stable | Real ports built twice come out byte-identical, including under a different umask, a different time zone and a threaded-compression variable |
| The build system | Stable | It builds the distribution. The orchestrator also runs end to end against a synthetic tree (`kdosbuild --selftest`): a build, a snapshot, a restore, plan narrowing and a deliberate failure |
| Snapshots and plans | Stable | Used by every incremental build, plus the synthetic-tree run |
| Upstream sources | In progress | `make fetch` resolves every recipe hash from the port directory, the local cache, the `kunaldawn/kdos` archive or upstream, and generates a port's own vendor bundle when none of those holds it; `ports/publish` uploads new sources and freezes a release's hash list; `script/hooks/pre-push` refuses a push naming an unarchived hash. Preflight checks that the four scripts parse, that git tracks no recipe-hashed archive and that the ignore rules cover every source suffix. No harness publishes to or fetches from an archive, and the pre-push hook runs only in a clone that has enabled it |
| The pack format | Stable | Byte-identical across rebuilds in a hostile environment. A small delta reconstructs its pack exactly. The parse-whole and hash-before-signature rules are asserted in the test suite |
| The binary host | In progress | Signing, the index, the three equality tests and deltas are all asserted against a synthetic port, including four distinct refusals. No public binary host exists: the mechanism is complete and unused |
| The application catalogue | Stable | 180 applications and 2 datasets over 7 runtimes and 2 base packs, offered in 7 groups (21 `group` lines). Applications are built on demand on the machine; nothing is baked into the image |

## Boot and system

| Subsystem | Status | Evidence |
|---|---|---|
| Boot and init | Stable | Boots. The A/B state machine is asserted by running slot selection past its attempt limit without confirming, and requiring the rollback |
| A/B root slots | In progress | The state machine is complete and asserted. `kdos update apply` installs into the mounted inactive slot, deploys its kernel and marks it to try. Nothing creates or first fills the second slot: that is done by hand — see [Known gaps](known-gaps.md#boot-and-updates) |
| Encrypted root | In progress | The unlock path is exercised against stub tools, with no encrypted volume and without root privileges. The initramfs unlocks the chosen slot's own container, and rolling from slot B back to A is asserted to hand back A's container. A second encrypted slot has never been booted |
| kinstall, the installer | Stable | Installs. Exercised by `testing/install-to-disk.sh`; its plan and its hardware probe can both be printed without installing anything |

## The desktop

| Subsystem | Status | Evidence |
|---|---|---|
| The compositor | Stable | A frozen fork of a mature upstream (labwc), in daily use. The KDOS additions — the files `src/desktop/kdos-comp/src/kdos-*.c`: per-box sandbox grants, the box chip, window grouping and memory, the phosphor pass, supervised chrome, the idle and lid policy, thumbnails and the command socket — are newer and less exercised than the labwc base; see [kdos-comp](../04-programs/kdos-comp.md) |
| kdos-shell | Stable | 52 surfaces under 53 names. 38 of them have committed reference frames, the panel and the desktop included. The 14 without are `kdos-ascii`, `kdos-cal`, `kdos-mediad`, `kdos-about`, `kdos-calc`, `kdos-time`, `kdos-users`, `kdos-note`, `kdos-slit`, `kdos-audio`, `kdos-bt`, `kdos-devices`, `kdos-clip` and `kdos-ime`. A surface that fails to compile or link drops out of the frame harness with a *goldens are skipped* line naming it, so it cannot pass unnoticed; see [Testing](../05-developer/testing.md) |
| kdos-res | Stable | Eleven pages, with reference frames at three widths for all of them plus the detail page, taken against a recorded system state |
| The phosphor pass | Stable | On by default. Its input and output can be dumped without a screen. It appears in no photograph from the [rig](../05-developer/testing.md) (the QEMU harness that boots a real image and photographs it), because the rig's virtual display puts the compositor on software rendering, where the pass switches itself off |
| The theme system | Stable | Generator output is verified byte-identical across all eight accents. The theme audit re-runs the generators and compares, and its four failure modes are asserted |
| The portal backend | Stable | Every boxed application's file dialog goes through it, and the dialog opens over the window that asked: `parent_window` is imported through `xdg-foreign` and the compositor centres the dialog on its parent |
| kdos-term | Experimental | One binary that opens as a Wayland window under the compositor, runs at a text console with `--tty`, and renders offscreen with `--dump`. Four committed reference frames, each taken by running it: a command's output, colour and cursor addressing, a kitty inline image and a sixel. The state machine is a fork of a mature one and the image path is fuzzed. It has not been driven through `vim`, `htop`, `mc`, `tmux` or `lf`, which is why `foot` remains the default terminal |
| The window model | In progress | `libkwm` reproduces a 106-row contract taken by reading the compositor line by line, replayed by the self-test and clean under the address and undefined-behaviour sanitizers. `kdos-comp` calls eight of its entries: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`, `kwm_ws_adjacent`, `kwm_edge_check`, `kwm_edge_best`, `kwm_clip_add` and `kwm_clip_sub`. That a window lands where a person expects is asserted against a fixture and has never been photographed |
| The display interface | In progress | `libkdisp` is the one place a program picks a display server. 66 source files across `kdos-shell`, `kdos-res`, `kdos-lock` and `kdos-term` name it, with 58 `kdisp_init()` call sites and 38 distinct `kdisp_*` names between them. One implementation is registered, `kwl_impl` in `libkwl`; the interface is what keeps a second one a matter of linking rather than a branch in every surface |
| kdos-ime | Experimental | The input-method candidate window, drawn as cells. It owns `org.kde.impanel` and answers both halves of the kimpanel protocol — signals for the preedit, a method call for the candidate list — driven with the exact shapes fcitx5 5.1 sends, read out of its source; the port ships 5.1.22. fcitx5 itself has not been run against it. The engines ship: `fcitx5`, `fcitx5-anthy`, `fcitx5-chinese-addons` and `fcitx5-hangul` are in `05_desktop`'s package list and on the image, and `/etc/skel/.config/fcitx5/profile` puts all four in one group. What is missing is the run |
| Touch | Experimental | One gesture recogniser in `libktui` — tap, long press, drag, scroll, pinch, edge swipe — asserted against driven sequences with the timestamps supplied, so a long press is tested without waiting for one. `wl_touch` is bound and feeds it. It has never run against a real touchscreen, and no rig pass uses a virtual touch device |
| Drag and drop | Experimental | Both directions: `libkwl` carries a data source on the sending side and an accepted offer on the receiving side, for `text/plain` and `text/uri-list`. The four protocol verbs and their order are asserted over a socketpair. Nothing has been dragged with a pointer, and no rig pass covers it |

## Applications and boxes

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-packd | Stable | Mount, compose, install and rollback are exercised on a booted machine by `testing/packlane.sh` |
| Boxes | Stable | One box per application, in daily use. Freezing is measured on a real box: a couple of megabytes against a merged root of several hundred |
| kdos-appbox | Stable | Every application launcher on the system goes through it. Launch times are measured cold, warm and repeated |
| kdos-boxsock | Stable | Every boxed client is tagged through it, and the compositor's sandbox denial is verified against a real screen-capture client |

## Daemons

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-powerd | Stable | In daily use by the desktop's power actions |
| kdos-mountd | Stable | Acceptance and both refusals are asserted against a recorded device tree with hand-built superblocks |
| kdos-energyd | In progress | Every arithmetic rule is asserted against four recorded power and process trees, and both traps are confirmed to bite by disabling them. There is no panel surface, and the readings are relative by design |
| kdos-oomd | Beta | Victim selection is asserted against a recorded tree arranged so that only the memory budget can produce the right answer, and it has fired for real: `testing/oomd-fire.sh` on a 4 GB guest took `full` pressure to 158 ms against a 150 ms trigger, and the socket went from `kills 0` to `kills 1, last: python3 (3644 MB)`. No run has had to choose between several large processes |

## Libraries, tooling and documentation

| Subsystem | Status | Evidence |
|---|---|---|
| The C libraries | Stable | 17 libraries, compiled with warnings as errors, with a shared assertion program and a consumer compile check. The whole suite — assertions, consumer builds and every committed reference frame — runs clean under the address and undefined-behaviour sanitizers |
| The test harness | Stable | 48 preflight checks, 193 committed reference frames, 38 recorded fixtures |
| The QEMU rig | Stable | Drives a real session, photographs it, and runs scripts inside the guest |
| The documentation | In progress | This book. Structural facts are taken from the tree; some measurements are quoted rather than re-taken. See [Known gaps](known-gaps.md#documentation) |
| aarch64 and mobile | Not started | `script-mobile/` holds seven phase-environment files, an orchestrator and one port helper. There are no phase steps, no build root and no port overlay, and nothing has been built for the architecture. See [Roadmap](roadmap.md#aarch64-and-mobile) |

## Scale

Each row below gives the command that produced it. Run it at the top of a clean checkout and you
should get the same number.

| Measurement | Value | Command |
|---|---|---|
| Port recipes | 1,038 | `find ports/core src/packages src/desktop -name kpkgbuild \| wc -l` |
| — in `ports/core` | 1,014 | `find ports/core -name kpkgbuild \| wc -l` |
| — in `src/packages` | 11 | `find src/packages -name kpkgbuild \| wc -l` |
| — in `src/desktop` | 13 | `find src/desktop -name kpkgbuild \| wc -l` |
| Catalogue applications | 180 | `grep -c '^app ' src/packages/kdos-appbox/catalogue` |
| Catalogue datasets | 2 | `grep -c '^data ' src/packages/kdos-appbox/catalogue` |
| Catalogue runtimes | 7 | `grep -c '^runtime ' src/packages/kdos-appbox/catalogue` |
| Catalogue base packs | 2 | `grep -c '^base ' src/packages/kdos-appbox/catalogue` |
| Catalogue groups | 21 | `grep -c '^group ' src/packages/kdos-appbox/catalogue` |
| Kernel | 7.2.7 | `grep '^version' ports/core/linux/kpkgbuild` |
| C libraries written here | 17 | `ls -d src/libs/*/ \| wc -l` |
| Names `kdos-shell` answers to | 53 | the `TOOLS[]` table in `src/desktop/kdos-shell/main.c` |
| Distinct `kdos-shell` surfaces | 52 | the distinct entry points in that table |
| `kdos` subcommands | 31 | the dispatch in `kdos_main()`, `src/packages/kdos-tools/kdos.c` |
| Names `kdos-tools` answers to | 12 | the `TOOLS[]` table in `src/packages/kdos-tools/main.c` |
| Names `kpkg` answers to | 5 | the `TOOLS[]` table in `src/packages/kdos-kpkg/main.c` |
| `kdos-res` pages | 11 | the page registry in `src/desktop/kdos-res/pages.c` |
| Control-centre pages | 9 | the `CAT_NAMES[]` table in `src/desktop/kdos-shell/settings.c` |
| Accents | 8 | the `KCOL_SCHEMES` macro in `src/libs/libkcolor/kcolor.h` |
| Build phases | 8 | `ls -d script/0*/ \| wc -l` |
| Preflight checks | 48 | `grep -c '==>' testing/preflight.sh`, less the one that builds `kpkg` |
| Committed reference frames | 193 | `ls testing/goldens/*.txt \| wc -l` |
| Recorded fixtures | 38 | `ls -d testing/fixtures/*/ \| wc -l` |
| Distinct `KDOS_*` variables read | 126 | the union of `getenv("KDOS_*")` under `src/` and `$KDOS_*` in `script/`, `src/`, `fs/`, `testing/`, `ports/fetch`, `ports/publish`, `ports/srclib.sh`, `ports/update` and the `Makefile` |

## The honest summary

The host — the ports tree, the package manager, the build system, reproducibility — is the most
mature part of KDOS and the most thoroughly checked. A distribution that cannot rebuild itself is
not one. The newest piece of it is the source archive, whose scripts are checked for soundness but
not yet exercised end to end by any harness.

The desktop is stable in daily use and is where the newest code lives. Its libraries are well
covered. Its surfaces are covered by reference frames rather than by interaction tests, and fourteen
of the shell's fifty-two are covered by neither; three of those have no offscreen dump at all.

The application lane works end to end on a booted machine, exercised by a harness that reports each
skip with its reason rather than passing silently.

The resource daemons are the weakest link, in a specific and stated way: their arithmetic is well
tested against recorded state, and one of them has never made its decision on a live machine.

Accessibility is the largest single absence, and it is not a missing feature but a missing
subsystem. Nothing reads this desktop aloud, and closing that means building a tree of accessible
objects this project does not have. The toolkit holds half the material already — every focused
control states what it is into a per-frame record that no shipped program reads — so what is missing
is a route out of the process and a client at the end of it. See
[Accessibility](../02-user-guide/accessibility.md).

Nothing here is tested against a broad hardware matrix, and nothing pretends to be.

## See also

- [Known gaps](known-gaps.md) — what does not exist at all
- [Roadmap](roadmap.md) — stated direction
- [Testing](../05-developer/testing.md) — what each harness proves, and what it cannot
- [Why KDOS](../01-philosophy/why-kdos.md) — the trade this page is the honest half of
