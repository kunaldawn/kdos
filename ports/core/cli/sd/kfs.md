## About
sd is find-and-replace for the command line: `sd before after file` replaces in place, with
regular expressions in the syntax JavaScript and Python use, or literal strings with `-F`, and
none of `sed`'s delimiters to escape. The port installs `sd` and the manual page upstream ships pre-generated in `gen/`.

## Notes
sd is built from a vendor bundle, `sd-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
`gen/sd.1` is the page upstream generates with its `xtask` crate and commits to the source; the
build installs it as released and runs no generator.
