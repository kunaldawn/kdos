## About
[ripgrep-all]{.idx} (`rga`) runs [ripgrep](#port-ripgrep) over files ripgrep cannot read as text:
PDFs, e-books and office documents, archives and compressed files, SQLite databases, mailboxes,
and the subtitles and metadata of media files. `rga-preproc` turns each file into text and caches
the result; `rga-fzf` and `rga-fzf-open` are an interactive search through [fzf](#port-fzf). It is
a Rust program built with cargo from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `ZSTD_SYS_USE_PKG_CONFIG=1` — make the `zstd-sys` crate link the [zstd](#port-zstd) port through
  pkg-config instead of compiling its own copy.
- `LIBSQLITE3_SYS_USE_PKG_CONFIG=1` — make `libsqlite3-sys` link the [sqlite](#port-sqlite) port,
  even though the program's `Cargo.toml` asks `rusqlite` for its `bundled` feature, which would
  otherwise compile SQLite into the binary. rga uses SQLite for its cache and for searching inside
  database files.

## Notes
Both variables are among those `script/env/chroot.env` exports from `40_lang` on (see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)); the recipe names them again for
itself. The recipe has no `readelf` check after `cargo build`, so a
crate that fell back to its bundled library would not stop the build.

Many file types are read by running another program, not by a crate: `pdftotext` from
[poppler](#port-poppler) for PDFs, [pandoc](#port-pandoc) for e-books and office documents, and
`ffprobe` and `ffmpeg` from [ffmpeg](#port-ffmpeg) for media. With one missing, rga reports
`Could not find executable` for every file of that type instead of searching it, so every such
program is in `depends` although none is linked.
