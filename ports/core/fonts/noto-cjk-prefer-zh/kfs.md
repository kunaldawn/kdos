## About
A fontconfig default with no source of its own: it makes [noto-cjk](#port-noto-cjk)'s Simplified Chinese variant
the first CJK face of `sans-serif`, `serif` and `monospace`, so Han text the program has not
tagged with a language is drawn with Simplified Chinese glyph forms. The `lang-zh` group installs it. A KDOS
session runs with `LANG=en_US.UTF-8`, so no CJK language reaches fontconfig unless a document
names one, and `70-noto-cjk.conf` only acts on a language; without this port the first CJK face
fontconfig reaches is Noto Sans CJK KR for `sans-serif` and Noto Sans Mono CJK KR for `monospace`,
whatever the reader's language.

## Notes
The recipe writes `/etc/fonts/conf.d/64-noto-cjk-prefer-zh.conf`, one `<alias><prefer>` per
generic family (`Noto Sans CJK SC` for `sans-serif` and `serif`, `Noto Sans Mono CJK SC` for
`monospace`). A `prefer` is inserted just before the generic name, after every face an earlier
file preferred, so the number decides where the face lands. At 64 it comes after
`56-noto-preferred.conf`, `57-dejavu-*.conf` and `60-latin.conf`, which keep Latin, Greek,
Cyrillic and the other scripts on their own faces, and before fontconfig's `65-nonlatin.conf`,
whose `sans-serif` and `monospace` lists name Noto Sans CJK KR: a file numbered 65 or later
puts the face behind KR there, and only `serif` moves. `70-noto-cjk.conf` still runs after it, so text that
does carry a CJK language keeps that language's variant.

With two of the three installed, the lower file name comes first: `ja` before `ko` before `zh`.
