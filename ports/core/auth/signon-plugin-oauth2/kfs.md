## About

signon-plugin-oauth2 is the OAuth 1.0a and OAuth 2.0 authentication plugin for
[signond](#port-signond). signond loads `liboauth2plugin.so` from `/usr/lib/signon` when a client
asks for the `oauth2` method, and the plugin runs the browser authorisation, token exchange and
token refresh. KDE's kaccounts-providers set up their accounts through it.

## Flags

- `CONFIG+=nomake_tests` — upstream's switch that drops the `tests` subdirectory, whose test
  program would otherwise be built and installed.
