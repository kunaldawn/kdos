## About

libseccomp builds seccomp filters, the kernel's per-process lists of allowed system calls, from a
plain C interface, so a program names the calls it allows instead of writing BPF by hand. In KDOS
its main consumer is the container stack behind every box: `crun`, the runtime podman starts each
container with, applies the seccomp profile of the container through it, and `podman`, `buildah`
and `conmon` are built with it as well; see
[The box](../../../../docs/kdos/03-architecture/packs-and-boxes.md#the-box). Outside the boxes,
`qemu` (`--enable-seccomp`) and `file` (`--enable-libseccomp`) sandbox themselves with it.

Apart from `--disable-static`, the port takes upstream's defaults. The Python bindings are off, as
they are unless `--enable-python` is given; they need Cython.

## Notes

`configure` stops with "please install gperf" when gperf is missing. The release archive already
carries the generated system-call lookup table, `src/syscalls.perf.c`, and the Makefile runs gperf
to rebuild it only when `syscalls.csv` or its template is newer, but the configure check runs
whatever the archive holds. [gperf](#port-gperf) is in `depends` for that check; the library does
not use it at run time.
