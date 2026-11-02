## About

Koko, named Photos in the menu, is KDE's Kirigami gallery: it indexes the pictures and videos
under the Pictures folder, groups them by folder, by date and by the place they were taken, plays
slideshows and videos, and crops, rotates and resizes images through
[kquickimageeditor](#port-kquickimageeditor). Places come from the photos' GPS tags, read with
[exiv2](#port-exiv2), matched offline against the GeoNames city and region tables that ship in
the tarball and are installed under `/usr/share/koko`.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6PrintSupport=ON` — Printing a picture. Upstream asks for
  Qt Print Support as an optional component and leaves the print action out without it; the
  module is part of [qt6-qtbase](#port-qt6-qtbase), and required, its absence stops the configure.
- `-D CMAKE_DISABLE_FIND_PACKAGE_XCB=ON` — No XCB code. It reads the display's colour profile
  only when Koko runs on the X11 platform plugin, which on KDOS it does not; refused, headers that
  happen to be present cannot compile it in.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Video playback is the `QtMultimedia` QML module of [qt6-qtmultimedia](#port-qt6-qtmultimedia),
and the shortcut settings come from [kirigami-app-components](#port-kirigami-app-components);
the configure checks the latter and not the former. Formats beyond Qt's own (AVIF, HEIF, PSD,
RAW previews and the rest) are decoded by the Qt image plugins of
[kimageformats](#port-kimageformats), loaded at run time.

The entry claims most image and video types. Which program opens a file of a type several entries
claim is decided by the desktop's `mimeapps.list`, not by the order the entries were installed.
The unit tests are compiled whatever `BUILD_TESTING` says and are not installed.
