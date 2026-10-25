## About

GNU groff is the [roff]{.idx} typesetting system: `troff` and `nroff`, the `eqn`, `tbl`, `pic` and
`refer` preprocessors, the `man`, `mdoc`, `ms`, `me` and `mm` macro packages, and output drivers for
terminals, PostScript, PDF, HTML and DVI. On KDOS manual pages are formatted by
[mandoc](#port-mandoc); groff is there for documents written for the full roff language and for
typesetting a page to PostScript, PDF or HTML.

## Flags

- `--disable-rpath` — Does not write a run-time library search path into the programs.
- `--without-x` — Does not build `gxditview`, the X11 previewer for troff output.
- `--with-uchardet=yes` — Builds `preconv`, the input-encoding converter, against
  [uchardet](#port-uchardet), so it can guess the encoding of a file that does not declare one.
  `yes` makes a missing uchardet a configure error instead of a `preconv` without it.
- `--with-gs=gs` — Names the Ghostscript program instead of searching for one. grohtml renders the
  pictures, tables and equations of an HTML page through [ghostscript](#port-ghostscript) and the
  [netpbm](#port-netpbm) tools. configure checks for every one of them; with any missing it warns
  that grohtml will not work properly and makes no HTML documentation.
- `--without-urw-fonts` — Turns off the search for the URW Type 1 fonts. That search takes
  Ghostscript's `Resource/Font` directory for the URW set, finds no AFM metrics there, and the
  install then stops on the `U-*` font descriptions that were never generated. Without them,
  configure rates gropdf's font support `intermediate` (Ghostscript found) rather than `full`.
- `--with-compatibility-wrappers=no` — Installs no wrappers that let groff use a vendor's non-GNU
  troff macro packages; there is no vendor troff on the system.

## Notes

- **mandoc owns `soelim` and `roff(7)`.** The package deletes groff's `soelim`, its manual page and
  `roff.7`, because [mandoc](#port-mandoc) installs the same paths and two packages cannot own one
  file. Both `soelim` programs read the same `.so` requests, so the manual-page pipeline is
  unchanged.
- **`charset.alias` is removed.** groff's bundled gnulib installs `/usr/lib/charset.alias`, a table
  of character-set name aliases, on C libraries other than glibc; the package does not ship it.
