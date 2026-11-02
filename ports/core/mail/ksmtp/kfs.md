## About

KSMTP is the SMTP client library of KDE PIM: a session over plain TCP, STARTTLS or TLS, with
`KJob`s for login and for sending one message. Authentication goes through Cyrus SASL, so every
mechanism a SASL plugin provides is offered, `XOAUTH2` included where a plugin adds it.
[kmailtransport](#port-kmailtransport) sends mail through it, and so KMail and Akonadi's outgoing
mail agent do.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Upstream calls `find_package(Sasl2)` and then sets its type to `REQUIRED`, so the configure stops
without [cyrus-sasl](#port-cyrus-sasl) and no switch is needed. The configure also requires
KF6KIO and the library links `KIOCore`, so [kio](#port-kio) is in `depends`; the SSL-error
prompt itself is left to the application, through the `SessionUiProxy` it installs.
