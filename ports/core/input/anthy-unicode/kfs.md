## About

Anthy is a Japanese kana-to-kanji conversion engine, and anthy-unicode is its maintained fork:
its internal encoding is UTF-8 rather than EUC-JP, and it keeps a user's files in
`$XDG_CONFIG_HOME/anthy` rather than `~/.anthy`. In KDOS it is the library under
[fcitx5-anthy](#port-fcitx5-anthy), the Japanese engine of the input method; see
[Input methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods).
The package also installs the conversion dictionary, which the build compiles from the text
dictionaries in the source.

Upstream publishes no release tarball, only tags, so the source is GitHub's archive of a tag,
renamed by the `name::url` form of `source =` to the `$name-$version` name kpkg expects. A tag
archive has no generated `configure`, so `build.sh` runs `autogen.sh` first, with `NOCONFIGURE=1`
so that it only generates the scripts.

## Flags

- `EMACS=no` — Answers configure's `AM_PATH_LISPDIR` check before it runs, so it looks for no
  Emacs and the Emacs Lisp front end is neither compiled nor installed. Emacs is not a port; an
  `emacs` that happened to be on the build machine's path would otherwise change the package.

## Notes

The version string is made by `autogen.sh`, not taken from the tag. The `AC_INIT` version in
`configure.ac` appends the output of `date +.%Y%m%d` at the time autoconf runs, unless the source
marks itself as a release, and this one does not. The version that `anthy-unicode.pc` and the
library's version call report therefore ends in the day the port was built, which need not match
the date in the tag the recipe names.
