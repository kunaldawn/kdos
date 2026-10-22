## About

Qt Creator is the Qt project's IDE for C++, QML and Python: an editor with a clangd code model,
CMake and qmake project support, a debugger front end for [gdb](#port-gdb), the Clang-Tidy and
Clazy runners, and a perf profiler. In KDOS its code model uses the [clang](#port-clang) port's
libraries at build time and its `clangd` at run time. The help viewer shows installed Qt
documentation (`.qch` files) through the bundled litehtml renderer, so the port needs no
QtWebEngine.

## Flags

- `-DBUILD_WITH_PCH=OFF` — Compile without precompiled headers, which are on by default.
- `-DWITH_TESTS=OFF`, `-DWITH_DOCS=OFF` — No test plugins and no generated documentation; both are
  upstream's defaults.
- `-DBUILD_QBS=OFF` — Do not build the Qbs build tool from the copy in `src/shared/qbs`. It is
  upstream's default; Qbs is a separate product.
- `-DWITH_QMLDESIGNER=OFF` — Leave out the QML Designer, the visual Qt Quick editor. Upstream turns
  it on by default whenever Qt Quick and a new enough compiler are present.
- `-DBUILD_DESIGNSTUDIO=OFF` — No Qt Design Studio plugins, examples or settings. It is upstream's
  default.
- `-DQTC_USE_INTERNAL_TASKTREE=ON` — Use Qt Creator's own copy of the TaskTree library. Upstream
  chooses it whenever Qt has no TaskTree module, as the Qt ports here do not; naming it keeps the
  choice the same on every build.
- `-DQTC_CLANG_BUILDMODE_MATCH=ON` — Declare that the clang libraries match this build's
  release mode. `cmake/FindClang.cmake` tests that only on Windows and on Linux sets it on
  whenever `clangTooling` is found; the one thing it gates is the ClangFormat plugin, which is off.
- `-DBUILD_EXECUTABLE_CMDBRIDGE=OFF` — Do not build `cmdbridge`, the helper for fast container and
  remote device access. It is written in Go and built with `go build` once for every remote
  platform and architecture.
- `-DBUILD_PLUGIN_CLANGFORMAT=OFF` — Leave out the ClangFormat plugin, which formats code with
  clang's `libclangFormat` linked into Qt Creator.
- `-DBUILD_PLUGIN_COPILOT=OFF` — This switch and `-DBUILD_PLUGIN_COMPILEREXPLORER=OFF`,
  `-DBUILD_PLUGIN_GITLAB=OFF`, `-DBUILD_PLUGIN_AXIVION=OFF`, `-DBUILD_PLUGIN_EXTENSIONMANAGER=OFF`,
  `-DBUILD_PLUGIN_LEARNING=OFF` and `-DBUILD_PLUGIN_UPDATEINFO=OFF` leave out the plugins that are
  front ends to online services: GitHub Copilot, Compiler Explorer, GitLab, the Axivion
  dashboard, the extension store, the Learning page and the update checker. Upstream builds every
  plugin whose own conditions are met unless its `BUILD_PLUGIN_<NAME>` switch is off.
- `-DBUILD_HELPVIEWERBACKEND_QTWEBENGINE=OFF` — Do not build the QtWebEngine help viewer, which
  upstream builds by default; [qt6-qtwebengine](#port-qt6-qtwebengine) is not in `depends`.
- `-DHELPVIEWER_DEFAULT_BACKEND=litehtml` — Show help pages with litehtml. It is upstream's
  default.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_Clang=ON` — This switch and the `CMAKE_REQUIRE_FIND_PACKAGE_`
  switches for `LibArchive`, `yaml-cpp`, `elfutils` and `Zstd` turn each of those optional
  `find_package` calls into a configure error when the package is missing. Without them a miss is
  quiet: libarchive and yaml-cpp fall back to the copies in `src/libs/3rdparty`, the perf parser
  is left out with only a status line when elfutils is missing, and without zstd it reads no
  compressed perf data.
- `-DCMAKE_DISABLE_FIND_PACKAGE_LibRustcDemangle=ON`, `-DCMAKE_DISABLE_FIND_PACKAGE_LibDDemangle=ON`
  — The perf parser can demangle Rust and D symbols through these two libraries. Neither is a
  port, so neither is looked for.

## Notes

`no-execinfo.h.patch` and `no-malloc_trim.patch` are Alpine's musl patches. musl has no
`execinfo.h`, so the backtrace on a failed assertion is compiled out, and no `malloc_trim`, so the
call that returns freed memory to the system a minute after a key or mouse press is removed.

Code Paster, a front end to paste services, stays built: the version-control plugin names it as a
recommended plugin, and configure stops if its target is missing.

Upstream's menu entry is replaced. The program sets its desktop file name, and so its Wayland
app_id, to `org.qt-project.qtcreator`, which the entry gives as `StartupWMClass`. Other editors
here claim the C and C++ source types too, so the entry claims no types and `mimeapps.list`
chooses.
