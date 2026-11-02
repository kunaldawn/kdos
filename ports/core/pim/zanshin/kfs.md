## About

Zanshin is a to-do manager built around Getting Things Done: an inbox, projects, contexts and a
workday view over the to-dos of Akonadi calendars. It runs on its own or as a page of
[kontact](#port-kontact), and its KRunner plugin, built against [krunner](#port-krunner), adds a
to-do from the search box with `todo: <text>`. The to-dos are the same ones
[korganizer](#port-korganizer) and [merkuro](#port-merkuro) show.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

[krunner](#port-krunner) is `REQUIRED`, with no switch, for the runner plugin, which goes to
`/usr/lib/qt6/plugins/kf6/krunner`; the search box that loads it is the Plasma session's, which
this port does not name (a port in 41 to 44 never depends on a session port). [boost](#port-boost)
is `REQUIRED` for one header-only utility.

[kdepim-runtime](#port-kdepim-runtime) is in `depends` for run time only: a to-do lives in an
Akonadi calendar, and without a calendar resource Zanshin has nowhere to keep one.

The Wayland app id is `org.kde.zanshin`, the desktop file name `KAboutData` derives from the
component name `zanshin`; it is the name of the entry upstream installs, which carries no
`StartupWMClass`, so the task manager matches the window to it with no change.
