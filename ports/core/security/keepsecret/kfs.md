## About

KeepSecret is a window onto the Secret Service, the D-Bus store that libsecret and QtKeychain
applications keep their passwords in: it lists the collections, locks and unlocks them, and shows,
edits, adds and deletes the items in each. It holds no store of its own, so what it shows is
whatever provider owns `org.freedesktop.secrets` in the session. It is a Kirigami application.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`find_package(KF6KirigamiAppComponents REQUIRED)` makes
[kirigami-app-components](#port-kirigami-app-components) a requirement of the configure; it also
provides the `org.kde.kirigami.actioncollection` QML module the interface imports.
[libsecret](#port-libsecret) is required through pkg-config.

On KDOS the provider is ksecretd, from [kwallet](#port-kwallet), which is in `depends` for run
time; in a session where nothing owns the name the window opens on an empty list. KeePassXC's own
Secret Service integration stays off, so the two never race for the bus name.

The [qqc2-desktop-style](#port-qqc2-desktop-style) style is selected at start-up and imported at
run time, so `depends` is what installs it.
