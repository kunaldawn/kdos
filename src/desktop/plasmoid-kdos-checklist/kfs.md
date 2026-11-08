## About
`plasmoid-kdos-checklist` is the first-run checklist that ticks itself: a QML module,
`org.kdos.checklist`, and the Getting Started tray entry, `org.kdos.checklist`, built on it. The
module's `ChecklistView` is also the list on the welcome tour's last page, so both show the same
items from the same probe.

The items are: set up a backup, pair your phone (optional), add a language, create your first
box, check firmware and confirm disk encryption. Which of them this machine has, and whether each
is done or dismissed, comes from `kdos checklist --json`, which kdos-tools provides and which omits
an item whose part of the system is not installed. The probe runs every five seconds while an item
is still to do, so creating a box ticks its row within that, and not at all once every item is
done or dismissed; then the tray entry hides itself completely. Not Now runs
`kdos checklist dismiss <id>`, and Show Dismissed Items `kdos checklist restore`.

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no
  tarball, and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the applet and the QML module under Qt's own
  directories, `/usr/lib/qt6/plugins/plasma/applets` and `/usr/lib/qt6/qml`, where plasmashell
  and plasma-welcome look.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/`, which
  drives a stand-in `kdos` from `tests/fake`, is run by the Qt regression net in the development
  container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build.

## Notes
The words of each item and what its button opens are in the module, keyed by the item's id, and
never in the probe's output: they are translated with the rest of the desktop, and nothing the
probe prints is run. An id the module does not know is left out, so a newer probe with a new
item does not show a row with no words. Each button runs a program and its arguments, found on
`PATH`: System Settings' `kcm_kup`, `kcm_regionandlang` and `kcm_kdos_boxes`, the KDE Connect
application (KDE Connect 26.08 has no settings module of its own), Discover's update page, and
the KDOS book's security chapter in the Help Center.

A probe that fails leaves the list as it was, with the probe's last line of error above it, since
a failure says nothing about whether an item is done.
