## About

KWallet is KDE's password store, in three programs and a library. `KF6::Wallet` is the client API
KDE applications store passwords with: browser and mail logins, and the Wi-Fi and VPN secrets of
the Plasma network applet. `ksecretd` is the store itself. It keeps each wallet as an encrypted
file, `~/.local/share/kwalletd/<wallet>.kwl`, sealed with Blowfish under a key derived from the
wallet password by PBKDF2-SHA512 from [libgcrypt](#port-libgcrypt), or with a GnuPG key, and it
serves three D-Bus names: `org.freedesktop.secrets`, the freedesktop Secret Service that
[libsecret](#port-libsecret), QtKeychain and Python's keyring are clients of;
`org.kde.secretservicecompat`, the same interface under a name of KDE's own; and
`org.freedesktop.impl.portal.desktop.kwallet`, the secret back end xdg-desktop-portal hands to
sandboxed applications. `kwalletd6` keeps the older KWallet D-Bus API, `org.kde.kwalletd6`, alive
for every program that still speaks it, and turns each call into a Secret Service call to
ksecretd. `kwallet-query` reads and writes entries from a shell.

At a password login [kwallet-pam](#port-kwallet-pam) runs `ksecretd --pam-login` with a hash of
the login password, so the default wallet, `kdewallet`, opens with no prompt when it has the same
password. The program it starts is `KWALLETD_BIN_PATH` from the installed
`KF6WalletConfig.cmake`, which names `/usr/bin/ksecretd`. [kwalletmanager](#port-kwalletmanager)
is the window for browsing and editing wallets.

## Flags

- `-D BUILD_KSECRETD=ON` — Build ksecretd, its D-Bus activation files and its portal
  definition. Without it nothing on the system owns `org.kde.secretservicecompat`, which
  kwalletd6 sends every request to, so every wallet fails to open, and kwallet-pam executes a
  program that does not exist. Upstream's default; it is named so the store cannot drop out
  unnoticed.
- `-D BUILD_KWALLETD=ON` — Build kwalletd6 and its D-Bus activation files for
  `org.kde.kwalletd5` and `org.kde.kwalletd6`, the names `KF6::Wallet` and older clients call.
  Upstream's default.
- `-D BUILD_KWALLET_QUERY=ON` — Build `kwallet-query`, the command-line client, and its manual
  page. Upstream's default.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Gpgmepp=ON` — Make GnuPG wallets a requirement. Upstream looks
  for the C++ binding of GPGME quietly; found, the wallet back end can seal a wallet with a GnuPG
  key in place of a password, and ksecretd's new-wallet dialog offers the choice. The binding is
  [gpgmepp](#port-gpgmepp), over [gpgme](#port-gpgme); kwallet does not use the Qt binding.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Make [kdoctools](#port-kdoctools) a
  requirement. Upstream looks for it quietly and, without it, ships `kwallet-query` with no
  `kwallet-query(1)`, which it renders from DocBook.
- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.

## Notes

Where kwalletd6 sends its calls is `kwalletrc`, `[KSecretD] Enabled`. True, the default, means
ksecretd under `org.kde.secretservicecompat`; false means whichever program owns
`org.freedesktop.secrets`. The port ships no `kwalletrc`, so the default holds. A `KF6::Wallet`
client calls `org.kde.kwalletd6`, which D-Bus activates kwalletd6, whose first request in turn
activates ksecretd. `org.freedesktop.secrets` has no
activation file: ksecretd claims it when it starts, unless `kwalletrc` sets
`[org.freedesktop.secrets] apiEnabled=false`. A libsecret client such as `secret-tool` therefore
finds the store only once ksecretd is running, from kwallet-pam at login or from an earlier KWallet
request.

Only one program can own `org.freedesktop.secrets`. [keepassxc](#port-keepassxc) offers the same
name when its own Secret Service integration is turned on, and the program that starts second does
not get it; its clients then talk to the other one's store. KeePassXC's integration stays off for
ksecretd to keep the name.

With an automatic login no password passes through PAM, so kwallet-pam has nothing to open the
wallet with, and the first program that needs a secret brings up ksecretd's password prompt (or,
before any wallet exists, its first-use wizard).

The Secret Service protocol lets a client encrypt each secret in transit; ksecretd supports the
`plain` and `dh-ietf1024-sha256-aes128-cbc-pkcs7` session algorithms, and negotiates the second
through [qca](#port-qca), whose OpenSSL provider ships in the same package. ksecretd and the
library install no systemd unit. With `WITH_X11` at its default, on, ksecretd places its dialogs
over an X11 client's window when it runs on X11 itself, through
[kwindowsystem](#port-kwindowsystem)'s X11 half; in a Wayland session that code is not reached.
