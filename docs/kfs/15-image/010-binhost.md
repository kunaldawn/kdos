# The binary host

The first step of the image phase does nothing on an ordinary build. With
`make build KDOS_MAKE_BINHOST=1`, `script/phases/70_image/010_binhost.sh` copies every package the
build kept into `build/binhost/` and signs an index over them. That directory is then a
[binary host]{.idx}: a directory of prebuilt packages that another KDOS machine trusting its key
can install from with `kpkg binhost`, instead of compiling. This page shows the step, the key it
signs with, and which packages end up in the directory.

## Switching it on

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=39-45 pin=aea8bfd4dfe3 -->

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

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=47-57 pin=79afd37a89a5 -->

The key pair is made once, by `kpkg keygen`, in `build/binhost-key/`, and every later run signs
with the same key. kpkg writes the private half with mode 0600 and never replaces an existing
key. Neither half enters the image: `build/` is not part of the root. The public half,
`kdos-binhost.pub`, is copied beside the packages, and a machine trusts the binhost by holding a
copy of it in `/etc/kdos/keys`. Deleting `build/binhost-key/` makes a new key on the next run, and
every machine holding the old public half then refuses the index. `make clean` and
`make cleanbuild` delete it too: the only key directory either keeps is `build/keys/`, which holds
the kernel's module-signing key and the release-signing key, not this one.

## Copying the packages

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=59-71 pin=740567ff5314 -->

Every package in the cache is copied into `build/binhost/`, over a file of the same name. Nothing
is removed from the directory, so it accumulates from run to run: `020_cleanup.sh` empties the
cache on every image run, and an incremental build keeps only the packages it installed. A run in
which nothing was built adds nothing, and the step stops only when the whole directory holds no
package.

An older package left by an earlier run is indexed beside its successor, and it is not told
apart from it. The index takes each package's recipe hash from the ports tree at the moment the
index is written, by package name (see [The index](#the-index)), so the older file carries the
current recipe's hash and passes a client's recipe test exactly as its successor does. A client
installs the first stanza that passes, and the index lists the packages in file-name order, so
after most version bumps that first stanza is the older package. A binhost meant to serve the
current recipes therefore starts from an empty `build/binhost/` and a build in which every port is
installed, which is a build from an empty tree. With the tree emptied as
[Starting from nothing](../01-preparing/build-tree.md#starting-from-nothing) describes:

```bash
rm -rf build/binhost
make build KDOS_MAKE_BINHOST=1 BUILD_ARGS=--fresh
```

## The index

<!-- kfs:include script/phases/70_image/010_binhost.sh lines=73-77 pin=b247da9e28f1 -->

`kpkg index --sign` writes the index, `PACKAGES`, its signature `PACKAGES.sig`, and a signature
beside each package, over every package in the directory. `kpkg verify-index` then reads the
index back with `KPKG_KEYRING` pointed at `build/binhost-key/`, so the one key the index was
signed with is the only key trusted, as a client holding that key would read it. A failed
verification stops the phase.

Two fields of the index limit who can use it. The build-config hash, `B:`, is taken from the
environment the index is written in, which is the chroot's, so a client has to run `kpkg binhost`
with the same compiler and the same `CC`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS` and `KDOS_TARGET`, or
every package reads as a different build config. The recipe hash, `E:`, is looked up with kpkg's
default `PORT_REPO`, and in the build chroot only `/ports/core` of that list exists: a package
built from `src/`, which is every program of KDOS's own, gets an empty `E:`, matches no client,
and is always built from source.

The index format and the rules a client applies are in
[The binary host](../../kdos/03-architecture/packaging.md#the-binary-host) and
[Signing](../../kdos/03-architecture/packaging.md#signing).

## What it leaves

Nothing on the image. On your machine, `build/binhost/` holds the packages, `PACKAGES`,
`PACKAGES.sig`, the per-package signatures and `kdos-binhost.pub`, and `build/binhost-key/` holds
the key pair. With `KDOS_ISO_SOURCES=1`, `110_iso.sh` also puts a copy of `build/binhost/` on the
medium, under `/sources/binhost`, when the directory holds an index.

## See also

- [The binary host](../../kdos/03-architecture/packaging.md#the-binary-host): the index, the tests a client applies and `kpkg binhost`
- [Signing](../../kdos/03-architecture/packaging.md#signing): the key rules
- [The package store](015-pkgstore.md): the other cache of built packages, used during a build

<!-- book-nav -->
---

*Part XV. The image, chapter 341.* Previous: [Part XV. The image](intro.md) · [Contents](../README.md) · Next: [342. The package store](015-pkgstore.md)
