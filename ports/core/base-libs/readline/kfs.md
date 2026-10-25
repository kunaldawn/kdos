## About

GNU readline is the line editor interactive programs link to give their prompt Emacs- and vi-style
editing, history and completion; `libhistory`, its history half, comes with it. On KDOS it is the
line editing of [bash](#port-bash), of [gawk](#port-gawk)'s debugger, and of the many other shells,
calculators and debuggers in the tree that name it. It builds in `20_selfhost` because gawk links
it, and `30_foundation` names it again so a changed recipe is rebuilt there before anything else
in that phase uses it.

The port builds upstream's release with its official patch series applied: numbered files
(`readline<ver>-NNN`) that upstream publishes for each release. Each is a `source =` of the recipe with
its own `sha256 =`, so the archive and every patch are verified before anything is unpacked (see
[Patches](../../../../docs/kdos/05-developer/writing-ports.md#patches)). The `for` loop at the top
of `build.sh` applies them in order with `-p0`, because they are diffs taken from the top of the
release tree.

## Flags

- `--with-curses` — take the terminal functions (`tgetent`, `tputs` and the rest) from a curses
  library rather than from a separate termcap library.
- `--with-shared-termcap-library` — link `libreadline.so` itself against that library, so the
  shared library records that it needs `libncursesw.so.6` and a program linking readline does not
  have to name ncurses as well. readline's default leaves that to each program.
- `--disable-install-examples` — do not copy the C sources of readline's example programs
  (`examples/*.c`) into `/usr/share/readline`.
- `bash_cv_termcap_lib=libncursesw` — answer, in advance, configure's question of which library
  holds `tgetent`. Left to search, configure tries libc, `termcap`, `tinfo` and `curses` before
  `ncursesw`, and finds it in `libcurses.so`, the compatibility link the [ncurses](#port-ncurses)
  port installs. The link line would then name `-lcurses` and the `Requires.private:` of
  `readline.pc` would name `ncurses`, both aliases; with the answer they name `ncursesw`, the
  library itself.
- `SHLIB_XLDFLAGS` — the linker flags readline's makefiles add when they link the shared
  libraries. readline's default for Linux is `-Wl,-rpath,$(libdir) -Wl,-soname,…`; the recipe
  passes the soname part alone, on both `make` lines, so `libreadline.so` and `libhistory.so` carry
  no run-time search path. A plain `/usr/lib` `RPATH` is one of the findings the
  [package lint](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
  warns on, and it names a directory the loader searches anyway.

## Notes

The last line of `build.sh` is a check, not a build step: it reads the dynamic section of the
installed `libreadline.so` and fails the build if it carries an `RPATH` or `RUNPATH`. A change
that lets readline's default `-rpath` through stops the build there instead of shipping it.
