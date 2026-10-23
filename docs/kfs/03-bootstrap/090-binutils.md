# Binutils

GNU binutils is the assembler, the linker and the tools that read and rewrite object files: `as`,
`ld`, `ar`, `nm`, `objdump`, `readelf`, `strip` and the rest. The binutils in `build/cross` runs only
in the container. This step builds a second, native one: compiled by the cross toolchain, it runs on
KDOS and produces programs for KDOS, so the GCC of the [next step](100-gcc.md) has an assembler and
a linker to call once the build moves inside `build/fs`. `090_binutils.sh` builds it from the
[binutils](#port-binutils) port's source tarball, the same source as
[Part II's cross binutils](../02-cross/binutils.md).

**Approximate build time:** 1.1 SBU

## Installation of binutils

The step unpacks the port's tarball and builds in a separate `build` directory, as binutils'
own documentation recommends:

<!-- kfs:include script/phases/10_bootstrap/090_binutils.sh lines=24-47 pin=41d2618656a3 -->

### The meaning of the configure options

`--host=$KDOS_TARGET`
: The tools run on KDOS. No `--target` is given, so the target defaults to the host, and the tools
  produce code for `x86_64-kdos-linux-musl` as well. Because host and target are the same, the
  programs are installed under their plain names (`/usr/bin/ld`, not `x86_64-kdos-linux-musl-ld`).

`--prefix=/usr`
: The programs go to `/usr/bin`. binutils also installs a second copy of its main tools, and the
  linker scripts, under its tool directory, `/usr/x86_64-kdos-linux-musl/`.

`--disable-nls`
: Build no message catalogues.

`--disable-werror`
: Never treat a compiler warning as an error, whatever the source's own default, so a warning the
  cross compiler raises does not stop the build.

`--disable-multilib`
: Build nothing for a second ABI such as `-m32`. KDOS has a 64-bit userland only.

`--enable-new-dtags`
: When a link is given a run-time search path, `ld` writes it as `DT_RUNPATH` instead of the older
  `DT_RPATH`. A `RUNPATH` yields to `LD_LIBRARY_PATH`; an `RPATH` is searched before it.

`--enable-gprofng=no`
: Do not build the gprofng profiler. binutils already leaves it out on a musl target; the switch
  states it.

`--enable-default-hash-style=gnu`
: The linker writes only the GNU symbol hash table (`.gnu.hash`). binutils' default on Linux is
  `both`, which also writes the older SysV `.hash`.

`--enable-shared` is not given, so `libbfd`, `libopcodes`, `libctf`, `libctf-nobfd` and
`libsframe` are built as static archives, each with a libtool archive (`.la`) beside it.

### Removing the libtool archives

The `rm -f` line deletes those five `.la` files. They are installed outside any package, so nothing
later removes them, and libtool copies an `.la` file's dependency list into every library linked
against it. Left in place, every later build that linked `libbfd` or `libopcodes` would inherit
them.

### Removing the Windows tools' manual pages

The last `rm` deletes the manual pages of `dlltool`, `windres` and `windmc`, which the install
writes although those programs are built only for Windows targets. The binutils port deletes the
same three pages from its package, so these copies would be adopted by no package and outlive every
install.

The step then removes the source and touches `build/mark/bootstrap/binutils`.

## Built again later

`20_selfhost` builds the [binutils](#port-binutils) port as its fifth list entry, and
`30_foundation` names it again so a changed recipe is rebuilt there. The package adopts
`/usr/bin/ld` and the other programs this step installed. The port build differs from this one in
several ways:

- It is native to the chroot's own triple, `x86_64-pc-linux-musl`, and installs with
  `tooldir=/usr`, so it writes no tree under a target name. The `x86_64-kdos-linux-musl` tool
  directory this step created belongs to no package; the [gcc](#port-gcc) package's install hook
  deletes it.
- It builds shared libraries (`--enable-shared`), the LTO plugin and plugin support
  (`--enable-lto`, `--enable-plugins`), and `--enable-64-bit-bfd`.
- It passes `--enable-deterministic-archives`, so `ar` writes zero timestamps and owners, which
  packages built twice from the same tree need in order to come out byte-identical.
- It links the system zlib and xxhash (`--with-system-zlib`, `--with-xxhash`) and turns off zstd,
  debuginfod and msgpack support.
- It passes none of `--enable-new-dtags`, `--enable-gprofng=no` or
  `--enable-default-hash-style=gnu`, so the packaged `ld` takes binutils' own defaults for them.
- It installs the manual pages of the tools by hand from the source's `doc` directories.

## See also

- [Bash](080-bash.md), the step before this one, and [GCC](100-gcc.md), the step after it
- [The binutils port](#port-binutils): its version, source and contents
- [The toolchain rebuilt as packages](../../kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost):
  why `20_selfhost` builds binutils again

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 25.* Previous: [24. Bash](080-bash.md) · [Contents](../README.md) · Next: [26. GCC](100-gcc.md)
