# Packs and boxes

The packaging system for applications: how one is built from the catalogue,
what a pack is, how a pack is verified and mounted, and how either kind of box
reaches the desktop.

This is a separate system from [host packaging](packaging.md) because it
answers a different question. Host packaging answers *what is installed on this
machine*. This answers *what software can this machine run, and how do I get a
piece of it without installing anything into the system*.

## Two lanes, one box

Two routes reach the same box. The store builds a stack of container images
from the catalogue and creates a box `FROM` the top one. An import installs
signed packs and composes a box out of an overlay of them. The lanes differ in
where the bytes came from and what vouches for them; everything from the
container root upward is identical.

| | Store | Import |
|---|---|---|
| Source | Debian archive, over the network | A `.ktar` somebody handed you |
| Verified by | Nothing — unsigned registry content | Payload hash and signature, at the mount |
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

The extension is `.kpack`, and a difference between two of them is `.kdelta`.

Appending rather than prepending is the whole trick. EROFS records the image's
extent in its own superblock and never reads past it, so the file is a
mountable filesystem exactly as it sits — measured with kilobytes of junk on
the end. Userspace seeks to `filesize - 512` and finds everything else from
there. No offset needs to be communicated out of band, and the kernel needs to
know nothing about the format.

### Rules the format keeps

**A pack that does not parse whole is absent, never partial.** A short footer,
a wrong magic number, a version from the future, an offset past the end of the
file, a section that starts before the one ahead of it ends — each answers
"there is no pack here" rather than handing back half a description.

Three of the spans are bounded by what the section can honestly be as well as
by the file: metadata 1 MiB, icon 4 MiB, signature block 64 KiB. All three are
read whole into memory by a root daemon before anything about the pack is
authenticated, so "it fits in the file" would be an attacker's budget rather
than a bound.

**The signature block ends exactly where the footer begins.** Slack there is
what makes a later `kdos-pack sign` silently do nothing: it appends its line at
`sig_off + sig_len` and writes a new footer straight after, landing in the
middle of the file, while the old footer at the end — still naming the old
`sig_len` — is the one a reader seeks to.

**The payload hash is checked before the signature means anything.** The
signature is over a small subject containing the pack's id and its hash, so
verification never holds a several-hundred-megabyte file in memory — and that
binds the signature to the bytes *only* because the bytes were hashed first.
The two are separate outcomes, because a caller told "bad signature" when the
truth is "bad hash" goes looking for a key problem that does not exist.

**From format 2 the hash covers the footer too**, with `payload_sha256` and
`sig_len` zeroed. The footer is what says where the filesystem, the metadata
and the icon are. Over a hash that stops at `sig_off` — which is all a format 1
pack's digest is — those offsets can be re-pointed (`meta_off` into the
payload, say) and the signature still verifies, because it is over bytes nobody
moved. A format 1 pack therefore leans on the medium's signed index, whose `C:`
hash is over the whole file, to cover its footer. The two zeroed fields are the
two written after the hash is taken: the digest itself, and the length that
grows each time a further key signs an already-signed pack.

**The format number is what says which span the digest covers**, and it is read
from the pack rather than assumed from the build. Format 1 is `[0, sig_off)`;
format 2 is that plus the footer. Every format from `KPK_FORMAT_MIN` (1) up is
verified the way it declares, which is what lets a pack published under an
older rule still mount. `KPK_FORMAT` (2) is only what a pack written here
declares.

Widening the span without moving the number is the failure this prevents. Every
pack already baked answers `HASH`, `kdos-packd` mounts nothing, no box
composes, and every application in it stops opening — indistinguishably from
corruption. `kdos-pack restamp <pack>` is both the repair and the upgrade: it
raises the footer to `KPK_FORMAT`, takes the digest under that format's span,
and drops the signature block with it, since the block names the digest it
replaced. The pack is then signed and the directory indexed again. A pack
already at this format with an agreeing digest is left alone, signature
included.

**Nothing in the library mounts, executes or writes outside the file it was
given.** A root daemon links it, so every line is code running as root.

### Signature outcomes

| Outcome | Meaning | Mounts? |
|---|---|---|
| `GOOD` | Verified against a trusted key | yes |
| `NONE` | No signature block at all | yes |
| `HASH` | The payload hash does not match | **no** |
| `BAD` | Signed, and the signature does not verify | **no** |
| `NOKEY` | Signed, and this machine holds no pack key at all | **no** |

