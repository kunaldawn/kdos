## About

`kwin-effect-kdos-keys` is the key card: a full-screen, searchable card of every global keyboard
shortcut the session holds, grouped into Windows, Desktops, Launch, KDOS, Accessibility and System.
Meta+F1 and Meta+/ open it, the same keys close it, and Esc clears the search and then closes it.
Plasma has no overview of its shortcuts; this is one.

It is a KWin effect written in QML, a KWin/Effect package installed to
`/usr/share/kwin/effects/kdos_keys/`. Each time it opens it asks kglobalacceld, over D-Bus, for every
component and each component's shortcuts with their current keys, so a chord changed on System
Settings' Shortcuts page is on the card the next time it opens, with no restart and no file in
between.

Which group a shortcut is listed under is `groups.conf`, a table of component and action patterns
read top to bottom. `keycard-gen.c` compiles it at build time into the `groups.js` the card reads;
the table, not the card, is what to edit when a group is wrong.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests. The tests are QML (`tests/tst_keys.qml.in`), run by
  `qmltestrunner` against the generated `groups.js`; `testing/plasmatest.sh` builds them in the
  development container, where Qt's test runner is.

## Notes

The effect's own key is Meta+F1, which KWin also binds to "Switch to Desktop 1" beside Ctrl+F1. The
shipped `kglobalshortcutsrc` takes Meta+F1 off desktop 1 and gives the card's action Meta+/ as its
second key; a `ShortcutHandler` registers one key, and one action with two keys is one row on the
Shortcuts page. Without the shipped entry (a person's own file holding its own value), Meta+F1 stays
desktop 1's and the card has no key until one is bound.

The build stops when the card's own shortcut is placed only by a fallback rule of `groups.conf`.
`testing/selftest.sh` runs the same check, `kdos-keycard-gen --check`, over the shipped
`kglobalshortcutsrc`, every kglobalaccel desktop entry and every desktop entry with
`X-KDE-Shortcuts` under `src/` and `fs/`, and every KWin `ShortcutHandler` and `registerShortcut()`
under `src/`, so a chord KDOS ships always has a group chosen for it. The table is
also installed as `/usr/share/kdos/keycard-groups.conf`, so `kdos help --keys` can group the
chords it prints the way the card does.

kglobalacceld goes on reporting a KWin script's keys after the script is unloaded, so a script KDOS
ships turned off, Krohnkite, would put keys on the card that do nothing. `keys.js` names each such
script with the prefix of its actions, the card asks KWin's `isScriptLoaded()` about each as it
opens, and leaves out the rows of one that is off; the Krohnkite rows appear after `kdos tiling on`.

Key names are Qt's own: `keynames.js` holds the text `QKeySequence::toString(PortableText)` gives
each named key of Qt 6, measured from Qt 6.11, so a chord reads on the card as it reads on the
Shortcuts page. kglobalacceld sends only the first key of a multi-key sequence over D-Bus, so a
sequence such as Meta+K, Meta+L shows as Meta+K.

A QML effect is loaded whatever the renderer, unlike a JavaScript one, so the card also opens under
software rendering. It is drawn with Kirigami and Plasma components in the colours of the current
colour scheme, and every row carries an accessible name of its action and keys.
