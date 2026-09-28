# Packs and boxes

This chapter explains how KDOS packages the graphical applications that run in containers rather
than as native ports: how an application is built from the catalogue, what a **pack** is, how a pack
is verified, installed and mounted, how a box's root filesystem is composed, and how a **box**,
whichever route built it, reaches your desktop. It is written for administrators who want to know
what happens when an application is installed, and for contributors working on `kdos-appbox` (and
`kdos-box`, the same program under a second name), `kdos-pack`, `kdos-packd`, `kdos-boxinit` or the
catalogue. If you only want to install and use applications, read
[Applications](../02-user-guide/applications.md) first; this chapter is the machinery underneath it.
[Principles](../01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop) explains
which applications are native ports instead, and why.

Seven terms carry the chapter:

| Term | Meaning |
|---|---|
| **Box** | A rootless container that one application, or a working environment, runs in. By default it shares your home directory, `/tmp`, `/run/user/<uid>`, the host's network, IPC and process namespaces, and `/dev`; a box profile can narrow each of these (see [The box](#the-box)) |
| **Catalogue** | The shipped list of every application KDOS knows how to build, expressed as Debian packages. It is not the [ports catalogue](../06-reference/ports-catalogue.md), which lists the host's own recipes |
| **Pack** | A single file holding a read-only filesystem image (EROFS) with metadata, an icon and signatures appended. Extension `.kpack` |
| **Runtime** | A layer shared by many applications, such as the GTK or Qt libraries, sitting between the base and an application |
| **Data pack** | A pack that carries a dataset rather than a program. It is mounted but never becomes part of a box's root |
| **Lane** | One of the two routes to a box: the store lane builds container images from the catalogue on this machine; the import lane installs packs made elsewhere |
| **Pack store** | `/var/lib/kdos/packs`, where `kdos-packd` keeps installed packs |

