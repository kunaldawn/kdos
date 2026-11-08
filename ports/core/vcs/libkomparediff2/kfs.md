## About

libkomparediff2 parses unified, context and normal diffs and the output of `diff`, `cvs diff` and
`perforce`, into a model of files and hunks that can be applied and reverted hunk by hunk.
[kdevelop](#port-kdevelop)'s patch review and [kompare](#port-kompare) are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
