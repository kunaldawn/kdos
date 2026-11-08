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

## Notes

No course ships in the package. The course list is downloaded through
[knewstuff](#port-knewstuff) when the learner asks for one, so the trainer needs the network once
per course. Recording and playback go through Qt Multimedia, which
[qt6-qtmultimedia](#port-qt6-qtmultimedia) builds with its FFmpeg backend.

Course and skeleton files are XML that [libxml2](#port-libxml2) validates against their schemas.
The learner profile is a library of the program's own, `libartikulatelearnerprofile`, installed
without its development link; nothing else links it. The interface imports the formcard module of
[kirigami-addons](#port-kirigami-addons), which CMake does not check.

`KAboutData` gives the window the app_id `org.kde.artikulate`, the name of upstream's entry.
