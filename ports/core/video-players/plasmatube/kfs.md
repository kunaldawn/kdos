## About

PlasmaTube is a Kirigami client for online video: it browses and searches YouTube through an
Invidious or Piped instance, and PeerTube instances directly, keeps subscriptions, watch history
and playlists, and plays videos in its own window through [mpvqt](#port-mpvqt), a Qt Quick item
over [mpv](#port-mpv)'s library. Account credentials go to the Secret Service through
[qtkeychain](#port-qtkeychain).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

mpv resolves a YouTube address into streams through its `ytdl` hook, which runs
[yt-dlp](#port-yt-dlp); without it nothing plays. yt-dlp is in `depends` for run time, as are the
QML modules the pages import that the configure does not check: `org.kde.kquickcontrolsaddons`
from [kdeclarative](#port-kdeclarative), `org.kde.kitemmodels` from
[kitemmodels](#port-kitemmodels) and `org.kde.notification` from
[knotifications](#port-knotifications).

Every page is a request to a third-party instance chosen in the settings; public instances come
and go, and one that stops answering leaves the feeds empty until another is picked.
