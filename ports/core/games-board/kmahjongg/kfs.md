## About

KMahjongg is KDE's Mahjong solitaire: free tiles are matched in pairs until the board is clear. Its
tile sets and backgrounds come from [libkmahjongg](#port-libkmahjongg), the rest from
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Builds the handbook, upstream's default, named;
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` requires [kdoctools](#port-kdoctools) for it.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.kmahjongg`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
