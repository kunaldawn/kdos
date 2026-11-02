## About

KSvg draws themed SVG artwork at any size. `KSvg::Svg` renders a named element of one SVG file
(plain or gzip-compressed `.svgz`, read through [karchive](#port-karchive)). A file with a
`<style id="current-color-scheme">` element has that stylesheet replaced, before it is rendered,
by `.ColorScheme-*` rules holding the current colour scheme's colours
([kcolorscheme](#port-kcolorscheme)), so one image follows every accent. `KSvg::FrameSvg` assembles
a resizable frame from nine elements (four corners, four edges and a centre), the shape every
panel, tooltip, dialog background and button of a Plasma style is drawn from. `KSvg::ImageSet`
finds each file in the active image set, a theme directory under a base path its caller chooses
(`plasma/desktoptheme/<name>/` under `/usr/share` for a Plasma style), and falls back to the set
named `default` for anything the theme leaves out. The QML module `org.kde.ksvg` (`SvgItem`, `FrameSvgItem`)
puts the same drawing in Qt Quick and takes its colours from `Kirigami.Theme`
([kirigami](#port-kirigami)).

[libplasma](#port-libplasma) builds the Plasma style, `Plasma::Theme`, on it, so the panel, every
applet, the lock screen of [kscreenlocker](#port-kscreenlocker) and the window decorations of
[aurorae](#port-aurorae) draw through this library, and [kwin](#port-kwin),
[kclock](#port-kclock) and [knights](#port-knights) link it directly. KDOS's Plasma style is a
directory of SVGs this library renders; the library itself carries no artwork.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The tarball carries a `po/` directory of `libksvg6.po`
  catalogues, but its CMake files install none of them and do not read the switch, so the
  configure reports it as unused and it changes nothing in this build. Every KDE recipe in the
  tree passes it, so a later release that installs translations builds none of them without a
  recipe change; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Rendering an SVG is slow, so KSvg keeps two caches per user. The pixmaps of an image set go into a
shared-memory cache, `~/.cache/plasma_theme_<set>_v<version>.kcache` (`KImageCache` from
[kguiaddons](#port-kguiaddons)); the position and size of each element go into
`~/.cache/ksvg-elements`. Neither test looks at the content of a file. The pixmap cache is named
after the `Version` in the image set's `metadata.json` and is discarded when that file is newer
than the cache; the element cache is keyed on each SVG's modification time. Every file a package
installs carries the tree's pinned `SOURCE_DATE_EPOCH` as its modification time (see
[Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages)),
so an update that changes a style's SVGs changes none of those times. A user who ran the old
style keeps seeing its pixmaps until the `Version` changes, which renames the cache and deletes
the old one at the next start. A style shipped by a port raises `Version` in its `metadata.json`
with every change to its images.

`BUILD_TOOLS` stays at its default, off. It builds `split-plasma-svgs`, a theme author's tool that
cuts a combined SVG into one file per element, and installs nothing a session uses. The API
documentation is built only by its own `prepare_docs` and `generate_docs` targets, which need
Qt's `qdoc`, and the recipe builds neither.
