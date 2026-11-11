## About

libqaccessibilityclient is a Qt client for the AT-SPI accessibility bus: it lists the
applications on the bus, walks their accessible objects, and reports focus, caret and selection
changes. KWin links it for its zoom effect's AT-SPI trackers of the keyboard focus and of an X11
window's text cursor (`KWIN_BUILD_QACCESSIBILITYCLIENT`), which on Wayland it runs only when
`KWIN_WAYLAND_ZOOM_FORCE_LEGACY_FOCUS_TRACKING` or
`KWIN_WAYLAND_ZOOM_FORCE_LEGACY_TEXT_CARET_TRACKING` is set; Accessibility Inspector is built on
it. The library is
`libqaccessibilityclient-qt6`, found with `find_package(QAccessibilityClient6)`.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6. The project asks ECM for a version older than KDE
  Frameworks 6, so ECM's `QtVersionOption` would otherwise pick Qt 5.

## Notes

Events arrive only while an AT-SPI bus is running and applications publish to it; the bus comes
from [at-spi2-core](#port-at-spi2-core). The `dumper` example is built with the tests, so
`BUILD_TESTING=OFF` leaves it out.
