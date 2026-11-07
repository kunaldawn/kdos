## About

libksieve is KDE's support for Sieve, the server-side mail filtering language (RFC 5228). It
has four libraries: `KPim6KSieve`, the script parser; `KPim6KManageSieve`, a ManageSieve
(RFC 5804) client that lists, uploads and activates scripts on a mail server over SASL;
`KPim6KSieveCore`, the account and capability handling; and `KPim6KSieveUi`, the script editor
with its syntax highlighting, the graphical rule builder and the vacation (out-of-office)
dialog. KMail's filter and vacation settings and
[pim-sieve-editor](#port-pim-sieve-editor) are built on it.

## Flags

- `-D OPTION_ADD_AUTOGENERATETEXT=OFF` — No AI text plugin in the script editor. With it, upstream
  links ktextaddons' `TextAutoGenerateText`, an LLM client, when it finds one; KDOS builds the
  PIM stack without it (see [ktextaddons](#port-ktextaddons)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextEditTextToSpeech=ON` — Requires ktextaddons' text-to-speech
  library, which the editor uses to read a script aloud. Upstream looks for it quietly and drops
  the feature without one, so a ktextaddons built without Qt TextToSpeech would change this
  library with no warning.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON` — No KDocTools. The tarball has no handbook of
  its own; upstream finds KDocTools only to build translated handbooks under `po/`, which the
  English-only rule would remove (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, for the
  same reason.

## Notes

The configure requires Qt WebEngine Widgets from [qt6-qtwebengine](#port-qt6-qtwebengine): the
editor's help view shows the Sieve reference pages in a web view. It also requires Sasl2, for
ManageSieve logins, through [cyrus-sasl](#port-cyrus-sasl).

Every KPim library it finds, [pimcommon](#port-pimcommon), [libkdepim](#port-libkdepim),
[kidentitymanagement](#port-kidentitymanagement), [kmailtransport](#port-kmailtransport) and
[kimap](#port-kimap), must be at least PIM 6.8.2, the version of Gear 26.08.2, so the PIM ports
move together. MIME parsing comes from KDE Frameworks' [kmime](#port-kmime).
