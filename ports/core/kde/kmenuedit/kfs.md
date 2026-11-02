## About

KMenuEdit edits the application menu that Kickoff, KRunner and the other launchers show: it adds,
hides and moves entries and categories and gives an entry a global shortcut. It writes the user's
changes as a menu file and desktop entries under `~/.config/menus` and `~/.local/share/applications`,
named for the session's `XDG_MENU_PREFIX`, which [plasma-workspace](#port-plasma-workspace)'s
`startplasma-wayland` sets to `plasma-`; the system menu it starts from is
`/etc/xdg/menus/plasma-applications.menu`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The handbook is built by default and makes [kdoctools](#port-kdoctools) a requirement of the
configure.
