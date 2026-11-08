## About

kaccounts-integration is KDE's front end to the accounts-sso stack. `libkaccounts` and the QML
module `org.kde.kaccounts` list, create and remove online accounts and hand out their
credentials; `kcm_kaccounts` is System Settings → Online Accounts; the kded module `accounts`
tells plugins when an account appears or goes away; and its one plugin, `kio-webdav`, adds a
Nextcloud or ownCloud account's files to the Places panel as a `webdav:` location. The providers
themselves (Google, Nextcloud, ownCloud) come from [kaccounts-providers](#port-kaccounts-providers).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Accounts are stored by [libaccounts-qt](#port-libaccounts-qt), over libaccounts-glib, and
credentials are held by [signond](#port-signond), which the configure requires as `SignOnQt6`.
signond runs on the session bus, started by D-Bus activation; it keeps passwords and tokens in
KWallet through [signon-kwallet-extension](#port-signon-kwallet-extension) and signs in to OAuth 2
providers through [signon-plugin-oauth2](#port-signon-plugin-oauth2), both in `depends` for run
time. The settings page's asynchronous calls are written with [qcoro](#port-qcoro), a requirement.

The kded module is loaded by the session's kded6, and the settings page is opened in System
Settings; neither is started by this port.
