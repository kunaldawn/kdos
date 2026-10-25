## About

OpenCPN is a chart plotter and navigation program for boats: it draws S-57 and S-63 vector
charts and raster charts, takes positions from a GPS and targets from AIS over serial, USB or the
network, and plans and follows routes. The port builds it on [wxwidgets](#port-wxwidgets) with its
GTK 3 backend and OpenGL, and ships the minimal GSHHS world coastline, the tide and current
harmonics and the manual in the package, so it opens a usable chart of the world with no download.

## Flags

- `-DOCPN_USE_BUNDLED_LIBS=OFF` — Links the system copy of every library OpenCPN can take from
  either place, its default on Linux, named. The libraries with no system form, among them wxcurl
  and the parsers under `libs/`, are compiled from the tree.
- `-DOCPN_BUNDLE_GSHHS=ON`, `-DOCPN_BUNDLE_TCDATA=ON` — Install the minimal GSHHS coastline data
  and the tide and current harmonics; upstream's defaults, named.
- `-DOCPN_BUNDLE_DOCS=ON` — Installs the manual, which upstream leaves out by default on Linux.
- `-DOCPN_USE_GL=ON`, `-DOCPN_USE_CURL=ON`, `-DOCPN_ENABLE_PORTAUDIO=ON`,
  `-DOCPN_ENABLE_SNDFILE=ON`, `-DOCPN_USE_UDEV_PORTS=ON` — Upstream's defaults on Linux, named:
  OpenGL chart drawing, libcurl for downloads, alarm sounds through
  [portaudio](#port-portaudio) and [libsndfile](#port-libsndfile), and serial ports listed
  through [eudev](#port-eudev).
- `-DOCPN_USE_WEBVIEW=OFF` — Builds without wxWidgets' WebView. The About and help window then
  shows its pages in wxWidgets' own HTML widget instead of an embedded WebKitGTK view; nothing
  else in OpenCPN uses WebView.
- `-DOCPN_BUILD_TEST=OFF` — Leaves out the test subproject, which is on by default.
- `-DOCPN_CI_BUILD=OFF` — Upstream's default, named: the version is the release's own, not one
  made by the project's CI numbering rules.
- `-DCMAKE_IGNORE_PATH='/usr/lib/cmake/glew;/usr/lib64/cmake/glew;/lib/cmake/glew;/lib64/cmake/glew'` — Hides GLEW's own CMake package
  so that CMake's `FindGLEW` finds the library itself; taken from the package, a shared GLEW's
  path is left NOTFOUND (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `"-DOCPN_TARGET_TUPLE=kdos;1;$(uname -m)"` — Names the platform as `kdos`, version `1`, on this
  machine's architecture. The plugin manager matches downloadable plugins against this name.
  Left empty, `cmake/TargetSetup.cmake` runs `lsb_release` for it, and stops configure when the
  program is missing, as it is here.
- `-Wl,--strip-debug` — Drops debug information at the link. The bundled plugins' own CMake files
  (`chartdldr_pi`'s and `wmm_pi`'s `PluginConfigure.cmake`) add `-g` to their compile options, which `CMAKE_BUILD_TYPE=Release` does not remove (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).

## Patches

- `mdns.patch` — Adds `#include <sys/select.h>` to `model/src/mdns_service.cpp`, which calls `select()`; musl declares it only there.
- `system-shapelib.patch` — Removes the `#define USE_CPL` that `gui/include/gui/dychart.h` makes for every source. With it, the system [shapelib](#port-shapelib)'s `shapefil.h` includes GDAL's `cpl_conv.h` on top of the CPL headers OpenCPN carries in `libs/gdal`, and the two sets do not compile together.
- `openssl4.patch` — Builds the name of the self-signed certificate OpenCPN makes for its network server as a new `X509_NAME`, then sets it as both subject and issuer. The code wrote into the name `X509_get_subject_name()` returns, which OpenSSL 4 returns `const`.
- `udev-rule-destdir.patch` — Replaces the install step that copies `98-sglock-ocpn.rules` straight into the running system's `/usr/lib/udev/rules.d` when that directory is writable with an ordinary install into `/usr/lib/udev/rules.d` under the package. The rule makes the SGLock USB licence dongle, vendor `1547` product `1000`, readable and writable by every user. The `test` at the end of `build.sh` fails the build if the rule is not in the package.

## Notes

At start OpenCPN sets `GDK_BACKEND=x11` for itself whenever `WAYLAND_DISPLAY` is set, unless
`OCPN_DISABLE_X11_GDK_BACKEND` is, so on the session it is an X11 window under Xwayland, whose class is the program's name. The
desktop entry is written in `build.sh` with `StartupWMClass=opencpn` to match it.
