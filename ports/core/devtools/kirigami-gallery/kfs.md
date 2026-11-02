## About

Kirigami Gallery shows every Kirigami component on a page of its own, with the QML that builds it:
pages, drawers, cards, form layouts, list delegates, dialogs and the rest. It is for developers,
and for checking how a Kirigami theme draws each control.

## Flags

- `-D ENABLE_QML_PROFILING=OFF` — Build without `QT_QML_DEBUG`. Upstream's default is on, which
  compiles in the QML debugging and profiling service: a program started with `-qmljsdebugger`
  then opens a debug port that accepts connections. A release build has no use for it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translations in `poqm/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The program sets the Qt Quick Controls style to `org.kde.desktop` unless
`QT_QUICK_CONTROLS_STYLE` names another, so [qqc2-desktop-style](#port-qqc2-desktop-style) is in
`depends` for run time; without it every control falls back to Qt's Basic style. It sets its own
desktop file name, `org.kde.kirigami2.gallery`, which matches its entry, so the entry needs no
`StartupWMClass`.
