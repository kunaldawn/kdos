## About

kdos-res is the [resource monitor]{.idx}, the program the desktop calls **Resources**. It is one
binary that runs as a window under the compositor and full screen in a terminal, on `tty2` or
over ssh, because both are the same grid of cells. It has a page per kind of device, a sortable
process table, and an application rollup that names a boxed application by the container
supervising it, which needs no cgroup delegation from an init system. It ships beside `btop`,
which stays installed as the fallback. Everything is in
[kdos-res](../../../docs/kdos/04-programs/kdos-res.md).

The same recipe builds `kdos-resctl`, a small setuid-root helper with three verbs and no paths in
its arguments: it signals or renices a process the caller does not own, and reads the SMBIOS
table, for members of `wheel` only, checked by real user id. `kdos-res` calls it only when you
act on a process you do not own, or renice one below zero; your own processes it signals and
renices directly. The argument for its safety is in
[kdos-resctl](../../../docs/kdos/03-architecture/security-model.md#kdos-resctl).

## Flags

- `-DKDOS_RES_VERSION` — The version `kdos-res --version` prints, here the recipe's `version`.
- `-ldl` — Link `dlopen()` and `dlsym()`, which `libkproc` uses to open NVIDIA's management
  library, `libnvidia-ml.so.1`, at run time when one is installed, so the GPU page can show its
  figures. Nothing is linked against NVIDIA at build time, and a stock KDOS has no such library.
  musl keeps both functions in the C library and installs `libdl.a` as an empty archive, so here
  the flag resolves and adds nothing.
- `-Dm4755` — Install `kdos-resctl` owned by root with the setuid bit, as a mode bit, which
  survives the package archive where a file capability would not; see
  [setuid binaries](../../../docs/kdos/03-architecture/security-model.md#setuid-binaries). When the
  helper is missing or has lost the bit, `kdos-res` disables the actions that need it and says why
  on the button, and `kdos doctor` reports the bit.

## Notes

- **Every `.c` file but `resctl.c` goes into `kdos-res`, by glob**, as `testing/selftest.sh`
  compiles it. `resctl.c` has a `main` of its own and is compiled on the second line, with
  `libkbase` and nothing else, so none of the Wayland stack and none of the drawing libraries runs
  in the setuid process. A new source file joins `kdos-res` by being in the directory.
- **wlroots is a dependency for its protocol files only.** The layer-shell and
  foreign-toplevel protocols are generated from `/usr/share/wlroots/protocols/`, which the
  `wlroots` recipe installs; the monitor links none of wlroots.
- **The whole protocol set is generated, used or not.** `libkwl` is compiled whole and its
  `kwl.c` includes the header of every protocol it can use, so each consumer generates the client
  header and the `private-code` of all of them. A missing one fails the compile or the link.
- **`kdos-res.desktop` is installed by the port**, so the **Resources** menu entry comes and goes
  with the package.
