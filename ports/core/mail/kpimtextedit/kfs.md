## About

KPimTextEdit is the KDE PIM library behind KMail's message composer: the rich-text composer
widget with its formatting actions, quoted-text highlighting, inline images, links, tables and
nested lists, the dialogs that insert them, and the step that turns a composed message into HTML
or plain text. Spelling comes from [sonnet](#port-sonnet), and the emoticon picker and the plain
text editor underneath come from [ktextaddons](#port-ktextaddons). [messagelib](#port-messagelib)'s
composer, [kmail](#port-kmail) and the other PIM editors link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

The configure requires ktextaddons 2.0.2 or newer for three of its libraries,
`TextEmoticonsWidgets`, `TextCustomEditor` and `TextAddonsWidgets`; ktextaddons is released on its
own schedule from `invent.kde.org/libraries`, not with KDE Gear, so a ktextaddons bump is checked
against this port's minimum.
