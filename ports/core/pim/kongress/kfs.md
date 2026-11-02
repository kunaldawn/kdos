## About

Kongress is a companion for conferences: it loads a conference's schedule as an iCalendar feed
through [kcalendarcore](#port-kcalendarcore), lists the talks by day and track, keeps favourites,
reminds of a talk before it starts, and shows the venue's floor plan through
[kosmindoormap](#port-kosmindoormap). It is a Kirigami application; the window is QML on
[kirigami](#port-kirigami) and [kirigami-addons](#port-kirigami-addons).

## Flags

- `-D REMINDERS_ENABLED=ON` — Build `kongressac`, the alarm client that raises a notification
  for a favourite talk. It is started over D-Bus by its service file, not from the menu. On by
  default upstream; named so the reminders cannot drop out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The map page imports `QtLocation` and `QtPositioning`, which the configure does not look for;
[qt6-qtlocation](#port-qt6-qtlocation) and [qt6-qtpositioning](#port-qt6-qtpositioning) are in
`depends` so the page loads.

`kongressac` asks `org.kde.Solid.PowerManagement` to wake the machine for a reminder. That
service is PowerDevil's, part of the Plasma session; without it a reminder still fires while the
machine is awake. Conference schedules are fetched from the network when the person adds one.
