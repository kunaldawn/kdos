# Packs and boxes

This page explains how KDOS packages and runs applications: how an application
is built from the catalogue, what a **pack** is, how a pack is verified and
mounted, and how either kind of **box** reaches your desktop. It is for
administrators who want to know what happens when an application is installed,
and for contributors working on `kdos-appbox`, `kdos-pack`, `kdos-packd` or the
catalogue.

Three terms carry the whole page:

| Term | Meaning |
|---|---|
| **Box** | A rootless container that one application (or a working environment) runs in. It shares your home directory with the host and little else |
| **Pack** | A single file holding a read-only filesystem image (EROFS) with metadata, an icon and signatures appended. Extension `.kpack` |
| **Catalogue** | The shipped list of every application KDOS knows how to build, as Debian packages |

This is a separate system from [host packaging](packaging.md) because it
answers a different question. Host packaging answers *what is installed on this
machine*. This answers *what software can this machine run, and how do I get a
piece of it without installing anything into the system*.

If you only want to install and use applications, read
[Applications](../02-user-guide/applications.md) first; this page is the
machinery underneath it.

## Two lanes, one box

Two routes reach a box:

- **The store** builds a stack of container images from the catalogue on your
  machine and creates a box from the top one.
- **An import** installs signed packs someone else exported, and composes a box
  from an overlay of them.

The lanes differ in where the bytes came from and what vouches for them. From
the container root upward they behave the same.

| | Store | Import |
|---|---|---|
| Source | Debian archive, over the network | A `.ktar` or `.kpack` somebody handed you, or a pack on the boot medium |
| Verified by | Nothing KDOS controls — apt checks the archive, and the resulting images are unsigned | Payload hash and signature, at the mount |
| Costs | Minutes of apt | A copy |
| Box base | `image:kdos/<id>` | `pack:<id>` |

## A pack is an image with parts appended

