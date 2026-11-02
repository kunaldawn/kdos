## About

JuK is KDE's jukebox: a collection list of every track it has been pointed at, playlists, search
playlists that fill themselves from a query, a tag editor, a file renamer that builds names from
tags, and cover art. It plays through Qt Multimedia, so through
[qt6-qtmultimedia](#port-qt6-qtmultimedia)'s FFmpeg backend, and reads and writes tags with
[taglib](#port-taglib).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every framework JuK uses, KDocTools for its handbook among them, is a required component of one
`find_package(KF6 … REQUIRED)` call, and Taglib is `REQUIRED` beside it, so there is nothing
optional to pin.

Three things JuK asks for at run time come from the Plasma session rather than from `depends`:
its global play, pause and skip shortcuts are registered with kglobalacceld, the tray icon needs
a StatusNotifierItem host, and while it plays it holds a sleep inhibition through PowerDevil's
`org.kde.Solid.PowerManagement`. Outside that session each one is skipped without an error. The
Last.fm scrobbler keeps its credentials in [kwallet](#port-kwallet).
