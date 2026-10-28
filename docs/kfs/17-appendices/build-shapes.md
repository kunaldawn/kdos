# Canonical build shapes

Almost every `build.sh` in the tree is one of a handful of shapes, one per build system, changed
only where the project needs it. This appendix sets each shape out as the tree's own reference
gives it, from autotools to a port that only copies files, says what a recipe of that shape can
count on kpkg and the phase environment to have done before its first line runs, and links to the
section of [Writing ports](../../kdos/05-developer/writing-ports.md#canonical-build-shapes) that
explains each shape in full. The switches the shapes use are explained one by one in the
[flag glossary](flag-glossary.md).

## What every shape can count on

A `build.sh` is sourced by bash under `set -e`, with the unpacked source as its working directory.
kpkg sets these shell variables first, among others; they are not exported, so a program the
script starts does not see them:

`$name`, `$version`, `$release`
:   The recipe's own values, and every helper the recipe defines.

`$SRC`
:   The unpacked first source, with the archive's top directory stripped, and the working
    directory.

`$SRC_ROOT`
:   The port's work directory, `/var/cache/kpkg/work/<name>`, one level above `$SRC`. A second
    tarball is unpacked here, unstripped.

`$PKG`
:   The empty staging tree. Every shape installs into it, through `DESTDIR=$PKG`, `--root=$PKG`,
    `INSTALL_ROOT=$PKG` or an explicit path, and never into `/`.

`$PORT_SRC`
:   The port directory, where patches, vendor bundles and any other file the recipe carries are
    found: `/ports/core/<shelf>/<name>` in the chroot, or `/kdos/src/<area>/<name>` for a port of
    KDOS's own.

[Anatomy of a kpkg install](../04-chroot/kpkg-install.md#build) shows the short script kpkg hands
bash to set them, and the full list is in
[build.sh](../../kdos/05-developer/writing-ports.md#buildsh).

The rest comes from the phase. From `20_selfhost` on, every phase's `phase.env` sources
`script/env/chroot.env`, and that file sources `script/env/common.env` in turn. Between them they
export the compiler (`CC=gcc`, `CXX=g++` from `30_foundation` on), the release flags, the job
count (`MAKEFLAGS=-j$KDOS_JOBS`, `CMAKE_BUILD_PARALLEL_LEVEL`, `CARGO_BUILD_JOBS`), the
reproducibility settings and the defaults for the build systems that read no `CFLAGS`. This is
the whole of `chroot.env`:

<!-- kfs:include script/env/chroot.env -->

So no shape below names a compiler, an optimisation level or a job count. A recipe that changes the
flags extends them, `export CFLAGS="$CFLAGS -Wno-error"`, and never assigns them outright, which
would drop the hardening and the reproducibility flags at once; see
[The release flags](../../kdos/05-developer/writing-ports.md#the-release-flags).

After the script returns, kpkg tidies the staging tree the same way for every shape: it deletes
libtool's `.la` files and the shared index files it rebuilds at install (the info `dir`, the MIME
cache and each font directory's `fonts.dir`), copies the top-level licence files, removes the HTML
and PDF manuals under `/usr/share/doc` and every non-English catalogue, takes the symbol table out
of any program or library over 32 MiB, compiles Python bytecode,
runs the package lint and rolls the package. No shape does any of that itself. The steps are listed in
[What kpkg does around build.sh](../../kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh).

## autotools

A `configure` script, then make. [pv](#port-pv) and [acl](#port-acl) are built with this shape,
each with its own switches in place of some of these:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=759-761 pin=1455cb9a0681 lang=bash -->

configure takes the exported flags by itself. In the chroot phases it also reads the site file
`script/env/config.site`, which `CONFIG_SITE` names, and which places `sysconfdir`,
`localstatedir` and `sharedstatedir` at `/etc`, `/var` and `/var/lib` under `--prefix=/usr`. The
shape names `--sysconfdir=/etc` anyway, because a recipe built outside the chroot phases does not
read the site file. See [autotools](../../kdos/05-developer/writing-ports.md#autotools).

## meson

meson configures into a build directory and ninja builds it:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=781-784 pin=f119190e4a66 lang=bash -->

Three things in it are deliberate. `--libdir=lib`, because meson picks its default library
directory from the machine it runs on: `lib64` wherever `/usr/lib64` is a real directory, and the
package lint refuses a package that installs there. In the chroot `/usr/lib64` is a link to `lib`,
so the default is already `lib`; naming it keeps the result the same in any root. A named buildtype, because meson's default, `debug`, builds
`-g -O0`, and `testing/preflight.sh` fails a `meson setup` that names none. And every `-D` option
spelt as the project's `meson_options.txt` or `meson.options` spells it, because meson stops at
setup on an option it does not know.

The `meson` and `ninja` the recipe runs are the tree's wrappers in `script/bin/`, which
`chroot.env` puts first on `PATH` once `/usr/bin/ninja` is installed: the first adds `--wrap-mode=nodownload` to a setup that chooses
no wrap mode, so a missing subproject fails by name instead of being fetched, and the second adds
`-j$KDOS_JOBS` to a ninja call that names no job count. See
[meson](../../kdos/05-developer/writing-ports.md#meson).

## cmake

The reference shape uses make as CMake's generator:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=845-850 pin=d390ab3f3189 lang=bash -->

The tree's commonest form uses Ninja instead, with CMake's own build and install commands. The KDE
Frameworks are built this way, here [karchive](#port-karchive):

<!-- kfs:include ports/core/kf6/karchive/build.sh -->

`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, in the reference form, lets the tree's CMake
configure a project whose `cmake_minimum_required` is older than CMake still accepts, without a
patch. Release appends `-O3 -DNDEBUG` after the exported flags, so a CMake port builds at `-O3`
with assertions off. A misspelt option is only a warning in CMake, the opposite of meson, so the
log is the check. A KDE project names `KDE_INSTALL_LIBDIR=lib`, because ECM's `KDEInstallDirs`
picks `lib64` by itself, apart from the tree's `GNUInstallDirs` default. See
[cmake](../../kdos/05-developer/writing-ports.md#cmake) and
[A graphical application](../../kdos/05-developer/writing-ports.md#a-graphical-application).

## cargo

A Rust program builds from its vendor bundle, with no network:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=880-886 pin=56bff5f813e7 lang=bash -->

The bundle, `<name>-vendor-<version>.tar.xz`, holds every crate the program needs and the cargo
configuration that points cargo at them; [Sources and the archive](../01-preparing/sources.md#vendor-bundles)
says where it comes from. The `RUSTFLAGS` line is required: without `-crt-static`, the musl target
links statically, and a crate that binds a system library either fails to link or carries a
private copy of it. `testing/preflight.sh` fails a `cargo build` without it. The program is copied
into `$PKG` by hand, since `cargo build` installs nothing. See
[rust](../../kdos/05-developer/writing-ports.md#rust).

## go

A Go program builds from its module vendor tree, also offline:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=938-941 pin=945a281da9a7 lang=bash -->

`-ldflags "-s -w"` is required, because the Go linker otherwise writes DWARF and a symbol table that
kpkg does not strip; `testing/preflight.sh` fails a `go build` without both. `-trimpath`,
`-buildvcs=false` and `-buildmode=pie` come from the exported `GOFLAGS`, so a recipe that sets
`GOFLAGS` itself extends it. See [go](../../kdos/05-developer/writing-ports.md#go).

## Python, with pip

A Python package is built and installed by pip in one command. The commonest `build.sh` in the
tree is this single line, here [python3-calver](#port-python3-calver)'s:

<!-- kfs:include ports/core/python/python3-calver/build.sh -->

A package that needs something no port provides carries it in a vendor bundle and points pip at
it. [python3-jinja2](#port-python3-jinja2)'s bundle holds its build backend, `flit_core`, and the
recipe leaves out `--no-build-isolation`, so pip installs the backend from the bundle into a
temporary environment for the build:

<!-- kfs:include ports/core/python-libs/python3-jinja2/build.sh -->

The other switches keep pip away from the network and from the live root: `--no-index` stops it
consulting the package index, and `--find-links=vendor` points it at the bundle; `--no-deps`
installs the named distribution alone; `--no-build-isolation` builds with the backends already
installed as ports; and `--ignore-installed` with `--root=$PKG` writes into the staging tree
without uninstalling the copy the build root holds. kpkg compiles the bytecode afterwards. See
[The three Python keys](../../kdos/05-developer/writing-ports.md#the-three-python-keys) and
[Python, Perl, Haskell, Node and OCaml modules](../../kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).

## qmake

A Qt project that ships a `.pro` file:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=989-992 pin=3c09c3145853 lang=bash -->

qmake reads no flags from the environment, so the three `QMAKE_*_RELEASE` variables hand it the
exported ones, and `CONFIG+=release` keeps a debug build out. The install goes to `INSTALL_ROOT`,
qmake's name for the staging directory. See [qmake](../../kdos/05-developer/writing-ports.md#qmake).

## Perl modules

A CPAN module with a `Makefile.PL` is built by ExtUtils::MakeMaker. Most `perl-*` ports are this
script exactly, here [perl-authen-sasl](#port-perl-authen-sasl)'s:

<!-- kfs:include ports/core/perl-cpan/perl-authen-sasl/build.sh -->

`INSTALLDIRS=vendor` installs the module into perl's vendor directories, `/usr/share/perl5/vendor_perl`
and `/usr/lib/perl5/vendor_perl`, rather than the `site_perl` ones, which by Perl's convention are
for modules installed locally. The first two `find` lines remove what MakeMaker writes for a local
install: `.packlist`, the list of files the install wrote, and `perllocal.pod`, one file every
module's install appends to, which two packages would both claim, and which kpkgadd would refuse as
a conflict. The third removes the directories those deletions leave empty. An XS module takes perl's own recorded compiler settings, so the recipe adds no flags.
See [Python, Perl, Haskell, Node and OCaml modules](../../kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).

## Plain make

A project with only a makefile:

<!-- kfs:include docs/kdos/05-developer/writing-ports.md lines=1070-1072 pin=88831e0ba377 lang=bash -->

The flags are exported, not passed as make arguments. A variable on the make command line overrides
the makefile's own assignment of it, so passing `CFLAGS=...` there would discard whatever the
makefile itself puts in `CFLAGS`, its `-fPIC` or feature defines included. Read the makefile before
relying on this: one that assigns `CFLAGS` outright ignores the environment, and then the variable
is passed on the command line after all. See
[make-only](../../kdos/05-developer/writing-ports.md#make-only).

## Install only

A port whose source needs no build, such as a font, a script or a set of data files, is copied
into place with `install`. [font-carlito](#port-font-carlito)'s is the whole recipe:

<!-- kfs:include ports/core/fonts/font-carlito/build.sh -->

`install -m` sets each file's mode explicitly, `-D` creates the directories above the target, and
`-d` creates a directory alone. kpkg would copy a top-level `LICENSE` by itself; a file `build.sh`
has already put at that place is kept as it is. The font cache this package's files feed is rebuilt
when kpkgadd installs it, so the recipe runs no `fc-cache`; see
[Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes).

## Other build systems

Zig, Java, SCons, waf, Haskell's cabal and Node's npm have shapes of their own in the same chapter
of the KDOS book: [zig](../../kdos/05-developer/writing-ports.md#zig),
[java](../../kdos/05-developer/writing-ports.md#java),
[SCons, waf and other build tools](../../kdos/05-developer/writing-ports.md#scons-waf-and-other-build-tools)
and [Python, Perl, Haskell, Node and OCaml modules](../../kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).
Whatever the build system, the checks in
[Every port](../../kdos/05-developer/writing-ports.md#every-port) apply: a release build, every
compile line taking the exported flags, and no code tuned to the build machine's processor.

## See also

- [Flag glossary](flag-glossary.md): every switch the shapes and the recipes use most
- [Writing ports](../../kdos/05-developer/writing-ports.md#canonical-build-shapes): the shapes, with every exception
- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md): one autotools port from verify to adopt
- [Build troubleshooting](../../kdos/05-developer/build-troubleshooting.md): what fails when a shape is left

<!-- book-nav -->
---

*Part XVII. Appendices, chapter 422.* Previous: [Part XVII. Appendices](intro.md) · [Contents](../README.md) · Next: [423. Flag glossary](flag-glossary.md)