`NOKEY` is deliberately its own outcome rather than a forgery. It is asked
before the block is even read: with an **empty** keyring every signature fails
and every failure looks the same, so that answer names the *machine* rather
than the artefact. Reporting it as tampering would send the reader to inspect
the pack instead of the key directory. A signature that no key in a non-empty
ring verifies is `BAD`.

Note the asymmetry this leaves standing, stated rather than hidden: an
**unsigned** pack mounts, so a pack that is signed and uncheckable is treated
more harshly than the same pack with no block at all.

## Building a pack

`kdos-pack` builds, inspects, signs, indexes and diffs them. It ships on the
target, because capturing a working box as a pack is something you do on the
machine you are working on.

| Verb | Does |
|---|---|
| `build` | Make the filesystem image and wrap it |
| `assemble` | Wrap an image somebody else already made |
| `image` | Extract just the filesystem image |
| `info` | Print the metadata |
| `imagehash` | Hash the image portion alone |
| `uuid` | Derive the image UUID for an id |
| `sign`, `verify` | Signature handling |
| `index` | Write the `PACKAGES` index |
| `delta`, `apply` | Difference between two packs, and rebuilding one from it |
| `keygen` | Make an Ed25519 pack-signing key pair |
| `extract-meta` | Write the metadata blob out on its own |
| `restamp` | Raise the footer format and re-take the digest |

`build` and `assemble` are split for a reason that is not convenience. Making
the filesystem image must run as **root**, because only root preserves the
deletion markers and extended attributes that a container layer depends on.
Wrapping an image is an ordinary file operation anyone can do.

### Reproducibility

Four flags make a pack a function of its inputs rather than of whoever ran the
build:

| Flag | Without it |
|---|---|
| `-T $SOURCE_DATE_EPOCH` | Every inode carries the second it was packed |
| `--force-uid=1000 --force-gid=1000` | The builder's own user id rides along |
| `-b 4096` | The block size follows the **page size**, so the same tree on a machine with larger pages is a different image |
| `-U <derived from the pack id>` | The UUID is random per build |

The version is deliberately not in that UUID. It lands in the filesystem
superblock, so a version taken from the build clock would make every rebuild a
different image even when no file inside had moved — and `imagehash`, which is
how a rebuild is asked whether anything moved, could never answer "unchanged".
The self-test packs the same tree under two versions and fails if the two
hashes differ.

`imagehash` covers the image and not the metadata, so it is only half the
question. A metadata line changes what the pack *declares* and no byte of its
filesystem, so anything deciding to keep an existing file has to compare both
or ship a pack on which a newly declared command does not exist.

## The catalogue

`src/packages/kdos-appbox/catalogue` defines what can be built, and ships to
`/usr/share/kdos/appstore/catalogue`. Measured against the file it carries 2
`base` rows, 7 `runtime` rows, 180 `app` rows, 2 `data` rows and 21 `group`
rows.

| Row | Declares |
|---|---|
| `base` | A whole root filesystem |
| `runtime` | A layer over the base, shared by many applications |
| `app` | One application, as a difference over a runtime |
| `data` | A dataset, mounted but never composed into a container |
| `cmd` | A command a row provides that has no graphical launcher |
| `env` | An environment variable a row or runtime needs |
| `deb` | An application Debian does not carry, by releases URL and asset pattern |
| `needs` | A data row an application is useless without |
| `graft`, `boxgraft` | Where a data row's contents should appear |
| `image` | A base that names its own container image |
| `group` | A curated bundle the store and the installer offer as one tick |
| `meta` | Display name, category, size estimate and tagline |
| `snapshot` | Which Debian archive date the packages come from |

A row's parent must appear above it. The resolver walks a chain upward in one
pass, so a forward reference is a chain it cannot close — reordering the file
makes an install fail naming the missing parent.

A group is not a category. The category is the application's own, out of its
desktop entry, and every application has exactly one. A group is curated, most
applications are in none, and a member that is not an `app` or `data` row is an
install refused by name.

`meta` is optional and its absence is not an error. A row without one presents
as its own id, category `Other`, no tagline and size 0. A row added today has
no `meta` until somebody writes one, and refusing to load would make adding
software a two-file change for no benefit. The size is an **estimate** and
every surface labels it one: what apt resolves on the day depends on the
snapshot.

