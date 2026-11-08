## About

kde-dev-utils holds small tools for developers of Qt and KDE programs. KUIViewer opens a Qt
Designer `.ui` file and shows the form it describes, live, without compiling it; its KParts
component shows the same view inside other programs, and a thumbnail plugin gives `.ui` files a
preview of their form in Dolphin. `kpartloader` loads any installed KParts component by name in a
bare window, for testing a part.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The viewer, its part and the thumbnailer load forms through `Qt6::UiTools` and `Qt6::Designer`
from [qt6-qttools](#port-qt6-qttools), whose recipe builds Designer, so its custom-widget plugins
load too.
