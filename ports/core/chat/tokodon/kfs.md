## About

Tokodon is KDE's client for Mastodon and the other ActivityPub servers of the fediverse: home,
local and federated timelines, posting with media, polls and content warnings, notifications,
direct messages, and several accounts at once. It is a Kirigami application; account tokens go
to the Secret Service through [qtkeychain](#port-qtkeychain), and the streaming timelines arrive
over WebSockets from [qt6-qtwebsockets](#port-qt6-qtwebsockets).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KUnifiedPush=ON` — Push notifications through
  [kunifiedpush](#port-kunifiedpush), with [openssl](#port-openssl) for the keys a push
  subscription needs. Upstream looks for it quietly as optional and builds without push otherwise;
  required, a missing one stops the configure.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebView=ON` — Logging in inside the application, through
  [qt6-qtwebview](#port-qt6-qtwebview), instead of only in the web browser. Optional upstream.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure checks the QML modules the pages import, `org.kde.kitemmodels`,
`org.kde.notification`, `org.kde.kquickcontrolsaddons` (from [kdeclarative](#port-kdeclarative)),
`org.kde.prison` and `org.kde.sonnet`, and stops when one is missing. Qt Quick's Breeze style,
which upstream prefers on Android and Sailfish, is not looked for on a desktop build, which draws
with [qqc2-desktop-style](#port-qqc2-desktop-style). Push messages need a UnifiedPush distributor
running in the session.
