# Why KDOS

This page explains what KDOS is, what it is trying to be, and whether it is worth your time. It is
written for anyone deciding whether to try KDOS, install it or contribute to it. No knowledge of
the source tree is assumed; the pages it links to go deeper.

Read it first. After it, [Principles](principles.md) gives the rules that follow from the ideas
here, and [Decisions](decisions.md) gives the choices that were close and what lost.

## What KDOS is

KDOS is a Linux distribution built from source, with a desktop of its own and a catalogue of
applications that run in containers. It is small enough that one person can read all of it, and
it is built so that the machine running it can rebuild it.

That combination is unusual, and it is deliberate. Most distributions choose between being
understandable and being useful. A system you can read end to end tends to stop at a shell
prompt; a system with a browser, a CAD package and a video editor tends to be too large to hold in
your head. KDOS draws a line instead of choosing a side:

- **Inside the line** is the host: the kernel, the libraries, the services and the desktop. All of
  it is compiled here from upstream source, and it is meant to be read.
- **Outside the line** are the graphical applications. They are somebody else's packaging, run in
  a container (a *box*), and they are meant to work.

Four properties follow from that line, and the rest of this documentation follows from the four.
When a decision elsewhere in the book looks arbitrary, it is usually one of these being paid for.

## Built from source, with named exceptions

The host is compiled in this repository from upstream source by 1,038 recipes. A *recipe* (also
called a *port*) is the small description of how to build one package: 1,014 of them live under
`ports/core` for upstream software, and 24 live under `src/` for the desktop, the daemons and the
tools written for KDOS.

