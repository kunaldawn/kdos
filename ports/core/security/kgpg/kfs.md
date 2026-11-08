## About

KGpg is a front end to GnuPG: it lists the keys in the keyring with their trust and validity,
generates, imports, exports, signs and revokes them, talks to key servers, and has an editor and
a clipboard action that encrypt, decrypt, sign and verify text. Dolphin service menus encrypt a
file or folder and show a decrypted file. "Create New Contact in Address Book" opens a key's
owner as an Akonadi contact, found by e-mail address or new. [kleopatra](#port-kleopatra) covers
the same ground with S/MIME as well; both are installed.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`find_package(KPim6AkonadiContactWidgets REQUIRED)` makes
[akonadi-contacts](#port-akonadi-contacts) a requirement of the configure, and with it the
Akonadi stack, so KGpg builds with the PIM wave. GpgME is found by upstream's own `FindGpgme`,
which runs `gpgme-config` from [gpgme](#port-gpgme); every key operation runs `gpg` from
[gnupg](#port-gnupg).

The port installs its desktop entry a second time into `/etc/xdg/autostart`, guarded by
`X-KDE-autostart-condition=kgpgrc:User Interface:AutoStart:false`, so KGpg starts with the
session only after its first-run assistant's "start automatically" box has been ticked.
