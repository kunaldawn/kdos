## About

musl is KDOS's C library and its dynamic linker in one file: `/usr/lib/libc.so` is the library,
and `/lib/ld-musl-x86_64.so.1`, the interpreter every dynamically linked program on the system
names, is a link to it. Why KDOS is built on musl rather than glibc, and what that costs, is in
[musl as the host C library](../../../../docs/kdos/01-philosophy/decisions.md#musl-as-the-host-c-library)
and [How KDOS differs](../../../../docs/kdos/01-philosophy/how-kdos-differs.md#the-c-library).

This port rebuilds, inside the chroot and with the bootstrap's native compiler, the musl that
[Part III](../../../../docs/kfs/03-bootstrap/020-musl-libc.md) cross-compiled, and kpkg adopts the files that step
installed. Every package of every later phase is linked against this build.

musl's `configure` is a short script of its own, not an autoconf one. It reads `CC` and `CFLAGS`
from the environment and takes its options in its own form.

## Flags

- `--enable-optimize` — compile the sources directly under `src/internal`, `src/malloc` and
  `src/string` at `-O3`; the makefile appends `-O3` to those files' `CFLAGS`, after the exported
  `-O2`. Left at its default, `auto`, configure does this only when `CFLAGS` names no optimisation
  level, and the chroot's `CFLAGS` carries `-O2`, so the whole library would be built at `-O2`.
  The patterns are `<dir>/*.c` and reach no subdirectory: the allocator musl builds, under
  `src/malloc/mallocng`, stays at `-O2`. The switch also adds musl's own tuning flags to every
  file (`-fno-align-jumps`, `-fno-align-functions`, `-fno-tree-ch` and the rest), which the
  default leaves out whenever `CFLAGS` names a level.

## Notes

`ldconfig`, which `build.sh` installs into `/usr/sbin` from the port directory, is a script that
does nothing and exits 0. musl's loader keeps no cache: it reads its search path from
`/etc/ld-musl-x86_64.path` on every program start (see
[Other machine files](../../../../docs/kdos/06-reference/configuration.md#other-machine-files)), so
there is nothing to rebuild. The script answers the makefiles and install scripts that run
`ldconfig` after installing a library, whatever arguments they pass.

The loader's path is written into every dynamically linked program as its interpreter, so it can
never move. `/lib` is a link to `/usr/lib` on KDOS, which is why the package records the loader
under `./lib/` and the file lands in `/usr/lib`.

The tree links with `-z pack-relative-relocs`, which writes relative relocations as `DT_RELR`,
a form musl loads from release 1.2.4 on (see [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)).
A binary from this tree does not run under an older C library.
