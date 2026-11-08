## About

KTnef parses and writes TNEF, the `winmail.dat` attachment Outlook sends in place of ordinary MIME
parts. It lists the files packed inside, extracts them, reads the message properties, and turns
a meeting request carried in TNEF into an iCalendar invitation. KMail's TNEF viewer and the
calendar plugins in [kdepim-addons](#port-kdepim-addons) use it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure requires `KPim6CalendarUtils` 6.8.2 or newer, the PIM library version of Gear
26.08.2's [kcalutils](#port-kcalutils); an older kcalutils stops it.
