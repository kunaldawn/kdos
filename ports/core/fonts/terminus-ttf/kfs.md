## About

Terminus TTF is the Terminus face as a scalable TrueType font, for the programs that draw text
through Pango, which renders no bitmap fonts. It is compiled here from the same BDF sources the
[terminus-font](#port-terminus-font) port builds the console and bitmap fonts from, with
upstream's own pipeline, mkttf: every pixel size goes into one face as a bitmap strike, the
largest is traced into curves with potrace, and FontForge writes the TrueType file. The family is
`Terminus (TTF)`, distinct from the bitmap `Terminus`, so a program that asks for `Terminus` by name keeps
drawing the bitmap, and only the programs that ask for `Terminus (TTF)` get this one.

## Flags

- `CC="${CC:-cc}"` — Builds mkitalic, from the mkbold-mkitalic source, with the tree's compiler;
  its makefile names `gcc` outright. mkitalic slants a BDF font, which is where the two italic faces come
  from. It is a build tool only and is not installed.
- `-f 'Terminus (TTF)'` — With `-n "$fname"` and `-N "$nice"`: the family, the PostScript name
  (`TerminusTTF`, `TerminusTTF-Bold`, …) and the display name of each face, the names upstream's
  own `mkttf.sh` gives them.
- `-C` — Appends the build year, the author and the SIL Open Font License notice to the copyright
  each face takes from its BDF, as `mkttf.sh` does. The year comes from `SOURCE_DATE_EPOCH`, not
  from the clock, so two builds of one recipe give the same file.
- `-e AX86` — The OS/2 vendor ID, upstream's own; mkttf's default is FontForge's `PfEd`.
- `-A ' -a 0'` — Passes `-a 0` to potrace, its corner threshold at zero, so every corner of the
  traced outline stays sharp, as a pixel font's should.
- `-V "$version"` — The font version written into each face: mkttf's release number for this
  Terminus, `version` in `kpkgbuild`, which differs from the Terminus release `_terminus`.
- `-O` — Has mkttf set the OS/2 table's weight fields from the face's weight itself, which some
  FontForge builds leave undone.

## Notes

`build.sh` runs the steps of upstream's `mkttf.sh` itself, with the same `mkttf.py` arguments,
rather than running the script, because the script stamps the clock's year into every face. It
works on a copy of the BDF files in `$SRC_ROOT/bdf`, where the italic BDFs are written, rather than
in the source directory.

mkttf.py's default tracer, `potrace-wrapper.sh`, enlarges each glyph with ImageMagick's `magick`
before potrace sees it, which is why [imagemagick](#port-imagemagick) is in `depends` beside
[potrace](#port-potrace) and [fontforge](#port-fontforge).

The four faces are installed in `/usr/share/fonts/TTF`, and mkttf's distribution licence as the
package's `COPYING`.
