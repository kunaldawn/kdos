## About

eventviews holds the calendar views of KOrganizer and the PIM calendar plugins: the agenda (day,
work week, week and the side-by-side multi-agenda), the month view, the timeline, the event and
to-do lists, the journal view and "What's Next". Each view reads its incidences from an
[akonadi-calendar](#port-akonadi-calendar) model. KOrganizer, [incidenceeditor](#port-incidenceeditor)
and the KOrganizer plugins in kdepim-addons link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The timeline view is drawn with `KGantt6` from [kdiagram](#port-kdiagram), and the views' holiday
marks come from [kholidays](#port-kholidays); the configure requires both. Upstream's
`viewerapp` test program is built only with tests and installs nothing.
