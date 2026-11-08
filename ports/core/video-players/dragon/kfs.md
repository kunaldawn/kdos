## About

Dragon Player is KDE's simple video player: one window, a play button, a seek bar, subtitles and
audio-track choice, DVDs and streams, and nothing that is not about playing the one video. It is
a Kirigami application that plays through Qt Multimedia, so through
[qt6-qtmultimedia](#port-qt6-qtmultimedia)'s FFmpeg backend. [haruna](#port-haruna) is the
video player the desktop opens by default; Dragon is installed beside it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` changes `StartupWMClass=dragonplayer` to `StartupWMClass=org.kde.dragonplayer` in
upstream's desktop entry. Dragon sets its desktop file name, and so its Wayland app_id, to
`org.kde.dragonplayer`; upstream's shorter class matches no window Dragon opens.

## Notes

`pkg_check_modules(libavcodec REQUIRED)` links libavcodec from [ffmpeg](#port-ffmpeg) directly,
for one check: whether FFmpeg has an H.264 decoder, without which the player warns that common
videos will not play.

A remote URL other than `http:` and `https:` is turned into a local path through KIO's FUSE
bridge, [kio-fuse](#port-kio-fuse), which is in `depends` for run time; without it an `sftp:` or
`smb:` video does not open.
