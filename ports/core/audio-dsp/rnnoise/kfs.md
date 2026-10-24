## About

RNNoise is Xiph's [noise suppression]{.idx} for speech: a small recurrent neural network,
trained on recorded noise and speech, that removes background noise from a 48 kHz voice signal in
real time. It installs `librnnoise` with its trained model compiled in. In KDOS, `obs-studio`'s
noise-suppression filter, `mlt` and `sdrangel` link it.

## Flags

- `--enable-x86-rtcd` — On x86_64, compile the SSE4.1 and AVX2 kernels as well and choose among
  them at run time from what the processor reports; without it the library is compiled for the
  baseline instruction set alone. `build.sh` passes it only when `uname -m` is `x86_64`.
- `--disable-examples` — Do not build `rnnoise_demo`, which is never installed.
- `--disable-doc` — Do not run Doxygen over the API.

## Notes

- **The model is in the release tarball.** It carries the trained weights as
  `src/rnnoise_data.c`; a checkout from git has none, and its `autogen.sh` fetches them from a
  model server. The release tarball is therefore the source that builds offline. The weights are
  one of the payloads [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)
  lists as data with no other source form.
- **The patch is needed for the run-time dispatch.** Without it `vec_avx.h` does not find
  `x86cpu.h`, and with `--enable-x86-rtcd` the AVX2 kernels do not compile.
- The last two lines of `build.sh` fail the build when the shared library or its `rnnoise.pc` is
  missing from the package.
- `freedv-gui` does not use this port: it builds a copy of RNNoise from its own source with its own
  weights.
