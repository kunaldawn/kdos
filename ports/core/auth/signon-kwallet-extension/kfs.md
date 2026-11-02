## About

signon-kwallet-extension is a storage plugin for signond, the Accounts-SSO daemon behind KAccounts
online accounts. With it, signond keeps the tokens and passwords of those accounts in KWallet
instead of its own unencrypted database.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as every KDE recipe passes it. The module has no `po/`
  directory and calls no `ki18n_install()`, so CMake reports the variable as unused.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The plugin is found through a bundled `FindSignOnExtension.cmake`, which asks pkg-config
([pkgconf](#port-pkgconf)) for [signond](#port-signond)'s `SignOnExtension` module and installs
`libkeyring-kwallet.so` into the directory that module's `plugindir` variable names,
`/usr/lib/signon/extensions`. A signond that names another directory moves the plugin with it.

At run time the plugin talks to the KWallet secret service from [kwallet](#port-kwallet); with
none running, signond falls back to its own storage.
