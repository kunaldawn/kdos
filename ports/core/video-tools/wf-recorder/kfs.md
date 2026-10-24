## About
[wf-recorder]{.idx} records the screen from a prompt: it captures an output, or a region of one,
through the wlr-screencopy protocol, which [kdos-comp](#port-kdos-comp) serves to host programs
(see [Screen capture and the clipboard](../../../../docs/kdos/03-architecture/session.md#screen-capture-and-the-clipboard)),
and encodes it with [ffmpeg](#port-ffmpeg)'s libraries, with sound from PipeWire.

## Flags
- `-Dpipewire=enabled` — With `-Dpulse=disabled` and `-Ddefault_audio_backend=pipewire`: record
  sound through PipeWire. Both back ends are `auto` upstream, and when the default is `auto`,
  PulseAudio wins whenever `libpulse-simple` is found, so the recorder's audio path would follow
  whatever the chroot happens to hold. Enabled, a missing `libpipewire-0.3` stops configure
  instead of producing a recorder with no sound.

## Notes
- **libavdevice has no switch.** `meson.build` probes for it with `required: false`; it is found
  because the ffmpeg port builds it, and it is what lets wf-recorder write to a v4l2loopback
  device.
- **The fish completion moves.** `meson.build` installs it under `share/fish/fish/`, which fish
  never reads; `build.sh` moves it to `share/fish/vendor_completions.d/`.
