## About
[TeXstudio]{.idx} is an editor for LaTeX documents: command and reference completion, a structure
view, spelling and grammar checking, and a built-in PDF viewer kept in step with the source
through SyncTeX. It runs the compilers of [texlive](#port-texlive), which is in `depends` for
that reason. It is a Qt 6 application.

## Flags
- `-D QT_VERSION_MAJOR=6` — build against Qt 6. Left unset, upstream looks for Qt 6 and then Qt 5,
  and builds against the first it finds.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Hunspell=ON` — make a missing system [hunspell](#port-hunspell)
  a configure error. Upstream looks for it quietly and compiles its own copy under
  `src/hunspell` when it is absent.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_QuaZip-Qt6=ON` — likewise for [quazip](#port-quazip), which
  TeXstudio otherwise replaces with its bundled copy under `src/quazip`, linked statically.
- `-D CMAKE_DISABLE_FIND_PACKAGE_QTermWidget=ON` — skip the search for QTermWidget, the library
  behind the optional built-in terminal panel, which the tree does not carry.
- `-D TEXSTUDIO_ENABLE_MEDIAPLAYER=ON` — play videos embedded in PDF files in the viewer, through
  Qt Multimedia. Off by default upstream.
- `-D TEXSTUDIO_ENABLE_TESTS=OFF` — no internal tests; upstream's default, named.

## Patches
- `no-update-check.patch` — in `src/configmanager.cpp`, changes the default of the `Update/AutoCheck` setting from `true` to `false`, so TeXstudio does not check for a newer release on its own; the check stays available in its preferences, where the user can turn it on.

## Notes
The PDF viewer exists only when the configure finds `poppler`, `poppler-cpp` and `poppler-qt6`
through pkg-config; without them upstream compiles TeXstudio with `NO_POPPLER_PREVIEW` and no
error. `build.sh` therefore reads
`CMakeCache.txt` after the configure and stops unless `POPPLER_FOUND` is set, so a TeXstudio with
no PDF preview cannot be packaged.

After the install, `build.sh` removes every file in `/usr/share/texstudio` that belongs to a
language other than English: the translation catalogues (`*.qm`) and the spelling dictionaries,
thesauri, hyphenation patterns and grammar word lists upstream ships for other languages, keeping
the `en_*`, `th_en_*` and `hyph_en_*` files. The HTML manual under `/usr/share/doc/texstudio`
stays: the Help menu opens it, and the tree's
[HTML prune](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy) exempts
that directory.
