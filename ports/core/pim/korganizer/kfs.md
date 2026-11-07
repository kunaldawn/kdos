## About

KOrganizer is KDE's calendar and scheduler. It shows and edits the events, to-dos and journal
entries of every Akonadi calendar a [kdepim-runtime](#port-kdepim-runtime) resource serves (local
iCalendar files and folders, CalDAV, Google), in day, week, month, agenda, timeline and list
views from [eventviews](#port-eventviews), with the editor from
[incidenceeditor](#port-incidenceeditor), meeting invitations sent through
[kmailtransport](#port-kmailtransport), holidays from [kholidays](#port-kholidays) and printing
from [calendarsupport](#port-calendarsupport). It runs on its own or as the calendar, to-do,
journal and special-dates pages of [kontact](#port-kontact). The port also builds four calendar
decorations: week numbers, moon phases, Wikipedia's picture of the day and "this day in history".

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: calendars can be tied to Plasma activities
  and shown only in them. Upstream searches for [pimcommon](#port-pimcommon)'s
  `PimCommonActivities` library quietly, and for [plasma-activities](#port-plasma-activities) only
  once that library is found, so either missing turns the feature off without a word. Both are
  required, and the configure stops when pimcommon is built without activities.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which KOrganizer
  would submit usage statistics. It is not a port, and refusing the search keeps a stray copy in
  the build root from turning telemetry on.

## Notes

The picture-of-the-day and this-day-in-history decorations read `en.wikipedia.org` for every
date they draw. No decoration is selected by default (`SelectedPlugins` is empty in upstream's
settings), so nothing is fetched until the user turns one on.

The configure requires Qt's `UiTools` module, from [qt6-qttools](#port-qt6-qttools), though no
source in this release uses it, so qt6-qttools is in `depends`.

[kdepim-runtime](#port-kdepim-runtime) is in `depends` for run time only: without its resources
Akonadi has no calendar to show. The Plasma Digital Clock's event list reads the same calendars
through [kdepim-addons](#port-kdepim-addons), which this port does not name.

The Wayland app id is `org.kde.korganizer`, the desktop file name `KAboutData` derives from the
component name `korganizer`; it is the name of the entry upstream installs, which carries no
`StartupWMClass`, so the task manager matches the window to it with no change. The main entry
claims `text/calendar`, as Thunderbird's does; the default for calendar files is chosen in
`mimeapps.list`, not by either entry. `korganizer-view.desktop`, which opens an Akonadi incidence
or a calendar file, and `korganizer-import.desktop` are `NoDisplay=true`.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.

Upstream looks for the X11 libraries without `REQUIRED`, and the recipe cannot refuse them:
[kwindowsystem](#port-kwindowsystem) is built with its X11 backend, so its CMake package finds X11
as a dependency of its own, and `CMAKE_DISABLE_FIND_PACKAGE_X11` stops the configure there. With
X11 found, KOrganizer compiles the path that raises an already open window for a calendar file
and the X11 startup notification, both through kwindowsystem; it links no X11 library itself.
