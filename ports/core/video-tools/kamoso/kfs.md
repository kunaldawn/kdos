## About

Kamoso is KDE's webcam booth: it takes pictures, bursts and videos from a camera through
[gstreamer](#port-gstreamer) and [pipewire](#port-pipewire), with effects, and shares them through
[purpose](#port-purpose).

## Flags

- `-D BUILD_DOC=ON` — Builds the handbook through [kdoctools](#port-kdoctools). Upstream's
  default, named.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D PATCHED_GSTREAMER=OFF` — Asks for GStreamer 1.26.3 or later, the first release with the
  camera-source change Kamoso relies on, instead of an older GStreamer carrying that change as a
  patch. Upstream's default, named; [gstreamer](#port-gstreamer) is newer.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.kamoso`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
