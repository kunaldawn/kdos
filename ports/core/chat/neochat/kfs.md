## About

NeoChat is KDE's Matrix client: rooms and direct chats with end-to-end encryption, spaces,
threads, replies and reactions, media and file sharing, and room search from KRunner. It is a
Kirigami application over [libquotient](#port-libquotient), which speaks the Matrix
client-server API and does the encryption. Login pages a homeserver serves as a web page open in
an embedded view from [qt6-qtwebview](#port-qt6-qtwebview), which upstream requires. Access
tokens are kept in the Secret Service.

## Flags

- `-D WITH_UNIFIEDPUSH=ON` — Push notifications through [kunifiedpush](#port-kunifiedpush), which
  is then a requirement, and the D-Bus service file that lets a push message start NeoChat. On by
  default upstream; named so it stays on.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Much of what the window needs is QML, loaded when a page opens rather than linked:
`org.kde.kquickimageeditor` from [kquickimageeditor](#port-kquickimageeditor) for editing an
image before sending it, `org.kde.prison` and its scanner from [prison](#port-prison) for QR
login, `org.kde.quickcharts` from [kquickcharts](#port-kquickcharts), `QtLocation` and
`QtPositioning` for shared locations, and the desktop style from
[qqc2-desktop-style](#port-qqc2-desktop-style). The configure only reports most of them, so each is
named in `depends`.

Push messages need a UnifiedPush distributor running in the session; without one NeoChat syncs
only while it runs. The KRunner plugin is a D-Bus runner file under
`/usr/share/krunner/dbusplugins`, answered by NeoChat itself while it runs.
