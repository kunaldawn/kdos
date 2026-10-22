## About

[GnuCash]{.idx} is a double-entry accounting program for personal and small-business books:
accounts, transactions, scheduled entries, invoices, customers and vendors, and reports. Its
engine is a C and C++ library with Scheme ([guile](#port-guile)) on top, the interface is GTK 3,
and the reports are HTML drawn by [webkitgtk](#port-webkitgtk) through its 4.1 API. A book here is
kept as an XML file.

## Flags

- `CFLAGS="$CFLAGS -Wno-error"` — with the same for `CXXFLAGS`. GnuCash's `CMakeLists.txt` puts
  `-Werror` in front of the compiler flags on every Unix build, so a warning that a newer compiler
  adds stops the build. `-Wno-error` comes later on the command line and wins.
- `-D WITH_GNUCASH=ON`, `-D BUILD_SHARED_LIBS=ON` — Build the whole program, not only the engine
  library, with shared libraries. Both are upstream's settings, written out.
- `-D WITH_SQL=OFF` — No SQLite, MySQL or PostgreSQL backends for the book. They go through libdbi,
  which is not a port, so books are XML files.
- `-D WITH_AQBANKING=OFF` — No AqBanking, which provides online banking (HBCI/FinTS) and several
  file import formats. AqBanking and its Gwenhywfar library are not ports.
- `-D WITH_OFX=ON` — OFX statement import through [libofx](#port-libofx). QIF and CSV import are
  built in regardless.
- `-D WITH_PYTHON=OFF` — No Python bindings or Python plugin; upstream's default.
- `-D DISABLE_NLS=ON` — No translations. The packager removes every non-English catalogue in any
  case (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.
- `-D COMPILE_GSCHEMAS=OFF` — Do not run `glib-compile-schemas` at install time. The GSettings
  schema cache is shared by every package, and kpkg rebuilds it after an install as one of its
  [shared indexes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#shared-indexes).

## Patches

- `install-rpath.patch` — drops `${CMAKE_INSTALL_FULL_LIBDIR}` from the run path GnuCash gives its installed programs and libraries, keeping only `/usr/lib/gnucash`, where its own modules are. The loader searches `/usr/lib` anyway, and the [package lint](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-package-lint) reports a plain `/usr/lib` run-path entry.

## Notes

The CMake configuration stops with `GTEST not found` unless GoogleTest ([gtest](#port-gtest)) is
installed, whether or not the tests are run, so it is a dependency.

`gnucash-valgrind`, a script that starts GnuCash under Valgrind, is installed by an
`install(FILES ...)` rule, which gives it mode 644; `build.sh` makes it executable.
