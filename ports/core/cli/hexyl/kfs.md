## About
hexyl is a hex viewer for the terminal. It colours each byte by its class (NUL, printable ASCII,
ASCII whitespace, other ASCII and non-ASCII), so the structure of a binary file shows without a
legend. The port installs `hexyl` and its manual page.

## Flags
- `-M` — Give [lowdown](#port-lowdown) the manual page's `source` field, `hexyl <version>`. The
  Markdown page's title block carries a version string of its own, which need not match the
  release, and lowdown takes the value given here over the document's.

## Notes
hexyl is built from a vendor bundle, `hexyl-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
