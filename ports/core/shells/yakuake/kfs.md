## About

Yakuake is a drop-down terminal: a global shortcut (F12 by default) slides a terminal down from
the top edge of the screen and back, with tabs and split panes. The terminal itself is
[konsole](#port-konsole)'s KPart, loaded at run time, so profiles, colour schemes and keys are
Konsole's. On Wayland the window is placed and animated through the Plasma shell protocol, from
[kwayland](#port-kwayland), and the shortcut is registered with KDE's global shortcut daemon
through [kglobalaccel](#port-kglobalaccel).

## Flags

- `-D WITH_X11=OFF` — Leave out the XCB code, which places the window and finds the screen under
  the pointer when Yakuake runs on the X11 platform plugin. Yakuake on KDOS is a Wayland client;
  upstream's default is on, and with it RandR and xcb-util become requirements for code that
  would never run.
- `-D REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS=OFF` — Keep the D-Bus methods that type text or run a
  command in a Yakuake session. Upstream's default; named because removing them is the switch a
  hardened build would flip, and scripts that drive Yakuake use them.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The slide-in, the keep-above and the placement on the active screen need KWin: they are requests
in KDE's own Wayland protocols, which another compositor ignores, leaving an ordinary window. The
`kde-output-order-v1` client code is generated at build time from
[plasma-wayland-protocols](#port-plasma-wayland-protocols) with `wayland-scanner`, from
[wayland](#port-wayland).

The "Get New Skins" button uses KNewStuff, which upstream requires; with KNewStuff switched off
in `kdeglobals` the button is greyed and the skins that ship with the package are the ones
offered.
