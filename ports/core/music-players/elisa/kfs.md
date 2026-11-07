## About
[Elisa]{.idx} is KDE's music player for a local collection: it indexes the music on the machine,
browses it by album, artist, genre and folder, and plays it through libVLC from
[vlc](#port-vlc). Its interface is written in QML with Kirigami.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LIBVLC=ON` — stop the configure if libVLC is missing. Upstream
  marks it recommended and plays through QtMultimedia without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6DBus=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DBusAddons=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FileMetaData=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6XmlGui=ON`: make required what upstream marks optional or
  recommended: the MPRIS interface on D-Bus, which lets
  the panel and media keys control playback, crash handling, tag reading through KFileMetaData,
  and the XmlGui shortcuts dialog. Each would otherwise be dropped without an error when missing.
- `-D CMAKE_DISABLE_FIND_PACKAGE_UPNPQT=ON` — never look for UPnP support. Upstream describes it
  as broken, and its library, UPnP-Qt, is not a port.

## Notes
The radio list Elisa ships points at internet streams, so it plays nothing offline; the
collection itself is local.

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.elisa`, the desktop file name KAboutData derives and so the Wayland app id, where
upstream's says `elisa`, so the task manager can match the window to its entry. It keeps
upstream's `MimeType=` list of audio and playlist types, which is how Elisa is offered for audio
files and can be the default music player the shipped MIME associations name;
[strawberry](#port-strawberry) claims the same types beside it.
