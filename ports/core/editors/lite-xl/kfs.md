## About

Lite XL is a small text editor written mostly in Lua, with a C core that draws through SDL3 and
FreeType and needs no toolkit. Its editing commands, syntax highlighting and plugins are all Lua
files under `/usr/share/lite-xl`.

## Flags

- `--wrap-mode=nofallback` — Use the system's Lua, PCRE2 and FreeType and never a meson wrap. The
  source carries wraps for all three, each of which builds a static copy; with this mode a missing
  system library is a configure error instead (see
  [meson](../../../../docs/kdos/05-developer/writing-ports.md#meson)).
- `-Duse_system_lua=true` — Look for Lua by pkg-config name, among them `lua5.4` from the
  [lua54](#port-lua54) port. Off by default upstream, and off, Lua is always built from its wrap,
  which `nofallback` refuses.
- `-Ddirmonitor_backend=inotify` — Watch the project directory with inotify. Upstream's default,
  an empty value, probes for a backend; naming it keeps the choice the same on every build.

## Notes

Both patches are git commits with their own descriptions.
`cve-2025-12121-remove-legacy-exec-function.patch` removes `system.exec`, which handed a string to
`system()`, and changes the folder drop in `rootview.lua` and the tree view's *Open in System*
to start an argument vector through `process.start`. Against 2.1.8 its hunk for
`core:open-project-folder` (`data/core/commands/core.lua`) lands on the wrong branch: choosing the
folder that is already open starts a second window with no folder argument instead of reporting
it, and choosing any other folder still calls `system.exec` and raises a Lua error instead of
opening a second window.
`poor-text-rendering-20260312-a17c0455.patch` stops lite-xl setting its own LCD filter weights
under slight and full hinting, leaving FreeType's filter in place; upstream's issue reports poor
text rendering with those weights.

Upstream's icon is SVG only, which `build.sh` rasterises to PNG for the panel. Upstream's menu
entry is replaced: SDL3 takes the Wayland app_id from the metadata `main.c` sets,
`com.lite_xl.LiteXL`, not from the program's name. The entry claims no MIME types:
`inode/directory` would make Lite XL the folder opener, and `mimeapps.list` is where the default
text editor is chosen.
