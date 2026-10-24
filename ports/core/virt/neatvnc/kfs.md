## About

Neat VNC is a VNC server library: it speaks the RFB protocol, encodes framebuffers and handles
authentication, and leaves capturing the screen to the program that links it. In KDOS that
program is [wayvnc](#port-wayvnc), which takes frames from the compositor through its
screen-capture protocols and serves the desktop to a VNC viewer on another machine. wayvnc
requires a 1.x Neat VNC and fails at `meson setup` against an older series.

## Flags

- `-Dtls=enabled` — VeNCrypt, TLS encryption and authentication, through
  [gnutls](#port-gnutls).
- `-Dnettle=enabled` — The low-level crypto through [nettle](#port-nettle), its `hogweed`
  public-key half and [gmp](#port-gmp): RSA-AES and Apple Diffie-Hellman authentication, the
  classic VNC DES challenge, and the WebSocket transport, which neatvnc builds only when these
  are found.
- `-Djpeg=enabled` — Tight encoding with JPEG compression through
  [libjpeg-turbo](#port-libjpeg-turbo).
- `-Dgbm=enabled` — Accept framebuffers that are GBM buffer objects, through
  [mesa](#port-mesa)'s `libgbm`.
- `-Dh264=disabled` — The Open H.264 encoding, which encodes through FFmpeg's libavcodec or a
  V4L2 memory-to-memory device. FFmpeg is not in `depends`; with the feature off neither encoder
  is built, and clients get the classic RFB encodings.
- `-Dexperimental=false` — Upstream's experimental features; meson warns when they are enabled
  in a release build.
- `-Dsystemtap=false` — USDT tracing probes, which need `sys/sdt.h`.
- `--wrap-mode=nodownload` — `meson.build` first asks for [aml](#port-aml) as an optional
  subproject and only then for the installed `aml1`. The tarball carries no `subprojects/`, and
  with downloads refused the subproject is not found, so the installed port is used.

## Notes

`nettle4.patch` is three upstream commits that move the nettle backend to nettle and hogweed 4,
the series of the [nettle](#port-nettle) port. The first makes `meson.build` require
`>=4.0, <5.0`, so a build against nettle 3 fails at configure rather than at compile, drops the
size argument from the EAX digest calls and includes `<nettle/sha2.h>` in place of
`<nettle/sha.h>`. The second drops the size argument from the hash digest calls, and the third
makes the WebSocket handshake pass `CRYPTO_HASH_SHA1` to `crypto_hash_many()` where the second
left the hash size.
