## About

[argp]{.idx} is glibc's command-line parser: a program describes its options in a table and
`argp_parse()` handles them, with `--help` and `--usage` written for it. musl leaves it out, so
this port supplies it as a separate library, taken from glibc's own source by the
argp-standalone project. `elfutils` is the port in this phase that needs it; `dwarves`, `bcc`,
`crun`, `v4l-utils` and `qv4l2`, `freeipmi`, `f3`, `tlf` and `ptouch-print` name it too.

The package is one header, `/usr/include/argp.h`, and one static library, `/usr/lib/libargp.a`.
A program that uses it carries its own copy of the code, so nothing on the installed system loads
`libargp` at run time.

## Flags

- `autoreconf -fi` — the source is GitHub's archive of a tag, which holds `configure.ac` and
  `Makefile.am` but no generated `configure`, so the build system is generated first.
- `CFLAGS="$CFLAGS -g0"` — given on the `make` command line, this replaces the `CFLAGS`
  `configure` wrote into the Makefile. argp-standalone's `configure.ac` appends `-ggdb3` and a run
  of warning flags to `CFLAGS` after its checks, so the library would otherwise carry debug
  information; with this, it is compiled with the tree's flags and `-g0` and nothing else. The
  tree's flags include `-fPIC`, which lets the archive be linked into a shared library such as
  elfutils' `libdw`.

## Notes

Upstream's `Makefile.am` declares `libargp.a` and `argp.h` as `noinst_`: built, never installed.
`make install` installs nothing, so `build.sh` copies the two files into `$PKG` itself.
