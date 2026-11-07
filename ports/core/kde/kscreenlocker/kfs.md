## About

KScreenLocker is the Plasma lock screen. `libKScreenLocker` is the locking side, which
[kwin](#port-kwin) links: it locks after the idle time set in `kscreenlockerrc`, when the session
is told to (`org.freedesktop.ScreenSaver.Lock`, `loginctl lock-session`, the lock shortcut), and
before the machine sleeps, and it keeps the session locked until the greeter reports success. The
greeter, `/usr/lib/libexec/kscreenlocker_greet`, draws the unlock screen over every output from
the lock-screen QML of the current Plasma look-and-feel package, falling back to a plain theme
compiled into it. It checks each attempt through `kscreenlocker_worker`, a helper that runs one PAM
conversation and runs as the user, not as root. The Screen Locking page of System Settings
(`kcm_screenlocker`) is also part of the port.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D KSCREENLOCKER_PAM_PASSWORD_SERVICE=kde` — The PAM service a password is checked against,
  `/etc/pam.d/kde`. It is upstream's name, named here because `build.sh` writes the service file
  under that name, so the two must agree; an unlock against a service with no file falls
  through to `/etc/pam.d/other`, which denies everything, and nobody can unlock. The password
  service cannot be turned off.
- `-D KSCREENLOCKER_PAM_FINGERPRINT_SERVICE=kde-fingerprint` — The service for unlocking with a
  fingerprint, `/etc/pam.d/kde-fingerprint`, whose stack uses `pam_fprintd.so` from
  [fprintd](#port-fprintd). The greeter offers it only once
  `kscreenlockerrc [Authenticators] Fingerprint=true` is set.
- `-D KSCREENLOCKER_PAM_SMARTCARD_SERVICE=disabled` — Compiles smart card unlocking out, with the
  value kscreenlocker's own cache help names for turning an authenticator off. It needs a PAM
  module for smart cards, such as `pam_pkcs11`, and none is a port; built in, turning it on in
  `kscreenlockerrc [Authenticators]` would offer an unlock method whose PAM service does not exist,
  which `/etc/pam.d/other` refuses.
- `-D KSCREENLOCKER_PAM_FACE_SERVICE=disabled` — Compiles face unlocking out. It needs a PAM module
  for face recognition, such as Howdy's, and none is a port.
- `-D KSCREENLOCKER_PAM_UNIVERSAL2FACTOR_SERVICE=disabled` — Compiles unlocking with a U2F or FIDO2
  key out. It needs `pam_u2f`, which is not a port.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_loginctl=ON` — Makes `loginctl`, from [elogind](#port-elogind),
  a requirement of the configure. The window a crashed greeter leaves tells the user to run
  `loginctl unlock-session` from another virtual terminal. Upstream looks for `loginctl` only as a
  run-time package, and when it finds none falls back to looking for ConsoleKit, which is not a
  port; with the flag the configure stops in a build root without elogind instead.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`build.sh` writes the lock screen's two PAM stacks, `/etc/pam.d/kde` and
`/etc/pam.d/kde-fingerprint`. kscreenlocker runs only the auth phase, `pam_authenticate` and then
`pam_setcred` with `PAM_REFRESH_CRED`, and never account or session, so `kde` is `system-auth`'s
auth lines and nothing else that would run. `pam_kwallet5` is not in it: its auth phase only keeps
the password for a session phase, and a lock screen has none, so an unlock opens no wallet.
`pam_unix` checks the password through the setuid `unix_chkpwd` that [pam](#port-pam) installs,
which is how a worker running as the user reads `/etc/shadow`. `system-auth` carries `nullok`, so
an account with no password, such as the live medium's, unlocks with an empty field.
`kde-fingerprint` is the fingerprint authenticator's service, used only when it is turned on in
`kscreenlockerrc [Authenticators] Fingerprint`; `pam_fprintd` is `required`, so a reader that is
absent or a finger that does not match fails the attempt rather than falling through. It names
`pam_fprintd.so`, which [fprintd](#port-fprintd) installs; without fprintd the stack fails closed,
so fprintd is not in `depends`.

The session is tracked by [elogind](#port-elogind) over `org.freedesktop.login1`: the locker
listens for the session's `Lock` and `Unlock` signals and for `PrepareForSleep`, and holds a sleep
delay inhibitor so that the lock is drawn before the machine suspends. If the greeter crashes, the
locker keeps the session locked behind a black window that tells the user to switch to another
virtual terminal, log in and run `loginctl unlock-session <id>`.

The fallback theme imports `org.kde.plasma.private.sessions` from plasma-workspace, which also
provides the look-and-feel package whose lock screen the greeter normally draws; plasma-workspace
depends on this port, so it is not named in `depends`. The other QML modules the fallback theme
and the settings page import at run time come from [kirigami](#port-kirigami),
[kdeclarative](#port-kdeclarative) (`org.kde.kquickcontrols` and `org.kde.kquickcontrolsaddons`),
[kcmutils](#port-kcmutils), [ksvg](#port-ksvg) and [libplasma](#port-libplasma).
