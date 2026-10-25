## About
[Kdenlive]{.idx} is KDE's multi-track video editor. The timeline, rendering and playback are
[MLT](#port-mlt)'s, and the effects, transitions and generators it offers are MLT's own and
[frei0r](#port-frei0r-plugins)'s, found at run time; [ffmpeg](#port-ffmpeg) does the decoding
and encoding underneath. [OpenTimelineIO](#port-opentimelineio) imports and exports timelines
from other editors.

## Flags
- `-D RELEASE_BUILD=ON` — Leave the Git revision out of the program's version string.
  Upstream's default, named.
- `-D USE_DBUS=ON` — Build the components that use D-Bus. Upstream's default on Linux, named.
- `-D CRASH_AUTO_TEST=OFF` — Upstream's default, named: on, Kdenlive generates test cases from
  some crashes through the RTTR library, and downloads and builds RTTR's source when no RTTR is
  installed.
- `-D BUILD_QCH=OFF` — With `-D BUILD_FUZZING=OFF` and `-D BUILD_DESIGNERPLUGIN=OFF`: no
  API documentation in Qt Help format, no fuzzing target and no Qt Designer plugin. All three are
  upstream's defaults, named.

## Notes
- **Features that download.** Speech-to-text and object masking build a Python virtual
  environment with pip and download their models when first asked for. Without a network both
  report the missing module and the editor goes on without them; the online resource providers
  and the KNewStuff downloads fail the same way.
- **The menu entry is replaced** so that `StartupWMClass` is `org.kde.kdenlive`, the desktop file
  name Kdenlive sets and its Wayland app_id. It claims only `application/x-kdenlive`, its own
  project type; the media types belong to the players and viewers.
