# Why KDOS

What this distribution is, what it refuses to be, and who should run it. Read this before the
rest of the book: every design in KDOS follows from four properties, and a decision that looks
arbitrary elsewhere in the documentation is usually one of these four being paid for.

## The four properties

**Built from source, with named exceptions.** The host is compiled here from upstream tarballs by
recipes in `ports/` — 764 of them, running cross toolchain → musl userland → self-hosting
bootstrap → libraries → desktop → kernel. There is no base image and no binary package archive
to fall back on. A handful of ports are exceptions, and they are listed in full below rather than
glossed over.

**KDOS can build KDOS.** Phase 2 of the build is a genuine self-hosting pass: inside the chroot,
the system rebuilds tar, musl, zlib, binutils and gcc *with itself*. The shipped system carries
gcc, binutils, rust, cmake, meson, ninja, python3, make and `kpkg`, so a running KDOS can rebuild
every port in the tree — the compiler, the kernel and the desktop included.
[`kdos rebuild`](../04-programs/kdos-command.md) does that from the sources on the medium, with
no network at any point.

**The repository builds offline.** `make build` runs with `--network none`. Every upstream
tarball, every vendored dependency bundle and every application pack is reachable at build time
with no network at all. They are not in git — they are release assets, and `make bootstrap` is
the one step that fetches them before the first build. Git holds what *identifies* them: the
`sha256 =` in each recipe, and the content hash of each pack in the signed index. A build that
reaches the internet is a build that stops reproducing the day a URL rots.

**Applications live in boxes.** KDOS builds the *desktop*. It does not native-port Firefox,
LibreOffice or Blender, and it never will. The outer ring is a catalogue of 181 applications,
each shipping as one signed image over a small set of shared runtimes, running in its own
rootless container and behaving like ordinary system software: launcher entries, MIME handlers,
terminal commands, one theme. The boundary sits exactly where the build cost is.

Everything else in this documentation follows from those four. See
[Principles](principles.md) for the rules they generate and [Decisions](decisions.md) for the
arguments behind the ones that were close.

## What is not built from source

"Built from source" is a claim, so here is the complete list of what it does not cover. Each
entry was checked against the tree rather than remembered.

### Vendor firmware and microcode — no source exists to build

| Port | Version | Source size | What it is |
|---|---|---|---|
| `linux-firmware` | 20260810 | 619 MB | Upstream's complete tree, unpruned, installed with upstream's own `copy-firmware.sh --zstd` — which creates the alias symlinks that a plain copy would omit |
| `intel-ucode` | 20260811 | 17 MB | Upstream's whole Intel microcode set, concatenated into one bundle that rides in front of the initramfs for the kernel's early loader |
| `sof-firmware` | 2025.01.1 | 10 MB | Intel SOF audio DSP firmware and topologies. Not part of `linux-firmware`; Tiger Lake and newer are silent without it |
| `wireless-regdb` | 2025.07.10 | 32 KB | The wireless regulatory database. It **must** ship prebuilt: the kernel is `CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y` and verifies upstream's signature, so a locally regenerated database is rejected in silence |

The firmware tree ships **whole rather than curated**. A pruned subset is a bet on which hardware
the machine turns out to have, and losing that bet is silent — `request_firmware()` finds
nothing and the device simply does not work, which reads as broken hardware rather than as a
missing file.

### Two bootstrap compilers — the chicken and the egg

Rust and Go are each written in themselves, so building either needs a working one first.

| Port | Version | Bootstrap payload |
|---|---|---|
| `rust` | 1.98.0 | 151 MB of upstream stage-0 binaries (`rustc`, `cargo`, `rust-std`) beside the 234 MB source |
| `go` | 1.27.0 | 58 MB of upstream bootstrap toolchain beside the 34 MB source |

Both bootstraps are pinned by version and sha256 like every other source, so the offline build
still holds. Everything the bootstraps then produce — the shipped `rustc`, `cargo` and `go`, and
every Rust and Go program in the tree — is compiled here.

### Two prebuilt font sets

`ttf-dejavu` (2.37) and `terminus-ttf` (4.49.3) ship as `.ttf` because upstream publishes them
that way. The console font is a different port and *is* built from source: `terminus-font`
(4.49.1) goes from BDF through `configure` and `make` into the PSF that
[`kdos-getty`](../03-architecture/boot-and-init.md) loads.

