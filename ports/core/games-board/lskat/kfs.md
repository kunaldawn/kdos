## About

Lieutenant Skat (`lskat`) is a card game for two from the Skat family: each player has sixteen
cards laid out as two rows of four pairs, the top card of each pair face up and the one beneath it
turned over once it is free, and tricks are taken under Skat's trump rules. The opponent is a
second person or the computer. It draws its cards from the decks [libkdegames](#port-libkdegames)
installs.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the themes' SVGs to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

`src/fromlibkdegames` holds a copy of libkdegames' card cache, deck information and deck dialog,
compiled into the program. Those files are LGPL 2.0 only; the rest of the game is LGPL 2.0 or
later.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.lskat.desktop`. `KAboutData` derives the window's Wayland
app id, `org.kde.lskat`, from the component name and the `apps.kde.org` home page, so it is the
entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
