## About

KConfigWidgets supplies the widgets KDE configuration dialogs are built from: `KConfigDialog`,
which binds a dialog's widgets to a `KConfigSkeleton` settings class, the standard actions (`KStandardAction`), the colour scheme menu (`KColorSchemeMenu`), the
hamburger menu and the command bar. [kxmlgui](#port-kxmlgui) and many KDE applications link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
