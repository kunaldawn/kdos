## About

KBlocks is KDE's falling-blocks game, played alone against the clock or against a computer player,
with themes from [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.kblocks`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
