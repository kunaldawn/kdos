## About
[ImHex]{.idx} is a hex editor for reverse engineering. Beside the hex view it has a pattern
language that decodes a file's structures into a tree, a disassembler on Capstone, data
inspectors, diffing, hashing, YARA rules and decompression of archive formats. It draws its own
interface with Dear ImGui in a GLFW window. The port also installs the ImHex-Patterns library,
the patterns, magic files, encodings and constants the editor ships with, from its second source.

## Flags
- `-DFMT_DEPRECATED_HEAVY_CORE` — Added to `CXXFLAGS`. The pattern language calls
  `fmt::format` with only `<fmt/core.h>` included. From fmt 12 on, `core.h` includes
  `format.h` only when `FMT_DEPRECATED_HEAVY_CORE` is defined, and [fmt](#port-fmt) is the system
  copy (`USE_SYSTEM_FMT=ON`).
- `-DCMAKE_INSTALL_RPATH='$ORIGIN'` — Give every installed library and plugin an `$ORIGIN` run
  path. `ui.hexpluglib` needs `fonts.hexpluglib`, and the builtin, diffing and disassembler plugins
  need `ui.hexpluglib`, each by bare name, all in `/usr/lib/imhex/plugins`. ImHex loads the two
  libraries first by full path, and musl's loader does not reuse a library loaded by path to
  satisfy a later bare-name dependency, so each plugin finds its sibling through the run path.
  Without it the builtin plugin does not load, no subcommand exists, and every command-line run
  aborts on an empty `std::function`.
- `-DIMHEX_OFFLINE_BUILD=ON` — Download nothing at build time. Without it the install step
  `git clone`s ImHex-Patterns; with it, the build installs the patterns from a directory named
  `ImHex-Patterns` inside the source or beside it, which is what the `mv` before the configure
  makes.
- `-DIMHEX_IGNORE_BAD_CLONE=ON` — Skip the check that stops the configure when a directory
  under `lib/external` or `lib/third_party` is empty, the sign of a git clone without its
  submodules.
- `-DIMHEX_USE_GTK_FILE_PICKER=OFF` — Open files through the FileChooser portal
  (`NFD_PORTAL`), not a GTK dialog. It is upstream's default; on KDOS the portal is
  [xdg-desktop-portal-kdos](../../../../docs/kdos/03-architecture/session.md).
- `-DIMHEX_STRICT_WARNINGS=OFF` — Do not turn on ImHex's warnings-as-errors set, which is on by
  default.
- `-DIMHEX_STRIP_RELEASE=OFF`, `-DIMHEX_COMPRESS_DEBUG_INFO=OFF` — Leave stripping to kpkg (the
  release build would otherwise link with `-s`) and do not compress debug sections.
- `-DIMHEX_DISABLE_STACKTRACE=ON` — Build without stack traces in crash reports. ImHex takes
  them from `<stacktrace>` with `libstdc++exp` when a test program using it links and runs, and
  otherwise from libbacktrace or `execinfo.h` when CMake finds one.
- `-DIMHEX_BUNDLE_DOTNET=OFF`, `-DCMAKE_DISABLE_FIND_PACKAGE_CoreClrEmbed=ON` — Build without the
  .NET scripting runtime and keep CMake from searching for it. The tree carries no .NET.
- `-DIMHEX_BUNDLE_PLUGIN_SDK=OFF` — Do not install the plugin SDK; `build.sh` also removes any
  `share/imhex/sdk` the install leaves.
- `-DIMHEX_ENABLE_UNIT_TESTS=OFF`, `-DIMHEX_ENABLE_PLUGIN_TESTS=OFF` — Build no tests.
- `-DUSE_SYSTEM_CAPSTONE=ON`, `-DUSE_SYSTEM_NLOHMANN_JSON=ON`, `-DUSE_SYSTEM_FMT=ON`,
  `-DUSE_SYSTEM_YARA=ON` — Link [capstone](#port-capstone), [nlohmann-json](#port-nlohmann-json),
  [fmt](#port-fmt) and [yara](#port-yara) from the tree instead of the copies under
  `lib/third_party`.
- `-DUSE_SYSTEM_LLVM=OFF` — Build the bundled `llvm-demangle` instead of looking for LLVM's
  Demangle component.
- `-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON` — Keep the ImGui backend from finding and linking
  libX11. The window comes from [GLFW](#port-glfw), and ImHex itself calls no Xlib.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_BZip2=ON`,
  `-DCMAKE_REQUIRE_FIND_PACKAGE_LibLZMA=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_ZSTD=ON`,
  `-DCMAKE_REQUIRE_FIND_PACKAGE_LZ4=ON` — Make each compression library of the decompress plugin
  a hard requirement, so a missing one stops the configure instead of silently dropping that
  format.

## Patches
- `fix-lfs64.patch` — Calls `fopen` and `lseek` where miniaudio and the disk provider call `fopen64` and `lseek64` on Linux. musl has no `*64` variants; its `off_t` is 64-bit.
- `no-update.patch` — Drops the `updater` subdirectory from `main/CMakeLists.txt`. The updater is a separate program that downloads release builds; its header notes it is only available on aarch64 and x86_64.
- `no-werror.patch` — Removes `-Werror` from the pattern language library's compile options.
- `no-network.patch` — Removes the startup task that checks for updates and posts telemetry, and the background task that uploads the crash log after a crash, so the program does not contact its server on its own.

## Notes
- **The pattern library must be renamed.** The second source unpacks as
  `ImHex-Patterns-ImHex-v<version>` beside the source; `build.sh` renames it to `ImHex-Patterns`,
  the sibling directory the offline build looks for. Without it the package carries no patterns,
  magic or encodings, and the build does not say so.
- **The menu icon is a PNG.** `build.sh` rasterises upstream's `resources/icon.svg` to a 256-pixel
  `hicolor` PNG with `rsvg-convert`, because the session's icon lookup reads no SVG; see
  [Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).
