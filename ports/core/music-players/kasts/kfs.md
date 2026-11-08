## About

Kasts is KDE's podcast player, a Kirigami application from the convergent set: it subscribes to
RSS and Atom feeds, keeps a queue of episodes, downloads them for offline listening, shows chapter
marks and resumes each episode where it stopped. Feeds are parsed with
[syndication](#port-syndication), and playback goes through its own `KMediaSession` library,
which also publishes an MPRIS player so the panel and the media keys can drive it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_LIBVLC=ON` — Make libVLC, from [vlc](#port-vlc), a requirement.
  Upstream marks it recommended and drops the VLC backend without it; when it is built,
  `KMediaSession` picks it as the default backend.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6BreezeIcons=ON` — Make the Breeze icon library from
  [breeze-icons](#port-breeze-icons) a requirement. Upstream looks for it quietly and, when it is
  found, links it and registers its icons at start-up, so the interface has its icons even under
  an icon theme that lacks them.
- `-D BUILD_GSTREAMER_BACKEND=ON` — Build the GStreamer backend, which upstream turns on by
  default but compiles only when `pkg_check_modules` finds `gstreamer-1.0`. There is no
  requirement switch for a pkg-config check, so the `grep` of `build/CMakeCache.txt` after the
  configure fails the build when [gstreamer](#port-gstreamer) was not found, instead of
  shipping without the backend.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` changes `StartupWMClass=kasts` to `StartupWMClass=org.kde.kasts` in upstream's
desktop entry. Kasts takes its Wayland app_id from KAboutData, which derives `org.kde.kasts`, the
desktop file name; upstream's shorter class matches no window Kasts opens.

## Notes

The three backends are Qt Multimedia, always built, libVLC and GStreamer, and the settings page
switches between them. The GStreamer backend plays through `playbin` and slows or speeds speech
with `scaletempo`, from [gst-plugins-base](#port-gst-plugins-base) and
[gst-plugins-good](#port-gst-plugins-good), both in `depends` for run time.

The password for a gpodder.net or Nextcloud sync account is stored through
[qtkeychain](#port-qtkeychain), which reaches the Secret Service; on KDOS that is ksecretd, from
[kwallet](#port-kwallet). Feeds and episodes come from the network only when the person refreshes
or downloads: upstream's defaults leave the refresh at start-up and the timed refresh off.
