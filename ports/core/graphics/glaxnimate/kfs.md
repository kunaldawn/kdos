## About

Glaxnimate is KDE's vector animation editor, for motion graphics and animated icons. It opens and
saves Lottie, animated SVG and its own format, and exports GIF and video through
[ffmpeg](#port-ffmpeg). It is built against Qt 6 and KDE Frameworks 6, with Python scripting.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KF6. Without it the project looks for Qt 5.
- `-D GLAXNIMATE_SYSTEM_POTRACE=ON` — Link the [potrace](#port-potrace) port for bitmap tracing
  instead of the copy under `external/`. It is upstream's default, written out.
- `-D MOBILE_UI=OFF` — Build the desktop interface, not the touch one. It is upstream's default
  off Android.
- `-D VERSION_SUFFIX=` — Add nothing to the version string. The default, `-git-`, makes the
  configure ask git for a revision when a `.git` directory is present, which a release tarball
  does not have.
- `-D KF_SKIP_PO_PROCESSING=ON` — Compile no translation catalogues; the program shows its English
  source strings.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6BreezeIcons=ON` — KF6BreezeIcons is a quiet find; when it is
  found the Breeze icon set is compiled into the program. Requiring it makes a missing
  [breeze-icons](#port-breeze-icons) a configure error, instead of a toolbar left blank under an
  icon theme that lacks those names.

## Notes

The project forces its own internal libraries static, so `BUILD_SHARED_LIBS` is not passed.
Python scripting builds against [python3](#port-python3) and the pybind11 copy bundled under
`external/QtAppSetup`; nothing is fetched at build time.

Upstream installs its SVG logo under `hicolor/512x512/apps` with a `.png` name. The loop after the
install rasterises the logo with `rsvg-convert` into real PNGs at five sizes, one of which
replaces that file, because the panel draws application icons from hicolor PNGs only (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
The desktop entry is written over upstream's with `StartupWMClass=org.kde.glaxnimate`, the
Wayland app id KAboutData sets.

The Google Fonts, LottieFiles and emoji download dialogs reach the network when they are opened,
and report the failure on a machine with no network; no build switch removes them.
