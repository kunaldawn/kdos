# Status

This page records how mature each part of KDOS is and what each verdict rests on. A maturity claim
with no evidence behind it is an opinion, so the third column of every table below names something
checkable: a passing test, a committed reference frame, a recorded fixture, a measurement on real
hardware, or daily use.

Where the evidence is weaker than the verdict would suggest, the row says so.

## Release line

KDOS is at **v0.2**. The version string appears in three places and nowhere else: this page, the
[repository README](../../../README.md), and `fs/etc/os-release`.

The v0.2 line installs, boots, runs a desktop and runs applications, and its interfaces are stable
enough to describe — which is what this book does. It carries no support commitment, no migration
guarantee between lines, and no tested hardware matrix. You are the integrator. See
[Why KDOS](../01-philosophy/why-kdos.md#the-trade).

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
| The ports tree | Stable | 877 recipes. Every one parses, declares its metadata, carries a syntactically valid build script and checksums its sources — all checked by `testing/preflight.sh` |
| kpkg | Stable | Built the whole tree. The dependency resolver is verified against its predecessor over every port individually and over every phase list; reproducibility is asserted by building a port twice under a deliberately hostile environment |
| Reproducible packages | Stable | Real ports built twice come out byte-identical, including under a different umask, a different time zone and a threaded-compression variable |
| The build system | Stable | Builds the distribution. The orchestrator additionally runs end to end against a synthetic tree: a build, a snapshot, a restore, plan narrowing and a deliberate failure |
| Snapshots and plans | Stable | Exercised by every incremental build, plus the synthetic-tree run |
| The pack format | Stable | Byte-identical across rebuilds under a hostile environment; a small delta reconstructs its pack exactly; the parse-whole and hash-before-signature rules are asserted in the suite |
| The binary host | In progress | Signing, the index, the three equality tests and deltas are all asserted against a synthetic port, including four distinct refusals. There is no public host — the mechanism is complete and unused |
| The application catalogue | Stable | 183 applications and 2 datasets over 7 runtimes and 2 base packs, in 21 groups. Built on demand; nothing is baked into the image |

## Boot and system

| Subsystem | Status | Evidence |
|---|---|---|
| Boot and init | Stable | Boots. The A/B state machine is asserted by running selection past its attempt limit without confirming and requiring the rollback |
| A/B root slots | In progress | The state machine is complete and asserted. Nothing fills the second slot; that is an updater's job, and there is no updater |
| Encrypted root | In progress | The unlock path is exercised against stub tools with no encrypted volume and no root. It is not wired to A/B slots |
| kinstall | Stable | Installs. Exercised by `testing/install-to-disk.sh`; its plan and its probe can both be dumped without running |

## The desktop

| Subsystem | Status | Evidence |
|---|---|---|
| The compositor | Stable | A frozen fork of a mature upstream, in daily use. The KDOS additions are the newer half |
| kdos-shell | Stable | 52 surfaces under 53 names. 38 of them have committed reference frames, the panel and the desktop included; the 14 without are `kdos-ascii`, `kdos-cal`, `kdos-mediad`, `kdos-about`, `kdos-calc`, `kdos-time`, `kdos-users`, `kdos-note`, `kdos-slit`, `kdos-audio`, `kdos-bt`, `kdos-devices`, `kdos-clip` and `kdos-ime`. A front end that fails to compile or to link drops out of the dump harness with a *goldens are skipped* line naming it, rather than passing as a surface nobody covered; see [Testing](../05-developer/testing.md) |
| kdos-res | Stable | Eleven pages, with reference frames at three widths for all of them plus the detail page, taken against a recorded system state |
| The phosphor pass | Stable | On by default. Its input and output can be dumped without a screen. It appears in no rig photograph: the rig's virtual display puts the compositor on software rendering, where the pass declines |
| The theme system | Stable | Generator output is verified byte-identical across all eight accents. The audit re-runs the generators and compares, and its four failure modes are asserted |
| The portal backend | Stable | Every boxed application's file dialog goes through it, and it opens over the window that asked: `parent_window` is imported through `xdg-foreign` and the compositor centres the dialog on its parent |
| kdos-term | Experimental | One binary that opens as a Wayland window under the compositor, runs at a prompt with `--tty`, and renders offscreen with `--dump`. Four committed reference frames, each taken by running it: a command's output, colour and cursor addressing, a kitty inline image and a sixel. The state machine is a fork of a mature one and the image path is fuzzed. It has not been driven through `vim`, `htop`, `mc`, `tmux` or `lf`, which is why `foot` remains the default terminal |
| The window model | In progress | `libkwm` reproduces a 106-row contract taken by reading the compositor line by line, replayed by the self-test and clean under both sanitizers. `kdos-comp` calls five of its entries: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`, `kwm_ws_adjacent` and `kwm_edge_check`. That a window lands where a person expects is asserted against a fixture and has never been photographed |
| The display interface | In progress | `libkdisp` is the one place a surface picks a display server. 61 source files name it, carrying 59 `kdisp_init()` call sites between them and using 46 distinct `kdisp_*` entry points, across `kdos-shell`, `kdos-res` and `kdos-lock`. One implementation is registered, `kwl_impl` in `libkwl`; the seam is what keeps a second one a link line rather than a branch in every surface |
| kdos-ime | Experimental | The candidate window as cells. It owns `org.kde.impanel` and answers both halves of kimpanel — signals for the preedit, a method call for the candidate list — driven with the exact shapes fcitx5 5.1.21 sends, read out of its source. fcitx5 itself has not been run against it. The engines ship: `fcitx5`, `fcitx5-anthy`, `fcitx5-chinese-addons` and `fcitx5-hangul` are in `05_desktop`'s package list and are on the image, and `/etc/skel/.config/fcitx5/profile` puts all four in one group. What is missing is the run |
| Touch | Experimental | One recogniser in `libktui` — tap, long press, drag, scroll, pinch, edge swipe — asserted against driven sequences with the timestamp supplied, so a long press is tested without waiting for one. `wl_touch` is bound and feeds it. Never run against a real touchscreen, and no rig pass with a virtual touch device |
| Drag and drop | Experimental | Both directions: `libkwl` carries a data source on the send side and an accepted offer on the receive side, for `text/plain` and `text/uri-list`. The four verbs and their order are asserted over a socketpair. Never dragged with a pointer, and no rig pass |

## Applications and boxes

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-packd | Stable | Mount, compose, install and rollback are exercised on a booted machine by `testing/packlane.sh` |
| Boxes | Stable | One box per application, in daily use. Freeze is measured on a real box: a couple of megabytes against a merged root of several hundred |
| kdos-appbox | Stable | Every launcher on the system goes through it. Launch timings are measured cold, warm and repeat |
| kdos-boxsock | Stable | Every boxed client is tagged through it, and the compositor's sandbox denial is verified against a real capture client |

## Daemons

| Subsystem | Status | Evidence |
|---|---|---|
| kdos-powerd | Stable | In daily use by the desktop's power actions |
| kdos-mountd | Stable | Acceptance and both refusals are asserted against a recorded device tree with hand-built superblocks |
| kdos-energyd | In progress | Every arithmetic rule is asserted against four recorded power and process trees, with both traps confirmed to bite by disabling them. There is no panel surface, and the readings are relative by design |
| kdos-oomd | Beta | Victim selection is asserted against a recorded tree arranged so that only the memory budget can produce the right answer, and it has fired for real: `testing/oomd-fire.sh` on a 4 GB guest took `full` pressure to 158 ms against a 150 ms trigger, and the socket went from `kills 0` to `kills 1, last: python3 (3644 MB)`. What no run has arbitrated is a choice between several large processes |

## Libraries, tooling and documentation

| Subsystem | Status | Evidence |
|---|---|---|
| The C libraries | Stable | 17 libraries, compiled under warnings-as-errors, with a shared assertion program and a consumer compile check. The whole suite — assertions, consumer builds and every committed reference frame — runs clean under the address and undefined-behaviour sanitizers |
| The test harness | Stable | 47 preflight checks, 193 committed reference frames, 38 recorded fixtures |
| The QEMU rig | Stable | Drives a real session, photographs it, and runs scripts inside the guest |
| The documentation | In progress | This book. Structural facts are derived from the tree; some measurements are quoted rather than re-taken. See [Known gaps](known-gaps.md#documentation) |
| aarch64 and mobile | Not started | `script-mobile/` holds seven phase-environment files, an orchestrator and one port helper. There are no phase steps, no build root and no port overlay, and nothing has been built for the architecture. See [Roadmap](roadmap.md) |

## Scale

Every row below carries the command that produced it. Run it in a clean checkout and the number
should come back.

| Measurement | Value | Command |
|---|---|---|
| Port recipes | 877 | `find ports/core src/packages src/desktop -name kpkgbuild \| wc -l` |
| — in `ports/core` | 853 | `find ports/core -name kpkgbuild \| wc -l` |
| — in `src/packages` | 11 | `find src/packages -name kpkgbuild \| wc -l` |
| — in `src/desktop` | 13 | `find src/desktop -name kpkgbuild \| wc -l` |
| Catalogue applications | 183 | `grep -c '^app ' src/packages/kdos-appbox/catalogue` |
| Catalogue datasets | 2 | `grep -c '^data ' src/packages/kdos-appbox/catalogue` |
| Catalogue runtimes | 7 | `grep -c '^runtime ' src/packages/kdos-appbox/catalogue` |
| Catalogue base packs | 2 | `grep -c '^base ' src/packages/kdos-appbox/catalogue` |
| Catalogue groups | 21 | `grep -c '^group ' src/packages/kdos-appbox/catalogue` |
| Kernel | 7.0.10 | `grep '^version' ports/core/linux/kpkgbuild` |
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
| Preflight checks | 47 | `grep -c '==>' testing/preflight.sh`, less the one that builds `kpkg` |
| Committed reference frames | 193 | `ls testing/goldens/*.txt \| wc -l` |
| Recorded fixtures | 38 | `ls -d testing/fixtures/*/ \| wc -l` |
| Distinct `KDOS_*` variables read | 113 | the union of `getenv("KDOS_*")` in C and `$KDOS_*` in shell |

## The honest summary

The host — the ports tree, the package manager, the build system, reproducibility — is the most
mature part of KDOS and the most thoroughly checked. A distribution that cannot rebuild itself is
not one.

The desktop is stable in daily use and is where the newest code lives. Its libraries are well
covered. Its surfaces are covered by reference frames rather than by interaction tests, and fourteen
of the shell's fifty-two are covered by neither — three of those have no offscreen dump at all.

The application lane works end to end on a booted machine, exercised by a harness that reports
skips with reasons rather than passing silently.

The resource daemons are the weakest link, in a specific and stated way. Their arithmetic is well
tested against recorded state; one of them has never made its decision on a live machine.

Accessibility is the largest single absence, and it is not a gap in a feature. Nothing reads this
desktop, and closing that means building a tree of accessible objects this project does not have.
The toolkit holds half the material already — every focused control states what it is into a
per-frame record that no shipped program drains — so what is missing is a route out of the process
and a client at the end of it. See [Accessibility](../02-user-guide/accessibility.md).

Nothing here is tested against a broad hardware matrix, and nothing pretends to be.

## See also

- [Known gaps](known-gaps.md) — what does not exist at all
- [Roadmap](roadmap.md) — stated direction
- [Testing](../05-developer/testing.md) — what each harness proves, and what it cannot
- [Why KDOS](../01-philosophy/why-kdos.md) — the trade this page is the honest half of
