## About

Blinken is KDE Education's memory game, after the electronic game Simon: four coloured lights
play a sequence with a tone each, and the player repeats it, one step longer every round. It
trains memory for children and adults, keeps a table of high scores and has three levels.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

This is the Qt Widgets build. Upstream's `QML_VERSION_FOR_DESKTOP` option, off by default, builds
a Kirigami interface instead and is meant for Android; the recipe leaves it at its default, which
is also the version upstream ships for the desktop. The handbook needs
[kdoctools](#port-kdoctools), which the widgets build requires.

The tones play through Qt Multimedia. The game's lettering font, `steve.ttf`, goes to
`/usr/share/blinken/fonts`, where only Blinken loads it; fontconfig does not see it.
`KAboutData` gives the window the app_id `org.kde.blinken`, the name of upstream's entry.
