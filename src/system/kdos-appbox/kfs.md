## About

`kdos-appbox` is the KDOS program that runs [alien apps]{.idx}, the graphical applications this
tree does not compile, each in a rootless [podman](#port-podman) container of its own. It builds
them from the catalogue it ships, creates and enters their boxes, writes the launchers that make
them appear in the Start menu as if they were native, and opens files and links in whatever
handles them, boxed or native. One binary dispatches on the name it was started under: `kdos-appbox`
installs and launches applications, `kdos-box` manages boxes, `xdg-open` opens files and links,
and any other name is a shim, a link named after an application that runs it in its box. The
`kdos app` subcommands run it, every generated launcher calls it, and the session's warm-up calls
it at login. Everything it does is
described in [kdos-appbox](../../../docs/kdos/04-programs/kdos-appbox.md).

The port has no upstream source. `build.sh` compiles the program's own files in this directory
together with the sources of four KDOS libraries, `libkbase`, `libktui`, `libkcolor` and
`libkxdg`, into one binary that links no library outside the C library; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Flags

- `-std=c11` — Compiles as ISO C11. It comes after `$CFLAGS`, so it replaces the tree's
  `-std=gnu11`; `-D_GNU_SOURCE` beside it keeps the POSIX and GNU declarations of musl's headers
  visible.

## Notes

The binary goes to `/usr/local/bin`, and so do its two links. `/etc/profile` puts `/usr/local/bin`
ahead of `/usr/bin`, and that order is the only thing that makes this `xdg-open` answer before the
script of the same name from [xdg-utils](#port-xdg-utils), which the port depends on: when it
finds no handler at all for a file or a link, it runs that script as `/usr/bin/xdg-open`. A
`PATH` with `/usr/bin` first gets the xdg-utils script for every link a program opens. The
`xdg-open` link belongs to this package rather than to the `fs/` overlay, so a root never holds
the link without the binary that answers to it.

The catalogue, `/usr/share/kdos/appstore/catalogue`, is data the program reads, and it ships in
this package for the same reason: with the file missing, `kdos-appbox catalogue`, `install`,
`export` and `import` stop with `cannot read the catalogue`, and the Store surface lists no
applications and reports no catalogue. Editing the catalogue is editing this port, and it rebuilds it.

The program is built in the work directory, never in `$PORT_SRC`, which is the program's directory
in the repository. A port under `src/` hashes its whole directory and all of `src/libs` as its
recipe, so editing any of the four libraries, or any other, rebuilds it; see
[the recipe hash](../../../docs/kdos/03-architecture/packaging.md#e--the-recipe-hash).
