## About

MailImporter reads the mail stores of other clients and hands each message to a destination.
`KPim6MailImporter` holds the filters, one per source: Thunderbird, Icedove and SeaMonkey,
Evolution 1 to 3, Claws Mail and Sylpheed, Balsa, KMail's own Maildir and archives, Opera,
Outlook Express, Apple Mail, Lotus Notes, The Bat!, Pegasus Mail, plain mbox and plain-text
folders. `KPim6MailImporterAkonadi` stores what they read into Akonadi folders.
[mailcommon](#port-mailcommon), the import wizard and the mbox importer are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every KPim library it finds, [akonadi](#port-akonadi), [akonadi-mime](#port-akonadi-mime) and
[pimcommon](#port-pimcommon), must be at least PIM 6.8.2, the version of Gear 26.08.2, so the PIM
ports move together. The Akonadi half does nothing without a running Akonadi server, which only a
PIM application starts.
