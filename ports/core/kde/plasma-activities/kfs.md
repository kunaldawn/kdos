## About

Plasma Activities is the client side of activities: separate sets of windows, files and
widgets a person switches between. The library and its QML module, `org.kde.activities`, ask the
activity manager, kactivitymanagerd, over D-Bus which activities exist and which one is current,
and tell it which files and applications are in use so it can keep its record.
`plasma-activities-cli6` lists, creates, switches and removes activities from a shell. The Plasma
shell and KWin link it, and so do Gwenview, kio-extras and the PIM modules that tag what they
open.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none, and CMake reports the variable as unused.
- `-D PLASMA_ACTIVITIES_LIBRARY_ONLY=OFF` — Build the QML module beside the library. It is
  upstream's default, and Plasma's activity switcher is written in QML that imports the module.

## Notes

The library does nothing without [kactivitymanagerd](#port-kactivitymanagerd), the daemon that
owns `org.kde.ActivityManager` on the session bus. It is a session port, in `50_desktop`, so this
library, which is built in `43_toolkits` for the applications that link it, cannot name it in
`depends`; the `kde-plasma` meta port installs it. Without the daemon every activity call returns
an empty answer and an application that links the library runs as though activities were off.
