## About

incidenceeditor is the editor KDE PIM opens on an event, a to-do or a journal entry: the dialog
with its general, attendees, reminders, recurrence and attachments pages, the scheduling dialog
that finds a free slot across attendees, the free/busy view, the resource and room picker over
LDAP, and the templates. KOrganizer, KMail's "create event" and "create to-do" plugins and
akonadi-calendar-tools open it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The free/busy editor is drawn with `KGantt6` from [kdiagram](#port-kdiagram). Attendee and
resource lookup goes through [kldap](#port-kldap)'s widgets, the description field is a
[kpimtextedit](#port-kpimtextedit) editor, and invitations are prepared with
[pimcommon](#port-pimcommon) and [libkdepim](#port-libkdepim); the configure requires every one.