## Where a pack comes from

**Nothing is baked onto the medium.** The ISO carries the catalogue and no
applications: `script/06_packaging/01_packs.sh` creates the store's two
directories — `/var/lib/kdos/packs/staging` at `01777` and `.../mnt` — and gets
out of the way. Staging is the one place an unprivileged write may land, and
its mode is set here as well as by the daemon, because a first boot that
inherited `0755` would refuse an import until `kdos-packd` had run once.

Three things make a pack, and each answers a different question:

| Route | Makes |
|---|---|
| `kdos-appbox export <file.ktar> <id>…` | One pack per store image this machine has built, tarred into one file to hand over |
| `kdos-box freeze <name> [out.kpack]` | A box's **writable upper** — only what you changed, deduped against its base by construction |
| `KDOS_PACK_KDOS=1` at packaging time | The running rootfs as the base pack `kdos`, so `kdos-box create ports base=pack:kdos` gives a running KDOS a clean KDOS to build ports in |

The export is a flatten: a container is created over the built image, `podman
export` writes its whole merged filesystem, and that is what gets wrapped. So
an exported pack carries no deletion markers of any kind and nothing has to
agree about how a deletion is spelled.

The rootfs pack is **opt-in** because it is a second `mkfs.erofs` over the
whole tree on every build, for a base most installs will never compose. It
lands in `build/` rather than in the store: the store is *inside* the rootfs,
so a pack written there would be squashed into `system.sfs` and the medium
would carry the tree twice. The ISO step picks it up from `build/` and puts it
on ISO9660 beside `system.sfs`, where `kdos-packd`'s scan for `*.kpack` finds
it.

### Exclusions must be probed, not trusted

Two rules about excluding paths from an image, both of which cost real disk
when broken.

A base pack must keep its mount points. Excluding a *directory* removes it, and
a container root with no `/proc`, `/sys`, `/dev`, `/tmp` or `/run` to mount
onto cannot be started at all — the engine reports a missing mount point for a
container that created perfectly. The exclusions are therefore expressed as a
pattern matching everything *inside* those directories and not the directories
themselves.

A path exclusion is relative to the tree being packed and must carry no leading
slash. It matches an exact literal, so a leading slash matches nothing, is not
an error, and excludes **nothing**. The packaging step therefore probes the
flag against a throwaway tree first — nested and top-level, and it refuses to
pack at all if a 2 MB directory it excluded comes back — because a silently
inert exclusion here fills the disk rather than printing a warning.

A hand-rolled image must still force the ownership. A box runs `--userns
keep-id`, so the process inside it is uid 1000; every pack `kdos-pack build`
makes forces uid and gid 1000 so that user owns the tree, and the rootfs pack
passes the same flags by hand. Packed with *real* ownership from a root-owned
rootfs, the container user can create nothing anywhere in it, and it fails in
two stages — a permission denial on `/etc/mtab`, then a missing bind
destination for `kdos-boxinit` — neither of which mentions ownership.

### The graphics stack is a base row

A base may name its own container image, which is what makes a second,
non-Debian base cost one line: `image alpine alpine:3.24.1` beside
`base alpine -`. Everything the generated build file emits below the image line
is package management, so a base whose value is the image as it stands declares
**no packages** and the image *is* the pack.

The drivers sit there rather than in a runtime. A pack's parent chain is a
single line — the browser sits on the GTK runtime, the video editor on the
media one — so a driver set placed on either reaches half the catalogue and no
more. The DRI drivers, the GL and EGL loaders and the VA-API drivers
(`libgl1-mesa-dri`, `libegl1`, `libgl1`, `libva2`, `va-driver-all`) are
therefore in the `base` row, where they are stored once and every box has them.

What that buys is the difference between a box that draws and decodes on the
card and one that does both on the CPU. The render nodes are bound into every
box and the compositor offers `linux-dmabuf` wherever there is a card, and the
piece that decides whether a video is decoded in silicon is `libva` plus a
`*_drv_video.so` beside it. A browser whose runtime has neither reports no
hardware decoder and decodes every frame on the CPU.

## Mounting

`kdos-packd` is the only thing on the system that mounts a pack. See
[The daemons](../04-programs/daemons.md).

Verification happens where the mount happens, and the two origins are treated
differently on purpose.

