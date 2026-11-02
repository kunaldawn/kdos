## About

libkcddb is KDE's CD database client: it identifies an audio CD from its table of contents and
fetches the album, artist and track names from a freedb-style CDDB server or from MusicBrainz,
with a local cache. [k3b](#port-k3b) names the tracks it rips through it. It also builds the CDDB
Retrieval page of System Settings, where the servers and the cache are set.

## Flags

- `-D BUILD_DOC=ON` — Builds the settings page's handbook through [kdoctools](#port-kdoctools).
  Upstream's default, named.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The MusicBrainz lookup is built only when the configure finds libmusicbrainz 5, and upstream
treats it as optional. The `grep` after the configure stops the build unless
`build/config-musicbrainz.h` defines `HAVE_MUSICBRAINZ5` as 1, so a missing
[libmusicbrainz5](#port-libmusicbrainz5) fails the build instead of leaving CDDB servers as the only
source.

The settings page, `kcm_cddb`, installs a hidden `kcm_cddb.desktop` that opens it in System
Settings, through which KRunner and the application launcher's search find it.
