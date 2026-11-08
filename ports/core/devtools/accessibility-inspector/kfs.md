## About

Accessibility Inspector browses the AT-SPI accessibility tree of every running program: each
object's role, name, description, states and actions, and the events it emits. It shows what a
screen reader such as Orca is told about a window, which makes it the tool for checking a
program's accessibility.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The tree comes from [libqaccessibilityclient](#port-libqaccessibilityclient), required as
`QAccessibilityClient6` 0.6.0 or later. It reads the accessibility bus that
[at-spi2-core](#port-at-spi2-core) runs, so the inspector shows nothing until that bus is up and a
program has registered on it.

The program installs a private shared library, `libaccessibilityinspector`, with the window it
draws.
