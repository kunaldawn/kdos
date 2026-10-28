## About

FFTW computes discrete Fourier transforms of any size and dimension. Its consumers in KDOS
include [pipewire](#port-pipewire), [alsa-utils](#port-alsa-utils), [ardour](#port-ardour),
[gnuradio](#port-gnuradio), [hackrf](#port-hackrf), [imagemagick](#port-imagemagick),
[krita](#port-krita) and [octave](#port-octave). FFTW builds one
floating-point precision per configure run, so `build.sh` configures, builds and installs three
times into the same package: double precision (`libfftw3`), single (`libfftw3f`) and long double
(`libfftw3l`), each with its threads and OpenMP companion libraries.

## Flags

- `--enable-threads` — Also build and install the threads library (`libfftw3_threads` and its
  single and long-double siblings), which runs one transform across several threads with
  pthreads. Upstream builds none by default.
- `--enable-openmp` — The same again with OpenMP, as a separate `libfftw3_omp`; the two libraries
  have different names, so both are installed and each consumer links the one it was written for.
- `--enable-sse2`, `--enable-avx`, `--enable-avx2`, `--enable-avx512` — Compile FFTW's SIMD
  codelets for each of these instruction sets. FFTW checks at run time which of them the processor
  has and uses only those, so the library still runs on a processor with none of the newer ones;
  this is the run-time dispatch the tree prefers to a build for one processor (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `--enable-fortran` — Build the Fortran-callable wrapper routines into the libraries, which is
  upstream's default; `F77=gfortran` names the compiler configure reads the Fortran symbol naming
  from. With no Fortran compiler found, configure warns and builds the wrappers with default
  names instead.
- `--enable-float` — The second run: single precision, `libfftw3f`.
- `--enable-long-double` — The third run: long-double precision, `libfftw3l`.

## Notes

The third configure line is written out rather than built from `$COMMON` because it must leave
out the four SIMD switches. FFTW's SIMD code exists for single and double precision only, and
configure stops with "SSE2 requires single or double precision" (and the same for each AVX
switch) when one is given with `--enable-long-double`.

`make clean` between the runs is what keeps the second and third builds from linking objects
compiled for the precision before. All three runs install into the same `$PKG`, and their files
differ in name by the `f` and `l` suffixes, so none overwrites another's library. The headers,
`fftw3.h` and the Fortran include files, do not depend on the precision and are installed three
times over.
