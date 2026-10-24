## About

Celestia is a real-time 3D space simulator: it flies from the planets of the solar system out to
the stars and galaxies, at any date, and runs scripted tours. The source is upstream's git tree
at a fixed commit. The package installs two programs over one engine, `celestia-qt6` with the full
Qt 6 interface and `celestia-sdl`, a lighter SDL one with a Dear ImGui overlay. The catalogues,
textures and models it draws come from [celestia-content](#port-celestia-content).

## Flags

- `-D ENABLE_QT6=ON`, `-D ENABLE_SDL=ON` — Build both front ends; upstream builds neither unless
  asked.
- `-D ENABLE_CELX=ON` — CELX, Celestia's Lua scripting, used by `.celx` scripts and add-ons. On
  by default; named so it cannot drop out.
- `-D CMAKE_DISABLE_FIND_PACKAGE_LuaJIT=ON` — With CELX on, Celestia links LuaJIT whenever it
  finds it and the reference [lua](#port-lua) otherwise. `luajit` is a port too, so which one is
  linked would depend on whether it happened to be installed; this pins the build to `lua`, the one
  in `depends`.
- `-D ENABLE_FFMPEG=ON` — Video capture of the view, and video files as overlays, through
  [ffmpeg](#port-ffmpeg). Off upstream.
- `-D ENABLE_LIBAVIF=ON` — AVIF textures through [libavif](#port-libavif). Off upstream.
- `-D ENABLE_MINIAUDIO=ON` — Sound playback for scripts through miniaudio, a single-header
  library compiled in from `thirdparty/miniaudio`. Off upstream.
- `-D ENABLE_SPICE=ON` — Read spacecraft and body trajectories from NAIF SPICE kernels with the
  [cspice](#port-cspice) toolkit, which is then required. Off upstream.
- `-D ENABLE_NLS=OFF` — No gettext translation of the interface. kpkg removes every
  non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D ENABLE_TOOLS=OFF`, `-D ENABLE_TESTS=OFF` — Build neither the data-preparation tools nor
  the unit tests; both are off upstream.
- `-D ENABLE_GLES=OFF` — Render through desktop OpenGL 3.3 core rather than OpenGL ES 3.0;
  upstream's default.

## Notes

GitHub's archive of a commit carries no git submodules, and `thirdparty/imgui` and
`thirdparty/miniaudio` are submodules. The recipe fetches each at the commit in `_imgui` and
`_miniaudio`, and `build.sh` copies them into place before the configure. A bump of Celestia's
commit moves those two with it.

After the install, `build.sh`:

- removes `/usr/share/celestia/locale`, which holds only translations: the key lists
  (`controls_<lang>.txt`) and the start, demo and guide scripts (`*_<lang>.cel`). The system is
  English only;
- replaces the two DejaVu faces upstream bundles in `/usr/share/celestia/fonts` with links to the
  ones [ttf-dejavu](#port-ttf-dejavu) compiles, so the package carries no font it did not build
  (see [what is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source));
- scales `celestia-logo.png` to a 128-pixel icon in `hicolor/128x128/apps` with ImageMagick.
  Upstream installs the logo only as `/usr/share/pixmaps/celestia.png`, at its own size;
- installs `kdos-celestia.xml`, which defines `application/x-celestia-script` for `*.cel` and
  `*.celx`. Upstream's Qt entry claims that type, and without a definition in the shared MIME
  database the claim resolves to nothing (see
  [desktop entries](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port));
- writes both desktop entries in place of upstream's, under upstream's file names, with
  `TryExec`, `StartupWMClass` and search keywords. The Qt one is named plain `Celestia` and the
  SDL one `Celestia (SDL)`.
