## About

libsamplerate, also called Secret Rabbit Code, is the [sample-rate converter]{.idx} library from
the libsndfile project: three band-limited sinc converters that trade quality against
computation, the best of them with a 145 dB signal-to-noise ratio and a passband reaching 96% of
the theoretical bandwidth, plus a linear and a zero-order-hold converter for when speed matters
more. In KDOS `rubberband` is built
against it rather than its own built-in resampler, and `alsa-utils`, `mpd`, `ardour` and `vlc` are
among the ports that link it.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Build `libsamplerate.so`. The project leaves the choice to CMake,
  whose default is a static archive.
- `-DLIBSAMPLERATE_EXAMPLES=OFF` — Skip the example programs, which are on by default when the
  project is built on its own. They are not installed, and they look for libsndfile and ALSA,
  neither of which this port depends on.
