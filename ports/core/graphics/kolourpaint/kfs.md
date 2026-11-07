## About

KolourPaint is KDE's simple paint program: freehand and shape tools, selections, text, colour
effects, resizing and cropping, and scanning a picture in through [libksane](#port-libksane). It
opens and saves the formats Qt's image plugins and [kimageformats](#port-kimageformats) read.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Builds the handbook through [kdoctools](#port-kdoctools). Upstream's
  default, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KSaneWidgets6=ON` — Requires [libksane](#port-libksane) for
  File → Scan. Upstream looks for it as optional and leaves scanning out without it.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.kolourpaint`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
It also lists the image types KolourPaint opens in `MimeType=`, so it is offered under Open With
for them; the default handlers are set in the shipped MIME associations, not here.
