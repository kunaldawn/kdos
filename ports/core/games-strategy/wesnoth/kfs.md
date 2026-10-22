## About

[The Battle for Wesnoth]{.idx} is a turn-based fantasy strategy game on a hex map, with
single-player campaigns, skirmishes against the computer, multiplayer, and a built-in map editor.
This port builds the game, `wesnoth`, and the multiplayer server, `wesnothd`.

## Flags

- `-DENABLE_GAME=ON` — with `-DENABLE_SERVER=ON`: the game and the multiplayer server. Both are
  upstream's defaults, named.
- `-DENABLE_CAMPAIGN_SERVER=OFF` — with `-DENABLE_MYSQL=OFF`: no add-ons server
  (`campaignd`) and no MySQL support for the servers. Both are upstream's defaults, named.
- `-DENABLE_TESTS=OFF` — do not build the unit tests. Upstream's default, named.
- `-DENABLE_NLS=OFF` — build no translations. Upstream's default follows `ENABLE_GAME`, so it would
  compile every language's catalogue, manual and manual pages; the game is English only, like the
  rest of the system.
- `-DENABLE_SYSTEM_LUA=OFF` — use the Lua in `src/modules/lua`, which upstream compiles as C++
  with its own `wesnoth_lua_config.h` forced in. The option's own description asks for a system
  Lua compiled as C++, and the tree's Lua is C.
- `-DENABLE_DISPLAY_REVISION=OFF` — do not show a revision number. Left unset, upstream runs
  `utils/autorevision.sh` at configure time to decide, which only succeeds in a checkout.
- `-DENABLE_DESKTOP_ENTRY=ON` — with `-DENABLE_APPDATA_FILE=ON`: install the desktop entry and the
  AppStream file. Both are upstream's defaults on Linux, named.
- `-DENABLE_NOTIFICATIONS=ON` — window-manager notifications over D-Bus, through
  [dbus](#port-dbus). Upstream's default; without `dbus-1` it turns itself off with only a
  message.
- `-DENABLE_STRICT_COMPILATION=OFF` — warnings are not errors. Upstream's default, named.
- `-DENABLE_LTO=OFF` — no link-time optimisation. Upstream's default, named.
- `-DFIFO_DIR=/run/wesnothd` — compile in the directory of `wesnothd`'s command FIFO, through which
  a running server takes commands. Upstream's default is `/var/run/wesnothd`.

## Notes

The install creates `FIFO_DIR` in the package, and `build.sh` removes it again with
`rm -rf "$PKG/run"`: `/run` is a tmpfs at run time, so a staged file there is lost, and the package
lint refuses it (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
`wesnothd` logs that it could not make the FIFO and runs without it.

Upstream's desktop entry runs the game through `sh -c` to hide its console output. `build.sh`
writes its own over it, which runs `wesnoth` directly, so the launcher sees the window it started,
sets `StartupWMClass=wesnoth`, the window's Wayland app id, and has a Map Editor action that runs
`wesnoth -e`.
