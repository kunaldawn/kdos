## About

Union is the style engine KDE is moving Plasma and its applications to: one description of how
controls look, written as CSS, drawn by three outputs, a Qt Widgets style (`Union`), a Qt Quick
Controls style (`org.kde.union`) and a Kirigami platform plugin. The CSS is read by a plugin that
parses it with [cxx-rust-cssparser](#port-cxx-rust-cssparser), a C++ wrapper around the Rust
`cssparser` crate, and the colours and icons come from the Plasma colour scheme and icon theme
through KDE Frameworks. Its styles are installed under `/usr/share/union`; the one shipped,
`breeze`, reproduces Breeze, with mobile and right-to-left variants.

[plasma-workspace](#port-plasma-workspace) requires it at configure time. KDOS builds it for that,
and leaves the widget style on Breeze: Union is marked "In Development", its theme entry is hidden
from the Application Style page, and its style format changes between Plasma releases, so no KDOS
style is written for it yet (see
[Known gaps](../../../../docs/kdos/06-reference/known-gaps.md)).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none, and CMake reports the variable as unused.
- `-D BUILD_TOOLS=OFF` — No `ruleinspector` and `styletool`, two programs for people writing
  Union styles, which upstream builds by default.
- `-D BUILD_EXAMPLES=OFF` — No gallery example; upstream's default.
- `-D BUILD_WITH_KDEFRAMEWORKS=ON` — With `-D BUILD_PLATFORM_PLASMA=ON`: read the colour scheme,
  icon theme and Plasma settings through KDE Frameworks, and build the Plasma platform plugin.
  Without them Union draws with fixed colours and ignores the desktop's settings.
- `-D BUILD_INPUT_CSS=ON` — The CSS input plugin, which is how every shipped style is written.
- `-D BUILD_INPUT_PLASMASVG=OFF` — No input that reads a Plasma SVG theme. It needs rapidyaml,
  which is not a port, and no shipped style uses it; it is upstream's default.
- `-D BUILD_OUTPUT_QTQUICK=ON` — With `-D BUILD_OUTPUT_KIRIGAMI=ON` and
  `-D BUILD_OUTPUT_QTWIDGETS=ON`: all three outputs, upstream's defaults, named so that each
  dependency they bring stays in `depends`. The Qt Quick output compiles its shaders with
  [qt6-qtshadertools](#port-qt6-qtshadertools).

## Notes

[plasma-integration](#port-plasma-integration)'s platform theme switches Qt Quick programs to the
`org.kde.union` style only when the widget style in `kdeglobals` is `union`; with KDOS's Breeze
default, nothing on the desktop draws with Union until somebody selects it.
