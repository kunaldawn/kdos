# The binary host

The first step of the image phase does nothing on an ordinary build. With
`make build KDOS_MAKE_BINHOST=1`, `script/phases/70_image/010_binhost.sh` moves every package the
build kept into `build/binhost/` and signs an index over them. That directory is then a
[binary host]{.idx}: a directory of prebuilt packages that another KDOS machine trusting its key
installs from, with `kdos update install` or `kpkg binhost`, instead of compiling. Published, it
is the package repository of a release: it holds every port the release built, the ISO carries
only the [base](017-stage.md), and a machine installs everything else from the binhost of the
release its `/ports/TREE` names. The step runs on `build/fs`, before the stage, so it sees every
package. This page shows the step, the key it signs with, and which packages end up in the
directory.

## Switching it on

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=59-62 pin=73a5f902efa6 -->

`KDOS_MAKE_BINHOST` comes from the `make build` command line, through the `Makefile` and
`script/chroot/exec.sh`. Set to anything but `1`, the step prints the line above and exits.

The flag has to be on for the whole build, not only for this phase. `exec.sh` turns the same value
into `KPKG_KEEP_CACHE` for every chroot command, and with that set, each `kpkg install` keeps the
package it installs in `/var/cache/kpkg/packages` instead of deleting it, compressed with the
heavier `xz` setting a kept package gets (see
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md#package)). A package reused from the
package store is copied into the cache as well. A port that is installed and current is skipped
by kpkg and contributes nothing. On your machine:

```bash
make build KDOS_MAKE_BINHOST=1
```

## The signing key

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=64-74 pin=79afd37a89a5 -->

The key pair is made once, by `kpkg keygen`, in `build/binhost-key/`, and every later run signs
with the same key. kpkg writes the private half with mode 0600 and never replaces an existing
key. Neither half enters the image: `build/` is not part of the root. The public half,
`kdos-binhost.pub`, is copied beside the packages, and a machine trusts the binhost by holding a
copy of it in `/etc/kdos/keys`. Deleting `build/binhost-key/` makes a new key on the next run, and
every machine holding the old public half then refuses the index. `make clean` and
`make cleanbuild` keep it, as they keep `build/keys/` with the kernel's module-signing key and the
release-signing key, so only removing the directory by hand makes a new key.

## Moving the packages

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=76-88 pin=19babb00269e -->

Every package in the cache is moved into `build/binhost/`, over a file of the same name, which
leaves `build/fs`'s cache empty for the next build in either image mode: under `KDOS_IMAGE=base`,
`020_cleanup.sh` empties only the stage's cache. Nothing is removed from `build/binhost/`, so it
accumulates from run to run, and an incremental build keeps only the packages it installed. A run in
which nothing was built adds nothing, and the step stops only when the whole directory holds no
package.

An older package left by an earlier run is indexed beside its successor, but with an empty `E:`.
The index takes each package's recipe hash from the ports tree at the moment the index is written,
by package name (see [The index](#the-index)), and gives it only to the file whose version and
release are the ones the recipe declares. A superseded version or release is listed and no client
installs it, and the release index leaves it out (see [The release index](#the-release-index)),
so old files need no clearing for the binhost to serve the current recipes.

The one file the hash cannot tell apart is a package of the current version and release that was
built from an earlier state of the recipe, by a run without the flag set after the run that kept
it: it carries the current hash and passes a client's recipe test. A run with the flag that
rebuilds the port copies the new package over it. A binhost that holds nothing but this tree's
builds starts from an empty `build/binhost/` and a build in which every port is installed, which
is a build from an empty tree. With the tree emptied as
[Starting from nothing](../01-preparing/build-tree.md#starting-from-nothing) describes:

```bash
rm -rf build/binhost
make build KDOS_MAKE_BINHOST=1 BUILD_ARGS=--fresh
```

## The index

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=90-100 pin=f9126924ee3d -->

`kpkg index --sign` writes the index, `PACKAGES`, its signature `PACKAGES.sig`, and a signature
beside each package, over every package in the directory. `kpkg verify-index` then reads the
index back with `KPKG_KEYRING` pointed at `build/binhost-key/`, so the one key the index was
signed with is the only key trusted, as a client holding that key would read it. A failed
verification stops the phase.

Two fields of the index limit who can use it. The build-config hash, `B:`, is taken from the
environment the index is written in, which is the chroot's, so a client has to run `kpkg binhost`
with the same compiler and the same `CC`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS` and `KDOS_TARGET`, or
every package reads as a different build config; `kdos update install --source` builds under
`/etc/kdos/chroot.env` for that reason. The recipe hash, `E:`, is looked up in the `PORT_REPO` the
step sets: `/ports/core` and the four areas of `src/` at `/kdos/src`. A machine finds the same
recipes under `/ports/src/<area>`, the copies [105_ports.sh](105-ports.md) stages, and computes
the same hash, so a program of KDOS's own is taken prebuilt like any other port.

The index format and the rules a client applies are in
[The binary host](../../kdos/03-architecture/packaging.md#the-binary-host) and
[Signing](../../kdos/03-architecture/packaging.md#signing).

## The catalogue

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=102-123 pin=3b85a50dfbf5 -->

Discover lists what a binhost can add from an AppStream catalogue beside its index, which a client
asks for as `binhost/swcatalog/xml/kdos-binhost.xml` and keeps at that path under
`/var/lib/kdos/binhost`; `kdos update fetch` downloads it through the release index below and
verifies it against the same keys. The step writes it as `swcatalog--xml--kdos-binhost.xml`, a flat
name, because a GitHub release asset cannot sit in a directory, and it writes it with `kdos-discover-catalog --origin kdos-binhost`, from
the [kdos-discover](#port-kdos-discover) port, naming each package the index lists that this tree
has installed: a package file in the directory whose package this tree does not have stays in the
index and gets no component. `kpkg sign` signs it with the index's key. A tree without
`kdos-discover-catalog` gets an index and no catalogue, and Discover then offers nothing from the
binhost. How each component is read is on [The software catalogue](086-swcatalog.md).

## The release index

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=125-159 pin=260e907312f3 -->

The release index carries this tree's packages and no others. The step reads `PACKAGES` and marks
every [superseded]{.idx} package: a package stanza, one with no `O:` (which marks a delta), whose
`E:` is empty. Those files stay in `build/binhost` and in `PACKAGES`, and are never published. A
current package larger than one GitHub release asset, `SRC_PART_SIZE` from `ports/srclib.sh`
(1,900 MiB unless `KDOS_PART_SIZE` names another), stops the phase, naming the file: a machine
takes a package whole, and the release could not carry it.

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=160-174 pin=44ef9b003423 -->

`kdos update` never asks for a binhost file by its name alone: it asks the [release index]{.idx},
`kdos-update.idx`, where the file is, and keeps it only with the size and SHA-256 the index gives.
The step writes the directory's own index last, over every file in it but the index, the public
key and the superseded packages with their signatures: each package, sidecar and delta at
`binhost/<name>`, the index and its signature at
`binhost/PACKAGES` and `binhost/PACKAGES.sig`, and the catalogue at its path above, every one with
the release `-`, which means beside the index. `ports/releaselib.sh` is the writer, the same one
`ports/publish --release` uses for a release. The `version` is the short commit hash the build
records, cut to twelve characters, which is what a machine whose `/ports` names no release calls
its tree. `kpkg sign` signs it with the binhost key and `kpkg verify-pkg` reads it back against that key alone, so a
machine trusting `kdos-binhost.pub` takes the directory, copied to a stick or served as it is, as
the place `binhost =` names in `/etc/kdos/update.conf`. The format, and the published form whose
files are in releases `binhost-<version>-01`, `-02`, … of at most 1,000 assets each, are in
[The release index](../../kdos/03-architecture/packaging.md#the-release-index) and
[Publishing a release](../../kdos/03-architecture/packaging.md#publishing-a-release).

## What it leaves

Nothing on the image. On your machine, `build/binhost/` holds the packages, `PACKAGES`,
`PACKAGES.sig`, the per-package signatures, `kdos-binhost.pub`, the catalogue with its signature
and `kdos-update.idx` with its signature, every name flat, and `build/binhost-key/` holds
the key pair. With `KDOS_ISO_SOURCES=1`, `110_iso.sh` also puts a copy of `build/binhost/` on the
medium, under `/sources/binhost`, when the same build sets `KDOS_MAKE_BINHOST=1` and the directory
holds an index. A binhost an earlier opt-in build left behind stays off the medium.

## See also

- [The binary host](../../kdos/03-architecture/packaging.md#the-binary-host): the index, the tests a client applies and `kpkg binhost`
- [Signing](../../kdos/03-architecture/packaging.md#signing): the key rules
- [Publishing a release](../../kdos/03-architecture/packaging.md#publishing-a-release): how `ports/publish --release` puts this directory in a release
- [The package store](015-pkgstore.md): the other cache of built packages, used during a build

<!-- book-nav -->
---

*Part XV. The image, chapter 364.* Previous: [Part XV. The image](intro.md) · [Contents](../README.md) · Next: [365. The package store](015-pkgstore.md)
