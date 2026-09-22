# Roadmap

Stated direction. Nothing on this page ships. A reader looking for what exists wants
[Status](status.md); a reader looking for what does not exist and is not intended wants
[Known gaps](known-gaps.md).

This page is deliberately short. Plans live at the repository root as `*.plan.md` files, and they
are plans rather than documentation: they change as work proceeds, and this page points at them
instead of duplicating them.

Nothing below is a schedule, and nothing below is a commitment. Each entry names what would have to
be true for it to land.

## aarch64 and mobile

A second build target, `aarch64-kdos-linux-musl`, on a specific phone and on an emulated machine,
reaching a login prompt over USB networking and SSH using KDOS's own toolchain, package manager and
ports tree.

Nothing of this exists in the tree. `script-mobile/` holds seven phase-environment files, an
orchestrator and one port helper. There is no mobile phase tree, no mobile build root and no mobile
port overlay.

The approach in outline:

- A sibling phase tree and build root, driven through the orchestrator's existing directory flags
  rather than by forking the orchestrator.
- The first phases cross-compile a base userland with a genuine cross toolchain; the later ones run
  inside an emulated-architecture chroot, so every existing recipe runs unmodified.
- A mobile port overlay ahead of the core repository, giving every existing recipe for free with
  per-port override — which is what the multiple-repository search order already exists for.
- Two boards sharing the early phases and their snapshots; only the kernel phase and packaging
  differ.

Beside it, a touchscreen desktop: a mobile-only package and an on-screen keyboard that keep every
principle of the existing one — the character-cell grid, the palette, one binary under many names,
no large toolkit on the host — redesigned from nothing for a finger rather than scaled down. And the
property only this arrangement can offer: the same device driving an external monitor with the
desktop session, rather than a phone interface stretched across it.

There is no plan of record for either. What is written down is this page; the shape above is what
the tree already supports, not something anybody has committed to.

## Direction

Themes the tree shows work heading toward, each with what would have to be true for it to land.

**Exercising the new input paths.** Touch and drag and drop are both implemented — one gesture
recogniser in the toolkit, fed by `wl_touch`; a data source and an accepted offer in the Wayland
backend — and neither has been run against real hardware. What would move them off
[Status](status.md)'s experimental line is a rig pass with a virtual touch device, and a real drag
between a KDOS surface and a boxed application.

**Widening what a drag can carry.** `text/plain` and `text/uri-list` only, with no MIME negotiation
and no deferred transfer, and only the trash accepts a drop on the desktop. Dropping onto a folder
means deciding what a move that half-succeeds across filesystems should do.

**A rig pass on the window model.** `libkwm` reproduces its 131-row contract fixture and the
compositor calls five of its entries, but no photograph shows a window landing where a person
expects. What would close it is one rig run on the ISO: open a terminal, tile a pair, switch a
workspace, lock and unlock, come back from `tty2`, and start a boxed graphical application.

**Something that reads the desktop.** Every widget already states what it is, what it is called and
what it is set to, through `ktui_announce()`, and nothing reads that record. It is the largest single
absence here, and it is a missing subsystem rather than a missing feature; see
[Accessibility](../02-user-guide/accessibility.md) for what would have to change.

**A greeter, or a decision not to have one.** `tty1` is handed to `agetty` and there is no account
chooser and no session chooser. One would be a Wayland client that runs before the compositor does.

**Per-output rendering settings.** The font size is one number for every screen, and fractional
scaling is not negotiated. Both matter on a machine with two displays of different densities, and
neither is a small change: the toolkit renders glyphs at an integer scale by construction.

**An updater for the second root slot.** The state machine is complete and nothing fills the
candidate slot. What is missing is the part that decides a new system image is available, writes it,
and marks the slot to try.

**A reference binary host.** The signing, the index, the three equality tests and deltas all work and
are tested. What does not exist is a public one, which is a hosting and key-custody question rather
than a code one.

**More reference frames, and lifting the skips.** Fourteen of the shell's fifty-two surfaces have
no committed reference frame, and three of those — `kdos-ascii`, `kdos-ime` and `kdos-mediad` —
have no offscreen dump to take one with. The compositor and the shell are not compiled by the
self-test on a bare host either. Both are the kind of gap that hides other gaps.

## Not planned

Things asked for often enough to be worth answering, with the reason and where it is argued.

| Not planned | Because |
|---|---|
| An X server on the host | [Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out) |
| systemd | [Principles](../01-philosophy/principles.md#no-systemd) |
| A large toolkit on the host | [Principles](../01-philosophy/principles.md#no-gtk-and-no-qt-on-the-host) |
| An existing desktop environment on the host | [Decisions](../01-philosophy/decisions.md#no-kde-gnome-or-any-existing-desktop-on-the-host) |
| Native ports of browsers, office suites or CAD | [Decisions](../01-philosophy/decisions.md#one-pack-per-application-not-one-image) |
| An application store | [Decisions](../01-philosophy/decisions.md#no-application-store) |
| Shipping a processor feature level | [Decisions](../01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population) |
| Telemetry of any kind | Nothing here reports anything anywhere |

## See also

- [Status](status.md) — what exists, and how mature it is
- [Known gaps](known-gaps.md) — what does not exist
- [Decisions](../01-philosophy/decisions.md) — the arguments behind what is not planned
- [Why KDOS](../01-philosophy/why-kdos.md) — the properties any future work has to preserve
