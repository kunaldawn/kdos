## About

KMailTransport is the KDE PIM library that keeps the list of outgoing mail servers and sends a
message through one of them. It holds the transport settings, the job that hands a message to a
transport, the configuration dialogs, and an SMTP plugin built on [ksmtp](#port-ksmtp) that also
signs in to Gmail and Outlook accounts over OAuth (Gmail's tokens through
[libkgapi](#port-libkgapi)). [kmail](#port-kmail), [kdepim-runtime](#port-kdepim-runtime)'s mail
dispatcher, [korganizer](#port-korganizer)'s invitations, [kalarm](#port-kalarm)'s email alarms and
the other PIM modules that send mail link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

Transport passwords are stored through [qtkeychain](#port-qtkeychain). Upstream finds it without
`REQUIRED` but marks it `TYPE REQUIRED`, and `ecm_feature_summary(… FATAL_ON_MISSING_REQUIRED_PACKAGES)`
stops the configure when it is missing, so no flag is needed. qtkeychain stores secrets through
the freedesktop Secret Service, which the session's `ksecretd` from [kwallet](#port-kwallet)
serves. When the Secret Service cannot take a password, the library offers to keep it in the
transport's configuration file, obscured but not encrypted.

The library links `KF6::Wallet` only when some other package has already defined that target, and
nothing this configure finds does, so the build has no KWallet link and needs no
[kwallet](#port-kwallet) at build time.
