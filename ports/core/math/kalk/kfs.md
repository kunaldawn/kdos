## About

Kalk is KDE's convergent calculator, a Kirigami application that fits a phone as well as a
window: an expression line with a history, a function pad, a binary and hexadecimal pad and a unit
converter. Expressions are evaluated by [libqalculate](#port-libqalculate) and units come from
[kunitconversion](#port-kunitconversion). [kcalc](#port-kcalc) is the calculator the desktop opens
by default; Kalk is installed beside it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`desktop-name.patch` names the desktop entry `Kalk`, with `Calculator` as its generic name, where
upstream calls it `Calculator`. That name belongs to [numbat](#port-numbat)'s entry already, and
two visible entries that share a name cannot be told apart in the launcher; `testing/preflight.sh`
refuses the pair.

## Notes

`pkg_search_module(QALCULATE libqalculate>4.7.0 REQUIRED)` makes libqalculate a requirement of
the configure, through [pkgconf](#port-pkgconf). The QML interface imports
[kirigami-addons](#port-kirigami-addons) and is drawn through
[qqc2-desktop-style](#port-qqc2-desktop-style), both of them in `depends` for run time.
