## About

Massif Visualizer reads the heap profiles Valgrind's Massif tool writes (`massif.out.<pid>`) and
draws memory use over time, with the peak and each detailed snapshot marked, and the allocation
tree behind any snapshot. A call graph of a snapshot shows which call paths hold the memory.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KGraphViewerPart=ON` — Make [kgraphviewer](#port-kgraphviewer)'s
  part a requirement. Upstream finds it as recommended and builds the call graph tab only when it
  is found. Depending on it makes kgraphviewer a build dependency of another port, so kgraphviewer
  is listed in `43_toolkits` rather than `44_apps`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_SharedMimeInfo=ON` — Make
  [shared-mime-info](#port-shared-mime-info) a requirement. The `application/x-valgrind-massif` type,
  by which a file manager opens a profile in this program, is installed only when
  `update-mime-database` is found.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

The desktop entry's `StartupWMClass` is upstream's `massif-visualizer`, but the window's Wayland
app_id is `org.kde.massif-visualizer`. The program passes `massif-visualizer@kde.org`, an
address with no URL scheme, where KAboutData takes the home page; KAboutData then falls back to
the `kde.org` domain and builds the desktop file name from it and the component name.
`app-id.patch` sets the class to the app_id so the task manager matches the window to its entry.

## Notes

The charts are KChart, from [kdiagram](#port-kdiagram), required at 3.0 or later. With `DESTDIR`
set, the install copies the MIME type's XML and leaves the database to kpkg, which regenerates it
when the package is installed (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). Profiles come
from [valgrind](#port-valgrind), in `depends` for run time.
