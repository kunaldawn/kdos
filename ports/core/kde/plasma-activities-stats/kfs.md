## About

Plasma Activities Stats reads the record the activity manager keeps of what was used, when and in
which activity, and turns it into Qt models: the recent and frequent files and applications that
Kickoff, the task manager's jump lists, KRunner and Dolphin's `recentlyused:/` folder show. It
reads kactivitymanagerd's SQLite database directly, opened read-only through Qt's SQLite driver,
and asks the daemon over D-Bus to forget an entry.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none, and CMake reports the variable as unused.
- `-D BUILD_QCH=OFF` — No API documentation in Qt Help format; no port builds QCH.

## Notes

The database is written by [kactivitymanagerd](#port-kactivitymanagerd), a session port that
this `43_toolkits` library cannot name in `depends`; the `kde-plasma` meta port installs it. With
no database every model is empty. The SQLite driver is the one built into
[qt6-qtbase](#port-qt6-qtbase).
