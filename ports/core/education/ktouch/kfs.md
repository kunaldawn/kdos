## About

KTouch is KDE's touch-typing tutor: graded courses for many keyboard layouts, an on-screen
keyboard that shows the next key, and per-lesson statistics drawn with
[kqtquickcharts](#port-kqtquickcharts). Its interface is QtQuick, on
[kdeclarative](#port-kdeclarative) and [kirigami](#port-kirigami).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D WITHOUT_X11=ON` — No X11 keyboard-layout detection, which asks the X server through XKB
  which layout is active and follows its changes. KTouch runs as a Wayland client, where there is
  no X server to ask; it then reports the layout as unknown, and the course and keyboard layout
  are picked by hand in its course selector.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Requires [kdoctools](#port-kdoctools) for the
  handbook, which upstream builds only when it finds kdoctools.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.ktouch`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
