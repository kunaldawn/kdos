## About

Stellarium is a planetarium: it draws the sky as it looks from any place on Earth at any time,
to the eye, through binoculars or through a telescope, with its stars, planets, deep-sky objects
and the sky cultures of many peoples. It is a Qt 6 program with OpenGL rendering and a set of
plugins built into it, among them telescope control, satellites, oculars and a lens distortion
estimator.

## Flags

- `-D STELLARIUM_RELEASE_BUILD=1` — Build as an official release: the version is the public one,
  with no patch number taken from the day of the year and no branch and commit asked of git, which
  has no repository to read in an unpacked tarball. It is upstream's default, named so it cannot
  change.
- `-D ENABLE_CCACHE=0` — Do not wrap the compiler in ccache when it is found; upstream's default
  is to use it.
- `-D ENABLE_PODIR=0` — Do not compile the translation files under `po/`. kpkg removes every
  non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D ENABLE_SHOWMYSKY=0` — No ShowMySky atmosphere model. Upstream's default is on, and it then
  looks for the library and, failing that, has CPM fetch and build it; ShowMySky is not a port.
- `-D ENABLE_QTWEBENGINE=0`, `-D USE_PLUGIN_ONLINEQUERIES=0` — Build without Qt WebEngine and
  without the Online Queries plugin, whose web view uses it when it is found. `qt6-qtwebengine`
  is not in `depends`, so neither is built against it.
- `-D ENABLE_GPS=1`, `-D CMAKE_REQUIRE_FIND_PACKAGE_GPS=ON` — Location from a GPS receiver. On
  Linux Stellarium queries [gpsd](#port-gpsd) through `libgps` when it finds it and otherwise
  reads NMEA from a serial device; requiring the package stops the configure when `libgps` is
  missing instead of quietly building the serial fallback alone.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_exiv2=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_NLopt=ON` — The Lens
  Distortion Estimator plugin reads photographs' metadata with [exiv2](#port-exiv2) and fits its
  model with [nlopt](#port-nlopt). It looks for exiv2 as optional, and for NLopt through CPM,
  which would fetch NLopt from GitHub when the system has none. Required, both must be the
  system's.
- `-D ENABLE_MEDIA=1`, `-D ENABLE_SPEECH=1` — Video and sound through Qt Multimedia, and speech
  output through Qt TextToSpeech. Both are on by default; named so they cannot drop out.
- `-D ENABLE_XLSX=1`, `-D CPM_QXlsxQt6_SOURCE="$SRC_ROOT/QXlsx-$_qxlsx"` — Export tables from the
  Astronomical Calculations window and the Satellites plugin to Excel files with QXlsx. QXlsx is not a port, and
  Stellarium builds it in through CPM; the `CPM_<name>_SOURCE` variable makes CPM use the copy
  this recipe fetched instead of downloading one.
- `-D ENABLE_INDI=1`, `-D PREFER_SYSTEM_INDILIB=0`, `-D CPM_indiclient_SOURCE="$SRC_ROOT/indi-$_indi"`
  — Telescope Control can drive a mount through an INDI server. INDI is not a port, so
  Stellarium builds the INDI client library from the INDI source this recipe fetched. With
  `PREFER_SYSTEM_INDILIB=0` it never links a `libindiclient` it happens to find instead.
- `-D CPM_md4c_SOURCE="$SRC_ROOT/md4c-release-$_md4c"` — md4c, the Markdown parser
  Stellarium turns the sky cultures' descriptions into HTML with, is not a port either, and is
  built in from the fetched source.
- `-D FETCHCONTENT_FULLY_DISCONNECTED=ON` — CPM is built on CMake's FetchContent; disconnected,
  FetchContent downloads and updates nothing and uses the source directories it is given, so the
  configure never reaches the network.
- `-D ENABLE_TESTING=0` — No unit tests; upstream's default.

## Notes

The three extra sources are the versions Stellarium's own CMake names in its `CPMAddPackage` and
`CPMFindPackage` calls, with the same SHA-256 sums, so a Stellarium bump has to move `_qxlsx`,
`_indi` and `_md4c` to whatever the new release names. kpkg unpacks a later tarball into
`$SRC_ROOT` by itself, which is where md4c comes from; a zip is not a tarball and is only copied
into `$SRC`, so `build.sh` unpacks the QXlsx and INDI zips into `$SRC_ROOT` with `cmake -E tar`.

After the install, four of the faces upstream ships in `/usr/share/stellarium/data` (Noto Sans,
Noto Sans Mono, DejaVu Sans and DejaVu Sans Mono) are replaced with links to the same faces from
[noto-fonts](#port-noto-fonts) and [ttf-dejavu](#port-ttf-dejavu). The fifth,
`NotoSansSC-Regular.otf`, ships as upstream built it (see
[what is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)). Upstream's
`org.stellarium.Stellarium.desktop` is replaced by `org.stellarium.stellarium.desktop`, with
`StartupWMClass=org.stellarium.stellarium`, `TryExec` and search keywords. The file name and the
class are `org.stellarium.stellarium` because that is the Wayland app id Qt gives the window: the
organisation domain `stellarium.org` reversed, then the executable's name. The entry keeps
upstream's `Exec=stellarium --startup-script=%f` and drops its desktop actions.
