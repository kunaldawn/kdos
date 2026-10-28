## About
[Dino]{.idx} is an XMPP (Jabber) chat client for GTK 4 and libadwaita, written in Vala: one-to-one
and group chats, file transfer, end-to-end encryption with OMEMO or OpenPGP, and voice and video
calls. Most of those features are plugins in `/usr/lib/dino/plugins`, and the build switches below
decide which of them exist.

## Flags
- `-Dset-install-rpath=false` — install the program and plugins without an RPATH to the library
  directory; upstream's default, named.
- `-Dplugin-http-files=enabled` — file upload over HTTP (XEP-0363). This and the next four are
  `auto` features upstream, built when their libraries are found; `enabled` makes a missing
  library a configure error instead of a smaller program.
- `-Dplugin-ice=enabled` — peer-to-peer connections through ICE, with [libnice](#port-libnice).
- `-Dplugin-omemo=enabled` — OMEMO end-to-end encryption, with [libomemo-c](#port-libomemo-c) and
  [libgcrypt](#port-libgcrypt).
- `-Dplugin-openpgp=enabled` — OpenPGP encryption through [gpgme](#port-gpgme).
- `-Dplugin-rtp=enabled` — voice and video calls, carried over GStreamer.
- `-Dplugin-rtp-webrtc-audio-processing=enabled` — echo cancellation and noise suppression for
  calls with [webrtc-audio-processing](#port-webrtc-audio-processing); without the library upstream
  leaves the voice processor out, and with this switch the configure stops instead.
- `-Dplugin-rtp-vp9=enabled` — offer VP9 in video calls, through GStreamer's `vp9enc` and `vp9dec`.
  Off by default upstream; VP8 is compiled in whatever the switches say.
- `-Dplugin-rtp-h264=disabled` — no H.264 in calls (`x264enc` and the H.264 decoders). It is
  upstream's default, named.
- `-Dplugin-rtp-msdk=disabled` — no Intel Media SDK encoders and decoders (`msdk…` elements), and
  with `-Dplugin-rtp-vaapi=disabled` no VA-API ones (`vaapi…` elements, from gstreamer-vaapi,
  which the tree does not carry). Both are upstream's defaults, named.
- `-Dplugin-notification-sound=enabled` — play a sound for incoming messages through
  [libcanberra](#port-libcanberra). Off by default upstream.

## Notes
The VP9, H.264, Media SDK and VA-API switches only compile in a codec and the names of the
GStreamer elements that handle it; the elements themselves come from the GStreamer plugin ports at
run time, and Dino checks each one (`is_element_supported` in `plugins/rtp/src/codec_util.vala`)
before it offers the codec.

`build.sh` renders the scalable icon to PNGs at four sizes with `rsvg-convert` and writes its own
`im.dino.Dino.desktop` over the one upstream installs; like upstream's, it registers Dino for
`xmpp:` links.
