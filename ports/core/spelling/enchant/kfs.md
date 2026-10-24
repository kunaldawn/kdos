## About

Enchant is the spell-checking library applications call instead of a particular checker: it loads
a provider module for each spelling engine and picks a dictionary by language. The ports that
link it include [gspell](#port-gspell), [libspelling](#port-libspelling) and
[qtspell](#port-qtspell), the spell-checking layers GTK and Qt applications use, the WebKitGTK
engines, and fcitx5, AbiWord, WeeChat and Profanity. This build has two providers, Hunspell and
Aspell, and the English dictionaries of both are in `depends`, so a check in English works on a
fresh system.

## Flags

- `--with-aspell` — Build the Aspell provider, which reads [aspell-en](#port-aspell-en)'s
  dictionaries. Upstream's default is to build a provider when configure finds its library; naming
  it with `--with` makes a missing library a failed configure instead of a smaller provider set.
- `--with-hunspell` — Build the Hunspell provider, which reads [hunspell-en](#port-hunspell-en)'s
  dictionaries from `/usr/share/hunspell`; named for the same reason.
- `--without-nuspell`, `--without-hspell`, `--without-voikko` — The Nuspell, Hebrew (Hspell) and
  Finnish (Voikko) engines, whose libraries are not ports. Naming them off keeps the provider set
  from following what the build root holds.
- `--without-winspell`, `--without-applespell` — The Windows and macOS system spell-checkers.
  configure tries WinSpell only on a native Windows build and AppleSpell only with an Objective-C++
  compiler, and each needs that system's headers.
- `--without-zemberek` — The Turkish Zemberek provider, which upstream marks experimental and
  deprecated and leaves off by default.
- `nodist_doc_DATA=` — Empty the make variable that lists the HTML renderings of the three manual
  pages (`enchant-2.1`, `enchant-lsmod-2.1`, `enchant.5`), which upstream makes with
  `groff -Thtml` and installs under `/usr/share/doc`. Set on the command line of both make runs,
  it reaches every subdirectory's make, so the HTML is neither built nor installed. The manual
  pages themselves install as pages.
