# kpkg

[`kpkg`]{.idx} is the [package manager]{.idx} of KDOS. The build uses it to make every package from its
port, and the same program is on every installed system and on the live medium, where it lists
what is installed, says which package owns a file, checks the installed files against what was
installed, removes packages, and builds and installs a port from the recipe tree at `/ports`. This
page is about that second use, on a running machine. Updating the whole system is
[Updating](updating.md); rebuilding a port with a recipe of your own is
[Local rebuilds and kdos march](local-rebuilds-and-march.md). What `kpkg install` does step by step
is [Anatomy of a kpkg install](../04-chroot/kpkg-install.md).

## Installation

`kpkg` is not a port, because building a port needs it. The bootstrap step `120_kpkg.sh` compiles
it from `src/system/kdos-kpkg` and three of KDOS's own libraries, and installs it into the root as
`/usr/bin/kpkg` with four symbolic links to it; [kpkg](../03-bootstrap/120-kpkg.md) in Part III
describes the step. No later phase rebuilds it, and no package database entry owns it, so
`kdos why /usr/bin/kpkg` reports that no package provides it.

The five names, each dispatching on the name it was run under:

| Name | Does |
|---|---|
| `kpkg` | The front end: every command below |
| [`kpkgadd`]{.idx} | Installs a package file that is already built |
| [`kpkgbuild`]{.idx} | Builds the port in the current directory into a package, and installs nothing |
| [`kpkgdel`]{.idx} | Removes packages |
| `kpkgdepends` | Prints the resolved install order, and nothing else |

Every image carries the recipe tree at `/ports`: each port's `kpkgbuild`, `build.sh` and patches,
`ports/shelves`, `ports/sources.idx`, and KDOS's own programs under `/ports/src`, with no source
archive ([105_ports](../15-image/105-ports.md) stages it). The trust ring for prebuilt packages,
[`/etc/kdos/keys/`]{.idx term="/etc/kdos/keys"}, comes from the base filesystem, `fs/etc/kdos/keys/`.