A pack in the store was hashed when root wrote it, and only root can write
there, so it is not re-hashed. Re-hashing a several-hundred-megabyte base on
every mount would cost a full read of a file the kernel is about to read lazily
anyway.

A pack on the medium has never been verified at all, and is hashed the first
time it is mounted, because installing on a live session mounts straight off
the medium with no copy.

That is also why the store must be **root-owned**. A store owned by whoever
cloned the tree could have a pack replaced by the desktop user, and the daemon
would mount it unverified — because "it was hashed when root wrote it" is
exactly the reasoning that skips the check.

Two mount routes exist and the daemon says which it is using. Where the kernel
can take a regular file as a filesystem source there is no loop device; where
it cannot, the pack goes through one, set up directly rather than by executing
a helper — this is a root daemon and every process it starts runs as root.

Mounts are reference counted, and taking a box apart does not unmount. Two
boxes legitimately share a runtime; unmounting it because one stopped pulls the
layer out from under the other, and the symptom is every path in a running
application vanishing. A pack nobody is using costs a mount entry and no
memory.

The mount table is rebuilt from the kernel's mount list at startup, because the
daemon can be restarted while boxes are running and one that forgot would
unmount a live box's own root.

## Composition

A store-built box has nothing to compose. Its root is a container image the
machine built, so the engine assembles the layers itself and the pack daemon is
not involved at all. Everything below is the import lane.

An imported application's container root is an overlay of the base, the runtime
it needs, the application pack, and a writable upper layer.

The daemon places that upper by asking the filesystem: under the user's data
directory where `$HOME`'s filesystem takes an overlay upper, and on the runtime
`tmpfs` where it does not, because a live session's `$HOME` is on the boot
overlay and the kernel refuses to stack an upper there. The `status` reply
names which of the two a composed box got — losing somebody's work quietly is
the failure that fallback exists to avoid.

There is **one box per application**, named after the pack. The alternative —
one box composing every installed application — hits two walls at once. An
overlay cannot gain a layer while it is mounted, so installing an application
would restart a container other applications are running in; and a hundred
lower layers do not fit in the option-string budget the kernel allows a mount.

The overlay lives under the runtime directory, which is a temporary filesystem,
so a reboot leaves a container whose root has been deleted. A box is therefore
recomposed before it is started. Composing is idempotent because the mounts are
reference counted, so a box already composed pays one round trip. Without that,
a box works until it is restarted, which is the worst kind of broken.

## The box

The container is created over the merged root directly rather than from an
image.

The merged path is the first positional argument and must be last on the
command line. The container engine parses non-interspersed: everything after
that positional is the container's own command, so a flag placed after it
becomes the program the runtime tries to execute, and the box is created and
then dies reporting that a flag is not an executable.

Over that root, everything hard is kept — user namespace mapping, seccomp,
cgroups and a supervisor process that every KDOS tool identifies boxes by — and
only the part that needed an image is replaced.

The container runs with the user's own identity mapped, so applications see a
real non-root account. Both arrangements were measured; this one is taken
because applications refuse to run as root. Ownership grants nothing either
way, since packs are mounted `nosuid`.

`kdos-boxinit` is the container's init, in place of a general-purpose
container-init program. It creates a user and group matching the host's, sets
the search path including the games directory, writes `/usr/local/bin/xdg-open`
and `/etc/asound.conf`, announces readiness where the launcher looks for it,
and then stays alive reaping. It is **statically linked**, because it is
bind-mounted into a container whose libraries are Debian's and a host-linked
binary would look for its loader there.

A wrong user record in the pack must be replaced, not left alone. A base pack
shipping an account whose home directory is the filesystem root gives every
application `HOME=/`, so none of them reads any configuration in the real home
— and a boxed application comes up in its toolkit's own light theme on a
phosphor desktop, with the palette sitting correctly in a home it never looked
at.

### The card, and the two keys about it

The card reaches a box as device nodes, and the box profile's two keys are
about different halves of it.

`gpu` is the nodes. A box that shares the host's `/dev` has them and nothing
can be subtracted from that; a box with a private `/dev` gets `/dev/dri` bound
back by this key alone.

`render` is who draws. It resolves against the machine by opening a render
node, and puts `LIBGL_ALWAYS_SOFTWARE` into a launch that asked for software.
It is advisory — an environment variable the application may unset — and it
governs GL and EGL only. VA-API and Vulkan find the render node by their own
route, which is the `gpu` key's job.

