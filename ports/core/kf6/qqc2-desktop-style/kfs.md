## About

qqc2-desktop-style is the `org.kde.desktop` style of Qt Quick Controls 2: buttons, fields, menus,
scroll bars and the rest drawn by the Qt Widgets style in use, Breeze for one, so a Qt Quick
program looks like a Qt Widgets one beside it. It also installs Kirigami's `org.kde.desktop`
platform plugin, through which [kirigami](#port-kirigami) takes its colours from the KDE colour
scheme and its icons from the icon theme. Plasma's System Settings pages and the Kirigami
applications in the tree, [elisa](#port-elisa), [haruna](#port-haruna) and
[kdeconnect](#port-kdeconnect) among them, draw through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Patches

`focusrect-hasfocus.patch` makes the keyboard focus of a Qt Quick check box or radio button
visible. Both draw their focus frame with the style's private `FocusRect` item, shown while the
control has the active focus, but upstream's item never sets the style item's `hasFocus`, and Breeze
draws a focus frame, the line under the label, only for an option whose state has the focus.
Unpatched, Tab moves the focus onto the control and no pixel changes, so a keyboard user of a
System Settings page or the graphical installer cannot tell which option Space toggles. The patch
binds `hasFocus` to the control's `activeFocus`, the same condition that shows the item. It is to
be offered upstream as a merge request against `frameworks/qqc2-desktop-style`.

## Notes

The configure looks for KIconThemes, KColorScheme and libX11 as optional: without
[kiconthemes](#port-kiconthemes) Kirigami's icons ignore the icon theme's colouring, and without
[kcolorscheme](#port-kcolorscheme) its palette ignores the colour scheme. All three are in
`depends`, so all three are built in. It also looks up the QML module `org.kde.sonnet`, the spell
checking of text fields, which [sonnet](#port-sonnet) installs.
