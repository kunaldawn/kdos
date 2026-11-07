## About

kdos-boxsock gives each launcher of a box its own [Wayland socket]{.idx}. It binds the socket in
the box's private runtime directory, `$XDG_RUNTIME_DIR/kdos/box/<box>/`, hands it to KWin through
the `security-context-v1` protocol tagged with the launcher's id as the app id, the box as the
instance id and the engine `io.kdos.appbox+private` or `io.kdos.appbox+tagged`, prints
`ready <path>`, and stays alive holding the descriptor that keeps the tag valid. Every client that connects on that socket is tagged by
KWin itself and never sees the tag, so it cannot forge or drop it. KWin's sandbox filter and its
grants (read by the instance id from the box's profile), the window's application id, the box's
title-bar colour rule and the sandbox badge all read that tag.
`kdos-appbox` starts one before it runs a launcher in a box and points the program's
`WAYLAND_DISPLAY` at the socket; when this program prints `error <reason>` instead, the box does
not start and the reason is what the person sees.

It is a program of its own for two reasons: `kdos-appbox run` replaces itself with the container
command, so it cannot hold the descriptor for the box's lifetime, and `kdos-appbox` links no
Wayland library. It is box infrastructure beside `kdos-appbox` and the boxes' init, and links no
toolkit. Its arguments, its lock and its exit codes are in
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
- **The socket's path has to fit a Unix socket address**, 108 bytes:
  `$XDG_RUNTIME_DIR/kdos/box/<box>/wl[.<stem>]@<display>`. A box name long enough to overflow it
  is refused with that reason rather than bound under a shortened name the box could not find.
- **A launcher id is the box, the box and one more component, or the box less its last one.**
  The last is a throwaway box, `<launcher-id>.tw<n>`, whose windows are its application's; a launcher
  id further from its box is refused, because the id is the window's identity in the Task Manager
  and KWin's rules.
- **The lock is outside the private directory**, at `$XDG_RUNTIME_DIR/kdos/box/.<box>.wl…lock`,
  so nothing running in the box can take it and keep the socket from being served.
