# Roadmap

This page says where KDOS is heading. It is for anyone wondering whether something they want is
coming, or looking for a piece of work to take on. Nothing on this page ships today.

Two other pages answer neighbouring questions. For what exists and how mature it is, read
[Status](status.md). For what does not exist — including what is deliberately left out — read
[Known gaps](known-gaps.md).

This page is short on purpose. It names directions, not designs, and nothing on it is a schedule or
a commitment. Each entry says what would have to be true for it to land, so that you can tell how
far away it is. Terms such as *surface*, *box*, *pack* and *rig* are defined in the
[Glossary](glossary.md).

## aarch64 and mobile

The goal: a second build target, `aarch64-kdos-linux-musl`, running on a specific phone and on an
emulated machine, and reaching a login prompt over USB networking and SSH — built with KDOS's own
toolchain, package manager and ports tree.

None of this exists yet. `script-mobile/` holds seven phase-environment files, an orchestrator and
one port helper. There is no mobile phase tree, no mobile build root and no mobile port overlay.

The approach in outline:

- A sibling phase tree and build root, driven through the orchestrator's existing `--script-dir`
  and `--build-dir` flags rather than by forking the orchestrator.
- The first phases cross-compile a base userland with a genuine cross toolchain; the later ones run
  inside an emulated-architecture chroot, so every existing recipe runs unmodified.
- A mobile port overlay searched ahead of the core repository. Every existing recipe comes for free,
  and any one can be overridden per port — which is what the multiple-repository search order
  already exists for.
- Two boards sharing the early phases and their snapshots; only the kernel phase and packaging
  differ.

Alongside it, a touchscreen desktop: a mobile-only package and an on-screen keyboard that keep every
principle of the existing desktop — the character-cell grid, the palette, one binary under many
names, no large toolkit on the host — redesigned for a finger rather than scaled down. And one
property only this arrangement can offer: the same device driving an external monitor with the full
desktop session, rather than a phone interface stretched across it.

There is no plan of record for either. What is written down is this section, and the shape above is
what the tree already supports, not something anybody has committed to.

## Direction

Areas where the tree shows work heading, each with what would have to be true for it to land.

**Exercising the new input paths.** Touch and drag and drop are both implemented — one gesture
recogniser in the toolkit, fed by `wl_touch`; a data source and an accepted offer in the Wayland
backend — and neither has been run against real hardware. What would move them off
[Status](status.md)'s experimental line is a rig pass with a virtual touch device, and a real drag
between a KDOS surface and a boxed application.

**Widening what a drag can carry.** Drags carry `text/plain` and `text/uri-list` only, with no MIME
negotiation and no deferred transfer, and only the trash accepts a drop on the desktop. Letting a
drop land on a folder means first deciding what a move should do when it half-succeeds across
filesystems.

**A rig pass on the window model.** `libkwm` reproduces its 106-row contract fixture and the
compositor calls eight of its entries, but no photograph shows a window landing where a person
expects. What would close it is one rig run on the ISO: open a terminal, tile a pair, switch a
workspace, lock and unlock, come back from `tty2`, and start a boxed graphical application.

**Something that reads the desktop.** Every widget already states what it is, what it is called and
what it is set to, through `ktui_announce()`, and nothing reads that record. It is the largest single
absence in KDOS, and it is a missing subsystem rather than a missing feature; see
[Accessibility](../02-user-guide/accessibility.md#what-would-have-to-change) for what would have to
change.

**A greeter, or a decision not to have one.** `tty1` is handed to `agetty`, and there is no account
chooser and no session chooser. A greeter would be a Wayland client that runs before the compositor
does.

**Per-output rendering settings.** The font size is one number for every screen, and fractional
scaling is not negotiated. Both matter on a machine with two displays of different densities, and
neither is a small change: the toolkit renders glyphs at an integer scale by construction. One font
for every output is a deliberate narrowing today — see
[Decisions](../01-philosophy/decisions.md#narrowings).

**Filling the second root slot.** The A/B state machine is complete, and `kdos update apply` installs
package updates into the inactive slot, deploys its kernel and marks it to try. What is missing is
the step before that: creating and first populating slot B, which is done by hand — see
[Known gaps](known-gaps.md#boot-and-updates) — and any source of whole-system images rather than
package-by-package updates.

**A reference binary host.** The signing, the index, the three equality tests and deltas all work
and are tested. What does not exist is a public binary host, which is a hosting and key-custody
question rather than a code one.

**More reference frames, and lifting the skips.** Fourteen of the shell's fifty-two surfaces have no
committed reference frame, and three of those — `kdos-ascii`, `kdos-ime` and `kdos-mediad` — have no
offscreen dump to take one with. The compositor and the shell are not compiled by the self-test on a
bare host either. Both are the kind of gap that hides other gaps.

## Not planned

Things asked for often enough to be worth answering, with where the reasoning is.

| Not planned | Because |
|---|---|
| An X server on the host | [Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out) |
| systemd | [Principles](../01-philosophy/principles.md#no-systemd) |
| A large toolkit (GTK or Qt) on the host | [Principles](../01-philosophy/principles.md#no-gtk-and-no-qt-on-the-host) |
| An existing desktop environment on the host | [Decisions](../01-philosophy/decisions.md#no-kde-gnome-or-any-existing-desktop-on-the-host) |
| Native ports of browsers, office suites or CAD | [Decisions](../01-philosophy/decisions.md#one-pack-per-application-not-one-image) |
| Applications baked onto the installation medium | [Decisions](../01-philosophy/decisions.md#a-store-that-builds-and-a-medium-that-carries-nothing) |
| Upstream source archives stored in git | [Decisions](../01-philosophy/decisions.md#upstream-archives-are-content-addressed-release-assets) |
| Shipping a processor feature level | [Decisions](../01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population) |
| Telemetry of any kind | Nothing here reports anything anywhere |

## See also

- [Status](status.md) — what exists, and how mature it is
- [Known gaps](known-gaps.md) — what does not exist
- [Decisions](../01-philosophy/decisions.md) — the arguments behind what is not planned
- [Why KDOS](../01-philosophy/why-kdos.md) — the properties any future work has to preserve
