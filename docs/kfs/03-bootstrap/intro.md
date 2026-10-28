# The bootstrap

<!-- kfs:phase 10_bootstrap -->

`10_bootstrap` uses the cross compiler to build, into `build/fs`, the smallest root that can
compile the rest of KDOS by itself: the Linux API headers, [musl]{.idx}, [libstdc++]{.idx}, the GNU userland a
configure script expects, bash, a native binutils and GCC, make, and finally kpkg and kinstall. Each
of its twenty-two step scripts has a chapter here, in the order the phase runs them. When the
phase ends, the root is entered as a chroot and every later package is built by kpkg from a
recipe. This page covers what every step has in common: its shape, the mark that lets it skip,
how it finds its source without kpkg, and what it leaves behind for the package phases.

The phase takes about 20 SBU, and more than half of that is the native GCC.

## The bootstrap and Linux From Scratch

A reader who knows the Linux From Scratch book will recognise the plan, because it is the same
plan. LFS builds a cross toolchain in its chapter 5, and so does KDOS: binutils and a first GCC in
[Part II](../02-cross/intro.md), then the Linux API headers, the C library and libstdc++ in steps
010 to 030 here. LFS's chapter 6 cross-compiles a set of temporary tools into the new root, ending
with a second binutils and GCC that run on it, and steps 040 to 110 do the same, with the same
`--host` switch, in most steps the same `--build`, and the same `DESTDIR` installs. Then both books enter the new root
as a chroot.

The differences are of three kinds. The C library is musl, not glibc, so the triplet ends in
`linux-musl` and there is no `ld-linux` to arrange; musl's dynamic linker is its `libc.so`. The
tool set differs: there is no m4 and no file, and readline is built on its own so that bash can
link it. And KDOS adds two steps LFS has no counterpart for, `120_kpkg.sh` and `130_kinstall.sh`,
because from `20_selfhost` on nothing is built by hand: every later package is a port that kpkg
builds from a recipe. The cross toolchain also lives outside the root, in `build/cross`, rather than
in a `tools` directory inside it, so nothing has to be cleaned out of `build/fs` when the [bootstrap]{.idx}
is over.

Like LFS's temporary tools, almost nothing here is meant to last. `20_selfhost` and `30_foundation`
rebuild the toolchain and the userland as ports, and the `linux` port in `60_kernel` takes over the
API headers. kpkg and kinstall are the exception: no phase builds them again, and the copies made
here are the ones the image ships. Each page of this part ends with a section saying how the later
build differs, or why there is none.

## The shape of a step

Every package step has the same frame. `066_grep.sh` is a plain one, shown whole:

<!-- kfs:include script/phases/10_bootstrap/066_grep.sh -->

`set -e` stops the step at the first command that fails, and kdosbuild stops the build with it.

