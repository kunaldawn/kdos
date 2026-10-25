## About

libedit, also called editline, is the NetBSD [line editor]{.idx} packaged for other systems: a
library that gives an interactive prompt history, Emacs and vi key bindings and tab completion,
with a compatibility layer for the readline API. In KDOS it is what `lldb` links for its prompt —
without it `(lldb)` has no history, arrow-key editing or completion, and lldb has no readline
fallback — and `openssh`'s `sftp`, `chrony`, `xfsprogs`, `ledger` and others link it too.

## Flags

- `--disable-examples` — Skip the example programs `tc1`, `wtc1` and `fileman`, which are
  compiled but never installed.

## Notes

- **The terminal database comes from ncurses.** `configure` walks `ncurses`, `curses`, `termcap`
  and `tinfo` looking for `tgetent` and stops at the first; with no `ncurses` installed the search
  ends in an error rather than a narrower library, which is why `ncurses` is the one dependency.
- **The version is the upstream date.** Upstream names its tarballs `libedit-<date>-<api>`; the
  recipe's `version` is the date and its `_api` helper the API version, and the `source =` line
  saves the download under the port's own `$name-$version` name.
