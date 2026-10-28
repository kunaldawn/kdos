## About
[RKWard]{.idx} is a KDE front end to the [R](#port-R) statistics language: a console, a script
editor, a spreadsheet-like data editor, menu-driven dialogs for common analyses, and an output
window for results, plots and R Markdown previews. R itself runs in a separate backend process,
`rkward.rbackend`, which the window talks to.

## Flags
- `-D FORCE_WITH_QWEBENGINE=ON` — build the HTML renderer of the output, help and preview windows
  on QtWebEngine alone and make Qt6WebEngineWidgets a required package, so a missing QtWebEngine
  stops the configure at the package check. Without the switch upstream builds whichever of the
  QtWebEngine and QtWebView renderers it finds, and stops only when it finds neither.
- `-D FORCE_WITH_QWEBVIEW=OFF` — no QtWebView renderer; QtWebView is not a port. It is upstream's
  default, named.
- `-D DLOPEN_RLIB=ON` — the backend opens `libR.so` with `dlopen()` at run time rather than
  linking it, so it follows the R that is installed. It is upstream's default, named.
- `-D R_EXECUTABLE=/usr/bin/R` — the R the configure runs to find `R_HOME`, R's include directory
  and its library, and the path compiled into RKWard to start R with. Upstream otherwise searches
  the `PATH`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KDSingleApplication-qt6=ON` — stop the configure if
  [kdsingleapplication](#port-kdsingleapplication) is missing, where upstream falls back to the
  copy under `3rdparty/` and links it statically.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON` — make KCrash, an optional component upstream,
  required, so the crash handler is always linked.

## Notes
The build packs RKWard's own R packages, `rkward` and `rkwardtests`, into tarballs under
`/usr/share/rkward/rpackages`. On start RKWard copies them into `.rkward_packages` under its files
directory (`~/.rkward` unless the settings name another), and the backend installs them with
`install.packages()` into a library of its own there, `.rkward_packages/<R version>`, whenever
loading `rkward` from it fails or finds a different RKWard version: on first start and after an
upgrade. `build.sh` fails if the `rkward.tgz` tarball or the `rkward` program is missing from the
package.

The desktop entry is KDOS's own, written over upstream's to add `StartupWMClass=org.kde.rkward`,
the Wayland app id KAboutData sets (see
[`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)). Every MIME type it
claims is defined by the XML files the port installs.
