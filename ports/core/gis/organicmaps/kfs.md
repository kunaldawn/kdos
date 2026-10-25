## About

Organic Maps is an offline map application built on OpenStreetMap data, with search and
turn-by-turn routing for walking, cycling and driving. The port builds its Qt 6 desktop program,
`OMaps`, and installs it with the data directory it reads, which carries the whole world at low
zoom (`World.mwm` and `WorldCoasts.mwm`). Detailed regions are `.mwm` files of the same data
version: the program's own downloader fetches them over the network, or they are placed by hand
in its writable data directory under `~/.local/share` for a machine that stays offline.

## Flags

- `-DWITH_SYSTEM_PROVIDED_3PARTY=ON` — Builds against the tree's libraries rather than the copies
  under `3party/`: [boost](#port-boost), [expat](#port-expat), [gflags](#port-gflags),
  [jansson](#port-jansson), [pugixml](#port-pugixml), [utfcpp](#port-utfcpp),
  [glm](#port-glm), [fast-double-parser](#port-fast-double-parser), ICU, FreeType and HarfBuzz.
  The three it has no system form for here are later sources of the recipe (see Notes).
- `-DBUILD_SHARED_LIBS=OFF` — Links the project's internal libraries into `OMaps` as static
  archives, so the package carries one program and no private shared libraries.
- `-DBUILD_DESIGNER=OFF` — Upstream's default: the ordinary desktop application, not the
  map-style designer variant, which links the map generator.
- `-DBUILD_STANDALONE=ON` — The switch upstream's `tools/unix/build_omim.sh` sets for a standalone
  desktop build. Its one effect beyond a status message is the `macdeployqt` step that
  `no-macdeployqt.patch` removes, so with the patch applied it does not change what is built.
- `-DSKIP_TESTS=ON` — Leaves out the unit tests, which need the googletest submodule the release
  archive carries empty.
- `-DSKIP_TOOLS=ON` — Leaves out the map generator's tools and the developer tools, none of which
  is installed. With the tests gone, some of them would not link, as they link the test support
  libraries.
- `-DSKIP_QT_GUI=OFF`, `-DPYBINDINGS=OFF`, `-DUSE_PCH=OFF` — Upstream's defaults, named: the Qt
  program is built, the Python bindings are not, and no precompiled headers are used.
- `-DUSE_CCACHE=OFF` — Do not compile through ccache, which upstream turns on by default.
- `-DCMAKE_EXE_LINKER_FLAGS_RELEASE="$LDFLAGS"` — Puts the tree's link flags back on every
  program. On Linux, when it finds `ld.lld` (or else `ld.gold`), upstream sets
  `CMAKE_EXE_LINKER_FLAGS` to `-fuse-ld=lld` (or `-fuse-ld=gold`) outright, which replaces the
  value CMake took from `LDFLAGS`; the per-configuration variable is added after it in a Release
  build.

## Patches

- `libs-hack.patch` — Stops `3party/CMakeLists.txt` from adding the bundled glaze, and, when system libraries are in use, the bundled glfw and imgui, which only the developer sandbox links; and drops the glaze test from the platform tests, with its link to `glaze::glaze`.
- `no-dev-sandbox.patch` — Removes `dev_sandbox`, the developer's rendering sandbox, from the desktop build.
- `no-macdeployqt.patch` — Removes the post-build step that runs `macdeployqt` on the program for a standalone build. That step bundles Qt into a macOS application and fails anywhere else.
- `use-internal-protobuf.patch` — Moves the `add_subdirectory(protobuf)` line out of the block that runs only without system libraries, so the project's own protobuf fork is built in both cases. The system-library branch looks for no protobuf at all, and without the fork the `protobuf` target that `libs/indexer` links would not exist.
- `version-from-environment.patch` — Makes `tools/unix/version.sh` take the version's date and commit count from `OM_VERSION_DATE` and `OM_VERSION_COUNT` when the first is set. A release archive is not a git work tree, and the script would otherwise make the build date the version the program reports. `build.sh` sets both from the recipe's `_tag`, which has the form `<date>-<count>-linux`.
- `skip-search-test-support.patch` — Builds `search_tests_support` and `search_quality` only when the tools or the tests are built. With both left out, those libraries link test libraries that are never defined.

## Notes

The release archive carries its git submodules as empty directories. Three of them have no
system form in the tree and are sources of the recipe at the commits the tag pins: the project's
own protobuf fork, `just_gtfs` and `fast_obj`. `build.sh` copies each into its place under
`3party/` before configure (see
[An empty submodule in a release archive](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-empty-submodule-in-a-release-archive)). A version bump has to take the new commit ids from the tag's
submodule references into `_protobuf`, `_gtfs` and `_fastobj`.

The install copies the whole `data/` directory, test data included; `data/test_data` is removed
from the package. The `[ -f … ]` check after it fails the build if the 512-pixel hicolor icon
the desktop entry names is not installed.

The program calls `setDesktopFileName("app.organicmaps.desktop")`, which makes that its Wayland
app_id. Upstream's entry is replaced by one carrying the same name as `StartupWMClass`, so the
panel ties the window to its launcher.
