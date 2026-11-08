## About

kaccounts-providers holds the provider and service definitions, and the setup pages, that
KAccounts offers in System Settings → Online Accounts: Google, ownCloud and Nextcloud. A
provider describes how to sign in to an account; a service names what a program may use it for,
such as storage or contacts.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineQuick=ON` — Make Qt WebEngine's Quick module, from
  [qt6-qtwebengine](#port-qt6-qtwebengine), a requirement. Upstream finds it as optional and
  leaves out the Nextcloud provider, its services and its login page without it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The definitions are written with `kaccounts_add_provider()` and `kaccounts_add_service()` from
[kaccounts-integration](#port-kaccounts-integration), a requirement, which install them where
libaccounts-glib reads them. [qcoro](#port-qcoro) is required for the Nextcloud page's
asynchronous calls.

Signing in to Google goes through signond's OAuth 2 plugin, [signon-plugin-oauth2](#port-signon-plugin-oauth2),
and signond shows the provider's web page through [signon-ui](#port-signon-ui); both are in
`depends` for run time, since without either a Google account cannot be added. The
`webkit-options.d` files the package installs under `/etc/signon-ui` are signon-ui's per-site
settings.
