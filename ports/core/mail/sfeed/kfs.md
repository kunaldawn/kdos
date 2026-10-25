## About
[sfeed]{.idx} parses RSS and Atom feeds into a tab-separated file, one line per item, and ships the
small programs that read that file: `sfeed_update` fetches the feeds listed in
`~/.sfeed/sfeedrc` with [curl](#port-curl), `sfeed_curses` is a terminal reader, and others write
the items out as plain text, HTML, JSON, an mbox, a gopher page or Atom again. Where
[newsboat](#port-newsboat) is a reader with a database, sfeed is a pipeline whose file can be read
with `grep` and `cut`.

## Flags
- `MANPREFIX=/usr/share/man` — upstream's default is `${PREFIX}/man`, which here would be
  `/usr/man`, a directory `man` does not search.
- `DOCPREFIX=/usr/share/doc/sfeed` — the directory the README and example files go to; upstream's
  default spells the same path from `PREFIX`.
- `SFEED_CURSES_LDFLAGS` — the link line of `sfeed_curses`. Upstream's default ends in `-lcurses`;
  this names the real library, `-lncurses`, with the exported `LDFLAGS` before it. The
  [ncurses](#port-ncurses) port installs `libcurses.so` only as a compatibility link.

## Notes
`build.sh` adds three files of KDOS's own.

- `/usr/bin/sfeed-read` is what the desktop entry runs. `sfeed_curses` with no file arguments reads
  a feed from standard input, which from a launcher is the terminal, so it would sit in an empty
  window waiting for end of input. The wrapper passes every file under `~/.sfeed/feeds`, where
  `sfeed_update` writes by default, and with none there yet prints how to fetch some and waits for
  Enter.
- `/etc/profile.d/50-sfeed.sh` sets `SFEED_YANKER` to `wl-copy -n` from
  [wl-clipboard](#port-wl-clipboard) unless it is already set. `sfeed_curses` pipes a link to that
  command when its yank key is pressed, and its default, `xclip -r`, needs an X clipboard.
- `sfeed.desktop`, with `Terminal=true`: upstream ships no desktop entry.

The exported `CFLAGS` already carry `-O2`, so the `-O2` that `build.sh` appends changes nothing in
the compile.
