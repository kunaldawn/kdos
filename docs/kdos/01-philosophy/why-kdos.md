# Why KDOS

KDOS is a source-built Linux distribution with a desktop of its own and an application catalogue
that lives in containers. It is small enough that one person can read all of it, and it is built
so that the machine running it can rebuild it.

That combination is unusual, and it is deliberate. Most distributions choose between being
understandable and being useful: a system you can read end to end tends to stop at a shell prompt,
and a system with a browser, a CAD package and a video editor tends to be too large to hold in
your head. KDOS draws a line instead of choosing a side. Everything inside the line is compiled
here from upstream source and is meant to be read. Everything outside it is somebody else's
packaging, run in a container, and is meant to work.

Four properties follow from that line, and the rest of this documentation follows from the four.
A decision elsewhere in the book that looks arbitrary is usually one of these being paid for.

## Built from source, with named exceptions

The host is compiled in this repository from upstream tarballs by 877 recipes — 853 under
`ports/core` for upstream software, 24 under `src/` for the desktop, the daemons and the tools —
running from a cross toolchain, through a musl userland, through a self-hosting pass, through the
build tools, the libraries, the desktop and finally the kernel.
There is no base image underneath it and no binary archive to fall back on. The finished system
carries 833 installed packages.

A claim like that is worth nothing without its exceptions, so [the exceptions are listed in
full](#what-is-not-built-from-source) rather than glossed over: vendor firmware, two bootstrap
compilers, seven font sets, some vendored artwork, one third-party C source set, and the Debian
packages that make up the application catalogue.

## KDOS can build KDOS

Phase 2 of the build is a genuine self-hosting pass. Inside the chroot, the system rebuilds `tar`,
`musl`, `zlib`, `binutils`, `diffutils`, `m4`, `gawk` and `gcc` using the toolchain it built in
phase 1 — the compiler compiles itself, and what phase 3 goes on to build is built by a compiler
that KDOS produced.

The property survives onto the shipped image. A running KDOS carries gcc, binutils, rust, cmake,
meson, ninja, python3, make and `kpkg`, which is the full set needed to rebuild every port in the
tree, the compiler and the kernel included.
[`kdos rebuild`](../04-programs/kdos-command.md) does exactly that from the sources on the
installation medium, with no network at any point.

This is what makes the distribution self-contained in a way a source-based distribution usually is
not. A system that can only be rebuilt by the machine that first built it has a single point of
failure sitting outside itself.

## The repository builds offline

`make build` runs its container with `--network none`. That is enforcement, not intention: a
recipe that reaches out fails immediately and visibly on the machine that added it, rather than
working there and failing everywhere else a year later.

Every upstream tarball and every vendored dependency bundle is reachable at build time because
each is in the tree, held by Git LFS — 1,025 objects, about 8.3 GB. A clone is therefore the
entire input to a build. Nothing is fetched in between and nothing can be missing.

The `sha256 =` line in a recipe is what verifies an archive, and it sits beside the bytes it
describes. A hash with nothing to hash is a promise nobody can check, and a build that reaches the
internet stops reproducing the day a URL rots. The cost of holding the archives this way is real
and is written up in [Decisions](decisions.md#the-tarballs-are-in-the-tree-through-git-lfs).

## Applications live in boxes

KDOS builds the desktop. It does not native-port Firefox, LibreOffice or Blender, and it will not.

The outer ring is a catalogue of 183 applications over 7 shared runtimes, each declared as a chain
of Debian packages rather than shipped as bytes, and each built by podman on the machine that asks
for one. An installed application runs in its own rootless container and behaves like ordinary
system software: it appears in the launcher, registers as a MIME handler, takes the desktop's
theme, and can be run from a terminal. Thirty-six `cmd` rows across 25 applications name commands
rather than applications — solvers, converters and toolchains driven from a prompt — and get no
launcher entry, because a program with no window has nothing for a launcher to open.

The boundary sits exactly where the build cost is. A browser is tens of millions of lines whose
packaging is a full-time job for other people; a panel is not. Putting the first group in
containers is what keeps the second group small enough to compile from source in one sitting.

## What is not built from source

Each entry here was checked against the tree.

### Vendor firmware and microcode

No source exists to build, so these four ports ship binaries.

| Port | Version | Payload | What it is |
|---|---|---|---|
| `linux-firmware` | 20260810 | 619 MB | Upstream's complete tree, unpruned, installed with upstream's own `copy-firmware.sh --zstd`, which creates the alias symlinks a plain copy omits |
| `intel-ucode` | 20260811 | 17 MB | Upstream's whole Intel microcode set, concatenated into one bundle that rides in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2025.01.1 | 10 MB | Intel SOF audio DSP firmware and topologies. Not part of `linux-firmware`; Tiger Lake and newer are silent without it |
| `wireless-regdb` | 2025.07.10 | 31 KB | The wireless regulatory database. It must ship prebuilt: the kernel sets `CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y` and verifies upstream's signature, so a locally regenerated database is rejected in silence |

The firmware tree ships whole rather than curated. Pruning it is a bet on which hardware the
machine turns out to have, and losing that bet is silent — `request_firmware()` finds nothing and
the device does not work, which reads as broken hardware rather than as a missing file.

### Two bootstrap compilers

Rust and Go are each written in themselves, so building either needs a working one first.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.0 | 150 MB of upstream 1.97.1 stage-0 binaries — `rustc` (101 MB), `rust-std` (37 MB) and `cargo` (11 MB) — beside the 233 MB source |
| `go` | 1.27.0 | 57 MB of upstream 1.25.9 bootstrap toolchain beside the 33 MB source |

Both bootstraps are pinned by version and sha256 like every other source, so the offline build
still holds. Everything the bootstraps produce — the shipped `rustc`, `cargo` and `go`, and every
Rust and Go program in the tree — is compiled here.

### Seven prebuilt font sets

`noto-fonts` (2.015), `noto-cjk` (2.004), `noto-emoji` (2.051), `noto-fonts-extra`,
`nerd-fonts-symbols` (3.5.1), `ttf-dejavu` (2.37) and `terminus-ttf` (4.49.3) ship as built faces
because upstream publishes them that way. Each recipe unpacks an archive and installs the `.ttf`,
`.ttc` or `.otf` files in it; none runs a font compiler. A font is drawn rather than compiled, and
regenerating one from its sources would produce different outlines.

The console font is a separate port and is built from source: `terminus-font` (4.49.1) goes from
BDF through `configure` and `make` into the PSF that
[`kdos-getty`](../03-architecture/boot-and-init.md) loads.

### Vendored artwork, remade rather than redrawn

`kdos-icons` (pruned Papirus SVG), `kdos-cursors` (Bibata Xcursor binaries) and `kdos-gtk-theme`
(adw-gtk3) are upstream assets committed under `src/packages/` and recoloured at build time by
generators beside them. The palette is this project's; the shapes are not. Each carries a
`LICENSE.notice` recording exactly what was changed. See [Theming](../02-user-guide/theming.md).

### One vendored third-party source set

Monocypher 4.0.3, in `src/libs/libksig/monocypher/` — four files of C99 providing Ed25519,
dual-licensed BSD-2-Clause and CC0, compiled here like everything else. It is the only code under
`src/` that was not written here, and [`libksig`](../05-developer/c-libraries.md) exists to wrap it.

### The application catalogue is Debian

The 183 applications and the runtimes beneath them are Debian trixie packages, built on
`debian:trixie-slim`. Nothing inside them is compiled by this repository and nothing is carried on
the medium: the catalogue records which packages an application is, and podman builds it on the
machine that asks. See [Packs and boxes](../03-architecture/packs-and-boxes.md).

### Data files are data

`ca-certificates`, `iana-etc`, `hwdata`, `xkeyboard-config`, `iso-codes` and `docbook-xml`/`xsl`
are text or tables installed as they arrive. One is not purely text: `alsa-ucm-conf` carries a
small number of binary `.bin` files, precomputed EQ coefficients for SOF DSPs, which belong with
the firmware group in kind if not in size.

Everything else on the host — every library, every daemon, the compiler, the kernel and the whole
desktop — is compiled here from a tarball whose URL and sha256 are in this repository.

## Who should run KDOS

KDOS assumes a reader who is comfortable with a build log, a package recipe, and the C that draws
the panel. It ships one user account, no first-boot wizard, no telemetry, and no configuration
layer between you and the file that takes effect.

It is worth your time if you want to change the desktop, the package manager or the installer
rather than configure around them; if you want a workstation whose applications are containerised
by default; or if you want to keep an offline, reproducible source for the exact system you are
running.

It is the wrong choice if you need broad hardware enablement, a large binary archive, commercial
support, or a system that stays out of your way. There is no vendor to escalate to and no
third-party repository to fall back on.

## The trade

You get a system that is inspectable end to end and rebuildable from the machine itself, offline,
with reproducible packages. Anything wrong is wrong somewhere you can read.

In exchange you become the integrator. When a port needs a version bump, you bump it. When a build
fails on a toolchain change, you read the log and fix the recipe. When hardware needs a kernel
option, you set it and rebuild. That work is the price of the paragraph above it, and this
documentation exists to make it tractable — start with
[Writing ports](../05-developer/writing-ports.md) and
[Build troubleshooting](../05-developer/build-troubleshooting.md).

## What is deliberately absent

Each of these is a decision with an argument behind it. The arguments are in
[Decisions](decisions.md); the rules they generate are in [Principles](principles.md).

| Absent | Instead |
|---|---|
| systemd | `seatd`, `basu`, `eudev`, `dbus`, `dnsmasq`, and init scripts run by `ksvc` |
| An Xorg server | Wayland only, with rootless Xwayland as the single carve-out for X11 clients |
| GTK and Qt on the host | Every KDOS surface drawn as a character-cell grid by libraries written for it, with the compositor's own chrome in pango at the cell size |
| A display manager | `kdos-desktop`, started by hand from a tty |
| A first-boot wizard | The installer asks its questions once, then the system is yours |
| A vendor app store | `kdos-store` lists a catalogue held in this repository and builds from Debian; there is no account and nothing to sign up to |
| Telemetry | Nothing reports anything anywhere |
| A binary package archive | Ports built here, with an optional signed binhost you run yourself |

## The system in numbers

Measured from the tree.

| | |
|---|---|
| Port recipes in `ports/core` | 853 |
| Port recipes under `src/` for KDOS's own software | 24 |
| Packages installed on the built system | 833 |
| Applications in the catalogue | 183 |
| Shared runtimes beneath them | 7 |
| Catalogue groups offered by the installer and the store | 7 |
| Base packs | 2 |
| Data packs | 2 |
| Boxed commands with no graphical launcher | 36 rows across 25 applications |
| Kernel | 7.0.10 |
| C libraries written for this system | 17 |
| Upstream archives held in Git LFS | 1,025 objects, about 8.3 GB |

## See also

- [Principles](principles.md) — the rules these four properties generate
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces fit together
- [Build system](../05-developer/build-system.md) — the phases described above, in detail
- [Status](../06-reference/status.md) — what is mature and what is not
