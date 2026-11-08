## About

Plasma Browser Integration is the native-messaging host behind the Plasma Integration browser
extension. With the extension installed, a browser's media playback appears in the media
controls and answers the media keys, its downloads appear in the notifications, its open tabs and
history are searchable in KRunner, its tabs are listed in the task manager's context menu, and a
page can be shared through [purpose](#port-purpose). The host is
`plasma-browser-integration-host`; the browser starts it from the manifest this package installs.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D INSTALL_CHROME_MANIFEST=OFF` — Install no Chromium extension policy. Turned on, it adds a
  file under `/usr/share/chromium/extensions` that makes Chromium download the extension from the
  Chrome Web Store at its next start. It is upstream's default, named because it is the one switch
  in the build that would reach the network.

## Notes

The manifest is installed once per browser family, each a copy of the same file naming the host:
`/usr/lib/mozilla/native-messaging-hosts/` for [firefox-esr](#port-firefox-esr), beside
[keepassxc](#port-keepassxc)'s; `/usr/lib/librewolf/native-messaging-hosts/` for
[librewolf](#port-librewolf); and `/etc/chromium/native-messaging-hosts/` for
[chromium](#port-chromium), plus the Chrome, Edge and Waterfox locations upstream always writes.
None of them does anything until the browser has the extension, so the package carries it: AMO's
signed build of the Plasma Integration extension, a second source, installed as
`distribution/extensions/plasma-browser-integration@kde.org.xpi` for
[firefox-esr](#port-firefox-esr) and [librewolf](#port-librewolf), which install it into every new
profile at its first start as an ordinary add-on the person can disable or remove. It is the signed
2.1, not the 2.2 in the tarball's `extension/` directory, because Firefox installs no unsigned
extension and `xpinstall.signatures.required` stays on; nothing reaches the store at run time.

The task manager's tab list goes through LibTaskManager from
[plasma-workspace](#port-plasma-workspace), and the KRunner tab and history search through the two
D-Bus runners installed under `/usr/share/krunner/dbusplugins`. The two kded modules it installs
are a reminder that offers the extension once a supported browser is used, and an integrator for
browsers installed as Flatpaks, which on KDOS finds none.
