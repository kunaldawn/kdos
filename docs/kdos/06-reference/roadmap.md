# Roadmap

This chapter describes where KDOS is heading: the areas in which the tree already contains partial
work, what each would need before it could be called finished, and the requests that are answered
with a deliberate no. It is for anyone who wants to know whether something they need is coming, and
for anyone looking for a piece of work to take on. What each direction would add, as opposed to
the partial work it starts from, does not ship today. Read [Status](status.md) first for what
exists and how mature it is, and [Known gaps](known-gaps.md) for what does not exist, including
what is left out on purpose. Terms such as *surface*, *box*, *root slot*, *golden* (a committed
reference frame) and *rig* are defined in the [Glossary](glossary.md).

The chapter names directions, not designs, and nothing in it is a schedule or a commitment. Each
entry says what is in the tree today and what would have to be true for the work to land, so that a
reader can judge how far away it is. The second build target comes first because it is the
largest direction; the smaller ones follow under [Direction](#direction).

## aarch64 and mobile

The goal is a second build target, `aarch64-kdos-linux-musl`, running on a phone and on an emulated
machine and reaching a login prompt over USB networking and SSH, built with KDOS's own toolchain,
package manager and ports tree.

What exists is a skeleton in `script-mobile/`, which nothing in the `Makefile` reaches.
Each environment file declares its phase's title in `KDOS_PHASE_TITLE`, which the orchestrator
shows when it runs the phase:

| File | Phase title | What it does |
|---|---|---|
| `kdosbuild.sh` | — | Compiles the build orchestrator and runs it with `--script-dir script-mobile --build-dir build-mobile` |
| `toolchain.env.sh` | Cross Toolchain | Cross binutils and GCC targeting `aarch64-kdos-linux-musl` |
| `phase1.env.sh` | Base Userland | musl, toybox, bash, native GCC and `kpkg`, cross-compiled into the sysroot |
| `phase2.env.sh` | Self-Hosting Bootstrap | A chroot phase |
| `phase3.env.sh` | Toolchain & Core Libraries | A chroot phase |
| `phase4.env.sh` | Userland | A chroot phase |
| `kernel.env.sh` | Kernel | The board's kernel, device tree and modules, cross-built outside the chroot |
| `packaging.env.sh` | Packaging | Firmware, `rootfs.ext4` and `boot.img`, built outside the chroot |
| `util/port.sh` | — | Reads a recipe's metadata for the mobile steps |

A *board* is one supported device, with its own kernel configuration, device tree and boot image.
The board is chosen by `KDOS_BOARD`. The toolchain, phase 1, kernel and packaging environment files
default it to `fajita`, the codename of the one phone the skeleton targets, and it is meant to be
forwarded into the chroot phases through `chroot_exec.sh`. Only the kernel and packaging phases are
meant to read it. There are no phase directories under
`script-mobile/`, so the orchestrator discovers no phases and a run builds nothing. There is no
`ports/mobile/` overlay, no `build-mobile/` build root, and no `chroot_exec.sh` in `script-mobile/`,
which the orchestrator looks for beside the phase files before it can run any chroot phase.

The approach the skeleton encodes:

- **A sibling phase tree and build root.** The orchestrator is architecture-neutral, and the two
  targets differ only in the `--script-dir` and `--build-dir` flags it is given, so the mobile build
  reuses it rather than forking it. See [The build system](../05-developer/build-system.md), and
  [How KDOS is built](../05-developer/how-kdos-is-built.md) for the x86-64 build the phases mirror.
- **Cross first, emulated after.** The toolchain phase and phase 1 cross-compile a base userland
  with a real cross toolchain. Phases 2 to 4 set `CHROOT=1` and are meant to run inside an aarch64
  chroot under the host's `qemu-user` `binfmt` handler, so that every recipe in `ports/core`
  would run unmodified, as it would natively. The kernel and packaging phases run outside the
  chroot, because the kernel is cross-compiled anyway and packaging needs the host's Python for
  `mkbootimg`.
- **A port overlay.** Phases 2 to 4 set `PORT_REPO="/kdos/ports/mobile /ports/core"`. `PORT_REPO`
  is an ordered search path in which the first repository wins on a duplicate name, so every core
  recipe is available and a board-specific recipe of the same name overrides it. See
  [Packaging](../03-architecture/packaging.md).
- **Boards share the early phases.** Phases 0 to 4 and their snapshots are common to every board;
  only the kernel phase and packaging differ per board.

Alongside the build target, the direction includes a touchscreen desktop: a mobile-only package and
an on-screen keyboard that keep every principle of the existing desktop (the character-cell grid,
the palette, one binary under many names, no large toolkit on the host) and are redesigned for a
finger rather than scaled down. The same arrangement would let one device drive an external monitor
with the full desktop session, rather than stretching a phone interface across it. Neither the
build target nor the touchscreen desktop has a design yet.

## Direction

This section lists the areas where the tree shows work under way, each with what would have to be
true for it to land.

### Exercising touch and drag and drop

Touch and drag and drop are both implemented and both marked experimental in [Status](status.md).
Touch is one gesture recogniser in `libktui` (`ktui_gesture.c`) fed by `wl_touch` in `libkwl`; drag
and drop is a data source on the sending side and an accepted offer on the receiving side, also in
`libkwl`. Touch has never run against a real touchscreen, nothing has been dragged with a pointer,
and no test asserts either direction of a drag. What would move them off the experimental line is
a rig pass with a virtual touch device, and a real drag between a KDOS surface and a boxed
application.

### Widening what a drag can carry

A drag started by a KDOS surface offers exactly one MIME type, and its payload is copied when the
drag starts rather than produced on request. The desktop's icons start drags as `text/uri-list`.
The receiving side ranks the types an offer carries and takes `text/uri-list` first, then
`text/plain;charset=utf-8`, then `text/plain`. The only surface that acts on a drop is the desktop,
and on the desktop only the trash accepts one. Letting a drop land on a folder means first deciding
what a move should do when it half-succeeds across filesystems.

### A rig pass on the window model

`libkwm` reproduces the 106 rows of its contract fixture (`testing/fixtures/wm/geometry.txt`), and
`kdos-comp` calls eight of its entries: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`,
`kwm_ws_adjacent`, `kwm_edge_check`, `kwm_edge_best`, `kwm_clip_add` and `kwm_clip_sub`. No
photograph shows a window landing where a person expects. What would close it is one rig run on the
ISO: open a terminal, tile a pair, switch a workspace, lock and unlock, come back from `tty2`, and
start a boxed graphical application. See [The window model](../03-architecture/window-model.md).

### Something that reads the desktop

Every widget states what it is, what it is called and what it is set to through `ktui_announce()`,
and the record can be read back with `ktui_announce_count()` and `ktui_announce_at()`. Outside the
self-test, nothing reads it. This is the largest single absence in KDOS, and it is a missing
subsystem rather than a missing feature; see
[Accessibility](../02-user-guide/accessibility.md#what-would-have-to-change) for what would have to
change.

### A greeter, or a decision not to have one

`/etc/inittab` runs `kdos-getty` on `tty1`, which hands the terminal to `kdos-login`, which starts
`agetty` with `--autologin` for the account named in `/etc/kdos/login.conf`. The desktop is started
by the login shell's profile. There is no graphical account chooser and no session chooser. A
greeter would have to put a display on the screen before any account is logged in, which means a
compositor or a console surface of its own ahead of the session.

### Per-output rendering settings

`chrome_font` and `panel_font` in `~/.config/kdos/comp.conf` are one value each for every output.
The compositor advertises `wp_fractional_scale_manager_v1`, so a client that asks is told a
fractional scale, but KDOS's own surfaces render at the output's integer scale
(`wl_surface_set_buffer_scale` in `libkwl`) and do not take a fractional one. Both matter on a
machine with two displays of different densities, and neither is a small change: the toolkit
renders glyphs at an integer scale by construction. One font for every output is a deliberate
narrowing; see [Decisions](../01-philosophy/decisions.md#narrowings).

### Filling the second root slot

The A/B state machine is complete, and `kdos update apply` installs package updates into the mounted
inactive slot, deploys that slot's kernel and marks it to try. What is missing is the step before
that: creating and first populating slot B, which is done by hand (see
[Known gaps](known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot)), and any source
of whole-system images rather than package-by-package updates. A second encrypted slot has never
been booted.

### A reference binary host

A *binary host* is a published directory of prebuilt, signed packages. The signing, the index, the
three [equality tests](../03-architecture/packaging.md#the-binary-host) that decide whether a
prebuilt package is usable, and deltas all work and are asserted against a synthetic port. What
does not exist is a public binary host, which is a hosting and key-custody question rather than a
code one.

### Running the experimental programs against real use

Two programs are complete enough to test and have not been tested against the thing they exist for.
`kdos-term`'s state machine replays streams recorded from `vim`, `htop`, `mc`, `less` and `tmux`,
but the terminal has not been used interactively with them, or with `lf`, which is why the
compositor's terminal key (Super+Return, written `W-Return` in `~/.config/kdos-comp/rc.xml`) opens
`foot`. `kdos-ime` answers the kimpanel protocol (the generic D-Bus interface through which an
input-method framework hands its candidates to a panel that draws them) in the shapes fcitx5 sends,
and the `fcitx5` port (5.1.22) ships on the image, but fcitx5 has not been run against it.

### A key-card reader for helix

The second page of the [keybinding card](../04-programs/kdos-shell.md#kdos-keys) shows the focused
program's own keys, and has one reader each for `tmux`, `mc` and `micro`
(`src/desktop/kdos-shell/progkeys.c`). A reader for the `helix` editor would come with that port
reaching the image: the recipe exists (`ports/core/helix`, 25.07.1), but no `packages.txt` names
it, so it is one of the ports listed under
[Not installed](ports-catalogue.md#not-installed) and a reader for it could neither run nor be
checked.

### More reference frames, and lifting the skips

Fourteen of `kdos-shell`'s 52 surfaces have no committed reference frame, and three of those,
`kdos-ascii`, `kdos-ime` and `kdos-mediad`, have no offscreen dump to take one with. The self-test
compiles KDOS's own source files in the compositor (`src/desktop/kdos-comp/src/kdos-*.c`) and
`kdos-shell` only where their dependencies (among them wlroots, sd-bus, ALSA, PipeWire and the
Wayland protocol files) are installed, and skips them on a bare host with a line naming the skip. A
surface with no reference frame can change its drawing unnoticed, and a skipped compile can break
unnoticed on a host without those dependencies. See [Testing](../05-developer/testing.md).

## Not planned

Requests made often enough to be worth answering, with where the reasoning is.

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
| Telemetry of any kind | Nothing in KDOS reports anything anywhere |

## See also

- [Status](status.md) — what exists, and how mature it is
- [Known gaps](known-gaps.md) — what does not exist
- [Decisions](../01-philosophy/decisions.md) — the arguments behind what is not planned
- [The build system](../05-developer/build-system.md) — the orchestrator the mobile target reuses
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the x86-64 build, phase by phase, that
  the mobile skeleton mirrors
- [The ports catalogue](ports-catalogue.md) — every port by phase and group, including the ones no
  list installs
- [Why KDOS](../01-philosophy/why-kdos.md) — the properties any future work has to preserve
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — the choices behind the answers under
  Not planned

<!-- book-nav -->
---

*Part VI — Reference, chapter 44.* Previous: [43. Known gaps](known-gaps.md) · [Contents](../README.md) · Next: [45. Status](status.md)
