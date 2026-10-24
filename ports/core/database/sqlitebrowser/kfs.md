## About
[DB Browser for SQLite]{.idx} (`sqlitebrowser`) is a graphical editor for SQLite database files:
it creates and alters tables and indexes, browses and edits rows, runs SQL with a highlighting
editor, plots query results and imports and exports CSV and SQL. It is a Qt 5 application built
against the system [sqlite](#port-sqlite).

## Flags
- `-DBUILD_STABLE_VERSION=ON` — build as a release. Upstream's default is its nightly build, which
  compiles the day of the build into the version string (`BUILD_VERSION`), so two builds of one
  recipe would differ.
- `-DENABLE_TESTING=OFF` — no unit tests; upstream's default, named.
- `-DFORCE_INTERNAL_QSCINTILLA=ON` — compile the copy of QScintilla under `libs/qscintilla`. The
  tree's [qscintilla](#port-qscintilla) port is built for Qt 6, and this program links Qt 5, so it
  cannot use it. Upstream's default prefers a system copy.
- `-DFORCE_INTERNAL_QCUSTOMPLOT=ON` — compile the bundled QCustomPlot, the plotting widget, and with
  `-DFORCE_INTERNAL_QHEXEDIT=ON` the bundled QHexEdit, the hex editor for binary cells. Neither is a
  port; both are upstream's defaults, named.
- `-Dsqlcipher=OFF` — link plain SQLite rather than SQLCipher, the encrypting fork, which the tree
  does not carry. It is upstream's default, so encrypted databases cannot be opened.

## Notes
`build.sh` installs the MIME type for DB4S project files (`.sqbpro`) from `distri/mime`, which the
CMake install leaves out, writes its own `sqlitebrowser.desktop` over upstream's, associating the
program with SQLite files and project files, and checks that the program and its 256-pixel icon were installed.
