## About
[KAlgebra]{.idx} is KDE's graph calculator, built on the [analitza](#port-analitza) maths library:
it evaluates expressions and plots functions in 2D and 3D. The port installs three front ends: the
desktop window `kalgebra`, the Kirigami touch interface `kalgebramobile`, and `calgebra`, a
calculator for the terminal.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Plasma=ON` — stop the configure if [libplasma](#port-libplasma) is
  missing. Upstream installs the Graph plasmoid under `plasmoids/`, a desktop and panel widget that
  plots a function, only when it finds libplasma, and otherwise drops it without an error.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON` — stop the configure if QtWebEngine's
  widgets are missing. Upstream builds the desktop window only when KDocTools,
  Qt6WebEngineWidgets, KConfigWidgets, KWidgetsAddons and KIO are all found, and otherwise drops it
  without an error.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Readline=ON` — with `-D CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON`:
  stop the configure if [readline](#port-readline) or [ncurses](#port-ncurses) is missing, the two
  libraries `calgebra` needs. Upstream treats both as optional and leaves `calgebra` out without
  them.

## Notes
The two `test -x` lines after the install fail the build when `kalgebra` or `calgebra` is missing
from the package, since a configure that dropped either would otherwise finish without an error.

Both desktop entries are KDOS's own, written over upstream's. They add `StartupWMClass`, matching
the Wayland app id `org.kde.<program>` that KAboutData sets, so the task manager can match each
window to its entry. They
claim no MIME type: upstream's claim `application/x-kalgebra`, which the shared MIME database does
not define.
