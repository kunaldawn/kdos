## About

Stella emulates the Atari 2600 Video Computer System, with a built-in debugger for the 6507 and
the cartridge's bank switching. It is an SDL 2 program, drawn here by
[sdl2-compat](#port-sdl2-compat) on SDL 3, with its own launcher and menus and no toolkit. The
player supplies the cartridge ROMs.

## Flags

- `--with-sdl-prefix="$SRC_ROOT/sdl-shim"` — Where configure looks for `sdl2-config`. configure
  takes SDL's flags only from that script and has no other route, and `sdl2-config` does not exist
  here, so `build.sh` writes a small one into `$SRC_ROOT/sdl-shim` that answers `--cflags`,
  `--libs` and `--version` from pkg-config, which reads sdl2-compat's `Provides: sdl2`.
- `--enable-release` — Build with every optimisation, for a release. Off by default upstream.
- `--enable-gui` — This switch and `--enable-sound`, `--enable-debugger`, `--enable-joystick`,
  `--enable-cheats`, `--enable-png`, `--enable-zip`, `--enable-windowed` and `--enable-shared` are
  upstream's defaults: the built-in interface, sound, the debugger, joysticks, cheat codes, PNG
  snapshots, zipped ROMs, windowed modes and a dynamically linked binary.

## Notes

zlib, libpng and SQLite, the store for settings and ROM properties, come from the system, and all
three are in `depends`. A missing one does not stop configure: it compiles the copy of SQLite in
`src/lib/sqlite` instead, and without libpng it drops PNG snapshots, and without zlib both PNG
snapshots and zipped ROMs, printing only `disabled`.

PlusROM cartridges reach a high-score server over HTTP from inside the game. On a machine with no
network the cartridge runs and its upload fails; nothing else in Stella goes online.

Upstream's menu entry is replaced for `StartupWMClass`: SDL's Wayland app_id is the executable's
name, `stella`.
