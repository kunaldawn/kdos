## About

GNU binutils is the assembler, `as`, the linker, `ld`, and the tools that read and edit object
files: `ar`, `nm`, `objcopy`, `objdump`, `readelf`, `strip` and the rest, over the shared libraries
`libbfd`, `libopcodes`, `libctf` and `libsframe`. GCC assembles and links every program it
compiles through this package, and kpkg itself runs `objcopy` on the largest objects of a package
(see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

`20_selfhost` builds it after [zlib](#port-zlib) and [xxhash](#port-xxhash), which it builds
against, and before [gcc](#port-gcc); `30_foundation` names it again so a changed recipe is
rebuilt before the rest of that phase. It replaces the binutils
[Part III](#03-bootstrap-090-binutils) built with the cross compiler, and it is native to the
chroot's own triple, so it installs every tool under its plain name. The reasons are in
[The toolchain rebuilt as packages](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost).

The source is configured and built in a `build` directory of its own inside the tree, as binutils'
`README` recommends.

## Flags

- `--disable-werror` — do not turn compiler warnings into errors. configure adds `-Werror` under
  GCC whenever `bfd/development.sh` sets `development=true`, as a development snapshot's does; with
  the switch, a warning a newer compiler raises never stops the build, whichever kind of source
  the recipe names.
- `--enable-64-bit-bfd` — build `libbfd` with 64-bit addresses. On an x86-64 host this is already
  the case; the switch states it.
- `--enable-deterministic-archives` — make `ar` and `ranlib` behave as with `D` by default: zero
  owners, groups and timestamps, and one file mode, for every archive member and the index. A static library a port builds is then the
  same bytes on every build, which
  [Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages)
  depends on.
- `--enable-ld=default` — build the GNU linker and make it the default `ld`; it is installed as
  `ld` and as `ld.bfd`. The choice matters only beside gold, which configure builds only when
  given `--enable-gold`, and whose source the release tarball does not carry, so `ld.bfd` is the
  one linker the package installs.
- `--enable-lto` — build link-time-optimisation support into the tools.
- `--enable-plugins` — let `ld`, `ar`, `nm` and `ranlib` load plugins. `ld` loads GCC's LTO plugin
  through it whenever GCC links with `-flto`. `ar` and `nm` search `/usr/lib/bfd-plugins`, which
  holds only binutils' own `libdep.so`, so a port that archives LTO objects uses the `gcc-ar`
  wrappers (see [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port), "LTO
  only for the hot libraries").
- `--enable-shared` — build `libbfd`, `libopcodes`, `libctf` and `libsframe` as shared libraries,
  which the tools link, instead of linking a static copy into each program.
- `--with-system-zlib` — use the installed zlib instead of the copy in binutils' source tree, for
  reading and writing compressed debug sections.
- `--with-xxhash` — compile xxHash into `ld` from `xxhash.h`, which adds the `--build-id=xx` style.
  Given outright, configure stops if `xxhash.h` is missing rather than leaving the style out. The
  tree's own `LDFLAGS` ask for `--build-id=sha1`.
- `--without-zstd` — no zstd-compressed debug sections. [zstd](#port-zstd) does not exist in
  `20_selfhost`, where binutils is first built, so the switch keeps it off in every phase and `ld`
  is the same whichever phase last rebuilt it.
- `--without-debuginfod` — `readelf` and `objdump` do not fetch debug information from debuginfod
  servers. The library for it comes from elfutils, and the [elfutils](#port-elfutils) port is built
  without it.
- `--without-msgpack` — `readelf` does not decode the msgpack-encoded metadata notes of AMD GPU
  objects. msgpack is not a port.
- `tooldir=/usr` — on both `make` lines: put the tool directory, which holds a second copy of the
  main programs and the linker scripts, at `/usr` instead of `/usr/<triple>`. The programs then
  install once, in `/usr/bin`, and the linker scripts in `/usr/lib/ldscripts`, with no directory
  named after the target.

## Notes

The `install` line after the build copies the manual pages of the `binutils/` tools and of `as`
from the source's `doc` directories, where the release carries them already formatted, into the
package's `man1`. The `rm` line then deletes the pages of `dlltool`, `windres` and `windmc`,
which document Windows-target tools this build does not install.

The shared libraries carry the full version in their file name and soname
(`libbfd-<version>.so`), so a program linked against `libbfd` or `libopcodes` must be rebuilt
whenever binutils changes version.
