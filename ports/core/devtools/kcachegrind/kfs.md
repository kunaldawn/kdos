## About

KCachegrind visualises the profiles Valgrind's Callgrind and Cachegrind tools write: costs per
function, callers and callees, a call graph, a treemap, and source and assembly annotated with the
cost of each line. It reads any file in the Callgrind format, so the converters it installs open
OProfile, Perl and memprof profiles too.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Make [kdoctools](#port-kdoctools) a
  requirement. Upstream finds it as optional and drops the handbook without one.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translations in `po/` and `poqm/`, which
  kpkg's English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  With it, ECM's `ecm_install_po_files_as_qm()` skips every `poqm/` catalogue before it looks for
  Qt's `lconvert`, so qt6-qttools is not a build dependency.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

The desktop entry's `StartupWMClass` is upstream's `kcachegrind`, but the program sets its desktop
file name, and with it the window's Wayland app_id, to `org.kde.kcachegrind`; `app-id.patch` sets
the class to the app_id so the task manager matches the window to its entry.

## Notes

`build.sh` removes two of the converters the install puts in `/usr/bin`. `hotshot2calltree` is
Python 2 (a `print` statement and Python 2's `hotshot` module), so no Python here can run it, and
`pprof2calltree` is PHP, which is not a port. The three that stay, `op2calltree`,
`dprof2calltree` and `memprof2calltree`, are Perl, so [perl](#port-perl) is in `depends` for run
time.

The call graph view runs `dot` from [graphviz](#port-graphviz), and the profiles come from
[valgrind](#port-valgrind); both are in `depends` for run time only. `qcachegrind`, the Qt-only
build of the same viewer, is compiled with the rest and not installed.
