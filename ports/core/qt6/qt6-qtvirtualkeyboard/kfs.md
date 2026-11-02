## About
Qt Virtual Keyboard for Qt 6 is an on-screen keyboard that Qt applications load as an input
method plugin (`QT_IM_MODULE=qtvirtualkeyboard`), with its Qt Quick types for embedding the
keyboard in a QML scene. It has layouts for some forty languages, word prediction through
Hunspell, and the Japanese, Chinese and Korean input methods. plasma-keyboard, the SDDM greeter's
`--inputmethod` and plasma-setup are built on it.

## Flags
- `-DFEATURE_system_hunspell=ON` — with `-DFEATURE_hunspell=ON`: word prediction through the
  [hunspell](#port-hunspell) library the tree builds. The release archive's `3rdparty/hunspell`
  holds only CMake files, not Hunspell's sources, so the system library is the only one there is.
  Both switches are named so that a missing library stops the configure instead of leaving
  prediction out.
- `-DFEATURE_vkb_sound_effects=ON` — key-press sounds, through Qt Multimedia; upstream turns
  them on only when it finds [qt6-qtmultimedia](#port-qt6-qtmultimedia), so it is named and in
  `depends`.
- `-DFEATURE_vkb_desktop=ON` — with `-DFEATURE_vkb_layouts=ON`: the desktop integration, which
  shows the keyboard in its own window over any focused Qt text field, and the built-in layouts.
  Both are upstream's defaults, named.

## Notes
The recipe does not pass the `INPUT_vkb_hunspell` choice: `cmake/QtVirtualKeyboardSetup.cmake`
overwrites it in the cache with an empty value on every configure, so it selects nothing.

Prediction reads dictionaries from `/usr/share/hunspell`, which
[hunspell-en](#port-hunspell-en) fills for English; it is in `depends` for run time. A layout
whose language has no dictionary there types without suggestions.

The keyboard layouts are input data, not interface translations, so every language upstream
autodetects is built; the package-wide English-only rule (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
does not touch them.

The configure finds Qt Quick and Qt SVG quietly and skips the whole module without either, so
[qt6-qtdeclarative](#port-qt6-qtdeclarative) and [qt6-qtsvg](#port-qt6-qtsvg) are in `depends`.

Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
