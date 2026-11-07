## About

KLDAP is KDE PIM's LDAP client: `KPim6LdapCore` holds the connection, search and LDIF code over
OpenLDAP, `KPim6LdapWidgets` the server configuration dialog and the address-book lookup KMail's
recipient completion and KAddressBook use, and the `ldap://` and `ldaps://` KIO worker lets any
KDE program browse a directory.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Ldap=ON` — Make [openldap](#port-openldap) a requirement.
  Upstream finds it as recommended, and without it every LDAP call is compiled out: the libraries
  and the worker build, and none of them can reach a server.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The `ldap` worker's handbook page, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

A bind password the person asks to keep is stored through [qtkeychain](#port-qtkeychain), which
the configure requires; on KDOS it lands in KWallet's Secret Service. SASL binds go through
[cyrus-sasl](#port-cyrus-sasl), also required.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
