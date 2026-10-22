## About

darktable is a raw photo developer and virtual lighttable: it keeps a catalogue of photographs and
develops each one non-destructively from the camera's raw data, with edits stored beside the
image. The port builds the GTK 3 program and its companions, `darktable-cli` for developing from
a shell, `darktable-chart` for building colour lookup tables from a chart, and
`darktable-rs-identify`. Raw decoding is the rawspeed library darktable carries in
`src/external/rawspeed`, with the tree's [libraw](#port-libraw) for the formats it hands on.

## Flags

- `-D BINARY_PACKAGE_BUILD=ON` — Passes `-mtune=generic` instead of probing for
  `-march=native`, so the code is built for the compiler's default target rather than the
  builder's processor. Bundled rawspeed reads the same switch for its own CPU settings. Without it
  the package carries instructions of the machine that built it and stops with an illegal
  instruction on an older processor.
- `-D RAWSPEED_ENABLE_WERROR=OFF` — Rawspeed turns every warning into an error by default; a
  warning this compiler adds would fail the build (see
  [An upstream `-Werror`](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-upstream--werror)).
- `-D RAWSPEED_ENABLE_LTO=OFF` — No link-time optimisation for rawspeed; upstream's default,
  named.
- `-D USE_CAMERA_SUPPORT=ON` — Tethered shooting and import straight from a camera through
  [libgphoto2](#port-libgphoto2).
- `-D USE_COLORD=OFF` — No display profile from colord, which is not a port.
- `-D USE_MAP=OFF` — No map view: it draws online tiles through osm-gps-map, which is not a port.
- `-D USE_AI=OFF` — No AI modules, which run on ONNX Runtime and download their models from a
  repository at run time.
- `-D USE_KWALLET=OFF`, `-D USE_LIBSECRET=OFF` — No password-store back end. darktable keeps only
  the credentials of its web export targets there, and both are on by default.
- `-D USE_UNITY=OFF` — No progress on a Unity launcher icon; upstream's default, named.
- `-D USE_LUA=ON`, `-D DONT_USE_INTERNAL_LUA=ON` — Lua scripting against the system
  [lua54](#port-lua54). darktable takes Lua 5.4 only, and with the second switch configure stops
  if it is missing rather than building the copy in the tarball. Both are upstream's defaults.
- `-D USE_OPENMP=ON`, `-D USE_OPENCL=ON` — Threads through OpenMP and GPU processing through
  OpenCL. The OpenCL loader is opened at run time, so neither needs anything more at build time.
- `-D TESTBUILD_OPENCL_PROGRAMS=OFF` — Does not test-compile the OpenCL kernels during the build,
  which needs LLVM and Clang; the kernels are compiled by the OpenCL driver when they run.
- `-D USE_GRAPHICSMAGICK=OFF`, `-D USE_IMAGEMAGICK=ON` — The fallback loader for formats
  darktable has no reader of its own for, such as FITS and GIF, is
  [imagemagick](#port-imagemagick) rather than GraphicsMagick, which is upstream's default.
- `-D USE_GMIC=OFF` — No G'MIC support in the LUT 3D module; G'MIC is not a port.
- `-D USE_LIBRAW=ON`, `-D DONT_USE_INTERNAL_LIBRAW=ON` — LibRaw for the raw formats darktable
  hands it, always Canon's CR3 and Sigma's X3F and any extension added to the `libraw_extensions`
  setting, from the system library rather than the copy in `src/external/LibRaw`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_libraw=ON` — Makes the system LibRaw required. Asked not to use
  the bundled copy, darktable still builds it, with only a status message, when the system one is
  not found (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D USE_XMLLINT=ON` — Validates the generated `darktableconfig.xml` with `xmllint --nonet` from
  [libxml2](#port-libxml2) during the build.
- `-D USE_PORTMIDI=ON`, `-D USE_SDL2=ON` — MIDI controllers through [portmidi](#port-portmidi)
  and gamepads through SDL2, as input devices for darktable's shortcuts.
- `-D USE_OPENJPEG=ON`, `-D USE_JXL=ON`, `-D USE_WEBP=ON`, `-D USE_AVIF=ON`, `-D USE_HEIF=ON`,
  `-D USE_XCF=ON`, `-D USE_ISOBMFF=ON`, `-D USE_OPENEXR=ON`, `-D USE_ICU=ON` — Upstream's
  defaults, named: JPEG 2000, JPEG XL, WebP, AVIF and HEIF, GIMP's XCF format, metadata of
  ISO base media files (CR3, AVIF, HEIF) through [exiv2](#port-exiv2) when it is built with that
  support, OpenEXR, and ICU.
- `-D BUILD_PRINT=ON` — The print view, through [cups](#port-cups); upstream's default, named.
- `-D BUILD_CMSTEST=OFF` — Leaves out `darktable-cmstest`, which reads display profiles through
  X11 and XRandR and has nothing to report on a Wayland session.
- `-D BUILD_RS_IDENTIFY=ON` — Builds `darktable-rs-identify`, which prints what rawspeed knows
  of a raw file; upstream's default, named.
- `-D VALIDATE_APPDATA_FILE=OFF` — Does not run `appstream-util` over the AppData file;
  upstream's default, named.
- `-Wl,--strip-debug` — darktable adds `-g` to `CMAKE_C_FLAGS` for every build type, after the
  tree's flags, so the debug information is dropped at the link (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).

## Patches

- `no-graphite.patch` — Removes `loop-nest-optimize`, `tree-loop-linear`, `loop-block` and `loop-strip-mine` from the `#pragma GCC optimize` lists in the blend, IOP-order, IOP-profile and channel-mixer sources and in `src/common/extra_optimizations.h`. They are Graphite passes, the tree's GCC is built without isl, and it rejects each one as unimplemented. A pragma takes precedence over the command line, so no flag can turn them off. In `channelmixerrgb.c` it also drops the blank continuation lines inside the `NOTEBOOK_PAGE` macro.

## Notes

The `find` after the install removes every directory under `/usr/share/man` that is not a
`man<section>` one, which takes out the translated manual pages; `kpkg`'s language rule removes
the same directories (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

darktable calls `g_set_prgname("org.darktable.darktable")`, which makes that its Wayland app_id.
Upstream's desktop entry names `StartupWMClass=darktable` and an absolute `Exec` path, so it is
replaced by one with `StartupWMClass=org.darktable.darktable` and a bare `Exec=darktable %U`.
The hicolor PNG icons it names are upstream's.
