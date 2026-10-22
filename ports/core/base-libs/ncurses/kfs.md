## About

ncurses is the terminal-handling library behind full-screen text programs: it reads the terminfo
database to learn what a terminal can do and draws windows, colours and menus through it. It also
installs the terminfo database itself and the tools that compile and query it, `tic`, `infocmp`
and `tput`. [readline](#port-readline), [bash](#port-bash) and the text-mode programs of the tree
build against it, and `20_selfhost` builds it because [gawk](#port-gawk) needs readline.

KDOS builds one ncurses, the wide-character one, and makes every other name a build may ask for
point at it. Its headers go flat into `/usr/include`, with no `ncursesw/` directory; what that
means for a port that includes `<ncursesw/ncurses.h>` or wants the wide API is in
[The wide curses API](../../../../docs/kdos/05-developer/build-troubleshooting.md#the-wide-curses-api).

## Flags

- `--mandir=/usr/share/man` — install the manual pages in `/usr/share/man`. This is also what
  configure's default, `${datarootdir}/man`, comes to under `--prefix=/usr`; the switch names it.
- `--with-shared` — build shared libraries as well as the static ones configure builds by default.
- `--without-debug` — skip the `_g` debugging variants of each library, which configure builds by
  default.
- `--with-cxx-shared` — build the C++ binding, `libncurses++w`, as a shared library too.
- `--enable-pc-files` — install a pkg-config file for each library (`ncursesw.pc`, `formw.pc` and
  the rest).
- `--disable-pkg-ldflags` — keep the linker flags of this build out of those `.pc` files and out of
  `ncursesw6-config`. Without it configure copies `$LDFLAGS` into them, and every program that
  asks pkg-config how to link ncurses would be handed this build's
  [release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags) as well.
- `--enable-widec` — build the wide-character libraries, which handle UTF-8. Each carries a `w` in
  its name: `libncursesw`, `libformw`, `libmenuw`, `libpanelw`. For this ABI it is configure's
  default; the switch names it.
- `--with-pkg-config-libdir=/usr/lib/pkgconfig` — install the `.pc` files there. Left to itself,
  configure takes the first directory of `$PKG_CONFIG_PATH` that exists. The chroot's
  `PKG_CONFIG_PATH` (from `script/env/chroot.env`) starts with `/usr/local/share/pkgconfig`,
  `/usr/local/lib/pkgconfig`, `/usr/local/lib64/pkgconfig` and `/usr/share/pkgconfig`, so the files would land in the first of
  those the root holds when the port builds, ahead of `/usr/lib/pkgconfig` later in the list.

## Notes

The `for` loop after the install links the names without the `w` to the wide libraries:
`libncurses.so`, `libform.so`, `libmenu.so` and `libpanel.so`, and the `.pc` files of the same
names. `libcurses.so` then links to `libncurses.so`. A build that asks for `-lncurses`, `-lcurses`
or `pkg-config ncurses` gets the wide library, and nothing in the tree links a narrow one, because
there is none.

Those links are also what [readline](#port-readline)'s configure finds first when it searches for
the library holding `tgetent`, which is why that port names `libncursesw` outright.
