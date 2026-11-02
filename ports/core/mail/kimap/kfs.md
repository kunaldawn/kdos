## About

KIMAP is the IMAP client library of KDE PIM: login, folder listing, fetch, search, append, IDLE,
quotas, ACLs and the other commands of IMAP4rev1 and its common extensions, as asynchronous
`KJob`s over a session. The Akonadi IMAP resource in kdepim-runtime and KMail's server tests use
it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Authentication goes through [cyrus-sasl](#port-cyrus-sasl), which the configure requires. The
mechanisms a login can use are the SASL plugins installed at run time; `XOAUTH2`, which Gmail and
Outlook accounts need, is the plugin of [cyrus-sasl-xoauth2](#port-cyrus-sasl-xoauth2) or the one
libkgapi installs.
