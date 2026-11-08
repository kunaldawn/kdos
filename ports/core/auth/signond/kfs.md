## About

signond is the accounts-sso single sign-on daemon. It keeps the credentials of the user's online
accounts and runs authentication plugins on behalf of client programs, which talk to it over the
session bus as `com.google.code.AccountsSSO.SingleSignOn`. The package holds the daemon, the
`signonpluginprocess` helper each plugin runs in, the password plugin, the client library
`libsignon-qt6` with its `SignOnQt6` CMake package, and the libraries plugins and extensions link.
[signon-plugin-oauth2](#port-signon-plugin-oauth2) is the plugin KDE's online accounts use.

## Flags

- `-after "SUBDIRS -= tests example test libsignon-qt-static.pro" "INSTALLS -= documentation"` —
  assignments qmake applies after reading each project file. They drop the test suite, the
  `example` and `test` authentication plugins, which would otherwise install into
  `/usr/lib/signon` beside the real ones, and the static copy of `libsignon-qt6`, which nothing
  links; the last drops the documentation target, which runs doxygen during `make install`.

## Notes

The daemon is started by D-Bus activation: the install puts
`com.google.code.AccountsSSO.SingleSignOn.service` and the backup service in
`/usr/share/dbus-1/services`, each running `/usr/bin/signond`. Credentials go to an unencrypted
SQLite database under `$XDG_CONFIG_HOME/signond`;
[signon-kwallet-extension](#port-signon-kwallet-extension) moves the secrets into KWallet.

`/etc/signond.conf` is upstream's file. Its active `[ObjectTimeouts]` drop an idle identity or
authentication session after 30 seconds and stop the daemon after 5 idle seconds, where the
built-in values are 300, 300 and never; D-Bus activation starts it again on the next call. Its
`[SecureStorage]` keys configure the cryptsetup extension, which is not built, and are unused.
