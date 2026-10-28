## About
[Typst]{.idx} is a markup-based typesetting system: one program that compiles a `.typ` source
to PDF, PNG, SVG or HTML, and reports an error with the line it is on. The phase list's comment
sets it against TeX Live, which is gigabytes to install. [kdos-kfs](#port-kdos-kfs), the port that
ships this book, needs it to typeset the book's PDF.

System fonts are found through fontconfig's configuration, which the `fontdb` crate reads
itself (typst does not link libfontconfig), so typst sees the font directories the rest of the
system does, plus the few fonts it embeds. `typst` links the system [openssl](#port-openssl) (`libssl.so`, `libcrypto.so`) for
its HTTPS downloads of Typst Universe packages, which a document asks for with an `@preview/`
import.

## Flags
- `--bin typst` — Build only the `typst` executable of the workspace.
- `--no-default-features`, `--features embedded-fonts,http-server` — Name the features instead
  of taking the defaults. The two named are `typst-cli`'s default set: `http-server` is the
  local live-reload server behind `typst watch` and HTML export, and `embedded-fonts` builds in
  typst's own fonts, which `typst-cli` also asks of `typst-kit` directly, so they are in the
  binary either way. `self-update`, which lets the program replace itself, and `vendor-openssl`,
  which compiles a private OpenSSL, stay off, and so does any feature upstream adds to its
  defaults until the recipe names it.
- `GEN_ARTIFACTS="$PWD/gen"` — `typst-cli`'s build script writes the manual pages and the bash,
  zsh and fish completions into this directory when it is set, and the install lines take them
  from there.

## Notes
`-C target-feature=-crt-static` matters here beyond the usual reason. Linked statically,
`openssl-sys` takes `libcrypto.a`, whose compression code calls zlib, and a static archive
records no dependency of its own, so the link fails on zlib's symbols. Linked dynamically,
`libcrypto.so` names `libz` itself. See
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).
