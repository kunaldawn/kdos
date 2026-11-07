## About

clinfo lists every OpenCL platform the ICD loader finds and every property of every device on it.
[kinfocenter](#port-kinfocenter) runs it at run time for its OpenCL page, which is empty without
it. It links `libOpenCL` from [ocl-icd](#port-ocl-icd) and compiles against the headers of
[opencl-headers](#port-opencl-headers).

## Notes

The `Makefile` puts `-g -pedantic -Werror` into `CFLAGS` only when `CFLAGS` is unset, and the tree
always exports it, so the build takes the release flags and no debug information. `MANDIR` is
named on the install line because upstream's default, `$(PREFIX)/man`, is not where `man` looks;
the page goes to `/usr/share/man/man1`. Upstream installs the program `0555` and the page `0444`,
modes the package lint accepts.

`LICENSE` is a CC0 waiver that refers to the full legal code in `legalcode.txt`; `kpkg` copies
only files whose names begin with `LICENSE`, so `build.sh` installs `legalcode.txt` beside it.
