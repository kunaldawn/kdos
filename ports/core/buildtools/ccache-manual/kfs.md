## About

This port is ccache's manual page, `ccache(1)`, and nothing else. ccache itself is built in
`30_foundation`, where every later CMake build can use it, and the tool that turns ccache's
AsciiDoc manual into a manual page, `asciidoctor`, is built in `31_compilers`. ccache's own
`ENABLE_DOCUMENTATION` switch builds the HTML manual and the page together and has no
manual-page-only form, so the `ccache` port turns it off and this port, in the later phase, builds
the page from the same tarball (see
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).

## Flags

- `doc/scripts/generate-manpage` — upstream's script, run with the arguments ccache's
  `doc/CMakeLists.txt` gives it for the manual page: the `asciidoctor` program, a `pandoc` slot
  (`""`, which the page script never reads; pandoc is used only for ccache's Markdown output), the
  stylesheet, the version, the input `doc/manual.adoc` and the output. It runs
  `asciidoctor -b manpage` and then `perl -pe` with a substitution meant to turn monospace text
  into bold. The script's quoting hands perl that program wrapped in single quotes, a string
  constant rather than a substitution, so it changes nothing: the installed `ccache.1` keeps
  asciidoctor's `\f(CR` monospace font changes.

## Notes

The source is the `ccache` port's tarball, with the same `sha256`, filed under this port's name
(`ccache-manual-<version>.tar.xz`). The page prints this recipe's `$version` as the version it
documents, so it is right only while the two recipes carry the same `version`; a bump of `ccache`
is a bump of this port too.
