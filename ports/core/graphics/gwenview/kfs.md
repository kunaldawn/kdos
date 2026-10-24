## About

Gwenview is KDE's image viewer and browser. It opens a single picture or a folder of them, with
zoom, rotation, cropping and red-eye removal, slideshows, a side-by-side compare mode, raw camera
files, annotation and an importer for cameras and memory cards. The port builds it on Qt 6 and
the KDE Frameworks.

## Flags

- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON`, `-D KDE_INSTALL_LIBDIR=lib` — The install layout every
  KDE application here shares (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D GWENVIEW_SEMANTICINFO_BACKEND=Baloo` — Keeps ratings, tags and comments through
  [baloo](#port-baloo), upstream's default. When Baloo is not found, upstream drops to the `None`
  backend with only a status message; `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON` makes it stop
  configure instead.
- `-D GWENVIEW_IMAGEANNOTATOR=ON` — The annotation tool, through
  [kimageannotator](#port-kimageannotator); upstream's default, which already requires the
  library.
- `-D WITHOUT_X11=OFF` — Builds the X11 integration, upstream's default, used when Gwenview runs
  as an X11 client under Xwayland. `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON` requires libX11 for it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_CFitsio=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON` — Turn optional finds into required ones, so a missing
  library stops configure instead of building a Gwenview without it: the Baloo backend, raw
  decoding through [libkdcraw](#port-libkdcraw), the FITS loader through
  [cfitsio](#port-cfitsio), libtiff, which Gwenview links only to silence libtiff's log messages,
  and libX11 (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6Purpose=ON` — Leaves out the Share menu, which Gwenview builds
  on KDE's Purpose framework whenever [purpose](#port-purpose) is installed; its plugins send
  images to online services.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PlasmaActivities=ON` — Leaves out Plasma Activities, the
  Plasma shell's activity tracking, which is not a port.

## Notes

`kdoctools_install()` builds the handbook in every translated language the tarball carries; the
translated handbooks are removed by `kpkg`'s language rule, not by the recipe.

KAboutData gives Gwenview the desktop file name `org.kde.gwenview`, which is also its Wayland
app_id. Upstream's entry is replaced by one with `StartupWMClass=org.kde.gwenview` and without
`inode/directory` in `MimeType=`: claimed by Gwenview, a folder could open in it instead of the
file manager. The hicolor PNG icons it names are upstream's.
