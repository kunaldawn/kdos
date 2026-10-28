## About

webrtc-audio-processing is the audio processing module of the WebRTC project — acoustic
[echo cancellation]{.idx}, noise suppression and automatic gain control — copied out of the
WebRTC source and given a meson build for distributions to package. In KDOS, PipeWire's echo-cancel
module, the `gst-plugins-bad` WebRTC DSP element and the chat client `dino` link it.

## Flags

- `--wrap-mode=nofallback` — Refuse every subproject fallback. When `pkg-config` does not find
  Abseil, `meson.build` falls back to the `abseil-cpp` subproject its `subprojects/abseil-cpp.wrap`
  describes, builds it into the library and installs its headers beside the library's own. With
  fallbacks refused, a missing `abseil-cpp` port is a configure error naming the dependency, and
  the library always links the system Abseil that every other consumer of it compiles against.
  The [meson wrapper](../../../../docs/kdos/05-developer/writing-ports.md#meson)'s own default,
  `nodownload`, would still try the fallback and stop on the subproject's missing source.
- `-Dgnustl=disabled` — The option chooses Android's GNU STL as the C++ library and is read only
  when the host system is Android; disabling it states that no such dependency is looked for.
- `-Dinline-sse=true` — Compile the SSE and SSE2 code paths inline, assuming the processor has
  them, rather than adding `-DWAP_DISABLE_INLINE_SSE`. It is upstream's default; SSE2 is part of
  the x86_64 baseline.

## Patches

- `cstdint.patch` — Includes `<cstdint>` in `multi_channel_content_detector.h` and `trace_event.h`, which use `int64_t` and `uint64_t` without it. libstdc++ from GCC 15 on does not reach `<cstdint>` through `<memory>` or `<string>`, so the first file that includes either header fails to compile.
- `abseil-nullability.patch` — Spells every nullability annotation as the qualifiers `absl_nonnull` and `absl_nullable`, and keeps the `absl::Nonnull<T>` and `absl::Nullable<T>` templates only when `ABSL_LTS_RELEASE_VERSION` is defined and below 20250512: in the public `audio_processing.h`, in `make_ref_counted.h` and `scoped_refptr.h`, and in the AEC dump factory and `AudioProcessingImpl` declarations and definitions. Abseil has no such templates from its 20250814 release on, and the `abseil-cpp` port is newer, so without the patch neither the library nor a program including its public header compiles against it.

## Notes

- `-Dneon` is left at `auto`: it is decided by the processor, on for aarch64, by a compile test
  of `arm_neon.h` on 32-bit ARM and unavailable on x86_64, never by a library being present.
- **An Abseil bump can break this port and its consumers.** The library's public header includes
  Abseil, so `pipewire`, `gst-plugins-bad` and `dino` compile against the same `abseil-cpp`
  headers; a change in Abseil's spelling of an annotation the header uses reaches all of them.
