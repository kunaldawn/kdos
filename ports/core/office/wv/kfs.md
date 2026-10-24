## About
wv reads Microsoft Word 6, 95, 97 and 2000 documents, and the text of Word 2 ones. It is a library, `libwv`, and the programs
built on it: `wvWare`, `wvRTF`, `wvSummary`, `wvConvert` and `wvVersion`, and the scripts around
`wvWare` such as `wvHtml`, `wvText` and `wvLatex`. In KDOS
[abiword](#port-abiword) links it to open `.doc` files.

## Flags
- `-Werror=int-conversion` — Make an implicit conversion between an integer and a pointer a
  compile error. A function called with no prototype in scope is taken to return `int`, and
  storing that `int` in a pointer drops the upper half of the address on x86-64; with the flag
  such a call stops the build instead of producing a program that crashes when it uses the
  pointer. `wvsummary-gsf-include.patch` supplies the prototypes for the calls in `wvSummary.c`.
- `MKDIR_P` — Given to configure as `mkdir -p`, which configure keeps instead of probing. Its probe
  accepts only a `mkdir` whose `--version` names GNU coreutils (or fileutils 4.1), and falls back
  to the bundled `install-sh -d` for any other. wv's `install-sh` is X11R5's and has no `-d`, so
  with that fallback `make install` stops at the first directory it creates.
- `--with-zlib` — Link [zlib](#port-zlib) and, with `--with-png`, [libpng](#port-libpng), which
  wv uses for pictures embedded in documents.
- `--without-libwmf` — Build without libwmf, so Windows Metafile pictures embedded in a document
  are skipped when it is converted. The tree has no libwmf port.

## Patches
- `werrorformat.patch` — In `wvRTF.c`, pass a colour string to `rtf_output()` as the argument of a `"%s"` format instead of as the format itself, so a `%` in the string cannot be read as a conversion.
- `wvsummary-gsf-include.patch` — Include `<gsf/gsf-doc-meta-data.h>` in `wvSummary.c`, which calls `gsf_doc_meta_data_new()` and `gsf_doc_meta_data_foreach()` from it without declaring them.
