## About

OpenBLAS is an optimised implementation of BLAS, the basic dense linear algebra routines, with
LAPACK built on top of it in the same library. It is the one BLAS and LAPACK in KDOS: there is no
reference `lapack`, `blas` or `cblas` port, so [numpy](#port-numpy), [scipy](#port-scipy),
[octave](#port-octave), [R](#port-R), [gsl](#port-gsl), [qrupdate](#port-qrupdate) and the rest
of the numerical ports all land their matrix work here.

## Flags

- `DYNAMIC_ARCH=1` — Compile a kernel set for each x86-64 processor family OpenBLAS knows
  (Prescott, Core2, Nehalem, Haswell, Zen, SkylakeX, Sapphire Rapids and the rest) into the one
  library, and choose among them by `cpuid` when it is loaded. This is the run-time dispatch the
  tree's rule asks for wherever upstream offers it; a build for one processor would be a
  [machine-specific build](../../../../docs/kdos/05-developer/writing-ports.md#every-port), which
  is [`kdos march`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-march)'s choice and not
  the recipe's.
- `TARGET=CORE2` — The processor assumed for the code outside the per-family kernels. Upstream
  advises setting it with `DYNAMIC_ARCH=1` to the oldest model expected: left out, the common code
  is compiled for the build machine and can die with an illegal instruction on a weaker one before
  it reaches a kernel.
- `USE_THREAD=1`, `USE_OPENMP=0` — Thread the library with its own pthreads server rather than
  OpenMP. With threading on, OpenBLAS also takes its own locks, so several threads of one program
  may call it at once.
- `NUM_THREADS=64` — The most threads the library will run, fixed at build time; a program may ask
  for fewer at run time. Upstream warns that a large value costs memory for internal structures
  even when fewer threads are used.
- `NO_STATIC=1` — Install no `libopenblas.a`.
- `NO_AFFINITY=1` — Never pin OpenBLAS's threads to cores. Upstream's default; it notes that
  pinning can lock a calling program's threads onto the same cores, with NumPy and R named as
  programs it has hurt.
- `FC=gfortran` — The Fortran compiler. With one, the LAPACK half is compiled from its Fortran
  sources; with none found, OpenBLAS falls back to a C translation of LAPACK. gfortran comes with
  [gcc](#port-gcc).

## Notes

The five links `build.sh` makes after the install, `libblas.so`, `libblas.so.3`, `liblapack.so`,
`liblapack.so.3` and `libcblas.so`, all point at `libopenblas.so`. Consumers look the libraries up
by those names: without the links a configure script that tests `-lblas` or `-llapack` reports no
BLAS, and a binary that asks the loader for `libblas.so.3` or `liblapack.so.3` does not start.
They are owned by this package, so no other port may install a reference BLAS or LAPACK under
those names.

Both `make` lines carry the same variables. `make install` reads `Makefile.system` again and
decides from them what to copy (`NO_STATIC=1` is what keeps `libopenblas.a` out of the package),
so a variable changed in one line and not the other installs something other than what was
built.
