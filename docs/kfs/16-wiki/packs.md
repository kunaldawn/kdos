# Packs

A [pack]{.idx} is one file, `.kpack`, holding a read-only [EROFS]{.idx} filesystem image with its metadata,
an icon and its signatures appended. It carries an application, a runtime, a base or a dataset, and
a box's root filesystem is composed from packs. Packs are how boxed applications travel between
machines without a network: one machine exports what it built as a set of packs, and another
imports the set, verified by the pack daemon, `kdos-packd`, as it installs each one. This page
covers exporting and importing a set, carrying one box as a pack, signing and verifying packs,
[deltas]{.idx term="delta"}, and the pack daemon's service and settings. Running the applications is [Boxes](boxes.md).

## Installation

- [kdos-pack](#port-kdos-pack) installs `/usr/bin/kdos-pack`, which builds, inspects, signs,
  indexes and diffs packs. It runs `mkfs.erofs` from [erofs-utils](#port-erofs-utils) and `zstd`
  from [zstd](#port-zstd).
- [kdos-packd](#port-kdos-packd) installs the daemon, `/usr/sbin/kdos-packd`, and the pack store's
  `staging` and `mnt` directories. It is the only program on the system that installs or mounts a
  pack. It answers `/run/kdos-packd.sock`, and only root and members of `wheel`.
- [kdos-appbox](#port-kdos-appbox) is the client: `kdos-appbox export` and `import`, and
  `kdos-box freeze` and `import`. `kdos app export` and `kdos app import`, from
  [kdos-tools](#port-kdos-tools), run it.
- [kdos-boxinit](#port-kdos-boxinit) is the init a pack box runs as process 1.
- The base filesystem supplies the service script, `/etc/init.d/59_packd.sh` (the service
  `packd`), the daemon's settings, [`/etc/kdos/packd.conf`]{.idx}, and the ring of trusted pack
  keys, [`/etc/kdos/keys/packs/`]{.idx term="/etc/kdos/keys/packs"}.
- The image step [100_packs](../15-image/100-packs.md) creates the pack store's directories.

The service `packd` is enabled on the live medium and on an installed system. It skips itself when
the kernel has no EROFS. The image carries no application packs; a medium built with
`KDOS_PACK_KDOS=1` carries one, KDOS's own root filesystem as the base pack `kdos`, at
`/mnt/iso/packs/kdos.kpack`.

## Configuration

### /etc/kdos/packd.conf

From `fs/etc/kdos/packd.conf`:

<!-- kfs:include fs/etc/kdos/packd.conf -->

`retain` is how many superseded versions of one pack the store keeps, at most 8. The daemon reads
the file once, the first time it needs the value, so an edit takes effect after
`sudo service restart packd`. `KDOS_PACK_RETAIN` in the daemon's environment overrides it. See
[/etc/kdos/packd.conf](../../kdos/06-reference/configuration.md#etckdospackdconf).

### /etc/init.d/59_packd.sh

The daemon runs under the service supervisor. The script loads the EROFS module first, and skips
the service when the kernel still has no EROFS:

<!-- kfs:include fs/etc/init.d/59_packd.sh lines=8-27 pin=2eb7c7d60ef9 -->

The script sets none of the daemon's environment variables (`KDOS_PACK_STORE`, `KDOS_PACK_RETAIN`,
`KDOS_REQUIRE_SIG` and the rest); they are listed under
[kdos-packd](../../kdos/04-programs/daemons.md#kdos-packd).

### /etc/kdos/keys/packs/

The ring a pack's signature is checked against. It is a directory of its own because a ring is a
flat read of `*.pub`, and a key that vouches for packs must not thereby vouch for host packages.
Trusting a key is copying its `.pub` in; removing trust is deleting it.

The image ships one key here, `kdos-packs.pub`. Nothing the build makes is signed with it: the
medium carries no signed pack and no pack index, and the optional base pack `kdos` is unsigned. The
shipped key therefore verifies nothing on a standard medium; it only means that a pack signed with
its secret half would be accepted. Delete it if you want the ring to hold only keys you chose. See
[Signing and trust](../../kdos/03-architecture/security-model.md#signing-and-trust) and
[Signing keys and trust](signing-keys-and-trust.md).

### The pack store

| Path | What |
|---|---|
| `/var/lib/kdos/packs/<id>.kpack` | The installed version of each pack |
| `/var/lib/kdos/packs/<id>-<version>.kpack` | Superseded versions, up to `retain`; `<version>` is the pack's `version =` without its release |
| `/var/lib/kdos/packs/staging/` | The one place an unprivileged write lands, mode `01777` |
| `/var/lib/kdos/packs/mnt/<id>/` | Where a pack is mounted, read-only, `nosuid` and `nodev`, and `noexec` too for a data pack |
| `/mnt/iso/packs/` | Packs on the boot medium, mounted from there |
| [`/var/lib/kdos/pack-manifest`]{.idx} | Every [graft]{.idx} made, for `ungraft` |

The daemon does not install the staged file itself. It copies it into a private file in the store,
owned by root with mode `0644`, verifies that copy and renames it to `<id>.kpack`, then deletes the
staged file. An installed pack is owned by root whoever staged it.

The image step makes the staging directory with its mode, so that a first boot accepts an import
before the daemon has ever run:

<!-- kfs:include script/phases/70_image/100_packs.sh lines=30-33 pin=a711dc612448 -->

## Usage

### Seeing the daemon and its packs

1. Ask the daemon for its status: the store, medium and staging directories, the `retain` count,
   the mount route (`file-backed` or `loop device`, and `not yet used` before the first mount),
   how many packs are installed, available and mounted, and each composed box:

   ```bash
   kdos-appbox status
   ```

   On a freshly booted live medium:

   ```text
   store	/var/lib/kdos/packs
   medium	/mnt/iso/packs
   staging	/var/lib/kdos/packs/staging
   retain	1
   route	not yet used
   packs	0 installed, 0 available, 0 mounted
   boxes	0 composed
   box   : not created (first launch will) (absent)
   ```

   `packd : not answering` means the daemon is not running, or refused you; see
   [Troubleshooting](#troubleshooting).

2. Look inside one pack file: its metadata, sizes and signature state:

   ```bash
   kdos-pack info app.hugin.kpack
   ```

3. `kdos doctor` checks the same things in its Boxes section: EROFS, the daemon and its mount route,
   whether you are uid 1000, whether your home can hold a box's writable layer, and whether every
   mounted pack still has a file behind it. In a live session:

   ```text
   Boxes
     [ ok ] erofs is available to the kernel
     [ ok ] kdos-packd is answering — not yet used
     [warn] $HOME is on overlayfs (a live session), so a persistent box cannot exist — its upper falls back to tmpfs and its changes end with the session
     [skip] nothing is mounted from the pack store
   ```

### Exporting a set of applications

An export packs applications this machine has built from the catalogue (see [Boxes](boxes.md)) into
one `.ktar` file for another machine.

1. Export by id, by group, or both. An id whose image is not built here is skipped with a message:

   ```bash
   kdos app export apps.ktar app.hugin app.scribus
   ```

2. To sign the set's index, name a key in `KDOS_PACK_KEY` (see
   [Signing packs](#signing-packs) for making one). Without it the export says that the index is
   unsigned:

   ```bash
   KDOS_PACK_KEY=~/keys/mine.key kdos app export apps.ktar app.hugin
   ```

   An import stages a signed set's `PACKAGES` and `PACKAGES.sig` beside its packs and names that
   index in each install it asks for, so the daemon installs each pack the index lists as signed
   by that key, provided its `.pub` is in `/etc/kdos/keys/packs`. The packs of an unsigned set are accepted as unsigned.

Each pack is the application's whole image flattened, base and runtime included, so it needs no
other pack. The archive also holds `SELECTION`, the ids that were named and the members of each
group that was named, `PACKAGES`, the index, and `PACKAGES.sig` when the index was signed.

### Importing a set

An import needs no network. It copies each pack into the staging directory and asks `kdos-packd` to
install it by file name; the daemon checks the pack's payload hash and its signature, and
`kdos-box create` then makes a box on the installed pack.

1. Import the whole set, or name the ids you want from it:

   ```bash
   kdos app import apps.ktar
   kdos app import apps.ktar app.hugin app.scribus
   ```

2. Read the last line, which counts what imported and what failed; the exit status is 1 if any
   failed. A pack the daemon refuses is named and skipped, and the rest still import.

3. The launchers are regenerated when at least one application imported, so the applications are
   in the menu and on the command line.

A bare import installs every id line in `SELECTION`, the members of a group listed under its
`group` line. An id whose application was not installed on the exporting machine is listed but has
no pack, and import counts it as a failure. To see what the set holds before importing:

```bash
tar -xOf apps.ktar SELECTION
```

### Carrying one box as a pack

A pack box's writable layer, everything it changed over its base, can be packed and taken
elsewhere. This works for boxes created on a pack (`base=pack:<id>`); a box built from a container
image has no such layer.

1. Freeze the box. The pack's id is `box.<name>`, and it records the box's base pack, so an import
   composes it over the same base. The default output is `<name>.kpack` in the current directory:

   ```bash
   kdos-box freeze work
   ```

2. Optionally sign it:

   ```bash
   kdos-pack sign work.kpack ~/keys/mine.key
   ```

3. On the other machine, which must hold the same base pack, import it and create a box on it:

   ```bash
   kdos-box import work.kpack as work
   ```

### Signing packs

1. Make a key pair once, in the current directory. `<name>.key` is written mode `0600`;
   `<name>.pub` is the half others trust, and `keygen` prints its key id beside it:

   ```bash
   mkdir -p ~/keys && cd ~/keys
   kdos-pack keygen mine
   ```

2. Sign a pack, which appends a signature block:

   ```bash
   kdos-pack sign app.hugin.kpack ~/keys/mine.key
   ```

3. On each machine that should accept it, trust the public key:

   ```bash
   sudo cp mine.pub /etc/kdos/keys/packs/
   ```

4. Check a pack against the ring, or against another directory of keys:

   ```bash
   kdos-pack verify app.hugin.kpack
   kdos-pack verify app.hugin.kpack --keys ~/keys
   ```

   It prints `<id>: signed by <key id>` or `<id>: unsigned` and exits 0, or exits 2 with one of
   `bad payload hash` (the filesystem image does not match its hash), `bad signature` (no key in
   the ring verifies the signature) and `signed, but this machine has no key to check it with`
   (the ring is empty). The shipped ring holds `kdos-packs.pub`, so a pack signed by a key you have
   not trusted yet is reported as `bad signature`, not as a missing key. With `KDOS_REQUIRE_SIG`
   set, an unsigned pack exits 2 as well: `<id>: unsigned, and KDOS_REQUIRE_SIG is set`.

The daemon accepts a pack whose signature is good, and one with no signature at all, which it logs
as unsigned. It refuses the other three outcomes. The packs `kdos-appbox export` makes carry no
signature block of their own. A set exported with `KDOS_PACK_KEY` installs as signed through its
index, which the import stages beside the packs, when that key's `.pub` is in
`/etc/kdos/keys/packs`; a pack whose hash the signed index contradicts is refused. Only the packs
of an unsigned set are accepted as unsigned, and `kdos-pack sign` gives a pack a signature of its
own that counts without an index. See
[Signature outcomes](../../kdos/03-architecture/packs-and-boxes.md#signature-outcomes).

### Deltas

A delta carries the difference between two versions of a pack:

```bash
kdos-pack delta old.kpack new.kpack
kdos-pack apply old.kpack new--from--old.kdelta rebuilt.kpack
sha256sum rebuilt.kpack
```

`delta` names its output `<new>--from--<old>.kdelta` when given no name, and says how much smaller
than the pack it is. `apply` writes the pack and prints
`<out> written — check it against the index's C: before using it`; it does not check what it
rebuilds. Compare the result's SHA-256 with the `C:` line of the pack in a signed index before you
install it. Nothing on the system applies a delta by itself.

### The KDOS base pack

On a medium built with `KDOS_PACK_KDOS=1`, the base pack `kdos` gives a clean KDOS root to build
ports in, without touching the running system:

```bash
kdos-box create ports base=pack:kdos
kdos-box enter ports
```

### Rolling back or removing a pack

The daemon has `rollback <id>`, which swaps the installed pack with the newest kept version,
unmounting it first, and `remove <id>`, and both refuse while a box is composed over the pack. No
command on the system wraps either. The daemon's socket takes one line per connection, so `socat`
reaches every verb, `list`, `info <id>`, `mount <id>`, `install <staged file>` among them, as
root or as a member of `wheel`:

```bash
echo list | socat - UNIX-CONNECT:/run/kdos-packd.sock
echo 'rollback app.hugin' | socat - UNIX-CONNECT:/run/kdos-packd.sock
echo 'remove app.hugin' | socat - UNIX-CONNECT:/run/kdos-packd.sock
```

A rollback answers `ok <id> <installed version> -> <restored version>` and a removal
`ok <id> removed`. The daemon's row for an id is the installed `<id>.kpack`: `list`, `info`,
`mount` and `remove` act on it, and a kept `<id>-<version>.kpack` is reached only by `rollback`,
which takes the newest kept version by version comparison. See
[Installing a pack](../../kdos/03-architecture/packs-and-boxes.md#installing-a-pack).

### On the live medium and on an installed system

The daemon works the same on both. On the live medium the pack store is on the session's overlay,
so an imported pack is gone at power-off unless the medium has a
[persistence store](live-persistence.md). A box's writable layer is lost at the end of every live
session, persistence store or not: the daemon puts it on a tmpfs because the kernel will not stack
it on the overlay that holds your home. `kdos-appbox status` marks such a box `ephemeral`. An
installed system keeps both on disk.

## Tips

- `kdos-pack imagehash <pack>` hashes the filesystem image alone. Two builds of the same tree give
  the same hash whatever their versions, so it answers whether a rebuild changed anything; it does
  not cover the metadata.
- `kdos-pack extract-meta <pack>` prints the metadata, and `--icon` writes the icon instead.
- A pack in the store is not hashed again on each mount; a pack on the medium is hashed and
  checked whenever it is mounted, which on a slow stick takes seconds.
- When one id is both installed and on the medium, the installed copy is the one used.

## Troubleshooting

`kdos-appbox status` prints `packd : not answering`
: Check the service with `service status packd`, and read its boot log,
  `/run/kdos-init.packd.log`. A `[SKIP] kdos-packd: erofs unavailable` line means the kernel has
  no EROFS (`grep erofs /proc/filesystems`). Start it with `sudo service start packd`. When the
  service is `[ OK ]`, the daemon is refusing you: it answers `err not permitted` to anyone not
  root or in `wheel`, and `kdos-appbox status` reports that as not answering.

An import is refused for every pack
: The daemon answers root and `wheel` only; check with `id` that you are in `wheel`. A daemon
  started with `KDOS_REQUIRE_SIG` refuses every unsigned pack, which includes every pack an export
  made.

One pack is refused with `bad payload hash`, `bad signature` or `no key to check it with`
: Run `kdos-pack verify <pack>` to see which. A bad payload hash is a damaged or altered file: copy
  it again. A bad signature is a pack signed by a key that is not in the ring, or one whose
  signature does not verify against any key that is. `kdos-pack verify <pack> --keys <dir>` with
  the signer's `.pub` in `<dir>` tells the two apart: it prints `signed by <key id>` for the
  first, and then adding that `.pub` to `/etc/kdos/keys/packs/` lets the daemon install it. A
  pack that still fails against its own key should not be used. `no key to check it with` appears
  only when the ring is empty.

An install is refused because a box is composed over the older version
: Stop and remove the box that uses it (`kdos-box stop <name>`, then `kdos-box remove <name>`; the
  profile and writable layer are kept), install, and launch the application again, which creates
  the box from the kept profile.

A frozen box imports and its box is not created
: Its base pack is not on this machine, and the daemon answers that nothing provides what it
  requires. `kdos-pack info <pack>` shows the `requires` line; import that pack first.

`kdos doctor` warns that the home directory cannot hold a box's writable layer
: The home is on the boot overlay (a live session), so a pack box's writes go to memory. See
  [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box).

## Known gaps

- [Applications imported or built during an install stay in the live session](../../kdos/06-reference/known-gaps.md#applications-imported-or-built-during-an-install-stay-in-the-live-session)
- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
- [A catalogue change reaches an installed application only when it is rebuilt](../../kdos/06-reference/known-gaps.md#a-catalogue-change-reaches-an-installed-application-only-when-it-is-rebuilt)
- A data set built by `kdos app install` is an image and is grafted nowhere; only an imported data
  pack is grafted. See [Grafts and data packs](../../kdos/03-architecture/packs-and-boxes.md#grafts-and-data-packs).
- No harness exercises the daemon's `rollback`; see the kdos-packd row of
  [Applications and boxes](../../kdos/06-reference/status.md#applications-and-boxes) in the status page.

## See also

- [Packs and boxes](../../kdos/03-architecture/packs-and-boxes.md): the format, verification, mounting and composition
- [kdos-packd](../../kdos/04-programs/daemons.md#kdos-packd): the daemon's verbs, paths and variables
- [kdos-appbox: export and import](../../kdos/04-programs/kdos-appbox.md#export-and-import): what a set holds and how it is checked
- [Carrying a set to another machine](../../kdos/02-user-guide/applications.md#carrying-a-set-to-another-machine) in the user guide
- [Boxes](boxes.md), [Containers](containers.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 407.* Previous: [406. Local rebuilds and kdos march](local-rebuilds-and-march.md) · [Contents](../README.md) · Next: [408. Boxes](boxes.md)
