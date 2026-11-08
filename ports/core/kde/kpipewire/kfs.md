## About

KPipeWire is Plasma's PipeWire screen-cast client. It takes the video stream a compositor offers
through the screen-cast portal or KWin's own screen-cast protocol, imports its frames as DMA-BUF
textures, and shows them in Qt Quick (`org.kde.pipewire`), encodes them to a file
(`org.kde.pipewire.record`), or watches which streams are open (`org.kde.pipewire.monitor`).
Spectacle records with it, the task manager's window thumbnails are drawn with it, and
[krdp](#port-krdp) and krfb send the desktop over the network through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of its messages, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every dependency is a requirement of the configure, with no switch: PipeWire, FFmpeg's
`libavcodec`, `libavformat`, `libavfilter`, `libavutil` and `libswscale`, GBM from
[mesa](#port-mesa), EGL through [libglvnd](#port-libglvnd), [libepoxy](#port-libepoxy),
[libdrm](#port-libdrm) and [libva](#port-libva).

The recorder asks FFmpeg for its encoders by name when a recording starts, so what it can write
is decided by the [ffmpeg](#port-ffmpeg) port and not here: `libx264`, `libvpx` and `libvpx-vp9`,
`libwebp`, `gif` and the VA-API `h264_vaapi` are built there. The `libopenh264` encoder is not,
and the recorder skips it.
