## About

Kiten is KDE Education's Japanese reference and study tool: a dictionary over EDICT and KANJIDIC,
searchable by English, reading or kanji, with a kanji browser by grade and stroke count
(`kitenkanjibrowser`) and a radical selector (`kitenradselect`). The dictionaries ship in the
package, so lookups work offline. The search engine is a library, `libkiten`, which the three
programs share.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The package carries about 12 MB of EDICT and 1 MB of KANJIDIC, both under the EDRDG licence
(`LicenseRef-EDRDG`), and the 17 MB stroke-order font `KanjiStrokeOrders.ttf` in
`/usr/share/fonts/kanjistrokeorders`. That directory is on fontconfig's path, so the font is
visible to every program. Kiten's dictionary update fetches newer copies from
`http://ftp.edrdg.org` only when the user runs it.

`libkiten` installs its headers and a shared library; no other port links it. The kanji browser
and the radical selector have upstream entries with `NoDisplay=true`, and Kiten opens them from
its own menu. `KAboutData` gives each window the app_id of its entry's file name
(`org.kde.kiten`, `org.kde.kitenkanjibrowser`, `org.kde.kitenradselect`).
