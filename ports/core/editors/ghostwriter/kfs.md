## About

ghostwriter is KDE's Markdown editor: a full-screen, distraction-free editing surface with focus
and Hemingway modes, a live HTML preview, a document outline, word and reading-time statistics,
spell checking through [sonnet](#port-sonnet), and export to HTML, PDF, ODT and DOCX. The preview
is a Qt WebEngine view, from [qt6-qtwebengine](#port-qt6-qtwebengine), which upstream requires.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The source carries three third-party trees under `3rdparty/`. cmark-gfm, GitHub's Markdown
parser, is C, compiled here as a static library and linked into the program; it parses for the
outline, the statistics and the built-in exporter. React and MathJax are built JavaScript that
the preview loads, compiled into the program's resources as upstream ships them; they are the
class of payload that
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)
lets ship as released.

The other exporters are programs ghostwriter looks for on `PATH` when the export dialog opens:
`pandoc` from [pandoc](#port-pandoc), which gives the PDF, ODT and DOCX formats, and `cmark` from
[cmark](#port-cmark). Both are in `depends` for run time; MultiMarkdown is not a port, so it never
appears in the list.
