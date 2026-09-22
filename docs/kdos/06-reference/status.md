# Status

How mature each part of KDOS is, and **what each verdict rests on**. A maturity claim with no
evidence behind it is an opinion, so the third column of the table below names something checkable
rather than a feeling.

This page and the [repository README](../../../README.md) are the only two places a version
appears.

## Release line

**v0.2.**

What that implies: the system installs, boots, runs a desktop and runs applications, and its
interfaces are stable enough to document — which is what this book is. It is **not** a release with
a support commitment, a migration guarantee between lines, or a tested hardware matrix. You are the
integrator; see [Why KDOS](../01-philosophy/why-kdos.md#the-trade).

## What the words mean

| Verdict | Means |
|---|---|
| **Stable** | Used daily, exercised by the test harness, and changes to it are additive |
| **In progress** | Works, and is incomplete or unexercised in places named below |
| **Experimental** | Present, and not to be relied upon |

Every verdict below rests on one of: a passing check in the test suite, a committed reference
frame, a recorded fixture, a measurement on a real machine, or daily use. Where the evidence is
weaker than the verdict would suggest, the row says so.

## Subsystems

| Subsystem | Status | What that rests on |
|---|---|---|
| **The ports tree** | Stable | 764 recipes; every one parses, declares its metadata, has a syntactically valid build script, and has its sources checksummed — all checked by preflight |
| **kpkg** | Stable | Built the whole tree; the dependency resolver verified against its predecessor over every port individually and every phase list; reproducibility asserted by building a port twice under a deliberately hostile environment |
| **Reproducible packages** | Stable | Real ports built twice come out byte-identical, including under a different mask, a different time zone and a threaded-compression variable |
| **The build system** | Stable | Builds the distribution. The orchestrator additionally runs end to end against a synthetic tree: a build, a snapshot, a restore, plan narrowing, and a deliberate failure |
| **Snapshots and plans** | Stable | Exercised by every incremental build, plus the synthetic-tree run |
| **The binary host** | In progress | Signing, the index, the three equality tests and deltas are all asserted against a synthetic port, including four distinct refusals. **There is no public host** — the mechanism is complete and unused |
| **The pack format** | Stable | Byte-identical across rebuilds under a hostile environment; a small delta reconstructs its pack exactly; the parse-whole and hash-before-signature rules asserted in the suite |
| **The application catalogue** | Stable | 183 applications and 2 datasets over 7 runtimes, in 7 groups; built on demand, nothing baked |
| **kdos-packd** | Stable | Mount, compose, install and rollback exercised on a booted machine by the application-lane harness |
| **Boxes** | Stable | One box per application, in daily use. Freeze measured on a real box: a couple of megabytes against a merged root of several hundred |
| **kdos-appbox** | Stable | Every launcher on the system goes through it; launch timings measured cold, warm and repeat |
| **The compositor** | Stable | A frozen fork of a mature upstream, in daily use. The KDOS additions are the newer half |
| **The phosphor pass** | Stable | On by default; its input and output can be dumped without a screen. **Not in any rig photograph** — the rig's display puts the compositor on software rendering, where the pass declines |
| **kdos-shell** | Stable | 52 surfaces under 53 names, with committed reference frames, the panel and the desktop included. A missing frame is a link failure in the dump harness rather than a surface nobody covered — see [testing](../05-developer/testing.md) |
| **kdos-res** | Stable | Ten pages, reference frames at three widths for all of them plus the detail page, against a recorded system state |
| **kdos-term** | **Experimental** | One binary that links as a Wayland window and as a console surface, with three committed reference frames taken by running it: a command's output, colour and cursor addressing, and a sixel. The state machine is a fork of a mature one and the image path is fuzzed. **No rig pass** — it has not been driven through `vim`, `htop`, `mc`, `tmux` or `lf`, which is why `foot` is still the default terminal everywhere |
| **kinstall** | Stable | Installs; exercised by the disk-install harness; its plan and probe can be dumped without running |
| **kdos-powerd** | Stable | In daily use by the desktop's power actions |
| **kdos-mountd** | Stable | Acceptance and both refusals asserted against a recorded device tree with hand-built superblocks |
| **kdos-energyd** | In progress | Every arithmetic rule asserted against four recorded power and process trees, with both traps confirmed to bite by disabling them. **No panel surface**, and the readings are relative by design |
| **kdos-oomd** | **Beta** | Victim selection asserted against a recorded tree arranged so only the memory budget can produce the right answer, and **it has fired for real**: `testing/oomd-fire.sh` on a 4 GB guest took `full` pressure to 158 ms against a 150 ms trigger and the socket went from `kills 0` to `kills 1, last: python3 (3644 MB)`. What no run has arbitrated is a CHOICE between several large processes |
| **kdos-boxsock** | Stable | Every boxed client is tagged through it; the compositor's sandbox denial verified against a real capture client |
| **The portal backend** | Stable | Every boxed application's file dialog goes through it, and it opens over the window that asked: `parent_window` is imported through `xdg-foreign` and the compositor centres the dialog on its parent. |
| **The C libraries** | Stable | 19 libraries, compiled under warnings-as-errors, with a shared assertion program and a consumer compile check; the whole suite — assertions, consumer builds and every committed reference frame — runs clean under address and undefined-behaviour sanitizers |
| **The window model** | In progress | `libkwm` reproduces a 127-row contract taken by reading the compositor line by line, replayed by the self-test and clean under both sanitizers. `kdos-comp` calls five of its entries — `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`, `kwm_ws_adjacent` and, of the edge API, only `kwm_edge_check`. **No rig pass** — that a window lands where a person expects is asserted against a fixture and has never been photographed |
| **The display interface** | In progress | `libkdisp` is the one place a surface picks a display server. The conversion moved 33 `kdisp_init` call sites and 174 other calls across `kdos-shell`, `kdos-res` and `kdos-lock`, and every committed reference frame came back byte-identical afterwards. One implementation is registered, `kwl_impl`, and the seam is what keeps a second one a link line rather than a branch in every surface |
| **kdos-ime** | **Experimental** | The candidate window as cells. It owns `org.kde.impanel`, answers both halves of kimpanel — signals for the preedit, a method call for the candidate list — and has been driven with the exact shapes fcitx5 5.1.21 sends, read out of its source. **fcitx5 itself has not been run against it.** It ships — `fcitx5` and the `anthy`, `chinese-addons` and `hangul` engines are ports in `05_desktop` and are installed on the image — so what is missing is the run, not the engine. |
| **Touch** | **Experimental** | One recogniser in `libktui` — tap, long press, drag, scroll, pinch, edge swipe — asserted against driven sequences with the timestamp supplied, so a long press is tested without waiting for one. `wl_touch` is bound and feeds it. **Never run against a real touchscreen**, and no rig pass with a virtual touch device |
| **Drag and drop** | **Experimental** | Both directions: `libkwl` has a data source on the send side and an accepted offer on the receive side. `text/plain` and `text/uri-list`. The four verbs and their order are asserted over a socketpair. **Never dragged with a pointer.** No rig pass |
| **The theme system** | Stable | Generator output verified byte-identical across all seven accents; the audit re-runs the generators and compares, and its four failure modes are asserted |
| **Boot and init** | Stable | Boots. The A/B state machine is asserted by running selection past its attempt limit without confirming, and requiring the rollback |
| **A/B root slots** | In progress | The state machine is complete and asserted. **Nothing fills the second slot** — that is an updater, and there is no updater |
| **Encrypted root** | In progress | The unlock path is exercised against stub tools with no encrypted volume and no root. **Not wired to A/B slots** |
| **The test harness** | Stable | 28 preflight checks, 63 reference frames, 20 recorded fixtures |
| **The QEMU rig** | Stable | Drives a real session, photographs it, and runs scripts in the guest |
| **The documentation** | In progress | This book. Structural facts are derived from the tree; some measurements are quoted rather than re-taken — see [Known gaps](known-gaps.md#documentation) |
| **aarch64 / mobile** | **Not started** | No mobile phase tree, build root or port overlay exists. See [Roadmap](roadmap.md) |

## Scale

Counted from the tree at the time of writing.

| | | Counted by |
|---|---|---|
| Port recipes | 859 | `find ports/core src/packages src/desktop -name kpkgbuild` |
| Applications in the catalogue | 183 apps, 2 datasets, 7 runtimes, 21 groups | the `app`/`data`/`runtime`/`group` rows of `src/packages/kdos-appbox/catalogue` |
| Kernel | 7.0.10 | `ports/core/linux/kpkgbuild` |
| C libraries written here | 17 | `ls -d src/libs/*/` |
| Names `kdos-shell` answers to | 53 | the dispatch table in `main.c` |
| `kdos` subcommands | 30 | the dispatch in `kdos.c` |
| Preflight checks | 46 | `==>` lines in `preflight.sh` |
| Committed reference frames | 190 | `testing/goldens/*.txt` |
| Recorded fixtures | 37 | `testing/fixtures/*/` |

## The honest summary

The **host** — the ports tree, the package manager, the build system, reproducibility — is the most
mature part of KDOS and the most thoroughly checked, because a distribution that cannot rebuild
itself is not one.

The **desktop** is stable in daily use and is where the newest code lives. Its libraries are well
covered; its surfaces are covered by reference frames rather than by interaction tests, and six of
them are covered by neither.

The **application lane** works end to end on a booted machine and is exercised by a harness that
reports skips with reasons rather than silently passing.

The **resource daemons** are the weakest link in a specific and stated way: their *arithmetic* is
well tested against recorded state, and one of them has never made its decision on a live machine.

**Accessibility is the largest single absence** and it is not a gap in a feature, it is a missing
subsystem: nothing reads this desktop, and the work to change that is a tree of accessible objects
this project does not build. See [Accessibility](../02-user-guide/accessibility.md).

Nothing here is tested against a broad hardware matrix, and nothing pretends to be.

## See also

- [Known gaps](known-gaps.md) — what does not exist at all
- [Roadmap](roadmap.md) — stated direction
- [Testing](../05-developer/testing.md) — what each harness proves, and what it cannot
- [Why KDOS](../01-philosophy/why-kdos.md) — the trade this status is the honest half of
