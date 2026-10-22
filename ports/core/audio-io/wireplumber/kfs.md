## About
WirePlumber is PipeWire's session manager: the policy half of the audio stack. It finds sound
cards, cameras and Bluetooth devices and creates their nodes, decides which sink or source a new
stream is linked to, picks a Bluetooth headset's profile, and remembers and restores volumes. Its
policy is written in Lua scripts run by an embedded Lua interpreter. [pipewire](#port-pipewire) is
built with no session manager of its own, and its daemon routes nothing without one, so on KDOS
WirePlumber is what makes sound work at all. The session starts it right after `pipewire` and
before `pipewire-pulse`; see
[Audio](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#audio).

## Flags
- `-Dsystem-lua=true` — Link the tree's [lua](#port-lua) instead of building the copy of Lua
  upstream bundles as a meson subproject and links statically, which would put a second Lua on the
  image and needs a subproject the offline build does not fetch.
- `-Dsystem-lua-version=5.5` — Look for Lua 5.5 by name, through the `lua5.5.pc` the lua port
  installs, and stop the configure step if it is missing. Left at `auto`, upstream tries 5.5, 5.4
  and 5.3 in turn and builds against the first it finds, so a missing 5.5 would give a WirePlumber
  linked to an older Lua with no error.
- `-Delogind=disabled` — No elogind integration. Seats and sessions on KDOS are seatd's, and left
  at `auto` the option finds nothing and turns itself off without a word.
- `-Dsystemd=disabled` — No systemd units and no logind integration; KDOS has no systemd.
- `-Ddoc=disabled` — Build no documentation.
- `-Dtests=false` — Build no test suite.
- `-Dintrospection=disabled` — Generate no GObject introspection bindings for `libwireplumber`.

## Notes
`kdos-oomd` protects processes by name, and names `wireplumber` on its own: the `pipewire` prefix
that covers `pipewire` and `pipewire-pulse` does not match it, and killing it leaves a running
graph with nothing routed.
