## About

Parley is KDE's vocabulary trainer: word lists in KVTML through
[libkeduvocdocument](#port-libkeduvocdocument), practised as flash cards, written answers,
multiple choice, conjugations and comparisons, with spaced repetition deciding what comes back
when. Lessons are exported to HTML.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_BROWSERINTEGRATION=OFF` — Leaves out the editor's web panel, which looks the current
  word up in online dictionaries such as dict.tu-chemnitz.de. Upstream's default is on, and the
  panel then requires Qt WebEngine.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibXslt=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON` — Require
  [libxslt](#port-libxslt) and [libxml2](#port-libxml2), the two libraries of the HTML export.
  Upstream treats both as optional and leaves the export out without them.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.parley`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