This is a separate system from [host packaging](packaging.md) because it answers a different
question. Host packaging answers *what is installed on this machine*. Packs and boxes answer *what
software can this machine run, and how does it get a piece of it without installing anything into
the system*. [How KDOS differs](../01-philosophy/how-kdos-differs.md#applications) sets this model
beside Flatpak, Snap and the conventional distribution package.

## Two lanes, one box

Two routes reach a box:

- **The store lane** builds a stack of container images from the catalogue on your machine and
  creates a box from the top one.
- **The import lane** installs signed or unsigned packs somebody else exported, and composes a box
  from an overlay of them.

The lanes differ in where the bytes came from and what vouches for them. From the container root
upward they behave the same.

| | Store | Import |
|---|---|---|
| Source | The Debian archive, over the network | A `.ktar` or `.kpack` somebody handed you, or a pack on the boot medium |
| Verified by | Nothing KDOS controls: apt checks the archive, and the resulting images are unsigned | `kdos-packd`: the payload hash and the signature, when it installs the pack or first mounts it from the medium |
| Cost | Minutes of apt per application | A copy |
| Stored as | Podman images `kdos/<id>`, one per catalogue row | `.kpack` files in `/var/lib/kdos/packs` |
| Box base | `image:kdos/<id>` | `pack:<id>` |

The store lane is the ordinary route on a machine with a network. The import lane is the route on a
machine without one, and the one that carries a verification KDOS itself performs.

## The catalogue

`src/packages/kdos-appbox/catalogue` defines what can be built, and is installed to
`/usr/share/kdos/appstore/catalogue`. The store lane, the installer (`kinstall`) and `kdos app` all
read this one file; there is no second copy and no generated index, so a row added here is offered
by every surface.

A row reads `<kind> <id> <parent> <apt packages…>`. The parent is a row id, never an image tag; the
store lane resolves it to the image `kdos/<parent>` that it built for that row. The two bases are
`base` (Debian) and `alpine`; the seven runtimes are `rt-gtk`, `rt-qt`, `rt-kde` (on top of
`rt-qt`), `rt-media`, `rt-sci`, `rt-electron` and `rt-wine`; the two data rows are
`data.kicad-packages3d` and `data.tesseract-langs`. Every other line kind decorates a row. The table
counts every line kind in the shipped file:

| Line | Declares | In the shipped file |
|---|---|---|
| `base` | A whole root filesystem | 2 |
| `runtime` | A layer over the base, shared by many applications | 7 |
| `app` | One application, as a difference over a runtime or the base | 73 |
| `data` | A dataset, mounted but never composed into a container | 2 |
| `image` | A base that names its own container image, `image <base> <ref>` | 1 |
| `snapshot` | Which Debian archive date the packages come from | 1 |
| `env` | An environment variable a row needs, `env <row> NAME=VALUE` | 7 |
| `cmd` | A command a row provides that has no graphical launcher, `cmd <row> <name>` | 25 |
| `deb` | An application Debian does not carry, `deb <row> <releases-api-url> <asset-pattern>` | 1 |
| `needs` | A data row an application is useless without, `needs <app> <data>` | 0 |
| `graft`, `boxgraft` | Where a data row's contents should appear, `graft <data> <path-in-pack> <destination>` | 2 and 1 |
| `group` | A curated bundle `kdos-store` and the installer offer as one tick | 17 lines, 7 groups |
| `meta` | Display name, category, size estimate and tagline | 75: one for each `app` and `data` row |

**A row's parent must appear above it.** The resolver walks a chain upward in one pass, so a
forward reference is a chain it cannot close; reordering the file makes an install fail naming the
missing parent rather than loop.

**`snapshot` pins the Debian archive.** The shipped value is `snapshot = auto`, which takes the
date from the base image's own sources file, so what is installed on top cannot disagree with the
root filesystem under it. A literal date such as `20260824T000000Z` pins explicitly; `off` reads
the live archive and gives up reproducibility. If `auto` cannot find a date, the build proceeds
unpinned and prints a warning saying so, because an unpinned build file is otherwise
indistinguishable from a pinned one.

**A group is not a category.** The first `group` line for a group id is its description and every
later one lists members. The seven groups are `creative`, `dev`, `essential`, `games`, `make`,
`office` and `science`; `essential` is what `kinstall` preselects. A category is the application's
own, taken from its desktop entry, and every application has exactly one. A group is curated, most
applications are in none, and bases and runtimes are in none (they arrive through the parent chain
of whatever is chosen). A word on a member line that does not start with `app.` or `data.` is
ignored, and a member id missing from the file fails on its own at install time (reported as a chain
with a missing parent) while the other members still install.

**`meta` is optional.** Its form is `meta <id> <name>|<category>|<bytes>|<tagline>`, separated by
pipes because a tagline contains spaces. In the shipped file every `app` and `data` row has one,
which is why `kdos app list --all` prints 75 entries: the 73 applications and the 2 data rows. A
row without one presents as its own id, category `Other`, no tagline and size 0, so adding software
is a one-line change. The size is an estimate and every surface labels it as one: what apt resolves
on the day depends on the snapshot.

**`env` rows are declarations the launch path does not read.** A runtime that installs a Qt
platform theme declares the variable that selects it (`env rt-qt QT_QPA_PLATFORMTHEME=gtk3`), and
an application may add a line of its own. A store box receives none of these rows: the variables a
box is given come from pack metadata, as described under
[The environment a box is given](#the-environment-a-box-is-given).

### The graphics stack is in the base row

The graphics drivers sit in the base rather than in a runtime. A row's parent chain is a single line
(the browser sits on the GTK runtime, the video editor on the media one), so drivers placed in
either runtime would reach half the catalogue and no more. The DRI drivers, the GL and EGL loaders
and the VA-API drivers (`libgl1-mesa-dri`, `libegl1`, `libgl1`, `libva2`, `va-driver-all`) are
therefore in the `base` row, where they are stored once and every box built on it has them. The
audio client libraries (`libpulse0`, `libasound2-plugins`) are in the base for the same reason.

That placement is the difference between a box that draws and decodes video on the graphics card
and one that does both on the CPU. The render nodes are bound into every box by default (see
[the `gpu` key](#the-graphics-card-and-the-two-profile-keys)) and the compositor
offers `linux-dmabuf` wherever there is a card; what decides whether video is decoded in hardware
is `libva` plus a `*_drv_video.so` beside it. A browser whose stack has neither reports no hardware
decoder and decodes every frame on the CPU.

## The store lane

`kdos-appbox install <id|group>…` (which `kdos app install` runs) builds each chosen application on
this machine. A chain is built bottom-up as a **stack of images**, one per catalogue row, each
`FROM` the one below:

| Row | `FROM` |
|---|---|
| A base with an `image` line | That image |
| Any other base | `debian:trixie-slim` |
| A runtime, an application or a data row | `kdos/<parent>` |

That is what makes a second GTK application one apt pass instead of three: the base and the runtime
are already images and are skipped. An image already present is never rebuilt; `podman image
exists` is the whole check, because a catalogue row carries no version.

A base may name its own container image, which is what makes a second, non-Debian base cost one
line: `image alpine alpine:3.24.1` beside `base alpine -`. Everything the store lane's generated
build file emits below the `FROM` line is apt, so a base that uses its image as it stands declares
no packages, gets no `RUN` line at all, and the image *is* the result.

The build file is generated in memory and fed to `podman build -f -` with an empty build context
(`/var/empty`), so podman hashes nothing it does not need. A row with packages gets one `ENV` line
and one `RUN` line, so the only layer with content over its parent is the `RUN` diff; the `ENV` line
adds no files. The `RUN`:

1. writes the Debian sources file for the snapshot date, over plain HTTP from
   `snapshot.debian.org`, with `Check-Valid-Until` off because a snapshot's Release file is stale by
   definition. HTTP is deliberate: the base row is the one that installs `ca-certificates`, and an
   apt repository's integrity comes from the archive signature named by `Signed-By`, not from the
   transport;
2. enables the `i386` architecture when a package name ends in `:i386` (`wine32:i386` in
   `rt-wine`);
3. for a `deb` row, fetches the newest release asset matching the pattern and hands it to apt with
   the rest, so its dependencies resolve normally;
4. runs `apt-get update` and `apt-get install -y --no-install-recommends`, then removes the apt
   lists.

`kdos-appbox install --dry-run` prints the build files instead of running them.

A data row an application `needs` is installed first. Once the chain is built, the box is created
with `kdos-box create <id> base=image:kdos/<id>`, and the user's launchers are regenerated (see
[Reaching the desktop](#reaching-the-desktop)). `kdos-box`, the box manager, is `kdos-appbox` under
a second name (a symlink); its verbs are listed in [kdos-appbox](../04-programs/kdos-appbox.md).
Nothing rolls back: six applications where the fourth fails leaves five installed and names the
fourth, because an installed application is not damaged by a later one failing.

`kdos-appbox uninstall` removes the box, then the application's image, then each runtime image
above the base that no remaining box's chain still names. The question is asked of the catalogue
rather than of podman, because a dangling-image sweep cannot tell a runtime nothing uses from one
whose only application is mid-install. The base image is never removed.

## A pack is an image with parts appended

```text
+----------------------------+  offset 0
|  EROFS image (zstd)        |  mounted as-is
+----------------------------+  erofs_len
|  metadata blob             |  flat key = value       ≤ 1 MiB
+----------------------------+  meta_off + meta_len
|  icon.png                  |  the application's mark ≤ 4 MiB
+----------------------------+  icon_off + icon_len
|  signature block           |  one line per signature ≤ 64 KiB
+----------------------------+  sig_off + sig_len
|  footer (512 bytes)        |  magic, format, flags, the image's length,
|                            |  three offset/length pairs, payload_sha256
+----------------------------+  end of file
```

Appending rather than prepending is what makes this simple. EROFS records the image's extent in its
own superblock and never reads past it, so the file is a mountable filesystem exactly as it sits,
whatever follows the image. Userspace seeks to `filesize - 512`, reads the footer, and finds
everything else from there. No offset has to be passed around separately, and the kernel needs to
know nothing about the format.

The footer is packed byte by byte, little-endian, rather than written as a C structure, because a
pack travels between machines and a structure on disk carries one compiler's padding:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | Magic, `KDOSPACK` |
| 8 | 4 | `format` |
| 12 | 4 | `flags` |
| 16 | 8 | `erofs_len` |
| 24 | 16 | `meta_off`, `meta_len` |
| 40 | 16 | `icon_off`, `icon_len` |
| 56 | 16 | `sig_off`, `sig_len` |
| 72 | 32 | `payload_sha256` |
| 104 | 408 | Zero |

Three file extensions belong to the format:

| Extension | What it is |
|---|---|
| `.kpack` | One pack |
| `.kdelta` | The difference between two packs (`zstd --patch-from`) |
| `.ktar` | An exported set of packs in one tar file, to hand to another machine |

### Rules the format keeps

**A pack that does not parse whole is absent, never partial.** A short footer, a wrong magic
number, a format from the future, an offset past the end of the file, a section that starts before
the one ahead of it ends: each answers "there is no pack here" rather than handing back half a
description.

**Each section has a size limit of its own**, as well as the file's length: metadata 1 MiB, icon
4 MiB, signature block 64 KiB. A root daemon reads all three whole into memory before anything
about the pack is authenticated, so "it fits in the file" would give an attacker the budget rather
than bound it.

**The signature block ends exactly where the footer begins.** Any gap there would make a later
`kdos-pack sign` silently do nothing: it appends its line at `sig_off + sig_len` and writes a new
footer straight after, landing in the middle of the file, while the old footer at the end, still
naming the old `sig_len`, is the one a reader finds. A footer that leaves a gap is therefore
rejected as not a pack.

**The payload hash is checked before the signature means anything.** The signature covers a small
subject, `kdos-pack-1\n<id>\n<payload sha256>\n`, so verification never holds a
several-hundred-megabyte file in memory, and the subject names the pack's id as well as its bytes,
so a signature cannot be lifted onto the same content under another id. That binds the signature to
the bytes *only* because the bytes were hashed first. The two failures are reported separately,
because a person told "bad signature" when the truth is "bad hash" goes looking for a key problem
that does not exist.

**From format 2 the hash covers the footer too**, with `payload_sha256` and `sig_len` zeroed. The
footer says where the filesystem, the metadata and the icon are. A hash that stops at `sig_off`,
which is all a format 1 pack's digest covers, lets those offsets be re-pointed (for example,
`meta_off` into the payload) while the signature still verifies, because it is over bytes nobody
moved. For a format 1 pack, only the `C:` hash of a signed index (see [The index](#the-index))
covers the footer, since that hash is over the whole file. The two zeroed fields are the two written
after the hash is taken: the digest itself, and the length that grows each time another key signs an
already-signed pack.

**The format number says which span the digest covers**, and it is read from the pack rather than
assumed. Format 1 covers `[0, sig_off)`; format 2 covers that plus the footer. Every format from
the oldest one still read (1, `KPK_FORMAT_MIN`) up to the current one is verified the way it
declares, so a pack published under an older format still mounts. A pack written by this tree
declares format 2 (`KPK_FORMAT`).

This rule protects every pack already in use. If the covered span were widened without a new format
number, every existing pack would answer `HASH`, `kdos-packd` would mount nothing, no box would
compose, and every application in them would stop opening, which is indistinguishable from
corruption. `kdos-pack restamp <pack>…` is both the repair and the upgrade: it raises the footer to
`KPK_FORMAT`, takes the digest over that format's span, and drops the signature block, since the
block names the digest it replaced. The pack is then signed and its directory indexed again. A pack
already at the current format with an agreeing digest is left alone, signature included.

**Nothing in the pack library mounts, executes or writes outside the file it was given.** A root
daemon links `libkpack`, so every line of it runs as root; the library links `libkbase`, `libksig`
and `libkpkg` and nothing else.

### The metadata blob

The metadata is flat `key = value` text, the same shape as a port recipe, so a person can read a
pack's description with `tail -c` and a pager. Repeatable keys appear once per value. An unknown key
is ignored, so a pack written by a newer KDOS stays readable; a line longer than 1023 bytes is
dropped rather than cut in half. `kdos-pack extract-meta` prints the canonical rendering, which
parses back to the same values.

| Key | Repeats | Meaning |
|---|---|---|
| `id` | no | The pack's identity: `[A-Za-z0-9._-]`, not starting with `.` or `-`, under 64 bytes. It becomes a filename in the pack store, so this rule is about safety |
| `kind` | no | `base`, `runtime`, `app` or `data` |
| `name`, `summary`, `description`, `category`, `licence` | `description` only | What `kdos-store` and `kdos-pack info` show |
| `version`, `release`, `arch` | no | `version` is required; `release` defaults to `1` and `arch` to `x86_64` |
| `requires` | yes | Another pack this one sits on: `rt-gtk`, or `rt-gtk >= 1` with `>=`, `>`, `=`, `<=` or `<` |
| `provides` | yes | A name a `requires` line may name instead of an id |
| `desktop`, `mime`, `command` | yes | What the application offers, for a program that lists a pack without mounting it |
| `needs` | yes | A data pack the application is useless without |
| `graft`, `boxgraft` | yes, up to 8 | Data packs only: where the contents should appear (see [Grafts and data packs](#grafts-and-data-packs)) |
| `env` | yes, up to 8 | `NAME=VALUE` exported into a box whose stack includes this pack; `NAME` must be letters, digits and `_` |
| `size`, `installed`, `launch_cold`, `recommended` | no | The pack file's size and its unpacked size in bytes, a measured cold-launch time in milliseconds, and whether an installer preselects it |

`kdos-pack build` refuses metadata that fails validation: a bad id, an unknown kind, no version, a
`requires` name that is not a valid id, a data pack declaring a desktop entry or a command (a data
pack is mounted `noexec`, so either would name something that can never run), a `graft` on a pack
that is not data, a graft path containing `..`, a `graft` destination starting with `/`, or a
`boxgraft` destination that is neither an id nor a `~/` path.

### Signature outcomes

| Outcome | Meaning | Accepted |
|---|---|---|
| `GOOD` | Verified against a trusted key | yes |
| `NONE` | No signature block at all | yes, unless `KDOS_REQUIRE_SIG` is set |
| `HASH` | The payload hash does not match | no |
| `BAD` | Signed, and no trusted key verifies the signature | no |
| `NOKEY` | Signed, and this machine holds no pack key at all | no |

Trusted pack keys are the `*.pub` files in `/etc/kdos/keys/packs`. KDOS ships one there,
`kdos-packs.pub`. It is a separate directory from `/etc/kdos/keys`, the host package ring, because
a keyring is a flat read of `*.pub`: a key that vouches for a set of packs must not thereby vouch
for a package repository. The `KDOS_KEYS` environment variable points `kdos-pack` at another
directory.

`NOKEY` is its own outcome rather than a forgery, and it is decided before the signature block is
even read. With an empty keyring every signature fails and every failure looks the same, so this
answer names the *machine* rather than the file. Reporting it as tampering would send you to inspect
the pack instead of the key directory. A signature that no key in a non-empty keyring verifies is
`BAD`.

This leaves an asymmetry: an unsigned pack is accepted, so a pack that is signed but cannot be
checked is treated more harshly than the same pack with no signature at all. `KDOS_REQUIRE_SIG=1`
in the environment of `kdos-packd` (at install) or `kdos-pack verify` makes `NONE` a refusal too.

### The index

`kdos-pack index <dir>` writes `PACKAGES`, an index of every `.kpack` and `.kdelta` in a directory,
in the same single-letter shape `kpkg` uses for the host
[binhost](../06-reference/glossary.md): one stanza per pack, a blank line
between, sorted by id so the same set of packs always produces the same bytes.

| Field | Meaning |
|---|---|
| `P:` | The id |
| `V:` | `version-release` |
| `A:`, `K:` | Architecture and kind |
| `S:` | The pack file's size |
| `C:` | The sha256 of the whole pack file |
| `F:` | The file name, relative to the index |
| `O:` | On a delta only: the pack file it patches |
| `R:` | `yes` when the pack asks to be preselected |
| `T:` | The pack's one-line summary |

The index also carries each pack's display name (`N:`), category (`G:`) and the names of the packs
it requires (`D:`), so a reader that links no pack library can present and plan a selection from one
flat file. With `--sign key`, one signature in `PACKAGES.sig` covers the index, and through each
`C:` hash every pack in it, which is why a pack in a signed index needs no signature of its own. An
index holds at most 512 entries; past that it warns that the rest are not indexed.

## Building a pack

`kdos-pack` builds, inspects, signs, indexes and diffs packs. It is installed on every KDOS machine
(`/usr/bin/kdos-pack`), because capturing a working box as a pack is something you do on the
machine you are working on. It executes `mkfs.erofs` and `zstd` through an argument vector and
never through a shell.

| Command | Does |
|---|---|
| `build <dir> <meta> <out.kpack> [--icon FILE]` | Make the filesystem image from a directory and wrap it |
| `assemble <image> <meta> <out.kpack> [--icon FILE]` | Wrap an image somebody else already made |
| `image <pack> <out.erofs>` | Extract the filesystem image alone |
| `info <pack>` | Print the metadata, sizes and signature state |
| `imagehash <pack>` | Hash the image portion alone |
| `uuid <id>` | Print the image UUID derived from a pack id |
| `extract-meta <pack> [--icon]` | Write the metadata blob, or the icon, to standard output |
| `sign <pack> <key>` | Append a signature |
| `verify <pack> [--keys DIR]` | Check the hash and the signatures; exits 2 on `HASH`, `BAD` or `NOKEY` |
| `restamp <pack>…` | Raise the footer format and re-take the digest |
| `index <dir> [--sign key]` | Write the `PACKAGES` index for a directory of packs |
| `delta <old> <new> [out.kdelta]` | Write the difference between two packs |
| `apply <old> <delta> <out.kpack>` | Rebuild a pack from a delta |
| `keygen <name>` | Make an Ed25519 key pair, `<name>.key` (mode `0600`) and `<name>.pub` |

The icon must be a PNG; anything else is refused at build time, because an unusable picture would
still take the icon slot and the launcher would never fall back to its glyph. The signature block
is empty when a pack is built: signing is a separate act, often on a separate machine.

`build` and `assemble` are separate for a reason beyond convenience. Making the filesystem image of
a container layer must run as root, because only root preserves the overlay deletion markers
and `trusted.overlay.*` extended attributes that layer depends on. Wrapping an existing image is an
ordinary file operation anyone can do.

### Reproducibility

Four `mkfs.erofs` settings make a pack a function of its inputs rather than of whoever ran the
build (the full command is `mkfs.erofs -zzstd -b 4096 -T <epoch> --all-time --force-uid=1000
--force-gid=1000 -U <uuid>`):

| Setting | Without it |
|---|---|
| `-T $SOURCE_DATE_EPOCH --all-time` | Every inode carries the second it was packed. The epoch defaults to `1735689600`, the value the build phases export |
| `--force-uid=1000 --force-gid=1000` | The builder's own user id rides along |
| `-b 4096` | The block size follows the page size, so the same tree on a machine with larger pages gives a different image |
| `-U <derived from the pack id>` | The UUID is random per build |

The UUID is derived by hashing the pack id alone; the version is not part of it. The UUID lands in
the filesystem superblock, so a UUID that changed with the version would make every rebuild a
different image even when no file inside had moved, and `imagehash`, which is how you ask whether a
rebuild changed anything, could never answer "unchanged". The self-test (`testing/selftest.sh`)
packs the same tree under two versions and fails if the two hashes differ. Two versions of one pack
therefore share a UUID, which costs nothing, because nothing mounts a pack by UUID.

`imagehash` covers the image and not the metadata, so it answers only half the question. A metadata
line changes what the pack *declares* without changing a byte of its filesystem, so anything
deciding whether to keep an existing pack has to compare both, or it will keep a pack on which a
newly declared command does not exist.

### Deltas

`kdos-pack delta` runs `zstd -19 --patch-from=<old>` over two packs and prints how much smaller the
delta is than the pack. It works although a pack is already compressed, because EROFS compresses
per cluster: blocks whose contents did not change come out as identical compressed bytes. With no
output name, a delta is called `<new>--from--<old>.kdelta`, which is the form `kdos-pack index`
parses back into an `O:` stanza whose id and version are those of the pack it reconstructs. A delta
carries no signature of its own. `kdos-pack apply` rebuilds the pack and does not check it: it
prints a reminder to compare the result with the `C:` hash in the signed index, and a tampered delta
produces a pack that fails that comparison. Nothing in the tree applies deltas automatically.

## Where a pack comes from

No applications are baked onto the installation medium. The ISO carries the catalogue; the only pack
it can carry is the opt-in KDOS base pack described below. The packaging step
`script/06_packaging/01_packs.sh` creates the pack store's two directories,
`/var/lib/kdos/packs/staging` at mode `01777` and `/var/lib/kdos/packs/mnt`. Staging is the one
place an unprivileged write may land. Its mode is set at build time as well as by the daemon, so a
first boot does not refuse an import because `kdos-packd` has not yet run.

Packs are made in these ways:

| Route | Makes |
|---|---|
| `kdos-appbox export <file.ktar> <id\|group>…` | One flattened pack per chosen application or data row that this machine has built, tarred into one file to hand over |
| `kdos-box freeze <name> [out.kpack]` | A box's writable upper layer: only what you changed, which by construction holds nothing its base already has |
| `KDOS_PACK_KDOS=1` at build time | The KDOS root filesystem itself as the base pack `kdos`, so `kdos-box create ports base=pack:kdos` gives a running KDOS a clean KDOS to build ports in |

And they are brought in with:

| Command | Takes |
|---|---|
| `kdos-appbox import <file.ktar> [<id>…]` | An exported set, or the named members of one |
| `kdos-box import <file.kpack> [as <name>]` | One pack; with `as <name>`, also creates a box on it |

Both copy the file into staging and ask `kdos-packd` to install it, because verification happens in
the daemon and not in the client. `kdos-appbox import` then creates one box per imported pack with
`kdos-box create <id> base=pack:<id>`, and a member the daemon refuses is named and skipped while
the rest of the archive still imports.

An export is flattened. For each application, a container is created over the built image,
`podman export` writes its whole merged filesystem, and `kdos-pack build` wraps that. An exported
pack therefore contains its base and runtime as well as the application, carries no `requires`
line, and carries no deletion markers of any kind, so nothing has to agree about how a deletion is
spelled and no step needs root. Its metadata holds the id, the kind, a version equal to the export
time in seconds, the display name and the tagline. The archive also contains a `SELECTION` file,
one id per line with a `group` line recording each group that was picked, which is what `import`
reads when no ids are named, and a `PACKAGES` index, signed when the `KDOS_PACK_KEY` environment
variable names a readable key and unsigned, with a message saying so, otherwise.

A frozen box requires its base. `kdos-box freeze` writes metadata with the id `box.<name>`,
kind `app`, and a `requires` line naming the box's base pack when the base is a pack, so an import
composes the frozen layer over the same base rather than producing a box missing its own libraries.
A box running with an ephemeral upper (see [Composition](#composition)) is frozen from that upper,
with a warning.

The root-filesystem pack is opt-in because it costs a second `mkfs.erofs` over the whole tree on
every build, for a base most machines will never use. The flag is passed by the `Makefile` and named
in `script/chroot_exec.sh`. The pack is written to `build/kdos-base/` rather than into the pack
store: the pack store is *inside* the root filesystem, so a pack written there would be squashed
into `system.sfs` and the medium would carry the tree twice. `script/06_packaging/02_iso.sh` copies
it to `/packs/kdos.kpack` on the ISO9660 filesystem, beside `system.sfs`. A booted medium appears at
`/mnt/iso`, and `kdos-packd` scans `/mnt/iso/packs` for `*.kpack`, so the pack is usable with
nothing installed. The copy is gated on the flag, not on the file being present, and `01_packs.sh`
deletes `build/kdos-base/` whenever the flag is off, so a pack left by an earlier opt-in build does
not reach later images.

### Building the root-filesystem pack

Three rules govern how the root-filesystem pack is built. The first two are about excluding paths,
and breaking either wastes real disk space or leaves a base that cannot start; the third is about
ownership. None of the three produces an error that names its cause.

**A base pack must keep its mount points.** Excluding a *directory* removes it, and a container
root with no `/proc`, `/sys`, `/dev`, `/tmp` or `/run` to mount onto cannot start at all: the
container engine reports a missing mount point for a container that was created without complaint.
The exclusions are therefore a pattern matching everything *inside* those directories
(`--exclude-regex='^(proc|sys|dev|tmp|run|mnt|media|var/cache|var/log)/'`), not the directories
themselves. What is excluded by path is what should not be there at all: the repository bind mounts
`kdos`, `ports` and `build`, the pack store `var/lib/kdos/packs` (installed packs and their mount
points do not belong in a clean base), and podman's own store under
`home/kdos/.local/share/containers`.

**A path exclusion is relative to the tree and has no leading slash.** It matches an exact literal,
so a leading slash matches nothing, is not an error, and excludes nothing. The packaging step
therefore tests the flags against a throwaway tree first, with nested and top-level paths, and
refuses to pack at all if a 2 MB directory it excluded comes back.

**A hand-made image must still force ownership.** A box runs with `--userns keep-id`, so the
process inside it is uid 1000. Every pack `kdos-pack build` makes forces uid and gid 1000 so that
user owns the tree, and the root-filesystem pack passes the same flags directly, with its UUID from
`kdos-pack uuid kdos`. Packed with its *real* ownership from a root-owned tree, the container user
can create nothing anywhere in it, and the box fails in two stages (permission denied on
`/etc/mtab`, then a missing bind destination for `kdos-boxinit`), neither of which mentions
ownership.

## Installing a pack

`kdos-packd` is the only program on the system that installs or mounts a pack. It runs as root under
the service supervisor (`/etc/init.d/59_packd.sh`; see [Boot and init](boot-and-init.md)), answers
`/run/kdos-packd.sock`, and accepts requests from root and members of `wheel`, checked by the peer's
credentials rather than by the socket's mode (`0666`). See [The daemons](../04-programs/daemons.md)
for its full interface.

The client never names a path. Every verb takes an id out of a list the daemon itself
published, and `install` takes a *filename* in the staging directory: no slash, no `..`, only
`[A-Za-z0-9._-]`. A daemon reachable from `wheel` that accepted a path would be `mount /dev/sda2
/etc` from any shell.

`install <file>` then:

1. opens the staged file and validates its metadata;
2. if the staging directory holds a `PACKAGES` index whose `PACKAGES.sig` verifies `GOOD` and which
   names this id, compares the file's sha256 with the index's `C:` hash and accepts or refuses on
   that alone;
3. otherwise verifies the pack itself, accepting `GOOD` and `NONE` (unless `KDOS_REQUIRE_SIG` is
   set) and refusing `HASH`, `BAD` and `NOKEY`;
4. if an older version of the same id is mounted, unmounts it, or refuses the install when a box
   is composed over it, so that no later composition puts a new application over an old runtime's
   bytes;
5. renames the version already installed to `<id>-<version>.kpack`, and moves the new file into the
   pack store as `<id>.kpack`. Staging is inside the pack store so that this is a rename, and
   installation has no half-finished state;
6. deletes superseded copies beyond the `retain` setting in `/etc/kdos/packd.conf` (default `1`),
   newest kept by version comparison. The sweep runs only after an install.

`kdos-appbox import` stages each pack file on its own, without the archive's `PACKAGES`, so an
imported pack is accepted on step 3: its own payload hash and its own signature block.

The `rollback <id>` verb swaps the installed pack with the kept `<id>-<version>.kpack` whose version
sorts last as a string (not by version comparison, unlike the retain sweep), and
`remove <id>` deletes an installed pack. Both refuse while a box is composed over the pack. Neither
has a command-line wrapper; they are reached by writing the verb to the socket.

## Mounting

Packs live in two places, and each is verified where it can be changed:

| Origin | Path | Verified |
|---|---|---|
| The pack store | `/var/lib/kdos/packs` | At install; not re-hashed on mount |
| The medium | `/mnt/iso/packs` | Hashed and signature-checked the first time it is mounted |

A pack in the pack store was hashed when root wrote it, and only root can write there, so it is not
re-hashed. Re-hashing a several-hundred-megabyte base on every mount would cost a full read of a
file the kernel is about to read lazily anyway. A pack on the medium has never been verified and is
mounted straight off the medium with no copy, so it is checked on first mount, which on a slow
USB medium costs seconds. `KDOS_REQUIRE_SIG` is consulted at install only.

That is also why the pack store must be owned by root. If the desktop user could replace a pack in
the pack store, the daemon would mount it unverified, because "it was hashed when root wrote it" is
exactly the reasoning that skips the check.

The daemon re-reads both directories on every request, so a medium pulled out between two
requests is not offered by the second. When one id is in both, the installed copy wins, so a pack
you updated is the one that runs even while the older one is still on the medium.

Every pack is mounted read-only, `nosuid` and `nodev` under `/var/lib/kdos/packs/mnt/<id>`; a data
pack is also `noexec`. There are two routes to the mount, and `kdos-appbox status` prints the one in
use (`route`) from the daemon's `status` reply. Where the kernel has
`CONFIG_EROFS_FS_BACKED_BY_FILE`, it takes the regular file as the filesystem source and no loop
device is needed (`file-backed`); where it answers `ENOTBLK`, the pack goes through a read-only loop
device, set up directly with three `ioctl` calls rather than by running `losetup`, because this is
a root daemon and every process it starts runs as root.

Mounts are reference counted, and taking a box apart does not unmount. Two boxes often share a
runtime; unmounting it because one stopped would pull the layer out from under the other, and every
path in a running application would vanish. A pack nobody is using costs a mount entry and no
memory, because its pages belong to the page cache.

At startup the daemon rebuilds its mount table from `/proc/mounts`, because it can be restarted
while boxes are running, and a daemon that forgot a mount would unmount a live box's own root.

## Composition

A store-built box has nothing to compose. Its root is a container image built on this machine, so
the container engine assembles the layers itself and `kdos-packd` is not involved. Everything in
this section is the import lane.

An imported application's container root is an overlay of the packs its stack resolves to, with a
writable upper layer on top. The client asks `compose <box> <id>` and the daemon solves the
stack: it follows each pack's `requires` lines, matching an id or a `provides` name, choosing the
newest provider when several match and comparing versions with the same routine `kpkg` uses, and
mounts the result dependencies first. A client that worked out its own stack could ask for one whose
requirements are not met, and the application would start in a root missing the library it needs.
An exported pack requires nothing, so its stack is itself; a frozen box's stack is its layer over
its base. A data pack in a stack is refused: it is grafted, never composed.

The overlay's `lowerdir` lists the highest layer first, so the solved order is reversed; the other
way round, the base would shadow the application. The daemon places the upper layer by asking the
filesystem that holds your home directory:

| Where `$HOME` is | Upper layer | Survives a reboot |
|---|---|---|
| ext4, btrfs, XFS or F2FS | `~/.local/share/kdos/boxes/<box>/upper` | Yes |
| tmpfs | `~/.local/share/kdos/boxes/<box>/upper`, on that tmpfs | No: tmpfs is emptied at every boot |
| Anything else, including an overlay (a live session) | `$XDG_RUNTIME_DIR/kdos/boxes/<box>/upper`, on tmpfs | No |

The kernel refuses to stack an upper layer on overlayfs, and a live session's `$HOME` is on the boot
overlay. A filesystem the daemon has not been shown to work on is treated the same way rather than
guessed at. The daemon's `status` reply marks each composed box `persistent` or `ephemeral`, so
losing your work is never silent. `persistent` means the upper layer is in your home directory; a
home on tmpfs is marked `persistent` and still loses the layer at the next boot. The merged root is
mounted at `$XDG_RUNTIME_DIR/kdos/boxes/<box>/root`. The daemon composes at most 32 boxes, each a
stack of at most 32 packs.

There is one box per application, named after the pack. One box composing every installed
application would hit two limits at once: an overlay cannot gain a layer while it is mounted, so
installing an application would restart a container other applications are running in; and a
hundred lower layers do not fit in the 4096 bytes the kernel allows a mount's option string. Per
application, a stack is a few layers, installing one application disturbs nothing else, and the
pages of a shared runtime are shared between boxes because the pack is mounted once. The cost is one
container monitor process per running application.

The merged overlay lives under the runtime directory, a temporary filesystem, so after a reboot the
container's root is gone while the container itself still exists. A box is therefore recomposed
every time before it is started. Composing is idempotent because mounts are reference counted, so a
box already composed costs one round trip to the daemon. Without this, a box would work until the
first reboot and then fail to start, with nothing wrong in the box itself.

## The box

The two lanes create their containers differently:

- **A store box** is a distrobox container created from the image `kdos/<id>` (base
  `image:kdos/<id>`). distrobox is a wrapper that creates and initialises podman containers for
  interactive use, and its own init sets the container up.
- **A pack box** (base `pack:<id>`) is created by `podman create --rootfs` over the merged overlay
  directly, with no image. It is labelled `manager=kdos-box`, runs `--userns keep-id`, and uses
  `kdos-boxinit` as its init. A box composed on first launch records `base=pack:<id>` in its
  profile, so every tool that asks what a box is made of gets the pack rather than an image name.

In both lanes podman handles the hard parts of a container (user-namespace mapping, seccomp,
cgroups, and the `conmon` monitor process that KDOS tools identify a box by), and a launch waits for
the same readiness marker, `container_setup_done`, in either lane.

A pack box is created with the flag set distrobox uses for the same job, so an application behaves
the same in either lane. The box shares a user namespace with you (`--userns keep-id`), so the files
in your home belong to the same user inside and out. It shares the host's network, IPC and process
namespaces unless its profile asks for its own, because the session bus, the Wayland socket and
PipeWire live in `/run/user/<uid>`, which is bound in with `/tmp`, `$HOME` and `/dev/shm`. It runs
with SELinux labelling disabled and AppArmor unconfined, because the host runs neither and a
profile that is not loaded is a denial rather than a policy. `/run` and `/run/lock` are tmpfs,
because a pack is read-only and `/run` must be writable. The exact flag set, and what each flag is
for, is in
[kdos-appbox: What a pack box is created with](../04-programs/kdos-appbox.md#what-a-pack-box-is-created-with).

The container runs with your own user identity mapped in, so applications see a real non-root
account; many applications refuse to run as root. Ownership grants nothing either way, because
packs are mounted `nosuid`.

**`kdos-boxinit`** is installed at `/usr/libexec/kdos/kdos-boxinit` and bind-mounted read-only into
every pack box at `/usr/libexec/kdos-boxinit`, where it runs as process 1. It:

1. writes records for a user and group matching yours into `/etc/group` and `/etc/passwd`, and a
   locked password into `/etc/shadow`, since nothing logs in to a box;
2. sets a search path that includes `/usr/games`, and `HOME` to your home directory;
3. writes `/usr/local/bin/xdg-open`, which hands links and files to the host's OpenURI portal over
   the shared session bus, and links `x-www-browser` and `sensible-browser` to it;
4. writes `/etc/asound.conf` where the PulseAudio ALSA plugin exists, so ALSA's `default` in the box
   is the host's sound server;
5. prints `container_setup_done`, which the launcher waits for, only after the records above exist;
6. stays alive, reaping exited processes, until it is told to stop.

It is statically linked because it runs in a container whose libraries are Debian's, where a
binary linked against the host's C library would look for its loader and not find it.

`kdos-boxinit` replaces a pack's existing record for your user rather than keeping it. A base pack
shipping an account whose home directory is `/` would otherwise give every application `HOME=/`, so
none reads any configuration in your real home, and a boxed application comes up in its toolkit's
default light theme on the dark KDOS desktop, with the palette sitting correctly in a home it never
looked at.

### The graphics card and the two profile keys

The graphics card reaches a box as device nodes, and the box profile's two keys govern different
halves of it. (A box profile is `~/.config/kdos/boxes/<name>.conf`; see
[kdos-appbox](../04-programs/kdos-appbox.md).)

`gpu` is the device nodes. A box that shares the host's `/dev` has them, and nothing can be
subtracted from that; a box with a private `/dev` gets `/dev/dri` bound back by this key alone.

`render` is who draws. `auto`, the default, is resolved against the machine by opening a render node
(`/dev/dri/renderD*`), not by checking that one exists, because a session outside the `render`
group reaches the same dead end as a machine with no card. `software` (or `pixman`, `no`, `off`,
`0`, `false`) is a refusal honoured whatever is plugged in. A launch that resolves to software gets
`LIBGL_ALWAYS_SOFTWARE=1`. The key is advisory, an environment variable the application may unset,
and it governs GL and EGL only. VA-API and Vulkan find the render node their own way, which is why
that is the `gpu` key's job.

A boxed client draws with its own Mesa and connects to `kdos-comp` as an ordinary Wayland client on
the socket named in `WAYLAND_DISPLAY`. Both keys default to the card, because the nodes and the
drivers are already there and a box drawing in software on a machine with a render node gains
nothing.

That socket is a per-box one held open by `kdos-boxsock`, a small per-box helper that binds it
with the `security-context-v1` Wayland protocol. The compositor tags every client that connects on
it, and that tag is what the compositor's allowlist uses to filter screen capture (screencopy),
clipboard managers (data-control), input methods and layer-shell surfaces (panels and overlays).
The tag filters those protocols; it does not isolate the box. The box shares
`$XDG_RUNTIME_DIR` with the session, so a client that opens the default `wayland-0` reaches the
session's own socket. Withholding the variable would advertise a confinement the sandbox does not
have, which is why no profile key withholds it. See [The security model](security-model.md).

### The environment a box is given

Every launch exports the `env =` lines from pack metadata. `kdos-appbox` asks `kdos-packd` for the
metadata of one pack and walks outward from it along `requires`, up to eight packs. The nearest
pack wins, and a value from further out is dropped rather than exported, so each variable is set
once and an application can override its runtime. The pack asked for is the one named by the box's
`pack:` base or, for any other box, the pack whose id is the box's name.

A store box has no such pack, so it gets none of these variables. It also gets none of the
catalogue's `env` rows, and no `QT_QPA_PLATFORMTHEME` chosen from image labels: the program holds
both lookups, but every launch reaches the pack lookup first with a non-empty id, and the others
sit behind it. The full list of what a box receives is in
[kdos-appbox: The environment a box gets](../04-programs/kdos-appbox.md#the-environment-a-box-gets).

A value beginning with `$HOME` has it replaced by your home directory, because a pack cannot know
your user name.

## Grafts and data packs

A **data pack** carries a dataset rather than a program, such as KiCad's 3D models or Tesseract's
language files. It is mounted read-only, `nosuid`, `nodev` and `noexec` (a data pack that ships
a binary cannot run it), and it is never composed into a container root.

What makes a data pack findable is its **grafts**: declared placements that the daemon carries out
when asked `graft <id>`. A pack contains no scripts, and no shell runs near one. There are two
graft namespaces, because the host's data directories are invisible inside a box:

| Key | Lands | For |
|---|---|---|
| `graft` | A symlink under `/usr/share` | Programs on the host |
| `boxgraft` | A symlink under `~/.local/share/kdos/packs/<name>` | Boxed applications, which share your home |

A `boxgraft` destination beginning with `~/` lands at that place in your home instead, for a program
that reads its own fixed directory and takes no variable. The shipped catalogue declares a host
graft for the KiCad models, where the natively built KiCad looks, and both kinds for the Tesseract
languages:

```text
graft    data.kicad-packages3d  usr/share/kicad/3dmodels  kicad/3dmodels
graft    data.tesseract-langs   usr/share/tesseract-ocr/5/tessdata  tesseract-ocr/5/tessdata
boxgraft data.tesseract-langs   usr/share/tesseract-ocr/5/tessdata  tessdata
```

A graft never replaces something that is not a symlink: an existing real file or directory at the
destination is left alone and logged. Every graft made is recorded in
`/var/lib/kdos/pack-manifest`, one `<id><TAB><path>` line each, so `ungraft <id>` removes exactly
what was added, and only a symlink is removed, because a real directory at that path belongs to
someone else.

The catalogue declares its grafts and the daemon carries them out, but the two are not connected by
anything in the tree. No program sends `graft` or `ungraft` to the daemon, an exported pack's
metadata carries no `graft`, `boxgraft`, `env` or `needs` lines, and in the store lane a data row is
built as an image and becomes a box of its own like any other row. A data set reaches an application
only when both are placed by hand.

## What an install carries

The installer (`kinstall`) copies no applications, and says so before it runs: the medium has none
to copy. What its Applications page chooses is a set of groups, and the installer turns that into
one of four routes, decided in this order:

| Route | When | What the install does |
|---|---|---|
| Nothing chosen | No group ticked | Makes the pack store's directories and stops |
| From removable media | A `.ktar` is found one directory below `/mnt`, `/media` or `/run/media` | Runs `kdos-appbox import` on the first one found, which stages each pack through `kdos-packd`. The only route on a machine with no network |
| Over the network | The machine has a default route | Runs `kdos-appbox install <group>…`: podman and apt, which takes minutes |
| At first login | Neither of the above | Writes the selection to `/var/lib/kdos/apps-pending` and builds nothing |

With a pending selection, the first session shows a notification offering the applications, and
`kdos app install --pending` does the build. It tries to delete the file after a clean run, but it
runs as the user and the file is root's in a root-owned directory, so the file stays until root
removes it; a partial run loses nothing
([Applications chosen during installation](session.md#applications-chosen-during-installation)).
The offer is made once; deleting `~/.config/kdos/apps-offered` brings it back at the next login.

The page shows which route applies before the install starts, because a person must not discover at
first boot that nothing was installed. Building can take most of an hour, and an installer that did
that silently would look hung.

An import or a build that fails falls back to *pending* rather than failing the install. The system
is on the disk and bootable; an archive that would not import is a reason to say so and carry on,
not to abandon a partitioned disk.

## Reaching the desktop

`kdos-appbox genlaunchers` makes installed applications appear in your menus and on your command
line. It has four forms:

| Form | Reads | Writes |
|---|---|---|
| `genlaunchers --packs --user` | Every installed or mounted app pack, mounted through the daemon | Your own tree |
| `genlaunchers --packs <fs-root>` | Every installed or mounted app pack | The system tree under `<fs-root>` |
| `genlaunchers --packs-dir <dir> <fs-root>` | One extracted pack per subdirectory of `<dir>`, named after the pack | The system tree under `<fs-root>` |
| `genlaunchers <desktop-dir> <fs-root>` | One directory of desktop entries | The system tree under `<fs-root>` |

The system tree is `/etc/skel/.local/share/applications`, `/usr/share/kdos/alien-apps` and
`/usr/local/bin`; the build's `script/06_packaging/00_launchers.sh` runs the `--packs-dir` form,
which with no packs on the medium clears any stale launchers (its place in the build is in
[How KDOS is built](../05-developer/how-kdos-is-built.md#packaging-06_packaging)). Your own tree is
`~/.local/share/applications`, `~/.local/share/kdos/alien-apps` and `~/.local/bin`, which every
reader consults first.

The pack forms parse the application's own desktop entries: a pack carries the real ones, so the
existing parser is reused rather than reimplemented against the metadata. Only `app` packs are read,
and only installed or already-mounted ones, so a regeneration never mounts every pack on the medium.
Store boxes have no pack for the daemon to mount. After a store install, uninstall or import,
`kdos-appbox` copies `/usr/share/applications` out of every store box's image with `podman cp` and
regenerates your tree from those in the `--packs-dir` shape; when no store box exists, it walks the
installed packs instead. The two sources are not merged in one run. `kdos-box export <box> <app>`
adds a launcher for one application in any box.

Each run writes its whole set, because the table, the cache and the shims are written whole, and
dropping any output breaks something visible:

| Output | Without it |
|---|---|
| A desktop entry per application, marked `X-KDOS-Alien=true` | No launcher |
| A MIME cache (`mimeinfo.cache`) beside them | File-type associations are never consulted |
| A name-to-command table (`alien-apps`) | The shim cannot find what to run or in which box |
| A shim per application in the binary directory | The application is not a command |

The details (entry naming, quoting, field codes, the tables that skip or rename entries) are in
[kdos-appbox](../04-programs/kdos-appbox.md).

## See also

- [Applications](../02-user-guide/applications.md) — installing and using applications
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the store and the box manager
- [The daemons](../04-programs/daemons.md) — `kdos-packd`'s verbs, files and environment
- [The security model](security-model.md) — mount options, signing and the sandbox
- [Packaging](packaging.md) — the host's separate packaging system
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#applications) — this model compared with
  Flatpak, Snap and distribution packages
- [How KDOS is built](../05-developer/how-kdos-is-built.md#packaging-06_packaging) — the packaging
  phase that writes launchers and, on request, the base pack
- [The ports catalogue](../06-reference/ports-catalogue.md) — the host's recipes, which this
  catalogue is not
- [Glossary](../06-reference/glossary.md) — pack, box, graft, runtime and the other terms

<!-- book-nav -->
---

*Part III — Architecture, chapter 16.* Previous: [15. Packaging](packaging.md) · [Contents](../README.md) · Next: [17. The security model](security-model.md)