`kpkg` needs no service. It downloads sources with `curl`, which the [curl](#port-curl) port
installs.

## Configuration

### /etc/kpkg.conf

Where `kpkg` finds ports and keeps its trees. The bootstrap step copies it into the root:

<!-- kfs:include script/phases/10_bootstrap/120_kpkg.sh lines=66-66 pin=67a31cf964ce -->

The file is `src/system/kdos-kpkg/kpkg.conf`:

<!-- kfs:include src/system/kdos-kpkg/kpkg.conf -->

Every line is `NAME="${NAME:-default}"`. `kpkg` reads it as a key and a value without running a
shell, and an environment variable of the same name wins over the file, so one command can point
`kpkg` somewhere else without editing anything:

```bash
PORT_REPO="/home/kdos/ports /ports/core" kpkg info foo
```

Each directory `PORT_REPO` names is searched for a port at `<name>/` and one shelf down at
`<shelf>/<name>/`, and the first directory holding the name wins. The keys and their limits are in
[/etc/kpkg.conf](../../kdos/06-reference/configuration.md#etckpkgconf).

### The package database

`/var/lib/kpkg/db/` is written only by `kpkg` itself, as it installs and removes packages. Do not
edit it by hand. For each installed package it holds `<name>` (the version and release, then every path the
package owns), and `.recipe/<name>`, `.pkgsha/<name>` and `.files/<name>` beside it;
[The database entry](../04-chroot/kpkg-install.md#the-database-entry) shows the four files of one
package, and [Files kpkg keeps to itself](../../kdos/03-architecture/packaging.md#files-kpkg-keeps-to-itself)
lists the rest.

### /etc/kdos/keys/

The keys `kpkg` accepts a signature from, for a binary host's index and for a package's own `.sig`.
The image ships the directory with no key in it, only its description:

<!-- kfs:include fs/etc/kdos/keys/README -->

The two subdirectories, `packs/` and `tree/`, are separate rings that `kpkg` does not read; they
belong to [Packs](packs.md) and [Updating](updating.md).

### Environment variables

The settings that change what `kpkg` does, rather than where its trees are, are read from the
environment only. Two matter on a running machine: `KPKG_FETCH=1`, which lets a build download the
sources it lacks, and `KPKG_REQUIRE_SIG=1`, which refuses any package file without a good
signature. `KPKG_CONF` names another configuration file and `KPKG_KEYRING` another trusted-key
directory. Every variable is in the table under
[kpkg](../../kdos/03-architecture/packaging.md#kpkg); the image sets none of them.

## Usage

Every command that changes the root runs as root. The ones that only read it do not, except where
a file they read is root's own.

### Seeing what is installed

1. List every installed package with its version and release:

   ```bash
   kpkg list
   kpkg list --json
   ```

2. Show one package. For an installed package this is its version, release, licence and file
   count; for one that is not installed, the recipe's description, licence and `depends =` list,
   and `Status: not installed`:

   ```bash
   kpkg info zlib
   ```

   ```text
   Package: zlib
   Version: 1.3.2-1
   License: Zlib
   Files: 19
   ```

3. Find the package that owns a file. `kdos why` reads the database's own file lists and prints the
   package, its version and its recipe, then any reason the system records for that path:

   ```bash
   kdos why /usr/bin/xz
   ```

   A path no package owns is reported as `none — provided by fs/ or 10_bootstrap, or not
   installed`: the base filesystem is copied into the image rather than installed by a package,
   and `kpkg` and `kinstall` are compiled by `10_bootstrap` and are not packages either. The [manifest]{.idx} itself
   can be searched too:

   ```bash
   grep -lx './usr/bin/xz' /var/lib/kpkg/db/*
   ```

4. Print a recipe's metadata as shell assignments, expanded:

   ```bash
   kpkg meta xz
   ```

### Checking the installed files

`kpkg check` compares the root with what was installed: every path a package's manifest lists, and
the SHA-256 and mode of every regular file it placed.

1. Check one package, or all of them:

   ```bash
   sudo kpkg check openssh
   sudo kpkg check --all
   ```

2. Read the findings. `missing`, `changed` and `mode` lines are faults and make the exit status 1;
   a `config` line is a file under `/etc` that someone edited, and an `unowned` line (with `--all`)
   is a file under `/usr` or `/opt` that no package claims. Neither of the last two changes the
   exit status. Each line names the package first, and the last line counts them:

   ```text
   libvirt: config ./etc/libvirt/qemu/networks/default.xml
   unowned ./usr/bin/kpkg
   2107 packages checked: 0 missing, 0 changed, 0 mode, 25 config, 73 unowned
   ```

   That is the freshly booted image, and it exits 0. The image leaves out the build's logs and
   caches but carries every directory a package claims under `/var/log` and `/var/cache`, empty
   ([110_iso](../15-image/110-iso.md)), so none of them is `missing`. Its `config` lines are
   libvirt's network and filter files under `/etc/libvirt`, which `libvirtd` rewrites at its first
   start, so their recorded hashes no longer match; the `unowned` ones include
   `kpkg` and `kinstall` themselves. The meaning of each line is in
   [Checking an installed root](../../kdos/03-architecture/packaging.md#checking-an-installed-root).

3. Check another root, such as a mounted second slot, with `--root`:

   ```bash
   sudo kpkg check --all --root /mnt/slot-b
   ```

### Building and installing a port

On a running machine a port is built from `/ports` with its sources fetched over the network,
because `/ports` holds no [source archive]{.idx}. This needs the network, and it compiles on the machine.

The image carries almost every port already installed, `pv` among them, so to follow these steps
as written remove it first with `sudo kpkg remove pv`; for a port that is installed, step 1
prints an empty line and step 3 prints `Nothing to do`.

1. Look at what would be built. `kpkgdepends` prints the install order, leaving out every package
   that is already installed, so with `ncurses` present it prints `pv` alone:

   ```bash
   kpkgdepends pv
   ```

2. Fetch the sources of the port. `kpkg fetch` takes each file the recipe hashes and the machine
   lacks from the KDOS source archive first and from the recipe's own URL second, and keeps a copy
   in [`/var/cache/kpkg/sources`]{.idx} only when it matches the recipe's `sha256 =`:

   ```bash
   sudo kpkg fetch pv
   ```

   ```text
   ==> Fetching pv-1.12.0.tar.gz from the archive (sources-1)
   ```

   It exits 0 when every source is present, 1 when one could not be had, and 3 when nothing could
   be reached at all. See
   [Fetching sources on a machine](../../kdos/03-architecture/packaging.md#fetching-sources-on-a-machine).

3. Build and install it. With `KPKG_FETCH=1` the port, and each dependency built on the way,
   fetches whatever sources are still missing as `kpkg` reaches it:

   ```bash
   sudo env KPKG_FETCH=1 kpkg install pv
   ```

   The build's output scrolls past, then `==> Package created:
   /var/cache/kpkg/packages/pv-1.12.0-1.tar.xz`, one `==> Installing` line per file, and
   `==> Package 'pv' installed successfully`. On an eight-thread virtual machine this takes about
   12 seconds. `--keep-cache` keeps the package file for a later `kpkgadd`.

4. Confirm the result:

   ```bash
   kpkg info pv
   kpkg check pv
   ```

   ```text
   1 package checked: 0 missing, 0 changed, 0 mode, 0 config
   ```

On a running system an installed package counts as current whatever its recipe says, so
`kpkg install` of an installed port prints `Nothing to do` and builds nothing. `-f` rebuilds the
named ports, though not their dependencies:

```bash
sudo env KPKG_FETCH=1 kpkg install -f pv
```

### Removing a package

```bash
sudo kpkg remove pv
```

`kpkg remove` is `kpkgdel`. It removes the files in the package's manifest that no other package
claims, and every directory of it that no other package's files still occupy. It does not check
reverse dependencies: `kpkg remove bash` removes bash. Before removing a library, list the recipes
whose `depends =` names it:

```bash
awk '$1 == "depends" { for (i = 3; i <= NF; i++) if ($i == "zstd") print FILENAME }' \
    /ports/core/*/*/kpkgbuild /ports/src/*/*/kpkgbuild
```

### Installing a prebuilt package

A package file built elsewhere, by `kpkgbuild` or kept in `/var/cache/kpkg/packages` by
`kpkg install --keep-cache`, is installed with `kpkgadd`. Its name is
`<name>-<version>-<release>.tar.xz`:

```bash
sudo kpkgadd /media/stick/pv-<version>-<release>.tar.xz
```

When a `.sig` file lies beside the package, `kpkgadd` verifies it against `/etc/kdos/keys/` and
refuses one that does not verify. A package with no `.sig` is installed, unless `KPKG_REQUIRE_SIG=1`
is set, when it is refused with `<file>: signature check FAILED — not installing`.

### Using a binary host

A [binary host]{.idx} is a directory of prebuilt packages with a signed index, `PACKAGES`. KDOS
publishes none; a build machine can write one with `make build KDOS_MAKE_BINHOST=1`, which
[010_binhost](../15-image/010-binhost.md) indexes.

1. On the build machine, make a signing key once, and give the public half to the machines that
   should trust it. It writes `builder.key`, mode `0600`, and `builder.pub` into the current
   directory, and prints the key's id:

   ```bash
   kpkg keygen builder
   ```

2. On each client, trust the key by copying it into the ring:

   ```bash
   sudo cp builder.pub /etc/kdos/keys/
   ```

3. Check the index, then install a package from it:

   ```bash
   kpkg verify-index /mnt/binhost
   sudo kpkg binhost /mnt/binhost zlib
   ```

`kpkg binhost` uses a package only when its architecture, its build-configuration hash and its
[recipe hash]{.idx} all equal this machine's. It exits 0 when it used the package, 1 when nothing matched
(build from source instead), and 2 when it refused: no index (`ERROR: no index at
<dir>/PACKAGES`), no trusted key, or a failed verification. `--dry-run` says what it would do. As
shipped the ring is empty, so until step 2 `kpkg verify-index` answers
`ERROR: no trusted keys in /etc/kdos/keys` and exits 2. See
[The binary host](../../kdos/03-architecture/packaging.md#the-binary-host).

### On the live medium

`kpkg` works the same on the live medium, but the root it installs into is an overlay held in RAM,
so everything it installs or removes is gone at power-off unless the medium has a persistence store
(`kdos persist`; see [kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)). A large
build on the live medium also compiles into RAM: `/var/cache/kpkg/work` is on the same overlay.

## Tips

- `kpkg help` prints the full command list. It also lists `update`, which `kpkg` does not
  implement; the system is updated with `kdos update` ([Updating](updating.md)).
- `kpkg install --root DIR <port>` installs into another root, such as a mounted target or a
  second slot, and `kpkg list --root DIR` lists one.
- `kpkg check` reads the record taken after the package's install hook ran, so a mode a hook sets
  is not a finding.
- A failed build leaves its work directory, `/var/cache/kpkg/work/<name>`, for you to look at. The
  next build of the same port empties it; delete it yourself to reclaim the space.
- Python software that is not a port goes in a virtual environment, `python3 -m venv DIR`. The
  system's `site-packages` belongs to `kpkg`, and `pip install` outside an environment is refused
  with `error: externally-managed-environment`, whose text suggests `python3 -m venv ~/.venvs/NAME`.
- `kpkg check` run as an ordinary user cannot read a file only root may read, and reports it as
  `changed`: `openssh: changed ./usr/lib/openssh/ssh-keysign` is that, not a fault. Run it with
  `sudo` before believing a `changed` line.
- `/var/cache/kpkg` does not exist on a fresh image; the first `kpkg fetch` or build makes it.

## Troubleshooting

`sha256 MISMATCH for <file>`
: The copy of a source does not hash to the recipe's `sha256 =`. Delete it from
  `/var/cache/kpkg/sources` and fetch it again with `sudo kpkg fetch <port>`. A second mismatch
  means the archive and upstream both serve a different file from the one the recipe names.

`ERROR: Source not found: <file> (checked /ports/core/<shelf>/<name> and /var/cache/kpkg/sources)`
: The build reached a port whose source is not on the machine, and `KPKG_FETCH=1` was not set, so
  it did not download it. Run `sudo kpkg fetch <name>`, or repeat the install with
  `sudo env KPKG_FETCH=1`. A dependency that has to be built on the way, for example under
  `--root` into an empty root, stops the same way.

`No sha256 for <file> in the recipe`
: The recipe names a source it gives no hash for, and `kpkg` refuses to unpack it. The recipe is
  wrong; see [Checksums](../../kdos/05-developer/writing-ports.md#checksums).

`kpkg fetch` exits 3
: Nothing could be reached. Check the network with `nmcli general status` and try again.

`kpkg fetch` exits 1
: Neither the archive nor the recipe's upstream URL gave a copy with the recipe's hash. The file
  has to come from somewhere else, such as a medium built with `KDOS_ISO_SOURCES=1`, which holds
  every source beside its recipe under `/mnt/iso/sources/ports/core/<shelf>/<name>/`; copy it into
  `/var/cache/kpkg/sources`.

`ERROR: File conflict detected`
: The package holds a path another installed package owns. `kpkg` takes such a path only when the
  recipe names the owner in `replaces =`; see
  [Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file). Find the owner with
  `kdos why <path>`.

`Nothing to do`
: Every port named is installed, and on a running system that is enough. Add `-f` to rebuild it.

`ERROR: Build failed`
: Read the build's output above the line; the unpacked source is still in
  `/var/cache/kpkg/work/<name>/`. A build on a running machine does not get the compiler flags the
  tree's build sets; see
  [Local rebuilds and kdos march](local-rebuilds-and-march.md#troubleshooting).

`kpkg check` reports `changed` files under `/usr`
: Something wrote over a package's file. Rebuild that package with `kpkg install -f`, and find what
  wrote it before trusting the root.

A `kpkg install` or `kpkg remove` waits and never finishes
: Another `kpkgadd` or `kpkgdel` holds the database's lock, `/var/lib/kpkg/db/.lock`. Find it with
  `pgrep -a kpkg`. An install hook that runs `kpkg` itself waits on its own installer for ever.

## Known gaps

- [There is no public binary host](../../kdos/06-reference/known-gaps.md#there-is-no-public-binary-host)
- [A tzdata upgrade can reset the time zone to UTC, once](../../kdos/06-reference/known-gaps.md#a-tzdata-upgrade-can-reset-the-time-zone-to-utc-once)
- `kpkg help` lists an `update` command that does not exist; see
  [Packaging](../../kdos/06-reference/command-index.md#packaging) in the command index.

## See also

- [Packaging](../../kdos/03-architecture/packaging.md): ports, packages, the solver, the recipe hash and the binary host
- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md): one `kpkg install` followed from start to finish
- [kpkg](../03-bootstrap/120-kpkg.md): how the bootstrap compiles it
- [/etc/kpkg.conf](../../kdos/06-reference/configuration.md#etckpkgconf): every key
- [Updating](updating.md), [Local rebuilds and kdos march](local-rebuilds-and-march.md): the two wiki pages built on it

<!-- book-nav -->
---

*Part XVI. System administration, chapter 404.* Previous: [403. Firmware updates](firmware-updates.md) · [Contents](../README.md) · Next: [405. Updating](updating.md)
