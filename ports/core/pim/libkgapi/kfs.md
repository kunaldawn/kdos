## About

LibKGAPI is KDE's client for Google's web APIs: Calendar, People (contacts), Drive, Tasks,
Blogger, Latitude and Static Maps, each as its own `KPim6GAPI*` library of jobs and data types,
over one OAuth 2 account object. Accounts and their tokens are kept in KWallet. The Google
calendar and contacts resources of [kdepim-runtime](#port-kdepim-runtime) and
[kio-gdrive](#port-kio-gdrive) are built on it.

## Flags

- `-D BUILD_SASL_PLUGIN=ON` — Builds `libkdexoauth2.so`, a Cyrus SASL plugin for the `XOAUTH2`
  mechanism, installed into `/usr/lib/sasl2` beside [cyrus-sasl](#port-cyrus-sasl)'s own. KIMAP
  and KSMTP authenticate to Gmail through it. Upstream's default is the same; the switch exists
  only for co-installing two versions, and is pinned so the plugin always ships. The configure
  then requires Sasl2.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile, and which the package-wide English-only rule
  would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every call is a request to Google's servers, made only for an account the user has added. The
OAuth client identity is KDE's, registered with Google; whether Google keeps accepting it is not
something the port controls. Tokens are stored through [kwallet](#port-kwallet), so with the
session's secret service locked the account asks for the wallet first.
