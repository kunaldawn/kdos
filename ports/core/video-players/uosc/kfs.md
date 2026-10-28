## About
[uosc]{.idx} is a replacement for mpv's built-in on-screen controller: a proximity-based
timeline and controls, menus, and a playlist, written in Lua as an mpv script. Installed in mpv's
system script directory, it replaces the built-in controller for every user of
[mpv](#port-mpv); its `main.lua` sets `osc=no` itself.

## Notes
- **ziggy is built here.** uosc's helper binary, `ziggy`, does this work for the script: the
  clipboard, the OpenSubtitles search and download, file downloads and other HTTP requests, and
  opening a path or address in its default program. Upstream's release archive carries it
  prebuilt for three systems; this port builds it from `src/ziggy` against the Go vendor bundle
  instead, and does not build `src/tools`, upstream's packaging helper. Its clipboard code runs `wl-copy` and
  `wl-paste` when `WAYLAND_DISPLAY` is set, which is why [wl-clipboard](#port-wl-clipboard) is in
  `depends`.
- **Where the files go.** The script tree goes to `/etc/mpv/scripts/uosc`, mpv's system script
  directory, and its options to `/etc/mpv/script-opts/uosc.conf`. The binary lives at
  `/usr/lib/uosc/ziggy`, and `bin/ziggy-linux` inside the script directory, the path `main.lua`
  looks for, is a link to it.
- **The two faces are system fonts.** `uosc_icons.otf` and `uosc_textures.ttf` are found by family
  name through fontconfig, so they go to `/usr/share/fonts/uosc` rather than into a per-user
  mpv fonts directory. They ship as upstream built them; see
  [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
- **English only.** The `intl` directory of translated strings is removed; the English strings are
  built into the script, and a missing locale file falls back to them.
