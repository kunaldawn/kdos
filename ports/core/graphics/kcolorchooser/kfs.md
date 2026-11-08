## About

KColorChooser is a stand-alone colour dialog: a palette, a hue and saturation field, numeric
entry in RGB, HSV and HTML notation, an alpha channel and a picker that takes a colour from
anywhere on the screen. It starts with the colour on the clipboard when there is one, takes a
starting colour with `--color`, and with `--print` writes the chosen colour to standard output,
so a script can ask a person for a colour.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The dialog is Qt's own `QColorDialog`. On Wayland its screen picker asks the desktop portal for
the colour under the pointer (the `PickColor` call of `org.freedesktop.portal.Screenshot`), so the
picker answers only in a session that runs xdg-desktop-portal-kde; the rest of the dialog works
without it.
