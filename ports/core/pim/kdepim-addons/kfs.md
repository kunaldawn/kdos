## About

kdepim-addons is the plugin set of the KDE PIM applications. For KMail's message viewer it adds
formatters for vCards, iCalendar invitations, TNEF attachments, Apple Wallet passes, travel
itineraries, Markdown and highlighted source, header styles, "create event" and "create to-do"
actions, a do-not-track header and an ad blocker; for the composer, editor tools, send checks and
grammar checking through LanguageTool and Grammalecte; plus plugins for KOrganizer, KAddressBook,
the Sieve editor, KMailTransport, KLDAP and the import wizard. Its `pimeventsplugin` is how the
Plasma Digital Clock shows Akonadi calendar events.

## Flags

- `-D OPTION_ADD_AUTOGENERATETEXT=OFF` — Build no AI plugins. On, the configure looks for
  ktextaddons' `KF6TextAutoGenerateText` and builds `pimautogeneratetext` and `kaichat-plugins`,
  which send mail text to network language-model services; KDOS builds the PIM stack without that
  client, as it does libksieve, kmail and pim-sieve-editor.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: Plasma activities support, from
  [pimcommon](#port-pimcommon)'s activities library and
  [plasma-activities](#port-plasma-activities). Both are optional to upstream, and without them
  the KMailTransport and KLDAP activity plugins, which tie a transport or an LDAP server to a Plasma
  activity, are left out. pimcommon must be built with its activities library for the first one to
  exist.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6ImportWizard=ON` — Make
  [akonadi-import-wizard](#port-akonadi-import-wizard) a requirement. Upstream finds it quietly
  and leaves out the importers for Evolution, Geary, Opera, The Bat!, Outlook Express and other
  mail programs without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6Itinerary=ON` — Make [kitinerary](#port-kitinerary) a
  requirement. It is optional to upstream, and the travel-reservation formatter is built only with
  it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Corrosion=ON` — Make [corrosion](#port-corrosion) a
  requirement. Upstream finds it as recommended and builds the message viewer's ad blocker, a Rust
  crate, only when it is found.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Discount=ON` — Make [discount](#port-discount) a requirement.
  It is recommended to upstream; without it the Markdown formatter falls back to Qt's simpler
  `QTextDocument` rendering.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The ad blocker is the `webengineviewer-adblock` crate under
`plugins/webengineurlinterceptor/adblock`, a Cargo workspace that Corrosion builds into a static
library with [rust](#port-rust). Its crates come from the port's vendor bundle, made by
`ports/fetch` from the workspace's `Cargo.lock` with `vendordir` set to that directory (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). `build.sh` unpacks the
bundle into the same directory, because Corrosion runs cargo with the workspace directory as its
working directory and cargo finds the bundle's `.cargo/config.toml` by walking up from there; set
anywhere else, every crate resolves as missing and the offline build fails. `RUSTFLAGS` keeps the
crate off a static C library, and `CARGO_NET_OFFLINE` stops cargo from trying the network. The
blocker is off by default and downloads its filter lists only once a user turns it on.

[gpgmepp](#port-gpgmepp) is a requirement, for the GnuPG Web Key Service formatter and the key
handling of KAddressBook's vCard import and editor pages, and Qt WebEngine for the message viewer
the plugins extend. The module sits at the top of the PIM stack: it needs messagelib,
mailcommon, kaddressbook, incidenceeditor, calendarsupport, libksieve, ktextaddons and the rest
built first, so it is one of the last PIM ports a build reaches. `BUILD_SCAM_SETTINGS_PLUGINS`, an
unfinished settings page, stays at its default, off, and `KDEPIMADDONS_BUILD_EXAMPLES` stays unset,
which leaves the external-script examples out.
