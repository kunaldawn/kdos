## About
[Elisa]{.idx} is KDE's music player for a local collection: it indexes the music on the machine,
browses it by album, artist, genre and folder, and plays it through libVLC from
[vlc](#port-vlc). Its interface is written in QML with Kirigami.

## Flags
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LIBVLC=ON` — stop the configure if libVLC is missing. Upstream
  marks it recommended and plays through QtMultimedia without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6DBus=ON` — this switch and the next four,
  `CMAKE_REQUIRE_FIND_PACKAGE_KF6DBusAddons`, `KF6Crash`, `KF6FileMetaData` and `KF6XmlGui`, make
  required what upstream marks optional or recommended: the MPRIS interface on D-Bus, which lets
  the panel and media keys control playback, crash handling, tag reading through KFileMetaData,
  and the XmlGui shortcuts dialog. Each would otherwise be dropped without an error when missing.
- `-D CMAKE_DISABLE_FIND_PACKAGE_UPNPQT=ON` — never look for UPnP support. Upstream describes it
  as broken, and its library, UPnP-Qt, is not a port.

## Notes
The radio list Elisa ships points at internet streams, so it plays nothing offline; the
collection itself is local.

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.elisa`, the desktop file name KAboutData derives and so the Wayland app id, where
upstream's says `elisa` (see
[`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)). It claims no MIME
type: [strawberry](#port-strawberry) is the music player that audio files open in, in
`/etc/xdg/kdos-mimeapps.list`.
