## About

The GNU Scientific Library is a C library of numerical routines outside dense linear algebra:
special functions, random number distributions, integration, interpolation, root finding,
minimisation, FFTs and statistics. [gnuradio](#port-gnuradio), [inkscape](#port-inkscape),
[krita](#port-krita), [labplot](#port-labplot), [qgis](#port-qgis) and [bcftools](#port-bcftools)
link it. Its vector and matrix operations call CBLAS, and in KDOS that CBLAS is
[openblas](#port-openblas).

## Patches

- `openblas-default-cblas.patch` — Changes the CBLAS library a consumer is told to link from GSL's own `-lgslcblas` to `-lopenblas`, in both places GSL hands it out: the `GSL_CBLAS_LIB` variable in `gsl.pc.in`, which `pkg-config --libs gsl` expands, and the default of `GSL_CBLAS_LIB` in `gsl-config --libs`, together with that script's help text. A program can still choose another CBLAS by setting `GSL_CBLAS_LIB` when it runs `gsl-config`.

## Notes

`libgsl` itself leaves the `cblas_*` symbols undefined, so every program that links it must link
a CBLAS as well, and takes the name of that library from `pkg-config` or `gsl-config`.

GSL still builds and installs `libgslcblas.so`, its own reference CBLAS; the patch only changes
what consumers are pointed at. A program that names `-lgslcblas` itself links the reference
routines, not OpenBLAS.

The loop after the install re-encodes each installed header that is not valid UTF-8 from
ISO-8859-1 to UTF-8, and leaves every valid one untouched: a consumer that reads a header as
text fails on upstream's. `gsl_complex_math.h`, whose copyright line spells a name in Latin-1, is
one the loop rewrites.