```
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

| Extension | What it is |
|---|---|
| `.kpack` | One pack |
| `.kdelta` | The difference between two packs (`zstd --patch-from`) |
| `.ktar` | An exported set of packs in one file, to hand to another machine |

Appending rather than prepending is what makes this simple. EROFS records the
image's extent in its own superblock and never reads past it, so the file is a
mountable filesystem exactly as it sits, whatever follows the image. Userspace
seeks to `filesize - 512`, reads the footer (magic `KDOSPACK`), and finds
everything else from there. No offset has to be passed around separately, and
the kernel needs to know nothing about the format.

### Rules the format keeps

**A pack that does not parse whole is absent, never partial.** A short footer, a
wrong magic number, a format from the future, an offset past the end of the
file, a section that starts before the one ahead of it ends — each answers
"there is no pack here" rather than handing back half a description.

**Each section has a size limit of its own**, as well as the file's length:
metadata 1 MiB, icon 4 MiB, signature block 64 KiB. A root daemon reads all
three whole into memory before anything about the pack is authenticated, so
"it fits in the file" would give an attacker the budget rather than bound it.

**The signature block ends exactly where the footer begins.** Any gap there
makes a later `kdos-pack sign` silently do nothing: it appends its line at
`sig_off + sig_len` and writes a new footer straight after, landing in the
middle of the file, while the old footer at the end — still naming the old
`sig_len` — is the one a reader finds.

**The payload hash is checked before the signature means anything.** The
signature covers a small subject containing the pack's id and its hash, so
verification never holds a several-hundred-megabyte file in memory — and that
binds the signature to the bytes *only* because the bytes were hashed first.
The two failures are reported separately, because a person told "bad
signature" when the truth is "bad hash" goes looking for a key problem that
does not exist.

**From format 2 the hash covers the footer too**, with `payload_sha256` and
`sig_len` zeroed. The footer says where the filesystem, the metadata and the
icon are. A hash that stops at `sig_off` — which is all a format 1 pack's
digest covers — lets those offsets be re-pointed (`meta_off` into the payload,
say) while the signature still verifies, because it is over bytes nobody moved.
A format 1 pack therefore relies on the signed index that travels with it (the
list of every pack's hash that an export writes and signs): each index entry
records the sha256 of the whole pack file (its `C:` field), footer included. The two zeroed fields are the two
written after the hash is taken: the digest itself, and the length that grows
each time another key signs an already-signed pack.

**The format number says which span the digest covers**, and it is read from the
pack rather than assumed. Format 1 covers `[0, sig_off)`; format 2 covers that
plus the footer. Every format from the oldest one still read (1,
`KPK_FORMAT_MIN`) up is verified the way it declares, so a pack published under
an older format still mounts. A pack written by this tree declares format 2
(`KPK_FORMAT`).

This rule protects every pack already in use. If the covered span were widened
without a new format number, every existing pack would answer `HASH`,
`kdos-packd` would mount nothing, no box would compose, and every application in
them would stop opening — indistinguishable from corruption.
`kdos-pack restamp <pack>…` is both the repair and the upgrade: it raises the
footer to `KPK_FORMAT`, takes the digest over that format's span, and drops the
signature block, since the block names the digest it replaced. The pack is then
signed and its directory indexed again. A pack already at the current format
with an agreeing digest is left alone, signature included.

**Nothing in the pack library mounts, executes or writes outside the file it was
given.** A root daemon links it, so every line of it runs as root.

### Signature outcomes

| Outcome | Meaning | Mounts? |
|---|---|---|
| `GOOD` | Verified against a trusted key | yes |
| `NONE` | No signature block at all | yes |
| `HASH` | The payload hash does not match | **no** |
| `BAD` | Signed, and no trusted key verifies the signature | **no** |
| `NOKEY` | Signed, and this machine holds no pack key at all | **no** |

Trusted pack keys are the `*.pub` files in `/etc/kdos/keys/packs`.

`NOKEY` is its own outcome rather than a forgery, and it is decided before the
signature block is even read. With an **empty** keyring every signature fails
and every failure looks the same, so this answer names the *machine* rather
than the file. Reporting it as tampering would send you to inspect the pack
instead of the key directory. A signature that no key in a non-empty keyring
verifies is `BAD`.

Note the asymmetry this leaves: an **unsigned** pack mounts, so a pack that is
signed but cannot be checked is treated more harshly than the same pack with no
signature at all.

## Building a pack

`kdos-pack` builds, inspects, signs, indexes and diffs packs. It is installed on
every KDOS machine (`/usr/bin/kdos-pack`), because capturing a working box as a
pack is something you do on the machine you are working on.

| Command | Does |
|---|---|
| `build <dir> <meta> <out.kpack> [--icon FILE]` | Make the filesystem image from a directory and wrap it |
| `assemble <image> <meta> <out.kpack> [--icon FILE]` | Wrap an image somebody else already made |
| `image <pack> <out.erofs>` | Extract just the filesystem image |
| `info <pack>` | Print the metadata |
| `imagehash <pack>` | Hash the image portion alone |
| `uuid <id>` | Print the image UUID derived from a pack id |
| `extract-meta <pack> [--icon]` | Write the metadata blob (or the icon) out on its own |
| `sign <pack> <key.sec>` | Add a signature |
| `verify <pack> [--keys DIR]` | Check the hash and signatures |
| `restamp <pack>…` | Raise the footer format and re-take the digest |
| `index <dir> [--sign key.sec]` | Write the `PACKAGES` index for a directory of packs |
| `delta <old> <new> [out.kdelta]` | Write the difference between two packs |
| `apply <old> <delta> <out.kpack>` | Rebuild a pack from a delta |
| `keygen <name>` | Make an Ed25519 pack-signing key pair |

`build` and `assemble` are separate for a reason beyond convenience. Making the
filesystem image of a container layer must run as **root**, because only root
preserves the deletion markers and extended attributes that layer depends on.
Wrapping an existing image is an ordinary file operation anyone can do.

### Reproducibility

Four `mkfs.erofs` flags make a pack a function of its inputs rather than of
whoever ran the build:

| Flag | Without it |
|---|---|
| `-T $SOURCE_DATE_EPOCH` | Every inode carries the second it was packed |
| `--force-uid=1000 --force-gid=1000` | The builder's own user id rides along |
| `-b 4096` | The block size follows the **page size**, so the same tree on a machine with larger pages gives a different image |
| `-U <derived from the pack id>` | The UUID is random per build |

The version is not part of that UUID. The UUID lands in the filesystem
superblock, so a UUID that changed with the version would make every rebuild a
different image even when no file inside had moved — and `imagehash`, which is
how you ask whether a rebuild changed anything, could never answer "unchanged".
The self-test packs the same tree under two versions and fails if the two hashes
differ.

`imagehash` covers the image and not the metadata, so it answers only half the
question. A metadata line changes what the pack *declares* without changing a
byte of its filesystem, so anything deciding whether to keep an existing pack
has to compare both, or it will keep a pack on which a newly declared command
does not exist.

## The catalogue

`src/packages/kdos-appbox/catalogue` defines what can be built, and is installed
to `/usr/share/kdos/appstore/catalogue`. The store, the installer and `kdos app`
all read this one file. It holds 2 `base` rows (`base`, which is Debian, and
`alpine`), 7 `runtime` rows (`rt-gtk`, `rt-qt`, `rt-kde` on top of `rt-qt`,
`rt-media`, `rt-sci`, `rt-electron`, `rt-wine`), 180 `app` rows, 2 `data` rows,
and 21 `group` lines defining 7 groups (`creative`, `dev`, `essential`, `games`,
`make`, `office`, `science`).

A row reads `<kind> <id> <parent> <apt packages…>`. The kinds of line:

| Line | Declares |
|---|---|
| `base` | A whole root filesystem |
| `runtime` | A layer over the base, shared by many applications |
| `app` | One application, as a difference over a runtime |
| `data` | A dataset, mounted but never composed into a container |
| `cmd` | A command a row provides that has no graphical launcher |
| `env` | An environment variable a row or runtime needs, `env <row> NAME=VALUE` |
| `deb` | An application Debian does not carry, by releases URL and asset pattern |
| `needs` | A data row an application is useless without |
| `graft`, `boxgraft` | Where a data row's contents should appear |
| `image` | A base that names its own container image |
| `group` | A curated bundle the store and the installer offer as one tick |
| `meta` | Display name, category, size estimate and tagline |
| `snapshot` | Which Debian archive date the packages come from |

**A row's parent must appear above it.** The resolver walks a chain upward in
one pass, so a forward reference is a chain it cannot close; reordering the file
makes an install fail naming the missing parent.

**`snapshot` pins the Debian archive.** The shipped value is `snapshot = auto`,
which takes the date from the base image's own sources file, so what is
installed on top cannot disagree with the root filesystem under it. A literal
date such as `20260824T000000Z` pins explicitly; `off` reads the live archive
and gives up reproducibility. If `auto` cannot find a date, the build proceeds
unpinned and prints a warning saying so.

**A group is not a category.** The category is the application's own, taken
from its desktop entry, and every application has exactly one. A group is
curated, most applications are in none, and a member that is not an `app` or
`data` row makes the install refuse by name.

**`meta` is optional.** A row without one presents as its own id, category
`Other`, no tagline and size 0, so adding software is a one-line change. The
size is an **estimate** and every surface labels it as one: what apt resolves on
the day depends on the snapshot.

## Where a pack comes from

**No applications are baked onto the installation medium.** The ISO carries
the catalogue; the only pack it can carry is the opt-in KDOS base pack described
below. The packaging step
`script/06_packaging/01_packs.sh` only creates the store's two directories —
`/var/lib/kdos/packs/staging` at mode `01777`, and `/var/lib/kdos/packs/mnt`.
Staging is the one place an unprivileged write may land. Its mode is set at
build time as well as by the daemon, so a first boot does not refuse an import
because `kdos-packd` has not yet run.

Packs are made in these ways:

| Route | Makes |
|---|---|
| `kdos-appbox export <file.ktar> <id\|group>…` | One pack per store image this machine has built, tarred into one file to hand over |
| `kdos-box freeze <name> [out.kpack]` | A box's **writable upper layer** — only what you changed, which by construction holds nothing its base already has |
| `KDOS_PACK_KDOS=1` at build time | The KDOS root filesystem itself as the base pack `kdos`, so `kdos-box create ports base=pack:kdos` gives a running KDOS a clean KDOS to build ports in |

And they are brought in with:

| Command | Takes |
|---|---|
| `kdos-appbox import <file.ktar> [<id>…]` | An exported set, or the named members of one |
| `kdos-box import <file.kpack> [as <name>]` | One pack; with `as <name>`, also creates a box on it |

Both copy the file into staging and ask `kdos-packd` to install it, because
verification happens where the mount happens.

An export is flattened: a container is created over the built image, `podman
export` writes its whole merged filesystem, and that is what gets wrapped. An
exported pack therefore carries no deletion markers of any kind, and nothing
has to agree about how a deletion is spelled.

The root-filesystem pack is **opt-in** because it costs a second `mkfs.erofs`
over the whole tree on every build, for a base most machines will never use. It
is written to `build/kdos-base/` rather than into the store: the store is
*inside* the root filesystem, so a pack written there would be squashed into
`system.sfs` and the medium would carry the tree twice. The ISO step copies it
to `/packs/kdos.kpack` on the ISO9660 filesystem, beside `system.sfs`. A booted
medium appears at `/mnt/iso`, and `kdos-packd` scans `/mnt/iso/packs` for
`*.kpack`, so the pack is usable with nothing installed. The copy is gated on
the flag, not on the file being present, so a pack left in `build/` by an
earlier opt-in build does not reach later images.

### Building the root-filesystem pack

Three rules govern how the root-filesystem pack is built. The first two are
about excluding paths, and breaking either wastes real disk space or leaves a
base that cannot start; the third is about ownership. None of the three
produces an error that names its cause.

**A base pack must keep its mount points.** Excluding a *directory* removes it,
and a container root with no `/proc`, `/sys`, `/dev`, `/tmp` or `/run` to mount
onto cannot start at all — the container engine reports a missing mount point
for a container that was created without complaint. The exclusions are
therefore a pattern matching everything *inside* those directories
(`--exclude-regex='^(proc|sys|dev|tmp|run|mnt|media|var/cache|var/log)/'`), not
the directories themselves.

**A path exclusion is relative to the tree and has no leading slash.** It
matches an exact literal, so a leading slash matches nothing, is not an error,
and excludes **nothing**. The packaging step therefore tests the flag against a
throwaway tree first — nested and top-level — and refuses to pack at all if a
2 MB directory it excluded comes back.

**A hand-made image must still force ownership.** A box runs with `--userns
keep-id`, so the process inside it is uid 1000. Every pack `kdos-pack build`
makes forces uid and gid 1000 so that user owns the tree, and the
root-filesystem pack passes the same flags directly. Packed with its *real*
ownership from a root-owned tree, the container user can create nothing anywhere
in it, and the box fails in two stages — permission denied on `/etc/mtab`, then
a missing bind destination for `kdos-boxinit` — neither of which mentions
ownership.

### The graphics stack is in the base row

A base may name its own container image, which is what makes a second,
non-Debian base cost one line: `image alpine alpine:3.24.1` beside
`base alpine -`. Everything the generated build file emits below the image line
is package management, so a base that uses the image as it stands declares **no
packages** and the image *is* the pack.

The graphics drivers sit in the base rather than in a runtime. A pack's parent
chain is a single line — the browser sits on the GTK runtime, the video editor
on the media one — so drivers placed in either runtime would reach half the
catalogue and no more. The DRI drivers, the GL and EGL loaders and the VA-API
drivers (`libgl1-mesa-dri`, `libegl1`, `libgl1`, `libva2`, `va-driver-all`)
are therefore in the `base` row, where they are stored once and every box has
them.

That is the difference between a box that draws and decodes video on the
graphics card and one that does both on the CPU. The render nodes are bound into
every box and the compositor offers `linux-dmabuf` wherever there is a card; what
decides whether video is decoded in hardware is `libva` plus a
`*_drv_video.so` beside it. A browser whose runtime has neither reports no
hardware decoder and decodes every frame on the CPU.

## Mounting

`kdos-packd` is the only thing on the system that mounts a pack. It runs as
root, answers `/run/kdos-packd.sock`, and accepts requests from root and members
of `wheel`. See [The daemons](../04-programs/daemons.md).

Packs live in two places, and verification happens where the mount happens:

| Origin | Path | Verified |
|---|---|---|
| The store | `/var/lib/kdos/packs` | When root wrote it, at install; not re-hashed on mount |
| The medium | `/mnt/iso/packs` | Hashed the first time it is mounted |

A pack in the store was hashed when root wrote it, and only root can write
there, so it is not re-hashed. Re-hashing a several-hundred-megabyte base on
every mount would cost a full read of a file the kernel is about to read lazily
anyway. A pack on the medium has never been verified, and installing on a live
session mounts straight off the medium with no copy — so it is hashed on first
mount.

That is also why the store must be **owned by root**. If the desktop user could
replace a pack in the store, the daemon would mount it unverified — because "it
was hashed when root wrote it" is exactly the reasoning that skips the check.

There are two ways to mount, and the daemon's `status` reply names the one in
use (`route`). Where the kernel can take a regular file as a filesystem source,
no loop device is needed; where it cannot, the pack goes through a loop device,
set up directly with `ioctl` rather than by running a helper program — this is a
root daemon, and every process it starts runs as root.

Mounts are reference counted, and taking a box apart does not unmount. Two boxes
often share a runtime; unmounting it because one stopped would pull the layer
out from under the other, and every path in a running application would vanish.
A pack nobody is using costs a mount entry and no memory.

At startup the daemon rebuilds its mount table from `/proc/mounts`, because it
can be restarted while boxes are running, and a daemon that forgot a mount would
unmount a live box's own root.

## Composition

A store-built box has nothing to compose. Its root is a container image built on
this machine, so the container engine assembles the layers itself and
`kdos-packd` is not involved. Everything in this section is the import lane.

An imported application's container root is an overlay of the base, the runtime
it needs, the application pack, and a writable upper layer.

The daemon places the upper layer by asking the filesystem:

| Where `$HOME` is | Upper layer | Survives a reboot |
|---|---|---|
| A filesystem that can hold an overlay upper | `~/.local/share/kdos/boxes/<box>/upper` | Yes |
| An overlay itself (a live session) | `$XDG_RUNTIME_DIR/kdos/boxes/<box>/upper`, on `tmpfs` | No |

The kernel refuses to stack an upper layer on overlayfs, and a live session's
`$HOME` is on the boot overlay. The daemon's `status` reply marks each composed
box `persistent` or `ephemeral`, so losing your work is never silent.

There is **one box per application**, named after the pack. One box composing
every installed application would hit two limits at once: an overlay cannot gain
a layer while it is mounted, so installing an application would restart a
container other applications are running in; and a hundred lower layers do not
fit in the option-string length the kernel allows a mount.

The merged overlay lives under the runtime directory, a temporary filesystem, so
after a reboot the container's root is gone. A box is therefore recomposed every
time before it is started. Composing is idempotent because mounts are reference
counted, so a box already composed costs one round trip to the daemon. Without
this, a box would work until it was restarted — the hardest kind of breakage to
notice.

## The box

The two lanes create their containers differently:

- **A store box** is a distrobox container created from the image
  `kdos/<id>` (base `image:kdos/<id>`). distrobox's own init sets it up.
- **A pack box** (base `pack:<id>`) is created by `podman create --rootfs` over
  the merged overlay directly, with no image. It is labelled
  `manager=kdos-box`, runs `--userns keep-id`, and uses `kdos-boxinit` as its
  init.

Both lanes keep everything difficult about containers — user namespace mapping,
seccomp, cgroups and a supervising process that every KDOS tool identifies
boxes by — and a launch waits for the same readiness marker,
`container_setup_done`, in either lane.

The container runs with your own user identity mapped in, so applications see a
real non-root account; many applications refuse to run as root. Ownership grants
nothing either way, because packs are mounted `nosuid`.

**`kdos-boxinit`** (`/usr/libexec/kdos/kdos-boxinit`) is process 1 in a pack
box. It:

1. creates a user and group matching yours;
2. sets a search path that includes `/usr/games`;
3. writes `/usr/local/bin/xdg-open`, which hands links and files to the host's
   portal;
4. writes `/etc/asound.conf`, so ALSA's `default` in the box is the host's
   sound server;
5. prints `container_setup_done`, which the launcher waits for;
6. stays alive, reaping exited processes.

It is **statically linked** because it is bind-mounted into a container whose
libraries are Debian's, where a host-linked binary would look for its loader
and not find it.

`kdos-boxinit` replaces a pack's existing record for your user rather than
keeping it (step 1 above). A base pack shipping an account whose home directory
is `/` would otherwise give every application
`HOME=/`, so none reads any configuration in your real home — and a boxed
application comes up in its toolkit's default light theme on the phosphor
desktop, with the palette sitting correctly in a home it never looked at.

### The graphics card and the two profile keys

The graphics card reaches a box as device nodes, and the box profile's two keys
govern different halves of it. (A box profile is
`~/.config/kdos/boxes/<name>.conf`; see
[kdos-appbox](../04-programs/kdos-appbox.md).)

`gpu` is the device nodes. A box that shares the host's `/dev` has them, and
nothing can be subtracted from that; a box with a private `/dev` gets `/dev/dri`
bound back by this key alone.

`render` is who draws. It is resolved against the machine by opening a render
node, and puts `LIBGL_ALWAYS_SOFTWARE=1` into a launch that asked for software
rendering. It is advisory — an environment variable the application may unset —
and it governs GL and EGL only. VA-API and Vulkan find the render node their own
way, which is why that is the `gpu` key's job.

A boxed client draws with its own Mesa and connects to `kdos-comp` as an
ordinary Wayland client on the socket named in `WAYLAND_DISPLAY`. Both keys
default to the card, because the nodes and the drivers are already there and a
box drawing in software on a machine with a render node gains nothing.

That socket is a per-box one that `kdos-boxsock` holds open, and its tag is what
the compositor's allowlist uses to filter screen capture (screencopy), clipboard
managers (data-control), input methods and layer-shell surfaces. It is a label,
not a wall. The box shares `$XDG_RUNTIME_DIR` with the session, so a client that
opens the default `wayland-0` reaches the session's own socket. Withholding the
variable would advertise a confinement the sandbox does not have, which is why
no profile key withholds it. See [The security model](security-model.md).

## Grafts and data packs

A **data pack** carries a dataset rather than a program — for example KiCad's 3D
models or Tesseract's language files. It is mounted read-only, `nosuid`,
`nodev` and **`noexec`** (a data pack that ships a binary cannot run it), and it
is never composed into a container root.

What makes a data pack useful is therefore its **grafts**: declared placements
that the daemon carries out. A pack contains no scripts, and no shell runs near
one.

There are two graft namespaces, because the host's data directories are
invisible inside a box:

| Line | Lands | For |
|---|---|---|
| `graft` | A symlink under `/usr/share` | Programs on the host |
| `boxgraft` | A symlink under `~/.local/share/kdos/packs/` | Boxed applications, which share your home |

A `boxgraft` target beginning with `~/` lands at that place in your home, for a
program that reads its own fixed directory and takes no variable. An `env` line
then tells the application where to look, and its value may begin with `$HOME`,
because the pack cannot know your user name. For example, from the catalogue:

```
boxgraft data.kicad-packages3d  usr/share/kicad/3dmodels  kicad-3dmodels
env      app.kicad              KICAD9_3DMODEL_DIR=$HOME/.local/share/kdos/packs/kicad-3dmodels
```

Every graft is recorded in `/var/lib/kdos/pack-manifest`, so removing it removes
exactly what was added — and only a **symlink** is removed, because a real
directory at that path belongs to someone else.

Installing a data pack grafts it in the same step, and removing one ungrafts it
first. A mounted dataset with no grafts is one nothing can find.

## What an install carries

The installer (`kinstall`) copies no applications, and says so before it runs:
the medium has none to copy. What its Applications page chooses is a set of
groups, and the installer turns that into one of four routes:

| Route | What the install does |
|---|---|
| Nothing chosen | Makes the store's directories and stops |
| From the stick | `kdos-appbox import` on the first `.ktar` found under a mounted device, staged through `kdos-packd` in the target, which verifies each pack as it mounts it. The only route on a machine with no network |
| Over the network | `kdos-appbox install <group>…` during the install: podman and apt, which takes minutes |
| At first login | The selection is written to `/var/lib/kdos/apps-pending`. Nothing is built during the install: your first session shows a notification offering the applications, and `kdos app install --pending` does the build. The offer is made once; deleting `~/.config/kdos/apps-offered` brings it back at the next login |

The page shows which route applies before the install starts, because a person
must not discover at first boot that nothing was installed. Building can take
most of an hour, and an installer that did that silently would look hung.

An import or a build that fails falls back to *pending* rather than failing the
install. The system is on the disk and bootable; an archive that would not
import is a reason to say so and carry on, not to abandon a partitioned disk.

## Reaching the desktop

`kdos-appbox genlaunchers` makes installed applications appear in your menus and
on your command line. It has four forms:

| Form | Reads | Writes |
|---|---|---|
| `genlaunchers --packs --user` | Every installed pack and store box | Your own tree; this is what `kdos app install` runs |
| `genlaunchers --packs <fs-root>` | Every installed pack | The system tables under `<fs-root>` |
| `genlaunchers --packs-dir <dir> <fs-root>` | One extracted pack per subdirectory of `<dir>` | Under `<fs-root>`; the form the build's launcher step uses |
| `genlaunchers <desktop-dir> <fs-root>` | One directory of desktop entries | Under `<fs-root>` |

In the pack forms it walks every installed pack, mounts each through the daemon, and parses the
application's **own** desktop entries — a pack carries the real ones, so the
existing parser is reused rather than reimplemented against the metadata. It
covers every pack and every store box in one pass, so no profile key decides
whether a box's applications get launchers; `kdos-box export <box> <app>` adds a
launcher for one application in a box that is neither.

It writes four things, and dropping any one breaks something visible:

| Output | Without it |
|---|---|
| A desktop entry per application | No launcher |
| A MIME cache beside them | File-type associations are never consulted |
| A name-to-command table (`alien-apps`) | The shim cannot find what to run |
| A shim per application in the local binary directory | The application is not a command |

The details — entry naming, quoting, field codes, the tables that skip or rename
entries — are in [kdos-appbox](../04-programs/kdos-appbox.md).

## See also

- [Applications](../02-user-guide/applications.md) — using all of this
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher and the box manager
- [The daemons](../04-programs/daemons.md) — `kdos-packd` in detail
- [The security model](security-model.md) — mount options, signing and the sandbox
- [Packaging](packaging.md) — the host's separate packaging system
- [Glossary](../06-reference/glossary.md) — pack, box, graft, runtime and the other terms
