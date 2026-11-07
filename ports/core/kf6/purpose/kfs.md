## About

Purpose is KDE's share framework: the Share menu of [dolphin](#port-dolphin),
[gwenview](#port-gwenview), [okular](#port-okular) and other KDE applications, and the share
dialog Qt Quick applications open. A program asks for the targets that take a given MIME type, and
Purpose lists every share plugin whose metadata accepts it and runs the one picked, showing that
plugin's configuration page (a device, a recipient, a file name) first. The port also installs the
Share entry of Dolphin's context menu. The targets it ships act on this machine or on a device the
user owns: save as, copy to the clipboard, a mail draft in the default mail program, a Telegram
chat, a QR code, a phone or SMS through [kdeconnect](#port-kdeconnect), and Bluetooth.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON` — No online-accounts integration. With
  KAccounts found, the YouTube and Nextcloud targets take their account from it, and the
  `org.kde.purpose.accountshelper` QML module lists its accounts whenever no KOnlineAccounts
  service is on the session bus; without it the module lists accounts only from that service.
  [kaccounts-integration](#port-kaccounts-integration)
  belongs to the accounts stack, which ships later, and both targets are removed from this package
  in any case.
- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.

## Notes

Upstream builds every target it has and offers no switch for one, so the five that upload the file
to an online service (imgur, YouTube, Nextcloud, Review Board and Phabricator) are deleted after
the install: each target's plugin, its configuration page, its icon and its QML directory. A target
whose plugin is not found stops the build, so a renamed plugin cannot keep its target in the menu
without anyone noticing; the loop's list is the one place to change when a target is added or
dropped.

The Bluetooth target stays. Its device list imports `org.kde.bluezqt` from
[bluez-qt](#port-bluez-qt), which is in `depends`, and sending runs `bluedevil-sendfile` from
[bluedevil](#port-bluedevil). bluedevil is part of the Plasma session, which a 43_toolkits port
cannot name in `depends`; the image installs it with the rest of the session, and where it is
missing the target is listed and the send fails. The two KDE Connect targets reach
[kdeconnect](#port-kdeconnect) at run time in the same way: the device target lists devices from
the KDE Connect daemon, `org.kde.kdeconnect` on the session bus, and sends through it, and the SMS
target starts `kdeconnect-sms`. The Telegram target starts Telegram Desktop through its desktop
entry, failing with "Could not find telegram" where none is installed.
