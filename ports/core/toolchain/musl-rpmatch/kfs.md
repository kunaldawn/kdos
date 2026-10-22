## About

`rpmatch(3)` decides whether a reply typed at a yes/no prompt means yes or no. glibc has it and musl
does not; this port is a small library, `librpmatch.so`, with the header `rpmatch.h`, that provides
the one function. It installs no pkg-config file and no manual page; `rpmatch(3)` comes from
`man-pages`.

`elfutils` is the only port that names it. Its recipe force-includes `rpmatch.h` into every
compile and adds `-lrpmatch` to the link; see that port's page for what that does with the
current elfutils source.

## Flags

- `autoreconf -fi` — the source is GitHub's archive of a tag, which has no generated `configure`,
  so the build system is generated from `configure.ac` and `Makefile.am` first.
