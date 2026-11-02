## About

Keysmith generates one-time passwords for two-factor sign-in: time-based (TOTP) and counter-based
(HOTP) codes, added by hand, from an `otpauth://` link or by scanning the QR code a website shows.
The accounts are kept encrypted with [libsodium](#port-libsodium) under a password asked for when
the application opens. It is a Kirigami application from KDE's convergent set.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`ecm_find_qmlmodule(org.kde.prison.scanner REQUIRED)` makes the QR scanner of
[prison](#port-prison) a requirement of the configure; it reads the camera through
[qt6-qtmultimedia](#port-qt6-qtmultimedia). libsodium is the system copy: upstream builds its own
only when `BUILD_EXTERNAL` is set, which this recipe never sets. The D-Bus interface, with
[kdbusaddons](#port-kdbusaddons) and [kwindowsystem](#port-kwindowsystem), is built whenever the
target is neither Android nor Haiku; it makes Keysmith a single instance, so an `otpauth://` link
opened while it runs reaches the running copy.

[qqc2-desktop-style](#port-qqc2-desktop-style) is `REQUIRED` by the configure, though upstream
marks it `TYPE RUNTIME`: the program links nothing of it and selects the `org.kde.desktop` Qt
Quick style at start-up.
