## About
[Opus]{.idx} is the IETF audio codec for speech and music, from narrowband voice to full-band
stereo, and the codec of WebRTC calls and of `.opus` files. This port is the reference library,
`libopus`. [pipewire](#port-pipewire), [ffmpeg](#port-ffmpeg), the GStreamer plugins,
[chromium](#port-chromium), [mumble](#port-mumble), [freerdp](#port-freerdp) and
[spice](#port-spice) encode and decode through it; [libopusenc](#port-libopusenc) and
[opusfile](#port-opusfile) build file writing and reading on top of it.

It is one of the hot libraries the tree builds with link-time optimisation (see
[Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).

## Flags
- `-flto=auto` — compile and link with link-time optimisation, in `CFLAGS` and `LDFLAGS`.
- `-frandom-seed=opus` — fix the seed GCC names each object's `.gnu.lto_*` sections with, so the
  objects are the same bytes on every build.
- `--enable-deep-plc` — conceal lost packets of SILK speech with a neural network instead of the
  classic concealment.
- `--enable-dred` — Deep REDundancy: an encoder can add neural redundancy to a stream, from which a
  decoder rebuilds speech lost in a burst of dropped packets.
- `--enable-osce` — enhance low-bit-rate speech in the decoder with a neural network.
- `--disable-doc` — with `--disable-extra-programs`: no API reference and no demo and test
  programs.

## Notes
All three neural features are off by upstream's default. Their model weights ship in the release
archive as C sources under `dnn/`, so nothing is downloaded; what they cost is the weights' size
in `libopus`. A caller turns each on: the decoder uses deep PLC and OSCE only above a complexity
the caller sets with `OPUS_SET_COMPLEXITY`, whose decoder default is 0, and the encoder adds DRED
only when the caller sets `OPUS_SET_DRED_DURATION`. A caller that asks for none of them encodes
and decodes as it would with a library built without them.
