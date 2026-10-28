## About

libpsl answers questions about the Public Suffix List, Mozilla's list of the domains under which
anyone may register a name (`com`, `co.uk`, `github.io`). A program asks it whether a domain is a
public suffix, or what the registrable domain of a host is. HTTP clients use it to refuse a cookie
set for a whole suffix, such as one a server at `a.co.uk` tries to set for all of `co.uk`.

[curl](#port-curl) links it for exactly that cookie check, as do [libsoup3](#port-libsoup3),
[networkmanager](#port-networkmanager) and [transmission](#port-transmission). The package also
installs the `psl` command and `psl-make-dafsa`, the Python script that compiles a list into the
form the library reads. Configure needs Python only when a compiled form of the list is missing
from the source; the release tarball carries all three, `src/suffixes_dafsa.h` and the test suite's
`tests/psl.dafsa` and `tests/psl_ascii.dafsa`, so the build itself runs no Python.
The recipe's `depends` names [python3](#port-python3), which the installed script runs on.

## Flags

- `--enable-runtime=libidn2` — the IDNA library libpsl uses at run time. It converts the Unicode
  entries of a list file the library loads, and a Unicode name looked up in such a list, to their
  ASCII form; `psl_str_to_utf8lower` lowercases through it as well. The built-in list needs no
  conversion. Left to choose, configure tries [libidn2](#port-libidn2), then ICU, and builds with no
  run-time library when it finds neither; the older libidn is used only when named. Naming libidn2
  makes a missing libidn2 an error, `You requested libidn2 but it is not installed or usable.`,
  instead of a library that quietly converts with ICU or not at all.
- `--enable-builtin` — upstream's default, named: compile a copy of the list into the library
  itself, so it answers with no list file installed. The copy is the one in the release tarball
  (`list/public_suffix_list.dat`, already converted as `src/suffixes_dafsa.h`), so the built-in list
  is as old as the release. No port in the tree installs a list file for the library to load
  instead.

## Notes

The library's manual page, `libpsl.3`, is installed by `make install` only when both gtk-doc and
manual pages are enabled, and building it then needs `xsltproc` and the DocBook stylesheets. The
release tarball carries the finished page in `docs/libpsl/`, so `build.sh` installs that file
itself, as
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)
asks for a page upstream ships but does not install.
