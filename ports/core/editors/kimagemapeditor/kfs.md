## About

KImageMapEditor edits HTML image maps: it opens an image or an HTML page, lets regions
(rectangles, circles, polygons, freehand) be drawn over each image, and writes the `<map>` and
`<area>` markup back. It is a KDE application on Qt 6 and also installs a KPart, so another
KParts host can embed the editor. The page preview is drawn with
[qt6-qtwebengine](#port-qt6-qtwebengine), which upstream requires.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The handbook is built through [kdoctools](#port-kdoctools), which upstream lists among its
required frameworks, so no switch is needed to keep it. The program sets its desktop file name to
`org.kde.kimagemapeditor`, the name its entry is installed under, so the Wayland app_id and the
entry agree without a patch.
