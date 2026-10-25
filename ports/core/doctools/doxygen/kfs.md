## About

Doxygen generates reference documentation from comments in C, C++ and other source code, as HTML,
LaTeX, man pages or XML. It is in `30_foundation` because a port of that phase needs it:
[libxml2](#port-libxml2) is configured `--with-python`, and its configure stops with "cannot find
doxygen" unless doxygen is installed, because the generator of the Python bindings reads the XML
description of the API that doxygen writes. Later, `pipewire`, `notmuch`, `brltty` and `srecord`
depend on it for their documentation.

Only the command-line `doxygen` program is built.

## Flags

- `-Dbuild_wizard=OFF` — Build no `doxywizard`, the Qt graphical front end for writing a Doxyfile.
  It is upstream's default, and Qt is not installed until much later in the build.
- `-Dbuild_search=OFF` — Build no `doxysearch` and `doxyindexer`, the server-side search tools for
  generated HTML. They need Xapian, whose port is built in `41_system`. Off is upstream's default.
- `-Duse_sys_sqlite3=ON` — Link the [sqlite](#port-sqlite) port's `libsqlite3` instead of compiling
  the copy of SQLite bundled in doxygen's source, which is why `sqlite` is in `depends`.
