## About
ixion is a threaded spreadsheet formula engine: it parses formulas, tracks dependencies between
cells and recalculates them. It is the calculation half of [liborcus](#port-liborcus), the
spreadsheet import library, and [labplot](#port-labplot) uses it too. The package installs the
library, the `ixion-parser`, `ixion-formula-tokenizer` and `ixion-sorter` tools, and a Python
module.

## Flags
- `--with-boost=/usr` — Where boost.m4 looks for [boost](#port-boost)'s headers and libraries.
- `boost_cv_rpath_link_ldflag=-L` — Answers boost.m4's check for a run-path linker flag in
  advance. boost.m4 otherwise adds `-Wl,-R/usr/lib` to every link against a Boost library,
  which writes `/usr/lib` into the library's `RPATH`; with `-L` the flag becomes a second
  `-L/usr/lib` and no `RPATH` is written. The
  [package lint](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
  warns on a `/usr/lib` entry. [liborcus](#port-liborcus) passes the same answer.
- `--enable-threads` — Calculate formula cells on several threads. It is configure's default.
- `--enable-python` — Build the `ixion` Python module, which installs into site-packages beside
  the library; the default, and the reason [python3](#port-python3) is in `depends`.
- `--disable-vulkan` — Build no Vulkan compute engine. It is configure's default.
- `--disable-debug-utils` — Build without the extra checks meant for development; also the
  default.

## Patches
- `boost-1.89.patch` — In `m4/boost.m4`, look only for the header `boost/system/error_code.hpp` when Boost is 1.70 or newer, instead of linking a `boost_system` library: Boost.System is header-only there, and Boost 1.89 installs no library for configure to find.

## Notes
The patch changes an m4 macro, so `build.sh` runs `autoreconf -fi` after it to regenerate
`configure`; that is why `autoconf`, `automake` and `libtool` are in `depends`.
