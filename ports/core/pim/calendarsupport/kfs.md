## About

calendarsupport is the shared code of KOrganizer and the calendar views: calendar printing (the
day, week, month, year, to-do, journal and incidence layouts and their settings dialogs), the
calendar selection model, the free/busy model, attachment handling, event archiving, the
incidence viewer and the `KCalPrefs` settings the calendar programs share.
[eventviews](#port-eventviews), [incidenceeditor](#port-incidenceeditor), KOrganizer,
akonadi-calendar-tools and akonadiconsole link it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Holidays printed on a calendar come from [kholidays](#port-kholidays), and the notes editor is
`KF6TextCustomEditor` from [ktextaddons](#port-ktextaddons); the configure requires both.
Printing goes through Qt's print support, which reaches CUPS at run time.
