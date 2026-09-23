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

The host is compiled in this repository from upstream tarballs by 903 recipes — 879 under
`ports/core` for upstream software, 24 under `src/` for the desktop, the daemons and the tools —
running from a cross toolchain, through a musl userland, through a self-hosting pass, through the
build tools, the libraries, the desktop and finally the kernel.
There is no base image underneath it and no binary archive to fall back on. The finished system
carries 833 installed packages.

A claim like that is worth nothing without its exceptions, so [the exceptions are listed in
full](#what-is-not-built-from-source) rather than glossed over: firmware and code for other
processors, compiled font data, three compiler bootstrap seeds, data with no other source form, some
vendored artwork, one third-party C source set, and the Debian packages that make up the
application catalogue. Everything the host installs and runs on its own processor is compiled here;
the rule and its four exempt classes are in [Principles](principles.md#everything-that-runs-on-the-host-is-built-from-source).

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
each is in the tree, held by Git LFS — 1,020 archives, about 8.3 GB. A clone is therefore the
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

### Firmware and code for another processor

Code that runs on a DSP, a GPU, a microcontroller or a guest rather than on the host is shipped as
upstream built it; for most of it no source is published. Three ports are nothing else:

| Port | Version | Payload | What it is |
|---|---|---|---|
| `linux-firmware` | 20260916 | 632 MB | Upstream's complete tree, unpruned, installed with upstream's own `copy-firmware.sh --zstd`, which creates the alias symlinks a plain copy omits |
| `intel-ucode` | 20260812 | 17 MB | Upstream's whole Intel microcode set, concatenated into one bundle that rides in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2026.09.1 | 17 MB | Intel SOF audio DSP firmware and topologies. Not part of `linux-firmware`; Tiger Lake and newer are silent without it |

The firmware tree ships whole rather than curated. Pruning it is a bet on which hardware the
machine turns out to have, and losing that bet is silent — `request_firmware()` finds nothing and
the device does not work, which reads as broken hardware rather than as a missing file.

`wireless-regdb` is not among them. Its `regulatory.db` is generated here from upstream's `db.txt`,
and the only binary taken from the tarball is upstream's detached signature, `regulatory.db.p7s`.
The kernel sets `CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y` and loads the database only when that
signature verifies, so the build checks it against the generated file with upstream's certificate
and fails on a mismatch: what ships is byte-for-byte the database upstream signed, or nothing.

The rest of this class rides inside ports that are otherwise compiled here:

| Port | Payload |
|---|---|
| `intel-media-driver` | The closed EU kernels, compiled in with `ENABLE_KERNELS=ON` and `BUILD_KERNELS=OFF`; rebuilding them from their assembly needs Intel's shader compiler, which is not a port |
| `alsa-ucm-conf` | A small number of `.bin` files: precomputed EQ coefficients loaded into SOF DSPs |
| `espflash`, `probe-rs`, `python3-esptool`, `openfpgaloader` | The flasher stubs, flash algorithms and bridge bitstreams each one uploads to the device it drives |
| `qemu` | The guest firmware images from `pc-bios/` that its install copies unfiltered — SeaBIOS, EDK2, OpenSBI and the rest — which run inside the guest |

### Three compiler bootstrap seeds

Rust, Go and Zig are each written in themselves, so building any of them needs a working one first.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.1 | 150 MB of upstream 1.97.1 stage-0 binaries — `rustc` (101 MB), `rust-std` (37 MB) and `cargo` (11 MB) — beside the 233 MB source |
| `go` | 1.27.1 | 57 MB of upstream 1.25.9 bootstrap toolchain beside the 33 MB source |
| `zig` | 0.16.0 | `stage1/zig1.wasm`, inside the source tarball: a WebAssembly build of the compiler that the build translates to C and compiles to start its own bootstrap |

Every seed is pinned by version and sha256 like every other source, so the offline build still
holds, and no seed is installed. Everything the seeds produce — the shipped `rustc`, `cargo`, `go`
and `zig`, and every Rust, Go and Zig program in the tree — is compiled here. Go's source tarball also carries
upstream-compiled objects — the race detector's runtime and the BoringCrypto module — and the `go`
port deletes them from what it installs rather than ship binaries it did not build.

### Compiled font data

`noto-fonts` (2.015), `noto-cjk` (2.004), `noto-fonts-extra` and `nerd-fonts-symbols` (3.5.1)
ship as built faces because upstream publishes them that way, and the sources behind them compile
through toolchains this tree does not carry — fontmake and gftools for Noto, AFDKO for Noto CJK,
the Nerd Fonts patcher and its icon sets for the symbols. Each recipe unpacks an archive and
installs the `.ttf`, `.ttc` or `.otf` files in it. The same holds for the faces bundled inside
`mupdf`, `matplotlib` and `seqkit` (the last through its Go vendor bundle), which are installed or
compiled in as their upstreams ship them.

Three faces are compiled here, each by its upstream's own pipeline:

| Port | From | Through |
|---|---|---|
| `ttf-dejavu` (2.37) | The FontForge sources, `src/*.sfd` | Upstream's `make full-ttf`: `fontforge` writes each face and `ttpostproc.pl` finishes its tables through `perl-font-ttf` |
| `terminus-ttf` (4.49.3) | `terminus-font` 4.49.1's BDF sources | mkttf: `mkitalic` slants the BDFs, `fontforge` gathers every size as a bitmap strike and traces the largest into outlines with `potrace` |
| `noto-emoji` (2.051) | The 128-pixel PNG artwork and the region flags | Upstream's `Makefile` for the CBDT face `NotoColorEmoji.ttf`: `waveflag`, ImageMagick, `pngquant`, `zopflipng`, and the builder scripts on `fonttools` and nototools |

`fontforge` is built without its GUI, so it brings no GTK to the host. `fontforge` stamps
`SOURCE_DATE_EPOCH` into every face it writes, and `terminus-ttf` takes the year in its copyright
notice from the same variable, so two builds of one recipe are the same file. `noto-emoji` ships the
bitmap face only; the COLRv1 face is built by nanoemoji, which is not a port.

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

### Data with no other source form

`hwdata`, `xkeyboard-config`, `iso-codes` and `docbook-xml`/`xsl` are text or tables installed as
they arrive. `iana-etc` is tables too, but generated at build time from IANA's own XML registries,
each pinned by its hash, rather than carried as text somebody else produced. `ca-certificates` is
generated the same way, from the `certdata.txt` of a pinned NSS release.

Some data is binary or generated upstream, and the file as shipped is the form upstream maintains —
there is nothing earlier to build it from:

| Port | Data |
|---|---|
| `tesseract` | `eng.traineddata`, `tessdata_fast`'s English model, a second `source =` line |
| `perl-xml-parser` | The `.enc` encoding maps under `share/` |
| `libkiwix` | The JavaScript of the server's skin under `static/`, minified files included |
| `fcitx5-chinese-addons` | The pinyin-to-character and stroke tables, two further `source =` lines |
| `john` | The `.chr` character-frequency files |
| `alsa-utils` | Recorded audio: the channel-name voice samples `speaker-test` plays |

Everything else on the host — every library, every daemon, the compiler, the kernel and the whole
desktop — is compiled here from a tarball whose URL and sha256 are in this repository. A prebuilt
object for the host that a tarball carries and that fits none of these classes is deleted from the
package: `go` drops its race-detector runtime and BoringCrypto module, and `john` drops `run/ztex`.

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
| Port recipes in `ports/core` | 879 |
| Port recipes under `src/` for KDOS's own software | 24 |
| Packages installed on the built system | 833 |
| Applications in the catalogue | 183 |
| Shared runtimes beneath them | 7 |
| Catalogue groups offered by the installer and the store | 7 |
| Base packs | 2 |
| Data packs | 2 |
| Boxed commands with no graphical launcher | 36 rows across 25 applications |
| Kernel | 7.2.7 |
| C libraries written for this system | 17 |
| Upstream archives held in Git LFS | 1,020 archives, about 8.3 GB |

## See also

- [Principles](principles.md) — the rules these four properties generate
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces fit together
- [Build system](../05-developer/build-system.md) — the phases described above, in detail
- [Status](../06-reference/status.md) — what is mature and what is not
