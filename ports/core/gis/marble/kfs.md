## About

Marble is KDE's virtual globe and world atlas: the Earth, the Moon and other bodies in Atlas,
satellite and OpenStreetMap views, with search, routing and position tracking. The port builds the
KDE application `marble` and two programs beside it that share its library: `marble-behaim`, the
Behaim globe of 1492, and `marble-maps`, a Kirigami map-and-navigation front end.

## Flags

- `-DKDE_INSTALL_USE_QT_SYS_PATHS=ON`, `-DKDE_INSTALL_LIBDIR=lib` — The install layout every KDE
  application here shares (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-DKF_SKIP_PO_PROCESSING=ON` — Makes `ki18n_install` return at once, so the `.mo` catalogs in
  `po/` are not built. It does not reach Marble's Qt catalogs in `poqm/` or the translated
  handbooks `kdoctools_install` builds from `po/`; both are built, and kpkg's language rule keeps
  them out of the package, as the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-DBUILD_QT_AND_KDE=OFF` — Builds one main program, not both a plain Qt one and a KDE one. With
  [kparts](#port-kparts) found, the one built is the KDE application, `marble-kde`, installed as
  `marble`.
- `-DBUILD_MARBLE_APPS=ON` — Builds the applications, upstream's default. Behaim and Marble Maps
  are built with it whenever KConfig, KI18n and KCoreAddons are found; they need
  [kirigami](#port-kirigami) at run time.
- `-DMOBILE=OFF`, `-DBUILD_TOUCH=OFF` — Upstream's defaults, named. `MOBILE` replaces the desktop
  data set with tiles pre-cut for handheld devices, and `BUILD_TOUCH` builds the touch-friendly
  variant.
- `-DBUILD_MARBLE_TOOLS=OFF`, `-DBUILD_MARBLE_EXAMPLES=OFF` — Leave out the file-conversion tools
  and the C++ examples of the library; upstream's defaults.
- `-DBUILD_WITH_DBUS=ON` — Builds the D-Bus interface of the Marble widget, upstream's default on
  Linux.
- `-DWITH_DESIGNER_PLUGIN=OFF` — Leaves out the Marble widget plugin for Qt Designer, which is on
  by default.
- `-DWITH_libgps=ON`, `-DWITH_libshp=ON`, `-DWITH_ZLIB=ON` — Marble probes each of these with a
  macro that makes an option `WITH_<name>`, on by default, and quietly leaves the feature out when
  the package is not found. Named here: positions from [gpsd](#port-gpsd), shapefiles through
  [shapelib](#port-shapelib), and the system zlib rather than the copy in `src/3rdparty/zlib`.
  The loop after configure checks that gpsd's and shapelib's libraries and `protoc` were found,
  so a missing one fails the build instead of shipping a Marble without it.
- `-DWITH_Phonon4Qt6=OFF` — No Phonon, which gives the routing plugin spoken turn instructions.
- `-DWITH_Plasma=OFF` — No Plasma applets and wallpaper. They build against libplasma, which is
  not a port, and the desktop has no Plasma shell to show them.
- `-DWITH_libwlocate=OFF` — No position from nearby WLAN networks through libwlocate, which is not
  a port.

## Notes

The `protoc` check stands for the OpenStreetMap PBF reader, which Marble builds only when
[protobuf](#port-protobuf) is found, with [abseil-cpp](#port-abseil-cpp) beside it in `depends`.

Behaim and Marble Maps install their icons only as SVG, which nothing in the session rasterises.
`build.sh` renders each with `rsvg-convert` to 48, 128 and 256 pixel hicolor PNGs under the same
name. The `[ -f … ]` check before that fails the build if the main program's own PNG icon is
missing.

All three of upstream's desktop entries are replaced by ones that add `StartupWMClass`. KAboutData
gives each program its desktop file name, and so its Wayland app_id: `org.kde.marble` for the
globe, and `org.kde.marble.behaim` and `org.kde.marble.maps` for the two that set theirs
explicitly. The globe's entry keeps the `geo:` and `worldwind:` link handlers.
