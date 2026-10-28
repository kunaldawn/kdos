## About
`kdos-boxinit` is process 1 inside a pack box, in place of distrobox's init. It writes a user and
group matching yours into the box, sets the search path and `HOME`, writes the box's `xdg-open`
and `asound.conf` so links and sound reach the host, prints the `container_setup_done` marker the
launcher waits for, and then stays alive reaping orphaned processes. What each step is for is in
[The box](../../../docs/kdos/03-architecture/packs-and-boxes.md#the-box).

The package installs one file, `/usr/libexec/kdos/kdos-boxinit`. It is not run on the host:
[kdos-appbox](#port-kdos-appbox) bind-mounts it read-only into every pack box at
`/usr/libexec/kdos-boxinit` and names it as the container's entrypoint
([What a pack box is created with](../../../docs/kdos/04-programs/kdos-appbox.md#what-a-pack-box-is-created-with)).
A store box runs distrobox's own init instead.

## Flags
- `-static-pie` — Link a static, position-independent executable. The binary comes from the
  host but runs inside a box whose `/usr` is Debian's, and a binary linked against the host's musl
  would look there for its loader and not find it.
- `-I"$LIBS/libkbase"`, `"$LIBS"/libkbase/*.c` — Compile libkbase into the binary, the one
  library it uses; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
  The search path it exports is libkbase's `KB_BOX_PATH`, the same string kdos-appbox puts in
  front of every command it runs through `podman exec`, which does not inherit process 1's
  environment; one definition keeps the two from disagreeing.
- `-std=gnu11`, `-D_GNU_SOURCE` — The C dialect, which the tree's `CFLAGS` already names, and the
  macro that exposes the C library's GNU and POSIX extensions.
- `-O2`, `-Wall`, `-Wextra` — The tree's optimisation level, named again, and the compiler's
  common and extra warnings.

## Notes
It is a process 1, with process 1's obligations: if it exits the container goes with it, and if
it stops reaping, every application that exits leaves a zombie until the process table fills.

It runs as the box's root and takes the box's name and the account it writes from its
environment: kdos-appbox passes `KDOS_BOX`, `KDOS_BOX_USER`, `KDOS_BOX_UID`, `KDOS_BOX_GID` and
`KDOS_BOX_HOME` when it creates the box, and `KDOS_BOX_SHELL`, which nothing passes, falls back to `/bin/bash`. It runs no
shell and builds no command line from those values.

`container_setup_done`, printed unbuffered on standard output, is the contract with the launcher,
which reads it out of podman's log. It is printed only after the account records are written:
printed earlier, it would let a launch run a program in a box that has no such user yet.
