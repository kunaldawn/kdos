# Roadmap

Stated direction. **Nothing on this page ships.** A reader looking for what exists wants
[Status](status.md); a reader looking for what does not exist and is not planned wants
[Known gaps](known-gaps.md).

This page is deliberately short. Plans live at the repository root as `*.plan.md` files and are
plans, not documentation — they change as work proceeds, and this page points at them rather than
duplicating them.

## aarch64 and mobile

**A second build target**, `aarch64-kdos-linux-musl`, on a specific phone and on an emulated
machine — reaching a login prompt over USB networking and SSH, using KDOS's own toolchain, package
manager and ports tree.

**Nothing of this exists in the tree yet.** There is no mobile phase tree, no mobile build root
and no mobile port overlay.

The approach in outline, from the plan of record:

- **A sibling phase tree and build root**, driven through the orchestrator's existing directory
  flags rather than by forking the orchestrator.
- **The first phases cross-compile** a base userland with a genuine cross toolchain; the later
  ones run inside an emulated-architecture chroot, so **every existing recipe runs unmodified**.
- **A mobile port overlay ahead of the core repository**, giving every existing recipe for free
  with per-port override — which is what the multiple-repository search order already exists for.
- **Two boards sharing the early phases and their snapshots**; only the kernel phase and packaging
  differ.

Beside it, a **touchscreen desktop**: a mobile-only package and an on-screen keyboard that keep
every principle of the existing one — the character-cell grid, the palette, one binary under many
names, no large toolkit on the host — redesigned from nothing for a finger rather than scaled down.
And the property only this arrangement can offer: **the same device driving an external monitor**
with the desktop session, rather than a phone interface stretched across it.

**Plan of record:** [`mobile.plan.md`](../../../mobile.plan.md) for the port,
[`mobile.plan2.md`](../../../mobile.plan2.md) for the touchscreen desktop.

## Direction

Themes the tree shows work heading toward, each with what would have to be true for it to land.

**Exercising the new input paths.** Touch and drag and drop are both implemented now — one
gesture recogniser in the toolkit, fed by `wl_touch`; a data source and an accepted offer in the
Wayland backend — and neither has been run against real hardware. What would move them off
[Status](status.md)'s experimental line is a rig pass with a virtual touch device and a real drag
between a KDOS surface and a boxed application.

**Widening what a drag can carry.** `text/plain` and `text/uri-list` only, with no MIME
negotiation and no deferred transfer, and only the trash accepts a drop on the desktop. Dropping
onto a folder means deciding what a move that half-succeeds across filesystems should do.

**Booting the console desktop.** Everything it is made of compiles, self-tests and reproduces its
goldens, and much of it has now been driven end to end between real processes — but none of it has
been booted into. What would move it off [Status](status.md)'s experimental line is one rig pass on
the ISO: log in, open a terminal, run `htop`, open `kdos-res` as a window, tile a pair, switch a
workspace, lock and unlock, come back from `tty2`, and start a boxed graphical application as a
window.

**Two specifics about a graphical application on the cell grid.** It is an ordinary window there —
`kdos-cage --embed` composites it in a process of its own and the session cuts the frames into
sprites — and two things about pointing at one are worth doing. A drag inside a single cell moves
its pointer nowhere, because a view reports a move when the cell changes; and an application that
needs acceleration a software renderer cannot give it has to be pinned to a terminal by hand,
rather than being detected.

**A second screen on the console.** `libkkms` takes the first card with a connected output and its
preferred mode. Multi-output means a view per output or a view that spans them, and the session
already treats the grid as one rectangle — which is the decision, not the code.

**A still screenshot of the console as a picture.** `kdos-view --cast` records the session into a
PipeWire stream — a view rasterises, so a recording is a view nobody looks at — and `kdos-shot`
still writes the grid as text. Rendering cells to a single image needs `libkcell`, which `kdos-tools`
does not link.

**Per-output rendering settings.** The font size is one number for every screen, and fractional
scaling is not negotiated. Both matter on a machine with two displays of different densities, and
neither is a small change — the toolkit renders glyphs at an integer scale by construction.

**An updater for the second root slot.** The state machine is complete and nothing fills the
candidate slot. What is missing is the part that decides a new system image is available, writes
it, and marks the slot to try.

**A reference binary host.** The signing, the index, the three equality tests and deltas all work
and are tested. What does not exist is a public one, which is a hosting and key-custody question
rather than a code one.

**More reference frames, and lifting the skips.** Six surfaces have no offscreen dump; the
compositor and the shell are not compiled by the self-test on a bare host. Both are the kind of gap
that hides other gaps.

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
| A KDOS demoscene | A from-scratch demo was written and removed at the maintainer's request. [kdos-bb](../04-programs/kdos-bb.md) is what ships |

## See also

- [Status](status.md) — what exists, and how mature it is
- [Known gaps](known-gaps.md) — what does not exist
- [Decisions](../01-philosophy/decisions.md) — the arguments behind what is not planned
- [Why KDOS](../01-philosophy/why-kdos.md) — the properties any future work has to preserve
