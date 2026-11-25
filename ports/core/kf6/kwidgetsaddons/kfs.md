## About

KWidgetsAddons is the KDE Frameworks collection of extra Qt Widgets: message boxes and the
`KMessageWidget` banner, collapsible group boxes, page dialogs, the font and date choosers, ratings,
the action menus and toolbar helpers that KDE dialogs are built from. It has no dependency beyond
Qt Widgets, and many KDE libraries and applications with a window link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch also skips upstream's `en`
  catalogue, which holds only English plural forms, so a count in one of the widgets reads as the
  source spells it, with the `(s)` left in.

## Patches

`rest-day-contrast.patch` changes the colour `KDateTable`, the month grid of `KDatePicker` and of
the Date & Time settings page, draws a rest day in: the weekday headers of the days outside the
locale's working week, and every Sunday's date. Upstream draws both in the fixed `Qt::darkRed`,
which reads on a light ground and measures about 1.8:1 on a dark one, so under every dark KDOS
accent (all but Paper) the Sunday column is close to invisible. With the patch the table takes
dark red or a light red (HSL 0°, 100%, 75%), whichever has the higher WCAG contrast with the
ground that cell is drawn on: the window colour for a header and the cell's background for a
date. Every dark KDOS accent then gives at least 6:1 with the light red, and Paper, like any light
scheme, keeps upstream's dark red at about 10:1. The selected day is drawn in the highlighted text
colour even on a Sunday: no red reaches 3:1 on the selection colour of any KDOS scheme (2.2 to
2.6:1), where the highlighted text gives 4.5:1 or better. A disabled table, such as the Date & Time
page's calendar while the time is set automatically, mixes the red toward the cell's ground until
it contrasts no more than that cell's plain text, which the palette's Disabled group dims; at
full strength the rest days would be the only dates that look clickable. A high-contrast colour
scheme still draws no red at all, as upstream does. It is to be offered upstream as a merge request
against `frameworks/kwidgetsaddons`.
