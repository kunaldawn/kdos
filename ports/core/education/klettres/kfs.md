## About

KLettres is KDE Education's alphabet trainer for young children and for adults starting a new
language: it says a letter or a syllable aloud and the learner types it. It ships the recorded
sounds of 22 languages, four themes and levels from single letters to syllables, and more
languages come through [knewstuff](#port-knewstuff).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The sound files under `/usr/share/klettres/<language>` are the program's data, not translations,
so the language rule keeps every one of them; they are most of the 47 MB tarball. Playback goes
through Qt Multimedia.

`tests/` is added whatever `BUILD_TESTING` says, so the configure requires Qt Test, which is part
of [qt6-qtbase](#port-qt6-qtbase); the test program is built and not installed.
`KAboutData` gives the window the app_id `org.kde.klettres`, the name of upstream's entry.

KNewStuff is off on KDOS (`[KDE Action Restrictions][$i] ghns=false` in
`/usr/share/kdos/xdg/kdeglobals`). "Get Alphabet in New Language..." is a plain action that opens
the KNewStuff dialog, so the key would leave it in the menu, opening a page that only says
downloading is not allowed; `kiosk-get-new.patch` hides it under the same check KNewStuff's own
actions make. The alphabets the package installs are all there is.
