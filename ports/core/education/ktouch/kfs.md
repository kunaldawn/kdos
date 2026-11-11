## About

KTouch is KDE's touch-typing tutor: graded courses for many keyboard layouts, an on-screen
keyboard that shows the next key, and per-lesson statistics drawn with
[kqtquickcharts](#port-kqtquickcharts). Its interface is QtQuick, on
[kdeclarative](#port-kdeclarative) and [kirigami](#port-kirigami).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D WITHOUT_X11=OFF`, `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_XCB=ON` — Build the X11 keyboard-layout detection, which asks the
  X server through XKB which layout is active and follows its changes, so the course selector
  starts on the active layout. Upstream builds it only when it finds Qt's private GUI headers,
  [libX11](#port-libX11) with its Xkb and Xkbfile parts ([libxkbfile](#port-libxkbfile)) and
  [libxcb](#port-libxcb) with its xkb library, and otherwise drops it silently. The two `REQUIRE`
  flags make a missing X11 or XCB package fail configure; a missing component passes them, so
  the check after the install fails the build when `ktouch` does not link `libxcb-xkb.so.1`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Requires [kdoctools](#port-kdoctools) for the
  handbook, which upstream builds only when it finds kdoctools.

## Notes

KTouch's `main()` sets `QT_QPA_PLATFORM=xcb` on Linux, over the session's `wayland;xcb`, because
upstream's key handling does not see key presses under Wayland. It always runs as an X11 client
on Xwayland, and the XKB layout detection above asks Xwayland, which KWin gives the session's
keyboard layout.

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.ktouch`, the desktop file name KAboutData sets, which KWin matches against the X11
window's class, so the task manager can match the window to its entry.
