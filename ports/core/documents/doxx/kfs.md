## About

doxx opens a Word `.docx` file in the terminal: it renders the headings, paragraphs, lists and
tables, lets the reader search and jump through an outline, and can export the text as Markdown, CSV
or JSON. It is a Rust program built from a vendored crate bundle. The port also writes a desktop
entry for it, so a `.docx` file opened from the desktop can open in doxx in a terminal.

## Notes

- **Zstandard is the system's.** The `zip` crate reads Zstandard-compressed archive members through
  `zstd-sys`, and the tree's environment sets `ZSTD_SYS_USE_PKG_CONFIG=1` for every Rust build from
  `40_lang` on, so it links [zstd](#port-zstd) instead of compiling its own copy (see
  [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)).
  The `readelf -d` line after `cargo build` stops the build unless the binary NEEDs `libzstd.so.1`.
- **The desktop entry is a handler, not a launcher.** `build.sh` writes `doxx.desktop` with
  `MimeType=` set to the `.docx` type, `Terminal=true` and `NoDisplay=true`. Its `MimeType=` line
  is how a document opened from the file manager reaches doxx; `NoDisplay=true` keeps it out of
  Start and the launcher. The rules for such entries are in [Desktop entries belong to the
  port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).
