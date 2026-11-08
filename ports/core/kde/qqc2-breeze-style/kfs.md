## About

The Breeze style for Qt Quick Controls, written in QML, so a Qt Quick application looks like
Breeze without loading a widget style. It is the style a Qt Quick program gets when it runs as a
plain `QGuiApplication`: [plasma-integration](#port-plasma-integration)'s platform theme picks it
for such programs whenever it is installed, and Kirigami applications on phones and in a plain
Qt Quick setup use it the same way. It also installs a Kirigami platform plugin,
`org.kde.breeze`, which gives Kirigami the Plasma colours, icons, fonts and units.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none, and CMake reports the variable as unused.
- `-D USE_DBUS=ON` — Build the Kirigami plugin with D-Bus, through which it hears the platform
  theme's `refreshFonts` signal and picks up a changed font, the small font included, while the
  program runs. It is upstream's default on Linux.
- `-D CMAKE_DISABLE_FIND_PACKAGE_X11=ON` — The configure looks for the X11 libraries and nothing
  in the build uses what it finds, so the search is switched off and the result is the same
  whatever the build root holds.
