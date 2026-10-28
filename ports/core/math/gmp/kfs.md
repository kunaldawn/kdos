## About

GMP, the GNU Multiple Precision Arithmetic Library, does arithmetic on integers, rationals and
floating-point numbers of any size. It is the bottom of the compiler's mathematics:
[mpfr](#port-mpfr) is built on it, [mpc](#port-mpc) on MPFR, and [gcc](#port-gcc) uses all three
to fold constant expressions while it compiles. No phase list names GMP; `20_selfhost` installs it
as gcc's dependency, ahead of the compiler that is rebuilt over it. [coreutils](#port-coreutils)
builds `expr` and `factor` against it, and [gawk](#port-gawk)'s `-M` uses it through MPFR.

In [Part III](../../../../docs/kfs/03-bootstrap/100-gcc.md) GMP is not a package: the bootstrap compiler builds a copy
inside its own source tree and links it statically. This port is the shared library every later
build of gcc, including the cross compilers, links instead.

## Flags

- `--enable-cxx` — also build `libgmpxx` and `gmpxx.h`, the C++ class interface. It is a separate
  library so that C programs linking `libgmp` do not pull in the C++ runtime.
- `--enable-fat` — build a "fat" library: the low-level routines for many x86 processor families
  are compiled in, and the one matching the processor is chosen at run time. Without it GMP builds
  the routines of a single processor family, the one configure detects on the build machine. This
  is the tree's rule for upstream run-time
  SIMD selection: turn it on, and never compile for the build machine (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port), "No
  machine-specific code").

## Notes

The compiler's own programs, `cc1` and `cc1plus` among them, link `libgmp.so`, `libmpfr.so` and
`libmpc.so` dynamically. A root missing any of the three, or holding one built for an incompatible
ABI, has a `gcc` that cannot compile anything.
