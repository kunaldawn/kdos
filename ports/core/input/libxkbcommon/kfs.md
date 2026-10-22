## About
libxkbcommon compiles a keyboard description (rules, model, layout, variant, options) from the
[xkeyboard-config](#port-xkeyboard-config) database into a keymap, and turns key codes into key
symbols and text as modifiers and layout groups change. Every Wayland compositor and client that
reads a keyboard uses it, so it sits under the whole desktop: [kdos-comp](#port-kdos-comp) and
[wlroots](#port-wlroots), [kdos-term](#port-kdos-term), [kdos-shell](#port-kdos-shell),
[kdos-lock](#port-kdos-lock) and [kdos-res](#port-kdos-res) link it, as do GTK, Qt, the browsers and
most of the other ports that depend on it. A layout it cannot compile makes the compositor fall
back to US for the whole session, the lock screen's password prompt included; see
[The shared bring-up](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#the-shared-bring-up-session-commonsh).

It also installs `libxkbregistry`, which lists the layouts and options the database offers (built
on [libxml2](#port-libxml2)), and the `xkbcli` tools. [kbd](#port-kbd) builds against it too,
which places kbd, and [kdos-tools](#port-kdos-tools) after it, in `42_graphics`; see
[Build phases are named, banded and closed](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/decisions.md#build-phases-are-named-banded-and-closed)
for that edge.

## Flags
- `-Denable-wayland=true` — Build the Wayland `xkbcli` program, `xkbcli interactive-wayland`,
  which prints what each key press resolves to under a Wayland compositor. It needs
  [wayland](#port-wayland) and [wayland-protocols](#port-wayland-protocols), and the configure
  step stops if either is missing.
- `-Denable-x11=true` — Build `libxkbcommon-x11`, which reads the keymap from an X server over
  `xcb-xkb`, from [libxcb](#port-libxcb). Qt's xcb platform plugin needs it for every Qt program
  that runs under Xwayland.
- `-Denable-docs=false` — Build no API documentation.

## Notes
The path to the layout database is compiled in. The configure step reads it from
xkeyboard-config's pkg-config file, and when that is missing it compiles in the legacy X11 path
instead, with no error. xkeyboard-config has to be installed before this port is built, which its
place in `depends` ensures.
