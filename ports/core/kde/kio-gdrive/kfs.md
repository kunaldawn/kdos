## About

kio-gdrive is the `gdrive:/` KIO worker. Dolphin and every other KIO program browse, open, upload
to and delete from a Google Drive account through it. It also installs a Purpose share plugin that
uploads a file to Drive, a context menu action that copies a file's Drive link, and a file
properties page with Drive's own metadata.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON` — Keep accounts in the worker's own store rather
  than in KAccounts. With KAccounts found, the worker takes Google credentials only from System
  Settings' Online Accounts, which needs the whole Accounts-SSO stack (signond, libaccounts-qt,
  kaccounts-integration) and signon-ui, which is not a port, to add an account at all. Without it
  the configure requires QtKeychain: the worker signs in through Google's page in the web browser,
  which libkgapi's `AuthJob` opens, and stores the token through [qtkeychain](#port-qtkeychain),
  built here against libsecret, so it lands in the Secret Service that ksecretd provides in the
  Plasma session.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON` — Make [purpose](#port-purpose) a requirement.
  Upstream finds it as recommended and drops the share plugin without it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The Drive API calls go through [libkgapi](#port-libkgapi), a requirement of the configure. This is
a network service: nothing reaches Google until the user opens `gdrive:/` and signs in.

`kdoctools_install(po)` still builds the handbook in every translated language; kpkg's language rule
removes `/usr/share/doc/HTML/<lang>/` from the package.
