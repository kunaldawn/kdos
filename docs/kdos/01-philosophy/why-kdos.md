# Why KDOS

This chapter says what KDOS is, how the system is shaped, which four properties define it and who
it is for. It is written for anyone deciding whether to try KDOS, install it or contribute to it,
and it assumes no knowledge of the source tree. It is the first chapter of the book; read it
before anything else. [How KDOS differs](how-kdos-differs.md) then compares KDOS, point by point,
with the ways other distributions solve the same problems, [Principles](principles.md) gives the
rules that follow from the ideas here, and [Decisions](decisions.md) records the choices that
were close and what lost.

## What KDOS is

KDOS is a Linux distribution for x86_64 machines, built from source on the musl C library, with KDE
Plasma as its desktop, graphical applications ported to it natively, a catalogue of further
applications that run in containers, and updates of the operating system that arrive as signed
images written into a second root slot. The software written for it (the package manager, the
`kdos` command, the daemons, the installer, and the code that brings KDOS's packs, boxes and slots
onto Plasma's own surfaces) is small enough for one person to read, and everything around it,
Plasma included, is compiled here from pinned upstream source.

Those two aims usually pull against each other. A system you can read end to end tends to stop at
a shell prompt; a system with a browser, a CAD package and a video editor tends to be too large
to hold in your head. KDOS draws a line between the two instead of choosing one:

- **Inside the line** is the host: the kernel, the C library, the services, the libraries and the
  desktop. All of it, apart from a short list of [named
  exceptions](#what-is-not-built-from-source) such as firmware, is compiled in this repository
  from upstream source or from source written here, and it is meant to be read.
- **Outside the line** is the application catalogue. It is other people's packaging, built on the
  machine that wants it and run in a container (a *box*), and it is meant to work. The graphical
  applications KDOS ports natively, so that a machine with no network is complete, are inside the
  line with everything else.

The host is also built so that the machine running it can rebuild it. Four properties define the
system, each described in its own section below: the host is built from source, KDOS can build
KDOS, the repository builds offline, and applications are native ports with boxes for the rest.
When a decision elsewhere in the book looks arbitrary, it is usually one of these four being paid
for.

## The shape of the system

A *port* is a directory that describes how to build one package; its *recipe* is the files in it,
a declarative `kpkgbuild` and a `build.sh` beside it (see the
[Glossary](../06-reference/glossary.md#port)). `kpkg`, the package manager, builds and installs
ports.

The software on a KDOS machine falls into three *rings*, and the ring decides how each piece is
built and who maintains it:

| Ring | Where it is in the repository | What it holds | How it is built |
|---|---|---|---|
| Core | `ports/core/`, about 2,400 recipes | musl, the GNU userland, sysvinit, the toolchains, the libraries, the services, KDE Plasma and its frameworks, the kernel and its firmware, and the natively ported applications, KDE Gear among them | Compiled here from upstream source archives, each pinned by its sha256 |
| Desktop | `src/`, a few dozen recipes | KDOS's own software: the package manager, the `kdos` command, the root daemons, the installer, the pack and box tools, the Plasma components that reach them (settings pages, plasmoids, the Discover backend, KWin effects and the look packages) and the nine C libraries they share | Compiled here from source written for KDOS; `kpkg` and the installer by two bootstrap scripts |
| Outer | `src/system/kdos-appbox/catalogue` | 73 graphical and command-line applications over 7 shared runtimes | Declared as Debian packages and built by podman on the machine that asks for them |

The core ring starts from [musl](https://musl.libc.org/), the small C library KDOS uses in place
of glibc, and the GNU tools (coreutils, grep, sed, gawk, findutils), each a program of its own,
as the core commands. The nine libraries of the
desktop ring have no recipes of their own; each program that uses one compiles it in.

A running machine is a short chain of programs with no systemd init in it. The Limine boot loader
starts the kernel and an initramfs, a small early root filesystem whose one job is to find the real
one; sysvinit's `init` runs the numbered scripts in `/etc/init.d/`, and `ksvc` supervises the
long-running services they start, among them elogind, which keeps track of logins. When the
scripts have finished and the boot splash has gone dark, `init` starts SDDM, the login screen, on
the first virtual terminal; the live image logs the `kdos` account in with no password prompt. A
login passes through PAM, where elogind records the session and turnstile starts the account's own
services (its message bus and PipeWire), and SDDM then starts Plasma: KWin, the compositor, and the
Plasma shell with its panel. An application from the catalogue runs in a container under podman, a
container runtime run as the ordinary user rather than as root, but appears on the desktop as
ordinary software.

[Architecture overview](../03-architecture/overview.md) draws that chain in full, names every
process on a working desktop and says where each kind of state is kept.

## Built from source, with named exceptions

The host is compiled in this repository from about 2,400 recipes under `ports/core` for upstream
software and a few dozen under `src/` for the tools, the daemons and the Plasma components written
for KDOS, and from two scripts: one builds `kpkg` itself, which has no recipe, and the other builds
the installer, `kinstall`, whose recipe exists but is named in no phase list.

The build runs in thirteen phases, numbered in bands of ten from `00_cross` to `70_image`. It starts
with a cross toolchain (`00_cross`), builds a base userland on musl (`10_bootstrap`), rebuilds that
userland's toolchain with itself (`20_selfhost`), adds the build systems and base libraries
(`30_foundation`) and then the large compilers (`31_compilers`), builds the rest of the userland in
five layers, from language modules through the system, the graphics stack and the toolkits to the
applications (`40_lang` to `44_apps`), then the desktop (`50_desktop`), then the kernel
(`60_kernel`), and finally packs the result into an ISO (`70_image`). The two scripts run in
`10_bootstrap`: `120_kpkg.sh` compiles `kpkg` from `src/system/kdos-kpkg`, and `130_kinstall.sh`
compiles `kinstall` from `src/system/kdos-installer`. `kpkg` is built this way because nothing can
be installed as a package until it exists, and `kinstall` because it links nothing but the C
library, so it can be cross-compiled that early and is on every tree from the first bootable one.
The finished system has no base image beneath it and no binary archive to fall back on: the
container the build runs in, an Alpine 3.23 image with a host compiler, supplies the tools that
build the cross toolchain and the first userland, and nothing from it is installed. Every recipe
that a phase list (`script/phases/*/packages.txt` or `packages.d/`) names is installed on the
finished system, together with everything those recipes depend on. That reaches every upstream
recipe but one: `texlive-doc`, the TeX Live manuals, is named in `ports/unbuilt`, keeps its recipe
for a machine that wants to build it with `kpkg install`, and is left off the image. A *phase* is
defined in the [Glossary](../06-reference/glossary.md#phase). [How KDOS is
built](../05-developer/how-kdos-is-built.md) follows the build from `git clone` to a bootable ISO,
[The build system](../05-developer/build-system.md) describes the orchestrator that runs the phases,
and [The ports catalogue](../06-reference/ports-catalogue.md) lists every recipe by shelf, the
subject directory it is filed under in `ports/core`, with its phase.

A machine that updates installs what this build produced. A release publishes the packages it
built from these recipes, on a signed binhost and as whole-slot images, signed with the release
key, so nothing in it was built anywhere else; `kdos update apply --source` builds the same recipes
on the machine instead of installing them.

The claim has exceptions, and they are [listed in full](#what-is-not-built-from-source) at the end
of this chapter. Five classes are exempt from building from source: firmware and code for other
processors, bootstrap seeds, compiled font data, data that has no other source form, and JavaScript
built upstream. One further entry is listed so that the inventory is complete, though it is not an
exemption: two pieces of third-party code under `src/`, which are compiled here like everything
else. The application catalogue sits outside the rule altogether,
because nothing in it runs on the host. The rule and its exempt classes are stated in
[Principles](principles.md#everything-that-runs-on-the-host-is-built-from-source).

## KDOS can build KDOS

The build's third phase, `20_selfhost`, is a self-hosting pass. Inside the build chroot (the
isolated root filesystem the build runs in), the system first installs `fortify-headers`, the
hardening headers every later port is compiled against, and then rebuilds `tar`, `musl`, `zlib`,
`binutils`, `diffutils`, `m4`, `gawk` and `gcc`, together with the libraries those depend on
(`xxhash`, `gmp`, `mpfr`, `mpc`, `readline` and the `ncurses` under it), using the toolchain that
`10_bootstrap` built. The chain behind that toolchain has three links. The Alpine 3.23 compilers of
the build image, the only C compilers in it that KDOS did not produce, build the `00_cross` cross
toolchain; the cross toolchain builds the native `gcc` and `binutils` of `10_bootstrap`; and in
`20_selfhost` that native `gcc` compiles a new `gcc`, with a new `musl` beneath it. Everything from
`30_foundation` onwards is built by the `20_selfhost` compiler, which KDOS produced with a compiler
of its own.

The toolchains survive onto the shipped image, because the build installs them rather than
removing them. A running KDOS carries `gcc`, `binutils`, `clang`, `rust`, `go`, `cmake`, `meson`,
`ninja`, `python3`, `make` and `kpkg`, so any single recipe, the compiler and the kernel included,
can be rebuilt on the machine itself and offline once its source is present, with `kpkg`.
`kdos update fetch` brings the newest signed recipe tree over the network, and
`kdos update apply --source` brings every package that is behind it up to date by downloading its
source and compiling it on the machine, where an update otherwise installs the release's signed
binaries. [How KDOS differs](how-kdos-differs.md#self-hosting) compares this with other
distributions.

A full rebuild, every phase from the cross toolchain to the ISO, is driven by [`kdos
rebuild`](../04-programs/kdos-command.md#kdos-rebuild) from a booted medium, with no network at any
point. It runs the same orchestrator and phase scripts as `make build`, in a work directory that
must be on a real disk with at least 25 GB free. No rebuild from a medium has been run through
every phase:

```sh
kdos rebuild /mnt/disk/rebuild             # every phase
kdos rebuild --iso-only /mnt/disk/rebuild  # the image phase (70_image) only
kdos rebuild --dry-run /mnt/disk/rebuild   # report the plan and stop
```

Every image carries the recipe tree at `/ports`, but not the upstream sources or the build
scripts, so on the default ISO `kdos rebuild` finds no tree and stops. A developer medium, made
with `make build KDOS_ISO_SOURCES=1`, carries the whole tree, every fetched source beside its
recipe, under `/sources` on the medium, which the live system shows at `/mnt/iso/sources`.
Elsewhere, `KDOS_SOURCES` names a checkout. The command copies the tree into the work directory
and names the copy in `KDOS_WORKSPACE`, which the phase environments use in place of the build
container's `/workspace`. [The kdos command](../04-programs/kdos-command.md#kdos-rebuild) says
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

A recipe's `sha256 =` line is what verifies a source file, wherever the file came from; [How KDOS
is built](../05-developer/how-kdos-is-built.md#fetching-the-only-step-that-uses-the-network) says
where `make fetch` looks for each one. The `ports/core` recipes name 2,572 distinct files. The 44
small ones that git carries itself, such as bash's upstream patches, stay in the repository. The
other 2,528, about 39.3 GiB, are held in the KDOS source archive: four numbered GitHub releases,
`sources-1` to `sources-4`, each holding the shelves `ports/shelves` assigns to it, with every file
located through the committed index `ports/sources.idx`. A file the index does not name comes from
its upstream URL until `ports/publish` adds it. Nothing in the archive is replaced, so a recipe
whose sources it holds keeps building after its upstream URL disappears. Why the sources are
held this way, and what it costs, is in [Decisions](decisions.md#upstream-archives-are-hash-checked-release-assets).

## Native applications, and boxes for the rest

KDOS ports the applications a machine needs natively, with the toolkit each is written in, so that a
machine installed from KDOS media has a browser, an office suite, graphics and media tools, maps and
an offline library with no network at all. These are ordinary recipes under `ports/core`, filed on
shelves named for what they are for, such as `browsers`, `mail`, `office`, `graphics`, `education`
and `cad`, and built in the last two userland phases, `43_toolkits` and `44_apps`; all of KDE Gear
is among them. GTK, Qt, KDE Frameworks, WebKitGTK, QtWebEngine, wxWidgets, FLTK and Tk are built
for them, each with its Wayland backend as the default and its X11 backend for the applications
that need Xwayland. Qt and KDE Frameworks carry the desktop as well; see
[Principles](principles.md#the-desktop-is-kde-plasma). [The ports
catalogue](../06-reference/ports-catalogue.md) lists every application by shelf.

Discover is the one place to find all of it: the native ports, the catalogue's boxes and packs,
updates of the system and firmware, through backends KDOS adds for its own kinds of software.
There is no Flathub and no "Get New…": everything Discover offers is described in this repository
or carried on the medium.

The outer ring is a catalogue of 73 applications. Each is declared as a chain of packages rather
than shipped as bytes: an application row sits on one of 7 shared runtimes (GTK, Qt, KDE, media,
scientific Python, Electron and Wine) or directly on the base, and podman builds the chain on
the machine that asks for it, storing each runtime once however many applications share it. An
installed application runs in its own rootless container and behaves like ordinary system
software: it appears in the launcher under its own icon, registers as a handler for its file
types, follows the desktop's light or dark preference through the Settings portal where its
toolkit reads it, and can be started from a terminal by name. A Qt application follows the accent
too, through the platform theme its runtime declares in the catalogue; see
[Theming](../02-user-guide/theming.md).

Some catalogue entries are commands rather than windows. Twenty-five `cmd` rows across 16
applications expose solvers, converters, emulators and toolchains to be run from a prompt. They
get no launcher entry, because a program with no window has nothing for a launcher to open.

The native ports and the boxes answer different needs. A native port is on the medium and works
with no network, but each one is a recipe that has to be kept building. A catalogue row is one line
naming Debian packages, but installing it needs a network and minutes of apt on the machine that
asks.
[Packs and boxes](../03-architecture/packs-and-boxes.md) describes the mechanism, and
[Applications](../02-user-guide/applications.md) describes installing and running them.

## Who should run KDOS

KDOS assumes a reader who is comfortable with a build log, a package recipe, and the C and Qt code
that join KDOS to Plasma. The image ships one user account, `kdos` (the installer lets you rename
it), no telemetry, and no configuration generated from a declaration: every setting is a file you
can open, yours under `~/.config`, the machine owner's under `/etc`, and KDOS's defaults under
`/usr/share/kdos/xdg` beneath both.

It suits you if:

- you want to change the package manager, the installer or the way the desktop is put together
  rather than configure around them;
- you want a workstation that carries its applications on its own medium and needs no network
  to install them; or
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
| systemd's init, units and service manager | sysvinit's `init`, numbered scripts in `/etc/init.d/`, and `ksvc` to supervise services, a user's included; turnstile to start a user's services at login. Three pieces of systemd's tree run on their own: elogind (logins and seats), `basu` (the sd-bus library) and `eudev` (device events); `dbus` is the message bus and `dnsmasq` the local DNS cache |
| An X server on the login path, and X11 sessions | Wayland only. SDDM's greeter runs on `kwin_wayland`, the only session is Plasma on Wayland, and Xwayland, the X server that runs as a Wayland client, is the single carve-out for X11 clients, boxed or native, started by KWin inside a session |
| Flatpak and Flathub | Discover lists the ports, packs and boxes this repository describes, and firmware; there is no account and nothing to sign up to |
| "Get New…" | KNewStuff is switched off system-wide; a curated add-on pack on the medium carries the wallpapers, colour schemes and KWin scripts KDOS offers |
| Telemetry | No KDOS program sends usage data and KUserFeedback is not ported; boxed applications behave as their Debian packages do |
| Binaries built elsewhere | Every package and slot image a release publishes is built from this tree's recipes and signed with the release key; `kdos update apply --source` builds them on the machine instead |

## The system in numbers

Measured from the tree: recipes by counting `kpkgbuild` files, reachability by following the phase
lists and every `depends =` line, and the catalogue by counting its rows by kind. The exact recipe
counts are kept in [Status](../06-reference/status.md#scale) and
[The ports catalogue](../06-reference/ports-catalogue.md#ports-per-shelf).

| | |
|---|---|
| Port recipes in `ports/core` | about 2,400 |
| Port recipes under `src/` for KDOS's own software | a few dozen, in `src/system`, `src/art`, `src/desktop` and `src/daemons` |
| Shelves the upstream recipes are filed on | 102 |
| Upstream recipes that no phase list or dependency reaches, and so are not built | the ones named in `ports/unbuilt`, `texlive-doc` among them; `testing/bookcounts.py` refuses any other |
| C libraries written for this system, under `src/libs` | 9 |
| Plasma's own modules left out | 13, each because it needs systemd, KDE Frameworks 5, Flatpak, GRUB, Plymouth or another firewall, or is a shell for phones and televisions |
| KDE Gear modules left out | 4: kjournald, kmag, kmousetool and plasma-settings |
| Kernel | Linux 7.2.11 |
| Applications in the catalogue | 73 |
| Shared runtimes beneath them | 7 |
| Catalogue bases | 2 (Debian trixie and Alpine 3.24.1) |
| Catalogue data sets | 2 |
| Catalogue groups offered by the installer and the store | 7 |
| Boxed commands with no graphical launcher | 25 rows across 16 applications |
| Distinct source files the recipes name | 2,572 (44 carried in git; 2,528, about 39.3 GiB, fetched from the source archive or upstream) |

## What is not built from source

This section is the complete inventory of what the host installs without compiling it here. Each
entry is checked against the recipes. A port that carries a prebuilt file has to fit one of these
classes.

### Firmware and code for another processor

Code that runs on a DSP, a GPU, a microcontroller, a radio or a guest (a virtual or emulated
machine, or a Windows program under Wine), rather than as a host program, is shipped as upstream
built it. For most of it no source is published; the rest needs a cross toolchain this tree does
not carry. Seven ports are nothing else:

| Port | Version | Download | What it is |
|---|---|---|---|
| `linux-firmware` | 20260916 | 632 MiB | Upstream's tree less the firmware no x86 kernel loads, installed with upstream's own `copy-firmware.sh --zstd`, which creates the alias symlinks a plain copy omits |
| `intel-ucode` | 20260812 | 17 MiB | Upstream's whole Intel microcode set, concatenated into one bundle that sits in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2026.09.1 | 17 MiB | Intel SOF audio DSP firmware and topologies. They are not part of `linux-firmware`, and Tiger Lake and newer machines have no sound without them |
| `meshtastic-firmware` | 2.7.26 | 260 MiB | Meshtastic's release images for ESP32, nRF52840, RP2040, RP2350 and STM32 boards, installed for flashing a radio, not for running here |
| `rnode-firmware` | 1.86 | 59 MiB | The RNode release images for each supported LoRa board and the console image, which `rnodeconf-local` flashes without a network |
| `wine-mono` | 10.4.1 | 38 MiB | Wine's .NET Framework runtime, installed where Wine looks before it offers a download. Building it needs MinGW and a .NET SDK, neither of which is a port |
| `wine-gecko` | 2.47.4 | 81 MiB | The HTML engine behind Wine's MSHTML, 32- and 64-bit, placed the same way. It is Windows code that runs only inside a Wine prefix |

The firmware tree is not curated by guessing at hardware. Pruning that way is a bet on which
hardware the machine turns out to have, and losing that bet is silent: the kernel's
`request_firmware()` finds nothing and the device does not work, which looks like broken hardware
rather than a missing file. The recipe leaves out only an explicit list of firmware no x86 driver
requests: the arm64 Qualcomm SoC images, superseded Mellanox Spectrum images and Marvell Prestera
v2.0 and v3.0, about 370 MiB installed (see
[Known gaps](../06-reference/known-gaps.md#linux-firmware-leaves-out-what-an-x86-kernel-cannot-load)).

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
| `libperseus-sdr` | The Perseus receiver's Cypress FX2 firmware and FPGA bitstreams, compiled into the library and uploaded to the receiver when it is opened |
| `fuse-emulator`, `vice`, `openmsx`, `amiberry` | The guest ROMs each emulator starts from: the Spectrum ROMs distributed with Amstrad's permission, the Commodore ROM sets, the free C-BIOS MSX ROMs, and AROS's Kickstart replacement with the 68k WHDLoad, JST and AmiQuit boot binaries |
| `dosbox-staging` | The DOS programs on its built-in `Y:` drive (`xcopy.exe`, `deltree.com`, `debug.com`) and the FreeDOS `KEYB*.SYS` keyboard drivers, all run by the emulated machine |
| `libretro-melonds`, `mgba`, `libretro-mgba` | Free replacement console BIOSes compiled in as byte arrays: melonDS's DS ARM7 and ARM9 BIOS in `FreeBIOS.h`, and mGBA's `hle-bios.c`, assembled upstream from the `hle-bios.s` beside it with an ARM assembler that is not a port |
| `qemu` | EDK2 for the riscv64 `virt` machine, `edk2-riscv-code.fd` and its variable store, unpacked from the tarball's `pc-bios/`, because a build of it with a riscv64 bare-metal gcc 15 faults before reaching a boot option. The rest of the guest firmware it installs (SeaBIOS and SeaVGABIOS, qboot, the iPXE NIC ROMs, the `-kernel` option ROMs, EDK2 for x86_64 and aarch64, and OpenSBI) is compiled here from the sources in the tarball's `roms/` |

### Bootstrap seeds

Rust, Go, Zig, Haskell's GHC, Java's OpenJDK and OCaml are each written in themselves, so building
any of them needs a working copy first. That copy is the *seed*.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.1 | 150 MiB of upstream 1.97.1 stage-0 binaries (`rustc` 101 MiB, `rust-std` 37 MiB and `cargo` 11 MiB) beside the 233 MiB source |
| `go` | 1.27.2 | 57 MiB of upstream 1.25.9 bootstrap toolchain beside the 33 MiB source |
| `zig` | 0.16.0 | `stage1/zig1.wasm`, inside the source tarball: a WebAssembly build of the compiler that the build translates to C and compiles to start its own bootstrap |
| `ghc` | 9.12.4 | 239 MiB of upstream GHC 9.10.3 for musl, the statically linked Alpine build, beside the 32 MiB source. It compiles Hadrian, GHC's build system, and GHC's first stage |
| `openjdk` | 25.0.4.1 | 134 MiB of Adoptium's Temurin 25.0.4.1 JDK for Alpine (musl), the same feature release, beside the 114 MiB source. It is the boot JDK the build compiles the class library and tools with |
| `ocaml` | 5.5.1 | `boot/ocamlc` and `boot/ocamllex`, inside the source tarball: bytecode images of the compiler and the lexer generator, run by the `ocamlrun` the build compiles first, which rebuild every compiler and tool from source |

Every seed is pinned by version and sha256 like every other source, so the offline build still
holds, and no seed is installed. Everything the seeds produce is compiled here: the shipped
`rustc`, `cargo`, `go`, `zig`, `ghc`, `cabal`, `java` and `ocamlopt`, and every Rust, Go, Zig,
Haskell, Java and OCaml program in the tree.

One build-only tool rides in the same class. `kodi` generates its Python add-on bindings with a
Groovy script, and Groovy is not a port, so the recipe carries the Apache Groovy 4.0.33 binary
distribution and the commons-lang3 3.17.0 and commons-text 1.13.0 jars it needs. They run on the
`openjdk` port during the build, and nothing of them reaches the package. Every other Java
library in the tree is compiled here from the `-sources.jar` Maven Central publishes for it.

Go's source tarball also carries upstream-compiled objects, the race detector's runtime and the
BoringCrypto module, and the `go` port deletes them from what it installs rather than ship binaries
it did not build.

### Compiled font data

`noto-fonts` (2.015), `noto-cjk` (2.004), `noto-fonts-extra`, `nerd-fonts-symbols` (3.5.1) and
`ttf-jetbrains-mono`, the desktop's monospace face, ship as built font files because upstream
publishes them that way, and the sources behind them compile through toolchains this tree does not
carry: fontmake and gftools for Noto, AFDKO for Noto CJK, the Nerd Fonts patcher and its icon sets
for the symbols, and `gftools builder` over Glyphs sources for JetBrains Mono. Each recipe unpacks an archive
and installs the `.ttf`, `.ttc` or `.otf` files in it. `font-carlito` and `font-caladea` are the
crosextrafonts builds LibreOffice pins, metric-compatible with Calibri and Cambria; later Carlito
sources compile only through fontmake, and later Caladea changed its metrics.

`texlive` installs the Type 1, OpenType and TFM files of its texmf tree as TeX Live distributes
them. The rest are faces bundled inside applications, installed or compiled in as their upstreams
ship them:

| Port | Faces |
|---|---|
| `mupdf`, `matplotlib`, `seqkit` | Their bundled faces (`seqkit`'s through its Go vendor bundle) |
| `kodi` | `media/Fonts/arial.ttf` and `teletext.ttf`, the Noto Sans, Noto Mono and Roboto Thin faces of the Estuary skin, and the Open Sans and icon web fonts of its default web interface |
| `koreader` | Its reading fonts: Droid Sans Mono, FreeSans, FreeSerif, the Nerd Fonts symbols and a Noto set |
| `qcad`, `freecad` | `osifont`, the ISO 3098 drawing face, and FreeCAD's TechDraw faces (`osifont-lgpl3fe`, `osifont-italic`, `Y14.5-2018`, `Y14.5-FreeCAD`) |
| `solvespace` | Bitstream Vera Sans, compiled into the binary from `res/fonts` |
| `stellarium` | `NotoSansSC-Regular.otf`, which no port ships as a single face, and the DejaVu web fonts of the RemoteControl plugin's page; the other four faces it bundles are links to `noto-fonts` and `ttf-dejavu` |
| `mupen64plus`, `ppsspp`, `dosbox-x`, `retroarch-assets` | The on-screen display and menu faces: `mupen64plus-core`'s `font.ttf`, PPSSPP's Roboto Condensed and Inconsolata, DOSBox-X's `contrib/fonts` (WenQuanYi bitmaps, Nouveau IBM, Sarasa Gothic and others), and RetroArch's menu fonts |
| `uosc`, `qtforkawesome` | Icon faces: `uosc_icons.otf` and `uosc_textures.ttf`, and Fork Awesome 1.2.0's `forkawesome-webfont.ttf`, compiled into the qtforkawesome library |

Four fonts, and the X.org bitmap fonts, are compiled here, each by its upstream's own pipeline:

| Port | From | Through |
|---|---|---|
| `ttf-dejavu` (2.37) | The FontForge sources, `src/*.sfd` | Upstream's `make full-ttf`: `fontforge` writes each face and `ttpostproc.pl` finishes its tables through `perl-font-ttf` |
| `terminus-ttf` (4.49.3) | `terminus-font` 4.49.1's BDF sources | mkttf: `mkitalic` slants the BDFs, `fontforge` gathers every size as a bitmap strike and traces the largest into outlines with `potrace` |
| `noto-emoji` (2.051) | The 128-pixel PNG artwork and the region flags | Upstream's `Makefile` for the CBDT face `NotoColorEmoji.ttf`: `waveflag`, ImageMagick, `pngquant`, `zopflipng`, and the builder scripts on `fonttools` and nototools |
| `font-liberation` (2.1.5) | The FontForge sources of Liberation and Liberation Sans Narrow (1.07.6) | Upstream's `make ttf-dir`, through `fontforge` and `fonttools` |
| `font-adobe-75dpi`, `font-cursor-misc`, `font-misc-misc` | The X.org BDF sources | Their autotools builds, through `bdftopcf`, into the PCF faces Xwayland's core font path reads |

The font builds use `fontforge` from its command line and its Python module; its GTK editor window
is in the same package and plays no part in them. `fontforge` stamps `SOURCE_DATE_EPOCH` into every
face it writes, and `terminus-ttf` takes the year in its copyright notice from the same variable, so
two builds of one recipe produce the same file. `noto-emoji` ships the bitmap face only; the COLRv1
face is built by nanoemoji, which is not a port.

The console font is a separate port and is built from source: `terminus-font` (4.49.1) goes from
BDF through `configure` and `make` into the PSF that
[`kdos-getty`](../03-architecture/boot-and-init.md#the-console) loads.

### Data with no other source form

`hwdata`, `iso-codes` and `docbook-xml` are text or tables installed as they arrive. `docbook-xsl`
is too, but for one patch to both of its trees: `string.subst` calls `str:replace` where the
processor has it, because the stock template recurses once per match and a page as long as
`git-config.1` passes `xsltproc`'s 3000-deep limit and fails.
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
| `tessdata-eng` | `eng.traineddata` and `osd.traineddata`, `tessdata_fast`'s English and script-detection models for `tesseract`, two `source =` lines |
| `whisper-model-base-en` | The Whisper `base.en` speech model in ggml form, which `whisper-cli` reads when Dolphin's Transcribe action runs it |
| `fluidr3-gm-sf3` | The FluidR3 Mono General MIDI SoundFont, compressed, the default instrument set for every MIDI player |
| `perl-xml-parser` | The `.enc` encoding maps under `share/` |
| `fcitx5-chinese-addons` | The pinyin-to-character and stroke tables, two further `source =` lines |
| `john` | The `.chr` character-frequency files |
| `picotool` | The last 512 bytes of each RP2350 revision's boot ROM, under `model/`. A connected chip will not read them out, so picotool supplies them when it dumps the ROM. They are a copy of the mask ROM, not a build |
| `alsa-utils` | Recorded audio: the channel-name voice samples `speaker-test` plays |
| `kdos-tools` | `/usr/share/kdos/secdb.txt`, Alpine's security database pruned into one table, committed under `src/system/kdos-tools/secdb/` and regenerated by hand with the `vendor.py` beside it |
| `digikam` | The face engine's models under `/usr/share/digikam/facesengine`: YuNet for detection, SFace for recognition, and the `dnntestimage.jpeg` it checks them with |
| `piper` | The ONNX models of its Arabic and Hebrew diacritisers, `tashkeel/model.onnx` and `hebrew/nakdimon.onnx` |
| `stockfish` | The NNUE evaluation network `nn-1a298aa575a0.nnue`, embedded in the binary |
| `rnnoise`, `freedv-gui` | Trained network weights: RNNoise's `src/rnnoise_data.c`, and the Opus DNN and RNNoise weight archives from media.xiph.org and RADE's generated `rade_*_data.c` inside FreeDV |
| `organicmaps` | `World.mwm` and `WorldCoasts.mwm`, the whole world at low zoom from OpenStreetMap data, and the style tables `drules_proto*.bin`, which upstream compiles from its MapCSS styles with kothic, which is not a port |
| `sniffnet` | The country and ASN databases under `resources/DB`, embedded in the binary |
| `josm` | The tag2link index, taken from its webjar as `index.json` |
| `koreader` | The CA bundle for its HTTPS requests, taken from the certifi wheel among its third-party downloads |
| `gcompris` | The word pictures, the English voices and the background music, four `.rcc` archives from KDE's CDN |
| `openttd-opengfx`, `openttd-opensfx`, `openttd-openmsx` | OpenTTD's free base sets: graphics as NewGRF `.grf` files built by nml and grfcodec, sounds as a `.cat` bundle built by catcodec, and MIDI music. None of those tools is a port |
| `xonotic-data`, `stk-assets`, `devilutionx`, `retroarch-assets` | Game content as released: Xonotic's `.pk3` archives of compiled maps, models, textures and sound; SuperTuxKart's karts, tracks, music and textures; `devilutionx.mpq`, the engine's own assets; RetroArch's menu artwork and sounds |

### JavaScript built upstream

Some ports ship a web front end or a tool as the output of a JavaScript toolchain: a bundler run
over an npm dependency tree. Rebuilding one here needs that tree as a node vendor bundle of its
own, as `cncjs` carries, and upstream's bundler, so each ships as upstream released it:

| Port | JavaScript |
|---|---|
| `libkiwix` | The server skin under `static/`, minified files included |
| `transmission` | The web client in `web/public_html`; rebuilding it (`REBUILD_WEB`) runs npm against the network |
| `deluge` | The minified ExtJS library of its web UI. Deluge's own `deluge-all.js` is re-minified here from its sources by rjsmin |
| `kolibri` | The webpack bundles under `kolibri/core/static` and each plugin's `build` directory |
| `pat` | `web/dist`, the webpack bundle upstream commits and the binary embeds |
| `kodi` | The default web interface, `webinterface.default` |
| `yarn` | `cli-dist`'s `yarn.js`, one bundled script run by `node`, which inlines an emscripten build of libzip |
| `ghostwriter` | The React and MathJax bundles its preview loads |
| `arianna` | foliate-js, the renderer that draws its pages, and the built `pdf.js`, `zip.js`, `fflate.js` and `jszip.min.js`, compiled into its resources |

### Third-party code under `src/`

Most of `src/` is written for KDOS. Two pieces are not, and each is compiled here like everything
else:

| Where | What |
|---|---|
| `src/libs/libksig/monocypher/` | Monocypher 4.0.3, four files of C99 providing Ed25519, dual-licensed BSD-2-Clause and CC0, vendored verbatim. [`libksig`](../05-developer/c-libraries.md) exists to wrap it |
| `src/art/kdos-bb` | A frozen hard fork of the AA-project's `bb` 1.3rc1 demo; see [Decisions](decisions.md#freezing-a-demo-rather-than-writing-one) |

KDOS's Plasma packages that differ from Breeze, such as its login-screen theme, are not vendored:
each copies the installed Breeze files at build time and applies a patch to them, so the upstream
part is always the version the tree builds.

### The application catalogue is Debian

The 73 applications, the 7 runtimes and the default base beneath them are Debian trixie
packages, built on `debian:trixie-slim`, with one exception: VSCodium comes from the `.deb` its
upstream publishes on GitHub, fetched by a `deb` row in the catalogue and handed to apt so that its
dependencies resolve from Debian. The catalogue also carries a second, empty base on
`alpine:3.24.1`, a scratch userland for trying something in a box. Nothing in the catalogue is
compiled by this repository and none of it is carried on the installation medium: the catalogue
records which packages an application is made of, and podman builds it on the machine that asks.
See [Packs and boxes](../03-architecture/packs-and-boxes.md).

### Everything else

Everything else on the host, including every library, every daemon, the compiler, the kernel and the
whole desktop, Plasma and its frameworks included, is compiled here from a source file whose URL and
sha256 are in this repository. A prebuilt object for the host that a tarball carries and that fits
none of these classes is deleted from the package or never taken from the tarball: `go` drops its
race-detector runtime and BoringCrypto module, `john` drops its ZTEX FPGA bitstreams and controller
image, `kolibri` drops the shared objects built elsewhere under its `dist`, `hplip` fails its build
if an upstream-built HP object reaches the package, and `xonotic-data` and `sauerbraten` take only
the data from release archives that also carry prebuilt engines. `libdvdcss`, which reads
CSS-encrypted DVDs, is compiled here from its source like any other library.

## See also

- [How KDOS differs](how-kdos-differs.md) — KDOS compared with other distributions, problem by
  problem: the C library, the core userland, init, the display stack, toolkits, the desktop and
  how other distributions ship Plasma, packages, source pinning, self-hosting, reproducibility,
  configuration, applications, updates, security, and hardware and support
- [Principles](principles.md) — the rules these four properties produce
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces of a running system fit
  together
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build described above, from
  `git clone` to a bootable ISO
- [The build system](../05-developer/build-system.md) — the orchestrator that runs the phases
- [The ports catalogue](../06-reference/ports-catalogue.md) — every recipe, by shelf and phase
- [Status](../06-reference/status.md) — what is mature and what is not
- [Glossary](../06-reference/glossary.md) — the terms this book uses

<!-- book-nav -->
---

*Part I — Introduction, chapter 1.* [Contents](../README.md) · Next: [2. How KDOS differs](how-kdos-differs.md)
