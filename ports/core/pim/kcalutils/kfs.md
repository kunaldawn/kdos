## About

KCalUtils is the calendar formatting library of KDE PIM: it turns an event, to-do or journal, or
an iTIP invitation, into the HTML KMail shows in a message and the text of tooltips and
reminders, and it holds the date and duration helpers and the drag-and-drop format KOrganizer
uses. The invitation and incidence templates are rendered with
[ktexttemplate](#port-ktexttemplate).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure requires `KPim6IdentityManagementCore` from
[kidentitymanagement](#port-kidentitymanagement) at exactly the PIM version of this release, so
the two ports move to a new Gear version together.