A boxed client draws with its own Mesa and connects to `kdos-comp` as an
ordinary Wayland client on the socket handed to it as `WAYLAND_DISPLAY`. Both
keys default to the card, because the nodes and the drivers are already there
and a box drawing with llvmpipe on a machine that has a render node is paying
for nothing.

That socket is a per-box one `kdos-boxsock` holds open, and the tag on it is
what the compositor's allowlist filters screencopy, data-control, input-method
and layer-shell on. It is a label and not a gate. The box shares
`$XDG_RUNTIME_DIR` with the session, so a client that opens the default
`wayland-0` reaches the session's own socket; withholding the variable would
advertise a confinement the sandbox does not have, which is why no profile key
withholds it.

### Building the launch command

Every shared directory is formatted into the argument vector directly, never
through one reused buffer. The argument builder stores the pointer rather than
copying, so several shares built in one buffer all point at the same bytes and
the engine is handed the last value repeatedly. The measured symptom is a box
that starts, runs the application, and has every path it wants silently absent.

## Grafts and data packs

A **data pack** carries a dataset rather than a program. It is mounted
read-only, `nosuid`, `nodev` and **`noexec`** — a data pack that ships a binary
cannot run it — and it is never composed into a container root.

What a data pack is *for* is therefore its **grafts**: declared placements the
daemon interprets. There is no script in a pack and no shell near one.

There are two graft namespaces, because the host's data directories are
invisible inside a box:

| Row | Lands | For |
|---|---|---|
| `graft` | Under the host's shared data directory | Host consumers |
| `boxgraft` | Under a directory in the home | Boxed applications, which share the home |

A `boxgraft` target beginning with `~/` lands at that place in the home, for a
program that reads its own fixed directory and takes no variable. Everything
else lands under a pack directory in the home, which an `env` row names. An
environment value may begin with the home variable, because the pack cannot
know the user's name.

Every graft is recorded in a manifest, so removing one removes exactly what was
added — and only a **symlink** is removed, because a real directory there is
somebody else's.

Installing a data pack grafts it in the same action, and removing one ungrafts
first. A mount with no grafts made is a dataset nothing can find.

## What an install carries

Nothing, and the installer says so before it runs. The medium has no
applications on it, so there is no copy to make; what the Applications page
picks is a set of groups, and `ki_apps_route()` turns that into one of four
answers:

| Route | What the install does |
|---|---|
| nothing chosen | Makes the store's directories and stops |
| from the stick | `kdos-appbox import` on the first `.ktar` found under a mounted device — staged through `kdos-packd` in the target, which verifies each pack where it mounts it. The only route on a machine with no network |
| over the network | `kdos-appbox install <group>…` during the install: podman and apt, which is minutes |
| at first login | The selection is written to `/var/lib/kdos/apps-pending`, and `kdos app install --pending` builds it |

The page has already **said** which, because a person must not discover at
first boot that nothing was installed. Building is most of an hour, and an
installer that did that silently is an installer that appears to have hung.

An import or a build that fails falls back to *pending* rather than failing the
install. The system is on the disk and bootable; an archive that would not
import is a reason to say so and carry on, not to abandon a partitioned disk.

## Reaching the desktop

`kdos-appbox genlaunchers` walks every installed pack, mounts it through the
daemon, and parses the application's **own** desktop entries. A pack carries
the real entries, so the existing parse is reused rather than reimplemented
against the metadata. It covers every pack and every store box in one pass, so
no profile key decides whether a box's applications get launchers; `kdos-box
export <box> <app>` is the per-application route for a box that is neither.

It writes four things, and dropping any one breaks something visible:

| Output | Without it |
|---|---|
| A desktop entry per application | No launcher |
| A MIME cache beside them | The type associations are never consulted |
| A name-to-command table | The shim cannot find what to run |
| A shim per application in the local binary directory | The application is not a command |

The details — entry naming, quoting, field codes, the tables that skip or
rename — are in [kdos-appbox](../04-programs/kdos-appbox.md).

## See also

- [Applications](../02-user-guide/applications.md) — using all of this
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher and the box manager
- [The daemons](../04-programs/daemons.md) — `kdos-packd` in detail
- [The security model](security-model.md) — mount options, signing and the sandbox
- [Packaging](packaging.md) — the host's separate packaging system
