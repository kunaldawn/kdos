## About

akonadi-mime is the mail half of [akonadi](#port-akonadi): the serializer plugin that stores a
MIME message as an Akonadi item, the message and folder models, the special folders (inbox,
outbox, sent, trash, drafts), message status flags, the outbox queue and the standard mail
actions KMail's menus are built from. KMail, the mail parts of
[akonadi-calendar](#port-akonadi-calendar), [akonadi-search](#port-akonadi-search) and the
message libraries all link it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure requires `xsltproc` from [libxslt](#port-libxslt): it turns the IMAP and POP3
resources' `.kcfg` settings files into the D-Bus interface descriptions the library calls them
through, with the `kcfg2dbus.xsl` stylesheet [akonadi](#port-akonadi) installs. The contact-group
MIME type it installs is registered through [shared-mime-info](#port-shared-mime-info). The QML
types in `src/quick` need Qt's QML integration headers, from
[qt6-qtdeclarative](#port-qt6-qtdeclarative).
