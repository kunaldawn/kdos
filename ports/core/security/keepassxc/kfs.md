## About

KeePassXC is a password manager for KeePass (`.kdbx`) databases. Besides passwords it keeps
one-time-password (TOTP) secrets, can hold SSH keys for a running SSH agent, fills logins into a
browser through the keepassxc-browser extension, and can serve its database as the desktop's
[Secret Service]{.idx}, the D-Bus API through which other programs store their own passwords. It
is a Qt 5 program. It also ships `keepassxc-cli`
and `keepassxc-proxy`, the browser extension's native-messaging helper.

## Flags

- `-D KEEPASSXC_BUILD_TYPE=Release` — the build type KeePassXC shows in its version string and
  about box (`Release`, `PreRelease` or `Snapshot`, which appends `-snapshot` to the version). The
  release tarball's `.version` file already makes it `Release`; the switch says the same.
- `-D WITH_TESTS=OFF` — no unit tests, which upstream builds by default; `-D WITH_GUI_TESTS=OFF`
  leaves out the GUI tests as well.
- `-D WITH_XC_DOCS=ON` — build the user guide, the getting-started guide, the keyboard-shortcut page and the
  manual pages with
  [asciidoctor](#port-asciidoctor), which is why it is in `depends`.
- `-D WITH_XC_UPDATECHECK=OFF` — no check for new KeePassXC releases at start-up; upstream's own
  description of the switch says to disable it for controlled distributions, where the package
  manager brings updates. CMake also turns it off whenever `WITH_XC_NETWORKING` is off.
- `-D WITH_XC_NETWORKING=OFF` — leave out all of KeePassXC's own network code: the downloader
  of website icons (favicons) for entries, the download of Have I Been Pwned breach data for the
  database report, and the update checker's code. This is upstream's default. Everything KeePassXC
  does is then local; browser integration is a local socket to `keepassxc-proxy`.
- `-D WITH_XC_FDOSECRETS=ON` — implement the freedesktop.org Secret Service on the session bus.
  Off by default upstream. The integration still has to be turned on in KeePassXC's own settings (see
  [A saved login needs KeePassXC running](../../../../docs/kdos/06-reference/known-gaps.md#a-saved-login-needs-keepassxc-running)).
- `-D WITH_XC_AUTOTYPE=ON` — Auto-Type, which types a username and password into another window.
  On Linux it is an X11 plugin, `keepassxc-autotype-xcb`, that needs libXi, libXtst and Qt 5's
  X11 extras, all in `depends`. KeePassXC loads the plugin named after Qt's platform, and there is
  no Wayland one, so in the KDOS session, where `QT_QPA_PLATFORM` puts Qt on Wayland first (see
  [Starting a session](../../../../docs/kdos/03-architecture/session.md#starting-a-session)),
  Auto-Type is unavailable. It works only in a KeePassXC started on Xwayland, with
  `QT_QPA_PLATFORM=xcb`, and then types only into X11 windows.
- `-D WITH_XC_X11=ON` — build the X11 helpers into the core and link libX11 and Qt 5 X11 Extras.
- `-D WITH_XC_BROWSER=ON` — browser integration through keepassxc-browser and `keepassxc-proxy`;
  `-D WITH_XC_BROWSER_PASSKEYS=ON` adds passkey (WebAuthn) support to it. Both are off by default
  upstream.
- `-D WITH_XC_SSHAGENT=ON` — add keys stored in a database to a running SSH agent while the
  database is open.
- `-D WITH_XC_KEESHARE=ON` — KeeShare, which synchronises groups of entries between databases
  through shared container files.
- `-D WITH_XC_YUBIKEY=ON` — challenge-response unlocking of a database with a YubiKey, reached over
  USB ([libusb](#port-libusb)) and smart-card readers ([pcsc-lite](#port-pcsc-lite)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Readline=ON` — `keepassxc-cli` links [readline](#port-readline)
  for its interactive shell only when configure finds it; required, a missing readline stops the
  configure instead of building a CLI with no line editing (see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Patches

- `app-id.patch` — In `share/linux/org.keepassxc.KeePassXC.desktop.in`, the template of the installed desktop entry, changes `StartupWMClass=keepassxc` to `StartupWMClass=org.keepassxc.KeePassXC`. KeePassXC sets its desktop file name to `org.keepassxc.KeePassXC.desktop`, which Qt's Wayland plugin turns into the app id `org.keepassxc.KeePassXC`; with the patch the entry's file name and its `StartupWMClass` both name that id, so the panel matches the window to the entry (see [`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)).

## Notes

`build.sh` deletes every translation catalogue under `/usr/share/keepassxc/translations` except the
English ones. kpkg's
[size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
removes the same catalogues from every package it makes, so the line does not change what ships.

It also writes `/usr/lib/mozilla/native-messaging-hosts/org.keepassxc.keepassxc_browser.json`,
the system-wide native-messaging manifest Firefox reads, which names `/usr/bin/keepassxc-proxy`
and allows only the keepassxc-browser extension. KeePassXC can write a per-user manifest from its
own browser-integration settings; the system one means Firefox finds the proxy without that step.
