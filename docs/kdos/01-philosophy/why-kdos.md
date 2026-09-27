# Why KDOS

This chapter says what KDOS is, how the system is shaped, which four properties define it and who
it is for. It is written for anyone deciding whether to try KDOS, install it or contribute to it,
and it assumes no knowledge of the source tree. It is the first chapter of the book; read it
before anything else. [How KDOS differs](how-kdos-differs.md) then compares KDOS, point by point,
with the ways other distributions solve the same problems, [Principles](principles.md) gives the
rules that follow from the ideas here, and [Decisions](decisions.md) records the choices that
were close and what lost.

## What KDOS is

KDOS is a Linux distribution for x86_64 machines, built from source, with a desktop of its own
and a catalogue of graphical applications that run in containers. The software written for it is
small enough for one person to read, and it still runs the large graphical applications people
expect.

Those two aims usually pull against each other. A system you can read end to end tends to stop at
a shell prompt; a system with a browser, a CAD package and a video editor tends to be too large
to hold in your head. KDOS draws a line between the two instead of choosing one:

- **Inside the line** is the host: the kernel, the C library, the services, the libraries and the
  desktop. All of it, apart from a short list of [named
  exceptions](#what-is-not-built-from-source) such as firmware, is compiled in this repository
  from upstream source or from source written here, and it is meant to be read.
- **Outside the line** are the graphical applications. They are other people's packaging, built
  on the machine that wants them and run in a container (a *box*), and they are meant to work.

The host is also built so that the machine running it can rebuild it. Four properties define the
system, each described in its own section below: the host is built from source, KDOS can build
KDOS, the repository builds offline, and applications live in boxes. When a decision elsewhere in
the book looks arbitrary, it is usually one of these four being paid for.

## The shape of the system

A *port* is a directory that describes how to build one package; its *recipe* is the files in it,
a declarative `kpkgbuild` and a `build.sh` beside it (see the
[Glossary](../06-reference/glossary.md#port)). `kpkg`, the package manager, builds and installs
ports.

The software on a KDOS machine falls into three *rings*, and the ring decides how each piece is
built and who maintains it:

| Ring | Where it is in the repository | What it holds | How it is built |
|---|---|---|---|
| Core | `ports/core/`, 1,014 recipes | musl, toybox, the toolchains, the libraries, the services, the kernel and its firmware | Compiled here from upstream source archives, each pinned by its sha256 |
| Desktop | `src/`, 24 recipes | The compositor, the panel, the terminal, the root daemons, the installer, the `kdos` command and the 17 C libraries they share | Compiled here from source written for KDOS; `kpkg` and the installer by two phase-1 scripts |
| Outer | `src/packages/kdos-appbox/catalogue` | 180 graphical and command-line applications over 7 shared runtimes | Declared as Debian packages and built by podman on the machine that asks for them |

The core ring starts from [musl](https://musl.libc.org/), the small C library KDOS uses in place
of glibc, and toybox, a single binary that provides the core commands. The 17 libraries of the
desktop ring have no recipes of their own; each program that uses one compiles it in.

A running machine is a short chain of programs with no systemd in it. The Limine boot loader starts
the kernel and an initramfs, a small early root filesystem whose one job is to find the real one;
toybox `init` runs the numbered scripts in `/etc/init.d/`, and `ksvc` supervises the long-running
services they start. `tty1` logs in the account that `/etc/kdos/login.conf` names, with no
password prompt on the live image, and that account's login profile starts `kdos-desktop`, which
starts the compositor, `kdos-comp`. The compositor in turn starts and supervises the panel,
`kdos-shell`. Every KDOS surface, the panel and the terminal included, is drawn as a grid of
character cells by the `libk*` libraries. An application from the catalogue runs in a container
under podman, a container runtime run as the ordinary user rather than as root, but appears on the
desktop as ordinary software.

[Architecture overview](../03-architecture/overview.md) draws that chain in full, names every
process on a working desktop and says where each kind of state is kept.

## Built from source, with named exceptions

The host is compiled in this repository from 1,038 recipes, 1,014 under `ports/core` for upstream
software and 24 under `src/` for the desktop, the daemons and the tools written for KDOS, and
from two scripts: one builds `kpkg` itself, which has no recipe, and the other builds the
installer, `kinstall`, whose recipe exists but is named in no phase list.

The build runs in eight phases. It starts with a cross toolchain, builds a base userland on musl,
rebuilds that userland's toolchain with itself, adds the compilers, runtimes and base libraries
(phase 3), then the rest of the userland (phase 4), then the desktop, then the kernel, and
finally packages the result into an ISO. The two scripts run in phase 1: `12_kpkg.sh` compiles
`kpkg` from `src/packages/kdos-kpkg`, and `13_kinstall.sh` compiles `kinstall` from
`src/packages/kdos-installer`, because nothing can be installed as a package until `kpkg` exists. The finished system has no base image beneath it and no
binary archive to fall back on: the container the build runs in, an Alpine 3.23 image with a host
compiler, supplies the tools that build the cross toolchain and the first userland, and nothing
from it is installed. Every recipe that a phase list (`script/*/packages.txt`) names is installed
on the finished system, together with everything those recipes depend on. That reaches all but
15 of the 1,014 upstream recipes; the other 15 stay in the tree unbuilt. A *phase* is defined in
the [Glossary](../06-reference/glossary.md). [How KDOS is
built](../05-developer/how-kdos-is-built.md) follows the build from `git clone` to a bootable ISO,
[The build system](../05-developer/build-system.md) describes the orchestrator that runs the
phases, and [The ports catalogue](../06-reference/ports-catalogue.md) lists every recipe by phase
and group.

The claim has exceptions, and they are [listed in full](#what-is-not-built-from-source) at the
end of this chapter. Four classes are exempt from building from source: firmware and code for
other processors, four compiler bootstrap seeds, compiled font data, and data that has no other
source form. Two further entries are listed so that the inventory is complete, though neither is
an exemption: recoloured upstream artwork (SVG icons, Xcursor images and a GTK theme), committed
here and processed at build time, and four pieces of third-party code under `src/`, which are
compiled here like everything else. The application catalogue sits outside the rule altogether,
because nothing in it runs on the host. The rule and its exempt classes are stated in
[Principles](principles.md#everything-that-runs-on-the-host-is-built-from-source).

## KDOS can build KDOS

Phase 2 of the build is a self-hosting pass. Inside the build chroot (the isolated root
filesystem the build runs in), the system rebuilds `tar`, `musl`, `zlib`, `binutils`,
`diffutils`, `m4`, `gawk` and `gcc`, together with the libraries those depend on (`xxhash`,
`gmp`, `mpfr`, `mpc`, `readline` and the `ncurses` under it), using the toolchain that phase 1
built. The chain behind that toolchain has three links. The Alpine 3.23 compilers of the build
image, the only C compilers in it that KDOS did not produce, build the phase 0 cross toolchain;
the cross toolchain builds phase 1's native `gcc` and `binutils`; and in phase 2 that native `gcc`
compiles a new `gcc`, with a new `musl` beneath it. Everything from phase 3 onwards is built by
the phase 2 compiler, which KDOS produced with a compiler of its own.

The toolchains survive onto the shipped image, because the build installs them rather than
removing them. A running KDOS carries `gcc`, `binutils`, `clang`, `rust`, `go`, `cmake`, `meson`,
`ninja`, `python3`, `make` and `kpkg`, so any single recipe, the compiler and the kernel included,
can be rebuilt on the machine itself and offline once its source is present, with `kpkg`.
`kdos update apply` brings every package that is behind its recipe up to date, compiling it on the
machine when no [binhost](../06-reference/glossary.md) supplies it. [How KDOS
differs](how-kdos-differs.md#self-hosting) compares this with other distributions.

A full rebuild of the system needs a build machine. [`kdos
rebuild`](../04-programs/kdos-command.md#kdos-rebuild) is the command meant to drive the whole
build from a booted medium, with no network at any point, but on a booted machine it does not
complete:

```sh
kdos rebuild /mnt/disk/rebuild             # every phase
kdos rebuild --iso-only /mnt/disk/rebuild  # the packaging phase only
kdos rebuild --dry-run /mnt/disk/rebuild   # report the plan and stop
```

The default ISO carries no source tree. A developer medium carries part of one with an empty
`ports/`, and `kdos rebuild` finds no ports there and stops before copying anything. With a
complete tree named by `KDOS_SOURCES` it still stops in phase 1, because that phase reads the `fs/`
overlay from `/workspace/fs`. [The kdos command](../04-programs/kdos-command.md#kdos-rebuild) says
where the command looks for the tree and what it checks before it starts.

## The repository builds offline

`make build` runs its build container with `--network none`. A recipe that tries to download
something therefore fails at once on the machine where it was written, rather than working there
and failing on every other machine later.

Every upstream tarball and every vendored dependency bundle is in its port directory before the
build starts, because `make fetch` put it there. Apart from the first `docker build` of the
build container itself, `make fetch` is the only step of a build that uses the network. Run it
once after cloning and again whenever a recipe changes; it is safe to re-run, and it reports
anything it could not obtain before the build starts. `make fetch-check` verifies what is already
on disk without touching the network.

```sh
make fetch        # the one networked step
make fetch-check  # offline: what is missing or corrupt
make build        # no network
```

A recipe's `sha256 =` line is what verifies a source file, wherever the file came from;
[Writing ports](../05-developer/writing-ports.md#sources-and-what-each-one-becomes) says where
`make fetch` looks for each one. The recipes name 1,232 distinct files, about 8.3 GiB in
all. The 40 small ones that git carries itself, such as patches, stay in the repository; the
other 1,192 are held in the source archive under their own sha256, located through the committed
index `ports/sources.idx`. The archive is append-only, so a recipe keeps building after its
upstream URL disappears. Why the sources are held this way, and what it costs, is in
[Decisions](decisions.md#upstream-archives-are-content-addressed-release-assets).

## Applications live in boxes

KDOS builds the desktop. It does not port Firefox, LibreOffice or Blender to the host.

The outer ring is a catalogue of 180 applications. Each is declared as a chain of packages rather
than shipped as bytes: an application row sits on one of 7 shared runtimes (GTK, Qt, KDE, media,
scientific Python, Electron and Wine) or directly on the base, and podman builds the chain on
the machine that asks for it, storing each runtime once however many applications share it. An
installed application runs in its own rootless container and behaves like ordinary system
software: it appears in the launcher, registers as a handler for its file types, takes the
desktop's GTK theme, icons and cursors, and can be started from a terminal by name. A Qt
application built by the store draws in Qt's defaults; see
[Theming](../02-user-guide/theming.md#theming-applications-inside-boxes).

Some catalogue entries are commands rather than windows. Thirty-three `cmd` rows across 22
applications expose solvers, converters, emulators and toolchains to be run from a prompt. They
get no launcher entry, because a program with no window has nothing for a launcher to open.

The boundary sits where the build cost is. A browser is tens of millions of lines whose packaging
is a full-time job for other people; a panel is not. Putting the first kind in containers keeps
the second kind small enough to compile from source in one build.
[Packs and boxes](../03-architecture/packs-and-boxes.md) describes the mechanism, and
[Applications](../02-user-guide/applications.md) describes installing and running them.

## Who should run KDOS

KDOS assumes a reader who is comfortable with a build log, a package recipe, and the C that draws
the panel. The image ships one user account, `kdos` (the installer lets you rename it), no
first-boot wizard, no telemetry, and no configuration layer between you and the file that takes
effect.

It suits you if:

- you want to change the desktop, the package manager or the installer rather than configure
  around them;
- you want a workstation whose applications are containerised by default; or
- you want to keep an offline, reproducible source for the exact system you are running.

It is the wrong choice if you need broad hardware enablement, a large binary archive, commercial
support, or a system that stays out of your way. There is no vendor to escalate to and no
third-party repository to fall back on.

## The trade

You get a system that is inspectable end to end and rebuildable on the machine itself, offline,
with [reproducible packages](../03-architecture/packaging.md#reproducible-packages). When
something is wrong, the cause is in source you can read.

In exchange you become the integrator. When a port needs a version bump, you bump it. When a build
fails after a toolchain change, you read the log and fix the recipe. When hardware needs a kernel
option, you set it and rebuild. That work is what the properties above cost, and the developer
chapters of this book cover it. Start with [Writing ports](../05-developer/writing-ports.md) and
[Build troubleshooting](../05-developer/build-troubleshooting.md).

## What is deliberately absent

Each of these is a decision with an argument behind it. The arguments are in
[Decisions](decisions.md); the rules they produce are in [Principles](principles.md).

| Absent | Instead |
|---|---|
| systemd | toybox `init`, numbered scripts in `/etc/init.d/`, and `ksvc` to supervise services; for the rest, `seatd` (device access for the session), `basu` (the sd-bus library on its own), `eudev` (device events), `dbus` (the message bus) and `dnsmasq` (the local DNS cache) |
| An Xorg server | Wayland only, with Xwayland, the X server that runs as a Wayland client, as the single carve-out for X11 clients |
| GTK and Qt on the host | Every KDOS surface drawn as a character-cell grid by libraries written for it; the compositor draws its own titlebars, root menu and window switcher with pango, the text layout library, at a size matched to the grid |
| A display manager | `tty1` logs in the account `/etc/kdos/login.conf` names, with no password prompt on the live image (`kdos`); an installed machine asks for a password unless automatic login was chosen (`autologin = <user>` in that file). That login shell's profile starts `kdos-desktop` |
| A first-boot wizard | The installer asks its questions once, then the system is yours |
| A vendor app store | `kdos-store` lists the catalogue held in this repository and builds from it; there is no account and nothing to sign up to |
| Telemetry | No KDOS program sends usage data; boxed applications behave as their Debian packages do |
| A binary package archive | Ports built here, with an optional signed [binhost](../06-reference/glossary.md) (a package server) you run yourself |

## The system in numbers

Measured from the tree: recipes by counting `kpkgbuild` files, reachability by following the phase
lists and every `depends =` line, and the catalogue by counting its rows by kind.

| | |
|---|---|
| Port recipes in `ports/core` | 1,014 |
| Port recipes under `src/` for KDOS's own software | 24 (13 in `src/desktop`, 11 in `src/packages`) |
| Upstream recipes that no phase list or dependency reaches, and so are not built | 15 |
| C libraries written for this system, under `src/libs` | 17, one of them (`libkvt`) a fork of libtsm |
| Kernel | Linux 7.2.7 |
| Applications in the catalogue | 180 |
| Shared runtimes beneath them | 7 |
| Catalogue bases | 2 (Debian trixie and Alpine 3.24.1) |
| Catalogue data sets | 2 |
| Catalogue groups offered by the installer and the store | 7 |
| Boxed commands with no graphical launcher | 33 rows across 22 applications |
| Distinct source files the recipes name | 1,232, about 8.3 GiB (1,192 in the source archive; 40 carried in git) |

## What is not built from source

This section is the complete inventory of what the host installs without compiling it here. Each
entry is checked against the recipes. A port that carries a prebuilt file has to fit one of these
classes.

### Firmware and code for another processor

Code that runs on a DSP, a GPU, a microcontroller or a virtual machine guest, rather than on the
host processor, is shipped as upstream built it. For most of it no source is published. Three
ports are nothing else:

| Port | Version | Download | What it is |
|---|---|---|---|
| `linux-firmware` | 20260916 | 632 MiB | Upstream's complete tree, unpruned, installed with upstream's own `copy-firmware.sh --zstd`, which creates the alias symlinks a plain copy omits |
| `intel-ucode` | 20260812 | 17 MiB | Upstream's whole Intel microcode set, concatenated into one bundle that sits in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2026.09.1 | 17 MiB | Intel SOF audio DSP firmware and topologies. They are not part of `linux-firmware`, and Tiger Lake and newer machines have no sound without them |

The firmware tree ships whole rather than curated. Pruning it is a bet on which hardware the
machine turns out to have, and losing that bet is silent: the kernel's `request_firmware()` finds
nothing and the device does not work, which looks like broken hardware rather than a missing file.

`wireless-regdb` (2026.09.03) is not in this class. Its `regulatory.db` is generated here from
upstream's `db.txt`, and the only binary taken from the tarball is upstream's detached signature,
`regulatory.db.p7s`. The kernel is built with `CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y` and loads
the database only when that signature verifies, so the build checks the signature against the
generated file with upstream's certificate and fails on a mismatch. What ships is byte-for-byte the
database upstream signed, or nothing.

The rest of this class rides inside ports that are otherwise compiled here:

| Port | Prebuilt payload |
|---|---|
| `intel-media-driver` | The closed EU kernels, compiled in with `ENABLE_KERNELS=ON` and `BUILD_KERNELS=OFF`. Rebuilding them from their assembly needs Intel's shader compiler, which is not a port |
| `libva-intel-driver` | The i965 driver's GPU shader kernels: the assembled `.g4b`–`.g10b` files in `src/shaders` that the driver includes as headers. Their assembly is in the tarball, and reassembling it needs `intel-gen4asm`, which is not a port |
| `alsa-ucm-conf` | 18 `.bin` files under `ucm2/blobs/sof/`: precomputed equaliser, dynamic-range and beamformer coefficients loaded into SOF DSPs |
| `espflash`, `probe-rs`, `python3-esptool`, `openfpgaloader` | The flasher stubs, flash algorithms and bridge bitstreams each one uploads to the device it drives |
| `qemu` | EDK2 for the riscv64 `virt` machine, `edk2-riscv-code.fd` and its variable store, unpacked from the tarball's `pc-bios/`, because a build of it with a riscv64 bare-metal gcc 15 faults before reaching a boot option. The rest of the guest firmware it installs (SeaBIOS and SeaVGABIOS, qboot, the iPXE NIC ROMs, the `-kernel` option ROMs, EDK2 for x86_64 and aarch64, and OpenSBI) is compiled here from the sources in the tarball's `roms/` |

### Four compiler bootstrap seeds

Rust, Go, Zig and Haskell's GHC are each written in themselves, so building any of them needs a
working copy first. That copy is the *seed*.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.1 | 150 MiB of upstream 1.97.1 stage-0 binaries (`rustc` 101 MiB, `rust-std` 37 MiB and `cargo` 11 MiB) beside the 233 MiB source |
| `go` | 1.27.1 | 57 MiB of upstream 1.25.9 bootstrap toolchain beside the 33 MiB source |
| `zig` | 0.16.0 | `stage1/zig1.wasm`, inside the source tarball: a WebAssembly build of the compiler that the build translates to C and compiles to start its own bootstrap |
| `ghc` | 9.12.4 | 239 MiB of upstream GHC 9.10.3 for musl, the statically linked Alpine build, beside the 32 MiB source. It compiles Hadrian, GHC's build system, and GHC's first stage |

Every seed is pinned by version and sha256 like every other source, so the offline build still
holds, and no seed is installed. Everything the seeds produce is compiled here: the shipped
`rustc`, `cargo`, `go`, `zig`, `ghc` and `cabal`, and every Rust, Go, Zig and Haskell program in
the tree. Go's source tarball also carries upstream-compiled objects, the race detector's runtime
and the BoringCrypto module, and the `go` port deletes them from what it installs rather than ship
binaries it did not build.

### Compiled font data

`noto-fonts` (2.015), `noto-cjk` (2.004), `noto-fonts-extra` and `nerd-fonts-symbols` (3.5.1)
ship as built font files because upstream publishes them that way, and the sources behind them
compile through toolchains this tree does not carry: fontmake and gftools for Noto, AFDKO for Noto
CJK, and the Nerd Fonts patcher and its icon sets for the symbols. Each recipe unpacks an archive
and installs the `.ttf`, `.ttc` or `.otf` files in it. The same holds for the fonts bundled inside
`mupdf`, `matplotlib` and `seqkit` (the last through its Go vendor bundle), which are installed or
compiled in as their upstreams ship them.

Three fonts are compiled here, each by its upstream's own pipeline:

| Port | From | Through |
|---|---|---|
| `ttf-dejavu` (2.37) | The FontForge sources, `src/*.sfd` | Upstream's `make full-ttf`: `fontforge` writes each face and `ttpostproc.pl` finishes its tables through `perl-font-ttf` |
| `terminus-ttf` (4.49.3) | `terminus-font` 4.49.1's BDF sources | mkttf: `mkitalic` slants the BDFs, `fontforge` gathers every size as a bitmap strike and traces the largest into outlines with `potrace` |
| `noto-emoji` (2.051) | The 128-pixel PNG artwork and the region flags | Upstream's `Makefile` for the CBDT face `NotoColorEmoji.ttf`: `waveflag`, ImageMagick, `pngquant`, `zopflipng`, and the builder scripts on `fonttools` and nototools |

`fontforge` is built without its GUI, so it brings no GTK to the host. `fontforge` stamps
`SOURCE_DATE_EPOCH` into every face it writes, and `terminus-ttf` takes the year in its copyright
notice from the same variable, so two builds of one recipe produce the same file. `noto-emoji`
ships the bitmap face only; the COLRv1 face is built by nanoemoji, which is not a port.

The console font is a separate port and is built from source: `terminus-font` (4.49.1) goes from
BDF through `configure` and `make` into the PSF that
[`kdos-getty`](../03-architecture/boot-and-init.md) loads.

### Data with no other source form

`hwdata`, `iso-codes`, `docbook-xml` and `docbook-xsl` are text or tables installed as they arrive.
`xkeyboard-config` is text upstream too, and its own meson build assembles the rules files from
upstream's fragments and compiles the translations. `iana-etc` is tables too, but generated at
build time from IANA's own XML registries, each pinned by its hash, rather than taken as text
somebody else produced. `ca-certificates` is generated the same way, from the `certdata.txt` of a
pinned NSS release. The time-zone rules `nodejs` compiles into its `Temporal` support are the
`zoneinfo64.res` that the `icu` port builds from its own source, in place of the copy inside the
tarball's vendored `zoneinfo64` crate.

Some data is binary or generated upstream, and the file as shipped is the form upstream maintains.
There is nothing earlier to build it from:

| Port | Data |
|---|---|
| `tesseract` | `eng.traineddata`, `tessdata_fast`'s English model, a second `source =` line |
| `perl-xml-parser` | The `.enc` encoding maps under `share/` |
| `libkiwix` | The JavaScript of the server's skin under `static/`, minified files included |
| `fcitx5-chinese-addons` | The pinyin-to-character and stroke tables, two further `source =` lines |
| `john` | The `.chr` character-frequency files |
| `picotool` | The last 512 bytes of each RP2350 revision's boot ROM, under `model/`. A connected chip will not read them out, so picotool supplies them when it dumps the ROM. They are a copy of the mask ROM, not a build |
| `alsa-utils` | Recorded audio: the channel-name voice samples `speaker-test` plays |
| `kdos-tools` | `/usr/share/kdos/secdb.txt`, Alpine's security database pruned into one table, committed under `src/packages/kdos-tools/secdb/` and regenerated by hand with the `vendor.py` beside it |

### Vendored artwork, recoloured rather than redrawn

`kdos-icons` (a pruned set of Papirus SVG icons), `kdos-cursors` (Bibata Xcursor images) and
`kdos-gtk-theme` (adw-gtk3, for the GTK applications in boxes) are upstream assets committed under
`src/packages/` and recoloured to the KDOS palette at build time. The palette is this project's;
the shapes are not. Each package carries a `LICENSE.notice` recording exactly what was changed,
and an `UPSTREAM` file naming the release it was taken from. See
[Theming](../02-user-guide/theming.md).

### Third-party code under `src/`

Most of `src/` is written for KDOS. Four pieces are not, and each is compiled here like everything
else:

| Where | What |
|---|---|
| `src/libs/libksig/monocypher/` | Monocypher 4.0.3, four files of C99 providing Ed25519, dual-licensed BSD-2-Clause and CC0, vendored verbatim. [`libksig`](../05-developer/c-libraries.md) exists to wrap it |
| `src/desktop/kdos-comp` | A frozen hard fork of the labwc 0.20.0 compositor; see [Decisions](decisions.md#the-compositor-is-a-frozen-fork-of-labwc) |
| `src/libs/libkvt` | A hard fork of the libtsm 4.7.1 terminal state machine; see [Decisions](decisions.md#forking-libtsm-rather-than-writing-a-terminal) |
| `src/packages/kdos-bb` | A frozen hard fork of the AA-project's `bb` 1.3rc1 demo; see [Decisions](decisions.md#freezing-a-demo-rather-than-writing-one) |

### The application catalogue is Debian

The 180 applications, the 7 runtimes and the default base beneath them are Debian trixie
packages, built on `debian:trixie-slim`, with one exception: VSCodium comes from the `.deb` its
upstream publishes on GitHub, fetched by a `deb` row in the catalogue and handed to apt so that its
dependencies resolve from Debian. The catalogue also carries a second, empty base on
`alpine:3.24.1`, a scratch userland for trying something in a box. Nothing in the catalogue is
compiled by this repository and none of it is carried on the installation medium: the catalogue
records which packages an application is made of, and podman builds it on the machine that asks.
See [Packs and boxes](../03-architecture/packs-and-boxes.md).

### Everything else

Everything else on the host, including every library, every daemon, the compiler, the kernel and
the whole desktop, is compiled here from a source file whose URL and sha256 are in this
repository. A prebuilt object for the host that a tarball carries and that fits none of these
classes is deleted from the package: `go` drops its race-detector runtime and BoringCrypto module,
and `john` drops its ZTEX FPGA bitstreams and controller image.

## See also

- [How KDOS differs](how-kdos-differs.md) — KDOS compared with other distributions, problem by
  problem: the C library, the core userland, init, the display stack, host toolkits, the desktop,
  packages, source pinning, self-hosting, reproducibility, configuration, applications, updates,
  security, and hardware and support
- [Principles](principles.md) — the rules these four properties produce
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces of a running system fit
  together
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build described above, from
  `git clone` to a bootable ISO
- [The build system](../05-developer/build-system.md) — the orchestrator that runs the phases
- [The ports catalogue](../06-reference/ports-catalogue.md) — every recipe, by phase and group
- [Status](../06-reference/status.md) — what is mature and what is not
- [Glossary](../06-reference/glossary.md) — the terms this book uses

<!-- book-nav -->
---

*Part I — Introduction, chapter 1.* [Contents](../README.md) · Next: [2. How KDOS differs](how-kdos-differs.md)
