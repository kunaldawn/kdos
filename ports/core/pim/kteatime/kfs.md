## About

KTeaTime is a tea timer that lives in the system tray: a tea is picked from a list of steeping
times, and when the time is up it raises a notification, through
[knotifications](#port-knotifications), and changes its tray icon. The teas and their times are
editable.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

KTeaTime has no window of its own; it needs a tray that hosts StatusNotifierItem icons, which the
Plasma panel is. Without one it runs with nothing on screen.