`phase.env` sources `script/env/host.env`, the environment of
[Part II](../02-cross/intro.md#the-environment), and sets `MARK` to `build/mark/bootstrap`.
`port.sh` supplies `extract_port_source`.

The `if` is the step's guard, described under [Marks](#marks-and-kdos_replay) below.

`--host=$KDOS_TARGET` makes configure compile with the `x86_64-kdos-linux-musl-` tools.
`--build` names the machine the build runs on, as the source's own `config.guess` sees the
container. With both given and different, configure knows it is cross-compiling and never runs a
test program it compiled. A few configure tests cannot be answered without running one; where a
step answers such a test itself, its page says so.

Given `--host` alone, configure decides for itself, by compiling a program and trying to run it.
The program is linked against `build/fs`'s musl, but its interpreter path,
`/lib/ld-musl-x86_64.so.1`, also exists in the Alpine container, so it runs, and configure
concludes it is not cross-compiling. The logs of `050_xz.sh`, `070_readline.sh`, `080_bash.sh`
and `110_make.sh` say `checking whether we are cross compiling... no`: their configure tests ran
in the container, against the container's musl. Steps 030, 040, and 060 to 068 pass `--build`
and report `yes`. The ncurses log also holds one `no`, from the first configure in
`040_ncurses.sh`, which builds a `tic` for the container itself.

`--prefix=/usr` with `DESTDIR=$SYSROOT` installs into `build/fs/usr` while compiling `/usr` into the
programs, which is where they will find their files once `build/fs` is the root.

The last two lines delete the unpacked source and touch the mark.

## Marks and KDOS_REPLAY

The mark is an empty file in `build/mark/bootstrap/`, named after the step, and a step whose mark
exists exits at once. A finished bootstrap is therefore done once per tree, and running the phase
again costs seconds. Two steps differ. `000_file_system.sh` has no guard and runs every time,
because it is the step that copies `fs/` into the root. `120_kpkg.sh` writes a hash of every source
file kpkg is compiled from into its mark and skips only while that hash still matches, because no
port rebuilds kpkg.

kdosbuild exports `KDOS_REPLAY` to every step: `1` for a step a build plan names, `0` otherwise. A
guard stands down when it is `1`, so one step is run again with:

```sh
make build BUILD_ARGS="--phases 10_bootstrap --steps 10_bootstrap:062_coreutils.sh"
```

That is safe only on a tree that has not gone past `10_bootstrap`. On a later tree the step installs
its early build over the files the port has put there since, without telling kpkg, and what fails
next looks like an ordinary build error. `000_file_system.sh` is the exception: it is meant to be
run on any tree, and it is how a change under `fs/` reaches `build/fs`. See
[Re-running an early phase on a later tree](../../kdos/05-developer/build-troubleshooting.md#re-running-an-early-phase-on-a-later-tree)
and
[Guards in the early phases](../../kdos/05-developer/build-system.md#guards-in-the-early-phases).

## How a step finds its source

kpkg does not exist yet, so a step cannot ask it for a port's source. `script/lib/port.sh` does
the part of kpkg's work the bootstrap needs, in three functions:

`port_dir NAME`
: Prints the directory of the port, `ports/core/<shelf>/NAME`, one shelf down and holding a
  `kpkgbuild`. Exactly one shelf may hold the name: a second is an error naming both, and none is
  `Port not found`. A recipe loose at `ports/core/NAME` is not a port and is never found.

`extract_port_source NAME`
: Reads the port's `kpkgbuild`, chooses its archive, unpacks it into `build/tmp`, and prints the
  directory the archive made.

`get_port_version NAME`
: Prints the recipe's `version`. Steps 030 and 100 use it for GCC's versioned paths.

The recipe is read, not sourced. Only `name`, `version`, `release`, `vendoring`, `source` and the
recipe's helpers (any key that is not on kpkg's list) are evaluated, by bash's own parameter
expansion, which is the subset of shell the recipes are written in, so `$name`, `${version%.*}`
and a helper declared above `source` all expand. A repeated `source` line adds to the list. Every
other key — `sha256`, `description`, `homepage`, `depends`, `pypackages`, `license`, `group`,
`secdb`, `replaces`, `bench`, `bench_setup`, `pyrequirements`, `pyruntime` and `watch` — is free
text and is skipped, never evaluated. A helper is still evaluated, so a backtick or `$(` in one
runs as a command when the recipe is read.

The archive is the first `source =` entry that is an archive present in the port directory. An
entry `FILE::URL` names its file outright. When the first entry is a plain URL, its local name is
`$name-$version` with the URL's archive extension, `.tar.xz` for instance, which is the name
`make fetch` stores it under; any later URL keeps its own file name. The file is in the port
directory because `make fetch` put it there, linked from `ports/.srccache`, and a step that reports
`Source tarball not found` is a tree that has not been fetched (see
[Sources and the archive](../01-preparing/sources.md)). It is then unpacked:

<!-- kfs:include script/lib/port.sh lines=158-167 pin=69f7883feabb -->

The printed path is `build/tmp/$name-$version`, so a port whose archive unpacks to a directory of
any other name cannot be built by a bootstrap step.

What the function does not do matters as much. It checks no hash, because `make fetch` checked
every archive when it stored it. It applies none of the port's patches: the one step that needs a
patch, `100_gcc.sh`, applies it by hand. It never reads the port's `build.sh`, so a step's commands
are its own, and the port builds the same source its own way later.

## What the bootstrap leaves behind

Nothing installed here belongs to a package. When a package phase installs a port over a path no
package claims, kpkg adopts the path, so the hand-installed files of the bootstrap become package
files one port at a time (see
[Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file)). A file the port would
not install is adopted by nobody and stays, unowned, for good. The steps therefore delete what
their port leaves out: the libtool archives (`.la`), which kpkg strips from every package and which
would otherwise put build-directory paths on later link lines, findutils' `locate`, which belongs
to plocate, gawk's C-shell profile script and maintainers' notes, binutils' manual pages for the
Windows tools, and the message catalogues, translated manual pages and HTML manuals that kpkg's
[size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy)
prunes from every package: every step whose package has translations is built with
`--disable-nls` (the list is in [What the bootstrap leaves](what-you-have.md#what-is-not-there-yet)),
and the readline and bash steps delete their HTML manuals. The image phase removes any such catalogue or
manual that is still unowned (see
[Unowned documentation and catalogues](../15-image/020-cleanup.md#unowned-documentation-and-catalogues)),
and stops on any unowned file left under `/usr/share/locale`, `man`, `doc` or `info` (see
[The three checks](../15-image/087-gates.md#the-three-checks)).

## See also

- [Part II](../02-cross/intro.md): the compiler this part uses
- [Part IV](../04-chroot/intro.md): entering the root this part builds
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md#a-minimal-kdos-10_bootstrap): the bootstrap in the whole journey
- [Build plans](../../kdos/05-developer/build-system.md#build-plans): naming one step to run again

<!-- book-nav -->
---

*Part III. The bootstrap.* Previous: [7. Cross GCC, pass 1](../02-cross/gcc.md) · [Contents](../README.md) · Next: [8. The file system](000-file-system.md)
