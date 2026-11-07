## About

MessageLib is the message half of KMail, as seven libraries other PIM programs share:
`MimeTreeParser` takes a message apart into its MIME tree, decrypting and verifying signed and
encrypted parts through GpgME; `MessageViewer` renders it in a Qt WebEngine view with header
themes, scam detection and plugins; `WebEngineViewer` is that view's base and the Safe Browsing
lookup; `MessageComposer` builds, signs, encrypts and sends a new message; `MessageList` is the
threaded message list; `MessageCore` holds the settings and helpers the others share; and
`TemplateParser` expands reply and forward templates. [kmail](#port-kmail),
[mailcommon](#port-mailcommon), [kdepim-addons](#port-kdepim-addons) and Merkuro's mail view
link it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Inotify=ON` — Requires inotify, which upstream looks for quietly.
  The composer watches an attachment opened in an external editor through it, to take the
  edited file back when the editor saves; without it the composer cannot tell when editing is
  done. musl's `sys/inotify.h` satisfies the check, with no separate library.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextEditTextToSpeech=ON` — Requires ktextaddons' text-to-speech
  library from [ktextaddons](#port-ktextaddons), which the viewer uses to read a message aloud.
  Upstream looks for it quietly and drops the feature without one.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The viewer requires Qt WebEngine Widgets from [qt6-qtwebengine](#port-qt6-qtwebengine). Signed
and encrypted mail goes through [gpgmepp](#port-gpgmepp) and [qgpgme](#port-qgpgme), 2.0 or
newer, and [libkleo](#port-libkleo)'s key widgets, with [gnupg](#port-gnupg)'s `gpg` and
`gpgsm` doing the work at run time. The viewer's DKIM verification checks the signing key
with [openssl](#port-openssl), which the configure requires.

On Linux the configure requires `KPim6AkonadiSearch` from [akonadi-search](#port-akonadi-search)
unless `FORCE_DISABLE_AKONADI_SEARCH` is set, which this recipe leaves off: the message list's
quick search reads the index through it.

Every KPim library it finds must be at least PIM 6.8.2, the version of Gear 26.08.2:
[akonadi](#port-akonadi), [akonadi-mime](#port-akonadi-mime),
[akonadi-contacts](#port-akonadi-contacts), [grantleetheme](#port-grantleetheme),
[libgravatar](#port-libgravatar), [kidentitymanagement](#port-kidentitymanagement),
[libkleo](#port-libkleo), [kmailtransport](#port-kmailtransport), [kmbox](#port-kmbox),
[pimcommon](#port-pimcommon), [kpimtextedit](#port-kpimtextedit) and
[libkdepim](#port-libkdepim). The PIM ports therefore move together.

The viewer can load remote content in HTML mail, look links up with Google Safe Browsing and
verify DKIM signatures over DNS; all three (`htmlLoadExternal`, `CheckPhishingUrl`,
`EnabledDkim`) default to off, so nothing is fetched until the user turns them on in KMail's
settings.
