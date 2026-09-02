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
| **The pack catalogue** | Stable | 181 applications declared; 194 packs baked, 24 GB |
| **kdos-packd** | Stable | Mount, compose, install and rollback exercised on a booted machine by the application-lane harness |
| **Boxes** | Stable | One box per application, in daily use. Freeze measured on a real box: a couple of megabytes against a merged root of several hundred |
| **kdos-appbox** | Stable | Every launcher on the system goes through it; launch timings measured cold, warm and repeat |
| **The compositor** | Stable | A frozen fork of a mature upstream, in daily use. The KDOS additions are the newer half |
| **The phosphor pass** | Stable | On by default; its input and output can be dumped without a screen. **Not in any rig photograph** — the rig's display puts the compositor on software rendering, where the pass declines |
| **kdos-shell** | Stable | 28 surfaces, with committed reference frames for most; the panel itself has none |
| **kdos-res** | Stable | Ten pages, reference frames at three widths for all of them plus the detail page, against a recorded system state |
| **kdos-term** | **Experimental** | One binary that links as a Wayland window and as a console surface, with three committed reference frames taken by running it: a command's output, colour and cursor addressing, and a sixel. The state machine is a fork of a mature one and the image path is fuzzed. **No rig pass** — it has not been driven through `vim`, `htop`, `mc`, `tmux` or `lf`, which is why `foot` is still the default terminal everywhere |
| **kdos-cage** | **Experimental** | A hard fork of cage 0.3.1 on the wlroots this tree already pins. It builds through its own recipe, links wlroots and XWayland and none of ours dynamically, and the self-test compiles all eight files wherever wlroots exists. `--embed` is exercised for real by a second process: a guest renders into a shared mapping and an injected keycode changes a later frame. It has also been driven by `kdos-con` end to end — a guest's frames become sprites in the cell grid, a click reaches it at the pixel inside the cell, and snapping, workspaces and closing behave as for any window. **No guest has run on a VT** |
| **kinstall** | Stable | Installs; exercised by the disk-install harness; its plan and probe can be dumped without running |
| **kdos-powerd** | Stable | In daily use by the desktop's power actions |
| **kdos-mountd** | Stable | Acceptance and both refusals asserted against a recorded device tree with hand-built superblocks |
| **kdos-energyd** | In progress | Every arithmetic rule asserted against four recorded power and process trees, with both traps confirmed to bite by disabling them. **No panel surface**, and the readings are relative by design |
| **kdos-oomd** | **Experimental** | Victim selection asserted against a recorded tree arranged so only the memory budget can produce the right answer. **It has never fired for real** — a genuine memory-pressure stall is the test that matters and has not been run |
| **kdos-boxsock** | Stable | Every boxed client is tagged through it; the compositor's sandbox denial verified against a real capture client |
| **The portal backend** | Stable | Every boxed application's file dialog goes through it |
| **The C libraries** | Stable | 19 libraries, compiled under warnings-as-errors, with a shared assertion program and a consumer compile check; the whole suite — assertions, consumer builds and every committed reference frame — runs clean under address and undefined-behaviour sanitizers |
| **The window model** | In progress | `libkwm` reproduces a 124-row contract taken by reading the compositor line by line, replayed by the self-test and clean under both sanitizers. `kdos-comp` calls it for placement, tiling, the edge search and workspace stepping, and **builds and links**. **No rig pass** — that a window lands where a person expects is not yet shown |
| **The display interface** | In progress | `libkdisp` is the one place a surface picks a display server. The conversion moved 33 `kdisp_init` call sites and 174 other calls across `kdos-shell`, `kdos-res` and `kdos-lock`, and every committed reference frame came back byte-identical afterwards. Only one implementation exists so far, so the seam is proved by construction rather than by use |
| **The console desktop** | **Experimental** | `kdos-con` composites windows, terminals and native surfaces on a character grid, checked against three committed goldens — one offscreen, one two-window, and one taken through a real session and a real view in two processes. Placement and tiling are `libkwm`'s, so they are the compositor's. A graphical application is a window: `kdos-cage --embed` composites it in a process of its own, the session cuts the frames into sprites, and a view with no pixels prints them as characters — run end to end against a real guest, with clicks, keys, snapping, workspaces and closing all observed. **It has never been booted into.** No rig pass, and the KMS path has never set a mode |
| **kdos-view** | **Experimental** | Compiles and **links** against real drm, input, seat, xkb, udev, fcft and pixman. The KMS mode is unproved — there is no display in a build container, so no mode has been set and no key read from libinput — but `--cast` has been: it registers a PipeWire node and a real consumer captured a frame of the console desktop, chrome, taskbar, clock and all. `--dump` and `--tty` turn a picture into characters by shape, which is how an embedded application and an animation are checked |
| **The console protocol** | In progress | `libkcon` carries surfaces and views over two typed unix sockets, asserted by the self-test including what it refuses. The kind of a client comes from which socket it reached rather than from its handshake, and the sources are grepped for `AF_INET` so the no-network claim cannot rot. **Attach, detach and forward have not been exercised between two machines** |
| **kdos-ime** | **Experimental** | The candidate window as cells, on both desktops. It owns `org.kde.impanel`, answers both halves of kimpanel — signals for the preedit, a method call for the candidate list — and has been driven with the exact shapes fcitx5 5.1.21 sends, read out of its source. **fcitx5 itself has not been run against it**: it is a large CMake port and is not in the build container |
| **Screen capture on the console** | **Experimental** | `kdos-view --cast` and a ScreenCast backend in `xdg-desktop-portal-kdos`, exercised against a real session bus, a real PipeWire and a real session: `CreateSession`, `SelectSources`, `Start` returning the node and its size, a frame taken out of it, and the view stopping when the session closes. **No application has recorded through it** — OBS in a box is a rig pass |
| **The console login** | **Experimental** | `kdos-con-login` reads `con.conf` and either hands the tty to `agetty --autologin` or draws a greeter that drops privilege and asks `kdos-checkpass`. **Neither path has been booted.** `kdos-getty` falls back to a plain autologin getty if it cannot be executed, so the failure mode is a console rather than none |
| **Touch** | **Experimental** | One recogniser in `libktui` — tap, long press, drag, scroll, pinch, edge swipe — asserted against driven sequences with the timestamp supplied, so a long press is tested without waiting for one. `wl_touch` is bound and feeds it. **Never run against a real touchscreen**, and no rig pass with a virtual touch device |
| **Drag and drop** | **Experimental** | Both directions implemented in `libkwl`: a data source on the send side, an accepted offer on the receive side, `text/plain` and `text/uri-list`. The desktop is a drag source and the trash a target. **Compiles and has never been dragged.** No rig pass |
| **The theme system** | Stable | Generator output verified byte-identical across all four accents; the audit re-runs the generators and compares, and its four failure modes are asserted |
| **Boot and init** | Stable | Boots. The A/B state machine is asserted by running selection past its attempt limit without confirming, and requiring the rollback |
| **A/B root slots** | In progress | The state machine is complete and asserted. **Nothing fills the second slot** — that is an updater, and there is no updater |
| **Encrypted root** | In progress | The unlock path is exercised against stub tools with no encrypted volume and no root. **Not wired to A/B slots** |
| **The test harness** | Stable | 28 preflight checks, 63 reference frames, 20 recorded fixtures |
| **The QEMU rig** | Stable | Drives a real session, photographs it, and runs scripts in the guest |
| **The documentation** | In progress | This book. Structural facts are derived from the tree; some measurements are quoted rather than re-taken — see [Known gaps](known-gaps.md#documentation) |
| **aarch64 / mobile** | **Not started** | No mobile phase tree, build root or port overlay exists. See [Roadmap](roadmap.md) |

## Scale

Counted from the tree at the time of writing.

| | |
|---|---|
| Port recipes | 764 |
| Packages on the built system | 758 |
| Applications in the catalogue | 181 |
| Packs baked | 194, 24 GB |
| Kernel | 7.0.10 |
| C libraries written here | 19 |
| Names `kdos-shell` answers to | 28 |
| `kdos` subcommands | 19 |
| Preflight checks | 28 |
| Committed reference frames | 70 |
| Recorded fixtures | 20 |

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

Nothing here is tested against a broad hardware matrix, and nothing pretends to be.

## See also

- [Known gaps](known-gaps.md) — what does not exist at all
- [Roadmap](roadmap.md) — stated direction
- [Testing](../05-developer/testing.md) — what each harness proves, and what it cannot
- [Why KDOS](../01-philosophy/why-kdos.md) — the trade this status is the honest half of
