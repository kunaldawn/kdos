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
| **kinstall** | Stable | Installs; exercised by the disk-install harness; its plan and probe can be dumped without running |
| **kdos-powerd** | Stable | In daily use by the desktop's power actions |
| **kdos-mountd** | Stable | Acceptance and both refusals asserted against a recorded device tree with hand-built superblocks |
| **kdos-energyd** | In progress | Every arithmetic rule asserted against four recorded power and process trees, with both traps confirmed to bite by disabling them. **No panel surface**, and the readings are relative by design |
| **kdos-oomd** | **Experimental** | Victim selection asserted against a recorded tree arranged so only the memory budget can produce the right answer. **It has never fired for real** — a genuine memory-pressure stall is the test that matters and has not been run |
| **kdos-boxsock** | Stable | Every boxed client is tagged through it; the compositor's sandbox denial verified against a real capture client |
| **The portal backend** | Stable | Every boxed application's file dialog goes through it |
| **The C libraries** | Stable | 13 libraries, compiled under warnings-as-errors, with a shared assertion program and a consumer compile check; clean under address and undefined-behaviour sanitizers |
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
| C libraries written here | 13 |
| Names `kdos-shell` answers to | 28 |
| `kdos` subcommands | 19 |
| Preflight checks | 28 |
| Committed reference frames | 63 |
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
