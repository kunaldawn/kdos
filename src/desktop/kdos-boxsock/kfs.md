## About

kdos-boxsock gives each box its own [Wayland socket]{.idx}. It binds a socket in the user's runtime
directory, hands it to the compositor through the `security-context-v1` protocol tagged with the
box's name, prints the socket's path, and stays alive holding the descriptor that keeps the tag
valid. Every client that connects on that socket is tagged by `kdos-comp` itself and never sees
the tag, so it cannot forge or drop it. The compositor's sandbox filter, the box chip on a title
bar and the panel's box names all read that tag. `kdos-appbox` starts one for a box before it runs
the box, and points the box's `WAYLAND_DISPLAY` at the socket.

It is a program of its own for two reasons: `kdos-appbox run` replaces itself with the container
command, so it cannot hold the descriptor for the box's lifetime, and `kdos-appbox` links no
Wayland library. Its arguments, its lock and its exit codes are in
[The daemons](../../../docs/kdos/04-programs/daemons.md#kdos-boxsock).

## Notes

- **It uses none of KDOS's own libraries.** The build compiles `main.c` with the protocol code
  `wayland-scanner` generates and links `wayland-client`, nothing else.
- **The protocol comes from wayland-protocols' `staging/` directory**, found through
  `pkg-config --variable=pkgdatadir wayland-protocols`. The scanner writes the client header and
  the `private-code` glue into the work directory, and `-I.` is what lets `main.c` include the
  header.
- **The compile line passes no `-D_GNU_SOURCE`.** `main.c` defines it itself, guarded by
  `#ifndef`, because `testing/selftest.sh` compiles the same file with the define on its command
  line and `-Werror`, where an unguarded second definition is an error.
