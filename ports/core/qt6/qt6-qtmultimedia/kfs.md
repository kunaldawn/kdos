## About
Qt Multimedia for Qt 6: audio and video playback, recording, camera and screen capture, and audio
devices, as the QtMultimedia library, its Qt Quick types and the Qt Spatial Audio module. Media
decoding and encoding go through an FFmpeg plugin; audio reaches the sound server through
PipeWire.

## Flags
- `-DFEATURE_ffmpeg=ON` — with `-DFEATURE_gstreamer=OFF`: build the FFmpeg media plugin, through
  [ffmpeg](#port-ffmpeg), and not the GStreamer one. [gstreamer](#port-gstreamer) is installed in
  an earlier phase and is not in `depends`, so without the second switch the GStreamer plugin
  would be built whenever it is found.
- `-DFEATURE_pipewire=ON` — PipeWire audio devices and streams, through [pipewire](#port-pipewire).
  At run time Qt picks PipeWire unless `QT_AUDIO_BACKEND` asks for another backend or the
  PulseAudio server it finds is not PipeWire's.
- `-DFEATURE_pulseaudio=ON` — the PulseAudio backend, through [libpulse](#port-libpulse), which
  Qt falls back to when PipeWire is not available.
- `-DFEATURE_alsa=OFF` — no ALSA backend, which upstream marks experimental, never detects on its
  own and offers only without PulseAudio.
- `-DFEATURE_pipewire_screencapture=ON` — screen capture through the desktop portal and a
  PipeWire stream.
- `-DFEATURE_vaapi=ON` — hardware video decoding through [libva](#port-libva), handing frames on
  as dma-bufs.
- `-DFEATURE_linux_v4l=ON` — cameras through Video4Linux.
- `-DFEATURE_spatialaudio=ON` — with `-DFEATURE_spatialaudio_quick3d=ON`: the Qt Spatial Audio
  module and its Qt Quick 3D types, which is why [qt6-qtquick3d](#port-qt6-qtquick3d) is in
  `depends`.

## Patches
- `select.patch` — adds `#include <sys/select.h>` to the FFmpeg plugin's `qv4l2camera_p.h`, for the `struct timeval` of its `m_firstFrameTime` member.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