The build runs in order from a cross toolchain, through a userland on
[musl](https://musl.libc.org/) (the small C library KDOS uses in place of glibc), through a self-hosting
pass, through the build tools and libraries, to the desktop and finally the kernel. There is no
base image underneath it and no binary archive to fall back on. Every recipe that the build's
[phase](../06-reference/glossary.md) lists name, together with everything those recipes depend on, is installed on the finished
system; that is all but 15 of the 1,014 upstream ports.

A claim like that is worth nothing without its exceptions, so [the exceptions are listed in
full](#what-is-not-built-from-source) below: firmware and code for other processors, compiled font
data, four compiler bootstrap seeds, data that has no other source form, recoloured upstream
artwork, and four pieces of third-party code under `src/` that are compiled here. Only the first
four are exemptions from building from source; the artwork is data and the third-party code is
compiled like everything else. The Debian packages that make up the application catalogue sit
outside the rule altogether, because they never run on the host. The rule and its four exempt
classes are stated in
[Principles](principles.md#everything-that-runs-on-the-host-is-built-from-source).

## KDOS can build KDOS

Phase 2 of the build is a genuine self-hosting pass. Inside the build chroot (the isolated root filesystem the build runs in), the system rebuilds
`tar`, `musl`, `zlib`, `binutils`, `diffutils`, `m4`, `gawk` and `gcc`, and the libraries those
depend on (`xxhash`, `gmp`, `mpfr`, `mpc`, `readline` and the `ncurses` under it), using the
toolchain that phase 1 built. The compiler compiles itself, so everything from phase 3 onwards is
built by a compiler that KDOS produced.

The property survives onto the shipped image, because the build installs its toolchains rather
than removing them. A running KDOS carries gcc, binutils, clang, rust, go, cmake, meson, ninja,
python3, make and `kpkg`, the package manager: the full set needed to rebuild every port in the
tree, the compiler and the kernel included.

[`kdos rebuild`](../04-programs/kdos-command.md) runs that rebuild on the machine itself, with no
network at any point. It needs a copy of the source tree. It uses the tree `KDOS_SOURCES` names when that is set, and
stops if that is not a KDOS tree; otherwise it tries `/mnt/iso/sources`, then `/kdos`, then the
current directory. It refuses to start with less than 25 GB free on real storage. The default ISO does not carry
the tree, because the upstream sources roughly double its size. A developer stick built with
`make build KDOS_ISO_SOURCES=1` does: it places `ports/`, `src/` and `script/` beside the system
image, so a booted stick can rebuild the stick.

This is what makes the distribution self-contained in a way a source-based distribution usually is
not. A system that can only be rebuilt by the machine that first built it has a single point of
failure sitting outside itself.

## The repository builds offline

`make build` runs its build container with `--network none`. That is enforcement, not intention:
a recipe that tries to download something fails immediately and visibly on the machine that added
it, rather than working there and failing everywhere else a year later.

Every upstream tarball and every vendored dependency bundle is in its port directory before the
build starts, because `make fetch` put it there. `make fetch` is the only step that uses the
network. You run it once after cloning and again whenever a recipe changes; it is safe to re-run,
and it reports anything it could not obtain before the build ever starts. `make fetch-check`
verifies what is already on disk without touching the network.

A recipe's `sha256 =` line is what verifies a source file, wherever it came from. `make fetch`
looks for each file in turn in the port directory, in the local cache `ports/.srccache/`, in the
KDOS source archive, and finally at the upstream URL the recipe names, and it keeps the first copy
whose hash matches. The recipes name 1,232 distinct files, about 8.3 GiB. The 40 small ones git
carries itself, such as patch files and registries, stay in the repository; the other 1,192 live in
the source archive under their own sha256, append-only. A recipe therefore keeps building after its upstream URL disappears.
Why the sources are held this way, and what it costs, is in
[Decisions](decisions.md#upstream-archives-are-content-addressed-release-assets).

## Applications live in boxes

KDOS builds the desktop. It does not port Firefox, LibreOffice or Blender to the host, and it will
not.

The outer ring is a catalogue of 180 applications over 7 shared runtimes. Each application is
declared as a chain of Debian packages rather than shipped as bytes, and podman builds it on the
machine that asks for it. An installed application runs in its own rootless container and behaves
like ordinary system software: it appears in the launcher, registers as a handler for its file
types, takes the desktop's theme, and can be started from a terminal.

Some catalogue entries are commands rather than windows. Thirty-three `cmd` rows across 22
applications expose solvers, converters and toolchains to be run from a prompt. They get no
launcher entry, because a program with no window has nothing for a launcher to open.

The boundary sits exactly where the build cost is. A browser is tens of millions of lines whose
packaging is a full-time job for other people; a panel is not. Putting the first group in
containers is what keeps the second group small enough to compile from source in one sitting.

## Who should run KDOS

KDOS assumes a reader who is comfortable with a build log, a package recipe, and the C that draws
the panel. The image ships one user account, `kdos` (the installer lets you rename it), no
first-boot wizard, no telemetry, and no configuration layer between you and the file that takes
effect.

It is worth your time if:

- you want to change the desktop, the package manager or the installer rather than configure
  around them;
- you want a workstation whose applications are containerised by default; or
- you want to keep an offline, reproducible source for the exact system you are running.

It is the wrong choice if you need broad hardware enablement, a large binary archive, commercial
support, or a system that stays out of your way. There is no vendor to escalate to and no
third-party repository to fall back on.

## The trade

You get a system that is inspectable end to end and rebuildable from the machine itself, offline,
with reproducible packages. Anything wrong is wrong somewhere you can read.

In exchange you become the integrator. When a port needs a version bump, you bump it. When a build
fails on a toolchain change, you read the log and fix the recipe. When hardware needs a kernel
option, you set it and rebuild. That work is the price of the paragraph above it, and this
documentation exists to make it manageable. Start with
[Writing ports](../05-developer/writing-ports.md) and
[Build troubleshooting](../05-developer/build-troubleshooting.md).

## What is deliberately absent

Each of these is a decision with an argument behind it. The arguments are in
[Decisions](decisions.md); the rules they produce are in [Principles](principles.md).

| Absent | Instead |
|---|---|
| systemd | `seatd`, `basu`, `eudev`, `dbus` and `dnsmasq`, with init scripts run by `ksvc` |
| An Xorg server | Wayland only, with rootless Xwayland as the single carve-out for X11 clients |
| GTK and Qt on the host | Every KDOS surface drawn as a character-cell grid by libraries written for it, with the compositor's own chrome drawn in pango at the cell size |
| A display manager | `tty1` logs in the account `/etc/kdos/login.conf` names (`kdos` by default, with no password prompt), and that login shell's profile starts `kdos-desktop`; comment the `autologin` line out to be asked for a password |
| A first-boot wizard | The installer asks its questions once, then the system is yours |
| A vendor app store | `kdos-store` lists a catalogue held in this repository and builds from Debian; there is no account and nothing to sign up to |
| Telemetry | Nothing reports anything anywhere |
| A binary package archive | Ports built here, with an optional signed [binhost](../06-reference/glossary.md) (a package server) you run yourself |

## What is not built from source

This section is the complete inventory of what the host installs without compiling it here. Each
entry was checked against the tree. If you are adding a port that carries a prebuilt file, this is
the list it has to fit into.

### Firmware and code for another processor

Code that runs on a DSP, a GPU, a microcontroller or a virtual machine guest, rather than on the
host processor, is shipped as upstream built it. For most of it no source is published. Three
ports are nothing else:

| Port | Version | Download | What it is |
|---|---|---|---|
| `linux-firmware` | 20260916 | 632 MiB | Upstream's complete tree, unpruned, installed with upstream's own `copy-firmware.sh --zstd`, which creates the alias symlinks a plain copy omits |
| `intel-ucode` | 20260812 | 17 MiB | Upstream's whole Intel microcode set, concatenated into one bundle that sits in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2026.09.1 | 17 MiB | Intel SOF audio DSP firmware and topologies. Not part of `linux-firmware`; Tiger Lake and newer machines have no sound without it |

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
| `alsa-ucm-conf` | A small number of `.bin` files: precomputed EQ coefficients loaded into SOF DSPs |
| `espflash`, `probe-rs`, `python3-esptool`, `openfpgaloader` | The flasher stubs, flash algorithms and bridge bitstreams each one uploads to the device it drives |
| `qemu` | EDK2 for the riscv64 `virt` machine, `edk2-riscv-code.fd` and its variable store, unpacked from the tarball's `pc-bios/`: compiled by a riscv64 bare-metal gcc 15 it faults before reaching a boot option. The rest of the guest firmware it installs — SeaBIOS and SeaVGABIOS, qboot, the iPXE NIC ROMs, the `-kernel` option ROMs, EDK2 for x86_64 and aarch64, and OpenSBI — is compiled here from the sources in the tarball's `roms/` |

### Four compiler bootstrap seeds

Rust, Go, Zig and Haskell's GHC are each written in themselves, so building any of them needs a
working copy first. That copy is the *seed*.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.1 | 150 MiB of upstream 1.97.1 stage-0 binaries — `rustc` (101 MiB), `rust-std` (37 MiB) and `cargo` (11 MiB) — beside the 233 MiB source |
| `go` | 1.27.1 | 57 MiB of upstream 1.25.9 bootstrap toolchain beside the 33 MiB source |
| `zig` | 0.16.0 | `stage1/zig1.wasm`, inside the source tarball: a WebAssembly build of the compiler that the build translates to C and compiles to start its own bootstrap |
| `ghc` | 9.12.4 | 239 MiB of upstream GHC 9.10.3 for musl, the statically linked Alpine build, beside the 32 MiB source. It compiles Hadrian, GHC's build system, and GHC's first stage |

Every seed is pinned by version and sha256 like every other source, so the offline build still
holds, and no seed is installed. Everything the seeds produce — the shipped `rustc`, `cargo`, `go`,
`zig`, `ghc` and `cabal`, and every Rust, Go, Zig and Haskell program in the tree — is compiled
here. Go's source tarball also carries upstream-compiled objects, the race detector's runtime and
the BoringCrypto module, and the `go` port deletes them from what it installs rather than ship
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

`hwdata`, `xkeyboard-config`, `iso-codes` and `docbook-xml`/`xsl` are text or tables installed as
they arrive. `iana-etc` is tables too, but generated at build time from IANA's own XML registries,
each pinned by its hash, rather than taken as text somebody else produced. `ca-certificates` is
generated the same way, from the `certdata.txt` of a pinned NSS release. The time-zone rules
`nodejs` compiles into its `Temporal` support are the `zoneinfo64.res` the `icu` port builds from
its own source, in place of the copy inside the tarball's vendored `zoneinfo64` crate.

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

### Vendored artwork, recoloured rather than redrawn

`kdos-icons` (a pruned set of Papirus SVG icons), `kdos-cursors` (Bibata Xcursor images) and
`kdos-gtk-theme` (adw-gtk3) are upstream assets committed under `src/packages/` and recoloured to
the KDOS palette at build time. The palette is this project's; the shapes are not. Each package
carries a `LICENSE.notice` recording exactly what was changed. See
[Theming](../02-user-guide/theming.md).

### Third-party code under `src/`

Most of `src/` is written for KDOS. Four pieces are not, and each is compiled here like everything
else:

| Where | What |
|---|---|
| `src/libs/libksig/monocypher/` | Monocypher 4.0.3, four files of C99 providing Ed25519, dual-licensed BSD-2-Clause and CC0, vendored verbatim. [`libksig`](../05-developer/c-libraries.md) exists to wrap it |
| `src/desktop/kdos-comp` | A frozen hard fork of the labwc 0.20.0 compositor — see [Decisions](decisions.md#the-compositor-is-a-frozen-fork-of-labwc) |
| `src/libs/libkvt` | A hard fork of the libtsm 4.7.1 terminal state machine — see [Decisions](decisions.md#forking-libtsm-rather-than-writing-a-terminal) |
| `src/packages/kdos-bb` | A frozen hard fork of the AA-project's `bb` 1.3rc1 demo — see [Decisions](decisions.md#freezing-a-demo-rather-than-writing-one) |

### The application catalogue is Debian

The 180 applications and the runtimes beneath them are Debian trixie packages, built on
`debian:trixie-slim`. Nothing inside them is compiled by this repository and nothing of them is
carried on the installation medium: the catalogue records which packages an application is made
of, and podman builds it on the machine that asks. See
[Packs and boxes](../03-architecture/packs-and-boxes.md).

### Everything else

Everything else on the host — every library, every daemon, the compiler, the kernel and the whole
desktop — is compiled here from a source file whose URL and sha256 are in this repository. A
prebuilt object for the host that a tarball carries and that fits none of these classes is deleted
from the package: `go` drops its race-detector runtime and BoringCrypto module, and `john` drops
`run/ztex`.

## The system in numbers

Measured from the tree.

| | |
|---|---|
| Port recipes in `ports/core` | 1,014 |
| Port recipes under `src/` for KDOS's own software | 24 |
| Upstream ports that no phase list or dependency reaches, and so are not built | 15 |
| Applications in the catalogue | 180 |
| Shared runtimes beneath them | 7 |
| Catalogue groups offered by the installer and the store | 7 |
| Base packs | 2 |
| Data packs | 2 |
| Boxed commands with no graphical launcher | 33 rows across 22 applications |
| Kernel | 7.2.7 |
| C libraries written for this system | 17 |
| Distinct source files the recipes name | 1,232, about 8.3 GiB (1,192 archived; 40 carried in git) |

## See also

- [Principles](principles.md) — the rules these four properties produce
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces fit together
- [Build system](../05-developer/build-system.md) — the phases described above, in detail
- [Status](../06-reference/status.md) — what is mature and what is not
- [Glossary](../06-reference/glossary.md) — the terms this book uses
