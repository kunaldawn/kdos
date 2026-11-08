## About

Konversation is KDE's IRC client: several networks and channels in tabs, TLS, SASL, DCC file
transfers and chats, nick completion, highlights and notifications, and Blowfish-encrypted
channels.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Builds the handbook through [kdoctools](#port-kdoctools). Upstream's
  default, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt6=ON` — Requires [qca](#port-qca), on which the Blowfish
  channel encryption and the DH1080 key exchange are built. Upstream looks for it as optional and
  leaves encryption out without it.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.konversation`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
It claims the `irc:` and `ircs:` link schemes and adds `SingleMainWindow=true`.
