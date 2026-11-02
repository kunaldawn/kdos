## About

KDecoration3 is the plugin interface a window decoration is written against: the title bar, its
buttons and the frame KWin draws around every window that does not draw its own. KWin loads the
decoration plugins through it, and [breeze](#port-breeze), [oxygen](#port-oxygen) and [aurorae](#port-aurorae) are
such plugins. It is a library with no window of its own and links nothing but Qt and
[ki18n](#port-ki18n).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the decoration button
  tooltips, which the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`sandbox-badge.patch` adds `DecoratedWindowPrivateV5`, whose three accessors give a decoration the
sandbox engine, app id and instance id of its window's client, and exposes them with
`isSandboxed()` as constant properties of `DecoratedWindow`. [kwin](#port-kwin) implements it with
its own `sandbox-badge.patch`, and [breeze](#port-breeze) draws a box glyph from it; both are built
against this patched port and do not compile against an unpatched one. The change is additive, so
a decoration built before it still loads, and a compositor that implements only the earlier
interface answers empty strings. It is proposed upstream with the KWin and Breeze halves.

## Notes

The port installs two libraries: `libkdecorations3`, which a decoration plugin links, and
`libkdecorations3private`, the compositor's side of the interface, which KWin links. The private
one makes no compatibility promise from one Plasma release to the next, so KWin and the
decoration plugins are built against the same version of this port.
