## About
[baresip]{.idx} is a SIP softphone: it registers with a SIP account and makes and takes voice and
video calls, with its features in loadable modules (codecs, audio and video devices, NAT traversal,
media encryption, user interfaces). It runs in a terminal, and its desktop entry, `Telephone`,
opens it in one. It is built on [libre](#port-libre), the SIP and media library from the same
project.

## Flags
- `-DSTATIC=OFF` — build `libbaresip` and every module as shared objects, loaded by name from the
  configuration. It is upstream's default off Windows, named.
- `-DDEFAULT_AUDIO_DEVICE=pipewire,default` — the audio device compiled into the program
  (`default_audio_device()` in `src/config.c`) instead of `alsa,default`. The same value decides
  the template baresip writes as `~/.baresip/config` on its first run: with a `pipewire` device,
  `module pipewire.so` is written uncommented and `alsa.so` and `pulse.so` commented out, so calls
  go to [PipeWire](../../../../docs/kdos/03-architecture/session.md#audio), the session's sound
  server.
- `-DMODULES` — the modules to build, joined from the `modules` array at the top of `build.sh` into
  CMake's `;`-separated list. Upstream's default list is every module it has, and each module
  builds whenever the library it needs is found, so the set would follow whatever happens to be
  installed in the build root. Naming them fixes it. Left out are the modules for other systems
  (Android, macOS, Windows, BSD); modules whose library the tree does not carry (AMR, AV1, G.722
  and G.722.1, ZRTP, and packet-loss concealment through spandsp; the `aptx` module looks for
  `libopenaptx`, where the tree has the `libfreeaptx` fork, and `webrtc_aec`, which looks only
  for `webrtc-audio-processing-1`, not the API the
  [webrtc-audio-processing](#port-webrtc-audio-processing) port installs); and modules whose library the tree does
  carry but this port does not use: `gtk`, `x11`, `pulse`, `jack` (PipeWire's `libjack`; see
  [JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host)),
  `portaudio`, `gst`, `mqtt` and `codec2`.

## Patches
- `default-modules.patch` — changes the configuration template in `src/config.c` so that the `opus`, `avcodec`, `vp8`, `vp9` and `sdl` modules are loaded instead of commented out. The template is written only when `~/.baresip/config` does not exist; upstream comments these lines out because a `module` line for a module that is not installed is a start-up error, and this port builds and installs all five. Without them a first-run phone negotiates only G.711 and has no video codec and nowhere to show a picture. The camera stays a choice: `v4l2` is built but its line is left as upstream wrote it, as are `x11`, which is not built, and `fakevideo`.

## Notes
A module named in `MODULES` whose library is missing is still skipped without an error, because
each module's own `CMakeLists.txt` returns quietly when its `find_package` fails. So every library
a listed module links is in `depends`, and a module added to the list needs its library added
there too.

`build.sh` deletes upstream's `com.github.baresip.desktop` and writes `baresip.desktop` with
`Terminal=true`: the interface the generated configuration loads is the `stdio` and `menu`
modules, which read keys from the terminal.
