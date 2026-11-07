## About

colord-kde is the System Settings page for colord: it lists the colour-managed devices colord
knows (displays, printers, scanners, cameras), the ICC profiles installed for each, and lets a
profile be assigned or made the default. A small importer installs an ICC profile file opened
from a file manager.

## Flags

- `-D WITHOUT_X11=ON` — Build no `colord-kded`. That module maps X11 RandR outputs to colord
  display devices and is built whenever X11 is found; under Wayland it has no outputs to map, and
  KWin applies a display's ICC profile itself. Off, libX11 and xcb are not looked for.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Nothing links colord: the page and the importer speak `org.freedesktop.ColorManager` on the system
bus, from interface descriptions the source carries, so [colord](#port-colord) is in `depends` for
run time only and the page lists nothing while the daemon is not running. Profile details are read
with [lcms2](#port-lcms2), found through pkg-config.

colord's own actions (creating devices, importing profiles system-wide) are polkit `allow_active`
grants, which an active elogind session satisfies.
