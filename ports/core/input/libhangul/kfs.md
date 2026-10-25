## About

libhangul turns keystrokes into Hangul syllables for the Korean keyboard layouts (the two-set
layout, the three-set layouts and romaja) and looks up Hanja for a word. In KDOS it is the
library under [fcitx5-hangul](#port-fcitx5-hangul), the Korean engine of the input method; see
[Input methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods).
The package also installs the keyboard definitions and the Hanja dictionaries under
`/usr/share/libhangul`, and `hangul`, a small command-line converter.

## Flags

- `--enable-nls` — Builds the library's message catalogues, as gettext's configure does by
  default. kpkg removes every non-English catalogue from the package afterwards (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the image carries no translation of the library's messages either way.
