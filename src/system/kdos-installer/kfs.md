## About

kdos-installer is [kinstall]{.idx}, the installer: the full-screen terminal wizard that copies the
running live system onto a disk and makes it bootable, and the headless backend the graphical
installer ([kdos-installer-gui](#port-kdos-installer-gui)) runs through pkexec with
`--unattended --events json`. It is written in C and links no library beyond musl, not even a
terminal library. The work itself — the probe, the answers, the install steps, the files that make
the machine a KDOS Plasma desktop and the event stream — is `kinstall-core` in
`src/system/kinstall-core`, which uses `libkbase` and `libkcolor` alone; this port's own sources
are the wizard, drawn with `libktui`. All of it is compiled from source into the binary, together
with `kdos-appbox`'s `catalogue.c`, so the installer reads the application catalogue itself instead
of calling a program that may not exist yet. Its pages, steps, answer files and event stream are in
[kinstall](../../../docs/kdos/04-programs/kinstall.md).

The `kinstall` on the image is not this port's. `10_bootstrap`'s `130_kinstall.sh` compiles the
same sources with the cross compiler, so that every tree from the first bootable one onward has
an installer, and no phase list names this port. The recipe is there so a running KDOS can
rebuild the installer natively with `kpkg`.

## Notes

- **The two builds must compile the same sources.** Both take every `.c` file in this directory
  and in `../kinstall-core` by glob, never by name, plus `catalogue.c`, so a file added on one side
  is on the other. Only the compiler flags differ: the port takes the tree's `CFLAGS` and
  `LDFLAGS`, and the bootstrap step writes its hardening flags out itself. See
  [How it is built](../../../docs/kdos/04-programs/kinstall.md#how-it-is-built).
- **kinstall-core is compiled without `libktui` on its include path** and archived as
  `libkinstall-core.a` before the wizard links it, so a `ktui.h` include or a `libktui` call in the
  backend fails this build, not only `testing/selftest.sh`, which links the backend alone too.
- **`-I"$APPBOX"` is for `catalogue.c`'s header**, which lives with `kdos-appbox`; nothing else of
  that program is compiled in.
- **The recipe hash does not cover `catalogue.c` or `kinstall-core`.** It covers this directory and
  `src/libs`, so a `kpkg` build of this port counts an edit to either alone as nothing to do; see
  [The image's installer is rebuilt only when its mark is gone](../../../docs/kdos/06-reference/known-gaps.md#the-images-installer-is-rebuilt-only-when-its-mark-is-gone).
- **`sysvinit` is a dependency** for `reboot`, which the installer runs at the end of an install
  that asks for it.
- **A new library on its line moves the installer.** One that needs a real `-l` cannot be
  cross-compiled in the bootstrap, so the bootstrap step would move to a package phase and every
  tree before that phase would lose its installer.
