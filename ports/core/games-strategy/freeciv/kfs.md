## About

Freeciv is a turn-based empire-building game in the tradition of Civilization: a civilisation
grows from one settler to a space programme against computer or human rivals. The port builds the
SDL2 client, `freeciv-sdl2`, the `freeciv-server` it starts for a local game, and two command-line
rules tools. The client draws through SDL2 on [sdl2-compat](#port-sdl2-compat), so it needs no
toolkit.

## Flags

- `-Dclients=sdl2` — Builds the SDL2 client and none of the others (GTK 3.22, GTK 4, Qt, SDL3 and
  the stub). Upstream's default is the GTK 4 client.
- `-Dfcmp='[]'` — Builds no front end of the modpack installer, which installs rulesets, tilesets
  and other add-ons by downloading them from the network. Upstream's default builds its GTK 4 front
  end.
- `-Dtools=manual,ruleup` — Builds `freeciv-manual`, which writes the rules of a ruleset out as an
  HTML or wiki manual, and `freeciv-ruleup`, which upgrades a ruleset to the current format. The
  third tool, the rules editor `freeciv-ruledit`, is a Qt program and is left out.
- `-Dsyslua=false` — Compiles the copy of Lua 5.4 that freeciv carries in
  `dependencies/lua-5.4/` instead of looking for a system Lua. Freeciv's tolua bindings are built
  against that copy.
- `-Dsys-tolua-cmd=false` — Builds the bundled `tolua` code generator and runs it, rather than a
  `tolua` from the system; there is no `tolua` port.
- `-Dreadline=true` — Gives the server's command line readline editing and history, and stops
  configure if [readline](#port-readline) is not found; upstream's `try` would leave it out
  quietly.
- `-Dmwand=false` — Leaves out MagickWand, which lets the server's `mapimg` command write map
  images in formats other than PPM.
- `-Dnls=false` — No translations; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Dserver=enabled`, `-Daudio=sdl2`, `-Djson-protocol=false`, `-Dgitrev=false`,
  `-Dappimage=false`, `-Dsvgflags=true` — Upstream's defaults, named so the build does not change
  with them: the plain server rather than the freeciv-web one, sound through SDL2, the ordinary
  network protocol, no git commit id in the version string, an ordinary install rather than an
  AppImage layout, and the SVG national flags.

## Notes

Freeciv installs a manual page for every client and front end whatever was built. The loop after
the install removes the pages for the GTK 3.22, GTK 4, Qt and SDL3 clients, the modpack installer
in all its forms, and the rules editor, so no page describes a program the package does not carry.

Upstream's `org.freeciv.sdl2.desktop` is replaced by one written in `build.sh`, which adds
`StartupWMClass=freeciv-sdl2`: SDL gives the window the program's name as its Wayland app_id, and
the line ties the window to its launcher. The server's entry stays installed but is hidden with
`desktop-file-edit`, so the menu lists the client alone (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
