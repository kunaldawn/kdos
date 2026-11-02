## About

KAtomic is a KDE puzzle game in the manner of Sokoban: atoms slide across the board until they hit
a wall, and the player arranges them into the molecule shown. It is built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Requires [kdoctools](#port-kdoctools) for the
  handbook, which upstream builds only when it finds kdoctools.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.katomic`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
