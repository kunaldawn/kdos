## About

SoundTouch changes the [tempo and pitch]{.idx} of an audio stream independently of each other, or
both together as a playback-rate change, in real time. It installs `libSoundTouch` and
`soundstretch`, a command that applies the same processing to a WAV file. In KDOS, `tenacity`
links it for its change-tempo and change-pitch effects.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Build `libSoundTouch.so`; the project leaves the choice to CMake,
  whose default is a static archive.
- `-DINTEGER_SAMPLES=OFF` — Process 32-bit float samples, upstream's default, rather than 16-bit
  integers.
- `-DOPENMP=OFF` — No OpenMP parallel processing, upstream's default. Its README gives the reason:
  managing the threads costs run time, and real-time processing that has the CPU to spare gains
  nothing from it.
- `-DNEON=OFF` — No ARM NEON code path. Upstream's default is on, and it is taken only on an ARM
  processor, so on x86_64 the switch changes nothing.
- `-DSOUNDSTRETCH=ON` — Build and install the `soundstretch` command (the default).
- `-DSOUNDTOUCH_DLL=OFF` — Do not build `SoundTouchDLL`, the C wrapper library around the C++ API
  (the default).

## Patches

- `no-fast.patch` — Compiles with `-O3` alone instead of `-O3 -ffast-math`. Fast-math lets the compiler drop NaN and infinity handling and reorder floating-point sums, so the output would differ from the arithmetic the source states.
