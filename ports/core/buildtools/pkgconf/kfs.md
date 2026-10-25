## About
pkgconf is the tree's [pkg-config]{.idx}: it reads the `.pc` files libraries install and prints
the compiler and linker flags a program needs to use them. A configure script reaches it through
`PKG_CHECK_MODULES`, and Meson and CMake call it to find dependencies, so most ports that link a
library run it, whether or not the recipe names pkgconf in `depends`; that is why it is in the
opening run of `30_foundation`. The package lint runs it too, resolving every `.pc`
file a package ships (see
[The package lint](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-package-lint)).
It is built with [muon](#port-muon).

## Flags
- `-Ddefault_library=both` — Build `libpkgconf` as a static archive as well as a shared library;
  Meson's default is shared only.
- `-Dwith-pkg-config-dir` — The search path compiled in for `.pc` files when
  `PKG_CONFIG_LIBDIR` is unset: `/usr/lib/pkgconfig` and `/usr/share/pkgconfig`. Upstream derives
  the same two from the prefix, `libdir` and `datadir`; naming them keeps the path independent of
  the `libdir` muon picks. The compiled-in path names nothing under `/usr/local`.
- `-Dwith-system-libdir` — The library directories whose `-L` pkgconf leaves out of `--libs`
  output, because the linker searches them anyway: `/lib` and `/usr/lib`, the two names of the one
  directory on KDOS's merged layout. Upstream's default is `{prefix}/{libdir}` alone.
- `-Dwith-system-includedir` — The header directory whose `-I` pkgconf leaves out of `--cflags`
  output, `/usr/include`.

## Patches
- `meson-version.patch` — Sets the project version in `meson.build` to the release's own. The released source declares the version before it, and pkgconf reports `meson.project_version()` as its own, in `pkgconf --version` and to every configure script that checks for a minimum pkg-config version.

## Notes
Upstream installs no program named `pkg-config`; the two `ln` lines add `/usr/bin/pkg-config`, a
link to `pkgconf`, and its manual page, because configure scripts and build systems run
`pkg-config` by that name.
