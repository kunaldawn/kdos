## About

Texinfo is the GNU documentation system. It installs `makeinfo` (`texi2any`), which builds info,
HTML and plain-text manuals from `.texi` sources, the `info` reader, `install-info`, and the TeX
side (`texi2dvi`, `texi2pdf`, `texindex`). Ports that build info manuals from their sources
depend on it, among them the cross toolchains for AVR, ARM and RISC-V, `bc`, `groff`, `R` and
`octave`.

`install-info` matters to the package manager as well: the info directory index,
`/usr/share/info/dir`, is one of the shared indexes `kpkgadd` and `kpkgdel` rebuild with it after
any install or removal that touches `/usr/share/info`; see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).

## Flags

- `--enable-perl-xs` — Build the C (XS) extensions of `texi2any` without first probing whether
  they can be built. Under the default probe, a failure turns them off quietly and `makeinfo` runs
  on its pure-Perl parser, which is much slower on a large manual; with the switch, a toolchain
  problem fails the build instead.
- `--enable-nls` — Ask for translated messages. musl's C library has no GNU libintl, and without
  the [libintl](#port-libintl) port, which is in `depends`, configure turns translations off and
  still succeeds; see Notes.
- `PERL_EXT_CFLAGS="$CFLAGS"` — The compiler flags for the XS extensions. Their subdirectory
  configures them with Perl's own recorded flags plus `PERL_EXT_CFLAGS`, and replaces the
  ordinary `CFLAGS` with that, so without this the tree's release flags never reach the XS code.
- `PERL_EXT_LDFLAGS="$LDFLAGS"` — The same for the linker flags of the XS extensions.

## Notes

`--enable-nls` only asks. The `grep` for `#define ENABLE_NLS 1` in `config.h`, straight after the
configure, reads what configure decided, and fails the build when it turned translations off
anyway, as it does when `libintl` is missing.
