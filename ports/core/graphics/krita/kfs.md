## About

Krita is a painting program for digital painting, illustration, comics and frame-by-frame
animation, with brush engines, layers and masks, colour management through OpenColorIO and lcms2,
pressure-sensitive tablet input, and Python scripting. The port builds Krita on Qt 6 and the KDE
Frameworks, with its Wayland platform integration and its X11 path for Xwayland. G'MIC-Qt, which
Krita can host as a filter plugin, is a separate program and is not part of this port.

## Flags

- `-D BUILD_WITH_QT6=ON` — Builds against Qt 6 and KF6. Without it the project looks for Qt 5.
- `-D ALLOW_UNSTABLE=QT6` — Upstream marks the Qt 6 build unstable, and configure stops unless
  this list names `QT6`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON`, `-D KDE_INSTALL_LIBDIR=lib` — The install layout every
  KDE application here shares (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D ENABLE_UPDATERS=OFF` — Removes the check for new releases and its notification, which are
  on by default.
- `-D FOUNDATION_BUILD=OFF`, `-D KRITA_ENABLE_PCH=OFF`, `-D BUILD_KRITA_QT_DESIGNER_PLUGINS=OFF` —
  Upstream's defaults, named: an ordinary system install rather than the Krita Foundation's binary
  release with its extra bundled resources, no precompiled headers, and no Qt Designer plugins for
  Krita's widgets.
- `-D USE_EXTERNAL_RAQM=OFF` — Builds the copy of Raqm, the text-layout library, that Krita
  carries, rather than fetching one from the network at configure time; upstream's default, named.
- `-D KRITA_QT_HAS_ENTER_LEAVE_PATCH=OFF`, `-D KRITA_QT_HAS_UNBALANCED_KEY_PRESS_RELEASE_PATCH=OFF`,
  `-D KRITA_QT_HAS_UPDATE_COMPRESSION_PATCH=OFF` — Each names a patch that Krita's own build of Qt
  carries and the tree's [qt6-qtbase](#port-qt6-qtbase) does not. Krita probes the Qt headers for
  each; named `OFF`, the answer is pinned. Off, Krita keeps its own workarounds for unbalanced
  enter and leave events and for unbalanced key presses and releases in non-Latin layouts, and
  draws the canvas without limiting its updates to the display's frame rate. Set `ON` against
  this Qt, those events would go unhandled.
- `-D KRITA_USE_SURFACE_COLOR_MANAGEMENT_API=ON` — Gives the canvas its own colour space through
  the per-surface colour-management API. Its Wayland client for the `color-management-v1`
  protocol generates its code from the protocol XML in the tarball. The default is on whenever Qt's
  Wayland client is found; `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON` makes that
  certain.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translations; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Quick=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6QuickWidgets=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6QuickControls2=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6DBus=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON` — Require the Qt and KDE parts Krita otherwise uses
  only when present: the Wayland platform plugin, the QML-based touch dockers, D-Bus, and KCrash.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PythonLibrary=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_SIP=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PyQt6=ON` — Require the Python plugin host and the scripts that
  ship with it, built through [python3-sip](#port-python3-sip) against
  [python3-pyqt6](#port-python3-pyqt6).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_GSL=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KSeExpr=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_FFTW3=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenColorIO=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LibMyPaint=ON` — Require the libraries behind filters, layers and
  engines: GSL, the SeExpr fill layer through [kseexpr](#port-kseexpr), FFTW, the OpenColorIO
  display filter, and the MyPaint brush engine through [libmypaint](#port-libmypaint).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_GIF=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_HEIF=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenJPEG=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_JPEGXL=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON` — Require the libraries behind file formats: EXR,
  TIFF, JPEG, GIF, HEIF and AVIF, JPEG 2000, JPEG XL, PDF import through poppler's Qt 6 binding,
  and camera raw import through [libkdcraw](#port-libkdcraw).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Mlt7=ON` — Requires [mlt](#port-mlt), which plays the audio
  track of an animation, with SDL2 for its output.

Every one of these finds is optional upstream, and a missing library builds a Krita without that
format, filter or engine, saying so only in the configure summary. Required, a missing port stops
configure instead (see
[A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Patches

- `uic-assistants-class-name.patch` — Removes the leading space from the class name `" QWidget"` of `showDockerOptionsPanel` in `plugins/assistants/Assistants/AssistantsToolOptions.ui`. The tree's `uic` checks every widget class name and stops with `Invalid class name` on one that does not open with a letter or an underscore.
- `sip-abi.patch` — Makes `cmake/modules/SIPMacros.cmake` give the Qt 6 Python bindings the sip ABI version `13` in place of `13.0`, so sip uses the newest minor version of ABI 13, as PyQt6 itself does. PyQt6 declares a minimum ABI above 13.0, and sip refuses to generate the `krita` module against a pinned 13.0.

## Notes

WebP and libjpeg-turbo cannot be required the same way. A requirement also applies to the
config-package search each of Krita's find modules makes first, and both fail it with the library
present: the config files of [libwebp](#port-libwebp) and [libjpeg-turbo](#port-libjpeg-turbo)
reject the components the module asks for (`demux` and `mux`; `turbojpeg`), which they define no
result for and which the module marks found only after the search has returned. Unrequired, that
search fails quietly and the module finds the library another way. As in
[A requirement a find module misreads](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-requirement-a-find-module-misreads),
the two `grep` lines after configure check for them in the generated build instead: the WebP
import filter's target in `build.ninja`, and `HAVE_JPEG_TURBO` in `config-jpeg.h`.

`0001-fftw-use-pkgconfig.patch` is needed because the FFTW autotools install writes an
`FFTW3Config.cmake` that names targets it never defines, and Krita's find module takes that file
first and fails.

Each import filter installs a hidden desktop entry claiming its format, `image/png`,
`image/jpeg` and the rest, which would make Krita a second handler for every common picture
type. `build.sh` removes those `krita_*.desktop` files and replaces the main entry with one that
claims only Krita's own document type, `application/x-krita`; the brush-preset type upstream's
entry names is not in the MIME database. `QGuiApplication::setDesktopFileName` makes the Wayland
app_id `org.kde.krita`, which the entry names as `StartupWMClass`; under Xwayland the window
class is `krita`.
