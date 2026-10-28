## Notes

`configure` treats PCRE2 and readline as optional and builds a reduced tig without them: searches
fall back to the C library's POSIX regular expressions, and the prompt loses line editing and
history. Wide-character ncurses is optional too unless asked for; without it configure only warns
that UTF-8 will be drawn wrongly. `--with-ncursesw` makes configure stop when it is missing, and
`build.sh` reads `config.h` after `configure` and stops when `HAVE_PCRE2`, `HAVE_READLINE` or
`HAVE_NCURSESW` is not defined, so a dependency that goes missing fails the build instead.

`make install` installs only the program and `/etc/tigrc`. The release archive carries the manual
pages already built, `doc/tig.1`, `doc/tigrc.5` and `doc/tigmanual.7`, and `build.sh` installs them
as they are, so the build runs neither asciidoc nor xmlto.

The desktop entry is KDOS's own, with `Terminal=true` and `NoDisplay=true`. tig browses the
repository of the directory it starts in, and an entry started from Start starts in the home
directory, where tig finds no repository and exits at once; it is run from a prompt inside a
repository.
