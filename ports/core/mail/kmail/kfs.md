## About

KMail is KDE's email client. It reads and sends mail through the Akonadi accounts that
[kdepim-runtime](#port-kdepim-runtime) serves (IMAP, POP3, Maildir, mbox, Exchange), shows
messages with [messagelib](#port-messagelib)'s viewer on
[qt6-qtwebengine](#port-qt6-qtwebengine), signs and encrypts with OpenPGP and S/MIME through
[libkleo](#port-libkleo) and [gpgmepp](#port-gpgmepp), filters mail on the client and on the
server (Sieve, through [libksieve](#port-libksieve)), and searches it with
[akonadi-search](#port-akonadi-search). It runs on its own or as a page of
[kontact](#port-kontact). The port also builds KMail's Akonadi agents (send later, archive,
follow-up reminders, mail filtering, the unified mailbox and mail merge), the KTnef viewer for
Outlook's `winmail.dat` attachments, and `kmail-refresh-settings`, which resets KMail's
configuration.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the KMail, KTnef and agent handbooks are built
  only when [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the
  configure instead of quietly dropping them.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextEditTextToSpeech=ON` — reading a message aloud in the
  viewer and the composer, through the `TextEditTextToSpeech` library of
  [ktextaddons](#port-ktextaddons), which exists only when ktextaddons found
  [qt6-qtspeech](#port-qt6-qtspeech). Upstream looks for it quietly; required, a ktextaddons
  without speech stops the configure instead of dropping the action.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: identities, transports and folders can be
  tied to Plasma activities and offered only in them. Upstream searches for
  [pimcommon](#port-pimcommon)'s `PimCommonActivities` library quietly, and for
  [plasma-activities](#port-plasma-activities) only once that library is found, so either missing
  turns the feature off without a word. Both are required, and the configure stops when pimcommon
  is built without activities.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which KMail would
  submit usage statistics. It is not a port, and refusing the search keeps a stray copy in the
  build root from turning telemetry on.
- `-D OPTION_ADD_AUTOGENERATETEXT=OFF` — no AI text-generation plugin. With the option on, which
  is upstream's default, KMail looks for ktextaddons' `TextAutoGenerateText` library and, when it
  finds one, offers to draft and rewrite text through a network language-model service.

## Notes

Three ports are in `depends` for run time only. KMail runs
[kmail-account-wizard](#port-kmail-account-wizard) on its first start to set up an account, and
[akonadi-import-wizard](#port-akonadi-import-wizard) on its first start when it finds another mail
client's profile, Thunderbird's among them, and the user takes its offer to import it; it looks
both up on `PATH` and shows an error when one is missing. [kdepim-runtime](#port-kdepim-runtime)
holds the resource every account is. [gnupg](#port-gnupg) is in `depends` because gpgme runs `gpg`
and `gpgsm` for every signature and encryption. The other programs its menus open,
`kaddressbook`, `kleopatra` and `pimdataexporter`, are not named; the `kde-gear-pim` and
`kde-gear-utilities` metas install them, and a menu entry whose program is missing says so when
it is chosen.

[akonadi-search](#port-akonadi-search) is `REQUIRED` on every Unix unless
`FORCE_DISABLE_AKONADI_SEARCH` is set, which this recipe leaves off.
[qtkeychain](#port-qtkeychain) and ktextaddons' `TextAddonsWidgets` are marked `TYPE REQUIRED` for
the feature summary, which stops the configure without them.

KMail's composer, viewer and folder plugins (the message-viewer and editor add-ons, the Kontact
summary plugins) come from [kdepim-addons](#port-kdepim-addons), which this port does not name;
the `kde-gear-pim` meta installs it.

The main entry, `org.kde.kmail2.desktop`, claims `x-scheme-handler/mailto`, as Thunderbird's does;
the default for `mailto:` links is chosen in `mimeapps.list`, not by either entry (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
The Wayland app id is `org.kde.kmail2`, which `main.cpp` sets as the desktop file name and which
`KAboutData` keeps; the entry carries no `StartupWMClass`, so the task manager matches the window
to it with no change. `kmail_view.desktop`, which opens a saved message, and
`org.kde.kmail-refresh-settings.desktop` are `NoDisplay=true`.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbooks in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
