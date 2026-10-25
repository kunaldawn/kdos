## About
[Hurl]{.idx} runs HTTP requests described in a plain-text `.hurl` file and checks each response
against the assertions written under it: status, headers, and the body queried by JSONPath,
XPath or regular expression. `hurlfmt` formats and converts such files. A test of an HTTP
service is then a file that can be kept beside the service and compared in a diff.

Hurl makes its requests through the system's [curl](#port-curl), so it speaks the protocols and
uses the CA store the rest of the machine does, and evaluates XPath with
[libxml2](#port-libxml2); the built `hurl` links both shared libraries.

## Flags
- `LIBCLANG_PATH=/usr/lib` — Where the `clang-sys` crate looks for `libclang.so`. The `libxml`
  crate generates its binding to libxml2 with bindgen (its `runtime` feature, which loads libclang
  at build time), so [clang](#port-clang) is in `depends`.
- `--bin hurl --bin hurlfmt` — Build the two programs by name; the workspace's third member, `hurl_core`, is a library they both link.

## Notes
The manual pages are shipped rendered in the source, under `docs/manual`, and installed as they
are.