### Vendored artwork, remade rather than redrawn

`kdos-icons` (pruned Papirus SVG), `kdos-cursors` (Bibata Xcursor binaries) and `kdos-gtk-theme`
(adw-gtk3) are upstream assets committed here and recoloured at build time by generators in
`src/packages/`. The palette is ours; the shapes are not. Each carries a `LICENSE.notice`
recording exactly what was changed. See [Theming](../02-user-guide/theming.md).

### One vendored third-party source set

Monocypher, in `src/libs/libksig/monocypher/` — four files of public-domain C99 providing
Ed25519, compiled here like everything else. It is the only code under `src/` that is not ours,
and [`libksig`](../05-developer/c-libraries.md) exists to wrap it.

### The application catalogue is Debian

The 181 application packs and the runtimes beneath them are built from Debian trixie packages.
Nothing inside them is compiled by this repository. That is the whole point of the outer ring,
and it is by far the largest body of binaries on the medium, so it is worth saying plainly. See
[Packs and boxes](../03-architecture/packs-and-boxes.md).

### Data files are data

`ca-certificates`, `iana-etc`, `hwdata`, `xkeyboard-config`, `iso-codes` and `docbook-xml`/`xsl`
are text or tables installed as they arrive. One is not purely text: `alsa-ucm-conf` carries a
small number of binary `.bin` files — precomputed EQ coefficients for SOF DSPs, which belong with
the firmware group in kind if not in size.

Everything else on the host — every library, every daemon, the compiler, the kernel and the whole
desktop — is compiled here from a tarball whose URL and sha256 are in this repository.

## Who this is for

KDOS assumes a reader who is comfortable with a build log, a package recipe, and the C that draws
the panel. It ships one user account, no first-boot wizard, no telemetry, and no configuration
layer between you and the file that takes effect.

It is worth your time if you want to **change** the desktop, the package manager or the installer
rather than configure around them; to run a workstation whose applications are containerised by
default; or to keep an offline, reproducible source for the exact system you are running.

It is the wrong choice if you need broad hardware enablement, a large binary archive, commercial
support, or a system that stays out of your way. There is no vendor to escalate to and no
third-party repository to fall back on.

## The trade

You get a system that is inspectable end to end and rebuildable from the machine itself,
offline, with reproducible packages. Anything wrong is wrong somewhere you can read.

You take on being the integrator. When a port needs a version bump, you bump it. When a build
fails on a toolchain change, you read the log and fix the recipe. When hardware needs a kernel
option, you set it and rebuild. That work is the price of the first paragraph, and this
documentation exists to make it tractable — see
[Writing ports](../05-developer/writing-ports.md) and
[Build troubleshooting](../05-developer/build-troubleshooting.md).

## What is deliberately absent

Each of these is a decision with an argument behind it, not an omission. The arguments are in
[Decisions](decisions.md); the rules they generate are in [Principles](principles.md).

| Absent | Instead |
|---|---|
| systemd | `seatd`, `basu`, `eudev`, `dbus`, `dnsmasq`, and init scripts under `ksvc` |
| An Xorg server | Wayland only, with rootless Xwayland as the single carve-out for X11 clients |
| GTK and Qt on the host | A desktop drawn as a character-cell grid by our own libraries |
| A display manager | `kdos-desktop`, started by hand from a tty |
| A first-boot wizard | The installer asks its questions once, then the system is yours |
| An application store | The medium **is** the software library; discovery is in the Start menu |
| Telemetry | Nothing reports anything anywhere |
| A binary package archive | Ports built here, with an optional signed binhost you run yourself |

## Scale

Counted from the tree at the time of writing.

| | |
|---|---|
| Port recipes in `ports/core` | 764 |
| Packages installed on the built system | 758 |
| Applications in the catalogue | 181 |
| Shared runtimes beneath them | 7 |
| Base packs | 2 |
| Data packs | 2 |
| Boxed commands with no graphical launcher | 33 |
| Kernel | 7.0.10 |
| C libraries written for this system | 13 |

## See also

- [Principles](principles.md) — the rules these four properties generate
- [Decisions](decisions.md) — the choices that were close, and what lost
- [Getting started](../02-user-guide/getting-started.md) — building an image and booting it
- [Architecture overview](../03-architecture/overview.md) — how the pieces fit together
- [Status](../06-reference/status.md) — what is mature and what is not
