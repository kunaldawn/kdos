## About

Artikulate is KDE Education's pronunciation trainer: the learner hears a native speaker say a
word or phrase, records their own attempt, and plays the two back to compare. Courses are sets of
recordings per language. The trainer is a Kirigami window; `artikulate_editor`, which builds
courses, is installed beside it without a menu entry of its own.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream looks for it as an optional package, and
  without it leaves the handbook out and only lists the package in the configure summary.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`system-courses.patch` makes the trainer read courses from `artikulate/courses` below every system
data directory as well as from the user's `~/.local/share/artikulate/courses`, where KNewStuff
installs them. Upstream reads only the user's directory, so a packaged course is never found. It
is to be offered upstream at `education/artikulate`.

## Notes

Four courses ship in the package, KDE's French, Greek, Bengali and Polish basic courses, the
course sets Artikulate's KNewStuff provider lists, each its own `source =` pinned by its
`sha256` and installed under `/usr/share/artikulate/courses/basic/<language>/`. They are
licensed CC-BY-SA-3.0. KNewStuff is off on KDOS (`[KDE Action Restrictions][$i] ghns=false` in
`/usr/share/kdos/xdg/kdeglobals`), so the trainer has no download entry and these are the courses
it offers. The `test` line in the install loop stops the build if a course's XML is missing.

Recording and playback go through Qt Multimedia, which
[qt6-qtmultimedia](#port-qt6-qtmultimedia) builds with its FFmpeg backend.

Course and skeleton files are XML that [libxml2](#port-libxml2) validates against their schemas.
The learner profile is a library of the program's own, `libartikulatelearnerprofile`, installed
without its development link; nothing else links it. The interface imports the formcard module of
[kirigami-addons](#port-kirigami-addons), which CMake does not check.

`KAboutData` gives the window the app_id `org.kde.artikulate`, the name of upstream's entry.
