## About

Akregator is KDE's feed reader. It subscribes to RSS and Atom feeds, parsed by
[syndication](#port-syndication), fetches them on a timer, keeps an archive of read and unread
articles, and shows each article, or the page it links to, in a viewer from
[messagelib](#port-messagelib) built on [qt6-qtwebengine](#port-qt6-qtwebengine). It runs on its
own or as a page of [kontact](#port-kontact). Akregator keeps its feeds and articles in its own
files under the user's data directory, not in Akonadi.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextEditTextToSpeech=ON` — reading an article aloud, through
  the `TextEditTextToSpeech` library of [ktextaddons](#port-ktextaddons). ktextaddons builds that
  library only when it finds [qt6-qtspeech](#port-qt6-qtspeech); upstream looks for it quietly, so
  a ktextaddons without speech would drop the action with nothing said. Required, it stops the
  configure instead.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: a feed can be tied to Plasma activities and
  the feed list then shows only the current activity's feeds. Upstream searches for
  [pimcommon](#port-pimcommon)'s `PimCommonActivities` library quietly, and for
  [plasma-activities](#port-plasma-activities) only once that library is found, so either missing
  turns the feature off without a word. Both are required, and the configure stops when
  pimcommon is built without activities.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which Akregator
  would submit usage statistics. It is not a port, and refusing the search keeps a stray copy in
  the build root from turning telemetry on.

## Notes

Akregator puts an icon in the system tray through
[kstatusnotifieritem](#port-kstatusnotifieritem), which needs a StatusNotifierItem host on the
session bus; the Plasma panel's system tray is one.

The Wayland app id is `org.kde.akregator`, which `main.cpp` sets as the desktop file name; it is
the name of the entry upstream installs, which carries no `StartupWMClass`, so the task manager
matches the window to it with no change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
