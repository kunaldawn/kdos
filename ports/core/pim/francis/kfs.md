## About

Francis is a Pomodoro timer: it alternates focus periods with short and long breaks, draws the
time left as a ring, and announces each change through a desktop notification. It is a Kirigami
application from KDE's convergent set, drawn through
[qqc2-desktop-style](#port-qqc2-desktop-style) on the desktop.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[knotifications](#port-knotifications), [kirigami](#port-kirigami) and qqc2-desktop-style are
in `depends` for run time: the notifications are sent from QML through the `org.kde.notification`
module, which the configure does not look for, and the program selects the `org.kde.desktop`
Qt Quick style unless `QT_QUICK_CONTROLS_STYLE` names another.
